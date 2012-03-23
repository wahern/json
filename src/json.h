#ifndef JSON_H
#define JSON_H

#include <stddef.h>	/* size_t */

#include <setjmp.h>	/* _setjmp(3) */


/*
 * J S O N  C O R E  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define JSON_F_PRETTY 1
#define JSON_F_TRACE  2
#define JSON_F_STRONG 4

struct json;

struct json *json_open(int, int *);

void json_close(struct json *);

int json_parse(struct json *, const void *, size_t);

int json_loadlstring(struct json *, const void *, size_t);

int json_loadstring(struct json *, const char *);

int json_loadfile(struct json *, FILE *);

int json_loadpath(struct json *, const char *);

/** compose JSON into buffer */
size_t json_compose(struct json *, void *, size_t, int, int *);

/** reset composition state */
void json_flush(struct json *);

/** return next character in JSON composition */
int json_getc(struct json *, int, int *);

/** compose JSON into FILE */
int json_printfile(struct json *, FILE *, int);


#if 0
int json_push(struct json *, int, const char *path);

struct json_value *json_root(struct json *, struct json_value *);
#endif

#define JSON_M_CREATE  0x01
#define JSON_M_CONVERT 0x02

struct json_value *json_v_search(struct json *, struct json_value *, int, const void *, size_t);

struct json_value *json_v_index(struct json *, struct json_value *, int, int);

void json_v_delete(struct json *, struct json_value *);

double json_v_number(struct json *, struct json_value *);

const char *json_v_string(struct json *, struct json_value *);

size_t json_v_length(struct json *, struct json_value *);

size_t json_v_count(struct json *, struct json_value *);

_Bool json_v_boolean(struct json *, struct json_value *);

void json_v_setstring(struct json *, struct json_value *, const void *, size_t);



/*
 * J S O N  E R R O R  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct jsonxs {
	jmp_buf trap, *otrap;
}; /* struct jsonxs */

jmp_buf *json_setjmp(struct json *, jmp_buf *);

#define json_enter(J, xs) ({ \
	(xs)->otrap = json_setjmp((J), &(xs)->trap); \
	_setjmp((xs)->trap); \
})

#define json_leave(J, xs) \
	json_setjmp((J), (xs)->otrap)

int json_throw(struct json *, int);


#endif /* JSON_H */
