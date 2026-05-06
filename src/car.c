// #define PLATFORM_NONE 1
// #include "queue.c"
/*
Generic car declaration for maximum dynamism. Cars have multiple fields for physics simulation
based on the four wheeled bicycle model, as well as a very custom heuristic a* search that
encapsulates optimal paths cars should take, actual paths cars do take, and unexpected and illogical
paths cars take (like in the real world), using a custom heuristic engine.
No generative ai used in creation.
*/
// (TODO) Make it so a car's speed it normally distributed and can hit other cars just
//  taking into account the car in front of it and the speed limit based on a personally trait
// (TODO) is car spawning at regular intervals due to not spawning on top of eachother or something else?
// (TODO) why is the car sometimes off of the middle of a link? It has something to do with links
// being at weird parts of other links ends... it's fine for now
// (TODO) see what happens with multiple start links, do the cars spawn all at once at the beginning and
// maybe make it so cars can spawn closer to each other? and do they wait for each other? like stop and wait?
// (TODO) if the current implementation doesn't work add cars stopping and waiting for other cars to pass and for
// a link to become unblocked (like cars actually wait for the light to turn and if there is a car already waiting)
// then the car behind it will wait for the car in front that is stopped -- this is essential and should all work
// ^^ is most important
// (TODO) if there is no current path that is valid, don't spawn in
// (TODO) test ^ and paths vs. blocks vs. queues in general
// (TODO) test with multiple entrance/exit points
// (TODO) add collision detection
// (TODO) add model saving functionality
// (TODO) make explosions on crashes
// Let's define a car
typedef struct {
    f32 distance_along_link;
    f32 speed;
    f32 max_speed;
    Node_Ids path;
    // NOTE(felix): path_index needed?
    f32 start_time;
    // TODO(felix): make bit set for bools
    bool is_active;
    bool should_remove;
    bool is_following;
    f32 follow_gap;
    f32 follow_delta_v;

    bool highlighted;
    bool waiting_for_green;
} Car;
typedef Array(Car) Array_Car;

#define CAR_LENGTH 4.5f
#define CAR_WIDTH  1.8f
#define CAR_SIZE ((V2){ CAR_LENGTH, CAR_WIDTH })

static V2 car_position(Graph graph, Car car) {
    assert(car.path.count > 0);

    Node from = graph.nodes.data[car.path.data[0].i];
    V2 position = from.position;

    if (car.path.count > 1) {
        Node to = graph.nodes.data[car.path.data[1].i];

        V2 distance_xy = v2_sub(to.position, from.position);
        f32 distance = v2_len(distance_xy);
        V2 direction = v2_scale(distance_xy, 1.f / distance);

        position = v2_add(from.position, v2_scale(direction, car.distance_along_link));
    }

    return position;
}

// Box-Müller (normal distribution)
f32 rand_normal_dist(f32 mean, f32 stddev)
{
    // NOTE(felix): not sure if the comment immediately following is out of date after replacing libc rand() with our own prng.h functionality

    // Because below we do a slight variation to not get 0 or 1 it's
    // 	not perfectly uniform, can change this by using normal box-muller
    // 	function an getting two rand variables but for now it's okay
    //	... we just don't want to log 0 b/c crash and log(1) is unexpected

    prng_State prng = prng_init(0xcafebabecafebabe);
    f32 u1 = (f32)prng_next_f64_between_1_and_2(&prng) - 1.f;
    f32 u2 = (f32)prng_next_f64_between_1_and_2(&prng) - 1.f;
    u1 = CLAMP_LOW(u1, FLT_EPSILON);
    u2 = CLAMP_LOW(u2, FLT_EPSILON);

    f32 x = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * PI_F32 * u2);
    f32 result = mean + x * stddev;
    // clamp
    result = CLAMP(result, 0, 1.0f);
    return result;
}

// API implementations
// initialize the simulation environment and spawn cars
// collision log system
typedef struct {
    f32 timestamp;

    u64 car_a_index;
    u64 car_b_index;

    Node_Id a_from;
    Node_Id a_to;
    Node_Id b_from;
    Node_Id b_to;

    f32 a_distance_along_link;
    f32 b_distance_along_link;

    f32 a_speed;
    f32 b_speed;
} Collision_Log;
typedef Array(Collision_Log) Array_Collision_Log;

