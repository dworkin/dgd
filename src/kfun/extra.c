# ifndef FUNCDEF
# define INCLUDE_CTYPE
# include "kfun.h"
# include "parse.h"
# endif


# ifdef FUNCDEF
FUNCDEF("crypt", kf_crypt, pt_crypt)
# else
char pt_crypt[] = { C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS, T_STRING, 2,
		    T_STRING | T_VARARGS, T_STRING };

/*
 * NAME:	kfun->crypt()
 * DESCRIPTION:	encrypt a string
 */
int kf_crypt(f, nargs)
register frame *f;
int nargs;
{
    static char salts[] =
	    "0123456789./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char salt[3], *p;
    
    if (nargs == 2 && f->sp->u.string->len >= 2) {
	/* fixed salt */
	salt[0] = f->sp->u.string->text[0];
	salt[1] = f->sp->u.string->text[1];
    } else {
	/* random salt */
	salt[0] = salts[P_random() % 64];
	salt[1] = salts[P_random() % 64];
    }
    salt[2] = '\0';
    if (nargs == 2) {
	str_del((f->sp++)->u.string);
    }

    i_add_ticks(f, 400);
    p = P_crypt(f->sp->u.string->text, salt);
    str_del(f->sp->u.string);
    PUT_STR(f->sp, str_new(p, (long) strlen(p)));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ctime", kf_ctime, pt_ctime)
# else
char pt_ctime[] = { C_TYPECHECKED | C_STATIC, T_STRING, 1, T_INT };

/*
 * NAME:	kfun->ctime()
 * DESCRIPTION:	convert a time value to a string
 */
int kf_ctime(f)
frame *f;
{
    char buf[26];

    i_add_ticks(f, 5);
    P_ctime(buf, f->sp->u.number);
    PUT_STRVAL(f->sp, str_new(buf, 24L));

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("explode", kf_explode, pt_explode)
# else
char pt_explode[] = { C_TYPECHECKED | C_STATIC, T_STRING | (1 << REFSHIFT), 2,
		      T_STRING, T_STRING };

/*
 * NAME:	kfun->explode()
 * DESCRIPTION:	explode a string
 */
int kf_explode(f)
register frame *f;
{
    register unsigned int len, slen, size;
    register char *p, *s;
    register value *v;
    array *a;

    p = f->sp[1].u.string->text;
    len = f->sp[1].u.string->len;
    s = f->sp->u.string->text;
    slen = f->sp->u.string->len;

    if (len == 0) {
	/*
	 * exploding "" always gives an empty array
	 */
	a = arr_new(f->data, 0L);
    } else if (slen == 0) {
	/*
	 * the sepatator is ""; split string into single characters
	 */
	a = arr_new(f->data, (long) len);
	for (v = a->elts; len > 0; v++, --len) {
	    PUT_STRVAL(v, str_new(p, 1L));
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
	while (len > slen) {
	    if (memcmp(p, s, slen) == 0) {
		/* separator found */
		p += slen;
		len -= slen;
		size++;
	    } else {
		/* next char */
		p++;
		--len;
	    }
	}

	a = arr_new(f->data, (long) size);
	v = a->elts;

	p = f->sp[1].u.string->text;
	len = f->sp[1].u.string->len;
	size = 0;
	if (len > slen && memcmp(p, s, slen) == 0) {
	    /* skip leading separator */
	    p += slen;
	    len -= slen;
	}
	while (len > slen) {
	    if (memcmp(p, s, slen) == 0) {
		/* separator found */
		PUT_STRVAL(v, str_new(p - size, (long) size));
		v++;
		p += slen;
		len -= slen;
		size = 0;
	    } else {
		/* next char */
		p++;
		--len;
		size++;
	    }
	}
	if (len != slen || memcmp(p, s, slen) != 0) {
	    /* remainder isn't a sepatator */
	    size += len;
	    p += len;
	}
	/* final array element */
	PUT_STRVAL(v, str_new(p - size, (long) size));
    }

    str_del((f->sp++)->u.string);
    str_del(f->sp->u.string);
    PUT_ARRVAL(f->sp, a);
    i_add_ticks(f, (Int) 2 * a->size);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("implode", kf_implode, pt_implode)
# else
char pt_implode[] = { C_TYPECHECKED | C_STATIC, T_STRING, 2,
		      T_STRING | (1 << REFSHIFT), T_STRING };

/*
 * NAME:	kfun->implode()
 * DESCRIPTION:	implode an array
 */
int kf_implode(f)
register frame *f;
{
    register long len;
    register unsigned int i, slen;
    register char *p, *s;
    register value *v;
    string *str;

    s = f->sp->u.string->text;
    slen = f->sp->u.string->len;

    /* first, determine the size of the imploded string */
    i = f->sp[1].u.array->size;
    i_add_ticks(f, i);
    if (i != 0) {
	len = (i - 1) * (long) slen;	/* size of all separators */
	for (v = d_get_elts(f->sp[1].u.array); i > 0; v++, --i) {
	    if (v->type != T_STRING) {
		/* not a (string *) */
		return 1;
	    }
	    len += v->u.string->len;
	}
	str = str_new((char *) NULL, len);

	/* create the imploded string */
	p = str->text;
	for (i = f->sp[1].u.array->size, v -= i; i > 1; --i, v++) {
	    /* copy array part */
	    memcpy(p, v->u.string->text, v->u.string->len);
	    p += v->u.string->len;
	    /* copy separator */
	    memcpy(p, s, slen);
	    p += slen;
	}
	/* copy final array part */
	memcpy(p, v->u.string->text, v->u.string->len);
    } else {
	/* zero size array gives zero size string */
	str = str_new((char *) NULL, 0L);
    }

    str_del((f->sp++)->u.string);
    arr_del(f->sp->u.array);
    PUT_STRVAL(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("random", kf_random, pt_random)
# else
char pt_random[] = { C_TYPECHECKED | C_STATIC, T_INT, 1, T_INT };

/*
 * NAME:	kfun->random()
 * DESCRIPTION:	return a random number
 */
int kf_random(f)
register frame *f;
{
    i_add_ticks(f, 1);
    PUT_INT(f->sp, (f->sp->u.number > 0) ? P_random() % f->sp->u.number : 0);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sscanf", kf_sscanf, pt_sscanf)
# else
char pt_sscanf[] = { C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS, T_INT, 3,
		     T_STRING, T_STRING, T_LVALUE | T_ELLIPSIS };

/*
 * NAME:	match
 * DESCRIPTION:	match a string possibly including %%, up to the next %[sdfc] or
 *		the end of the string
 */
static bool match(f, s, flenp, slenp)
register char *f, *s;
unsigned int *flenp, *slenp;
{
    register char *p;
    register unsigned int flen, slen;

    flen = *flenp;
    slen = *slenp;

    while (flen > 0) {
	/* look for first % */
	p = (char *) memchr(f, '%', flen);

	if (p == (char *) NULL) {
	    /* no remaining % */
	    if (memcmp(f, s, flen) == 0) {
		*slenp -= slen - flen;
		return TRUE;
	    } else {
		return FALSE;	/* no match */
	    }
	}

	if (p[1] == '%') {
	    /* %% */
	    if (memcmp(f, s, ++p - f) == 0) {
		/* matched up to and including the first % */
		s += p - f;
		slen -= p - f;
		flen -= ++p - f;
		f = p;
	    } else {
		return FALSE;	/* no match */
	    }
	} else if (memcmp(f, s, p - f) == 0) {
	    /* matched up to the first % */
	    *flenp -= flen - (p - f);
	    *slenp -= slen - (p - f);
	    return TRUE;
	} else {
	    return FALSE;	/* no match */
	}
    }

    *slenp -= slen;
    return TRUE;
}

/*
 * NAME:	kfun->sscanf()
 * DESCRIPTION:	scan a string
 */
int kf_sscanf(f, nargs)
register frame *f;
int nargs;
{
    register unsigned int flen, slen, size;
    register char *format, *x;
    unsigned int fl, sl;
    int matches;
    char *s;
    Int i;
    xfloat flt;
    bool skip;

    if (nargs > MAX_LOCALS + 2) {
	return 4;
    }
    s = f->sp[nargs - 1].u.string->text;
    slen = f->sp[nargs - 1].u.string->len;
    nargs -= 2;
    format = f->sp[nargs].u.string->text;
    flen = f->sp[nargs].u.string->len;
    i_reverse(f, nargs);

    i_add_ticks(f, 8 * nargs);
    matches = 0;

    while (flen > 0) {
	if (format[0] != '%' || format[1] == '%') {
	    /* match initial part */
	    fl = flen;
	    sl = slen;
	    if (!match(format, s, &fl, &sl) || fl == flen) {
		goto no_match;
	    }
	    format += fl;
	    flen -= fl;
	    s += sl;
	    slen -= sl;
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
			if ((x[0] == '-' || x[0] == '.') && isdigit(x[1])) {
			    break;
			}
			x++;
			--slen;
		    }
		    size -= slen;
		    break;

		default:
		    error("Bad sscanf format string");
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
		    /* get # of chars to match after string */
		    for (x = format, size = 0; (x - format) != flen;
			 x++, size++) {
			x = (char *) memchr(x, '%', flen - (x - format));
			if (x == (char *) NULL) {
			    x = format + flen;
			    break;
			} else if (x[1] != '%') {
			    break;
			}
		    }
		    size = (x - format) - size;

		    x = s;
		    for (;;) {
			sl = slen - (x - s);
			if (sl < size) {
			    goto no_match;
			}
			x = (char *) memchr(x, format[0], sl - size + 1);
			if (x == (char *) NULL) {
			    goto no_match;
			}
			fl = flen;
			if (match(format, x, &fl, &sl)) {
			    format += fl;
			    flen -= fl;
			    size = x - s;
			    x += sl;
			    slen -= size + sl;
			    break;
			}
			x++;
		    }
		}
	    }

	    if (!skip) {
		if (nargs == 0) {
		    error("No lvalue for %%s");
		}
		--nargs;
		PUSH_STRVAL(f, str_new(s, (long) size));
		i_store(f);
		f->sp->u.string->ref--;
		f->sp += 2;
	    }
	    s = x;
	    break;

	case 'd':
	    /* %d */
	    x = s;
	    i = strtol(s, &s, 10);
	    if (s == x) {
		goto no_match;
	    }
	    slen -= (s - x);

	    if (!skip) {
		if (nargs == 0) {
		    error("No lvalue for %%d");
		}
		--nargs;
		PUSH_INTVAL(f, i);
		i_store(f);
		f->sp += 2;
	    }
	    break;

	case 'f':
	    /* %f */
	    x = s;
	    if (!flt_atof(&s, &flt) || s == x) {
		goto no_match;
	    }
	    slen -= (s - x);

	    if (!skip) {
		if (nargs == 0) {
		    error("No lvalue for %%f");
		}
		--nargs;
		PUSH_FLTVAL(f, flt);
		i_store(f);
		f->sp += 2;
	    }
	    break;

	case 'c':
	    /* %c */
	    if (slen == 0) {
		goto no_match;
	    }
	    if (!skip) {
		if (nargs == 0) {
		    error("No lvalue for %%c");
		}
		--nargs;
		PUSH_INTVAL(f, UCHAR(*s));
		i_store(f);
		f->sp += 2;
	    }
	    s++;
	    --slen;
	    break;

	default:
	    error("Bad sscanf format string");
	}
	matches++;
    }

no_match:
    if (nargs > 0) {
	i_pop(f, nargs);	/* pop superfluous arguments */
    }
    str_del((f->sp++)->u.string);
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, matches);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("parse_string", kf_parse_string, pt_parse_string)
# else
char pt_parse_string[] = { C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS,
			   T_MIXED | (1 << REFSHIFT), 3,
			   T_STRING, T_STRING | T_VARARGS, T_INT };

/*
 * NAME:	kfun->parse_string()
 * DESCRIPTION:	parse a string
 */
int kf_parse_string(f, nargs)
register frame *f;
int nargs;
{
    Int maxalt;
    array *a;

    if (nargs > 2) {
	maxalt = (f->sp++)->u.number + 1;
	if (maxalt <= 0) {
	    return 3;
	}
    } else {
	maxalt = 1;	/* default: just one valid parse tree */
    }

    if (OBJR(f->oindex)->flags & O_SPECIAL) {
	error("parse_string() from special purpose object");
    }

    a = ps_parse_string(f, f->sp[1].u.string, f->sp->u.string, maxalt);
    str_del((f->sp++)->u.string);
    str_del(f->sp->u.string);

    if (a != (array *) NULL) {
	/* return parse tree */
	PUT_ARRVAL(f->sp, a);
    } else {
	/* parsing failed */
	*f->sp = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("hash_crc16", kf_hash_crc16, pt_hash_crc16)
# else
char pt_hash_crc16[] = { C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS, T_INT, 2,
			 T_STRING, T_STRING | T_ELLIPSIS };

/*
 * NAME:	kfun->hash_crc16()
 * DESCRIPTION:	Compute a 16 bit cyclic redundancy code for a string.
 *		Based on "A PAINLESS GUIDE TO CRC ERROR DETECTION ALGORITHMS",
 *		by Ross N. Williams.
 *
 *		    Name:	"CRC-16/CCITT"	(supposedly)
 *		    Width:	16
 *		    Poly:	1021		(X^16 + X^12 + X^5 + 1)
 *		    Init:	FFFF
 *		    RefIn:	False
 *		    RefOut:	False
 *		    XorOut:	0000
 *		    Check:	29B1
 */
int kf_hash_crc16(f, nargs)
register frame *f;
int nargs;
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
    register unsigned short crc;
    register int i;
    register ssizet len;
    register char *p;
    register Int cost;

    cost = 0;
    for (i = nargs; --i >= 0; ) {
	cost += f->sp[i].u.string->len;
    }
    cost = 3 * nargs + (cost >> 2);
    if (!f->rlim->noticks && f->rlim->ticks <= cost) {
	f->rlim->ticks = 0;
	error("Out of ticks");
    }
    i_add_ticks(f, cost);

    crc = 0xffff;
    for (i = nargs; --i >= 0; ) {
	p = f->sp[i].u.string->text;
	for (len = f->sp[i].u.string->len; len != 0; --len) {
	    crc = (crc >> 8) ^ crctab[UCHAR(crc ^ *p++)];
	}
	str_del(f->sp[i].u.string);
    }
    crc = (crc >> 8) + (crc << 8);

    f->sp += nargs - 1;
    PUT_INTVAL(f->sp, crc);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("hash_md5", kf_hash_md5, pt_hash_md5)
# else
char pt_hash_md5[] = { C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS, T_STRING, 2,
		       T_STRING, T_STRING | T_ELLIPSIS };

# define ROTL(x, s)			((x << s) | (x >> (32 - s)))
# define R1(a, b, c, d, Mj, s, ti)	(a += ((b & c) | (~b & d)) + Mj + ti, \
					 a = b + ROTL(a, s))
# define R2(a, b, c, d, Mj, s, ti)	(a += ((b & d) | (c & ~d)) + Mj + ti, \
					 a = b + ROTL(a, s))
# define R3(a, b, c, d, Mj, s, ti)	(a += (b ^ c ^ d) + Mj + ti,	      \
					 a = b + ROTL(a, s))
# define R4(a, b, c, d, Mj, s, ti)	(a += (c ^ (b | ~d)) + Mj + ti,	      \
					 a = b + ROTL(a, s))

/*
 * NAME:	md5_block()
 * DESCRIPTION:	add another 512 bit block to the message digest
 */
static void md5_block(ABCD, block)
Uint *ABCD;
register char *block;
{
    Uint M[16];
    register int i, j;
    register Uint a, b, c, d;

    for (i = j = 0; i < 16; i++, j += 4) {
	M[i] = UCHAR(block[j + 0]) | (UCHAR(block[j + 1]) << 8) |
	       (UCHAR(block[j + 2]) << 16) | (UCHAR(block[j + 3]) << 24);
    }

    a = ABCD[0];
    b = ABCD[1];
    c = ABCD[2];
    d = ABCD[3];

    R1(a, b, c, d, M[ 0],  7, 0xd76aa478);
    R1(d, a, b, c, M[ 1], 12, 0xe8c7b756);
    R1(c, d, a, b, M[ 2], 17, 0x242070db);
    R1(b, c, d, a, M[ 3], 22, 0xc1bdceee);
    R1(a, b, c, d, M[ 4],  7, 0xf57c0faf);
    R1(d, a, b, c, M[ 5], 12, 0x4787c62a);
    R1(c, d, a, b, M[ 6], 17, 0xa8304613);
    R1(b, c, d, a, M[ 7], 22, 0xfd469501);
    R1(a, b, c, d, M[ 8],  7, 0x698098d8);
    R1(d, a, b, c, M[ 9], 12, 0x8b44f7af);
    R1(c, d, a, b, M[10], 17, 0xffff5bb1);
    R1(b, c, d, a, M[11], 22, 0x895cd7be);
    R1(a, b, c, d, M[12],  7, 0x6b901122);
    R1(d, a, b, c, M[13], 12, 0xfd987193);
    R1(c, d, a, b, M[14], 17, 0xa679438e);
    R1(b, c, d, a, M[15], 22, 0x49b40821);

    R2(a, b, c, d, M[ 1],  5, 0xf61e2562);
    R2(d, a, b, c, M[ 6],  9, 0xc040b340);
    R2(c, d, a, b, M[11], 14, 0x265e5a51);
    R2(b, c, d, a, M[ 0], 20, 0xe9b6c7aa);
    R2(a, b, c, d, M[ 5],  5, 0xd62f105d);
    R2(d, a, b, c, M[10],  9, 0x02441453);
    R2(c, d, a, b, M[15], 14, 0xd8a1e681);
    R2(b, c, d, a, M[ 4], 20, 0xe7d3fbc8);
    R2(a, b, c, d, M[ 9],  5, 0x21e1cde6);
    R2(d, a, b, c, M[14],  9, 0xc33707d6);
    R2(c, d, a, b, M[ 3], 14, 0xf4d50d87);
    R2(b, c, d, a, M[ 8], 20, 0x455a14ed);
    R2(a, b, c, d, M[13],  5, 0xa9e3e905);
    R2(d, a, b, c, M[ 2],  9, 0xfcefa3f8);
    R2(c, d, a, b, M[ 7], 14, 0x676f02d9);
    R2(b, c, d, a, M[12], 20, 0x8d2a4c8a);

    R3(a, b, c, d, M[ 5],  4, 0xfffa3942);
    R3(d, a, b, c, M[ 8], 11, 0x8771f681);
    R3(c, d, a, b, M[11], 16, 0x6d9d6122);
    R3(b, c, d, a, M[14], 23, 0xfde5380c);
    R3(a, b, c, d, M[ 1],  4, 0xa4beea44);
    R3(d, a, b, c, M[ 4], 11, 0x4bdecfa9);
    R3(c, d, a, b, M[ 7], 16, 0xf6bb4b60);
    R3(b, c, d, a, M[10], 23, 0xbebfbc70);
    R3(a, b, c, d, M[13],  4, 0x289b7ec6);
    R3(d, a, b, c, M[ 0], 11, 0xeaa127fa);
    R3(c, d, a, b, M[ 3], 16, 0xd4ef3085);
    R3(b, c, d, a, M[ 6], 23, 0x04881d05);
    R3(a, b, c, d, M[ 9],  4, 0xd9d4d039);
    R3(d, a, b, c, M[12], 11, 0xe6db99e5);
    R3(c, d, a, b, M[15], 16, 0x1fa27cf8);
    R3(b, c, d, a, M[ 2], 23, 0xc4ac5665);

    R4(a, b, c, d, M[ 0],  6, 0xf4292244);
    R4(d, a, b, c, M[ 7], 10, 0x432aff97);
    R4(c, d, a, b, M[14], 15, 0xab9423a7);
    R4(b, c, d, a, M[ 5], 21, 0xfc93a039);
    R4(a, b, c, d, M[12],  6, 0x655b59c3);
    R4(d, a, b, c, M[ 3], 10, 0x8f0ccc92);
    R4(c, d, a, b, M[10], 15, 0xffeff47d);
    R4(b, c, d, a, M[ 1], 21, 0x85845dd1);
    R4(a, b, c, d, M[ 8],  6, 0x6fa87e4f);
    R4(d, a, b, c, M[15], 10, 0xfe2ce6e0);
    R4(c, d, a, b, M[ 6], 15, 0xa3014314);
    R4(b, c, d, a, M[13], 21, 0x4e0811a1);
    R4(a, b, c, d, M[ 4],  6, 0xf7537e82);
    R4(d, a, b, c, M[11], 10, 0xbd3af235);
    R4(c, d, a, b, M[ 2], 15, 0x2ad7d2bb);
    R4(b, c, d, a, M[ 9], 21, 0xeb86d391);

    ABCD[0] += a;
    ABCD[1] += b;
    ABCD[2] += c;
    ABCD[3] += d;
}

/*
 * NAME:	kfun->hash_md5()
 * DESCRIPTION:	Compute MD5 message digest.  See "Applied Cryptography" by
 *		Bruce Schneier, Second Edition, p. 436-441.
 */
int kf_hash_md5(f, nargs)
register frame *f;
int nargs;
{
    char buffer[64];
    Uint cv[4];
    register int i;
    register ssizet len;
    register unsigned short bufsz;
    register char *p;
    register Int cost;
    register Uint length;

    cost = 3 * nargs + 64;
    for (i = nargs; --i >= 0; ) {
	cost += f->sp[i].u.string->len;
    }
    if (!f->rlim->noticks && f->rlim->ticks <= cost) {
	f->rlim->ticks = 0;
	error("Out of ticks");
    }
    i_add_ticks(f, cost);

    /*
     * These constants must apparently be little-endianized, though AC2 does
     * not explicitly say so.
     */
    cv[0] = 0x67452301;
    cv[1] = 0xefcdab89;
    cv[2] = 0x98badcfe;
    cv[3] = 0x10325476;
    length = 0;
    bufsz = 0;

    for (i = nargs; --i >= 0; ) {
	len = f->sp[i].u.string->len;
	if (len != 0) {
	    length += len;
	    p = f->sp[i].u.string->text;
	    if (bufsz != 0) {
		register unsigned short size;

		/* fill buffer and digest */
		size = 64 - bufsz;
		if (size > len) {
		    size = len;
		}
		memcpy(buffer + bufsz, p, size);
		p += size;
		len -= size;
		bufsz += size;

		if (bufsz == 64) {
		    md5_block(cv, buffer);
		    bufsz = 0;
		}
	    }

	    while (len >= 64) {
		/* digest directly from string */
		md5_block(cv, p);
		p += 64;
		len -= 64;
	    }

	    if (len != 0) {
		/* put remainder in buffer */
		memcpy(buffer, p, bufsz = len);
	    }
	}
	str_del(f->sp[i].u.string);
    }

    /* append padding and digest final block(s) */
    buffer[bufsz++] = 0x80;
    if (bufsz > 56) {
	memset(buffer + bufsz, '\0', 64 - bufsz);
	md5_block(cv, buffer);
	bufsz = 0;
    }
    memset(buffer + bufsz, '\0', 64 - bufsz);
    buffer[56] = length << 3;
    buffer[57] = length >> 5;
    buffer[58] = length >> 13;
    buffer[59] = length >> 21;
    buffer[60] = length >> 29;
    md5_block(cv, buffer);

    for (bufsz = i = 0; i < 4; bufsz += 4, i++) {
	buffer[bufsz + 0] = cv[i];
	buffer[bufsz + 1] = cv[i] >> 8;
	buffer[bufsz + 2] = cv[i] >> 16;
	buffer[bufsz + 3] = cv[i] >> 24;
    }
    f->sp += nargs - 1;
    PUT_STR(f->sp, str_new(buffer, 16L));
    return 0;
}
# endif
