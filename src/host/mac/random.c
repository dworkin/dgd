# include <Quickdraw.h>
# include "dgd.h"

/*
 * NAME:	P->srandom()
 * DESCRIPTION:	set seed for random number
 */
void P_srandom(long s)
{
    qd.randSeed = s;
}

/*
 * NAME:	P->random()
 * DESCRIPTION:	generate "random" number
 */
long P_random(void)
{
    return (Random() + Random() * 32768) & 0x7fffffff;
}
