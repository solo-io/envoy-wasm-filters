#include <string.h>
#include <time.h>

static struct tm time_buffer;

long daysInYear(long year) {

    if (year % 4 == 0) {
        // year devides in 4, check with 100 and 400:
        if ((year % 400 == 0)  ) {
            return 366;
        }
        if ((year % 100 == 0)  ) {
            return 365;
        }
        return 366;
    }

    return 365;
}

long daysInMonth(long month ,long year) {
    constexpr long daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    long daysInThisMonth = daysInMonth[month];
    if (month == 1) {
        // 1 is februrary
        if (daysInYear(year) == 366) {
            daysInThisMonth++;
        }
    }
    return daysInThisMonth;
}

extern "C" struct tm * gmtime (const time_t * timer){
    if (timer == NULL) {
        return NULL;
    }
    long deltat = *timer  ;
    if (deltat < 0) {
        return NULL;
    }
    memset(&time_buffer, 0, sizeof(time_buffer));
    // poor's man gmttime.
    // this is only used after 2019 and won't be used for anything high-resolution.
    // so we can make a few assumptions to make our life easy
    
  time_buffer.tm_sec = deltat % 60;			/* Seconds.	[0-60] (1 leap second) */
  deltat /= 60;
  time_buffer.tm_min = deltat % 60;			/* Minutes.	[0-59] */
  deltat /= 60;
  time_buffer.tm_hour = deltat % 24;			/* Hours.	[0-23] */
  deltat /= 24;

  // remove days till we get to now
  long year;
  for (year = 1970; deltat >= daysInYear(year); year++) {
    deltat -= daysInYear(year);
  }
  time_buffer.tm_year = year-1900; /* Year	- 1900.  */

    // deltat now has the day in the year, lets find the month
    long month;
    for (month = 0; month < 12; month++) {
        long daysInThisMonth = daysInMonth(month, time_buffer.tm_year);
        if (deltat < daysInThisMonth) {
            break;
        }
        deltat -= daysInThisMonth;
    }
  time_buffer.tm_mon = month;			/* Month.	[0-11] */
  time_buffer.tm_mday = deltat + 1; /* Day.		[1-31] */

// this should be good enough for signature
    return &time_buffer;
}