// https://github.com/felix-u 2026-04-16
// Public domain. NO WARRANTY - use at your own risk.

#if !defined(CPU_DRAW_H)
#define CPU_DRAW_H


typedef struct {
    unsigned *pixels;
    int stride;
    int height;
    int clip_left;
    int clip_top;
    int clip_right;
    int clip_bottom;
} cpu_draw_Surface;

#if !defined(CPU_DRAW_GLYPH_MAX_WIDTH_OVER_8)
    #define CPU_DRAW_GLYPH_MAX_WIDTH_OVER_8 2
#endif
#if !defined(CPU_DRAW_GLYPH_MAX_HEIGHT)
    #define CPU_DRAW_GLYPH_MAX_HEIGHT 16
#endif

typedef struct {
    signed char width;
    signed char bound_width, bound_height, bound_left, bound_bottom;
    unsigned char bytes[CPU_DRAW_GLYPH_MAX_WIDTH_OVER_8 * CPU_DRAW_GLYPH_MAX_HEIGHT];
} cpu_draw_Glyph;

typedef struct {
    int height;
    int ascent, descent;
    cpu_draw_Glyph glyphs[96];
} cpu_draw_Font;

#if !defined(CPU_DRAW_SPRITE_MAX_WIDTH_OVER_8)
    #define CPU_DRAW_SPRITE_MAX_WIDTH_OVER_8 CPU_DRAW_GLYPH_MAX_WIDTH_OVER_8
#endif
#if !defined(CPU_DRAW_SPRITE_MAX_HEIGHT)
    #define CPU_DRAW_SPRITE_MAX_HEIGHT CPU_DRAW_GLYPH_MAX_HEIGHT
#endif

typedef struct {
    signed char width;
    signed char height;
    unsigned char bytes[CPU_DRAW_SPRITE_MAX_WIDTH_OVER_8 * CPU_DRAW_GLYPH_MAX_HEIGHT];
} cpu_draw_Sprite;

#if !defined(CPU_DRAW_FUNCTION)
    #define CPU_DRAW_FUNCTION
#endif

CPU_DRAW_FUNCTION void cpu_draw_line(cpu_draw_Surface surface, int start_x, int start_y, int end_x, int end_y, unsigned color);
CPU_DRAW_FUNCTION void cpu_draw_pixel_if_in_bounds(cpu_draw_Surface surface, int x, int y, unsigned color);
CPU_DRAW_FUNCTION void cpu_draw_rectangle(cpu_draw_Surface surface, int left, int top, int right, int bottom, unsigned color);
CPU_DRAW_FUNCTION void cpu_draw_rectangle_rotated(cpu_draw_Surface surface, int left, int top, int right, int bottom, unsigned color, float radians, int pivot_x, int pivot_y);
CPU_DRAW_FUNCTION void cpu_draw_rounded_rectangle(cpu_draw_Surface surface, int left, int top, int right, int bottom, unsigned fill_color, unsigned border_color, int border_radius, int border_width);
CPU_DRAW_FUNCTION void cpu_draw_sdf_corners(cpu_draw_Surface surface, int left, int top, int right, int bottom, unsigned fill_color, unsigned border_color, int border_radius, int border_width);
CPU_DRAW_FUNCTION void cpu_draw_sprite_1bit(cpu_draw_Surface surface, int x, int y, cpu_draw_Sprite sprite, unsigned color);
CPU_DRAW_FUNCTION cpu_draw_Surface cpu_draw_subsurface(cpu_draw_Surface surface, int x, int y, int width, int height);
CPU_DRAW_FUNCTION cpu_draw_Surface cpu_draw_surface(unsigned *pixels, int stride, int width, int height);
CPU_DRAW_FUNCTION void cpu_draw_text(cpu_draw_Surface surface, const cpu_draw_Font *font, int left, int top, unsigned color, const char *text, int length, int *only_measure_width);
CPU_DRAW_FUNCTION void cpu_draw_triangle(cpu_draw_Surface surface, int ax, int ay, int bx, int by, int cx, int cy, unsigned color);
CPU_DRAW_FUNCTION void cpu_draw_triangle_interpolate(cpu_draw_Surface surface, int ax, int ay, int bx, int by, int cx, int cy, unsigned acolor, unsigned bcolor, unsigned ccolor);


#endif // CPU_DRAW_H


#if defined(CPU_DRAW_IMPLEMENTATION)


#if !defined(CPU_DRAW_ASSERT)
    #include <assert.h>
    #define CPU_DRAW_ASSERT assert
#endif

