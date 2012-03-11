#include <stddef.h>	/* size_t offsetof */
#include <stdarg.h>	/* va_list va_start va_end va_arg */
#include <stdlib.h>	/* malloc(3) realloc(3) free(3) strtod(3) */
#include <stdio.h>	/* snprintf(3) */

#include <string.h>	/* memset(3) strncmp(3) */

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

#define MIN(a, b) (((a) < (b))? (a) : (b))
#define CMP(a, b) (((a) < (b))? -1 : ((a) > (b))? 1 : 0)


static void *make(size_t size, int *error) {
	void *p;

	if (!(p = malloc(size)))
		*error = errno;

	return p;
} /* make() */


static void *make0(size_t size, int *error) {
	void *p;

	if ((p = make(size, error)))
		memset(p, 0, size);

	return p;
} /* make0() */


enum json_errors {
	JSON_EASSERT = -10,
	JSON_ELEXICAL,
	JSON_ESYNTAX,
}; /* enum json_errors */


const char *json_strerror(int error) {
	static const char *descr[] = {
		[JSON_EASSERT-JSON_EASSERT] = "JSON assertion",
		[JSON_ELEXICAL-JSON_EASSERT] = "JSON lexical",
		[JSON_ESYNTAX-JSON_EASSERT] = "JSON syntax",
	};

	if (error >= 0)
		return strerror(error);

	return descr[error - JSON_EASSERT];
} /* json_strerror() */


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
			return JSON_ELEXICAL;
		if (errno == ERANGE)
			return ERANGE;
	} else if (number == HUGE_VAL && errno == ERANGE) {
		return ERANGE;
	} else if (*end != '\0')
		return JSON_ELEXICAL;

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

static int lex_parse(struct lexer *L, const void *src, size_t len) {
	const unsigned char *p = src, *pe = p + len;
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

	error = JSON_ELEXICAL;

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
		struct json_value *key;
		index_t index;
	};

	struct json_value *value;
	struct json_value *parent;

	union {
		LLRB_ENTRY(node) rbe;
		CIRCLEQ_ENTRY(node) cqe;
	};
}; /* struct node */


struct json_value {
	enum json_values {
		JSON_V_ARRAY   = T_BEGIN_ARRAY,
		JSON_V_OBJECT  = T_BEGIN_OBJECT,
		JSON_V_STRING  = T_STRING,
		JSON_V_NUMBER  = T_NUMBER,
		JSON_V_BOOLEAN = T_BOOLEAN,
		JSON_V_NULL    = T_NULL,
	} type;

	struct node *node;

	union { /* mutually exclusive usage */ 
		struct json_value *root;
		void *state;
	};

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
}; /* struct json_value */


static int array_cmp(struct node *a, struct node *b) {
	return CMP(a->index, b->index);
} /* array_cmp() */

LLRB_GENERATE(array, node, rbe, array_cmp)


/* NOTE: All keys must be strings per RFC 4627. */
static int object_cmp(struct node *a, struct node *b) {
	int cmp;

	if ((cmp = strncmp(a->key->string->text, b->key->string->text, MIN(a->key->string->length, b->key->string->length))))
		return cmp;

	return CMP(a->key->string->length, b->key->string->length);
} /* object_cmp() */

LLRB_GENERATE(object, node, rbe, object_cmp)


static size_t value_sizeof(enum json_values type) {
	switch (type) {
	case JSON_V_ARRAY:
		return offsetof(struct json_value, array) + sizeof ((struct json_value *)0)->array;
	case JSON_V_OBJECT:
		return offsetof(struct json_value, object) + sizeof ((struct json_value *)0)->object;
	case JSON_V_STRING:
		return offsetof(struct json_value, string) + sizeof ((struct json_value *)0)->string;
	case JSON_V_NUMBER:
		return offsetof(struct json_value, number) + sizeof ((struct json_value *)0)->number;
	case JSON_V_BOOLEAN:
		return offsetof(struct json_value, boolean) + sizeof ((struct json_value *)0)->boolean;
	default:
		return offsetof(struct json_value, array);
	} /* switch() */
} /* value_sizeof() */


static struct json_value *value_open(enum json_values type, struct token *T, int *error) {
	struct json_value *V;

	if (!(V = make0(value_sizeof(type), error)))
		return 0;

