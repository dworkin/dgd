# define INCLUDE_CTYPE
# include "dgd.h"
# include "xfloat.h"

typedef struct {
    unsigned short sign;	/* 0: positive, 0x8000: negative */
    unsigned short exp;		/* bias: 32767 */
    unsigned short high;	/* 0 / 1 / 14 bits */
    Uint low;			/* 0 / 29 bits / 0 / 0 */
} flt;

# define NBITS		44
# define BIAS		0x7fff


/* constants */

xfloat max_int =	{ 0x41df, 0xffffffc0L };	/* 0x7fffffff */
xfloat thousand =	{ 0x408f, 0x40000000L };	/* 1e3 */
xfloat thousandth =	{ 0x3f50, 0x624dd2f2L };	/* 1e-3 */

static flt half =	{ 0x0000, 0x7ffe, 0x4000, 0x00000000L };
static flt one =	{ 0x0000, 0x7fff, 0x4000, 0x00000000L };
static flt maxlog =	{ 0x0000, 0x8008, 0x588c, 0x57baf578L };
static flt minlog =	{ 0x8000, 0x8008, 0x588c, 0x57baf578L };
static flt sqrth =	{ 0x0000, 0x7ffe, 0x5a82, 0x3cccfe78L };
static flt pi =		{ 0x0000, 0x8000, 0x6487, 0x76a8885cL };
static flt pio2 =	{ 0x0000, 0x7fff, 0x6487, 0x76a8885cL };
static flt pio4 =	{ 0x0000, 0x7ffe, 0x6487, 0x76a8885cL };


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
 * DESCRIPTION:	a = a + b.  The result is normalized, but not guaranteed to 
 *		be in range.
 */
