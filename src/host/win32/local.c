# include "dgd.h"

/*
 * NAME:	P->srandom()
 * DESCRIPTION:	set random seed
 */
void P_srandom(long seed)
{
    srand((unsigned int) seed);
}

/*
 * NAME:	P->random()
 * DESCRIPTION:	get random number
 */
long P_random(void)
{
    return (long) (rand() ^ (rand() << 9) ^ (rand() << 16));
}
