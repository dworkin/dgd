# include "dgd.h"
# include <time.h>
# include <sys\timeb.h>

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the time in seconds since Jan 1, 1970
 */
Uint P_time()
{
    return (Uint) time((time_t *) NULL);
}

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	return time as string
 */
char *P_ctime(t)
Uint t;
{
    return ctime((time_t *) &t);
}

static struct _timeb timeout;

/*
 * NAME:        P->alarm()
 * DESCRIPTION: set the timeout to <delay> seconds in the future
 */
void P_alarm(delay)
unsigned int delay;
{
    _ftime(&timeout);
    timeout.time += delay;
}

/*
 * NAME:        P->timeout()
 * DESCRIPTION: return TRUE if there is a timeout, FALSE otherwise
 */
bool P_timeout()
{
    struct _timeb t;

    _ftime(&t);
    return (t.time >= timeout.time && 
	    (t.time > timeout.time || t.millitm >= timeout.millitm));
}
