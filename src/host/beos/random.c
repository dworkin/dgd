# include "dgd.h"

/*
 * NAME:	P->srandom()
 * DESCRIPTION:	set the random seed
 */
void P_srandom(long s)
{
    srandom((int) s);
}

/*
 * NAME:	P->random()
 * DESCRIPTION:	return a long random number
 */
long P_random()
{
    return random();
}
