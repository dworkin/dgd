# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "interpret.h"
# include "asn.h"

/*
 * NAME:	asi->add()
 * DESCRIPTION:	a += b (sizea >= sizeb)
 */
static bool asi_add(a, b, sizea, sizeb)
register Uint *a, *b, sizea, sizeb;
{
    register Uint tmp, carry;

    sizea -= sizeb;
    carry = 0;
    do {
	tmp = *a + carry;
	carry = (tmp < carry) + ((*a++ = tmp + *b++) < tmp);
    } while (--sizeb != 0);

    while (carry && sizea != 0) {
	carry = (++(*a++) == 0);
	--sizea;
    }

    return carry;
}

/*
 * NAME:	asi->sub()
 * DESCRIPTION:	a -= b (a >= b, sizea >= sizeb)
 */
static bool asi_sub(a, b, sizea, sizeb)
register Uint *a, *b, sizea, sizeb;
{
    register Uint tmp, borrow;

    sizea -= sizeb;
    borrow = 0;
    do {
	borrow = ((tmp = *a - borrow) > *a);
	borrow += ((*a++ = tmp - *b++) > tmp);
    } while (--sizeb != 0);

    while (borrow && sizea != 0) {
	borrow = ((*a++)-- == 0);
	--sizea;
    }

    return borrow;
}

/*
 * NAME:	asi->lshift()
 * DESCRIPTION:	a <<= lshift, lshift != 0
 */
static Uint asi_lshift(a, size, lshift)
register Uint *a, size, lshift;
{
    register Uint offset, rshift, tmp, bits;

    offset = (lshift + 31) >> 5;
    lshift &= 0x1f;
    size -= offset;

    if (lshift == 0) {
	a += size;
	for (tmp = size; tmp > 0; --tmp) {
	    --a;
	    a[offset] = *a;
	}
	bits = 0;
    } else {
	rshift = 32 - lshift;
	a += size;
	bits = *a << lshift;
	while (size != 0) {
	    tmp = *--a;
	    a[offset] = bits | (tmp >> rshift);
	    bits = tmp << lshift;
	    --size;
	}
    }

    a += offset;
    do {
	*--a = bits;
	bits = 0;
    } while (--offset != 0);
}

/*
 * NAME:	asi->rshift()
 * DESCRIPTION:	a >>= rshift, rshift != 0
 */
static Uint asi_rshift(a, size, rshift)
register Uint *a, size, rshift;
{
    register Uint offset, lshift, tmp, bits;

    offset = (rshift + 31) >> 5;
    rshift &= 0x1f;
    size -= offset;

    if (rshift == 0) {
	for (tmp = size; tmp > 0; --tmp) {
	    *a = a[offset];
	    a++;
	}
	bits = 0;
    } else {
	lshift = 32 - rshift;
	bits = a[offset - 1] >> rshift;
	while (size != 0) {
	    tmp = a[offset];
	    *a++ = bits | (tmp << lshift);
	    bits = tmp >> rshift;
	    --size;
	}
    }

    do {
	*a++ = bits;
	bits = 0;
    } while (--offset != 0);
}

/*
 * NAME:	asi->cmp()
 * DESCRIPTION:	compare a and b (sizea >= sizeb)
 */
static int asi_cmp(a, b, sizea, sizeb)
register Uint *a, *b, sizea, sizeb;
{
    a += sizea;
    while (sizea > sizeb) {
	if (*--a != 0) {
	    return 1;
	}
	--sizea;
    }
    b += sizea;

    do {
	if (*--a < *--b) {
	    return -1;
	} else if (*a > *b) {
	    return 1;
	}
    } while (--sizea != 0);

    return 0;
}

# ifdef Uuint
# define asi_mult1(c, a, b)	do {				\
				    Uuint _t;			\
				    _t = (Uuint) (a) * (b);	\
				    (c)[0] = _t;		\
				    (c)[1] = _t >> 32;		\
				} while (0)
# else
static void asi_mult1(c, a, b)
Uint *c, a, b;
{
    register unsigned short a0, a1, b0, b1;
    register Uint t0, t1;

    a0 = a;
    a1 = a >> 16;
    b0 = b;
    b1 = b >> 16;

    t0 = (Uint) a0 * b1;
    t1 = (Uint) a1 * b0;
    t1 = ((t0 += t1) < t1);
    t1 = (t1 << 16) | (t0 >> 16);
    t0 <<= 16;
    if ((c[0] = (Uint) a0 * b0 + t0) < t0) {
	t1++;
    }
    c[1] = (Uint) a1 * b1 + t1;
}
# endif

/*
 * NAME:	asi->mult()
 * DESCRIPTION:	c = a * b (sizea - sizeb <= 1)
 *		sizeof(t) = sizea + sizeb + 3
 */
