#define BASE_PLATFORM_WINGDI

// TODO(felix): replace pragma comment(lib) with meta(link)

#pragma comment(lib, "Dwmapi.lib")
WIN32_CALL(long) DwmFlush(void);

#pragma comment(lib, "gdi32.lib")
WIN32_CALL(void *) CreateCompatibleDC(void *device_context_handle);
#if !defined(_WINGDI_)
    typedef struct {
        unsigned long biSize;
        long biWidth;
        long biHeight;
        unsigned short biPlanes;
        unsigned short biBitCount;
        unsigned long biCompression;
        unsigned long biSizeImage;
        long biXPelsPerMeter;
        long biYPelsPerMeter;
        unsigned long biClrUsed;
        unsigned long biClrImportant;
    } BITMAPINFOHEADER;
    typedef struct {
        unsigned char rgbBlue;
        unsigned char rgbGreen;
        unsigned char rgbRed;
        unsigned char rgbReserved;
    } RGBQUAD;
    typedef struct {
        BITMAPINFOHEADER bmiHeader;
        RGBQUAD bmiColors[1];
    } BITMAPINFO;
#endif
WIN32_CALL(void *) CreateDIBSection(void *device_context_handle, const BITMAPINFO *, unsigned usage, void **bits, void *section_handle, unsigned long offset);
WIN32_CALL(int)    DeleteDC(void *device_context_handle);
WIN32_CALL(int)    DeleteObject(void *object_handle);
WIN32_CALL(void *) SelectObject(void *device_context_handle, void *object_handle);
WIN32_CALL(int)    StretchBlt(void *destination_device_context_handle, int destination_x, int destination_y, int destination_width, int destination_height, void *source_device_context_handle, int source_x, int source_y, int source_width, int source_height, unsigned long raster_operation);

#pragma comment(lib, "user32.lib")
WIN32_CALL(void *)    CreateWindowExA(unsigned long extended_style, const char *class_name, const char *window_name, unsigned long style, int x, int y, int width, int height, void *window_parent_handle, void *menu_handle, void *instance_handle, void *param);
WIN32_CALL(long long) DefWindowProcA(void *window_handle, unsigned message, unsigned long long wparam, long long lparam);
WIN32_CALL(int)       DestroyWindow(void *window_handle);
#if !defined(_WINUSER_)
    #if !defined(_WINDEF_)
        typedef struct { long x, y; } POINT;
    #endif
    typedef struct {
        void *hwnd;
        unsigned message;
        unsigned long long wParam;
        long long lParam;
        unsigned long time;
        POINT pt;
        unsigned long lPrivate;
    } MSG;
#endif
WIN32_CALL(long long) DispatchMessageA(const MSG *);
WIN32_CALL(void *)    GetDC(void *window_handle);
#if !defined(_WINUSER_)
    typedef struct {
        POINT ptReserved;
        POINT ptMaxSize;
        POINT ptMaxPosition;
        POINT ptMinTrackSize;
        POINT ptMaxTrackSize;
    } MINMAXINFO;

    #if !defined(_WINDEF_)
        typedef struct {
            long left;
            long top;
            long right;
            long bottom;
        } RECT;
    #endif
    typedef struct {
        unsigned long cbSize;
        RECT rcMonitor;
        RECT rcWork;
        unsigned long dwFlags;
    } MONITORINFO;
#endif
WIN32_CALL(unsigned) GetDpiForWindow(void *window_handle);
WIN32_CALL(int)      GetMonitorInfoA(void *monitor_handle, MONITORINFO *);
#if !defined(_WINUSER_)
    #define MAKEINTRESOURCEA(i) ((char *)((unsigned long)((unsigned short)(i))))
    #define MAKEINTRESOURCE MAKEINTRESOURCEA
    #define IDC_ARROW MAKEINTRESOURCE(32512)
#endif
WIN32_CALL(void *)   LoadCursorA(void *instance_handle, const char *cursor_name);
WIN32_CALL(void *)   MonitorFromWindow(void *window_handle, unsigned long flags);
WIN32_CALL(int)      PeekMessageA(MSG *, void *window_handle, unsigned message_filter_min, unsigned message_filter_max, unsigned remove_message);
WIN32_CALL(void)     PostQuitMessage(int exit_code);
#if !defined(_WINUSER_)
    typedef long long (*WNDPROC)(void *window_handle, unsigned message, unsigned long long wparam, long long lparam);
    typedef struct {
      unsigned style;
      WNDPROC lpfnWndProc;
      int cbClsExtra;
      int cbWndExtra;
      void *hInstance;
      void *hIcon;
      void *hCursor;
      void *hbrBackground;
      const char *lpszMenuName;
      const char *lpszClassName;
    } WNDCLASSA;
