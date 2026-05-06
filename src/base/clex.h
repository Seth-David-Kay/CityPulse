// https://github.com/felix-u 2026-03-07
// Public domain. NO WARRANTY - use at your own risk

#if !defined(CLEX_H)
#define CLEX_H


typedef struct { const char *start, *end; } clex_Data;

typedef enum {
    clex_Token_ERROR = 0,
    clex_Token_PREPROCESSOR_DIRECTIVE,
    clex_Token_COMMENT,
    clex_Token_IDENTIFIER,
    clex_Token_INTEGER,
    clex_Token_FLOAT,
    clex_Token_STRING,

    clex_Token__ASCII_BEGIN = ' ' - 1,
    clex_Token_NOT                  = '!',
    clex_Token_MODULO               = '%',
    clex_Token_AMPERSAND            = '&',
    clex_Token_OPEN_PAREN           = '(',
    clex_Token_CLOSE_PAREN          = ')',
    clex_Token_ASTERISK             = '*',
    clex_Token_PLUS                 = '+',
    clex_Token_COMMA                = ',',
    clex_Token_MINUS                = '-',
    clex_Token_DOT                  = '.',
    clex_Token_DIVIDE               = '/',
    clex_Token_COLON                = ':',
    clex_Token_SEMICOLON            = ';',
    clex_Token_LESS_THAN            = '<',
    clex_Token_EQUALS               = '=',
    clex_Token_GREATER_THAN         = '>',
    clex_Token_QUESTION             = '?',
    clex_Token_OPEN_SQUARE_BRACKET  = '[',
    clex_Token_CLOSE_SQUARE_BRACKET = ']',
    clex_Token_CARET                = '^',
    clex_Token_OPEN_CURLY           = '{',
    clex_Token_PIPE                 = '|',
    clex_Token_CLOSE_CURLY          = '}',
    clex_Token_BIT_NOT              = '~',
    clex_Token__ASCII_END = 128,

    clex_Token__DOUBLE_OPERATOR,
    clex_Token_AND            = '&' + clex_Token__DOUBLE_OPERATOR,
    clex_Token_PLUS_PLUS      = '+' + clex_Token__DOUBLE_OPERATOR,
    clex_Token_MINUS_MINUS    = '-' + clex_Token__DOUBLE_OPERATOR,
    clex_Token_LEFT_SHIFT     = '<' + clex_Token__DOUBLE_OPERATOR,
    clex_Token_EQUALITY_CHECK = '=' + clex_Token__DOUBLE_OPERATOR,
    clex_Token_RIGHT_SHIFT    = '>' + clex_Token__DOUBLE_OPERATOR,
    clex_Token_OR             = '|' + clex_Token__DOUBLE_OPERATOR,
    clex_Token_Kind__DOUBLE_OPERATOR_END = clex_Token__DOUBLE_OPERATOR + 128,

    // TODO(felix): ASCII order
    clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_NOT_EQUALS            = '!' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_MODULO_ASSIGN         = '%' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_BIT_AND_ASSIGN        = '&' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_MULTIPLY_ASSIGN       = '*' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_PLUS_ASSIGN           = '+' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_MINUS_ASSIGN          = '-' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_DIVIDE_ASSIGN         = '/' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_LESS_THAN_OR_EQUAL    = '<' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_GREATER_THAN_OR_EQUAL = '>' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_XOR_ASSIGN            = '^' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_BIT_OR_ASSIGN         = '|' + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token__ASSIGNMENT_OPERATOR_END = clex_Token__DOUBLE_OPERATOR + 128,

    clex_Token__ASSIGNMENT_DOUBLE_OPERATOR = clex_Token__DOUBLE_OPERATOR + clex_Token__ASSIGNMENT_OPERATOR,
    clex_Token_LEFT_SHIFT_ASSIGN  = '<' + clex_Token__ASSIGNMENT_DOUBLE_OPERATOR,
    clex_Token_RIGHT_SHIFT_ASSIGN = '>' + clex_Token__ASSIGNMENT_DOUBLE_OPERATOR,
    clex_Token__ASSIGNMENT_DOUBLE_OPERATOR_END,

    clex_Token_END_OK,
} clex_Token_Kind;