static void asi_mult(c, t, a, b, sizea1, sizeb1)
register Uint *c, *t, *a, *b, sizea1, sizeb1;
{
    if (sizeb1 == 1) {
	/* sizea <= 2, sizeb == 1 */
	asi_mult1(c, a[0], b[0]);
	if (sizea1 > 1) {
	    /* sizea == 2, sizeb == 1 */
	    asi_mult1(t, a[1], b[0]);
	    if ((c[1] += t[0]) < t[0]) {
		t[1]++;
	    }
	    c[2] = t[1];
	}
    } else {
	register Uint sizeab0, sizet2, sizet3, *t2;
	bool minus;

	sizeab0 = sizea1 >> 1;
	sizea1 -= sizeab0;
	sizeb1 -= sizeab0;

	/* t2 = a0 - a1, t3 = b1 - b0 */
	t2 = t + sizea1 + sizeb1 + 1;
	if (asi_cmp(a + sizeab0, a, sizea1, sizeab0) <= 0) {
	    /* a0 - a1 */
	    sizet2 = sizeab0;
	    memcpy(t2, a, sizet2 * sizeof(Uint));
	    asi_sub(t2, a + sizeab0, sizet2, sizet2);
	    minus = FALSE;
	} else {
	    /* -(a1 - a0) */
	    sizet2 = sizea1;
	    memcpy(t2, a + sizeab0, sizet2 * sizeof(Uint));
	    asi_sub(t2, a, sizet2, sizeab0);
	    minus = TRUE;
	}
	if ((sizeab0 <= sizeb1) ?
	     asi_cmp(b + sizeab0, b, sizeb1, sizeab0) >= 0 :
	     asi_cmp(b, b + sizeab0, sizeab0, sizeb1) <= 0) {
	    /* b1 - b0 */
	    sizet3 = sizeb1;
	    memcpy(t2 + sizet2, b + sizeab0, sizet3 * sizeof(Uint));
	    asi_sub(t2 + sizet2, b, sizet3, sizet3);
	} else {
	    /* -(b0 - b1) */
	    sizet3 = sizeab0;
	    memcpy(t2 + sizet2, b, sizet3 * sizeof(Uint));
	    asi_sub(t2 + sizet2, b + sizeab0, sizet3, sizet3);
	    minus ^= TRUE;
	}

	/* t1:t0 = t2 * t3 */
	if (sizet2 >= sizet3) {
	    asi_mult(t, c, t2, t2 + sizet2, sizet2, sizet3);
	} else {
	    asi_mult(t, c, t2 + sizet2, t2, sizet3, sizet2);
	}

	/* c1:c0 = a0 * b0, c3:c2 = a1 * b1 */
	asi_mult(c, t2, a, b, sizeab0, sizeab0);
	asi_mult(c + (sizeab0 << 1), t2, a + sizeab0, b + sizeab0, sizea1,
		 sizeb1);

	/* t3:t2 = c3:c2 + c1:c0 + t1:t0 */
	sizea1 += sizeb1;
	memcpy(t2, c + (sizeab0 << 1), sizea1 * sizeof(Uint));
	t2[sizea1] = 0;
	sizeb1 = sizea1 + 1;
	asi_add(t2, c, sizeb1, sizeab0 << 1);
	if (minus) {
	    asi_sub(t2, t, sizeb1, sizet2 + sizet3);
	} else {
	    asi_add(t2, t, sizeb1, sizet2 + sizet3);
	}

	/* c3:c2:c1 += t3:t2 */
	asi_add(c + sizeab0, t2, sizeab0 + sizea1, sizeb1);
    }
}

/*
 * NAME:	asi->mult_row()
 * DESCRIPTION:	c += a * word
 */
static bool asi_mult_row(c, a, b, size)
register Uint *c, *a, b, size;
{
    register Uint s, carry;
    Uint t[2];

    s = 0;
    carry = 0;
    do {
	asi_mult1(t, *a++, b);
	if ((s += t[0]) < t[0]) {
	    t[1]++;
	}
	carry = ((s += carry) < carry);
	carry += ((*c++ += s) < s);
	s = t[1];
    } while (--size != 0);

    carry = ((s += carry) < carry);
    return carry + ((*c += s) < s);
}

# ifdef Uuint
# define asi_sqr1(b, a)	do {				\
			    Uuint _t;			\
			    _t = (Uuint) (a) * (a);	\
			    (b)[0] = _t;		\
			    (b)[1] = _t >> 32;		\
			} while (0)
# else
static void asi_sqr1(b, a)
Uint *b, a;
{
    register unsigned short a0, a1;
    register Uint t0, t1;

    a0 = a;
    a1 = a >> 16;

    t0 = (Uint) a0 * a1;
    t1 = t0;
    t1 = ((t0 <<= 1) < t1);
    t1 = (t1 << 16) | (t0 >> 16);
    t0 <<= 16;
    if ((b[0] = (Uint) a0 * a0 + t0) < t0) {
	t1++;
    }
    b[1] = (Uint) a1 * a1 + t1;
}
# endif

/*
 * NAME:	asi->sqr()
 * DESCRIPTION:	b = a * a
 *		sizeof(t) = (sizea + 1) << 1
 */
static void asi_sqr(b, t, a, sizea1)
register Uint *b, *t, *a, sizea1;
{
    if (sizea1 == 1) {
	asi_sqr1(b, a[0]);
    } else {
	register Uint sizea0, sizet2, *t2;

	sizea0 = sizea1 >> 1;
	sizea1 -= sizea0;

	/* t2 = a0 - a1 */
	t2 = t + (sizea1 << 1);
	if (asi_cmp(a + sizea0, a, sizea1, sizea0) <= 0) {
	    /* a0 - a1 */
	    sizet2 = sizea0;
	    memcpy(t2, a, sizet2 * sizeof(Uint));
	    asi_sub(t2, a + sizea0, sizet2, sizet2);
	} else {
	    /* -(a1 - a0) */
	    sizet2 = sizea1;
	    memcpy(t2, a + sizea0, sizet2 * sizeof(Uint));
	    asi_sub(t2, a, sizet2, sizea0);
	}

	/* t1:t0 = t2 * t2 */
	asi_sqr(t, b, t2, sizet2);

	/* b1:b0 = a0 * a0, b3:b2 = a1 * a1 */
	asi_sqr(b, t2, a, sizea0);
	asi_sqr(b + (sizea0 << 1), t2, a + sizea0, sizea1);

	/* t3:t2 = b3:b2 + b1:b0 - t1:t0 */
	sizea1 <<= 1;
	memcpy(t2, b + (sizea0 << 1), sizea1 * sizeof(Uint));
	t2[sizea1] = 0;
	asi_add(t2, b, sizea1 + 1, sizea0 << 1);
	asi_sub(t2, t, sizea1 + 1, sizet2 << 1);

	/* b3:b2:b1 += t3:t2 */
	asi_add(b + sizea0, t2, sizea0 + sizea1, sizea1 + 1);
    }
}

# ifdef Uuint
# define asi_div1(a, b)		((((Uuint) (a)[1] << 32) | (a)[0]) / (b))
# else
static Uint asi_div1(aa, b)
Uint *aa, b;
{
    register Uint q1, q0;
    Uint a[2], t[2], c[2];

    a[0] = aa[0];
    a[1] = aa[1];

    q1 = ((a[1] ^ b) >> 16 == 0) ? 0xffff : a[1] / (unsigned short) (b >> 16);
    asi_mult1(t, b, q1 << 16);
    if (asi_cmp(t, a, 2, 2) > 0) {
	c[0] = b << 16;
	c[1] = b >> 16;
	asi_sub(t, c, 2, 2);
	--q1;
	if (asi_cmp(t, a, 2, 2) > 0) {
	    asi_sub(t, c, 2, 2);
	    --q1;
	}
    }
    asi_sub(a, t, 2, 2);

    q0 = (a[1] == b >> 16) ?
	  0xffff : ((a[1] << 16) | (a[0] >> 16)) / (unsigned short) (b >> 16);
    asi_mult1(t, b, q0);
    if (asi_cmp(t, a, 2, 2) > 0) {
	asi_sub(t, &b, 2, 1);
	--q0;
	if (asi_cmp(t, a, 2, 2) > 0) {
	    asi_sub(t, &b, 2, 1);
	    --q0;
	}
    }

    return (q1 << 16) | q0;
}
# endif

