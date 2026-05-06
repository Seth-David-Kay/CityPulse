//CONSTANTS: (generalize later)
#define STATE_SIZE 5 //size of observation/state vector
#define ACTION_COUNT 2 //number of discrete actions, left and right (increasing this to 4 for the grid, up and down)
#define MAX_EP_LEN 30 //cap episodes
#define BATCH_SIZE 10 //number of episodes before traning update
#define GAMMA 0.90f //discount factor
#define LEARNING_RATE 0.01f //pretty big to see movement early
#define ENTROPY_COEF 0.01f //avoid collapse
#define LAMBDA 0.95f
#define CLIP_EPS 0.20f
#define HIDDEN_SIZE 64
#define PPO_OBS_SIZE 5
#define ROLLOUT_CAPACITY (BATCH_SIZE * MAX_EP_LEN)

//controller timing and observation scaling
#define PPO_DECISION_INTERVAL 2.0f
#define PPO_STOP_SPEED_THRESHOLD 5.0f
#define PPO_QUEUE_NORMALIZER 15.0f

//hardcoded links for demo
#define PPO_VERTICAL_APPROACH_LINK_INDEX   1
#define PPO_HORIZONTAL_APPROACH_LINK_INDEX 3

//reward coefficents
#define REWARD_IMPROVEMENT_SCALE  1.0f
#define REWARD_QUEUE_SCALE        0.75f
#define REWARD_IMBALANCE_SCALE    0.50f
#define REWARD_SWITCH_SCALE      0.03f
#define REWARD_COLLISION_SCALE   1.00f

//rng utility
typedef struct { u64 s; } RNG;
static inline RNG rng_init(u64 seed){ //initialize RNG with a seed (0 → fixed constant)
    RNG r; //local rng
    r.s = seed ? seed : 0x9E3779B97F4A7C15ULL;  //default non-zero seed
    return r;  //return rng
}
static inline u32 rng_u32(RNG *r){ //return a radnom integer
    r->s ^= r->s<<13; //xorshift
    r->s ^= r->s>>7;
    r->s ^= r->s>>17;
    return (u32)r->s;
}
static inline f32 rng_unif01(RNG *r){ //uniform float in [0,1)
    return ((f32)rng_u32(r) / 4294967296.0f); //divide by 2^32
 } // [0,1)

//policy & value
//policy: logits = W_policy (ACTION_COUNT x STATE_SIZE) * state
//value: v_hat = W_value (STATE_SIZE * state)
//this decides WHICH action to take based on the state
//Input (25) -> Hidden (64) -> Output (4 Logits)
typedef struct {
    //weights and biases
    //input to hidden
    f32 W1[HIDDEN_SIZE * STATE_SIZE];
    f32 b1[HIDDEN_SIZE];

    //hidden to output
    f32 W2[ACTION_COUNT * HIDDEN_SIZE];
    f32 b2[ACTION_COUNT];
} PolicyNet;

//estimates HOW GOOD the action is (scalar value)
//Input (25) -> Hidden (64) -> Output (1 value)
typedef struct {
    //layer 1
    f32 W1[HIDDEN_SIZE * STATE_SIZE];
    f32 b1[HIDDEN_SIZE];

    //layer 2: hidden to value (1 output)
    f32 W2[1* HIDDEN_SIZE];
    f32 b2[1];
} ValueNet;

typedef struct {
    PolicyNet policy; //actor parameters
    ValueNet value; //critic parameters
} Agent;

//small init, dont need orthonoemal for logits now
static void agent_init(Agent *A, RNG *rng){
    //xavier initialization
    //set inital weights to small values for stability

    //scaling factors based on layer sizes
    f32 scale_p1 = sqrtf(6.0f /  (f32)(STATE_SIZE + HIDDEN_SIZE));
    f32 scale_p2 = sqrtf(6.0f /  (f32)(HIDDEN_SIZE + ACTION_COUNT));

    //policy init
    for(i32 i=0; i<HIDDEN_SIZE*STATE_SIZE; i++) A->policy.W1[i] = (rng_unif01(rng) - 0.5f) * 2.0f * scale_p1;
    for(i32 i=0; i<HIDDEN_SIZE; i++) A->policy.b1[i] = 0.0f;
    for(i32 i=0; i<ACTION_COUNT*HIDDEN_SIZE; i++) A->policy.W2[i] = (rng_unif01(rng) - 0.5f) * 2.0f * scale_p2;
    for(i32 i=0; i<ACTION_COUNT; i++) A->policy.b2[i] = 0.0f;

    //value init, same logic really
    for(i32 i=0; i<HIDDEN_SIZE*STATE_SIZE; i++) A->value.W1[i] = (rng_unif01(rng) - 0.5f) * 2.0f * scale_p1;
    for(i32 i=0; i<HIDDEN_SIZE; i++) A->value.b1[i] = 0.0f;
    for(i32 i=0; i<HIDDEN_SIZE; i++) A->value.W2[i] = (rng_unif01(rng) - 0.5f) * 0.1f;
    A->value.b2[0] = 0.0f;
}

//hyperbolic tanget
//wrapper
//normalizes output between -1 and 1 for non linear patterns
static inline f32 tanhf_poly(f32 x) { return tanhf(x); }

//temporary buffer for ONE agent's episode
//we compute GAE here before flushing to the main Rollout
typedef struct {
    i32 count;
    f32 states[MAX_EP_LEN][STATE_SIZE];
    i32 actions[MAX_EP_LEN];
    f32 logp[MAX_EP_LEN];
    f32 rewards[MAX_EP_LEN];
    f32 values[MAX_EP_LEN];
    f32 entropy[MAX_EP_LEN];
} AgentTraj;

//forward: logits[a] = sum_j W[a,j] * s[j]
//runs the policy network
//we input the state vector
//we output the logits which are the raw scores for actions and the hidden state which is going to be used for backprop
static inline void policy_forward(const Agent *A, const f32 state[STATE_SIZE], f32 out_logits[ACTION_COUNT], f32 out_hidden[HIDDEN_SIZE]){
    //layer 1, linear transform + tanh activation
    //hidden = tanh(W1 * STATE + b1)
    for (i32 i = 0; i < HIDDEN_SIZE; ++i){
        f32 sum = A->policy.b1[i];
        for (i32 j = 0; j < STATE_SIZE; ++j) {
            sum += A->policy.W1[i * STATE_SIZE + j] * state[j];
        }
        out_hidden[i] = tanhf_poly(sum);
    }

    //layer 2, linear transformation to output
    //hidden = W2 * hidden + b2
    for (i32 a=0;a<ACTION_COUNT;++a){ //loop indicies for each action
        f32 sum = A->policy.b2[a]; //accumulator
        for (i32 i=0;i<HIDDEN_SIZE;++i) {
            sum += A->policy.W2[a*HIDDEN_SIZE + i] * out_hidden[i]; //dot product
        }
        out_logits[a] = sum; //store logit
    }
}

