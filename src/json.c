#include <stddef.h>	/* size_t offsetof */
#include <stdarg.h>	/* va_list va_start va_end va_arg */
#include <stdlib.h>	/* malloc(3) realloc(3) strtod(3) */
#include <stdio.h>	/* snprintf(3) */

#include <string.h>	/* memset(3) */

#include <ctype.h>	/* isdigit(3) isgraph(3) */

#include <math.h>	/* HUGE_VAL modf(3) */

#include <errno.h>	/* errno ERANGE EOVERFLOW EINVAL */

#include <sys/queue.h>

#include "llrb.h"


/*
 * M I S C E L L A N E O U S  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define PASTE(x, y) x ## y
#define XPASTE(x, y) PASTE(x, y)


static void *make(size_t size, int *error) {
	void *p;

	if (!(p = malloc(size)))
		*error = errno;

	return p;
} /* make() */


/*
 * L E X E R  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct string {
	size_t length;
	char text[];
}; /* struct string */


struct token {
	enum tokens {
		T_BEGIN_ARRAY,
		T_END_ARRAY,
		T_BEGIN_OBJECT,
		T_END_OBJECT,
		T_NAME_SEPARATOR,
		T_VALUE_SEPARATOR,
		T_STRING,
		T_NUMBER,
		T_BOOLEAN,
		T_NULL,
	} type;

	CIRCLEQ_ENTRY(token) cqe;

	union {
		struct string *string;
		double number;
		_Bool boolean;
	};
}; /* struct token */


static const char *lex_strtype(enum tokens type) {
	static const char *name[] = {
		[T_BEGIN_ARRAY] = "begin-array",
		[T_END_ARRAY] = "end-array",
		[T_BEGIN_OBJECT] = "begin-object",
		[T_END_OBJECT] = "end-object",
		[T_NAME_SEPARATOR] = "name-separator",
		[T_VALUE_SEPARATOR] = "value-separator",
		[T_STRING] = "string",
		[T_NUMBER] = "number",
		[T_BOOLEAN] = "boolean",
		[T_NULL] = "null",
	};

	return name[type];
} /* lex_strtype() */


struct lexer {
	void *state;

	struct {
		unsigned pos;
		unsigned row;
		unsigned col;
	} cursor;

	CIRCLEQ_HEAD(, token) tokens;

	struct token *token;

	struct string *string;
	char *sp, *pe;

	int i, code;

	char number[64];

	int error;
}; /* struct lexer */


static void lex_init(struct lexer *L) {
	memset(L, 0, sizeof *L);

	L->cursor.row = 1;

	CIRCLEQ_INIT(&L->tokens);
} /* lex_init() */


static void lex_destroy(struct lexer *L) {
	struct token *T;

	while (!CIRCLEQ_EMPTY(&L->tokens)) {
		T = CIRCLEQ_FIRST(&L->tokens);
		CIRCLEQ_REMOVE(&L->tokens, T, cqe);

		if (T->type == T_STRING)
			free(T->string);

		free(T);
	} /* while (tokens) */

	free(L->string);
} /* lex_destroy() */


static int lex_push(struct lexer *L, enum tokens type, ...) {
	struct token *T;
	va_list ap;
	int error;

	if (!(T = make(sizeof *T, &error)))
		return error;

	memset(T, 0, sizeof *T);

	T->type = type;

	switch (type) {
	case T_STRING:
		va_start(ap, type);
		T->string = va_arg(ap, struct string *);
		va_end(ap);
		break;
	case T_NUMBER:
		va_start(ap, type);
		T->number = va_arg(ap, double);
		va_end(ap);
		break;
	case T_BOOLEAN:
		va_start(ap, type);
		T->boolean = va_arg(ap, int);
		va_end(ap);
		break;
	default:
		break;
	} /* switch() */

	CIRCLEQ_INSERT_TAIL(&L->tokens, T, cqe);
	L->token = T;

	return 0;
} /* lex_push() */