typedef struct {
    prng_State *prng;
    Arena *arena;
    Graph *graph;

    Array_Car cars;

    f32 simulation_time; // current simulation time, in seconds
    u64 collision_count;

    Array_Collision_Log collision_events;
} Simulation_State;

static void simulation_init(Simulation_State *state, Arena *arena, Graph *graph, prng_State *prng, u64 car_count) {
    state->arena = arena;
    state->prng = prng;
    state->graph = graph;

    // track last spawn time per entrance to space out cars
    f32* last_spawn_time_per_entrance = arena_make(state->arena, graph->entrances.count, f32);
    for (u64 i = 0; i < graph->entrances.count; i++) {
        last_spawn_time_per_entrance[i] = -2.0f; // allow first car at t=0
    }

    // spread car starts out over time
    reserve(state->arena, &state->cars, car_count);
    state->cars.count = 0;

    state->simulation_time = 0;

    u64 capacity = car_count * car_count;
    capacity += !capacity;
    reserve(state->arena, &state->collision_events, capacity);
    state->collision_events.count = 0;

    f32 start_time = 0;
    for (u64 i = 0; i < car_count; i++) {
        Car new_car = {
            .max_speed = 20.f, // in m/s, about 70kmh
        };

        new_car.start_time = start_time;
        start_time += 2.f;

        push_assume_capacity(&state->cars, new_car);
    }
}

typedef struct {
    Simulation_State *simulation;
    Car *car;
    u64 index;
} Simulation_Car_Iterator;

static Simulation_Car_Iterator simulation_car_iterator(Simulation_State *simulation) {
    Simulation_Car_Iterator it = {
        .simulation = simulation,
        .car = &simulation->cars.data[-1],
        .index = (u64)-1,
    };
    return it;
}

static bool simulation_car_next(Simulation_Car_Iterator *it) {
    do {
        it->index += 1;
    } while (it->index < it->simulation->cars.count && !it->simulation->cars.data[it->index].is_active);

    if (it->index == it->simulation->cars.count) return false;
    it->car = &it->simulation->cars.data[it->index];
    return true;
}