//softmax + log-softmax, log-sum-exp for two elements
static inline f32 logsumexp(const f32 *nums, i32 count){
    f32 m = nums[0];
    for (i32 i = 1; i < count; i++) if (nums[i] > m) m = nums[i];
    f32 sum = 0.f;
    for (i32 i = 0; i < count; i++) sum +=expf(nums[i] - m);
    return m + logf(sum);
}

static inline void softmax_log(const f32 *logits, i32 count, f32 *out_prob, f32 *out_logprob){
    f32 lse = logsumexp(logits, count);
    for (i32 i=0; i<count; ++i){
        out_logprob[i] = logits[i] - lse;
        out_prob[i] = expf(out_logprob[i]);
    }
}


//sample from categorical({p0,p1}) and return action + logpi(a|s) + entropy
//samples and action based on probabilities
//also returns entropy of distribution
static inline void policy_sample(const Agent *A, RNG *rng, const f32 state[STATE_SIZE], i32 *out_action, f32 *out_logprob, f32 *out_entropy){
    f32 logits[ACTION_COUNT] = {0};  //logits buffer
    f32 hidden[HIDDEN_SIZE]; //dummy
    policy_forward(A, state, logits, hidden); //compute logits

    f32 p[ACTION_COUNT], lp[ACTION_COUNT]; //probs and logprobs
    softmax_log(logits, ACTION_COUNT, p , lp); //conver to probs and log probs

    //sample u∈[0,1)
    //random sampling: pick action based on cdf
    f32 u = rng_unif01(rng);
    f32 cdf = 0.f;
    i32 a = ACTION_COUNT - 1;

    for(i32 i=0; i<ACTION_COUNT; ++i){
        cdf +=p[i];
        if (u < cdf){ a=i; break; }
    }

    *out_action = a;
    *out_logprob = lp[a];
    //entropy H = -Σ p log p
    //implementing randomness
    f32 H = 0.f;
    for(i32 i=0; i<ACTION_COUNT; ++i) H -= p[i]*lp[i];
    *out_entropy = H;
}

//critic forward: vhat = W*s
//runs the value network
//input the state vector
//output the value which would be the prediction for future reward
static inline f32 value_forward(const Agent *A, const f32 state[STATE_SIZE]){
    f32 hidden[HIDDEN_SIZE];
    //layer 1
    for (i32 i = 0; i < HIDDEN_SIZE; ++i){
        f32 sum = A->value.b1[i];
        for (i32 j = 0; j < STATE_SIZE; ++j) {
            sum += A->value.W1[i * STATE_SIZE + j] * state[j];
        }
        hidden[i] = tanhf_poly(sum);
    }

    //layer 2, output a vector
    f32 sum = A->value.b2[0]; //accumulator
    for (i32 i=0;i<HIDDEN_SIZE;++i) {
        sum += A->value.W2[i] * hidden[i];
    }
    return sum;
}

//rollout buffer
//we store 1 batch = BATCH EPISDOES * <= MAX_EP_LEN trajectories

typedef struct {
    //storage
    f32 states[ROLLOUT_CAPACITY * STATE_SIZE];      // [N * STATE_SIZE] — flattened rows of one-hot states
    i32 actions[ROLLOUT_CAPACITY];     // [N]              — action indices 0/1
    f32 logp[ROLLOUT_CAPACITY];        // [N]              — log π(a|s) at sampling time
    f32 rewards[ROLLOUT_CAPACITY];     // [N]              — rewards
    f32 values[ROLLOUT_CAPACITY];      // [N]              — V(s) at sampling time
    f32 entropy[ROLLOUT_CAPACITY];     // [N]              — entropy at sampling (for logging)
    f32 returns[ROLLOUT_CAPACITY];     // [N]              — discounted returns (for now)
    f32 advantages[ROLLOUT_CAPACITY];  // [N]              — returns - values (baseline)
    bool dones[ROLLOUT_CAPACITY];

    i32  ep_len[BATCH_SIZE];     // [BATCH_EPISODES] — per-episode lengths for summary
    f32  ep_ret[BATCH_SIZE];     // [BATCH_EPISODES] — per-episode returns for summary

    i32 N;            // number of trajectories actually stored this batch
} Rollout;

//collect - run batch episodes, pack trajectories in R
//compute simple discounted returns for now; advantages = returns-values
//GAE comes later hopefully

//refactor this to include [MAX_AGENTS]

// static void collect_batch(GridWorld *env, Agent *agent, RNG *rng, Rollout *R){
//     R->N = 0; //reset trajectories

//     //AgrentTraj trajs[MAX_AGENTS];

//     for (i32 e=0;e<BATCH_SIZE;++e){ //loop over episofes in batch
//         //(re)start episode
//         grid_reset(env, rng_u32(rng));
//         f32 ep_return = 0.f;
//         i32 ep_len = 0;

//         //local episode to compute returns per epiosde
//         i32 start_idx = R->N;

//         for(;;){ //step until done or cap
//             //state
//             f32 s[STATE_SIZE];
//             grid_obs(env, s);

//             //sample action + record logpi(a|s), entropy, V(s)
//             i32 a;
//             f32 lp, H;
//             policy_sample(agent, rng, s, &a, &lp, &H);
//             f32 v = value_forward(agent, s);

//             //step
//             f32 r;
//             bool done = false;
//             grid_step(env, a, &r, &done);

//             //write into R
//             i32 k = R->N++;
//             assert(R->N <= ROLLOUT_CAPACITY);
//             //pack state row
//             for (i32 j=0;j<STATE_SIZE;++j) R->states[k*STATE_SIZE + j] = s[j];
//             R->actions[k] = a;
//             R->logp[k]    = lp;
//             R->rewards[k] = r;
//             R->values[k]  = v;
//             R->entropy[k] = H;

//             ep_return += r;
//             ep_len    += 1;
//             if (done != 0 || ep_len >= MAX_EP_LEN) break;
//         }

//         //compute discounted returns
//         //last value after episode boundary is 0 (terminal)
//         f32 G = 0.f; //return accumulator
//         f32 gae = 0.f;
//         for (i32 k = R->N-1; k >= start_idx; --k){
//             f32 v = R->values[k];
//             f32 vnext = (k == R->N - 1) ? 0.f : R->values[k+1];
//             f32 delta = R->rewards[k] + GAMMA * vnext - v;

//             gae = delta + GAMMA * LAMBDA * gae;
//             R->advantages[k] = gae;

//             G = R->rewards[k] + GAMMA * G;
//             R->returns[k] = G;
//         }

