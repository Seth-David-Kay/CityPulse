/*
This is just a playground / basic understanding of PPO
*SETH*
*/

// Nawwaf stuff

// ok so im going to use this file as a just a playground to implement ppo and test some stuff
// just a simple thing for now then move on to implementing the real deal later
// so im thinking of using a pretty simple enviroment with discrete action and states
// - define an agent (policy and value network)
// - run episodes, collect trajectories
// - calculate advantages
// - perform ppo updates (clipped surrogate loss)

//im thinking of just having a small simulated env
// we can build this agent that has this policy (actor) and value (critic) network
// we forward pass to get action and value estomate
// store those transitions in a rollout buffer
// we do the updates using gae (Generalized Advantage Estimate)

//PPO background and reference, policy gradient
// sample trajectories (sequences of states, actions, rewards)
// estimating advantages to measure how good an action was compared to average
// update the policy without making big jumps (via clipped objective)

//ppo working parts:
//Policy Network: outputs probabilities of actions (actor)
// Value network: predicts expected return from a state (critic)
//rolliut buffer: stores (state, action, reward, value, log_prob)
// advantage estimator: uses gae to estimate advantage
//loss function: combine policy loss (clipped), value loss, and entropy bonus (pretty much how random the agent is)

//PPO in steps
//1. Initlzie agent with random weights for bith networks (actor and critic)
//2. Rollout loop
// -- interact with enviroment (take action pased on policy)
// -- store transitions (state, action, reward, value, log_prob)
//3. Compute rewards and advantages
// --use GAE
//  delta = r + gamma * V(s') - V(s)
//  advantage _t = delta_t + lambda * gamma * advantage_{t+1}
//4. PPO update
// -- Policy loss: use clipped surrogate objective
//  L = min(r_t * A_t, clip(r_t, 1 - eps, 1 + eps) * A_t)
// -- Value loss: just squared diff between predicted and target return
// -- entropy bonus: encourages explorration (we add entropy to loss)
// -- use gradient descent to update both nn's
//5. Repeat
//6. ?
//7. Profit


//im doing this for clarity and correctness, not for use so yeah, so im going to do this again prob

//we build the smallest possible RL loop to make the wiring obvious.
//environment: a 1D "LineWorld" of 4 cells. You start at cell 1,
//move left/right, episode ends if you reach an edge or after a cap.
//right edge gives +1 (good), left edge 0 + small time penalty each step.
//policy: linear logits over 2 actions (left/right).
//value: linear baseline estimating return from a state.
//rollout: collect (s, a, r, logπ(a|s), V(s)), compute returns and
//advantages = returns − values, then do a vanilla policy-gradient step,
//plus value regression.
//we print a tiny probe of π(a=right|s=1) to prove learning happens.

#define NUM_APPROACHES 4
#define FEATURES_PER_APPROACH 2 //queue length + waiting time
#define NUM_PHASES 2
#define EXTRA_FEATURES 1 //time in phase

typedef struct {
    i32 pos; //0..3
    i32 steps; //step counter
    RNG rng;
} LineWorld;

typedef struct {
    i32 pos; //0..4
    i32 switch_on; //0 or 1
    i32 steps; //step counter
    RNG rng;
} Corridor;

#define MAX_CARS 64
#define MIN_GREEN_STEPS 5
#define SPAWN_PROB_PER_STEP 0.3f
#define DT_PER_STEP 1.0f
//state: 9 links + light state = 10 inputs

typedef enum { //index arrays like queue length[approach] and waiting_time[approach]
    //to group links into approaches
    Approach_North = 0,
    Approach_East = 1,
    Approach_South = 2,
    Approach_West = 3,
} Approach_Id;

//TrafficEnv: environment state for one intersection
//entire "world state" from the RL perspective.
//
// It contains:
//-a road graph
//-a fixed-size pool of cars currently in the simulation
//-aggregated per-approach stats (queue_len, waiting_time)
//-signal timing (current_phase, time_in_phase)
//-episode time (steps)
//-RNG handle for stochastic events (spawn)
//-pointer to an Arena for pathfinding allocations
//
// PPO only ever sees a *projection* of this state via lw_obs().
typedef struct {
    Graph graph; //definition roads

    Car cars[MAX_CARS];//active cars currently
    i32 car_count; //how many above are valid

    f32 queue_len[NUM_APPROACHES]; //how many cars are stopped/waiting on approach a
    f32 waiting_time[NUM_APPROACHES]; //acculates total waiting over the episode for a

    f32 time_in_phase; //how long weve been in this phase in steps
    i32 current_phase; //0= NS green, 1 = WE green

    i32 steps;//episode time in discrete steps
    RNG rng; //rng for stochastic events like car spawns

    Arena *arena; //used for temporary allocations (pathfinding etc.)
} TrafficEnv;

