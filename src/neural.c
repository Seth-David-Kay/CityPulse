#define PLATFORM_NONE 1
#include "random.c"

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

// initializes a new neural net and allocates + randomizes weights and biases
static NeuralNetwork nn_init(Arena *arena, u32 input_size, u32 hidden_size, u32 output_size) {
    NeuralNetwork net = {
        .input_size = input_size,
        .hidden_size = hidden_size,
        .output_size = output_size,
    };

    // allocate weights and biases using arena allocator
    net.weights_input_hidden = arena_make(arena, input_size * hidden_size, sizeof(f32));
    net.weights_hidden_output = arena_make(arena, hidden_size * output_size, sizeof(f32));

    net.bias_hidden = arena_make(arena, hidden_size, sizeof(f32));
    net.bias_output = arena_make(arena, output_size, sizeof(f32));

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
    print("[NN] Output: % | Input: %, %\n",fmt(f32, output[0]), fmt(f32, input[0]), fmt(f32, input[1]));
}

// main entry point: sets up arena + neural net + runs test input through forward pass
static void program(void){
//     Arena arena = arena_init(1024 * 1024); // 1MB arena

//     NeuralNetwork net = nn_init(&arena, 2, 4, 1); // 2 inputs, 4 hidden units, 1 output

//     f32 input[2] = { 1.0f, 0.5f };  // hardcoded test input
//     f32 output[1] = { 0 };         // store output

//     nn_forward(&net, input, output);  // forward pass

//     printf("NN Output = %.4f for input [%.2f, %.2f]\n", output[0], input[0], input[1]); // confirm result
}
