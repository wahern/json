all: splice

CPPFLAGS = -I../src
CFLAGS = -Wall -Wextra -O2 -g
CXXFLAGS = $(CFLAGS)

libjson.a: ../src/json.c
	$(CC) $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

splice: splice.c libjson.a

splice.cc: splice.c
	ln -f $^ $@

splice++: splice.cc libjson.a
	$(CXX) $(CXXFLAGS) $^ -o $@ $(CPPFLAGS)

.PHONY: clean clean~

clean:
	$(RM) -rf splice splice++ libjson.a *.dSYM

clean~: clean
	$(RM) -f *~