typedef struct {
    EnvKind kind;
    union {
        LineWorld line;
        Corridor corridor;
        TrafficEnv traffic;
    };
} Env;

//rough intersection center; tweak, measure "distance to intersecion" for cars
//if a car is close and its approach is red thenwe treat it as queued/waiting
static V2 intersection_center = { .x = 800.f, .y = 400.f };

//the road graph is defined as 9 links (0..8) and manually assign which approach each link belongs to.
//- Links 0,1,2 : approaching from North
//- Links 3,4   : approaching from East
//- Links 7,8   : approaching from South
//- Anything else  : West
//we use this to determine which approach a car is on and wherther the signal is currently green or red
static Approach_Id car_approach_from_link(Link_Id sid) {
    if (sid == 0 || sid == 1 || sid == 2) return Approach_North;
    if (sid == 3 || sid == 4)             return Approach_East;
    if (sid == 7 || sid == 8)             return Approach_South;
    return Approach_West;
}

//we dont care about perserving order of cars in array, so we can remove a car
//swap with last removal
static void remove_car(TrafficEnv *E, i32 idx) {
    if (idx < 0 || idx >= E->car_count) return;
    E->cars[idx] = E->cars[E->car_count - 1];
    E->car_count--;
}


//// This is called by lw_step() with some probability.
//- Enforce max-car cap
//- Choose an entrance and exit link (for now, always [0] entrance/exits)
//- Initialize car physical parameters (mass, wheelbase, speed limits, etc.)
//- Compute a path from start → end using A* (find_path)
//- Place the car at the start of the first link and orient it along
//that link's direction.
//
//If pathfinding fails (no path), the car is discarded.
static void spawn_car(TrafficEnv *E) {
    if (E->car_count >= MAX_CARS) return;
    if (E->graph.entrances.count == 0 || E->graph.exits.count == 0) return;

    //allocat enew car and zero init
    Car *c = &E->cars[E->car_count++];
    *c = (Car){0};

    c->size              = (V2){.x = 4.f, .y = 2.f};
    c->mass              = 1200.f;
    c->wheelbase         = 2.5f;
    c->max_speed         = 15.0f;
    c->max_acceleration  = 3.0f;
    c->max_break         = 5.0f;
    c->max_steer_angle   = 0.4f;
    c->velocity          = 0.0f;

    //single fixed enrance and exit
    Link_Id start_seg = E->graph.entrances.data[0];
    Link_Id end_seg   = E->graph.exits.data[0];

    //find a path through segements
    Link_Ids path = find_path(E->graph, *c, start_seg, end_seg, E->arena);
    c->path = path;

    //if no path dont spawn
    if (path.count == 0) {
        E->car_count--;
        return;
    }

    //place the car at the start of the first link along the path
    Link *s = &E->graph.links.data[path.data[0]];
    c->position.x = s->start.x;
    c->position.y = s->start.y;

    //set the cars heading to match the link direction
    //convert link vector (end-start) into heading angle
    f32 dx = s->end.x - s->start.x;
    f32 dy = s->end.y - s->start.y;
    c->direction = atan2f(-dx, dy);

    //may use this later
    c->initial_direction = c->direction;
    c->initial_speed     = 0.0f;
}