	if (T) {
		switch (type) {
		case JSON_V_STRING:
			V->string = T->string;
			T->string = 0; /* steal the string */
			break;
		case JSON_V_NUMBER:
			V->number = T->number;
			break;
		case JSON_V_BOOLEAN:
			V->boolean = T->boolean;
			break;
		default:
			break;
		} /* switch() */
	} /* if (T) */

	return V;
} /* value_open() */


static void value_close(struct json_value *);

static int array_push(struct json_value *A, struct json_value *V) {
	struct node *N;
	int error;

	if (!(N = make(sizeof *N, &error)))
		return error;

	N->index = A->array.count++;
	N->value = V;
	N->parent = A;
	V->node = N;

	LLRB_INSERT(array, &V->array.nodes, N);

	return 0;
} /* array_push() */


static int object_insert(struct json_value *O, struct json_value *K, struct json_value *V) {
	struct node *N, *prev;
	int error;

	if (!(N = make(sizeof *N, &error)))
		return error;

	N->key = K;
	N->value = V;
	N->parent = O;
	K->node = N;
	V->node = N;

	if (!(prev = LLRB_INSERT(object, &O->object.nodes, N)))
		return 0;

	free(N);

	value_close(prev->key);
	value_close(prev->value);

	prev->key = K;
	prev->value = V;
	K->node = prev;
	V->node = prev;

	return 0;
} /* object_insert() */


CIRCLEQ_HEAD(orphans, node);

static void array_remove(struct json_value *V, struct node *N, struct orphans *indices) {
	LLRB_REMOVE(array, &V->array.nodes, N);
	N->parent = 0;

	CIRCLEQ_INSERT_TAIL(indices, N, cqe);
} /* array_remove() */


static void array_clear(struct json_value *V, struct orphans *indices) {
	struct node *N, *nxt;

	for (N = LLRB_MIN(array, &V->array.nodes); N; N = nxt) {
		nxt = LLRB_NEXT(array, &V->array.nodes, N);

		array_remove(V, N, indices);
	}
} /* array_clear() */


static void object_remove(struct json_value *V, struct node *N, struct orphans *keys) {
	LLRB_REMOVE(object, &V->object.nodes, N);
	N->parent = 0;

	CIRCLEQ_INSERT_TAIL(keys, N, cqe);
} /* object_remove() */


static void object_clear(struct json_value *V, struct orphans *keys) {
	struct node *N, *nxt;

	for (N = LLRB_MIN(object, &V->object.nodes); N; N = nxt) {
		nxt = LLRB_NEXT(object, &V->array.nodes, N);

		object_remove(V, N, keys);
	}
} /* object_clear() */


static void value_clear(struct json_value *V, struct orphans *indices, struct orphans *keys) {
	if (V->type == JSON_V_ARRAY)
		array_clear(V, indices);
	else if (V->type == JSON_V_OBJECT)
		object_clear(V, keys);
} /* value_clear() */


static void value_destroy(struct json_value *V, struct orphans *indices, struct orphans *keys) {
	value_clear(V, indices, keys);

	if (V->type == JSON_V_STRING) {
		free(V->string);
		V->string = 0;
	}
} /* value_destroy() */


static void node_remove(struct node *N, struct orphans *indices, struct orphans *keys) {
	if (N->parent->type == JSON_V_ARRAY)
		array_remove(N->parent, N, indices);
	else
		object_remove(N->parent, N, keys);
} /* node_remove() */


static void value_close(struct json_value *V) {
	struct orphans indices, keys;
	struct node *N;

	CIRCLEQ_INIT(&indices);
	CIRCLEQ_INIT(&keys);

	value_destroy(V, &indices, &keys);

	do {
		while (!CIRCLEQ_EMPTY(&indices)) {
			N = CIRCLEQ_FIRST(&indices);
			CIRCLEQ_REMOVE(&indices, N, cqe);

			value_destroy(N->value, &indices, &keys);
			free(N->value);
			free(N);
		}

		while (!CIRCLEQ_EMPTY(&keys)) {
			N = CIRCLEQ_FIRST(&keys);
			CIRCLEQ_REMOVE(&keys, N, cqe);

			value_destroy(N->key, &indices, &keys);
			free(N->key);

			value_destroy(N->value, &indices, &keys);
			free(N->value);

			free(N);
		}
	} while (!CIRCLEQ_EMPTY(&indices) || !CIRCLEQ_EMPTY(&keys));
} /* value_close() */


