# define INCLUDE_CTYPE
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "grammar.h"
# include "dfa.h"
# include "parse.h"


typedef struct _item_ {
    char *ref;			/* pointer to rule in grammar */
    unsigned short ruleno;	/* rule number */
    unsigned short offset;	/* offset in rule */
    struct _item_ *next;	/* next in linked list */
} item;

# define ITCHUNKSZ	32

typedef struct _itchunk_ {
    item it[ITCHUNKSZ];		/* chunk of items */
    int chunksz;		/* size of this chunk */
    item *flist;		/* list of free items */
    struct _itchunk_ *next;	/* next in linked list */
} itchunk;

/*
 * NAME:	item->new()
 * DESCRIPTION:	create a new item
 */
static item *it_new(c, ref, ruleno, offset, next)
register itchunk **c;
char *ref;
int ruleno, offset;
item *next;
{
    register item *it;

    if (*c == (itchunk *) NULL ||
	((*c)->flist == (item *) NULL && (*c)->chunksz == ITCHUNKSZ)) {
	register itchunk *x;

	x = ALLOC(itchunk, 1);
	x->flist = (*c != (itchunk *) NULL) ? (*c)->flist : (item *) NULL;
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }
    if ((*c)->flist != (item *) NULL) {
	it = (*c)->flist;
	(*c)->flist = it->next;
    } else {
	it = &(*c)->it[(*c)->chunksz++];
    }

    it->ref = ref;
    it->ruleno = ruleno;
    it->offset = offset;
    it->next = next;

    return it;
}

/*
 * NAME:	item->del()
 * DESCRIPTION:	delete an item
 */
static item *it_del(c, it)
register itchunk *c;
register item *it;
{
    item *next;

    next = it->next;
    it->next = c->flist;
    c->flist = it;
    return next;
}

/*
 * NAME:	item->clear()
 * DESCRIPTION:	free all items in memory
 */
