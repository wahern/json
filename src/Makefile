CC = $(shell env PATH="/usr/local/gcc46/bin:$${PATH}" command -v gcc)
CPPFLAGS=-D_GNU_SOURCE -D_BSD_SOURCE -D_DARWIN_C_SOURCE
CFLAGS = -std=c1x -Wall -Wextra -O2 -g
LDFLAGS = -lffi

all: json

.PHONY: clean clean~

clean:
	rm -rf json *.o *.dSYM

clean~: clean
	rm -f *~
