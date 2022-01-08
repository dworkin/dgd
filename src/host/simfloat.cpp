/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2022 DGD Authors (see the commit log for details)
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

# define INCLUDE_CTYPE
# include "dgd.h"
# include "xfloat.h"

struct flt {
    unsigned short sign;	/* 0: positive, 0x8000: negative */
    unsigned short exp;		/* bias: 32767 */
    unsigned short high;	/* 0 / 1 / 14 bits */
    Uint low;			/* 0 / 29 bits / 0 / 0 */
};

# define NBITS		44
# define BIAS		0x7fff


/* constants */

Float max_int =		{ 0x41df, 0xffffffc0L };	/* 0x7fffffff */
Float thousand =	{ 0x408f, 0x40000000L };	/* 1e3 */
Float thousandth =	{ 0x3f50, 0x624dd2f2L };	/* 1e-3 */

# define FLT_CONST(s, e, h, l)	{ (unsigned short) (s) << 15,		\
				  (e) + BIAS,				\
				  0x4000 + ((h) >> 2),			\
				  (((Uint) (h) << 29) +			\
				   ((l) << 1) + 2) & 0x7ffffffcL }

static flt half =	FLT_CONST(0,  -1, 0x0000, 0x0000000L);
static flt one =	FLT_CONST(0,   0, 0x0000, 0x0000000L);
static flt maxlog =	FLT_CONST(0,   9, 0x62e4, 0x2fefa39L);
static flt minlog =	FLT_CONST(1,   9, 0x62e4, 0x2fefa39L);
static flt sqrth =	FLT_CONST(0,  -1, 0x6a09, 0xe667f3bL);
static flt pi =		FLT_CONST(0,   1, 0x921f, 0xb54442dL);
static flt pio2 =	FLT_CONST(0,   0, 0x921f, 0xb54442dL);
static flt pio4 =	FLT_CONST(0,  -1, 0x921f, 0xb54442dL);
static flt ln2c1 =	FLT_CONST(0,  -1, 0x62e4, 0x0000000L);
static flt ln2c2 =	FLT_CONST(0, -20, 0x7f7d, 0x1cf79abL);


/*
 * NAME:	f_edom()
 * DESCRIPTION:	Domain error
 */
static void f_edom()
{
    error("Math argument");
}

/*
 * NAME:	f_erange()
 * DESCRIPTION:	Out of range
 */
static void f_erange()
{
    error("Result too large");
}

static void f_sub (flt*, flt*);

/*
 * NAME:	f_add()
 * DESCRIPTION:	a = a + b.  The result is normalized, but not guaranteed to
 *		be in range.
 */
static void f_add(flt *a, flt *b)
{
    unsigned short h, n;
    Uint l;
    flt tmp;

    if (b->exp == 0) {
	/* b is 0 */
	return;
    }
    if (a->exp == 0) {
	/* a is 0 */
	*a = *b;
	return;
    }
    if (a->sign != b->sign) {
	a->sign ^= 0x8000;
	f_sub(a, b);
	a->sign ^= 0x8000;
	return;
    }
    if (a->exp < b->exp) {
	/* b is the largest; exchange a and b */
	tmp = *a;
	*a = *b;
	b = &tmp;
    }

    n = a->exp - b->exp;
    if (n <= NBITS) {
	h = a->high;
	l = a->low;

	/*
	 * perform addition
	 */
	if (n < 31) {
	    h += (Uint) b->high >> n;
	    l += (((Uint) b->high << (31 - n)) & 0x7fffffffL) | (b->low >> n);
	} else {
	    l += b->high >> (n - 31);
	}
	if ((Int) l < 0) {
	    /* carry */
	    l &= 0x7fffffffL;
	    h++;
	}
	if ((short) h < 0) {
	    /* too large */
	    l |= (Uint) h << 31;
	    l >>= 1;
	    h >>= 1;
	    a->exp++;
	}

	/*
	 * rounding
	 */
	if ((Int) (l += 2) < 0 && (short) ++h < 0) {
	    h >>= 1;
	    a->exp++;
	}
	l &= 0x7ffffffcL;

	a->high = h;
	a->low = l;
    }
}

/*
 * NAME:	f_sub()
 * DESCRIPTION:	a = a - b.  The result is normalized, but not guaranteed to be
 *		in range.
 */
static void f_sub(flt *a, flt *b)
{
    unsigned short h, n;
    Uint l;
    flt tmp;

    if (b->exp == 0) {
	/* b is 0 */
	return;
    }
    if (a->exp == 0) {
	*a = *b;
	a->sign ^= 0x8000;
	return;
    }
    if (a->sign != b->sign) {
	a->sign ^= 0x8000;
	f_add(a, b);
	a->sign ^= 0x8000;
	return;
    }
    if (a->exp <= b->exp &&
	(a->exp < b->exp || (a->high <= b->high &&
			     (a->high < b->high || a->low < b->low)))) {
	/* b is the largest; exchange a and b */
	tmp = *a;
	*a = *b;
	b = &tmp;
	a->sign ^= 0x8000;
    }

    n = a->exp - b->exp;
    if (n <= NBITS) {
	h = a->high;
	l = a->low;

	/*
	 * perform subtraction
	 */
	if (n < 31) {
	    h -= (Uint) b->high >> n;
	    l -= (((Uint) b->high << (31 - n)) & 0x7fffffffL) | (b->low >> n);
	    if (b->low & ((1 << n) - 1)) {
		--l;
	    }
	} else {
	    n -= 31;
	    l -= b->high >> n;
	    if (b->low != 0 || (b->high & ((1 << n) - 1))) {
		--l;
	    }
	}
	if ((Int) l < 0) {
	    /* borrow */
	    l &= 0x7fffffffL;
	    --h;
	}

	/*
	 * normalize
	 */
	if (h == 0) {
	    if (l == 0) {
		a->exp = 0;
		return;
	    }
	    n = 15;
	    if ((l & 0xffff0000L) == 0) {
		l <<= 15;
		n += 15;
	    }
	    h = l >> 16;
	    l <<= 15;
	    l &= 0x7fffffffL;
	    a->exp -= n;
	}
	if (h < 0x4000) {
	    n = 0;
	    if ((h & 0xff00) == 0) {
		h <<= 7;
		n += 7;
	    }
	    while (h < 0x4000) {
		h <<= 1;
		n++;
	    }
	    h |= l >> (31 - n);
	    l <<= n;
	    l &= 0x7fffffffL;
	    a->exp -= n;
	}

	/*
	 * rounding
	 */
	if ((Int) (l += 2) < 0 && (short) ++h < 0) {
	    h >>= 1;
	    a->exp++;
	}
	l &= 0x7ffffffcL;

	a->high = h;
	a->low = l;
    }
}

/*
 * NAME:	f_mult()
 * DESCRIPTION:	a = a * b.  The result is normalized, but may be out of range.
 */