#if !defined(CPU_DRAW_SINF)
    #include <math.h>
    #define CPU_DRAW_SINF sinf
    #define CPU_DRAW_COSF cosf
    #define CPU_DRAW_ROUNDF roundf
    #define CPU_DRAW_SQRTF sqrtf
#endif

#define CPU_DRAW__SWAP(a, b) do {\
    int swap__temp__ = *(a); *(a) = *(b); *(b) = swap__temp__; \
} while (0)

#define CPU_DRAW__ABS(x) ((x) < 0 ? -(x) : (x))

#define CPU_DRAW__MIN(a, b) ((a) < (b) ? (a) : (b))
#define CPU_DRAW__MAX(a, b) ((a) > (b) ? (a) : (b))

CPU_DRAW_FUNCTION void cpu_draw_line(cpu_draw_Surface surface, int start_x, int start_y, int end_x, int end_y, unsigned color) {
    // Reference: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm

    // TODO(felix): investigate possibility of hoisting bounds check out of loop

    _Bool steep = CPU_DRAW__ABS(end_y - start_y) > CPU_DRAW__ABS(end_x - start_x);
    if (steep) {
        _Bool bottom_to_top = start_y > end_y;
        if (bottom_to_top) {
            CPU_DRAW__SWAP(&start_x, &end_x);
            CPU_DRAW__SWAP(&start_y, &end_y);
        }

        int dx = end_x - start_x;
        int dy = end_y - start_y;
        int x_add = 1;

        if (dx < 0) {
            x_add = -1;
            dx = -dx;
        }

        int difference = 2 * dx - dy;
        int x = start_x;

        start_y = CPU_DRAW__MAX(start_y, surface.clip_top);
        end_y = CPU_DRAW__MIN(end_y, surface.clip_bottom);

        for (int y = start_y; y < end_y; y += 1) {
            cpu_draw_pixel_if_in_bounds(surface, x, y, color);
            if (difference > 0) {
                x += x_add;
                difference += 2 * (dx - dy);
            } else difference += 2 * dx;
        }
    } else {
        _Bool right_to_left = start_x > end_x;
        if (right_to_left) {
            CPU_DRAW__SWAP(&start_x, &end_x);
            CPU_DRAW__SWAP(&start_y, &end_y);
        }

        int dx = end_x - start_x;
        int dy = end_y - start_y;
        int y_add = 1;

        if (dy < 0) {
            y_add = -1;
            dy = -dy;
        }

        int difference = 2 * dy - dx;
        int y = start_y;

        for (int x = start_x; x <= end_x; x += 1) {
            cpu_draw_pixel_if_in_bounds(surface, x, y, color);
            if (difference > 0) {
                y += y_add;
                difference += 2 * (dy - dx);
            } else difference += 2 * dy;
        }
    }
}

CPU_DRAW_FUNCTION void cpu_draw_pixel_if_in_bounds(cpu_draw_Surface surface, int x, int y, unsigned color) {
    _Bool draw = surface.clip_left <= x && x < surface.clip_right;
    draw = draw && surface.clip_top <= y && y < surface.clip_bottom;
    if (draw) surface.pixels[y * surface.stride + x] = color;
}

CPU_DRAW_FUNCTION void cpu_draw_rectangle(cpu_draw_Surface surface, int left, int top, int right, int bottom, unsigned color) {
    left = CPU_DRAW__MAX(left, surface.clip_left);
    right = CPU_DRAW__MIN(right, surface.clip_right);
    top = CPU_DRAW__MAX(top, surface.clip_top);
    bottom = CPU_DRAW__MIN(bottom, surface.clip_bottom);

    int row = top * surface.stride;
    for (int y = top; y < bottom; y += 1, row += surface.stride) {
        for (int x = left; x < right; x += 1) {
            surface.pixels[row + x] = color;
        }
    }
}

typedef struct { int x, y; } cpu_draw__V2;
static inline cpu_draw__V2 cpu_draw__v2_add(cpu_draw__V2 b, cpu_draw__V2 a) {
    cpu_draw__V2 result = { .x = b.x + a.x, .y = b.y + a.y };
    return result;
}
static inline cpu_draw__V2 cpu_draw__v2_rotate(cpu_draw__V2 v, float radians) {
    float sin_angle = CPU_DRAW_SINF(radians);
    float cos_angle = CPU_DRAW_COSF(radians);

    float vx = (float)v.x;
    float vy = (float)v.y;
    float x = vx * cos_angle - vy * sin_angle;
    float y = vx * sin_angle + vy * cos_angle;

    cpu_draw__V2 result = { .x = (int)CPU_DRAW_ROUNDF(x), .y = (int)CPU_DRAW_ROUNDF(y) };
    return result;
}
static inline cpu_draw__V2 cpu_draw__v2_sub(cpu_draw__V2 b, cpu_draw__V2 a) {
    cpu_draw__V2 result = { .x = b.x - a.x, .y = b.y - a.y };
    return result;
}

