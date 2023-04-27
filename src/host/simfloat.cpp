/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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

# ifdef LARGENUM
# error LARGENUM not supported with simulated floats
# endif

class Flt {
public:
    void add(Flt *b);
    void sub(Flt *b);
    void mult(Flt *b);
    void div(Flt *b);
    void trunc();
    void round();
    int cmp(Flt *b);
    void itof(LPCint i);
    LPCint ftoi();
    void toFloat(Float *f);
    void fromFloat(Float *f);
    void poly(Flt *coef, int n);
    void poly1(Flt *coef, int n);
    void expn();
    void log();
    void powi(int n);
    void sqrt();

    static void edom();
    static void erange();

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

# define FLT_CONST(s, e, h, l)	{ (unsigned short) (s) << 15,		\
				  (e) + BIAS,				\
				  0x4000 + ((h) >> 2),			\
				  (((Uint) (h) << 29) +			\
				   ((l) << 1) + 2) & 0x7ffffffcL }

static Flt half =	FLT_CONST(0,  -1, 0x0000, 0x0000000L);
static Flt one =	FLT_CONST(0,   0, 0x0000, 0x0000000L);
static Flt maxlog =	FLT_CONST(0,   9, 0x62e4, 0x2fefa39L);
static Flt minlog =	FLT_CONST(1,   9, 0x62e4, 0x2fefa39L);
static Flt sqrth =	FLT_CONST(0,  -1, 0x6a09, 0xe667f3bL);
static Flt pi =		FLT_CONST(0,   1, 0x921f, 0xb54442dL);
static Flt pio2 =	FLT_CONST(0,   0, 0x921f, 0xb54442dL);
static Flt pio4 =	FLT_CONST(0,  -1, 0x921f, 0xb54442dL);
static Flt ln2c1 =	FLT_CONST(0,  -1, 0x62e4, 0x0000000L);
static Flt ln2c2 =	FLT_CONST(0, -20, 0x7f7d, 0x1cf79abL);


/*
 * Domain error
 */
void Flt::edom()
{
    EC->error("Math argument");
}

/*
 * Out of range
 */
void Flt::erange()
{
    EC->error("Result too large");
}

/*
 * a = a + b.  The result is normalized, but not guaranteed to be in range.
 */
void Flt::add(Flt *b)
{
    unsigned short n;
    Flt tmp;

    if (b->exp == 0) {
	/* b is 0 */
	return;
    }
    if (exp == 0) {
	/* a is 0 */
	*this = *b;
	return;
    }
    if (sign != b->sign) {
	sign ^= 0x8000;
	sub(b);
	sign ^= 0x8000;
	return;
    }
    if (exp < b->exp) {
	/* b is the largest; exchange a and b */
	tmp = *this;
	*this = *b;
	b = &tmp;
    }

    n = exp - b->exp;
    if (n <= NBITS) {
	/*
	 * perform addition
	 */
	if (n < 31) {
	    high += (Uint) b->high >> n;
	    low += (((Uint) b->high << (31 - n)) & 0x7fffffffL) | (b->low >> n);
	} else {
	    low += b->high >> (n - 31);
	}
	if ((Int) low < 0) {
	    /* carry */
	    low &= 0x7fffffffL;
	    high++;
	}
	if ((short) high < 0) {
	    /* too large */
	    low |= (Uint) high << 31;
	    low >>= 1;
	    high >>= 1;
	    exp++;
	}

	/*
	 * rounding
	 */
	if ((Int) (low += 2) < 0 && (short) ++high < 0) {
	    high >>= 1;
	    exp++;
	}
	low &= 0x7ffffffcL;
    }
}

/*
 * a = a - b.  The result is normalized, but not guaranteed to be in range.
 */
void Flt::sub(Flt *b)
{
    unsigned short n;
    Flt tmp;

    if (b->exp == 0) {
	/* b is 0 */
	return;
    }
    if (exp == 0) {
	*this = *b;
	sign ^= 0x8000;
	return;
    }
    if (sign != b->sign) {
	sign ^= 0x8000;
	add(b);
	sign ^= 0x8000;
	return;
    }
    if (exp <= b->exp &&
	(exp < b->exp || (high <= b->high && (high < b->high || low < b->low))))
    {
	/* b is the largest; exchange a and b */
	tmp = *this;
	*this = *b;
	b = &tmp;
	sign ^= 0x8000;
    }

    n = exp - b->exp;
    if (n <= NBITS) {
	/*
	 * perform subtraction
	 */
	if (n < 31) {
	    high -= (Uint) b->high >> n;
	    low -= (((Uint) b->high << (31 - n)) & 0x7fffffffL) | (b->low >> n);
	    if (b->low & ((1 << n) - 1)) {
		--low;
	    }
	} else {
	    n -= 31;
	    low -= b->high >> n;
	    if (b->low != 0 || (b->high & ((1 << n) - 1))) {
		--low;
	    }
	}
	if ((Int) low < 0) {
	    /* borrow */
	    low &= 0x7fffffffL;
	    --high;
	}

	/*
	 * normalize
	 */
	if (high == 0) {
	    if (low == 0) {
		exp = 0;
		return;
	    }
	    n = 15;
	    if ((low & 0xffff0000L) == 0) {
		low <<= 15;
		n += 15;
	    }
	    high = low >> 16;
	    low <<= 15;
	    low &= 0x7fffffffL;
	    exp -= n;
	}
	if (high < 0x4000) {
	    n = 0;
	    if ((high & 0xff00) == 0) {
		high <<= 7;
		n += 7;
	    }
	    while (high < 0x4000) {
		high <<= 1;
		n++;
	    }
	    high |= low >> (31 - n);
	    low <<= n;
	    low &= 0x7fffffffL;
	    exp -= n;
	}

	/*
	 * rounding
	 */
	if ((Int) (low += 2) < 0 && (short) ++high < 0) {
	    high >>= 1;
	    exp++;
	}
	low &= 0x7ffffffcL;
    }
}

