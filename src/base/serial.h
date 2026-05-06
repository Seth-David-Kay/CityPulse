// https://github.com/felix-u 2026-04-21
// Public domain. NO WARRANTY - use at your own risk.

#if !defined(SERIAL_H)
#define SERIAL_H

typedef struct {
    const char *data;
    int data_size;
    int cursor;
    int depth;
} serial_Reader;

typedef char serial_Type;
enum {
    serial_Type_ERROR  = '!',
    serial_Type_END    = '}',
    serial_Type_OBJECT = '{',
    serial_Type_ARRAY  = '[',
    serial_Type_F64    = 'f',
    serial_Type_BOOL   = 'b',
    serial_Type_STRING = '"',
};

typedef struct {
    serial_Type type;
    union {
        struct { const char *string; int string_length; };
        double f64;
        _Bool bool;
    };
} serial_Value;

typedef struct {
    char *data;
    int data_size;
    int written;
} serial_Writer;

#if !defined(SERIAL_FUNCTION)
    #define SERIAL_FUNCTION
#endif

SERIAL_FUNCTION serial_Value  serial_read(serial_Reader *r);
SERIAL_FUNCTION serial_Reader serial_reader(const char *data, int data_size);
SERIAL_FUNCTION void          serial_write(serial_Writer *w, serial_Value v);
SERIAL_FUNCTION void          serial_write_cstring(serial_Writer *w, const char *cstring);
SERIAL_FUNCTION void          serial_write_f64(serial_Writer *w, double f64);
SERIAL_FUNCTION void          serial_write_tag(serial_Writer *w, serial_Type tag);
SERIAL_FUNCTION serial_Writer serial_writer(char *data, int data_size);


#endif // SERIAL_H


#if defined(SERIAL_IMPLEMENTATION)

#if !defined(SERIAL_ASSERT)
    #include <assert.h>
    #define SERIAL_ASSERT assert
#endif

static inline void serial__copy(void *destination, const void *source, int bytes) {
    for (int i = 0; i < bytes; i += 1) ((char *)destination)[i] = ((const char *)source)[i];
}

SERIAL_FUNCTION serial_Value serial_read(serial_Reader *r) {
    serial_Value v = {0};
    _Bool ok = 0;

    if (r->cursor < r->data_size) {
        v.type = (serial_Type)r->data[r->cursor++];
        switch (v.type) {
            case serial_Type_ERROR: break;
            case serial_Type_END: {
                r->depth -= 1;
                ok = r->depth >= 0;
            } break;
            case serial_Type_OBJECT: case serial_Type_ARRAY: {
                r->depth += 1;
                ok = 1;
            } break;
            case serial_Type_F64: {
                ok = r->cursor + (int)(sizeof v.f64) < r->data_size;
                if (ok) {
                    serial__copy(&v.f64, &r->data[r->cursor], sizeof v.f64);
                    r->cursor += sizeof v.f64;
                }
            } break;
            case serial_Type_BOOL: {
                ok = r->cursor + (int)(sizeof v.bool) < r->data_size;
                if (ok) {
                    serial__copy(&v.bool, &r->data[r->cursor], sizeof v.bool);
                    r->cursor += sizeof v.bool;
                }
            } break;
            case serial_Type_STRING: {
                ok = r->cursor + (int)(sizeof v.string_length) < r->data_size;
                if (ok) {
                    serial__copy(&v.string_length, &r->data[r->cursor], sizeof v.string_length);
                    r->cursor += sizeof v.string_length;

                    ok = r->cursor + v.string_length + 1 < r->data_size;
                    if (ok) {
                        v.string = &r->data[r->cursor];
                        r->cursor += v.string_length + 1;
                    }
                }
            } break;
            default: break;
        }
    }

    if (!ok) v.type = serial_Type_ERROR;
    return v;
}

SERIAL_FUNCTION serial_Reader serial_reader(const char *data, int data_size) {
    serial_Reader reader = {
        .data = data,
        .data_size = data_size,
    };
    return reader;
}

static void serial__write(serial_Writer *w, const void *data, int size) {
    for (int i = 0; w->written + i + 1 < w->data_size && i < size; i += 1) {
        w->data[w->written + i] = ((const char *)data)[i];
    }
    w->written += size;
}

SERIAL_FUNCTION void serial_write(serial_Writer *w, serial_Value v) {
    serial__write(w, &v.type, sizeof v.type);
    switch (v.type) {
        case serial_Type_F64: {
            serial__write(w, &v.f64, sizeof v.f64);
        } break;
        case serial_Type_BOOL: {
            serial__write(w, &v.bool, sizeof v.bool);
        } break;
        case serial_Type_STRING: {
            serial__write(w, &v.string_length, sizeof v.string_length);
            serial__write(w, v.string, v.string_length);
            serial__write(w, "", 1);
        } break;
        case serial_Type_OBJECT: case serial_Type_ARRAY: case serial_Type_END: break;
        default: SERIAL_ASSERT(0 && "unreachable");
    }
}

SERIAL_FUNCTION void serial_write_cstring(serial_Writer *w, const char *cstring) {
    serial_Value v = {
        .type = serial_Type_STRING,
        .string = cstring,
    };
    while (cstring[v.string_length] != 0) v.string_length += 1;
    serial_write(w, v);
}

SERIAL_FUNCTION void serial_write_f64(serial_Writer *w, double f64) {
    serial_Value v = { .type = serial_Type_F64, .f64 = f64 };
    serial_write(w, v);
}

SERIAL_FUNCTION void serial_write_tag(serial_Writer *w, serial_Type tag) {
    serial_Value v = { .type = tag };
    serial_write(w, v);
}

SERIAL_FUNCTION serial_Writer serial_writer(char *data, int data_size) {
    serial_Writer writer = {
        .data = data,
        .data_size = data_size,
    };
    return writer;
}


#endif // SERIAL_IMPLEMENTATION
