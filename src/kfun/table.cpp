/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

# define INCLUDE_FILE_IO
# include "kfun.h"
# include "ext.h"

/*
 * prototypes
 */
# define FUNCDEF(name, func, proto, v) extern int func(Frame*, int, KFun*); extern char proto[];
# include "builtin.cpp"
# include "std.cpp"
# include "file.cpp"
# include "math.cpp"
# include "extra.cpp"
# undef FUNCDEF

/*
 * kernel function table
 */
static KFun kforig[] = {
# define FUNCDEF(name, func, proto, v) { name, proto, func, (ExtFunc) NULL, v },
# include "builtin.cpp"
# include "std.cpp"
# include "file.cpp"
# include "math.cpp"
# include "extra.cpp"
# undef FUNCDEF
};

KFun kftab[KFTAB_SIZE];		/* kfun tab */
KFun kfenc[KFCRYPT_SIZE];	/* encryption */
KFun kfdec[KFCRYPT_SIZE];	/* decryption */
KFun kfhsh[KFCRYPT_SIZE];	/* hashing */
kfindex kfind[KFTAB_SIZE];	/* n -> index */
static kfindex kfx[KFTAB_SIZE];	/* index -> n */
int nkfun, ne, nd, nh;		/* # kfuns */
static int okfun, oe, od, oh;	/* # original kfuns */

extern void kf_enc(Frame *, int, Value *);
extern void kf_enc_key(Frame *, int, Value *);
extern void kf_dec(Frame *, int, Value *);
extern void kf_dec_key(Frame *, int, Value *);
extern void kf_xcrypt(Frame *, int, Value *);
extern void kf_md5(Frame *, int, Value *);
extern void kf_sha1(Frame *, int, Value *);

/*
 * handle an argument error in a builtin kfun
 */
void KFun::argError(int n)
{
    EC->error("Bad argument %d for kfun %s", n, name);
}

/*
 * handle unary operator
 */
