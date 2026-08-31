/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <orbis/clib_math.h>
#include <orbis/clib_stream.h>
#include <orbis/json_private.h>
#include <orbis/json_encoder.h>
#include <orbis/json_inspector.h>
#include <orbis/json_validator.h>
#include <sqlite3.h>
#include "headers.h"
#include "loader.h"
#include "router.h"
#include "static.h"
#include "solver.h"

enum
{
    HTTP_OK = 200,
    HTTP_CREATED = 201,
    HTTP_BAD_REQUEST = 400,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
};

static sqlite3 *db;
static int db_command;
static sqlite3_stmt *auth;
static session_t *session;
static buffer_t buffer;

static void db_exec(const char *sql)
{
    char *error = NULL;

    if (sqlite3_exec(db, sql, NULL, NULL, &error) != SQLITE_OK)
    {
        fprintf(stderr, "%s\n", error);
        sqlite3_free(error);
        exit(EXIT_FAILURE);
    }
}

static void db_on_change(void *context, int command, const char *db_name,
    const char *table, sqlite3_int64 rowid)
{
    (void)context;
    (void)db_name;
    (void)table;
    (void)rowid;
    db_command = command;
}

/**
 * Custom SQL function: assert(condition, message). Sets no result
 * (defaults to NULL) when 'condition' is truthy, and aborts the whole
 * statement with 'message' as the SQL error otherwise. Meant as an
 * early guard inside a WHERE clause, so @stmt can enforce invariants
 * without an extra round trip to C:
 *   SELECT TRUE WHERE assert(:a > :b, 'a must be greater than b') IS NULL;
 * If you don't want to return a value (i.e. checking before a
 * statement), simply use:
 *   SELECT assert(:a > :b, 'a must be greater than b');
 */
static void db_assert(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc != 2)
    {
        sqlite3_result_error(context, "assert() takes 2 arguments", -1);
        return;
    }
    if (sqlite3_value_int(argv[0]))
    {
        return;
    }

    const char *message = (const char *)sqlite3_value_text(argv[1]);
    
    sqlite3_result_error(context, message ? message : "Aborted", -1);
}

/**
 * Custom SQL function: new_token(user, role, token, days). Rebuilds
 * this request's session — user, role, token, and its Set-Cookie
 * header — via session_build(), and returns the resulting token as
 * text so @stmt can store it directly:
 *   UPDATE users SET token = new_token(id, role, token, 30)
 *   WHERE email = :email AND password = :password;
 * 'days' sets the cookie's Max-Age (30 above means the session expires
 * in 30 days) — converted to seconds here, since that's what
 * session_build() expects. Here 'token' is the row's own (pre-update)
 * token column: empty means session_build() generates a fresh random
 * one; non-empty means it's kept as-is, only 'user'/'role', the expiry
 * and the cookie get refreshed.
 */
static void db_new_token(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc != 4)
    {
        sqlite3_result_error(context, "new_token() takes 4 arguments", -1);
        return;
    }

    int user = sqlite3_value_int(argv[0]);
    int role = sqlite3_value_int(argv[1]);
    const char *token = (const char *)sqlite3_value_text(argv[2]);
    long long max_age = (long long)sqlite3_value_int(argv[3]) * 60 * 60 * 24;
    const char *value = session_build(session, user, role, token, max_age);

    if (value == NULL)
    {
        sqlite3_result_error(context, "new_token() failed", -1);
        return;
    }
    sqlite3_result_text(context, value, -1, SQLITE_STATIC);
}

/**
 * Custom SQL function: delete_token(). Clears this request's session
 * via session_clear() — token wiped, Set-Cookie header set to expire
 * the cookie — and returns the (now empty) token as text, so @stmt can
 * blank the stored one in the same statement:
 *   UPDATE users SET token = delete_token() WHERE id = $USER;
 */
