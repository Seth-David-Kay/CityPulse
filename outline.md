# Control Flow

MAIN THREAD
- program opens
- program starts up
    - program loads map
    - program inits vehicles & corresponding initial paths
    - program places vehicles at start positions
- program starts THREAD 1

THREAD 1
    - every t seconds (variable), simulate vehicles
    - calculate next target (recalculating path if needed)
    - find any 'interrupters' (entry exit conditions) that will impede movement
    - calculate next target position based on next target
    - calculate physics through time step to next position given curr and target positions
    - return exact move based on above calculations

THREAD 1 (MAYBE THREAD 2??)
    - every Nth timestep (t * N seconds), step all agents
    OR
    - every t seconds, step 1/N agents

define "intersection" = arbitrary number of interconnected segments, hooked up to a single agent

AGENT input:
    - time passed since last step of this agent
    - observation space = {
        - per vehicle waiting on path through intersection:
            - how long has it been waiting
    }
    - state of intersection
AGENT output:
    - action: new state of constraints on segment connections, for each segment in intersection

PPO: every "simulation" (n timesteps >>> 1)
PPO input:
    - TODO
PPO output:
    - TODO

# Simulation
    ## init inputs
        - Graph? (can be internal)
        - Amount of cars (internal computation but exposed)
    ## step inputs
        - time in step
    ## returns
        - car positions
        - to add more
    ## vectorized returns
        - dimesions
        - ect.
    # flow
        - will be populated as coded


# PPO role:

Each intersection is controlled by a single PPO agent.

The agent observes a compressed summary of traffic conditions at its intersection

The agent outputs high-level control actions (constraints on segment connectivity/signal phases)

Learning happens offline relative to the simulation loop, using batches of experience

PPO never runs every simulation tick. It operates on a coarser control timescale

Time structure and control frequency:
The system should operate on three timescales:

1. Simulation timestep

Vehicle motion, queues, and waiting times are updated

Should run continouslly (eg. every 0.1-1.0 seconds)

PPO isnt involved

2. Control timestep

Every `K` simulation steps, the agent may issue a new action.

The chosen action is held constant for the next `K` simulation steps.

This should conceptually define one MDP transition from PPO's perspective

3. Learning timestep (batch-level)

PPO updates parameters only after collecting a batch of transitions

Training happens infrequently compares to simulation

Agent interface:

Input:

At each control timestep, the agent recieves a fixed-length observation vector representing the state of their intersection

This should ideally include:

-aggregated per-approach traffic statistics(queue length, accumulated waiting time)

-current signal phase

-time elapsed since last phase switch

-maybe other traffic meta data (not sure if nessesary)

This observation is fully Markovian at every control step so individual vehicle states are not nessesary such as vehicle position or anything like that, we just need an aggreagated observation for traffic density.

Agent output

The agent outputs a discrete control action that modifies intersection constraints.

-switch signal phase

-keep current phase

(just ideas, phase patterns, lane priorties, etc but switching and staying is it for now)

actions remain active until then next control timestep

PPO data requirements (so what the learning needs):

We operate on trajectories collected from the enviroment.

For each control timestep, PPO should collect

-`s_t` — observation at control timestep (snapshot of the data, what the agent saw to make a decision)
-`a_t` — sampled action (the action the agent made at that observation)
-`r_t` — reward accumulated over the last control interval
-`done_t` — episode termination flag
-`logπ_old(a_t | s_t)` — log probability under the old policy (actor calculation, how confident it is to make an action)
-`V(s_t)` — value estimate under the old critic (critic calculation, long term reward, used to calculate advantage estimate)

this makes a transition in the mdp


reward semantics(ppo perspective)

PPO assumes the reward represents how good the chosen actionover the control interval

The reward should ideally encode

- penalties for queue buildup

- penalties for excessive waiting time

- penalties for undesirable control behaviour(frequent phase switching)

- bonuses for throughput/clearing vehicles (realistically optional but its really good to have)

Reward is aggregated through many simulation ticks and provided as a single scalar per agent decision

Rollout collection:

PPO collects experience by repeatedly, observing the intersection state, sampling an action from the current policy, applying that action to the simulation, letting the simulation run until the next control tick, recieving an aggregated reward and terminal flag, storing the transition in a rollout buffer.

This is repeated until a full batch of transitions is collected.

Learning phase:

Once a batch is collected, PPO performs learning:

compute advantages using GAE

Optimize the polict using clipped surrogate objective to prevent large updates

train a value function to predict long term return from observations

apply multiple optimization epochs to the same batch for sample efficiency

After training:

Updated policy parameters applied to agents

The rollout buffer is cleared

The system resumes


PPO outputs:

- A trained policy network

- A value network estimating expected return (used only during training)

- Maybe we can have diagnostic metrics outputted too (avg return, entropy, KL divergence)

At runtime, only the policy network is required


How do we scale to MARL:

Two ways:

So, we have one single neural network for PPO and multiple intersection, at each control step, each agent collects a tuple, we then throw all these in the SAME rollout buffer

When we learn the PPO algorithim trains on this massive list of trajectories and it learns a universal strategy for traffic lights (eg. if queue is long switch to green)

This could work since our observation space is generic and doesnt have specfic IDs for each intersection, so all data is compatible for all agents

Another way

Every intersection acts as its own critic...

But this may be unnesscaey since intersection are the same (no speed limits or traffic rules etc)

Tweaks:

Selfish reward: each agent gets points only for its intersection

Cooperative reward: Agent gets points for its intersection + 50% of its neighbors throughput (this should prevent an agent from just flushing their traffic an already jammed neighbor)

For PPO MARL, we step ALL agents simultaneously, this just makes batch collection cleaner, if agents step at different times its going to be a nightmare managaing the math and the done flags.

Something to keep in mind when scaling: NON-STATIONARY

So many agents the world is bound to act unpredictably so Intersection A may change its policy and send a wave of cars to Intersection B. To Intersection B, the world just changed unpredictabily

the current design should handle this but just something to keep in mind since PPO is generally robust enough to handle this naturally

// - batch_size: How many transitions to collect before training (e.g., 4000)
// - obs_size: Size of obs
PPO_Agent* ppo_init(u32 batch_size, u32 obs_size, u32 seed);


// Inputs:
//   agent
//   observations: Array of size [N] (current state of all intersections)
//   count: Number of intersections (N)
//
// Outputs:
//   actions: Array of size [N] to be filled by the agent.
//   history: Array of PPO_StepHistory* [N]. The agent fills this in.
//             This is a struct that holds the ppo state (log_prob, value_estimate) to specfic action, good to keep this hidden
void ppo_select_actions(
    PPO_Agent* agent,
    const TrafficObs* observations,
    i32 count,
    TrafficAction* out_actions,
    PPO_StepHist* out_history
);

//closes the loop on a previous decision.
//
// Inputs:
//   agent
//   history: The array of ppo states we get from ppo_select_actions K steps ago.
//   rewards: Array of size [N]. The accumulated reward over the last K steps.
//   dones: Array of size [N].
//   count: Number of intersections (N)
//
//this function pushes the (State, Action, Reward, NextState) tuple into the internal PPO buffer.
//it frees/invalidates the receipts.
void ppo_commit_experience(
    PPO_Agent* agent,
    PPO_StepHistory* history,
    const f32* rewards,
    const bool* dones,
    i32 count
);

//checks if the internal buffer is full. If yes, runs training epochs.
//returns: true if training happened, false otherwise.
bool ppo_update_if_ready(PPO_Agent* agent);
