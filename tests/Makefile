CC      = gcc
CFLAGS  = -std=c11 -Wpedantic -Wall -Wextra -Wconversion -Wshadow -I../include
LDLIBS  = -lm
SRCDIR  = ../src

COMMON  = $(SRCDIR)/clib_math.c $(SRCDIR)/clib_unicode.c $(SRCDIR)/json_parser.c
WRITER  = $(COMMON) $(SRCDIR)/json_writer.c
READER  = $(WRITER) $(SRCDIR)/json_reader.c $(SRCDIR)/clib_match.c $(SRCDIR)/clib_regex.c $(SRCDIR)/clib_date.c
POINTER = $(READER) $(SRCDIR)/json_pointer.c
BUFFER  = $(WRITER) $(SRCDIR)/clib_buffer.c $(SRCDIR)/json_buffer.c
SORTER  = $(WRITER) $(SRCDIR)/json_sorter.c
VALIDATOR = $(READER) $(SRCDIR)/sexp_parser.c $(SRCDIR)/clib_string.c $(SRCDIR)/json_validator.c

TESTS = test_parser test_writer test_reader test_pointer test_buffer test_sorter test_validator

.PHONY: all run clean

all: $(TESTS)

test_parser: test_parser.c $(COMMON)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test_writer: test_writer.c $(WRITER)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test_reader: test_reader.c $(READER)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test_pointer: test_pointer.c $(POINTER)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test_buffer: test_buffer.c $(BUFFER)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test_sorter: test_sorter.c $(SORTER)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

test_validator: test_validator.c $(VALIDATOR)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

run: all
	@failed=0; \
	for t in $(TESTS); do \
		./$$t || failed=$$((failed + 1)); \
	done; \
	[ $$failed -eq 0 ] && echo "All tests passed" || echo "$$failed test(s) failed"; \
	exit $$failed

clean:
	rm -f $(TESTS) test_struct
