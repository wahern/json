/* ==========================================================================
 * json.h - Path Autovivifying JSON C Library
 * --------------------------------------------------------------------------
 * Copyright (c) 2012  William Ahern
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ==========================================================================
 */
#ifndef JSON_H
#define JSON_H

#include <stddef.h>	/* size_t */

#include <setjmp.h>	/* _setjmp(3) */


/*
 * J S O N  V E R S I O N  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define JSON_VERSION JSON_V_REL
#define JSON_VENDOR "william@25thandClement.com"

#define JSON_V_REL 0x20120507
#define JSON_V_ABI 0x20120505
#define JSON_V_API 0x20120505

int json_version(void);
const char *json_vendor(void);

int json_v_rel(void);
int json_v_abi(void);
int json_v_api(void);


/*
 * J S O N  C O R E  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define JSON_F_PRETTY  1
#define JSON_F_TRACE   2
#define JSON_F_STRONG  4
#define JSON_F_AUTOVIV 8

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


/*
 * J S O N  V A L U E  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define JSON_M_AUTOVIV 0x01
#define JSON_M_CONVERT 0x02

struct json_value *json_v_search(struct json *, struct json_value *, int, const void *, size_t);

struct json_value *json_v_index(struct json *, struct json_value *, int, int);

int json_v_delete(struct json *, struct json_value *);

int json_v_clear(struct json *, struct json_value *);

double json_v_number(struct json *, struct json_value *);

const char *json_v_string(struct json *, struct json_value *);

size_t json_v_length(struct json *, struct json_value *);

size_t json_v_count(struct json *, struct json_value *);

_Bool json_v_boolean(struct json *, struct json_value *);

int json_v_setnumber(struct json *, struct json_value *, double);

int json_v_setlstring(struct json *, struct json_value *, const void *, size_t);

int json_v_setstring(struct json *, struct json_value *, const void *);

int json_v_setboolean(struct json *, struct json_value *, _Bool);

int json_v_setnull(struct json *, struct json_value *);

int json_v_setarray(struct json *, struct json_value *);

int json_v_setobject(struct json *, struct json_value *);


/*
 * J S O N  P A T H  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int json_push(struct json *J, const char *, ...);

void json_pop(struct json *);

void json_popall(struct json *);

void json_delete(struct json *J, const char *, ...);

double json_number(struct json *, const char *, ...);

const char *json_string(struct json *J, const char *, ...);

size_t json_length(struct json *J, const char *, ...);

_Bool json_boolean(struct json *J, const char *, ...);

int json_setnumber(struct json *, double, const char *, ...);

int json_setlstring(struct json *, const void *, size_t, const char *, ...);

int json_setstring(struct json *, const void *, const char *, ...);

int json_setboolean(struct json *, _Bool, const char *, ...);

int json_setnull(struct json *, const char *, ...);

int json_setarray(struct json *, const char *, ...);

int json_setobject(struct json *, const char *, ...);



/*
 * J S O N  E R R O R  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define JSON_EBASE -(('J' << 24) | ('S' << 16) | ('O' << 8) | 'N')
#define JSON_ERROR(error) ((error) <= JSON_EBASE && (error) < JSON_ELAST)

enum json_errors {
	JSON_EASSERT = JSON_EBASE,
	JSON_ELEXICAL,
	JSON_ESYNTAX,
	JSON_ETRUNCATED,
	JSON_ENOMORE,
	JSON_ETYPING,
	JSON_ELAST
}; /* enum json_errors */


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


const char *json_strerror(int);


#endif /* JSON_H */
