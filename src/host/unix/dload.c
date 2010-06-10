/*
 * This file is part of DGD, http://www.dworkin.nl/dgd/
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

# ifdef LPC_EXTENSION

# include "dgd.h"
# include <dlfcn.h>
# ifdef SOLARIS
# include <link.h>
# endif

char *P_dload(module, symbol)
char *module, *symbol;
{
    char *h;

    h = dlopen(module, RTLD_NOW | RTLD_LOCAL | RTLD_FIRST);
    if (h == (char *) NULL) {
	return (char *) NULL;
    }
    return (char *) dlsym(h, symbol);
}
# endif /* LPC_EXTENSION */
