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
void date_utc(int *, int *, int *);
void date_now(int *, int *, int *);
void time_utc(int *, int *, int *);
void time_now(int *, int *, int *);
void datetime_utc(int *, int *, int *, int *, int *, int *);
void datetime_now(int *, int *, int *, int *, int *, int *);
long long timestamp_utc(int, int, int, int, int, int);
long long timestamp_now(void);

#endif

