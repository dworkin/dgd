/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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

static class MacroChunk : public Chunk<Macro, MCHUNKSZ> {
public:
    /*
     * free strings when iterating through items
     */
    virtual bool item(Macro *m) {
	if (m->name != (char *) NULL) {
	    FREE(m->name);
	    if (m->replace != (char *) NULL) {
		FREE(m->replace);
	    }
	}
	return TRUE;
    }
} mchunk;

static Hash::Hashtab *mt;	/* macro hash table */

/*
 * intiialize the macro table
 */
void Macro::init()
{
    mt = HM->create(MACTABSZ, MACHASHSZ, FALSE);
}

/*
 * clear the macro table
 */
void Macro::clear()
{
    if (mt != (Hash::Hashtab *) NULL) {
	delete mt;
	mt = (Hash::Hashtab *) NULL;

	mchunk.items();
	mchunk.clean();
    }
}

/*
 * constructor
 */
Macro::Macro(const char *name)
{
    next = (Hash::Entry *) NULL;
    this->name = strcpy(ALLOC(char, strlen(name) + 1), name);
    replace = (char *) NULL;
}

/*
 * destructor
 */
Macro::~Macro()
{
    FREE(name);
    name = (char *) NULL;
    if (replace != (char *) NULL) {
	FREE(replace);
	replace = (char *) NULL;
    }
}

/*
 * define a macro
 */
bool Macro::define(const char *name, const char *replace, int narg)
{
    bool status;
    Hash::Entry **m;
    Macro *mac;

    status = TRUE;
    m = mt->lookup(name, FALSE);
    if ((Macro *) *m != (Macro *) NULL) {
	/* the macro already exists. */
	mac = (Macro *) *m;
	if (mac->replace != (char *) NULL &&
	    (mac->narg != narg || strcmp(mac->replace, replace) != 0)) {
	    status = FALSE;
	}
    } else {
	*m = mac = chunknew (mchunk) Macro(name);
    }
    /* fill in macro */
    if (replace != (char *) NULL) {
	mac->replace = strcpy(REALLOC(mac->replace, char, 0,
				      strlen(replace) + 1),
			       replace);
    } else {
	mac->replace = (char *) NULL;
    }
    mac->narg = narg;

    return status;
}

/*
 * undefine a macro
 */
void Macro::undef(char *name)
{
    Hash::Entry **m;
    Macro *mac;

    m = mt->lookup(name, FALSE);
    if ((Macro *) *m != (Macro *) NULL) {
	/* it really exists. */
	mac = (Macro *) *m;
	*m = mac->next;
	delete mac;
    }
}

/*
 * lookup a macro definition in the macro table. Return NULL if
 * the macro is not found.
 */
Macro *Macro::lookup(char *name)
{
    return (Macro *) *mt->lookup(name, TRUE);
}
