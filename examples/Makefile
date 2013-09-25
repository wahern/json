all: splice

CPPFLAGS = -I../src
CFLAGS = -Wall -Wextra

libjson.a: ../src/json.c
	$(CC) $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

splice: splice.c libjson.a