#endif
WIN32_CALL(unsigned short) RegisterClassA(const WNDCLASSA *);
WIN32_CALL(int)            ReleaseDC(void *window_handle, void *device_context_handle);
#if !defined(_WINDEF_)
    #define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
    DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
    #define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
WIN32_CALL(int)            SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT);
WIN32_CALL(int)            ShowWindow(void *window_handle, int show_command);
WIN32_CALL(int)            TranslateMessage(const MSG *);

struct Platform_Implementation {
    Platform base;

    u32 *pixels;
    f32 real_over_virtual_window_scale;
    V2 real_window_size;
    V2 real_mouse_position;

    void *window;
    void *memory_dc;
    void *bitmap;

    cpu_draw_Font font;
};

static V2 platform_measure_text(Platform *platform, String text, f32 font_size) {
    return cpu_draw_platform_measure_text(&((Platform_Implementation *)platform)->font, text, font_size);
}

static App_Key app_key_from_win32(u64 winkey) {
    App_Key key = 0;

    // https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

    switch (winkey) {
        case 0x10: key = App_Key_SHIFT; break;
        case 0x11: key = App_Key_CONTROL; break;
        case 0x25: key = App_Key_LEFT; break;
        case 0x26: key = App_Key_UP; break;
        case 0x27: key = App_Key_RIGHT; break;
        case 0x28: key = App_Key_DOWN; break;
        case 0xbb: key = '='; break;
        case 0xbd: key = '-'; break;
        default: {
            bool is_same = ' ' <= winkey && winkey <= 'Z';
            if (is_same) {
                key = (App_Key)winkey;
                break;
            }

            bool is_function_key = 0x70 <= winkey && winkey <= 0x87;
            if (is_function_key) {
                key = (App_Key)(winkey - 0x70 + App_Key_F1);
                break;
            }

            static bool once = false;
            if (!once) {
                log_info("unimplemented translation from win32 keycode <0x%x> (logging ONCE for all keys)", winkey);
                once = true;
            }
        } break;
    }

    return key;
}