CPU_DRAW_FUNCTION void cpu_draw_rectangle_rotated(cpu_draw_Surface surface, int left, int top, int right, int bottom, unsigned color, float radians, int pivot_x, int pivot_y) {
    enum { TOP_LEFT, BOTTOM_LEFT, BOTTOM_RIGHT, TOP_RIGHT };
    cpu_draw__V2 corners[4] = {
        [TOP_LEFT] = { .x = left, .y = top },
        [BOTTOM_LEFT] = { .x = left, .y = bottom },
        [BOTTOM_RIGHT] = { .x = right, .y = bottom },
        [TOP_RIGHT] = { .x = right, .y = top },
    };

    cpu_draw__V2 pivot = { .x = pivot_x, .y = pivot_y };

    for (int i = 0; i < 4; i += 1) {
        cpu_draw__V2 dist = cpu_draw__v2_sub(corners[i], pivot);
        dist = cpu_draw__v2_rotate(dist, radians);
        corners[i] = cpu_draw__v2_add(pivot, dist);
    }

    cpu_draw__V2
        tl = corners[TOP_LEFT],
        bl = corners[BOTTOM_LEFT],
        br = corners[BOTTOM_RIGHT],
        tr = corners[TOP_RIGHT];

    cpu_draw_triangle(surface, tl.x, tl.y, bl.x, bl.y, br.x, br.y, color);
    cpu_draw_triangle(surface, tl.x, tl.y, br.x, br.y, tr.x, tr.y, color);
}

CPU_DRAW_FUNCTION void cpu_draw_rounded_rectangle(cpu_draw_Surface surface, int left, int top, int right, int bottom, unsigned fill_color, unsigned border_color, int border_radius, int border_width) {
    cpu_draw_sdf_corners(surface, left, top, right, bottom, fill_color, border_color, border_radius, border_width);

    // TODO(felix): bound here, then use non-bounds-checked cpu_draw function variants below

    int in_left = left + border_radius;
    int in_right = right - border_radius;
    int in_top = top + border_radius;
    int in_bottom = bottom - border_radius;

    if (fill_color != 0) {
        // top between corners
        cpu_draw_rectangle(surface, in_left, top + border_width, in_right, in_top, fill_color);
        // middle
        cpu_draw_rectangle(surface, left + border_width, in_top, right - border_width, in_bottom, fill_color);
        // bottom between corners
        cpu_draw_rectangle(surface, in_left, in_bottom, in_right, bottom - border_width, fill_color);
    }

    if (border_color != 0) {
        // top between corners
        cpu_draw_rectangle(surface, in_left, top, in_right, top + border_width, border_color);
        // middle left
        cpu_draw_rectangle(surface, left, in_top, left + border_width, in_bottom, border_color);
        // middle right
        cpu_draw_rectangle(surface, right - border_width, in_top, right, in_bottom, border_color);
        // bottom between corners
        cpu_draw_rectangle(surface, in_left, bottom - border_width, in_right, bottom, border_color);
    }
}

CPU_DRAW_FUNCTION void cpu_draw_sdf_corners(cpu_draw_Surface surface, int left, int top, int right, int bottom, unsigned fill_color, unsigned border_color, int border_radius, int border_width) {
    float radius = (float)border_radius;

    int x_add[4] = { 1, -1, 1, -1 };
    int y_add[4] = { 1, 1, -1, -1 };
    int x_bound[4] = { left, right, left, right };
    int y_bound[4] = { top, top, bottom, bottom };

    for (int corner = 0; corner < 4; corner += 1) {
        int x_sign = x_add[corner];
        int y_sign = y_add[corner];

        for (int x = 0; x < border_radius; x += 1) {
            float distance_x = (float)(border_radius - x);

            for (int y = 0; y < border_radius; y += 1) {
                float distance_y = (float)(border_radius - y);
                float distance = CPU_DRAW_SQRTF(distance_x * distance_x + distance_y * distance_y);

                if (distance >= radius) continue;

                int x_coordinate = x_bound[corner] + x * x_sign - (x_sign == -1);
                int y_coordinate = y_bound[corner] + y * y_sign - (y_sign == -1);
                if (radius - distance <= (float)border_width && border_color != 0) {
                    cpu_draw_pixel_if_in_bounds(surface, x_coordinate, y_coordinate, border_color);
                } else if (fill_color != 0) {
                    cpu_draw_pixel_if_in_bounds(surface, x_coordinate, y_coordinate, fill_color);
                }
            }
        }
    }
}

