# include "dgd.h"
# include "str.h"
# include "srp.h"

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


struct _srp_ {
    char *grammar;		/* grammar */
    unsigned short ntoken;	/* # of tokens (regexp & string) */
    unsigned short nprod;	/* # of nonterminals */

    unsigned short nstates;	/* number of states */
    Uint sttsize;		/* state table size */
    Uint sthsize;		/* state hash table size */
    srpstate *states;		/* state array */
    unsigned short *sthtab;	/* state hash table */

    itchunk *itc;		/* item chunk */

    Uint gap;			/* first gap in packed mapping */
    Uint spread;		/* max spread in packed mapping */
    Uint mapsize;		/* packed mapping size */
    char *shift;		/* packed shifts */
    char *check;		/* packed check for shift validity */
};

# define NOSHIFT	((short) 0x8000)

/*
 * NAME:	srp->new()
 * DESCRIPTION:	create new shift/reduce parser
 */
srp *srp_new(grammar)
char *grammar;
{
    register srp *lr;
    register char *p;
    Uint nrule;

    lr = ALLOC(srp, 1);

    /* grammar info */
    lr->grammar = grammar;
    lr->ntoken = ((UCHAR(grammar[2]) + UCHAR(grammar[6])) << 8) +
		 UCHAR(grammar[3]) + UCHAR(grammar[7]);
    lr->nprod = (UCHAR(grammar[8]) << 8) + UCHAR(grammar[9]);
    nrule = (UCHAR(grammar[10]) << 8) + UCHAR(grammar[11]);

    /* states */
    lr->nstates = 1;
    lr->sttsize = nrule << 1;
    lr->sthsize = nrule << 2;
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

    /* packed mapping for shift */
    lr->gap = lr->spread = 0;
    lr->mapsize = (Uint) (lr->ntoken + lr->nprod) << 2;
    lr->shift = ALLOC(char, lr->mapsize);
    memset(lr->shift, '\0', lr->mapsize);
    lr->check = ALLOC(char, lr->mapsize);
    memset(lr->check, '\xff', lr->mapsize);

    return lr;
}

/*
 * NAME:	srp->del()
 * DESCRIPTION:	delete shift/reduce parser
 */
void srp_del(lr)
register srp *lr;
{
    /* XXX do something about reductions and stuff */
    it_clear(lr->itc);
    FREE(lr->sthtab);
    FREE(lr->states);
    FREE(lr->shift);
    FREE(lr->check);
    FREE(lr);
}

/*
 * NAME:	srp->load()
 * DESCRIPTION:	load a shift/reduce parser from strings
 */
srp *srp_load(grammar, s1, s2)
char *grammar;
string *s1, *s2;
{
    return srp_new(grammar);
}

/*
 * NAME:	srp->save()
 * DESCRIPTION:	save a shift/reduce parser to strings
 */
bool srp_save(lr, s1, s2)
srp *lr;
string **s1, **s2;
{
    *s1 = str_new("", 0L);
    *s2 = (string *) NULL;
    return TRUE;
}

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two Ints
 */
static int cmp(i1, i2)
cvoid *i1, *i2;
{
    return (*(Int *) i1 < *(Int *) i2) ?
	    -1 : (*(Int *) i1 == *(Int *) i2) ? 0 : 1;
}

/*
 * NAME:	srp->pack()
 * DESCRIPTION:	add a new set of shifts and gotos to the packed mapping
 */
