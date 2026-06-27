/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "config.h"
#include "parser.h"
#include "server.h"

static struct { char text[BUFFER_SIZE]; size_t length; } buffer;
static volatile sig_atomic_t stop;

typedef struct pollfd conn_t;
typedef struct buffer pool_t;

static void signal_handle(int signum)
{
    (void)signum;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    write(STDERR_FILENO, "\nCaught SIGINT\n", 15);
#pragma GCC diagnostic pop
    stop = 1;
}

static void signal_connect(void)
{
    struct sigaction sa;

    sa.sa_handler = signal_handle;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

static int fd_set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int server_listen(uint16_t port)
{
    struct sockaddr_in6 server;

    memset(&server, 0, sizeof server);
    server.sin6_family = AF_INET6;
    server.sin6_port = htons(port);
    server.sin6_addr = in6addr_any;

    int fd, yes = 1, no = 0;

    if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof no) == -1)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    if (fd_set_nonblock(fd) == -1)
    {
        perror("fd_set_nonblock");
        exit(EXIT_FAILURE);
    }
    if (bind(fd, (struct sockaddr *)&server, sizeof server) == -1)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(fd, MAX_CLIENTS) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return fd;
}

static void pool_reset(pool_t *pool)
{
    buffer_reset(pool);
}

static void pool_clear(pool_t *pool)
{
    buffer_clear(pool);
}

static void conn_attach(conn_t *conn, int fd)
{
    conn->fd = fd;
    conn->events = POLLIN;
}

static void conn_reset(conn_t *conn)
{
    close(conn->fd);
    conn->fd = -1;
    conn->events = 0;
}

static int conn_recv(conn_t *conn, pool_t *pool)
{
    ssize_t bytes = recv(conn->fd, buffer.text, BUFFER_SIZE - 1, 0);

    if (bytes == 0)
    {
        return 0;
    }
    if (bytes == -1)
    {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        {
            return -1;
        }
        perror("recv");
        return 0;
    }
    buffer.length = (size_t)bytes;
    buffer.text[bytes] = '\0';

    int status = 1;

    if ((pool->length == 0)
    && ((status = parser_status(buffer.text, buffer.length)) == 1))
    {
        return status;
    }
    if (status != 0)
    {
        if (!buffer_append(pool, buffer.text, buffer.length))
        {
            perror("buffer_append");
            return 0;
        }
        if (status != -1)
        {
            status = parser_status(pool->text, pool->length);
        }
    }
    return status;
}

static int conn_send(conn_t *conn, pool_t *pool, const pool_t *message)
{
    ssize_t bytes = send(conn->fd, message->text, message->length, 0);

    if (bytes == 0)
    {
        return 0;
    }
    if (bytes == -1)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            perror("send");
            return 0;
        }
        bytes = 0;
    }

    size_t sent = (size_t)bytes;

    if (sent == message->length)
    {
        return 1;
    }
    if (pool != message)
    {
        if (!buffer_append(pool, message->text + sent, message->length - sent))
        {
            perror("buffer_append");
            return 0;
        }
    }
    else
    {
        buffer_delete(pool, 0, sent);
    }
    return -1;
}

static void conn_handle(conn_t *conn, pool_t *pool)
{
    if (!(conn->revents & ~(POLLIN | POLLOUT)))
    {
        const pool_t *message = pool;

        if (conn->revents == POLLIN)
        {
            switch (conn_recv(conn, pool))
            {
                case 0:
                    goto reset;
                case -1:
                    return;
                default:
                    message = pool->length
                        ? parser_handle(pool->text)
                        : parser_handle(buffer.text);
                    pool_reset(pool);
                    break;
            }
        }
        if (message != NULL)
        {
            switch (conn_send(conn, pool, message))
            {
                case 0:
                    goto reset;
                case -1:
                    conn->events |= POLLOUT;
                    return;
                default:
                    conn->events &= ~POLLOUT;
                    pool_reset(pool);
                    return;
            }
        }
    }
reset:
    conn_reset(conn);
    pool_reset(pool);
}

static void conn_close(conn_t *conn, pool_t *pool)
{
    conn_reset(conn);
    pool_clear(pool);
}

static void server_loop(uint16_t port)
{
    enum { server = 0, maxfds = MAX_CLIENTS + 1 };
    conn_t conn[maxfds] = { 0 };
    pool_t pool[maxfds] = { 0 };

    conn_attach(&conn[server], server_listen(port));
    for (nfds_t client = 1; client < maxfds; client++)
    {
        conn[client].fd = -1;
    }
    while (!stop)
    {
        if (poll(conn, maxfds, -1) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("poll");
            break;
        }
        if (conn[server].revents & POLLIN)
        {
            int fd = accept(conn[server].fd, NULL, NULL);

            if (fd == -1)
            {
                if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
                {
                    perror("accept");
                    break;
                }
            }
            else
            {
                int done = 0;

                for (nfds_t client = 1; client < maxfds; client++)
                {
                    if (conn[client].fd == -1)
                    {
                        if (fd_set_nonblock(fd) == -1)
                        {
                            perror("fd_set_nonblock");
                            break;
                        }
                        conn_attach(&conn[client], fd);
                        done = 1;
                        break;
                    }
                }
                if (done == 0)
                {
                    close(fd);
                }
            }
        }
        for (nfds_t client = 1; client < maxfds; client++)
        {
            if (conn[client].revents)
            {
                conn_handle(&conn[client], &pool[client]);
            }
        }
    }
    for (nfds_t fd = 0; fd < maxfds; fd++)
    {
        if (conn[fd].fd != -1)
        {
            conn_close(&conn[fd], &pool[fd]);
        }
    }
}

void server_run(uint16_t port)
{
    signal_connect();
    server_loop(port);
}

