#include "car.c"
#include "graph.c"
#include "ppo.c"


#define STATE_SIZE 3 //4 incoming queues + 1 current light phase = 5 inputs
#define ACTION_COUNT 2
#define HIDDEN_SIZE 64
#define PPO_OBS_SIZE 3

#define MAX_EP_LEN 100 //decsions before reset
#define PPO_BATCH_SIZE 2000 ///steps before training