static void it_clear(c)
register itchunk *c;
{
    register itchunk *f;

    while (c != (itchunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }
}

/*
 * NAME:	item->add()
 * DESCRIPTION:	add an item to a set
 */
static void it_add(c, ri, ref, ruleno, offset, sort)
itchunk **c;
register item **ri;
register char *ref;
int ruleno;
register int offset;
bool sort;
{
    /*
     * add item to set
     */
    if (offset == UCHAR(ref[0]) << 1) {
	offset = UCHAR(ref[1]);	/* skip possible function at the end */
    }

    if (sort) {
	while (*ri != (item *) NULL &&
	       ((*ri)->ref < ref ||
		((*ri)->ref == ref && (*ri)->offset <= offset))) {
	    if ((*ri)->ref == ref && (*ri)->offset == offset) {
		return;	/* already in set */
	    }
	    ri = &(*ri)->next;
	}
    } else {
	while (*ri != (item *) NULL) {
	    if ((*ri)->ref == ref && (*ri)->offset == offset) {
		return;	/* already in set */
	    }
	    ri = &(*ri)->next;
	}
    }

    *ri = it_new(c, ref, ruleno, offset, *ri);
}


typedef struct {
    item *items;		/* rules and offsets */
    char *reds;			/* reductions */
    unsigned short nitem;	/* # items */
    short nred;			/* # reductions, -1 if unexpanded */
    short shoffset;		/* offset for shifts */
    unsigned short next;	/* next in linked list */
} srpstate;

/*
 * NAME:	srpstate->hash()
 * DESCRIPTION:	put a new state in the hash table, or return an old one
 */
static int ss_hash(htab, htabsize, states, idx)
unsigned short *htab;
int htabsize, idx;
srpstate *states;
{
    register unsigned long x;
    register srpstate *newstate;
    register item *it, *it2;
    srpstate *s;
    unsigned short *sr;

    /* hash on items */
    newstate = &states[idx];
    x = 0;
    for (it = newstate->items; it != (item *) NULL; it = it->next) {
	x ^= (long) it->ref;
	x = (x >> 3) ^ (x << 29) ^ it->offset;
    }

    /* check state hash table */
    sr = &htab[(Uint) x % htabsize];
    s = &states[*sr];
    while (s != states) {
	it = newstate->items;
	it2 = s->items;
	while (it != (item *) NULL && it2 != (item *) NULL &&
	       it->ref == it2->ref && it->offset == it2->offset) {
	    it = it->next;
	    it2 = it2->next;
	}
	if (it == it2) {
	    return *sr;	/* state already exists */
	}
	sr = &s->next;
	s = &states[*sr];
    }

    newstate->next = *sr;
    return *sr = idx;
}


typedef struct {
    char *grammar;		/* grammar */
    unsigned short ntoken;	/* # of tokens (regexp & string) */
    unsigned short nprod;	/* # of productions symbols */

    unsigned short nstates;	/* number of states */
    Uint sttsize;		/* state table size */
    Uint sthsize;		/* state hash table size */
    srpstate *states;		/* state array */
    unsigned short *sthtab;	/* state hash table */

    itchunk *itc;		/* item chunk */

    unsigned short gap;		/* first unused in shift table */
    unsigned short spread;	/* max spread in shift table */
    Uint shtabsize;		/* shift table size */
    char *shift;		/* shift table */
    char *check;		/* check for shift table */
} srp;

/*
 * NAME:	srp->new()
 * DESCRIPTION:	create new shift/reduce parser
 */
static srp *srp_new(grammar)
char *grammar;
{
    register srp *lr;
    register char *p;
    Uint nprod;

    lr = ALLOC(srp, 1);

    /* grammar info */
    lr->grammar = grammar;
    lr->ntoken = ((UCHAR(grammar[2]) + UCHAR(grammar[6])) << 8) +
		 UCHAR(grammar[3]) + UCHAR(grammar[7]);
    lr->nprod = (UCHAR(grammar[8]) << 8) + UCHAR(grammar[9]);
    nprod = (UCHAR(grammar[10]) << 8) + UCHAR(grammar[11]);

    /* states */
    lr->nstates = 1;
    lr->sttsize = nprod << 1;
    lr->sthsize = nprod << 2;
    lr->states = ALLOC(srpstate, lr->sttsize);
    lr->sthtab = ALLOC(unsigned short, lr->sthsize);
    memset(lr->sthtab, '\0', lr->sthsize * sizeof(unsigned short));
    lr->itc = (itchunk *) NULL;

    /* state 0 */
    p = grammar + 12 + (lr->ntoken << 1);
    p = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
    lr->states[0].items = it_new(&lr->itc, p + 2, lr->ntoken, 0, (item *) NULL);
    lr->states[0].reds = (char *) NULL;
    lr->states[0].nitem = 1;
    lr->states[0].nred = -1;
    lr->states[0].shoffset = 0;
    lr->states[0].next = 0;		/* but don't put it in hash table */

    /* shift table */
    lr->gap = lr->spread = 0;
    lr->shtabsize = (Uint) (lr->ntoken + lr->nprod) << 2;
    lr->shift = ALLOC(char, lr->shtabsize);
    memset(lr->shift, '\0', lr->shtabsize);
    lr->check = ALLOC(char, lr->shtabsize);
    memset(lr->check, '\xff', lr->shtabsize);

    return lr;
}

/*
 * NAME:	srp->del()
 * DESCRIPTION:	delete shift/reduce parser
 */
static void srp_del(lr)
register srp *lr;
{
    /* XXX do something about reductions */
    FREE(lr->states);
    FREE(lr->sthtab);
    it_clear(lr->itc);
    FREE(lr->shift);
    FREE(lr->check);
}

typedef struct {
    unsigned short symbol;	/* shift/goto symbol */
    unsigned short state;	/* state to shift to */
} shift;

/*
 * NAME:	cmp()
 * DESCRIPTION:	comparision function for shift symbols
 */
static int cmp(sym1, sym2)
cvoid *sym1, *sym2;
{
    return (*(unsigned short *) sym1 < *(unsigned short *) sym2) ? -1 : 1;
}

/*
 * NAME:	srp->packtable()
 * DESCRIPTION:	add new shift/goto table to packed global table
 */
static int srp_packtable(lr, shifttab, n, state)
register srp *lr;
shift *shifttab;
register int n;
int state;
{
    register int i, j;
    register char *p;
    unsigned short *offstab;
    int offset, range;

    if (n == 0) {
	return 0;	/* stuck in this state */
    }

    /* create offset table */
    offstab = ALLOCA(unsigned short, n);
    for (i = n; --i >= 0; ) {
	offstab[i] = shifttab[i].symbol << 1;
    }
    qsort(offstab, n, sizeof(unsigned short), cmp);
    j = offset = offstab[0];
    for (i = 1; i < n; i++) {
	offstab[i] -= j;
	j += offstab[i];
    }
    range = j - offset + 2;

    /*
     * add shift and gotos to packed table
     */
    for (i = lr->gap, p = &lr->check[i]; ; i += 2, p += 2) {
	/* find first free slot */
	if (UCHAR(p[0]) == 0xff && UCHAR(p[1]) == 0xff) {
	    if (i + range >= lr->shtabsize) {
		/* grow tables */
		j = (i + range) << 1;
		lr->shift = REALLOC(lr->shift, char, lr->shtabsize, j);
		lr->check = REALLOC(lr->check, char, lr->shtabsize, j);
		j -= lr->shtabsize;
		memset(lr->shift + lr->shtabsize, '\0', j);
		memset(lr->check + lr->shtabsize, '\xff', j);
		lr->shtabsize += j;
	    }
	    /* match each symbol with free slot */
	    for (j = 1; j < n; j++) {
		p += offstab[j];
		if (UCHAR(p[0]) != 0xff || UCHAR(p[1]) != 0xff) {
		    goto next;
		}
	    }
	    AFREE(offstab);

	    /* free slots found: adjust gap and spread */
	    lr->gap = i + 2;
	    if (i + range > lr->spread) {
		lr->spread = i + range;
	    }

	    /* add to packed table */
	    offset = i - offset;
	    for (j = n; --j >= 0; ) {
		i = (shifttab[j].symbol << 1) + offset;
		p = &lr->shift[i];
		*p++ = shifttab[j].state >> 8;
		*p = shifttab[j].state;
		p = &lr->check[i];
		*p++ = state >> 8;
		*p = state;
	    }
	    return offset / 2;

	next:
	    p = &lr->check[i];
	}
    }
}

/*
 * NAME:	srp->expand()
 * DESCRIPTION:	expand a srpstate
 */
static srpstate *srp_expand(lr, state)
register srp *lr;
srpstate *state;
{
    register int i, n;
    register char *p;
    register item *it;
    item **itemtab, *next;
    shift *shifttab;
    srpstate *newstate;
    int nred, nshift;

    if (state - lr->states == 1) {
	/* final state */
	state->nred = 0;
	return state;
    }

    n = lr->ntoken + lr->nprod;
    itemtab = ALLOCA(item*, n);
    memset(itemtab, '\0', n * sizeof(item*));
    shifttab = ALLOCA(shift, n);
    nred = 0;
    nshift = 0;

    /*
     * compute closure of kernel item set
     */
    for (it = state->items; it != (item *) NULL; it = it->next) {
	i = it->offset;
	p = it->ref + 1;
	if (i == UCHAR(*p++)) {
	    /* end of production */
	    nred++;
	} else {
	    p += i;
	    n = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    if (n >= lr->ntoken) {
		p = lr->grammar + 12 + (n << 1);
		p = lr->grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
		for (i = (UCHAR(p[0]) << 8) + UCHAR(p[1]), p += 2; --i >= 0; ) {
		    it_add(&lr->itc, &state->items, p, n, 0, FALSE);
		    p += UCHAR(p[1]) + 2;
		}
	    }
	}
    }

    state->nred = nred;
    if (nred != 0) {
	state->reds = ALLOC(char, (nred << 2));
	nred = 0;
    }

    /*
     * compute reductions and shifts
     */
    if (state == lr->states) {
	shifttab[nshift++].symbol = lr->ntoken;
    }
    for (it = state->items; it != (item *) NULL; it = it->next) {
	p = it->ref;
	if (it->offset == UCHAR(p[1])) {
	    /* reduction */
	    n = p - lr->grammar;
	    p = &state->reds[nred++ << 2];
	    *p++ = n >> 8;
	    *p++ = n;
	    *p++ = it->ruleno >> 8;
	    *p = it->ruleno;
	} else {
	    /* shift/goto */
	    p += 2 + it->offset;
	    n = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    if (itemtab[n] == (item *) NULL) {
		shifttab[nshift++].symbol = n;
	    }
	    it_add(&lr->itc, &itemtab[n], it->ref, it->ruleno, it->offset + 2,
		   TRUE);
	}
    }

    /*
     * delete non-kernel items
     */
    for (it = state->items, i = state->nitem; --i > 0; it = it->next) ;
    next = it->next;
    it->next = (item *) NULL;
    for (it = next; it != (item *) NULL; it = it_del(lr->itc, it)) ;

    /*
     * create shift/goto table
     */
    for (i = 0; i < nshift; i++) {
	newstate = &lr->states[lr->nstates];
	newstate->items = itemtab[shifttab[i].symbol];

	n = ss_hash(lr->sthtab, lr->sthsize, lr->states, lr->nstates);
	shifttab[i].state = n;
	if (n == lr->nstates) {
	    /*
	     * new state
	     */
	    newstate->reds = (char *) NULL;
	    n = 0;
	    for (it = newstate->items; it != (item *) NULL; it = it->next) {
		n++;
	    }
	    newstate->nitem = n;
	    newstate->nred = -1;
	    newstate->shoffset = 0;

	    if (++lr->nstates == lr->sttsize) {
		/* grow table */
		n = state - lr->states;
		lr->states = REALLOC(lr->states, srpstate, lr->nstates,
				     lr->sttsize <<= 1);
		state = lr->states + n;
	    }
	}
    }

    /*
     * add to global table
     */
    state->shoffset = srp_packtable(lr, shifttab, nshift, state - lr->states);
    AFREE(shifttab);
    AFREE(itemtab);

    return state;
}


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

    ps = ALLOC(parser, 1);
    ps->frame = f;
    str_ref(ps->source = source);
    str_ref(ps->grammar = grammar);
    ps->fa = dfa_new(grammar->text);
    ps->lr = srp_new(grammar->text);

    ps->pnc = (pnchunk *) NULL;
    ps->list.snc = (snchunk *) NULL;
    ps->list.first = ps->list.free = (snode *) NULL;

    ps->strc = (strchunk *) NULL;
    ps->arrc = (arrchunk *) NULL;

    ps->traverse = ps->lr->ntoken + ps->lr->nprod;

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
    p = &ps->lr->shift[(ps->lr->states[next->state].shoffset + symb) * 2];
    n = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
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
register snode *sn;
short token;
char *text;
unsigned short len;
{
    register int n;
    register char *p;

    n = (ps->lr->states[sn->pn->state].shoffset + token) * 2;
    if (n >= 0 && n < ps->lr->shtabsize) {
        p = &ps->lr->check[n];
        if ((UCHAR(p[0]) << 8) + UCHAR(p[1]) == sn->pn->state) {
	    /* shift works: add new snode */
	    p = &ps->lr->shift[n];
	    n = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    ps->states[n] = sn_add(&ps->list, sn,
				   pn_new(&ps->pnc, token, n, text, len,
					  sn->pn, (pnode *) NULL),
				   ps->states[n]);
	    return;
	}
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
    register srpstate *state;
    snode *next;
    char *ttext;
    unsigned int size, tlen;

    /* initialize */
    size = str->len;
    ps->nstates = (ps->lr->nstates != 1) ? ps->lr->nstates : ps->lr->nprod;
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
	    state = &ps->lr->states[sn->pn->state];
	    if (state->nred < 0) {
		state = srp_expand(ps->lr, state);
		if (ps->nstates < ps->lr->nstates) {
		    /* grow tables */
		    n = ps->lr->nstates << 1;
		    ps->states = REALLOC(ps->states, snode*, ps->nstates, n);
		    memset(ps->states + ps->nstates, '\0',
			   (n - ps->nstates) * sizeof(snode*));
		    ps->nstates = n;
		}
	    }
	    for (n = 0; n < state->nred; n++) {
		ps_reduce(ps, sn->pn, &state->reds[n << 2]);
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
	if (pn->symbol < ps->lr->ntoken) {
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
			    UCHAR(pn->u.text[1]) - n, TRUE, 1)) {
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
