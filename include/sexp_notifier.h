/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef SEXP_NOTIFIER_H
#define SEXP_NOTIFIER_H

typedef void (*sexp_bind_callback)(const void *, void *);

void *sexp_compile(char *);
int sexp_bind(const void *, sexp_bind_callback, void *);

#endif

