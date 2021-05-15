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


class Srp : public Allocated {
public:
    virtual ~Srp();

    void loadtmp();
    bool save(char **str, Uint *len);
    short reduce(unsigned int num, unsigned short *nredp, char **redp);
    short shift(unsigned int num, unsigned int token);
    short _goto(unsigned int num, unsigned int symb);

    static Srp *create(char *grammar);
    static Srp *load(char *grammar, char *str, Uint len);

private:
    Srp(char *grammar);

    Int pack(unsigned short *check, unsigned short *from, unsigned short *to,
	     unsigned short n);
    class SrpState *expand(SrpState *state);

    char *grammar;		/* grammar */
    unsigned short nsstring;	/* # of source grammar strings */
    unsigned short ntoken;	/* # of tokens (regexp & string) */
    unsigned short nprod;	/* # of nonterminals */

    Uint nred;			/* # of reductions */
    Uint nitem;			/* # of items */
    Uint srpsize;		/* size of shift/reduce parser */
    Uint tmpsize;		/* size of temporary data */
    bool modified;		/* srp needs saving */
    bool allocated;		/* srp allocated */
    char *srpstr;		/* srp string */
    char *tmpstr;		/* tmp string */

    unsigned short nstates;	/* number of states */
    unsigned short nexpanded;	/* number of expanded states */
    Uint sttsize;		/* state table size */
    Uint sthsize;		/* state hash table size */
    class SrpState *states;	/* state array */
    unsigned short *sthtab;	/* state hash table */

    class ItChunk *itc;		/* item chunk */

    Uint gap;			/* first gap in packed mapping */
    Uint spread;		/* max spread in packed mapping */
    Uint mapsize;		/* packed mapping size */
    char *data;			/* packed shifts */
    char *check;		/* packed check for shift validity */
    bool alloc;			/* data and check allocated separately? */

    class SlChunk *slc;		/* shlink chunk */
    Uint nshift;		/* number of shifts (from/to pairs) */
    Uint shtsize;		/* shift table size */
    Uint shhsize;		/* shift hash table size */
    char *shtab;		/* shift (from/to) table */
    class ShLink **shhtab;	/* shift hash table */
};