static void f_mult(flt *a, flt *b)
{
    Uint m, l, albl, ambm, ahbh;
    short al, am, ah, bl, bm, bh;

    if (a->exp == 0) {
	/* a is 0 */
	return;
    }
    if (b->exp == 0) {
	/* b is 0 */
	a->exp = 0;
	return;
    }

    al = ((unsigned short) a->low) >> 1;
    bl = ((unsigned short) b->low) >> 1;
    am = a->low >> 16;
    bm = b->low >> 16;
    ah = a->high;
    bh = b->high;

    albl = (Uint) al * bl;
    ambm = (Uint) am * bm;
    ahbh = (Uint) ah * bh;
    m = albl;
    m >>= 15;
    m += albl + ambm + (Int) (al - am) * (bm - bl);
    m >>= 15;
    m += albl + ambm + ahbh + (Int) (al - ah) * (bh - bl);
    m >>= 13;
    l = m & 0x03;
    m >>= 2;
    m += ambm + ahbh + (Int) (am - ah) * (bh - bm);
    l |= (m & 0x7fff) << 2;
    m >>= 15;
    m += ahbh;
    l |= m << 17;
    ah = m >> 14;

    a->sign ^= b->sign;
    a->exp += b->exp - BIAS;
    if (ah < 0) {
	ah = (unsigned short) ah >> 1;
	l >>= 1;
	a->exp++;
    }
    l &= 0x7fffffffL;

    /*
     * rounding
     */
    if ((Int) (l += 2) < 0 && ++ah < 0) {
	ah = (unsigned short) ah >> 1;
	a->exp++;
    }
    l &= 0x7ffffffcL;

    a->high = ah;
    a->low = l;
}

/*
 * NAME:	f_div()
 * DESCRIPTION:	a = a / b.  b must be non-zero.  The result is normalized,
 *		but may be out of range.
 */
static void f_div(flt *a, flt *b)
{
    unsigned short n[3];
    Uint numh, numl, divl, high, low, q;
    unsigned short divh, i;

    if (b->exp == 0) {
	error("Division by zero");
    }
    if (a->exp == 0) {
	/* a is 0 */
	return;
    }

    numh = ((Uint) a->high << 16) | (a->low >> 15);
    numl = a->low << 17;

    divh = (b->high << 1) | (b->low >> 30);
    divl = b->low << 2;

    n[0] = 0;
    n[1] = 0;
    i = 3;
    do {
	/* estimate the high word of the quotient */
	q = numh / divh;
	/* highlow = div * q */
	low = (unsigned short) (high = q * (unsigned short) divl);
	high >>= 16;
	high += q * (divl >> 16);
	low |= high << 16;
	high >>= 16;
	high += q * divh;

	/* the estimated quotient may be 2 off; correct it if needed */
	if (high >= numh && (high > numh || low > numl)) {
	    high -= divh;
	    if (low < divl) {
		--high;
	    }
	    low -= divl;
	    --q;
	    if (high >= numh && (high > numh || low > numl)) {
		high -= divh;
		if (low < divl) {
		    --high;
		}
		low -= divl;
		--q;
	    }
	}

	n[--i] = q;
	if (i == 0) {
	    break;
	}

	/* subtract highlow */
	numh -= high;
	if (numl < low) {
	    --numh;
	}
	numl -= low;
	numh <<= 16;
	numh |= numl >> 16;
	numl <<= 16;

    } while (numh != 0 || numl != 0);

    a->sign ^= b->sign;
    a->exp -= b->exp - BIAS + 1;
    high = n[2];
    low = ((Uint) n[1] << 15) | (n[0] >> 1);
    if ((short) high < 0) {
	low |= high << 31;
	low >>= 1;
	high >>= 1;
	a->exp++;
    }

    /*
     * rounding
     */
    if ((Int) (low += 2) < 0 && (short) ++high < 0) {
	high >>= 1;
	a->exp++;
    }
    low &= 0x7ffffffcL;

    a->high = high;
    a->low = low;
}

/*
 * NAME:	f_trunc()
 * DESCRIPTION:	truncate a flt
 */
static void f_trunc(flt *a)
{
    static unsigned short maskh[] = {
		0x4000, 0x6000, 0x7000, 0x7800, 0x7c00, 0x7e00, 0x7f00,
	0x7f80, 0x7fc0, 0x7fe0, 0x7ff0, 0x7ff8, 0x7ffc, 0x7ffe, 0x7fff,
		0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
	0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
	0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff,
	0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff
    };
    static Uint maskl[] = {
		     0x00000000L, 0x00000000L, 0x00000000L,
	0x00000000L, 0x00000000L, 0x00000000L, 0x00000000L,
	0x00000000L, 0x00000000L, 0x00000000L, 0x00000000L,
	0x00000000L, 0x00000000L, 0x00000000L, 0x00000000L,
		     0x40000000L, 0x60000000L, 0x70000000L,
	0x78000000L, 0x7c000000L, 0x7e000000L, 0x7f000000L,
	0x7f800000L, 0x7fc00000L, 0x7fe00000L, 0x7ff00000L,
	0x7ff80000L, 0x7ffc0000L, 0x7ffe0000L, 0x7fff0000L,
	0x7fff8000L, 0x7fffc000L, 0x7fffe000L, 0x7ffff000L,
	0x7ffff800L, 0x7ffffc00L, 0x7ffffe00L, 0x7fffff00L,
	0x7fffff80L, 0x7fffffc0L, 0x7fffffe0L, 0x7ffffff0L,
	0x7ffffff8L
    };

    if (a->exp < BIAS) {
	a->exp = 0;
    } else if (a->exp < BIAS + NBITS - 1) {
	a->high &= maskh[a->exp - BIAS];
	a->low &= maskl[a->exp - BIAS];
    }
}

/*
 * NAME:	f_round()
 * DESCRIPTION:	round off a flt
 */
static void f_round(flt *a)
{
    static flt half = FLT_CONST(0, -1, 0x0000, 0x0000000L);

    half.sign = a->sign;
    f_add(a, &half);
    f_trunc(a);
}

/*
 * NAME:	f_cmp()
 * DESCRIPTION:	compate two flts
 */
static int f_cmp(flt *a, flt *b)
{
    if (a->exp == 0) {
	if (b->exp == 0) {
	    return 0;
	}
	return (b->sign == 0) ? -1 : 1;
    } else if (b->exp == 0) {
	if (a->exp == 0) {
	    return 0;
	}
	return (a->sign == 0) ? 1 : -1;
    } else if (a->sign != b->sign) {
	return (a->sign == 0) ? 1 : -1;
    } else {
	if (a->exp == b->exp && a->high == b->high && a->low == b->low) {
	    return 0;
	}
	if (a->exp <= b->exp &&
	    (a->exp < b->exp || (a->high <= b->high &&
				 (a->high < b->high || a->low < b->low)))) {
	    return (a->sign == 0) ? -1 : 1;
	}
	return (a->sign == 0) ? 1 : -1;
    }
}

/*
 * NAME:	f_itof()
 * DESCRIPTION:	convert an integer to a flt
 */
static void f_itof(Int i, flt *a)
{
    Uint n;
    unsigned short shift;

    /* deal with zero and sign */
    if (i == 0) {
	a->exp = 0;
	return;
    } else if (i < 0) {
	a->sign = 0x8000;
	n = -i;
    } else {
	a->sign = 0;
	n = i;
    }

    shift = 0;
    while ((n & 0xff000000L) == 0) {
	n <<= 8;
	shift += 8;
    }
    while ((Int) n >= 0) {
	n <<= 1;
	shift++;
    }
    a->exp = BIAS + 31 - shift;
    a->high = n >> 17;
    a->low = (n << 14) & 0x7fffffff;
}

/*
 * NAME:	f_ftoi()
 * DESCRIPTION:	convert a flt to an integer, discarding the fractional part
 */
