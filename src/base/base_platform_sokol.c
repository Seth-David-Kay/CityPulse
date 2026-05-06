// NOTE(felix): useful resources for using sokol:
// https://floooh.github.io/sokol-html5/
// https://zeromake.github.io/learnopengl-examples/


#if !defined(BASE_PLATFORM_H)
#define BASE_PLATFORM_H

#include "base/base_context.h"

#if COMPILER_CLANG
	#pragma clang diagnostic push
	// #pragma diagnostic ignored "-Wgnu-empty-struct"
#elif COMPILER_MSVC
	#pragma warning(push)
	#pragma warning(disable: 4267)
	#pragma warning(disable: 4996)
#endif

#if BASE_OS == BASE_OS_WINDOWS
	#define SOKOL_D3D11
#elif BASE_OS == BASE_OS_MACOS
	#define SOKOL_METAL
#else
	#error "unsupported OS"
#endif

#if BUILD_DEBUG
    #define SOKOL_DEBUG
#endif

#define SOKOL_NO_ENTRY

#if defined(BASE_PLATFORM_IMPLEMENTATION)
	#define SOKOL_IMPL
	#define FONTSTASH_IMPLEMENTATION
    #define FLT_MAX 3.40282347e38f
#else
    typedef  u8  uint8_t;
    typedef u16 uint16_t;
    typedef u32 uint32_t;
    typedef u64 uint64_t;
#endif

// TODO(felix): a metaprogram "#include capture" system would let us override the system directories to provide our own stubs for stdint, etc.

#include "sokol/sokol_gfx.h" // NOTE(felix): guarded 3 stdlib includes starting on line 1927, and 3 more starting line 5219
#include "sokol/sokol_gl.h" // NOTE(felix): commented out 3 stdlib includes starting on line 697

#include "sokol/sokol_app.h" // NOTE(felix): commented out 3 stdlib includes on line 1295
#include "fontstash/fontstash.h"

#include "sokol/sokol_fontstash.h" // NOTE(felix): commented out the 3 stdlib includes on line 187
#include "sokol/sokol_glue.h"

#if COMPILER_CLANG
	#pragma clang diagnostic pop
#elif COMPILER_MSVC
	#pragma warning(pop)
#endif


#endif // !defined(BASE_PLATFORM_H)


#if defined(BASE_IMPLEMENTATION)


struct Platform {
    Platform_Base base;

    sg_pipeline pipeline;
    sg_pass_action pass_action;
    f32 max_font_size;
    FONScontext *fontstash_context;
    i32 font_id;
};

#include "../shader.c"

static void sokol_event(const sapp_event *event, void *user_data) {
    Platform *platform = user_data;
    App_Frame_Info *info = &platform->base.frame;

    App_Key key_code = 0;
    // NOTE(felix): up to 96 (ASCII grave '`'), both Sokol and us use the corresponding ASCII value
    if (event->key_code <= '`')  {
        key_code = (App_Key)event->key_code;
    } else switch (event->key_code) {
        case SAPP_KEYCODE_LEFT_CONTROL: key_code = App_Key_CONTROL; break;
        case SAPP_KEYCODE_LEFT: key_code = App_Key_LEFT; break;
        case SAPP_KEYCODE_RIGHT: key_code = App_Key_RIGHT; break;
        default: {
            log_info("unimplemented key translation from sokol key code %d", (i32)event->key_code);
        } break;
    }

    App_Mouse_Button mouse_button = 0;
    switch (event->mouse_button) {
        case SAPP_MOUSEBUTTON_INVALID: mouse_button = App_Mouse_Button_NIL; break;
        case SAPP_MOUSEBUTTON_LEFT: mouse_button = App_Mouse_Button_LEFT; break;
        case SAPP_MOUSEBUTTON_RIGHT: mouse_button = App_Mouse_Button_RIGHT; break;
        case SAPP_MOUSEBUTTON_MIDDLE: mouse_button = App_Mouse_Button_MIDDLE; break;
        default: unreachable;
    }

    switch (event->type) {
        case SAPP_EVENTTYPE_KEY_DOWN: {
            info->key_pressed[key_code] = true;
            info->key_down[key_code] = true;
        } break;
        case SAPP_EVENTTYPE_KEY_UP: {
            info->key_down[key_code] = false;
        } break;
        case SAPP_EVENTTYPE_MOUSE_DOWN: {
            info->mouse_clicked[mouse_button] = true;
            info->mouse_down[mouse_button] = true;
        } break;
        case SAPP_EVENTTYPE_MOUSE_UP: {
            info->mouse_down[mouse_button] = false;
        } break;
        case SAPP_EVENTTYPE_MOUSE_MOVE: {
            info->mouse_position = (V2){ .x = event->mouse_x, .y = event->mouse_y };
        } break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL: {
            info->scroll = event->scroll_y * 0.25f;
        } break;
        default: break;
    }
}

static void sokol_logger(const char *tag, u32 level, u32 item, const char *message, u32 line_number, const char *filename, void *user_data) {
    (void)user_data;
    print("%s:%u: %s %u %u '%s'\n", filename, line_number, tag, level, item, message);
}

