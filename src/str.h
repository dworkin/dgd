/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2015 DGD Authors (see the commit log for details)
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

struct String {
    struct strref *primary;	/* primary reference */
    Uint ref;			/* number of references + const bit */
    ssizet len;			/* string length */
    char text[1];		/* actual characters following this struct */
};

extern String	       *str_alloc	(const char*, long);
extern String	       *str_new		(const char*, long);
# define str_ref(s)	((s)->ref++)
extern void		str_del		(String*);

extern void		str_merge	(void);
extern Uint		str_put		(String*, Uint);
extern void		str_clear	(void);

extern int		str_cmp		(String*, String*);
extern String	       *str_add		(String*, String*);
extern ssizet		str_index	(String*, long);
extern void		str_ckrange	(String*, long, long);
extern String	       *str_range	(String*, long, long);
