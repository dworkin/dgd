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

struct _string_ {
    struct _strref_ *primary;	/* primary reference */
    Uint ref;			/* number of references + const bit */
    ssizet len;			/* string length */
    char text[1];		/* actual characters following this struct */
};

extern string	       *str_alloc	P((char*, long));
extern string	       *str_new		P((char*, long));
# define str_ref(s)	((s)->ref++)
extern void		str_del		P((string*));

extern void		str_merge	P((void));
extern Uint		str_put		P((string*, Uint));
extern void		str_clear	P((void));

extern int		str_cmp		P((string*, string*));
extern string	       *str_add		P((string*, string*));
extern ssizet		str_index	P((string*, long));
extern void		str_ckrange	P((string*, long, long));
extern string	       *str_range	P((string*, long, long));