// //         if (e == 0) {  //first episode of first batch
// //             print("Episode 0 advantages: ");
// //             for (i32 k = start_idx; k < R->N; ++k) {
// //                 print("% ", fmt(f32, R->advantages[k]));
// //             }
// //             print("\n");
// // }

//         //defunct; this is was prev advantage computation for vanilla policy gradident
//         //baseline advantage = return - value
//         // for (i32 k = start_idx; k < R->N; ++k){
//         //     R->advantages[k] = R->returns[k] - R->values[k];
//         // }

//         R->ep_ret[e] = ep_return;
//         R->ep_len[e] = ep_len;
//     }
// }

//vanilla policy gradient + value regression
//Loss = -E[ logπ(a|s) * A ] + c_v * MSE(V - return) - c_H * entropy
static void train_step(Agent *A, Rollout *R, f32 lr){
    i32 N = R->N;

    // 1) Normalize advantages
    f32 meanA = 0.f, varA = 0.f;
    for (i32 i=0;i<N;++i) meanA += R->advantages[i];
    meanA /= (f32)N;
    for (i32 i=0;i<N;++i){ f32 d = R->advantages[i]-meanA; varA += d*d; }
    varA /= (f32)N;
    f32 invStdA = 1.0f / (sqrtf(varA) + 1e-8f);

    //ppo epochs: run over the same batch multiple times
    for(i32 epoch = 0; epoch < 15; ++epoch){

        // 2) Accumulate grads for policy W and value W
        //    Policy: ∇W = - (A_norm) * (∂ logπ / ∂W)
        //    For softmax with 2 actions, ∂ logπ(a) / ∂W[a,:] = (1 - π(a)) * s
        //    and for the other action k≠a: ∂ logπ(a) / ∂W[k,:] = -π(k) * s
        f32 gp_W1[HIDDEN_SIZE * STATE_SIZE] = {0};
        f32 gp_b1[HIDDEN_SIZE] = {0};
        f32 gp_W2[ACTION_COUNT * HIDDEN_SIZE] = {0};
        f32 gp_b2[ACTION_COUNT] = {0};

        f32 gv_W1[HIDDEN_SIZE * STATE_SIZE] = {0};
        f32 gv_b1[HIDDEN_SIZE] = {0};
        f32 gv_W2[1* HIDDEN_SIZE] = {0};
        f32 gv_b2[1] = {0};

        for (i32 i=0;i<N;++i){
            const f32 *s = &R->states[i*STATE_SIZE];
            i32 a = R->actions[i];

            // normalized advantage
            f32 An = (R->advantages[i] - meanA) * invStdA;
            //f32 An = R->advantages[i];

            // // policy gradient part (vanilla)
            // // loss_pg_i = - logπ(a|s) * An  - ENTROPY_COEF * H
            // // H grad w.r.t logits gives -(∂H/∂W)
            // total_loss_pg += -lp[a] * An;

            // // gradient of -logπ(a|s)*An w.r.t W:
            // // For each action k: coeff = ( (k==a) - p[k] ) * (-An)
            // // Derivation:
            // //   ∂ logπ(a) / ∂z_k = 1_{k=a} - π_k
            // //   ∂ z_k / ∂W[k,:] = s[:]
            // //   So ∂( -logπ(a)*An )/∂W[k,:] = -(1_{k=a} - π_k)*An * s[:]
            // for (i32 k=0;k<ACTION_COUNT;++k){
            //     f32 coeff = -((k == a ? 1.f : 0.f) - p[k]) * An; // minus sign here
            //     for (i32 j=0;j<STATE_SIZE;++j){
            //         gW_policy[k*STATE_SIZE + j] += coeff * s[j];
            //     }
            // }

            // ---POLICY BACKPROP---
            f32 logits[ACTION_COUNT];
            f32 hidden[HIDDEN_SIZE];

            policy_forward(A, s, logits, hidden);

            f32 p[ACTION_COUNT], lp[ACTION_COUNT];
            softmax_log(logits, ACTION_COUNT, p, lp);

            //ppo clipping logic
            f32 logp_new = lp[a];
            f32 logp_old = R->logp[i]; //stored at rollout time
            f32 ratio = expf(logp_new - logp_old); //r_t

            f32 unclipped = ratio * An;
            f32 clipped_val = CLAMP(ratio, 1.f - CLIP_EPS, 1.f + CLIP_EPS);
            i32 use_unclipped = (unclipped <= clipped_val * An);
            f32 g_scale = use_unclipped ? -(ratio * An) : 0.f; //negative since we minimize loss

            // ----- policy gradient for PPO-Clip -----
            //We need ∂ l_clip / ∂ logits. A simple unbiased estimator is to
            //take the branch actually used by min(·) and differentiate that branch.
            //d(-r*A)/d logits = -(r)*A * d(logπ_new)/d logits
            //where d logπ_new / d logits_k = (1_{k=a} - p[k])
            //(add entropy grad later.)

            //gradient of chosen branch:
            //d( -min(r*A, clip(r)*A) )/d logits = -(branch_coeff * A) * d logπ / d logits
            //where d logπ_k / d z_j = (1_{j=k} - p_j)
            // if (i == 0 && epoch == 0) {
            //     print("DEBUG: ratio=% unclipped=% clipped=% use=%\n",
            //         fmt(f32, ratio), fmt(f32, unclipped), fmt(f32, clipped), fmt(i32, use_unclipped));
            // }



            //f32 branch_coeff = use_unclipped ? ratio : clamp(ratio, 1.f - CLIP_EPS, 1.f + CLIP_EPS);
            //if !use_unclipped, gscale remains 0.f (stop gradient)

            //entropy (H = -Σ p log p)
            f32 H = -(p[0]*lp[0] + p[1]*lp[1]);

            //backprop through output layer (W2, b2)
            f32 d_logits[ACTION_COUNT];
            f32 d_hidden[HIDDEN_SIZE] = {0};

            //accumulate ∇W via chain rule z_k = W_k·s
            for (i32 k=0;k<ACTION_COUNT;++k){
                //1. PPO Gradient: -(ratio * An) * dlogp/dlogits
                //dlogp[a] / dlogits[k] = (delta_ak - p[k])
                f32 dlogp_dzk = ((k==a) ? 1.f : 0.f) - p[k];
                f32 grad_ppo = g_scale * dlogp_dzk;

                //2. Entropy Gradient: -ENTROPY_COEF * dH/dlogits
                //dH/dz_k = p_k * (-log p_k - 1 - H + 1) ... simplification:
                //dH/dz_k = p_k * (-log p_k - H)
                //we want to MINIMIZE (-H), so we use gradient of (-H)
                //grad(-H) = -1 * [ p_k * (-log p_k - H) ]
                f32 grad_ent = -1.0f * (p[k] * (-lp[k] - H));

                //combine
                d_logits[k] = grad_ppo + (ENTROPY_COEF * grad_ent);
                gp_b2[k] += d_logits[k];
                for (i32 h=0;h<HIDDEN_SIZE;h++){
                    gp_W2[k*HIDDEN_SIZE + h] += d_logits[k] * hidden[h];

                    //propagate error back to hidden layer
                    //dHidden+=dLogit*Weight
                    d_hidden[h] += d_logits[k] * A->policy.W2[k*HIDDEN_SIZE + h];
                }
            }


            //backprop through hidden layer (W1, b1)
            f32 d_z1[HIDDEN_SIZE];
            for (i32 h=0; h<HIDDEN_SIZE; h++) {
                //derivative of tanh is (1-tanh^2)
                d_z1[h] = d_hidden[h] * (1.0f - hidden[h] * hidden[h]);
            }

            for (i32 h=0; h<HIDDEN_SIZE; h++) {
                gp_b1[h] += d_z1[h];
                for (i32 j=0; j<STATE_SIZE; j++) {
                    gp_W1[h*STATE_SIZE + j] += d_z1[h] * s[j];
                }
            }


            //value backpropagation
            //same logic as policy pretty much, but for mean squared error loss
            f32 v_hidden[HIDDEN_SIZE];
            //rerun value forward pass
            for (i32 h = 0; h < HIDDEN_SIZE; h++){
                f32 sum = A->value.b1[h];
                for (i32 j = 0; j < STATE_SIZE; j++) {
                    sum += A->value.W1[h * STATE_SIZE + j] * s[j];
                }
                v_hidden[h] = tanhf_poly(sum);
            }
            f32 v = A->value.b2[0];
            for (i32 h = 0; h < HIDDEN_SIZE; h++) v += A->value.W2[h] * v_hidden[h];

            //MSE gradient (prediction-target)
            f32 td = v - R->returns[i];
            f32 d_v_out = 0.5f * 2.0f * td;

            //backprop value layer 2
            gv_b2[0] += d_v_out;
            f32 d_v_hidden[HIDDEN_SIZE] = {0};
            for (i32 h=0; h<HIDDEN_SIZE; h++) {
                gv_W2[h] += d_v_out * v_hidden[h];
                d_v_hidden[h] += d_v_out * A->value.W2[h];
            }

            //backprop value layer 1
            f32 d_v_z1[HIDDEN_SIZE];
            for (i32 h=0; h<HIDDEN_SIZE; h++){
                d_v_z1[h] = d_v_hidden[h] * (1.0f - v_hidden[h] * v_hidden[h]);
            }

            for (i32 h=0; h<HIDDEN_SIZE; h++){
                gv_b1[h] += d_v_z1[h];
                for (i32 j=0; j<STATE_SIZE; j++) {
                    gv_W1[h*STATE_SIZE + j] += d_v_z1[h] * s[j];
                }
            }

        }

        // 3) Apply SGD step: W ← W - lr * grad  (policy & value)
        //policy network update
        for (i32 k=0;k<ACTION_COUNT*HIDDEN_SIZE;k++) A->policy.W2[k] -= lr * (gp_W2[k] / (f32)N);
        for (i32 k=0;k<ACTION_COUNT;k++)             A->policy.b2[k]  -= lr * (gp_b2[k]  / (f32)N);
        for (i32 k=0;k<HIDDEN_SIZE*STATE_SIZE;k++) A->policy.W1[k] -= lr * (gp_W1[k] / (f32)N);
        for (i32 k=0;k<HIDDEN_SIZE;k++)             A->policy.b1[k]  -= lr * (gp_b1[k]  / (f32)N);

        //value network update
        for (i32 k=0; k<1*HIDDEN_SIZE;k++)          A->value.W2[k] -= lr * (gv_W2[k] / (f32)N);
        A->value.b2[0]                              -= lr * (gv_b2[0] / (f32)N);
        for (i32 k=0; k<HIDDEN_SIZE*STATE_SIZE;k++) A->value.W1[k] -= lr * (gv_W1[k] / (f32)N);
        for (i32 k=0; k<HIDDEN_SIZE;k++)          A->value.b1[k] -= lr * (gv_b1[k] / (f32)N);

    }
}

