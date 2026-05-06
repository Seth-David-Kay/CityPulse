#define BASE_IMPLEMENTATION
#include "base/base.h"

meta(generate_here)
#include "src/meta.generated.c"

#if BASE_OS == BASE_OS_WASM
    meta(embed "deps/Chicago-12.bdf")
    extern const char platform__font_bytes[];
    extern const u64 platform__font_size;

    meta(embed "map_intersection.bin")
    extern const char intersection_bytes[];
    extern const u64 intersection_size;

    meta(embed "map_roundabout.bin")
    extern const char roundabout_bytes[];
    extern const u64 roundabout_size;

    meta(platform)
    #include "base/base_platform_wasm.c"
#else

    // // TODO(felix): move embed into platform files after adding ability to parse #includes
    // meta(embed "deps/Inter-4.1/InterVariable.ttf")
    // extern const char platform__font_ttf_chars[];
    // extern const u64 platform__font_ttf_size;
    // meta(platform)
    // #include "base/base_platform_sokol.c"

    // meta(embed "deps/scientifica-11.bdf")
    // extern const char platform__font_bytes_scientifica[];
    // extern const u64 platform__font_size_scientifica;
    meta(embed "deps/Chicago-12.bdf")
    extern const char platform__font_bytes[];
    extern const u64 platform__font_size;
    // meta(embed "deps/spleen-8x16.bdf")
    // extern const char platform__font_bytes_spleen[];
    // extern const u64 platform__font_size_spleen;
    meta(platform)
    #include "base/base_platform_wingdi.c"

    // for asan
    meta(link_libc = true)

#endif

static void app_quit(Platform *platform) {
    (void)platform;
}

#include "graph.c"
#include "queue.c"
#include "car.c"
#include "ppo.c"

#define MAX_INTERSECTIONS 16
static PPO_Context *global_ppos[MAX_INTERSECTIONS] = {0};
static u32 active_ppo_count = 0;

#define LIGHT_GREEN_COLOR 0x859900ff
#define LIGHT_YELLOW_COLOR 0xb58900ff
#define LIGHT_RED_COLOR 0xdc322fff

static const char *tutorial_tutorial_text[] = {
    "To exit or re-enter the tutorial at",
    "time, click the tutorial button in the",
    "top right, or press m!",
    0,
};

static const char *motivation_text[] = {
    "CityPulse was created to explore",
    "how reinforcement learning can",
    "improve city traffic light.",
    "timings, unmoderated.",
    "Wanting full control over the",
    "system, we built everything from",
    "scratch in C with no external",
    "libraries.",
    0,
};

static const char *innovation_text[] = {
    "CityPulse is written entirely in C",
    "so we can control every part of the",
    "program. We wrote our own build",
    "system, UI system, and supporting",
    "modules instead of relying on outside",
    "frameworks. This keeps the app small,",
    "fast, and lean.",
    0,
};

static const char *implementation_text[] = {
    "The top bar contains the main controls for",
    "the program. It includes zoom controls,",
    "simulation speed controls, reset, play and",
    "pause, and more. The reset button will restart",
    "the simulation but not the agent.",
    0,
};

static const char *map_controls_text[] = {
    "The main view shows the traffic network as",
    "a graph. Intersections are shown as circles,",
    "and roads are shown as  lines. You can select",
    "lights and roads. Doing so will show you more",
    "information about them on the right panels of",
    "the screen,  and using those same panels you",
    "are able to remove, add, or edit traffic lights",
    "and roads to create a fully custom network.",
    "You can even download your custom network!",
    0,
};

static const char *car_and_ppo_text[] = {
    "The left side of the screen displays",
    "useful information during the simulation. At",
    "the top you can see how well the agent",
    "is performing, and at the bottom you can",
    "see each individual car and more information   ",
    "about it.",
    0,
};

typedef struct {
    u32 override_fg_color;
    ui_Style ui_idle_style;
    ui_Style ui_disabled_style;
    ui_Style ui_click_style;
    ui_Style ui_dragging_style;
    ui_State ui;
    Graph graph;
    prng_State prng;
    PPO_Info ppo_info;

    f32 simulation_speed;
    u64 simulation_num_cars;
    Simulation_State simulation;
    Arena simulation_arena;

    struct {
        bool interacting;
        V2 anchor_world, anchor_screen;
        V2 display_offset, target_offset, computed_offset_to_fit;
        f32 display_zoom, target_zoom, computed_zoom_to_fit_;
        bool resetting_to_fit;
    } view;

    Node_Id selected_node;
    Link_Id selected_link;

    Array(Map) maps;
} Context;
static Context g;

static serial_Value read(serial_Reader *r, serial_Type type) {
    serial_Value v = serial_read(r);
    assert(v.type == type);
    return v;
}

static serial_Value read_after_string(serial_Reader *r, const char *name, serial_Type type) {
    serial_Value v = read(r, serial_Type_STRING);
    assert(cstring_equals(v.string, name));
    v = read(r, type);
    return v;
}

static void write_named_f64(serial_Writer *w, const char *name, f64 value) {
    serial_write_cstring(w, name);
    serial_write_f64(w, value);
}

static void app_start(Platform *platform) {
    platform->clear_color = 0xe5e5e5ff;

    ui_create(&g.ui, &platform->frame, base_ui_measure_text);
    g.ui.user_data = platform;

    const char *filenames[] = { "map_intersection.bin", "map_roundabout.bin", 0 };

    for (const char **filename = filenames; *filename != 0; filename += 1) {
        Map map = {0};

        String file = {0};
        #if BASE_OS == BASE_OS_WASM
        {
            if (cstring_equals(*filename, "map_intersection.bin")) {
                file.data =  intersection_bytes;
                file.count = intersection_size;
            } else if (cstring_equals(*filename, "map_roundabout.bin")) {
                file.data = roundabout_bytes;
                file.count = roundabout_size;
            } else unreachable;
        }
        #else
        {
            Arena scratch = *platform->frame_arena;
            file = os_read_entire_file(&scratch, *filename);
        }
        #endif
        assert(file.count > 0);

        serial_Reader r = serial_reader(file.data, (int)file.count);
        serial_Value v = {0};

        v = read_after_string(&r, "maps", serial_Type_F64);
        assert(v.f64 == 1.0);

        v = read(&r, serial_Type_ARRAY);
        {
            v = read(&r, serial_Type_OBJECT);
            {
                v = read_after_string(&r, "links", serial_Type_F64);
                map.link_count = (u32)v.f64;
                map.links = arena_make(platform->persistent_arena, (u64)map.link_count, Link);

                v = read(&r, serial_Type_ARRAY);
                for (u32 i = 0; i < map.link_count; i += 1) {
                    Link link = {0};
                    v = read(&r, serial_Type_OBJECT);
                    {
                        v = read_after_string(&r, "from", serial_Type_F64);
                        link.from.i = (u32)v.f64;

                        v = read_after_string(&r, "to", serial_Type_F64);
                        link.to.i = (u32)v.f64;

                        v = serial_read(&r);
                        while (v.type == serial_Type_STRING) {
                            if (cstring_equals(v.string, "flags")) {
                                v = read(&r, serial_Type_F64);
                                link.flags = (Link_Flags)v.f64;
                            } else if (cstring_equals(v.string, "intersection_id")) {
                                v = read(&r, serial_Type_F64);
                                link.intersection_id = (u8)v.f64;
                            } else unreachable;

                            v = serial_read(&r);
                        }
                    }
                    assert(v.type == serial_Type_END);
                    map.links[i] = link;
                }
                v = read(&r, serial_Type_END);

                v = read_after_string(&r, "nodes", serial_Type_F64);
                map.node_count = (u32)v.f64;
                map.nodes = arena_make(platform->persistent_arena, (u64)map.node_count, Node);

                v = read(&r, serial_Type_ARRAY);
                for (u32 i = 0; i < map.node_count; i += 1) {
                    Node node = {0};
                    v = read(&r, serial_Type_OBJECT);

                    v = read_after_string(&r, "x", serial_Type_F64);
                    node.position.x = (f32)v.f64;

                    v = read_after_string(&r, "y", serial_Type_F64);
                    node.position.y = (f32)v.f64;

                    v = serial_read(&r);
                    if (v.type == serial_Type_STRING && cstring_equals(v.string, "flags")) {
                        v = read(&r, serial_Type_F64);
                        node.flags = (Node_Flags)v.f64;
                        v = read(&r, serial_Type_END);
                    } else assert(v.type == serial_Type_END);

                    map.nodes[i] = node;
                }
                v = read(&r, serial_Type_END);

                v = read_after_string(&r, "entrances", serial_Type_F64);
                map.entrances.count = (u32)v.f64;
                map.entrances.data = arena_make(platform->persistent_arena, (u64)map.entrances.count, Node_Id);

                v = read(&r, serial_Type_ARRAY);
                for (u32 i = 0; i < map.entrances.count; i += 1) {
                    v = read(&r, serial_Type_F64);
                    Node_Id id = { .i = (u32)v.f64 };
                    map.entrances.data[i] = id;
                }
                v = read(&r, serial_Type_END);

                v = read_after_string(&r, "exits", serial_Type_F64);
                map.exits.count = (u32)v.f64;
                map.exits.data = arena_make(platform->persistent_arena, (u64)map.exits.count, Node_Id);

                v = read(&r, serial_Type_ARRAY);
                for (u32 i = 0; i < map.exits.count; i += 1) {
                    v = read(&r, serial_Type_F64);
                    Node_Id id = { .i = (u32)v.f64 };
                    map.exits.data[i] = id;
                }
                v = read(&r, serial_Type_END);
            }
            v = read(&r, serial_Type_END);
        }
        v = read(&r, serial_Type_END);

        push(platform->persistent_arena, &g.maps, map);
    }
    g.graph = graph_init(g.maps.data[0]);
    graph_clear_lights_to_nodes(&g.graph, 5, 7);

    Intersection_Config config_1 = ppo_auto_config(&g.graph, 1);
    global_ppos[0] = ppo_make(platform->persistent_arena, 4242, config_1);
    active_ppo_count = 1;

    g.prng = prng_init(0xcafebabecafebabe);

    g.simulation_speed = 1.f;
    g.simulation_arena = arena_init(32 * 1024 * 1024);
    g.simulation_num_cars = 500;
    simulation_init(&g.simulation, &g.simulation_arena, &g.graph, &g.prng, g.simulation_num_cars);
}

