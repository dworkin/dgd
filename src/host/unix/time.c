# include "dgd.h"
# include <time.h>

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the current time
 */
unsigned long P_time()
{
    return (unsigned long) time((time_t *) NULL);
}

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	convert the given time to a string
 */
char *P_ctime(t)
unsigned long t;
{
    return ctime(&t);
}
