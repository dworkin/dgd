# include "dgd.h"
# include <errno.h>

/*
 * NAME:	rename()
 * DESCRIPTION:	rename a file
 */
int rename(from, to)
char *from, *to;
{
    static unsigned short count;
    char buf[9];

    /* check if 'from' can be linked at all */
    sprintf(buf, "_rnm%04x", ++count);
    if (strcmp(from, to) == 0 || link(from, buf) < 0 || unlink(buf) < 0 ||
	(unlink(to) < 0 && errno != ENOENT) ||
	link(from, to) < 0 || unlink(from) < 0) {
	return -1;
    }
    return 0;
}
