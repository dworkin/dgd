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

pcfunc *pcfunctions;		/* table of precompiled functions */

/*
 * NAME:	precomp->preload()
 * DESCRIPTION:	preload compiled objects
 */
bool pc_preload(auto_obj, driver_obj)
char *auto_obj, *driver_obj;
{
    return TRUE;
}

/*
 * NAME:	precomp->list()
 * DESCRIPTION:	return an array with all precompiled objects
 */
array *pc_list(data)
dataspace *data;
{
    return arr_new(data, 0L);
}

/*
 * NAME:	precomp->control()
 * DESCRIPTION:	initialize the control block of a precompiled object
 */
void pc_control(ctrl, obj)
control *ctrl;
object *obj;
{
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

typedef struct {
    Uint compiled;		/* compile time */
    short ninherits;		/* # inherits */
    uindex imapsz;		/* imap size */
    unsigned short nstrings;	/* # strings */
    Uint stringsz;		/* strings size */
    short nfuncdefs;		/* # funcdefs */
    short nvardefs;		/* # vardefs */
    uindex nfuncalls;		/* # function calls */
    short nvariables;		/* # variables */
} dump_precomp;

static char dp_layout[] = "isusissus";

typedef struct {
    uindex oindex;		/* object index */
    uindex progoffset;		/* program offset */
    uindex funcoffset;		/* function offset */
    unsigned short varoffset;	/* variable offset */
    bool priv;			/* privately inherited? */
} dump_inherit;

static char di_layout[] = "uuusc";

/*
 * NAME:	precomp->dump()
 * DESCRIPTION:	dump precompiled objects
 */
bool pc_dump(fd)
int fd;
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
    return P_write(fd, (char *) &dh, sizeof(dump_header)) ==
							sizeof(dump_header);
}

/*
 * NAME:	precomp->restore()
 * DESCRIPTION:	restore and replace precompiled objects
 */
void pc_restore(fd, conv)
int fd, conv;
{
    dump_header dh;
    register uindex oindex;
    register char *name;

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
    }

    if (dh.nprecomps != 0) {
	register dump_precomp *dpc;
	register dump_inherit *dinh;

	/*
	 * Restore old precompiled objects.
	 */
	dpc = ALLOCA(dump_precomp, dh.nprecomps);
	conf_dread(fd, (char *) dpc, dp_layout, (Uint) dh.nprecomps);
	dinh = ALLOCA(dump_inherit, dh.ninherits);
	conf_dread(fd, (char *) dinh, di_layout, dh.ninherits);

	/* but restored objects are no longer precompiled */
	oindex = dinh[dpc->ninherits - 1].oindex;
	name = OBJ(oindex)->chain.name;
	error("Restored object not precompiled: /%s", name);
    }
}