//This function does the per-timestep simulation of the physical world.
//1. Reset queue_len[a] to 0 for all approaches (recompute)
//2. Compute which approaches are green (based on current_phase)
//3. For each car:
//       a. Determine its approach based on current link
//       b. Compute distance to the intersection center
//       c. Decide target velocity (max_speed or 0 if red and close)
//       d. Update acceleration, velocity, heading, and position
//       e. If the car is close enough to the exit link end → remove it
//
//RL interacts with this only via:
//   - The chosen action (phase switch or not) that affects `green[]`
//   - The aggregate outputs queue_len[] and waiting_time[]
static void advance_cars(TrafficEnv *E, f32 dt) {
    //reset per-approach queue lengths for this step
    //accumulate based on which cars are sstopped near the intersection
    for (i32 a = 0; a < NUM_APPROACHES; ++a) {
        E->queue_len[a] = 0.f;
    }

    //compute which approaches have green lights in current phase
    bool green[NUM_APPROACHES] = {0};
    if (E->current_phase == 0) {
        green[Approach_North] = true;
        green[Approach_South] = true;
    } else {
        green[Approach_East]  = true;
        green[Approach_West]  = true;
    }

    //iterate over cars
    i32 i = 0;
    while (i < E->car_count) {
        Car *c = &E->cars[i];

        //detrmine which link and approach car is on
        //we inspect first link in the path
        // i should track which index the car is actually later
        Link_Id cur_seg_id = c->path.count ? c->path.data[0] : 0;
        Approach_Id approach  = car_approach_from_link(cur_seg_id);

        //compute distance to intersection center
        //is it in queue region and stop if its red
        f32 dx = c->position.x - intersection_center.x;
        f32 dy = c->position.y - intersection_center.y;
        f32 dist_to_intersection = sqrtf(dx*dx + dy*dy);

        //by default go max speed
        f32 command_velocity = c->max_speed;

        //if approach is red and car is close then queue it
        //stop car and increment queue and waiting time
        if (!green[approach] && dist_to_intersection < 80.f) {
            command_velocity = 0.f;
            E->queue_len[approach] += 1.f;
            E->waiting_time[approach] += dt;
        }

        //compute alectration needed to move velocity
        f32 a = update_acceleration(*c, command_velocity, dt);
        //integrate velocuty
        f32 v = update_velocity(*c, a, dt);
        //cars just go straight
        c->steering_angle = 0.f;
        //update heading/direction
        f32 theta = update_direction(*c, dt);
        //integrate position using updated velocity and direction
        update_position(c, dt);

        c->velocity  = v;
        c->direction = theta;

        //check if car is close to end of seg, if so it has exited the network and remove the car from the env
        Link *last_seg =
            &E->graph.links.data[c->path.data[c->path.count - 1]];
        f32 ex = last_seg->end.x;
        f32 ey = last_seg->end.y;
        f32 d_end_x = c->position.x - ex;
        f32 d_end_y = c->position.y - ey;
        f32 dist_to_exit = sqrtf(d_end_x*d_end_x + d_end_y*d_end_y);

        if (dist_to_exit < 10.f) {
            //swap car with last
            remove_car(E, i);
            continue;
        }

        ++i; //if we didnt remove car move on to the next
    }
}

static inline f32 logsumexp2(f32 x, f32 y){
    //helper for 2 elements; actioncount=2
    f32 m = (x > y) ? x : y; //max for stability
    return m + logf(expf(x - m) + expf(y - m)); //lse = m +log(exp(x-m)+exp(y-m))
}

//compute softmax probabilities and log probabilties for action_count=2
static inline void softmax2_log(const f32 logits[2], f32 out_prob[2], f32 out_logprob[2]){
    f32 lse = logsumexp2(logits[0], logits[1]); //compute log-sum-exp
    out_logprob[0] = logits[0] - lse; //log p0
    out_logprob[1] = logits[1] - lse; //log p1
    out_prob[0] = expf(out_logprob[0]); //p0
    out_prob[1] = expf(out_logprob[1]); //p1
}


// static inline void lw_reset(LineWorld *E, u64 seed){ //reset episdoe to starting state
//     E->rng = rng_init(seed); //reseed rng
//     //start at 1 to allow both sides to matter
//     E->pos = 1;
//     E->steps = 0;//reset step counter
// }

// static inline void lw_obs(LineWorld *E, f32 out_state[STATE_SIZE]){ //produces one-hot observation
//     for (i32 i=0;i<STATE_SIZE;++i) out_state[i] = 0.f; //zero the vector
//     out_state[E->pos] = 1.f; //mark current postion as 1
// }

// static inline void lw_step(LineWorld *E, i32 action, f32 *out_reward, f32 *out_done){ //apply action; return reward and done
//     //apply action
//     if      (action == 0) E->pos = (E->pos > 0) ? E->pos-1 : 0; // go left unless already at 0
//     else if (action == 1) E->pos = (E->pos < 3) ? E->pos+1 : 3; // go right unless already at 3
//     else                  E->pos = E->pos; //ignore bad action

//     //terminal reward at edges
//     f32 r = 0.f; //init reward
//     if (E->pos == 3) r = +1.f; //reward for reaching right edge
//     if (E->pos == 0) r = 0.f; //no penalty for reaching left edge
//     r -= 0.01f; //time penalty

