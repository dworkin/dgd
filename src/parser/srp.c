# define INCLUDE_CTYPE
# include "dgd.h"
# include "srp.h"

typedef struct {
    char **follow;	/* actual follow info */
    Uint size;		/* size of follow string */
} follow;

/*
 * NAME:	follow->new()
 * DESCRIPTION:	compute FOLLOW() for each nonterminal
 */
static follow *fl_new(grammar, size, ntoken, nprod, nrule)
char *grammar;
unsigned short size, ntoken, nprod, nrule;
{
    register char *p, *q;
    register int i, j, k, l;
    Uint *dstart, *depend, dep;
    char **rule;
    unsigned short *rsym, *cstart, *copysym, csym, *rcount;
    unsigned short *queue1, *qhead1, *qtail1, *queue2, *qhead2, *qtail2;
    unsigned short *fstart, *fmap, *fsize;
    bool *nullable, *flag1, *flag2;
    follow *fl;

    /* offset of productions */
    p = grammar + 12 + (ntoken << 1);
    p = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);

    /*
     * allocate and initialize tables
     */
    fstart = ALLOCA(unsigned short, nprod);
    memset(fstart, '\0', nprod * sizeof(unsigned short));
    fmap = ALLOCA(unsigned short, ntoken * nprod);
    fsize = ALLOCA(unsigned short, nprod);
    memset(fsize, '\0', nprod * sizeof(unsigned short));

    dstart = ALLOCA(Uint, ntoken + nprod);
    memset(dstart, '\0', (ntoken + nprod) * sizeof(Uint));
    depend = ALLOCA(Uint, 2 * (size - (p - grammar)));
    rule = ALLOCA(char*, nrule);
    rsym = ALLOCA(unsigned short, nrule);
    cstart = ALLOCA(unsigned short, nprod);
    copysym = ALLOCA(unsigned short, nprod + nrule);
    queue1 = ALLOCA(unsigned short, nrule + 1);
    queue2 = ALLOCA(unsigned short, nprod);
    rcount = ALLOCA(unsigned short, nrule);
    memset(rcount, '\0', nrule * sizeof(unsigned short));
    nullable = ALLOCA(bool, nprod);
    memset(nullable, '\0', nprod * sizeof(bool));
    flag1 = ALLOCA(bool, nprod);
    flag2 = ALLOCA(bool, nprod);

    /*
     * collect dependancy info from grammar
     */
    nrule = 0;
    dep = 1;
    csym = 0;
    qhead1 = qtail1 = queue1;
    for (i = 0; i < nprod; i++) {
	cstart[i] = csym;

	j = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	p += 2;
	do {
	    rsym[nrule] = i;
	    rule[nrule] = p;
	    k = UCHAR(p[0]);
	    if (k == 0) {
		/* empty rule */
		if (!nullable[i]) {
		    nullable[i] = TRUE;
		    *qtail1++ = i;
		}
	    } else {
		q = p;
		do {
		    q += 2;
		    l = (UCHAR(q[0]) << 8) + UCHAR(q[1]);
		    depend[dep] = nrule + ((Uint) (q - p) << 16);
		    depend[dep + 1] = dstart[l];
		    dstart[l] = dep;
		    dep += 2;
		    rcount[nrule]++;
		} while (--k != 0);

		if (l >= ntoken) {
		    copysym[csym++] = l;
		}
	    }
	    p += 2 + UCHAR(p[1]);
	    nrule++;
	} while (--j != 0);

	copysym[csym++] = 0;
    }

    /*
     * propagate nullable
     */
    while (qhead1 != qtail1) {
	for (i = dstart[*qhead1++]; i != 0; i = depend[i + 1]) {
	    j = depend[i] & 0xffff;
	    if (--rcount[j] == 0 && !nullable[j = rsym[j]]) {
		nullable[j] = TRUE;
		*qtail1++ = j;
	    }
	}
    }

    /*
     * get follow info
     */
    for (i = 0; i < ntoken; i++) {
	/*
	 * pass 1
	 */
	memset(flag1, '\0', nprod);
	memset(flag2, '\0', nprod);
	qhead1 = qtail1 = queue1;
	qhead2 = qtail2 = queue2;
	*qtail1++ = i;
	do {
	    for (j = dstart[*qhead1++]; j != 0; j = depend[j + 1]) {
		k = depend[j] & 0xffff;
		if (!flag1[rsym[k]]) {
		    p = rule[k];
		    q = p + (depend[j] >> 16);
		    do {
			q -= 2;
			if (q == p) {
			    l = rsym[k];
			    flag1[l] = TRUE;
			    *qtail1++ = l + ntoken;
			    break;
			}

			l = (UCHAR(q[0]) << 8) + UCHAR(q[1]);
			if (l < ntoken) {
			    break;
			}
			l -= ntoken;

			if (!flag2[l]) {
			    flag2[l] = TRUE;
			    *qtail2++ = l;
			}
		    } while (nullable[l]);
		}
	    }
	} while (qhead1 != qtail1);

	/*
	 * pass 2
	 */
	memset(flag1, '\0', nprod);
	while (qhead2 != qtail2) {
	    l = *qhead2++;
	    if (!flag1[l]) {
		flag1[l] = TRUE;
		fmap[l * ntoken + i] = fstart[l];
		fstart[l] = i + 1;
		fsize[l]++;

		for (k = cstart[l]; (l=copysym[k]) != 0; k++) {
		    l -= ntoken;
		    if (!flag2[l]) {
			flag2[l] = TRUE;
			*qtail2++ = l;
		    }
		}
	    }
	}
    }

    AFREE(flag2);
    AFREE(flag1);
    AFREE(nullable);
    AFREE(rcount);
    AFREE(queue2);
    AFREE(queue1);
    AFREE(copysym);
    AFREE(cstart);
    AFREE(rsym);
    AFREE(rule);
    AFREE(depend);
    AFREE(dstart);

    /*
     * convert to proper table
     */
    for (i = j = 0; i < nprod; i++) {
	j += 2 * fsize[i] + 2;
    }
    fl = ALLOC(follow, 1);
    fl->follow = ALLOC(char*, nprod);
    fl->size = j;
    p = ALLOC(char, j);

    for (i = 0; i < nprod; i++) {
	fl->follow[i] = p;
	*p++ = fsize[i] >> 8;
	*p++ = fsize[i];
	for (j = fstart[i]; j != 0; j = fmap[i * ntoken + j]) {
	    *p++ = --j >> 8;
	    *p++ = j;
	}
    }

    AFREE(fsize);
    AFREE(fmap);
    AFREE(fstart);

    return fl;
}


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
    Uint gap;			/* first gap in packed mapping */
    Uint spread;		/* max spread in packed mapping */
    Uint mapsize;		/* packed mapping size */
    char *data;			/* packed data (may be NULL) */
    char *check;		/* packed check for data validity */
} packed;

