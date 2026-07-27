# orbis

A C library for Unix: JSON parsing, a schema validator with its own compact
DSL, and a small set of general-purpose utilities (strings, buffers,
hashmap, pattern matching, time handling, Unicode). It compiles without
warnings under `-std=c11 -Wpedantic -Wall -Wextra -Wmissing-prototypes
-Wstrict-prototypes -Wconversion -Wshadow -Wcast-qual -Wnested-externs
-Wformat=2`, uses no compiler extensions, and runs clean under Valgrind:
0 errors, 0 leaks. The schema validator, on top of that, never touches
the heap.

## Why

Most JSON libraries stop at parsing and encoding. orbis adds a schema
validator that compiles JSON Schema-like constraints into flat, linear
bytecode instead of walking a tree of rule objects at validation time — no
allocations, no recursion through validator internals, just an array of
opcodes executed in a straight line, with jumps to handle objects, arrays
and optional properties.

Zero-allocation JSON parsers already exist (jsmn, for one), but they get
there by staying at the tokenizer level: no real tree, no validation. The
goal here was different: keep the full tree, the schema DSL and the
validator, and still make the part that runs the most — validating a
document against a schema — entirely heap-free.

## A quick look

Given this JSON document:

```json
{
  "name": "John Smith",
  "age": 42,
  "email": "john.doe@mail.com",
  "items": [1, 2, 3]
}
```

...and this schema, written as S-expressions:

```lisp
(object
    (property "name" (string (minLength 1) (maxLength 50)))
    (property "age" (integer (min 0) (max 120)))
    (property "email" (string (format "email")))
    (property "items" (array (integer) (minItems 1) (uniqueItems)))
)
```

```c
#include <orbis/clib_stream.h>
#include <orbis/json_writer.h>
#include <orbis/json_validator.h>

static void on_error(const json_t *node, void *data)
{
    (void)data;
    json_print(node); // {"path": "...", "rule": "..."}
}

int main(void)
{
    char *doc    = file_read("data.json");
    char *schema = file_read("schema.sexp");
    json_t *node = json_decode(doc);
    void   *code = json_compile(schema);

    int ok = json_validate(node, code, on_error, NULL);
    // ...
}
```

Supported keywords: `object`, `tuple`, `array`, `string`, `integer`,
`number`, `boolean`, `null`, `any`, `property`, `optional`, `nullable`,
`etc`, `uniqueItems`, `minItems`, `maxItems`, `minProperties`,
`maxProperties`, `const`, `enum`, `pattern`, `format`, `mask`, `minLength`,
`maxLength`, `min`, `max`, `multipleOf`.

## Design

### What this is (and isn't)

The library is closer to a mini-framework than a finished product: it
covers what its own use cases have needed so far — JSON, schema
validation, a REST server on top. It's built to grow: new
`clib_*`/`json_*` units get added the same way the existing ones were,
not bolted on as an afterthought. What exists today compiles without
warnings, is covered by unit tests, and runs the demo server end to end.

