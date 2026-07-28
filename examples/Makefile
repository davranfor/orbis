CC := gcc
CFLAGS := -std=c11 -Wpedantic -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -Wconversion -Wshadow -Wcast-qual -Wnested-externs -Wformat=2
SOURCES := $(shell find . -name demo.c)

.PHONY: all clean check

all:
	@for f in $(SOURCES); do \
		dir=$$(dirname $$f); \
		$(CC) $(CFLAGS) -o $$dir/demo $$f -lorbis; \
	done

clean:
	find . -type f -name "demo" -exec rm -f {} +	

check:
	clang --analyze -Xanalyzer -analyzer-output=text $(SOURCES)

