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

typedef struct _pnode_ {
    short symbol;		/* node symbol */
    unsigned short state;	/* state reached after this symbol */
    Uint len;			/* token/reduction length or subtree size */
    union {
	char *text;		/* token/reduction text */
	string *str;		/* token string */
	array *arr;		/* rule array */
    } u;
    struct _pnode_ *next;	/* next in linked list */
    struct _pnode_ *list;	/* list of nodes for reduction */
} pnode;

# define PNCHUNKSZ	256

typedef struct _pnchunk_ {
    int chunksz;		/* size of this chunk */
    struct _pnchunk_ *next;	/* next in linked list */
    pnode pn[PNCHUNKSZ];	/* chunk of pnodes */
} pnchunk;

/*
 * NAME:	pnode->new()
 * DESCRIPTION:	create a new pnode
 */
static pnode *pn_new(c, symb, state, text, len, next, list)
register pnchunk **c;
short symb;
unsigned short state;
char *text;
ssizet len;
pnode *next, *list;
{
    register pnode *pn;

    if (*c == (pnchunk *) NULL || (*c)->chunksz == PNCHUNKSZ) {
	register pnchunk *x;

	x = ALLOC(pnchunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }
    pn = &(*c)->pn[(*c)->chunksz++];

    pn->symbol = symb;
    pn->state = state;
    pn->len = len;
    pn->u.text = text;
    pn->next = next;
    pn->list = list;

    return pn;
}

/*
 * NAME:	pnode->clear()
 * DESCRIPTION:	free all pnodes in memory
 */
static void pn_clear(c)
register pnchunk *c;
{
    register pnchunk *f;

    while (c != (pnchunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }
}

typedef struct _snode_ {
    pnode *pn;			/* pnode */
    struct _snode_ *next;	/* next to be treated */
    struct _snode_ *slist;	/* per-state list */
} snode;

# define SNCHUNKSZ	32

typedef struct _snchunk_ {
    int chunksz;		/* size of this chunk */
    struct _snchunk_ *next;	/* next in linked list */
    snode sn[SNCHUNKSZ];	/* chunk of snodes */
} snchunk;

typedef struct {
    snchunk *snc;		/* snode chunk */
    snode *first;		/* first node in list */
    snode *last;		/* last node in list */
    snode *free;		/* first node in free list */
} snlist;

/*
 * NAME:	snode->new()
 * DESCRIPTION:	create a new snode
 */
static snode *sn_new(list, pn, slist)
register snlist *list;
pnode *pn;
snode *slist;
{
    register snode *sn;

    if (list->free != (snode *) NULL) {
	sn = list->free;
	list->free = sn->next;
    } else {
	if (list->snc == (snchunk *) NULL || list->snc->chunksz == SNCHUNKSZ) {
	    register snchunk *x;

	    x = ALLOC(snchunk, 1);
	    x->next = list->snc;
	    list->snc = x;
	    x->chunksz = 0;
	}
	sn = &list->snc->sn[list->snc->chunksz++];
    }
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
static snode *sn_add(list, sn, pn, slist)
register snlist *list;
register snode *sn;
pnode *pn;
snode *slist;
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
 * NAME:	snode->del()
 * DESCRIPTION:	put an existing node in the free list
 */
static void sn_del(list, sn)
snlist *list;
snode *sn;
{
    sn->next = list->free;
    list->free = sn;
}

/*
 * NAME:	snode->clear()
 * DESCRIPTION:	free all snodes in memory
 */
static void sn_clear(list)
register snlist *list;
{
    register snchunk *c, *f;

    c = list->snc;
    while (c != (snchunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }
    list->snc = (snchunk *) NULL;
    list->first = list->free = (snode *) NULL;
}


# define STRCHUNKSZ	256

typedef struct _strchunk_ {
    int chunksz;		/* size of chunk */
    struct _strchunk_ *next;	/* next in linked list */
    string *str[STRCHUNKSZ];	/* strings */
} strchunk;

/*
 * NAME:	strchunk->add()
 * DESCRIPTION:	add a string to the current chunk
 */
static void sc_add(c, str)
register strchunk **c;
string *str;
{
    if (*c == (strchunk *) NULL || (*c)->chunksz == STRCHUNKSZ) {
	strchunk *x;

	x = ALLOC(strchunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }

    str_ref((*c)->str[(*c)->chunksz++] = str);
}

/*
 * NAME:	strchunk->clean()
 * DESCRIPTION:	remove string chunks, and strings, from memory
 */
static void sc_clean(c)
register strchunk *c;
{
    register strchunk *f;
    register int i;

    while (c != (strchunk *) NULL) {
	for (i = c->chunksz; --i >= 0; ) {
	    str_del(c->str[i]);
	}
	f = c;
	c = c->next;
	FREE(f);
    }
}


# define ARRCHUNKSZ	256

typedef struct _arrchunk_ {
    int chunksz;		/* size of chunk */
    struct _arrchunk_ *next;	/* next in linked list */
    array *arr[ARRCHUNKSZ];	/* arrays */
} arrchunk;

/*
 * NAME:	arrchunk->add()
 * DESCRIPTION:	add an array to the current chunk
 */
static void ac_add(c, arr)
arrchunk **c;
array *arr;
{
    if (*c == (arrchunk *) NULL || (*c)->chunksz == ARRCHUNKSZ) {
	arrchunk *x;

	x = ALLOC(arrchunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }

    arr_ref((*c)->arr[(*c)->chunksz++] = arr);
}

/*
 * NAME:	arrchunk->clean()
 * DESCRIPTION:	remove array chunks, and arrays, from memory
 */
static void ac_clean(c)
register arrchunk *c;
{
    register arrchunk *f;
    register int i;

    while (c != (arrchunk *) NULL) {
	for (i = c->chunksz; --i >= 0; ) {
	    arr_del(c->arr[i]);
	}
	f = c;
	c = c->next;
	FREE(f);
    }
}


struct _parser_ {
    frame *frame;		/* interpreter stack frame */
    dataspace *data;		/* dataspace for current object */

    string *source;		/* grammar source */
    string *grammar;		/* preprocessed grammar */
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

    strchunk *strc;		/* string chunk */
    arrchunk *arrc;		/* array chunk */

    Int maxalt;			/* max number of branches */
};

/*
 * NAME:	parser->new()
 * DESCRIPTION:	create a new parser instance
 */
static parser *ps_new(f, source, grammar)
frame *f;
string *source, *grammar;
{
    register parser *ps;
    register char *p;

    ps = ALLOC(parser, 1);
    ps->frame = f;
    ps->data = f->data;
    ps->data->parser = ps;
    str_ref(ps->source = source);
    str_ref(ps->grammar = grammar);
    ps->fastr = (char *) NULL;
    ps->lrstr = (char *) NULL;
    ps->fa = dfa_new(source->text, grammar->text);
    ps->lr = srp_new(grammar->text);

    ps->pnc = (pnchunk *) NULL;
    ps->list.snc = (snchunk *) NULL;
    ps->list.first = ps->list.free = (snode *) NULL;

    ps->strc = (strchunk *) NULL;
    ps->arrc = (arrchunk *) NULL;

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
void ps_del(ps)
register parser *ps;
{
    ps->data->parser = (parser *) NULL;
    str_del(ps->source);
    str_del(ps->grammar);
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
static void ps_reduce(ps, pn, p)
register parser *ps;
pnode *pn;
register char *p;
{
    register snode *sn;
    register pnode *next;
    register unsigned short n;
    register short symb;
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
	    register pnode **ppn;

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
static void ps_shift(ps, sn, token, text, len)
register parser *ps;
snode *sn;
short token;
char *text;
ssizet len;
{
    register int n;

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
    sn_del(&ps->list, sn);
}

/*
 * NAME:	parser->parse()
 * DESCRIPTION:	parse a string, return a parse tangle
 */
static pnode *ps_parse(ps, str, toobig)
register parser *ps;
string *str;
bool *toobig;
{
    register snode *sn;
    register short n;
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
static void ps_flatten(pn, next, v)
register pnode *pn, *next;
register value *v;
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
static Int ps_traverse(ps, pn, next)
register parser *ps;
register pnode *pn;
pnode *next;
{
    register Int n;
    register pnode *sub;
    register Uint len, i;
    register value *v;
    array *a;
    bool call;

    if (pn->symbol >= 0) {
	/*
	 * node hasn't been traversed before
	 */
	if (pn->symbol < ps->ntoken) {
	    /*
	     * token
	     */
	    pn->u.str = str_new(pn->u.text, (long) pn->len);
	    sc_add(&ps->strc, pn->u.str);

	    pn->symbol = PN_STRING;
	    return pn->len = 1;
	} else if (pn->u.text != (char *) NULL) {
	    /*
	     * production rule
	     */
	    len = 0;
	    if (pn->len != 0) {
		pnode *nodes[255];
		register pnode **list;

		pn->symbol = PN_BLOCKED;

		/* traverse subtrees in left-to-right order */
		list = nodes;
		for (i = pn->len, sub = pn->list; i != 0; --i, sub = sub->next)
		{
		    *list++ = sub;
		}
		for (i = pn->len; i != 0; --i) {
		    sub = *--list;
		    n = ps_traverse(ps, sub, sub->next);
		    if (n < 0) {
			return n;	/* blocked branch */
		    }
		    len += n;
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
		a = arr_new(ps->data, (long) len);
		if (len != 0) {
		    ps_flatten(pn, next, a->elts + len);
		    d_ref_imports(a);
		}
		ps->data->parser = (parser *) NULL;

		if (ec_push((ec_ftn) NULL)) {
		    /* error: restore original parser */
		    if (ps->data->parser != (parser *) NULL) {
			ps_del(ps->data->parser);
		    }
		    ps->data->parser = ps;
		    error((char *) NULL);	/* pass on error */
		} else {
		    PUSH_ARRVAL(ps->frame, a);
		    call = i_call(ps->frame, OBJR(ps->frame->oindex),
				  (array *) NULL, pn->u.text + 2 + n,
				  UCHAR(pn->u.text[1]) - n - 1, TRUE, 1);
		    ec_pop();
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
		arr_del(pn->u.arr);
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
		ac_add(&ps->arrc, a = arr_new(ps->data, (long) n));
		v = a->elts;
		memset(v, '\0', n * sizeof(value));
	    }
	    for (sub = pn->list, i = n; i != 0; sub = sub->next) {
		if (sub->symbol != PN_BLOCKED) {
		    if (n == 1) {
			/* sole branch */
			sub->next = pn->next;
			*pn = *sub;
			return pn->len;
		    } else {
			if (sub->symbol == PN_ARRAY) {
			    PUT_ARRVAL(v, sub->u.arr);
			} else {
			    PUT_ARRVAL(v, arr_new(ps->data, (long) sub->len));
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
static parser *ps_load(f, elts)
frame *f;
register value *elts;
{
    register parser *ps;
    register char *p;
    register short i;
    register Uint len;
    short fasize, lrsize;

    ps = ALLOC(parser, 1);
    ps->frame = f;
    ps->data = f->data;
    ps->data->parser = ps;
    fasize = elts->u.number >> 16;
    lrsize = (elts++)->u.number & 0xffff;
    str_ref(ps->source = (elts++)->u.string);
    str_ref(ps->grammar = (elts++)->u.string);

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
    ps->list.first = ps->list.free = (snode *) NULL;

    ps->strc = (strchunk *) NULL;
    ps->arrc = (arrchunk *) NULL;

    p = ps->grammar->text;
    ps->ntoken = ((UCHAR(p[5]) + UCHAR(p[9])) << 8) + UCHAR(p[6]) +
		 UCHAR(p[10]);
    ps->nprod = (UCHAR(p[11]) << 8) + UCHAR(p[12]);

    return ps;
}

/*
 * NAME:	parser->save()
 * DESCRIPTION:	save parse_string data
 */
void ps_save(ps)
register parser *ps;
{
    register value *v;
    register dataspace *data;
    register Uint len;
    value val;
    short fasize, lrsize;
    char *fastr, *lrstr;
    Uint falen, lrlen;
    bool save;

    save = dfa_save(ps->fa, &fastr, &falen) | srp_save(ps->lr, &lrstr, &lrlen);

    if (save) {
	data = ps->data;
	fasize = 1 + (falen - 1) / USHRT_MAX;
	lrsize = 1 + (lrlen - 1) / USHRT_MAX;
	PUT_ARRVAL_NOREF(&val, arr_new(data, 3L + fasize + lrsize));

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
	    PUT_STRVAL(v, str_new(fastr, (long) len));
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
	    PUT_STRVAL(v, str_new(lrstr, (long) len));
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
array *ps_parse_string(f, source, str, maxalt)
frame *f;
string *source;
string *str;
Int maxalt;
{
    register dataspace *data;
    register parser *ps;
    value *val;
    bool same, toobig;
    pnode *pn;
    array *a;
    Int len;

    /*
     * create or load parser
     */
    data = f->data;
    if (data->parser != (parser *) NULL) {
	ps = data->parser;
	ps->frame = f;
	same = (str_cmp(ps->source, source) == 0);
    } else {
	val = d_get_extravar(data);
	if (val->type == T_ARRAY && d_get_elts(val->u.array)->type == T_INT &&
	    str_cmp(val->u.array->elts[1].u.string, source) == 0 &&
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
    ps->maxalt = maxalt;
    if (ec_push((ec_ftn) NULL)) {
	/*
	 * error occurred; clean up
	 */
	sn_clear(&ps->list);
	pn_clear(ps->pnc);
	ps->pnc = (pnchunk *) NULL;

	sc_clean(ps->strc);
	ps->strc = (strchunk *) NULL;
	ac_clean(ps->arrc);
	ps->arrc = (arrchunk *) NULL;

	error((char *) NULL);	/* pass on error */
    } else {
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
	a = (array *) NULL;
	if (pn != (pnode *) NULL) {
	    /*
	     * valid parse tree was created
	     */
	    len = ps_traverse(ps, pn, pn->next);
	    if (len >= 0) {
		a = arr_new(data, (long) len);
		ps_flatten(pn, pn->next, a->elts + len);
		d_ref_imports(a);
	    }

	    /* clean up */
	    sc_clean(ps->strc);
	    ps->strc = (strchunk *) NULL;
	    ac_clean(ps->arrc);
	    ps->arrc = (arrchunk *) NULL;
	} else if (toobig) {
	    /*
	     * lexer or parser has become too big
	     */
	    ec_pop();
	    pn_clear(ps->pnc);
	    ps->data->parser = (parser *) NULL;
	    ps_del(ps);

	    error("Grammar too large");
	}
	pn_clear(ps->pnc);
	ps->pnc = (pnchunk *) NULL;

	ec_pop();

	return a;
    }
}
