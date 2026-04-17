/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef SEXP_HEADER_H
#define SEXP_HEADER_H

#define SEXP_MAX_DEPTH 32

enum
{
    SEXP_UNDEFINED = 0,
    SEXP_SYMBOL = 1,
    SEXP_STRING = 2,
    SEXP_INTEGER = 4,
    SEXP_REAL = 8,
    SEXP_TRUE = 16,
    SEXP_FALSE = 32,
    SEXP_NULL = 64,
    SEXP_SYMBOL_END = 128,
};

enum
{
    SEXP_NUMBER = SEXP_INTEGER | SEXP_REAL,
    SEXP_BOOLEAN = SEXP_TRUE | SEXP_FALSE,
    SEXP_SCALAR = SEXP_STRING | SEXP_NUMBER | SEXP_BOOLEAN | SEXP_NULL,
};

#endif

