/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2025 DGD Authors (see the commit log for details)
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
# include "xfloat.h"
# include "data.h"
# include "interpret.h"
# include "grammar.h"
# include "dfa.h"
# include "srp.h"
# include "parse.h"

class PNode : public ChunkAllocated {
public:
    static PNode *create(class PnChunk **c, short symb, unsigned short state,
			 char *text, ssizet len, PNode *next, PNode *list);

    short symbol;		/* node symbol */
    unsigned short state;	/* state reached after this symbol */
    Uint len;			/* token/reduction length or subtree size */
    union {
	char *text;		/* token/reduction text */
	String *str;		/* token string */
	Array *arr;		/* rule array */
    };
    PNode *next;		/* next in linked list */
    PNode *list;		/* list of nodes for reduction */
    PNode *trav;		/* traverse list */
};

# define PNCHUNKSZ	256

class PnChunk : public Chunk<PNode, PNCHUNKSZ> {
};

/*
 * create a new pnode
 */
PNode *PNode::create(PnChunk **c, short symb, unsigned short state, char *text,
		     ssizet len, PNode *next, PNode *list)
{
    PNode *pn;

    if (*c == (PnChunk *) NULL) {
	*c = new PnChunk;
    }
    pn = chunknew (**c) PNode;

    pn->symbol = symb;
    pn->state = state;
    pn->len = len;
    pn->text = text;
    pn->next = next;
    pn->list = list;

    return pn;
}

struct PState {
    SNode *first;		/* first in list */
    SNode *last;		/* last in list */
};

class SNode : public ChunkAllocated {
public:
    void add(SnList *list, PNode *pn, PState *state);

    static void create(SnList *list, PNode *pn, PState *state);
    static void clear(SnList *list);

    PNode *pn;			/* pnode */
    SNode *next;		/* next to be treated */
    SNode *slist;		/* per-state list */
};

# define SNCHUNKSZ	32

class SnChunk : public Chunk<SNode, SNCHUNKSZ> {
};

/*
 * create a new snode
 */
void SNode::create(SnList *list, PNode *pn, PState *state)
{
    SNode *sn;

    if (list->snc == (SnChunk *) NULL) {
	list->snc = new SnChunk;
    }
    sn = chunknew (*list->snc) SNode;
    if (list->first == (SNode *) NULL) {
	list->first = list->last = sn;
    } else {
	list->last->next = sn;
	list->last = sn;
    }

    sn->pn = pn;
    sn->next = (SNode *) NULL;
    if (state->first == (SNode *) NULL) {
	state->first = state->last = sn;
    } else {
	state->last->slist = sn;
	state->last = sn;
    }
    sn->slist = (SNode *) NULL;
}

/*
 * add an existing snode to a list
 */
void SNode::add(SnList *list, PNode *pn, PState *state)
{
    if (list->first == (SNode *) NULL) {
	list->first = list->last = this;
    } else {
	list->last->next = this;
	list->last = this;
    }

    this->pn = pn;
    next = (SNode *) NULL;
    if (state->first == (SNode *) NULL) {
	state->first = state->last = this;
    } else {
	state->last->slist = this;
	state->last = this;
    }
    slist = (SNode *) NULL;
}

/*
 * free all snodes in memory
 */
void SNode::clear(SnList *list)
{
    if (list->snc != (SnChunk *) NULL) {
	list->snc->clean();
	delete list->snc;
	list->snc = (SnChunk *) NULL;
	list->first = (SNode *) NULL;
    }
}


# define STRCHUNKSZ	256

struct StrPtr : public ChunkAllocated {
    String *str;
};

class StrPChunk : public Chunk<StrPtr, STRCHUNKSZ> {
public:
    /*
     * iterate through items from destructor
     */
    virtual ~StrPChunk() {
	items();
    }

    /*
     * dereference strings when iterating through items
     */
    virtual bool item(StrPtr *str) {
	str->str->del();
	return TRUE;
    }

