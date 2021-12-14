/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2016 DGD Authors (see the commit log for details)
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

class Float {
public:
    static bool atof(char **buf, Float *f);
    static void itof(LPCint i, Float *f);

    void initZero() {
	high = 0;
	low = 0;
    }
    void initOne() {
	high = 0x3ff0;
	low = 0;
    }

    void ftoa(char *buffer);
    LPCint ftoi();

    void abs() {
	high &= ~0x8000;
    }
    void negate() {
	high ^= 0x8000;
    }
    bool negative() {
	return !!(high & 0x8000);
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

    unsigned short high;	/* high word of float */
    Uint low;			/* low longword of float */
};				/* 1 sign, 11 exponent, 36 mantissa */

# define FLOAT_ISZERO(h, l)	((h) == 0)
# define FLOAT_ISONE(h, l)	((h) == 0x3ff0 && (l) == 0L)
# define FLOAT_ISMONE(h, l)	((h) == 0xbff0 && (l) == 0L)

extern Float max_int, thousand, thousandth;
