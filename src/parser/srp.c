# include "dgd.h"
# include "srp.h"

typedef struct _item_ {
    char *ref;			/* pointer to rule in grammar */
    unsigned short ruleno;	/* rule number */
    unsigned short offset;	/* offset in rule */
    struct _item_ *next;	/* next in linked list */
} item;

# define ITCHUNKSZ	32

typedef struct _itchunk_ {
    int chunksz;		/* size of this chunk */
    item *flist;		/* list of free items */
    struct _itchunk_ *next;	/* next in linked list */
    item it[ITCHUNKSZ];		/* chunk of items */
} itchunk;

/*
 * NAME:	item->new()
 * DESCRIPTION:	create a new item
 */
static item *it_new(c, ref, ruleno, offset, next)
register itchunk **c;
char *ref;
unsigned short ruleno, offset;
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
unsigned short ruleno;
register unsigned short offset;
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

/*
 * NAME:	item->load()
 * DESCRIPTION:	load an item
 */
static item *it_load(c, n, buf, grammar)
itchunk **c;
unsigned short n;
char **buf, *grammar;
{
    register char *p;
    register item **ri;
    item *it;
    char *ref;
    unsigned short ruleno;

    ri = &it;
    p = *buf;
    do {
	ref = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	p += 2;
	ruleno = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	p += 2;
	*ri = it_new(c, ref, ruleno, UCHAR(*p++), (item *) NULL);
	ri = &(*ri)->next;
    } while (--n != 0);
    *buf = p;

    return it;
}

/*
 * NAME:	item->save()
 * DESCRIPTION:	save an item
 */
static char *it_save(it, buf, grammar)
register item *it;
register char *buf;
char *grammar;
{
    unsigned short offset;

    while (it != (item *) NULL) {
	offset = it->ref - grammar;
	*buf++ = offset >> 8;
	*buf++ = offset;
	*buf++ = it->ruleno >> 8;
	*buf++ = it->ruleno;
	*buf++ = it->offset;
	it = it->next;
    }

    return buf;
}


typedef struct {
    item *items;		/* rules and offsets */
    union {
	char e[4];		/* 1 */
	char *a;		/* > 1 */
    } reds;			/* reductions */
    unsigned short nitem;	/* # items */
    short nred;			/* # reductions, -1 if unexpanded */
    Int shoffset;		/* offset for shifts */
    Int gtoffset;		/* offset for gotos */
    unsigned short shcheck;	/* shift offset check */
    unsigned short next;	/* next in linked list */
    bool alloc;			/* reductions allocated? */
} srpstate;

# define REDA(state)   (((state)->nred == 1) ? \
			(state)->reds.e : (state)->reds.a)
# define UNEXPANDED	-1
# define NOSHIFT	((Int) 0xff800000L)

/*
 * NAME:	srpstate->hash()
 * DESCRIPTION:	put a new state in the hash table, or return an old one
 */
static unsigned short ss_hash(htab, htabsize, states, idx)
unsigned short *htab, idx;
Uint htabsize;
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

/*
 * NAME:	srpstate->load()
 * DESCRIPTION:	load a srpstate
 */