static void sokol_populate_frame_info(App_Frame_Info *info) {
    info->dpi_scale = sapp_dpi_scale();
    info->window_size = (V2){ .x = sapp_widthf(), .y = sapp_heightf() };
    info->seconds_since_last_frame = (f32)sapp_frame_duration();
}

static void sokol_cleanup(void *user_data) {
    Platform *platform = user_data;

    app_quit(&platform->base);

    sfons_destroy(platform->fontstash_context);
    sgl_shutdown();
    sg_shutdown();
}

static void sokol_init(void *user_data) {
    Platform *platform = user_data;
    app_start(&platform->base);

    sokol_populate_frame_info(&platform->base.frame);

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = sokol_logger,
    });

    sgl_setup(&(sgl_desc_t){
        .logger.func = sokol_logger,
    });

    sg_shader shader = sg_make_shader(uber_shader_desc(sg_query_backend()));

    platform->pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shader,
        .colors[0].blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .src_factor_alpha = SG_BLENDFACTOR_ONE,
            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
    });

    platform->pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR },
    };

    {
        platform->max_font_size = 1024.f * platform->base.frame.dpi_scale;
        u64 next_power_of_2_from_dpi = 0;
        {
            u64 x = (u64)(platform->max_font_size) - 1;
            for (u64 i = 0; i < 5; i += 1) x |= (x >> (1 << i));
            next_power_of_2_from_dpi = x + 1;
        }
        i32 atlas_dimension = (i32)next_power_of_2_from_dpi;

        platform->fontstash_context = sfons_create(&(sfons_desc_t){
            .width = atlas_dimension,
            .height = atlas_dimension,
        });

        String font_ttf_file = { .data = platform__font_ttf_chars, .count = platform__font_ttf_size };

        platform->font_id = fonsAddFontMem(platform->fontstash_context, "sans", (unsigned char *)font_ttf_file.data, (i32)font_ttf_file.count, false);
        fonsSetFont(platform->fontstash_context, platform->font_id);
    }

    V4 clear_color = rgba_float_from_hex(platform->base.clear_color);
    memcpy(&platform->pass_action.colors[0].clear_value, &clear_color, sizeof clear_color);
}

