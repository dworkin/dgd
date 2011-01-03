/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010 DGD Authors (see the file Changelog for details)
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

typedef struct {
    char *name;			/* name of object */
    uindex progoffset;		/* program offset */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset */
    bool priv;			/* privately inherited? */
} pcinherit;

typedef void (*pcfunc) (frame*);

typedef struct {
    uindex oindex;		/* precompiled object */

    short ninherits;		/* # of inherits */
    pcinherit *inherits;	/* inherits */

    uindex imapsz;		/* inherit map size */
    char *imap;			/* inherit map */

    Uint compiled;		/* compile time */

    unsigned short progsize;	/* program size */
    char *program;		/* program */

    unsigned short nstrings;	/* # of strings */
    dstrconst* sstrings;	/* string constants */
    char *stext;		/* string text */
    Uint stringsz;		/* string size */

    unsigned short nfunctions;	/* # functions */
    pcfunc *functions;		/* functions */

    short nfuncdefs;		/* # function definitions */
    dfuncdef *funcdefs;		/* function definitions */

    short nvardefs;		/* # variable definitions */
    short nclassvars;		/* # class variable definitions */
    dvardef *vardefs;		/* variable definitions */
    char *classvars;		/* variable classes */

    uindex nfuncalls;		/* # function calls */
    char *funcalls;		/* function calls */

    uindex nsymbols;		/* # symbols */
    dsymbol *symbols;		/* symbols */

    unsigned short nvariables;	/* # variables */
    char *vtypes;		/* variable types */

    short typechecking;		/* typechecking level */
} precomp;

extern precomp	*precompiled[];	/* table of precompiled objects */
extern pcfunc	*pcfunctions;	/* table of precompiled functions */


bool   pc_preload	(char*, char*);
array *pc_list		(dataspace*);
void   pc_control	(control*, object*);
bool   pc_dump		(int);
void   pc_restore	(int, int);
