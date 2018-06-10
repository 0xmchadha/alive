#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>

// stretchy buffers

//buf_push
//buf_len
//buf_cap
//buf_free

struct bufHdr {
        size_t len;
        size_t cap;
        char buf_[0];
};

#define buf__hdr(buf) ((struct bufHdr *) ((char *)buf - offsetof(struct bufHdr, buf_)))
#define buf__fit(buf) ((!(buf) || buf__hdr(buf)->len == buf__hdr(buf)->cap) ? \
                       (buf = buf_grow(buf, sizeof(*buf))) : (void)0)


// macros to be used by clients
#define buf_push(buf, val) (buf__fit(buf), ((buf)[buf__hdr(buf)->len++] = (val)))
#define buf_len(buf) ((buf) ? (buf__hdr(buf)->len) : 0)
#define buf_cap(buf) ((buf) ? buf__hdr(buf)->cap : 0)
#define buf_free(buf) (buf) ? (free(buf__hdr(buf)), (buf) = NULL) : (void)0

void *xmalloc(size_t num_bytes)
{
        void *ptr = malloc(num_bytes);

        if (!ptr) {
                perror("malloc failed\n");
                exit(1);
        }

        return ptr;
}

void *xrealloc(void *ptr, size_t num_bytes)
{
        ptr = realloc(ptr, num_bytes);

        if (!ptr) {
                perror("realloc failed\n");
                exit(1);
        }

        return ptr;
}

void *buf_grow(void *_buf, int elem_size)
{
        struct bufHdr *buf;
        if (!_buf) {
                buf = xmalloc(sizeof(struct bufHdr) + elem_size);
                buf->len = 0;
                buf->cap = 1;
        } else {
                buf = buf__hdr(_buf);
                buf = xrealloc(buf, sizeof(struct bufHdr) + buf->cap * 2 * elem_size);
                buf->cap = buf->cap * 2;
        }

        return (char *) buf + offsetof(struct bufHdr, buf_);
}

void buf_test()
{
        int *buf = NULL;

        enum { N = 1024 };

        assert(buf_len(buf) == 0);
        for (int i = 0; i < N; i++) {
                buf_push(buf, i);
          }

        assert(buf_len(buf) == N);

        for (int i = 0; i < N; i++) {
                assert(buf[i] == i);
        }

        buf_free(buf);
        assert(buf == NULL);
        assert(buf_len(buf) == 0);

        printf("buf test passed\n");
}

struct internStr {
        size_t len;
        char *str;
};

struct internStr *interns = NULL;

const char *str_intern_range(const char *start, const char *end)
{
        size_t len = end - start + 1;
        size_t i;

        for (i = 0; i < buf_len(interns); i++) {
                if (interns[i].len == len && !strncmp(interns[i].str, start, len)) {
                        return interns[i].str;
                }
        }

        char *strp = xmalloc(len);
        strncpy(strp, start, len);
        strp[len] = '\0';

        buf_push(interns, ((struct internStr){len, strp}));

        return interns[i].str;
}

const char *str_intern(const char *str)
{
        return str_intern_range(str, str + strlen(str) - 1);
}

void str_intern_test()
{
        char a[] = "hello";
        char b[] = "hello";
        char c[] = "hell";

        assert(a != b);
        assert(str_intern(&a[0]) == str_intern(&b[0]));
        assert(str_intern_range(&a[0], &a[strlen(a)-1]) != str_intern_range(&c[0], &b[strlen(c)-1]));
        assert(str_intern(&a[0]) == str_intern_range(&a[0], &a[4]));
        printf("string intern test passed\n");
}

typedef enum TokenKind {
        // Reserve the first 128 values for one-char tokens
        TOKEN_INT = 128,
        TOKEN_FLOAT,
        TOKEN_IDENT,
        TOKEN_STR,
        TOKEN_KEYWORD
} tokenKind;

typedef enum TokenMod {
        TOKENMOD_NONE,
        TOKENMOD_CHAR
} tokenMod;

typedef struct {
        tokenKind kind;
        tokenMod mod;
        const char *start;
        const char *end;
        union {
                uint64_t int_val;
                double float_val;
                const char *name;
                const char *str_val;
        };
} token_t;

token_t token;
char *stream;

void fatal(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printf("FATAL: ");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
}

void syntax_error(const char *fmt, ...) {
        va_list args;

        va_start(args, fmt);
        printf("Syntax Error: ");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
}

