# include "dgd.h"

/*
 * NAME:	rename()
 * DESCRIPTION:	rename a file
 */
int rename(from, to)
char *from, *to;
{
    if (link(from, to) < 0) {
	return -1;
    }
    if (unlink(from) < 0) {
	unlink(to);
	return -1;
    }
    return 0;
}
