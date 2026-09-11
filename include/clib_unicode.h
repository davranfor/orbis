/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef CLIB_UNICODE_H
#define CLIB_UNICODE_H

#include <stddef.h>

#define is_lead(c) (((c) & 0xc0) != 0x80)

static inline int is_cntrl(int c)
{
    return (c >= 0) && (c <= 0x1f);
}

static inline int is_ascii(int c)
{
    return (c >= 0) && (c <= 0x7f);
}

static inline int is_digit(int c)
{
    return (c >= '0') && (c <= '9');
}

static inline int is_xdigit(int c)
{
    return ((c >= '0') && (c <= '9'))
        || ((c >= 'A') && (c <= 'F'))
        || ((c >= 'a') && (c <= 'f'));
}

static inline int is_alpha(int c)
{
    return ((c >= 'a') && (c <= 'z'))
        || ((c >= 'A') && (c <= 'Z'));
}

static inline int is_alnum(int c)
{
    return ((c >= 'a') && (c <= 'z'))
        || ((c >= 'A') && (c <= 'Z'))
        || ((c >= '0') && (c <= '9'));
}

static inline int is_print(int c)
{
    return (c >= 0x20) && (c <= 0x7e);
}

static inline int is_space(int c)
{
    return (c == ' ') || (c == '\n') || (c == '\r') || (c == '\t');
}

static inline int is_esc(const char *str)
{
    switch (*str)
    {
        case '\\':
        case '/' :
        case '"' :
        case 'b' :
        case 'f' :
        case 'n' :
        case 'r' :
        case 't' :
            return 1;
        default  :
            return 0;
    }
}

static inline char decode_esc(const char *str)
{
    switch (*str)
    {
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        default : return *str;
    }
}

static inline char encode_esc(const char *str)
{
    switch (*str)
    {
        case '\b': return 'b';
        case '\f': return 'f';
        case '\n': return 'n';
        case '\r': return 'r';
        case '\t': return 't';
        case '\"': return '"';
        case '\\': return '\\';
        default  : return '\0';
    }
}

static inline int is_hex(const char *str)
{
    return (('u') == str[0])
        && is_xdigit(str[1])
        && is_xdigit(str[2])
        && is_xdigit(str[3])
        && is_xdigit(str[4]);
}

static inline int hex_to_dec(int c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return c - '0';
    }
    if ((c >= 'A') && (c <= 'F'))
    {
        return c - 'A' + 10;
    }
    if ((c >= 'a') && (c <= 'f'))
    {
        return c - 'a' + 10;
    }
    return -1;
}

size_t decode_hex(const char *, char *);
size_t encode_hex(const char *, char *);

#endif

