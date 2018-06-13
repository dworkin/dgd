/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
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

# define INCLUDE_CTYPE
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "grammar.h"
# include "dfa.h"
# include "srp.h"
# include "parse.h"

struct pnode : public ChunkAllocated {
    short symbol;		/* node symbol */
    unsigned short state;	/* state reached after this symbol */
    Uint len;			/* token/reduction length or subtree size */
    union {
	char *text;		/* token/reduction text */
	String *str;		/* token string */
	Array *arr;		/* rule array */
    } u;
    pnode *next;		/* next in linked list */
    pnode *list;		/* list of nodes for reduction */
    pnode *trav;		/* traverse list */
};

# define PNCHUNKSZ	256

typedef Chunk<pnode, PNCHUNKSZ> pnchunk;

/*
 * NAME:	pnode->new()
 * DESCRIPTION:	create a new pnode
 */
static pnode *pn_new(pnchunk **c, short symb, unsigned short state, char *text, ssizet len, pnode *next, pnode *list)
{
    pnode *pn;

    if (*c == (pnchunk *) NULL) {
	*c = new pnchunk;
    }
    pn = chunknew (**c) pnode;

    pn->symbol = symb;
    pn->state = state;
    pn->len = len;
    pn->u.text = text;
    pn->next = next;
    pn->list = list;

    return pn;
}

struct snode : public ChunkAllocated {
    pnode *pn;			/* pnode */
    snode *next;		/* next to be treated */
    snode *slist;		/* per-state list */
};

# define SNCHUNKSZ	32

typedef Chunk<snode, SNCHUNKSZ> snchunk;

struct snlist {
    snchunk *snc;		/* snode chunk */
    snode *first;		/* first node in list */
    snode *last;		/* last node in list */
};

/*
 * NAME:	snode->new()
 * DESCRIPTION:	create a new snode
 */
static snode *sn_new(snlist *list, pnode *pn, snode *slist)
{
    snode *sn;

    if (list->snc == (snchunk *) NULL) {
	list->snc = new snchunk;
    }
    sn = chunknew (*list->snc) snode;
    if (list->first == (snode *) NULL) {
	list->first = list->last = sn;
    } else {
	list->last->next = sn;
	list->last = sn;
    }

    sn->pn = pn;
    sn->next = (snode *) NULL;
    sn->slist = slist;

    return sn;
}

/*
 * NAME:	snode->add()
 * DESCRIPTION:	add an existing snode to a list
 */
static snode *sn_add(snlist *list, snode *sn, pnode *pn, snode *slist)
{
    if (list->first == (snode *) NULL) {
	list->first = list->last = sn;
    } else {
	list->last->next = sn;
	list->last = sn;
    }

    sn->pn = pn;
    sn->next = (snode *) NULL;
    sn->slist = slist;

    return sn;
}

/*
 * NAME:	snode->clear()
 * DESCRIPTION:	free all snodes in memory
 */
static void sn_clear(snlist *list)
{
    if (list->snc != (snchunk *) NULL) {
	list->snc->clean();
	delete list->snc;
	list->snc = (snchunk *) NULL;
	list->first = (snode *) NULL;
    }
}


# define STRCHUNKSZ	256

struct strptr : public ChunkAllocated {
    String *str;
};

class strpchunk : public Chunk<strptr, STRCHUNKSZ> {
public:
    /*
     * NAME:		~strpchunk()
     * DESCRIPTION:	iterate through items from destructor
     */
    virtual ~strpchunk() {
	items();
    }

    /*
     * NAME:		item()
     * DESCRIPTION:	dereference strings when iterating through items
     */
    virtual bool item(strptr *str) {
	str->str->del();
	return TRUE;
    }
};

/*
 * NAME:	strpchunk->add()
 * DESCRIPTION:	add a string to the current chunk
 */
static void sc_add(strpchunk **c, String *str)
{
    if (*c == (strpchunk *) NULL) {
	*c = new strpchunk;
    }

    (chunknew (**c) strptr)->str = str;
    str->ref();
}


# define ARRCHUNKSZ	256

struct arrptr : public ChunkAllocated {
    Array *arr;
};

class arrpchunk : public Chunk<arrptr, ARRCHUNKSZ> {
public:
    /*
     * NAME:		~arrpchunk()
     * DESCRIPTION:	iterate through items from destructor
     */
    virtual ~arrpchunk() {
	items();
    }

