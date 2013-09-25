#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <libgen.h>

#include <err.h>

#include "json.h"


static struct json *loadpath(const char *path) {
	struct json *J;
	int error;

	if (!(J = json_open(0, &error)))
		goto error;
	
	if ((error = (!strcmp(path, "-"))? json_loadfile(J, stdin) : json_loadpath(J, path)))
		goto error;

	return J;
error:
	errx(EXIT_FAILURE, "%s: %s", path, json_strerror(error));
} /* loadpath() */


static void splice(const char *to_file, const char *to_path, const char *from_file, const char *from_path) {
	struct json *to, *from;
	struct jsonxs trap[2];
	struct json_iterator I;
	struct json_value *V, *K;
	int index, error;

	to = loadpath(to_file);
	from = loadpath(from_file);

	if ((error = json_enter(to, &trap[0])))
		errx(EXIT_FAILURE, "%s: %s", to_file, json_strerror(error));

	if ((error = json_enter(from, &trap[1])))
		errx(EXIT_FAILURE, "%s: %s", from_file, json_strerror(error));

	json_push(to, to_path);
	json_push(from, from_path);

	memset(&I, 0, sizeof I);
	I.depth = 64;
	json_v_start(from, &I, json_top(from));

	while ((V = json_v_next(from, &I))) {
		if (I._.order & JSON_I_PREORDER) {
			if (I._.depth > 0) {
				if ((K = json_v_keyof(from, V))) {
					json_push(to, "$", json_v_string(from, K));
				} else {
					index = json_v_indexof(from, V);
					json_push(to, "[#]", index);
				}
			}

			switch (json_v_type(from, V)) {
			case JSON_T_ARRAY:
				json_setarray(to, ".");

				break;
			case JSON_T_OBJECT:
				json_setobject(to, ".");

				break;
			case JSON_T_NULL:
				json_setnull(to, ".");
				json_pop(to);

				break;
			case JSON_T_BOOLEAN:
				json_setboolean(to, json_v_boolean(from, V), ".");
				json_pop(to);

				break;
			case JSON_T_NUMBER:
				json_setnumber(to, json_v_number(from, V), ".");
				json_pop(to);

				break;
			case JSON_T_STRING:
				json_setstring(to, json_v_string(from, V), ".");
				json_pop(to);

				break;
			}
		} else {
			if (I._.depth > 0 && (json_v_type(from, V) == JSON_T_ARRAY || json_v_type(from, V) == JSON_T_OBJECT))
				json_pop(to);
		}
	}

	json_printfile(to, stdout, JSON_F_PRETTY);

	json_leave(from, &trap[1]);
	json_leave(to, &trap[0]);
} /* splice() */


static void usage(const char *progname, FILE *fp) {
#define USAGE \
	"%s [-h] to-file to-path from-file [from-path]\n" \
	"  -h  print usage\n" \
	"\n" \
	"Report bugs to <william@25thandClement.com>\n"

	fprintf(fp, USAGE, progname);
} /* usage() */

int main(int argc, char **argv) {
	extern int optind;
	int opt;

	while (-1 != (opt = getopt(argc, argv, "h"))) {
		switch (opt) {
		case 'h':
			usage(basename(argv[0]), stdout);

			return 0;
		default:
usage:
			usage(basename(argv[0]), stderr);

			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 3)
		goto usage;
	else if (argc < 4)
		splice(argv[0], argv[1], argv[2], ".");
	else
		splice(argv[0], argv[1], argv[2], argv[3]);

	return 0;
} /* main() */

