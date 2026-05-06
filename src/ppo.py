import numpy as np
from scipy.stats import dirichlet
from scipy.special import gammaln, psi
from math import lgamma

np.random.seed(42)

class RolloutBuffer:
    def __init__(self):
        self.states = []
        self.actions = []
        self.rewards = []
        self.values = []
        self.log_probs = []
        self.dones = []

    def store(self, state, action, reward, value, log_prob, done):
        self.states.append(state)
        self.actions.append(action)
        self.rewards.append(reward)
        self.values.append(value)
        self.log_probs.append(log_prob)
        self.dones.append(done)
    def clear(self):
        self.__init__()

def stable_softplus(x):
    return np.log1p(np.exp(-np.abs(x))) + np.maximum(x, 0)

def dirichlet_log_prob(weights, alphas):
    weights = np.clip(weights, 1e-10, 1.0)
    sum_alpha = np.sum(alphas)
    log_norm = lgamma(sum_alpha) - np.sum([lgamma(a) for a in alphas])
    log_p = np.sum((alphas - 1) * np.log(weights))
    return log_norm + log_p

def dirichlet_entropy(alpha):
    sum_alpha = np.sum(alpha)
    k = len(alpha)
    return (
        gammaln(sum_alpha)
        - np.sum(gammaln(alpha))
        - (sum_alpha - k) * psi(sum_alpha)
        + np.sum((alpha - 1) * psi(alpha))
    )

def compute_gae(rewards, values, dones, next_value, gamma=0.99, lam=0.95):
    advantages = []
    gae = 0
    values = values + [next_value] #add final bootstrap value

    for t in reversed(range(len(rewards))):
        delta = rewards[t] + gamma * values[t + 1] * (1 - dones[t]) - values [t]
        gae = delta + gamma * lam * (1 - dones[t]) * gae
        advantages.insert(0, gae)

    returns = [adv + val for adv, val in zip(advantages, values[:-1])]
    return advantages, returns

def ppo_update(agent, buffer, clip_epsilon=0.2, epochs=5, lr_policy = 3e-4, lr_value = 1e-3, entropy_coef = 0.01):
    """
    Performs PPO updates on polciy and value networks
    """
    print(f"PPO Update Called: {len(buffer.states)} steps in buffer")
    states = np.array(buffer.states)
    actions = np.array(buffer.actions)
    rewards = np.array(buffer.rewards)
    values = list(buffer.values)
    log_prob_old = np.array(buffer.log_probs)
    dones = np.array(buffer.dones)

    #compute advantages and returns
    next_value = agent.value_net.predict_value(states[-1])
    advantages, returns = compute_gae(rewards, values, dones, next_value)
    advantages = np.array(advantages)
    returns = np.array(returns)

    advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

    batch_size = 32
    n = len(states)

    for epoch in range(epochs):
            idxs = np.random.permutation(n)

            epoch_loss_pi = 0
            epoch_loss_v = 0
            epoch_ratio = 0
            epoch_ent = 0
            n_batches = 0

            for start in range(0, n, batch_size):
                end = start + batch_size
                batch_idx = idxs[start:end]

                batch_loss_pi = 0
                batch_loss_v = 0
                batch_ratio = 0
                batch_ent = 0

                for i in batch_idx:
                    state = states[i]
                    action_taken = actions[i]
                    action_taken = np.clip(actions[i], 1e-10, 1.0)
                    action_taken = action_taken / np.sum(action_taken)

                    adv = advantages[i]
                    ret = returns[i]
                    old_log_prob = log_prob_old[i]

                    #policy forward pass
                    logits = agent.policy_net.forward(state)

                    # alpha = np.log1p(np.exp(logits)) + 1e-3
                    # alpha = np.clip(alpha, 1e-3, 20.0)
                    alpha = np.exp(np.clip(logits, -1, 1)) + 1.5
                    alpha = np.clip(alpha, 1.0, 5.0)

                    # action_taken = np.clip(action_taken, 1e-8, 1.0)
                    # action_taken /= np.sum(action_taken)

                    #new log prob of taken action (multivariate softmax)
                    log_prob = dirichlet.logpdf(action_taken, alpha)
                    ratio = np.exp(log_prob - old_log_prob)

                    #clip ratio
                    clipped_ratio = np.clip(ratio, 1 - clip_epsilon, 1 + clip_epsilon)
                    policy_loss = -np.minimum(ratio * adv, clipped_ratio * adv).mean()

                    #value forward pass
                    value_est = agent.value_net.predict_value(state)
                    value_loss = (value_est - ret) ** 2

                    #combine losses
                    entropy = dirichlet.entropy(alpha + 1e-3)
                    # entropy_coef = max(0.02 * (0.995 ** epoch), 0.001)
                    # total_loss = policy_loss + 0.5 * value_loss - entropy_coef * entropy #entropy maybe

                    if i % 10 == 0:
                        print(f"Epoch {epoch} Step {i} | Advantage: {adv:.5f}, Return: {ret:.5f}, "
                            f"Old log prob: {old_log_prob:.5f}, New log prob: {log_prob:.5f}, "
                            f"Ratio: {ratio:.5f}, Policy Loss: {policy_loss:.5f}, Value Loss: {value_loss:.5f}")

                    grad_policy(agent.policy_net, state, action_taken, adv, lr * 1.0)
                    grad_value(agent.value_net, state, ret, lr * 5.0)

                    loss_pi += policy_loss
                    loss_v += value_loss
                    mean_ratio += ratio
                    mean_ent += entropy
                mean_ratio /= len(batch_idx)
                mean_ent /= len(batch_idx)
                loss_pi /= len(batch_idx)
                loss_v /= len(batch_idx)
            if epoch % 1 == 0:
                print(f"Epoch {epoch} | π_loss {loss_pi:.4f} | V_loss {loss_v:.4f} | "
                    f"ratio {mean_ratio:.3f} | entropy {mean_ent:.3f}")

                #backpropagation (manual gradient descent)
                #we use finite differences for now (but it is very basic i know)
        #         for net, loss_grad_fn in [(agent.policy_net, grad_policy),
        #                                     (agent.value_net, grad_value)]:
        #             update_weights(agent.policy_net, state, action_taken, adv, lr, loss_grad_fn)
        #             update_weights(agent.value_net, state, ret, None, lr, grad_value)
        # # print("After  update w1[0][:3]:", agent.policy_net.w1[0][:3])
        # print("delta:", agent.policy_net.w1[0][:3] - before_w1)

