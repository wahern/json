all: splice

CFLAGS = -Wall -Wextra -O2 -g
CXXFLAGS = $(CFLAGS)
CXX = c++

libjson.a: ../src/json.c
	$(CC) $(CFLAGS) -c -o $@ $^ $(CPPFLAGS)

splice: splice.c libjson.a
	$(CC) $(CFLAGS) $^ -o $@ $(CPPFLAGS) -I../src -lm

splice.cc: splice.c
	ln -f $^ $@

splice++: splice.cc libjson.a
	$(CXX) $(CXXFLAGS) $^ -o $@ $(CPPFLAGS) -I../src -lm

.PHONY: clean clean~

clean:
	$(RM) -rf splice splice++ libjson.a *.dSYM

clean~: clean
	$(RM) -f *~
