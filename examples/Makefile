all: splice

CPPFLAGS = -I../src
CFLAGS = -Wall -Wextra -O2 -g

libjson.a: ../src/json.c
	$(CC) $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

splice: splice.c libjson.a

splice.cc: splice.c
	ln $^ $@

splice++: splice.cc libjson.a
	$(CXX) $(CFLAGS) $^ -o $@ $(CPPFLAGS)

.PHONY: clean clean~

clean:
	$(RM) -rf splice splice++ *.dSYM

clean~: clean
	$(RM) -f *~
