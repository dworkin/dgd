# include "dgd.h"
# include <ctype.h>
# include "xfloat.h"

typedef struct {
    unsigned short sign;	/* 0: positive, 0x8000: negative */
    unsigned short exp;		/* bias: 32767 */
    unsigned short high;	/* 0 / 1 / 14 bits */
    Uint low;			/* 0 / 29 bits / 0 / 0 */
} flt;

# define NBITS		44
# define BIAS		0x7fff

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

static void f_sub P((flt*, flt*));

/*
 * NAME:	f_add()
 * DESCRIPTION:	a = a + b.  b may not be 0.  The result is normalized, but not
 *		guaranteed to be in range.
 */
static void f_add(a, b)
register flt *a, *b;
{
    register unsigned short h, n;
    register Uint l;
    flt tmp;

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
	    l += (((Uint) b->high << (31 - n)) & 0x7fffffffL) |
		 (b->low >> n);
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
	 * rounding off
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
 * DESCRIPTION:	a = a - b.  b may not be 0.  The result is normalized, but not
 *		guaranteed to be in range.
 */
static void f_sub(a, b)
register flt *a, *b;
{
    register unsigned short h, n;
    register Uint l;
    flt tmp;

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
	    l -= (((Uint) b->high << (31 - n)) & 0x7fffffffL) |
		 (b->low >> n);
	    if ((b->low >> n) << n != b->low) {
		--l;
	    }
	} else {
	    n -= 31;
	    l -= b->high >> n;
	    if (b->low != 0 || (b->high >> n) << n != b->high) {
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
	n = 0;
	if (h == 0) {
	    if (l == 0) {
		a->exp = 0;
		return;
	    }
	    if ((l & 0xffff0000L) == 0) {
		l <<= 15;
		n += 15;
	    }
	    h = l >> 16;
	    l <<= 15;
	    a->exp -= n + 15;
	    n = 0;
	}
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

	/*
	 * rounding off
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
static void f_mult(a, b)
register flt *a, *b;
{
    register Uint m;
    register unsigned short al, am, ah, bl, bm, bh;

    if (a->exp == 0) {
	return;
    }

    al = ((unsigned short) a->low) >> 1;
    bl = ((unsigned short) b->low) >> 1;
    am = (a->low >> 16) & 0x7fff;
    bm = (b->low >> 16) & 0x7fff;
    ah = a->high;
    bh = b->high;

    m = (Uint) al * bl;
    m >>= 15;
    m += (Uint) al * bm + (Uint) am * bl;
    m >>= 15;
    m += (Uint) al * bh + (Uint) am * bm + (Uint) ah * bl + 0x4000;
    m >>= 15;
    m += (Uint) am * bh + (Uint) ah * bm;
    a->low = (m << 2) & 0x1ffffL;
    m >>= 15;
    m += (Uint) ah * bh;
    a->low |= m << 17;
    a->high = m >> 14;

    a->sign ^= b->sign;
    a->exp += b->exp - BIAS;
    if ((short) a->high < 0) {
	a->high >>= 1;
	a->low >>= 1;
	a->exp++;
    }
    a->low &= 0x7ffffffcL;
}

/*
 * NAME:	f_div()
 * DESCRIPTION:	c = a / b.  b must be non-zero.  The result is normalized,
 *		but may be out of range.
 */
static void f_div(a, b)
register flt *a, *b;
{
    unsigned short n[3];
    register Uint numh, numl, divl, high, low, q;
    register unsigned short divh, i;

    if (a->exp == 0) {
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
	/* highlow = num * q */
	low = (unsigned short) (high = q * (unsigned short) divl);
	high >>= 16;
	high += q * (divl >> 16);
	low |= high << 16;
	high >>= 16;
	high += q * divh;

	/* the estimated quotient may be 2 off; correct it if needed */
	while (high >= numh && (high > numh || low > numl)) {
	    high -= divh;
	    if (low < divl) {
		--high;
	    }
	    low -= divl;
	    --q;
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
     * rounding off
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
static void f_trunc(a)
register flt *a;
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
    } else if (a->exp < BIAS + NBITS) {
	a->high &= maskh[a->exp - BIAS];
	a->low &= maskl[a->exp - BIAS];
    }
}

/*
 * NAME:	f_37bits()
 * DESCRIPTION:	round a flt to 37 binary digits of precision
 */
static void f_37bits(a)
register flt *a;
{
    if ((Int) (a->low += 0x100) < 0) {
	a->low = 0;
	if ((short) ++(a->high) < 0) {
	    a->high >>= 1;
	    a->exp++;
	}
    } else {
	a->low &= 0xfffffe00L;
    }
}

/*
 * NAME:	f_round()
 * DESCRIPTION:	round off a flt
 */
static void f_round(a)
register flt *a;
{
    static flt half = { 0, 0x7ffe, 0x4000, 0x00000000L };

    half.sign = a->sign;
    f_add(a, &half);
    f_trunc(a);
}

/*
 * NAME:	f_frexp()
 * DESCRIPTION:	split a flt into a fraction and an exponent
 */
static short f_frexp(a)
register flt *a;
{
    short e;

    if (a->exp == 0) {
	return 0;
    }
    e = a->exp - BIAS + 1;
    a->exp = BIAS - 1;
    return e;
}

/*
 * NAME:	f_ldexp()
 * DESCRIPTION:	add an integer value to the exponent of a flt
 */
static void f_ldexp(a, exp)
register flt *a;
register short exp;
{
    if (a->exp == 0) {
	return;
    }
    exp += a->exp;
    if (exp <= 0) {
	a->exp = 0;
	return;
    }
    a->exp = exp;
}

/*
 * NAME:	f_cmp()
 * DESCRIPTION:	compate two flts
 */
static int f_cmp(a, b)
register flt *a, *b;
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
 * NAME:	f_ftoxf()
 * DESCRIPTION:	convert flt to xfloat
 */
static void f_ftoxf(a, f)
register flt *a;
register xfloat *f;
{
    register unsigned short exp;
    register unsigned short high;
    register Uint low;

    exp = a->exp;
    if (exp == 0) {
	/* zero */
	f->high = 0;
	f->low = 0;
	return;
    }
    high = a->high;
    low = a->low;

    /* mantissa */
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
 * DESCRIPTION:	convert xfloat to flt
 */
static void f_xftof(f, a)
register xfloat *f;
register flt *a;
{
    register unsigned short exp;

    exp = (f->high >> 4) & 0x07ff;
    if (exp == 0) {
	/* zero */
	a->exp = 0;
	return;
    }
    a->exp = exp + BIAS - 1023;
    a->sign = f->high & 0x8000;
    a->high = 0x4000 | ((f->high & 0x0f) << 10) | (f->low >> 22);
    a->low = (f->low << 9) & 0x7fffffffL;
}


static flt tens[] = {
    { 0, 0x8002, 0x5000, 0x00000000L },	/* 10 ** 1 */
    { 0, 0x8005, 0x6400, 0x00000000L },	/* 10 ** 2 */
    { 0, 0x800C, 0x4E20, 0x00000000L },	/* 10 ** 4 */
    { 0, 0x8019, 0x5F5E, 0x08000000L },	/* 10 ** 8 */
    { 0, 0x8034, 0x470D, 0x726FC100L },	/* 10 ** 16 */
    { 0, 0x8069, 0x4EE2, 0x6B6A0ADCL },	/* 10 ** 32 */
    { 0, 0x80D3, 0x613C, 0x07D27FF4L },	/* 10 ** 64 */
    { 0, 0x81A8, 0x49DD, 0x11F2603CL },	/* 10 ** 128 */
    { 0, 0x8351, 0x553F, 0x3AFEE780L },	/* 10 ** 256 */
    { 0, 0x86A3, 0x718C, 0x682BA984L },	/* 10 ** 512 */
};

static flt tenths[] = {
    { 0, 0x7FFB, 0x6666, 0x33333334L },	/* 10 ** -1 */
    { 0, 0x7FF8, 0x51EB, 0x428F5C28L },	/* 10 ** -2 */
    { 0, 0x7FF1, 0x68DB, 0x45D63888L },	/* 10 ** -4 */
    { 0, 0x7FE4, 0x55E6, 0x1DC46118L },	/* 10 ** -8 */
    { 0, 0x7FC9, 0x734A, 0x652FB114L },	/* 10 ** -16 */
    { 0, 0x7F94, 0x67D8, 0x47AB5150L },	/* 10 ** -32 */
    { 0, 0x7F2A, 0x543F, 0x7A89E950L },	/* 10 ** -64 */
    { 0, 0x7E55, 0x6EE8, 0x119F1930L },	/* 10 ** -128 */
    { 0, 0x7CAC, 0x6018, 0x50C958E0L },	/* 10 ** -256 */
    { 0, 0x795A, 0x4824, 0x7B8CB6C8L },	/* 10 ** -512 */
};

/*
 * NAME:	float->atof()
 * DESCRIPTION:	Convert a string to a float.  The string must be in the
 *		proper format.  Return TRUE if the operation was successful,
 *		FALSE otherwise.
 */
bool flt_atof(p, f)
register char *p;
xfloat *f;
{
    flt a, b, c, *t;
    register unsigned short e, h;

    /* sign */
    if (*p == '-') {
	a.sign = b.sign = 0x8000;
	p++;
    } else {
	a.sign = b.sign = 0;
    }

    a.exp = 0;
    b.low = 0;

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
	}
    }

    /* exponent */
    if (*p == 'e' || *p == 'E') {
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
    }
    if (a.exp >= BIAS + 1023 &&
	(a.exp > BIAS + 1023 || (a.high == 0x7fff && a.low >= 0x7fffff00L))) {
	return FALSE;
    }

    f_ftoxf(&a, f);
    return TRUE;
}

/*
 * NAME:	float->ftoa()
 * DESCRIPTION:	convert a float to a string
 */
void flt_ftoa(f, buffer)
xfloat *f;
char *buffer;
{
    static flt tenmillion =	{ 0, 0x8016, 0x4c4b, 0x20000000L };
    static flt half =		{ 0, 0x7ffe, 0x4000, 0x00000000L };
    register unsigned short i;
    register short e;
    register Uint n;
    register char *p;
    register flt *t, *t2;
    char digits[9];
    flt a;

    f_xftof(f, &a);
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
	    if (a.exp >= t->exp &&
		(a.exp > t->exp || (a.high >= t->high &&
				    (a.high > t->high || a.low >= t->low)))) {
		e |= 1;
		f_mult(&a, t2);
	    }
	}
    } else {
	/* < 1 */
	for (i = 10, t = &tenths[9], t2 = &tens[9]; i > 0; --i, --t, --t2) {
	    e <<= 1;
	    if (a.exp <= t->exp &&
		(a.exp < t->exp || (a.high <= t->high &&
				    (a.high < t->high || a.low <= t->low)))) {
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
    f_mult(&a, &tenmillion);
    f_37bits(&a);

    /*
     * obtain digits
     */
    f_add(&a, &half);
    i = a.exp - BIAS + 1 - 15;
    n = ((Uint) a.high << i) | (a.low >> (31 - i));
    if (n == 100000000L) {
	p = digits;
	p[0] = '1';
	p[1] = '\0';
	i = 1;
	e++;
    } else {
	i = 8;
	while (n % 10 == 0) {
	    n /= 10;
	    --i;
	}
	p = digits + i;
	*p = '\0';
	do {
	    *--p = n % 10 + '0';
	    n /= 10;
	} while (n != 0);
    }

    if (e >= 8 || (e < -3 && i - e > 8)) {
	if (i == 1) {
	    sprintf(buffer, "%ce%s%d", digits[0], "+" + (e < 0), e);
	} else {
	    sprintf(buffer, "%c.%se%s%d", digits[0], digits + 1,
		    "+" + (e < 0), e);
	}
    } else if (e < 0) {
	sprintf(buffer, "0.%s%s", "000000" + 7 + e, digits);
    } else {
	while (e >= 0) {
	    *buffer++ = (*p == '\0') ? '0' : *p++;
	    --e;
	}
	if (*p != '\0') {
	    sprintf(buffer, ".%s", p);
	} else {
	    *buffer = '\0';
	}
    }
}

/*
 * NAME:	float->itof()
 * DESCRIPTION:	convert an integer to a float
 */
void flt_itof(i, f)
Int i;
register xfloat *f;
{
    register Uint n;
    register unsigned short shift;

    /* deal with zero and sign */
    if (i == 0) {
	f->high = 0;
	f->low = 0;
	return;
    } else if (i < 0) {
	f->high = 0x8000;
	n = -i;
    } else {
	f->high = 0;
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
    f->high |= ((1023 + 31 - shift) << 4) | ((n >> 27) & 0x000f);
    f->low = n << 5;
}

/*
 * NAME:	float->ftoi()
 * DESCRIPTION:	convert a float to an integer
 */
Int flt_ftoi(f)
xfloat *f;
{
    flt a;
    Uint i;

    f_xftof(f, &a);
    f_round(&a);
    if (a.exp == 0) {
	return 0;
    }
    if (a.exp > BIAS + 30 &&
	(a.sign == 0 || a.exp != BIAS + 31 || a.high != 0x4000 || a.low != 0)) {
	f_erange();
    }

    i = (((Uint) a.high << 17) | (a.low >> 14)) >> (BIAS + 31 - a.exp);

    return (a.sign == 0) ? i : -i;
}

/*
 * NAME:	float->isint()
 * DESCRIPTION:	check if a float holds an integer value
 */
bool flt_isint(f)
xfloat *f;
{
    flt a, b;

    f_xftof(f, &a);
    if (a.exp > BIAS + 30 &&
	(a.sign == 0 || a.exp != BIAS + 31 || a.high != 0x4000 || a.low != 0)) {
	return FALSE;
    }
    b = a;
    f_trunc(&b);
    return (a.high == b.high && a.low == b.low);
}

/*
 * NAME:	float->add()
 * DESCRIPTION:	add two floats
 */
void flt_add(f1, f2)
xfloat *f1, *f2;
{
    flt a, b;

    f_xftof(f2, &b);
    if (b.exp == 0) {
	return;
    }
    f_xftof(f1, &a);
    f_add(&a, &b);
    f_ftoxf(&a, f1);
}

/*
 * NAME:	float->sub()
 * DESCRIPTION:	subtract a float from a float
 */
void flt_sub(f1, f2)
xfloat *f1, *f2;
{
    flt a, b;

    f_xftof(f2, &b);
    if (b.exp == 0) {
	return;
    }
    f_xftof(f1, &a);
    f_sub(&a, &b);
    f_ftoxf(&a, f1);
}

/*
 * NAME:	float->mult()
 * DESCRIPTION:	multiply two floats
 */
void flt_mult(f1, f2)
xfloat *f1, *f2;
{
    flt a, b;

    f_xftof(f1, &a);
    if (a.exp == 0) {
	return;
    }
    f_xftof(f2, &b);
    if (b.exp == 0) {
	*f1 = *f2;
	return;
    }

    f_mult(&a, &b);
    f_ftoxf(&a, f1);
}

/*
 * NAME:	float->div()
 * DESCRIPTION:	devide a float by a float
 */
void flt_div(f1, f2)
xfloat *f1, *f2;
{
    flt a, b;

    f_xftof(f2, &b);
    if (b.exp == 0) {
	f_edom();
    }
    f_xftof(f1, &a);
    if (a.exp == 0) {
	return;
    }

    f_div(&a, &b);
    f_ftoxf(&a, f1);
}

/*
 * NAME:	float->cmp()
 * DESCRIPTION:	compare two xfloats
 */
int flt_cmp(f1, f2)
xfloat *f1, *f2;
{
    flt a, b;

    f_xftof(f1, &a);
    f_xftof(f2, &b);
    return f_cmp(&a, &b);
}

static flt one = { 0, 0x7fff, 0x4000, 0x00000000L };

/*
 * NAME:	float->floor()
 * DESCRIPTION:	round a float downwards
 */
void flt_floor(f)
xfloat *f;
{
    flt a, b;

    f_xftof(f, &a);
    b = a;
    f_trunc(&b);
    if (b.sign != 0 && (a.exp != b.exp || a.high != b.high || a.low != b.low)) {
	f_sub(&b, &one);
    }
    f_ftoxf(&b, f);
}

/*
 * NAME:	float->ceil()
 * DESCRIPTION:	round a float upwards
 */
void flt_ceil(f)
xfloat *f;
{
    flt a, b;

    f_xftof(f, &a);
    b = a;
    f_trunc(&b);
    if (b.sign == 0 && (a.exp != b.exp || a.high != b.high || a.low != b.low)) {
	f_add(&b, &one);
    }
    f_ftoxf(&b, f);
}

/*
 * NAME:	float->fmod()
 * DESCRIPTION:	perform fmod
 */
void flt_fmod(f1, f2)
xfloat *f1, *f2;
{
    flt a, b, c;

    f_xftof(f2, &b);
    if (b.exp == 0) {
	f_edom();
    }
    f_xftof(f1, &a);
    if (a.exp == 0) {
	return;
    }

    c.sign = a.sign;
    c.high = b.high;
    c.low = b.low;
    while (a.exp >= b.exp &&
	   (a.exp > b.exp ||
	    (a.high >= b.high && (a.high > b.high || a.low >= b.low)))) {
	c.exp = a.exp;
	if (a.high <= c.high && (a.high < c.high || a.low < c.low)) {
	    c.exp--;
	}
	f_sub(&a, &c);
    }

    f_ftoxf(&a, f1);
}

/*
 * NAME:	float->frexp()
 * DESCRIPTION:	split a float into a fraction and an exponent
 */
Int flt_frexp(f)
register xfloat *f;
{
    short e;

    if (f->high == 0) {
	return 0;
    }
    e = ((f->high & 0x7ff0) >> 4) - 1022;
    f->high = (f->high & 0x800f) | (1022 << 4);
    return e;
}

/*
 * NAME:	float->ldexp()
 * DESCRIPTION:	make a float from a fraction and an exponent
 */
void flt_ldexp(f, exp)
register xfloat *f;
register Int exp;
{
    if (f->high == 0) {
	return;
    }
    exp += (f->high & 0x7ff0) >> 4;
    if (exp <= 0) {
	f->high = 0;
	f->low = 0;
	return;
    }
    if (exp > 1023 + 1023) {
	f_erange();
    }
    f->high = (f->high & 0x800f) | (exp << 4);
}

/*
 * NAME:	float->modf()
 * DESCRIPTION:	split float into fraction and integer part
 */
void flt_modf(f1, f2)
xfloat *f1, *f2;
{
    flt a, b;

    f_xftof(f1, &a);
    if (a.exp < BIAS) {
	b.exp = 0;
    } else {
	b = a;
	f_trunc(&b);
	f_sub(&a, &b);
    }
    f_ftoxf(&a, f1);
    f_ftoxf(&b, f2);
}