    /*
     * NAME:		item()
     * DESCRIPTION:	dereference arrays when iterating through items
     */
    virtual bool item(arrptr *arr) {
	arr->arr->del();
	return TRUE;
    }
};

/*
 * NAME:	arrpchunk->add()
 * DESCRIPTION:	add an array to the current chunk
 */
static void ac_add(arrpchunk **c, Array *arr)
{
    if (*c == (arrpchunk *) NULL) {
	*c = new arrpchunk;
    }

    (chunknew (**c) arrptr)->arr = arr;
    arr->ref();
}


struct parser {
    Frame *frame;		/* interpreter stack frame */
    Dataspace *data;		/* dataspace for current object */

    String *source;		/* grammar source */
    String *grammar;		/* preprocessed grammar */
    char *fastr;		/* DFA string */
    char *lrstr;		/* SRP string */

    dfa *fa;			/* (partial) DFA */
    srp *lr;			/* (partial) shift/reduce parser */
    short ntoken;		/* # of tokens (regexp + string) */
    short nprod;		/* # of nonterminals */

    pnchunk *pnc;		/* pnode chunk */

    unsigned short nstates;	/* state table size */
    snode **states;		/* state table */
    snlist list;		/* snode list */

    strpchunk *strc;		/* string chunk */
    arrpchunk *arrc;		/* array chunk */

    Int maxalt;			/* max number of branches */
};

/*
 * NAME:	parser->new()
 * DESCRIPTION:	create a new parser instance
 */
static parser *ps_new(Frame *f, String *source, String *grammar)
{
    parser *ps;
    char *p;

    ps = ALLOC(parser, 1);
    ps->frame = f;
    ps->data = f->data;
    ps->data->parser = ps;
    ps->source = source;
    ps->source->ref();
    ps->grammar = grammar;
    ps->grammar->ref();
    ps->fastr = (char *) NULL;
    ps->lrstr = (char *) NULL;
    ps->fa = dfa_new(source->text, grammar->text);
    ps->lr = srp_new(grammar->text);

    ps->pnc = (pnchunk *) NULL;
    ps->list.snc = (snchunk *) NULL;
    ps->list.first = (snode *) NULL;

    ps->strc = (strpchunk *) NULL;
    ps->arrc = (arrpchunk *) NULL;

    p = grammar->text;
    ps->ntoken = ((UCHAR(p[5]) + UCHAR(p[9]) + UCHAR(p[11])) << 8) +
		 UCHAR(p[6]) + UCHAR(p[10]) + UCHAR(p[12]);
    ps->nprod = (UCHAR(p[13]) << 8) + UCHAR(p[14]);

    return ps;
}

/*
 * NAME:	parser->del()
 * DESCRIPTION:	delete parser
 */
void ps_del(parser *ps)
{
    ps->data->parser = (parser *) NULL;
    ps->source->del();
    ps->grammar->del();
    if (ps->fastr != (char *) NULL) {
	FREE(ps->fastr);
    }
    if (ps->lrstr != (char *) NULL) {
	FREE(ps->lrstr);
    }
    dfa_del(ps->fa);
    srp_del(ps->lr);
    FREE(ps);
}

/*
 * NAME:	parser->reduce()
 * DESCRIPTION:	perform a reduction
 */
static void ps_reduce(parser *ps, pnode *pn, char *p)
{
    snode *sn;
    pnode *next;
    unsigned short n;
    short symb;
    char *red;
    ssizet len;

    /*
     * get rule to reduce by
     */
    red = ps->grammar->text + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
    p += 2;
    symb = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
    len = UCHAR(red[0]);

    /*
     * create reduce node
     */
    next = pn;
    if (len == 0) {
	pn = (pnode *) NULL;
    } else {
	n = len;
	do {
	    next = next->next;
	} while (--n != 0);
    }
    n = srp_goto(ps->lr, next->state, symb);
    pn = pn_new(&ps->pnc, symb, n, red, len, next, pn);

    /*
     * see if this reduction can be merged with another
     */
    i_add_ticks(ps->frame, 2);
    for (sn = ps->states[n]; sn != (snode *) NULL; sn = sn->slist) {
	if (sn->pn->symbol == symb && sn->pn->next == next) {
	    pnode **ppn;

	    if (sn->pn->u.text != (char *) NULL) {
		/* first alternative */
		sn->pn->list = pn_new(&ps->pnc, symb, n, sn->pn->u.text,
				      sn->pn->len, (pnode *) NULL,
				      sn->pn->list);
		sn->pn->u.text = (char *) NULL;
		sn->pn->len = 1;
	    }

	    /* add alternative */
	    for (ppn = &sn->pn->list;
		 *ppn != (pnode *) NULL && (*ppn)->u.text < red;
		 ppn = &(*ppn)->next) ;
	    sn->pn->len++;

	    pn->next = *ppn;
	    *ppn = pn;
	    return;
	}
	i_add_ticks(ps->frame, 1);
    }

    /*
     * new reduction
     */
    ps->states[n] = sn_new(&ps->list, pn, ps->states[n]);
}

