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
#include <orbis/json_pointer.h>
#include <orbis/json_validator.h>
#include <sqlite3.h>
#include "headers.h"
#include "session.h"
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
static buffer_t buffer;
static const char *session_sql;

static int load_db(const char *metadata)
{
    char *err = NULL;

    if (sqlite3_exec(db, metadata, NULL, NULL, &err) != SQLITE_OK)
    {
        fprintf(stderr, "metadata:\n%s\n", err);
        sqlite3_free(err);
        return 0;
    }

    const endpoint_t *endpoint = router_search("GET /api/session", 0);

    if (endpoint == NULL)
    {
        fprintf(stderr, "'GET /api/session' is not found\n");
        return 0;
    }
    session_sql = endpoint->stmt;
    return 1;
}

static void db_exec(const char *sql)
{
    char *error;

    if (sqlite3_exec(db, sql, NULL, NULL, &error) != SQLITE_OK)
    {
        fprintf(stderr, "%s\n", error);
        sqlite3_free(error);
        exit(EXIT_FAILURE);
    }
}

static void new_token(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc != 4)
    {
        sqlite3_result_error(context, "new_token() takes 4 arguments", -1);
        return;
    }

    int user = sqlite3_value_int(argv[0]);
    int role = sqlite3_value_int(argv[1]);
    const unsigned char *token = sqlite3_value_text(argv[2]);
    char *session = sqlite3_value_pointer(argv[3], "$SESSION");

    if (!session_create(user, role, (const char *)token, session))
    {
        sqlite3_result_error(context, "new_token() failed", -1);
        return;
    }
    sqlite3_result_text(context, strrchr(session, ':') + 1, -1, SQLITE_STATIC);
}