static int lex_newstr(struct lexer *L) {
	size_t size = offsetof(struct string, text) * 2;
	int error;

	if (!(L->string = make(size, &error)))
		return error;

	L->sp = L->string->text;
	L->pe = &L->string->text[size - offsetof(struct string, text)];

	return 0;
} /* lex_newstr() */


static int lex_catstr(struct lexer *L, int ch) {
	size_t count, osize, size;
	void *tmp;

	if (L->sp < L->pe) {
		*L->sp++ = ch;

		return 0;
	}

	count = L->sp - L->string->text;

	osize = offsetof(struct string, text) + (L->pe - L->string->text);
	size = osize << 1;

	if (size < osize)
		return ERANGE;

	if (!(tmp = realloc(L->string, size)))
		return errno;

	L->string = tmp;

	L->sp = &L->string->text[count];
	L->pe = &L->string->text[size - offsetof(struct string, text)];

	*L->sp++ = ch;

	return 0;
} /* lex_catstr() */


static void lex_newnum(struct lexer *L) {
	L->sp = L->number;
	L->pe = &L->number[sizeof L->number - 1];
} /* lex_newnum() */


static inline _Bool lex_isnum(int ch) {
	static const char table[256] = {
		['+'] = 1, ['-'] = 1, ['.'] = 1,
		['0' ... '9'] = 1, ['E'] = 1, ['e'] = 1,
	};

	return table[ch & 0xff];
} /* lex_isnum() */


static int lex_catnum(struct lexer *L, int ch) {
	if (L->sp < L->pe) {
		*L->sp++ = ch;

		return 0;
	} else
		return EOVERFLOW;
} /* lex_catnum() */


static int lex_pushnum(struct lexer *L) {
	double number;
	char *end;

	*L->sp = '\0';

	number = strtod(L->number, &end);

	if (number == 0) {
		if (end == L->number)
			return EINVAL;
		if (errno == ERANGE)
			return ERANGE;
	} else if (number == HUGE_VAL && errno == ERANGE) {
		return ERANGE;
	} else if (*end != '\0')
		return EINVAL;

	return lex_push(L, T_NUMBER, number);
} /* lex_pushnum() */


#define resume() do { goto *((L->state)? L->state : &&start); } while (0)

#define popchar() do { \
	XPASTE(L, __LINE__): \
	if (p >= pe) { L->state = &&XPASTE(L, __LINE__); return 0; } \
	ch = *p++; \
	L->cursor.pos++; \
	L->cursor.col++; \
} while (0)

#define ungetchar() do { \
	p--; \
	L->cursor.pos--; \
	L->cursor.col--; \
} while (0)

#define pushtoken(...) do { \
	if ((error = lex_push(L, __VA_ARGS__))) \
		goto error; \
} while (0)

#define expect(c) do { popchar(); if (ch != (c)) goto invalid; } while (0)

