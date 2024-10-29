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

# ifndef FUNCDEF
# define INCLUDE_CTYPE
# include "kfun.h"
# include "parse.h"
# include "asn.h"
# endif

# ifdef FUNCDEF
FUNCDEF("encrypt", kf_encrypt, pt_encrypt, 0)
# else
extern String *P_encrypt_des_key (Frame*, String*);
extern String *P_encrypt_des (Frame*, String*, String*);
extern void ext_runtime_error (Frame*, const char*);

char pt_encrypt[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 1, 1, 0, 8,
		      T_MIXED, T_STRING, T_MIXED };

/*
 * prepare a key for encryption
 */
void kf_enc_key(Frame *f, int nargs, Value *val)
{
    String *str;

    if (nargs != 1) {
	ext_runtime_error(f, "Too many arguments for kfun encrypt");
    }
    str = P_encrypt_des_key(f, f->sp->string);
    PUT_STRVAL_NOREF(val, str);
}

/*
 * encrypt
 */
void kf_enc(Frame *f, int nargs, Value *val)
{
    String *str;

    if (nargs != 2) {
	ext_runtime_error(f, "Too few arguments for kfun encrypt");
    }
    str = P_encrypt_des(f, f->sp[1].string, f->sp->string);
    PUT_STRVAL_NOREF(val, str);
}

/*
 * encrypt a string
 */
