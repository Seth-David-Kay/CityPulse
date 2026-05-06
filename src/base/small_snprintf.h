// https://github.com/felix-u 2026-04-06
// Public domain. NO WARRANTY - use at your own risk.

#if !defined(SMALL_SNPRINTF_H)
#define SMALL_SNPRINTF_H


#if !defined(SMALL_SNPRINTF_NO_STD_INCLUDE)
    #include <stddef.h> // size_t
    #include <stdarg.h>
#endif

#if !defined(SMALL_SNPRINTF_FUNCTION)
    #define SMALL_SNPRINTF_FUNCTION
#endif

SMALL_SNPRINTF_FUNCTION int small_snprintf(char *buffer, size_t buffer_size, const char *format, ...);
SMALL_SNPRINTF_FUNCTION int small_vsnprintf(char *buffer, size_t buffer_size, const char *format, va_list arguments);


#endif // SMALL_SNPRINTF_H


#if defined(SMALL_SNPRINTF_IMPLEMENTATION)


#if !defined(SMALL_SNPRINTF_UNKNOWN_FORMAT_HANDLER)
    #define SMALL_SNPRINTF_UNKNOWN_FORMAT_HANDLER(...) (-1)
#endif

#if !defined(SMALL_SNPRINTF_ASSERT)
    #if defined(SMALL_SNPRINTF_NO_STD_INCLUDE)
        #error "provide a custom definition for SMALL_SNPRINTF_ASSERT, since you defined SMALL_SNPRINTF_NO_STD_INCLUDE"
    #else
        #include <assert.h>
        #define SMALL_SNPRINTF_ASSERT assert
    #endif
#endif

SMALL_SNPRINTF_FUNCTION int small_snprintf(char *buffer, size_t buffer_size, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int result = small_vsnprintf(buffer, buffer_size, format, arguments);
    va_end(arguments);
    return result;
}

static void small_snprintf__write(char *buffer, size_t buffer_size, int *written, const void *data, size_t size) {
    for (size_t i = 0; (size_t)(*written) + i + 1 < buffer_size && i < size; i += 1) {
        buffer[*written + (int)i] = ((const char *)data)[i];
    }
    *written += (int)size;
}

static void small_snprintf__write_u64(char *buffer, size_t buffer_size, int *written, unsigned long long value, unsigned long long base, int padding, char pad_char, _Bool uppercase) {
    if (value == 0) {
        for (int j = 0; j < padding - 1; j += 1) {
            small_snprintf__write(buffer, buffer_size, written, &pad_char, 1);
        }
        small_snprintf__write(buffer, buffer_size, written, "0", 1);
    } else {
        char printed[sizeof(unsigned long long) * 8];
        size_t print_index = sizeof printed;

        const char *character_from_digit = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

        for (; value > 0; value /= base) {
            print_index -= 1;
            unsigned char digit = (unsigned char)(value % base);
            printed[print_index] = character_from_digit[digit];
        }

        size_t chars = (sizeof printed) - print_index;
        int to_pad = padding - (int)chars;
        for (int j = 0; j < to_pad; j += 1) {
            small_snprintf__write(buffer, buffer_size, written, &pad_char, 1);
        }

        small_snprintf__write(buffer, buffer_size, written, &printed[print_index], chars);
    }
}

SMALL_SNPRINTF_FUNCTION int small_vsnprintf(char *buffer, size_t buffer_size, const char *format, va_list arguments) {
    int written = 0;

    for (size_t i = 0; format[i] != 0; i += 1) {
        size_t start_index = i;
        while (format[i] != 0 && format[i] != '%') i += 1;

        _Bool non_format_chars = i > start_index;
        if (non_format_chars) {
            small_snprintf__write(buffer, buffer_size, &written, &format[start_index], i - start_index);
        }
        if (format[i] == 0) break;

        i += 1;
        int byte_length = 4;
        char type = 0;

        int padding = 0;
        char pad_char = ' ';

        int precision = 0;
        _Bool is_custom_precision = 0;

        int *parsing = &padding;

        parse_type: {
            type = format[i];

            if ('1' <= type && type <= '9') {
                *parsing *= 10;
                *parsing += (int)(type - '0');
                i += 1;
                goto parse_type;
            }

            switch (type) {
                case 'l': {
                    if (format[i + 1] == 'l') {
                        byte_length = sizeof(long long);
                        i += 1;
                    } else byte_length = sizeof(long);

                    i += 1;
                    goto parse_type;
                } break;
                case 'z': {
                    byte_length = sizeof(size_t);
                    i += 1;
                    goto parse_type;
                } break;
                case '0': {
                    if (parsing == &padding) pad_char = '0';
                    i += 1;
                    goto parse_type;
                } break;
                case '.': {
                    parsing = &precision;
                    is_custom_precision = 1;
                    i += 1;
                    goto parse_type;
                } break;
                case 'u': case 'd': case 'i': case 'x': case 'X': case 'o': case 'b':
                case 'c':
                case 's':
                case 'f': case 'F':
                case '%':
                    break; // everything is handled below - these just need to not error
                default: {
                    int written_by_handler = SMALL_SNPRINTF_UNKNOWN_FORMAT_HANDLER(&buffer[written], buffer_size - (size_t)written, format, &i, &arguments);

                    if (written_by_handler < 0) {
                        small_snprintf__write(buffer, buffer_size, &written, &format[i - 1], 2);
                    }
                    written += written_by_handler;
                    continue;
                } break;
            }
        }

        precision += 6 * (int)!is_custom_precision;

        size_t value = 0;
        _Bool unsigned_int_value_already_populated = 0;
        unsigned long long base = 10;
        _Bool is_uppercase = 0;

        switch (type) {
            case 'X': is_uppercase = 1; // fallthrough
            case 'x': base = 16; goto unsigned_int;
            case 'o': base = 8; goto unsigned_int;
            case 'b': base = 2; goto unsigned_int;
            case 'd': case 'i': {
                long long signed_value = 0;
                if (byte_length == 4) signed_value = va_arg(arguments, int);
                else {
                    SMALL_SNPRINTF_ASSERT(byte_length == 8);
                    signed_value = va_arg(arguments, long long);
                }

                if (signed_value < 0) {
                    small_snprintf__write(buffer, buffer_size, &written, "-", 1);
                    signed_value = -signed_value;
                }

                value = (size_t)signed_value;
                unsigned_int_value_already_populated = 1;
            } // fallthrough
            case 'u': { unsigned_int:
                if (!unsigned_int_value_already_populated) {
                    if (byte_length == 4) value = va_arg(arguments, unsigned);
                    else {
                        SMALL_SNPRINTF_ASSERT(byte_length == 8);
                        value = va_arg(arguments, unsigned long long);
                    }
                }

                small_snprintf__write_u64(buffer, buffer_size, &written, value, base, padding, pad_char, is_uppercase);
            } break;
            case 'c': {
                unsigned c = va_arg(arguments, unsigned);
                char as_char = (char)c;
                small_snprintf__write(buffer, buffer_size, &written, &as_char, 1);
            } break;
            case 's': {
                const char *cstring = va_arg(arguments, const char *);
                if (cstring == 0) {
                    small_snprintf__write(buffer, buffer_size, &written, "(null)", 6);
                } else {
                    size_t j = 0;
                    while ((size_t)written + j + 1 < buffer_size && cstring[j] != 0) {
                        buffer[written + (int)j] = cstring[j];
                        j += 1;
                    }
                    while (cstring[j] != 0) j += 1;
                    written += (int)j;
                }
            } break;
            case 'F': is_uppercase = 1; // fallthrough
            case 'f': {
                double floating = va_arg(arguments, double);
                unsigned long long bits = *(unsigned long long *)&floating;

                long long biased_exponent = (long long)((bits >> 52) & 0x7ff);
                unsigned long long mantissa_mask = (unsigned long long)(-1) >> 12;
                unsigned long long mantissa = bits & mantissa_mask;

                unsigned long long is_negative = bits >> 63;
                if (is_negative) small_snprintf__write(buffer, buffer_size, &written, "-", 1);

                const char *inf = is_uppercase ? "INF" : "inf";
                const char *nan = is_uppercase ? "NAN" : "nan";

                if (biased_exponent == 0x7ff) {
                    const char *string = (mantissa == 0) ? inf : nan;
                    small_snprintf__write(buffer, buffer_size, &written, string, 3);
                    break;
                }

                if (biased_exponent == 0 && mantissa == 0) {
                    small_snprintf__write(buffer, buffer_size, &written, "0", 1);
                    break;
                }

                // NOTE(felix): this is the part responsible for only being able to represent integer parts up to UINT64T_MAX
                double absolute_value = is_negative ? -floating : floating;
                if (absolute_value > (double)(unsigned long long)(-1)) {
                    small_snprintf__write(buffer, buffer_size, &written, "(...)", 5);
                    break;
                }

                small_snprintf__write_u64(buffer, buffer_size, &written, (unsigned long long)absolute_value, 10, padding, pad_char, is_uppercase);

                if (precision > 0) {
                    // TODO(felix): rounding

                    small_snprintf__write(buffer, buffer_size, &written, ".", 1);

                    double fraction = absolute_value;

                    for (int j = 0; j < precision; j += 1) {
                        fraction -= (double)(unsigned long long)fraction;
                        fraction *= 10.0;
                        char fraction_as_char = (char)fraction + '0';
                        small_snprintf__write(buffer, buffer_size, &written, &fraction_as_char, 1);
                    }
                }
            } break;
            case '%': {
                small_snprintf__write(buffer, buffer_size, &written, "%", 1);
            } break;
            default: SMALL_SNPRINTF_ASSERT(0 && "unreachable"); break;
        }
    }

    if ((size_t)written < buffer_size) buffer[written] = 0;
    else if (buffer_size > 0) buffer[buffer_size - 1] = 0;

    return written;
}


#endif // SMALL_SNPRINTF_IMPLEMENTATION