CPU_DRAW_FUNCTION void cpu_draw_sprite_1bit(cpu_draw_Surface surface, int x, int y, cpu_draw_Sprite sprite, unsigned color) {
    // TODO(felix): move bounds checks outside loop
    for (int row = 0; row < (int)sprite.height; row += 1) {
        for (int column = 0; column < (int)sprite.width; column += 1) {
            unsigned char bits = sprite.bytes[row * CPU_DRAW_SPRITE_MAX_WIDTH_OVER_8 + column / 8];
            unsigned char bit = (bits >> (7 - (column & 7))) & 1;
            if (bit != 0) cpu_draw_pixel_if_in_bounds(surface, x + column, y + row, color);
        }
    }
}

CPU_DRAW_FUNCTION cpu_draw_Surface cpu_draw_subsurface(cpu_draw_Surface surface, int x, int y, int width, int height) {
    surface.clip_left = x;
    surface.clip_top = y;
    surface.clip_right = x + width;
    surface.clip_bottom = y + height;
    return surface;
}

CPU_DRAW_FUNCTION cpu_draw_Surface cpu_draw_surface(unsigned *pixels, int stride, int width, int height) {
    cpu_draw_Surface surface = {
        .pixels = pixels,
        .stride = stride,
        .clip_right = width,
        .clip_bottom = height,
    };
    return surface;
}

CPU_DRAW_FUNCTION void cpu_draw_text(cpu_draw_Surface surface, const cpu_draw_Font *font, int left, int top, unsigned color, const char *text, int length, int *only_measure_width) {
    if (only_measure_width != 0) *only_measure_width = 0;

    int x = left;
    for (int i = 0; i < length; i += 1) {
        char c = text[i];
        CPU_DRAW_ASSERT(c != 0);

        _Bool is_ascii = (c & 0x80) == 0x00;
        if (!is_ascii) {
            int bytes_in_codepoint = 0;
            if ((c & 0xc0) == 0xc0) {
                bytes_in_codepoint = 2;
                if ((c & 0x20) == 0x20) {
                    bytes_in_codepoint = 3;
                    if ((c & 0x10) == 0x10) {
                        bytes_in_codepoint = 4;
                        CPU_DRAW_ASSERT((c & 0x08) == 0x08);
                    }
                }
            }
            if (bytes_in_codepoint == 0) CPU_DRAW_ASSERT(0 && "malformed codepoint");

            CPU_DRAW_ASSERT(i + bytes_in_codepoint <= length);
            for (int j = 1; j < bytes_in_codepoint; j += 1) {
                c = text[i + j];
                CPU_DRAW_ASSERT((c & 0x80) == 0x80);
                CPU_DRAW_ASSERT((c & 0x40) == 0x00);
            }
            i += bytes_in_codepoint - 1;

            c = 127;
        }

        CPU_DRAW_ASSERT((unsigned long long)(c - 32) < (sizeof font->glyphs / sizeof *font->glyphs));

        cpu_draw_Glyph glyph = font->glyphs[c - 32];

        if (only_measure_width != 0) {
            *only_measure_width += glyph.width;
        } else {
            for (int row = 0; row < glyph.bound_height; row += 1) {
                int y = top + font->ascent - glyph.bound_bottom - glyph.bound_height + row;
                for (int column = 0; column < glyph.bound_width; column += 1) {
                    // TODO(felix): move bounds check outside
                    int local_x = x + glyph.bound_left + column;

                    _Bool in_bounds = (surface.clip_left <= local_x && local_x < surface.clip_right);
                    in_bounds = in_bounds && (surface.clip_top <= y && y < surface.clip_bottom);
                    if (!in_bounds) continue;

                    unsigned char bits = glyph.bytes[row * CPU_DRAW_GLYPH_MAX_WIDTH_OVER_8 + column / 8];
                    unsigned char bit = (bits >> (7 - (column & 7))) & 1;
                    if (bit != 0) surface.pixels[y * surface.stride + local_x] = color;
                }
            }

            x += glyph.width;
        }
    }
}

static inline float cpu_draw__signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
    float result = 0.5f * (float)((by - ay) * (bx + ax) + (cy - by) * (cx + bx) + (ay - cy) * (ax + cx));
    return result;
}

