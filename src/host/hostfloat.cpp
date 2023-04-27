/*
 * This file is part of DGD, https://github.com/dworkin/dgd
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
# include "ext.h"
# include <float.h>
# include <math.h>


/* constants */

# ifdef LARGENUM
Float max_int =		{ 0x403dffffL, 0xfffffffff0000000LL };
Float thousand =	{ 0x4008f400L, 0x0000000000000000LL };
# else
Float max_int =		{ 0x41df, 0xffffffc0L };	/* 0x7fffffff */
Float thousand =	{ 0x408f, 0x40000000L };	/* 1e3 */
# endif


/*
 * Domain error
 */
static void f_edom()
{
    EC->error("Math argument");
}

/*
 * Out of range
 */
static void f_erange()
{
    EC->error("Result too large");
}

/*
 * retrieve a float from a value
 */
static double f_get(const Float *flt)
{
    return Ext::getFloat(flt);
}

/*
 * store a float in a value
 */
static void f_put(Float *flt, double d)
{
    Ext::constrainFloat(&d);
    Ext::putFloat(flt, d);
}

static const double tens[] = {
    1e+1L,
    1e+2L,
    1e+4L,
    1e+8L,
    1e+16L,
    1e+32L,
    1e+64L,
    1e+128L,
    1e+256L
};

static const double tenths[] = {
    1e-1L,
    1e-2L,
    1e-4L,
    1e-8L,
    1e-16L,
    1e-32L,
    1e-64L,
    1e-128L,
    1e-256L
};

/*
 * Convert a string to a float.  The string must be in the
 * proper format.  Return TRUE if the operation was successful,
 * FALSE otherwise.
 */
bool Float::atof(char **s, Float *f)
{
    double a, b;
    const double *t;
    unsigned short e;
    char *p, *q;
    bool negative, digits;

    p = *s;

    /* sign */
    if (*p == '-') {
	negative = TRUE;
	p++;
    } else {
	negative = FALSE;
    }

    a = 0.0;
    digits = FALSE;

    /* digits before . */
    while (isdigit(*p)) {
	a = a * tens[0] + (*p++ - '0');
	if (!isfinite(a)) {
	    return FALSE;
	}
	digits = TRUE;
    }

    /* digits after . */
    if (*p == '.') {
	b = tenths[0];
	while (isdigit(*++p)) {
	    a += b * (*p - '0');
	    b *= tenths[0];
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
		    a *= *t;
		}
		e >>= 1;
		t++;
	    }
	} else {
	    /* roll back before exponent */
	    p = q;
	}
    }

    /* sign */
    if (negative) {
	a = -a;
    }

    if (!Ext::checkFloat(&a)) {
	return FALSE;
    }
    Ext::putFloat(f, a);
    *s = p;
    return TRUE;
}

/*
 * convert a float to a string
 */
