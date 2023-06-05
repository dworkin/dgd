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

struct SnList {
    class SnChunk *snc;		/* snode chunk */
    class SNode *first;		/* first node in list */
    SNode *last;		/* last node in list */
};

class Parser : public Allocated {
public:
    virtual ~Parser();

    void save();

    static Array *parse_string(Frame *f, String *source, String *str,
			       LPCint maxalt);

private:
    void reset();
    void reduce(class PNode *pn, char *p);
    void shift(SNode *sn, short token, char *text, ssizet len);
    PNode *parse(String *str, bool *toobig);
    Int traverse(PNode *pn, PNode *next);

    static Parser *create(Frame *f, String *source, String *grammar);
    static void flatten(PNode *pn, PNode *next, Value *v);
    static Parser *load(Frame *f, Value *elts);

    Frame *frame;		/* interpreter stack frame */
    Dataspace *data;		/* dataspace for current object */

    String *source;		/* grammar source */
    String *grammar;		/* preprocessed grammar */
    char *fastr;		/* DFA string */
    char *lrstr;		/* SRP string */

    class Dfa *fa;		/* (partial) DFA */
    class Srp *lr;		/* (partial) shift/reduce parser */
    short ntoken;		/* # of tokens (regexp + string) */
    short nprod;		/* # of nonterminals */

    class PnChunk *pnc;		/* pnode chunk */

    unsigned short nstates;	/* state table size */
    struct PState *states;	/* state table */
    SnList list;		/* snode list */

    class StrPChunk *strc;	/* string chunk */
    class ArrPChunk *arrc;	/* array chunk */

    LPCint maxalt;		/* max number of branches */
};