int kf_encrypt(Frame *f, int nargs, KFun *func)
{
    int n;

    UNREFERENCED_PARAMETER(func);

    n = KFun::find(kfenc, 0, ne, f->sp[nargs - 1].string->text);
    if (n < 0) {
	EC->error("Unknown cipher");
    }
    (kfenc[n].func)(f, nargs - 1, &kfenc[n]);
    f->sp[1].del();
    f->sp[1] = f->sp[0];
    f->sp++;

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("decrypt", kf_decrypt, pt_decrypt, 0)
# else
extern String *P_decrypt_des_key (Frame*, String*);

char pt_decrypt[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 1, 1, 0, 8,
		      T_MIXED, T_STRING, T_MIXED };

/*
 * prepare a key for decryption
 */
void kf_dec_key(Frame *f, int nargs, Value *val)
{
    String *str;

    if (nargs != 1) {
	ext_runtime_error(f, "Too many arguments for kfun decrypt");
    }
    str = P_decrypt_des_key(f, f->sp->string);
    PUT_STRVAL_NOREF(val, str);
}

/*
 * decrypt
 */
void kf_dec(Frame *f, int nargs, Value *val)
{
    String *str;

    if (nargs != 2) {
	ext_runtime_error(f, "Too few arguments for kfun decrypt");
    }
    /* Given the proper key, DES is its own inverse */
    str = P_encrypt_des(f, f->sp[1].string, f->sp->string);
    PUT_STRVAL_NOREF(val, str);
}

/*
 * decrypt a string
 */
int kf_decrypt(Frame *f, int nargs, KFun *func)
{
    int n;

    UNREFERENCED_PARAMETER(func);

    n = KFun::find(kfdec, 0, nd, f->sp[nargs - 1].string->text);
    if (n < 0) {
	EC->error("Unknown cipher");
    }
    (kfdec[n].func)(f, nargs - 1, &kfdec[n]);
    f->sp[1].del();
    f->sp[1] = f->sp[0];
    f->sp++;

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ctime", kf_ctime, pt_ctime, 0)
# else
char pt_ctime[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_STRING, T_INT };

/*
 * convert a time value to a string
 */
int kf_ctime(Frame *f, int n, KFun *kf)
{
    char buf[26];

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(5);
    P_ctime(buf, f->sp->number);
    PUT_STRVAL(f->sp, String::create(buf, 24));

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("explode", kf_explode, pt_explode, 0)
# else
char pt_explode[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8,
		      T_STRING | (1 << REFSHIFT), T_STRING, T_STRING };

/*
 * internal version of memmem()
 */
static char *memxmem(char *mem, unsigned int mlen, char *str,
		     unsigned int slen)
{
    unsigned int i, checksum, mult, accu;
    char *p;

    if (mlen < slen) {
	return (char *) NULL;
    }
    p = (char *) memchr(mem, UCHAR(*str), mlen - slen + 1);
    if (p == (char *) NULL) {
	return (char *) NULL;
    }
    mlen -= p - mem;
    mem = p;

    /* compute checksum */
    checksum = 0;
    for (i = 0; i < slen; i++) {
	checksum = checksum * 0x14b + UCHAR(str[i]);
    }

    /* initialize accumulator and multiplicator */
    accu = 0;
    mult = 0xbfce8063;
    for (i = 0; i < slen; i++) {
	accu = accu * 0x14b + UCHAR(mem[i]);
	mult *= 0x14b;
    }

    for (;;) {
	if (accu == checksum && memcmp(mem, str, slen) == 0) {
	    return mem;
	}
	if (--mlen < slen) {
	    return (char *) NULL;
	}

	/* remove head byte from accumulator, add new tail byte */
	accu -= mult * UCHAR(*mem++);
	accu = accu * 0x14b + UCHAR(mem[slen - 1]);
    }

    return (char *) NULL;
}

/*
 * explode a string
 */
int kf_explode(Frame *f, int n, KFun *kf)
{
    unsigned int len, slen, size;
    char *p, *q, *s;
    Value *v;
    Array *a;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    p = f->sp[1].string->text;
    len = f->sp[1].string->len;
    s = f->sp->string->text;
    slen = f->sp->string->len;

    if (len == 0) {
	/*
	 * exploding "" always gives an empty array
	 */
	a = Array::create(f->data, 0);
    } else if (slen == 0) {
	/*
	 * the sepatator is ""; split string into single characters
	 */
	a = Array::create(f->data, len);
	for (v = a->elts; len > 0; v++, --len) {
	    PUT_STRVAL(v, String::create(p, 1));
	    p++;
	}
    } else {
	/*
	 * split up the string with the separator
	 */
	size = 1;
	if (len >= slen && memcmp(p, s, slen) == 0) {
	    /* skip leading separator */
	    p += slen;
	    len -= slen;
	}
	if (len > slen) {
	    --len;
	    while ((q=memxmem(p, len, s, slen)) != (char *) NULL) {
		q += slen;
		len -= q - p;
		p = q;
		size++;
	    }
	}

	a = Array::create(f->data, size);
	v = a->elts;

	p = f->sp[1].string->text;
	len = f->sp[1].string->len;
	if (len > slen && memcmp(p, s, slen) == 0) {
	    /* skip leading separator */
	    p += slen;
	    len -= slen;
	}
	while ((q=memxmem(p, len, s, slen)) != (char *) NULL) {
	    /* separator found */
	    PUT_STRVAL(v, String::create(p, q - p));
	    v++;
	    q += slen;
	    len -= q - p;
	    p = q;
	    --size;
	}
	if (size != 0) {
	    /* final array element */
	    PUT_STRVAL(v, String::create(p, len));
	}
    }

    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_ARRVAL(f->sp, a);
    f->addTicks((LPCint) 2 * a->size);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("implode", kf_implode, pt_implode, 0)
# else
char pt_implode[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_STRING,
		      T_STRING | (1 << REFSHIFT), T_STRING };

/*
 * implode an array
 */
int kf_implode(Frame *f, int n, KFun *kf)
{
    long len;
    unsigned int i, slen;
    char *p, *s;
    Value *v;
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    s = f->sp->string->text;
    slen = f->sp->string->len;

    /* first, determine the size of the imploded string */
    i = f->sp[1].array->size;
    f->addTicks(i);
    if (i != 0) {
	len = (i - 1) * (long) slen;	/* size of all separators */
	for (v = Dataspace::elts(f->sp[1].array); i > 0; v++, --i) {
	    if (v->type != T_STRING) {
		/* not a (string *) */
		return 1;
	    }
	    len += v->string->len;
	}
	str = String::create((char *) NULL, len);

	/* create the imploded string */
	p = str->text;
	for (i = f->sp[1].array->size, v -= i; i > 1; --i, v++) {
	    /* copy array part */
	    memcpy(p, v->string->text, v->string->len);
	    p += v->string->len;
	    /* copy separator */
	    memcpy(p, s, slen);
	    p += slen;
	}
	/* copy final array part */
	memcpy(p, v->string->text, v->string->len);
    } else {
	/* zero size array gives zero size string */
	str = String::create((char *) NULL, 0);
    }

    (f->sp++)->string->del();
    f->sp->array->del();
    PUT_STRVAL(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("random", kf_random, pt_random, 0)
# else
char pt_random[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * return a random number
 */
int kf_random(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    if (f->sp->number < 0) {
	return 1;
    }
    if (f->sp->number == 0) {
# ifdef LARGENUM
	PUT_INT(f->sp, ((LPCuint) (Uint) P_random() << 31) ^ (Uint) P_random());
    } else if (f->sp->number > 0xffffffffL) {
	PUT_INT(f->sp, (((LPCuint) (Uint) P_random() << 31) ^
			(Uint) P_random()) % f->sp->number);
# else
	PUT_INT(f->sp, (Uint) P_random() & 0x7fffffffL);
# endif
    } else {
	PUT_INT(f->sp, (Uint) P_random() % f->sp->number);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.sscanf", kf_unused, pt_unused, 0)
FUNCDEF("sscanf", kf_sscanf, pt_sscanf, 2)
# else
char pt_sscanf[] = { C_STATIC | C_ELLIPSIS, 2, 1, 0, 9, T_INT, T_STRING,
		     T_STRING, T_LVALUE };

/*
 * obtain a string to match from the format string
 */
unsigned int scan(char *f, unsigned int *flenp, char *buf)
{
    char *p;
    unsigned int flen;

    p = buf;
    flen = *flenp;
    while (flen != 0) {
	if (*f != '%') {
	    *p++ = *f++;
	    --flen;
	} else if (flen > 1 && f[1] == '%') {
	    *p++ = '%';
	    f += 2;
	    flen -= 2;
	} else {
	    break;
	}
    }
    *flenp -= flen;

    return p - buf;
}

/*
 * scan a string
 */
int kf_sscanf(Frame *f, int nargs, KFun *kf)
{
    struct {
	char type;			/* int, float or string */
	union {
	    FloatHigh fhigh;		/* high word of float */
	    ssizet len;			/* length of string */
	};
	union {
	    LPCint number;		/* number */
	    FloatLow flow;		/* low longword of float */
	    char *text;			/* text of string */
	};
    } results[MAX_LOCALS];
    unsigned int flen, slen, size;
    char *format, *x;
    unsigned int fl;
    int matches;
    char *s, *buffer, *match;
    LPCint i;
    Float flt;
    bool skip;
    Value *top, *elts;
    Array *a;

    UNREFERENCED_PARAMETER(kf);

    x = NULL;

    if (nargs < 2) {
	return -1;
    }
    top = f->sp + nargs - 2;
    if (top[1].type != T_STRING) {
	return 1;
    }
    s = top[1].string->text;
    slen = top[1].string->len;
    if (top[0].type != T_STRING) {
	return 2;
    }
    format = top[0].string->text;
    flen = top[0].string->len;

    buffer = ALLOCA(char, flen);
    matches = 0;
    nargs = 0;

    while (flen > 0) {
	fl = flen;
	size = scan(format, &fl, buffer);
	if (size != 0) {
	    if (size > slen || memcmp(buffer, s, size) != 0) {
		goto no_match;
	    }

	    s += size;
	    slen -= size;
	    format += fl;
	    flen -= fl;
	    if (flen == 0) {
		break;
	    }
	}

	/* skip first % */
	format++;
	--flen;

	/*
	 * check for %*
	 */
	if (*format == '*') {
	    /* no assignment */
	    format++;
	    --flen;
	    skip = TRUE;
	} else {
	    skip = FALSE;
	}

	--flen;
	switch (*format++) {
	case 's':
	    /* %s */
	    if (format[0] == '%' && format[1] != '%') {
		switch ((format[1] == '*') ? format[2] : format[1]) {
		case 'd':
		    /*
		     * %s%d
		     */
		    size = slen;
		    x = s;
		    while (!isdigit(*x)) {
			if (slen == 0) {
			    goto no_match;
			}
			if (x[0] == '-' && isdigit(x[1])) {
			    break;
			}
			x++;
			--slen;
		    }
		    size -= slen;
		    break;

		case 'f':
		    /*
		     * %s%f
		     */
		    size = slen;
		    x = s;
		    while (!isdigit(*x)) {
			if (slen == 0) {
			    goto no_match;
			}
			if ((x[0] == '.' && isdigit(x[1])) ||
			    (x[0] == '-' &&
			     (isdigit(x[1]) || (x[1] == '.' && isdigit(x[2])))))
			{
			    break;
			}
			x++;
			--slen;
		    }
		    size -= slen;
		    break;

		default:
		    AFREE(buffer);
		    EC->error("Bad sscanf format string");
		}
	    } else {
		/*
		 * %s followed by non-%
		 */
		if (flen == 0) {
		    /* match whole string */
		    size = slen;
		    x = s + slen;
		    slen = 0;
		} else {
		    fl = flen;
		    size = scan(format, &fl, buffer);
		    x = s + size;
		    match = memxmem(s, slen, buffer, size);
		    if (match == NULL) {
			goto no_match;
		    }
		    format += fl;
		    flen -= fl;
		    size = match - s;
		    x += size;
		    slen -= x - s;
		}
	    }

	    f->addTicks(8);
	    if (!skip) {
		results[nargs].type = T_STRING;
		results[nargs].len = size;
		results[nargs].text = s;
		nargs++;
	    }
	    s = x;
	    break;

	case 'd':
	    /* %d */
	    x = s;
	    while (slen != 0 && *x == ' ') {
		x++;
		--slen;
	    }
	    s = x;
	    i = strtoint(&s);
	    if (s == x) {
		goto no_match;
	    }
	    slen -= (s - x);

	    f->addTicks(8);
	    if (!skip) {
		results[nargs].type = T_INT;
		results[nargs].number = i;
		nargs++;
	    }
	    break;

	case 'f':
	    /* %f */
	    x = s;
	    while (slen != 0 && *x == ' ') {
		x++;
		--slen;
	    }
	    s = x;
	    if (!Float::atof(&s, &flt) || s == x) {
		goto no_match;
	    }
	    slen -= (s - x);

	    f->addTicks(8);
	    if (!skip) {
		results[nargs].type = T_FLOAT;
		results[nargs].fhigh = flt.high;
		results[nargs].flow = flt.low;
		nargs++;
	    }
	    break;

	case 'c':
	    /* %c */
	    if (slen == 0) {
		goto no_match;
	    }
	    f->addTicks(8);
	    if (!skip) {
		results[nargs].type = T_INT;
		results[nargs].number = UCHAR(*s);
		nargs++;
	    }
	    s++;
	    --slen;
	    break;

	default:
	    AFREE(buffer);
	    EC->error("Bad sscanf format string");
	}
	matches++;
    }

no_match:
    AFREE(buffer);
    a = Array::create(f->data, nargs);
    for (elts = a->elts, size = 0; size < nargs; elts++, size++) {
	switch (results[size].type) {
	case T_INT:
	    PUT_INTVAL(elts, results[size].number);
	    break;

	case T_FLOAT:
	    flt.high = results[size].fhigh;
	    flt.low = results[size].flow;
	    PUT_FLTVAL(elts, flt);
	    break;

	case T_STRING:
	    PUT_STRVAL(elts, String::create(results[size].text,
					    results[size].len));
	    break;
	}
    }

    top[1].string->del();
    PUT_INTVAL(&top[1], matches);
    top[0].string->del();
    memmove(f->sp + 1, f->sp, (top - f->sp) * sizeof(Value));

    PUT_ARRVAL(f->sp, a);
    f->kflv = TRUE;

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("1.sscanf", kf_old_sscanf, pt_old_sscanf, 1)
# else
char pt_old_sscanf[] = { C_STATIC | C_ELLIPSIS, 2, 1, 0, 9, T_INT, T_STRING,
			 T_STRING, T_LVALUE };

/*
 * scan a string
 */
int kf_old_sscanf(Frame *f, int nargs, KFun *kf)
{
    int n;
    char *pc;

    n = kf_sscanf(f, nargs, kf);
    if (n != 0) {
	return n;
    }
    f->kflv = FALSE;

    pc = f->pc;
# ifdef DEBUG
    if ((FETCH1U(pc) & I_INSTR_MASK) != I_STORES) {
	EC->fatal("stores expected");
    }
# else
    pc++;
# endif
    n = FETCH1U(pc);
    f->pc = pc;
    f->lvalues(n);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("parse_string", kf_parse_string, pt_parse_string, 0)
# else
char pt_parse_string[] = { C_TYPECHECKED | C_STATIC, 2, 1, 0, 9,
			   T_MIXED | (1 << REFSHIFT), T_STRING, T_STRING,
			   T_INT };

/*
 * parse a string
 */
int kf_parse_string(Frame *f, int nargs, KFun *kf)
{
    LPCint maxalt;
    Array *a;

    UNREFERENCED_PARAMETER(kf);

    if (nargs > 2) {
	maxalt = (f->sp++)->number + 1;
	if (maxalt <= 0) {
	    return 3;
	}
    } else {
	maxalt = 1;	/* default: just one valid parse tree */
    }

    if (OBJR(f->oindex)->flags & O_SPECIAL) {
	EC->error("parse_string() from special purpose object");
    }

    a = Parser::parse_string(f, f->sp[1].string, f->sp->string, maxalt);
    (f->sp++)->string->del();
    f->sp->string->del();

    if (a != (Array *) NULL) {
	/* return parse tree */
	PUT_ARRVAL(f->sp, a);
    } else {
	/* parsing failed */
	*f->sp = nil;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("hash_crc16", kf_hash_crc16, pt_hash_crc16, 0)
# else
char pt_hash_crc16[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 1, 1, 0, 8,
			 T_INT, T_STRING, T_STRING };

/*
 * Compute a 16 bit cyclic redundancy code for a string.
 * Based on "A PAINLESS GUIDE TO CRC ERROR DETECTION ALGORITHMS",
 * by Ross N. Williams.
 *
 *     Name:	"CRC-16/CCITT"	(supposedly)
 *     Width:	16
 *     Poly:	1021		(X^16 + X^12 + X^5 + 1)
 *     Init:	FFFF
 *     RefIn:	False
 *     RefOut:	False
 *     XorOut:	0000
 *     Check:	29B1
 */
int kf_hash_crc16(Frame *f, int nargs, KFun *kf)
{
    static unsigned short crctab[] = {
	0x0000, 0x2110, 0x4220, 0x6330, 0x8440, 0xa550, 0xc660, 0xe770,
	0x0881, 0x2991, 0x4aa1, 0x6bb1, 0x8cc1, 0xadd1, 0xcee1, 0xeff1,
	0x3112, 0x1002, 0x7332, 0x5222, 0xb552, 0x9442, 0xf772, 0xd662,
	0x3993, 0x1883, 0x7bb3, 0x5aa3, 0xbdd3, 0x9cc3, 0xfff3, 0xdee3,
	0x6224, 0x4334, 0x2004, 0x0114, 0xe664, 0xc774, 0xa444, 0x8554,
	0x6aa5, 0x4bb5, 0x2885, 0x0995, 0xeee5, 0xcff5, 0xacc5, 0x8dd5,
	0x5336, 0x7226, 0x1116, 0x3006, 0xd776, 0xf666, 0x9556, 0xb446,
	0x5bb7, 0x7aa7, 0x1997, 0x3887, 0xdff7, 0xfee7, 0x9dd7, 0xbcc7,
	0xc448, 0xe558, 0x8668, 0xa778, 0x4008, 0x6118, 0x0228, 0x2338,
	0xccc9, 0xedd9, 0x8ee9, 0xaff9, 0x4889, 0x6999, 0x0aa9, 0x2bb9,
	0xf55a, 0xd44a, 0xb77a, 0x966a, 0x711a, 0x500a, 0x333a, 0x122a,
	0xfddb, 0xdccb, 0xbffb, 0x9eeb, 0x799b, 0x588b, 0x3bbb, 0x1aab,
	0xa66c, 0x877c, 0xe44c, 0xc55c, 0x222c, 0x033c, 0x600c, 0x411c,
	0xaeed, 0x8ffd, 0xeccd, 0xcddd, 0x2aad, 0x0bbd, 0x688d, 0x499d,
	0x977e, 0xb66e, 0xd55e, 0xf44e, 0x133e, 0x322e, 0x511e, 0x700e,
	0x9fff, 0xbeef, 0xdddf, 0xfccf, 0x1bbf, 0x3aaf, 0x599f, 0x788f,
	0x8891, 0xa981, 0xcab1, 0xeba1, 0x0cd1, 0x2dc1, 0x4ef1, 0x6fe1,
	0x8010, 0xa100, 0xc230, 0xe320, 0x0450, 0x2540, 0x4670, 0x6760,
	0xb983, 0x9893, 0xfba3, 0xdab3, 0x3dc3, 0x1cd3, 0x7fe3, 0x5ef3,
	0xb102, 0x9012, 0xf322, 0xd232, 0x3542, 0x1452, 0x7762, 0x5672,
	0xeab5, 0xcba5, 0xa895, 0x8985, 0x6ef5, 0x4fe5, 0x2cd5, 0x0dc5,
	0xe234, 0xc324, 0xa014, 0x8104, 0x6674, 0x4764, 0x2454, 0x0544,
	0xdba7, 0xfab7, 0x9987, 0xb897, 0x5fe7, 0x7ef7, 0x1dc7, 0x3cd7,
	0xd326, 0xf236, 0x9106, 0xb016, 0x5766, 0x7676, 0x1546, 0x3456,
	0x4cd9, 0x6dc9, 0x0ef9, 0x2fe9, 0xc899, 0xe989, 0x8ab9, 0xaba9,
	0x4458, 0x6548, 0x0678, 0x2768, 0xc018, 0xe108, 0x8238, 0xa328,
	0x7dcb, 0x5cdb, 0x3feb, 0x1efb, 0xf98b, 0xd89b, 0xbbab, 0x9abb,
	0x754a, 0x545a, 0x376a, 0x167a, 0xf10a, 0xd01a, 0xb32a, 0x923a,
	0x2efd, 0x0fed, 0x6cdd, 0x4dcd, 0xaabd, 0x8bad, 0xe89d, 0xc98d,
	0x267c, 0x076c, 0x645c, 0x454c, 0xa23c, 0x832c, 0xe01c, 0xc10c,
	0x1fef, 0x3eff, 0x5dcf, 0x7cdf, 0x9baf, 0xbabf, 0xd98f, 0xf89f,
	0x176e, 0x367e, 0x554e, 0x745e, 0x932e, 0xb23e, 0xd10e, 0xf01e
    };
    unsigned short crc;
    int i;
    ssizet len;
    char *p;
    LPCint cost;

    UNREFERENCED_PARAMETER(kf);

    cost = 0;
    for (i = nargs; --i >= 0; ) {
	cost += f->sp[i].string->len;
    }
    cost = 3 * nargs + (cost >> 2);
    if (!f->rlim->noticks && f->rlim->ticks <= cost) {
	f->rlim->ticks = 0;
	EC->error("Out of ticks");
    }
    f->addTicks(cost);

    crc = 0xffff;
    for (i = nargs; --i >= 0; ) {
	p = f->sp[i].string->text;
	for (len = f->sp[i].string->len; len != 0; --len) {
	    crc = (crc >> 8) ^ crctab[UCHAR(crc ^ *p++)];
	}
	f->sp[i].string->del();
    }
    crc = (crc >> 8) + (crc << 8);

    f->sp += nargs - 1;
    PUT_INTVAL(f->sp, crc);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("hash_crc32", kf_hash_crc32, pt_hash_crc32, 0)
# else
char pt_hash_crc32[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 1, 1, 0, 8,
			 T_INT, T_STRING, T_STRING };

/*
 * Compute a 32 bit cyclic redundancy code for a string.
 * Based on "A PAINLESS GUIDE TO CRC ERROR DETECTION ALGORITHMS",
 * by Ross N. Williams.
 *
 *     Name:	"CRC-32"	(as in libz)
 *     Width:	16
 *     Poly:	04C11DB7
 *     Init:	FFFFFFFF
 *     RefIn:	True
 *     RefOut:	True
 *     XorOut:	FFFFFFFF
 *     Check:	CBF43926
 */
int kf_hash_crc32(Frame *f, int nargs, KFun *kf)
{
    static Uint crctab[] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
    };
    Uint crc;
    int i;
    ssizet len;
    char *p;
    LPCint cost;

    UNREFERENCED_PARAMETER(kf);

    cost = 0;
    for (i = nargs; --i >= 0; ) {
	cost += f->sp[i].string->len;
    }
    cost = 3 * nargs + (cost >> 2);
    if (!f->rlim->noticks && f->rlim->ticks <= cost) {
	f->rlim->ticks = 0;
	EC->error("Out of ticks");
    }
    f->addTicks(cost);

    crc = 0xffffffff;
    for (i = nargs; --i >= 0; ) {
	p = f->sp[i].string->text;
	for (len = f->sp[i].string->len; len != 0; --len) {
	    crc = (crc >> 8) ^ crctab[UCHAR(crc ^ *p++)];
	}
	f->sp[i].string->del();
    }
    crc ^= 0xffffffffL;

    f->sp += nargs - 1;
    PUT_INTVAL(f->sp, crc);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.hash_md5", kf_unused, pt_unused, 0)
FUNCDEF("0.hash_sha1", kf_unused, pt_unused, 0)
FUNCDEF("hash_string", kf_hash_string, pt_hash_string, 0)
# else
extern char *P_crypt (char*, char*);

char pt_hash_string[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 2, 1, 0, 9,
			  T_STRING, T_STRING, T_STRING, T_STRING };

/*
 * hash a string with Unix password crypt
 */
void kf_xcrypt(Frame *f, int nargs, Value *val)
{
    static char salts[] =
	    "0123456789./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char s[3];
    String *str;

    if (nargs > 2) {
	ext_runtime_error(f, "Too many arguments for kfun hash_string");
    }
    if (nargs == 2 && f->sp->string->len >= 2) {
	/* fixed salt */
	s[0] = f->sp->string->text[0];
	s[1] = f->sp->string->text[1];
    } else {
	Uint n;

	/* random salt */
	n = P_random();
	s[0] = salts[n & 63];
	s[1] = salts[(n >> 8) & 63];
    }
    s[2] = '\0';

    f->addTicks(900);
    str = String::create(P_crypt(f->sp[nargs - 1].string->text, s), 13);
    PUT_STRVAL_NOREF(val, str);
}

# define ROTL(x, s)			(((x) << s) | ((x) >> (32 - s)))
# define R1(a, b, c, d, Mj, s, ti)	(a += (((c ^ d) & b) ^ d) + Mj + ti, \
					 a = b + ROTL(a, s))
# define R2(a, b, c, d, Mj, s, ti)	(a += (((b ^ c) & d) ^ c) + Mj + ti, \
					 a = b + ROTL(a, s))
# define R3(a, b, c, d, Mj, s, ti)	(a += (b ^ c ^ d) + Mj + ti,	     \
					 a = b + ROTL(a, s))
# define R4(a, b, c, d, Mj, s, ti)	(a += (c ^ (b | ~d)) + Mj + ti,	     \
					 a = b + ROTL(a, s))

/*
 * MD5 message digest.  See "Applied Cryptography" by Bruce
 * Schneier, Second Edition, p. 436-441.
 */
void hash_md5_start(Uint *digest)
{
    /*
     * These constants must apparently be little-endianized, though AC2 does
     * not explicitly say so.
     */
    digest[0] = 0x67452301L;
    digest[1] = 0xefcdab89L;
    digest[2] = 0x98badcfeL;
    digest[3] = 0x10325476L;
}

/*
 * add another 512 bit block to the message digest
 */
void hash_md5_block(Uint *ABCD, char *block)
{
    Uint M[16];
    int i, j;
    Uint a, b, c, d;

    for (i = j = 0; i < 16; i++, j += 4) {
	M[i] = UCHAR(block[j + 0]) | (UCHAR(block[j + 1]) << 8) |
	       (UCHAR(block[j + 2]) << 16) | (UCHAR(block[j + 3]) << 24);

    }

    a = ABCD[0];
    b = ABCD[1];
    c = ABCD[2];
    d = ABCD[3];

    R1(a, b, c, d, M[ 0],  7, 0xd76aa478L);
    R1(d, a, b, c, M[ 1], 12, 0xe8c7b756L);
    R1(c, d, a, b, M[ 2], 17, 0x242070dbL);
    R1(b, c, d, a, M[ 3], 22, 0xc1bdceeeL);
    R1(a, b, c, d, M[ 4],  7, 0xf57c0fafL);
    R1(d, a, b, c, M[ 5], 12, 0x4787c62aL);
    R1(c, d, a, b, M[ 6], 17, 0xa8304613L);
    R1(b, c, d, a, M[ 7], 22, 0xfd469501L);
    R1(a, b, c, d, M[ 8],  7, 0x698098d8L);
    R1(d, a, b, c, M[ 9], 12, 0x8b44f7afL);
    R1(c, d, a, b, M[10], 17, 0xffff5bb1L);
    R1(b, c, d, a, M[11], 22, 0x895cd7beL);
    R1(a, b, c, d, M[12],  7, 0x6b901122L);
    R1(d, a, b, c, M[13], 12, 0xfd987193L);
    R1(c, d, a, b, M[14], 17, 0xa679438eL);
    R1(b, c, d, a, M[15], 22, 0x49b40821L);

    R2(a, b, c, d, M[ 1],  5, 0xf61e2562L);
    R2(d, a, b, c, M[ 6],  9, 0xc040b340L);
    R2(c, d, a, b, M[11], 14, 0x265e5a51L);
    R2(b, c, d, a, M[ 0], 20, 0xe9b6c7aaL);
    R2(a, b, c, d, M[ 5],  5, 0xd62f105dL);
    R2(d, a, b, c, M[10],  9, 0x02441453L);
    R2(c, d, a, b, M[15], 14, 0xd8a1e681L);
    R2(b, c, d, a, M[ 4], 20, 0xe7d3fbc8L);
    R2(a, b, c, d, M[ 9],  5, 0x21e1cde6L);
    R2(d, a, b, c, M[14],  9, 0xc33707d6L);
    R2(c, d, a, b, M[ 3], 14, 0xf4d50d87L);
    R2(b, c, d, a, M[ 8], 20, 0x455a14edL);
    R2(a, b, c, d, M[13],  5, 0xa9e3e905L);
    R2(d, a, b, c, M[ 2],  9, 0xfcefa3f8L);
    R2(c, d, a, b, M[ 7], 14, 0x676f02d9L);
    R2(b, c, d, a, M[12], 20, 0x8d2a4c8aL);

    R3(a, b, c, d, M[ 5],  4, 0xfffa3942L);
    R3(d, a, b, c, M[ 8], 11, 0x8771f681L);
    R3(c, d, a, b, M[11], 16, 0x6d9d6122L);
    R3(b, c, d, a, M[14], 23, 0xfde5380cL);
    R3(a, b, c, d, M[ 1],  4, 0xa4beea44L);
    R3(d, a, b, c, M[ 4], 11, 0x4bdecfa9L);
    R3(c, d, a, b, M[ 7], 16, 0xf6bb4b60L);
    R3(b, c, d, a, M[10], 23, 0xbebfbc70L);
    R3(a, b, c, d, M[13],  4, 0x289b7ec6L);
    R3(d, a, b, c, M[ 0], 11, 0xeaa127faL);
    R3(c, d, a, b, M[ 3], 16, 0xd4ef3085L);
    R3(b, c, d, a, M[ 6], 23, 0x04881d05L);
    R3(a, b, c, d, M[ 9],  4, 0xd9d4d039L);
    R3(d, a, b, c, M[12], 11, 0xe6db99e5L);
    R3(c, d, a, b, M[15], 16, 0x1fa27cf8L);
    R3(b, c, d, a, M[ 2], 23, 0xc4ac5665L);

    R4(a, b, c, d, M[ 0],  6, 0xf4292244L);
    R4(d, a, b, c, M[ 7], 10, 0x432aff97L);
    R4(c, d, a, b, M[14], 15, 0xab9423a7L);
    R4(b, c, d, a, M[ 5], 21, 0xfc93a039L);
    R4(a, b, c, d, M[12],  6, 0x655b59c3L);
    R4(d, a, b, c, M[ 3], 10, 0x8f0ccc92L);
    R4(c, d, a, b, M[10], 15, 0xffeff47dL);
    R4(b, c, d, a, M[ 1], 21, 0x85845dd1L);
    R4(a, b, c, d, M[ 8],  6, 0x6fa87e4fL);
    R4(d, a, b, c, M[15], 10, 0xfe2ce6e0L);
    R4(c, d, a, b, M[ 6], 15, 0xa3014314L);
    R4(b, c, d, a, M[13], 21, 0x4e0811a1L);
    R4(a, b, c, d, M[ 4],  6, 0xf7537e82L);
    R4(d, a, b, c, M[11], 10, 0xbd3af235L);
    R4(c, d, a, b, M[ 2], 15, 0x2ad7d2bbL);
    R4(b, c, d, a, M[ 9], 21, 0xeb86d391L);

    ABCD[0] += a;
    ABCD[1] += b;
    ABCD[2] += c;
    ABCD[3] += d;
}

/*
 * finish up MD5 hash
 */
void hash_md5_end(char *hash, Uint *digest, char *buffer, unsigned int bufsz,
		  Uint length)
{
    int i;

    /* append padding and digest final block(s) */
    buffer[bufsz++] = '\x80';
    if (bufsz > 56) {
	memset(buffer + bufsz, '\0', 64 - bufsz);
	hash_md5_block(digest, buffer);
	bufsz = 0;
    }
    memset(buffer + bufsz, '\0', 64 - bufsz);
    buffer[56] = length << 3;
    buffer[57] = length >> 5;
    buffer[58] = length >> 13;
    buffer[59] = length >> 21;
    buffer[60] = length >> 29;
    hash_md5_block(digest, buffer);

    for (i = 0; i < 4; hash += 4, i++) {
	hash[0] = digest[i];
	hash[1] = digest[i] >> 8;
	hash[2] = digest[i] >> 16;
	hash[3] = digest[i] >> 24;
    }
}

/*
 * SHA-1 message digest.  See FIPS 180-2.
 */
static LPCint hash_sha1_start(Frame *f, int nargs, Uint *digest)
{
    LPCint cost;

    digest[0] = 0x67452301L;
    digest[1] = 0xefcdab89L;
    digest[2] = 0x98badcfeL;
    digest[3] = 0x10325476L;
    digest[4] = 0xc3d2e1f0L;

    cost = 3 * nargs + 64;
    while (--nargs >= 0) {
	cost += f->sp[nargs].string->len;
    }
    return cost;
}

/*
 * add another 512 bit block to the message digest
 */
static void hash_sha1_block(Uint *ABCDE, char *block)
{
    Uint W[80];
    int i, j;
    Uint a, b, c, d, e, t;

    for (i = j = 0; i < 16; i++, j += 4) {
       W[i] = (UCHAR(block[j + 0]) << 24) | (UCHAR(block[j + 1]) << 16) |
	      (UCHAR(block[j + 2]) << 8) | UCHAR(block[j + 3]);

    }
    while (i < 80) {
	W[i] = ROTL(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);
	i++;
    }

    a = ABCDE[0];
    b = ABCDE[1];
    c = ABCDE[2];
    d = ABCDE[3];
    e = ABCDE[4];

    for (i = 0; i < 20; i++) {
	t = ROTL(a, 5) + (((c ^ d) & b) ^ d) + e + W[i] + 0x5a827999L;
	e = d;
	d = c;
	c = ROTL(b, 30);
	b = a;
	a = t;
    }
    while (i < 40) {
	t = ROTL(a, 5) + (b ^ c ^ d) + e + W[i] + 0x6ed9eba1L;
	e = d;
	d = c;
	c = ROTL(b, 30);
	b = a;
	a = t;
	i++;
    }
    while (i < 60) {
	t = ROTL(a, 5) + ((b & c) | ((b | c) & d)) + e + W[i] + 0x8f1bbcdcL;
	e = d;
	d = c;
	c = ROTL(b, 30);
	b = a;
	a = t;
	i++;
    }
    while (i < 80) {
	t = ROTL(a, 5) + (b ^ c ^ d) + e + W[i] + 0xca62c1d6L;
	e = d;
	d = c;
	c = ROTL(b, 30);
	b = a;
	a = t;
	i++;
    }

    ABCDE[0] += a;
    ABCDE[1] += b;
    ABCDE[2] += c;
    ABCDE[3] += d;
    ABCDE[4] += e;
}


/*
 * finish up SHA-1 hash
 */
static String *hash_sha1_end(Uint *digest, char *buffer, unsigned int bufsz, Uint length)
{
    int i;

    /* append padding and digest final block(s) */
    buffer[bufsz++] = '\x80';
    if (bufsz > 56) {
	memset(buffer + bufsz, '\0', 64 - bufsz);
	hash_sha1_block(digest, buffer);
	bufsz = 0;
    }
    memset(buffer + bufsz, '\0', 64 - bufsz);
    buffer[59] = length >> 29;
    buffer[60] = length >> 21;
    buffer[61] = length >> 13;
    buffer[62] = length >> 5;
    buffer[63] = length << 3;
    hash_sha1_block(digest, buffer);

    for (bufsz = i = 0; i < 5; bufsz += 4, i++) {
	buffer[bufsz + 0] = digest[i] >> 24;
	buffer[bufsz + 1] = digest[i] >> 16;
	buffer[bufsz + 2] = digest[i] >> 8;
	buffer[bufsz + 3] = digest[i];
    }
    return String::create(buffer, 20);
}

/*
 * hash string blocks with a given function
 */
static Uint hash_blocks(Frame *f, int nargs, Uint *digest, char *buffer,
	unsigned short *bufsize, unsigned int blocksz,
	void (*hash_block) (Uint*, char*))
{
    ssizet len;
    unsigned short bufsz;
    char *p;
    Uint length;

    length = 0;
    bufsz = 0;
    while (--nargs >= 0) {
	len = f->sp[nargs].string->len;
	if (len != 0) {
	    length += len;
	    p = f->sp[nargs].string->text;
	    if (bufsz != 0) {
		unsigned short size;

		/* fill buffer and digest */
		size = blocksz - bufsz;
		if (size > len) {
		    size = len;
		}
		memcpy(buffer + bufsz, p, size);
		p += size;
		len -= size;
		bufsz += size;

		if (bufsz == blocksz) {
		    (*hash_block)(digest, buffer);
		    bufsz = 0;
		}
	    }

	    while (len >= blocksz) {
		/* digest directly from string */
		(*hash_block)(digest, p);
		p += blocksz;
		len -= blocksz;
	    }

	    if (len != 0) {
		/* put remainder in buffer */
		memcpy(buffer, p, bufsz = len);
	    }
	}
    }

    *bufsize = bufsz;
    return length;
}

/*
 * compute MD5 hash
 */
void kf_md5(Frame *f, int nargs, Value *val)
{
    char buffer[64];
    Uint digest[5];
    LPCint cost;
    int i;
    Uint length;
    unsigned short bufsz;
    String *str;

    cost = 3 * nargs + 64;
    for (i = nargs; i > 0; ) {
	cost += f->sp[--i].string->len;
    }
    if (!f->rlim->noticks && f->rlim->ticks <= cost) {
	f->rlim->ticks = 0;
	ext_runtime_error(f, "Out of ticks");
    }
    f->addTicks(cost);

    hash_md5_start(digest);
    length = hash_blocks(f, nargs, digest, buffer, &bufsz, 64, &hash_md5_block);
    hash_md5_end(buffer, digest, buffer, bufsz, length);
    str = String::create(buffer, 16);
    PUT_STRVAL_NOREF(val, str);
}

/*
 * compute SHA1 hash
 */
void kf_sha1(Frame *f, int nargs, Value *val)
{
    char buffer[64];
    Uint digest[5];
    LPCint cost;
    Uint length;
    unsigned short bufsz;
    String *str;

    cost = hash_sha1_start(f, nargs, digest);
    if (!f->rlim->noticks && f->rlim->ticks <= cost) {
	f->rlim->ticks = 0;
	ext_runtime_error(f, "Out of ticks");
    }
    f->addTicks(cost);

    length = hash_blocks(f, nargs, digest, buffer, &bufsz, 64,
			 &hash_sha1_block);
    str = hash_sha1_end(digest, buffer, bufsz, length);
    PUT_STRVAL_NOREF(val, str);
}

/*
 * hash a string
 */
int kf_hash_string(Frame *f, int nargs, KFun *func)
{
    int n;

    UNREFERENCED_PARAMETER(func);

    n = KFun::find(kfhsh, 0, nh, f->sp[nargs - 1].string->text);
    if (n < 0) {
	EC->error("Unknown hash algorithm");
    }
    (kfhsh[n].func)(f, nargs - 1, &kfhsh[n]);
    f->sp[1].del();
    f->sp[1] = f->sp[0];
    f->sp++;

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("crypt", kf_crypt, pt_crypt, 0)
# else
char pt_crypt[] = { C_TYPECHECKED | C_STATIC, 1, 1, 0, 8, T_STRING, T_STRING,
		    T_STRING };

/*
 * hash_string("crypt", ...)
 */
int kf_crypt(Frame *f, int nargs, KFun *kf)
{
    Value val;

    UNREFERENCED_PARAMETER(kf);

    if (nargs > 2) {
	EC->error("Too many arguments for kfun crypt");
    }
    kf_xcrypt(f, nargs, &val);
    val.ref();
    f->pop(nargs);
    *--f->sp = val;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.asn_add", kf_asn_add, pt_asn_add_old, 0)
FUNCDEF("asn_add", kf_asn_add, pt_asn_add, 1)
# else
char pt_asn_add_old[] = { C_TYPECHECKED | C_STATIC, 3, 0, 0, 9, T_STRING,
			  T_STRING, T_STRING, T_STRING };
char pt_asn_add[] = { C_TYPECHECKED | C_STATIC, 2, 1, 0, 9, T_STRING, T_STRING,
		      T_STRING, T_STRING };

/*
 * add two arbitrary precision numbers
 */
int kf_asn_add(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(kf);

    if (n < 3) {
	return -1;
    }
    str = ASN::add(f, f->sp[2].string, f->sp[1].string, f->sp[0].string);
    (f->sp++)->string->del();
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.asn_sub", kf_asn_sub, pt_asn_sub_old, 0)
FUNCDEF("asn_sub", kf_asn_sub, pt_asn_sub, 1)
# else
char pt_asn_sub_old[] = { C_TYPECHECKED | C_STATIC, 3, 0, 0, 9, T_STRING,
			  T_STRING, T_STRING, T_STRING };
char pt_asn_sub[] = { C_TYPECHECKED | C_STATIC, 2, 1, 0, 9, T_STRING, T_STRING,
		      T_STRING, T_STRING };

/*
 * subtract arbitrary precision numbers
 */
int kf_asn_sub(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(kf);

    if (n < 3) {
	return -1;
    }
    str = ASN::sub(f, f->sp[2].string, f->sp[1].string, f->sp[0].string);
    (f->sp++)->string->del();
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asn_cmp", kf_asn_cmp, pt_asn_cmp, 0)
# else
char pt_asn_cmp[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_INT, T_STRING,
		      T_STRING };

/*
 * subtract arbitrary precision numbers
 */
int kf_asn_cmp(Frame *f, int n, KFun *kf)
{
    int cmp;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    cmp = ASN::cmp(f, f->sp[1].string, f->sp[0].string);
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_INTVAL(f->sp, cmp);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.asn_mult", kf_asn_mult, pt_asn_mult_old, 0)
FUNCDEF("asn_mult", kf_asn_mult, pt_asn_mult, 1)
# else
char pt_asn_mult_old[] = { C_TYPECHECKED | C_STATIC, 3, 0, 0, 9, T_STRING,
			   T_STRING, T_STRING, T_STRING };
char pt_asn_mult[] = { C_TYPECHECKED | C_STATIC, 2, 1, 0, 9, T_STRING, T_STRING,
		       T_STRING, T_STRING };

/*
 * multiply arbitrary precision numbers
 */
int kf_asn_mult(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(kf);

    if (n < 3) {
	return -1;
    }
    str = ASN::mult(f, f->sp[2].string, f->sp[1].string, f->sp[0].string);
    (f->sp++)->string->del();
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.asn_div", kf_asn_div, pt_asn_div_old, 0)
FUNCDEF("asn_div", kf_asn_div, pt_asn_div, 1)
# else
char pt_asn_div_old[] = { C_TYPECHECKED | C_STATIC, 3, 0, 0, 9, T_STRING,
			  T_STRING, T_STRING, T_STRING };
char pt_asn_div[] = { C_TYPECHECKED | C_STATIC, 2, 1, 0, 9, T_STRING, T_STRING,
		      T_STRING, T_STRING };

/*
 * divide arbitrary precision numbers
 */
int kf_asn_div(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(kf);

    if (n < 3) {
	return -1;
    }
    str = ASN::div(f, f->sp[2].string, f->sp[1].string, f->sp[0].string);
    (f->sp++)->string->del();
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asn_mod", kf_asn_mod, pt_asn_mod, 0)
# else
char pt_asn_mod[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING,
		      T_STRING };

/*
 * modulus of arbitrary precision number
 */
int kf_asn_mod(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    str = ASN::mod(f, f->sp[1].string, f->sp[0].string);
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.asn_pow", kf_asn_pow, pt_asn_pow_old, 0)
FUNCDEF("asn_pow", kf_asn_pow, pt_asn_pow, 1)
# else
char pt_asn_pow_old[] = { C_TYPECHECKED | C_STATIC, 3, 0, 0, 9, T_STRING,
			  T_STRING, T_STRING, T_STRING };
char pt_asn_pow[] = { C_TYPECHECKED | C_STATIC, 2, 1, 0, 9, T_STRING, T_STRING,
		      T_STRING, T_STRING };

/*
 * power of an arbitrary precision number
 */
int kf_asn_pow(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(kf);

    if (n < 3) {
	return -1;
    }
    str = ASN::pow(f, f->sp[2].string, f->sp[1].string, f->sp[0].string);
    (f->sp++)->string->del();
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asn_modinv", kf_asn_modinv, pt_asn_modinv, 0)
# else
char pt_asn_modinv[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_STRING,
			 T_STRING, T_STRING };

/*
 * modular inverse of arbitrary precision number
 */
int kf_asn_modinv(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    str = ASN::modinv(f, f->sp[1].string, f->sp[0].string);
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.asn_lshift", kf_asn_lshift, pt_asn_lshift_old, 0)
FUNCDEF("asn_lshift", kf_asn_lshift, pt_asn_lshift, 1)
# else
char pt_asn_lshift_old[] = { C_TYPECHECKED | C_STATIC, 3, 0, 0, 9, T_STRING,
			     T_STRING, T_INT, T_STRING };
char pt_asn_lshift[] = { C_TYPECHECKED | C_STATIC, 2, 1, 0, 9, T_STRING,
			 T_STRING, T_INT, T_STRING };

/*
 * left shift an arbitrary precision number
 */
int kf_asn_lshift(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(kf);

    if (n < 3) {
	return -1;
    }
    str = ASN::lshift(f, f->sp[2].string, f->sp[1].number, f->sp->string);
    f->sp->string->del();
    f->sp += 2;
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asn_rshift", kf_asn_rshift, pt_asn_rshift, 0)
# else
char pt_asn_rshift[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_STRING,
			 T_STRING, T_INT };

/*
 * right shift of arbitrary precision number
 */
int kf_asn_rshift(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    str = ASN::rshift(f, f->sp[1].string, f->sp->number);
    f->sp++;
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asn_and", kf_asn_and, pt_asn_and, 0)
# else
char pt_asn_and[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING,
		      T_STRING };

/*
 * logical and of arbitrary precision numbers
 */
int kf_asn_and(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    str = ASN::_and(f, f->sp[1].string, f->sp->string);
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asn_or", kf_asn_or, pt_asn_or, 0)
# else
char pt_asn_or[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING,
		     T_STRING };

/*
 * logical or of arbitrary precision numbers
 */
int kf_asn_or(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    str = ASN::_or(f, f->sp[1].string, f->sp->string);
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asn_xor", kf_asn_xor, pt_asn_xor, 0)
# else
char pt_asn_xor[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING,
		      T_STRING };

/*
 * logical xor of arbitrary precision numbers
 */
int kf_asn_xor(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    str = ASN::_xor(f, f->sp[1].string, f->sp->string);
    (f->sp++)->string->del();
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif
