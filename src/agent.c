#include <stdlib.h>
#define BASE_PLATFORM_NONE 1
#define DEPENDENCIES_NO_IMPLEMENTATION
#include "dependencies.c"
#include "base/base.c"
#include "shader.c"

#define MAX_HIDDEN_SIZE 64

// struct of a neural network
typedef struct {
    u32 input_size;              // how many input neurons
    u32 hidden_size;             // hidden layer width
    u32 output_size;            // how many output neurons

    f32 *weights_input_hidden;  // input → hidden weights (flattened 2D array)
    f32 *weights_hidden_output; // hidden → output weights (flattened 2D array)

    f32 *bias_hidden;           // biases for hidden layer
    f32 *bias_output;           // biases for output layer
} NeuralNetwork;

typedef struct {
    f32 queue_ns;                    // number of cars in north-south direction
    f32 queue_ew;                    // number of cars in east-west direction
    i32 phase;                       // current phase: 0 = NS green, 1 = EW green
    i32 ticks_since_last_change;    // how long it's been since light changed
    i32 action;                      // last action: 0 = hold, 1 = switch
    void *policy;  // pointer to NeuralNetwork (cast later)
} Agent;

void agent_observe(Agent *agent, f32 queue_ns, f32 queue_ew);
i32 agent_decide_hardcoded(Agent *agent);
f32 agent_compute_reward(Agent *agent);
f32 randf_range(f32 min, f32 max);

f32 randf_range(f32 min, f32 max) {
    return min + (f32)rand() / (f32)RAND_MAX * (max - min);
}

// initializes a new neural net and allocates + randomizes weights and biases
static NeuralNetwork nn_init(Arena *arena, u32 input_size, u32 hidden_size, u32 output_size) {
    NeuralNetwork net = {
        .input_size = input_size,
        .hidden_size = hidden_size,
        .output_size = output_size,
    };

    // allocate weights and biases using arena allocator
    net.weights_input_hidden = arena_make(arena, input_size * hidden_size, f32);
    net.weights_hidden_output = arena_make(arena, hidden_size * output_size, f32);

    net.bias_hidden = arena_make(arena, hidden_size, f32);
    net.bias_output = arena_make(arena, output_size, f32);

    // fill weights and biases with random values between -1 and 1
    for (u32 i = 0; i < input_size * hidden_size; i++) net.weights_input_hidden[i] = randf_range(-1, 1);
    for (u32 i = 0; i < hidden_size * output_size; i++) net.weights_hidden_output[i] = randf_range(-1, 1);
    for (u32 i = 0; i < hidden_size; i++) net.bias_hidden[i] = randf_range(-1, 1);
    for (u32 i = 0; i < output_size; i++) net.bias_output[i] = randf_range(-1, 1);

    return net;
}

// sigmoid activation function
static f32 sigmoid(f32 x) {
    return 1.0f / (1.0f + expf(-x));
}

// forward pass through the neural network
static void nn_forward(NeuralNetwork *net, f32 *input, f32 *output) {
    f32 hidden[MAX_HIDDEN_SIZE];  // temp array to hold hidden layer values
    assert(net->hidden_size <= MAX_HIDDEN_SIZE); // safety check

    // input → hidden
    for (u32 h = 0; h < net->hidden_size; h++) {
        f32 sum = net->bias_hidden[h];  // start with bias
        for (u32 i = 0; i < net->input_size; i++) {
            sum += input[i] * net->weights_input_hidden[i + h * net->input_size]; // weighted sum
        }
        hidden[h] = sigmoid(sum); // activate
    }

    // hidden → output
    for (u32 o = 0; o < net->output_size; o++) {
        f32 sum = net->bias_output[o];  // start with bias
        for (u32 h = 0; h < net->hidden_size; h++) {
            sum += hidden[h] * net->weights_hidden_output[h + o * net->hidden_size]; // weighted sum
        }
        output[o] = sigmoid(sum); // activate
    }

    // debug print to see what output came from what input
    printf("[NN] Output: %.4f | Input: %.2f, %.2f\n", output[0], input[0], input[1]);
}

void agent_observe(Agent *agent, f32 queue_ns, f32 queue_ew) {
    agent->queue_ns = queue_ns;
    agent->queue_ew = queue_ew;
    agent->ticks_since_last_change += 1;
}

i32 agent_decide_hardcoded(Agent *agent) {
    // basic rule: switch if one queue gets too long
    // hard coded logic commented out to make way for the nn
    // if (agent->phase == 0 && agent->queue_ew > agent->queue_ns + 3) {
    //     agent->action = 1;
    // } else if (agent->phase == 1 && agent->queue_ns > agent->queue_ew + 3) {
    //     agent->action = 1;
    // } else {
    //     agent->action = 0;
    // }

    // if (agent->action == 1) {
    //     agent->phase = 1 - agent->phase;
    //     agent->ticks_since_last_change = 0;
    // }

    // return agent->action;

    //cast nn pointer and setup input
    NeuralNetwork *net = (NeuralNetwork *)agent->policy;
    f32 input[2] = { agent->queue_ns / 10.0f, agent->queue_ew / 10.0f}; //normalize
    f32 output[1] = { 0 };

    nn_forward(net, input, output); //pass through nn

    agent->action = (output[0] > 0.5f) ? 1 : 0; //convert to binary action

    //update phase and timer if switching
    if(agent->action == 1){
        agent->phase = 1 - agent->phase;
        agent->ticks_since_last_change = 0;
    }

    //debug shennaigans
    //printf("[NN Agent] Phase: %d | Action: %d | Queues: NS=%.2f, EW=%.2f | Output=%.4f\n",
           //agent->phase, agent->action, agent->queue_ns, agent->queue_ew, output[0]);

    return agent->action;
}

f32 agent_compute_reward(Agent *agent) {
    // simple negative reward: sum of queues
    return -(agent->queue_ns + agent->queue_ew);
}

//entry point: create agent nn, simulate 10 timesteps
static void agent_program(void){
    Arena arena = arena_init(1024 * 1024); //arena allocation
    NeuralNetwork net = nn_init(&arena, 2, 4, 1); //nn:  2 inputs, 4 hidden, 1 output

    Agent agent = {0}; //zero init agnet
    agent.phase = 0; //start with ns green
    agent.policy = &net; //attach nn to agent

    for(int t=0; t<10; t++){
        f32 ns = randf_range(0, 10); //simulate cars ns
        f32 ew = randf_range(0, 10); //simulate cars ew

        agent_observe(&agent, ns, ew); //update agent
        i32 action = agent_decide_hardcoded(&agent); //get nn decison
        f32 reward = agent_compute_reward(&agent); //get reward

        printf("[Tiem %d] Reward: %.2f\n", t, reward); //print summary
    }

}