//the "Receipt" the environment holds onto during the K-step delay.
typedef struct {
    f32 obs[PPO_OBS_SIZE];
    i32 action;
    f32 log_prob;
    f32 value_est;
} PPO_Receipt;

//the main PPO Context
typedef struct {
    Agent agent; //(Policy + Value nets)
    Rollout buffer;
    RNG rng;
    //internal state for updates
    f32 decision_timer;

    //track how long current light phase has been active
    f32 phase_elapsed;

    bool waiting_for_reward;
    PPO_Receipt current_receipt;
    i32 last_action;
    i32 current_phase;

    //define ppo episode boundaries
    i32 steps_in_episode;
    i32 completed_episodes_in_batch;
    f32 running_episode_return;

    //store which two links this controller actually manages
    //4-way intersection uses two NS links and two EW links
    Link_Id north_approaches[2];
    Link_Id south_approaches[2];
    Link_Id east_approaches[2];
    Link_Id west_approaches[2];

    i32 prev_north_load;
    i32 prev_south_load;
    i32 prev_east_load;
    i32 prev_west_load;

    u64 prev_collision_count;

    f32 rolling_avg_reward;
} PPO_Context;

static inline Link_Id ppo_invalid_link(void) {
    Link_Id id = { (u32)-1 };
    return id;
}

static inline bool ppo_link_is_valid(Link_Id id) {
    return id.i != (u32)-1;
}

//simple switching penalty helper
static inline i32 ppo_switch_penalty(i32 new_action, i32 old_action) {
    return (new_action != old_action) ? 1 : 0;
}

//cars move on node paths, so make sure we only read current link if path has 2 nodes
static inline bool car_has_current_link(Car *c) {
    return c->path.count >= 2;
}

//this fixes the node/link mismatch
//car paths are nodes, but traffic lights control links
static inline Link_Id car_current_link(Graph *graph, Car *c) {
    if (!car_has_current_link(c)) return ppo_invalid_link();
    return link_between_nodes(*graph, c->path.data[0], c->path.data[1]);
}

//helper for "queued/stopped" cars
static inline bool is_stopped_car(Car *c) {
    return c->speed < PPO_STOP_SPEED_THRESHOLD;
}

static inline bool link_in_pair(Link_Id current, Link_Id pair[2]) {
    return current.i == pair[0].i || current.i == pair[1].i;
}