static V2 screen_from_world(V2 world, V2 camera, f32 zoom, V2 screen_centre) {
    V2 result = v2_sub(world, camera);
    result = v2_scale(result, zoom);
    result = v2_add(result, screen_centre);
    return result;
}

// NOTE(felix): brilliant source for public domain bitmap fonts: https://www.pentacom.jp/pentacom/bitfontmaker2/gallery/

static ui_Box *app_ui_push_header_panel(ui_Box *parent, const char *format, ...) {
    ui_Box *box = ui_push(&g.ui, parent, ui_Y | ui_Flag_DRAW_BACKGROUND | ui_Flag_DRAW_SHADOW | ui_Flag_DRAW_BORDER, &g.ui_idle_style, 0);
    box->size[ui_Y] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
    box->style.padding[ui_X] = 0;
    box->style.padding[ui_Y] = 0;

    va_list arguments;
    va_start(arguments, format);
    ui_Box *header = ui_pushv(&g.ui, box, ui_Flag_DRAW_BACKGROUND | ui_Flag_DRAW_SHADOW | ui_Flag_DRAW_TEXT, &g.ui_idle_style, format, arguments);
    va_end(arguments);
    header->style = g.ui_click_style;
    header->style.bg_color = 0xc0c0c0ff;
    header->size[ui_X] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
    header->size[ui_Y] = (ui_Size){ .kind = ui_Size_TEXT };

    return box;
}

typedef enum {
    Text_DISABLED = 1 << 0,
} Text_Flags;

static ui_Box *text(ui_Box *parent, Text_Flags text_flags, const char *format, ...) {
    ui_Flags flags = ui_Flag_DRAW_TEXT;

    va_list arguments;
    va_start(arguments, format);
    ui_Style style = (text_flags & Text_DISABLED) ? g.ui_disabled_style : g.ui_idle_style;
    if (g.override_fg_color != 0) style.fg_color = g.override_fg_color;
    ui_Box *box = ui_pushv(&g.ui, parent, flags, &style, format, arguments);
    va_end(arguments);

    for (ui_Flags axis = 0; axis < ui_Axis_COUNT; axis += 1) box->size[axis].kind = ui_Size_TEXT;
    return box;
}

#define BUTTON_FLAGS (ui_Flag_DRAW_BACKGROUND | ui_Flag_DRAW_BORDER | ui_Flag_DRAW_SHADOW | ui_Flag_HOVERABLE | ui_Flag_CLICKABLE)

static Draw_Command circle_sprite_draw_command = {
    .kind = Draw_Kind_SPRITE_1BIT,
    .sprite = {
        .width = 9,
        .height = 9,
        .bytes = {
            BYTE_FROM_BITS(0,0,1,1,1,1,1,0), BYTE_FROM_BITS(0, 0,0,0,0,0,0,0),
            BYTE_FROM_BITS(0,1,1,1,1,1,1,1), BYTE_FROM_BITS(0, 0,0,0,0,0,0,0),
            BYTE_FROM_BITS(1,1,1,1,1,1,1,1), BYTE_FROM_BITS(1, 0,0,0,0,0,0,0),
            BYTE_FROM_BITS(1,1,1,1,1,1,1,1), BYTE_FROM_BITS(1, 0,0,0,0,0,0,0),
            BYTE_FROM_BITS(1,1,1,1,1,1,1,1), BYTE_FROM_BITS(1, 0,0,0,0,0,0,0),
            BYTE_FROM_BITS(1,1,1,1,1,1,1,1), BYTE_FROM_BITS(1, 0,0,0,0,0,0,0),
            BYTE_FROM_BITS(1,1,1,1,1,1,1,1), BYTE_FROM_BITS(1, 0,0,0,0,0,0,0),
            BYTE_FROM_BITS(0,1,1,1,1,1,1,1), BYTE_FROM_BITS(0, 0,0,0,0,0,0,0),
            BYTE_FROM_BITS(0,0,1,1,1,1,1,0), BYTE_FROM_BITS(0, 0,0,0,0,0,0,0),
        },
    },
};

static ui_Box *buttonv(ui_Box *parent, const Draw_Command *command, App_Key keybind, Text_Flags text_flags, const char *format, va_list arguments) {
    ui_Flags flags = BUTTON_FLAGS;
    flags |= (command == 0) * ui_Flag_DRAW_TEXT;
    flags |= (command != 0) * ui_Flag_DRAW_COMMAND;

    bool disabled = !!(text_flags & Text_DISABLED);
    if (disabled) flags &= ~ui_Flag_DRAW_SHADOW;

    ui_Style style = disabled ? g.ui_disabled_style : g.ui_idle_style;
    ui_Box *box = ui_pushv(&g.ui, parent, flags, &style, format, arguments);

    if (command == 0) {
        for (ui_Flags axis = 0; axis < ui_Axis_COUNT; axis += 1) box->size[axis].kind = ui_Size_TEXT;
    } else {
        box->user_data = (void *)command;

        V2 bounds = draw_command_bounds(*command);
        for (ui_Flags axis = 0; axis < ui_Axis_COUNT; axis += 1) {
            box->size[axis] = (ui_Size){ .kind = ui_Size_PIXELS, .value = v(bounds)[axis] };
        }
    }

    bool key_pressed = !disabled && keybind != 0 && g.ui.frame->key_pressed[keybind];
    box->flags |= ui_Flag_CLICKED * key_pressed;
    if (disabled) box->flags &= ~ui_Flag_CLICKED;

    bool use_click_style = (box->flags & ui_Flag_CLICKED) || ((box->flags & ui_Flag_HOVERED) && g.ui.frame->mouse_down[App_Mouse_LEFT]);
    use_click_style = use_click_style && !disabled;
    if (use_click_style) box->style = g.ui_click_style;

    return box;
}

static ui_Box *button(ui_Box *parent, const Draw_Command *command, App_Key keybind, Text_Flags text_flags, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    ui_Box *b = buttonv(parent, command, keybind, text_flags, format, arguments);
    va_end(arguments);
    return b;
}