static void db_delete_token(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    (void)argv;
    if (argc != 0)
    {
        sqlite3_result_error(context, "delete_token() doesn't take arguments", -1);
        return;
    }
    sqlite3_result_text(context, session_clear(session), -1, SQLITE_STATIC);
}

/**
 * Custom SQL function: new_password(). Returns a random 8-character
 * password as text (rand_password()), for @stmt to store or hand back
 * to the caller. Not wired into any @stmt yet.
 */
static void db_new_password(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    (void)argv;
    if (argc != 0)
    {
        sqlite3_result_error(context, "new_password() doesn't take arguments", -1);
        return;
    }

    char password[9];

    if (!rand_password(password, sizeof password))
    {
        sqlite3_result_error(context, "new_password() failed", -1);
        return;
    }
    sqlite3_result_text(context, password, -1, SQLITE_TRANSIENT);
}

#define db_create_function(func, name, argc)                        \
    do                                                              \
    {                                                               \
        if (SQLITE_OK != sqlite3_create_function(                   \
            db, name, argc, SQLITE_UTF8, NULL, func, NULL, NULL))   \
        {                                                           \
            fprintf(stderr, "%s\n", sqlite3_errmsg(db));            \
            return 0;                                               \
        }                                                           \
    } while (0)

static int db_create_functions(void)
{
    db_create_function(db_assert, "assert", 2);
    db_create_function(db_new_token, "new_token", 4);
    db_create_function(db_delete_token, "delete_token", 0);
    db_create_function(db_new_password, "new_password", 0);
    return 1;
}

static struct { sqlite3_stmt **stmt; size_t size, room; } statements;

/**
 * Called once per endpoint by db_create_statement(), itself called once
 * per endpoint via router_walk() (see db_create_statements()), not per
 * request. A single @stmt can hold several ';'-separated SQL statements:
 * sqlite3_prepare_v2()'s last argument advances 'sql' past whatever it
 * just compiled, so the while loop keeps preparing until nothing is
 * left. Each prepared statement is appended to the 'statements' pool
 * once and reused (reset, never finalized) on every future request;
 * 'statement->offset'/'size' record where this endpoint's own statements
 * landed in that pool. 'mode' becomes STATEMENT_MODE_WRITE as soon as any
 * one of them isn't read-only, so handle_statement() knows whether to
 * wrap the group in a transaction.
 */
static int create_statement(statement_t *statement)
{
    const char *sql = statement->sql;

    while (sql && *sql)
    {
        sqlite3_stmt *stmt = NULL;
        int prepared = sqlite3_prepare_v2(db, sql, -1, &stmt, &sql);

        if ((prepared == SQLITE_OK) && (stmt == NULL))
        {
            continue;
        }
        if (prepared != SQLITE_OK)
        {
            fprintf(stderr, "%s\n", sqlite3_errmsg(db));
            return 0;
        }
        if (statements.size == statements.room)
        {
            size_t room = statements.room ? statements.room * 2 : 1;
            sqlite3_stmt **temp = realloc(statements.stmt, sizeof(*temp) * room);

            if (temp == NULL)
            {
                sqlite3_finalize(stmt);
                return 0;
            }
            statements.stmt = temp;
            statements.room = room;
        }
        if (statement->offset == 0)
        {
            statement->offset = statements.size;
        }
        statement->size++;
        statement->mode |= !sqlite3_stmt_readonly(stmt);
        statements.stmt[statements.size++] = stmt;
    }
    return 1;
}

static int db_create_statement(endpoint_t *endpoint)
{
    if (!create_statement(&endpoint->statement))
    {
        fprintf(stderr, "Can't create '%s' statement\n", endpoint->path);
        return 0;
    }
    return 1;
}

static int db_create_statements(void)
{
    return router_walk(db_create_statement);
}

static int db_load(const char *metadata)
{
    char *error = NULL;

    if (sqlite3_exec(db, metadata, NULL, NULL, &error) != SQLITE_OK)
    {
        fprintf(stderr, "metadata:\n%s\n", error);
        sqlite3_free(error);
        return 0;
    }
    sqlite3_update_hook(db, db_on_change, NULL);
    return db_create_functions() && db_create_statements();
}