def grad_policy(net, state, action_taken, advantage, lr=5e-5):
    """"
    Backprop for PPO policy net using softmax and advantage
    """

    x = np.array(state).reshape(1, -1)

    #forward pass
    z1 = x @ net.w1 + net.b1
    a1 = np.tanh(z1)
    z2 = a1 @ net.w2 + net.b2
    logits = z2.flatten()

    alpha = stable_softplus(logits) + 1e-3

    #gradient of log dirichlet wrt alpha
    grad_logpdf = (psi(np.sum(alpha)) - psi(alpha) + np.log(action_taken + 1e-8)) * advantage

    #chain rule back to logits (via softplus derivative)
    softplus_deriv = 1 /  (1 + np.exp(-logits))
    dlogits = grad_logpdf * softplus_deriv

    #backprop w2, b2
    dL_dw2 = a1.T @ dlogits.reshape(1, -1)
    dL_db2 = dlogits

    #backprop thru tanh
    da1_dz1 = 1 - np.tanh(z1)**2
    dz1 = (dlogits @ net.w2.T) * da1_dz1

    #backprop w1, b1
    dL_dw1 = x.T @ dz1
    dL_db1 = dz1[0]

    #print("Gradient norm (policy w1):", np.linalg.norm(dL_dw1))
    dL_dw1 = np.clip(dL_dw1, -1, 1)
    dL_dw2 = np.clip(dL_dw2, -1, 1)
    dL_db1 = np.clip(dL_db1, -1, 1)
    dL_db2 = np.clip(dL_db2, -1, 1)

    #gradient step
    net.w1 -= lr * dL_dw1
    net.b1 -= lr * dL_db1
    net.w2 -= lr * dL_dw2
    net.b2 -= lr * dL_db2

def grad_value(net, state, target_return, lr=5e-5):
    """"
    Backprop for value MLP: MSE loss between value estimate and return
    """
    x = np.array(state).reshape(1, -1)

    #forward pass
    z1 = x @ net.w1 + net.b1
    a1 = np.tanh(z1)
    z2 = a1 @ net.w2 + net.b2
    value_est = z2[0, 0] #scalar

    #Loss: (V - R)^2
    dloss_dz2 = 2 * (value_est - target_return)

    #backprop w2, b2
    dL_dw2 = a1.T * dloss_dz2
    dL_db2 = dloss_dz2

    #backprop a1 -> z1
    da1_dz1 = 1 - np.tanh(z1)**2
    dz1 = (dloss_dz2 * net.w2.T) * da1_dz1

    #backprop w1, b1
    dL_dw1 = x.T @ dz1
    dL_db1 = dz1[0]

    #print("Gradient norm (value w1):", np.linalg.norm(dL_dw1))

    dL_dw1 = np.clip(dL_dw1, -1, 1)
    dL_dw2 = np.clip(dL_dw2, -1, 1)
    dL_db1 = np.clip(dL_db1, -1, 1)
    dL_db2 = np.clip(dL_db2, -1, 1)


    #gradient step
    net.w1 -= lr * dL_dw1
    net.b1 -= lr * dL_db1
    net.w2 -= lr * dL_dw2
    net.b2 -= lr * dL_db2


# def update_weights(net, state, action, target, adv, lr, grad_fn):
#     """
#     update weights of net using approximated gradients
#     """
#     if grad_fn == grad_policy:
#         grad_policy(net, state, action, adv, lr)
#     elif grad_fn == grad_value:
#         grad_value(net, state, target, lr)
