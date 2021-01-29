/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2021 DGD Authors (see the commit log for details)
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

# include "ed.h"
# include "vars.h"

/*
 * The editor variables are handled here.
 */

/*
 * allocate and initialize a variable buffer
 */
Vars *Vars::create()
{
    static Vars dflt[] = {
	{ "ignorecase",	"ic",	FALSE },
	{ "shiftwidth",	"sw",	4 },
	{ "window",	"wi",	20 },
    };
    Vars *v;

    v = ALLOC(Vars, NUMBER_OF_VARS);
    memcpy(v, dflt, sizeof(dflt));

    return v;
}

/*
 * delete a variable buffer
 */
void Vars::del(Vars *v)
{
    FREE(v);
}

/*
 * set the value of a variable.
 */
void Vars::set(Vars *v, char *option)
{
    char buffer[2];
    char *val;
    Int i;

    if (strncmp(option, "no", 2) == 0) {
	option += 2;
	strcpy(buffer, "0");
	val = buffer;
    } else {
	val = strchr(option, '=');
	if (val != (char *) NULL) {
	    *val++ = '\0';
	}
    }

    for (i = NUMBER_OF_VARS; i > 0; --i, v++) {
	if (strcmp(v->name, option) == 0 ||
	  strcmp(v->sname, option) == 0) {
	    if (!val) {
		v->val = 1;
	    } else {
		char *p;

		p = val;
		i = strtol(p, &p, 10);
		if (val == p || i < 0) {
		    EDC->error("Bad numeric value for option \"%s\"", v->name);
		}
		v->val = i;
	    }
	    return;
	}
    }
    EDC->error("No such option");
}

/*
 * show all variables
 */
void Vars::show(Vars *v)
{
    EDC->message("%signorecase\011",   ((v++)->val) ? "" : "no");	/* HT */
    EDC->message("shiftwidth=%ld\011", (long) (v++)->val);		/* HT */
    EDC->message("window=%ld\012",     (long) (v++)->val);		/* LF */
}
