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

# include "hash.h"

struct macro : public Hashtab::Entry {
    char *replace;		/* replace text */
    int narg;			/* number of arguments */
};

# define MA_NARG	0x1f
# define MA_NOEXPAND	0x20
# define MA_STRING	0x40
# define MA_TAG		0x80

# define MAX_NARG	31

# define MAX_REPL_SIZE	(4 * MAX_LINE_SIZE)

extern void   mc_init	();
extern void   mc_clear  ();
extern void   mc_define (const char*, const char*, int);
extern void   mc_undef  (char*);
extern macro *mc_lookup (char*);