static Int f_ftoi(flt *a)
{
    Uint i;

    if (a->exp < BIAS) {
	return 0;
    }
    if (a->exp > BIAS + 30 &&
	(a->sign == 0 || a->exp != BIAS + 31 || a->high != 0x4000 ||
	 a->low != 0)) {
	f_erange();
    }

    i = (((Uint) a->high << 17) | (a->low >> 14)) >> (BIAS + 31 - a->exp);

    return (a->sign == 0) ? i : -i;
}

/*
 * NAME:	f_ftoxf()
 * DESCRIPTION:	convert flt to Float
 */
static void f_ftoxf(flt *a, Float *f)
{
    unsigned short exp;
    unsigned short high;
    Uint low;

    exp = a->exp;
    if (exp == 0) {
	/* zero */
	f->high = 0;
	f->low = 0;
	return;
    }
    high = a->high;
    low = a->low;

    /* rounding */
    if ((Int) (low += 0x100) < 0) {
	low = 0;
	if ((short) ++high < 0) {
	    high >>= 1;
	    exp++;
	}
    }

    /* exponent */
    if (exp > BIAS + 1023) {
	f_erange();
    }
    if (exp < BIAS - 1022) {
	/* underflow */
	f->high = 0;
	f->low = 0;
	return;
    }

    f->high = a->sign | ((exp - BIAS + 1023) << 4) | ((high >> 10) & 0x000f);
    f->low = ((Uint) high << 22) | (low >> 9);
}

/*
 * NAME:	f_xftof()
 * DESCRIPTION:	convert Float to flt
 */
static void f_xftof(Float *f, flt *a)
{
    unsigned short exp;

    a->sign = f->high & 0x8000;
    exp = (f->high >> 4) & 0x07ff;
    if (exp == 0) {
	/* zero */
	a->exp = 0;
	return;
    }
    a->exp = exp + BIAS - 1023;
    a->high = 0x4000 | ((f->high & 0x0f) << 10) | (f->low >> 22);
    a->low = (f->low << 9) & 0x7fffffffL;
}


static flt tens[] = {
    FLT_CONST(0,     3, 0x4000, 0x0000000L),	/* 10 ** 1 */
    FLT_CONST(0,     6, 0x9000, 0x0000000L),	/* 10 ** 2 */
    FLT_CONST(0,    13, 0x3880, 0x0000000L),	/* 10 ** 4 */
    FLT_CONST(0,    26, 0x7d78, 0x4000000L),	/* 10 ** 8 */
    FLT_CONST(0,    53, 0x1c37, 0x937e080L),	/* 10 ** 16 */
    FLT_CONST(0,   106, 0x3b8b, 0x5b5056eL),	/* 10 ** 32 */
    FLT_CONST(0,   212, 0x84f0, 0x3e93ff9L),	/* 10 ** 64 */
    FLT_CONST(0,   425, 0x2774, 0x8f9301dL),	/* 10 ** 128 */
    FLT_CONST(0,   850, 0x54fd, 0xd7f73bfL),	/* 10 ** 256 */
    FLT_CONST(0,  1700, 0xc633, 0x415d4c1L),	/* 10 ** 512 */
};

static flt tenths[] = {
    FLT_CONST(0,    -4, 0x9999, 0x9999999L),	/* 10 ** -1 */
    FLT_CONST(0,    -7, 0x47ae, 0x147ae14L),	/* 10 ** -2 */
    FLT_CONST(0,   -14, 0xa36e, 0x2eb1c43L),	/* 10 ** -4 */
    FLT_CONST(0,   -27, 0x5798, 0xee2308cL),	/* 10 ** -8 */
    FLT_CONST(0,   -54, 0xcd2b, 0x297d889L),	/* 10 ** -16 */
    FLT_CONST(0,  -107, 0x9f62, 0x3d5a8a7L),	/* 10 ** -32 */
    FLT_CONST(0,  -213, 0x50ff, 0xd44f4a7L),	/* 10 ** -64 */
    FLT_CONST(0,  -426, 0xbba0, 0x8cf8c97L),	/* 10 ** -128 */
    FLT_CONST(0,  -851, 0x8062, 0x864ac6fL),	/* 10 ** -256 */
    FLT_CONST(0, -1701, 0x2093, 0xdc65b64L),	/* 10 ** -512 */
};

/*
 * NAME:	Float::atof()
 * DESCRIPTION:	Convert a string to a float.  The string must be in the
 *		proper format.  Return TRUE if the operation was successful,
 *		FALSE otherwise.
 */
bool Float::atof(char **s, Float *f)
{
    flt a = { 0 };
    flt b, c, *t;
    unsigned short e, h;
    char *p, *q;
    bool digits;

    p = *s;

    /* sign */
    if (*p == '-') {
	a.sign = b.sign = 0x8000;
	p++;
    } else {
	a.sign = b.sign = 0;
    }

    a.exp = 0;
    b.low = 0;
    digits = FALSE;

    /* digits before . */
    while (isdigit(*p)) {
	f_mult(&a, &tens[0]);
	h = (*p++ - '0') << 12;
	if (h != 0) {
	    e = BIAS + 3;
	    while ((short) h >= 0) {
		h <<= 1;
		--e;
	    }
	    b.exp = e;
	    b.high = h >> 1;
	    f_add(&a, &b);
	}
	if (a.exp > 0xffff - 10) {
	    return FALSE;
	}
	digits = TRUE;
    }

    /* digits after . */
    if (*p == '.') {
	c = tenths[0];
	while (isdigit(*++p)) {
	    if (c.exp > 10) {
		h = (*p - '0') << 12;
		if (h != 0) {
		    e = BIAS + 3;
		    while ((short) h >= 0) {
			h <<= 1;
			--e;
		    }
		    b.exp = e;
		    b.high = h >> 1;
		    b.low = 0;
		    f_mult(&b, &c);
		    f_add(&a, &b);
		}
		f_mult(&c, &tenths[0]);
	    }
	    digits = TRUE;
	}
    }
    if (!digits) {
	return FALSE;
    }

    /* exponent */
    if (*p == 'e' || *p == 'E') {
	/* in case of no exponent */
	q = p;

	/* sign of exponent */
	if (*++p == '-') {
	    t = tenths;
	    p++;
	} else {
	    t = tens;
	    if (*p == '+') {
		p++;
	    }
	}

	if (isdigit(*p)) {
	    /* get exponent */
	    e = 0;
	    do {
		e *= 10;
		e += *p++ - '0';
		if (e >= 1024) {
		    return FALSE;
		}
	    } while (isdigit(*p));

	    /* adjust number */
	    while (e != 0) {
		if ((e & 1) != 0) {
		    f_mult(&a, t);
		    if (a.exp < 0x1000 || a.exp > 0xf000) {
			break;
		    }
		}
		e >>= 1;
		t++;
	    }
	} else {
	    /* roll back before exponent */
	    p = q;
	}
    }
    if (a.exp >= BIAS + 1023 &&
	(a.exp > BIAS + 1023 || (a.high == 0x7fff && a.low >= 0x7fffff00L))) {
	return FALSE;
    }

    f_ftoxf(&a, f);
    *s = p;
    return TRUE;
}

/*
 * NAME:	Float::ftoa()
 * DESCRIPTION:	convert a float to a string
 */