#define WIN32__LOWORD(l) ((unsigned short)(((unsigned long long)(l)) & 0xffff))
#define WIN32__HIWORD(l) ((unsigned short)((((unsigned long long)(l)) >> 16) & 0xffff))
#define WIN32__GET_X_LPARAM(l) ((int)(short)WIN32__LOWORD(l))
#define WIN32__GET_Y_LPARAM(l) ((int)(short)WIN32__HIWORD(l))
static Platform_Implementation *window_procedure__platform;
static i64 window_procedure__(void *window, u32 message, u64 w, i64 l) {
    Platform_Implementation *platform = window_procedure__platform;
    App_Frame_Info *frame = &platform->base.frame;

    enum {
        wm_destroy       = 0x0002,
        wm_size          = 0x0005,
        wm_close         = 0x0010,
        wm_getminmaxinfo = 0x0024,
        wm_displaychange = 0x007E,
        wm_keydown       = 0x0100,
        wm_keyup         = 0x0101,
        wm_mousemove     = 0x0200,
        wm_mousewheel    = 0x020a,
        wm_lbuttondown   = 0x0201,
        wm_lbuttonup     = 0x0202,
        wm_rbuttondown   = 0x0204,
        wm_rbuttonup     = 0x0205,
        wm_sizing        = 0x0214,
        wm_dpichanged    = 0x02E0,
    };

    switch (message) {
        case wm_close: platform->base.should_quit = true; break;
        case wm_destroy: PostQuitMessage(0); break;
        case wm_displaychange: {
            enum { monitor_defaulttonearest = 2 }; // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfrompoint
            void *monitor = MonitorFromWindow(window, monitor_defaulttonearest);
            MONITORINFO info = { .cbSize = sizeof info };
            int ok = GetMonitorInfoA(monitor, &info);
            ensure(ok);
            platform->real_window_size.x = (f32)(info.rcMonitor.right - info.rcMonitor.left);
            platform->real_window_size.y = (f32)(info.rcMonitor.bottom - info.rcMonitor.top);
        } break;
        case wm_dpichanged: {
            f32 dpi_1x = 96.f;
            frame->dpi_scale = (f32)GetDpiForWindow(window) / dpi_1x;
        } break;
        case wm_getminmaxinfo: { // TODO(felix): not working. The goal is to not allow the window to become smaller than a certain size
            MINMAXINFO *info = (MINMAXINFO *)l;
            int min_width = 100, min_height = 100;
            info->ptMinTrackSize.x = min_width;
            info->ptMinTrackSize.y = min_height;
        } break;
        case wm_keydown: {
            App_Key key = app_key_from_win32(w);
            frame->key_pressed[key] = !frame->key_down[key];
            frame->key_down[key] = true;
        } break;
        case wm_keyup: {
            App_Key key = app_key_from_win32(w);
            frame->key_down[key] = false;
        } break;
        case wm_lbuttondown: {
            App_Mouse_Button button = App_Mouse_LEFT;
            frame->mouse_clicked[button] = !frame->mouse_down[button];
            frame->mouse_down[button] = true;
        } break;
        case wm_lbuttonup: {
            App_Mouse_Button button = App_Mouse_LEFT;
            frame->mouse_down[button] = false;
        } break;
        case wm_mousemove: {
            platform->real_mouse_position.x = (f32)WIN32__GET_X_LPARAM(l);
            platform->real_mouse_position.y = (f32)WIN32__GET_Y_LPARAM(l);
        } break;
        case wm_mousewheel: {
            i16 delta = (i16)WIN32__HIWORD(w);
            frame->scroll = (f32)delta / 120.f;
        } break;
        case wm_rbuttondown: {
            App_Mouse_Button button = App_Mouse_RIGHT;
            frame->mouse_clicked[button] = !frame->mouse_down[button];
            frame->mouse_down[button] = true;
        } break;
        case wm_rbuttonup: {
            App_Mouse_Button button = App_Mouse_RIGHT;
            frame->mouse_down[button] = false;
        } break;
        case wm_size: {
            platform->real_window_size.x = (f32)WIN32__LOWORD(l);
            platform->real_window_size.y = (f32)WIN32__HIWORD(l);
        } break;
        case wm_sizing: {
            // TODO(felix): handle redrawing here so that we're not blocked while resizing
            // NOTE(felix): apparently there might be some problems with this and the better way to do it is for the thread with window events to be different than the application thread that submits draw calls and updates the program
        } break;
        default: break;
    }
    return DefWindowProcA(window, message, w, l);
}

