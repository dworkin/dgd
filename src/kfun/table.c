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
    kfext = (extfunc *) NULL;
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
    register extfunc *kfe;

    kfext = REALLOC(kfext, extfunc, nkfun - sizeof(kforig) / sizeof(kfunc),
		    nkfun - sizeof(kforig) / sizeof(kfunc) + n);
    kfadd += n;
    nkfun += n;
    kf = kftab + nkfun;
    kfe = kfext + nkfun - sizeof(kforig) / sizeof(kfunc);
    while (n != 0) {
	(--kf)->name = (--kfadd)->name;
	kf->proto = prototype(kfadd->proto);
	kf->func = (int (*)()) &kf_callgate;
	*--kfe = kfadd->func;
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
    register int i, n;
    register char *k1, *k2;

    memcpy(kftab, kforig, sizeof(kforig));
    for (i = 0; i < nkfun; i++) {
	kftab[i].num = i;
    }
    for (i = 0, k1 = kfind, k2 = kfx; i < KF_BUILTINS; i++) {
	*k1++ = i;
	*k2++ = i;
    }
    qsort(kftab + KF_BUILTINS, nkfun - KF_BUILTINS, sizeof(kfunc), kf_cmp);
    for (n = 0; kftab[i].name[0] == '('; n++) {
	*k2++ = '\0';
	i++;
    }
    for (k1 = kfind + 128; i < nkfun; i++) {
	*k1++ = i;
	*k2++ = i + 128 - KF_BUILTINS - n;
    }
}

/*
 * NAME:	kfun->index()
 * DESCRIPTION:	search for kfun in the kfun table, return raw index or -1
 */
static int kf_index(name)
register char *name;
{
    register unsigned int h, l, m;
    register int c;

    l = KF_BUILTINS;
    h = nkfun;
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

/*
 * NAME:	kfun->func()
 * DESCRIPTION:	search for kfun in the kfun table, return index or -1
 */
int kf_func(name)
char *name;
{
    register int n;

    n = kf_index(name);
    if (n >= 0) {
	n = UCHAR(kfx[n]);
    }
    return n;
}

# ifdef DEBUG
/*
 * NAME:	kfun->reclaim()
 * DESCRIPTION:	reclaim kfun space
 */
void kf_reclaim()
{
    register int i, n, last;

    /* remove duplicates at the end */
    for (i = nkfun; --i >= KF_BUILTINS; ) {
	n = UCHAR(kfind[i + 128 - KF_BUILTINS]);
	if (UCHAR(kfx[n]) == i + 128 - KF_BUILTINS) {
	    if (++i != nkfun) {
		message("*** Reclaimed %d kernel function%s\012", nkfun - i,
			((nkfun - i > 1) ? "s" : ""));
		nkfun = i;
	    }
	    break;
	}
    }

    /* copy last to (removed_kfuns) */
    last = nkfun;
    for (i = KF_BUILTINS; i < nkfun && kftab[i].name[0] == '('; i++) {
	if (kfx[i] != '\0') {
	    message("*** Preparing to reclaim unused kfun %s\012",
		    kftab[i].name);
	    n = UCHAR(kfind[--last + 128 - KF_BUILTINS]);
	    kfx[n] = UCHAR(kfx[i]);
	    kfind[UCHAR(kfx[n])] = n;
	    kfx[i] = '\0';
	}
    }
}
# endif


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
    register int i, n;
    register unsigned int len, buflen;
    register kfunc *kf;
    dump_header dh;
    char *buffer;
    bool flag;

    /* prepare header */
    dh.nbuiltin = KF_BUILTINS;
    dh.nkfun = nkfun - KF_BUILTINS;
    dh.kfnamelen = 0;
    for (i = KF_BUILTINS; i < nkfun; i++) {
	n = UCHAR(kfind[i + 128 - KF_BUILTINS]);
	if (kfx[n] != '\0') {
	    dh.kfnamelen += strlen(kftab[n].name) + 1;
	} else {
	    --dh.nkfun;
	}
    }

    /* write header */
    if (P_write(fd, (char *) &dh, sizeof(dump_header)) < 0) {
	return FALSE;
    }

    /* write kfun names */
    buffer = ALLOCA(char, dh.kfnamelen);
    buflen = 0;
    for (i = KF_BUILTINS; i < nkfun; i++) {
	n = UCHAR(kfind[i + 128 - KF_BUILTINS]);
	if (kfx[n] != '\0') {
	    kf = &kftab[n];
	    len = strlen(kf->name) + 1;
	    memcpy(buffer + buflen, kf->name, len);
	    buflen += len;
	}
    }
    flag = (P_write(fd, buffer, buflen) >= 0);
    AFREE(buffer);

    return flag;
}

/*
 * NAME:	kfun->restore()
 * DESCRIPTION:	restore the kfun table
 */
void kf_restore(fd, oldcomp)
int fd, oldcomp;
{
    register int i, n, buflen;
    dump_header dh;
    char *buffer;

    /* read header */
    conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);

    /* fix kfuns */
    buffer = ALLOCA(char, dh.kfnamelen);
    if (P_read(fd, buffer, (unsigned int) dh.kfnamelen) < 0) {
	fatal("cannot restore kfun names");
    }
    memset(kfx + KF_BUILTINS, '\0', nkfun);
    buflen = 0;
    for (i = 0; i < dh.nkfun; i++) {
	n = kf_index(buffer + buflen);
	if (n < 0) {
	    if (strcmp(buffer + buflen, "hash_md5") == 0) {
		n = kf_index("(hash_md5)");
	    } else if (strcmp(buffer + buflen, "hash_sha1") == 0) {
		n = kf_index("(hash_sha1)");
	    } else {
		error("Restored unknown kfun: %s", buffer + buflen);
	    }
	}
	if (kfx[n] == '\0') {
	    if (kftab[n].func == kf_old_compile_object) {
		oldcomp = FALSE;
	    } else if (kftab[n].func == kf_compile_object && oldcomp) {
		/* convert compile_object() */
		n = kf_index("(compile_object)");
	    }
	    kfx[n] = i + 128;
	}
	kfind[i + 128] = n;
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
	    if (kfx[i] == '\0' && kftab[i].name[0] != '(') {
		/* new kfun */
		kfind[n] = i;
		kfx[i] = n++;
	    }
	}
    }
}
