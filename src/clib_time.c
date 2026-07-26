/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#define _POSIX_C_SOURCE 200809L

#include <time.h>
#include "clib_time.h"

static int leap_count(int year, int month)
{
    if (month <= 2)
    {
        year--;
    }
    return year / 4 - year / 100 + year / 400;
}

int is_leap(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}
 
int is_date(int year, int month, int day)
{
    return (year >= 1) && (year <= 9999) &&
           (month >= 1) && (month <= 12) &&
           (day >= 1) && (day <= days_in_month(year, month));
}

int days_in_month(int year, int month)
{
    static const int days[2][12] =
    {
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
        { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
    };

    return days[is_leap(year)][month - 1];
}

/**
 * Tomohiko Sakamoto's Algorithm
 * Sunday = 0 ... Saturday = 6
 */
int day_of_week(int year, int month, int day)
{
    static const int offset[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

    year -= month < 3;
    return (year + year / 4 - year / 100 + year / 400 + offset[month - 1] + day) % 7;
}

/**
 * ISO 8601 date and time standard
 * Monday = 1 ... Sunday = 7
 */
int ISO_day_of_week(int year, int month, int day)
{
    static const int offset[] = { 6, 2, 1, 4, 6, 2, 4, 0, 3, 5, 1, 3 };

    year -= month < 3;
    return (year + year / 4 - year / 100 + year / 400 + offset[month - 1] + day) % 7 + 1;
}

int day_of_year(int year, int month, int day)
{
    static const int days[2][12] =
    {
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
        { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
    };

    return days[is_leap(year)][month - 1] + day;
}

int julian_day(int year, int month, int day)
{
    static const int days[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    return year * 365 + day + days[month - 1] + leap_count(year, month) + 1721060;
}

int week_of_month(int year, int month, int day)
{
    return (day - ISO_day_of_week(year, month, day) + 10) / 7;
}

int week_of_year(int year, int month, int day)
{
    return (day_of_year(year, month, day) - ISO_day_of_week(year, month, day) + 10) / 7;
}

/**
 * Inverse of julian_day()
 * Fliegel & Van Flandern algorithm (proleptic Gregorian calendar)
 */
void date_from_julian_day(int jd, int *year, int *month, int *day)
{
    int a = jd + 32044;
    int b = ((4 * a) + 3) / 146097;
    int c = a - ((146097 * b) / 4);
    int d = ((4 * c) + 3) / 1461;
    int e = c - ((1461 * d) / 4);
    int m = ((5 * e) + 2) / 153;

    *day = e - (((153 * m) + 2) / 5) + 1;
    *month = m + 3 - (12 * (m / 10));
    *year = (100 * b) + d - 4800 + (m / 10);
}

void date_now(int *year, int *month, int *day)
{
    time_t t = time(NULL);
    struct tm tm;

    localtime_r(&t, &tm);
    *year = tm.tm_year + 1900;
    *month = tm.tm_mon + 1;
    *day = tm.tm_mday;
}

datetime_t datetime_now(void)
{
    time_t t = time(NULL);
    struct tm tm;

    localtime_r(&t, &tm);

    datetime_t dt;

    dt.year = tm.tm_year + 1900;
    dt.month = tm.tm_mon + 1;
    dt.day = tm.tm_mday;
    dt.hour = tm.tm_hour;
    dt.minutes = tm.tm_min;
    dt.seconds = tm.tm_sec;
    return dt;
}

datetime_t datetime_utc(void)
{
    time_t t = time(NULL);
    struct tm tm;

    gmtime_r(&t, &tm);

    datetime_t dt;

    dt.year = tm.tm_year + 1900;
    dt.month = tm.tm_mon + 1;
    dt.day = tm.tm_mday;
    dt.hour = tm.tm_hour;
    dt.minutes = tm.tm_min;
    dt.seconds = tm.tm_sec;
    return dt;
}

int64_t unixtime_now(void)
{
    return datetime_to_unixtime(datetime_now());
}

int64_t unixtime_utc(void)
{
    return (int64_t)time(NULL);
}

/**
 * Encodes a datetime as an absolute number of seconds since
 * julian_day 1970-01-01 (JDN 2440588). Does NOT assume or convert
 * any timezone — if you pass local time, you get "local seconds";
 * if you pass UTC, you get "UTC seconds". Only meaningful when
 * compared against another value produced with the same encoding.
 */
int64_t datetime_to_unixtime(datetime_t dt)
{
    int64_t days = julian_day(dt.year, dt.month, dt.day) - 2440588;

    return days * 86400 + dt.hour * 3600 + dt.minutes * 60 + dt.seconds;
}

/**
 * Inverse of datetime_to_unixtime()
 * Encoding-agnostic: if ts was built from local time, this returns
 * local time; if built from UTC, this returns UTC.
 * Fixes truncation towards zero for negative ts (pre-1970 dates).
 */
datetime_t unixtime_to_datetime(int64_t ts)
{
    int64_t days = ts / 86400;
    int64_t secs = ts % 86400;

    if (secs < 0)
    {
        secs += 86400;
        days--;
    }

    datetime_t dt;

    date_from_julian_day((int)(days + 2440588), &dt.year, &dt.month, &dt.day);
    dt.hour = (int)(secs / 3600);
    dt.minutes = (int)((secs / 60) % 60);
    dt.seconds = (int)(secs % 60);
    return dt;
}