/*
 * a = a * b.  The result is normalized, but may be out of range.
 */
void Flt::mult(Flt *b)
{
    Uint m, albl, ambm, ahbh;
    short al, am, bl, bm, bh;

    if (exp == 0) {
	/* a is 0 */
	return;
    }
    if (b->exp == 0) {
	/* b is 0 */
	exp = 0;
	return;
    }

    al = ((unsigned short) low) >> 1;
    bl = ((unsigned short) b->low) >> 1;
    am = low >> 16;
    bm = b->low >> 16;
    bh = b->high;

    albl = (Uint) al * bl;
    ambm = (Uint) am * bm;
    ahbh = (Uint) high * bh;
    m = albl;
    m >>= 15;
    m += albl + ambm + (Int) (al - am) * (bm - bl);
    m >>= 15;
    m += albl + ambm + ahbh + (Int) (al - (short) high) * (bh - bl);
    m >>= 13;
    low = m & 0x03;
    m >>= 2;
    m += ambm + ahbh + (Int) (am - (short) high) * (bh - bm);
    low |= (m & 0x7fff) << 2;
    m >>= 15;
    m += ahbh;
    low |= m << 17;
    high = m >> 14;

    sign ^= b->sign;
    exp += b->exp - BIAS;
    if ((short) high < 0) {
	high >>= 1;
	low >>= 1;
	exp++;
    }
    low &= 0x7fffffffL;

    /*
     * rounding
     */
    if ((Int) (low += 2) < 0 && (short) ++high < 0) {
	high >>= 1;
	exp++;
    }
    low &= 0x7ffffffcL;
}

/*
 * a = a / b.  b must be non-zero.  The result is normalized, but may be out
 * of range.
 */
void Flt::div(Flt *b)
{
    unsigned short n[3];
    Uint numh, numl, divl, h, l, q;
    unsigned short divh, i;

    if (b->exp == 0) {
	EC->error("Division by zero");
    }
    if (exp == 0) {
	/* a is 0 */
	return;
    }

    numh = ((Uint) high << 16) | (low >> 15);
    numl = low << 17;

    divh = (b->high << 1) | (b->low >> 30);
    divl = b->low << 2;

    n[0] = 0;
    n[1] = 0;
    i = 3;
    do {
	/* estimate the high word of the quotient */
	q = numh / divh;
	/* highlow = div * q */
	l = (unsigned short) (h = q * (unsigned short) divl);
	h >>= 16;
	h += q * (divl >> 16);
	l |= h << 16;
	h >>= 16;
	h += q * divh;

	/* the estimated quotient may be 2 off; correct it if needed */
	if (h >= numh && (h > numh || l > numl)) {
	    h -= divh;
	    if (l < divl) {
		--h;
	    }
	    l -= divl;
	    --q;
	    if (h >= numh && (h > numh || l > numl)) {
		h -= divh;
		if (l < divl) {
		    --h;
		}
		l -= divl;
		--q;
	    }
	}

	n[--i] = q;
	if (i == 0) {
	    break;
	}

	/* subtract highlow */
	numh -= h;
	if (numl < l) {
	    --numh;
	}
	numl -= l;
	numh <<= 16;
	numh |= numl >> 16;
	numl <<= 16;

    } while (numh != 0 || numl != 0);

    sign ^= b->sign;
    exp -= b->exp - BIAS + 1;
    h = n[2];
    l = ((Uint) n[1] << 15) | (n[0] >> 1);
    if ((short) h < 0) {
	l |= h << 31;
	l >>= 1;
	h >>= 1;
	exp++;
    }

    /*
     * rounding
     */
    if ((Int) (l += 2) < 0 && (short) ++h < 0) {
	h >>= 1;
	exp++;
    }
    l &= 0x7ffffffcL;

    high = h;
    low = l;
}

/*
 * truncate a Flt
 */
void Flt::trunc()
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

    if (exp < BIAS) {
	exp = 0;
    } else if (exp < BIAS + NBITS - 1) {
	high &= maskh[exp - BIAS];
	low &= maskl[exp - BIAS];
    }
}

/*
 * round a Flt
 */
void Flt::round()
{
    static Flt half = FLT_CONST(0, -1, 0x0000, 0x0000000L);

    half.sign = sign;
    add(&half);
    trunc();
}

/*
 * compate two flts
 */
int Flt::cmp(Flt *b)
{
    if (exp == 0) {
	if (b->exp == 0) {
	    return 0;
	}
	return (b->sign == 0) ? -1 : 1;
    } else if (b->exp == 0) {
	if (exp == 0) {
	    return 0;
	}
	return (sign == 0) ? 1 : -1;
    } else if (sign != b->sign) {
	return (sign == 0) ? 1 : -1;
    } else {
	if (exp == b->exp && high == b->high && low == b->low) {
	    return 0;
	}
	if (exp <= b->exp &&
	    (exp < b->exp || (high <= b->high &&
			      (high < b->high || low < b->low)))) {
	    return (sign == 0) ? -1 : 1;
	}
	return (sign == 0) ? 1 : -1;
    }
}

/*
 * convert an integer to a Flt
 */
void Flt::itof(LPCint i)
{
    LPCuint n;
    unsigned short shift;

    /* deal with zero and sign */
    if (i == 0) {
	exp = 0;
	return;
    } else if (i < 0) {
	sign = 0x8000;
	n = -i;
    } else {
	sign = 0;
	n = i;
    }

    shift = 0;
    while ((n & 0xff000000L) == 0) {
	n <<= 8;
	shift += 8;
    }
    while ((LPCint) n >= 0) {
	n <<= 1;
	shift++;
    }
    exp = BIAS + 31 - shift;
    high = n >> 17;
    low = (n << 14) & 0x7fffffff;
}