/*
 * NAME:	asi->div()
 * DESCRIPTION:	c1 = a / b, c0 = a % b (a >= b, b != 0)
 *		sizeof(t) = (sizeb << 1) + 1
 */
static Uint *asi_div(c, t, a, b, sizea, sizeb)
Uint *c;
register Uint *t, *a, *b, sizea, sizeb;
{
    register Uint *v, d, q, shift;

    /* copy values */
    if (a != c) {
	memcpy(c, a, sizea * sizeof(Uint));
	a = c;
    }
    c[sizea] = 0;
    memcpy(t, b, sizeb * sizeof(Uint));
    b = t;
    t += sizeb;

    /*
     * left shift until most significant bit of b is 1
     */
    for (shift = 0, d = b[sizeb - 1]; !(d & 0x80000000L); shift++, d <<= 1) ;
    if (shift != 0) {
	asi_lshift(b, sizeb, shift);
	asi_lshift(a, sizea + 1, shift);
	if (a[sizea] != 0) {
	    sizea++;
	}
    }

    if (sizea == sizeb) {
	/* a >= b */
	asi_sub(a, b, sizeb, sizeb);
	c[sizea] = 1;
    } else {
	a += sizea - sizeb - 1;
	if (asi_cmp(a + 1, b, sizeb, sizeb) >= 0) {
	    asi_sub(a + 1, b, sizeb, sizeb);
	    c[sizea] = 1;
	} else {
	    c[sizea] = 0;
	}

	/*
	 * perform actual division
	 */
	d = b[sizeb - 1];
	do {
	    q = (a[sizeb] == d) ? 0xffffffffL : asi_div1(a + sizeb - 1, d);
	    memset(t, '\0', (sizeb + 1) * sizeof(Uint));
	    asi_mult_row(t, b, q, sizeb);
	    if (asi_cmp(t, a, sizeb + 1, sizeb + 1) > 0) {
		asi_sub(t, b, sizeb + 1, sizeb);
		--q;
		if (asi_cmp(t, a, sizeb + 1, sizeb + 1) > 0) {
		    asi_sub(t, b, sizeb + 1, sizeb);
		    --q;
		}
	    }
	    asi_sub(a, t, sizeb + 1, sizeb + 1);
	    c[--sizea] = q;
	    --a;
	} while (sizea > sizeb);
    }

    if (shift != 0) {
	/* compensate for left shift */
	asi_rshift(c, sizeb, shift);
    }
    return c + sizeb;
}

/*
 * NAME:	asi->strtonum()
 * DESCRIPTION:	convert a string to an ASI
 */
static void asi_strtonum(num, str, sz, minus)
register Uint *num;
string *str;
Uint *sz;
bool *minus;
{
    register ssizet len;
    register char *text;
    register Uint *tmp, size, bits;

    len = str->len;
    text = str->text;
    if (len == 0) {
	/*
	 * empty string: 0
	 */
	num[0] = 0;
	num[1] = 0;
	*sz = 1;
	*minus = FALSE;
	return;
    } else if ((text[0] & 0x80)) {
	/*
	 * negative
	 */
	while (*text == '\xff') {
	    *text++;
	    if (--len == 0) {
		/* -1 */
		num[0] = 1;
		num[1] = 0;
		*sz = 1;
		*minus = TRUE;
		return;
	    }
	}
	size = (len + 3) / sizeof(Uint);
	memset(num, '\0', (size + 1) * sizeof(Uint));

	tmp = ALLOCA(Uint, size);
	text += len;
	while (len >= 4) {
	    text -= 4;
	    *tmp++ = (UCHAR(text[0]) << 24) |
		     (UCHAR(text[1]) << 16) |
		     (UCHAR(text[2]) << 8) |
		     UCHAR(text[3]);
	    len -= 4;
	}
	if (len != 0) {
	    text -= len;
	    bits = 0xffffffffL;
	    do {
		bits = (bits << 8) | UCHAR(*text++);
	    } while (--len != 0);
	    *tmp++ = bits;
	}
	tmp -= size;
	asi_sub(num, tmp, size, size);
	AFREE(tmp);

	*sz = size;
	*minus = TRUE;
	return;

    } else {
	/*
	 * positive
	 */
	while (*text == '\0') {
	    text++;
	    if (--len == 0) {
		/* 0 */
		num[0] = 0;
		num[1] = 0;
		*sz = 1;
		*minus = FALSE;
		return;
	    }
	}
	size = (len + 3) / sizeof(Uint);
	num[size] = 0;

	text += len;
	while (len >= 4) {
	    text -= 4;
	    *num++ = (UCHAR(text[0]) << 24) |
		     (UCHAR(text[1]) << 16) |
		     (UCHAR(text[2]) << 8) |
		     UCHAR(text[3]);
	    len -= 4;
	}
	if (len != 0) {
	    text -= len;
	    bits = 0;
	    do {
		bits = (bits << 8) | UCHAR(*text++);
	    } while (--len != 0);
	    *num++ = bits;
	}
	num -= size;

	*sz = size;
	*minus = FALSE;
	return;
    }
}

/*
 * NAME:	asi->numtostr()
 * DESCRIPTION:	convert an ASI to a string
 */
static string *asi_numtostr(num, size, minus)
register Uint *num;
register Uint size;
bool minus;
{
    register ssizet len;
    register Uint bits;
    register char *text;
    Uint *tmp;
    bool prefix;
    string *str;

    /*
     * skip leading zeroes
     */
    while (num[size - 1] == 0) {
	if (--size == 0) {
	    /* +0, -0 */
	    return str_new("\0", 1L);
	}
    }

    prefix = FALSE;
    if (minus) {
	tmp = ALLOCA(Uint, size);
	memset(tmp, '\0', size * sizeof(Uint));
	asi_sub(tmp, num, size, size);
	num = tmp;

	num += --size;
	bits = *num;
	if (size == 0 && bits == 0xffffffffL) {
	    len = 0;
	} else {
	    /* skip leading 0xff bytes */
	    for (len = 24; UCHAR(bits >> len) == 0xff; len -= 8) ;
	    if (!((bits >> len) & 0x80)) {
		prefix = TRUE;
	    }
	}
    } else {
	/* skip leading 0x00 bytes */
	num += --size;
	bits = *num;
	for (len = 24; (bits >> len) == 0; len -= 8) ;
	if ((bits >> len) & 0x80) {
	    prefix = TRUE;
	}
    }
    len = (len >> 3) + 1;

    str = str_new((char *) NULL, (long) size * sizeof(Uint) + len + prefix);
    text = str->text;
    if (prefix) {
	/* extra sign indicator */
	*text++ = (minus) ? 0xff : 0x00;
    }
    do {
	*text++ = bits >> (--len << 3);
    } while (len != 0);
    while (size != 0) {
	bits = *--num;
	*text++ = bits >> 24;
	*text++ = bits >> 16;
	*text++ = bits >> 8;
	*text++ = bits;
	--size;
    }

    if (minus) {
	AFREE(tmp);
    }
    return str;
}