static int srp_pack(lr, from, to, n, check)
register srp *lr;
short *from, *to;
register int n;
int check;
{
    register int i, j;
    register char *p;
    Int *offstab;
    int offset, range;

    /* create offset table */
    offstab = ALLOCA(Int, n);
    for (i = n; --i >= 0; ) {
	offstab[i] = from[i] * 2;
    }
    qsort(offstab, n, sizeof(Int), cmp);
    j = offset = offstab[0];
    for (i = 1; i < n; i++) {
	offstab[i] -= j;
	j += offstab[i];
    }
    range = j - offset + 2;

    /*
     * add from/to pairs to packed mapping
     */
    for (i = lr->gap, p = &lr->check[i];
	 UCHAR(p[0]) != 0xff || UCHAR(p[1]) != 0xff; i += 2, p += 2) ;
    lr->gap = i;
    for (;;) {
    next:
	if (i + range >= lr->mapsize) {
	    /* grow tables */
	    j = (i + range) << 1;
	    lr->shift = REALLOC(lr->shift, char, lr->mapsize, j);
	    lr->check = REALLOC(lr->check, char, lr->mapsize, j);
	    memset(lr->shift + lr->mapsize, '\0', j - lr->mapsize);
	    memset(lr->check + lr->mapsize, '\xff', j - lr->mapsize);
	    lr->mapsize = j;
	}

	/* match each symbol with free slot */
	for (j = 1; j < n; j++) {
	    p += offstab[j];
	    if (UCHAR(p[0]) != 0xff || UCHAR(p[1]) != 0xff) {
		p = &lr->check[i];
		do {
		    i += 2;
		    p += 2;
		} while (UCHAR(p[0]) != 0xff || UCHAR(p[1]) != 0xff);
		goto next;
	    }
	}
	AFREE(offstab);

	/* free slots found: adjust spread */
	if (i + range > lr->spread) {
	    lr->spread = i + range;
	}

	/* add to packed mapping */
	offset = i - offset;
	for (j = n; --j >= 0; ) {
	    i = from[j] * 2 + offset;
	    p = &lr->shift[i];
	    *p++ = to[j] >> 8;
	    *p = to[j];
	    p = &lr->check[i];
	    *p++ = check >> 8;
	    *p = check;
	}
	return offset / 2;
    }
}

/*
 * NAME:	srp->expand()
 * DESCRIPTION:	expand a state
 */
static srpstate *srp_expand(lr, state)
register srp *lr;
srpstate *state;
{
    register int i, n;
    register char *p;
    register item *it;
    item **itemtab, *next;
    short *tokens, *targets;
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
    tokens = ALLOCA(short, n);
    targets = ALLOCA(short, n);
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
	tokens[nshift++] = lr->ntoken;
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
		tokens[nshift++] = n;
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
     * create target table
     */
    for (i = 0; i < nshift; i++) {
	newstate = &lr->states[lr->nstates];
	newstate->items = itemtab[tokens[i]];

	n = ss_hash(lr->sthtab, lr->sthsize, lr->states, lr->nstates);
	targets[i] = n;
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
     * add shifts and gotos to packed mapping
     */
    if (nshift != 0) {
	state->shoffset = srp_pack(lr, tokens, targets, nshift,
				   state - lr->states);
    } else {
	state->shoffset = NOSHIFT;
    }
    AFREE(targets);
    AFREE(tokens);
    AFREE(itemtab);

    return state;
}

/*
 * NAME:	srp->check()
 * DESCRIPTION:	fetch reductions for a given state, possibly first expanding it
 */
int srp_check(lr, num, nredp, redp)
register srp *lr;
int num;
int *nredp;
char **redp;
{
    register srpstate *state;

    state = &lr->states[num];
    if (state->nred < 0) {
	state = srp_expand(lr, state);
    }

    *nredp = state->nred;
    *redp = state->reds;
    return lr->nstates;
}

/*
 * NAME:	srp->shift()
 * DESCRIPTION:	shift to a new state, if possible
 */
int srp_shift(lr, num, token)
register srp *lr;
int num, token;
{
    register int n;
    register char *p;
    srpstate *state;

    n = lr->states[num].shoffset;
    if (n != NOSHIFT) {
	n = (n + token) * 2;
	if (n >= 0 && n < lr->mapsize) {
	    p = &lr->check[n];
	    if ((UCHAR(p[0]) << 8) + UCHAR(p[1]) == num) {
		/* shift works: return new state */
		p = &lr->shift[n];
		return (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    }
	}
    }

    /* shift failed */
    return -1;
}

/*
 * NAME:	srp->goto()
 * DESCRIPTION:	goto a new state
 */
int srp_goto(lr, num, symb)
register srp *lr;
int num, symb;
{
    register char *p;

    p = &lr->shift[(lr->states[num].shoffset + symb) * 2];
    return (UCHAR(p[0]) << 8) + UCHAR(p[1]);
}