    /*
     * add a string to the current chunk
     */
    static void add(StrPChunk **c, String *str) {
	if (*c == (StrPChunk *) NULL) {
	    *c = new StrPChunk;
	}

	(chunknew (**c) StrPtr)->str = str;
	str->ref();
    }
};


# define ARRCHUNKSZ	256

struct ArrPtr : public ChunkAllocated {
    Array *arr;
};

class ArrPChunk : public Chunk<ArrPtr, ARRCHUNKSZ> {
public:
    /*
     * iterate through items from destructor
     */
    virtual ~ArrPChunk() {
	items();
    }

    /*
     * dereference arrays when iterating through items
     */
    virtual bool item(ArrPtr *arr) {
	arr->arr->del();
	return TRUE;
    }

    /*
     * add an array to the current chunk
     */
    static void add(ArrPChunk **c, Array *arr)
    {
	if (*c == (ArrPChunk *) NULL) {
	    *c = new ArrPChunk;
	}

	(chunknew (**c) ArrPtr)->arr = arr;
	arr->ref();
    }
};


/*
 * create a new parser instance
 */
Parser *Parser::create(Frame *f, String *source, String *grammar)
{
    Parser *ps;
    char *p;

    ps = new Parser;
    ps->frame = f;
    ps->data = f->data;
    ps->data->parser = ps;
    ps->source = source;
    ps->source->ref();
    ps->grammar = grammar;
    ps->grammar->ref();
    ps->fastr = (char *) NULL;
    ps->lrstr = (char *) NULL;
    ps->fa = Dfa::create(source->text, grammar->text);
    ps->lr = Srp::create(grammar->text);

    ps->pnc = (PnChunk *) NULL;
    ps->list.snc = (SnChunk *) NULL;
    ps->list.first = (SNode *) NULL;

    ps->strc = (StrPChunk *) NULL;
    ps->arrc = (ArrPChunk *) NULL;

    p = grammar->text;
    ps->ntoken = ((UCHAR(p[5]) + UCHAR(p[9]) + UCHAR(p[11])) << 8) +
		 UCHAR(p[6]) + UCHAR(p[10]) + UCHAR(p[12]);
    ps->nprod = (UCHAR(p[13]) << 8) + UCHAR(p[14]);

    return ps;
}

/*
 * delete parser
 */
Parser::~Parser()
{
    data->parser = (Parser *) NULL;
    source->del();
    grammar->del();
    if (fastr != (char *) NULL) {
	FREE(fastr);
    }
    if (lrstr != (char *) NULL) {
	FREE(lrstr);
    }
    delete fa;
    delete lr;
}

/*
 * reset a parser that has grown too large
 */
void Parser::reset()
{
    if (fastr != (char *) NULL) {
	FREE(fastr);
	fastr = (char *) NULL;
    }
    if (lrstr != (char *) NULL) {
	FREE(lrstr);
	lrstr = (char *) NULL;
    }
    delete fa;
    delete lr;
    fa = Dfa::create(source->text, grammar->text);
    lr = Srp::create(grammar->text);
}

/*
 * perform a reduction
 */