The server doesn't chase the throughput of something like
[Drogon](https://github.com/drogonframework/drogon) or Go with goroutines
and channels. It's single-threaded, one `poll()` loop, no worker pool — a
real ceiling: it runs on a single CPU core. What it optimizes for instead
is a C programmer feeling at home: no green threads, no bespoke callback
framework to learn, no macros hiding what a request actually does. For an
internal tool, an admin panel, or a small-to-medium API, that trade-off
holds up in practice. For saturating many cores it isn't the right tool,
and it doesn't claim to be — scaling that far would mean running several
`orbis` processes behind nginx (SQLite's WAL mode already tolerates one
writer and several concurrent readers across processes), a reasonable
path but not one this project sets out to solve itself.

### Modules

- **JSON** (`json_*`) — parser, encoder/writer, tree reader/query API,
  JSON Pointer (RFC 6901), and the schema validator described above.
- **clib** (`clib_*`) — buffers, a djb2 hashmap with incremental rehashing,
  format validators (dates, emails, URLs, IPv4/IPv6, UUIDs...), a compact
  calendar/time module built on Julian Day arithmetic, Unicode/UTF-8
  helpers, and assorted string/stream/math utilities.
- **server/** — see below.

### Naming and style

Function and variable names spell things out — `json_validate()`,
`buffer_append()`, `map_insert()`, `session_parse()` — a `module_verb()`
shape close to GLib/GTK's (`g_hash_table_insert()`, `gtk_widget_show()`;
GTK builds on GLib and follows its naming conventions): namespaced,
snake_case, no abbreviations that force a guess, but no
`create_a_new_session_token_from_credentials()`-length names either — full
words, kept to as few as stay unambiguous. English isn't my first
language, so if a name, a comment, or anything in this README reads
awkwardly, I'd genuinely like to hear about it.

### Under the hood

Parsing is SAX-style, not DOM: `json_parser.c` and `sexp_parser.c` don't
build anything, they walk the input once and fire a callback per token
(`JSON_OBJECT` on `{`, `JSON_STRING` on a string, `JSON_OBJECT_END` on the
matching `}`, and so on) — the same shape as SAX parsing in the XML world.
The parser has no opinion about what happens with an event; the callback
decides. `json_decode()` uses this to build a `json_t` tree (`decode()` in
`json_writer.c`); `json_compile()` uses the exact same mechanism over
S-expressions to emit bytecode directly (`compile()` in
`json_validator.c`) — no tree involved at all for schemas. One parsing
engine, two completely different consumers, neither of which the parser
itself knows about.

Both parsers decode in place: escape sequences are always shorter than
or equal to their source, so `decode_string()` overwrites the input
buffer between the quotes as it unescapes it, instead of allocating a
separate output string. This means `json_decode()`/`json_compile()`
need a writable buffer — pass a string literal and the parser will most
likely segfault the moment it tries to write into read-only memory. If
you need to keep the original text around, copy it first; in practice
this is rarely necessary, since the usual pattern is reading a file or a
request body straight into a buffer you already own.

Buffers reuse their own capacity: `buffer_t` (`clib_buffer.c`) separates
"how much is written" (`length`) from "how much memory is held" (`size`),
and `buffer_reset()` only touches the former — the underlying allocation
stays put. The server relies on this throughout: each connection's
accumulation buffer is reset after every request instead of freed, so a
connection handling many requests over time settles into reusing the same
block of memory rather than allocating and freeing on every one; the
single, module-level response buffer in `solver.c` works the same way
across every request the process ever handles. `buffer_clear()` — the one
that actually frees — is reserved for connections closing or the server
shutting down.

### Strings and numbers

Escape sequences (`\\`, `\/`, `\"`, `\b`, `\f`, `\n`, `\r`, `\t`, `\uXXXX`)
are accepted anywhere inside a string, mixed freely with literal
characters — `"line one\nline two"` is exactly as valid as `"plain"`.
What isn't supported yet is combining two `\uXXXX` escapes into a UTF-16
surrogate pair to represent a codepoint above U+FFFF (most emoji, for
instance); that's on the list of pending work.

JSON doesn't distinguish integers from floats, but orbis does: every
number is tagged `JSON_INTEGER` or `JSON_REAL` depending on its literal
shape. Internally, though, both are stored as a `double` (`struct
json`'s `number` field), and an IEEE 754 double only has 52 bits of
mantissa — enough to represent integers exactly up to ±2^53 - 1
(9007199254740991), the same bound JavaScript calls
`Number.MAX_SAFE_INTEGER`, for the same reason. An integer literal
outside that range still parses, but as `JSON_REAL`, having already lost
precision on the way through `strtod()`. If you need exact integers
beyond that range, encode them as strings.

## Server

A single-threaded, non-blocking (`poll()`-based) REST server backed by
SQLite:

- Endpoints live in one file, `server/api/endpoints.sql` — a route, a
  schema (the same DSL above), and a SQL statement per endpoint. No C
  code to write or rebuild for a new route.
- `POST /api/reload` picks up changes to that file without restarting the
  process.
- `MAX_CLIENTS` (64 by default, `server/include/config.h`) caps
  concurrent connections; raising it is a one-line change.

The server does only REST: parse a request, validate it against a schema,
run one SQL statement, write back JSON. Nothing else — no static files,
no TLS, no file uploads. That's deliberate: `server/config/orbis.conf` is
a ready-to-use nginx config that handles everything around it. `/api/*` is
proxied to the orbis process; everything else is served straight from
`server/public/` (the login + CRUD demo in vanilla JS). A third block,
`/files/`, shows how to keep protected downloads out of the REST server
entirely: nginx serves them directly via `alias`, gated by its
`auth_request` module, which calls back into `/api/auth` — orbis only ever
sees a cheap session check, nginx does the actual byte-serving. It's a
clean split: orbis owns the data and the rules, nginx owns everything that
looks like "being a web server."

### Known limitations

Routes are matched as exact strings (`"GET /api/users"`, bsearch'd), which
keeps routing O(log n) with zero allocation but rules out path segments
like `/api/users/:id/activate` — today that shape is written as
`/api/users?id=...` instead (see `server/api/endpoints.sql`). All
parameters, whether they come from the query string, a JSON body, or the
session, are matched by name (`@name`, `:name`, `$USER`...); there's no
positional or `:id`-style path binding.

Supporting real path segments without giving up the zero-allocation part
would mean two things: capturing path segments in-place the same way
`decode_params()` already captures query string params (no copying, just
start/length pointers, into the same `params` array), and a trie/radix
router to match them — built once at startup, when `endpoints.sql` is
parsed, never at request time. Neither is in place yet.

## Getting started

### Build and install

```sh
make
sudo make install
sudo ldconfig    # Linux only
```

### Unit tests

```sh
cd tests
make
make run
```

### Examples

```sh
cd examples/validate
CFLAGS="-std=c11 -Wpedantic -Wall -Wextra -O2" LDLIBS="-lorbis" make demo
./demo
```

Other examples cover parsing, decoding, sorting, date/time arithmetic,
pattern matching and raw S-expression parsing — see `examples/`.

`server/scripts/` has small bash scripts (`login`, `get_users`,
`insert_user`, `get_file`...) that drive the demo server end to end with
`curl` — a quick way to see the whole request/response shape without
writing any code.

### Dependencies

Zero external packages. The library (`src/` + `include/`) only needs a C11
compiler and libc/libm — nothing to `apt install`, `brew install` or
vendor. It does rely on a few POSIX headers beyond plain C11: `<unistd.h>`,
`<fcntl.h>`, `<sys/stat.h>` (file I/O in `clib_stream.c`) and `<regex.h>`
(`clib_regex.c`, for the `pattern` keyword). On Linux, macOS and BSD these
ship with the system's libc, so there's nothing extra to install there
either.

**Windows (MinGW-w64):** `<unistd.h>`, `<fcntl.h>` and `<sys/stat.h>` are
bundled with MinGW-w64 and work out of the box. `<regex.h>` is the one
gap — MinGW-w64 doesn't ship POSIX regex. Installing
[libgnurx](https://packages.msys2.org/packages/mingw-w64-x86_64-libgnurx)
(`pacman -S mingw-w64-x86_64-libgnurx` under MSYS2) provides `regex.h` and
`-lregex`; with that in place the library should build as-is. Untested by
me so far — reports welcome.

`server/` is a separate story: it's Unix-only (`<sys/poll.h>`,
`<sys/socket.h>`, `<netinet/in.h>`) and links against SQLite, which is an
actual external dependency (`libsqlite3-dev` or equivalent).

## License

GPL. See the source file headers for details.

## Author

David Ranieri — <davranfor@gmail.com>

