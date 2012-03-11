CC = $(shell env PATH="/usr/local/gcc46/bin:$${PATH}" command -v gcc)
CFLAGS = -std=gnu99 -Wall -Wextra -O3

all: json

.PHONY: clean clean~

clean:
	rm -rf json *.o *.dSYM

clean~: clean
	rm -f *~