void Parser::reduce(PNode *pn, char *p)
{
    SNode *sn;
    PNode *next;
    unsigned short n;
    short symb;
    char *red;
    ssizet len;

    /*
     * get rule to reduce by
     */
    red = grammar->text + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
    p += 2;
    symb = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
    len = UCHAR(red[0]);

    /*
     * create reduce node
     */
    next = pn;
    if (len == 0) {
	pn = (PNode *) NULL;
    } else {
	n = len;
	do {
	    next = next->next;
	} while (--n != 0);
    }
    n = lr->_goto(next->state, symb);
    pn = PNode::create(&pnc, symb, n, red, len, next, pn);

    /*
     * see if this reduction can be merged with another
     */
    frame->addTicks(2);
    for (sn = states[n].first; sn != (SNode *) NULL; sn = sn->slist) {
	PNode **ppn;

	if (sn->pn->symbol == symb && sn->pn->next == next) {
	    if (sn->pn->text != (char *) NULL) {
		/* first alternative */
		sn->pn->list = PNode::create(&pnc, symb, n, sn->pn->text,
					     sn->pn->len, (PNode *) NULL,
					     sn->pn->list);
		sn->pn->text = (char *) NULL;
		sn->pn->len = 1;
	    }

	    /* add alternative */
	    for (ppn = &sn->pn->list;
		 *ppn != (PNode *) NULL && (*ppn)->text < red;
		 ppn = &(*ppn)->next) ;
	    sn->pn->len++;

	    pn->next = *ppn;
	    *ppn = pn;
	    return;
	}
	frame->addTicks(1);
    }

    /*
     * new reduction
     */
    SNode::create(&list, pn, &states[n]);
}

/*
 * perform a shift
 */
void Parser::shift(SNode *sn, short token, char *text, ssizet len)
{
    int n;

    n = lr->shift(sn->pn->state, token);
    if (n >= 0) {
	/* shift works: add new snode */
	sn->add(&list, PNode::create(&pnc, token, n, text, len, sn->pn,
				     (PNode *) NULL),
		&states[n]);
	return;
    }

    /* no shift: add node to free list */
    delete sn;
}

/*
 * parse a string, return a parse tangle
 */
PNode *Parser::parse(String *str, bool *toobig)
{
    SNode *sn;
    short n;
    SNode *next;
    char *ttext;
    ssizet size, tlen;
    unsigned short nred;
    char *red;

    /* initialize */
    size = str->len;
    nstates = lr->reduce(0, &nred, &red);
    if (nstates < nprod) {
	nstates = nprod;
    }
    states = ALLOC(PState, nstates);
    memset(states, '\0', nstates * sizeof(PState));
    list.first = (SNode *) NULL;

    /* state 0 */
    SNode::create(&list, PNode::create(&pnc, 0, 0, (char *) NULL, (ssizet) 0,
				       (PNode *) NULL, (PNode *) NULL),
		  &states[0]);

    do {
	/*
	 * apply reductions for current states, expanding states if needed
	 */
	for (sn = list.first; sn != (SNode *) NULL; sn = sn->next) {
	    n = lr->reduce(sn->pn->state, &nred, &red);
	    if (n < 0) {
		/* parser grown to big */
		FREE(states);
		*toobig = TRUE;
		return (PNode *) NULL;
	    }
	    if (n > nstates) {
		unsigned short stsize;

		/* grow tables */
		stsize = n;
		stsize <<= 1;
		states = REALLOC(states, PState, nstates, stsize);
		memset(states + nstates, '\0',
		       (stsize - nstates) * sizeof(PState));
		nstates = stsize;
	    }
	    for (n = 0; n < nred; n++) {
		reduce(sn->pn, red);
		red += 4;
		if (frame->rlim->ticks < 0) {
		    if (frame->rlim->noticks) {
			frame->rlim->ticks = 0x7fffffff;
		    } else {
			FREE(states);
			EC->error("Out of ticks");
		    }
		}
	    }
	    frame->addTicks(1);
	}

	switch (n = fa->scan(str, &size, &ttext, &tlen)) {
	case DFA_EOS:
	    /* if end of string, return node from state 1 */
	    sn = states[1].first;
	    FREE(states);
	    return (sn != (SNode *) NULL) ? sn->pn : (PNode *) NULL;

	case DFA_REJECT:
	    /* bad token */
	    FREE(states);
	    EC->error("Bad token at offset %u", str->len - size);
	    return (PNode *) NULL;

	case DFA_TOOBIG:
	    FREE(states);
	    *toobig = TRUE;
	    return (PNode *) NULL;

	default:
	    /* shift */
	    memset(states, '\0', nstates * sizeof(PState));
	    sn = list.first;
	    list.first = (SNode *) NULL;
	    do {
		next = sn->next;
		shift(sn, n, ttext, tlen);
		sn = next;
	    } while (sn != (SNode *) NULL);
	}
    } while (list.first != (SNode *) NULL);

    /*
     * parsing failed
     */
    FREE(states);
    return (PNode *) NULL;
}