/*
 * convert a Flt to an integer, discarding the fractional part
 */
LPCint Flt::ftoi()
{
    LPCuint i;

    if (exp < BIAS) {
	return 0;
    }
    if (exp > BIAS + 30 &&
	(sign == 0 || exp != BIAS + 31 || high != 0x4000 || low != 0)) {
	erange();
    }

    i = (((Uint) high << 17) | (low >> 14)) >> (BIAS + 31 - exp);

    return (sign == 0) ? i : -i;
}

/*
 * convert Flt to Float
 */
void Flt::toFloat(Float *f)
{
    unsigned short e;
    unsigned short h;
    Uint l;

    e = exp;
    if (e == 0) {
	/* zero */
	f->high = 0;
	f->low = 0;
	return;
    }
    h = high;
    l = low;

    /* rounding */
    if ((Int) (l += 0x100) < 0) {
	l = 0;
	if ((short) ++h < 0) {
	    h >>= 1;
	    e++;
	}
    }

    /* exponent */
    if (e > BIAS + 1023) {
	erange();
    }
    if (e < BIAS - 1022) {
	/* underflow */
	f->high = 0;
	f->low = 0;
	return;
    }

    f->high = sign | ((e - BIAS + 1023) << 4) | ((h >> 10) & 0x000f);
    f->low = ((Uint) h << 22) | (l >> 9);
}

/*
 * convert Float to Flt
 */
void Flt::fromFloat(Float *f)
{
    sign = f->high & 0x8000;
    exp = (f->high >> 4) & 0x07ff;
    if (exp == 0) {
	/* zero */
	return;
    }
    exp += BIAS - 1023;
    high = 0x4000 | ((f->high & 0x0f) << 10) | (f->low >> 22);
    low = (f->low << 9) & 0x7fffffffL;
}

/*
 * The algorithms for much of the following are taken from the Cephes Math
 * Library 2.9, by Stephen L. Moshier.
 */

/*
 * evaluate polynomial
 */
void Flt::poly(Flt *coef, int n)
{
    Flt result;

    result = *coef++;
    do {
	result.mult(this);
	result.add(coef++);
    } while (--n != 0);

    *this = result;
}

/*
 * evaluate polynomial with coefficient of x ** (n + 1) == 1.0.
 */
void Flt::poly1(Flt *coef, int n)
{
    Flt result;

    result = *this;
    result.add(coef++);
    do {
	result.mult(this);
	result.add(coef++);
    } while (--n != 0);

    *this = result;
}

# define POLY(x, c)	((x)->poly(c, sizeof(c) / sizeof(Flt) - 1))
# define POLY1(x, c)	((x)->poly1(c, sizeof(c) / sizeof(Flt) - 1))

/*
 * internal version of exp(f)
 */
void Flt::expn()
{
    static Flt p[] = {
	FLT_CONST(0, -13, 0x089c, 0xdd5e44bL),
	FLT_CONST(0,  -6, 0xf06d, 0x10cca2cL),
	FLT_CONST(0,   0, 0x0000, 0x0000000L)
    };
    static Flt q[] = {
	FLT_CONST(0, -19, 0x92eb, 0x6bc365fL),
	FLT_CONST(0,  -9, 0x4ae3, 0x9b508b6L),
	FLT_CONST(0,  -3, 0xd170, 0x99887e0L),
	FLT_CONST(0,   1, 0x0000, 0x0000000L)
    };
    static Flt log2e = FLT_CONST(0, 0, 0x7154, 0x7652b82L);
    Flt b, c;
    short n;

    b = *this;
    b.mult(&log2e);
    b.add(&half);
    b.trunc();
    n = b.ftoi();
    c = b;
    c.mult(&ln2c1);
    sub(&c);
    b.mult(&ln2c2);
    sub(&b);

    b = *this;
    b.mult(this);
    c = b;
    POLY(&c, p);
    mult(&c);
    POLY(&b, q);
    b.sub(this);
    div(&b);

    if (exp != 0) {
	exp++;
    }
    add(&one);
    if (exp != 0) {
	exp += n;
    }
}

static Flt logp[] = {
    FLT_CONST(0, -14, 0xab4c, 0x293c31bL),
    FLT_CONST(0,  -2, 0xfd6f, 0x53f5652L),
    FLT_CONST(0,   2, 0x2d2b, 0xaed9269L),
    FLT_CONST(0,   3, 0xcff7, 0x2c63eebL),
    FLT_CONST(0,   4, 0x1efd, 0x6924bc8L),
    FLT_CONST(0,   2, 0xed56, 0x37d7edcL)
};
static Flt logq[] = {
    FLT_CONST(0,   3, 0x6932, 0x0ae97efL),
    FLT_CONST(0,   5, 0x69d2, 0xc4e19c0L),
    FLT_CONST(0,   6, 0x4bf3, 0x3a326bdL),
    FLT_CONST(0,   6, 0x1c9e, 0x2eb5eaeL),
    FLT_CONST(0,   4, 0x7200, 0xa9e1f25L)
};

static Flt logr[] = {
    FLT_CONST(1,  -1, 0x9443, 0xddc6c0eL),
    FLT_CONST(0,   4, 0x062f, 0xc73027bL),
    FLT_CONST(1,   6, 0x0090, 0x611222aL)
};
static Flt logs[] = {
    FLT_CONST(1,   5, 0x1d60, 0xd43ec6dL),
    FLT_CONST(0,   8, 0x3818, 0x0112ae4L),
    FLT_CONST(1,   9, 0x80d8, 0x919b33fL)
};