//     E->steps += 1; //increment step
//     //terminal if at either edge or if at step cap, done flag
//     f32 done = (E->pos == 0) || (E->pos == 3) || (E->steps >= MAX_EP_LEN);

//     *out_reward = r; //return reward
//     *out_done = done;
// }

static inline void line_reset(LineWorld *E, u64 seed){
    E->rng = rng_init(seed);
    E->pos = 1; //start near middle
    E->steps = 0;
}

static inline void corridor_reset(Corridor *E, u64 seed){
    E->rng = rng_init(seed);
    E->pos = 2; //start in the middle
    E->switch_on = 0; //off
    E->steps = 0;
}

//just reinit everything like usual
static inline void traffic_reset(TrafficEnv *E, u64 seed){ //reset episdoe to starting state
    E->rng = rng_init(seed); //reseed rng
    E->current_phase = 0;
    E->time_in_phase = 0.f;
    E->steps         = 0;
    E->car_count     = 0;

    //reset per approach stats
    for (i32 a = 0; a < NUM_APPROACHES; ++a) {
        E->queue_len[a]    = 0.f;
        E->waiting_time[a] = 0.f;
    }
}


static inline void line_obs(LineWorld *E, f32 out_state[STATE_SIZE]){
    for (i32 i = 0; i < STATE_SIZE; ++i) out_state[i] = 0.f;
    //only first 4 entries used
    out_state[E->pos] = 1.f;
}

static inline void corridor_obs(Corridor *E, f32 out_state[STATE_SIZE]){
    for (i32 i = 0; i < STATE_SIZE; ++i) out_state[i] = 0.f;
    // map (pos, switch) → index
    // switch off: indices 0..4
    // switch on : indices 5..9
    i32 idx = E->pos + (E->switch_on ? 5 : 0);
    out_state[idx] = 1.f;
}

//it maps the TrafficEnv state into a fixed-length float vector
//of length STATE_SIZE, which is the input to the policy and value networks.
//
//Current design:
//For each approach a in {N, E, S, W}:
//     - normalized queue length:   queue_len[a]    / 10.0
//     - normalized waiting time:   waiting_time[a] / 200.0
//
//   Plus a one-hot encoding of the current phase (NUM_PHASES entries).
//   Plus a normalized time-in-phase scalar.
//
// Layout in out_state:
//   [0]     = queue_len[N] / 10
//   [1]     = waiting_time[N] / 200
//   [2]     = queue_len[E] / 10
//   [3]     = waiting_time[E] / 200
//   ...
//   [8..(8+NUM_PHASES-1)] = one-hot current_phase
//   [last] = time_in_phase / 30
static inline void traffic_obs(TrafficEnv *E, f32 out_state[STATE_SIZE]){ //produces one-hot observation
    for (i32 i=0;i<STATE_SIZE;++i) out_state[i] = 0.f; //zero the vector

    //walk through out_state
    i32 idx = 0;

    //for each we emit 2 features
    //normalized queue length
    //normalized accumulated waiting time
    //normaliaztaion constants are heuristics to keep values between 0 and 1
    for (i32 a = 0; a < NUM_APPROACHES; ++a) {
        out_state[idx++] = E->queue_len[a] / 10.0f;
        out_state[idx++] = E->waiting_time[a] / 200.0f;
    }

    //one hot encoding
    //num_phases is 2, (NS green, WE green)
    //we write a one-hot vector of length num_phases:
    //   [1,0] if current_phase == 0
    //   [0,1] if current_phase == 1
    for (i32 p = 0; p < NUM_PHASES; ++p) {
        out_state[idx++] = (p == E->current_phase) ? 1.f : 0.f;
    }

    //we add a scalar feature that grows as we stay longer in the same phase
    //diving by 30.0f is a heuristic
    //This gives the agent a notion of "how stale" the current phase is.
    //which can help it learn phase change timing strategies
    out_state[idx++] = E->time_in_phase / 30.0f;
}

static inline void line_step(LineWorld *E, i32 action, f32 *out_reward, bool *out_done){
    //apply action
    if      (action == 0) E->pos = (E->pos > 0) ? E->pos - 1 : 0;
    else if (action == 1) E->pos = (E->pos < 3) ? E->pos + 1 : 3;

    f32 r = 0.f;
    if (E->pos == 3) r = +1.f;
    if (E->pos == 0) r = 0.f;
    r -= 0.01f;

    E->steps += 1;
    bool done = (E->pos == 0) || (E->pos == 3) || (E->steps >= 12);

    *out_reward = r;
    *out_done   = done;
}