# define PN_STRING	-1	/* string node */
# define PN_ARRAY	-2	/* array node */
# define PN_RULE	-3	/* rule node */
# define PN_BRANCH	-4	/* branch node */
# define PN_BLOCKED	-5	/* blocked branch */

/*
 * traverse parse tree, collecting values in a flat array
 */
void Parser::flatten(PNode *pn, PNode *next, Value *v)
{
    do {
	switch (pn->symbol) {
	case PN_STRING:
	    --v;
	    PUT_STRVAL(v, pn->str);
	    break;

	case PN_ARRAY:
	    v -= pn->len;
	    Value::copy(v, Dataspace::elts(pn->arr), (unsigned int) pn->len);
	    break;

	case PN_BRANCH:
	    --v;
	    PUT_ARRVAL(v, pn->arr);
	    break;

	case PN_RULE:
	    if (pn->list != (PNode *) NULL) {
		pn = pn->list;
		continue;
	    }
	    break;
	}

	pn = pn->next;
    } while (pn != next);
}

/*
 * traverse the parse tree, returning the size
 */
Int Parser::traverse(PNode *pn, PNode *next)
{
    Int n;
    PNode *sub;
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
	if (pn->symbol < ntoken) {
	    /*
	     * token
	     */
	    pn->str = String::create(pn->text, pn->len);
	    StrPChunk::add(&strc, pn->str);

	    pn->symbol = PN_STRING;
	    return pn->len = 1;
	} else if (pn->text != (char *) NULL) {
	    /*
	     * production rule
	     */
	    len = 0;
	    if (pn->len != 0) {
		PNode *trav;

		pn->symbol = PN_BLOCKED;

		/* traverse subtrees in left-to-right order */
		trav = (PNode *) NULL;
		for (i = pn->len, sub = pn->list; i != 0; --i, sub = sub->next)
		{
		    sub->trav = trav;
		    trav = sub;
		}
		for (i = pn->len; i != 0; --i) {
		    n = traverse(trav, trav->next);
		    if (n < 0) {
			return n;	/* blocked branch */
		    }
		    len += n;
		    trav = trav->trav;
		}

		pn->symbol = PN_RULE;
	    }

	    n = UCHAR(pn->text[0]) << 1;
	    if (n == UCHAR(pn->text[1])) {
		/* no ?func */
		pn->len = len;
	    } else {
		/*
		 * call LPC function to process subtree
		 */
		a = Array::create(data, len);
		if (len != 0) {
		    flatten(pn, next, a->elts + len);
		    Dataspace::refImports(a);
		}
		data->parser = (Parser *) NULL;

		try {
		    EC->push();
		    PUSH_ARRVAL(frame, a);
		    call = frame->call(OBJR(frame->oindex),
				       (LWO *) NULL, pn->text + 2 + n,
				       UCHAR(pn->text[1]) - n - 1, TRUE, 1);
		    EC->pop();
		} catch (const char*) {
		    /* error: restore original parser */
		    if (data->parser != (Parser *) NULL) {
			delete data->parser;
		    }
		    data->parser = this;
		    EC->error((char *) NULL);	/* pass on error */
		}

		/* restore original parser */
		if (data->parser != (Parser *) NULL) {
		    delete data->parser;
		}
		data->parser = this;

		pn->symbol = PN_BLOCKED;
		if (!call) {
		    return -1;	/* no function: block branch */
		}
		if (frame->sp->type != T_ARRAY) {
		    /*
		     * wrong return type: block branch
		     */
		    (frame->sp++)->del();
		    return -1;
		}

		pn->symbol = PN_ARRAY;
		ArrPChunk::add(&arrc, pn->arr = (frame->sp++)->array);
		pn->arr->del();
		pn->len = pn->arr->size;
	    }
	    return pn->len;
	} else {
	    /*
	     * branches
	     */
	    pn->symbol = PN_BLOCKED;

	    /* pass 1: count branches */
	    n = 0;
	    for (sub = pn->list; sub != (PNode *) NULL; sub = sub->next) {
		if (n < maxalt && traverse(sub, next) >= 0) {
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
		ArrPChunk::add(&arrc, a = Array::create(data, n));
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
			    PUT_ARRVAL(v, sub->arr);
			} else {
			    PUT_ARRVAL(v, Array::create(data, sub->len));
			    if (sub->len != 0) {
				flatten(sub, next, v->array->elts + sub->len);
				Dataspace::refImports(v->array);
			    }
			}
			v++;
			--i;
		    }
		}
	    }
	    pn->symbol = PN_BRANCH;
	    pn->arr = a;
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
 * load parse_string data
 */