void Float::ftoa(char *buffer)
{
    unsigned short i;
    short e;
    Uint n;
    char *p;
    flt *t, *t2;
    char digits[10];
    flt a;

    f_xftof(this, &a);
    if (a.exp == 0) {
	strcpy(buffer, "0");
	return;
    }

    if (a.sign != 0) {
	*buffer++ = '-';
	a.sign = 0;
    }

    /* reduce the float to range 1 .. 9.999999999, and extract exponent */
    e = 0;
    if (a.exp >= BIAS) {
	/* >= 1 */
	for (i = 10, t = &tens[9], t2 = &tenths[9]; i > 0; --i, --t, --t2) {
	    e <<= 1;
	    if (f_cmp(&a, t) >= 0) {
		e |= 1;
		f_mult(&a, t2);
	    }
	}
    } else {
	/* < 1 */
	for (i = 10, t = &tenths[9], t2 = &tens[9]; i > 0; --i, --t, --t2) {
	    e <<= 1;
	    if (f_cmp(&a, t) <= 0) {
		e |= 1;
		f_mult(&a, t2);
	    }
	}
	if (a.exp < BIAS) {
	    /* still < 1 */
	    f_mult(&a, &tens[0]);
	    e++;
	}
	e = -e;
    }
    f_mult(&a, &tens[3]);

    /*
     * obtain digits
     */
    f_add(&a, &half);
    i = a.exp - BIAS + 1 - 15;
    n = ((Uint) a.high << i) | (a.low >> (31 - i));
    if (n == 1000000000L) {
	p = digits + 8;
	p[0] = '1';
	p[1] = '\0';
	i = 1;
	e++;
    } else {
	while (n != 0 && n % 10 == 0) {
	    n /= 10;
	}
	p = digits + 9;
	*p = '\0';
	i = 0;
	do {
	    i++;
	    *--p = '0' + n % 10;
	    n /= 10;
	} while (n != 0);
    }

    if (e >= 9 || (e < -3 && i - e > 9)) {
	buffer[0] = *p;
	if (i != 1) {
	    buffer[1] = '.';
	    memcpy(buffer + 2, p + 1, i - 1);
	    i++;
	}
	buffer[i++] = 'e';
	if (e >= 0) {
	    buffer[i] = '+';
	} else {
	    buffer[i] = '-';
	    e = -e;
	}
	p = digits + 9;
	do {
	    *--p = '0' + e % 10;
	    e /= 10;
	} while (e != 0);
	strcpy(buffer + i + 1, p);
    } else if (e < 0) {
	e = 1 - e;
	memcpy(buffer, "0.0000000", e);
	strcpy(buffer + e, p);
    } else {
	while (e >= 0) {
	    *buffer++ = (*p == '\0') ? '0' : *p++;
	    --e;
	}
	if (*p != '\0') {
	    *buffer = '.';
	    strcpy(buffer + 1, p);
	} else {
	    *buffer = '\0';
	}
    }
}

/*
 * NAME:	float->itof()
 * DESCRIPTION:	convert an integer to a float
 */
void Float::itof(Int i, Float *f)
{
    flt a;

    f_itof(i, &a);
    f_ftoxf(&a, f);
}

/*
 * NAME:	Float::ftoi()
 * DESCRIPTION:	convert a float to an integer
 */
Int Float::ftoi()
{
    flt a;

    f_xftof(this, &a);
    f_round(&a);
    return f_ftoi(&a);
}

/*
 * NAME:	Float::add()
 * DESCRIPTION:	add a Float
 */
