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
    unsigned short symbol;	/* node symbol */
    unsigned short state;	/* state reached after this symbol */
    unsigned short len;		/* token/reduction length */
    union {
	char *text;		/* token/reduction text */
	string *str;		/* token string */
	array *arr;		/* rule array */
    } u;
    struct _pnode_ *next;	/* next in linked list */
    struct _pnode_ *list;	/* list of nodes for reduction */
} pnode;

# define PNCHUNKSZ	128

typedef struct _pnchunk_ {
    pnode pn[PNCHUNKSZ];	/* chunk of pnodes */
    int chunksz;		/* size of this chunk */
    struct _pnchunk_ *next;	/* next in linked list */
} pnchunk;

/*
 * NAME:	pnode->new()
 * DESCRIPTION:	create a new pnode
 */
static pnode *pn_new(c, symb, state, text, len, next, list)
register pnchunk **c;
unsigned short symb, state, len;
char *text;
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

# define SNCHUNKSZ	128

typedef struct _snchunk_ {
    snode sn[SNCHUNKSZ];	/* chunk of snodes */
    int chunksz;		/* size of this chunk */
    struct _snchunk_ *next;	/* next in linked list */
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
snlist *list;
{
    register snchunk *c, *f;

    c = list->snc;
    while (c != (snchunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }
}


# define STRCHUNKSZ	128

typedef struct _strchunk_ {
    string *str[STRCHUNKSZ];	/* strings */
    int chunksz;		/* size of chunk */
    struct _strchunk_ *next;	/* next in linked list */
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


# define ARRCHUNKSZ	128

typedef struct _arrchunk_ {
    array *arr[ARRCHUNKSZ];	/* arrays */
    int chunksz;		/* size of chunk */
    struct _arrchunk_ *next;	/* next in linked list */
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


typedef struct _parser_ {
    frame *frame;		/* interpreter stack frame */
    string *source;		/* grammar source */
    string *grammar;		/* preprocessed grammar */
    dfa *fa;			/* (partial) DFA */
    srp *lr;			/* (partial) shift/reduce parser */
    int ntoken;			/* number of tokens in grammar */

    pnchunk *pnc;		/* pnode chunk */

    unsigned short nstates;	/* state table size */
    snode **states;		/* state table */
    snlist list;		/* snode list */

    strchunk *strc;		/* string chunk */
    arrchunk *arrc;		/* array chunk */

    short traverse;		/* traverse code (ntoken + nprod) */
    Int maxalt;			/* max number of branches */
} parser;

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
    str_ref(ps->source = source);
    str_ref(ps->grammar = grammar);
    ps->fa = dfa_new(grammar->text);
    ps->lr = srp_new(grammar->text, grammar->len);

    ps->pnc = (pnchunk *) NULL;
    ps->list.snc = (snchunk *) NULL;
    ps->list.first = ps->list.free = (snode *) NULL;

    ps->strc = (strchunk *) NULL;
    ps->arrc = (arrchunk *) NULL;

    p = grammar->text;
    ps->ntoken = ((UCHAR(p[2]) + UCHAR(p[6])) << 8) + UCHAR(p[3]) + UCHAR(p[7]);
    ps->traverse = ps->ntoken + (UCHAR(p[8]) << 8) + UCHAR(p[9]);

    return ps;
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
    register unsigned short n, symb;
    char *red;
    unsigned short len;

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
	    sn->pn->len++;
	    for (ppn = &sn->pn->list;
		 *ppn != (pnode *) NULL && (*ppn)->u.text < red;
		 ppn = &(*ppn)->next) ;

	    pn->next = *ppn;
	    *ppn = pn;
	    return;
	}
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
unsigned short len;
{
    register int n;
    register char *p;

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
static pnode *ps_parse(ps, str)
register parser *ps;
string *str;
{
    register snode *sn;
    register int n;
    snode *next;
    char *ttext;
    unsigned int size, tlen;
    int nred;
    char *red;

    /* initialize */
    size = str->len;
    ps->nstates = srp_check(ps->lr, 0, 0, &nred, &red);
    ps->states = ALLOC(snode*, ps->nstates);
    memset(ps->states, '\0', ps->nstates * sizeof(snode*));
    ps->list.first = (snode *) NULL;

    /* state 0 */
    ps->states[0] = sn_new(&ps->list,
			   pn_new(&ps->pnc, 0, 0, (char *) NULL, 0,
				  (pnode *) NULL, (pnode *) NULL),
			   (snode *) NULL);

    do {
	/*
	 * apply reductions for current states, expanding states if needed
	 */
	for (sn = ps->list.first; sn != (snode *) NULL; sn = sn->next) {
	    n = srp_check(ps->lr, sn->pn->state, 0, &nred, &red);
	    if (n > ps->nstates) {
		/* grow tables */
		n <<= 1;
		ps->states = REALLOC(ps->states, snode*, ps->nstates, n);
		memset(ps->states + ps->nstates, '\0',
		       (n - ps->nstates) * sizeof(snode*));
		ps->nstates = n;
	    }
	    for (n = 0; n < nred; n++) {
		ps_reduce(ps, sn->pn, red);
		red += 4;
	    }
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
	    return (pnode *) NULL;

	case DFA_TOOBIG:
	    FREE(ps->states);
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

    /* parsing failed */
    return (pnode *) NULL;
}


# define PN_STRING	0	/* string node */
# define PN_ARRAY	1	/* array node */
# define PN_RULE	2	/* rule node */
# define PN_BRANCH	3	/* branch node */
# define PN_BLOCKED	4	/* blocked branch */

/*
 * NAME:	parser->flatten()
 * DESCRIPTION:	traverse parse tree, collecting values in a flat array
 */
static void ps_flatten(pn, traverse, v)
register pnode *pn;
register short traverse;
register value *v;
{
    register pnode *next;

    next = pn->next;
    do {
	switch (pn->symbol - traverse) {
	case PN_STRING:
	    (--v)->type = T_STRING;
	    str_ref(v->u.string = pn->u.str);
	    break;

	case PN_ARRAY:
	    v -= pn->len;
	    i_copy(v, d_get_elts(pn->u.arr), pn->len);
	    break;

	case PN_BRANCH:
	    (--v)->type = T_ARRAY;
	    arr_ref(v->u.array = pn->u.arr);
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
static Int ps_traverse(ps, pn)
register parser *ps;
register pnode *pn;
{
    register Int n;
    register pnode *sub;
    register unsigned short len, i;
    register value *v;
    array *a;

    if (pn->symbol < ps->traverse) {
	/*
	 * node hasn't been traversed before
	 */
	if (pn->symbol < ps->ntoken) {
	    /*
	     * token
	     */
	    pn->u.str = str_new(pn->u.text, (long) pn->len);
	    sc_add(&ps->strc, pn->u.str);

	    pn->symbol = ps->traverse + PN_STRING;
	    return pn->len = 1;
	} else if (pn->u.text != (char *) NULL) {
	    /*
	     * production rule
	     */
	    pn->symbol = ps->traverse + PN_BLOCKED;
	    len = 0;
	    for (i = pn->len, sub = pn->list; i != 0; --i, sub = sub->next) {
		n = ps_traverse(ps, sub);
		if (n < 0) {
		    return n;	/* blocked branch */
		}
		len += n;
	    }
	    pn->symbol = ps->traverse + PN_RULE;

	    n = UCHAR(pn->u.text[0]) << 1;
	    if (n == UCHAR(pn->u.text[1])) {
		/* no ?func */
		pn->len = len;
	    } else {
		/*
		 * call LPC function to process subtree
		 */
		a = arr_new(ps->frame->data, (long) len);
		ps_flatten(pn, ps->traverse, a->elts + len);
		(--ps->frame->sp)->type = T_ARRAY;
		arr_ref(ps->frame->sp->u.array = a);

		if (!i_call(ps->frame, ps->frame->obj, pn->u.text + 2 + n,
			    UCHAR(pn->u.text[1]) - n - 1, TRUE, 1)) {
		    return -1;	/* no function: block branch */
		}
		if (ps->frame->sp->type != T_ARRAY) {
		    /*
		     * wrong return type: block branch
		     */
		    i_del_value(ps->frame->sp++);
		    return -1;
		}

		pn->symbol = ps->traverse + PN_ARRAY;
		ac_add(&ps->arrc, pn->u.arr = (ps->frame->sp++)->u.array);
		pn->len = pn->u.arr->size;
	    }
	    return pn->len;
	} else {
	    /*
	     * branches
	     */
	    pn->symbol = ps->traverse + PN_BLOCKED;

	    /* pass 1: count branches */
	    n = 0;
	    for (sub = pn->list; sub != (pnode *) NULL; sub = sub->next) {
		if (ps_traverse(ps, sub) >= 0) {
		    n++;
		} else {
		    sub->symbol = ps->traverse + PN_BLOCKED;
		}
	    }
	    if (n == 0) {
		return -1;	/* no unblocked branches */
	    }
	    if (n > ps->maxalt) {
		n = ps->maxalt;
	    }

	    /* pass 2: create branch arrays */
	    if (n != 1) {
		ac_add(&ps->arrc, a = arr_new(ps->frame->data, (long) n));
		v = a->elts;
		memset(v, '\0', n * sizeof(value));
	    }
	    for (sub = pn->list, i = 0; i < n; sub = sub->next) {
		if (sub->symbol != ps->traverse + PN_BLOCKED) {
		    if (n == 1) {
			/* sole branch */
			*pn = *sub;
			return pn->len;
		    } else {
			arr_ref(v->u.array = arr_new(ps->frame->data,
				(long) pn->len));
			v->type = T_ARRAY;
			ps_flatten(pn, ps->traverse, (v++)->u.array->elts);
			i++;
		    }
		}
	    }
	    pn->symbol = ps->traverse + PN_BRANCH;
	    pn->u.arr = a;
	    return pn->len = n;
	}
    } else {
	/*
	 * node has been traversed before
	 */
	if (pn->symbol == ps->traverse + PN_BLOCKED) {
	    return -1;
	}
	return pn->len;
    }
}

/*
 * NAME:	parser->load()
 * DESCRIPTION:	load parse_string data
 */
static parser *ps_load()
{
    return (parser *) NULL;
}

/*
 * NAME:	parser->save()
 * DESCRIPTION:	save parse_string data
 */
void ps_save(data)
register dataspace *data;
{
    register parser *ps;
    register value *v;
    value val;
    string *s1, *s2;

    ps = data->parser;
    if (dfa_save(ps->fa, &s1, &s2)) {
	val.type = T_ARRAY;
	val.u.array = arr_new(data, 4L);
	v = val.u.array->elts;
	v->type = T_STRING;
	str_ref((v++)->u.string = ps->source);
	v->type = T_STRING;
	str_ref((v++)->u.string = ps->grammar);
	v->type = T_STRING;
	str_ref((v++)->u.string = s1);
	if (s2 != (string *) NULL) {
	    v->type = T_STRING;
	    str_ref(v->u.string = s2);
	} else {
	    v->type = T_INT;
	    v->u.number = 0;
	}
	d_assign_var(data, d_get_variable(data, data->nvariables - 1), &val);
    }
}

/*
 * NAME:	parser->free()
 * DESCRIPTION:	free parse_string data
 */
void ps_free(data)
dataspace *data;
{
    register parser *ps;

    ps = data->parser;
    srp_del(ps->lr);
    dfa_del(ps->fa);
    str_del(ps->source);
    str_del(ps->grammar);
    FREE(ps);
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
    string *grammar;
    bool same;
    pnode *pn;
    array *a;
    Int len;

    data = f->data;
    if (data->parser != (parser *) NULL) {
	ps = data->parser;
	same = (str_cmp(ps->source, source) == 0);
    } else {
	value *val;

	val = d_get_variable(data, data->nvariables - 1);
	if (val->type == T_ARRAY &&
	    str_cmp(d_get_elts(val->u.array)->u.string, source) == 0) {
	    ps = data->parser = ALLOC(parser, 1);
	    val = d_get_elts(val->u.array);
	    str_ref(ps->source = val[0].u.string);
	    str_ref(ps->grammar = val[1].u.string);
	    ps->fa = dfa_load(ps->grammar->text, val[2].u.string,
			      val[3].u.string);
	    same = TRUE;
	} else {
	    ps = (parser *) NULL;
	    same = FALSE;
	}
    }

    if (!same) {
	grammar = parse_grammar(source);

	if (data->parser != (parser *) NULL) {
	    ps_free(data);
	}
	ps = data->parser = ps_new(f, source, grammar);
    }

    ps->maxalt = maxalt;
    pn = ps_parse(ps, str);
    sn_clear(&ps->list);

    a = (array *) NULL;
    if (pn != (pnode *) NULL) {
	len = ps_traverse(ps, pn);
	if (len >= 0) {
	    a = arr_new(data, (long) len);
	    ps_flatten(pn, ps->traverse, a->elts + len);
	}

	sc_clean(ps->strc);
	ac_clean(ps->arrc);
    }

    pn_clear(ps->pnc);

    return a;
}