/*
 * NAME:	asn->add()
 * DESCRIPTION:	add two ASIs
 */
string *asn_add(s1, s2, s3)
string *s1, *s2, *s3;
{
    register Uint *a, *b, *c;
    Uint *mod, sizea, sizeb, sizec, sizemod;
    bool minusa, minusb, minusc;
    string *str;

    mod = ALLOCA(Uint, (s3->len >> 2) + 2);
    asi_strtonum(mod, s3, &sizemod, &minusa);
    if (minusa || (sizemod == 1 && mod[0] == 0)) {
	AFREE(mod);
	error("Invalid modulus");
    }

    a = ALLOCA(Uint, (s1->len >> 2) + 4);
    asi_strtonum(a, s1, &sizea, &minusa);
    b = ALLOCA(Uint, (s2->len >> 2) + 4);
    asi_strtonum(b, s2, &sizeb, &minusb);

    if (minusa != minusb) {
	if (sizea > sizeb ||
	    (sizea == sizeb && asi_cmp(a, b, sizea, sizeb) >= 0)) {
	    /* a + -b, -a + b */
	    asi_sub(a, b, sizea, sizeb);
	    c = a;
	    sizec = sizea;
	    minusc = minusa;
	} else {
	    /* -b + a, b + -a */
	    asi_sub(b, a, sizeb, sizea);
	    c = b;
	    sizec = sizeb;
	    minusc = minusb;
	}
    } else if (sizea >= sizeb) {
	/* a + b, -a + -b */
	asi_add(a, b, sizea + 1, sizeb);
	c = a;
	sizec = sizea + 1;
	minusc = minusa;
    } else {
	/* b + a, -b + -a */
	asi_add(b, a, sizeb + 1, sizea);
	c = b;
	sizec = sizeb + 1;
	minusc = minusb;
    }

    if (sizec >= sizemod && asi_cmp(c, mod, sizec, sizemod) >= 0) {
	asi_sub(c, mod, sizec, sizemod);
	if (asi_cmp(c, mod, sizec, sizemod) >= 0) {
	    Uint *t;

	    t = ALLOCA(Uint, (sizemod << 1) + 1);
	    asi_div(c, t, c, mod, sizec, sizemod);
	    sizec = sizemod;
	    AFREE(t);
	}
    }
    str = asi_numtostr(c, sizec, minusc);
    AFREE(b);
    AFREE(a);
    AFREE(mod);
    return str;
}

/*
 * NAME:	asn->sub()
 * DESCRIPTION:	subtract one ASI from another
 */
string *asn_sub(s1, s2, s3)
string *s1, *s2, *s3;
{
    register Uint *a, *b, *c;
    Uint *mod, sizea, sizeb, sizec, sizemod;
    bool minusa, minusb, minusc;
    string *str;

    mod = ALLOCA(Uint, (s3->len >> 2) + 2);
    asi_strtonum(mod, s3, &sizemod, &minusa);
    if (minusa || (sizemod == 1 && mod[0] == 0)) {
	AFREE(mod);
	error("Invalid modulus");
    }

    a = ALLOCA(Uint, (s1->len >> 2) + 4);
    asi_strtonum(a, s1, &sizea, &minusa);
    b = ALLOCA(Uint, (s2->len >> 2) + 4);
    asi_strtonum(b, s2, &sizeb, &minusb);

    if (minusa == minusb) {
	if (sizea > sizeb ||
	    (sizea == sizeb && asi_cmp(a, b, sizea, sizeb) >= 0)) {
	    /* a - b, -a - -b */
	    asi_sub(a, b, sizea, sizeb);
	    c = a;
	    sizec = sizea;
	    minusc = minusa;
	} else {
	    /* b - a, -b - -a */
	    asi_sub(b, a, sizeb, sizea);
	    c = b;
	    sizec = sizeb;
	    minusc = !minusb;
	}
    } else if (sizea >= sizeb) {
	/* a - -b, -a - b */
	asi_add(a, b, sizea + 1, sizeb);
	c = a;
	sizec = sizea + 1;
	minusc = minusa;
    } else {
	/* b - -a, -b - a */
	asi_add(b, a, sizeb + 1, sizea);
	c = b;
	sizec = sizeb + 1;
	minusc = !minusb;
    }

    if (sizec >= sizemod && asi_cmp(c, mod, sizec, sizemod) >= 0) {
	asi_sub(c, mod, sizec, sizemod);
	if (asi_cmp(c, mod, sizec, sizemod) >= 0) {
	    Uint *t;

	    t = ALLOCA(Uint, (sizemod << 1) + 1);
	    asi_div(c, t, c, mod, sizec, sizemod);
	    sizec = sizemod;
	    AFREE(t);
	}
    }
    str = asi_numtostr(c, sizec, minusc);
    AFREE(b);
    AFREE(a);
    AFREE(mod);
    return str;
}

/*
 * NAME:	asn->cmp()
 * DESCRIPTION:	compare one ASI with another
 */
int asn_cmp(s1, s2)
string *s1, *s2;
{
    Uint *a, *b, sizea, sizeb;
    bool minusa, minusb;
    int cmp;

    a = ALLOCA(Uint, (s1->len >> 2) + 2);
    asi_strtonum(a, s1, &sizea, &minusa);
    b = ALLOCA(Uint, (s2->len >> 2) + 2);
    asi_strtonum(b, s2, &sizeb, &minusb);

    if (minusa != minusb) {
	if (minusa) {
	    cmp = -1;
	} else {
	    cmp = 1;
	}
    } else {
	if (sizea != sizeb) {
	    if (sizea < sizeb) {
		cmp = -1;
	    } else {
		cmp = 1;
	    }
	} else {
	    cmp = asi_cmp(a, b, sizea, sizeb);
	}
	if (minusa) {
	    cmp = -cmp;
	}
    }

    AFREE(b);
    AFREE(a);
    return cmp;
}