static bool numeric_button_lock_;
static u32 numeric_button(App_Frame_Info frame, u32 value, ui_Box *parent, bool *active, Text_Flags flags, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    ui_Box *b = buttonv(parent, 0, 0, flags, format, arguments);
    va_end(arguments);

    if (numeric_button_lock_) return value;

    if (b->flags & ui_Flag_CLICKED) {
        *active = !(*active);
    }

    if (*active) {
        b->style = g.ui_click_style;

        for (App_Key key = '0'; key <= '9'; key += 1) if (frame.key_pressed[key]) {
            value = (u32)key - '0';
            *active = false;
            break;
        }
    }

    if (*active) {
        value -= (u32)frame.key_pressed['-'];
        value += (u32)frame.key_pressed['='];
    }

    if (*active) numeric_button_lock_ = true;

    return value;
}

static f32 computed_zoom_to_fit(Platform *platform) {
    f32 zoom = g.view.computed_zoom_to_fit_;
    zoom /= ((Platform_Implementation *)platform)->real_over_virtual_window_scale;
    return zoom;
}

typedef u64 Draw_Data;
#define Draw_Data_NODE ((u64)1 << 63)
#define Draw_Data_LINK ((u64)1 << 62)
#define DRAW_DATA_INDEX_MASK 0xffffffff