/*
 * NAME:	parser->shift()
 * DESCRIPTION:	perform a shift
 */
static void ps_shift(parser *ps, snode *sn, short token, char *text, ssizet len)
{
    int n;

    n = srp_shift(ps->lr, sn->pn->state, token);
    if (n >= 0) {
	/* shift works: add new snode */
	ps->states[n] = sn_add(&ps->list, sn,
			       pn_new(&ps->pnc, token, n, text, len,
			       sn->pn, (pnode *) NULL),
			       ps->states[n]);
	return;
    }

    /* no shift: add node to free list */
    delete sn;
}

/*
 * NAME:	parser->parse()
 * DESCRIPTION:	parse a string, return a parse tangle
 */
static pnode *ps_parse(parser *ps, String *str, bool *toobig)
{
    snode *sn;
    short n;
    snode *next;
    char *ttext;
    ssizet size, tlen;
    unsigned short nred;
    char *red;

    /* initialize */
    size = str->len;
    ps->nstates = srp_check(ps->lr, 0, &nred, &red);
    if (ps->nstates < ps->nprod) {
	ps->nstates = ps->nprod;
    }
    ps->states = ALLOC(snode*, ps->nstates);
    memset(ps->states, '\0', ps->nstates * sizeof(snode*));
    ps->list.first = (snode *) NULL;

    /* state 0 */
    ps->states[0] = sn_new(&ps->list,
			   pn_new(&ps->pnc, 0, 0, (char *) NULL, (ssizet) 0,
				  (pnode *) NULL, (pnode *) NULL),
			   (snode *) NULL);

    do {
	/*
	 * apply reductions for current states, expanding states if needed
	 */
	for (sn = ps->list.first; sn != (snode *) NULL; sn = sn->next) {
	    n = srp_check(ps->lr, sn->pn->state, &nred, &red);
	    if (n < 0) {
		/* parser grown to big */
		FREE(ps->states);
		*toobig = TRUE;
		return (pnode *) NULL;
	    }
	    if (n > ps->nstates) {
		unsigned short stsize;

		/* grow tables */
		stsize = n;
		stsize <<= 1;
		ps->states = REALLOC(ps->states, snode*, ps->nstates, stsize);
		memset(ps->states + ps->nstates, '\0',
		       (stsize - ps->nstates) * sizeof(snode*));
		ps->nstates = stsize;
	    }
	    for (n = 0; n < nred; n++) {
		ps_reduce(ps, sn->pn, red);
		red += 4;
		if (ps->frame->rlim->ticks < 0) {
		    if (ps->frame->rlim->noticks) {
			ps->frame->rlim->ticks = 0x7fffffff;
		    } else {
			FREE(ps->states);
			error("Out of ticks");
		    }
		}
	    }
	    i_add_ticks(ps->frame, 1);
	}

	switch (n = dfa_scan(ps->fa, str, &size, &ttext, &tlen)) {
	case DFA_EOS:
	    /* if end of string, return node from state 1 */
	    sn = ps->states[1];
	    FREE(ps->states);
	    return (sn != (snode *) NULL) ? sn->pn : (pnode *) NULL;

	case DFA_REJECT:
	    /* bad token */
	    FREE(ps->states);
	    error("Bad token at offset %u", str->len - size);
	    return (pnode *) NULL;

	case DFA_TOOBIG:
	    FREE(ps->states);
	    *toobig = TRUE;
	    return (pnode *) NULL;

	default:
	    /* shift */
	    memset(ps->states, '\0', ps->nstates * sizeof(snode*));
	    sn = ps->list.first;
	    ps->list.first = (snode *) NULL;
	    do {
		next = sn->next;
		ps_shift(ps, sn, n, ttext, tlen);
		sn = next;
	    } while (sn != (snode *) NULL);
	}
    } while (ps->list.first != (snode *) NULL);

    /*
     * parsing failed
     */
    FREE(ps->states);
    return (pnode *) NULL;
}