/*
 * NAME:	asn->mult()
 * DESCRIPTION:	multiply one ASI with another
 */
string *asn_mult(s1, s2, s3)
string *s1, *s2, *s3;
{
    register Uint *a, *b, *c, *t1, *t2;
    Uint *aa, *bb, *cc, *mod, sizea, sizeb, sizec, sizemod;
    bool minusa, minusb;
    string *str;

    mod = ALLOCA(Uint, (s3->len >> 2) + 2);
    asi_strtonum(mod, s3, &sizemod, &minusa);
    if (minusa || (sizemod == 1 && mod[0] == 0)) {
	AFREE(mod);
	error("Invalid modulus");
    }

    aa = a = ALLOCA(Uint, (s1->len >> 2) + 2);
    asi_strtonum(a, s1, &sizea, &minusa);
    bb = b = ALLOCA(Uint, (s2->len >> 2) + 2);
    asi_strtonum(b, s2, &sizeb, &minusb);

    sizec = sizea + sizeb;
    cc = c = ALLOCA(Uint, sizec);
    t1 = ALLOCA(Uint, (sizec << 1) + 3);
    t2 = t1 + sizec;
    memset(c, '\0', sizec * sizeof(uint));
    while (sizea != sizeb) {
	if (sizea < sizeb) {
	    Uint *t, sizet;

	    t = a;
	    a = b;
	    b = t;
	    sizet = sizea;
	    sizea = sizeb;
	    sizeb = sizet;
	}
	if (sizea - sizeb == 1) {
	    break;
	}

	asi_mult(t1, t2, a, b, sizeb, sizeb);
	asi_add(c, t1, sizeb << 1, sizeb << 1);
	a += sizeb;
	sizea -= sizeb;
	c += sizeb;
    }
    asi_mult(t1, t2, a, b, sizea, sizeb);
    asi_add(c, t1, sizea + sizeb, sizea + sizeb);

    if (sizec >= sizemod && asi_cmp(cc, mod, sizec, sizemod) >= 0) {
	asi_div(cc, t1, cc, mod, sizec, sizemod);
	sizec = sizemod;
    }
    str = asi_numtostr(cc, sizec, minusa ^ minusb);
    AFREE(t1);
    AFREE(cc);

    AFREE(bb);
    AFREE(aa);
    AFREE(mod);
    return str;
}

/*
 * NAME:	asn->div()
 * DESCRIPTION:	divide one ASI by another
 */
string *asn_div(s1, s2, s3)
string *s1, *s2, *s3;
{
    register Uint *a, *b, *c, *d, *t;
    Uint *mod, sizea, sizeb, sizemod;
    bool minusa, minusb;
    string *str;

    mod = ALLOCA(Uint, (s3->len >> 2) + 2);
    asi_strtonum(mod, s3, &sizemod, &minusa);
    if (minusa || (sizemod == 1 && mod[0] == 0)) {
	AFREE(mod);
	error("Invalid modulus");
    }

    b = ALLOCA(Uint, (s2->len >> 2) + 2);
    asi_strtonum(b, s2, &sizeb, &minusb);
    if (sizeb == 1 && b[0] == 0) {
	AFREE(b);
	AFREE(mod);
	error("Division by zero");
    }
    a = ALLOCA(Uint, (s1->len >> 2) + 2);
    asi_strtonum(a, s1, &sizea, &minusa);

    c = ALLOCA(Uint, sizea + 2);
    t = ALLOCA(Uint, (sizeb + sizemod) << 1); /* more than enough */
    if (sizea >= sizeb && asi_cmp(a, b, sizea, sizeb) >= 0) {
	d = asi_div(c, t, a, b, sizea, sizeb);
	sizea -= sizeb - 1;
	if (sizea >= sizemod && asi_cmp(d, mod, sizea, sizemod) >= 0) {
	    asi_div(c, t, d, mod, sizea, sizemod);
	    d = c;
	    sizea = sizemod;
	}
    } else {
	c[0] = 0;
	d = c;
	sizea = 1;
    }
    str = asi_numtostr(d, sizea, minusa ^ minusb);
    AFREE(t);
    AFREE(c);

    AFREE(a);
    AFREE(b);
    AFREE(mod);
    return str;
}

/*
 * NAME:	asn->mod()
 * DESCRIPTION:	take the modulus of an ASI
 */
string *asn_mod(s1, s2)
string *s1, *s2;
{
    register Uint *a, *b, *c, *t;
    Uint sizea, sizeb;
    bool minusa, minusb;
    string *str;

    b = ALLOCA(Uint, (s2->len >> 2) + 2);
    asi_strtonum(b, s2, &sizeb, &minusb);
    if (minusb || (sizeb == 1 && b[0] == 0)) {
	AFREE(b);
	AFREE(mod);
	error("Invalid modulus");
    }
    a = ALLOCA(Uint, (s1->len >> 2) + 2);
    asi_strtonum(a, s1, &sizea, &minusa);

    c = ALLOCA(Uint, sizea + 2);
    t = ALLOCA(Uint, (sizeb << 1) + 1);
    if (sizea >= sizeb && asi_cmp(a, b, sizea, sizeb) > 0) {
	asi_div(c, t, a, b, sizea, sizeb);
    } else {
	memcpy(c, a, sizea * sizeof(Uint));
	sizeb = sizea;
    }
    str = asi_numtostr(c, sizeb, minusa ^ minusb);
    AFREE(t);
    AFREE(c);

    AFREE(b);
    AFREE(a);
    return str;
}

/*
 * NAME:	asn->modinv()
 * DESCRIPTION:	Compute the multiplicative inverse of a modulo b.
 *		From "Applied Cryptography" by Bruce Schneier, Second Edition, 
 *		page 247.
 */
