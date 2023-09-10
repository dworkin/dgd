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

class Dfa : public Allocated {
public:
    virtual ~Dfa();

    bool save(char **str, Uint *len);
    short scan(String *str, ssizet *strlen, char **token, ssizet *len);

    static Dfa *create(char *source, char *grammar);
    static Dfa *load(char *source, char *grammar, char *str, Uint len);

private:
    Dfa(char *source, char *grammar);

    void extend(class DfaState *state, unsigned short limit);
    void loadtmp();
    void split(class Charset *iset, Charset *cset, Uint ncset);
    unsigned short newstate(DfaState *state, DfaState *newstate, Charset *ecset,
			    Charset *cset);
    DfaState *expand(DfaState *state);

    char *source;		/* source grammar */
    char *grammar;		/* reference grammar */
    char *strings;		/* offset of strings in grammar */
    unsigned short nsstrings;	/* # strings in source grammar */
    short whitespace;		/* whitespace rule or -1 */
    short nomatch;		/* nomatch rule or -1 */

    bool modified;		/* dfa modified */
    bool allocated;		/* dfa strings allocated locally */
    Uint dfasize;		/* size of state machine */
    Uint tmpssize;		/* size of temporary state data */
    Uint tmppsize;		/* size of temporary posn data */
    char *dfastr;		/* saved dfa */
    char *tmpstr;		/* saved temporary data */

    unsigned short nregexp;	/* # regexps */
    Uint nposn;			/* number of unique positions */
    class RpChunk *rpc;		/* current rgxposn chunk */
    Hash::Hashtab *posnhtab;	/* position hash table */

    unsigned short nstates;	/* # states */
    unsigned short nexpanded;	/* # expanded states */
    unsigned short endstates;	/* # states with no valid transitions */
    Uint sttsize;		/* state table size */
    Uint sthsize;		/* size of state hash table */
    DfaState *states;		/* dfa states */
    unsigned short *sthtab;	/* state hash table */

    unsigned short ecnum;	/* number of equivalence classes */
    char *ecsplit;		/* equivalence class split history */
    char *ecmembers;		/* members per equivalence class */
    Charset *ecset;		/* equivalence class sets */
    char eclass[256];		/* equivalence classes */

    char zerotrans[2 * 256];	/* shared zero transitions */
};

# define DFA_EOS	-1
# define DFA_REJECT	-2
# define DFA_TOOBIG	-3