static int lex_parse(struct lexer *L, void *src, size_t len) {
	unsigned char *p = src, *pe = p + len;
	int ch, error;

	resume();
start:
	popchar();

	switch (ch) {
	case ' ': case '\t': case '\r':
		break;
	case '\n':
		L->cursor.row++;
		L->cursor.col = 0;

		break;
	case '[':
		pushtoken(T_BEGIN_ARRAY);
		break;
	case ']':
		pushtoken(T_END_ARRAY);
		break;
	case '{':
		pushtoken(T_BEGIN_OBJECT);
		break;
	case '}':
		pushtoken(T_END_OBJECT);
		break;
	case ':':
		pushtoken(T_NAME_SEPARATOR);
		break;
	case ',':
		pushtoken(T_VALUE_SEPARATOR);
		break;
	case '"':
		goto string;
	case '+': case '-': case '.':
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		ungetchar();

		goto number;
	case 'n':
		goto null;
	case 't':
		goto true;
	case 'f':
		goto false;
	default:
		goto invalid;
	} /* switch (ch) */

	goto start;
string:
	if ((error = lex_newstr(L)))
		goto error;

	for (;;) {
		popchar();

		switch (ch) {
		case '"':
			goto endstr;
		case '\\':
			popchar();

			switch (ch) {
			case '"':
			case '/':
			case '\\':
				goto catstr;
			case 'b':
				ch = '\b';
				goto catstr;
			case 'f':
				ch = '\f';
				goto catstr;
			case 'n':
				ch = '\n';
				goto catstr;
			case 'r':
				ch = '\r';
				goto catstr;
			case 't':
				ch = '\t';
				goto catstr;
			case 'u':
				L->i = 0;
				L->code = 0;

				while (L->i < 4) {
					popchar();

					if (isdigit(ch)) {
						L->code <<= 4;
						L->code += ch - '0';
					} else if (ch >= 'A' && ch <= 'F') {
						L->code <<= 4;
						L->code += 10 + (ch - 'A');
					} else if (ch >= 'a' && ch <= 'f') {
						L->code <<= 4;
						L->code += 10 + (ch - 'a');
					} else
						goto invalid;
				} /* while() */

				/* FIXME: Convert UTF-16 to UTF-8? */
				if ((error = lex_catstr(L, L->code)))
					goto error;

				break;
			default:
				goto invalid;
			} /* switch() */

			break;
		default:
catstr:
			if ((error = lex_catstr(L, ch)))
				goto error;

			break;
		} /* switch() */
	} /* for() */
endstr:
	if ((error = lex_catstr(L, '\0')))
		goto error;

	L->string->length = (L->sp - L->string->text) - 1;

	pushtoken(T_STRING, L->string);
	L->string = 0;

	goto start;
number:
	lex_newnum(L);

	popchar();

	while (lex_isnum(ch)) {
		if ((error = lex_catnum(L, ch)))
			goto error;

		popchar();
	}

	ungetchar();

	if ((error = lex_pushnum(L)))
		goto error;

	goto start;
null:
	expect('u');
	expect('l');
	expect('l');

	pushtoken(T_NULL);

	goto start;
true:
	expect('r');
	expect('u');
	expect('e');

	pushtoken(T_BOOLEAN, 1);

	goto start;
false:
	expect('a');
	expect('l');
	expect('s');
	expect('e');

	pushtoken(T_BOOLEAN, 0);

	goto start;
invalid:
	if (isgraph(ch))
		fprintf(stderr, "invalid char (%c) at line %u, column %u\n", ch, L->cursor.row, L->cursor.col);
	else
		fprintf(stderr, "invalid char (0x%.2x) at line %u, column %u\n", ch, L->cursor.row, L->cursor.col);

	error = EINVAL;

	goto error;
error:
	L->error = error;
	L->state = &&failed;
failed:
	return L->error;
} /* lex_parse() */


/*
 * V A L U E  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef int index_t;

struct node {
	union {
		struct value *key;
		index_t index;
	};

	struct value *value;
	struct value *parent;
}; /* struct node */


struct value {
	enum values {
		V_ARRAY   = T_BEGIN_ARRAY,
		V_OBJECT  = T_BEGIN_OBJECT,
		V_STRING  = T_STRING,
		V_NUMBER  = T_NUMBER,
		V_BOOLEAN = T_BOOLEAN,
		V_NULL    = T_NULL,
	} type;

	union {
		struct {
			LLRB_HEAD(array, node) nodes;
			index_t count;
		} array;

		struct {
			LLRB_HEAD(object, node) nodes;
			index_t count;
		} object;

		struct string *string;

		double number;

		_Bool boolean;
	};
}; /* struct value */


struct printer {
	int state, error;
	struct value *value;

	char literal[64];

	struct {
		char *p, *pe;
	} ibuf;
}; /* struct printer */

#define RESUME() switch (P->state) { case 0: (void)0

#define YIELD() P->state = __LINE__; return p - dst; case __LINE__: (void)0

#define RETURN() P->state = __LINE__; case __LINE__: return p - dst

#define PUTCHAR(ch) do { \
	while (p >= pe) \
		YIELD(); \
	*p++ = (ch); \
} while (0)

#define END } (void)0