# define PN_STRING	-1	/* string node */
# define PN_ARRAY	-2	/* array node */
# define PN_RULE	-3	/* rule node */
# define PN_BRANCH	-4	/* branch node */
# define PN_BLOCKED	-5	/* blocked branch */

/*
 * NAME:	parser->flatten()
 * DESCRIPTION:	traverse parse tree, collecting values in a flat array
 */
static void ps_flatten(pnode *pn, pnode *next, Value *v)
{
    do {
	switch (pn->symbol) {
	case PN_STRING:
	    --v;
	    PUT_STRVAL(v, pn->u.str);
	    break;

	case PN_ARRAY:
	    v -= pn->len;
	    i_copy(v, d_get_elts(pn->u.arr), (unsigned int) pn->len);
	    break;

	case PN_BRANCH:
	    --v;
	    PUT_ARRVAL(v, pn->u.arr);
	    break;

	case PN_RULE:
	    if (pn->list != (pnode *) NULL) {
		pn = pn->list;
		continue;
	    }
	    break;
	}

	pn = pn->next;
    } while (pn != next);
}

/*
 * NAME:	parser->traverse()
 * DESCRIPTION:	traverse the parse tree, returning the size
 */
static Int ps_traverse(parser *ps, pnode *pn, pnode *next)
{
    Int n;
    pnode *sub;
    Uint len, i;
    Value *v;
    Array *a;
    bool call;

    a = NULL;
    v = NULL;

    if (pn->symbol >= 0) {
	/*
	 * node hasn't been traversed before
	 */
	if (pn->symbol < ps->ntoken) {
	    /*
	     * token
	     */
	    pn->u.str = String::create(pn->u.text, pn->len);
	    sc_add(&ps->strc, pn->u.str);

	    pn->symbol = PN_STRING;
	    return pn->len = 1;
	} else if (pn->u.text != (char *) NULL) {
	    /*
	     * production rule
	     */
	    len = 0;
	    if (pn->len != 0) {
		pnode *trav;

		pn->symbol = PN_BLOCKED;

		/* traverse subtrees in left-to-right order */
		trav = (pnode *) NULL;
		for (i = pn->len, sub = pn->list; i != 0; --i, sub = sub->next)
		{
		    sub->trav = trav;
		    trav = sub;
		}
		for (i = pn->len; i != 0; --i) {
		    n = ps_traverse(ps, trav, trav->next);
		    if (n < 0) {
			return n;	/* blocked branch */
		    }
		    len += n;
		    trav = trav->trav;
		}

		pn->symbol = PN_RULE;
	    }

	    n = UCHAR(pn->u.text[0]) << 1;
	    if (n == UCHAR(pn->u.text[1])) {
		/* no ?func */
		pn->len = len;
	    } else {
		/*
		 * call LPC function to process subtree
		 */
		a = Array::create(ps->data, len);
		if (len != 0) {
		    ps_flatten(pn, next, a->elts + len);
		    d_ref_imports(a);
		}
		ps->data->parser = (parser *) NULL;

		try {
		    ec_push((ec_ftn) NULL);
		    PUSH_ARRVAL(ps->frame, a);
		    call = i_call(ps->frame, OBJR(ps->frame->oindex),
				  (Array *) NULL, pn->u.text + 2 + n,
				  UCHAR(pn->u.text[1]) - n - 1, TRUE, 1);
		    ec_pop();
		} catch (...) {
		    /* error: restore original parser */
		    if (ps->data->parser != (parser *) NULL) {
			ps_del(ps->data->parser);
		    }
		    ps->data->parser = ps;
		    error((char *) NULL);	/* pass on error */
		}

		/* restore original parser */
		if (ps->data->parser != (parser *) NULL) {
		    ps_del(ps->data->parser);
		}
		ps->data->parser = ps;

		pn->symbol = PN_BLOCKED;
		if (!call) {
		    return -1;	/* no function: block branch */
		}
		if (ps->frame->sp->type != T_ARRAY) {
		    /*
		     * wrong return type: block branch
		     */
		    i_del_value(ps->frame->sp++);
		    return -1;
		}

		pn->symbol = PN_ARRAY;
		ac_add(&ps->arrc, pn->u.arr = (ps->frame->sp++)->u.array);
		pn->u.arr->del();
		pn->len = pn->u.arr->size;
	    }
	    return pn->len;
	} else {
	    /*
	     * branches
	     */
	    pn->symbol = PN_BLOCKED;

	    /* pass 1: count branches */
	    n = 0;
	    for (sub = pn->list; sub != (pnode *) NULL; sub = sub->next) {
		if (n < ps->maxalt && ps_traverse(ps, sub, next) >= 0) {
		    n++;
		} else {
		    sub->symbol = PN_BLOCKED;
		}
	    }
	    if (n == 0) {
		return -1;	/* no unblocked branches */
	    }

	    /* pass 2: create branch arrays */
	    if (n != 1) {
		ac_add(&ps->arrc, a = Array::create(ps->data, n));
		v = a->elts;
		memset(v, '\0', n * sizeof(Value));
	    }
	    for (sub = pn->list, i = n; i != 0; sub = sub->next) {
		if (sub->symbol != PN_BLOCKED) {
		    if (n == 1) {
			/* sole branch */
			sub->next = pn->next;
			sub->trav = pn->trav;
			*pn = *sub;
			return pn->len;
		    } else {
			if (sub->symbol == PN_ARRAY) {
			    PUT_ARRVAL(v, sub->u.arr);
			} else {
			    PUT_ARRVAL(v, Array::create(ps->data, sub->len));
			    if (sub->len != 0) {
				ps_flatten(sub, next,
					   v->u.array->elts + sub->len);
				d_ref_imports(v->u.array);
			    }
			}
			v++;
			--i;
		    }
		}
	    }
	    pn->symbol = PN_BRANCH;
	    pn->u.arr = a;
	    return pn->len = 1;
	}
    } else {
	/*
	 * node has been traversed before
	 */
	if (pn->symbol == PN_BLOCKED) {
	    return -1;
	}
	return pn->len;
    }
}

