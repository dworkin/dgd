# include "dgd.h"

extern void srand48	P((long));
extern long lrand48	P((void));

/*
 * NAME:	P->srandom()
 * DESCRIPTION:	set the random seed
 */
void P_srandom(s)
long s;
{
    srand48(s);
}

/*
 * NAME:	P->random()
 * DESCRIPTION:	return a long random number
 */
long P_random()
{
    return lrand48();
}