static inline void corridor_step(Corridor * E, i32 action, f32 *out_reward, bool *out_done){
    f32 r = -0.01f; // time penalty
    bool done = false;

    // movement
    if      (action == 0 && E->pos > 0) E->pos--;
    else if (action == 1 && E->pos < 4) E->pos++;

    // flip switch at pos 0
    if (E->pos == 0 && E->switch_on == 0) {
        E->switch_on = 1;
        r = 0.5f; // reward for discovering switch
    }

    // goal at pos 4
    if (E->pos == 4) {
        if (E->switch_on) {
            r = 1.0f;  // success
            done = true;
        } else {
            r = -0.2f; // bonk door
            E->pos = 3;
        }
    }

    E->steps += 1;
    if (E->steps >= 20) done = true;

    *out_reward = r;
    *out_done   = done;
}


//lw_step() is the core transition function:
//
// Inputs:
//     - action   : discrete action chosen by the policy (0 or 1)
//     - out_reward : where to write the scalar reward for this step
//     - out_done   : where to write the terminal flag (1.0 if episode ended)
//
//flow:
//     1. Possibly switch signal phase (if action requests it and min green met).
//     2. Stochastically spawn a new car.
//     3. Advance all cars by dt (physics + queue calculation).
//     4. Compute reward based on queue & waiting time + phase-change penalty.
//     5. Increment episode time and time-in-phase; check termination.
//
//reward shaping:
//   - step_cost = 0.1 * (sum queue_len) + 0.001 * (sum waiting_time)
//   - If we switch phase (action == 1) and we *just* switched (time_in_phase == 0),
//     add +0.5 to cost (penalty for switching).
//   - reward = - step_cost  (we want the agent to minimize cost).
//   i need to change this at some point, its never rewarded so im prob gonna reward for a car exiting the frame/obs space
static inline void traffic_step(TrafficEnv *E, i32 action, f32 *out_reward, bool *out_done){ //apply action; return reward and done
    f32 dt = DT_PER_STEP; // simulation step

    //action semantics:
    //   0 → keep current phase
    //   1 → request phase switch (if allowed)
    //
    //we enforce a minimum green duration (MIN_GREEN_STEPS).
    //if the policy tries to switch earlier, we ignore the switch request.
    if (action == 1 && E->time_in_phase >= MIN_GREEN_STEPS) {
        E->current_phase = 1 - E->current_phase;
        E->time_in_phase = 0.f;
    }

    //stochastic car spawn
    //with probability SPAWN_PROB_PER_STEP, we inject a new car into the network
    //rng_unif01 ∈ [0,1), if less than threshold spawn
    if (rng_unif01(&E->rng) < SPAWN_PROB_PER_STEP) {
        spawn_car(E);
    }

    //advance
    //updates cars positions and velocities, queue_len[a] for each approach (I recompute this from scratch, there must be a better way to do this)
    //waiting_time[a] accumulated with dt where cars are stopped
    advance_cars(E, dt);

    //compute cost/reward
    //aggregate queuelength and waiting time over all approaches
    f32 queue_sum = 0.f;
    f32 wait_sum  = 0.f;
    for (i32 a = 0; a < NUM_APPROACHES; ++a) {
        queue_sum += E->queue_len[a];
        wait_sum  += E->waiting_time[a];
    }

    //base step cost, linear penalty on queue and accumulated time waiting
    //queue_sum is "how many cars are stopped", waiting_time is "how long in total"
    //coeffcients are design and arbitrary choices that control relative importance
    f32 step_cost = 0.1f * queue_sum + 0.001f * wait_sum;

    //phase switch penalty
    //if action == 1 and we actually switched time_in_phase == 0 after switch),
    //add a fixed ectra cost. This discourages the agent from switching too often just to make it realistic
    if (action == 1 && E->time_in_phase == 0.f) {
        step_cost += 0.5f;
    }

    //reward is negative of cost: PPO is maximizing reward -> minimizing cost
    f32 r = -step_cost;

    //advance step and time spent in current phase
    E->steps        += 1;
    E->time_in_phase += 1.f;

    //episode wnds if we hit the max ep len
    //i should add another termination condition ("all cars cleared", stuck, etc.)
    bool done = (E->steps >= MAX_EP_LEN);

    //return reward and done to the caller (the RL loop)
    *out_reward = r;
    *out_done   = done;
}


