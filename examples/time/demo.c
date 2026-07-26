#include <stdio.h>
#include <orbis/clib_time.h>

int main(void)
{
    datetime_t dt_lt = datetime_now(); // Local time
    datetime_t dt_gm = datetime_utc(); // UTC time

    for (int seconds = 0; seconds < 3600 * 24; seconds += 3600)
    {
        int64_t ut_lt = datetime_to_unixtime(dt_lt) + seconds;
        int64_t ut_gm = datetime_to_unixtime(dt_gm) + seconds;
        datetime_t lt = unixtime_to_datetime(ut_lt);
        datetime_t gm = unixtime_to_datetime(ut_gm);

        printf("Local time: %04d-%02d-%02d %02d:%02d:%02d\n",
            lt.year, lt.month, lt.day, lt.hour, lt.minutes, lt.seconds
        );
        printf("  UTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
            gm.year, gm.month, gm.day, gm.hour, gm.minutes, gm.seconds
        );
    }
    return 0;
}

