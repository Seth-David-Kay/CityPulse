#define BASE_PLATFORM_WASM

struct Platform_Implementation {
    Platform base;

    f32 real_over_virtual_window_scale;
    cpu_draw_Surface surface;
    cpu_draw_Font font;
};

static Platform_Implementation platform__;

typedef enum { // https://developer.mozilla.org/en-US/docs/Web/API/MouseEvent/button
    JS_Mouse_Button_LEFT = 0,
    JS_Mouse_Button_MIDDLE = 1,
    JS_Mouse_Button_RIGHT = 2,

    JS_Mouse_Button_MAX_VALUE,
} JS_Mouse_Button;

static App_Key app_key_from_js(const char *bytes, u32 count) {
    App_Key key = 0;
    String js_key = { .data = bytes, .count = (u64)count };

    // https://developer.mozilla.org/en-US/docs/Web/API/UI_Events/Keyboard_event_code_values

    char compare_alpha_buffer[] = "KeyA";
    String compare_alpha = { .data = compare_alpha_buffer, .count = 4 };
    for (char letter = 'A'; letter <= 'Z'; letter += 1) {
        compare_alpha_buffer[3] = letter;
        if (string_equals(js_key, compare_alpha)) {
            key = (App_Key)letter;
            break;
        }
    }

    if (key == 0) {
        char compare_digit_buffer[] = "Digit0";
        String compare_digit = { .data = compare_digit_buffer, .count = 6 };
        for (char digit = '0'; digit <= '9'; digit += 1) {
            if (string_equals(js_key, compare_digit)) {
                key = (App_Key)digit;
                break;
            }
        }
    }

    if (key == 0) {
        if (0) {}
        else if (string_equals(js_key, S("ShiftLeft"))) key = App_Key_SHIFT;
        else if (string_equals(js_key, S("Equal"))) key = '=';
        else if (string_equals(js_key, S("Minus")))  key = '-';
        else {
            static bool once = false;
            if (!once) {
                log_info("unimplemented translation from JS key '%S' (logging ONCE for all keys)", js_key);
                once = true;
            }
        }
    }

    return key;
}

static App_Mouse_Button app_mouse_button_from_js(JS_Mouse_Button js_button) {
    App_Mouse_Button button = 0;
    switch (js_button) {
        case JS_Mouse_Button_LEFT: button = App_Mouse_LEFT; break;
        case JS_Mouse_Button_MIDDLE: button = App_Mouse_MIDDLE; break;
        case JS_Mouse_Button_RIGHT: button = App_Mouse_RIGHT; break;
        default: {
            static bool once = false;
            if (!once) {
                log_info("unimplemented translation from JS button <0x%x> (logging ONCE for all buttons)", js_button);
                once = true;
            }
        } break;
    }
    return button;
}

cpu_draw_Surface *platform__get_draw_surface_pointer(void) {
    return &platform__.surface;
}

char *platform__get_temp_js_string_buffer(void) {
    static char platform__js_string_buffer[64];
    return platform__js_string_buffer;
}

void platform__on_key_down(const char *bytes, u32 count) {
    App_Frame_Info *frame = &platform__.base.frame;
    App_Key key = app_key_from_js(bytes, count);
    frame->key_pressed[key] = !frame->key_down[key];
    frame->key_down[key] = true;
}

void platform__on_key_up(const char *bytes, u32 count) {
    App_Key key = app_key_from_js(bytes, count);
    platform__.base.frame.key_down[key] = false;
}

void platform__on_mouse_down(JS_Mouse_Button js_button) {
    App_Frame_Info *frame = &platform__.base.frame;
    App_Mouse_Button button = app_mouse_button_from_js(js_button);
    frame->mouse_clicked[button] = !frame->mouse_down[button];
    frame->mouse_down[button] = true;
}

void platform__on_mouse_up(JS_Mouse_Button js_button) {
    App_Mouse_Button button = app_mouse_button_from_js(js_button);
    platform__.base.frame.mouse_down[button] = false;
}

void platform__on_scroll(f32 x, f32 y) {
    (void)x; // for now
    platform__.base.frame.scroll = y / -120.f;
}

