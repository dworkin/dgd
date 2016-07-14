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

# include "lex.h"
# include "macro.h"

/*
 * The macro handling routines. These use the hash table.
 */

# define MCHUNKSZ	32

static class macrochunk : public Chunk<macro, MCHUNKSZ> {
public:
    /*
     * NAME:		items()
     * DESCRIPTION:	free strings when iterating through items
     */
    virtual bool item(macro *m) {
	if (m->name != (char *) NULL) {
	    FREE(m->name);
	    if (m->replace != (char *) NULL) {
		FREE(m->replace);
	    }
	}
	return TRUE;
    }
} mchunk;

static Hashtab *mt;		/* macro hash table */

/*
 * NAME:	macro->init()
 * DESCRIPTION:	intiialize the macro table
 */
void mc_init()
{
    mt = Hashtab::create(MACTABSZ, MACHASHSZ, FALSE);
}

/*
 * NAME:	macro->clear()
 * DESCRIPTION:	clear the macro table
 */
void mc_clear()
{
    if (mt != (Hashtab *) NULL) {
	delete mt;
	mt = (Hashtab *) NULL;

	mchunk.items();
	mchunk.clean();
    }
}

/*
 * NAME:	macro->define()
 * DESCRIPTION:	define a macro
 */
void mc_define(const char *name, const char *replace, int narg)
{
    macro **m;

    m = (macro **) mt->lookup(name, FALSE);
    if (*m != (macro *) NULL) {
	/* the macro already exists. */
	if ((*m)->replace != (char *) NULL &&
	    ((*m)->narg != narg || strcmp((*m)->replace, replace) != 0)) {
	    warning("macro %s redefined", name);
	}
    } else {
	*m = mchunk.alloc();
	(*m)->next = (Hashtab::Entry *) NULL;
	(*m)->name = strcpy(ALLOC(char, strlen(name) + 1), name);
	(*m)->replace = (char *) NULL;
    }
    /* fill in macro */
    if (replace != (char *) NULL) {
	(*m)->replace = strcpy(REALLOC((*m)->replace, char, 0,
				       strlen(replace) + 1),
			       replace);
    } else {
	(*m)->replace = (char *) NULL;
    }
    (*m)->narg = narg;
}

/*
 * NAME:	macro->undef()
 * DESCRIPTION:	undefine a macro
 */
void mc_undef(char *name)
{
    macro **m, *mac;

    m = (macro **) mt->lookup(name, FALSE);
    if (*m != (macro *) NULL) {
	/* it really exists. */
	mac = *m;
	FREE(mac->name);
	mac->name = (char *) NULL;
	if (mac->replace != (char *) NULL) {
	    FREE(mac->replace);
	    mac->replace = (char *) NULL;
	}
	*m = (macro *) mac->next;
	mchunk.del(mac);
    }
}

/*
 * NAME:	macro->lookup()
 * DESCRIPTION:	lookup a macro definition in the macro table. Return &NULL if
 *		the macro is not found.
 */
macro *mc_lookup(char *name)
{
    return *(macro **) mt->lookup(name, TRUE);
}