typedef enum {
    clex_Suffix_FLOAT = 1 << 0,

    clex_Suffix_UNSIGNED = 1 << 1,
    clex_Suffix_LONG = 1 << 2,
    clex_Suffix_LONG_LONG = 1 << 3,
    clex_Suffix_BIT_PRECISE_INT = 1 << 4,
} clex_Suffixes;

typedef struct {
    clex_Token_Kind kind;
    clex_Data data, data2;
    clex_Suffixes suffixes;
} clex_Token;

typedef enum {
    clex_PARSE_COMMENTS = 1 << 0,
} clex_Flags;

typedef struct {
    const char *start, *c, *end;
    clex_Flags flags;
    clex_Token token;
} clex_Lexer;

#if !defined(CLEX_FUNCTION)
    #define CLEX_FUNCTION
#endif

CLEX_FUNCTION clex_Lexer clex_init(const char *start, const char *end, clex_Flags flags);
CLEX_FUNCTION      _Bool clex_lex(clex_Lexer *l);


#endif // CLEX_H


#if defined(CLEX_IMPLEMENTATION)


#if !defined(CLEX_ASSERT)
    #include <assert.h>
    #define CLEX_ASSERT assert
#endif

static const char *clex__cstring_end(const char *s) {
    const char *end = s;
    while (*end != 0) end += 1;
    return end;
}

CLEX_FUNCTION clex_Lexer clex_init(const char *start, const char *end, clex_Flags flags) {
    clex_Lexer p = { .start = start, .c = start, .end = end, .flags = flags };
    return p;
}

static _Bool clex__is_digit(char c) { return '0' <= c && c <= '9'; }

static _Bool clex__is_newline(clex_Lexer *l, const char *c) {
    if (*c == '\n') return 1;
    if (*c == '\r' && c + 1 < l->end && c[1] == '\n') return 1;
    return 0;
}

static _Bool clex__is_nondigit(char c) {
    _Bool is = c == '_';
    is = is || ('A' <= c && c <= 'Z');
    is = is || ('a' <= c && c <= 'z');
    return is;
}

static void clex__skip_identifier(clex_Lexer *l) {
    // identifier:
    //      identifier-start
    //      identifier identifier-continue
    // identifier-start:
    //      nondigit
    //      XID_Start character
    // identifier-continue:
    //      digit
    //      nondigit
    //      XID_Continue character
    // XID_Start, XID_continue:
    //      implementation-defined, something to do with unicode? ignore for now

    if (l->c >= l->end || !clex__is_nondigit(*l->c)) return;
    l->c += 1;
    for (; l->c < l->end; l->c += 1) {
        if (!clex__is_nondigit(*l->c) && !clex__is_digit(*l->c)) break;
    }
}

static void clex__skip_to_newline(clex_Lexer *l) {
    for (; l->c < l->end; l->c += 1) {
        if (clex__is_newline(l, l->c)) break;
    }
}

static void clex__skip_whitespace_except_newline(clex_Lexer *l) {
    for (; l->c < l->end; l->c += 1) {
        char c = *l->c;
        if (c != '\t' && c != '\r' && c != ' ') break;
    }
}

static void clex__skip_whitespace(clex_Lexer *l) {
    for (; l->c < l->end; l->c += 1) {
        char c = *l->c;
        if (c != '\n' && c != '\t' && c != '\r' && c != ' ') break;
    }
}

static _Bool clex__string_equals(const char *start, const char *end, const char *compare) {
    for (const char *c = start; c < end; c += 1) {
        unsigned long long i = (unsigned long long)(c - start);
        if (*c != compare[i]) return 0;
    }
    return 1;
}

#define CLEX__TOKEN_STACK_COUNT 2
typedef struct {
    clex_Token stack[CLEX__TOKEN_STACK_COUNT];
    unsigned count;
} clex__Token_Stack;
static _Bool clex__pop_token(clex__Token_Stack *tokens, clex_Token *popped) {
    if (tokens->count == 0) return 0;
    *popped = tokens->stack[--tokens->count];
    return 1;
}
static void clex__push_token(clex__Token_Stack *tokens, clex_Token push) {
    CLEX_ASSERT(tokens->count < CLEX__TOKEN_STACK_COUNT);
    tokens->stack[tokens->count++] = push;
}

