# include "dgd.h"
# include <Quickdraw.h>

/*
 * NAME:	P->srandom()
 * DESCRIPTION:	set seed for random number
 */
void P_srandom(long s)
{
    GetDateTime(&qd.randSeed);
}

/*
 * NAME:	P->random()
 * DESCRIPTION:	generate "random" number
 */
long P_random(void)
{
    return ((Random() & 0xffff) << 16) | (Random() & 0xffff);
}
	