void simulation_step(Simulation_State* state, f32 dt) {
    state->simulation_time += dt;

    // Step all cars (movement, steering, etc.)
    for (u64 i = 0; i < state->cars.count; i++) {
        Car* car = &state->cars.data[i];

        // check if car should be active yet based on start_time
        if (!car->is_active) {
            if (state->simulation_time < car->start_time) continue;

            Graph *graph = state->graph;
            // NOTE(felix): I don't know when it should be ok for this not to be true
            assert(graph->entrances.count > 0 && graph->exits.count > 0);

            // pick a random entrance and exit for the car's path
            if (graph->entrances.count > 0 && graph->exits.count > 0) {
                car->is_active = true;

                u64 entrance_idx = prng_next_u64(state->prng) % graph->entrances.count;
                u64 exit_idx = prng_next_u64(state->prng) % graph->exits.count;
                Node_Id start_node = graph->entrances.data[entrance_idx];
                Node_Id end_node = graph->exits.data[exit_idx];

                // set initial position to start of entrance link
                car->path = graph_pathfind(state->arena, *graph, start_node, end_node);
            }
        }

        if (car->path.count <= 1) {
            car->should_remove = true;
        }

        // skip cars that are marked for removal
        if (car->should_remove) continue;

        // IDM follow model
        {
            car->is_following = false;
            car->follow_gap = FLT_MAX;
            car->follow_delta_v = 0.0f;

            if (car->path.count >= 1) for (u64 j = 0; j < state->cars.count; j += 1) if (j != i) {
                Car *other = &state->cars.data[j];
                if (!other->is_active || other->should_remove || other->path.count == 0) continue;
                if (other->path.data[0].i != car->path.data[0].i) continue;
                f32 gap = other->distance_along_link - car->distance_along_link;
                if (0 < gap && gap < car->follow_gap) {
                    car->is_following   = true;
                    car->follow_gap     = gap;
                    car->follow_delta_v = car->speed - other->speed;
                }
            }
        }

        Link_Id link_id = link_between_nodes(*state->graph, car->path.data[0], car->path.data[1]);
        Link *link = &state->graph->links.data[link_id.i];
        bool is_red_light = !!(link->flags & Link_Flag_TRAFFIC_LIGHT_RED);
        bool should_stop = !!(link->flags & LINK_FLAG_ANY_STOP);
        f32 link_len = link_length(*state->graph, *link);

        // stop target for stop links: end of segment, or behind queued car near end
        f32 stop_target = link_len;
        if (should_stop) {
            f32 desired_gap = CAR_LENGTH * 1.2f;
            for (u64 j = 0; j < state->cars.count; j += 1) if (j != i) {
                Car *other = &state->cars.data[j];
                if (!other->is_active || other->should_remove || other->path.count < 2) continue;
                if (other->path.data[0].i != car->path.data[0].i) continue;
                if (other->path.data[1].i != car->path.data[1].i) continue;

                // only consider cars ahead
                if (other->distance_along_link <= car->distance_along_link) continue;

                f32 candidate = other->distance_along_link - desired_gap;
                if (candidate < stop_target) stop_target = candidate;
            }
            // keep stop target near end and in-bounds
            f32 min_stop = link_len - 20.0f;
            stop_target = CLAMP(stop_target, min_stop, link_len);
        }

        // Latch behavior: once stopped for a red light, remain stopped until red clears.
        if (car->waiting_for_green && !is_red_light) {
            car->waiting_for_green = false;
        }

        f32 target_speed = 0;
        // Intelligent Driver Model: https://en.wikipedia.org/wiki/Intelligent_driver_model
        if (car->waiting_for_green) {
            target_speed = 0.0f;
        } else if (!should_stop) {
            f32 free_speed = car->max_speed;
            f32 max_acceleration = 4.f;
            f32 max_free = MAX(free_speed, 0.1f);
            f32 base = car->speed / max_free;

            // NOTE(felix): powf(base, 4.f), when base == 0, incorrectly gives 1.f on MSVC. This is the workaround
            f32 intermediate = base * base * base * base;

            f32 free_term = 1.f - intermediate;
            target_speed = MAX(car->speed + max_acceleration * free_term, 0.f);
        } else {
            // decelerate to stop at end (or behind queue)
            f32 remaining = stop_target - car->distance_along_link;
            if (remaining <= 0.0f) {
                target_speed = 0.0f;
            } else {
                // v^2 = 2*a*d with easy deceleration
                f32 comfortable_decel = 6.0f;
                target_speed = sqrtf(2.0f * comfortable_decel * remaining);
                target_speed = MIN(target_speed, car->max_speed);
            }
        }

        // simple following: stop behind a stopped || slow car ahead on the same link
        if (car->is_following) {
            f32 desired_gap = CAR_LENGTH * 1.2f;
            f32 available_gap = car->follow_gap - desired_gap;
            if (available_gap <= 0.0f) {
                target_speed = 0.0f;
            } else {
                f32 follow_limit = sqrtf(2.0f * 6.0f * available_gap);
                target_speed = MIN(target_speed, follow_limit);
            }
        }

        f32 acceleration = CLAMP((target_speed - car->speed) / dt, -8.f, 4.f);
        car->speed = CLAMP(car->speed + acceleration * dt, 0, car->max_speed);
        car->distance_along_link += car->speed * dt;

        if (should_stop && car->distance_along_link > stop_target) {
            car->distance_along_link = stop_target;
            car->speed = 0.0f;
        }

        // If we're effectively at the stop target on red, latch and hold.
        if (is_red_light && car->distance_along_link >= stop_target - 0.05f) {
            car->waiting_for_green = true;
            car->distance_along_link = MIN(car->distance_along_link, stop_target);
            car->speed = 0.0f;
        }

        // Do not advance to the next link while latched at red.
        if (!car->waiting_for_green && car->distance_along_link >= link_len) {
            car->path.data += 1;
            car->path.count -= 1;
            car->distance_along_link = 0;

            if (car->path.count == 0) car->should_remove = true;
        }
    }

    // collision detection/removal + event logging (graph-space overlap)
    for (u64 i = 0; i < state->cars.count; i++) {
        Car *a = &state->cars.data[i];
        if (!a->is_active || a->should_remove || a->path.count < 2) continue;

        V2 pos_a = car_position(*state->graph, *a);

        for (u64 j = i + 1; j < state->cars.count; j++) {
            Car *b = &state->cars.data[j];
            if (!b->is_active || b->should_remove || b->path.count < 2) continue;

            V2 pos_b = car_position(*state->graph, *b);
            f32 center_distance = v2_len(v2_sub(pos_a, pos_b));

            if (center_distance < (CAR_LENGTH - 1.5f)) {
                Collision_Log event = {
                    .timestamp = state->simulation_time,
                    .car_a_index = i,
                    .car_b_index = j,
                    .a_from = a->path.data[0],
                    .a_to = a->path.data[1],
                    .b_from = b->path.data[0],
                    .b_to = b->path.data[1],
                    .a_distance_along_link = a->distance_along_link,
                    .b_distance_along_link = b->distance_along_link,
                    .a_speed = a->speed,
                    .b_speed = b->speed,
                };
                push_assume_capacity(&state->collision_events, event);

                a->should_remove = true;
                b->should_remove = true;
                state->collision_count += 1;
            }
        }
    }

    // remove cars marked for removal (iterate backwards to avoid index issues)
    for (i64 i = (i64)state->cars.count - 1; i >= 0; i--) {
        if (state->cars.data[i].should_remove) {
            state->cars.data[i] = state->cars.data[--state->cars.count];
        }
    }
}