static void f_add(a, b)
register flt *a, *b;
{
    register unsigned short h, n;
    register Uint l;
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
 * DESCRIPTION:	a = a - b.  The result is normalized, but not guaranteed to be
 *		in range.
 */
static void f_sub(a, b)
register flt *a, *b;
{
    register unsigned short h, n;
    register Uint l;
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
    register Uint m, l, albl, ambm, ahbh;
    register short al, am, ah, bl, bm, bh;

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
     * rounding off
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
static void f_div(a, b)
register flt *a, *b;
{
    unsigned short n[3];
    register Uint numh, numl, divl, high, low, q;
    register unsigned short divh, i;

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
 * NAME:	f_itof()
 * DESCRIPTION:	convert an integer to a flt
 */
static void f_itof(i, a)
Int i;
register flt *a;
{
    register Uint n;
    register unsigned short shift;

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
static Int f_ftoi(a)
register flt *a;
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
bool flt_atof(s, f)
char **s;
xfloat *f;
{
    flt a, b, c, *t;
    register unsigned short e, h;
    register char *p;

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
    *s = p;
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
    f_mult(&a, &tenmillion);
    f_37bits(&a);

    /*
     * obtain digits
     */
    f_add(&a, &half);
    i = a.exp - BIAS + 1 - 15;
    n = ((Uint) a.high << i) | (a.low >> (31 - i));
    if (n == 100000000L) {
	p = digits + 7;
	p[0] = '1';
	p[1] = '\0';
	i = 1;
	e++;
    } else {
	while (n != 0 && n % 10 == 0) {
	    n /= 10;
	}
	p = digits + 8;
	*p = '\0';
	i = 0;
	do {
	    i++;
	    *--p = '0' + n % 10;
	    n /= 10;
	} while (n != 0);
    }

    if (e >= 8 || (e < -3 && i - e > 8)) {
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
	p = digits + 8;
	do {
	    *--p = '0' + e % 10;
	    e /= 10;
	} while (e != 0);
	strcpy(buffer + i + 1, p);
    } else if (e < 0) {
	e = 1 - e;
	memcpy(buffer, "0.000000", e);
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
void flt_itof(i, f)
Int i;
xfloat *f;
{
    flt a;

    f_itof(i, &a);
    f_ftoxf(&a, f);
}

/*
 * NAME:	float->ftoi()
 * DESCRIPTION:	convert a float to an integer
 */
Int flt_ftoi(f)
xfloat *f;
{
    flt a;

    f_xftof(f, &a);
    f_round(&a);
    return f_ftoi(&a);
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
    f_xftof(f2, &b);
    f_mult(&a, &b);
    f_ftoxf(&a, f1);
}

/*
 * NAME:	float->div()
 * DESCRIPTION:	divide a float by a float
 */
void flt_div(f1, f2)
xfloat *f1, *f2;
{
    flt a, b;

    f_xftof(f2, &b);
    f_xftof(f1, &a);
    f_div(&a, &b);
    f_ftoxf(&a, f1);
}

/*
 * NAME:	float->cmp()
 * DESCRIPTION:	compare two xfloats
 */
int flt_cmp(f1, f2)
register xfloat *f1, *f2;
{
    if ((short) (f1->high ^ f2->high) < 0) {
	return ((short) f1->high < 0) ? -1 : 1;
    }

    if (f1->high == f2->high && f1->low == f2->low) {
	return 0;
    }
    if (f1->high <= f2->high && (f1->high < f2->high || f1->low < f2->low)) {
	return ((short) f1->high < 0) ? 1 : -1;
    }
    return ((short) f1->high < 0) ? -1 : 1;
}

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
    if (b.sign != 0 && f_cmp(&a, &b) != 0) {
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
    if (b.sign == 0 && f_cmp(&a, &b) != 0) {
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
    unsigned short sign;

    f_xftof(f2, &b);
    if (b.exp == 0) {
	f_edom();
    }
    f_xftof(f1, &a);
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


/*
 * The algorithms for much of the following are taken from the Cephes Math
 * Library 2.1, by Stephen L. Moshier.
 */

/*
 * NAME:	f_poly()
 * DESCRIPTION:	evaluate polynomial
 */
static void f_poly(x, coef, n)
register flt *x, *coef;
register int n;
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
static void f_poly1(x, coef, n)
register flt *x, *coef;
register int n;
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

/*
 * NAME:	f_exp()
 * DESCRIPTION:	internal version of exp(f)
 */
static void f_exp(a)
register flt *a;
{
    static flt p[] = {
	{ 0x0000, 0x7ff2, 0x4228, 0x01073370L },
	{ 0x0000, 0x7ff9, 0x7c1b, 0x4362a050L },
	{ 0x0000, 0x7fff, 0x4000, 0x00000000L }
    };
    static flt q[] = {
	{ 0x0000, 0x7fec, 0x64bd, 0x3130af58L },
	{ 0x0000, 0x7ff6, 0x52b9, 0x2c76e408L },
	{ 0x0000, 0x7ffc, 0x745c, 0x1b8352c0L },
	{ 0x0000, 0x8000, 0x4000, 0x00000000L }
    };
    static flt log2e = { 0x0000, 0x7fff, 0x5c55, 0x0eca5704L };
    static flt c1 = { 0x0000, 0x7ffe, 0x58c0, 0x00000000L };
    static flt c2 = { 0x0000, 0x7ff2, 0x6f40, 0x20b8c218L };
    flt b, c;
    register short n;

    b = *a;
    f_mult(&b, &log2e);
    f_round(&b);
    n = f_ftoi(&b);
    c = b;
    f_mult(&c, &c1);
    f_sub(a, &c);
    f_mult(&b, &c2);
    f_add(a, &b);

    b = *a;
    f_mult(&b, a);
    c = b;
    f_poly(&c, p, 2);
    f_mult(a, &c);
    f_poly(&b, q, 3);
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
 * NAME:	float->exp()
 * DESCRIPTION:	exp(f)
 */
void flt_exp(f)
xfloat *f;
{
    flt a;

    f_xftof(f, &a);
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

    f_ftoxf(&a, f);
}

static flt logp[] = {
    { 0x0000, 0x7ff0, 0x6026, 0x4ed4bf30L },
    { 0x0000, 0x7ffd, 0x7f9f, 0x5db2f2b4L },
    { 0x0000, 0x8001, 0x6902, 0x458cd8e8L },
    { 0x0000, 0x8003, 0x7726, 0x52fc7a84L },
    { 0x0000, 0x8004, 0x7939, 0x5ac9d7b8L },
    { 0x0000, 0x8004, 0x7178, 0x244a33a8L },
    { 0x0000, 0x8003, 0x4f8e, 0x4b136264L }
};
static flt logq[] = {
    { 0x0000, 0x8002, 0x7840, 0x2c1bf7a0L },
    { 0x0000, 0x8005, 0x52bd, 0x5a8f5cf4L },
    { 0x0000, 0x8006, 0x6e55, 0x0548968cL },
    { 0x0000, 0x8007, 0x4cd0, 0x22530620L },
    { 0x0000, 0x8006, 0x6b7a, 0x28551a68L },
    { 0x0000, 0x8004, 0x7755, 0x709d1394L }
};

/*
 * NAME:	float->log()
 * DESCRIPTION:	log(f)
 */
void flt_log(f)
xfloat *f;
{
    static flt r[] = {
	{ 0x8000, 0x7ffe, 0x6510, 0x7bb8d81cL },
	{ 0x0000, 0x8003, 0x418b, 0x78e604f8L },
	{ 0x8000, 0x8005, 0x4024, 0x0c224454L }
    };
    static flt s[] = {
	{ 0x8000, 0x8004, 0x4758, 0x1a87d8dcL },
	{ 0x0000, 0x8007, 0x4e06, 0x002255c8L },
	{ 0x8000, 0x8008, 0x6036, 0x12336680L }
    };
    static flt c1 = { 0x0000, 0x7ff2, 0x6f40, 0x20b8c218L };
    static flt c2 = { 0x0000, 0x7ffe, 0x58c0, 0x00000000L };
    flt a, b, c, d;
    register short n;

    f_xftof(f, &a);
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
	f_poly(&b, r, 2);
	f_mult(&b, &c);
	f_poly1(&c, s, 2);
	f_div(&b, &c);
	f_mult(&b, &a);
	f_add(&a, &b);
    } else {
	if (f_cmp(&a, &sqrth) < 0) {
	    --n;
	    a.exp++;
	}
	f_sub(&a, &one);

	b = a;
	f_mult(&b, &a);
	c = a;
	f_poly(&c, logp, 6);
	f_mult(&c, &b);
	d = a;
	f_poly1(&d, logq, 5);
	f_div(&c, &d);
	f_mult(&c, &a);
	if (b.exp != 0) {
	    --b.exp;
	}
	f_sub(&c, &b);
	f_add(&a, &c);
    }

    if (n != 0) {
	f_itof((Int) n, &b);
	c = b;
	f_mult(&c, &c1);
	f_sub(&a, &c);
	f_mult(&b, &c2);
	f_add(&a, &b);
    }

    f_ftoxf(&a, f);
}

/*
 * NAME:	float->log10()
 * DESCRIPTION:	log10(f)
 */
void flt_log10(f)
xfloat *f;
{
    static flt l102a = { 0x0000, 0x7ffd, 0x4d00, 0x00000000L };
    static flt l102b = { 0x0000, 0x7ff3, 0x4135, 0x04fbcff8L };
    static flt l10ea = { 0x0000, 0x7ffd, 0x6f00, 0x00000000L };
    static flt l10eb = { 0x0000, 0x7ff4, 0x5bd8, 0x549b9438L };
    flt a, b, c, d;
    register short n;

    f_xftof(f, &a);
    if (a.sign != 0 || a.exp == 0) {
	/* <= 0.0 */
	f_edom();
    }

    n = a.exp - BIAS + 1;
    a.exp = BIAS - 1;

    if (f_cmp(&a, &sqrth) < 0) {
	--n;
	a.exp++;
    }
    f_sub(&a, &one);

    b = a;
    f_mult(&b, &a);
    c = a;
    f_poly(&c, logp, 6);
    f_mult(&c, &b);
    d = a;
    f_poly1(&d, logq, 5);
    f_div(&c, &d);
    f_mult(&c, &a);
    if (b.exp != 0) {
	--b.exp;
    }
    f_sub(&c, &b);

    b = a;
    f_add(&b, &c);
    f_mult(&b, &l10eb);
    f_mult(&a, &l10ea);
    f_add(&a, &b);
    f_mult(&c, &l10ea);
    f_add(&a, &c);
    f_itof((Int) n, &b);
    c = b;
    f_mult(&b, &l102b);
    f_add(&a, &b);
    f_mult(&c, &l102a);
    f_add(&a, &c);

    f_ftoxf(&a, f);
}

/*
 * NAME:	f_powi()
 * DESCRIPTION:	take a number to an integer power
 */
static void f_powi(a, n)
register flt *a;
register int n;
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
    /* range of b is checked when converting back to xfloat */

    b.sign = sign;
    if (neg) {
	*a = one;
	f_div(a, &b);
    } else {
	*a = b;
    }
}

/*
 * NAME:	float->pow()
 * DESCRIPTION:	pow(f1, f2)
 */
void flt_pow(f1, f2)
xfloat *f1, *f2;
{
    static flt p[] = {
	{ 0x0000, 0x7ffd, 0x7f6e, 0x32feb6b8L },
	{ 0x0000, 0x8000, 0x7777, 0x5fd53dc0L },
	{ 0x0000, 0x8001, 0x7b32, 0x7afef1d8L },
	{ 0x0000, 0x8001, 0x4aaa, 0x076cb938L }
    };
    static flt q[] = {
	{ 0x0000, 0x8002, 0x4aaa, 0x69364124L },
	{ 0x0000, 0x8003, 0x6fff, 0x7e838394L },
	{ 0x0000, 0x8004, 0x4332, 0x78362ec8L },
	{ 0x0000, 0x8002, 0x6fff, 0x0b2315d4L }
    };
    static flt aloga[] = {
	{ 0x0000, 0x7fff, 0x4000, 0x00000000L },
	{ 0x0000, 0x7ffe, 0x7a92, 0x5f454920L },
	{ 0x0000, 0x7ffe, 0x7560, 0x31b9f748L },
	{ 0x0000, 0x7ffe, 0x7066, 0x37bb0aa4L },
	{ 0x0000, 0x7ffe, 0x6ba2, 0x3f32b5a8L },
	{ 0x0000, 0x7ffe, 0x6712, 0x230547e0L },
	{ 0x0000, 0x7ffe, 0x62b3, 0x4a845540L },
	{ 0x0000, 0x7ffe, 0x5e84, 0x28e7d604L },
	{ 0x0000, 0x7ffe, 0x5a82, 0x3cccfe78L },
	{ 0x0000, 0x7ffe, 0x56ac, 0x0fba90a8L },
	{ 0x0000, 0x7ffe, 0x52ff, 0x35aa6c54L },
	{ 0x0000, 0x7ffe, 0x4f7a, 0x4c982468L },
	{ 0x0000, 0x7ffe, 0x4c1b, 0x7c146370L },
	{ 0x0000, 0x7ffe, 0x48e1, 0x74dceac4L },
	{ 0x0000, 0x7ffe, 0x45ca, 0x7078faa4L },
	{ 0x0000, 0x7ffe, 0x42d5, 0x30d9f314L },
	{ 0x0000, 0x7ffe, 0x4000, 0x00000000L }
    };
    static flt alogb[] = {
	{ 0x0000, 0x0000, 0x0000, 0x00000000L },
	{ 0x0000, 0x7fc7, 0x4bb4, 0x05aeb670L },
	{ 0x0000, 0x7fc8, 0x5e87, 0x1a68bb98L },
	{ 0x0000, 0x7fc8, 0x5ba7, 0x62ad0c98L },
	{ 0x8000, 0x7fc8, 0x6f74, 0x682764c8L },
	{ 0x0000, 0x7fc6, 0x750e, 0x2f5fd884L },
	{ 0x0000, 0x7fc7, 0x5bd1, 0x55a46304L },
	{ 0x8000, 0x7fc7, 0x4641, 0x0373af14L },
	{ 0x0000, 0x0000, 0x0000, 0x00000000L }
    };
    static flt r[] = {
	{ 0x0000, 0x7fee, 0x7d8c, 0x0fafe528L },
	{ 0x0000, 0x7ff2, 0x50be, 0x7cc1f924L },
	{ 0x0000, 0x7ff5, 0x5761, 0x7d9095e0L },
	{ 0x0000, 0x7ff8, 0x4eca, 0x56dde268L },
	{ 0x0000, 0x7ffa, 0x71ac, 0x11ae0834L },
	{ 0x0000, 0x7ffc, 0x7afe, 0x7bff058cL },
	{ 0x0000, 0x7ffe, 0x58b9, 0x05fdf474L }
    };
    static flt log2ea = { 0x0000, 0x7ffd, 0x7154, 0x3b295c18L };
    static flt sixteenth = { 0x0000, 0x7ffb, 0x4000, 0x00000000L };
    flt a, b, c, d, e;
    register int n, i;
    unsigned short sign;

    f_xftof(f1, &a);
    f_xftof(f2, &b);

    c = b;
    f_trunc(&c);
    if (f_cmp(&b, &c) == 0 && b.exp < 0x800e) {
	/* integer power < 32768 */
	f_powi(&a, (int) f_ftoi(&c));
	f_ftoxf(&a, f1);
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

    n = a.exp - BIAS + 1;
    a.exp = BIAS - 1;

    if (f_cmp(&a, &aloga[1]) >= 0) {
	i = 0;
    } else {
	i = 1;
	if (f_cmp(&a, &aloga[9]) <= 0) {
	    i = 9;
	}
	if (f_cmp(&a, &aloga[i + 4]) <= 0) {
	    i += 4;
	}
	if (f_cmp(&a, &aloga[i + 2]) <= 0) {
	    i += 2;
	}
	i++;
    }
    f_sub(&a, &aloga[i]);
    f_sub(&a, &alogb[i >> 1]);
    f_div(&a, &aloga[i]);

    c = a;
    f_mult(&c, &a);
    d = a;
    f_poly(&d, p, 3);
    f_mult(&d, &c);
    e = a;
    f_poly1(&e, q, 3);
    f_div(&d, &e);
    f_mult(&d, &a);
    if (c.exp != 0) {
	--c.exp;
    }
    f_sub(&d, &c);

    c = d;
    f_mult(&d, &log2ea);
    f_add(&c, &d);
    d = a;
    f_mult(&d, &log2ea);
    f_add(&c, &d);
    f_add(&c, &a);
    f_mult(&c, &b);

    f_itof((Int) -i, &d);
    if (d.exp != 0) {
	d.exp -= 4;
    }
    f_itof((Int) n, &e);
    f_add(&d, &e);

    e = b;
    e.exp += 4;
    f_trunc(&e);
    if (e.exp != 0) {
	e.exp -= 4;
    }
    f_sub(&b, &e);

    f_mult(&b, &d);
    f_add(&c, &b);
    b = c;
    if (b.exp != 0) {
	b.exp += 4;
	f_trunc(&b);
	if (b.exp != 0) {
	    b.exp -= 4;
	}
    }
    f_sub(&c, &b);

    f_mult(&d, &e);
    f_add(&b, &d);
    d = b;
    if (d.exp != 0) {
	d.exp += 4;
	f_trunc(&d);
	if (d.exp != 0) {
	    d.exp -= 4;
	}
    }
    f_sub(&b, &d);

    f_add(&b, &c);
    c = b;
    if (c.exp != 0) {
	c.exp += 4;
	f_trunc(&c);
	if (c.exp != 0) {
	    c.exp -= 4;
	}
    }
    f_add(&d, &c);
    if (d.exp != 0) {
	d.exp += 4;
    }

    if (d.exp >= 0x800d) {
	/* exponent >= 16384 */
	if (d.sign == 0) {
	    /* overflow */
	    f_erange();
	}
	/* underflow */
	a.exp = 0;
	f_ftoxf(&a, f1);
	return;
    }
    n = f_ftoi(&d);
    f_sub(&b, &c);
    if (b.sign == 0 && b.exp != 0) {
	n++;
	f_sub(&b, &sixteenth);
    }

    a = b;
    f_poly(&a, r, 6);
    f_mult(&a, &b);

    i = n / 16 + ((n < 0) ? 0 : 1);
    n = i * 16 - n;
    f_mult(&a, &aloga[n]);
    f_add(&a, &aloga[n]);
    if (a.exp != 0) {
	a.exp += i;
    }
    a.sign = sign;

    f_ftoxf(&a, f1);
}

/*
 * NAME:	f_sqrt()
 * DESCRIPTION:	internal version of sqrt(f)
 */
static void f_sqrt(a)
register flt *a;
{
    static flt c1 = { 0x0000, 0x7ffe, 0x4b8a, 0x371e5fa0L };
    static flt c2 = { 0x0000, 0x7ffd, 0x6ad4, 0x55de691cL };
    static flt sqrt2 = { 0x0000, 0x7fff, 0x5a82, 0x3cccfe78L };
    flt b, c;
    register int n;

    if (a->exp == 0) {
	return;
    }

    b = *a;
    n = a->exp - BIAS + 1;
    a->exp = BIAS - 1;
    f_mult(a, &c1);
    f_add(a, &c2);
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
 * NAME:	float->sqrt()
 * DESCRIPTION:	sqrt(f)
 */
void flt_sqrt(f)
xfloat *f;
{
    flt a;

    f_xftof(f, &a);
    if (a.sign != 0) {
	f_edom();
    }
    f_sqrt(&a);
    f_ftoxf(&a, f);
}

static flt sincof[] = {
    { 0x0000, 0x7fde, 0x5763, 0x7a3fa338L },
    { 0x8000, 0x7fe5, 0x6b97, 0x4b525240L },
    { 0x0000, 0x7fec, 0x5c77, 0x46acfa90L },
    { 0x8000, 0x7ff2, 0x6806, 0x40337fc0L },
    { 0x0000, 0x7ff8, 0x4444, 0x222221f0L },
    { 0x8000, 0x7ffc, 0x5555, 0x2aaaaaacL }
};
static flt coscof[] = {
    { 0x8000, 0x7fda, 0x63e9, 0x13410c34L },
    { 0x0000, 0x7fe2, 0x47ba, 0x3af69c80L },
    { 0x8000, 0x7fe9, 0x49f9, 0x1efd5898L },
    { 0x0000, 0x7fef, 0x6806, 0x40339088L },
    { 0x8000, 0x7ff5, 0x5b05, 0x582d82a0L },
    { 0x0000, 0x7ffa, 0x5555, 0x2aaaaaacL }
};
static flt sc1 = { 0x0000, 0x7ffe, 0x6487, 0x76800000L };
static flt sc2 = { 0x0000, 0x7fe6, 0x5110, 0x5a000000L };
static flt sc3 = { 0x0000, 0x7fce, 0x611a, 0x313198a4L };

/*
 * NAME:	float->cos()
 * DESCRIPTION:	cos(f)
 */
void flt_cos(f)
xfloat *f;
{
    flt a, b, c;
    register int n;
    unsigned short sign;

    f_xftof(f, &a);
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
	f_poly(&c, sincof, 5);
    } else {
	a = one;
	c = b;
	if (c.exp != 0) {
	    --c.exp;
	}
	f_sub(&a, &c);
	c = b;
	f_mult(&b, &b);
	f_poly(&c, coscof, 5);
    }
    f_mult(&b, &c);
    f_add(&a, &b);
    a.sign ^= sign;

    f_ftoxf(&a, f);
}

/*
 * NAME:	float->sin()
 * DESCRIPTION:	sin(f)
 */
void flt_sin(f)
xfloat *f;
{
    flt a, b, c;
    register int n;
    unsigned short sign;

    f_xftof(f, &a);
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
	f_poly(&c, coscof, 5);
    } else {
	c = b;
	f_mult(&b, &a);
	f_poly(&c, sincof, 5);
    }
    f_mult(&b, &c);
    f_add(&a, &b);
    a.sign ^= sign;

    f_ftoxf(&a, f);
}

/*
 * NAME:	float->tan()
 * DESCRIPTION:	float(f)
 */
void flt_tan(f)
xfloat *f;
{
    static flt p[] = {
	{ 0x8000, 0x800c, 0x664b, 0x31a49e80L },
	{ 0x0000, 0x8013, 0x4667, 0x594bf93cL },
	{ 0x8000, 0x8017, 0x447f, 0x55a65324L }
    };
    static flt q[] = {
	{ 0x0000, 0x800c, 0x6ae2, 0x4bdd66ccL },
	{ 0x8000, 0x8013, 0x509e, 0x78b05578L },
	{ 0x0000, 0x8017, 0x5f66, 0x1f85d5b0L },
	{ 0x8000, 0x8018, 0x66bf, 0x40797cb4L }
    };
    static flt p1 = { 0x0000, 0x7ffe, 0x6487, 0x76800000L };
    static flt p2 = { 0x0000, 0x7fe6, 0x5110, 0x5a000000L };
    static flt p3 = { 0x0000, 0x7fce, 0x611a, 0x313198a4L };
    flt a, b, c;
    register int n;
    unsigned short sign;

    f_xftof(f, &a);
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
	f_poly(&b, p, 2);
	f_mult(&b, &c);
	f_poly1(&c, q, 3);
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

    f_ftoxf(&a, f);
}

static flt ascp[] = {
    { 0x8000, 0x7ffe, 0x5931, 0x3dd1792cL },
    { 0x0000, 0x8002, 0x5147, 0x2a1c6244L },
    { 0x8000, 0x8004, 0x4f77, 0x6c7ab96cL },
    { 0x0000, 0x8004, 0x7295, 0x0d081500L },
    { 0x8000, 0x8003, 0x6da8, 0x634b0bb0L }
};
static flt ascq[] = {
    { 0x8000, 0x8003, 0x5f58, 0x7442cc70L },
    { 0x0000, 0x8006, 0x4b8c, 0x15abd1acL },
    { 0x8000, 0x8007, 0x5f95, 0x630cc2e0L },
    { 0x0000, 0x8007, 0x6871, 0x0dbab9b8L },
    { 0x8000, 0x8006, 0x523e, 0x4a7848c4L }
};

/*
 * NAME:	float->acos()
 * DESCRIPTION:	acos(f)
 */
void flt_acos(f)
xfloat *f;
{
    flt a, b, c;
    unsigned short sign;
    bool flag;

    f_xftof(f, &a);
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

    if (a.exp >= 0x7fe7) {	/* ~1e-7 */
	c = b;
	f_poly(&c, ascp, 4);
	f_mult(&c, &b);
	f_poly1(&b, ascq, 4);
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

    f_ftoxf(&a, f);
}

/*
 * NAME:	float->asin()
 * DESCRIPTION:	asin(f)
 */
void flt_asin(f)
xfloat *f;
{
    flt a, b, c;
    unsigned short sign;
    bool flag;

    f_xftof(f, &a);
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

    if (a.exp >= 0x7fe7) {	/* ~1e-7 */
	c = b;
	f_poly(&c, ascp, 4);
	f_mult(&c, &b);
	f_poly1(&b, ascq, 4);
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

    f_ftoxf(&a, f);
}

static flt atp[] = {
    { 0x8000, 0x7ffe, 0x6ba5, 0x2175f64cL },
    { 0x8000, 0x8002, 0x46b5, 0x3c27114cL },
    { 0x8000, 0x8003, 0x5763, 0x7b6b8ba4L },
    { 0x8000, 0x8002, 0x76a5, 0x2457275cL }
};
static flt atq[] = {
    { 0x0000, 0x8002, 0x7bfa, 0x59b1a2acL },
    { 0x0000, 0x8004, 0x7d94, 0x68676274L },
    { 0x0000, 0x8005, 0x5c3c, 0x7b2444acL },
    { 0x0000, 0x8004, 0x58fb, 0x7b415d88L }
};
static flt t3p8 = { 0x0000, 0x8000, 0x4d41, 0x1e667f3cL };
static flt tp8 = { 0x0000, 0x7ffd, 0x6a09, 0x7333f9e0L };

/*
 * NAME:	float->atan()
 * DESCRIPTION:	atan(f)
 */
void flt_atan(f)
xfloat *f;
{
    flt a, b, c, d, e;
    unsigned short sign;

    f_xftof(f, &a);
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
    f_poly(&c, atp, 3);
    f_poly1(&d, atq, 3);
    f_div(&c, &d);
    f_mult(&c, &e);
    f_mult(&c, &a);
    f_add(&c, &b);
    f_add(&a, &c);
    a.sign ^= sign;

    f_ftoxf(&a, f);
}

/*
 * NAME:	float->atan2()
 * DESCRIPTION:	atan2(f)
 */
void flt_atan2(f1, f2)
xfloat *f1, *f2;
{
    flt a, b, c, d, e;
    unsigned short asign, bsign;

    f_xftof(f1, &a);
    f_xftof(f2, &b);

    if (b.exp == 0) {
	if (a.exp == 0) {
	    /* atan2(0.0, 0.0); */
	    return;
	}
	a.exp = pio2.exp;
	a.high = pio2.high;
	a.low = pio2.low;
	f_ftoxf(&a, f1);
	return;
    }
    if (a.exp == 0) {
	if (b.sign != 0) {
	    a = pi;
	}
	f_ftoxf(&a, f1);
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
    f_poly(&c, atp, 3);
    f_poly1(&d, atq, 3);
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

    f_ftoxf(&a, f1);
}

/*
 * NAME:	float->cosh()
 * DESCRIPTION:	cosh(f)
 */
void flt_cosh(f)
xfloat *f;
{
    flt a, b;

    f_xftof(f, &a);
    a.sign = 0;
    if (f_cmp(&a, &maxlog) > 0) {
	f_erange();
    }

    f_exp(&a);
    b = one;
    f_div(&b, &a);
    f_add(&a, &b);
    --a.exp;

    f_ftoxf(&a, f);
}

/*
 * NAME:	float->sinh()
 * DESCRIPTION:	sinh(f)
 */
void flt_sinh(f)
xfloat *f;
{
    static flt p[] = {
	{ 0x8000, 0x7ffe, 0x650d, 0x3fd17678L },
	{ 0x8000, 0x8006, 0x51dc, 0x74731fe8L },
	{ 0x8000, 0x800c, 0x5a52, 0x718e3ac4L },
	{ 0x8000, 0x8011, 0x55e0, 0x57b7ed58L }
    };
    static flt q[] = {
	{ 0x8000, 0x8007, 0x456d, 0x412dd2c8L },
	{ 0x0000, 0x800e, 0x469e, 0x74fdae44L },
	{ 0x8000, 0x8014, 0x4068, 0x41c9f200L }
    };
    flt a, b, c, d;
    unsigned short sign;

    f_xftof(f, &a);
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
	f_poly(&c, p, 3);
	f_poly1(&d, q, 2);
	f_div(&c, &d);
	f_mult(&b, &a);
	f_mult(&b, &c);
	f_add(&a, &b);
    }

    f_ftoxf(&a, f);
}

/*
 * NAME:	float->tanh()
 * DESCRIPTION:	tanh(f)
 */
void flt_tanh(f)
xfloat *f;
{
    static flt p[] = {
	{ 0x8000, 0x7ffe, 0x7b71, 0x3755fae0L },
	{ 0x8000, 0x8005, 0x6349, 0x541c4cd0L },
	{ 0x8000, 0x8009, 0x64eb, 0x0060b00cL }
    };
    static flt q[] = {
	{ 0x0000, 0x8005, 0x70cf, 0x6514b038L },
	{ 0x0000, 0x800a, 0x45db, 0x741caa6cL },
	{ 0x0000, 0x800b, 0x4bb0, 0x20488408L }
    };
    static flt mlog2 = { 0x0000, 0x8007, 0x588c, 0x57baf578L };
    static flt d625 = { 0x0000, 0x7ffe, 0x5000, 0x00000000L };
    static flt two = { 0x0000, 0x8000, 0x4000, 0x00000000L };
    flt a, b, c, d;
    unsigned short sign;

    f_xftof(f, &a);
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
    } else {
	b = a;
	f_mult(&b, &a);
	c = d = b;
	f_poly(&c, p, 2);
	f_poly1(&d, q, 2);
	f_div(&c, &d);
	f_mult(&b, &c);
	f_mult(&b, &a);
	f_add(&a, &b);
    }
    a.sign = sign;

    f_ftoxf(&a, f);
}