static char *ss_load(buf, rbuf, state)
register char *buf, **rbuf;
register srpstate *state;
{
    state->items = (item *) NULL;
    state->nitem = (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    buf += 2;
    state->nred = (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    buf += 2;
    state->shoffset = ((Int) SCHAR(buf[0]) << 16) + (UCHAR(buf[1]) << 8) +
		      UCHAR(buf[2]);
    buf += 3;
    state->gtoffset = ((Int) SCHAR(buf[0]) << 16) + (UCHAR(buf[1]) << 8) +
		      UCHAR(buf[2]);
    buf += 3;
    state->shcheck = (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    buf += 2;

    if (state->nred > 0) {
	if (state->nred == 1) {
	    memcpy(state->reds.e, *rbuf, 4);
	} else {
	    state->reds.a = *rbuf;
	}
	*rbuf += 4 * state->nred;
    }
    state->alloc = FALSE;

    return buf;
}

/*
 * NAME:	srpstate->save()
 * DESCRIPTION:	save a srpstate
 */
static char *ss_save(state, buf, rbuf)
register srpstate *state;
register char *buf, **rbuf;
{
    *buf++ = state->nitem >> 8;
    *buf++ = state->nitem;
    *buf++ = state->nred >> 8;
    *buf++ = state->nred;
    *buf++ = state->shoffset >> 16;
    *buf++ = state->shoffset >> 8;
    *buf++ = state->shoffset;
    *buf++ = state->gtoffset >> 16;
    *buf++ = state->gtoffset >> 8;
    *buf++ = state->gtoffset;
    *buf++ = state->shcheck >> 8;
    *buf++ = state->shcheck;

    if (state->nred > 0) {
	memcpy(*rbuf, REDA(state), state->nred * 4);
	*rbuf += state->nred * 4;
    }

    return buf;
}


typedef struct _shlink_ {
    Int shifts;			/* offset in shift table */
    struct _shlink_ *next;	/* next in linked list */
} shlink;

# define SLCHUNKSZ	64

typedef struct _slchunk_ {
    int chunksz;		/* size of chunk */
    struct _slchunk_ *next;	/* next in linked list */
    shlink sl[SLCHUNKSZ];	/* shlinks */
} slchunk;

/*
 * NAME:	shlink->hash()
 * DESCRIPTION:	put a new shlink in the hash table, or return an old one
 */
static shlink *sl_hash(htab, htabsize, c, shtab, shifts, n)
shlink **htab;
Uint htabsize;
register slchunk **c;
char *shtab;
register char *shifts;
register Uint n;
{
    register unsigned long h;
    register Uint i;
    register shlink **ssl, *sl;

    /* search in hash table */
    shifts += 5;
    n -= 5;
    h = 0;
    for (i = n; i > 0; --i) {
	h = (h >> 3) ^ (h << 29) ^ UCHAR(*shifts++);
    }
    shifts -= n;
    ssl = &htab[h % htabsize];
    while (*ssl != (shlink *) NULL) {
	if (memcmp(shtab + (*ssl)->shifts + 5, shifts, n) == 0) {
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
    sl->shifts = NOSHIFT;

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

    Uint nred;			/* # of reductions */
    Uint nitem;			/* # of items */
    Uint srpsize;		/* size of shift/reduce parser */
    Uint tmpsize;		/* size of temporary data */
    bool modified;		/* srp needs saving */
    bool allocated;		/* srp allocated */
    char *srpstr;		/* srp string */
    char *tmpstr;		/* tmp string */

    unsigned short nstates;	/* number of states */
    unsigned short nexpanded;	/* number of expanded states */
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
    bool alloc;			/* data and check allocated separately? */

    slchunk *slc;		/* shlink chunk */
    Uint nshift;		/* number of shifts (from/to pairs) */
    Uint shtsize;		/* shift table size */
    Uint shhsize;		/* shift hash table size */
    char *shtab;		/* shift (from/to) table */
    shlink **shhtab;		/* shift hash table */
};

# define SRP_VERSION	1

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
    lr->ntoken = ((UCHAR(grammar[5]) + UCHAR(grammar[9])) << 8) +
		 UCHAR(grammar[6]) + UCHAR(grammar[10]);
    lr->nprod = (UCHAR(grammar[11]) << 8) + UCHAR(grammar[12]);
    nrule = (UCHAR(grammar[13]) << 8) + UCHAR(grammar[14]);

    /* sizes */
    lr->srpstr = (char *) NULL;
    lr->tmpstr = (char *) NULL;
    lr->nred = lr->nitem = 0;
    lr->srpsize = 14 + 12 + 4;	/* srp header + 1 state + data/check overhead */
    lr->tmpsize = 7 + 5;	/* tmp header + 1 item */
    lr->modified = TRUE;
    lr->allocated = FALSE;

    /* states */
    lr->nstates = 1;
    lr->nexpanded = 0;
    lr->sttsize = nrule << 1;
    lr->sthsize = nrule << 2;
    lr->states = ALLOC(srpstate, lr->sttsize);
    lr->sthtab = ALLOC(unsigned short, lr->sthsize);
    memset(lr->sthtab, '\0', lr->sthsize * sizeof(unsigned short));
    lr->itc = (itchunk *) NULL;

    /* state 0 */
    p = grammar + 15 + (lr->ntoken << 1);
    p = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
    lr->states[0].items = it_new(&lr->itc, p + 2, lr->ntoken, 0, (item *) NULL);
    lr->states[0].nitem = 1;
    lr->states[0].nred = UNEXPANDED;
    lr->states[0].shoffset = NOSHIFT;
    lr->states[0].gtoffset = 0;
    lr->states[0].shcheck = 0;
    lr->states[0].alloc = FALSE;

    /* packed mapping for shift */
    lr->gap = lr->spread = 0;
    lr->mapsize = (Uint) (lr->ntoken + lr->nprod) << 2;
    lr->data = ALLOC(char, lr->mapsize);
    memset(lr->data, '\0', lr->mapsize);
    lr->check = ALLOC(char, lr->mapsize);
    memset(lr->check, '\xff', lr->mapsize);
    lr->alloc = TRUE;

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
    register unsigned short i;
    register srpstate *state;

    if (lr->allocated) {
	FREE(lr->srpstr);
    }
    it_clear(lr->itc);
    if (lr->sthtab != (unsigned short *) NULL) {
	FREE(lr->sthtab);
    }
    for (i = lr->nstates, state = lr->states; i > 0; --i, state++) {
	if (state->alloc) {
	    FREE(state->reds.a);
	}
    }
    FREE(lr->states);
    if (lr->alloc) {
	FREE(lr->data);
	FREE(lr->check);
    }
    sl_clear(lr->slc);
    if (lr->shtab != (char *) NULL) {
	FREE(lr->shtab);
    }
    if (lr->shhtab != (shlink **) NULL) {
	FREE(lr->shhtab);
    }
    FREE(lr);
}

/*
 * format of SRP permanent data:
 *
 * header	[1]		version number
 *		[x][y]		# states
 *		[x][y]		# expanded states
 *		[x][y][z]	# reductions
 *		[x][y][z]	initial gap in packed mapping
 *		[x][y][z]	max spread of packed mapping
 *
 * state	[x][y]		# items				}
 *		[x][y]		# reductions			}
 *		[x][y][z]	shift offset in packed mapping	} ...
 *		[x][y][z]	goto offset in packed mapping	}
 *		[x][y]		shift check			}
 *
 * reduction	[x][y]		rule offset in grammar		} ...
 *		[x][y]		rule number			}
 *
 * mapping	[...]		data table (spread + 2)
 *		[...]		check table (spread + 2)
 *
 *
 * format of SRP temporary data:
 *
 * header	[0]		version number
 *		[x][y][z]	# items
 *		[x][y][z]	shift table size
 *
 * item		[x][y]		rule offset in grammar		}
 *		[x][y]		rule number			} ...
 *		[x]		offset in rule			}
 *
 * shift 	[...]		shift table
 */

/*
 * NAME:	srp->load()
 * DESCRIPTION:	load a shift/reduce parser from string
 */
srp *srp_load(grammar, str, len)
char *grammar, *str;
Uint len;
{
    register srp *lr;
    register char *buf;
    register Uint i;
    register srpstate *state;
    char *rbuf;

    if (UCHAR(str[0]) != SRP_VERSION) {
	return srp_new(grammar);
    }

    lr = ALLOC(srp, 1);

    /* grammar info */
    lr->grammar = grammar;
    lr->ntoken = ((UCHAR(grammar[5]) + UCHAR(grammar[9])) << 8) +
		 UCHAR(grammar[6]) + UCHAR(grammar[10]);
    lr->nprod = (UCHAR(grammar[11]) << 8) + UCHAR(grammar[12]);

    lr->srpstr = buf = str;

    /* header */
    lr->nstates = (UCHAR(buf[1]) << 8) + UCHAR(buf[2]);
    lr->nexpanded = (UCHAR(buf[3]) << 8) + UCHAR(buf[4]);
    lr->nred = ((Uint) UCHAR(buf[5]) << 16) + (UCHAR(buf[6]) << 8) +
	       UCHAR(buf[7]);
    lr->gap = ((Uint) UCHAR(buf[8]) << 16) + (UCHAR(buf[9]) << 8) +
	      UCHAR(buf[10]);
    lr->spread = ((Uint) UCHAR(buf[11]) << 16) + (UCHAR(buf[12]) << 8) +
		 UCHAR(buf[13]);
    buf += 14;

    /* states */
    lr->sttsize = lr->nstates + 1;
    lr->sthsize = 0;
    lr->states = ALLOC(srpstate, lr->sttsize);
    lr->sthtab = (unsigned short *) NULL;
    lr->itc = (itchunk *) NULL;

    /* load states */
    rbuf = buf + lr->nstates * 12;
    for (i = lr->nstates, state = lr->states; i > 0; --i, state++) {
	buf = ss_load(buf, &rbuf, state);
    }
    buf = rbuf;

    /* load packed mapping */
    lr->mapsize = lr->spread + 2;
    lr->data = buf;
    buf += lr->spread + 2;
    lr->check = buf;
    buf += lr->spread + 2;
    lr->alloc = FALSE;

    lr->tmpstr = buf;

    /* sizes */
    lr->nitem = 0;
    lr->srpsize = (long) buf - (long) str;
    lr->tmpsize = len - lr->srpsize;
    lr->modified = lr->allocated = FALSE;

    /* shift hash table */
    lr->slc = (slchunk *) NULL;
    lr->nshift = 0;
    lr->shtsize = 0;
    lr->shhsize = 0;
    lr->shtab = (char *) NULL;
    lr->shhtab = (shlink **) NULL;

    return lr;
}

/*
 * NAME:	srp->loadtmp()
 * DESCRIPTION:	load the temporary data for a shift/reduce parser
 */
static void srp_loadtmp(lr)
register srp *lr;
{
    register Uint i, n;
    register srpstate *state;
    register char *p;
    Uint nrule;
    char *buf;

    nrule = (UCHAR(lr->grammar[13]) << 8) + UCHAR(lr->grammar[14]);

    buf = lr->tmpstr;
    lr->nitem = ((Uint) UCHAR(buf[1]) << 16) + (UCHAR(buf[2]) << 8) +
		UCHAR(buf[3]);
    lr->nshift = ((Uint) UCHAR(buf[4]) << 16) + (UCHAR(buf[5]) << 8) +
		 UCHAR(buf[6]);
    buf += 7;

    /* states */
    lr->sthsize = nrule << 2;
    lr->sthtab = ALLOC(unsigned short, lr->sthsize);
    memset(lr->sthtab, '\0', lr->sthsize * sizeof(unsigned short));
    for (i = 0, state = lr->states; i < lr->nstates; i++, state++) {
	if (state->nitem != 0) {
	    state->items = it_load(&lr->itc, state->nitem, &buf, lr->grammar);
	}
	ss_hash(lr->sthtab, lr->sthsize, lr->states, (unsigned short) i);
    }

    /* shifts */
    lr->shtsize = lr->nshift * 2;
    lr->shhsize = nrule << 2;
    lr->shtab = ALLOC(char, lr->shtsize);
    memcpy(lr->shtab, buf, lr->nshift);
    lr->shhtab = ALLOC(shlink*, lr->shhsize);
    memset(lr->shhtab, '\0', lr->shhsize * sizeof(shlink*));
    for (i = 0, p = buf; i != lr->nshift; i += n, p += n) { 
	n = (Uint) 4 * ((UCHAR(p[5]) << 8) + UCHAR(p[6])) + 7;
	sl_hash(lr->shhtab, lr->shhsize, &lr->slc, lr->shtab, p, n)->shifts =
							(long) p - (long) buf;
    }
}

/*
 * NAME:	srp->save()
 * DESCRIPTION:	save a shift/reduce parser to string
 */
bool srp_save(lr, str, len)
register srp *lr;
char **str;
Uint *len;
{
    register char *buf;
    register unsigned short i;
    register srpstate *state;
    char *rbuf;

    if (!lr->modified) {
	*str = lr->srpstr;
	*len = lr->srpsize + lr->tmpsize;
	return FALSE;
    }

    if (lr->nstates == lr->nexpanded) {
	lr->tmpsize = 0;
    }
    if (lr->allocated) {
	FREE(lr->srpstr);
    }
    lr->srpstr = buf = *str = ALLOC(char, *len = lr->srpsize + lr->tmpsize);

    /* header */
    *buf++ = SRP_VERSION;
    *buf++ = lr->nstates >> 8;
    *buf++ = lr->nstates;
    *buf++ = lr->nexpanded >> 8;
    *buf++ = lr->nexpanded;
    *buf++ = lr->nred >> 16;
    *buf++ = lr->nred >> 8;
    *buf++ = lr->nred;
    *buf++ = lr->gap >> 16;
    *buf++ = lr->gap >> 8;
    *buf++ = lr->gap;
    *buf++ = lr->spread >> 16;
    *buf++ = lr->spread >> 8;
    *buf++ = lr->spread;

    /* save states */
    rbuf = buf + lr->nstates * 12;
    for (i = lr->nstates, state = lr->states; i > 0; --i, state++) {
	buf = ss_save(state, buf, &rbuf);
    }
    buf = rbuf;

    /* save packed mapping */
    memcpy(buf, lr->data, lr->spread + 2);
    buf += lr->spread + 2;
    memcpy(buf, lr->check, lr->spread + 2);
    buf += lr->spread + 2;

    lr->modified = FALSE;
    lr->allocated = TRUE;
    if (lr->tmpsize == 0) {
	/* no tmp data */
	return TRUE;
    }

    lr->tmpstr = buf;

    /* tmp header */
    *buf++ = 0;
    *buf++ = lr->nitem >> 16;
    *buf++ = lr->nitem >> 8;
    *buf++ = lr->nitem;
    *buf++ = lr->nshift >> 16;
    *buf++ = lr->nshift >> 8;
    *buf++ = lr->nshift;

    /* save items */
    for (i = lr->nstates, state = lr->states; i > 0; --i, state++) {
	buf = it_save(state->items, buf, lr->grammar);
    }

    /* shift data */
    memcpy(buf, lr->shtab, lr->nshift);

    return TRUE;
}

/*
 * NAME:	srp->pack()
 * DESCRIPTION:	add a new set of shifts and gotos to the packed mapping
 */
static Int srp_pack(lr, check, from, to, n)
register srp *lr;
unsigned short *check, *from, *to;
register unsigned short n;
{
    register Uint i, j;
    register char *p;
    char *shifts;
    shlink *sl;
    Uint range, *offstab;
    Int offset;

    /*
     * check hash table
     */
    shifts = ALLOCA(char, j = (Uint) 4 * n + 7);
    p = shifts + 5;
    *p++ = n >> 8;
    *p++ = n;
    for (i = 0; i < n; i++) {
	*p++ = from[i] >> 8;
	*p++ = from[i];
	*p++ = to[i] >> 8;
	*p++ = to[i];
    }
    sl = sl_hash(lr->shhtab, lr->shhsize, &lr->slc, lr->shtab, shifts, j);
    if (sl->shifts != NOSHIFT) {
	/* same as before */
	AFREE(shifts);
	p = lr->shtab + sl->shifts;
	*check = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	return ((Int) SCHAR(p[2]) << 16) + ((UCHAR(p[3]) << 8) + UCHAR(p[4]));
    }

    /* not in hash table */
    if (lr->nshift + j > lr->shtsize) {
	/* grow shift table */
	i = (lr->nshift + j) * 2;
	lr->shtab = REALLOC(lr->shtab, char, lr->shtsize, i);
	lr->shtsize = i;
    }
    sl->shifts = lr->nshift;
    lr->nshift += j;
    lr->tmpsize += j;
    memcpy(lr->shtab + sl->shifts, shifts, j);
    AFREE(shifts);

    /* create offset table */
    offstab = ALLOCA(Uint, n);
    for (i = 0; i < n; i++) {
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
    for (i = lr->gap, p = &lr->check[i];
	 UCHAR(p[0]) != 0xff || UCHAR(p[1]) != 0xff;
	 i += 2, p += 2) ;
    lr->gap = i;

next:
    if (i + range >= lr->mapsize) {
	/* grow tables */
	j = (i + range) << 1;
	if (lr->alloc) {
	    lr->data = REALLOC(lr->data, char, lr->mapsize, j);
	    lr->check = REALLOC(lr->check, char, lr->mapsize, j);
	} else {
	    char *table;

	    table = ALLOC(char, j);
	    memcpy(table, lr->data, lr->mapsize);
	    lr->data = table;
	    table = ALLOC(char, j);
	    memcpy(table, lr->check, lr->mapsize);
	    lr->check = table;
	    lr->alloc = TRUE;
	}
	memset(lr->data + lr->mapsize, '\0', j - lr->mapsize);
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
	lr->srpsize += 2 * (i + range - lr->spread);
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
	*p++ = *check >> 8;
	*p = *check;
    } while (n != 0);

    p = lr->shtab + sl->shifts;
    offset /= 2;
    *p++ = *check >> 8;
    *p++ = *check;
    *p++ = offset >> 16;
    *p++ = offset >> 8;
    *p = offset;
    return offset;
}

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two unsigned shorts
 */
static int cmp(sh1, sh2)
cvoid *sh1, *sh2;
{
    return (*(unsigned short *) sh1 < *(unsigned short *) sh2) ?
	    -1 : (*(unsigned short *) sh1 == *(unsigned short *) sh2) ? 0 : 1;
}

/*
 * NAME:	srp->expand()
 * DESCRIPTION:	expand a state
 */
static srpstate *srp_expand(lr, state)
register srp *lr;
srpstate *state;
{
    register unsigned short i, n;
    register char *p;
    register item *it;
    item **itemtab, *next;
    unsigned short *tokens, *symbols, *targets;
    srpstate *newstate;
    unsigned short nred, nshift, ngoto;

    lr->modified = TRUE;
    if (state - lr->states == 1) {
	/* final state */
	state->nred = 0;
	lr->nexpanded++;
	return state;
    }

    if (lr->sthtab == (unsigned short *) NULL) {
	srp_loadtmp(lr);	/* load tmp info */
    }

    n = lr->ntoken + lr->nprod;
    itemtab = ALLOCA(item*, n);
    memset(itemtab, '\0', n * sizeof(item*));
    symbols = ALLOCA(unsigned short, n);
    targets = ALLOCA(unsigned short, n);
    tokens = ALLOCA(unsigned short, lr->ntoken);
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
		p = lr->grammar + 15 + (n << 1);
		p = lr->grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
		for (i = (UCHAR(p[0]) << 8) + UCHAR(p[1]), p += 2; i > 0; --i) {
		    it_add(&lr->itc, &state->items, p, n, 0, FALSE);
		    p += UCHAR(p[1]) + 2;
		}
	    }
	}
    }

    state->nred = nred;
    if (nred != 0) {
	if (nred > 1) {
	    state->reds.a = ALLOC(char, (Uint) nred << 2);
	    state->alloc = TRUE;
	}
	lr->nred += nred;
	lr->srpsize += (Uint) nred << 2;
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
	    p = &REDA(state)[(Uint) nred++ << 2];
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
    qsort(symbols, ngoto, sizeof(unsigned short), cmp);
    memcpy(symbols + ngoto, tokens, nshift * sizeof(unsigned short));
    AFREE(tokens);
    tokens = symbols + ngoto;
    qsort(tokens, nshift, sizeof(unsigned short), cmp);

    /*
     * create target table
     */
    for (i = 0; i < nshift + ngoto; i++) {
	newstate = &lr->states[lr->nstates];
	newstate->items = itemtab[symbols[i]];

	n = ss_hash(lr->sthtab, lr->sthsize, lr->states,
		    (unsigned short) lr->nstates);
	targets[i] = n;
	if (n == lr->nstates) {
	    /*
	     * new state
	     */
	    n = 0;
	    for (it = newstate->items; it != (item *) NULL; it = it->next) {
		n++;
	    }
	    lr->srpsize += 12;
	    lr->nitem += n;
	    lr->tmpsize += (Uint) 5 * n;
	    newstate->nitem = n;
	    newstate->nred = UNEXPANDED;
	    newstate->shoffset = NOSHIFT;
	    newstate->gtoffset = 0;
	    newstate->shcheck = 0;
	    newstate->alloc = FALSE;

	    if (++lr->nstates == lr->sttsize) {
		unsigned short save;

		/* grow table */
		save = state - lr->states;
		lr->states = REALLOC(lr->states, srpstate, lr->nstates,
				     lr->sttsize <<= 1);
		state = lr->states + save;
	    }
	}
    }

    /*
     * add shifts and gotos to packed mapping
     */
    if (nshift != 0) {
	state->shcheck = state - lr->states;
	state->shoffset = srp_pack(lr, &state->shcheck, tokens, targets + ngoto,
				   nshift);
    }
    if (ngoto != 0) {
	unsigned short dummy;

	dummy = -258;
	state->gtoffset = srp_pack(lr, &dummy, symbols, targets, ngoto);
    }
    AFREE(targets);
    AFREE(symbols);
    AFREE(itemtab);

    lr->nexpanded++;
    return state;
}

/*
 * NAME:	srp->check()
 * DESCRIPTION:	fetch reductions for a given state, possibly first expanding it
 */
short srp_check(lr, num, nredp, redp)
register srp *lr;
unsigned int num;
unsigned short *nredp;
char **redp;
{
    register srpstate *state;

    state = &lr->states[num];
    if (state->nred < 0) {
	state = srp_expand(lr, state);
	if (lr->srpsize + lr->tmpsize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
	    unsigned short save;

	    /*
	     * too much temporary data: attempt to expand all states
	     */
	    save = state - lr->states;
	    for (state = lr->states; lr->nstates != lr->nexpanded; state++) {
		if (lr->nstates > SHRT_MAX ||
		    lr->srpsize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
		    return -1;	/* too big */
		}
		if (state->nred < 0) {
		    state = srp_expand(lr, state);
		}
	    }
	    state = &lr->states[save];
	}
	if (lr->nstates > SHRT_MAX ||
	    lr->srpsize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
	    return -1;	/* too big */
	}
    }

    *nredp = state->nred;
    *redp = REDA(state);
    return lr->nstates;
}

/*
 * NAME:	srp->shift()
 * DESCRIPTION:	shift to a new state, if possible
 */
short srp_shift(lr, num, token)
register srp *lr;
unsigned int num, token;
{
    register Int n;
    register char *p;
    srpstate *state;

    state = &lr->states[num];
    n = state->shoffset;
    if (n != NOSHIFT) {
	n = (n + (Int) token) * 2;
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
short srp_goto(lr, num, symb)
register srp *lr;
unsigned int num, symb;
{
    register char *p;

    p = &lr->data[(lr->states[num].gtoffset + symb) * 2];
    return (UCHAR(p[0]) << 8) + UCHAR(p[1]);
}
