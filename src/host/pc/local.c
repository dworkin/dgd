# include "dgd.h"

/*
 * NAME:	P->getevent()
 * DESCRIPTION:	(don't) get the next event
 */
void P_getevent()
{
}

/*
 * NAME:	P->srandom()
 * DESCRIPTION:	set random seed
 */
void P_srandom(seed)
long seed;
{
    srand((unsigned int) seed);
}

/*
 * NAME:	P->random()
 * DESCRIPTION:	get random number
 */
long P_random()
{
    return (long) rand();
}