/*
 * NAME:	parser->load()
 * DESCRIPTION:	load parse_string data
 */
static parser *ps_load(Frame *f, Value *elts)
{
    parser *ps;
    char *p;
    short i;
    Uint len;
    short fasize, lrsize;

    ps = ALLOC(parser, 1);
    ps->frame = f;
    ps->data = f->data;
    ps->data->parser = ps;
    fasize = elts->u.number >> 16;
    lrsize = (elts++)->u.number & 0xffff;
    ps->source = (elts++)->u.string;
    ps->source->ref();
    ps->grammar = (elts++)->u.string;
    ps->grammar->ref();

    if (fasize > 1) {
	for (i = fasize, len = 0; --i >= 0; ) {
	    len += elts[i].u.string->len;
	}
	p = ps->fastr = ALLOC(char, len);
	for (i = fasize; --i >= 0; ) {
	    memcpy(p, elts->u.string->text, elts->u.string->len);
	    p += (elts++)->u.string->len;
	}
	p -= len;
    } else {
	p = elts->u.string->text;
	len = (elts++)->u.string->len;
	ps->fastr = (char *) NULL;
    }
    ps->fa = dfa_load(ps->source->text, ps->grammar->text, p, len);

    if (lrsize > 1) {
	for (i = lrsize, len = 0; --i >= 0; ) {
	    len += elts[i].u.string->len;
	}
	p = ps->lrstr = ALLOC(char, len);
	for (i = lrsize; --i >= 0; ) {
	    memcpy(p, elts->u.string->text, elts->u.string->len);
	    p += (elts++)->u.string->len;
	}
	p -= len;
    } else {
	p = elts->u.string->text;
	len = elts->u.string->len;
	ps->lrstr = (char *) NULL;
    }
    ps->lr = srp_load(ps->grammar->text, p, len);

    ps->pnc = (pnchunk *) NULL;
    ps->list.snc = (snchunk *) NULL;
    ps->list.first = (snode *) NULL;

    ps->strc = (strpchunk *) NULL;
    ps->arrc = (arrpchunk *) NULL;

    p = ps->grammar->text;
    ps->ntoken = ((UCHAR(p[5]) + UCHAR(p[9]) + UCHAR(p[11])) << 8) +
		 UCHAR(p[6]) + UCHAR(p[10]) + UCHAR(p[12]);
    ps->nprod = (UCHAR(p[13]) << 8) + UCHAR(p[14]);

    return ps;
}

/*
 * NAME:	parser->save()
 * DESCRIPTION:	save parse_string data
 */