static void app_update_and_render(Platform *platform) {
    numeric_button_lock_ = false;
    if (platform->frame.key_pressed['Q']) platform->should_quit = true;
    static V2 last_screen_size;

    {
        f32 scale = platform->frame.dpi_scale;
        g.ui_idle_style = (ui_Style){
            .font_size = 12.f * scale,
            .padding = { 5.f * scale, 4.f * scale },
            .child_gap = 3.f * scale,
            .border_width = 1.f * scale,
            .border_radius = 2.f * scale,
            .shadow_offset = { 0.f * scale, 1.f * scale },
            .fg_color = 0x000000ff,
            .shadow_color = 0x6a6a6aff,
            .bg_color = 0xd8d8d8ff,
            .border_color = 0xb0b0b0ff,
        };

        g.ui_disabled_style = g.ui_idle_style;
        g.ui_disabled_style.fg_color = 0x909090ff;

        g.ui_click_style = g.ui_idle_style;
        g.ui_click_style.bg_color = 0x9b9b9bff;
        g.ui_click_style.border_color = 0x515151ff;

        g.ui_dragging_style = g.ui_idle_style;
        g.ui_dragging_style.bg_color = 0x4c7af7ff;
        g.ui_dragging_style.border_color = 0x000000ff;
    }

    bool map_view_reset = false;

    bool screen_changed = !v2_difference_within_margin(last_screen_size, platform->frame.window_size, FLT_EPSILON);
    V2 screen_centre = v2_scale(platform->frame.window_size, 0.5f);
    if (screen_changed) {
        V2 average_node_centre = {0};
        f32 min_x = FLT_MAX;
        f32 min_y = FLT_MAX;
        f32 max_x = -FLT_MAX;
        f32 max_y = -FLT_MAX;

        for (Node_Iterator it = node_iterator(g.graph.nodes.data, g.graph.nodes.count); node_iterate(&it);) {
            Node node = *it.node;

            average_node_centre = v2_add(average_node_centre, node.position);

            min_x = MIN(min_x, node.position.x);
            min_y = MIN(min_y, node.position.y);
            max_x = MAX(max_x, node.position.x);
            max_y = MAX(max_y, node.position.y);
        }
        average_node_centre = v2_scale(average_node_centre, 1.f / (f32)g.graph.links.count);

        V2 world = { max_x - min_x, max_y - min_y };
        V2 zoom = v2_div(platform->frame.window_size, world);
        g.view.computed_zoom_to_fit_ = MIN(zoom.x, zoom.y) * 2.f;
        g.view.computed_offset_to_fit = average_node_centre;
        g.view.computed_offset_to_fit.y += 0.02f * platform->frame.window_size.y;
    }

    static bool init = false;
    if (!init) {
        assert(screen_changed);

        g.view.target_zoom = computed_zoom_to_fit(platform);
        g.view.display_zoom = g.view.target_zoom; // don't lerp from 0 on launch
        g.view.target_offset = g.view.computed_offset_to_fit;
        g.view.display_offset = g.view.target_offset;
        g.view.anchor_world = g.view.display_offset;
        g.view.anchor_screen = screen_centre;

        init = true;
    }

    Array_Draw_Command map_items = {0};

    static bool paused = false;

    ui_State *ui = &g.ui;
    ui_begin(ui);
    ui->input_blocked = g.view.interacting;

    ui_Box *ui_root = ui_push(ui, 0, ui_Y, 0, 0);

    ui_Style bar_style = g.ui_idle_style;
    bar_style.bg_color = 0xc0c0c0ff;
    bar_style.border_width = 0;
    bar_style.border_radius = 0;
    bar_style.padding[ui_X] = 4.f;
    bar_style.padding[ui_Y] = 2.f;

    bool reset_this_frame = false;

    static bool tutorial_mode = true;

    ui_Flags bar_flags = (ui_Flags)(ui_X) | ui_Flag_DRAW_BACKGROUND | ui_Flag_DRAW_SHADOW;
    ui_Box *menubar = &(ui_Box){0};
    ui_Box *tutorial_menubar = &(ui_Box){0};
    {
        ui_Box *bar = ui_push(ui, ui_root, bar_flags, &bar_style, 0);
        bar->size[ui_X] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };

        menubar = bar;
        tutorial_menubar = bar;
    }
    {
        text(tutorial_menubar, 0, "CityPulse");
        ui_spacer(ui, tutorial_menubar);
        text(tutorial_menubar, 0, "by Nawwaf Aleisa, Seth David Kay, and Felix Ulmanu");
    }
    {

        // TODO(felix): map selector button

        #if defined(BASE_PLATFORM_WINGDI) || defined(BASE_PLATFORM_WASM)
        {
            f32 *scale = &((Platform_Implementation *)platform)->real_over_virtual_window_scale;
            f32 least = 1.f;
            f32 most = 5.f;

            bool less = !!(button(menubar, 0, 0, *scale <= least ? Text_DISABLED : 0, "-##ui scale control")->flags & ui_Flag_CLICKED);
            bool more = !!(button(menubar, 0, 0, *scale >= most ? Text_DISABLED : 0, "+##ui scale control")->flags & ui_Flag_CLICKED);
            if (less) *scale = CLAMP_LOW(*scale - 1.f, least);
            if (more) *scale = CLAMP_HIGH(*scale + 1.f, most);

            text(menubar, 0, "UI###ui scale label");

            ui_spacer(ui, menubar);
        }
        #endif


        text(menubar, 0, "Zoom:###");
        {
            f32 zoom_percent_max_width = platform_measure_text(platform, S("0000%"), g.ui_idle_style.font_size).x;
            ui_Box *zoom_percent = text(menubar, 0, "%4d%%###zoom level", (int)roundf(g.view.target_zoom * 100.f));
            zoom_percent->size[ui_X] = (ui_Size){ .kind = ui_Size_PIXELS, .value = zoom_percent_max_width };

            f32 least_zoom = 0.125f;
            f32 most_zoom = 16.f;

            f32 handle_radius = 10.f * platform->frame.dpi_scale;
            ui_slider(ui, menubar, ui_X, least_zoom, most_zoom, &g.view.target_zoom, (ui_Slider_Style){
                .bar = g.ui_click_style,
                .dragging = g.ui_dragging_style,

                .length = 50.f * platform->frame.dpi_scale,
                .thickness = 3.f * platform->frame.dpi_scale,
                .radius = 1.f * platform->frame.dpi_scale,
                .handle_length = handle_radius,
                .handle_thickness = handle_radius,
                .handle_radius = handle_radius * 0.5f,
            }, "zoom slider");

            ui_Box *map_view_reset_button = button(menubar, 0, 'F', 0, "Fit##zoom");
            map_view_reset = map_view_reset || (map_view_reset_button->flags & ui_Flag_CLICKED);
        }

        ui_spacer(ui, menubar);

        f32 min_sim_speed = 0.125f;
        f32 max_sim_speed = 32.f;

        assert(CPU_DRAW_SPRITE_MAX_WIDTH_OVER_8 == 2); // otherwise below literal bytes are wrong
        static Draw_Command slow_icon = {
            .kind = Draw_Kind_SPRITE_1BIT,
            .sprite = {
                .width = 8,
                .height = 7,
                .bytes = {
                    BYTE_FROM_BITS(0,0,0,1,1,0,1,1), 0,
                    BYTE_FROM_BITS(0,0,1,1,0,1,1,0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0,0), 0,
                    BYTE_FROM_BITS(1,1,0,1,1,0,0,0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0,0), 0,
                    BYTE_FROM_BITS(0,0,1,1,0,1,1,0), 0,
                    BYTE_FROM_BITS(0,0,0,1,1,0,1,1), 0,
                },
            },
        };
        ui_Box *slow_button = button(menubar, &slow_icon, 0, g.simulation_speed <= min_sim_speed ? Text_DISABLED : 0, "slow button");

        text(menubar, 0, "%.2fx###simulation speed text", g.simulation_speed);

        static Draw_Command fast_icon = {
            .kind = Draw_Kind_SPRITE_1BIT,
            .sprite = {
                .width = 8,
                .height = 7,
                .bytes = {
                    BYTE_FROM_BITS(1,1,0,1,1,0,0,0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0,0), 0,
                    BYTE_FROM_BITS(0,0,1,1,0,1,1,0), 0,
                    BYTE_FROM_BITS(0,0,0,1,1,0,1,1), 0,
                    BYTE_FROM_BITS(0,0,1,1,0,1,1,0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0,0), 0,
                    BYTE_FROM_BITS(1,1,0,1,1,0,0,0), 0,
                },
            },
        };
        ui_Box *fast_button = button(menubar, &fast_icon, 0, g.simulation_speed >= max_sim_speed ? Text_DISABLED : 0, "fast button");

        bool should_slow_down = !!(slow_button->flags & ui_Flag_CLICKED);
        bool should_speed_up = !!(fast_button->flags & ui_Flag_CLICKED);
        {
            f32 multiply = 1.f;
            if (should_slow_down) multiply = 0.5f;
            if (should_speed_up) multiply = 2.f;
            g.simulation_speed = CLAMP(g.simulation_speed * multiply, min_sim_speed, max_sim_speed);
        }

        ui_spacer(ui, menubar);

        static Draw_Command play_icon = {
            .kind = Draw_Kind_SPRITE_1BIT,
            .sprite = {
                .width = 7,
                .height = 9,
                .bytes = {
                    BYTE_FROM_BITS(0,1,1,0,0,0,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,1,0,0,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,1,1,0,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,1,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,1,1,1,1, 0), 0,
                    BYTE_FROM_BITS(0,1,1,1,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,1,1,0,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,1,0,0,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,0,0,0,0, 0), 0,
                },
            },
        };
        static Draw_Command pause_icon = {
            .kind = Draw_Kind_SPRITE_1BIT,
            .sprite = {
                .width = 7,
                .height = 8,
                .bytes = {
                    BYTE_FROM_BITS(0,1,1,0,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0, 0), 0,
                    BYTE_FROM_BITS(0,1,1,0,1,1,0, 0), 0,
                },
            },
        };

        ui_Box *reset_sim_button = button(menubar, 0, 'R', 0, "Reset##simulation button");
        if (reset_sim_button->flags & ui_Flag_CLICKED) {
            reset_this_frame = true;
        }

        ui_Box *play_pause_button = button(menubar, paused ? &play_icon : &pause_icon, 'P', 0, "play/pause button");
        if (play_pause_button->flags & ui_Flag_CLICKED) paused = !paused;

        {
            f32 s = g.simulation.simulation_time;
            text(menubar, 0, "%02d:%02d:%02d###runtime", (int)(s / 60.f), (int)s % 60, (int)(s * 100.f) % 100);
        }

        ui_Style circle_style = g.ui_idle_style;

        u32 pause_color = 0xa04040ff;
        u32 going_color = 0xff0000ff;
        static f32 animation_time;

        f32 animation_cycle_time = 2.f;
        animation_time = g.simulation.simulation_time;
        animation_time = fmodf(animation_time, animation_cycle_time);
        bool alternate_color = animation_time > 0.5f * animation_cycle_time;

        circle_style.fg_color = (paused || alternate_color) ? pause_color : going_color;
        base_ui_draw_command(platform->frame_arena, ui, menubar, circle_sprite_draw_command, ui_Flag_DRAW_COMMAND, &circle_style, "circle indicator");

        ui_spacer(ui, menubar);

        slow_button->size[ui_Y] = fast_button->size[ui_Y] = play_pause_button->size[ui_Y] = (ui_Size){
            .kind = ui_Size_OF_PARENT,
            .value = 1.f,
        };

        text(menubar, 0, "%.0fFPS###fps in menubar", 1.f / platform->frame.seconds_since_last_frame);

        ui_Box *tutorial_mode_toggle_button = button(menubar, 0, 'M', 0, "Tutorial##tutorial button");
        if (tutorial_mode_toggle_button->flags & ui_Flag_CLICKED) {
            tutorial_mode = !tutorial_mode;
        }

        {
            Arena *a = platform->persistent_arena;
            Arena *f = platform->frame_arena;
            f32 ui_percent = (f32)ui->display_string_used / (f32)(sizeof ui->display_string_buffer) * 100.f;
            text(menubar, 0, "mem:%.0f%%,%.0f%%,%.0f%%###mem in menubar", (f32)a->offset / (f32)a->capacity * 100.f, (f32)f->offset / (f32)f->capacity * 100.f, ui_percent);
        }
    }

    if (!paused) {
        // TODO(felix): for determinism, the simulation_step should always run with the same dt (or up to a set limit that we establish doesn't break the simulation), and beyond that point we should run MORE steps per frame, rather than increasing dt more

        f32 step = g.simulation_speed * platform->frame.seconds_since_last_frame;
        simulation_step(&g.simulation, step);
        //loop through all intersections
        for (u32 i = 0; i < active_ppo_count; i++){
            ppo_update(&g.ppo_info, global_ppos[i], &g.simulation, &g.graph, step);
        }
    }

    static bool all_highlighted = false;
    if (platform->frame.key_pressed['H']) all_highlighted = !all_highlighted;

	for (Link_Iterator it = link_iterator(g.graph.links.data, g.graph.links.count); link_iterate(&it);) {
        Link *link = it.link;
        Node from = g.graph.nodes.data[link->from.i];
        Node to = g.graph.nodes.data[link->to.i];

        Draw_Command command = {
            .kind = Draw_Kind_LINE,
            .position = from.position,
            .line = {
                .end = to.position,
                .thickness = 1.f,
            },
            .user_data = (u64)((u32)it.i | Draw_Data_LINK),
        };
        command.color[Draw_Color_SOLID] = 0x0080a0ff;

        if (link->flags & LINK_FLAG_ANY_TRAFFIC_LIGHT) {
            f32 light_radius = 2.f * platform->frame.dpi_scale;

            u32 color = 0;
            if (link->flags & Link_Flag_TRAFFIC_LIGHT_RED) color = LIGHT_RED_COLOR;
            if (link->flags & Link_Flag_TRAFFIC_LIGHT_YELLOW) color = LIGHT_YELLOW_COLOR;
            if (link->flags & Link_Flag_TRAFFIC_LIGHT_GREEN) color = 0x40c020ff;

            Draw_Command light = {
                .kind = Draw_Kind_RECTANGLE,
                .position = v2_sub(to.position, v2(light_radius * 2.f)),
                .color[Draw_Color_SOLID] = color,
                .rectangle = {
                    .size = v2(light_radius * 2.f),
                    .border_radius = light_radius,
                    .border_width = 1.f * platform->frame.dpi_scale,
                    .border_color = 0x505050ff,
                },
            };
            push(platform->frame_arena, &map_items, light);
        }

        push(platform->frame_arena, &map_items, command);
    }

    if (platform->frame.key_pressed['O']) {
        g.selected_node.i = 1;
        g.selected_link.i = 1;
    }

    f32 node_draw_radius = 2.f * platform->frame.dpi_scale;
    for (Node_Iterator it = node_iterator(g.graph.nodes.data, g.graph.nodes.count); node_iterate(&it);) {
        Node node = *it.node;
        u32 i = (u32)it.i;

        bool is_selected = i == (u64)g.selected_node.i;

        Draw_Command command = {
            .kind = Draw_Kind_RECTANGLE,
            .position = v2_sub(node.position, v2(node_draw_radius)),
            .color[Draw_Color_SOLID] = is_selected ? 0x00ff00ff : 0x40d0d0ff,
            .rectangle = {
                .size = v2(node_draw_radius * 2.f),
                .border_radius = node_draw_radius,
                .border_width = 1.f * platform->frame.dpi_scale,
                .border_color = is_selected ? 0x004020ff : 0x009040ff,
            },
            .user_data = (u64)(i | Draw_Data_NODE),
        };
        push(platform->frame_arena, &map_items, command);

        Draw_Command label = {
            .kind = Draw_Kind_TEXT,
            .position = v2_scale(v2_add(node.position, command.position), 0.5f),
            .color[Draw_Color_SOLID] = g.ui_idle_style.fg_color,
            .text = {
                .string = string_print(platform->frame_arena, "%u", i),
                .font_size = g.ui_idle_style.font_size,
            },
        };
        push(platform->frame_arena, &map_items, label);
    }

    for (Simulation_Car_Iterator it = simulation_car_iterator(&g.simulation); simulation_car_next(&it);) {
        Car *car = it.car;

        V2 position = car_position(g.graph, *car);

        f32 radius = CAR_WIDTH;
        position.x -= radius;
        position.y -= radius;

        Draw_Command command = {
            .kind = Draw_Kind_RECTANGLE,
            .position = position,
            .color[Draw_Color_SOLID] = car->highlighted ? 0x909000ff : 0x374354ff,
            .rectangle = {
                .size = { radius * 2.f, radius * 2.f },
                .border_radius = radius,
                .border_width = car->highlighted ? (sqrtf(g.view.target_zoom) * platform->frame.dpi_scale * 0.5f) : 0,
                .border_color = 0x505000ff,
            },
        };
        push(platform->frame_arena, &map_items, command);
    }

    bool do_zoom = platform->frame.scroll != 0.f || map_view_reset;
    if (do_zoom) {
        V2 world_before = v2_add(v2_scale(v2_sub(platform->frame.mouse_position, screen_centre), 1.f / g.view.target_zoom), g.view.target_offset);

        f32 new_zoom = computed_zoom_to_fit(platform);
        if (platform->frame.scroll > 0) {
            new_zoom = CLAMP_HIGH(16.f, 2.f * g.view.target_zoom);
        } else if (platform->frame.scroll < 0) {
            new_zoom = CLAMP_LOW(0.125f, 0.5f * g.view.target_zoom);
        }
        g.view.target_zoom = new_zoom;

        if (map_view_reset) {
            g.view.target_offset = g.view.computed_offset_to_fit;
            g.view.resetting_to_fit = true;
        } else {
            g.view.target_offset = v2_sub(world_before, v2_scale(v2_sub(platform->frame.mouse_position, screen_centre), 1.f / g.view.target_zoom));
            g.view.anchor_world = world_before;
            g.view.anchor_screen = platform->frame.mouse_position;
        }
    }

    f32 animation_speed = 20.f;
    {
        f32 zoom_speed = animation_speed;
        if (g.view.resetting_to_fit) zoom_speed *= 2.f;
        g.view.display_zoom = f32_stable_lerp(g.view.display_zoom, g.view.target_zoom, zoom_speed, platform->frame.seconds_since_last_frame);
    }

    if (g.view.resetting_to_fit) {
        g.view.display_offset = v2_stable_lerp(g.view.display_offset, g.view.target_offset, animation_speed, platform->frame.seconds_since_last_frame);

        bool lerp_done = f32_difference_within_margin(g.view.display_offset.x, g.view.target_offset.x, 0.01f);
        lerp_done = lerp_done && f32_difference_within_margin(g.view.display_offset.y, g.view.target_offset.y, 0.01f);
        if (lerp_done) {
            g.view.resetting_to_fit = false;
            g.view.anchor_world = g.view.display_offset;
            g.view.anchor_screen = screen_centre;
        }
    } else {
        g.view.display_offset = v2_sub(g.view.anchor_world, v2_scale(v2_sub(g.view.anchor_screen, screen_centre), 1.f / g.view.display_zoom));
    }

    ui_Box *map_part = 0;
    static int active_car_count = 0;

    ui_Flags axis = ui_X;
    if (platform->frame.window_size.y > platform->frame.window_size.x) axis = ui_Y;
    ui_Box *rest = ui_push(ui, ui_root, axis, &g.ui_idle_style, 0);
    rest->size[ui_X] = rest->size[ui_Y] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
    {
        ui_Box *left_side = ui_push(ui, rest, !axis, &(ui_Style){ .child_gap = g.ui_idle_style.child_gap }, 0);
        left_side->size[!axis] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
        {
            ui_Box *ppo_panel = &(ui_Box){0};
            ui_Box *tutorial_text_panel = &(ui_Box){0};
            ui_Box *tutorial_motivation_panel = &(ui_Box){0};
            {
                ui_Box *panel = app_ui_push_header_panel(left_side, "%s###ppo panel", tutorial_mode ? "Tutorial" : "PPO");
                panel->size[ui_X] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
                panel->size[ui_Y] = (ui_Size){ .kind = ui_Size_SUM_OF_CHILDREN };

                if (tutorial_mode) tutorial_text_panel = panel;
                else ppo_panel = panel;
            }
            if (tutorial_mode) {
                tutorial_motivation_panel = app_ui_push_header_panel(left_side, "Motivation###motivation panel");
                tutorial_motivation_panel->size[ui_X] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
                tutorial_motivation_panel->size[ui_Y] = (ui_Size){ .kind = ui_Size_SUM_OF_CHILDREN };
            }
            {
                for (const char **line = tutorial_tutorial_text; *line != 0; line += 1) {
                    text(tutorial_text_panel, 0, "%s", *line);
                }
            }
            {
                for (const char **line = motivation_text; *line != 0; line += 1) {
                    text(tutorial_motivation_panel, 0, "%s", *line);
                }
            }
            {
                PPO_Info ppo = g.ppo_info;

                // TODO(felix): blue or orange background
                Text_Flags reward_flags = ppo.rolling_avg_reward == 0.f ? Text_DISABLED : 0;
                ui_Box *reward_part = ui_push(ui, ppo_panel, ui_Y, 0, 0);
                {
                    text(reward_part, reward_flags, "Last Ep Score: %.2f ###ppo ep score", ppo.last_episode_total_score);
                    text(reward_part, reward_flags, "Rolling Reward Avg: %.2f ###ppo avg reward", ppo.rolling_avg_reward);
                    text(reward_part, reward_flags, "Step %2d / 30 ###ppo panel episodes", ppo.steps_in_episode);
                }


                // TODO(felix): section background
                ui_Box *observations = ui_push(ui, ppo_panel, ui_Y, &g.ui_idle_style, 0);
                {
                    bool horizontal = ppo.decision_axis == 0;
                    text(observations, reward_flags, "Phase = %s###ppo panel axis", horizontal ? "Horizontal" : "Vertical");

                    ui_spacer(ui, observations);

                    text(observations, reward_flags, "Observations##ppo panel");
                    ui_Box *list = ui_push(ui, observations, ui_X, 0, 0);
                    for (int i = 0; i < STATE_SIZE; i += 1) {
                        text(list, reward_flags, "%.2f###ppo observation %d", ppo.observation[i], i);
                    }
                }
            }

            ui_Box *car_panel = &(ui_Box){0};
            ui_Box *tutorial_innovation_panel = &(ui_Box){0};
            {
                if (tutorial_mode) tutorial_innovation_panel = app_ui_push_header_panel(left_side, "Innovation###Cars header");
                else car_panel = app_ui_push_header_panel(left_side, "Cars (%u active of %llu)###Cars header", active_car_count, g.simulation.cars.count);

                for (const char **line = innovation_text; *line != 0; line += 1) {
                    text(tutorial_innovation_panel, 0, "%s", *line);
                }
            }
            ui_Box *info_part = ui_push(ui, car_panel, ui_Y, &g.ui_idle_style, 0);
            {
                f32 now_t = g.simulation.simulation_time;
                f32 window_s = 10.f;
                u64 collisions_recent_window = 0;
                u64 collisions_previous_window = 0;

                // TODO(felix): collisions plot

                text(info_part, 0, "Collisions total: %llu###cars collision total", g.simulation.collision_count);

                for (u64 i = 0; i < g.simulation.collision_events.count; i += 1) {
                    Collision_Log event = g.simulation.collision_events.data[i];

                    f32 age = now_t - event.timestamp;
                    if (0.f <= age && age <= window_s) {
                        collisions_recent_window += 1;
                    } else if (window_s < age && age <= 2.f * window_s) {
                        collisions_previous_window += 1;
                    }
                }

                text(info_part, 0, "Collisions last %.0fs: %llu###cars collision recent window", window_s, collisions_recent_window);
                text(info_part, 0, "Collisions previous %.0fs: %llu###cars collision previous window", window_s, collisions_previous_window);
                ui_spacer(ui, info_part);

                Simulation_Car_Iterator it = simulation_car_iterator(&g.simulation);

                for (active_car_count = 0; simulation_car_next(&it); active_car_count += 1) {
                    Car *car = it.car;

                    if (axis == ui_X) {
                        ui_Box *info_box = ui_push(ui, info_part, ui_Y | ui_Flag_DRAW_BACKGROUND | ui_Flag_DRAW_BORDER | ui_Flag_DRAW_SHADOW | ui_Flag_HOVERABLE, &g.ui_idle_style, "###car header %d", active_car_count);
                        info_box->style.bg_color = car->is_active ? 0xe5e5e5ff : 0xd0d0d0ff;
                        if (car->highlighted) info_box->style.bg_color = 0xe0e0a0ff;

                        u64 left = car->path.count - 1;
                        text(info_box, 0, "%s (t >= %.0f): %llu junction%s left###", car->is_active ? "Active" : "Inactive", car->start_time, left, left > 1 ? "s" : "");

                        V2 position = car_position(g.graph, *car);

                        if (car->path.count > 0) text(info_box, 0, "On junction %u, %.0fx%.0f###", car->path.data[0], position.x, position.y);

                        text(info_box, 0, "Speed %.0f (max %.0f)###", car->speed, car->max_speed);

                        car->highlighted = !!(info_box->flags & ui_Flag_HOVERED);
                    }
                }

                if (active_car_count == 0) {
                    text(info_part, Text_DISABLED, "No active cars remaining");
                }
            }
        }

        ui_Flags map_part_flags = ui_Flag_DRAW_BACKGROUND | ui_Flag_DRAW_BORDER | ui_Flag_DRAW_SHADOW;
        ui_Style map_part_style = g.ui_idle_style;
        map_part_style.border_width *= 2.f;
        map_part_style.border_radius *= 2.f;
        map_part_style.bg_color = 0x60a0c0ff;
        map_part_style.border_color = 0x305060ff;
        map_part_style.shadow_color = 0x000000ff;
        map_part = ui_push(ui, rest, map_part_flags, &map_part_style, "###map part");
        map_part->size[ui_X] = map_part->size[ui_Y] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };

        ui_Box *right_side = ui_push(ui, rest, !axis, &(ui_Style){ .child_gap = g.ui_idle_style.child_gap }, 0);
        right_side->size[!axis] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
        right_side->size[axis] = (ui_Size){ .kind = ui_Size_LARGEST_CHILD };
        {
            Node *selected_node = &g.graph.nodes.data[g.selected_node.i];
            V2 move = {0};

            if (axis == ui_X) {
                ui_Box *junction_panel = &(ui_Box){0};
                ui_Box *tutorial_implementation_panel = &(ui_Box){0};
                ui_Box *tutorial_map_controls_panel = &(ui_Box){0};
                ui_Box *tutorial_car_and_ppo_panel = &(ui_Box){0};
                {
                    if (tutorial_mode) tutorial_implementation_panel = app_ui_push_header_panel(right_side, "Implementation");
                    else junction_panel = app_ui_push_header_panel(right_side, "Junction");

                    for (const char **line = implementation_text; *line != 0; line += 1) {
                        text(tutorial_implementation_panel, 0, "%s", *line);
                    }
                }
                {
                    if (tutorial_mode) tutorial_map_controls_panel = app_ui_push_header_panel(right_side, "Map Controls");
                    for (const char **line = map_controls_text; *line != 0; line += 1) {
                        text(tutorial_map_controls_panel, 0, "%s", *line);
                    }
                }
                {
                    if (tutorial_mode) tutorial_car_and_ppo_panel = app_ui_push_header_panel(right_side, "Information Panels");
                    for (const char **line = car_and_ppo_text; *line != 0; line += 1) {
                        text(tutorial_car_and_ppo_panel, 0, "%s", *line);
                    }
                }
                junction_panel->size[!axis] = (ui_Size){ .kind = ui_Size_SUM_OF_CHILDREN };
                junction_panel->size[axis] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };

                Text_Flags flags = g.selected_node.i == 0 ? Text_DISABLED : 0;

                ui_Box *first_row = ui_push(ui, junction_panel, ui_X, &(ui_Style){ .padding[ui_X] = g.ui_idle_style.padding[ui_X] }, 0);
                first_row->size[ui_X] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
                {
                    text(first_row, flags, "selected ID=###selected node id", g.selected_node.i);
                    {
                        static bool active = false;
                        u32 new_selection = numeric_button(platform->frame, g.selected_node.i, first_row, &active, flags, "%u###selected node id button", g.selected_node.i);
                        if (new_selection != g.selected_node.i) {
                            g.selected_node.i = CLAMP(new_selection, 1, (u32)g.graph.nodes.count - 1);
                        }
                    }

                    // ui_spacer(ui, first_row);
                }

                ui_Box *inside_panel = ui_push(ui, junction_panel, ui_Y, &g.ui_idle_style, 0);
                {
                    ui_Style style = g.ui_click_style;
                    style.bg_color = 0xc0c0c0ff;
                    style.border_color = 0xa0a0a0ff;

                    ui_Box *arrow_box = ui_push(ui, inside_panel, ui_Y | ui_Flag_DRAW_BORDER | ui_Flag_DRAW_BACKGROUND, &style, 0);
                    {
                        text(arrow_box, flags, "%.0f,%.0f   ###selected node position", selected_node->position.x, selected_node->position.y);

                        ui_Box *buttons = ui_push(ui, arrow_box, ui_X, &style, 0);
                        {
                            ui_spacer(ui, buttons);

                            ui_Box *left_button = ui_push(ui, buttons, ui_Y, &g.ui_idle_style, 0);
                            {
                                ui_spacer(ui, left_button);
                                move.x -= (f32)!!(button(left_button, 0, App_Key_LEFT, flags, "Left##node move")->flags & ui_Flag_CLICKED);
                                ui_spacer(ui, left_button);
                            }

                            ui_Box *up_button = ui_push(ui, buttons, ui_Y, &g.ui_idle_style, 0);
                            {
                                move.y -= (f32)!!(button(up_button, 0, App_Key_UP, flags, "Up##node move")->flags & ui_Flag_CLICKED);
                                ui_spacer(ui, up_button);

                                ui_spacer(ui, up_button);
                                move.y += (f32)!!(button(up_button, 0, App_Key_DOWN, flags, "Down##node move")->flags & ui_Flag_CLICKED);
                            }

                            ui_Box *right_button = ui_push(ui, buttons, ui_Y, &g.ui_idle_style, 0);
                            {
                                ui_spacer(ui, right_button);
                                move.x += (f32)!!(button(right_button, 0, App_Key_RIGHT, flags, "Right##node move")->flags & ui_Flag_CLICKED);
                                ui_spacer(ui, right_button);
                            }

                            ui_spacer(ui, buttons);
                        }

                        text(arrow_box, flags, "Hold shift to increase change");
                    }
                }

                ui_Box *last_row = ui_push(ui, junction_panel, ui_X, &g.ui_idle_style, 0);

                ui_Box *delete_button = button(last_row, 0, 0, flags, "Remove###selected node remove button");
                bool delete = !!(delete_button->flags & ui_Flag_CLICKED);
                if (delete) {
                    selected_node->flags |= Node_Flag_DEAD;

                    for (Link_Iterator it = link_iterator(g.graph.links.data, g.graph.links.count); link_iterate(&it);) {
                        if (it.link->from.i == g.selected_node.i) {
                            it.link->from.i = 0;
                            it.link->flags |= Link_Flag_DEAD;
                        }
                        if (it.link->to.i == g.selected_node.i) {
                            it.link->to.i = 0;
                            it.link->flags |= Link_Flag_DEAD;
                        }
                    }

                    g.selected_node.i = 0;
                }

                ui_Box *new = button(last_row, 0, 0, 0, "New##add node button");
                if (new->flags & ui_Flag_CLICKED) {
                    Node node = { .position = g.graph.nodes.data[g.graph.nodes.count - 1].position };
                    node.position = v2_add(node.position, v2(10.f));
                    g.selected_node.i = (u32)g.graph.nodes.count;
                    push(platform->persistent_arena, &g.graph.nodes, node);
                }
            }

            if (platform->frame.key_down[App_Key_SHIFT]) move = v2_scale(move, 10.f);
            selected_node->position = v2_add(selected_node->position, move);

            ui_Box *lane_panel = &(ui_Box){0};
            if (!tutorial_mode) lane_panel = app_ui_push_header_panel(right_side, "Lane##link panel");
            lane_panel->size[ui_X] = (ui_Size){ .kind = ui_Size_LARGEST_CHILD };
            lane_panel->size[ui_Y] = (ui_Size){ .kind = ui_Size_SUM_OF_CHILDREN };
            {
                Link *link = &g.graph.links.data[g.selected_link.i];
                Text_Flags flags = g.selected_link.i == 0 ? Text_DISABLED : 0;

                ui_Box *first_row = ui_push(ui, lane_panel, ui_X, 0, 0);
                first_row->size[ui_X] = (ui_Size){ .kind = ui_Size_SUM_OF_CHILDREN };
                {
                    text(first_row, flags, "selected ID=###selected link id", g.selected_link.i);
                    {
                        static bool active = false;
                        u32 new_selection = numeric_button(platform->frame, g.selected_link.i, first_row, &active, flags, "%u###selected link id button", g.selected_link.i);
                        if (new_selection != g.selected_link.i) {
                            g.selected_link.i = CLAMP(new_selection, 1, (u32)g.graph.links.count - 1);
                        }
                    }
                    ui_spacer(ui, first_row);
                    text(first_row, flags, "intersection###selected link intersection id");

                    u8 last_intersection_id = link->intersection_id;
                    static bool changing = false;
                    link->intersection_id = (u8)numeric_button(platform->frame, (u32)link->intersection_id, first_row, &changing, flags, "%d###selected link intersection id button", (int)link->intersection_id);
                    if (link->intersection_id != last_intersection_id) {
                        Intersection_Config config = ppo_auto_config(&g.graph, 1);
                        for (u32 i = 0; i < active_ppo_count; i += 1) {
                            ppo_update_intersection_config(global_ppos[i], config);
                        }
                    }

                    text(first_row, flags, " ###");
                }

                ui_Box *to_from = ui_push(ui, lane_panel, ui_X, 0, 0);
                {
                    text(to_from, flags, "Links junction");
                    {
                        static bool active = false;
                        u32 selected = numeric_button(platform->frame, link->from.i, to_from, &active, flags, "%u###from node button", link->from.i);
                        selected = CLAMP(selected, 0, (u32)g.graph.nodes.count - 1);
                        if (selected != link->from.i) {
                            g.selected_node.i = selected;
                            link->from.i = selected;
                        }
                    }
                    text(to_from, flags, "to##junction link 'to'");
                    {
                        static bool active = false;
                        u32 selected = numeric_button(platform->frame, link->to.i, to_from, &active, flags, "%u###to node button", link->to.i);
                        selected = CLAMP(selected, 0, (u32)g.graph.nodes.count - 1);
                        if (selected != link->to.i) {
                            g.selected_node.i = selected;
                            link->to.i = selected;
                        }
                    }
                }

                f32 length = link_length(g.graph, *link);
                text(lane_panel, flags, "%.0f %s long###link length in panel", length, (int)floorf(length) == 1 ? "metre" : "metres");

                // TODO(felix): tickboxes, radio buttons

                bool show_flags = false;
                if (show_flags) {
                    bool has_speedbump = !!(link->flags & Link_Flag_SPEEDBUMP);
                    text(lane_panel, flags | (!has_speedbump * Text_DISABLED), "Speed bump? %s###speed bump", has_speedbump ? "Yes" : "No");

                    bool has_stop_sign = !!(link->flags & Link_Flag_STOP_SIGN);
                    text(lane_panel, flags | (!has_stop_sign * Text_DISABLED), "Stop sign? %s###stop sign", has_stop_sign ? "Yes": "No");

                    bool has_blocked = !!(link->flags & Link_Flag_BLOCKED);
                    text(lane_panel, flags | (!has_blocked * Text_DISABLED), "Blocked? %s###link blocked", has_blocked ? "Yes": "No");
                }

                ui_Box *light_part = ui_push(ui, lane_panel, ui_X, 0, 0);
                {
                    bool has_light = !!(link->flags & LINK_FLAG_ANY_TRAFFIC_LIGHT);
                    text(light_part, flags | (!has_light * Text_DISABLED), "Light:###link light prefix");
                    if (has_light) {
                        const char *value = 0;

                        if (link->flags & Link_Flag_TRAFFIC_LIGHT_GREEN) {
                            g.override_fg_color = LIGHT_GREEN_COLOR;
                            value = "Green";
                        } else if (link->flags & Link_Flag_TRAFFIC_LIGHT_YELLOW) {
                            g.override_fg_color = LIGHT_YELLOW_COLOR;
                            value = "Yellow";
                        } else if (link->flags & Link_Flag_TRAFFIC_LIGHT_RED) {
                            g.override_fg_color = LIGHT_RED_COLOR;
                            value = "Red";
                        }

                        text(light_part, flags, "%s###link light value", value);
                        g.override_fg_color = 0;
                    } else {
                        text(light_part, flags | Text_DISABLED, "None###link light value");
                    }
                }

                ui_Box *last_row = ui_push(ui, lane_panel, ui_X, &g.ui_idle_style, 0);

                ui_Box *remove = button(last_row, 0, 0, flags, "Remove###remove link button");
                if (remove->flags & ui_Flag_CLICKED) {
                    g.selected_link.i = 0;
                    link->flags |= Link_Flag_DEAD;
                }

                ui_Box *new = button(last_row, 0, 0, 0, "New##add link button");
                if (new->flags & ui_Flag_CLICKED) {
                    Link new_link = {0};
                    if (g.graph.nodes.count > 1) new_link.from.i = (u32)g.graph.nodes.count - 1;
                    if (g.graph.nodes.count > 2) new_link.to.i = (u32)g.graph.nodes.count - 2;
                    g.selected_link.i = (u32)g.graph.links.count;
                    push(platform->persistent_arena, &g.graph.links, new_link);
                }
            }

            ui_Box *map_panel = &(ui_Box){0};
            if (!tutorial_mode) map_panel = app_ui_push_header_panel(right_side, "Map##panel");
            map_panel->size[ui_X] = (ui_Size){ .kind = ui_Size_OF_PARENT, .value = 1.f };
            {
                ui_Box *panel = ui_push(ui, map_panel, ui_Y, &g.ui_idle_style, 0);

                text(panel, 0, "Preset##a map from the buttons");

                ui_Style style = g.ui_click_style;
                style.bg_color = 0xc0c0c0ff;
                style.border_color = 0xa0a0a0ff;
                ui_Box *first_row = ui_push(ui, panel, ui_X | ui_Flag_DRAW_BORDER | ui_Flag_DRAW_BACKGROUND, &style, 0);
                {
                    static bool roundabout_selected = false;

                    ui_Box *intersection = button(first_row, 0, 0, roundabout_selected ? 0 : Text_DISABLED, "Intersection##map button");
                    ui_Box *roundabout = button(first_row, 0, 0, roundabout_selected ? Text_DISABLED : 0, "Roundabout##map button");
                    if (intersection->flags & ui_Flag_CLICKED) {
                        roundabout_selected = false;
                        g.graph = graph_init(g.maps.data[0]);

                        graph_clear_lights_to_nodes(&g.graph, 5, 7);

                        Intersection_Config config = ppo_auto_config(&g.graph, 1);
                        for (u32 i = 0; i < active_ppo_count; i += 1) {
                            ppo_update_intersection_config(global_ppos[i], config);
                        }

                        g.simulation.graph = &g.graph;
                    }
                    if (roundabout->flags & ui_Flag_CLICKED) {
                        roundabout_selected = true;
                        g.graph = graph_init(g.maps.data[1]);

                        graph_clear_lights_to_nodes(&g.graph, 5, 7);

                        Intersection_Config config = ppo_auto_config(&g.graph, 1);
                        for (u32 i = 0; i < active_ppo_count; i += 1) {
                            ppo_update_intersection_config(global_ppos[i], config);
                        }

                        g.simulation.graph = &g.graph;
                    }
                }

                ui_Box *save_button = button(panel, 0, 'S', 0, "%s##map button", BASE_OS == BASE_OS_WASM ? "Download" : "Save");
                if (save_button->flags & ui_Flag_CLICKED) {
                    int buffer_size = 1 * 1024 * 1024;
                    char *buffer = arena_make(platform->frame_arena, (u64)buffer_size, char);

                    serial_Writer writer = serial_writer(buffer, buffer_size);
                    write_named_f64(&writer, "maps", 1.0);
                    serial_write_tag(&writer, serial_Type_ARRAY);

                    serial_write_tag(&writer, serial_Type_OBJECT);
                    {
                        write_named_f64(&writer, "links", (f64)g.graph.links.count);
                        serial_write_tag(&writer, serial_Type_ARRAY);
                        for (u64 i = 0; i < g.graph.links.count; i += 1) {
                            Link link = g.graph.links.data[i];
                            serial_write_tag(&writer, serial_Type_OBJECT);
                            write_named_f64(&writer, "from", (f64)link.from.i);
                            write_named_f64(&writer, "to", (f64)link.to.i);
                            write_named_f64(&writer, "flags", (f64)link.flags);
                            write_named_f64(&writer, "intersection_id", (f64)link.intersection_id);
                            serial_write_tag(&writer, serial_Type_END);
                        }
                        serial_write_tag(&writer, serial_Type_END);

                        write_named_f64(&writer, "nodes", (f64)g.graph.nodes.count);
                        serial_write_tag(&writer, serial_Type_ARRAY);
                        for (u64 i = 0; i < g.graph.nodes.count; i += 1) {
                            Node node = g.graph.nodes.data[i];
                            serial_write_tag(&writer, serial_Type_OBJECT);
                            write_named_f64(&writer, "x", (f64)node.position.x);
                            write_named_f64(&writer, "y", (f64)node.position.y);
                            write_named_f64(&writer, "flags", (f64)node.flags);
                            serial_write_tag(&writer, serial_Type_END);
                        }
                        serial_write_tag(&writer, serial_Type_END);

                        write_named_f64(&writer, "entrances", (f64)g.graph.entrances.count);
                        serial_write_tag(&writer, serial_Type_ARRAY);
                        for (u32 i = 0; i < g.graph.entrances.count; i += 1) {
                            Node_Id entrance = g.graph.entrances.data[i];
                            serial_write_f64(&writer, (f64)entrance.i);
                        }
                        serial_write_tag(&writer, serial_Type_END);

                        write_named_f64(&writer, "exits", (f64)g.graph.exits.count);
                        serial_write_tag(&writer, serial_Type_ARRAY);
                        for (u32 i = 0; i < g.graph.exits.count; i += 1) {
                            Node_Id exit = g.graph.exits.data[i];
                            serial_write_f64(&writer, (f64)exit.i);
                        }
                        serial_write_tag(&writer, serial_Type_END);
                    }
                    serial_write_tag(&writer, serial_Type_END);

                    serial_write_tag(&writer, serial_Type_END);

                    bool save_ok = os_write_entire_file("saved_state.bin", buffer, (u64)writer.written);
                    assert(save_ok);
                }
            }
        }
    }

    reset_this_frame = reset_this_frame || active_car_count == 0;
    if (reset_this_frame) {
        simulation_init(&g.simulation, &g.simulation_arena, &g.graph, &g.prng, g.simulation_num_cars);
    }

    ui_end(ui);
    base_ui_renderer(ui, ui->root);

    ui_Rectangle map_clip = map_part->rectangle;
    map_clip.position[0] += map_part->style.border_width;
    map_clip.position[1] += map_part->style.border_width;
    map_clip.size[0] -= 2.f * map_part->style.border_width;
    map_clip.size[1] -= 2.f * map_part->style.border_width;

    V2 top_left = { map_clip.position[0], map_clip.position[1] };
    V2 size = { map_clip.size[0], map_clip.size[1] };
    V2 map_centre = v2_add(top_left, v2_scale(size, 0.5f));
    V2 screen_offset = map_centre;

    for (u64 i = 0; i < map_items.count; i += 1) {
        Draw_Command *command = &map_items.data[i];

        f32 zoom = g.view.display_zoom;
        V2 offset = g.view.display_offset;

        command->position = screen_from_world(command->position, offset, zoom, screen_offset);
        memcpy(&command->clip, &map_clip, sizeof map_clip);

        switch (command->kind) {
            case Draw_Kind_QUADRILATERAL: {
                for (Draw_Corner corner = 0; corner < Draw_Corner_COUNT; corner += 1) {
                    command->quadrilateral.corners[corner] = screen_from_world(command->quadrilateral.corners[corner], offset, zoom, screen_offset);
                }
                command->quadrilateral.border_width *= zoom;
            } break;
            case Draw_Kind_RECTANGLE: {
                command->rectangle.size = v2_scale(command->rectangle.size, zoom);
                command->rectangle.border_radius *= zoom;
            } break;
            case Draw_Kind_LINE: {
                command->line.end = screen_from_world(command->line.end, offset, zoom, screen_offset);
            } break;
            case Draw_Kind_TEXT: break;
            default: unreachable;
        }

        bool has_data = command->user_data != 0;
        Draw_Data data = (Draw_Data)command->user_data;
        bool is_link = has_data && !!(data & Draw_Data_LINK);
        u64 index = data & DRAW_DATA_INDEX_MASK;
        if (has_data) {
            if (platform->frame.mouse_clicked[App_Mouse_LEFT]) {
                bool is_node = !!(data & Draw_Data_NODE);
                if (is_node) {
                    V4 rectangle = { .x = command->position.x, .y = command->position.y };
                    v4_right(rectangle) = v4_left(rectangle) + command->rectangle.size.x;
                    v4_bottom(rectangle) = v4_top(rectangle) + command->rectangle.size.y;

                    bool clicked = intersect_point_in_rectangle(platform->frame.mouse_position, rectangle);
                    if (clicked) {
                        g.selected_node.i = (u32)index;
                    }
                }

                if (is_link) {
                    V2 a = command->position;
                    V2 b = command->line.end;

                    V4 rectangle = {0};
                    v4_left(rectangle) = MIN(a.x, b.x);
                    v4_right(rectangle) = MAX(a.x, b.x);
                    v4_top(rectangle) = MIN(a.y, b.y);
                    v4_bottom(rectangle) = MAX(a.y, b.y);

                    {
                        f32 sign = f32_difference_within_margin(v4_left(rectangle), v4_right(rectangle), node_draw_radius * 2.f) ? -1.f : 1.f;
                        v4_left(rectangle) += sign * 2.f * node_draw_radius;
                        v4_right(rectangle) -= sign * 2.f * node_draw_radius;
                    }
                    {
                        f32 sign = f32_difference_within_margin(v4_top(rectangle), v4_bottom(rectangle), node_draw_radius * 2.f) ? -1.f : 1.f;
                        v4_top(rectangle) += sign * 2.f * node_draw_radius;
                        v4_bottom(rectangle) -= sign * 2.f * node_draw_radius;
                    }

                    bool clicked = intersect_point_in_rectangle(platform->frame.mouse_position, rectangle);
                    if (clicked) {
                        g.selected_link.i = (u32)index;
                    }
                }
            }
        }

        if (is_link && g.selected_link.i == (u32)index) {
            command->color[Draw_Color_SOLID] = 0x80ffffff;
        }

    }
    draw_many(platform, map_items.data, map_items.count);

    static bool mouse_down_last_frame;
    static V2 mouse_position_start_drag;
    static V2 view_origin_start_drag;

    bool *map_interaction = &g.view.interacting;
    *map_interaction = !ui->interacted_this_frame;
    *map_interaction = *map_interaction && platform->frame.mouse_down[App_Mouse_LEFT];
    *map_interaction = *map_interaction && mouse_down_last_frame;
    if (*map_interaction) {
        V2 mouse_delta = v2_sub(platform->frame.mouse_position, mouse_position_start_drag);
        V2 map_delta = v2_scale(mouse_delta, 1.f / g.view.target_zoom);
        g.view.target_offset = v2_sub(view_origin_start_drag, map_delta);
        g.view.display_offset = g.view.target_offset;
        g.view.anchor_world = g.view.display_offset;
        g.view.anchor_screen = screen_centre;
        g.view.resetting_to_fit = false;
    }

    bool start_drag = platform->frame.mouse_down[App_Mouse_LEFT] && !mouse_down_last_frame;
    if (start_drag) {
        mouse_position_start_drag = platform->frame.mouse_position;
        view_origin_start_drag = g.view.target_offset;
    }

    mouse_down_last_frame = platform->frame.mouse_down[App_Mouse_LEFT];

    if (platform->frame.key_pressed['B']) breakpoint;
    last_screen_size = platform->frame.window_size;
}