void Float::add(Float &f)
{
    flt a, b;

    f_xftof(&f, &b);
    f_xftof(this, &a);
    f_add(&a, &b);
    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::sub()
 * DESCRIPTION:	subtract a Float
 */
void Float::sub(Float &f)
{
    flt a, b;

    f_xftof(&f, &b);
    f_xftof(this, &a);
    f_sub(&a, &b);
    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::mult()
 * DESCRIPTION:	multiply by a Float
 */
void Float::mult(Float &f)
{
    flt a, b;

    f_xftof(this, &a);
    f_xftof(&f, &b);
    f_mult(&a, &b);
    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::div()
 * DESCRIPTION:	divide by a Float
 */
void Float::div(Float &f)
{
    flt a, b;

    f_xftof(&f, &b);
    f_xftof(this, &a);
    f_div(&a, &b);
    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::cmp()
 * DESCRIPTION:	compare with a Float
 */
int Float::cmp(Float &f)
{
    if ((short) (high ^ f.high) < 0) {
	return ((short) high < 0) ? -1 : 1;
    }

    if (high == f.high && low == f.low) {
	return 0;
    }
    if (high <= f.high && (high < f.high || low < f.low)) {
	return ((short) high < 0) ? 1 : -1;
    }
    return ((short) high < 0) ? -1 : 1;
}

/*
 * NAME:	Float::floor()
 * DESCRIPTION:	round a float downwards
 */
void Float::floor()
{
    flt a, b;

    f_xftof(this, &a);
    b = a;
    f_trunc(&b);
    if (b.sign != 0 && f_cmp(&a, &b) != 0) {
	f_sub(&b, &one);
    }
    f_ftoxf(&b, this);
}

/*
 * NAME:	Float::ceil()
 * DESCRIPTION:	round a float upwards
 */
void Float::ceil()
{
    flt a, b;

    f_xftof(this, &a);
    b = a;
    f_trunc(&b);
    if (b.sign == 0 && f_cmp(&a, &b) != 0) {
	f_add(&b, &one);
    }
    f_ftoxf(&b, this);
}

/*
 * NAME:	Float::fmod()
 * DESCRIPTION:	perform fmod
 */
void Float::fmod(Float &f)
{
    flt a, b, c;
    unsigned short sign;

    f_xftof(&f, &b);
    if (b.exp == 0) {
	f_edom();
    }
    f_xftof(this, &a);
    if (a.exp == 0) {
	return;
    }

    sign = a.sign;
    a.sign = b.sign = 0;
    c = b;
    while (f_cmp(&a, &b) >= 0) {
	c.exp = a.exp;
	if (f_cmp(&a, &c) < 0) {
	    c.exp--;
	}
	f_sub(&a, &c);
    }

    a.sign = sign;
    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::frexp()
 * DESCRIPTION:	split a float into a fraction and an exponent
 */
Int Float::frexp()
{
    short e;

    if (high == 0) {
	return 0;
    }
    e = ((high & 0x7ff0) >> 4) - 1022;
    high = (high & 0x800f) | (1022 << 4);
    return e;
}

/*
 * NAME:	Float::ldexp()
 * DESCRIPTION:	make a float from a fraction and an exponent
 */
void Float::ldexp(Int exp)
{
    if (high == 0) {
	return;
    }
    exp += (high & 0x7ff0) >> 4;
    if (exp <= 0) {
	high = 0;
	low = 0;
	return;
    }
    if (exp > 1023 + 1023) {
	f_erange();
    }
    high = (high & 0x800f) | (exp << 4);
}

/*
 * NAME:	Float::modf()
 * DESCRIPTION:	split float into fraction and integer part
 */
void Float::modf(Float *f)
{
    flt a, b;

    f_xftof(this, &a);
    if (a.exp < BIAS) {
	b.exp = 0;
    } else {
	b = a;
	f_trunc(&b);
	f_sub(&a, &b);
    }
    f_ftoxf(&a, this);
    f_ftoxf(&b, f);
}


/*
 * The algorithms for much of the following are taken from the Cephes Math
 * Library 2.9, by Stephen L. Moshier.
 */

/*
 * NAME:	f_poly()
 * DESCRIPTION:	evaluate polynomial
 */
static void f_poly(flt *x, flt *coef, int n)
{
    flt result;

    result = *coef++;
    do {
	f_mult(&result, x);
	f_add(&result, coef++);
    } while (--n != 0);

    *x = result;
}

/*
 * NAME:	f_poly1()
 * DESCRIPTION:	evaluate polynomial with coefficient of x ** (n + 1) == 1.0.
 */
static void f_poly1(flt *x, flt *coef, int n)
{
    flt result;

    result = *x;
    f_add(&result, coef++);
    do {
	f_mult(&result, x);
	f_add(&result, coef++);
    } while (--n != 0);

    *x = result;
}

# define POLY(x, c)	f_poly(x, c, sizeof(c) / sizeof(flt) - 1)
# define POLY1(x, c)	f_poly1(x, c, sizeof(c) / sizeof(flt) - 1)

/*
 * NAME:	f_exp()
 * DESCRIPTION:	internal version of exp(f)
 */
static void f_exp(flt *a)
{
    static flt p[] = {
	FLT_CONST(0, -13, 0x089c, 0xdd5e44bL),
	FLT_CONST(0,  -6, 0xf06d, 0x10cca2cL),
	FLT_CONST(0,   0, 0x0000, 0x0000000L)
    };
    static flt q[] = {
	FLT_CONST(0, -19, 0x92eb, 0x6bc365fL),
	FLT_CONST(0,  -9, 0x4ae3, 0x9b508b6L),
	FLT_CONST(0,  -3, 0xd170, 0x99887e0L),
	FLT_CONST(0,   1, 0x0000, 0x0000000L)
    };
    static flt log2e = FLT_CONST(0, 0, 0x7154, 0x7652b82L);
    flt b, c;
    short n;

    b = *a;
    f_mult(&b, &log2e);
    f_add(&b, &half);
    f_trunc(&b);
    n = f_ftoi(&b);
    c = b;
    f_mult(&c, &ln2c1);
    f_sub(a, &c);
    f_mult(&b, &ln2c2);
    f_sub(a, &b);

    b = *a;
    f_mult(&b, a);
    c = b;
    POLY(&c, p);
    f_mult(a, &c);
    POLY(&b, q);
    f_sub(&b, a);
    f_div(a, &b);

    if (a->exp != 0) {
	a->exp++;
    }
    f_add(a, &one);
    if (a->exp != 0) {
	a->exp += n;
    }
}

/*
 * NAME:	Float::exp()
 * DESCRIPTION:	exp(f)
 */
void Float::exp()
{
    flt a;

    f_xftof(this, &a);
    if (f_cmp(&a, &maxlog) > 0) {
	/* overflow */
	f_erange();
    }
    if (f_cmp(&a, &minlog) < 0) {
	/* underflow */
	a.exp = 0;
    } else {
	f_exp(&a);
    }

    f_ftoxf(&a, this);
}

static flt logp[] = {
    FLT_CONST(0, -14, 0xab4c, 0x293c31bL),
    FLT_CONST(0,  -2, 0xfd6f, 0x53f5652L),
    FLT_CONST(0,   2, 0x2d2b, 0xaed9269L),
    FLT_CONST(0,   3, 0xcff7, 0x2c63eebL),
    FLT_CONST(0,   4, 0x1efd, 0x6924bc8L),
    FLT_CONST(0,   2, 0xed56, 0x37d7edcL)
};
static flt logq[] = {
    FLT_CONST(0,   3, 0x6932, 0x0ae97efL),
    FLT_CONST(0,   5, 0x69d2, 0xc4e19c0L),
    FLT_CONST(0,   6, 0x4bf3, 0x3a326bdL),
    FLT_CONST(0,   6, 0x1c9e, 0x2eb5eaeL),
    FLT_CONST(0,   4, 0x7200, 0xa9e1f25L)
};

static flt logr[] = {
    FLT_CONST(1,  -1, 0x9443, 0xddc6c0eL),
    FLT_CONST(0,   4, 0x062f, 0xc73027bL),
    FLT_CONST(1,   6, 0x0090, 0x611222aL)
};
static flt logs[] = {
    FLT_CONST(1,   5, 0x1d60, 0xd43ec6dL),
    FLT_CONST(0,   8, 0x3818, 0x0112ae4L),
    FLT_CONST(1,   9, 0x80d8, 0x919b33fL)
};

/*
 * NAME:	f_log()
 * DESCRIPTION:	internal version of log(f)
 */
static void f_log(flt *a)
{
    flt b, c, d, e;
    short n;

    n = a->exp - BIAS + 1;
    a->exp = BIAS - 1;

    if (n > 2 || n < -2) {
	if (f_cmp(a, &sqrth) < 0) {
	    --n;
	    f_sub(a, &half);
	    b = *a;
	} else {
	    b = *a;
	    f_sub(a, &half);
	    f_sub(a, &half);
	}
	if (b.exp != 0) {
	    --b.exp;
	}
	f_add(&b, &half);

	f_div(a, &b);
	b = *a;
	f_mult(&b, &b);
	c = b;
	POLY(&c, logr);
	f_mult(&c, &b);
	POLY1(&b, logs);
	f_div(&c, &b);
	f_mult(&c, a);

	f_itof(n, &d);
	b = d;
	f_mult(&b, &ln2c2);
	f_add(a, &b);
	f_add(a, &c);
	f_mult(&d, &ln2c1);
	f_add(a, &d);
    } else {
	if (f_cmp(a, &sqrth) < 0) {
	    --n;
	    a->exp++;
	}
	f_sub(a, &one);

	b = *a;
	f_mult(&b, a);
	c = *a;
	POLY(&c, logp);
	f_mult(&c, &b);
	d = *a;
	POLY1(&d, logq);
	f_div(&c, &d);
	f_mult(&c, a);

	if (n != 0) {
	    f_itof(n, &d);
	    e = d;
	    f_mult(&e, &ln2c2);
	    f_add(&c, &e);
	}
	if (b.exp != 0) {
	    --b.exp;
	    f_sub(&c, &b);
	}
	f_add(a, &c);
	if (n != 0) {
	    f_mult(&d, &ln2c1);
	    f_add(a, &d);
	}
    }
}

/*
 * NAME:	Float::log()
 * DESCRIPTION:	log(f)
 */
void Float::log()
{
    flt a;

    f_xftof(this, &a);
    if (a.sign != 0 || a.exp == 0) {
	/* <= 0.0 */
	f_edom();
    }

    f_log(&a);
    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::log10()
 * DESCRIPTION:	log10(f)
 */
void Float::log10()
{
    static flt l102a = FLT_CONST(0, -2, 0x4000, 0x0000000L);
    static flt l102b = FLT_CONST(1, -7, 0x77d9, 0x5ec10c0L);
    static flt l10eb = FLT_CONST(1, -4, 0x0d21, 0x3ab646bL);
    flt a, b, c, d, e;
    short n;

    f_xftof(this, &a);
    if (a.sign != 0 || a.exp == 0) {
	/* <= 0.0 */
	f_edom();
    }

    n = a.exp - BIAS + 1;
    a.exp = BIAS - 1;

    if (n > 2 || n < -2) {
	if (f_cmp(&a, &sqrth) < 0) {
	    --n;
	    f_sub(&a, &half);
	    b = a;
	} else {
	    b = a;
	    f_sub(&a, &half);
	    f_sub(&a, &half);
	}
	if (b.exp != 0) {
	    --b.exp;
	}
	f_add(&b, &half);

	f_div(&a, &b);
	b = a;
	f_mult(&b, &b);
	c = b;
	POLY(&c, logr);
	f_mult(&c, &b);
	POLY1(&b, logs);
	f_div(&c, &b);
	f_mult(&c, &a);
    } else {
	if (f_cmp(&a, &sqrth) < 0) {
	    --n;
	    a.exp++;
	}
	f_sub(&a, &one);

	b = a;
	f_mult(&b, &a);
	c = a;
	POLY(&c, logp);
	f_mult(&c, &b);
	d = a;
	POLY1(&d, logq);
	f_div(&c, &d);
	f_mult(&c, &a);
	if (b.exp != 0) {
	    --b.exp;
	    f_sub(&c, &b);
	}
    }

    b = c;
    f_mult(&b, &l10eb);
    d = a;
    f_mult(&d, &l10eb);
    f_add(&b, &d);
    if (n != 0) {
	f_itof(n, &d);
	e = d;
	f_mult(&e, &l102b);
	f_add(&b, &e);
    }
    if (c.exp != 0) {
	--c.exp;
	f_add(&b, &c);
    }
    if (a.exp != 0) {
	--a.exp;
	f_add(&b, &a);
    }
    if (n != 0) {
	f_mult(&d, &l102a);
	f_add(&b, &d);
    }

    f_ftoxf(&b, this);
}

/*
 * NAME:	f_powi()
 * DESCRIPTION:	take a number to an integer power
 */
static void f_powi(flt *a, int n)
{
    flt b;
    unsigned short sign;
    bool neg;

    if (n == 0) {
	/* pow(x, 0.0) == 1.0 */
	*a = one;
	return;
    }

    if (a->exp == 0) {
	if (n < 0) {
	    /* negative power of 0.0 */
	    f_edom();
	}
	/* pow(0.0, y) == 0.0 */
	return;
    }

    sign = a->sign;
    a->sign = 0;

    if (n < 0) {
	neg = TRUE;
	n = -n;
    } else {
	neg = FALSE;
    }

    if (n & 1) {
	b = *a;
    } else {
	b = one;
	sign = 0;
    }
    while ((n >>= 1) != 0) {
	f_mult(a, a);
	if (a->exp > BIAS + 1023) {
	    f_erange();
	}
	if (n & 1) {
	    f_mult(&b, a);
	}
    }
    /* range of b is checked when converting back to Float */

    b.sign = sign;
    if (neg) {
	*a = one;
	f_div(a, &b);
    } else {
	*a = b;
    }
}

/*
 * NAME:	Float::pow()
 * DESCRIPTION:	pow(f1, f2)
 */
void Float::pow(Float &f)
{
    flt a, b, c;
    unsigned short sign;

    f_xftof(this, &a);
    f_xftof(&f, &b);

    c = b;
    f_trunc(&c);
    if (f_cmp(&b, &c) == 0 && b.exp < 0x800e) {
	/* integer power < 32768 */
	f_powi(&a, (int) f_ftoi(&c));
	f_ftoxf(&a, this);
	return;
    }

    sign = a.sign;
    if (sign != 0) {
	if (f_cmp(&b, &c) != 0) {
	    /* non-integer power of negative number */
	    f_edom();
	}
	a.sign = 0;
	--c.exp;
	f_trunc(&c);
	if (f_cmp(&b, &c) == 0) {
	    /* even power of negative number */
	    sign = 0;
	}
    }
    if (a.exp == 0) {
	if (b.sign != 0) {
	    /* negative power of 0.0 */
	    f_edom();
	}
	/* pow(0.0, y) == 0.0 */
	return;
    }

    f_log(&a);
    f_mult(&a, &b);
    if (f_cmp(&a, &maxlog) > 0) {
	/* overflow */
	f_erange();
    }
    if (f_cmp(&a, &minlog) < 0) {
	/* underflow */
	a.exp = 0;
    } else {
	f_exp(&a);
    }
    a.sign = sign;

    f_ftoxf(&a, this);
}

/*
 * NAME:	f_sqrt()
 * DESCRIPTION:	internal version of sqrt(f)
 */
static void f_sqrt(flt *a)
{
    static flt c1 =    FLT_CONST(1, -3, 0xa29f, 0x864cdc3L);
    static flt c2 =    FLT_CONST(0, -1, 0xc7c7, 0x8481a43L);
    static flt c3 =    FLT_CONST(0, -2, 0x4117, 0xb9aebcaL);
    static flt sqrt2 = FLT_CONST(0,  0, 0x6a09, 0xe667f3bL);
    flt b, c;
    int n;

    if (a->exp == 0) {
	return;
    }

    b = *a;
    n = a->exp - BIAS + 1;
    a->exp = BIAS - 1;
    c = *a;
    f_mult(&c, &c1);
    f_add(&c, &c2);
    f_mult(a, &c);
    f_add(a, &c3);
    if (n & 1) {
	f_mult(a, &sqrt2);
    }
    a->exp += n >> 1;

    c = b;
    f_div(&c, a);
    f_add(a, &c);
    --a->exp;
    c = b;
    f_div(&c, a);
    f_add(a, &c);
    --a->exp;
    f_div(&b, a);
    f_add(a, &b);
    --a->exp;
}

/*
 * NAME:	Float::sqrt()
 * DESCRIPTION:	sqrt(f)
 */
void Float::sqrt()
{
    flt a;

    f_xftof(this, &a);
    if (a.sign != 0) {
	f_edom();
    }
    f_sqrt(&a);
    f_ftoxf(&a, this);
}

static flt sincof[] = {
    FLT_CONST(0, -33, 0x5d8f, 0xd1fd19cL),
    FLT_CONST(1, -26, 0xae5e, 0x5a9291fL),
    FLT_CONST(0, -19, 0x71de, 0x3567d48L),
    FLT_CONST(1, -13, 0xa01a, 0x019bfdfL),
    FLT_CONST(0,  -7, 0x1111, 0x11110f7L),
    FLT_CONST(1,  -3, 0x5555, 0x5555555L)
};
static flt coscof[] = {
    FLT_CONST(1, -37, 0x8fa4, 0x9a0861aL),
    FLT_CONST(0, -29, 0x1ee9, 0xd7b4e3fL),
    FLT_CONST(1, -22, 0x27e4, 0xf7eac4bL),
    FLT_CONST(0, -16, 0xa01a, 0x019c844L),
    FLT_CONST(1, -10, 0x6c16, 0xc16c14fL),
    FLT_CONST(0,  -5, 0x5555, 0x5555555L)
};
static flt sc1 = FLT_CONST(0,  -1, 0x921f, 0xb400000L);
static flt sc2 = FLT_CONST(0, -25, 0x4442, 0xd000000L);
static flt sc3 = FLT_CONST(0, -49, 0x8469, 0x898cc51L);

/*
 * NAME:	Float::cos()
 * DESCRIPTION:	cos(f)
 */
void Float::cos()
{
    flt a, b, c;
    int n;
    unsigned short sign;

    f_xftof(this, &a);
    if (a.exp >= 0x801d) {
	f_edom();
    }

    a.sign = sign = 0;
    b = a;
    f_div(&b, &pio4);
    f_trunc(&b);
    n = f_ftoi(&b);
    if (n & 1) {
	n++;
	f_add(&b, &one);
    }
    n &= 7;
    if (n > 3) {
	sign = 0x8000;
	n -= 4;
    }
    if (n > 1) {
	sign ^= 0x8000;
    }

    c = b;
    f_mult(&c, &sc1);
    f_sub(&a, &c);
    c = b;
    f_mult(&c, &sc2);
    f_sub(&a, &c);
    f_mult(&b, &sc3);
    f_sub(&a, &b);

    b = a;
    f_mult(&b, &a);
    if (n == 1 || n == 2) {
	c = b;
	f_mult(&b, &a);
	POLY(&c, sincof);
    } else {
	a = one;
	c = b;
	if (c.exp != 0) {
	    --c.exp;
	}
	f_sub(&a, &c);
	c = b;
	f_mult(&b, &b);
	POLY(&c, coscof);
    }
    f_mult(&b, &c);
    f_add(&a, &b);
    a.sign ^= sign;

    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::sin()
 * DESCRIPTION:	sin(f)
 */
void Float::sin()
{
    flt a, b, c;
    int n;
    unsigned short sign;

    f_xftof(this, &a);
    if (a.exp >= 0x801d) {
	f_edom();
    }

    sign = a.sign;
    a.sign = 0;
    b = a;
    f_div(&b, &pio4);
    f_trunc(&b);
    n = f_ftoi(&b);
    if (n & 1) {
	n++;
	f_add(&b, &one);
    }
    n &= 7;
    if (n > 3) {
	sign ^= 0x8000;
	n -= 4;
    }

    c = b;
    f_mult(&c, &sc1);
    f_sub(&a, &c);
    c = b;
    f_mult(&c, &sc2);
    f_sub(&a, &c);
    f_mult(&b, &sc3);
    f_sub(&a, &b);

    b = a;
    f_mult(&b, &a);
    if (n == 1 || n == 2) {
	a = one;
	c = b;
	if (c.exp != 0) {
	    --c.exp;
	}
	f_sub(&a, &c);
	c = b;
	f_mult(&b, &b);
	POLY(&c, coscof);
    } else {
	c = b;
	f_mult(&b, &a);
	POLY(&c, sincof);
    }
    f_mult(&b, &c);
    f_add(&a, &b);
    a.sign ^= sign;

    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::tan()
 * DESCRIPTION:	float(f)
 */
void Float::tan()
{
    static flt p[] = {
	FLT_CONST(1, 13, 0x992d, 0x8d24f3fL),
	FLT_CONST(0, 20, 0x199e, 0xca5fc9dL),
	FLT_CONST(1, 24, 0x11fe, 0xad32991L)
    };
    static flt q[] = {
	FLT_CONST(0, 13, 0xab8a, 0x5eeb365L),
	FLT_CONST(1, 20, 0x427b, 0xc582abcL),
	FLT_CONST(0, 24, 0x7d98, 0xfc2ead8L),
	FLT_CONST(1, 25, 0x9afe, 0x03cbe5aL)
    };
    static flt p1 = FLT_CONST(0,  -1, 0x921f, 0xb500000L);
    static flt p2 = FLT_CONST(0, -27, 0x110b, 0x4600000L);
    static flt p3 = FLT_CONST(0, -55, 0x1a62, 0x633145cL);
    flt a, b, c;
    int n;
    unsigned short sign;

    f_xftof(this, &a);
    if (a.exp >= 0x801d) {
	f_edom();
    }

    sign = a.sign;
    a.sign = 0;
    b = a;
    f_div(&b, &pio4);
    f_trunc(&b);
    n = f_ftoi(&b);
    if (n & 1) {
	n++;
	f_add(&b, &one);
    }

    c = b;
    f_mult(&c, &p1);
    f_sub(&a, &c);
    c = b;
    f_mult(&c, &p2);
    f_sub(&a, &c);
    f_mult(&b, &p3);
    f_sub(&a, &b);

    b = a;
    f_mult(&b, &a);
    if (b.exp > 0x7fd0) {	/* ~1e-14 */
	c = b;
	POLY(&b, p);
	f_mult(&b, &c);
	POLY1(&c, q);
	f_div(&b, &c);
	f_mult(&b, &a);
	f_add(&a, &b);
    }

    if (n & 2) {
	b = one;
	f_div(&b, &a);
	a = b;
	a.sign ^= 0x8000;
    }
    a.sign ^= sign;

    f_ftoxf(&a, this);
}

static flt ascp[] = {
    FLT_CONST(0, -8, 0x16b9, 0xb0bd48aL),
    FLT_CONST(1, -1, 0x3434, 0x1333e5cL),
    FLT_CONST(0,  2, 0x5c74, 0xb178a2dL),
    FLT_CONST(1,  4, 0x0433, 0x1de2790L),
    FLT_CONST(0,  4, 0x3900, 0x7da7792L),
    FLT_CONST(1,  3, 0x0656, 0xc06ceafL)
};
static flt ascq[] = {
    FLT_CONST(1,  3, 0xd7b5, 0x90b5e0eL),
    FLT_CONST(0,  6, 0x19fc, 0x025fe90L),
    FLT_CONST(1,  7, 0x265b, 0xb6d3576L),
    FLT_CONST(0,  7, 0x1705, 0x684ffbfL),
    FLT_CONST(1,  5, 0x8982, 0x20a3607L)
};

/*
 * NAME:	Float::acos()
 * DESCRIPTION:	acos(f)
 */
void Float::acos()
{
    flt a, b, c;
    unsigned short sign;
    bool flag;

    f_xftof(this, &a);
    sign = a.sign;
    a.sign = 0;
    if (f_cmp(&a, &one) > 0) {
	f_edom();
    }

    if (f_cmp(&a, &half) > 0) {
	b = half;
	f_sub(&b, &a);
	f_add(&b, &half);
	if (b.exp != 0) {
	    --b.exp;
	}
	a = b;
	f_sqrt(&a);
	flag = TRUE;
    } else {
	b = a;
	f_mult(&b, &a);
	flag = FALSE;
    }

    if (a.exp >= 0x7fe4) {	/* ~1e-8 */
	c = b;
	POLY(&c, ascp);
	f_mult(&c, &b);
	POLY1(&b, ascq);
	f_div(&c, &b);
	f_mult(&c, &a);
	f_add(&a, &c);
    }

    if (flag) {
	if (a.exp != 0) {
	    a.exp++;
	}
	if (sign != 0) {
	    b = pi;
	    f_sub(&b, &a);
	    a = b;
	}
    } else {
	if (sign != 0) {
	    f_add(&a, &pio2);
	} else {
	    b = pio2;
	    f_sub(&b, &a);
	    a = b;
	}
    }

    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::asin()
 * DESCRIPTION:	asin(f)
 */
void Float::asin()
{
    flt a, b, c;
    unsigned short sign;
    bool flag;

    f_xftof(this, &a);
    sign = a.sign;
    a.sign = 0;
    if (f_cmp(&a, &one) > 0) {
	f_edom();
    }

    if (f_cmp(&a, &half) > 0) {
	b = half;
	f_sub(&b, &a);
	f_add(&b, &half);
	if (b.exp != 0) {
	    --b.exp;
	}
	a = b;
	f_sqrt(&a);
	flag = TRUE;
    } else {
	b = a;
	f_mult(&b, &a);
	flag = FALSE;
    }

    if (a.exp >= 0x7fe4) {	/* ~1e-8 */
	c = b;
	POLY(&c, ascp);
	f_mult(&c, &b);
	POLY1(&b, ascq);
	f_div(&c, &b);
	f_mult(&c, &a);
	f_add(&a, &c);
    }

    if (flag) {
	if (a.exp != 0) {
	    a.exp++;
	}
	b = pio2;
	f_sub(&b, &a);
	a = b;
    }
    a.sign ^= sign;

    f_ftoxf(&a, this);
}

static flt atp[] = {
    FLT_CONST(1, -1, 0xc007, 0xfa1f725L),
    FLT_CONST(1,  4, 0x0285, 0x45b6b80L),
    FLT_CONST(1,  6, 0x2c08, 0xc368802L),
    FLT_CONST(1,  6, 0xeb8b, 0xf2d05baL),
    FLT_CONST(1,  6, 0x0366, 0x9fd28ecL)
};
static flt atq[] = {
    FLT_CONST(0,  4, 0x8dbc, 0x45b1460L),
    FLT_CONST(0,  7, 0x4a0d, 0xd43b8faL),
    FLT_CONST(0,  8, 0xb0e1, 0x8d2e2beL),
    FLT_CONST(0,  8, 0xe563, 0xf13b049L),
    FLT_CONST(0,  7, 0x8519, 0xefbbd62L)
};
static flt t3p8 = FLT_CONST(0,  1, 0x3504, 0xf333f9dL);
static flt tp8 =  FLT_CONST(0, -2, 0xa827, 0x999fcefL);

/*
 * NAME:	Float::atan()
 * DESCRIPTION:	atan(f)
 */
void Float::atan()
{
    flt a, b, c, d, e;
    unsigned short sign;

    f_xftof(this, &a);
    sign = a.sign;
    a.sign = 0;

    if (f_cmp(&a, &t3p8) > 0) {
	b = pio2;
	c = one;
	f_div(&c, &a);
	a = c;
	a.sign = 0x8000;
    } else if (f_cmp(&a, &tp8) > 0) {
	b = pio4;
	c = a;
	f_sub(&a, &one);
	f_add(&c, &one);
	f_div(&a, &c);
    } else {
	b.exp = 0;
    }

    c = a;
    f_mult(&c, &a);
    d = e = c;
    POLY(&c, atp);
    POLY1(&d, atq);
    f_div(&c, &d);
    f_mult(&c, &e);
    f_mult(&c, &a);
    f_add(&c, &b);
    f_add(&a, &c);
    a.sign ^= sign;

    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::atan2()
 * DESCRIPTION:	atan2(f)
 */
void Float::atan2(Float &f)
{
    flt a, b, c, d, e;
    unsigned short asign, bsign;

    f_xftof(this, &a);
    f_xftof(&f, &b);

    if (b.exp == 0) {
	if (a.exp == 0) {
	    /* atan2(0.0, 0.0); */
	    return;
	}
	a.exp = pio2.exp;
	a.high = pio2.high;
	a.low = pio2.low;
	f_ftoxf(&a, this);
	return;
    }
    if (a.exp == 0) {
	if (b.sign != 0) {
	    a = pi;
	}
	f_ftoxf(&a, this);
	return;
    }

    asign = a.sign;
    bsign = b.sign;
    f_div(&a, &b);
    a.sign = 0;

    if (f_cmp(&a, &t3p8) > 0) {
	b = pio2;
	c = one;
	f_div(&c, &a);
	a = c;
	a.sign = 0x8000;
    } else if (f_cmp(&a, &tp8) > 0) {
	b = pio4;
	c = a;
	f_sub(&a, &one);
	f_add(&c, &one);
	f_div(&a, &c);
    } else {
	b.exp = 0;
    }

    c = a;
    f_mult(&c, &a);
    d = e = c;
    POLY(&c, atp);
    POLY1(&d, atq);
    f_div(&c, &d);
    f_mult(&c, &e);
    f_mult(&c, &a);
    f_add(&c, &b);
    f_add(&a, &c);
    a.sign ^= asign ^ bsign;

    if (bsign != 0) {
	if (asign == 0) {
	    f_add(&a, &pi);
	} else {
	    f_sub(&a, &pi);
	}
    }

    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::cosh()
 * DESCRIPTION:	cosh(f)
 */
void Float::cosh()
{
    flt a, b;

    f_xftof(this, &a);
    a.sign = 0;
    if (f_cmp(&a, &maxlog) > 0) {
	f_erange();
    }

    f_exp(&a);
    b = one;
    f_div(&b, &a);
    f_add(&a, &b);
    --a.exp;

    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::sinh()
 * DESCRIPTION:	sinh(f)
 */
void Float::sinh()
{
    static flt p[] = {
	FLT_CONST(1, -1, 0x9435, 0xfe8bb3cL),
	FLT_CONST(1,  7, 0x4773, 0xa398ff4L),
	FLT_CONST(1, 13, 0x694b, 0x8c71d61L),
	FLT_CONST(1, 18, 0x5782, 0xbdbf6abL)
    };
    static flt q[] = {
	FLT_CONST(1,  8, 0x15b6, 0x096e964L),
	FLT_CONST(0, 15, 0x1a7b, 0xa7ed722L),
	FLT_CONST(1, 21, 0x01a2, 0x0e4f900L)
    };
    flt a, b, c, d;
    unsigned short sign;

    f_xftof(this, &a);
    if (f_cmp(&a, &maxlog) > 0 || f_cmp(&a, &minlog) < 0) {
	f_erange();
    }

    sign = a.sign;
    a.sign = 0;

    if (f_cmp(&a, &one) > 0) {
	f_exp(&a);
	b = half;
	f_div(&b, &a);
	--a.exp;
	f_sub(&a, &b);
	a.sign ^= sign;
    } else {
	b = a;
	f_mult(&b, &a);
	c = d = b;
	POLY(&c, p);
	POLY1(&d, q);
	f_div(&c, &d);
	f_mult(&b, &a);
	f_mult(&b, &c);
	f_add(&a, &b);
    }

    f_ftoxf(&a, this);
}

/*
 * NAME:	Float::tanh()
 * DESCRIPTION:	tanh(f)
 */
void Float::tanh()
{
    static flt p[] = {
	FLT_CONST(1, -1, 0xedc5, 0xbaafd6fL),
	FLT_CONST(1,  6, 0x8d26, 0xa0e2668L),
	FLT_CONST(1, 10, 0x93ac, 0x0305805L)
    };
    static flt q[] = {
	FLT_CONST(0,  6, 0xc33f, 0x28a581bL),
	FLT_CONST(0, 11, 0x176f, 0xa0e5535L),
	FLT_CONST(0, 12, 0x2ec1, 0x0244204L)
    };
    static flt mlog2 = FLT_CONST(0,  8, 0x62e4, 0x2fefa39L);
    static flt d625 =  FLT_CONST(0, -1, 0x4000, 0x0000000L);
    static flt two =   FLT_CONST(0,  1, 0x0000, 0x0000000L);
    flt a, b, c, d;
    unsigned short sign;

    f_xftof(this, &a);
    sign = a.sign;
    a.sign = 0;

    if (f_cmp(&a, &mlog2) > 0) {
	a.exp = one.exp;
	a.high = one.high;
	a.low = one.low;
    } else if (f_cmp(&a, &d625) >= 0) {
	a.exp++;
	f_exp(&a);
	f_add(&a, &one);
	b = two;
	f_div(&b, &a);
	a = one;
	f_sub(&a, &b);
    } else if (a.exp != 0) {
	b = a;
	f_mult(&b, &a);
	c = d = b;
	POLY(&c, p);
	POLY1(&d, q);
	f_div(&c, &d);
	f_mult(&b, &c);
	f_mult(&b, &a);
	f_add(&a, &b);
    }
    a.sign = sign;

    f_ftoxf(&a, this);
}