void Float::ftoa(char *buffer)
{
    short i, e;
    FloatLow n;
    char *p;
    char digits[FLOAT_DIGITS + 1];
    double a;

    a = f_get(this);
    if (a == 0.0) {
	strcpy(buffer, "0");
	return;
    }

    if (a < 0.0) {
	*buffer++ = '-';
	a = -a;
    }

    /* reduce the float to range 1 .. 9.999999999, and extract exponent */
    e = 0;
    if (a >= 1.0) {
	for (i = sizeof(tens) / sizeof(double) - 1; i >= 0; --i) {
	    e <<= 1;
	    if (a >= tens[i]) {
		e |= 1;
		a *= tenths[i];
	    }
	}
    } else {
	for (i = sizeof(tens) / sizeof(double) - 1; i >= 0; --i) {
	    e <<= 1;
	    if (a <= tenths[i]) {
		e |= 1;
		a *= tens[i];
	    }
	}
	if (a < 1.0) {
	    a *= tens[0];
	    e++;
	}
	e = -e;
    }
# ifdef LARGENUM
    a *= 1e13;
# else
    a *= tens[3];
# endif

    /*
     * obtain digits
     */
    a += 0.5;
    n = (FloatLow) a;
    if (n == FLOAT_LIMIT) {
	p = digits + FLOAT_DIGITS - 1;
	p[0] = '1';
	p[1] = '\0';
	i = 1;
	e++;
    } else {
	while (n != 0 && n % 10 == 0) {
	    n /= 10;
	}
	p = digits + FLOAT_DIGITS;
	*p = '\0';
	i = 0;
	do {
	    i++;
	    *--p = '0' + n % 10;
	    n /= 10;
	} while (n != 0);
    }

    if (e >= FLOAT_DIGITS || (e < -3 && i - e > FLOAT_DIGITS)) {
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
	p = digits + FLOAT_DIGITS;
	do {
	    *--p = '0' + e % 10;
	    e /= 10;
	} while (e != 0);
	strcpy(buffer + i + 1, p);
    } else if (e < 0) {
	e = 1 - e;
	memcpy(buffer, "0.000000000000", e);
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
    f_put(f, (double) i);
}

/*
 * convert a float to an integer
 */
LPCint Float::ftoi()
{
    double a;

    a = f_get(this);
    if (a >= 0) {
	a = ::floor(a + 0.5);
	if (a > (double) (LPCint) LPCINT_MAX) {
	    f_erange();
	}
    } else {
	a = ::ceil(a - 0.5);
	if (a < (double) (LPCint) LPCINT_MIN) {
	    f_erange();
	}
    }
    return (LPCint) a;
}

/*
 * add a Float
 */
void Float::add(Float &f)
{
    f_put(this, f_get(this) + f_get(&f));
}

/*
 * subtract a Float
 */
void Float::sub(Float &f)
{
    f_put(this, f_get(this) - f_get(&f));
}

/*
 * multiply by a Float
 */
void Float::mult(Float &f)
{
    f_put(this, f_get(this) * f_get(&f));
}

/*
 * divide by a Float
 */
void Float::div(Float &f)
{
    double a;

    a = f_get(&f);
    if (a == 0.0) {
	EC->error("Division by zero");
    }
    f_put(this, f_get(this) / a);
}

/*
 * compare with a Float
 */
int Float::cmp(Float &f)
{
    if ((high ^ f.high) & FLOAT_SIGN) {
	return (high & FLOAT_SIGN) ? -1 : 1;
    }

    if (high == f.high && low == f.low) {
	return 0;
    }
    if (high <= f.high && (high < f.high || low < f.low)) {
	return (high & FLOAT_SIGN) ? 1 : -1;
    }
    return (high & FLOAT_SIGN) ? -1 : 1;
}

/*
 * round a float downwards
 */
void Float::floor()
{
    f_put(this, ::floor(f_get(this)));
}

/*
 * round a float upwards
 */
void Float::ceil()
{
    f_put(this, ::ceil(f_get(this)));
}

/*
 * perform fmod
 */
void Float::fmod(Float &f)
{
    double a;

    a = f_get(&f);
    if (a == 0.0) {
	f_edom();
    }
    f_put(this, ::fmod(f_get(this), a));
}

/*
 * split a float into a fraction and an exponent
 */
LPCint Float::frexp()
{
    int e;

    f_put(this, ::frexp(f_get(this), &e));
    return e;
}

/*
 * make a float from a fraction and an exponent
 */
void Float::ldexp(LPCint exp)
{
    f_put(this, ::ldexp(f_get(this), exp));
}

/*
 * split float into fraction and integer part
 */
void Float::modf(Float *f)
{
    double a;

    f_put(this, ::modf(f_get(this), &a));
    f_put(f, a);
}


/*
 * exp(f)
 */
void Float::exp()
{
    f_put(this, ::exp(f_get(this)));
}

/*
 * log(f)
 */
void Float::log()
{
    double a;

    a = f_get(this);
    if (a <= 0.0) {
	f_edom();
    }
    f_put(this, ::log(a));
}

/*
 * log10(f)
 */
void Float::log10()
{
    double a;

    a = f_get(this);
    if (a <= 0.0) {
	f_edom();
    }
    f_put(this, ::log10(a));
}

/*
 * pow(f1, f2)
 */
void Float::pow(Float &f)
{
    double a, b;

    a = f_get(this);
    b = f_get(&f);
    if (a < 0.0) {
	if (b != ::floor(b)) {
	    /* non-integer power of negative number */
	    f_edom();
	}
    } else if (a == 0.0) {
	if (b < 0.0) {
	    /* negative power of 0.0 */
	    f_edom();
	}
    }

    f_put(this, ::pow(a, b));
}

/*
 * sqrt(f)
 */
void Float::sqrt()
{
    double a;

    a = f_get(this);
    if (a < 0.0) {
	f_edom();
    }
    f_put(this, ::sqrt(a));
}

/*
 * cos(f)
 */
void Float::cos()
{
    f_put(this, ::cos(f_get(this)));
}

/*
 * sin(f)
 */
void Float::sin()
{
    f_put(this, ::sin(f_get(this)));
}

/*
 * float(f)
 */
void Float::tan()
{
    f_put(this, ::tan(f_get(this)));
}

/*
 * acos(f)
 */
void Float::acos()
{
    double a;

    a = f_get(this);
    if (fabs(a) > 1.0) {
	f_edom();
    }
    f_put(this, ::acos(a));
}

/*
 * asin(f)
 */
void Float::asin()
{
    double a;

    a = f_get(this);
    if (fabs(a) > 1.0) {
	f_edom();
    }
    f_put(this, ::asin(a));
}

/*
 * atan(f)
 */
void Float::atan()
{
    f_put(this, ::atan(f_get(this)));
}

/*
 * atan2(f)
 */
void Float::atan2(Float &f)
{
    f_put(this, ::atan2(f_get(this), f_get(&f)));
}

/*
 * cosh(f)
 */
void Float::cosh()
{
    f_put(this, ::cosh(f_get(this)));
}

/*
 * sinh(f)
 */
void Float::sinh()
{
    f_put(this, ::sinh(f_get(this)));
}

/*
 * tanh(f)
 */
void Float::tanh()
{
    f_put(this, ::tanh(f_get(this)));
}