static inline void grid_reset(GridWorld *E, u64 seed){
    E->rng = rng_init(seed);
    //start top left
    E->x = 0;
    E->y = 0;
    E->steps = 0;
}

static inline void grid_obs(GridWorld *E, f32 out_state[STATE_SIZE]){
    //zero out the whole vector
    for (i32 i = 0; i < STATE_SIZE; ++i) out_state[i] = 0.f;

    //set only the current tile index to 1.0
    //(0,0) -> Index 0. (4,4) -> Index 24.
    i32 idx = E->y * GRID_SIZE + E->x;

    if (idx >= 0 && idx < STATE_SIZE) {
        out_state[idx] = 1.0f;
    }
}

static inline void grid_step(GridWorld *E, i32 action, f32 *out_reward, bool *out_done){
    //we should calculate distance before moving (manhattan distance)
    i32 dist_before = (GRID_SIZE - 1 - E->x) + (GRID_SIZE - 1 - E->y);
    //0:Up, 1:Right, 2:Down, 3:Left
    i32 dx = 0, dy = 0;

    if      (action == 0) dy = -1; //up
    else if (action == 1) dx = +1; //right
    else if (action == 2) dy = +1; //down
    else if (action == 3) dx = -1; //left

    //new position
    i32 nx = E->x + dx;
    i32 ny = E->y + dy;

    //check bounds (walls)
    //if out of bounds just bonk
    bool hit_wall = false;

    if (nx >= 0 && nx < GRID_SIZE) {
        E->x = nx;
    } else {
        hit_wall = true;
    }
    if (ny >= 0 && ny < GRID_SIZE) {
        E->y = ny;
    } else {
        hit_wall = true;
    }

    //calculate distance after moving
    i32 dist_after = (GRID_SIZE - 1 - E->x) + (GRID_SIZE - 1 - E->y);

    //reward shaping
    //if we got closer (dist goes down), reward
    //if we moved away (dist goes down), punish
    //if we hit a wall (dist stays same), neutral
    f32 shaping = (f32)(dist_before - dist_after) * 0.1f;

    //base time penalty
    f32 r = shaping - 0.1f;

    //standard time penalty yeah

    if (hit_wall) {
        r -= 0.2f;
    }

    bool done = false;

    //check if we hit goal (bottom right)
    if (E->x == GRID_SIZE - 1 && E->y == GRID_SIZE - 1) {
        r += 10.0f; //big reward
        done = true;
    }

    E->steps++;
    //fail if we take too long
    if (E->steps >= MAX_EP_LEN) done = true;

    *out_reward = r;
    *out_done = done;

}

static void env_self_test(void){
    print("=== GRID ENV SELF TEST ===\n");

    Env env = {0};
    env.kind = ENV_GRIDWORLD;
    grid_reset(&env.grid, 1234);

    f32 r = 0;
    bool done = false;
    //goal is at (4,4), we start at (0,0)
    //we will force 4 right moves then 4 down moves
    //total steps needed is 8

    i32 moves[8] = {1, 1, 1, 1, 2, 2, 2, 2};

    for (i32 t = 0; t < 8; ++t) {
        //allocate temp state vector
        f32 state[STATE_SIZE];

        //encode env into obs vector
        grid_obs(&env.grid, state);

        //check coords from state before moving
        //state[0] is x, state[1] is y
        //print("Step %d | Pos (%d, %d) | ",
             //fmt(i32, t), fmt(i32, env.grid.x), fmt(i32, env.grid.y));

        //execute forced action
        i32 action = moves[t];
        grid_step(&env.grid, action, &r, &done);

        //print("Action %d -> Reward %.2f Done %d\n",
             //fmt(i32, action), fmt(f32, r), fmt(i32, done));

        //verify minimum steps shouldnt end episode
        if (t < 7 && done) {
             print("ERROR: Episode ended too early!\n");
             break;
    }

    }
    //after 8 steps we should be at (4,4) and done should be true
    if (done && env.grid.x == 4 && env.grid.y == 4) {
        print("SUCCESS: Goal reached, episode terminated correctly.\n");
    } else {
        print("FAILURE: Did not finish at goal or done flag missing.\n");
    }

    print("=== END GRID ENV TEST ===\n\n");
}
