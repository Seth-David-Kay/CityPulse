// https://github.com/felix-u 2026-03-06
// Public domain. NO WARRANTY - use at your own risk.

// References:
// https://en.wikipedia.org/wiki/Glyph_Bitmap_Distribution_Format


#if !defined(BDF_H)
#define BDF_H


#if !defined(BDF_FUNCTION)
    #define BDF_FUNCTION
#endif

#if !defined(BDF_GLYPH_MAX_HEIGHT)
    #define BDF_GLYPH_MAX_HEIGHT 16
#endif

#if !defined(BDF_GLYPH_MAX_WIDTH_OVER_8)
    #define BDF_GLYPH_MAX_WIDTH_OVER_8 2
#endif

typedef struct {
    signed char width, height;
    signed char left, bottom;
} bdf_Bounding_Box;

typedef struct {
    signed char width;
    bdf_Bounding_Box bound;
    unsigned char bytes[BDF_GLYPH_MAX_HEIGHT * BDF_GLYPH_MAX_WIDTH_OVER_8];
} bdf_Glyph;

typedef struct {
    int size;
    bdf_Bounding_Box bound;
    int ascent, descent;
    int number_of_glyphs;

    bdf_Glyph glyphs[96];
} bdf_Font;

BDF_FUNCTION bdf_Glyph *bdf_glyph(const bdf_Font *font, char character);
BDF_FUNCTION _Bool      bdf_parse(bdf_Font *font, const void *bdf, unsigned long long bdf_size);
BDF_FUNCTION _Bool      bdf_pixel(const bdf_Glyph *glyph, int x, int y);


#endif // BDF_H


#if defined(BDF_IMPLEMENTATION)


#if !defined(BDF_ASSERT)
    #include <assert.h>
    #define BDF_ASSERT assert
#endif

static void bdf__skip_spaces(const char **cursor, const char *end) {
    while (*cursor < end && **cursor == ' ') *cursor += 1;
}

static _Bool bdf__parse_and_skip_decimal(const char **cursor, const char *end, int *out) {
    *out = 0;
    if (*cursor >= end) return 0;

    int sign = 1;
    if (**cursor == '-') {
        sign = -1;
        *cursor += 1;
    }

    if (!('0' <= **cursor && **cursor <= '9')) return 0;

    for (; *cursor < end; *cursor += 1) {
        if (!('0' <= **cursor && **cursor <= '9')) break;

        *out *= 10;
        *out += **cursor - '0';
    }

    *out *= sign;
    bdf__skip_spaces(cursor, end);
    return 1;
}

static _Bool bdf__parse_and_skip_decimal_8bit(const char **cursor, const char *end, signed char *out) {
    int i32_out = 0;
    _Bool result = bdf__parse_and_skip_decimal(cursor, end, &i32_out);
    BDF_ASSERT(-128 <= i32_out && i32_out <= 127);
    *out = (signed char)i32_out;
    return result;
}

static _Bool bdf__skip(const char **cursor, const char *end, const char *word) {
    top:

    while (**cursor != *word) {
        if (*cursor >= end) return 0;
        *cursor += 1;
    }

    *cursor += 1;
    for (const char *compare = &word[1];;) {
        if (*cursor >= end) return 0;
        if (*compare == 0) break;
        if (**cursor != *compare) goto top;

        *cursor += 1;
        compare += 1;
    }

    bdf__skip_spaces(cursor, end);
    return 1;
}

BDF_FUNCTION bdf_Glyph *bdf_glyph(const bdf_Font *font, char character) {
    BDF_ASSERT(32 <= character && character <= 127);
    bdf_Glyph *glyph = (bdf_Glyph *)&font->glyphs[character - 32];
    return glyph;
}