static void sokol_frame(void *user_data) {
    Platform *platform = user_data;

    sokol_populate_frame_info(&platform->base.frame);
    Scratch scratch = scratch_begin(platform->base.frame_arena);
    platform->base.draw_commands = (Array_Draw_Command){ .arena = scratch.arena };

    if (platform->fontstash_context != 0) fonsClearState(platform->fontstash_context);

    sg_begin_pass(&(sg_pass){
        .action = platform->pass_action,
        .swapchain = sglue_swapchain(),
    });
    sg_apply_pipeline(platform->pipeline);

    V4 clear_color = rgba_float_from_hex(platform->base.clear_color);
    memcpy(&platform->pass_action.colors[0].clear_value, &clear_color, sizeof clear_color);
    app_update_and_render(&platform->base);

    for (u64 i = 0; i < platform->base.draw_commands.count; i += 1) {
        Draw_Command *command = &platform->base.draw_commands.data[i];
        switch (command->kind) {
            case Draw_Kind_TEXT: {
                u32 font_color_abgr = 0;
                {
                    u32 c = command->color[Draw_Color_SOLID];
                    u8 r = (u8)(c >> 24);
                    u8 g = (u8)(c >> 16);
                    u8 b = (u8)(c >> 8);
                    u8 a = (u8)(c);
                    font_color_abgr = sfons_rgba(r, g, b, a);
                }

                fonsSetSize(platform->fontstash_context, command->text.font_size);

                String s = command->text.string;

                fonsSetAlign(platform->fontstash_context, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
                fonsSetColor(platform->fontstash_context, font_color_abgr);
                fonsDrawText(platform->fontstash_context, command->position.x, command->position.y, (char *)s.data, (char *)s.data + s.count);
            } break;
            case Draw_Kind_RECTANGLE: {
                assert(command->rectangle.size.x >= 0);
                assert(command->rectangle.size.y >= 0);

                M3 model = m3_fill_diagonal(1.f);

                u32 *c = command->color;

                vertex_parameters_t vertex_parameters = {
                    .kind = 0,
                    .model = m4_from_top_left_m3(model),
                    .inverse_model = m4_from_top_left_m3(m3_inverse(model)),
                    .border_data = { .x = command->rectangle.border_width, .y = command->rectangle.border_radius },
                };

                vertex_parameters.framebuffer_size = platform->base.frame.window_size;

                vertex_parameters.top_left_pixels = command->position;

                vertex_parameters.bottom_right_pixels = v2_add(command->position, command->rectangle.size);

                vertex_parameters.color = rgba_float_from_hex(c[Draw_Color_SOLID]);
                vertex_parameters.bottom_left_color = rgba_float_from_hex(command->gradient ? c[Draw_Color_BOTTOM_LEFT] : c[Draw_Color_SOLID]);
                vertex_parameters.bottom_right_color = rgba_float_from_hex(command->gradient ? c[Draw_Color_BOTTOM_RIGHT] : c[Draw_Color_SOLID]);
                vertex_parameters.top_right_color = rgba_float_from_hex(command->gradient ? c[Draw_Color_TOP_RIGHT] : c[Draw_Color_SOLID]);
                vertex_parameters.border_color = rgba_float_from_hex(command->rectangle.border_color);

                _Static_assert(sizeof(vertex_parameters_t) == sizeof(fragment_parameters_t), "");
                fragment_parameters_t fragment_parameters;
                memcpy(&fragment_parameters, &vertex_parameters, sizeof vertex_parameters);

                sg_apply_uniforms(UB_vertex_parameters, &SG_RANGE(vertex_parameters));
                sg_apply_uniforms(UB_fragment_parameters, &SG_RANGE(fragment_parameters));

                sg_draw(0, 6, 1);
            } break;
            case Draw_Kind_QUADRILATERAL: {
                u32 *c = command->color;
                vertex_parameters_t vertex_parameters = {
                    .kind = 1,
                    .model = m4_fill_diagonal(1.f),
                };

                vertex_parameters.framebuffer_size = platform->base.frame.window_size;

                vertex_parameters.quad_top_left = command->quadrilateral.corners[Draw_Corner_TOP_LEFT];
                vertex_parameters.quad_bottom_left = command->quadrilateral.corners[Draw_Corner_BOTTOM_LEFT];
                vertex_parameters.quad_bottom_right = command->quadrilateral.corners[Draw_Corner_BOTTOM_RIGHT];
                vertex_parameters.quad_top_right = command->quadrilateral.corners[Draw_Corner_TOP_RIGHT];

                vertex_parameters.color = rgba_float_from_hex(c[Draw_Color_SOLID]);
                vertex_parameters.bottom_left_color = rgba_float_from_hex(command->gradient ? c[Draw_Color_BOTTOM_LEFT] : c[Draw_Color_SOLID]);
                vertex_parameters.bottom_right_color = rgba_float_from_hex(command->gradient ? c[Draw_Color_BOTTOM_RIGHT] : c[Draw_Color_SOLID]);
                vertex_parameters.top_right_color = rgba_float_from_hex(command->gradient ? c[Draw_Color_TOP_RIGHT] : c[Draw_Color_SOLID]);
                vertex_parameters.border_color = rgba_float_from_hex(command->rectangle.border_color);

                _Static_assert(sizeof(vertex_parameters_t) == sizeof(fragment_parameters_t), "");
                fragment_parameters_t fragment_parameters;
                memcpy(&fragment_parameters, &vertex_parameters, sizeof vertex_parameters);

                sg_apply_uniforms(UB_vertex_parameters, &SG_RANGE(vertex_parameters));
                sg_apply_uniforms(UB_fragment_parameters, &SG_RANGE(fragment_parameters));

                sg_draw(0, 6, 1);
            } break;
            default: unreachable;
        }
    }

    sfons_flush(platform->fontstash_context);
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0, platform->base.frame.window_size.x, platform->base.frame.window_size.y, 0, -1.f, 1.f);
    sgl_draw();

    sg_end_pass();
    sg_commit();

    platform->base.frame.scroll = 0;
    memset(platform->base.frame.mouse_clicked, 0, sizeof platform->base.frame.mouse_clicked);
    memset(platform->base.frame.key_pressed, 0, sizeof platform->base.frame.key_pressed);
    scratch_end(scratch);

    if (platform->base.should_quit) sapp_quit();
}

static V2 platform_measure_text(Platform_Base *platform, String text, f32 font_size) {
    FONScontext *fontstash_context = ((Platform *)platform)->fontstash_context;
    fonsSetSize(fontstash_context, font_size);

    f32 ascender, descender, line_height;
    fonsVertMetrics(fontstash_context, &ascender, &descender, &line_height);

    f32 bounds[4] = {0};
    String s = text;
    f32 advance = fonsTextBounds(fontstash_context, 0, 0, (char *)s.data, (char *)s.data + s.count, bounds);

    V2 result = { .x = advance, .y = line_height };
    return result;
}

static void program(void) {
    static Arena persistent_arena;
    persistent_arena = arena_init(32 * 1024 * 1024);
    static Arena frame_arena;
    frame_arena = arena_init(32 * 1024 * 1024);

    static Platform platform = {
        .base = {
            .persistent_arena = &persistent_arena,
            .frame_arena = &frame_arena,
        },
    };

    sapp_run(&(sapp_desc){
        .user_data = &platform,
        .init_userdata_cb = sokol_init,
        .frame_userdata_cb = sokol_frame,
        .cleanup_userdata_cb = sokol_cleanup,
        .event_userdata_cb = sokol_event,
        .window_title = "app",
        .logger.func = sokol_logger,
        .high_dpi = true,
    });
}


#endif // defined(BASE_IMPLEMENTATION)
