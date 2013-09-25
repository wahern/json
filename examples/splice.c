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
	int error;

	to = loadpath(to_file);
	from = loadpath(from_file);

	if ((error = json_enter(to, &trap[0])))
		errx(EXIT_FAILURE, "%s: %s", to_file, json_strerror(error));

	if ((error = json_enter(from, &trap[1])))
		errx(EXIT_FAILURE, "%s: %s", from_file, json_strerror(error));

	json_push(to, to_path);
	json_push(from, from_path);


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