static struct json_value *value_descend(struct json_value *V) {
	struct node *N;

	if (V->type == JSON_V_ARRAY) {
		if ((N = LLRB_MIN(array, &V->array.nodes)))
			return N->value;
	} else if (V->type == JSON_V_OBJECT) {
		if ((N = LLRB_MIN(object, &V->object.nodes)))
			return N->key;
	}

	return 0;
} /* value_descend() */


static struct node *node_next(struct node *N) {
	if (N->parent->type == JSON_V_ARRAY)
		return LLRB_NEXT(array, &N->parent->array.nodes, N);
	else
		return LLRB_NEXT(object, &N->parent->object.nodes, N);
} /* node_next() */


static struct json_value *value_sibling(struct json_value *V) {
	struct node *N;

	if ((N = node_next(V->node))) {
		if (N->parent->type == JSON_V_ARRAY)
			return N->value;
		else
			return N->key;
	}

	return 0;
} /* value_sibling() */


static _Bool value_iskey(struct json_value *V) {
	return (V->node && V->node->parent->type == JSON_V_OBJECT);
} /* value_iskey() */


#define ORDER_PRE 0x01
#define ORDER_POST 0x02

static struct json_value *value_next(struct json_value *V, int *order) {
	struct json_value *nxt;

	if ((*order & ORDER_PRE) && (V->type == JSON_V_ARRAY || V->type == JSON_V_OBJECT)) {
		if ((nxt = value_descend(V)))
			return nxt;

		*order = ORDER_POST;

		return V;
	} else if (!V->node) {
		return 0;
	} else if (value_iskey(V)) {
		*order = ORDER_PRE;

		return V->node->value;
	} else if ((nxt = value_sibling(V))) {
		*order = ORDER_PRE;

		return nxt;
	} else {
		*order = ORDER_POST;

		return V->node->parent;
	}
} /* value_next() */


#define PRINT_PRETTY 0x01

struct printer {
	int flags, state, sstate, order, error;
	struct json_value *root, *value;

	unsigned i, depth;

	char literal[64];

	struct {
		char *p, *pe;
	} ibuf;
}; /* struct printer */


static void print_init(struct printer *P, struct json_value *V, int flags) {
	memset(P, 0, sizeof *P);
	P->flags = flags;
	P->order = (V->type == JSON_V_ARRAY || V->type == JSON_V_OBJECT)? ORDER_PRE : ORDER_POST;
	P->root = P->value = V;
} /* print_init() */


#define RESUME() switch (P->sstate) { case 0: (void)0

#define YIELD() P->sstate = __LINE__; return p - (char *)dst; case __LINE__: (void)0

#define STOP() P->sstate = __LINE__; case __LINE__: return p - (char *)dst

#define PUTCHAR(ch) do { \
	while (p >= pe) \
		YIELD(); \
	*p++ = (ch); \
} while (0)

#define END } (void)0

