# include <OSUtils.h>
# include <Events.h>
# include "dgd.h"

static unsigned long timediff;
static unsigned long timeoffset;

/*
 * NAME:	tminit()
 * DESCRIPTION:	initialize the time package
 */
void tminit(void)
{
    static DateTimeRec ubirth = {
	1970, 1, 1, 0, 0, 0, 0
    };
    unsigned long t;

    DateToSeconds(&ubirth, &timediff);
    GetDateTime(&t);
    timeoffset = t - timediff - TickCount() / 60;
}

/*
 * NAME:	m2utime()
 * DESCRIPTION:	convert Mac time to Unix time, will only work with
 *		times up to 2040
 */
Uint m2utime(long t)
{
    return (Uint) (t - timediff);
}

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the current time in seconds since midnight Jan 1, 1970
 */
Uint P_time(void)
{
    return timeoffset + TickCount() / 60;
}

/*
 * NAME:	P->mtime()
 * DESCRIPTION:	return the current time in milliseconds
 */
Uint P_mtime(milli)
unsigned short *milli;
{
    long t;

    t = TickCount();
    *milli = t % 60 * 1667 / 100;
    return timeoffset + t / 60;
}

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	convert a time to a string
 */
char *P_ctime(char *buf, Uint t)
{
    static char *weekday[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static char *month[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    DateTimeRec date;
    int offset;

    for (offset = 0; t + timediff > 2147397248L; t -= 883612800L, offset += 28)
	;
    SecondsToDate((long) t + timediff, &date);
    if (offset != 0) {
	if (date.year + offset > 2100 ||
	    (date.year + offset == 2100 &&
	     (date.month > 2 || (date.month == 2 && date.day == 29)))) {
	    t -= 378604800L;
	    offset += 12;
	    SecondsToDate((long) t + timediff, &date);
	}
	date.year += offset;
    }

    sprintf(buf, "%s %s %2d %02d:%02d:%02d %d\012", /* LF */
	    weekday[date.dayOfWeek - 1], month[date.month - 1], date.day,
	    date.hour, date.minute, date.second, date.year);
    return buf;
}