static int convert_hex(char c)
{
#define hex_to_num(a, b) case a : return b
        switch (c) {
                hex_to_num('0', 0);
                hex_to_num('1', 1);
                hex_to_num('2', 2);
                hex_to_num('3', 3);
                hex_to_num('4', 4);
                hex_to_num('5', 5);
                hex_to_num('6', 6);
                hex_to_num('7', 7);
                hex_to_num('8', 8);
                hex_to_num('9', 9);
                hex_to_num('a', 10);
                hex_to_num('b', 11);
                hex_to_num('c', 12);
                hex_to_num('d', 13);
                hex_to_num('e', 14);
                hex_to_num('f', 15);
                hex_to_num('A', 10);
                hex_to_num('B', 11);
                hex_to_num('C', 12);
                hex_to_num('D', 13);
                hex_to_num('E', 14);
                hex_to_num('F', 15);
        default: 
                return -1;
        }
}

char escape_to_char[256] = {
        ['r'] = '\r',
        ['n'] = '\n',
        ['t'] = '\t',
        ['v'] = '\v',
        ['b'] = '\b',
        ['a'] = '\a',
        ['0'] = '\0',
};

void scan_char()
{
        char val;
        assert(*stream == '\'');
        stream++;

        if (*stream == '\'') {
                syntax_error("char literal can not be empty");
        }

        if (*stream == '\n') {
                syntax_error("can not have new line in a char literal");
        }

        if (*stream == '\\') {
                stream++;
                val = escape_to_char[(int)*stream];
                if (val == 0 && *stream != '0') {
                        syntax_error("Invalid char literal escape '\\%c'", *stream);
                }
        } else {
                val = *stream;
        }

        stream++;
        
        if (*stream != '\'') {
                syntax_error("Expected literal ' but instead got '%c'", *stream);
        } else {
                stream++;
        }

        token.kind = TOKEN_INT;
        token.mod= TOKENMOD_CHAR;
        token.int_val = val;
}

void scan_float()
{
        double val;
        char *digit_stream = stream;
        bool parse_done = false;
top:
        switch (*stream) {
        case '0' : case '1' : case '2' : case '3' : case '4' : case '5' : case '6' : case '7' : case '8' : case '9' :
        {
                if (isdigit(*stream)) stream++;
                break;
        }
        case 'e' : case 'E' : case '.' : case '+' : case '-':
        {
                stream++;
                break;
        }

        default:
                parse_done = true;
        }

        if (!parse_done) goto top;

        val = strtod(digit_stream, NULL);
        if (val == HUGE_VAL || val == -HUGE_VAL) {
                syntax_error("float literal overflow");
        }

        token.kind = TOKEN_FLOAT;
        token.float_val = val;
}

void scan_int()
{
        int base, digit;
        uint64_t val = 0;

        switch (*stream) {
        case '0':
                base = 8;
                stream++;

                if (*stream == 'x') {
                        base = 16;
                        stream++;
                } else if (!isdigit(*stream)) {
                        syntax_error("unrecognized char %c in integer literal", *stream);
                }
                break;
        default:
                base = 10;
        }

        while (*stream && (digit = convert_hex(*stream)) != -1) {
                //               int digit = *stream - '0';
                if (val > (uint64_t)(UINT64_MAX - digit) / base) {
                        syntax_error("Interger literal overflow");

                        while (isdigit(*stream)) {
                                stream++;
                        }
                        val = 0;
                }
                val = val * base + digit;
                stream++;
        }

        token.kind = TOKEN_INT;
        token.int_val = val;
}

void scan_str()
{
        assert(*stream == '"');
        stream++;

        char *str = NULL;
        char val;

        while (*stream && *stream != '"') {
                val = *stream;

                if (*stream == '\n') {
                        syntax_error("String literals can not contain new line characters");
                }

                if (*stream == '\\') {
                        stream++;
                        if (*stream == '"') {
                                val = *stream;
                        } else {
                                val = escape_to_char[(int)*stream];
                                if (val == 0 && *stream != '0') {
                                        syntax_error("invalid string literal escape '\\%c'", *stream);
                                }
                        }
                }

                buf_push(str, val);
                stream++;
        }

        stream++;
        buf_push(str, '\0');
        token.kind = TOKEN_STR;
        token.str_val = str;
}