static size_t print_simple(struct printer *P, void *dst, size_t lim, struct json_value *V, int order) {
	char *p = dst, *pe = p + lim;

	RESUME();

	switch (V->type) {
	case JSON_V_ARRAY:
		if (order & ORDER_PRE)
			P->literal[0] = '[';
		else
			P->literal[0] = ']';

		P->ibuf.p = P->literal;
		P->ibuf.pe = &P->literal[1];

		goto literal;
	case JSON_V_OBJECT:
		if (order & ORDER_PRE)
			P->literal[0] = '{';
		else
			P->literal[0] = '}';

		P->ibuf.p = P->literal;
		P->ibuf.pe = &P->literal[1];

		goto literal;
	case JSON_V_STRING:
		P->ibuf.p = V->string->text;
		P->ibuf.pe = &V->string->text[V->string->length];

		goto string;
	case JSON_V_NUMBER: {
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
	case JSON_V_BOOLEAN: {
		size_t count = strlcpy(P->literal, ((V->boolean)? "true" : "false"), sizeof P->literal);

		P->ibuf.p = P->literal;
		P->ibuf.pe = &P->literal[count];

		goto literal;
	}
	case JSON_V_NULL: {
		size_t count = strlcpy(P->literal, "null", sizeof P->literal);

		P->ibuf.p = P->literal;
		P->ibuf.pe = &P->literal[count];

		goto literal;
	}
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

	STOP();
literal:
	while (P->ibuf.p < P->ibuf.pe)
		PUTCHAR(*P->ibuf.p++);

	STOP();
error:
	STOP();

	END;

	return 0;
} /* print_simple() */

#undef RESUME
#undef YIELD
#undef PUTCHAR
#undef STOP
#undef END


#define RESUME() switch (P->state) { case 0: (void)0

#define YIELD() P->state = __LINE__; return p - (char *)dst; case __LINE__: (void)0

#define STOP() P->state = __LINE__; case __LINE__: return p - (char *)dst

#define PUTCHAR(ch) do { \
	while (p >= pe) \
		YIELD(); \
	*p++ = (ch); \
} while (0)

#define END } (void)0

static size_t print(struct printer *P, void *dst, size_t lim) {
	char *p = dst, *pe = p + lim;
	size_t count;

	RESUME();

	while (P->value) {
		if (P->value->type == JSON_V_ARRAY || P->value->type == JSON_V_OBJECT) {
			if (P->order == ORDER_PRE)
				P->depth++;
			else
				P->depth--;
		}

		P->i = 1;

		while (P->i < P->depth)
			PUTCHAR('\t');

		P->sstate = 0;

		while ((count = print_simple(P, p, pe - p, P->value, P->order)))
			p += count;

		if (P->error)
			STOP();

		if (P->value == P->root && P->order == ORDER_POST)
			STOP();

		if (value_iskey(P->value)) {
			PUTCHAR(' ');
			PUTCHAR(':');
			PUTCHAR(' ');
		} else {
			PUTCHAR(',');
			PUTCHAR('\n');
		}

		P->value = value_next(P->value, &P->order);
	}

	STOP();

	END;

	return 0;
} /* print() */

#undef RESUME
#undef YIELD
#undef PUTCHAR
#undef STOP
#undef END


static int printfile(struct json_value *V, int flags, FILE *fp) {
	struct printer P;
	char obuf[512];
	size_t count;

	print_init(&P, V, flags);

	while ((count = print(&P, obuf, sizeof obuf))) {
		if (count != fwrite(obuf, 1, count, fp))
			return errno;
	}

	return P.error;
} /* printfile() */


/*
 * P A R S E R  R O U T I N E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct parser {
	struct lexer lexer;
	CIRCLEQ_HEAD(, token) tokens;
	void *state;
	struct json_value *root;
	struct json_value *key;
	struct json_value *value;
	int error;
}; /* struct parser */


static void parse_init(struct parser *P) {
	lex_init(&P->lexer);
	CIRCLEQ_INIT(&P->tokens);
} /* parse_init() */


static struct json_value *tovalue(struct token *T, int *error) {
	/* value types identical to relevant token types */
	return value_open(T->type, T, error);
} /* tovalue() */


#define RESUME() do { \
	T = (CIRCLEQ_EMPTY(&P->tokens))? 0 : CIRCLEQ_LAST(&P->tokens); \
	goto *((P->state)? P->state : &&start); \
} while (0)

#define YIELD() do { \
	P->state = &&XPASTE(L, __LINE__); \
	return EAGAIN; \
	XPASTE(L, __LINE__): (void)0; \
} while (0)

#define STOP(why) do { \
	P->error = (why); \
	P->state = &&XPASTE(L, __LINE__); \
	XPASTE(L, __LINE__): return P->error; \
} while (0)

#define POPTOKEN() do { \
	while (CIRCLEQ_EMPTY(&P->lexer.tokens)) \
		YIELD(); \
	T = CIRCLEQ_FIRST(&P->lexer.tokens); \
	CIRCLEQ_REMOVE(&P->lexer.tokens, T, cqe); \
	CIRCLEQ_INSERT_TAIL(&P->tokens, T, cqe); \
} while (0)

#define POPSTACK() do { \
	void *state = P->root->state; \
	if (P->root->node) \
		P->root = P->root->node->parent; \
	goto *state; \
} while (0)

#define PUSHARRAY(V) do { \
	(V)->state = &&XPASTE(L, __LINE__); \
	P->root = V; \
	goto array; \
	XPASTE(L, __LINE__): (void)0; \
} while (0)