CPU_DRAW_FUNCTION void cpu_draw_triangle(cpu_draw_Surface surface, int ax, int ay, int bx, int by, int cx, int cy, unsigned color) {
    int bound_min_x = CPU_DRAW__MIN(CPU_DRAW__MIN(ax, bx), cx);
    int bound_min_y = CPU_DRAW__MIN(CPU_DRAW__MIN(ay, by), cy);
    int bound_max_x = CPU_DRAW__MAX(CPU_DRAW__MAX(ax, bx), cx);
    int bound_max_y = CPU_DRAW__MAX(CPU_DRAW__MAX(ay, by), cy);

    if (bound_max_x < surface.clip_left || surface.clip_right <= bound_min_x) return;
    if (bound_max_y < surface.clip_top || surface.clip_bottom <= bound_min_y) return;

    float total_area = cpu_draw__signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area == 0.f) return;
    float reciprocal_total_area = 1.f / total_area;

    bound_min_x = CPU_DRAW__MAX(0, bound_min_x);
    bound_min_y = CPU_DRAW__MAX(0, bound_min_y);
    bound_max_x = CPU_DRAW__MIN(surface.clip_right, bound_max_x + 1);
    bound_max_y = CPU_DRAW__MIN(surface.clip_bottom, bound_max_y + 1);

    for (int x = bound_min_x; x < bound_max_x; x += 1) {
        for (int y = bound_min_y; y < bound_max_y; y += 1) {
            float alpha = cpu_draw__signed_triangle_area(x, y, bx, by, cx, cy) * reciprocal_total_area;
            float beta = cpu_draw__signed_triangle_area(x, y, cx, cy, ax, ay) * reciprocal_total_area;
            float gamma = cpu_draw__signed_triangle_area(x, y, ax, ay, bx, by) * reciprocal_total_area;

            _Bool outside = alpha < 0 || beta < 0 || gamma < 0;
            if (outside) continue;

            surface.pixels[y * surface.stride + x] = color;
        }
    }
}

CPU_DRAW_FUNCTION void cpu_draw_triangle_interpolate(cpu_draw_Surface surface, int ax, int ay, int bx, int by, int cx, int cy, unsigned acolor, unsigned bcolor, unsigned ccolor) {
    int bound_min_x = CPU_DRAW__MIN(CPU_DRAW__MIN(ax, bx), cx);
    int bound_min_y = CPU_DRAW__MIN(CPU_DRAW__MIN(ay, by), cy);
    int bound_max_x = CPU_DRAW__MAX(CPU_DRAW__MAX(ax, bx), cx);
    int bound_max_y = CPU_DRAW__MAX(CPU_DRAW__MAX(ay, by), cy);

    if (bound_max_x < 0 || surface.clip_right <= bound_min_x) return;
    if (bound_max_y < 0 || surface.clip_bottom <= bound_min_y) return;

    float total_area = cpu_draw__signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area == 0.f) return;
    float reciprocal_total_area = 1.f / total_area;

    unsigned colors[3] = { acolor, bcolor, ccolor };
    float channels[3][4];
    for (int i = 0; i < 3; i += 1) {
        channels[i][0] = (float)(colors[i] >> 24);
        channels[i][1] = (float)((colors[i] >> 16) & 0xff);
        channels[i][2] = (float)((colors[i] >> 8) & 0xff);
        channels[i][3] = (float)(colors[i] & 0xff);
    }

    bound_min_x = CPU_DRAW__MAX(0, bound_min_x);
    bound_min_y = CPU_DRAW__MAX(0, bound_min_y);
    bound_max_x = CPU_DRAW__MIN(surface.clip_right, bound_max_x + 1);
    bound_max_y = CPU_DRAW__MIN(surface.clip_bottom, bound_max_y + 1);

    for (int x = bound_min_x; x < bound_max_x; x += 1) {
        for (int y = bound_min_y; y < bound_max_y; y += 1) {
            float alpha = cpu_draw__signed_triangle_area(x, y, bx, by, cx, cy) * reciprocal_total_area;
            float beta = cpu_draw__signed_triangle_area(x, y, cx, cy, ax, ay) * reciprocal_total_area;
            float gamma = cpu_draw__signed_triangle_area(x, y, ax, ay, bx, by) * reciprocal_total_area;

            _Bool outside = alpha < 0 || beta < 0 || gamma < 0;
            if (outside) continue;

            // TODO(felix): is there fast integer-based interpolation we can do?
            unsigned interpolated[4];
            for (int i = 0; i < 4; i += 1) {
                interpolated[i] = (unsigned)(alpha * channels[0][i] + beta * channels[1][i] + gamma * channels[2][i]);
            }
            unsigned color = (interpolated[0] << 24) | (interpolated[1] << 16) | (interpolated[2] << 8) | interpolated[3];

            surface.pixels[y * surface.stride + x] = color;
        }
    }
}


#endif // CPU_DRAW_IMPLEMENTATION
