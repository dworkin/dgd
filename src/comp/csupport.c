/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010,2012-2013 DGD Authors (see the commit log for details)
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

/*
 * NAME:	precomp->list()
 * DESCRIPTION:	return an array with all precompiled objects
 */
array *pc_list(dataspace *data)
{
    return arr_new(data, 0L);
}


typedef struct {
    uindex nprecomps;		/* # precompiled objects */
    Uint ninherits;		/* total # inherits */
    Uint imapsz;		/* total imap size */
    Uint nstrings;		/* total # strings */
    Uint stringsz;		/* total strings size */
    Uint nfuncdefs;		/* total # funcdefs */
    Uint nvardefs;		/* total # vardefs */
    Uint nfuncalls;		/* total # function calls */
} dump_header;

static char dh_layout[] = "uiiiiiii";

typedef struct {
    uindex nprecomps;		/* # precompiled objects */
    Uint ninherits;		/* total # inherits */
    Uint nstrings;		/* total # strings */
    Uint stringsz;		/* total strings size */
    Uint nfuncdefs;		/* total # funcdefs */
    Uint nvardefs;		/* total # vardefs */
    Uint nfuncalls;		/* total # function calls */
} odump_header;

static char odh_layout[] = "uiiiiii";

/*
 * NAME:	precomp->dump()
 * DESCRIPTION:	dump precompiled objects
 */
bool pc_dump(int fd)
{
    dump_header dh;

    dh.nprecomps = 0;
    dh.ninherits = 0;
    dh.imapsz = 0;
    dh.nstrings = 0;
    dh.stringsz = 0;
    dh.nfuncdefs = 0;
    dh.nvardefs = 0;
    dh.nfuncalls = 0;

    /* write header */
    if (P_write(fd, (char *) &dh, sizeof(dump_header)) != sizeof(dump_header)) {
	return FALSE;
    }

    return TRUE;
}

/*
 * NAME:	precomp->restore()
 * DESCRIPTION:	restore and replace precompiled objects
 */
void pc_restore(int fd, int conv)
{
    dump_header dh = {0};

    /* read header */
    if (conv) {
	odump_header odh;

	conf_dread(fd, (char *) &odh, odh_layout, (Uint) 1);
	if (odh.nprecomps != 0) {
	    fatal("precompiled objects during conversion");
	}
	dh.nprecomps = 0;
    } else {
	conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);
	if (dh.nprecomps != 0) {
	    fatal("precompiled objects in snapshot");
	}
    }
}