/*
 * internal version of log(f)
 */
void Flt::log()
{
    Flt b, c, d, e;
    short n;

    n = exp - BIAS + 1;
    exp = BIAS - 1;

    if (n > 2 || n < -2) {
	if (cmp(&sqrth) < 0) {
	    --n;
	    sub(&half);
	    b = *this;
	} else {
	    b = *this;
	    sub(&half);
	    sub(&half);
	}
	if (b.exp != 0) {
	    --b.exp;
	}
	b.add(&half);

	div(&b);
	b = *this;
	b.mult(&b);
	c = b;
	POLY(&c, logr);
	c.mult(&b);
	POLY1(&b, logs);
	c.div(&b);
	c.mult(this);

	d.itof(n);
	b = d;
	b.mult(&ln2c2);
	add(&b);
	add(&c);
	d.mult(&ln2c1);
	add(&d);
    } else {
	if (cmp(&sqrth) < 0) {
	    --n;
	    exp++;
	}
	sub(&one);

	b = *this;
	b.mult(this);
	c = *this;
	POLY(&c, logp);
	c.mult(&b);
	d = *this;
	POLY1(&d, logq);
	c.div(&d);
	c.mult(this);

	if (n != 0) {
	    d.itof(n);
	    e = d;
	    e.mult(&ln2c2);
	    c.add(&e);
	}
	if (b.exp != 0) {
	    --b.exp;
	    c.sub(&b);
	}
	add(&c);
	if (n != 0) {
	    d.mult(&ln2c1);
	    add(&d);
	}
    }
}

/*
 * take a number to an integer power
 */
void Flt::powi(int n)
{
    Flt b;
    unsigned short bsign;
    bool neg;

    if (n == 0) {
	/* pow(x, 0.0) == 1.0 */
	*this = one;
	return;
    }

    if (exp == 0) {
	if (n < 0) {
	    /* negative power of 0.0 */
	    edom();
	}
	/* pow(0.0, y) == 0.0 */
	return;
    }

    bsign = sign;
    sign = 0;

    if (n < 0) {
	neg = TRUE;
	n = -n;
    } else {
	neg = FALSE;
    }

    if (n & 1) {
	b = *this;
    } else {
	b = one;
	bsign = 0;
    }
    while ((n >>= 1) != 0) {
	mult(this);
	if (exp > BIAS + 1023) {
	    erange();
	}
	if (n & 1) {
	    b.mult(this);
	}
    }
    /* range of b is checked when converting back to Float */

    b.sign = bsign;
    if (neg) {
	*this = one;
	div(&b);
    } else {
	*this = b;
    }
}

/*
 * internal version of sqrt(f)
 */
void Flt::sqrt()
{
    static Flt c1 =    FLT_CONST(1, -3, 0xa29f, 0x864cdc3L);
    static Flt c2 =    FLT_CONST(0, -1, 0xc7c7, 0x8481a43L);
    static Flt c3 =    FLT_CONST(0, -2, 0x4117, 0xb9aebcaL);
    static Flt sqrt2 = FLT_CONST(0,  0, 0x6a09, 0xe667f3bL);
    Flt b, c;
    int n;

    if (exp == 0) {
	return;
    }

    b = *this;
    n = exp - BIAS + 1;
    exp = BIAS - 1;
    c = *this;
    c.mult(&c1);
    c.add(&c2);
    mult(&c);
    add(&c3);
    if (n & 1) {
	mult(&sqrt2);
    }
    exp += n >> 1;

    c = b;
    c.div(this);
    add(&c);
    --exp;
    c = b;
    c.div(this);
    add(&c);
    --exp;
    b.div(this);
    add(&b);
    --exp;
}