CLEX_FUNCTION _Bool clex_lex(clex_Lexer *l) {
    // clex_Token last_token = l->token;
    l->token = (clex_Token){0};

    static clex__Token_Stack tokens = {0};
    if (clex__pop_token(&tokens, &l->token)) return 1;

    next_token:

    clex__skip_whitespace(l);

    CLEX_ASSERT(l->c <= l->end);
    if (l->c == l->end) {
        l->token = (clex_Token){
            .kind = clex_Token_END_OK,
            .data = { .start = l->c, .end = l->c },
        };
        return 0;
    }

    switch (*l->c) {
        case '#': {
            l->c += 1;
            clex__skip_whitespace(l);

            const char *directive = l->c;
            clex__skip_identifier(l);
            const char *directive_end = l->c;
            if (directive_end == directive) goto error;

            const char *definition = directive_end;
            clex__skip_to_newline(l);
            const char *definition_end = l->c;

            l->token = (clex_Token){
                .kind = clex_Token_PREPROCESSOR_DIRECTIVE,
                .data = { .start = directive, .end = directive_end },
                .data2 = { .start = definition, .end = definition_end },
            };
            return 1;
        } break;
        case '(': case ')': case ',': case '.': case ':': case ';': case'[': case ']': case '{': case '}': case '?': {
            l->token = (clex_Token){
                .kind = (clex_Token_Kind)(*l->c),
                .data = { .start = l->c, .end = ++l->c },
            };
            return 1;
        } break;
        case '0': {
            const char *integer = l->c;

            char nil = 0;
            const char *next = l->c + 1 < l->end ? &l->c[1] : &nil;

            if (*next == '.') {
                const char *floating = l->c;

                l->c += 2;
                while (l->c < l->end && ('0' <= *l->c && *l->c <= '9')) l->c += 1;
                if (l->c == l->end || l->c == &next[1]) goto error;

                const char *floating_end = l->c;
                _Bool float_suffix = *l->c == 'f';
                l->c += float_suffix;

                l->token = (clex_Token){
                    .kind = clex_Token_FLOAT,
                    .data = { .start = floating, .end = floating_end },
                    .suffixes = float_suffix * clex_Suffix_FLOAT,
                };
                return 1;
            }

            if (*next == 'b' || *next == 'B') {
                CLEX_ASSERT(0 && "TODO(felix)");
            }

            if (*next == 'x' || *next == 'X') {
                const char *hex = l->c;

                l->c += 2;
                while (l->c < l->end) {
                    char c = *l->c & 0x20;
                    if (!(('0' <= *l->c && *l->c <= '9') || ('a' <= c && c <= 'f'))) break;
                    l->c += 1;
                }

                const char *hex_end = l->c;
                // TODO(felix): suffixes

                l->token = (clex_Token){
                    .kind = clex_Token_INTEGER,
                    .data = { .start = hex, .end = hex_end },
                    // TODO(felix): hex flag
                };
                return 1;
            }

            if ('1' <= *next && *next <= '7') {
                CLEX_ASSERT(0 && "TODO(felix): octal");
            }

            if (*next == '8' || *next == '9') goto error;

            l->token = (clex_Token){
                .kind = clex_Token_INTEGER,
                .data = { .start = integer, .end = ++l->c },
            };
            return 1;
        } break;
        case '!': case '%': case '&': case '*': case '+': case '-': case '/': case '<': case '>': case '^': case '|': // can be immediately followed by `=`
        case '=': case '~':
        {
            _Bool single_line_comment = *l->c == '/' && (&l->c[1] < l->end && l->c[1] == '/');
            if (single_line_comment) {
                l->c += 2;
                const char *comment = l->c;
                const char *comment_end = comment;

                while (comment_end < l->end && !clex__is_newline(l, comment_end)) comment_end += 1;
                if (comment_end == l->end) goto error;

                l->c = comment_end + 1;

                if (!(l->flags & clex_PARSE_COMMENTS)) goto next_token;

                l->token = (clex_Token){
                    .kind = clex_Token_COMMENT,
                    .data = { .start = comment, .end = comment_end },
                };
                return 1;
            }

            const char *operator = l->c;

            _Bool doubled = l->c + 1 < l->end && l->c[1] == *l->c;
            l->c += doubled;

            if (doubled) switch (*operator) {
                case '+': case '-': case '<': case '>': case '=': case '&': case '|': break;
                // these are just repeated operators with no special meaning conveyed by this
                case '!': case '~': case '^': case '*': {
                    clex_Token token = {
                        .kind = (clex_Token_Kind)*operator,
                        .data = { .start = operator + 1, .end = operator + 2 },
                    };
                    clex__push_token(&tokens, token);

                    token.data.start += 1;
                    token.data.end += 1;
                    l->token = token;
                    return 1;
                } break;
                default: goto error;
            }

            _Bool assignment = l->c + 1 < l->end && l->c[1] == '=';
            l->c += assignment;

            // only some operators have `!=` form
            if (assignment) switch (*operator) {
                case '!': case '<': case '>': case '*': case '/': case '%': case '+': case '-': case '&': case '^': case '|': break;
                default: goto error;
            }

            const char *operator_end = ++l->c;

            // only `<<=` and `>>=` have this form
            if (doubled && assignment && *operator != '<' && *operator != '>') goto error;

            clex_Token_Kind kind = (clex_Token_Kind)(*operator + doubled * clex_Token__DOUBLE_OPERATOR + assignment * clex_Token__ASSIGNMENT_OPERATOR);

            l->token = (clex_Token){
                .kind = kind,
                .data = { .start = operator, .end = operator_end },
            };
            return 1;
        } break;
        case '\'': {
            // TODO(felix): handle encoding prefix somewhere, probably in the identifier parsing logic

            if (l->c + 2 >= l->end) goto error;

            l->c += 1;
            const char *character = l->c;

            if (*l->c == '\\') {
                CLEX_ASSERT(0 && "TODO(felix): escape sequences");
            }

            const char *character_end = l->c + 1;
            l->c += 2; // after closing quote

            l->token = (clex_Token){
                .kind = clex_Token_INTEGER,
                .data = { .start = character, .end = character_end },
                // TODO(felix): flag for character literal
            };
            return 1;
        } break;
        case '"': {
            // TODO(felix): handle encoding prefix somewhere, probably in the identifier parsing logic
            if (++l->c >= l->end) goto error;

            const char *string = l->c;

            for (l->c += 1; l->c < l->end; l->c += 1) {
                if (*l->c == '"') break;
                if (*l->c == '\\') {
                    if (l->c >= l->end) goto error;

                    switch (*(++l->c)) {
                        case '"': case '?': case '\\': case 'a': case 'b': case 'f': case 'n': case 'r': case 't': case 'v': break;
                        case '0': CLEX_ASSERT(0 && "TODO(felix): octal digit escape sequence");
                        case 'x': CLEX_ASSERT(0 && "TODO(felix): hex digit escape sequence");
                        default: goto error;
                    }
                }
                if (clex__is_newline(l, l->c)) goto error;
            }
            if (l->c >= l->end) goto error;

            const char *string_end = l->c++;
            l->token = (clex_Token){
                .kind = clex_Token_STRING,
                .data = { .start = string, .end = string_end },
                // TODO(felix): prefix
            };
            return 1;
        } break;
        default: break;
    }

    // primary-expression:
    //      identifier
    //      constant
    //      string-literal
    //      ( expression )
    //      generic-selection
    // constant:
    //      integer-constant
    //      floating-constant
    //      enumeration-constant
    //      character-constant
    //      predefined-constant
    // string-literal:
    //      encoding-prefix[opt] " s-char-sequence[opt] "
    // expression:
    //      assignment-expression
    //      expression , assignment-expression
    // generic-selection:
    //      _Generic ( assignment-expression , generic-assoc-list )
    // assignment-expression:
    //      conditional-expression
    //      unary-expression assignment-operator assignment-expression
    // unary-expression:
    //      postfix-expression
    //      ++ unary-expression
    //      -- unary-expression
    //      unary-operator cast-expression
    //      sizeof unary-expression
    //      sizeof ( type-name )
    //      alignof ( type-name )
    // postfix-expression:
    //      primary-expression
    //      postfix-expression [ expression ]
    //      postfix-expression ( argument-expression-list[opt] )
    //      postfix-expression . identifier
    //      postfix-expression -> identifier
    //      postfix-expression ++
    //      postfix-expression --
    //      compound-literal
    // unary-operator: one of
    //      & * + - ~ !
    // assignment-operator: one of
    //      = *= /= %= += -= <<= >>= &= ^= |=

    const char *identifier = l->c;
    clex__skip_identifier(l);
    const char *identifier_end = l->c;
    _Bool is_identifier = identifier_end > identifier;
    if (is_identifier) {
        l->token = (clex_Token){
            .kind = clex_Token_IDENTIFIER,
            .data = { .start = identifier, .end = identifier_end },
        };
        return 1;
    }

    // constant:
    //      integer-constant: before integer-suffix[opt]
    //          decimal-constant:
    //              nonzero-digit
    //              decimal-constant ,[opt] digit
    //          octal-constant:
    //              0
    //              octal-constant ,[opt] octal-digit
    //          hexadecimal-constant:
    //              hexadecimal-prefix hexadecimal-digit-sequence
    //          binary-constant
    //              binary-prefix binary-digit
    //              binary-constant ,[opt] binary-digit
    //      floating-constant:
    //          decimal-floating-constant: before floating-suffix[opt]
    //              fractional-constant exponent-part[opt]
    //              digit-sequence exponent-part
    //          hexadecimal-floating-constant: before floating-suffix[opt]
    //              hexadecimal-prefix hexadecimal-fractional-constant binary-exponent-part
    //              hexadecimal-prefix hexadecimal-digit-sequence binary-exponent-part
    //      enumeration-constant = identifier
    //      character-constant:
    //          encoding-prefix[opt] ' c-char-sequence '

    if ('1' <= *l->c && *l->c <= '9') {
        const char *number = l->c;
        while (l->c < l->end && '0' <= *l->c && *l->c <= '9') l->c += 1;

        _Bool floating = l->c < l->end && *l->c == '.';
        l->c += floating;
        if (floating) while (l->c < l->end && '0' <= *l->c && *l->c <= '9') l->c += 1;
        const char *number_end = l->c;

        _Bool unsigned_suffix = 0, long_suffix = 0, long_long_suffix = 0, bit_suffix = 0, floating_suffix = 0;
        for (;;) {
            const char *old_c = l->c;

            floating_suffix = l->c < l->end && *l->c == 'f';
            l->c += floating_suffix;

            unsigned_suffix = l->c < l->end && (*l->c | 0x20) == 'u';
            l->c += unsigned_suffix;

            long_suffix = l->c < l->end && (*l->c | 0x20) == 'l';
            long_long_suffix = long_suffix && l->c + 1 < l->end && l->c[1] == *l->c;
            long_suffix = long_suffix && !long_long_suffix;
            l->c += long_suffix;
            l->c += 2 * long_long_suffix;

            bit_suffix = 0;
            if ((*l->c | 0x20) == 'w' && l->c + 1 < l->end) {
                char next = 'B' | (*l->c & 0x20);
                bit_suffix = l->c[1] == next;
            }
            l->c += 2 * bit_suffix;

            _Bool no_more_suffixes = l->c == old_c;
            if (no_more_suffixes) break;
        }

        clex_Suffixes suffixes = 0;
        suffixes |= floating_suffix * clex_Suffix_FLOAT;
        suffixes |= unsigned_suffix * clex_Suffix_UNSIGNED;
        suffixes |= long_suffix * clex_Suffix_LONG;
        suffixes |= long_long_suffix * clex_Suffix_LONG_LONG;
        suffixes |= bit_suffix * clex_Suffix_BIT_PRECISE_INT;

        l->token = (clex_Token){
            .kind = floating ? clex_Token_FLOAT : clex_Token_INTEGER,
            .data = { .start = number, .end = number_end },
            .suffixes = suffixes,
        };
        return 1;
    }

    CLEX_ASSERT(0 && "TODO(felix)");

    error:
    l->token = (clex_Token){0};
    return 0;
}


#endif // CLEX_IMPLEMENTATION
