# include "kfun.h"
# include "table.h"

/*
 * prototypes
 */
# define FUNCDEF(name, func, proto)	extern int func(); extern char proto[];
# include "builtin.c"
# include "std.c"
# include "file.c"
# include "math.c"
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
# include "math.c"
# include "extra.c"
# include "debug.c"
# undef FUNCDEF
};

char kfind[sizeof(kftab) / sizeof(kfunc)];	/* n -> index */
static char kfx[sizeof(kftab) / sizeof(kfunc)];	/* index -> n */

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
    register int i;
    register char *k1, *k2;

    qsort(kftab + KF_BUILTINS, sizeof(kftab) / sizeof(kfunc) - KF_BUILTINS,
	  sizeof(kfunc), kf_cmp);
    for (i = sizeof(kftab) / sizeof(kfunc), k1 = kfind + i, k2 = kfx + i;
	 i > 0; ) {
	*--k1 = --i;
	*--k2 = i;
    }
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
	    return UCHAR(kfx[m]);	/* found */
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


typedef struct {
    short nbuiltin;	/* # builtin kfuns */
    short nkfun;	/* # other kfuns */
    short kfnamelen;	/* length of all kfun names */
} dump_header;

/*
 * NAME:	kfun->dump()
 * DESCRIPTION:	dump the kfun table
 */
bool kf_dump(fd)
int fd;
{
    register int i, len, buflen;
    register kfunc *kf;
    dump_header dh;
    char *buffer;
    bool flag;

    /* prepare header */
    dh.nbuiltin = KF_BUILTINS;
    dh.nkfun = sizeof(kftab) / sizeof(kfunc) - KF_BUILTINS;
    dh.kfnamelen = 0;
    for (i = dh.nkfun, kf = kftab + KF_BUILTINS; i > 0; --i, kf++) {
	dh.kfnamelen += strlen(kf->name) + 1;
    }

    /* write header */
    if (write(fd, &dh, sizeof(dump_header)) < 0) {
	return FALSE;
    }

    /* write kfun names */
    buffer = ALLOCA(char, dh.kfnamelen);
    buflen = 0;
    for (i = 0; i < dh.nkfun; i++) {
	kf = &KFUN(i + KF_BUILTINS);
	len = strlen(kf->name) + 1;
	memcpy(buffer + buflen, kf->name, len);
	buflen += len;
    }
    flag = (write(fd, buffer, buflen) >= 0);
    AFREE(buffer);

    return flag;
}

/*
 * NAME:	kfun->restore()
 * DESCRIPTION:	restore the kfun table
 */
void kf_restore(fd)
int fd;
{
    register int i, n, buflen;
    dump_header dh;
    char *buffer;

    /* deal with header */
    if (read(fd, &dh, sizeof(dump_header)) != sizeof(dump_header) ||
	dh.nbuiltin > KF_BUILTINS || dh.nkfun > sizeof(kftab) / sizeof(kfunc)) {
	fatal("cannot restore kfun table");
    }

    /* fix kfuns */
    buffer = ALLOCA(char, dh.kfnamelen);
    if (read(fd, buffer, (unsigned int) dh.kfnamelen) < 0) {
	fatal("cannot restore kfun names");
    }
    buflen = 0;
    for (i = 0; i < dh.nkfun; i++) {
	n = kf_func(buffer + buflen);
	if (n < 0) {
	    fatal("restored unknown kfun: %s", buffer + buflen);
	}
	kfind[i + KF_BUILTINS] = n;
	kfx[n] = i + KF_BUILTINS;
	buflen += strlen(buffer + buflen) + 1;
    }
    AFREE(buffer);

    if (dh.nkfun < sizeof(kftab) / sizeof(kfunc) - KF_BUILTINS) {
	/*
	 * There are more kfuns in the current driver than in the driver
	 * which created the dump file: deal with those new kfuns.
	 */
	n = dh.nkfun + KF_BUILTINS;
	for (i = KF_BUILTINS; i < sizeof(kftab) / sizeof(kfunc); i++) {
	    if (kfx[i] >= dh.nkfun + KF_BUILTINS || kfind[kfx[i]] != i) {
		/* new kfun */
		kfind[n] = i;
		kfx[i] = n++;
	    }
	}
    }
}