static Flt tens[] = {
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

static Flt tenths[] = {
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
 * Convert a string to a float.  The string must be in the proper format.
 * Return TRUE if the operation was successful, FALSE otherwise.
 */
bool Float::atof(char **s, Float *f)
{
    Flt a = { 0 };
    Flt b, c, *t;
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
	a.mult(&tens[0]);
	h = (*p++ - '0') << 12;
	if (h != 0) {
	    e = BIAS + 3;
	    while ((short) h >= 0) {
		h <<= 1;
		--e;
	    }
	    b.exp = e;
	    b.high = h >> 1;
	    a.add(&b);
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
		    b.mult(&c);
		    a.add(&b);
		}
		c.mult(&tenths[0]);
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
		    a.mult(t);
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

    a.toFloat(f);
    *s = p;
    return TRUE;
}

/*
 * convert a float to a string
 */
void Float::ftoa(char *buffer)
{
    unsigned short i;
    short e;
    Uint n;
    char *p;
    Flt *t, *t2;
    char digits[10];
    Flt a;

    a.fromFloat(this);
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
	    if (a.cmp(t) >= 0) {
		e |= 1;
		a.mult(t2);
	    }
	}
    } else {
	/* < 1 */
	for (i = 10, t = &tenths[9], t2 = &tens[9]; i > 0; --i, --t, --t2) {
	    e <<= 1;
	    if (a.cmp(t) <= 0) {
		e |= 1;
		a.mult(t2);
	    }
	}
	if (a.exp < BIAS) {
	    /* still < 1 */
	    a.mult(&tens[0]);
	    e++;
	}
	e = -e;
    }
    a.mult(&tens[3]);

    /*
     * obtain digits
     */
    a.add(&half);
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
 * convert an integer to a float
 */
void Float::itof(LPCint i, Float *f)
{
    Flt a;

    a.itof(i);
    a.toFloat(f);
}

/*
 * convert a float to an integer
 */
LPCint Float::ftoi()
{
    Flt a;

    a.fromFloat(this);
    a.round();
    return a.ftoi();
}

/*
 * add a Float
 */
void Float::add(Float &f)
{
    Flt a, b;

    b.fromFloat(&f);
    a.fromFloat(this);
    a.add(&b);
    a.toFloat(this);
}

/*
 * subtract a Float
 */
void Float::sub(Float &f)
{
    Flt a, b;

    b.fromFloat(&f);
    a.fromFloat(this);
    a.sub(&b);
    a.toFloat(this);
}

/*
 * multiply by a Float
 */
void Float::mult(Float &f)
{
    Flt a, b;

    a.fromFloat(this);
    b.fromFloat(&f);
    a.mult(&b);
    a.toFloat(this);
}

/*
 * divide by a Float
 */
void Float::div(Float &f)
{
    Flt a, b;

    b.fromFloat(&f);
    a.fromFloat(this);
    a.div(&b);
    a.toFloat(this);
}

/*
 * compare with a Float
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
 * round a float downwards
 */
void Float::floor()
{
    Flt a, b;

    a.fromFloat(this);
    b = a;
    b.trunc();
    if (b.sign != 0 && a.cmp(&b) != 0) {
	b.sub(&one);
    }
    b.toFloat(this);
}

/*
 * round a float upwards
 */
void Float::ceil()
{
    Flt a, b;

    a.fromFloat(this);
    b = a;
    b.trunc();
    if (b.sign == 0 && a.cmp(&b) != 0) {
	b.add(&one);
    }
    b.toFloat(this);
}

/*
 * perform fmod
 */
void Float::fmod(Float &f)
{
    Flt a, b, c;
    unsigned short sign;

    b.fromFloat(&f);
    if (b.exp == 0) {
	Flt::edom();
    }
    a.fromFloat(this);
    if (a.exp == 0) {
	return;
    }

    sign = a.sign;
    a.sign = b.sign = 0;
    c = b;
    while (a.cmp(&b) >= 0) {
	c.exp = a.exp;
	if (a.cmp(&c) < 0) {
	    c.exp--;
	}
	a.sub(&c);
    }

    a.sign = sign;
    a.toFloat(this);
}

/*
 * split a float into a fraction and an exponent
 */
LPCint Float::frexp()
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
 * make a float from a fraction and an exponent
 */
void Float::ldexp(LPCint exp)
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
	Flt::erange();
    }
    high = (high & 0x800f) | (exp << 4);
}

/*
 * split float into fraction and integer part
 */
void Float::modf(Float *f)
{
    Flt a, b;

    a.fromFloat(this);
    if (a.exp < BIAS) {
	b.exp = 0;
    } else {
	b = a;
	b.trunc();
	a.sub(&b);
    }
    a.toFloat(this);
    b.toFloat(f);
}


/*
 * exp(f)
 */
void Float::exp()
{
    Flt a;

    a.fromFloat(this);
    if (a.cmp(&maxlog) > 0) {
	/* overflow */
	Flt::erange();
    }
    if (a.cmp(&minlog) < 0) {
	/* underflow */
	a.exp = 0;
    } else {
	a.expn();
    }

    a.toFloat(this);
}

/*
 * log(f)
 */
void Float::log()
{
    Flt a;

    a.fromFloat(this);
    if (a.sign != 0 || a.exp == 0) {
	/* <= 0.0 */
	Flt::edom();
    }

    a.log();
    a.toFloat(this);
}

/*
 * log10(f)
 */
void Float::log10()
{
    static Flt l102a = FLT_CONST(0, -2, 0x4000, 0x0000000L);
    static Flt l102b = FLT_CONST(1, -7, 0x77d9, 0x5ec10c0L);
    static Flt l10eb = FLT_CONST(1, -4, 0x0d21, 0x3ab646bL);
    Flt a, b, c, d, e;
    short n;

    a.fromFloat(this);
    if (a.sign != 0 || a.exp == 0) {
	/* <= 0.0 */
	Flt::edom();
    }

    n = a.exp - BIAS + 1;
    a.exp = BIAS - 1;

    if (n > 2 || n < -2) {
	if (a.cmp(&sqrth) < 0) {
	    --n;
	    a.sub(&half);
	    b = a;
	} else {
	    b = a;
	    a.sub(&half);
	    a.sub(&half);
	}
	if (b.exp != 0) {
	    --b.exp;
	}
	b.add(&half);

	a.div(&b);
	b = a;
	b.mult(&b);
	c = b;
	POLY(&c, logr);
	c.mult(&b);
	POLY1(&b, logs);
	c.div(&b);
	c.mult(&a);
    } else {
	if (a.cmp(&sqrth) < 0) {
	    --n;
	    a.exp++;
	}
	a.sub(&one);

	b = a;
	b.mult(&a);
	c = a;
	POLY(&c, logp);
	c.mult(&b);
	d = a;
	POLY1(&d, logq);
	c.div(&d);
	c.mult(&a);
	if (b.exp != 0) {
	    --b.exp;
	    c.sub(&b);
	}
    }

    b = c;
    b.mult(&l10eb);
    d = a;
    d.mult(&l10eb);
    b.add(&d);
    if (n != 0) {
	d.itof(n);
	e = d;
	e.mult(&l102b);
	b.add(&e);
    }
    if (c.exp != 0) {
	--c.exp;
	b.add(&c);
    }
    if (a.exp != 0) {
	--a.exp;
	b.add(&a);
    }
    if (n != 0) {
	d.mult(&l102a);
	b.add(&d);
    }

    b.toFloat(this);
}

/*
 * pow(f1, f2)
 */