/*
 * NAME:	packed->new()
 * DESCRIPTION:	create a new packed table
 */
static packed *pk_new(size, data)
register Uint size;
bool data;
{
    register packed *pk;

    pk = ALLOC(packed, 1);
    pk->gap = pk->spread = 0;
    pk->mapsize = size;
    if (data) {
	pk->data = ALLOC(char, size);
	memset(pk->data, '\0', size);
    } else {
	pk->data = (char *) NULL;
    }
    pk->check = ALLOC(char, size);
    memset(pk->check, '\xff', size);

    return pk;
}


/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two Ints
 */
static int cmp(sh1, sh2)
cvoid *sh1, *sh2;
{
    return (*(Int *) sh1 < *(Int *) sh2) ?
	    -1 : (*(Int *) sh1 == *(Int *) sh2) ? 0 : 1;
}

/*
 * NAME:	packed->add()
 * DESCRIPTION:	add a new set of pairs to a packed mapping
 */
static int pk_add(pk, from, to, n, check)
register packed *pk;
short *from, *to;
register int n;
int check;
{
    register int i, j;
    register char *p;
    Int *offstab;
    int offset, range;

    if (n == 0) {
	return 0;	/* stuck in this state */
    }

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
     * add shift and gotos to packed table
     */
    for (i = pk->gap, p = &pk->check[i]; ; i += 2, p += 2) {
	/* find first free slot */
	if (UCHAR(p[0]) == 0xff && UCHAR(p[1]) == 0xff) {
	    if (i + range >= pk->mapsize) {
		/* grow tables */
		j = (i + range) << 1;
		if (pk->data != (char *) NULL) {
		    pk->data = REALLOC(pk->data, char, pk->mapsize, j);
		    memset(pk->data + pk->mapsize, '\0', j - pk->mapsize);
		}
		pk->check = REALLOC(pk->check, char, pk->mapsize, j);
		memset(pk->check + pk->mapsize, '\xff', j - pk->mapsize);
		pk->mapsize = j;
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
	    pk->gap = i + 2;
	    if (i + range > pk->spread) {
		pk->spread = i + range;
	    }

	    /* add to packed table */
	    offset = i - offset;
	    for (j = n; --j >= 0; ) {
		i = from[j] * 2 + offset;
		if (to != (short *) NULL) {
		    p = &pk->data[i];
		    *p++ = to[j] >> 8;
		    *p = to[j];
		}
		p = &pk->check[i];
		*p++ = check >> 8;
		*p = check;
	    }
	    return offset / 2;

	next:
	    p = &pk->check[i];
	}
    }
}


struct _srp_ {
    char *grammar;		/* grammar */
    unsigned short ntoken;	/* # of tokens (regexp & string) */
    unsigned short nprod;	/* # of nonterminals */

    follow *fl;			/* follow info */

    unsigned short nstates;	/* number of states */
    Uint sttsize;		/* state table size */
    Uint sthsize;		/* state hash table size */
    srpstate *states;		/* state array */
    unsigned short *sthtab;	/* state hash table */

    itchunk *itc;		/* item chunk */

    packed *looka;		/* reduction lookaheads */
    packed *shift;		/* shifts & gotos */
};

/*
 * NAME:	srp->new()
 * DESCRIPTION:	create new shift/reduce parser
 */
srp *srp_new(grammar, size)
char *grammar;
unsigned int size;
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

    /* follow info */
    lr->fl = fl_new(grammar, size, lr->ntoken, lr->nprod,
		    (unsigned short) nrule);

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

    /* packed mappings */
    lr->looka = pk_new((Uint) (lr->ntoken + lr->nprod) << 2, FALSE);
    lr->shift = pk_new((Uint) (lr->ntoken + lr->nprod) << 2, TRUE);

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
    FREE(lr->states);
    FREE(lr->sthtab);
    it_clear(lr->itc);
}

