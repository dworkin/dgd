# include "kfun.h"
# include "table.h"

/*
 * prototypes
 */
# define FUNCDEF(name, func, proto)	extern int func(); extern char proto[];
# include "builtin.c"
# include "std.c"
# include "file.c"
# include "extra.c"
# include "debug.c"
# undef FUNCDEF

/*
 * kernel function table
 */
kfunc kftab[] = {
# define FUNCDEF(name, func, proto)	{ name, func, proto },
# include "builtin.c"
# include "std.c"
# include "file.c"
# include "extra.c"
# include "debug.c"
# undef FUNCDEF
};

/*
 * NAME:	kfun->cmp()
 * DESCRIPTION:	compare two kftable entries
 */
static int kf_cmp(kf1, kf2)
kfunc *kf1, *kf2;
{
    return strcmp(kf1->name, kf2->name);
}

/*
 * NAME:	kfun->init()
 * DESCRIPTION:	initialize the kfun table
 */
void kf_init()
{
    qsort(kftab + KF_BUILTINS, sizeof(kftab) / sizeof(kfunc) - KF_BUILTINS,
	  sizeof(kfunc), kf_cmp);
}

/*
 * NAME:	kfun->func()
 * DESCRIPTION:	search for kfun in the kfun table, return index or -1
 */
int kf_func(name)
register char *name;
{
    register int h, l, m, c;

    l = KF_BUILTINS;
    h = sizeof(kftab) / sizeof(kfunc);
    do {
	c = strcmp(name, kftab[m = (l + h) >> 1].name);
	if (c == 0) {
	    return m;	/* found */
	} else if (c < 0) {
	    h = m;	/* search in lower half */
	} else {
	    l = m + 1;	/* search in upper half */
	}
    } while (l < h);
    /*
     * not found
     */
    return -1;
}