// Tests

// Run a complete simulation test and print results
static void test_simulation_full(void) {
    print("test_simulation_full\n");

    Arena test_arena = arena_init(1024 * 1024);

    print("init graph\n");
    Graph graph = graph_init((Map){0});
    print("links: %llu\n", graph.links.count);
    print("entrances: %llu\n", graph.entrances.count);
    print("exits: %llu\n", graph.exits.count);

    u64 num_cars = 10;
    print("init sim: %llu cars\n", num_cars);

    prng_State prng = prng_init(0xcafebabecafebabe);

    Simulation_State sim = {0};
    simulation_init(&sim, &test_arena, &graph, &prng, num_cars);
    print("cars created: %llu\n", sim.cars.count);
    print("start time: %f\n", sim.simulation_time);

    print("initial states:\n");
    for (u64 i = 0; i < sim.cars.count && i < 5; i++) {
        Car* c = &sim.cars.data[i];
        print("car %llu: active=%d pos=%f path_len=%llu\n",
            i, c->is_active, c->distance_along_link, c->path.count);
    }

    f32 dt = 0.1f;
    f32 total_time = 100.0f;
    i32 steps = (i32)(total_time / dt);
    u64 max_active = 0;
    u64 initial_car_count = sim.cars.count;

    for (i32 step = 0; step < steps; step++) {
        simulation_step(&sim, dt);
        u64 active_count = 0;
        for (u64 i = 0; i < sim.cars.count; i++) {
            if (sim.cars.data[i].is_active) active_count++;
        }
        if (active_count > max_active) max_active = active_count;
        if (step % 20 == 0) {
            print("t: %f | cars: %llu | active: %llu | col: %llu\n",
                sim.simulation_time, sim.cars.count, active_count, sim.collision_count);
        }
    }
    u64 cars_completed = initial_car_count - sim.cars.count - (sim.collision_count * 2);

    print("sim complete\n");
    print("total_time: %f\n", sim.simulation_time);
    print("initial: %llu\n", initial_car_count);
    print("remaining: %llu\n", sim.cars.count);
    print("completed: %llu\n", cars_completed);
    print("collisions: %llu\n", sim.collision_count);
    print("max_active: %llu\n", max_active);

    print("done\n");
}