Parser *Parser::load(Frame *f, Value *elts)
{
    Parser *ps;
    char *p;
    short i;
    Uint len;
    short fasize, lrsize;

    ps = new Parser;
    ps->frame = f;
    ps->data = f->data;
    ps->data->parser = ps;
    fasize = elts->number >> 16;
    lrsize = (elts++)->number & 0xffff;
    ps->source = (elts++)->string;
    ps->source->ref();
    ps->grammar = (elts++)->string;
    ps->grammar->ref();

    if (fasize > 1) {
	for (i = fasize, len = 0; --i >= 0; ) {
	    len += elts[i].string->len;
	}
	p = ps->fastr = ALLOC(char, len);
	for (i = fasize; --i >= 0; ) {
	    memcpy(p, elts->string->text, elts->string->len);
	    p += (elts++)->string->len;
	}
	p -= len;
    } else {
	p = elts->string->text;
	len = (elts++)->string->len;
	ps->fastr = (char *) NULL;
    }
    ps->fa = Dfa::load(ps->source->text, ps->grammar->text, p, len);

    if (lrsize > 1) {
	for (i = lrsize, len = 0; --i >= 0; ) {
	    len += elts[i].string->len;
	}
	p = ps->lrstr = ALLOC(char, len);
	for (i = lrsize; --i >= 0; ) {
	    memcpy(p, elts->string->text, elts->string->len);
	    p += (elts++)->string->len;
	}
	p -= len;
    } else {
	p = elts->string->text;
	len = elts->string->len;
	ps->lrstr = (char *) NULL;
    }
    ps->lr = Srp::load(ps->grammar->text, p, len);

    ps->pnc = (PnChunk *) NULL;
    ps->list.snc = (SnChunk *) NULL;
    ps->list.first = (SNode *) NULL;

    ps->strc = (StrPChunk *) NULL;
    ps->arrc = (ArrPChunk *) NULL;

    p = ps->grammar->text;
    ps->ntoken = ((UCHAR(p[5]) + UCHAR(p[9]) + UCHAR(p[11])) << 8) +
		 UCHAR(p[6]) + UCHAR(p[10]) + UCHAR(p[12]);
    ps->nprod = (UCHAR(p[13]) << 8) + UCHAR(p[14]);

    return ps;
}

/*
 * save parse_string data
 */
