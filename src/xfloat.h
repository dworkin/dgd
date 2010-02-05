/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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

typedef struct {
    unsigned short high;	/* high word of float */
    Uint low;			/* low longword of float */
} xfloat;			/* 1 sign, 11 exponent, 36 mantissa */

# define FLT_ISZERO(h, l)	((h) == 0)
# define FLT_ISNEG(h, l)	((h) & 0x8000)
# define FLT_ISONE(h, l)	((h) == 0x3ff0 && (l) == 0L)
# define FLT_ISMONE(h, l)	((h) == 0xbff0 && (l) == 0L)
# define FLT_ZERO(h, l)		((h) = 0, (l) = 0L)
# define FLT_ONE(h, l)		((h) = 0x3ff0, (l) = 0L)
# define FLT_ABS(h, l)		((h) &= ~0x8000)
# define FLT_NEG(h, l)		((h) ^= 0x8000)

extern bool	flt_atof	P((char**, xfloat*));
extern void	flt_ftoa	P((xfloat*, char*));
extern void	flt_itof	P((Int, xfloat*));
extern Int	flt_ftoi	P((xfloat*));

extern void	flt_add		P((xfloat*, xfloat*));
extern void	flt_sub		P((xfloat*, xfloat*));
extern void	flt_mult	P((xfloat*, xfloat*));
extern void	flt_div		P((xfloat*, xfloat*));
extern int	flt_cmp		P((xfloat*, xfloat*));
extern void	flt_floor	P((xfloat*));
extern void	flt_ceil	P((xfloat*));
extern void	flt_fmod	P((xfloat*, xfloat*));
extern Int	flt_frexp	P((xfloat*));
extern void	flt_ldexp	P((xfloat*, Int));
extern void	flt_modf	P((xfloat*, xfloat*));

extern void	flt_exp		P((xfloat*));
extern void	flt_log		P((xfloat*));
extern void	flt_log10	P((xfloat*));
extern void	flt_pow		P((xfloat*, xfloat*));
extern void	flt_sqrt	P((xfloat*));
extern void	flt_cos		P((xfloat*));
extern void	flt_sin		P((xfloat*));
extern void	flt_tan		P((xfloat*));
extern void	flt_acos	P((xfloat*));
extern void	flt_asin	P((xfloat*));
extern void	flt_atan	P((xfloat*));
extern void	flt_atan2	P((xfloat*, xfloat*));
extern void	flt_cosh	P((xfloat*));
extern void	flt_sinh	P((xfloat*));
extern void	flt_tanh	P((xfloat*));

extern xfloat max_int, thousand, thousandth;
