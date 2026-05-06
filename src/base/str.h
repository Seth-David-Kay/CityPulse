// https://github.com/felix-u 2026-04-20
// Public domain. NO WARRANTY - use at your own risk.

#if !defined(STR_H)
#define STR_H


typedef struct {
    const char *data;
    unsigned long long count;
} String;

typedef struct {
    unsigned long long index;
    _Bool ok;
} String_Has;

typedef struct {
    String head, tail;
    _Bool ok;
} String_Cut;

#define STRING_LITERAL_CONSTANT(s) { .data = (s), .count = sizeof(s) - 1 }
#define STRING_LITERAL(s) (String)STRING_LITERAL_CONSTANT(s)

#if !defined(STR_FUNCTION)
    #define STR_FUNCTION
#endif

STR_FUNCTION _Bool              cstring_equals(const char *a, const char *b);
STR_FUNCTION unsigned long long cstring_length(const char *cstring);
STR_FUNCTION String_Cut         string_cut_last(String s, char c);
STR_FUNCTION _Bool              string_equals(String a, String b);
STR_FUNCTION String             string_from_cstring(const char *s);
STR_FUNCTION String             string_from_pointers(const char *start, const char *end);
STR_FUNCTION String_Has         string_has(String s, String substring);
STR_FUNCTION String             string_range(String s, unsigned long long start, unsigned long long end);
STR_FUNCTION _Bool              string_starts_with(String s, String start);


#endif // STR_H


#if defined(STR_IMPLEMENTATION)


#if !defined(STR_ASSERT)
    #include <assert.h>
    #define STR_ASSERT assert
#endif

STR_FUNCTION _Bool cstring_equals(const char *a, const char *b) {
    if (*a == 0 || *b == 0) return 0;
    while (*a != 0 && *a == *b) {
        a += 1;
        b += 1;
    }
    return *a == *b;
}

STR_FUNCTION unsigned long long cstring_length(const char *cstring) {
    unsigned long long result = 0;
    for (const char *c = cstring; c != 0 && *c != 0; c += 1) result += 1;
    return result;
}

STR_FUNCTION String_Has string_has(String s, String substring) {
    String_Has result = {0};

    for (unsigned long long i = 0; i + substring.count <= s.count; i += 1) {
        const char *maybe_substring = &s.data[i];

        _Bool found = 1;
        for (unsigned long long j = 0; j < substring.count; j += 1) {
            if (maybe_substring[j] != substring.data[j]) {
                found = 0;
                break;
            }
        }

        if (found) {
            result.index = i;
            result.ok = 1;
            break;
        }
    }

    return result;
}

STR_FUNCTION String_Cut string_cut_last(String s, char c) {
    String_Cut cut = { .head = s, .tail = s };

    for (unsigned long long i = 0; i < s.count; i += 1) {
        unsigned long long index = s.count - 1 - i;
        if (s.data[index] == c) {
            cut.head = string_range(s, 0, index);
            cut.tail = string_range(s, index + 1, s.count);
            cut.ok = 1;
            break;
        }
    }

    return cut;
}

static _Bool string_equals(String a, String b) {
    if (a.count != b.count) return 0;
    for (unsigned long long i = 0; i < a.count; i += 1) {
        if (a.data[i] != b.data[i]) return 0;
    }
    return 1;
}

static String string_from_cstring(const char *s) {
    String result = { .data = s };
    if (s != 0) while (s[result.count] != 0) result.count += 1;
    return result;
}

STR_FUNCTION String string_from_pointers(const char *start, const char *end) {
    String s = { .data = start, .count = (unsigned long long)(end - start) };
    return s;
}

static String string_range(String s, unsigned long long start, unsigned long long end) {
    STR_ASSERT(end <= s.count);
    STR_ASSERT(start <= end);
    String result = { .data = s.data + start, .count = end - start };
    return result;
}

static _Bool string_starts_with(String s, String start) {
    if (s.count < start.count) return 0;
    for (u64 i = 0; i < start.count; i += 1) if (s.data[i] != start.data[i]) return 0;
    return 1;
}


#endif // STR_IMPLEMENTATION