void KFun::unary(Frame *f)
{
    if (!f->call((Object *) NULL, dynamic_cast<LWO *> (f->sp->array), name,
		 strlen(name), TRUE, 0)) {
	argError(1);
    }
    if (f->sp->type != T_LWOBJECT || f->sp->array->elts[0].type != T_OBJECT) {
	EC->error("operator %s did not return a light-weight object", name);
    }

    f->sp[1].array->del();
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * handle binary operator
 */
void KFun::binary(Frame *f)
{
    if (VAL_NIL(f->sp)) {
	argError(2);
    }

    if (!f->call((Object *) NULL, dynamic_cast<LWO *> (f->sp[1].array), name,
		 strlen(name), TRUE, 1))
    {
	argError(1);
    }
    if (f->sp->type != T_LWOBJECT || f->sp->array->elts[0].type != T_OBJECT) {
	EC->error("operator %s did not return a light-weight object", name);
    }

    f->sp[1].array->del();
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * handle compare operator
 */
void KFun::compare(Frame *f)
{
    if (VAL_NIL(f->sp)) {
	argError(2);
    }

    if (!f->call((Object *) NULL, dynamic_cast<LWO *> (f->sp[1].array), name,
		 strlen(name), TRUE, 1))
    {
	argError(1);
    }
    if (f->sp->type != T_INT || (f->sp->number & ~1)) {
	EC->error("operator %s did not return a truth value", name);
    }

    f->sp[1].array->del();
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * handle ternary operator
 */
void KFun::ternary(Frame *f)
{
    if (!f->call((Object *) NULL, dynamic_cast<LWO *> (f->sp[2].array), name,
		 strlen(name), TRUE, 2))
    {
	argError(1);
    }
    if (f->sp->type != T_LWOBJECT || f->sp->array->elts[0].type != T_OBJECT) {
	EC->error("operator %s did not return a light-weight object", name);
    }

    f->sp[1].array->del();
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * clear previously added kfuns from the table
 */
void KFun::clear()
{
    static char proto[] = { T_VOID, 0 };
    static ExtKFun builtin[] = {
	{ "encrypt DES", proto, kf_enc },
	{ "encrypt DES key", proto, kf_enc_key },
	{ "decrypt DES", proto, kf_dec },
	{ "decrypt DES key", proto, kf_dec_key },
	{ "hash MD5", proto, kf_md5 },
	{ "hash SHA1", proto, kf_sha1 },
	{ "hash crypt", proto, kf_xcrypt }
    };

    nkfun = sizeof(kforig) / sizeof(KFun);
    oe = ne = od = nd = oh = nh = 0;
    add(builtin, 7);

    init();
    okfun = nkfun;
    oe = ne;
    od = nd;
    oh = nh;
}

/*
 * extra kfun call gate
 */
int KFun::callgate(Frame *f, int nargs, KFun *kf)
{
    if (!setjmp(*EC->push())) {
	Value val;

	val = nil;
	(kf->ext)(f, nargs, &val);
	val.ref();
	f->pop(nargs);
	*--f->sp = val;
	if (kf->lval) {
	    f->kflv = TRUE;
	    PUSH_ARRVAL(f, ext_value_temp(f->data)->array);
	}

	EC->pop();
    } else {
	EC->error((char *) NULL);
    }
    return 0;
}

/*
 * construct proper prototype for new kfun
 */
char *KFun::prototype(char *proto, bool *lval)
{
    char *p, *q;
    int nargs, vargs;
    int tclass, type;
    bool varargs;

    tclass = C_STATIC;
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
		    tclass |= C_ELLIPSIS;
		    if (!varargs) {
			--nargs;
			vargs++;
		    }
		    break;
		}
		varargs = TRUE;
	    } else {
		if (*p != T_MIXED) {
		    if (*p == T_LVALUE) {
			/* lvalue arguments: turn off typechecking */
			tclass &= ~C_TYPECHECKED;
			*lval = TRUE;
		    } else {
			/* non-mixed arguments: typecheck this function */
			tclass |= C_TYPECHECKED;
		    }
		}
		if (varargs) {
		    vargs++;
		} else {
		    nargs++;
		}
	    }
	    p++;
	}
    }

    /* allocate new prototype */
    p = proto;
    q = proto = ALLOC(char, 6 + nargs + vargs);
    *q++ = tclass;
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
 * possibly replace an existing algorithmic kfun
 */
KFun *KFun::replace(KFun *table, int from, int to, int *size, const char *name)
{
    int i;
    KFun *kf;

    i = find(table, from, to, name);
    if (i >= 0) {
	return &table[i];
    }

    kf = &table[(*size)++];
    kf->name = name;
    kf->version = 0;
    return kf;
}

/*
 * add new kfuns
 */
void KFun::add(const ExtKFun *kfadd, int n)
{
    KFun *kf;

    for (; n != 0; kfadd++, --n) {
	if (strncmp(kfadd->name, "encrypt ", 8) == 0) {
	    kf = replace(kfenc, 0, oe, &ne, kfadd->name + 8);
	} else if (strncmp(kfadd->name, "decrypt ", 8) == 0) {
	    kf = replace(kfdec, 0, od, &nd, kfadd->name + 8);
	} else if (strncmp(kfadd->name, "hash ", 5) == 0) {
	    kf = replace(kfhsh, 0, oh, &nh, kfadd->name + 5);
	} else {
	    kf = replace(kftab, KF_BUILTINS, okfun, &nkfun, kfadd->name);
	}
	kf->lval = FALSE;
	kf->proto = prototype(kfadd->proto, &kf->lval);
	kf->func = &callgate;
	kf->ext = kfadd->func;
    }
}

/*
 * compare two kftable entries
 */
int KFun::cmp(cvoid *cv1, cvoid *cv2)
{
    return strcmp(((KFun *) cv1)->name, ((KFun *) cv2)->name);
}

/*
 * initialize the kfun table
 */
void KFun::init()
{
    int i, n;
    kfindex *k1, *k2;

    memcpy(kftab, kforig, sizeof(kforig));
    for (i = 0, k1 = kfind, k2 = kfx; i < KF_BUILTINS; i++) {
	*k1++ = i;
	*k2++ = i;
    }
    std::qsort((void *) (kftab + KF_BUILTINS), nkfun - KF_BUILTINS,
	       sizeof(KFun), cmp);
    std::qsort(kfenc, ne, sizeof(KFun), cmp);
    std::qsort(kfdec, nd, sizeof(KFun), cmp);
    std::qsort(kfhsh, nh, sizeof(KFun), cmp);
    for (n = 0; kftab[i].name[1] == '.'; n++) {
	*k2++ = '\0';
	i++;
    }
    for (k1 = kfind + 128; i < nkfun; i++) {
	*k1++ = i;
	*k2++ = i + 128 - KF_BUILTINS - n;
    }
}

/*
 * prepare JIT compiler
 */
void KFun::jit()
{
    int size, i, nkf;
    char *protos, *proto;

    for (size = 0, i = 0; i < KF_BUILTINS; i++) {
	size += PROTO_SIZE(kftab[i].proto);
    }
    nkf = nkfun - KF_BUILTINS;
    for (; i < nkfun; i++) {
	if (kfx[i] != 0) {
	    size += PROTO_SIZE(kftab[i].proto);
	} else {
	    --nkf;
	}
    }

    protos = ALLOCA(char, size);
    for (i = 0; i < KF_BUILTINS; i++) {
	proto = kftab[i].proto;
	memcpy(protos, proto, PROTO_SIZE(proto));
	protos += PROTO_SIZE(proto);
    }
    for (i = 0; i < nkf; i++) {
	proto = kftab[kfind[i + 128]].proto;
	memcpy(protos, proto, PROTO_SIZE(proto));
	protos += PROTO_SIZE(proto);
    }

    protos -= size;
    Ext::kfuns(protos, size, nkf);
    AFREE(protos);
}

/*
 * search for kfun in the kfun table, return raw index or -1
 */
int KFun::find(KFun *kf, unsigned int l, unsigned int h, const char *name)
{
    unsigned int m;
    int c;

    while (l < h) {
	c = strcmp(name, kf[m = (l + h) >> 1].name);
	if (c == 0) {
	    return m;	/* found */
	} else if (c < 0) {
	    h = m;	/* search in lower half */
	} else {
	    l = m + 1;	/* search in upper half */
	}
    }
    /*
     * not found
     */
    return -1;
}

/*
 * search for kfun in the kfun table, return index or -1
 */
int KFun::kfunc(const char *name)
{
    int n;

    /* some kfuns are remapped to builtins */
    n = strcmp(name, "call_trace");
    if (n == 0) {
	return KF_CALL_TRACE;
    } else if (n < 0) {
	if (strcmp(name, "call_other") == 0) {
	    return KF_CALL_OTHER;
	}
    } else {
	n = strcmp(name, "strlen");
	if (n == 0) {
	    return KF_STRLEN;
	} else if (n < 0 && strcmp(name, "status") == 0) {
	    return KF_STATUS;
	}
    }

    n = find(kftab, KF_BUILTINS, nkfun, name);
    if (n >= 0) {
	n = kfx[n];
    }
    return n;
}

/*
 * reclaim kfun space
 */
void KFun::reclaim()
{
    int i, n, last;

    /* skip already-removed kfuns */
    for (last = nkfun; kfind[--last + 128 - KF_BUILTINS] == '\0'; ) ;

    /* remove duplicates at the end */
    for (i = last; i >= KF_BUILTINS; --i) {
	n = kfind[i + 128 - KF_BUILTINS];
	if (kfx[n] == i + 128 - KF_BUILTINS) {
	    if (i != last) {
		EC->message("*** Reclaimed %d kernel function%s\012", last - i,
			    ((last - i > 1) ? "s" : ""));
	    }
	    break;
	}
	kfind[i + 128 - KF_BUILTINS] = '\0';
    }

    /* copy last to 0.removed_kfuns */
    for (i = KF_BUILTINS; i < nkfun && kftab[i].name[1] == '.'; i++) {
	if (kfx[i] != '\0') {
	    EC->message("*** Preparing to reclaim unused kfun %s\012",
			kftab[i].name);
	    n = kfind[last-- + 128 - KF_BUILTINS];
	    kfx[n] = kfx[i];
	    kfind[kfx[n]] = n;
	    kfx[i] = '\0';
	}
    }
}


struct KfunHeader {
    short nbuiltin;	/* # builtin kfuns */
    short nkfun;	/* # other kfuns */
    short kfnamelen;	/* length of all kfun names */
};

static char dh_layout[] = "sss";

/*
 * dump the kfun table
 */
bool KFun::dump(int fd)
{
    int i, n;
    unsigned int len, buflen;
    KFun *kf;
    KfunHeader dh;
    char *buffer;
    bool flag;

    /* prepare header */
    dh.nbuiltin = KF_BUILTINS;
    dh.nkfun = nkfun - KF_BUILTINS;
    dh.kfnamelen = 0;
    for (i = KF_BUILTINS; i < nkfun; i++) {
	n = kfind[i + 128 - KF_BUILTINS];
	if (kfx[n] != '\0') {
	    dh.kfnamelen += strlen(kftab[n].name) + 1;
	    if (kftab[n].name[1] != '.') {
		dh.kfnamelen += 2;
	    }
	} else {
	    --dh.nkfun;
	}
    }

    /* write header */
    if (!Swap::write(fd, &dh, sizeof(KfunHeader))) {
	return FALSE;
    }

    /* write kfun names */
    buffer = ALLOCA(char, dh.kfnamelen);
    buflen = 0;
    for (i = KF_BUILTINS; i < nkfun; i++) {
	n = kfind[i + 128 - KF_BUILTINS];
	if (kfx[n] != '\0') {
	    kf = &kftab[n];
	    if (kf->name[1] != '.') {
		buffer[buflen++] = '0' + kf->version;
		buffer[buflen++] = '.';
	    }
	    len = strlen(kf->name) + 1;
	    memcpy(buffer + buflen, kf->name, len);
	    buflen += len;
	}
    }
    flag = Swap::write(fd, buffer, buflen);
    AFREE(buffer);

    return flag;
}

/*
 * restore the kfun table
 */
void KFun::restore(int fd)
{
    int i, n, buflen;
    KfunHeader dh;
    char *buffer;

    /* read header */
    Config::dread(fd, (char *) &dh, dh_layout, (Uint) 1);

    /* fix kfuns */
    buffer = ALLOCA(char, dh.kfnamelen);
    if (P_read(fd, buffer, (unsigned int) dh.kfnamelen) < 0) {
	EC->fatal("cannot restore kfun names");
    }
    memset(kfx + KF_BUILTINS, '\0', (nkfun - KF_BUILTINS) * sizeof(kfindex));
    buflen = 0;
    for (i = 0; i < dh.nkfun; i++) {
	if (buffer[buflen + 1] == '.') {
	    n = find(kftab, KF_BUILTINS, nkfun, buffer + buflen + 2);
	    if (n < 0 || kftab[n].version != buffer[buflen] - '0') {
		n = find(kftab, KF_BUILTINS, nkfun, buffer + buflen);
		if (n < 0) {
		    EC->error("Restored unknown kfun: %s", buffer + buflen);
		}
	    }
	} else {
	    n = find(kftab, KF_BUILTINS, nkfun, buffer + buflen);
	    if (n < 0) {
		EC->error("Restored unknown kfun: %s", buffer + buflen);
	    }
	}
	kfx[n] = i + 128;
	kfind[i + 128] = n;
	buflen += strlen(buffer + buflen) + 1;
    }
    AFREE(buffer);

    if (dh.nkfun < nkfun - KF_BUILTINS) {
	/*
	 * There are more kfuns in the current driver than in the driver
	 * which created the snapshot: deal with those new kfuns.
	 */
	n = dh.nkfun + 128;
	for (i = KF_BUILTINS; i < nkfun; i++) {
	    if (kfx[i] == '\0' && kftab[i].name[1] != '.') {
		/* new kfun */
		kfind[n] = i;
		kfx[i] = n++;
	    }
	}
    }
}