static size_t simple_print(struct printer *P, char *dst, size_t lim, struct value *V) {
	char *p = dst, *pe = &dst[lim];

	RESUME();

	switch (V->type) {
	case T_STRING:
		P->ibuf.p = V->string->text;
		P->ibuf.pe = &V->string->text[V->string->length];

		goto string;
	case T_NUMBER: {
		double i;
		int count;

		if (0.0 == modf(V->number, &i))
			count = snprintf(P->literal, sizeof P->literal, "%lld", (long long)i);
		else
			count = snprintf(P->literal, sizeof P->literal, "%f", V->number);

		if (count == -1) {
			P->error = errno;

			goto error;
		} else if ((size_t)count >= sizeof P->literal) {
			P->error = EOVERFLOW;

			goto error;
		}

		P->ibuf.p = P->literal;
		P->ibuf.pe = &P->literal[count];

		goto literal;
	}
	case T_BOOLEAN: {
		size_t count = strlcpy(P->literal, ((V->boolean)? "true" : "false"), sizeof P->literal);

		P->ibuf.p = P->literal;
		P->ibuf.pe = &P->literal[count];

		goto literal;
	}
	case T_NULL: {
		size_t count = strlcpy(P->literal, "null", sizeof P->literal);

		P->ibuf.p = P->literal;
		P->ibuf.pe = &P->literal[count];

		goto literal;
	}
	default:
		return 0;
	} /* switch (V->type) */
string:
	PUTCHAR('"');

	while (P->ibuf.p < P->ibuf.pe) {
		if (isgraph(*P->ibuf.p)) {
			if (*P->ibuf.p == '"' || *P->ibuf.p == '/' || *P->ibuf.p == '\\')
				PUTCHAR('\\');
			PUTCHAR(*P->ibuf.p++);
		} else if (*P->ibuf.p == ' ') {
			PUTCHAR(*P->ibuf.p++);
		} else {
			PUTCHAR('\\');

			if (*P->ibuf.p == '\b')
				PUTCHAR('b');
			else if (*P->ibuf.p == '\f')
				PUTCHAR('f');
			else if (*P->ibuf.p == '\n')
				PUTCHAR('n');
			else if (*P->ibuf.p == '\r')
				PUTCHAR('r');
			else if (*P->ibuf.p == '\t')
				PUTCHAR('t');
			else {
				PUTCHAR('u');
				PUTCHAR('0');
				PUTCHAR('0');
				PUTCHAR("0123456789abcdef"[0x0f & (*P->ibuf.p >> 4)]);
				PUTCHAR("0123456789abcdef"[0x0f & (*P->ibuf.p >> 0)]);
			}

			P->ibuf.p++;
		}
	} /* while() */

	PUTCHAR('"');

	RETURN();
literal:
	while (P->ibuf.p < P->ibuf.pe)
		PUTCHAR(*P->ibuf.p++);

	RETURN();
error:
	RETURN();

	END;
} /* simple_print() */


static int simple_fprint(struct value *V, FILE *fp) {
	struct printer P;
	char tmp[8];
	size_t count;

	memset(&P, 0, sizeof P);

	while ((count = simple_print(&P, tmp, sizeof tmp, V))) {
		fwrite(tmp, 1, count, fp);
	}

	return P.error;
} /* simple_fprint() */



#include <stdio.h>
#include <err.h>

int main(void) {
	struct lexer L;
	char ibuf[1];
	size_t count;
	struct token *T;
	int error;

	lex_init(&L);

	while ((count = fread(ibuf, 1, sizeof ibuf, stdin))) {
		if ((error = lex_parse(&L, ibuf, count)))
			errx(1, "parse: %s", strerror(error));
	}

	CIRCLEQ_FOREACH(T, &L.tokens, cqe) {
		switch (T->type) {
		case T_STRING:
			fprintf(stdout, "%s: %.*s\n", lex_strtype(T->type), (int)T->string->length, T->string->text);
			break;
		case T_NUMBER:
			fprintf(stdout, "%s: %f\n", lex_strtype(T->type), T->number);
			break;
		default:
			fprintf(stdout, "%s\n", lex_strtype(T->type));
		}
	}

	lex_destroy(&L);

	return 0;
} /* main() */