static bool asn_modinv(c, sizec, a, b, sizea, sizeb)
Uint *c, *sizec, *a, *b, sizea, sizeb;
{
    register Uint *a1, *b1, *g1, *a2, *b2, *g2;
    register Uint sizea1, sizeb1, sizeg1, sizea2, sizeb2, sizeg2;
    bool inverse;

    if (!((a[0] | b[0]) & 1)) {
	return FALSE;	/* GCD >= 2 */
    }

    if (sizea > sizeb ||
	(sizea == sizeb && asi_cmp(a, b, sizea, sizeb) > 0)) {
	/* a > b: a <=> b */
	a1 = a; sizea1 = sizea;
	a = b; sizea = sizeb;
	b = a1; sizeb = sizea1;
	inverse = TRUE;
    } else {
	inverse = FALSE;
    }

    /* b1 * b - a1 * a = b */
    b1 = ALLOCA(Uint, sizea + sizeb);			/* b1 = 1 */
    memset(b1, '\0', (sizea + sizeb) * sizeof(Uint));
    b1[0] = 1;
    sizeb1 = 1;
    a1 = ALLOCA(Uint, sizea + sizeb);			/* a1 = 0 */
    memset(a1, '\0', (sizea + sizeb) * sizeof(Uint));
    sizea1 = 1;
    g1 = ALLOCA(Uint, sizeg1 = sizeb);			/* g1 = b */
    memcpy(g1, b, sizeg1 * sizeof(Uint));

    /* b2 * b - a2 * a = a */
    b2 = ALLOCA(Uint, sizea + sizeb);			/* b2 = a */
    memset(b2 + sizea, '\0', sizeb * sizeof(Uint));
    memcpy(b2, a, (sizeb2 = sizea) * sizeof(Uint));
    a2 = ALLOCA(Uint, sizea + sizeb);			/* a2 = b - 1 */
    memset(a2 + sizeb, '\0', sizea * sizeof(Uint));
    memcpy(a2, b, (sizea2 = sizeb) * sizeof(Uint));
    asi_sub(a2, b1, sizea2, 1);
    while (a2[sizea2 - 1] == 0) {
	if (--sizea2 == 0) {
	    sizea2++;
	    break;
	}
    }
    g2 = ALLOCA(Uint, sizeg2 = sizea);			/* g2 = a */
    memcpy(g2, a, sizeg2 * sizeof(Uint));

    do {
	do {
	    if (!(g1[0] & 1)) {
		if ((a1[0] | b1[0]) & 1) {
		    if (sizea1 < sizeb) {
			sizea1 = sizeb;
		    }
		    if (asi_add(a1, b, sizea1, sizeb)) {
			a1[sizea1++] = 1;
		    }
		    if (sizeb1 < sizea) {
			sizeb1 = sizea;
		    }
		    if (asi_add(b1, a, sizeb1, sizea)) {
			b1[sizeb1++] = 1;
		    }
		}
		asi_rshift(a1, sizea1, 1);
		if (a1[sizea1 - 1] == 0 && --sizea1 == 0) {
		    sizea1++;
		}
		asi_rshift(b1, sizeb1, 1);
		if (b1[sizeb1 - 1] == 0 && --sizeb1 == 0) {
		    sizeb1++;
		}
		asi_rshift(g1, sizeg1, 1);
		if (g1[sizeg1 - 1] == 0 && --sizeg1 == 0) {
		    sizeg1++;
		}
	    }
	    if (!(g2[0] & 1) || sizeg1 < sizeg2 ||
		asi_cmp(g1, g2, sizeg1, sizeg2) < 0) {
		register Uint *t, sizet;

		/* a1 <=> a2, b1 <=> b2, g1 <=> g2 */
		t = a1; sizet = sizea1;
		a1 = a2; sizea1 = sizea2;
		a2 = t; sizea2 = sizet;
		t = b1; sizet = sizeb1;
		b1 = b2; sizeb1 = sizeb2;
		b2 = t; sizeb2 = sizet;
		t = g1; sizet = sizeg1;
		g1 = g2; sizeg1 = sizeg2;
		g2 = t; sizeg2 = sizet;
	    }
	} while (!(g1[0] & 1));

	while (sizea1 < sizea2 || asi_cmp(a1, a2, sizea1, sizea2) < 0 ||
	       sizeb1 < sizeb2 || asi_cmp(b1, b2, sizeb1, sizeb2) < 0) {
	    if (sizea1 < sizeb) {
		sizea1 = sizeb;
	    }
	    if (asi_add(a1, b, sizea1, sizeb)) {
		b[sizea1++] = 1;
	    }
	    if (sizeb1 < sizea) {
		sizeb1 = sizea;
	    }
	    if (asi_add(b1, a, sizeb1, sizea)) {
		b1[sizeb1++] = 1;
	    }
	}

	asi_sub(a1, a2, sizea1, sizea2);
	while (a1[sizea1 - 1] == 0) {
	    if (--sizea1 == 0) {
		sizea1++;
		break;
	    }
	}
	asi_sub(b1, b2, sizeb1, sizeb2);
	while (b1[sizeb1 - 1] == 0) {
	    if (--sizeb1 == 0) {
		sizeb1++;
		break;
	    }
	}
	asi_sub(g1, g2, sizeg1, sizeg2);
	while (g1[sizeg1 - 1] == 0) {
	    if (--sizeg1 == 0) {
		sizeg1++;
		break;
	    }
	}
    } while (sizeg2 != 1 || g2[0] != 0);

    while (sizea1 >= sizeb && asi_cmp(a1, b, sizea1, sizeb) >= 0 &&
	   sizeb1 >= sizea && asi_cmp(b1, a, sizeb1, sizea) >= 0) {
	asi_sub(a1, b, sizea1, sizeb);
	while (a1[sizea1 - 1] == 0) {
	    if (--sizea1 == 0) {
		sizea1++;
		break;
	    }
	}
	asi_sub(b1, a, sizeb1, sizea);
	while (b1[sizeb1 - 1] == 0) {
	    if (--sizeb1 == 0) {
		sizeb1++;
		break;
	    }
	}
    }

    if (!inverse) {
	sizeb1 = sizeb;
	memcpy(b1, b, sizeb1 * sizeof(Uint));
	asi_sub(b1, a1, sizeb1, sizea1);
	while (c[sizeb1 - 1] == 0) {
	    if (--sizeb1 == 0) {
		sizeb1++;
		break;
	    }
	}
    }
    memcpy(c, b1, sizeb1 * sizeof(Uint));
    *sizec = sizeb1;
    inverse = (sizeg1 == 1 && g1[0] == 1);
    AFREE(g2);
    AFREE(a2);
    AFREE(b2);
    AFREE(g1);
    AFREE(a1);
    AFREE(b1);

    return inverse;
}

/*
 * The algorithms for fast exponentiation are based on:
 * "High-Speed RSA Implementation" by Çetin Koya Koç, Version 2.0,
 * RSA Laboratories
 */

/*
 * NAME:	asn->wordinv()
 * DESCRIPTION:	compute an inverse modulo the word size (for odd n)
 */