void Float::pow(Float &f)
{
    Flt a, b, c;
    unsigned short sign;

    a.fromFloat(this);
    b.fromFloat(&f);

    c = b;
    c.trunc();
    if (b.cmp(&c) == 0 && b.exp < 0x800e) {
	/* integer power < 32768 */
	a.powi((int) c.ftoi());
	a.toFloat(this);
	return;
    }

    sign = a.sign;
    if (sign != 0) {
	if (b.cmp(&c) != 0) {
	    /* non-integer power of negative number */
	    Flt::edom();
	}
	a.sign = 0;
	--c.exp;
	c.trunc();
	if (b.cmp(&c) == 0) {
	    /* even power of negative number */
	    sign = 0;
	}
    }
    if (a.exp == 0) {
	if (b.sign != 0) {
	    /* negative power of 0.0 */
	    Flt::edom();
	}
	/* pow(0.0, y) == 0.0 */
	return;
    }

    a.log();
    a.mult(&b);
    if (a.cmp(&maxlog) > 0) {
	/* overflow */
	Flt::erange();
    }
    if (a.cmp(&minlog) < 0) {
	/* underflow */
	a.exp = 0;
    } else {
	a.expn();
    }
    a.sign = sign;

    a.toFloat(this);
}

/*
 * sqrt(f)
 */
void Float::sqrt()
{
    Flt a;

    a.fromFloat(this);
    if (a.sign != 0) {
	Flt::edom();
    }
    a.sqrt();
    a.toFloat(this);
}

static Flt sincof[] = {
    FLT_CONST(0, -33, 0x5d8f, 0xd1fd19cL),
    FLT_CONST(1, -26, 0xae5e, 0x5a9291fL),
    FLT_CONST(0, -19, 0x71de, 0x3567d48L),
    FLT_CONST(1, -13, 0xa01a, 0x019bfdfL),
    FLT_CONST(0,  -7, 0x1111, 0x11110f7L),
    FLT_CONST(1,  -3, 0x5555, 0x5555555L)
};
static Flt coscof[] = {
    FLT_CONST(1, -37, 0x8fa4, 0x9a0861aL),
    FLT_CONST(0, -29, 0x1ee9, 0xd7b4e3fL),
    FLT_CONST(1, -22, 0x27e4, 0xf7eac4bL),
    FLT_CONST(0, -16, 0xa01a, 0x019c844L),
    FLT_CONST(1, -10, 0x6c16, 0xc16c14fL),
    FLT_CONST(0,  -5, 0x5555, 0x5555555L)
};
static Flt sc1 = FLT_CONST(0,  -1, 0x921f, 0xb400000L);
static Flt sc2 = FLT_CONST(0, -25, 0x4442, 0xd000000L);
static Flt sc3 = FLT_CONST(0, -49, 0x8469, 0x898cc51L);

/*
 * cos(f)
 */
