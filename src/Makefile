# Find most recent GCC version
CC := $(shell find $$(echo $${PATH} | tr ':' ' ') -maxdepth 1 -name gcc -o -name gcc-\* | while read GCC; do echo $$($$GCC -v 2>&1 | tail -n1 | cut -d' ' -f3) $$GCC; done | sort -nr | head -n1 | cut -d' ' -f2)

override CPPFLAGS += -D_GNU_SOURCE -D_BSD_SOURCE -D_DARWIN_C_SOURCE -DJSON_MAIN
override DFLAGS   += -Wall -Wextra -O0 -g
override CFLAGS   += -std=c1x $(DFLAGS)
override LDFLAGS  += -lffi

all: json

.PHONY: clean clean~

clean:
	rm -rf json *.o *.dSYM

clean~: clean
	rm -f *~