void platform__quit(void) {
    app_quit(&platform__.base);
}

void platform__start(void) {
    Platform *base = &platform__.base;

    static Arena persistent_arena, frame_arena;

    persistent_arena = arena_init(16 * 1024 * 1024);
    base->persistent_arena = &persistent_arena;

    frame_arena = arena_init(16 * 1024 * 1024);
    base->frame_arena = &frame_arena;

    app_start(base);

    platform__.font = cpu_draw_font_from_bdf(platform__font_bytes, platform__font_size);

    base->frame.dpi_scale = 1.f;
    if (platform__.real_over_virtual_window_scale == 0.f) platform__.real_over_virtual_window_scale = 3.f;
}

void platform__update_and_render(f32 window_width, f32 window_height, f32 mouse_x, f32 mouse_y) {
    Arena scratch = *platform__.base.frame_arena;
    App_Frame_Info *frame = &platform__.base.frame;

    static Time_Averaged_Sampler sampler = {0};
    static bool sampler_init = false;
    if (!sampler_init) {
        #define PLATFORM__TIME_SAMPLES 16
        static f64 samples[PLATFORM__TIME_SAMPLES] = {0};

        sampler = time_averaged_sampler(samples, PLATFORM__TIME_SAMPLES);
        sampler_init = true;
    }
    frame->seconds_since_last_frame = (f32)time_averaged_sample_elapsed_seconds(&sampler);
    frame->seconds_since_last_frame = CLAMP_HIGH(0.1f, frame->seconds_since_last_frame);

    // NOTE(felix): these have to match the values on the JS side!
    i32 max_virtual_width = 1920;
    i32 max_virtual_height = 1200;

    cpu_draw_Surface *surface = &platform__.surface;
    if (surface->pixels == 0) {
        u32 *pixels = arena_make(platform__.base.persistent_arena, (u64)(max_virtual_width * max_virtual_height), u32);
        *surface = cpu_draw_surface(pixels, max_virtual_width, (int)roundf(window_width), (int)roundf(window_height));
    }

    f32 actual_real_over_virtual_window_scale = platform__.real_over_virtual_window_scale;
    actual_real_over_virtual_window_scale = ceilf(MAX(window_width / (f32)max_virtual_width, actual_real_over_virtual_window_scale));
    actual_real_over_virtual_window_scale = ceilf(MAX(window_height / (f32)max_virtual_height, actual_real_over_virtual_window_scale));
    assert(actual_real_over_virtual_window_scale > 0);

    platform__.base.frame.window_size = v2_scale((V2){ window_width, window_height }, 1.f / actual_real_over_virtual_window_scale);
    surface->clip_right = (int)roundf(platform__.base.frame.window_size.x);
    surface->clip_bottom = (int)roundf(platform__.base.frame.window_size.y);

    f32 real_window_size[2] = { window_width, window_height };
    f32 real_mouse_position[2] = { mouse_x, mouse_y };
    for (u64 i = 0; i < 2; i += 1) {
        v(frame->mouse_position)[i] = real_mouse_position[i] * (v(frame->window_size)[i] / real_window_size[i]);
    }

    platform__.base.draw_commands = (Array_Draw_Command){0};
    app_update_and_render(&platform__.base);

    u32 clear = abgr_from_rgba(platform__.base.clear_color);
    cpu_draw_rectangle(*surface, 0, 0, surface->clip_right, surface->clip_bottom, clear);
    cpu_draw_commands(*surface, &platform__.font, platform__.base.draw_commands.data, platform__.base.draw_commands.count);

    frame->scroll = 0;
    memset(frame->mouse_clicked, 0, sizeof frame->mouse_clicked);
    memset(frame->key_pressed, 0, sizeof frame->key_pressed);
    *platform__.base.frame_arena = scratch;
}

static V2 platform_measure_text(Platform *platform, String text, f32 font_size) {
    return cpu_draw_platform_measure_text(&((Platform_Implementation *)platform)->font, text, font_size);
}

static void program(void) {
    panic("stub, don't call");
}
