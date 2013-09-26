.POSIX:

all:
	+cd src && make all
        
.DEFAULT:
	+cd src && make $<