#define PUSHOBJECT(V) do { \
	(V)->state = &&XPASTE(L, __LINE__); \
	P->root = V; \
	goto object; \
	XPASTE(L, __LINE__): (void)0; \
} while (0)

#define TOVALUE(T) do { \
	if (!(P->value = V = tovalue(T, &error))) \
		STOP(error); \
} while (0)

#define TOKEY(T) do { \
	if (!(P->key = V = tovalue(T, &error))) \
		STOP(error); \
} while (0)

#define INSERT2ARRAY() do { \
	if ((error = array_push(P->root, P->value))) \
		STOP(error); \
	P->value = 0; \
} while (0)

#define INSERT2OBJECT() do { \
	if ((error = object_insert(P->root, P->key, P->value))) \
		STOP(error); \
	P->key = 0; \
	P->value = 0; \
} while (0)

#define LOOP for (;;)

static int parse(struct parser *P, const void *src, size_t len) {
	struct token *T;
	struct json_value *V;
	int error;

	if ((error = lex_parse(&P->lexer, src, len)))
		return error;

	RESUME();
start:
	POPTOKEN();

	switch (T->type) {
	case T_BEGIN_ARRAY:
		if (!(P->root = value_open(JSON_V_ARRAY, T, &error)))
			STOP(error);

		P->root->state = &&stop;

		goto array;
	case T_BEGIN_OBJECT:
		if (!(P->root = value_open(JSON_V_OBJECT, T, &error)))
			STOP(error);

		P->root->state = &&stop;

		goto object;
	default:
		STOP(JSON_ESYNTAX);
	} /* switch() */
array:
	LOOP {
		POPTOKEN();

		switch (T->type) {
		case T_BEGIN_ARRAY:
			TOVALUE(T);

			INSERT2ARRAY();

			PUSHARRAY(V);

			break;
		case T_END_ARRAY:
			POPSTACK();
		case T_BEGIN_OBJECT:
			TOVALUE(T);

			INSERT2ARRAY();

			PUSHOBJECT(V);

			break;
		case T_END_OBJECT:
			STOP(JSON_ESYNTAX);
		case T_VALUE_SEPARATOR:
			break;
		case T_NAME_SEPARATOR:
			STOP(JSON_ESYNTAX);
		default:
			TOVALUE(T);

			INSERT2ARRAY();

			break;
		} /* switch() */
	} /* LOOP */
object:
	LOOP {
		POPTOKEN();

		if (T->type == T_END_OBJECT)
			POPSTACK();
		else if (T->type != T_STRING)
			STOP(JSON_ESYNTAX);

		TOKEY(T);

		POPTOKEN();

		if (T->type != T_NAME_SEPARATOR)
			STOP(JSON_ESYNTAX);

		POPTOKEN();

		switch (T->type) {
		case T_BEGIN_ARRAY:
			TOVALUE(T);

			INSERT2OBJECT();

			PUSHARRAY(V);

			break;
		case T_END_ARRAY:
			STOP(JSON_ESYNTAX);
		case T_BEGIN_OBJECT:
			TOVALUE(T);

			INSERT2OBJECT();

			PUSHOBJECT(V);

			break;
		case T_END_OBJECT:
			STOP(JSON_ESYNTAX);
		case T_NAME_SEPARATOR:
			STOP(JSON_ESYNTAX);
		default:
			TOVALUE(T);

			INSERT2OBJECT();

			break;
		} /* switch() */
	} /* LOOP */
stop:
	return 0;
} /* parse() */


static int parsefile(struct parser *P, FILE *fp) {
	char ibuf[1024];
	size_t count;
	int error;

	while ((count = fread(ibuf, 1, sizeof ibuf, fp))) {
		if ((error = parse(P, ibuf, count)))
			return error;
	}

	if (ferror(fp))
		return errno;

	return 0;
} /* parsefile() */


#include <stdio.h>
#include <err.h>

#if 1
int main(void) {
	struct parser parser;
	int error;

	parse_init(&parser);

	if ((error = parsefile(&parser, stdin)))
		errx(1, "stdin: %s", json_strerror(error));

	if ((error = printfile(parser.root, PRINT_PRETTY, stdout)))
		errx(1, "stdout: %s", json_strerror(error));

	return 0;
} /* main() */
#else
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
#endif
