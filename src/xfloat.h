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

extern bool	flt_atof	(char**, xfloat*);
extern void	flt_ftoa	(xfloat*, char*);
extern void	flt_itof	(Int, xfloat*);
extern Int	flt_ftoi	(xfloat*);

extern void	flt_add		(xfloat*, xfloat*);
extern void	flt_sub		(xfloat*, xfloat*);
extern void	flt_mult	(xfloat*, xfloat*);
extern void	flt_div		(xfloat*, xfloat*);
extern int	flt_cmp		(xfloat*, xfloat*);
extern void	flt_floor	(xfloat*);
extern void	flt_ceil	(xfloat*);
extern void	flt_fmod	(xfloat*, xfloat*);
extern Int	flt_frexp	(xfloat*);
extern void	flt_ldexp	(xfloat*, Int);
extern void	flt_modf	(xfloat*, xfloat*);

extern void	flt_exp		(xfloat*);
extern void	flt_log		(xfloat*);
extern void	flt_log10	(xfloat*);
extern void	flt_pow		(xfloat*, xfloat*);
extern void	flt_sqrt	(xfloat*);
extern void	flt_cos		(xfloat*);
extern void	flt_sin		(xfloat*);
extern void	flt_tan		(xfloat*);
extern void	flt_acos	(xfloat*);
extern void	flt_asin	(xfloat*);
extern void	flt_atan	(xfloat*);
extern void	flt_atan2	(xfloat*, xfloat*);
extern void	flt_cosh	(xfloat*);
extern void	flt_sinh	(xfloat*);
extern void	flt_tanh	(xfloat*);

extern xfloat max_int, thousand, thousandth;