//centralized light flag setter
static void set_link_light(Graph *graph, Link_Id id, bool is_green) {
    if (!ppo_link_is_valid(id)) return;
    if (id.i >= graph->links.count) return;

    if (graph->links.data[id.i].to.i == 5 || graph->links.data[id.i].to.i == 7) {
        graph->links.data[id.i].flags &= ~LINK_FLAG_ANY_TRAFFIC_LIGHT;
        return;
    }

    if (is_green) {
        graph->links.data[id.i].flags |= Link_Flag_TRAFFIC_LIGHT_GREEN;
        graph->links.data[id.i].flags &= ~Link_Flag_TRAFFIC_LIGHT_RED;
    } else {
        graph->links.data[id.i].flags |= Link_Flag_TRAFFIC_LIGHT_RED;
        graph->links.data[id.i].flags &= ~Link_Flag_TRAFFIC_LIGHT_GREEN;
    }
}

static void graph_clear_lights_to_nodes(Graph *graph, u32 a, u32 b) {
    for (Link_Iterator it = link_iterator(graph->links.data, graph->links.count); link_iterate(&it);) {
        Link *link = it.link;

        if (link->to.i == a || link->to.i == b) {
            link->flags &= ~LINK_FLAG_ANY_TRAFFIC_LIGHT;
        }
    }
}

//apply PPO action to graph light flags
static void traffic_apply_action(Graph *graph, PPO_Context *ctx, i32 action) {
    if (action == 0) {
        // print("[LIGHTS] NS GREEN | N=(%u,%u) S=(%u,%u) | E=(%u,%u) W=(%u,%u)\n",
        //     ctx->north_approaches[0].i, ctx->north_approaches[1].i,
        //     ctx->south_approaches[0].i, ctx->south_approaches[1].i,
        //     ctx->east_approaches[0].i,  ctx->east_approaches[1].i,
        //     ctx->west_approaches[0].i,  ctx->west_approaches[1].i);

        set_link_light(graph, ctx->north_approaches[0], true);
        set_link_light(graph, ctx->north_approaches[1], true);
        set_link_light(graph, ctx->south_approaches[0], true);
        set_link_light(graph, ctx->south_approaches[1], true);

        set_link_light(graph, ctx->east_approaches[0], false);
        set_link_light(graph, ctx->east_approaches[1], false);
        set_link_light(graph, ctx->west_approaches[0], false);
        set_link_light(graph, ctx->west_approaches[1], false);
    } else {
        // print("[LIGHTS] EW GREEN | N=(%u,%u) S=(%u,%u) | E=(%u,%u) W=(%u,%u)\n",
        //     ctx->north_approaches[0].i, ctx->north_approaches[1].i,
        //     ctx->south_approaches[0].i, ctx->south_approaches[1].i,
        //     ctx->east_approaches[0].i,  ctx->east_approaches[1].i,
        //     ctx->west_approaches[0].i,  ctx->west_approaches[1].i);

        set_link_light(graph, ctx->east_approaches[0], true);
        set_link_light(graph, ctx->east_approaches[1], true);
        set_link_light(graph, ctx->west_approaches[0], true);
        set_link_light(graph, ctx->west_approaches[1], true);

        set_link_light(graph, ctx->north_approaches[0], false);
        set_link_light(graph, ctx->north_approaches[1], false);
        set_link_light(graph, ctx->south_approaches[0], false);
        set_link_light(graph, ctx->south_approaches[1], false);
    }

    ctx->current_phase = action;
    ctx->phase_elapsed = 0.0f;
}

//one place to measure queues/waiting on the controlled approaches
static void traffic_measure_local(
    Graph *graph,
    Simulation_State *sim,
    PPO_Context *ctx,
    i32 *out_north,
    i32 *out_south,
    i32 *out_east,
    i32 *out_west,
    i32 *out_waiting_total
) {
    i32 north = 0;
    i32 south = 0;
    i32 east = 0;
    i32 west = 0;
    i32 waiting_total = 0;

    Simulation_Car_Iterator it = simulation_car_iterator(sim);
    while (simulation_car_next(&it)) {
        Car *c = it.car;
        if (!car_has_current_link(c)) continue;

        Link_Id current = car_current_link(graph, c);
        if (!ppo_link_is_valid(current)) continue;

        if (link_in_pair(current, ctx->north_approaches)) {
            north += 1;
            if (is_stopped_car(c)) waiting_total += 1;
        }

        if (link_in_pair(current, ctx->south_approaches)) {
            south += 1;
            if (is_stopped_car(c)) waiting_total += 1;
        }

        if (link_in_pair(current, ctx->east_approaches)) {
            east += 1;
            if (is_stopped_car(c)) waiting_total += 1;
        }

        if (link_in_pair(current, ctx->west_approaches)) {
            west += 1;
            if (is_stopped_car(c)) waiting_total += 1;
        }
    }

    *out_north = north;
    *out_south = south;
    *out_east = east;
    *out_west = west;
    *out_waiting_total = waiting_total;
}

//observation extractor
// obs[0] = vertical queue
// obs[1] = horizontal queue
// obs[2] = current phase
static void traffic_get_obs(
    Graph *graph,
    Simulation_State *sim,
    PPO_Context *ctx,
    f32 out_obs[STATE_SIZE]
) {
    i32 north = 0, south = 0, east = 0, west = 0;
    i32 waiting_total = 0;

    traffic_measure_local(graph, sim, ctx, &north, &south, &east, &west, &waiting_total);
    (void)waiting_total;

    out_obs[0] = (f32)north / PPO_QUEUE_NORMALIZER;
    out_obs[1] = (f32)south / PPO_QUEUE_NORMALIZER;
    out_obs[2] = (f32)east  / PPO_QUEUE_NORMALIZER;
    out_obs[3] = (f32)west  / PPO_QUEUE_NORMALIZER;
    out_obs[4] = CLAMP(ctx->phase_elapsed / PPO_DECISION_INTERVAL, 0.0f, 1.0f);
}