static void new_password(sqlite3_context *context, int argc, sqlite3_value **argv)
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
    if (!load_db(metadata))
    {
        free(metadata);
        exit(EXIT_FAILURE);
    }
    free(metadata);

    int status;

    status = sqlite3_create_function(
        db, "new_token", 4, SQLITE_UTF8, NULL, new_token, NULL, NULL
    );
    if (status != SQLITE_OK)
    {
        fprintf(stderr, "%s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    status = sqlite3_create_function(
        db, "new_password", 0, SQLITE_UTF8, NULL, new_password, NULL, NULL
    );
    if (status != SQLITE_OK)
    {
        fprintf(stderr, "%s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
}

static void unload(void)
{
    buffer_clear(&buffer);
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

static int verify_request(const json_t *request)
{
    const json_t *session = json_find(request, "session");
    int user = (int)session->child[SESSION_USER].number;
    int role = (int)session->child[SESSION_ROLE].number;
    const char *token = session->child[SESSION_TOKEN].string;

    if (role == 0)
    {
        return 1;
    }

    sqlite3_stmt *stmt;

    if ((sqlite3_prepare_v2(db, session_sql, -1, &stmt, NULL) != SQLITE_OK) ||
        (sqlite3_bind_int(stmt, 1, user) != SQLITE_OK) ||
        (sqlite3_bind_int(stmt, 2, role) != SQLITE_OK) ||
        (sqlite3_bind_text(stmt, 3, token, -1, SQLITE_STATIC) != SQLITE_OK))
    {
        goto error;
    }

    int step, verified = 0;

    while ((step = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        verified = sqlite3_column_int(stmt, 0);
    }
    if (step != SQLITE_DONE)
    {
        goto error;
    }
    sqlite3_finalize(stmt);
    return verified;
error:
    sqlite3_finalize(stmt);
    return 0;
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
        if (!(index = sqlite3_bind_parameter_index(stmt, key)))
        {
            continue;
        }
        if (!bind_child(stmt, index, &content->child[i]))
        {
            return 0;
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
        if ((index = sqlite3_bind_parameter_index(stmt, key)) == 0)
        {
            continue;
        }
        if (!bind_child(stmt, index, &content->child[i]))
        {
            return 0;
        }
    }
    return 1;
}

static int bind_content(sqlite3_stmt *stmt, const json_t *content)
{
    switch (content->type)
    {
        case JSON_OBJECT: return bind_content_as_object(stmt, content);
        case JSON_ARRAY: return bind_content_as_array(stmt, content);
        default: return 1;
    }
}

static int bind_session(sqlite3_stmt *stmt, const json_t *session)
{
    int index, status;

    if ((index = sqlite3_bind_parameter_index(stmt, "$USER")) != 0)
    {
        int user = (int)session->child[SESSION_USER].number;

        status = sqlite3_bind_int(stmt, index, user);
        if (status != SQLITE_OK)
        {
            return 0;
        }
    }
    if ((index = sqlite3_bind_parameter_index(stmt, "$ROLE")) != 0)
    {
        int role = (int)session->child[SESSION_ROLE].number;

        status = sqlite3_bind_int(stmt, index, role);
        if (status != SQLITE_OK)
        {
            return 0;
        }
    }
    if ((index = sqlite3_bind_parameter_index(stmt, "$TOKEN")) != 0)
    {
        char *value = session->child[SESSION_TOKEN].string;

        status = sqlite3_bind_text(stmt, index, value, -1, SQLITE_STATIC);
        if (status != SQLITE_OK)
        {
            return 0;
        }
    }
    if ((index = sqlite3_bind_parameter_index(stmt, "$SESSION")) != 0)
    {
        char *value = session->child[SESSION_VALUE].string;

        status = sqlite3_bind_pointer(stmt, index, value, "$SESSION", NULL);
        if (status != SQLITE_OK)
        {
            return 0;
        }
    }
    return 1;
}

static int handle_stmt(const json_t *request, const char *sql)
{
    int total_changes = sqlite3_total_changes(db);
    int method = router_method(json_find(request, "path")->string);

    if (method != GET)
    {
        db_exec("BEGIN TRANSACTION;");
    }

    sqlite3_stmt *stmt = NULL;
    int error_status = 0;

    while (sql && *sql)
    {
        if ((sqlite3_prepare_v2(db, sql, -1, &stmt, &sql) != SQLITE_OK) ||
            !bind_params(stmt, json_find(request, "params")) ||
            !bind_content(stmt, json_find(request, "content")) ||
            !bind_session(stmt, json_find(request, "session")))
        {
            buffer_reset(&buffer);
            write_error("Internal Server Error", sqlite3_errmsg(db));
            error_status = HTTP_SERVER_ERROR;
            goto error;
        }

        int step;

        while ((step = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            const unsigned char *text = sqlite3_column_text(stmt, 0);

            if (text != NULL)
            {
                buffer_write(&buffer, (const char *)text);
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
        stmt = NULL;
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

static int handle_task(const json_t *request)
{
    const char *path = json_string(json_find(request, "path"));

    if (!strcmp(path, "POST /api/exec"))
    {
        return handle_stmt(request, json_text(json_find(request, "content")));
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
    int *status;
    char *path;
    int index;
} context_t;

static void on_validate_request(const json_t *node, void *data)
{
    const char *path = json_text(json_pointer(node, "/path"));
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

static const endpoint_t *validate_request(const json_t *request, int *status)
{
    char *path = json_string(json_find(request, "path")); 
    const endpoint_t *endpoint = router_search(path, 0);

    if (endpoint == NULL)
    {
        write_error("Not Found", "Endpoint not found");
        *status = HTTP_NOT_FOUND;
        return NULL;
    }

    context_t context = { endpoint, status, path, 0 };
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

static const buffer_t *solve_request(const json_t *request, int status)
{
    const char *session = json_pointer(request, "/session/value")->string;
    char cookie[256] = "";

    if (session[0] != '\0')
    {
#ifdef ALLOW_INSECURE_TOKEN
        /**
         * For testing purposes where you can not provide an SSL connection:
         * Some browsers (i.e. Safari) doesn't send a Secure token on non-https connections even
         * for testing with localhost (https requires 'Secure;')
         * You can set an environment variable on .zshrc or .bashrc:
         * export ALLOW_INSECURE_TOKEN=1
         * Then, inside the Makefile, there is a rule to add a preprocessor flag:
         * ifdef ALLOW_INSECURE_TOKEN
         * CFLAGS += -DALLOW_INSECURE_TOKEN
         * endif
         * Depending on this flag, the 'Secure;' flag is sent or not to the client.
         * Max-Age = 1 year
         */
        snprintf(cookie, sizeof cookie,
            "Set-Cookie: session=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=31536000\r\n",
            session);
#else
        snprintf(cookie, sizeof cookie,
            "Set-Cookie: session=%s; Path=/; Secure; HttpOnly; SameSite=Strict; Max-Age=31536000\r\n",
            session);
#endif
    }

#define write_headers(response) \
    snprintf(headers, sizeof headers, response "%s" \
        "Content-Type: application/json\r\n" \
        "Content-Length: %zu\r\n\r\n", \
        cookie, buffer.length)
#define write_headers_no_content(response) \
    snprintf(headers, sizeof headers, response "%s\r\n", cookie)
 
    char headers[1024];

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
    if (buffer.length)
    {
        puts(buffer.text);
    }
#endif
    return buffer.length ? &buffer : static_server_error();
}

const buffer_t *solver_handle(const json_t *request)
{
    if (!verify_request(request))
    {
        return static_unauthorized();
    }
    buffer_reset(&buffer);

    int status = HTTP_BAD_REQUEST;
    const endpoint_t *endpoint = validate_request(request, &status);

    if (endpoint == NULL)
    {
        return solve_request(request, status);
    }

    const char *stmt = endpoint->stmt;

    return stmt != NULL
        ? solve_request(request, handle_stmt(request, stmt))
        : solve_request(request, handle_task(request));
}