/*
 * NAME:	srp->check()
 * DESCRIPTION:	fetch reductions for a given state, possibly first expanding it
 */
int srp_check(lr, num, token, nredp, redp)
register srp *lr;
int num;
int token;
int *nredp;
char **redp;
{
    register int i, n;
    register char *p;
    register item *it;
    item **itemtab, *next;
    short *tokens, *targets;
    srpstate *state, *newstate;
    int nred, nshift;

    state = &lr->states[num];
    if (state->nred >= 0) {
	/* already expanded */
	*nredp = state->nred;
	*redp = state->reds;
	return lr->nstates;
    }

    if (state - lr->states == 1) {
	/* final state */
	state->nred = 0;
	*nredp = state->nred;
	*redp = state->reds;
	return lr->nstates;
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
     * create shift/goto table
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
     * add to global table
     */
    state->shoffset = pk_add(lr->shift, tokens, targets, nshift,
			     state - lr->states);
    AFREE(targets);
    AFREE(tokens);
    AFREE(itemtab);

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

    n = (lr->states[num].shoffset + token) * 2;
    if (n >= 0 && n < lr->shift->mapsize) {
        p = &lr->shift->check[n];
        if ((UCHAR(p[0]) << 8) + UCHAR(p[1]) == num) {
	    /* shift works: return new state */
	    p = &lr->shift->data[n];
	    return (UCHAR(p[0]) << 8) + UCHAR(p[1]);
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

    p = &lr->shift->data[(lr->states[num].shoffset + symb) * 2];
    return (UCHAR(p[0]) << 8) + UCHAR(p[1]);
}
