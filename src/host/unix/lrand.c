# include "dgd.h"

extern long seed	P((long));
extern long lrand	P((void));

/*
 * NAME:	P->srandom()
 * DESCRIPTION:	set the random seed
 */
void P_srandom(s)
long s;
{
    seed(s);
}

/*
 * NAME:	P->random()
 * DESCRIPTION:	return a long random number
 */
long P_random()
{
    return lrand();
}
