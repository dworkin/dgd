# include "dgd.h"

extern long lrand	P((void));

/*
 * NAME:	random()
 * DESCRIPTION:	return a long random number
 */
long random()
{
    return lrand();
}
