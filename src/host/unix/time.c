# include "dgd.h"
# include <time.h>

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the current time
 */
Uint P_time()
{
    return (Uint) time((time_t *) NULL);
}

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	convert the given time to a string
 */
char *P_ctime(buf, t)
char *buf;
Uint t;
{
    register int offset;

    offset = 0;
    for (offset = 0; t >= 2147397248L; t -= 1009843200L, offset += 32) ;
    memcpy(buf, ctime((time_t *) &t), 26);
    if (offset != 0) {
	long year;

	year = strtol(buf + 20, (char **) NULL, 10) + offset;
	if (year >= 2100 && (buf[4] != 'J' || buf[5] != 'a') &&
	    (buf[4] != 'F' || (buf[8] == '2' && buf[9] == '9'))) {
	    /* 2100 is not a leap year */
	    t += 86400L;
	    if (t >= 2147397248L) {
		t -= 1009843200L;
		offset += 32;
	    }
	    memcpy(buf, ctime((time_t *) &t), 26);
	    year = strtol(buf + 20, (char **) NULL, 10) + offset;
	}
	sprintf(buf + 20, "%ld\012", year);
    }
    return buf;
}
