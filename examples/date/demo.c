/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <orbis/clib_time.h>
#include <orbis/clib_match.h>

int main(int argc, char *argv[])
{
    int y, m, d;

    if (argc > 1)
    {
        char *str = argv[1];

        if (test_is_date(str)) // YYYY-MM-DD format is expected
        {
            y = (int)strtol(&str[0], NULL, 10);
            m = (int)strtol(&str[5], NULL, 10);
            d = (int)strtol(&str[8], NULL, 10);
        }
        else
        {
            fprintf(stderr, "'%s' is not a valid date\n", str);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        date_now(&y, &m, &d);
    }

    const char *dow[] =
    {
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
    };

    printf(" Input date: %04d-%02d-%02d\n", y, m, d);
    printf(" Julian day: %d\n", julian_day(y, m, d));
    printf("Day of week: %s\n", dow[day_of_week(y, m, d) - 1]);
    printf("Day of year: %d\n", day_of_year(y, m, d));
    return 0;
}

