/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef CLIB_TIME_H
#define CLIB_TIME_H

int is_leap(int);
int is_date(int, int, int);
int is_time(int, int, int);
int is_datetime(int, int, int, int, int, int);
int days_in_month(int, int);
int day_of_week(int, int, int);
int ISO_day_of_week(int, int, int);
int day_of_year(int, int, int);
int julian_day(int, int, int);
int week_of_month(int, int, int);
int week_of_year(int, int, int);
void date_from_julian_day(int, int *, int *, int *);
void date_now(int *, int *, int *);
void time_now(int *, int *, int *);
void datetime_now(int *, int *, int *, int *, int *, int *);
void datetime_utc(int *, int *, int *, int *, int *, int *);
long long unixtime_now(void);
long long unixtime_utc(void);
long long datetime_to_unixtime(int, int, int, int, int, int);
void unixtime_to_datetime(long long, int *, int *, int *, int *, int *, int *);

#endif

