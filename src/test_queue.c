#include <stdlib.h>
#include <time.h>
#define DEPENDENCIES_NO_IMPLEMENTATION
#include "dependencies.c"
#include "base/base.c"
#include "shader.c"

typedef enum {
    LightState_GREEN,
    LightState_RED,
    LightState_COUNT,
} LightState;

structdef(Coordinate) {
    f32 x, y;
};

structdef(Car) {
    i32 id;
    i32 arrival_time;
    using(Coordinate, position);
};

structdef(Node) {
    Car car;
    struct Node* next;
};

structdef(Queue) {
    Arena *arena;
    Node* front;
    Node* rear;
    i32 size;
    i32 max_size;
};

// Traffic Light
structdef(TrafficLight) {
    LightState state;
    i32 timer;
    i32 green_duration;
    i32 red_duration;
};

void enqueue(Queue* queue, Car car) {
    Node *new_node = arena_make(queue->arena, 1, sizeof(Node));
    *new_node = (Node){
        .car = car
    };

    if (queue->rear == NULL) {
        queue->front = queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    queue->size++;
    if (queue->size > queue->max_size)
        queue->max_size = queue->size;
}

bool dequeue(Queue* queue, Car* out) {
    if (queue->front == NULL)
        return false;

    Node* temp = queue->front;
    *out = temp->car;

    queue->front = queue->front->next;
    if (queue->front == NULL)
        queue->rear = NULL;

    queue->size--;
    return true;
}

void update_traffic_light(TrafficLight* light) {
    light->timer--;
    if (light->timer <= 0) {
        if (light->state == LightState_GREEN) {
            light->state = LightState_RED;
            light->timer = light->red_duration;
        } else {
            light->state = LightState_GREEN;
            light->timer = light->green_duration;
        }
    }
}

bool car_arrives(f64 probability) {
    return ((f64)rand() / (RAND_MAX + 1.0)) < probability;
}

static void program(void) {
    // Arena arena = arena_init(8 * 1024 * 1024);
    srand((u32)time(NULL));

    const i32 simulation_duration = 300;// 300 ticks
    const f64 arrival_rate = 0.3;//30%
    const i32 green_duration = 10;
    const i32 red_duration = 10;

    TrafficLight light = {
        .state = LightState_GREEN,
        .timer = green_duration,
        .green_duration = green_duration,
        .red_duration = red_duration,
    };

    Arena arena = arena_init(16 * 1024 * 1024);
    Queue queue = { .arena = &arena };

    i32 tick = 0;
    i32 car_id = 1;
    i32 total_arrived = 0;
    i32 total_served = 0;
    i32 total_wait_time = 0;

    while (tick < simulation_duration) {
        //vehicle arrives
        if (car_arrives(arrival_rate)) {
            Car car = {
                .id = car_id++,
                .arrival_time = tick,
                .position = {0.0f, 0.0f}
            };
            enqueue(&queue, car);
            total_arrived++;
        }

        //update light
        update_traffic_light(&light);

        //go car if light is green
        if (light.state == GREEN && queue.size > 0) {
            Car served;
            if (dequeue(&queue, &served)) {
                i32 wait_time = tick - served.arrival_time;
                total_wait_time += wait_time;
                total_served++;
            }
        }

        //logging
        if (tick % 10 == 0) {
            print("[Tick %] Light: % | Queue size: %\n",
                  fmt(i32, tick),
                  fmt(cstring, light.state == GREEN ? "GREEN" : "RED"),
                  fmt(i32, queue.size));
        }

        tick++;
    }

    //results
    f64 avg_wait = total_served > 0 ? ((f64)total_wait_time / total_served) : 0;

    print("\n===== Simulation Complete =====\n");
    print("Total Vehicles Arrived: %\n", fmt(i32, total_arrived));
    print("Total Vehicles Served: %\n", fmt(i32, total_served));
    print("Average Wait Time: %.2f ticks\n", fmt(f64, avg_wait));
    print("Maximum Queue Length: %\n", fmt(i32, queue.max_size));
}
