.POSIX:

all:
	+make -C src all
        
.DEFAULT:
	+make -C src $<
