# include "dgd.h"
# include <time.h>
# include <sys\timeb.h>

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the time in seconds since Jan 1, 1970
 */
Uint P_time(void)
{
    return (Uint) time((time_t *) NULL);
}

/*
 * NAME:	P->mtime()
 * DESCRIPTION:	return the time in seconds since Jan 1, 1970 in milliseconds
 */
Uint P_mtime(unsigned short *milli)
{
    struct _timeb t;

    _ftime(&t);
    *milli = t.millitm;
    return (Uint) t.time;
}

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	return time as string
 */
char *P_ctime(char *buf, Uint t)
{
    int offset;

    offset = 0;
    for (offset = 0; (Int) t < 0; t -= 1009843200, offset += 32) ;
    memcpy(buf, ctime((time_t *) &t), 26);
    if (offset != 0) {
	long year;

	year = strtol(buf + 20, (char **) NULL, 10) + offset;
	if (year >= 2100 && (buf[4] != 'J' || buf[5] != 'a') &&
	    (buf[4] != 'F' || (buf[8] == '2' && buf[9] == '9'))) {
	    /* 2100 is not a leap year */
	    t += 86400;
	    if ((Int) t < 0) {
		t -= 1009843200;
		offset += 32;
	    }
	    memcpy(buf, ctime((time_t *) &t), 26);
	    year = strtol(buf + 20, (char **) NULL, 10) + offset;
	}
	sprintf(buf + 20, "%ld\012", year);
    }
    if (buf[8] == '0') {
	buf[8] = ' ';	/* MSDEV ctime weirdness */
    }
    return buf;
}

static struct _timeb timeout;

/*
 * NAME:        P->alarm()
 * DESCRIPTION: set the timeout to <delay> seconds in the future
 */
void P_alarm(unsigned int delay)
{
    _ftime(&timeout);
    timeout.time += delay;
}

/*
 * NAME:        P->timeout()
 * DESCRIPTION: return TRUE if there is a timeout, FALSE otherwise
 */
bool P_timeout(void)
{
    struct _timeb t;

    _ftime(&t);
    return (t.time >= timeout.time && 
	    (t.time > timeout.time || t.millitm >= timeout.millitm));
}
