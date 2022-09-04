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

# ifdef LARGENUM

# define FLOAT_BIAS	0x3fff
# define FLOAT_SIGN	0x80000000L
# define FLOAT_ONE	0x3fff0000L
# define FLOAT_MONE	0xbfff0000L
# define FLOAT_DIGITS	14
# define FLOAT_LIMIT	100000000000000LL
# define FLOAT_BUFFER	LPCINT_BUFFER

typedef Uint FloatHigh;
typedef uint64_t FloatLow;

# else

# define FLOAT_BIAS	0x3ff
# define FLOAT_SIGN	0x8000
# define FLOAT_ONE	0x3ff0
# define FLOAT_MONE	0xbff0
# define FLOAT_DIGITS	9
# define FLOAT_LIMIT	1000000000
# define FLOAT_BUFFER	17

typedef unsigned short FloatHigh;
typedef Uint FloatLow;

# endif

class Float {
public:
    static bool atof(char **buf, Float *f);
    static void itof(LPCint i, Float *f);

    void initZero() {
	high = 0;
	low = 0;
    }
    void initOne() {
	high = FLOAT_ONE;
	low = 0;
    }

    void ftoa(char *buffer);
    LPCint ftoi();

    void abs() {
	high &= ~FLOAT_SIGN;
    }
    void negate() {
	high ^= FLOAT_SIGN;
    }
    bool negative() {
	return !!(high & FLOAT_SIGN);
    }

    void add(Float &f);
    void sub(Float &f);
    void mult(Float &f);
    void div(Float &f);
    int cmp(Float &f);
    void floor();
    void ceil();
    void fmod(Float &f);
    LPCint frexp();
    void ldexp(LPCint exp);
    void modf(Float *f);

    void exp();
    void log();
    void log10();
    void pow(Float &f);
    void sqrt();
    void cos();
    void sin();
    void tan();
    void acos();
    void asin();
    void atan();
    void atan2(Float &f);
    void cosh();
    void sinh();
    void tanh();

    FloatHigh high;		/* high word of float */
    FloatLow low;		/* low longword of float */
};				/* 1 sign, 11 exponent, 36 mantissa */

# define FLOAT_ISZERO(h, l)	((h) == 0)
# define FLOAT_ISONE(h, l)	((h) == FLOAT_ONE && (l) == 0)
# define FLOAT_ISMONE(h, l)	((h) == FLOAT_MONE && (l) == 0)

extern Float max_int, thousand, thousandth;