BDF_FUNCTION _Bool bdf_parse(bdf_Font *font, const void *bdf, unsigned long long bdf_size) {
    const char *bdf_end = (const char *)bdf + bdf_size;
    const char *cursor = bdf;

    if (!bdf__skip(&cursor, bdf_end, "SIZE")) return 0;
    if (!bdf__parse_and_skip_decimal(&cursor, bdf_end, &font->size)) return 0;

    if (!bdf__skip(&cursor, bdf_end, "FONTBOUNDINGBOX")) return 0;
    if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &font->bound.width)) return 0;
    if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &font->bound.height)) return 0;
    if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &font->bound.left)) return 0;
    if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &font->bound.bottom)) return 0;

    if (!bdf__skip(&cursor, bdf_end, "FONT_ASCENT")) return 0;
    if (!bdf__parse_and_skip_decimal(&cursor, bdf_end, &font->ascent)) return 0;

    if (!bdf__skip(&cursor, bdf_end, "FONT_DESCENT")) return 0;
    if (!bdf__parse_and_skip_decimal(&cursor, bdf_end, &font->descent)) return 0;

    if (!bdf__skip(&cursor, bdf_end, "CHARS ")) return 0;
    if (!bdf__parse_and_skip_decimal(&cursor, bdf_end, &font->number_of_glyphs)) return 0;

    while (bdf__skip(&cursor, bdf_end, "ENCODING")) {
        int codepoint = 0;
        if (!bdf__parse_and_skip_decimal(&cursor, bdf_end, &codepoint)) return 0;
        if (codepoint < 32 || codepoint > 127) continue;

        bdf_Glyph *glyph = &font->glyphs[codepoint - 32];

        if (!bdf__skip(&cursor, bdf_end, "DWIDTH")) return 0;
        if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &glyph->width)) return 0;

        if (!bdf__skip(&cursor, bdf_end, "BBX")) return 0;
        if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &glyph->bound.width)) return 0;
        if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &glyph->bound.height)) return 0;
        BDF_ASSERT(glyph->bound.height <= BDF_GLYPH_MAX_HEIGHT);
        if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &glyph->bound.left)) return 0;
        if (!bdf__parse_and_skip_decimal_8bit(&cursor, bdf_end, &glyph->bound.bottom)) return 0;

        if (!bdf__skip(&cursor, bdf_end, "BITMAP")) return 0;
        if (!bdf__skip(&cursor, bdf_end, "\n")) return 0;

        int hex_bytes_per_row = (glyph->bound.width + 7) / 8;
        BDF_ASSERT(hex_bytes_per_row <= BDF_GLYPH_MAX_WIDTH_OVER_8);
        if (cursor + (2 * hex_bytes_per_row + 1) * glyph->bound.height >= bdf_end) return 0;
        int hex_digits_per_row = hex_bytes_per_row * 2;

        for (int row = 0; row < glyph->bound.height; row += 1) {
            for (int hex_digit_index = 0; hex_digit_index + 1 < hex_digits_per_row; hex_digit_index += 2) {
                unsigned char digits[2] = { (unsigned char)cursor[hex_digit_index], (unsigned char)cursor[hex_digit_index + 1] };
                for (int i = 0; i < 2; i += 1) {
                    unsigned char uppercase = digits[i] & ~0x20;
                    if ('0' <= digits[i] && digits[i] <= '9') digits[i] -= '0';
                    else if ('A' <= uppercase && uppercase <= 'F') digits[i] = uppercase - 'A' + 0xA;
                    else BDF_ASSERT(hex_digit_index + 1 == hex_digits_per_row);
                }

                unsigned char value = digits[0] * 0x10 + digits[1];
                glyph->bytes[row * BDF_GLYPH_MAX_WIDTH_OVER_8 + hex_digit_index / 2] = value;
            }

            while (*cursor != '\n') cursor += 1;
            cursor += 1;
        }
    }

    return 1;
}

BDF_FUNCTION _Bool bdf_pixel(const bdf_Glyph *glyph, int x, int y) {
    unsigned char bits = glyph->bytes[y * BDF_GLYPH_MAX_WIDTH_OVER_8 + x / 8];
    _Bool bit = (_Bool)((bits >> (7 - (x & 7))) & 1);
    return bit;
}


#endif // BDF_IMPLEMENTATION
