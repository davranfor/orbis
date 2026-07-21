/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <orbis/clib_math.h>
#include <orbis/clib_stream.h>
#include <orbis/json_private.h>
#include <orbis/json_reader.h>
#include <orbis/json_buffer.h>
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
    HTTP_SERVER_ERROR = 500,
};

enum { GET = 1, POST, PUT, PATCH, DELETE };

static sqlite3 *db;
static sqlite3_stmt *auth;
static session_t *session;
static buffer_t buffer;

static int db_load(const char *metadata)
{
    char *error = NULL;

    if (sqlite3_exec(db, metadata, NULL, NULL, &error) != SQLITE_OK)
    {
        fprintf(stderr, "metadata:\n%s\n", error);
        sqlite3_free(error);
        return 0;
    }

    const endpoint_t *endpoint = router_search("GET /api/session", 0);

    if ((endpoint == NULL) || (endpoint->stmt == NULL))
    {
        fprintf(stderr, "'GET /api/session' statement not found\n");
        return 0;
    }
    if (sqlite3_prepare_v2(db, endpoint->stmt, -1, &auth, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "session: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

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

static void db_new_token(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc != 3)
    {
        sqlite3_result_error(context, "new_token() takes 3 arguments", -1);
        return;
    }

    int user = sqlite3_value_int(argv[0]);
    int role = sqlite3_value_int(argv[1]);
    const char *token = (const char *)sqlite3_value_text(argv[2]);
    const char *value = session_build(session, user, role, token);

    if (value == NULL)
    {
        sqlite3_result_error(context, "new_token() failed", -1);
        return;
    }
    sqlite3_result_text(context, value, -1, SQLITE_STATIC);
}

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

#define db_create_function(func, name, argc)                        \
    do                                                              \
    {                                                               \
        if (SQLITE_OK != sqlite3_create_function(                   \
            db, name, argc, SQLITE_UTF8, NULL, func, NULL, NULL))   \
        {                                                           \
            fprintf(stderr, "%s\n", sqlite3_errmsg(db));            \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while (0)

    db_create_function(db_assert, "assert", 2);
    db_create_function(db_new_token, "new_token", 3);
    db_create_function(db_delete_token, "delete_token", 0);
    db_create_function(db_new_password, "new_password", 0);
}

static void unload(void)
{
    buffer_clear(&buffer);
    sqlite3_finalize(auth);
    sqlite3_close(db);
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
#pragma GCC diagnostic pop

static void write_issue(const json_t *node)
{
    const json_t message =
    {
        .child = (json_t [])
        {
            { .key = "title", .string = "Bad Request", .type = JSON_STRING },
            { .key = "issue", .child = node->child, .type = node->type, .size = node->size }
        },
        .type = JSON_OBJECT,
        .size = 2
    };

    json_buffer_encode(&buffer, &message, 2);
}

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
        char key[8];
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

static int handle_stmt(const json_t *request, const char *path, const char *sql)
{
    int total_changes = sqlite3_total_changes(db);
    int method = router_method(path);

    if (method != GET)
    {
        db_exec("BEGIN TRANSACTION;");
    }

    sqlite3_stmt *stmt = NULL;
    int error_status;

    while (sql && *sql)
    {
        int prepared = sqlite3_prepare_v2(db, sql, -1, &stmt, &sql);

        if ((prepared == SQLITE_OK) && (stmt == NULL))
        {
            continue;
        }
        if ((prepared != SQLITE_OK) ||
            !bind_params(stmt, json_find(request, "params")) ||
            !bind_content(stmt, json_find(request, "content")) ||
            !bind_session(stmt))
        {
            buffer_reset(&buffer);
            write_error("Internal Server Error", sqlite3_errmsg(db));
            error_status = HTTP_SERVER_ERROR;
            goto error;
        }

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
        sqlite3_finalize(stmt);
    }
    if (method != GET)
    {
        db_exec("COMMIT;");
    }
    if (buffer.length == 0)
    {
        if ((method == GET) || (sqlite3_total_changes(db) - total_changes == 0))
        {
            write_error("Not Found", "Resource not found");
            return HTTP_NOT_FOUND;
        }
    }
    return method == POST ? HTTP_CREATED : HTTP_OK;
error:
    sqlite3_finalize(stmt);
    if (method != GET)
    {
        db_exec("ROLLBACK;");
    }
    return error_status;
}

static int handle_path(const json_t *request, const char *path)
{
    if (!strcmp(path, "GET /api/auth"))
    {
        return HTTP_OK;
    }
    if (!strcmp(path, "GET /api/exec") || !strcmp(path, "POST /api/exec"))
    {
        return handle_stmt(request, path, json_text(json_find(request, "content")));
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
    return HTTP_SERVER_ERROR;
}

typedef struct
{
    const endpoint_t *endpoint;
    const char *path;
    int *status;
    int index;
} context_t;

static void on_validate_request(const json_t *node, void *data)
{
    const char *path = json_text(json_find(node, "path"));
    context_t *context = data;

    if (!strncmp(path, "/params", 7))
    {
        context->endpoint = router_search(context->path, ++context->index);
        if (context->endpoint != NULL)
        {
            return;
        }
        context->index > 1
            ? write_error("Bad Request", "Missing or invalid parameters")
            : write_issue(node);
    }
    else if (!strncmp(path, "/session", 8))
    {
        *context->status = HTTP_FORBIDDEN;
        context->endpoint = NULL;
        write_error("Forbidden", "Access denied");
    }
    else
    {
        context->endpoint = NULL;
        write_issue(node);
    }
}

static const endpoint_t *validate_request(const json_t *request,
    const char *path, int *status)
{
    const endpoint_t *endpoint = router_search(path, 0);

    if (endpoint == NULL)
    {
        write_error("Not Found", "Endpoint not found");
        *status = HTTP_NOT_FOUND;
        return NULL;
    }

    context_t context = { endpoint, path, status, 0 };
    const void *code = endpoint->code;

    while (!json_validate(request, code, on_validate_request, &context))
    {
        if (context.endpoint == NULL)
        {
            return NULL;
        }
        code = context.endpoint->code;
    }
    return context.endpoint;
}

static const buffer_t *solve_request(int status)
{
    char headers[512];

#define write_headers(response) \
    snprintf(headers, sizeof headers, response "%s" \
        "Content-Type: application/json\r\n" \
        "Content-Length: %zu\r\n\r\n", \
        session->cookie, buffer.length)
#define write_headers_no_content(response) \
    snprintf(headers, sizeof headers, response "%s\r\n", \
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
        case HTTP_SERVER_ERROR:
            write_headers(HEADER_SERVER_ERROR);
            break;
    }
    buffer_insert(&buffer, 0, headers, strlen(headers));
#ifdef DEBUG
    if (buffer.length > 0)
    {
        puts(buffer.text);
    }
#endif
    return buffer.length ? &buffer : static_server_error();
}

const buffer_t *solver_handle(const json_t *request, const char *path,
    session_t *user_session)
{
    if (!handle_session(user_session))
    {
        return static_unauthorized();
    }
    buffer_reset(&buffer);

    int status = HTTP_BAD_REQUEST;
    const endpoint_t *endpoint = validate_request(request, path, &status);

    if (endpoint == NULL)
    {
        return solve_request(status);
    }

    const char *stmt = endpoint->stmt;

    return stmt != NULL
        ? solve_request(handle_stmt(request, path, stmt))
        : solve_request(handle_path(request, path));
}

