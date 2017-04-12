/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2017 DGD Authors (see the commit log for details)
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

# define INCLUDE_FILE_IO
# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "table.h"
# include "control.h"
# include "node.h"
# include "compile.h"
# include "csupport.h"


struct dump_header {
    uindex nprecomps;		/* # precompiled objects */
    Uint ninherits;		/* total # inherits */
    Uint imapsz;		/* total imap size */
    Uint nstrings;		/* total # strings */
    Uint stringsz;		/* total strings size */
    Uint nfuncdefs;		/* total # funcdefs */
    Uint nvardefs;		/* total # vardefs */
    Uint nfuncalls;		/* total # function calls */
};

static char dh_layout[] = "uiiiiiii";

/*
 * NAME:	precomp->restore()
 * DESCRIPTION:	restore and replace precompiled objects
 */
void pc_restore(int fd)
{
    dump_header dh = {0};

    /* read header */
    conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);
    if (dh.nprecomps != 0) {
	fatal("precompiled objects in snapshot");
    }
}
