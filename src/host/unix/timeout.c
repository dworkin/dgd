# include "dgd.h"
# include <sys/time.h>

static struct timeval timeout;

/*
 * NAME:        P->alarm()
 * DESCRIPTION: set the timeout to <delay> seconds in the future
 */
void P_alarm(delay)
unsigned int delay;
{
    gettimeofday(&timeout, (struct timezone *) NULL);
    timeout.tv_sec += delay;
}

/*
 * NAME:        P->timeout()
 * DESCRIPTION: return TRUE if there is a timeout, FALSE otherwise
 */
bool P_timeout()
{
    struct timeval t;

    gettimeofday(&t, (struct timezone *) NULL);
    return (t.tv_sec >= timeout.tv_sec && 
	    (t.tv_sec > timeout.tv_sec || t.tv_usec >= timeout.tv_usec));
}