void Float::cos()
{
    Flt a, b, c;
    int n;
    unsigned short sign;

    a.fromFloat(this);
    if (a.exp >= 0x801d) {
	Flt::edom();
    }

    a.sign = sign = 0;
    b = a;
    b.div(&pio4);
    b.trunc();
    n = b.ftoi();
    if (n & 1) {
	n++;
	b.add(&one);
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
    c.mult(&sc1);
    a.sub(&c);
    c = b;
    c.mult(&sc2);
    a.sub(&c);
    b.mult(&sc3);
    a.sub(&b);

    b = a;
    b.mult(&a);
    if (n == 1 || n == 2) {
	c = b;
	b.mult(&a);
	POLY(&c, sincof);
    } else {
	a = one;
	c = b;
	if (c.exp != 0) {
	    --c.exp;
	}
	a.sub(&c);
	c = b;
	b.mult(&b);
	POLY(&c, coscof);
    }
    b.mult(&c);
    a.add(&b);
    a.sign ^= sign;

    a.toFloat(this);
}

/*
 * sin(f)
 */
void Float::sin()
{
    Flt a, b, c;
    int n;
    unsigned short sign;

    a.fromFloat(this);
    if (a.exp >= 0x801d) {
	Flt::edom();
    }

    sign = a.sign;
    a.sign = 0;
    b = a;
    b.div(&pio4);
    b.trunc();
    n = b.ftoi();
    if (n & 1) {
	n++;
	b.add(&one);
    }
    n &= 7;
    if (n > 3) {
	sign ^= 0x8000;
	n -= 4;
    }

    c = b;
    c.mult(&sc1);
    a.sub(&c);
    c = b;
    c.mult(&sc2);
    a.sub(&c);
    b.mult(&sc3);
    a.sub(&b);

    b = a;
    b.mult(&a);
    if (n == 1 || n == 2) {
	a = one;
	c = b;
	if (c.exp != 0) {
	    --c.exp;
	}
	a.sub(&c);
	c = b;
	b.mult(&b);
	POLY(&c, coscof);
    } else {
	c = b;
	b.mult(&a);
	POLY(&c, sincof);
    }
    b.mult(&c);
    a.add(&b);
    a.sign ^= sign;

    a.toFloat(this);
}

/*
 * float(f)
 */
void Float::tan()
{
    static Flt p[] = {
	FLT_CONST(1, 13, 0x992d, 0x8d24f3fL),
	FLT_CONST(0, 20, 0x199e, 0xca5fc9dL),
	FLT_CONST(1, 24, 0x11fe, 0xad32991L)
    };
    static Flt q[] = {
	FLT_CONST(0, 13, 0xab8a, 0x5eeb365L),
	FLT_CONST(1, 20, 0x427b, 0xc582abcL),
	FLT_CONST(0, 24, 0x7d98, 0xfc2ead8L),
	FLT_CONST(1, 25, 0x9afe, 0x03cbe5aL)
    };
    static Flt p1 = FLT_CONST(0,  -1, 0x921f, 0xb500000L);
    static Flt p2 = FLT_CONST(0, -27, 0x110b, 0x4600000L);
    static Flt p3 = FLT_CONST(0, -55, 0x1a62, 0x633145cL);
    Flt a, b, c;
    int n;
    unsigned short sign;

    a.fromFloat(this);
    if (a.exp >= 0x801d) {
	Flt::edom();
    }

    sign = a.sign;
    a.sign = 0;
    b = a;
    b.div(&pio4);
    b.trunc();
    n = b.ftoi();
    if (n & 1) {
	n++;
	b.add(&one);
    }

    c = b;
    c.mult(&p1);
    a.sub(&c);
    c = b;
    c.mult(&p2);
    a.sub(&c);
    b.mult(&p3);
    a.sub(&b);

    b = a;
    b.mult(&a);
    if (b.exp > 0x7fd0) {	/* ~1e-14 */
	c = b;
	POLY(&b, p);
	b.mult(&c);
	POLY1(&c, q);
	b.div(&c);
	b.mult(&a);
	a.add(&b);
    }

    if (n & 2) {
	b = one;
	b.div(&a);
	a = b;
	a.sign ^= 0x8000;
    }
    a.sign ^= sign;

    a.toFloat(this);
}

static Flt ascp[] = {
    FLT_CONST(0, -8, 0x16b9, 0xb0bd48aL),
    FLT_CONST(1, -1, 0x3434, 0x1333e5cL),
    FLT_CONST(0,  2, 0x5c74, 0xb178a2dL),
    FLT_CONST(1,  4, 0x0433, 0x1de2790L),
    FLT_CONST(0,  4, 0x3900, 0x7da7792L),
    FLT_CONST(1,  3, 0x0656, 0xc06ceafL)
};
static Flt ascq[] = {
    FLT_CONST(1,  3, 0xd7b5, 0x90b5e0eL),
    FLT_CONST(0,  6, 0x19fc, 0x025fe90L),
    FLT_CONST(1,  7, 0x265b, 0xb6d3576L),
    FLT_CONST(0,  7, 0x1705, 0x684ffbfL),
    FLT_CONST(1,  5, 0x8982, 0x20a3607L)
};

/*
 * acos(f)
 */
void Float::acos()
{
    Flt a, b, c;
    unsigned short sign;
    bool flag;

    a.fromFloat(this);
    sign = a.sign;
    a.sign = 0;
    if (a.cmp(&one) > 0) {
	Flt::edom();
    }

    if (a.cmp(&half) > 0) {
	b = half;
	b.sub(&a);
	b.add(&half);
	if (b.exp != 0) {
	    --b.exp;
	}
	a = b;
	a.sqrt();
	flag = TRUE;
    } else {
	b = a;
	b.mult(&a);
	flag = FALSE;
    }

    if (a.exp >= 0x7fe4) {	/* ~1e-8 */
	c = b;
	POLY(&c, ascp);
	c.mult(&b);
	POLY1(&b, ascq);
	c.div(&b);
	c.mult(&a);
	a.add(&c);
    }

    if (flag) {
	if (a.exp != 0) {
	    a.exp++;
	}
	if (sign != 0) {
	    b = pi;
	    b.sub(&a);
	    a = b;
	}
    } else {
	if (sign != 0) {
	    a.add(&pio2);
	} else {
	    b = pio2;
	    b.sub(&a);
	    a = b;
	}
    }

    a.toFloat(this);
}

/*
 * asin(f)
 */
void Float::asin()
{
    Flt a, b, c;
    unsigned short sign;
    bool flag;

    a.fromFloat(this);
    sign = a.sign;
    a.sign = 0;
    if (a.cmp(&one) > 0) {
	Flt::edom();
    }

    if (a.cmp(&half) > 0) {
	b = half;
	b.sub(&a);
	b.add(&half);
	if (b.exp != 0) {
	    --b.exp;
	}
	a = b;
	a.sqrt();
	flag = TRUE;
    } else {
	b = a;
	b.mult(&a);
	flag = FALSE;
    }

    if (a.exp >= 0x7fe4) {	/* ~1e-8 */
	c = b;
	POLY(&c, ascp);
	c.mult(&b);
	POLY1(&b, ascq);
	c.div(&b);
	c.mult(&a);
	a.add(&c);
    }

    if (flag) {
	if (a.exp != 0) {
	    a.exp++;
	}
	b = pio2;
	b.sub(&a);
	a = b;
    }
    a.sign ^= sign;

    a.toFloat(this);
}

static Flt atp[] = {
    FLT_CONST(1, -1, 0xc007, 0xfa1f725L),
    FLT_CONST(1,  4, 0x0285, 0x45b6b80L),
    FLT_CONST(1,  6, 0x2c08, 0xc368802L),
    FLT_CONST(1,  6, 0xeb8b, 0xf2d05baL),
    FLT_CONST(1,  6, 0x0366, 0x9fd28ecL)
};
static Flt atq[] = {
    FLT_CONST(0,  4, 0x8dbc, 0x45b1460L),
    FLT_CONST(0,  7, 0x4a0d, 0xd43b8faL),
    FLT_CONST(0,  8, 0xb0e1, 0x8d2e2beL),
    FLT_CONST(0,  8, 0xe563, 0xf13b049L),
    FLT_CONST(0,  7, 0x8519, 0xefbbd62L)
};
static Flt t3p8 = FLT_CONST(0,  1, 0x3504, 0xf333f9dL);
static Flt tp8 =  FLT_CONST(0, -2, 0xa827, 0x999fcefL);

/*
 * atan(f)
 */
void Float::atan()
{
    Flt a, b, c, d, e;
    unsigned short sign;

    a.fromFloat(this);
    sign = a.sign;
    a.sign = 0;

    if (a.cmp(&t3p8) > 0) {
	b = pio2;
	c = one;
	c.div(&a);
	a = c;
	a.sign = 0x8000;
    } else if (a.cmp(&tp8) > 0) {
	b = pio4;
	c = a;
	a.sub(&one);
	c.add(&one);
	a.div(&c);
    } else {
	b.exp = 0;
    }

    c = a;
    c.mult(&a);
    d = e = c;
    POLY(&c, atp);
    POLY1(&d, atq);
    c.div(&d);
    c.mult(&e);
    c.mult(&a);
    c.add(&b);
    a.add(&c);
    a.sign ^= sign;

    a.toFloat(this);
}

/*
 * atan2(f)
 */
void Float::atan2(Float &f)
{
    Flt a, b, c, d, e;
    unsigned short asign, bsign;

    a.fromFloat(this);
    b.fromFloat(&f);

    if (b.exp == 0) {
	if (a.exp == 0) {
	    /* atan2(0.0, 0.0); */
	    return;
	}
	a.exp = pio2.exp;
	a.high = pio2.high;
	a.low = pio2.low;
	a.toFloat(this);
	return;
    }
    if (a.exp == 0) {
	if (b.sign != 0) {
	    a = pi;
	}
	a.toFloat(this);
	return;
    }

    asign = a.sign;
    bsign = b.sign;
    a.div(&b);
    a.sign = 0;

    if (a.cmp(&t3p8) > 0) {
	b = pio2;
	c = one;
	c.div(&a);
	a = c;
	a.sign = 0x8000;
    } else if (a.cmp(&tp8) > 0) {
	b = pio4;
	c = a;
	a.sub(&one);
	c.add(&one);
	a.div(&c);
    } else {
	b.exp = 0;
    }

    c = a;
    c.mult(&a);
    d = e = c;
    POLY(&c, atp);
    POLY1(&d, atq);
    c.div(&d);
    c.mult(&e);
    c.mult(&a);
    c.add(&b);
    a.add(&c);
    a.sign ^= asign ^ bsign;

    if (bsign != 0) {
	if (asign == 0) {
	    a.add(&pi);
	} else {
	    a.sub(&pi);
	}
    }

    a.toFloat(this);
}

/*
 * cosh(f)
 */
void Float::cosh()
{
    Flt a, b;

    a.fromFloat(this);
    a.sign = 0;
    if (a.cmp(&maxlog) > 0) {
	Flt::erange();
    }

    a.expn();
    b = one;
    b.div(&a);
    a.add(&b);
    --a.exp;

    a.toFloat(this);
}

/*
 * sinh(f)
 */
void Float::sinh()
{
    static Flt p[] = {
	FLT_CONST(1, -1, 0x9435, 0xfe8bb3cL),
	FLT_CONST(1,  7, 0x4773, 0xa398ff4L),
	FLT_CONST(1, 13, 0x694b, 0x8c71d61L),
	FLT_CONST(1, 18, 0x5782, 0xbdbf6abL)
    };
    static Flt q[] = {
	FLT_CONST(1,  8, 0x15b6, 0x096e964L),
	FLT_CONST(0, 15, 0x1a7b, 0xa7ed722L),
	FLT_CONST(1, 21, 0x01a2, 0x0e4f900L)
    };
    Flt a, b, c, d;
    unsigned short sign;

    a.fromFloat(this);
    if (a.cmp(&maxlog) > 0 || a.cmp(&minlog) < 0) {
	Flt::erange();
    }

    sign = a.sign;
    a.sign = 0;

    if (a.cmp(&one) > 0) {
	a.expn();
	b = half;
	b.div(&a);
	--a.exp;
	a.sub(&b);
	a.sign ^= sign;
    } else {
	b = a;
	b.mult(&a);
	c = d = b;
	POLY(&c, p);
	POLY1(&d, q);
	c.div(&d);
	b.mult(&a);
	b.mult(&c);
	a.add(&b);
    }

    a.toFloat(this);
}

/*
 * tanh(f)
 */
void Float::tanh()
{
    static Flt p[] = {
	FLT_CONST(1, -1, 0xedc5, 0xbaafd6fL),
	FLT_CONST(1,  6, 0x8d26, 0xa0e2668L),
	FLT_CONST(1, 10, 0x93ac, 0x0305805L)
    };
    static Flt q[] = {
	FLT_CONST(0,  6, 0xc33f, 0x28a581bL),
	FLT_CONST(0, 11, 0x176f, 0xa0e5535L),
	FLT_CONST(0, 12, 0x2ec1, 0x0244204L)
    };
    static Flt mlog2 = FLT_CONST(0,  8, 0x62e4, 0x2fefa39L);
    static Flt d625 =  FLT_CONST(0, -1, 0x4000, 0x0000000L);
    static Flt two =   FLT_CONST(0,  1, 0x0000, 0x0000000L);
    Flt a, b, c, d;
    unsigned short sign;

    a.fromFloat(this);
    sign = a.sign;
    a.sign = 0;

    if (a.cmp(&mlog2) > 0) {
	a.exp = one.exp;
	a.high = one.high;
	a.low = one.low;
    } else if (a.cmp(&d625) >= 0) {
	a.exp++;
	a.expn();
	a.add(&one);
	b = two;
	b.div(&a);
	a = one;
	a.sub(&b);
    } else if (a.exp != 0) {
	b = a;
	b.mult(&a);
	c = d = b;
	POLY(&c, p);
	POLY1(&d, q);
	c.div(&d);
	b.mult(&c);
	b.mult(&a);
	a.add(&b);
    }
    a.sign = sign;

    a.toFloat(this);
}
