# include "dgd.h"

/*
 * NAME:	alloc()
 * DESCRIPTION:	allocate memory
 */
char *alloc(size)
unsigned int size;
{
    char *mem;

    mem = (char *) malloc(size);
    if (mem == (char*) NULL) {
	fatal("out of memory");
    }
# ifdef DEBUG
    printf("ALLOC(%06X, %u)\n", mem, size);
# endif
    return mem;
}

#ifdef DEBUG
void xfree(p)
char *p;
{
    printf("FREE(%06X, %U)\n", p, (long)(((char **)p)[-1]) - (long)p - 4);
    free(p);
}
#endif
