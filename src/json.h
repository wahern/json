#ifndef JSON_H
#define JSON_H

#include <setjmp.h>	/* _setjmp(3) */


/*
 * J S O N  C O R E  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct json;

struct json *json_open(int *);

void json_close(struct json *);

int json_loadfile(struct json *, FILE *);

int json_loadpath(struct json *, const char *);

#define JSON_PRETTY 1

int json_printfile(struct json *, FILE *, int);



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
