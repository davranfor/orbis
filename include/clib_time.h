/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef CLIB_TIME_H
#define CLIB_TIME_H

#include <stdint.h>

typedef struct { int year, month, day, hour, minutes, seconds; } datetime_t;

int is_leap(int);
int is_date(int, int, int);
int julian_day(int, int, int);
int days_in_month(int, int);
int day_of_week(int, int, int);
int day_of_year(int, int, int);
int week_of_month(int, int, int);
int week_of_year(int, int, int);
void date_now(int *, int *, int *);
void date_from_julian_day(int, int *, int *, int *);
datetime_t datetime_now(void);
datetime_t datetime_utc(void);
int64_t unixtime_now(void);
int64_t unixtime_utc(void);
int64_t datetime_to_unixtime(datetime_t);
datetime_t unixtime_to_datetime(int64_t);

#endif

