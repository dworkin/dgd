# include "dgd.h"
# include <time.h>
# include <osbind.h>

static unsigned long starttime;	/* alarm start time */
unsigned long stoptime;		/* alarm stop time (used by connect.c) */

/*
 * NAME:	P->alarm()
 * DESCRIPTION:	cause an alarm signal to occur after the given number of
 *		seconds
 */
void P_alarm(delay)
unsigned int delay;
{
    starttime = clock();
    stoptime = starttime + delay * CLOCKS_PER_SEC;
    if (stoptime == 0) {
	stoptime = 1;	/* one millisecond extra */
    }
}

/*
 * NAME:	P->timeout()
 * DESCRIPTION:	return the value of the timeout flag
 */
bool P_timeout()
{
    unsigned long time;

    if (stoptime != 0) {
	time = clock();
	return (time >= stoptime || time < starttime);
    }
    return FALSE;
}