//reward function
static f32 traffic_compute_reward(Graph *graph, Simulation_State *sim, PPO_Context *ctx) {
    i32 north = 0, south = 0, east = 0, west = 0;
    i32 waiting_total = 0;

    traffic_measure_local(graph, sim, ctx, &north, &south, &east, &west, &waiting_total);

    i32 ns_load = north + south;
    i32 ew_load = east + west;

    i32 current_total = ns_load + ew_load;
    i32 prev_total =
        ctx->prev_north_load +
        ctx->prev_south_load +
        ctx->prev_east_load +
        ctx->prev_west_load;

    i32 improvement = prev_total - current_total;

    i32 imbalance = ns_load - ew_load;
    if (imbalance < 0) imbalance = -imbalance;

    i32 switched = ppo_switch_penalty(ctx->current_receipt.action, ctx->last_action);

    //we only count collisions that happen on this specfic intersection
    u64 local_collisions_now = 0;
    for (u64 i = 0; i < sim->collision_events.count; i++) {
        Collision_Log event = sim->collision_events.data[i];

        Link_Id collision_link = link_between_nodes(*graph, event.a_from, event.a_to);

        //if crash happened on one of this intersection's approach links
        if (link_in_pair(collision_link, ctx->north_approaches) ||
            link_in_pair(collision_link, ctx->south_approaches) ||
            link_in_pair(collision_link, ctx->east_approaches)  ||
            link_in_pair(collision_link, ctx->west_approaches)) {
            local_collisions_now++;
        }
    }

    i32 collision_delta = (i32)(local_collisions_now - ctx->prev_collision_count); //compare local collisions against personal history

    f32 reward =
        REWARD_IMPROVEMENT_SCALE * (f32)improvement
        - 0.25f * (f32)current_total
        - 0.30f * (f32)waiting_total
        - 0.05f * (f32)imbalance
        - 1.00f * (f32)switched
        - REWARD_COLLISION_SCALE * (f32)collision_delta;

    //for presentation

    reward = CLAMP(reward, -10.0f, 10.0f);

    //update queue memory for next interval
    ctx->prev_north_load = north;
    ctx->prev_south_load = south;
    ctx->prev_east_load = east;
    ctx->prev_west_load = west;
    ctx->prev_collision_count = local_collisions_now;

    print("[DEBUG REWARD] N=%d S=%d E=%d W=%d wait=%d improve=%d imbalance=%d switched=%d collisions=%d reward=%f\n",
        north, south, east, west, waiting_total, improvement, imbalance, switched, collision_delta, reward);

    return reward;
}

typedef struct {
    Link_Id north_approaches[2];
    Link_Id south_approaches[2];
    Link_Id east_approaches[2];
    Link_Id west_approaches[2];
} Intersection_Config;

static Intersection_Config ppo_auto_config(Graph *graph, u8 target_intersection_id) {
    Intersection_Config config;

    //fill up with placeholder invalid links
    for (int i = 0; i < 2; i++) {
        config.north_approaches[i].i = (u32)-1;
        config.south_approaches[i].i = (u32)-1;
        config.east_approaches[i].i = (u32)-1;
        config.west_approaches[i].i = (u32)-1;
    }

    int n = 0, s = 0, e = 0, w = 0;

	for (Link_Iterator it = link_iterator(graph->links.data, graph->links.count); link_iterate(&it);) {
        Link *link = it.link;
        u32 i = (u32)it.i;

        if (link->to.i == 5 || link->to.i == 7) {
            link->flags &= ~LINK_FLAG_ANY_TRAFFIC_LIGHT;
        }

        if (link->intersection_id == target_intersection_id) {

            if (link->to.i == 5 || link->to.i == 7) {
                continue;
            }

            Node from = graph->nodes.data[link->from.i];
            Node to = graph->nodes.data[link->to.i];

            //calculate direction vector
            f32 dx = to.position.x - from.position.x;
            f32 dy = to.position.y - from.position.y;

            //if y diff is bigger, its a vertical road
            if (fabsf(dy) > fabsf(dx)) {
                if (dy > 0 && n < 2) config.north_approaches[n++] = (Link_Id){i};      //traveling down (+Y)
                else if (dy < 0 && s < 2) config.south_approaches[s++] = (Link_Id){i}; //traveling up (-Y)
            }
            //if x diff is bigger, its a horizontal road
            else {
                if (dx > 0 && w < 2) config.west_approaches[w++] = (Link_Id){i};       //traveling right (+X)
                else if (dx < 0 && e < 2) config.east_approaches[e++] = (Link_Id){i};  //traveling left (-X)
            }
        }
    }
    print("---------------------------------------------------------\n");
    print("[INIT] Intersection %d Auto-Detected Links:\n", target_intersection_id);
    print("  North approaches: %u, %u\n", config.north_approaches[0].i, config.north_approaches[1].i);
    print("  South approaches: %u, %u\n", config.south_approaches[0].i, config.south_approaches[1].i);
    print("  East approaches:  %u, %u\n", config.east_approaches[0].i, config.east_approaches[1].i);
    print("  West approaches:  %u, %u\n", config.west_approaches[0].i, config.west_approaches[1].i);
    print("---------------------------------------------------------\n");
    return config;
}

static void ppo_update_intersection_config(PPO_Context *ppo, Intersection_Config config) {
    ppo->north_approaches[0] = config.north_approaches[0];
    ppo->north_approaches[1] = config.north_approaches[1];

    ppo->south_approaches[0] = config.south_approaches[0];
    ppo->south_approaches[1] = config.south_approaches[1];

    ppo->west_approaches[0] = config.west_approaches[0];
    ppo->west_approaches[1] = config.west_approaches[1];

    ppo->east_approaches[0] = config.east_approaches[0];
    ppo->east_approaches[1] = config.east_approaches[1];
}

static PPO_Context* ppo_make(Arena *arena, u64 seed, Intersection_Config config) {
    PPO_Context* ctx = arena_make(arena, 1, PPO_Context);

    ctx->rng = rng_init(seed);

    //Init Agent (Reusing agent_init logic),  for one specfic intersection
    agent_init(&ctx->agent, &ctx->rng);

    //Init Rollout Buffer
    ctx->buffer.N = 0;

    //initalize controller state
    ctx->decision_timer = PPO_DECISION_INTERVAL; //start ready to trigger
    ctx->phase_elapsed = 0.0f;
    ctx->waiting_for_reward = false;
    ctx->last_action=0;
    ctx->current_phase=0;

    //PPO episode tracking
    ctx->steps_in_episode = 0;
    ctx->completed_episodes_in_batch = 0;
    ctx->running_episode_return = 0.0f;

    //store the controlled links inside the context
    ppo_update_intersection_config(ctx, config);

    ctx->prev_north_load = 0;
    ctx->prev_south_load = 0;
    ctx->prev_east_load = 0;
    ctx->prev_west_load = 0;

    ctx->prev_collision_count = 0; //personal collision tracker

    ctx->rolling_avg_reward = 0.0f;

    return ctx;
}


//called by Env. Takes an observation, returns an action, fills the receipt.
static i32 ppo_get_action(PPO_Context* ctx, f32* obs_data, PPO_Receipt* out_receipt) {
    i32 action;
    f32 log_prob, entropy;

    //functions from ppo.c
    policy_sample(&ctx->agent, &ctx->rng, obs_data, &action, &log_prob, &entropy);

    f32 value = value_forward(&ctx->agent, obs_data);

    //fill the receipt (Save state for later)
    for(i32 i=0; i<PPO_OBS_SIZE; i++) out_receipt->obs[i] = obs_data[i];
    out_receipt->action    = action;
    out_receipt->log_prob  = log_prob;
    out_receipt->value_est = value;

    return action; //Return 0 or 1 (NS or EW)
}

