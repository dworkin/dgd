# define INCLUDE_TIME
# include "dgd.h"

/*
 * NAME:	_time()
 * DESCRIPTION:	return the current time
 */
time_t _time()
{
    return time((time_t *) NULL);
}

/*
 * NAME:	_ctime()
 * DESCRIPTION:	convert the given time to a string
 */
char *_ctime(t)
time_t t;
{
    return ctime(&t);
}