static void db_unload(void)
{
    for (size_t i = 0; i < statements.size; i++)
    {
        sqlite3_finalize(statements.stmt[i]);
    }
    free(statements.stmt);
    statements.stmt = NULL;
    statements.size = 0;
    statements.room = 0;
    sqlite3_close(db);
}

static void load(void)
{
    const char *path_db = "storage/app.db";

    printf("Loading '%s'\n", path_db);
    if (sqlite3_open(path_db, &db))
    {
        fprintf(stderr, "%s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    
    const char *path_sql = "storage/app.sql";

    printf("Loading '%s'\n", path_sql);

    char *metadata = file_read(path_sql);

    if (metadata == NULL)
    {
        perror("file_read");
        exit(EXIT_FAILURE);
    } 
    if (!db_load(metadata))
    {
        free(metadata);
        exit(EXIT_FAILURE);
    }
    free(metadata);

    const endpoint_t *endpoint = router_search("GET /api/session", 0);

    if ((endpoint == NULL) || (endpoint->statement.size == 0))
    {
        fprintf(stderr, "'GET /api/session' statement not found\n");
        exit(EXIT_FAILURE);
    }
    auth = statements.stmt[endpoint->statement.offset];
}

static void unload(void)
{
    buffer_clear(&buffer);
    db_unload();
}

void solver_load(void)
{
    atexit(unload);
    load();
}

void solver_reload(void)
{
    unload();
    load();
}

static int handle_session(session_t *user_session)
{
    session = user_session;
    if (session->role == 0)
    {
        return 1;
    }

    int step, authorized = 0;

    if ((sqlite3_bind_int(auth, 1, session->user) != SQLITE_OK) ||
        (sqlite3_bind_int(auth, 2, session->role) != SQLITE_OK) ||
        (sqlite3_bind_text(auth, 3, session->token, -1, SQLITE_STATIC) != SQLITE_OK))
    {
        goto done;
    }
    while ((step = sqlite3_step(auth)) == SQLITE_ROW)
    {
        authorized = sqlite3_column_int(auth, 0);
    }
    if (step != SQLITE_DONE)
    {
        authorized = 0;
    }
done:
    sqlite3_reset(auth);
    return authorized;
}

static char allow[128];

static void write_allow(unsigned methods)
{
    static const char *list[] = { "GET", "POST", "PUT", "PATCH", "DELETE" };
    size_t offset = 0;

    for (unsigned count = 0, i = 0; i < sizeof list / sizeof list[0]; i++)
    {
        if (methods & (1u << i))
        {
            int length = snprintf(allow + offset, sizeof allow - offset,
                count++ == 0 ? "%s" : ", %s", list[i]);

            offset += (size_t)length;
        }
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
static void write_error(const char *title, const char *issue)
{
    const json_t message =
    {
        .child = (json_t [])
        {
            { .key = "title", .string = (char *)title, .type = JSON_STRING },
            { .key = "issue", .string = (char *)issue, .type = JSON_STRING }
        },
        .type = JSON_OBJECT,
        .size = 2
    };

    json_buffer_encode(&buffer, &message, 2);
}

static void write_fault(const char *title, const json_t *node)
{
    const json_t message =
    {
        .child = (json_t [])
        {
            { .key = "title", .string = (char *)title, .type = JSON_STRING },
            { .key = "issue", .child = node->child, .type = node->type, .size = node->size }
        },
        .type = JSON_OBJECT,
        .size = 2
    };

    json_buffer_encode(&buffer, &message, 2);
}
#pragma GCC diagnostic pop

/**
 * Three independent binding conventions feed one sqlite3_stmt:
 * @name  <- request.params (query string, always text)
 * :name  <- request.content, when it's a JSON object (by field name)
 * $N     <- request.content, when it's a JSON array (by 1-based index)
 * $USER / $ROLE <- the session (see bind_session()), not the request
 * A placeholder that isn't present in the statement is simply skipped
 * (sqlite3_bind_parameter_index() returns 0), so @stmt only needs to
 * name what it actually uses.
 */
static int bind_params(sqlite3_stmt *stmt, const json_t *params)
{
    for (unsigned i = 0; i < params->size; i++)
    {
        const json_t *child = &params->child[i];
        char key[128];
        int index;

        snprintf(key, sizeof key, "@%s", child->key);
        if ((index = sqlite3_bind_parameter_index(stmt, key)) == 0)
        {
            continue;
        }

        int status = sqlite3_bind_text(stmt, index, child->string, -1, SQLITE_STATIC);

        if (status != SQLITE_OK)
        {
            return 0;
        }
    }
    return 1;
}

static int bind_child(sqlite3_stmt *stmt, int index, const json_t *child)
{
    int rc;

    switch (child->type)
    {
        case JSON_STRING:
            rc = sqlite3_bind_text(stmt, index, child->string, -1, SQLITE_STATIC);
            break;
        case JSON_INTEGER:
            rc = sqlite3_bind_int64(stmt, index, (int64_t)child->number);
            break;
        case JSON_REAL:
            rc = sqlite3_bind_double(stmt, index, child->number);
            break;
        case JSON_TRUE:
            rc = sqlite3_bind_int(stmt, index, 1);
            break;
        case JSON_FALSE:
            rc = sqlite3_bind_int(stmt, index, 0);
            break;
        case JSON_NULL:
            rc = sqlite3_bind_null(stmt, index);
            break;
        default:
            rc = SQLITE_ERROR;
            break;
    }
    return rc == SQLITE_OK;
}

static int bind_content_as_object(sqlite3_stmt *stmt, const json_t *content)
{
    for (unsigned i = 0; i < content->size; i++)
    {
        char key[128];
        int index;

        snprintf(key, sizeof key, ":%s", content->child[i].key);
        if ((index = sqlite3_bind_parameter_index(stmt, key)))
        {
            if (!bind_child(stmt, index, &content->child[i]))
            {
                return 0;
            }
        }
    }
    return 1;
}

static int bind_content_as_array(sqlite3_stmt *stmt, const json_t *content)
{
    for (unsigned i = 0; i < content->size; i++)
    {
        char key[16];
        int index;

        snprintf(key, sizeof key, "$%u", i + 1);
        if ((index = sqlite3_bind_parameter_index(stmt, key)))
        {
            if (!bind_child(stmt, index, &content->child[i]))
            {
                return 0;
            }
        }
    }
    return 1;
}

static int bind_content(sqlite3_stmt *stmt, const json_t *content)
{
    switch (content->type)
    {
        case JSON_OBJECT:
            return bind_content_as_object(stmt, content);
        case JSON_ARRAY:
            return bind_content_as_array(stmt, content);
        default:
            return 1;
    }
}

static int bind_session(sqlite3_stmt *stmt)
{
    int index;

    if ((index = sqlite3_bind_parameter_index(stmt, "$USER")))
    {
        if (sqlite3_bind_int(stmt, index, session->user) != SQLITE_OK)
        {
            return 0;
        }
    }
    if ((index = sqlite3_bind_parameter_index(stmt, "$ROLE")))
    {
        if (sqlite3_bind_int(stmt, index, session->role) != SQLITE_OK)
        {
            return 0;
        }
    }
    return 1;
}

/**
 * 'statement->offset'/'size' index into the 'statements' pool, already
 * prepared once at load time by db_set_statement() — this only binds,
 * steps and resets them, no prepare/finalize per request. Only the
 * first column of any SELECT rows is collected, as raw text, straight
 * into 'buffer' — endpoints.sql uses SQLite's json_array()/
 * json_group_array() so that text already is the JSON response body,
 * no separate encoding step needed here. 'db_last_op' (set by
 * db_on_change(), the sqlite3_update_hook callback) records the last
 * real INSERT/UPDATE/DELETE among the group; together with
 * sqlite3_total_changes() it tells a write that changed nothing apart
 * from a real "nothing found" (see the caller), and decides whether
 * to answer 201 Created (last write was an INSERT) or 200 OK.
 */
static int handle_statement(const json_t *request, const statement_t *statement)
{
    int total_changes = sqlite3_total_changes(db);
    sqlite3_stmt *stmt = NULL;
    int error_status = 0;

    if (statement->mode == STATEMENT_MODE_WRITE)
    {
        db_exec("BEGIN TRANSACTION;");
    }
    for (size_t i = 0; i < statement->size; i++)
    {
        stmt = statements.stmt[statement->offset + i];
        if (!bind_params(stmt, json_find(request, "params")) ||
            !bind_content(stmt, json_find(request, "content")) ||
            !bind_session(stmt))
        {
            buffer_reset(&buffer);
            write_error("Internal Server Error", sqlite3_errmsg(db));
            error_status = HTTP_INTERNAL_SERVER_ERROR;
            goto error;
        }
        db_command = 0;

        int step;

        while ((step = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const char *text = (const char *)sqlite3_column_text(stmt, 0);

            if (text != NULL)
            {
                buffer_write(&buffer, text);
            }
        }
        if (step != SQLITE_DONE)
        {
            buffer_reset(&buffer);
            write_error("Bad Request", sqlite3_errmsg(db));
            error_status = HTTP_BAD_REQUEST;
            goto error;
        }
        sqlite3_reset(stmt);
    }
    if (statement->mode == STATEMENT_MODE_WRITE)
    {
        db_exec("COMMIT;");
    }
    if ((buffer.length == 0) &&
       ((db_command == 0) || (sqlite3_total_changes(db) - total_changes == 0)))
    {
        write_error("Not Found", "Resource not found");
        return HTTP_NOT_FOUND;
    }
    return db_command == SQLITE_INSERT ? HTTP_CREATED : HTTP_OK;
error:
    if (statement->mode == STATEMENT_MODE_WRITE)
    {
        db_exec("ROLLBACK;");
    }
    sqlite3_reset(stmt);
    return error_status;
}

static int handle_task(const char *path)
{
    if (!strcmp(path, "GET /api/auth"))
    {
        return HTTP_OK;
    }
    if (!strcmp(path, "POST /api/backup"))
    {
        file_delete("storage/backup.db");
        db_exec("VACUUM INTO 'storage/backup.db';");
        return HTTP_OK;
    }
    if (!strcmp(path, "POST /api/vacuum"))
    {
        db_exec("VACUUM;");
        return HTTP_OK;
    }
    if (!strcmp(path, "POST /api/reload"))
    {
        loader_reload();
        return HTTP_OK;
    }
    if (!strcmp(path, "POST /api/stop"))
    {
        raise(SIGINT);
        return HTTP_OK;
    }
    write_error("Internal Server Error", "Unhandled task");
    return HTTP_INTERNAL_SERVER_ERROR;
}

typedef struct { const endpoint_t *endpoint; int *status; } context_t;

/**
 * json_validate()'s error callback, and the piece that implements the
 * "same path, several @eval variants" overload described in router.c.
 * A validation failure under /params means this variant's schema
 * just doesn't match the request; try the next endpoint sharing the
 * same path (router_search(path, index + 1)) instead of giving up.
 * A failure anywhere else (a bad /session, or the request body itself)
 * is a real error and is reported as such.
 */
static void on_validate_request(const json_t *node, void *data)
{
    const char *path = json_text(json_find(node, "path"));
    context_t *context = data;

    if (!strncmp(path, "/params", 7))
    {
        int index = context->endpoint->index;

        context->endpoint = router_search(context->endpoint->path, index + 1);
        if (context->endpoint != NULL)
        {
            return;
        }
        if (index > 0)
        {
            write_error("Bad Request", "Missing or invalid parameters");
        }
        else
        {
            write_fault("Bad Request", node);
        }
    }
    else if (!strncmp(path, "/session", 8))
    {
        context->endpoint = NULL;
        *context->status = HTTP_FORBIDDEN;
        write_error("Forbidden", "Access denied");
    }
    else
    {
        context->endpoint = NULL;
        write_fault("Bad request", node);
    }
}

static const endpoint_t *validate_request(const json_t *request,
    const endpoint_t *endpoint, int *status)
{
    context_t context = { endpoint, status };
    const void *schema = endpoint->schema;

    while (!json_validate(request, schema, on_validate_request, &context))
    {
        if (context.endpoint == NULL)
        {
            return NULL;
        }
        schema = context.endpoint->schema;
    }
    return context.endpoint;
}

static const buffer_t *solve_request(int status)
{
    char headers[512];

#define write_headers(header) \
    snprintf(headers, sizeof headers, header "%s" \
        "Content-Type: application/json\r\n" \
        "Content-Length: %zu\r\n\r\n", \
        session->cookie, buffer.length)
#define write_headers_with_allow(header) \
    snprintf(headers, sizeof headers, header "%s" \
        "Allow: %s\r\n" \
        "Content-Type: application/json\r\n" \
        "Content-Length: %zu\r\n\r\n", \
        session->cookie, allow, buffer.length)
#define write_headers_no_content(header) \
    snprintf(headers, sizeof headers, header "%s\r\n", \
        session->cookie)
 
    switch (status)
    {
        case HTTP_OK:
            buffer.length > 0
                ? write_headers(HEADER_OK)
                : write_headers_no_content(HEADER_NO_CONTENT);
            break;
        case HTTP_CREATED:
            buffer.length > 0
                ? write_headers(HEADER_CREATED)
                : write_headers_no_content(HEADER_NO_CONTENT);
            break;
        case HTTP_BAD_REQUEST:
            write_headers(HEADER_BAD_REQUEST);
            break;
        case HTTP_FORBIDDEN:
            write_headers(HEADER_FORBIDDEN);
            break;
        case HTTP_NOT_FOUND:
            write_headers(HEADER_NOT_FOUND);
            break;
        case HTTP_METHOD_NOT_ALLOWED:
            write_headers_with_allow(HEADER_METHOD_NOT_ALLOWED);
            break;
        case HTTP_INTERNAL_SERVER_ERROR:
            write_headers(HEADER_INTERNAL_SERVER_ERROR);
            break;
    }
    buffer_insert(&buffer, 0, headers, strlen(headers));
#ifdef DEBUG
    if (buffer.length > 0)
    {
        puts(buffer.text);
    }
#endif
    return buffer.length ? &buffer : static_internal_server_error();
}

const buffer_t *solver_handle(const json_t *request, const char *path,
    session_t *user_session)
{
    if (!handle_session(user_session))
    {
        return static_unauthorized();
    }
    buffer_reset(&buffer);

    const endpoint_t *endpoint = router_search(path, 0);

    if (endpoint == NULL)
    {
        unsigned allowed_methods = router_methods(path);

        if (allowed_methods != 0)
        {
            write_allow(allowed_methods);
            write_error("Method Not Allowed", "See the 'Allow' header");
            return solve_request(HTTP_METHOD_NOT_ALLOWED);
        }
        else
        {
            write_error("Not Found", "Endpoint not found");
            return solve_request(HTTP_NOT_FOUND);
        }
    }

    int status = HTTP_BAD_REQUEST;

    if (!(endpoint = validate_request(request, endpoint, &status)))
    {
        return solve_request(status);
    }
    return endpoint->statement.size
        ? solve_request(handle_statement(request, &endpoint->statement))
        : solve_request(handle_task(path));
}