//called by Env. Takes the OLD receipt and the NEW reward.
//stores it in the buffer.
static void ppo_commit(PPO_Context* ctx, PPO_Receipt* receipt, f32 reward, bool done) {
    Rollout* R = &ctx->buffer;

    //If buffer full, drop data (or auto-train, see below)
    if (R->N >= ROLLOUT_CAPACITY) return;

    //push to  EXISTING Rollout structure
    i32 k = R->N++;

    //Copy Obs
    for (i32 j=0; j<STATE_SIZE; ++j) R->states[k*STATE_SIZE + j] = receipt->obs[j];

    R->actions[k] = receipt->action;
    R->logp[k]    = receipt->log_prob;
    R->values[k]  = receipt->value_est;
    R->rewards[k] = reward;
    //R->entropy[k] = ...

    R->dones[k] = done; //prevents AI from thinking ep1's crash caused ep2 success

    ctx->running_episode_return += reward; //track episode return for stats
    //store stats
    if (done) {
        i32 e = ctx->completed_episodes_in_batch;
        if (e < BATCH_SIZE) {
            R->ep_len[e] = ctx->steps_in_episode;
            R->ep_ret[e] = ctx->running_episode_return;
        }
        ctx->completed_episodes_in_batch += 1;
        ctx->running_episode_return = 0.0f;
    }
}


//returns true if trained
static bool ppo_check_and_train(PPO_Context* ctx) {
    if (ctx->buffer.N < ROLLOUT_CAPACITY) return false;

    //compute Advantages / GAE
    //reuse logic from 'collect_batch', but standalone.
    Rollout* R = &ctx->buffer;

    f32 last_gae = 0.f;

    for (i32 t = R->N - 1; t >= 0; --t) {
        //this severs math to next array element
        f32 mask = R->dones[t] ? 0.0f : 1.0f;
        f32 next_val = (t == R->N - 1) ? 0.f : R->values[t+1];
        f32 delta = R->rewards[t] + GAMMA * next_val * mask - R->values[t];
        last_gae = delta + GAMMA * LAMBDA * mask * last_gae;

        R->advantages[t] = last_gae;
        R->returns[t]    = last_gae + R->values[t];
    }

    //debug metric before training
    f32 avg_reward = 0.0f;
    for (i32 i = 0; i < R->N; ++i) avg_reward += R->rewards[i];
    avg_reward /= (f32)MAX(1, R->N);

    train_step(&ctx->agent, R, LEARNING_RATE);

    print("\n=========================================================\n");
    print("[PPO] TRAIN COMPLETE\n");
    print("[PPO] Samples: %d | Avg reward: %f\n", R->N, avg_reward);
    print("=========================================================\n\n");

    R->N = 0;

    //reset batch/episode after training
    ctx->completed_episodes_in_batch = 0;
    ctx->running_episode_return = 0.0f;

    return true;
}

typedef struct {
    //roads this light controls
    Link_Id vertical_approach;
    Link_Id horizontal_approach;
    //current state of the light
    i32 current_phase;
    //agent assigned to this intersection
    PPO_Context* controller;
} TrafficLight;

typedef struct {
    f32 reward;
    i32 steps_in_episode;
    u8 decision_axis;
    f32 observation[STATE_SIZE];
    f32 rolling_avg_reward;
    f32 last_episode_total_score;
} PPO_Info;

static void ppo_update(PPO_Info *info, PPO_Context *ctx, Simulation_State *sim, Graph *graph, f32 dt) {
    ctx->decision_timer += dt;

    //track phase duration
    ctx->phase_elapsed += dt;

    f32 obs[STATE_SIZE] = {0};

    if (ctx->decision_timer >= PPO_DECISION_INTERVAL) {
        //now evaluate previous action after fixed control interval
        if (ctx->waiting_for_reward) {
            info->reward = traffic_compute_reward(graph, sim, ctx);

            if (ctx->rolling_avg_reward == 0.0f) {
                ctx->rolling_avg_reward = info->reward;
            } else {
                // Smooth the graph: 99% history, 1% new data
                ctx->rolling_avg_reward = (ctx->rolling_avg_reward * 0.99f) + (info->reward * 0.01f);
            }
            // Pass it to info so the UI can see it
            info->rolling_avg_reward = ctx->rolling_avg_reward;

            //PPO episode progresses per controller decision, not per frame
            ctx->steps_in_episode += 1;
            bool done = (ctx->steps_in_episode >= MAX_EP_LEN);

            ppo_commit(ctx, &ctx->current_receipt, info->reward, done);

            if (done) {
                //grab the total score exactly as the episode finishes
                info->last_episode_total_score = ctx->running_episode_return;
            }

            print("[AI REWARD] reward=%f | episode_step=%d\n", info->reward, ctx->steps_in_episode);
            print("[AI REWARD] Step: %2d | Immediate: %6.2f | Rolling Avg: %6.2f\n",
                ctx->steps_in_episode, info->reward, ctx->rolling_avg_reward);
            info->steps_in_episode = ctx->steps_in_episode;

            ppo_check_and_train(ctx);

            //explicit episode reset
            if (done) {
                ctx->steps_in_episode = 0;
            }

            ctx->last_action = ctx->current_receipt.action;
        }

        //get observation through helper instead of inline logic
        traffic_get_obs(graph, sim, ctx, obs);
        memcpy(info->observation, obs, sizeof obs);

        //save current queue snapshot so next reward can compare improvement
        ctx->prev_north_load = (i32)roundf(obs[0] * PPO_QUEUE_NORMALIZER);
        ctx->prev_south_load = (i32)roundf(obs[1] * PPO_QUEUE_NORMALIZER);
        ctx->prev_east_load  = (i32)roundf(obs[2] * PPO_QUEUE_NORMALIZER);
        ctx->prev_west_load  = (i32)roundf(obs[3] * PPO_QUEUE_NORMALIZER);
        i32 action = ppo_get_action(ctx, obs, &ctx->current_receipt);

        //apply light change to graph
        traffic_apply_action(graph, ctx, action);

        // if (action == 0) {
        //     info->decision_axis = 1;
        //     print("[AI DECISION] phase=VERTICAL GREEN | obs=(N:%.2f, S:%.2f, E:%.2f, W:%.2f, T:%.2f)\n",
        //         obs[0], obs[1], obs[2], obs[3], obs[4]);
        // } else {
        //     info->decision_axis = 0;
        //     print("[AI DECISION] phase=HORIZONTAL GREEN | obs=(N:%.2f, S:%.2f, E:%.2f, W:%.2f, T:%.2f)\n",
        //         obs[0], obs[1], obs[2], obs[3], obs[4]);
        // }
        // print("---------------------------------------------------------\n");

        ctx->waiting_for_reward = true;
        ctx->decision_timer = 0.0f;
    }
}


