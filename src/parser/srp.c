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
    union {
	char e[4];		/* 1 */
	char *a;		/* > 1 */
    } reds;			/* reductions */
    unsigned short nitem;	/* # items */
    short nred;			/* # reductions, -1 if unexpanded */
    short shoffset;		/* offset for shifts */
    unsigned short shcheck;	/* shift offset check */
    short gtoffset;		/* offset for gotos */
    unsigned short next;	/* next in linked list */
} srpstate;

# define REDA(state)   (((state)->nred == 1) ? \
			(state)->reds.e : (state)->reds.a)

/*
 * NAME:	srpstate->hash()
 * DESCRIPTION:	put a new state in the hash table, or return an old one
 */
static int ss_hash(htab, htabsize, states, idx)
unsigned short *htab;
int htabsize, idx;
srpstate *states;
{
    register unsigned long h;
    register srpstate *newstate;
    register item *it, *it2;
    srpstate *s;
    unsigned short *sr;

    /* hash on items */
    newstate = &states[idx];
    h = 0;
    for (it = newstate->items; it != (item *) NULL; it = it->next) {
	h ^= (long) it->ref;
	h = (h >> 3) ^ (h << 29) ^ it->offset;
    }

    /* check state hash table */
    sr = &htab[(Uint) h % htabsize];
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


# define NOSHIFT	((short) 0x8000)

typedef struct _shlink_ {
    char *shifts;		/* shift from to */
    short offset;		/* offset in table */
    unsigned short check;	/* shift offset check */
    struct _shlink_ *next;	/* next in linked list */
} shlink;

# define SLCHUNKSZ	64

typedef struct _slchunk_ {
    shlink sl[SLCHUNKSZ];	/* shlinks */
    int chunksz;		/* size of chunk */
    struct _slchunk_ *next;	/* next in linked list */
} slchunk;

/*
 * NAME:	shlink->hash()
 * DESCRIPTION:	put a new shlink in the hash table, or return an old one
 */
static shlink *sl_hash(htab, htabsize, c, shifts, n)
shlink **htab;
Uint htabsize;
register slchunk **c;
register char *shifts;
register int n;
{
    register unsigned long h;
    register int i;
    register shlink **ssl, *sl;

    /* search in hash table */
    h = 0;
    for (i = n; --i >= 0; ) {
	h = (h >> 3) ^ (h << 29) ^ *shifts++;
    }
    shifts -= n;
    ssl = &htab[h % htabsize];
    while (*ssl != (shlink *) NULL) {
	if (memcmp((*ssl)->shifts, shifts, n) == 0) {
	    /* seen this one before */
	    return *ssl;
	}
	ssl = &(*ssl)->next;
    }

    if (*c == (slchunk *) NULL || (*c)->chunksz == SLCHUNKSZ) {
	register slchunk *x;

	x = ALLOC(slchunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }
    sl = &(*c)->sl[(*c)->chunksz++];
    sl->next = *ssl;
    *ssl = sl;
    sl->shifts = (char *) NULL;

    return sl;
}

/*
 * NAME:	shlink->clear()
 * DESCRIPTION:	clean up shlinks
 */
static void sl_clear(c)
register slchunk *c;
{
    register slchunk *f;

    while (c != (slchunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }
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
    char *data;			/* packed shifts */
    char *check;		/* packed check for shift validity */

    slchunk *slc;		/* sholink chunk */
    Uint nshift;		/* number of shifts (from/to pairs) */
    Uint shtsize;		/* shift table size */
    Uint shhsize;		/* shift hash table size */
    char *shtab;		/* shift (from/to) table */
    shlink **shhtab;		/* shift hash table */
};

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
    lr->states[0].nitem = 1;
    lr->states[0].nred = -1;
    lr->states[0].shoffset = NOSHIFT;
    lr->states[0].gtoffset = 0;
    lr->states[0].next = 0;		/* but don't put it in hash table */

    /* packed mapping for shift */
    lr->gap = lr->spread = 0;
    lr->mapsize = (Uint) (lr->ntoken + lr->nprod) << 2;
    lr->data = ALLOC(char, lr->mapsize);
    memset(lr->data, '\0', lr->mapsize);
    lr->check = ALLOC(char, lr->mapsize);
    memset(lr->check, '\xff', lr->mapsize);

    /* shift hash table */
    lr->slc = (slchunk *) NULL;
    lr->nshift = 0;
    lr->shtsize = lr->mapsize;
    lr->shhsize = nrule << 2;
    lr->shtab = ALLOC(char, lr->shtsize);
    lr->shhtab = ALLOC(shlink*, lr->shhsize);
    memset(lr->shhtab, '\0', lr->shhsize * sizeof(shlink*));

    return lr;
}

/*
 * NAME:	srp->del()
 * DESCRIPTION:	delete shift/reduce parser
 */
void srp_del(lr)
register srp *lr;
{
    register int i;
    register srpstate *state;

    it_clear(lr->itc);
    FREE(lr->sthtab);
    for (i = lr->nstates, state = lr->states; --i >= 0; state++) {
	if (state->nred > 1) {
	    FREE(state->reds.a);
	}
    }
    FREE(lr->states);
    FREE(lr->data);
    FREE(lr->check);
    sl_clear(lr->slc);
    FREE(lr->shtab);
    FREE(lr->shhtab);
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
 * NAME:	srp->pack()
 * DESCRIPTION:	add a new set of shifts and gotos to the packed mapping
 */
static int srp_pack(lr, from, to, n, check)
register srp *lr;
short *from, *to;
register int n;
unsigned short *check;
{
    register int i, j;
    register char *p;
    char *shifts;
    shlink *sl;
    Int *offstab;
    int offset, range;

    /*
     * check hash table
     */
    p = shifts = ALLOCA(char, j = 4 * n + 2);
    *p++ = n >> 8;
    *p++ = n;
    for (i = 0; i < n; i++) {
	*p++ = from[i] >> 8;
	*p++ = from[i];
	*p++ = to[i] >> 8;
	*p++ = to[i];
    }
    sl = sl_hash(lr->shhtab, lr->shhsize, &lr->slc, shifts, j);
    if (sl->shifts != (char *) NULL) {
	/* same as before */
	AFREE(shifts);
	*check = sl->check;
	return sl->offset;
    }

    /* not in hash table */
    if (lr->nshift + j > lr->shtsize) {
	/* grow shift table */
	i = (lr->nshift + j) * 2;
	lr->shtab = REALLOC(lr->shtab, char, lr->shtsize, i);
	lr->shtsize = i;
    }
    sl->shifts = lr->shtab + lr->nshift;
    lr->nshift += j;
    memcpy(sl->shifts, shifts, j);
    AFREE(shifts);

    /* create offset table */
    offstab = ALLOCA(Int, n);
    for (i = n; --i >= 0; ) {
	offstab[i] = from[i] * 2;
    }
    j = offset = offstab[0];
    for (i = 1; i < n; i++) {
	offstab[i] -= j;
	j += offstab[i];
    }
    range = j - offset + 2;

    /*
     * add from/to pairs to packed mapping
     */
    for (i = lr->gap, p = &lr->check[i]; UCHAR(p[0]) != 0xff; i += 2, p += 2) ;
    lr->gap = i;
    for (;;) {
    next:
	if (i + range >= lr->mapsize) {
	    /* grow tables */
	    j = (i + range) << 1;
	    lr->data = REALLOC(lr->data, char, lr->mapsize, j);
	    lr->check = REALLOC(lr->check, char, lr->mapsize, j);
	    memset(lr->data + lr->mapsize, '\0', j - lr->mapsize);
	    memset(lr->check + lr->mapsize, '\xff', j - lr->mapsize);
	    lr->mapsize = j;
	}

	/* match each symbol with free slot */
	for (j = 1; j < n; j++) {
	    p += offstab[j];
	    if (UCHAR(p[0]) != 0xff) {
		p = &lr->check[i];
		do {
		    i += 2;
		    p += 2;
		} while (UCHAR(p[0]) != 0xff);
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
	do {
	    j = from[--n] * 2 + offset;
	    p = &lr->data[j];
	    *p++ = to[n] >> 8;
	    *p = to[n];
	    p = &lr->check[j];
	    *p++ = i >> 8;
	    *p = i;
	} while (n != 0);
	sl->check = *check = i;
	return sl->offset = offset / 2;
    }
}

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two shorts
 */
static int cmp(sh1, sh2)
cvoid *sh1, *sh2;
{
    return (*(short *) sh1 < *(short *) sh2) ?
	    -1 : (*(short *) sh1 == *(short *) sh2) ? 0 : 1;
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
    short *tokens, *symbols, *targets;
    srpstate *newstate;
    int nred, nshift, ngoto;

    if (state - lr->states == 1) {
	/* final state */
	state->nred = 0;
	return state;
    }

    n = lr->ntoken + lr->nprod;
    itemtab = ALLOCA(item*, n);
    memset(itemtab, '\0', n * sizeof(item*));
    symbols = ALLOCA(short, n);
    targets = ALLOCA(short, n);
    tokens = ALLOCA(short, lr->ntoken);
    nred = nshift = ngoto = 0;

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
	if (nred > 1) {
	    state->reds.a = ALLOC(char, (nred << 2));
	}
	nred = 0;
    }

    /*
     * compute reductions and shifts
     */
    if (state == lr->states) {
	symbols[ngoto++] = lr->ntoken;
    }
    for (it = state->items; it != (item *) NULL; it = it->next) {
	p = it->ref;
	if (it->offset == UCHAR(p[1])) {
	    /* reduction */
	    n = p - lr->grammar;
	    p = &REDA(state)[nred++ << 2];
	    *p++ = n >> 8;
	    *p++ = n;
	    *p++ = it->ruleno >> 8;
	    *p = it->ruleno;
	} else {
	    /* shift/goto */
	    p += 2 + it->offset;
	    n = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    if (itemtab[n] == (item *) NULL) {
		if (n < lr->ntoken) {
		    tokens[nshift++] = n;
		} else {
		    symbols[ngoto++] = n;
		}
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
     * sort and merge token and goto tables
     */
    qsort(symbols, ngoto, sizeof(short), cmp);
    memcpy(symbols + ngoto, tokens, nshift * sizeof(short));
    AFREE(tokens);
    tokens = symbols + ngoto;
    qsort(tokens, nshift, sizeof(short), cmp);

    /*
     * create target table
     */
    for (i = 0; i < nshift + ngoto; i++) {
	newstate = &lr->states[lr->nstates];
	newstate->items = itemtab[symbols[i]];

	n = ss_hash(lr->sthtab, lr->sthsize, lr->states, lr->nstates);
	targets[i] = n;
	if (n == lr->nstates) {
	    /*
	     * new state
	     */
	    n = 0;
	    for (it = newstate->items; it != (item *) NULL; it = it->next) {
		n++;
	    }
	    newstate->nitem = n;
	    newstate->nred = -1;
	    newstate->shoffset = NOSHIFT;
	    newstate->gtoffset = 0;

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
	state->shoffset = srp_pack(lr, tokens, targets + ngoto, nshift,
				   &state->shcheck);
    }
    if (ngoto != 0) {
	unsigned short dummy;

	state->gtoffset = srp_pack(lr, symbols, targets, ngoto, &dummy);
    }
    AFREE(targets);
    AFREE(symbols);
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
    *redp = REDA(state);
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

    state = &lr->states[num];
    n = state->shoffset;
    if (n != NOSHIFT) {
	n = (n + token) * 2;
	if (n >= 0 && n < lr->mapsize) {
	    p = &lr->check[n];
	    if ((UCHAR(p[0]) << 8) + UCHAR(p[1]) == state->shcheck) {
		/* shift works: return new state */
		p = &lr->data[n];
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

    p = &lr->data[(lr->states[num].gtoffset + symb) * 2];
    return (UCHAR(p[0]) << 8) + UCHAR(p[1]);
}