static Uint asn_wordinv(n)
register Uint n;
{
    register Uint n1, mask, i;

    n1 = 1;
    mask = 0x3;
    i = 0x2;
    do {
	if (((n * n1) & mask) > i) {
	    n1 += i;
	}
	i <<= 1;
	mask |= i;
    } while (i != 0);

    return n1;
}

/*
 * NAME:	asn->monpro()
 * DESCRIPTION:	compute the Montgomery product of a and b
 *		sizeof(t) = (size + 1) << 1
 */
static void asn_monpro(c, t, a, b, n, size, n0)
register Uint *c, size, n0;
Uint *t, *a, *b, *n;
{
    register Uint i, j, m, *d, carry;

    if (a == b) {
	asi_sqr(c, t, a, size);
    } else {
	asi_mult(c, t, a, b, size, size);
    }
    c[size << 1] = 0;
    for (i = size; i > 0; --i) {
	m = c[0] * n0;
	if (asi_mult_row(c++, n, m, size)) {
	    d = c + size;
	    j = i;
	    carry = 1;
	    do {
		carry = ((*d++ += carry) < carry);
	    } while (carry && --j != 0);
	}
    }
    if (asi_cmp(c, n, size + 1, size) >= 0) {
	asi_sub(c, n, size + 1, size);
    }
}

# define WINDOWSZ	6

/*
 * NAME:	asn->powqmod()
 * DESCRIPTION:	compute a ** b % mod (a > 1, b > 1, (mod & 1) != 0)
 *		sizeof(t) = (sizemod + 1) << 1
 */
static void asn_powqmod(c, t, a, b, mod, sizea, sizeb, sizemod)
Uint *c, *a, *b, *mod, sizea, sizeb;
register Uint *t, sizemod;
{
    register Uint bit, e, window, wsize, zsize, n0, *x, *xx, *y, *yy;
    Uint *tab[1 << WINDOWSZ];

    /* allocate */
    x = ALLOCA(Uint, (sizemod << 1) + 1);
    xx = x + sizemod;
    y = ALLOCA(Uint, (sizemod << 1) + sizea + 1);
    yy = y + sizemod;

    /* xx = a * R % mod */
    memset(y, '\0', sizemod * sizeof(Uint));
    memcpy(y + sizemod, a, sizea * sizeof(Uint));
    asi_div(y, t, y, mod, sizemod + sizea, sizemod);
    memcpy(xx, y, sizemod * sizeof(Uint));

    /* tab[] = { powers of xx } */
    memset(tab, '\0', (1 << WINDOWSZ) * sizeof(Uint*));
    tab[0] = xx;	/* sentinel */
    memcpy(tab[1] = ALLOCA(Uint, sizemod), xx, sizemod * sizeof(Uint));

    n0 = -asn_wordinv(mod[0]);
    window = wsize = zsize = 0;
    b += sizeb;
    e = *--b;
    for (bit = 0x80000000L; (e & bit) == 0; bit >>= 1) ;
    bit >>= 1;	/* skip most significant bit of top word */

    for (;;) {
	while (bit != 0) {
	    if (wsize == WINDOWSZ || (zsize != 0 && !(e & bit))) {
		asn_monpro(y, t, xx, tab[window], mod, sizemod, n0);
		window = wsize = zsize = 0;
	    } else {
		window <<= 1;
		if (tab[window] == (Uint *) NULL) {
		    asn_monpro(y, t, tab[window >> 1], tab[window >> 1], mod,
			       sizemod, n0);
		    memcpy(tab[window] = ALLOCA(Uint, sizemod), yy,
			   sizemod * sizeof(Uint));
		}
		memcpy(yy, xx, sizemod * sizeof(Uint));
	    }

	    asn_monpro(x, t, yy, yy, mod, sizemod, n0);

	    if (e & bit) {
		window |= 1;
		if (tab[window] == (Uint *) NULL) {
		    asn_monpro(y, t, tab[window - 1], tab[1], mod, sizemod, n0);
		    memcpy(tab[window] = ALLOCA(Uint, sizemod), yy,
			   sizemod * sizeof(Uint));
		}
		zsize = 0;
		wsize++;
	    } else if (wsize != 0) {
		zsize++;
		wsize++;
	    }

	    bit >>= 1;
	}

	if (--sizeb == 0) {
	    break;
	}
	/* next word in exponent */
	e = *--b;
	bit = 0x80000000L;
    }

    if (wsize != 0) {
	/*
	 * still a remaining window of bits to deal with
	 */
	asn_monpro(y, t, xx, tab[window], mod, sizemod, n0);
	memcpy(xx, yy, sizemod * sizeof(Uint));
    }

    /* c = xx * (R ** -1) */
    memset(tab[1], '\0', sizemod * sizeof(Uint));
    tab[1][0] = 1;
    asn_monpro(y, t, xx, tab[1], mod, sizemod, n0);
    memcpy(c, yy, sizemod * sizeof(Uint));

    for (window = 1 << WINDOWSZ; --window != 0; ) {
	if (tab[window] != (Uint *) NULL) {
	    AFREE(tab[window]);
	}
    }
    AFREE(y);
    AFREE(x);
}

/*
 * NAME:	asn->pow2mod()
 * DESCRIPTION:	compute a ** b, (all operations in size words)
 *		sizeof(t) = (size + 1) << 1
 */
static void asn_pow2mod(c, t, a, b, sizea, size)
register Uint *c, *t, *b, size;
Uint *a, sizea;
{
    register Uint *x, *y, *z, e, bit, sizeb;

    x = ALLOCA(Uint, size);
    y = ALLOCA(Uint, size << 1);
    z = ALLOCA(Uint, size << 1);

    /* x = a reduced to size words */
    if (sizea >= size) {
	memcpy(x, a, size * sizeof(Uint));
	sizea = size;
    } else {
	memcpy(x, a, sizea * sizeof(Uint));
	memset(x + sizea, '\0', (size - sizea) * sizeof(Uint));
    }

    /* remove leading zeroes from b */
    for (sizeb = size; b[sizeb - 1] == 0; --sizeb) {
	if (sizeb == 1) {
	    /* a ** 0 = 1 */
	    memset(c, '\0', size * sizeof(Uint));
	    c[0] = 1;
	    return;
	}
    }

    memcpy(y, x, size * sizeof(Uint));
    b += sizeb;
    e = *--b;
    for (bit = 0x80000000L; (e & bit) == 0; bit >>= 1) ;
    bit >>= 1;	/* skip most significant bit of top word */

    for (;;) {
	while (bit != 0) {
	    asi_sqr(z, t, y, size);
	    if (e & bit) {
		asi_mult(y, t, z, x, size, size);
	    } else {
		memcpy(y, z, size * sizeof(Uint));
	    }
	    bit >>= 1;
	}

	if (--sizeb == 0) {
	    break;
	}
	/* next word in exponent */
	e = *--b;
	bit = 0x80000000L;
    }

    memcpy(c, y, size * sizeof(Uint));

    AFREE(z);
    AFREE(y);
    AFREE(x);
}