// static void traffic_obs(Simulation_State * sim, f32 out_state[STATE_SIZE]) {
//     //clear sttae
//     for (i32 i = 0; i < STATE_SIZE; ++i) out_state[i] = 0.f;

//     //this is where we place the actual segment ids for a 4 way intersection
//     // Link_Id ns_approaches[2] = {1, 2};
//     // Link_Id ew_approaches[2] = {4, 5};

//     Link_Id vertical_approach = 2;
//     Link_Id horizontal_approach = 7;

//     //count waiting cars on the approaches
//     for (u64 i = 0; i < sim->cars.count; i++) {
//         Car *c = &sim->cars.data[i];

//         //if car is moving slow (stuck in traffic) and knows path
//         if (c->speed < 2.0f && c->path.count > 0){
//             Link_Id current_seg = c->path.data[0];

//             // if (current_seg == ns_approaches[0]) out_state[0] += 1.0f; //North Queue
//             // if (current_seg == ns_approaches[1]) out_state[1] += 1.0f; //South Queue
//             // if (current_seg == ew_approaches[0]) out_state[2] += 1.0f; //East Queue
//             // if (current_seg == ew_approaches[1]) out_state[3] += 1.0f; //West Queue

//             if (current_seg == vertical_approach) out_state[0] += 1.0f;
//             if (current_seg == horizontal_approach) out_state[1] += 1.0f;
//         }
//     }
//     //normalize data for neural networks other it will fail, we divide by an expected max queue, arbitrary for now
//     // for (i32 j = 0; j < 4; j++) {
//     //     out_state[j] /= 20.0f;
//     // }
//     out_state[0] /= 15.0f;
//     out_state[1] /= 15.0f;

//     //add the current light phase so AI knows what state lights are in
//     out_state[2] = (f32)sim->current_phase;
// }

// //flips bitflags for traffic lights
// static void set_light(Simulation_State *sim, Link_Id id, bool is_green) {
//     if (is_green) {
//         sim->graph.links[id].flags |= Link_Flag_TRAFFIC_LIGHT_GREEN;
//         sim->graph.links[id].flags &= ~Link_Flag_TRAFFIC_LIGHT_RED;
//     } else {
//         sim->graph.links[id].flags |= Link_Flag_TRAFFIC_LIGHT_RED;
//         sim->graph.links[id].flags &= ~Link_Flag_TRAFFIC_LIGHT_GREEN;
//     }
// }

// static void traffic_step(Simulation_State *sim, i32 action, f32 *out_reward, bool *out_done) {

//     //this is where we place the actual segment ids for a 4 way intersection
//     // Link_Id ns_approaches[2] = {1, 2};
//     // Link_Id ew_approaches[2] = {4, 5};

//     Link_Id vertical_approach = 2;
//     Link_Id horizontal_approach = 7;

//     //apply action to lights
//     if (action == 0) {
//         //AI chose 0: North/South Green, East/West Red
//         set_light(sim, vertical_approach, true);
//         set_light(sim, horizontal_approach, false);
//         sim->current_phase = 0;
//     } else {
//         //AI chose 1: East/West Green, North/South Red
//         set_light(sim, horizontal_approach, true);
//         set_light(sim, vertical_approach, false);
//         sim->current_phase = 1;
//     }

//     //ai just changed the lights, so simulate a few seconds to let cars react before next decsiosn
//     f32 dt = 1.0f / 60.0f; //60hz tick
//     i32 simulated_frames = 60*3; //3 seconds

//     for(int i = 0; i < simulated_frames; i++) {
//         simulation_step(sim, dt);
//     }

//     //reward calculation (punishment)
//     //minimize traffic jams. We give the AI a negative reward for every car stuck.
//     f32 waiting_penalty = 0.0f;
//     for (u64 i = 0; i < sim->cars.count; i++) {
//         Car *c = &sim->cars.data[i];
//         if (c->speed < 2.0f && c->path.count > 0) {
//             Link_Id current_seg = c->path.data[0];
//             //only penalize ai for cars stuck at its intersection not others
//             if (current_seg == vertical_approach || current_seg == horizontal_approach) {
//                 waiting_penalty += 1.0f; // -1 point for every stopped car
//             }
//         }
//     }

//     //Flicker Penalty: If it changes lights every single turn, it's bad.
//     //We add a "cost" to changing lights.
//     static i32 last_action = 0;
//     if (action != last_action) {
//         waiting_penalty += 3.0f; //Costs "3 cars worth of pain" to flip the switch
//     }
//     last_action = action;

//     //AI learns by maximizing reward, so we make the penalty negative.
//     *out_reward = -waiting_penalty;

//     //check Episode End
//     sim->steps++;
//     *out_done = (sim->steps >= MAX_EP_LEN); //end episode after 100 AI decisions
// }

// i32 IT = 0;
// i32 N = 0;
// f32 AVG_LEN = 0;
// f32 AVG_RET = 0;

// //run a few updates; we expect avg episodic return to trend up
// //(policy should learn to move right to get +1 and avoid left's -1).
// static void ppo_everything(Rollout *R) {
//     print("=== PPO DEMO ===\n");

//     RNG rng = rng_init(42); //fixed seed
//     Agent agent = {0};
//     agent_init(&agent, &rng);

//     GridWorld env = {0};

//     //Pick which environment you want (details of each enviroment at the top of the file):
//     //env.kind = ENV_LINEWORLD;
//     // env.kind = ENV_CORRIDOR;
//     //env.kind = ENV_GRIDWORLD;

//     // if (env.kind == ENV_TRAFFIC) {
//     //     env.traffic.arena = arena;
//     //     env.traffic.graph = make_four_way_stop(arena);
//     //     //traffic_reset will set rng, steps, etc.
//     // }

//     for (i32 it = 0; it < 200; ++it) {
//         collect_batch(&env, &agent, &rng, R);

//         f32 avg_len = 0.f, avg_ret = 0.f;
//         for (i32 e = 0; e < BATCH_SIZE; ++e){
//             avg_len += (f32)R->ep_len[e];
//             avg_ret += R->ep_ret[e];
//         }
//         avg_len /= (f32)BATCH_SIZE;
//         avg_ret /= (f32)BATCH_SIZE;

//         IT = it; N = R->N; AVG_LEN = avg_len; AVG_RET = avg_ret;
//         if (it % 10 == 0) {
//             print("[it %d] N=%d  avg_len=%f  avg_ret=%f\n",
//                 it, R->N, avg_len, avg_ret);
//         }
//         //evaluate ppo vs q learning
//         //data driven decsion making, choose an algorithim based on evaluation

//         train_step(&agent, R, LEARNING_RATE);
//     }

//     print("\nDone\n");
// }