static void program(void) {
    static Arena persistent_arena;
    persistent_arena = arena_init(4 * 1024 * 1024);
    static Arena frame_arena;
    frame_arena = arena_init(4 * 1024 * 1024);

    static Platform_Implementation platform = { .base = {
        .persistent_arena = &persistent_arena,
        .frame_arena = &frame_arena,
    } };
    App_Frame_Info *frame = &platform.base.frame;

    app_start(&platform.base);

    const char *window_name = platform.base.window_name != 0 ? platform.base.window_name : "app";

    window_procedure__platform = &platform;
    WNDCLASSA window_class = {
        .lpfnWndProc = window_procedure__,
        .hCursor = LoadCursorA(0, IDC_ARROW),
        .lpszClassName = window_name,
    };
    unsigned short atom = RegisterClassA(&window_class);
    ensure(atom != 0);

    int bool_ok = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ensure(bool_ok);

    {
        unsigned long extended_style = 0;
        const char *class_name = window_name;
        int cw_usedefault = (int)0x80000000;
        int x = cw_usedefault, y = cw_usedefault, window_width = cw_usedefault, window_height = cw_usedefault;

        enum { // https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles
            ws_overlapped = 0x00000000L,
            ws_maximizebox = 0x00010000L,
            ws_minimizebox = 0x00020000L,
            ws_thickframe = 0x00040000L,
            ws_sysmenu = 0x00080000L,
            ws_caption = 0x00C00000L,

            ws_overlappedwindow = (ws_overlapped | ws_caption | ws_sysmenu | ws_thickframe | ws_minimizebox | ws_maximizebox),
        };

        platform.window = CreateWindowExA(extended_style, class_name, window_name, ws_overlappedwindow, x, y, window_width, window_height, 0, 0, 0, 0);
        ensure(platform.window != 0);
    }

    enum { // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
        sw_showdefault = 10,
    };
    ShowWindow(platform.window, sw_showdefault);

    if (platform.real_over_virtual_window_scale == 0.f) platform.real_over_virtual_window_scale = 3.f;
    frame->dpi_scale = 1.f;

    i32 max_virtual_width = 1920;
    i32 max_virtual_height = 1200;

    {
        void *window_dc = GetDC(platform.window);
        ensure(window_dc != 0);

        BITMAPINFOHEADER bitmap_info_header = {
            .biSize = sizeof bitmap_info_header,
            .biWidth = max_virtual_width,
            .biHeight = -max_virtual_height,
            .biPlanes = 1,
            .biBitCount = sizeof(*platform.pixels) * 8,
        };

        platform.memory_dc = CreateCompatibleDC(window_dc);
        ensure(platform.memory_dc != 0);

        enum { dib_rgb_colors = 0 };
        platform.bitmap = CreateDIBSection(platform.memory_dc, (BITMAPINFO *)&bitmap_info_header, dib_rgb_colors, (void **)&platform.pixels, 0, 0);
        ensure(platform.bitmap != 0);

        SelectObject(platform.memory_dc, platform.bitmap);

        ReleaseDC(platform.window, window_dc);
    }

    platform.font = cpu_draw_font_from_bdf(platform__font_bytes, platform__font_size);

    for (Arena frame_scratch = *platform.base.frame_arena; !platform.base.should_quit; *platform.base.frame_arena = frame_scratch) {
        platform.base.draw_commands = (Array_Draw_Command){0};

        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagea
        enum { pm_remove = 0x0001 };

        enum { wm_quit = 0x0012 };

        for (MSG message = {0}; PeekMessageA(&message, 0, 0, 0, pm_remove);) {
            if (message.message == wm_quit) {
                platform.base.should_quit = true;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        if (platform.base.should_quit) break;

        f32 actual_real_over_virtual_window_scale = platform.real_over_virtual_window_scale;
        actual_real_over_virtual_window_scale = ceilf(MAX(platform.real_window_size.x / (f32)max_virtual_width, actual_real_over_virtual_window_scale));
        actual_real_over_virtual_window_scale = ceilf(MAX(platform.real_window_size.y / (f32)max_virtual_height, actual_real_over_virtual_window_scale));
        assert(actual_real_over_virtual_window_scale > 0);

        platform.base.frame.window_size = v2_scale(platform.real_window_size, 1.f / actual_real_over_virtual_window_scale);

        for (u64 i = 0; i < 2; i += 1) {
            v(frame->mouse_position)[i] = v(platform.real_mouse_position)[i] * (v(frame->window_size)[i] / v(platform.real_window_size)[i]);
        }

        static Time_Averaged_Sampler sampler = {0};
        static bool sampler_init = false;
        if (!sampler_init) {
            #define PLATFORM__TIME_SAMPLES 16
            static f64 samples[PLATFORM__TIME_SAMPLES] = {0};

            sampler = time_averaged_sampler(samples, PLATFORM__TIME_SAMPLES);
            sampler_init = true;
        }
        platform.base.frame.seconds_since_last_frame = (f32)time_averaged_sample_elapsed_seconds(&sampler);

        app_update_and_render(&platform.base);

        i32 virtual_width = (i32)frame->window_size.x;
        i32 virtual_height = (i32)frame->window_size.y;

        cpu_draw_Surface surface = cpu_draw_surface(platform.pixels, max_virtual_width, virtual_width, virtual_height);

        u32 clear = argb_from_rgba(platform.base.clear_color);
        cpu_draw_rectangle(surface, 0, 0, surface.clip_right - surface.clip_left, surface.clip_bottom - surface.clip_top, clear);
        cpu_draw_commands(surface, &platform.font, platform.base.draw_commands.data, platform.base.draw_commands.count);

        void *window_dc = GetDC(platform.window);
        ensure(window_dc != 0);
        {
            i32 real_width = (i32)platform.real_window_size.x;
            i32 real_height = (i32)platform.real_window_size.y;

            enum { srccopy = 0x00CC0020 };

            int ok = StretchBlt(
                window_dc,
                0, 0, real_width, real_height,
                platform.memory_dc,
                0, 0, virtual_width, virtual_height,
                srccopy
            );
            ensure(ok);

            #pragma comment(lib, "dwmapi.lib")
            DwmFlush(); // TODO(felix): better way to "vsync"
        }
        ReleaseDC(platform.window, window_dc);

        frame->scroll = 0;
        memset(frame->mouse_clicked, 0, sizeof frame->mouse_clicked);
        memset(frame->key_pressed, 0, sizeof frame->key_pressed);

        if (platform.base.should_quit) break;
    }

    DestroyWindow(platform.window);
    DeleteObject(platform.bitmap);
    DeleteDC(platform.memory_dc);

    app_quit(&platform.base);
}