void Parser::save()
{
    Value *v;
    Uint len;
    Value val;
    short fasize, lrsize;
    char *fastr, *lrstr;
    Uint falen, lrlen;
    bool save;

    save = fa->save(&fastr, &falen) | lr->save(&lrstr, &lrlen);

    if (save) {
	fasize = 1 + (falen - 1) / USHRT_MAX;
	lrsize = 1 + (lrlen - 1) / USHRT_MAX;
	PUT_ARRVAL_NOREF(&val, Array::create(data, 3L + fasize + lrsize));

	/* grammar */
	v = val.array->elts;
	PUT_INTVAL(v, ((LPCint) fasize << 16) + lrsize);
	v++;
	PUT_STRVAL(v, source);
	v++;
	PUT_STRVAL(v, grammar);
	v++;

	/* dfa */
	if (this->fastr != (char *) NULL && fastr != this->fastr) {
	    FREE(this->fastr);
	    this->fastr = (char *) NULL;
	}
	do {
	    len = (falen > USHRT_MAX) ? USHRT_MAX : falen;
	    PUT_STRVAL(v, String::create(fastr, len));
	    v++;
	    fastr += len;
	    falen -= len;
	} while (falen != 0);

	/* srp */
	if (this->lrstr != (char *) NULL && lrstr != this->lrstr) {
	    FREE(this->lrstr);
	    this->lrstr = (char *) NULL;
	}
	do {
	    len = (lrlen > USHRT_MAX) ? USHRT_MAX : lrlen;
	    PUT_STRVAL(v, String::create(lrstr, len));
	    v++;
	    lrstr += len;
	    lrlen -= len;
	} while (lrlen != 0);

	Dataspace::setExtra(data, &val);
    }
}

/*
 * parse a string
 */
Array *Parser::parse_string(Frame *f, String *source, String *str,
			    LPCint maxalt)
{
    Dataspace *data;
    Parser *ps;
    Value *val;
    bool same, toobig;
    PNode *pn;
    Array *a;
    Int len;

    /*
     * create or load parser
     */
    data = f->data;
    if (data->parser != (Parser *) NULL) {
	ps = data->parser;
	ps->frame = f;
	same = (ps->source->cmp(source) == 0);
    } else {
	val = Dataspace::extra(data);
	if (val->type == T_ARRAY &&
	    Dataspace::elts(val->array)->type == T_INT &&
	    val->array->elts[1].string->cmp(source) == 0 &&
	    val->array->elts[2].string->text[0] == GRAM_VERSION) {
	    ps = load(f, val->array->elts);
	    same = TRUE;
	} else {
	    ps = (Parser *) NULL;
	    same = FALSE;
	}
    }
    if (!same) {
	/* new parser */
	if (ps != (Parser *) NULL) {
	    delete ps;
	}
	ps = create(f, source, Grammar::parse(source));
    }

    /*
     * parse string
     */
    a = (Array *) NULL;
    ps->maxalt = maxalt;
    try {
	EC->push();
	/*
	 * do the parse thing
	 */
	ps->frame->addTicks(400);
	toobig = FALSE;
	pn = ps->parse(str, &toobig);
	SNode::clear(&ps->list);

	/*
	 * put result in array
	 */
	if (pn != (PNode *) NULL) {
	    /*
	     * valid parse tree was created
	     */
	    len = ps->traverse(pn, pn->next);
	    if (len >= 0) {
		a = Array::create(data, len);
		flatten(pn, pn->next, a->elts + len);
		Dataspace::refImports(a);
	    }

	    /* clean up */
	    delete ps->strc;
	    ps->strc = (StrPChunk *) NULL;
	    delete ps->arrc;
	    ps->arrc = (ArrPChunk *) NULL;
	} else if (toobig) {
	    /*
	     * lexer or parser has become too big
	     */
	    ps->reset();
	    EC->error("Grammar too large");
	}
	delete ps->pnc;
	ps->pnc = (PnChunk *) NULL;

	EC->pop();
    } catch (const char*) {
	/*
	 * error occurred; clean up
	 */
	SNode::clear(&ps->list);
	delete ps->pnc;
	ps->pnc = (PnChunk *) NULL;

	delete ps->strc;
	ps->strc = (StrPChunk *) NULL;
	delete ps->arrc;
	ps->arrc = (ArrPChunk *) NULL;

	EC->error((char *) NULL);	/* pass on error */
    }

    return a;
}