void next_token() 
{
top:
        token.start = stream;
        token.mod = TOKENMOD_NONE;

        switch (*stream) {
        case ' ' : case '\n' : case '\r' : case '\t' : case '\v':
                while (isspace(*stream)) {
                        stream++;
                }
                goto top;
                break;
        case '\'':
                scan_char();
                break;
        case '"':
                scan_str();
                break;
        case '.':
                scan_float();
                break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        {
                char *digit_stream = stream;

                while(isdigit(*digit_stream)) {digit_stream++;}
                if (*digit_stream == '.' ||
                    tolower(*digit_stream) == 'e') {
                        scan_float();
                } else {
                        scan_int();
                }
                break;
        }
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z': case 'A': case 'B': case 'C': case 'D':
        case 'E': case 'F': case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
        case '_':
        {
                const char *start = stream++;
                const char *end = NULL;

                while (isalnum(*stream) || *stream == '_') {
                        stream++;
                }

                end = stream - 1;

                token.kind = TOKEN_IDENT;
                token.start = start;
                token.end = end;

                token.name = str_intern_range(start, end);
                break;
        }
        default:
                token.kind = *stream++;
        }

        token.end = stream - 1;
}

bool match_token(tokenKind kind)
{
        if (token.kind == kind) {
                next_token();
                return true;
        }

        return false;
}

void print_token()
{
        switch (token.kind) {
        case TOKEN_INT:
                printf("%lu\n", token.int_val);
                break;
        case TOKEN_FLOAT:
                printf("%f\n", token.float_val);
                break;
        case TOKEN_IDENT:
                printf("%.*s\n", (int)(token.end - token.start + 1), token.start);
                break;

        default:
                printf("token kind = %c\n", token.kind);
        }
}

static void init_stream(const char *str)
{
        stream = (char *) str;
        next_token();
}

#define assert_token_int(val) assert(token.int_val == val && match_token(TOKEN_INT))
#define assert_token_float(val) assert(token.float_val == val && match_token(TOKEN_FLOAT))
#define assert_token_char(val) assert(token.int_val == val && token.mod == TOKENMOD_CHAR && match_token(TOKEN_INT))
#define assert_token_str(val) assert(strcmp(token.str_val, val) == 0 && match_token(TOKEN_STR))
#define assert_token_ident(val) assert(token.name == (val) && match_token(TOKEN_IDENT))
#define assert_token_eof() assert(token.kind == '\0')

static void lex_test()
{
//        init_stream("+ 123,HELLO(), abc32343 84384384 0111 0xffffffffffffffff 0xa 1.4 1.4e10 1e10 4 0x5 1e10");
        // identifier test
        init_stream("hello123");
        assert_token_ident(str_intern("hello123"));
        assert_token_eof();

        // integer literal test
        init_stream("123");
        assert_token_int(123);

        // floating point test
        init_stream("0xff 1.2");
        assert_token_int(255);
        assert_token_float(1.2);
        assert_token_eof();

        // char literal test
        init_stream("'\\n' 'a' " );
        assert_token_int('\n');
        assert_token_int('a');
        assert_token_eof();

        // string literal tests
        init_stream("\"foo\" \"a\\n\"");

        assert_token_str("foo");
        assert_token_str("a\n");
        assert_token_eof();

        printf("lex test passed\n");
}

/*******************************************************************************************************************/
/* void lex_test()                                                                                                 */
/* {                                                                                                               */
/*         char *prog = "+ 123,HELLO(), abc32343 84384384 0111 0xffffffffffffffff 0xa 1.4 1.4e10 1e10 4 0x5 1e10"; */
/*         token_t *token_arr = NULL;                                                                              */
/*                                                                                                                 */
/*         stream = prog;                                                                                          */
/*         next_token();                                                                                           */
/*                                                                                                                 */
/*         while (token.kind) {                                                                                    */
/*                 print_token();                                                                                  */
/*                 next_token();                                                                                   */
/*                 buf_push(token_arr, token);                                                                     */
/*         }                                                                                                       */
/* }                                                                                                               */
/*******************************************************************************************************************/

/****************************************************/
/* void init_keywords()                             */
/* {                                                */
/*         const char *if_k = str_intern("if");     */
/*         const char *else_k = str_intern("else"); */
/* }                                                */
/****************************************************/

int main()
{
        buf_test();
        str_intern_test();
        lex_test();
}



