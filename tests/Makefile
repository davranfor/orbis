CC      = gcc
CFLAGS  = -std=c11 -Wpedantic -Wall -Wextra -Wconversion -Wshadow -I../include
LDLIBS  = -lm
SRCDIR  = ../src

COMMON  = $(SRCDIR)/clib_math.c $(SRCDIR)/clib_unicode.c $(SRCDIR)/json_parser.c
BUILDER = $(COMMON) $(SRCDIR)/json_builder.c
FULL    = $(BUILDER) $(SRCDIR)/json_pointer.c

TESTS = test_builder test_parser test_pointer

.PHONY: all run clean

all: $(TESTS)

test_builder: test_builder.c $(BUILDER)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test_parser: test_parser.c $(COMMON)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test_pointer: test_pointer.c $(FULL)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

run: all
	@failed=0; \
	for t in $(TESTS); do \
		./$$t || failed=$$((failed + 1)); \
	done; \
	[ $$failed -eq 0 ] && echo "All tests passed" || echo "$$failed test(s) failed"; \
	exit $$failed

clean:
	rm -f $(TESTS)
