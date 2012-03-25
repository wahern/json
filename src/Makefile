#
# NOTE: This Makefile is just an example, intended for the regression
# utility, and is not likely to work in your environment because there is no
# standard location for the libffi headers and library.
#
# The ffi.h in OS X is broken and needs to be patched:
#
# --- /usr/include/ffi/ffi.h.xcode	2011-05-25 09:12:43.000000000 -0700
# +++ /usr/include/ffi/ffi.h	2012-03-10 20:18:54.000000000 -0800
# @@ -302,7 +302,7 @@
#  	void				(*fun)(ffi_cif*,void*,ffi_raw*,void*),
#  	void*				user_data);
#  
# -#elif
# +#else
#  #error FFI_CLOSURES not defined
#  #endif
#

cc-option = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null \
              > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi;)

# Find most recent GCC version
CC := $(shell find $$(echo $${PATH} | tr ':' ' ') -maxdepth 1 -name gcc -o -name gcc-\* | while read GCC; do echo $$($$GCC -v 2>&1 | tail -n1 | cut -d' ' -f3) $$GCC; done | sort -nr | head -n1 | cut -d' ' -f2)

override CPPFLAGS += -D_GNU_SOURCE -D_BSD_SOURCE -D_DARWIN_C_SOURCE -DJSON_MAIN
override DFLAGS   += -Wall -Wextra -O0 -g
override CFLAGS   += $(call cc-option, -std=c11, -std=gnu99) $(DFLAGS)
override LDFLAGS  += 
override LDLIBS   += -lffi

ifeq (Linux,$(shell uname -s))
override CPPFLAGS += -DFFI_H_PATH=\<ffi.h\>
override LDLIBS   += -lm
else
ifeq (OpenBSD,$(shell uname -s))
override CPPFLAGS += -I/usr/local/include -DFFI_H_PATH=\<ffi.h\>
override LDFLAGS  += -L/usr/local/lib
override LDLIBS   += -pthread
endif
endif

all: json

.PHONY: clean clean~

clean:
	rm -rf json *.o *.dSYM

clean~: clean
	rm -f *~
