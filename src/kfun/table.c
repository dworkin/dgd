# define INCLUDE_FILE_IO
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
static kfunc kforig[] = {
# define FUNCDEF(name, func, proto)	{ name, proto, func, 0 },
# include "builtin.c"
# include "std.c"
# include "file.c"
# include "math.c"
# include "extra.c"
# include "debug.c"
# undef FUNCDEF
};

kfunc kftab[256];	/* kfun tab */
char kfind[256];	/* n -> index */
static char kfx[256];	/* index -> n */
static int nkfun;	/* # kfuns */
static extfunc *kfext;	/* additional kfun pointers */

/*
 * NAME:	kfun->clear()
 * DESCRIPTION:	clear previously added kfuns from the table
 */
void kf_clear()
{
    nkfun = sizeof(kforig) / sizeof(kfunc);
}

/*
 * NAME:	kfun->callgate()
 * DESCRIPTION:	extra kfun call gate
 */
static int kf_callgate(f, nargs, kf)
frame *f;
int nargs;
kfunc *kf;
{
    value val;

    val = nil_value;
    (*kfext[kf->num - sizeof(kforig) / sizeof(kfunc)])(f, nargs, &val);
    i_pop(f, nargs);
    *--f->sp = val;

    return 0;
}

/*
 * NAME:	prototype()
 * DESCRIPTION:	construct proper prototype for new kfun
 */
static char *prototype(proto)
char *proto;
{
    register char *p, *q;
    register int nargs, vargs;
    int class, type;
    bool varargs;

    class = C_STATIC;
    type = *proto++;
    p = proto;
    nargs = vargs = 0;
    varargs = FALSE;

    /* pass 1: check prototype */
    if (*p != T_VOID) {
	while (*p != '\0') {
	    if (*p == T_VARARGS) {
		/* varargs or ellipsis */
		if (p[1] == '\0') {
		    class |= C_ELLIPSIS;
		    if (!varargs) {
			--nargs;
			vargs++;
		    }
		    break;
		}
		varargs = TRUE;
	    } else {
		if (*p != T_MIXED) {
		    /* non-mixed arguments: typecheck this function */
		    class |= C_TYPECHECKED;
		}
		if (varargs) {
		    nargs++;
		} else {
		    vargs++;
		}
	    }
	    p++;
	}
    }

    /* allocate new prototype */
    p = proto;
    q = proto = ALLOC(char, 6 + nargs + vargs);
    *q++ = class;
    *q++ = nargs;
    *q++ = vargs;
    *q++ = 0;
    *q++ = 6 + nargs + vargs;
    *q++ = type;

    /* pass 2: fill in new prototype */
    if (*p != T_VOID) {
	while (*p != '\0') {
	    if (*p != T_VARARGS) {
		*q++ = *p;
	    }
	    p++;
	}
    }

    return proto;
}

/*
 * NAME:	kfun->ext_kfun()
 * DESCRIPTION:	add new kfuns
 */
void kf_ext_kfun(kfadd, n)
register extkfunc *kfadd;
register int n;
{
    register kfunc *kf;

    kfext = ALLOC(extfunc, n) + n;
    kfadd += n;
    nkfun += n;
    kf = kftab + nkfun;
    while (n != 0) {
	(--kf)->name = (--kfadd)->name;
	kf->proto = prototype(kfadd->proto);
	kf->func = (int (*)()) &kf_callgate;
	*--kfext = kfadd->func;
	--n;
    }
}

/*
 * NAME:	kfun->cmp()
 * DESCRIPTION:	compare two kftable entries
 */
static int kf_cmp(cv1, cv2)
cvoid *cv1, *cv2;
{
    return strcmp(((kfunc *) cv1)->name, ((kfunc *) cv2)->name);
}

/*
 * NAME:	kfun->init()
 * DESCRIPTION:	initialize the kfun table
 */
void kf_init()
{
    register int i;
    register char *k1, *k2;

    memcpy(kftab, kforig, sizeof(kforig));
    for (i = 0; i < nkfun; i++) {
	kftab[i].num = i;
    }
    qsort(kftab + KF_BUILTINS, nkfun - KF_BUILTINS, sizeof(kfunc), kf_cmp);
    for (i = nkfun, k1 = kfind + nkfun + 128 - KF_BUILTINS, k2 = kfx + nkfun;
	 i > KF_BUILTINS; ) {
	*--k1 = --i;
	*--k2 = i + 128 - KF_BUILTINS;
    }
    for (k1 = kfind + KF_BUILTINS; i > 0; ) {
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
    register unsigned int h, l, m;
    register int c;

    l = KF_BUILTINS;
    h = nkfun;
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

static char dh_layout[] = "sss";

/*
 * NAME:	kfun->dump()
 * DESCRIPTION:	dump the kfun table
 */
bool kf_dump(fd)
int fd;
{
    register int i;
    register unsigned int len, buflen;
    register kfunc *kf;
    dump_header dh;
    char *buffer;
    bool flag;

    /* prepare header */
    dh.nbuiltin = KF_BUILTINS;
    dh.nkfun = nkfun - KF_BUILTINS;
    dh.kfnamelen = 0;
    for (i = dh.nkfun, kf = kftab + KF_BUILTINS; i > 0; --i, kf++) {
	dh.kfnamelen += strlen(kf->name) + 1;
    }

    /* write header */
    if (P_write(fd, (char *) &dh, sizeof(dump_header)) < 0) {
	return FALSE;
    }

    /* write kfun names */
    buffer = ALLOCA(char, dh.kfnamelen);
    buflen = 0;
    for (i = 0; i < dh.nkfun; i++) {
	kf = &KFUN(i + 128);
	len = strlen(kf->name) + 1;
	memcpy(buffer + buflen, kf->name, len);
	buflen += len;
    }
    flag = (P_write(fd, buffer, buflen) >= 0);
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
    bool converted;

    /* read header */
    conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);

    /* fix kfuns */
    buffer = ALLOCA(char, dh.kfnamelen);
    if (P_read(fd, buffer, (unsigned int) dh.kfnamelen) < 0) {
	fatal("cannot restore kfun names");
    }
    buflen = 0;
    converted = FALSE;
    for (i = 0; i < dh.nkfun; i++) {
	n = kf_func(buffer + buflen);
	if (n < 0) {
	    error("Restored unknown kfun: %s", buffer + buflen);
	}
	n += KF_BUILTINS - 128;
	if (kftab[n].func == kf_old_compile_object) {
	    converted = TRUE;
	} else if (kftab[n].func == kf_compile_object && !converted) {
	    /* HACK: convert compile_object() */
	    n = KF_BUILTINS;
	}
	kfind[i + 128] = n;
	kfx[n] = i + 128;
	buflen += strlen(buffer + buflen) + 1;
    }
    AFREE(buffer);

    if (dh.nkfun < nkfun - KF_BUILTINS) {
	/*
	 * There are more kfuns in the current driver than in the driver
	 * which created the dump file: deal with those new kfuns.
	 */
	n = dh.nkfun + 128;
	for (i = KF_BUILTINS; i < nkfun; i++) {
	    if (UCHAR(kfx[i]) >= dh.nkfun + 128 ||
		UCHAR(kfind[UCHAR(kfx[i])]) != i) {
		/* new kfun */
		kfind[n] = i;
		kfx[i] = n++;
	    }
	}
    }
}