void ps_save(parser *ps)
{
    Value *v;
    Dataspace *data;
    Uint len;
    Value val;
    short fasize, lrsize;
    char *fastr, *lrstr;
    Uint falen, lrlen;
    bool save;

    save = dfa_save(ps->fa, &fastr, &falen) | srp_save(ps->lr, &lrstr, &lrlen);

    if (save) {
	data = ps->data;
	fasize = 1 + (falen - 1) / USHRT_MAX;
	lrsize = 1 + (lrlen - 1) / USHRT_MAX;
	PUT_ARRVAL_NOREF(&val, Array::create(data, 3L + fasize + lrsize));

	/* grammar */
	v = val.u.array->elts;
	PUT_INTVAL(v, ((Int) fasize << 16) + lrsize);
	v++;
	PUT_STRVAL(v, ps->source);
	v++;
	PUT_STRVAL(v, ps->grammar);
	v++;

	/* dfa */
	if (ps->fastr != (char *) NULL && fastr != ps->fastr) {
	    FREE(ps->fastr);
	    ps->fastr = (char *) NULL;
	}
	do {
	    len = (falen > USHRT_MAX) ? USHRT_MAX : falen;
	    PUT_STRVAL(v, String::create(fastr, len));
	    v++;
	    fastr += len;
	    falen -= len;
	} while (falen != 0);

	/* srp */
	if (ps->lrstr != (char *) NULL && lrstr != ps->lrstr) {
	    FREE(ps->lrstr);
	    ps->lrstr = (char *) NULL;
	}
	do {
	    len = (lrlen > USHRT_MAX) ? USHRT_MAX : lrlen;
	    PUT_STRVAL(v, String::create(lrstr, len));
	    v++;
	    lrstr += len;
	    lrlen -= len;
	} while (lrlen != 0);

	d_set_extravar(data, &val);
    }
}

/*
 * NAME:	parse_string()
 * DESCRIPTION:	parse a string
 */
Array *ps_parse_string(Frame *f, String *source, String *str, Int maxalt)
{
    Dataspace *data;
    parser *ps;
    Value *val;
    bool same, toobig;
    pnode *pn;
    Array *a;
    Int len;

    /*
     * create or load parser
     */
    data = f->data;
    if (data->parser != (parser *) NULL) {
	ps = data->parser;
	ps->frame = f;
	same = (ps->source->cmp(source) == 0);
    } else {
	val = d_get_extravar(data);
	if (val->type == T_ARRAY && d_get_elts(val->u.array)->type == T_INT &&
	    val->u.array->elts[1].u.string->cmp(source) == 0 &&
	    val->u.array->elts[2].u.string->text[0] == GRAM_VERSION) {
	    ps = ps_load(f, val->u.array->elts);
	    same = TRUE;
	} else {
	    ps = (parser *) NULL;
	    same = FALSE;
	}
    }
    if (!same) {
	/* new parser */
	if (ps != (parser *) NULL) {
	    ps_del(ps);
	}
	ps = ps_new(f, source, parse_grammar(source));
    }

    /*
     * parse string
     */
    a = (Array *) NULL;
    ps->maxalt = maxalt;
    try {
	ec_push((ec_ftn) NULL);
	/*
	 * do the parse thing
	 */
	i_add_ticks(ps->frame, 400);
	toobig = FALSE;
	pn = ps_parse(ps, str, &toobig);
	sn_clear(&ps->list);

	/*
	 * put result in array
	 */
	if (pn != (pnode *) NULL) {
	    /*
	     * valid parse tree was created
	     */
	    len = ps_traverse(ps, pn, pn->next);
	    if (len >= 0) {
		a = Array::create(data, len);
		ps_flatten(pn, pn->next, a->elts + len);
		d_ref_imports(a);
	    }

	    /* clean up */
	    delete ps->strc;
	    ps->strc = (strpchunk *) NULL;
	    delete ps->arrc;
	    ps->arrc = (arrpchunk *) NULL;
	} else if (toobig) {
	    /*
	     * lexer or parser has become too big
	     */
	    error("Grammar too large");
	}
	delete ps->pnc;
	ps->pnc = (pnchunk *) NULL;

	ec_pop();
    } catch (...) {
	/*
	 * error occurred; clean up
	 */
	sn_clear(&ps->list);
	delete ps->pnc;
	ps->pnc = (pnchunk *) NULL;

	delete ps->strc;
	ps->strc = (strpchunk *) NULL;
	delete ps->arrc;
	ps->arrc = (arrpchunk *) NULL;

	error((char *) NULL);	/* pass on error */
    }

    return a;
}