/*
 * NAME:	asn->power()
 * DESCRIPTION:	compute a ** b % mod (a >= 0, b >= 0)
 *		sizeof(t) == (sizemod + 2) << 1
 */
static void asn_power(c, t, a, b, mod, sizea, sizeb, sizemod)
Uint *c, *t, *a, *b, *mod, sizea, sizeb, sizemod;
{
    if (sizeb == 1 && b[0] == 0) {
	/* a ** 0 = 1 */
	memset(c, '\0', sizemod * sizeof(Uint));
	c[0] = 1;
	return;
    }
    if (sizea == 1) {
	if (a[0] == 0) {
	    /* 0 ** b = 0 */
	    memset(c, '\0', sizemod * sizeof(Uint));
	    return;
	}
	if (a[0] == 1) {
	    /* 1 ** b = 1 */
	    memset(c, '\0', sizemod * sizeof(Uint));
	    c[0] = 1;
	    return;
	}
    }

    if (mod[0] & 1) {
	/*
	 * modulo odd number
	 */
	asn_powqmod(c, t, a, b, mod, sizea, sizeb, sizemod);
    } else {
	register Uint size, i, *x, *y, *z;
	Uint *q, *qinv, sizeq, sizeqinv;

	/*
	 * modulo even number
	 */
	x = ALLOCA(Uint, sizemod);
	y = ALLOCA(Uint, sizemod + 1);
	z = ALLOCA(Uint, sizemod << 1);
	q = ALLOCA(Uint, sizemod);
	qinv = ALLOCA(Uint, sizemod);

	/* j = (size << 5) + i = number of least significant zero bits */
	for (size = 0; mod[size] == 0; size++) ;
	for (i = 0; !(mod[size] & (1 << i)); i++) ;

	/* q = mod >> j */
	sizeq = sizemod - size;
	memcpy(q, mod, sizeq * sizeof(Uint));
	asi_rshift(q, sizeq, (size << 5) + i);
	if (mod[sizeq - 1] == 0) {
	    --sizeq;
	}

	/* size = number of words, i = mask */
	if (i != 0) {
	    i = 0xffffffffL >> (32 - i);
	    size++;
	} else {
	    i = 0xffffffffL;
	}

	/* x = b % 2 ** (j - 1) */
	if (sizeb >= size) {
	    memcpy(x, b, size * sizeof(Uint));
	} else {
	    memcpy(x, b, sizeb * sizeof(Uint));
	    memset(x + sizeb, '\0', (size - sizeb) * sizeof(Uint));
	}
	x[size - 1] &= i >> 1;

	/* y = a ** x % 2 ** j */
	asn_pow2mod(y, t, a, x, sizea, size);
	y[size - 1] &= i;

	if (sizeq != 1 || q[0] != 1) {
	    asn_powqmod(x, t, a, b, q, sizea, sizeb, sizeq);

	    /*
	     * y = x + q * ((y - x) * q ** -1 % 2 ** j)
	     */
	    if (sizeq < size) {
		memset(x + sizeq, '\0', (size - sizeq) * sizeof(Uint));
	    }
	    /* y - x */
	    asi_sub(y, x, size, size);
	    /* q ** -1 */
	    if (size == 1) {
		qinv[0] = asn_wordinv(q[0]);
		sizea = 1;
	    } else {
		memset(z, '\0', size * sizeof(Uint));
		if (i == 0xffffffffL) {
		    z[size] = 1;
		    sizea = size + 1;
		} else {
		    z[size - 1] = i + 1;
		    sizea = size;
		}
		asn_modinv(qinv, &sizea, q, z, sizeq, sizea);
	    }
	    /* (y - x) * q ** -1 % 2 ** j */
	    asi_mult(z, t, y, qinv, size, sizea);
	    z[size - 1] &= i;
	    /* q * ((y - x) * q ** -1 % 2 ** j) */
	    asi_mult(y, t, q, z, sizeq, size);
	    /* x + q * ((y - x) * q ** -1 % 2 ** j) */
	    asi_add(y, x, sizemod, sizeq);
	}

	memcpy(c, y, size * sizeof(Uint));

	AFREE(qinv);
	AFREE(q);
	AFREE(z);
	AFREE(y);
	AFREE(z);
    }
}

/*
 * NAME:	asn->pow()
 * DESCRIPTION:	compute a power of an ASI
 */
string *asn_pow(s1, s2, s3)
string *s1, *s2, *s3;
{
    register Uint *a, *b, *c, *t;
    Uint *mod, sizea, sizeb, sizemod;
    bool minusa, minusb;
    string *str;

    mod = ALLOCA(Uint, (s3->len >> 2) + 2);
    asi_strtonum(mod, s3, &sizemod, &minusa);
    if (minusa || (sizemod == 1 && mod[0] == 0)) {
	AFREE(mod);
	error("Invalid modulus");
    }

    a = ALLOCA(Uint, (s1->len >> 2) + 2);
    asi_strtonum(a, s1, &sizea, &minusa);
    b = ALLOCA(Uint, (s2->len >> 2) + 2);
    asi_strtonum(b, s2, &sizeb, &minusb);

    c = ALLOCA(Uint, sizemod);
    t = ALLOCA(Uint, (sizemod + 2) << 1);
    if (minusb) {
	/* a ** -b = (a ** -1) ** b */
	if (!asn_modinv(c, &sizea, a, mod, sizea, sizemod)) {
	    AFREE(t);
	    AFREE(c);
	    AFREE(b);
	    AFREE(a);
	    AFREE(mod);
	    error("No inverse");
	}
	asn_power(c, t, c, b, mod, sizea, sizeb, sizemod);
    } else {
	asn_power(c, t, a, b, mod, sizea, sizeb, sizemod);
    }
    str = asi_numtostr(c, sizemod, minusa & b[0]);
    AFREE(t);
    AFREE(c);

    AFREE(b);
    AFREE(a);
    AFREE(mod);
    return str;
}
