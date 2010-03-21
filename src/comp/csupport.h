/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
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


# define i_foffset(n)		(&f->ctrl->funcalls[2L * (f->foffset + (n))])

/*
 * prototypes for kfuns that might be called directly from precompiled code
 */
int kf_add (frame*), kf_add1 (frame*), kf_and (frame*),
    kf_div (frame*), kf_eq (frame*), kf_ge (frame*), kf_gt (frame*),
    kf_le (frame*), kf_lshift (frame*), kf_lt (frame*),
    kf_mod (frame*), kf_mult (frame*), kf_ne (frame*),
    kf_neg (frame*), kf_not (frame*), kf_or (frame*),
    kf_rangeft (frame*), kf_rangef (frame*), kf_ranget (frame*),
    kf_range (frame*), kf_rshift (frame*), kf_sub (frame*),
    kf_sub1 (frame*), kf_tofloat (frame*), kf_toint (frame*),
    kf_tst (frame*), kf_umin (frame*), kf_xor (frame*),
    kf_tostring (frame*), kf_ckrangeft (frame*), kf_ckrangef (frame*),
    kf_ckranget (frame*), kf_sum (frame*, int);

int kf_this_object (frame*), kf_call_trace (frame*),
    kf_this_user (frame*), kf_users (frame*), kf_time (frame*),
    kf_swapout (frame*), kf_dump_state (frame*), kf_shutdown (frame*);

Int  push_number        (frame*, Int);
void push_lvalue        (frame*, value*, unsigned int);
void push_lvclass       (frame*, Int);
Int  pop_number         (frame*);
void store_value        (frame*);
Int  store_int          (frame*);
void call_kfun		(frame*, int);
void call_kfun_arg	(frame*, int, int);
Int  xdiv		(Int, Int);
Int  xmod		(Int, Int);
Int  xlshift		(Int, Int);
Int  xrshift		(Int, Int);
bool poptruthval	(frame*);
void new_rlimits	(frame*);
int  switch_range	(Int, Int*, int);
int  switch_str		(value*, control*, char*, int);
