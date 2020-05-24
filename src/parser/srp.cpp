/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2020 DGD Authors (see the commit log for details)
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

# include "dgd.h"
# include "srp.h"

class Item : public ChunkAllocated {
public:
    Item *del();

    static Item *create(class ItChunk **c, char *ref, unsigned short ruleno,
			unsigned short offset, Item *next);
    static void add(ItChunk **c, Item **ri, char *ref, unsigned short ruleno,
		    unsigned short offset, bool sort);
    static Item *load(ItChunk **c, unsigned short n, char **buf, char *grammar);
    static char *save(Item *it, char *buf, char *grammar);

    char *ref;			/* pointer to rule in grammar */
    unsigned short ruleno;	/* rule number */
    unsigned short offset;	/* offset in rule */
    Item *next;			/* next in linked list */
};

# define ITCHUNKSZ	32

class ItChunk : public Chunk<Item, ITCHUNKSZ> {
};

/*
 * create a new item
 */
Item *Item::create(ItChunk **c, char *ref, unsigned short ruleno,
		   unsigned short offset, Item *next)
{
    Item *it;

    if (*c == (ItChunk *) NULL) {
	*c = new ItChunk;
    }
    it = chunknew (**c) Item;

    it->ref = ref;
    it->ruleno = ruleno;
    it->offset = offset;
    it->next = next;

    return it;
}

/*
 * delete an item
 */
Item *Item::del()
{
    Item *next;

    next = this->next;
    delete this;
    return next;
}

/*
 * add an item to a set
 */
void Item::add(ItChunk **c, Item **ri, char *ref, unsigned short ruleno,
	       unsigned short offset, bool sort)
{
    /*
     * add item to set
     */
    if (offset == UCHAR(ref[0]) << 1) {
	offset = UCHAR(ref[1]);	/* skip possible function at the end */
    }

    if (sort) {
	while (*ri != (Item *) NULL &&
	       ((*ri)->ref < ref ||
		((*ri)->ref == ref && (*ri)->offset <= offset))) {
	    if ((*ri)->ref == ref && (*ri)->offset == offset) {
		return;	/* already in set */
	    }
	    ri = &(*ri)->next;
	}
    } else {
	while (*ri != (Item *) NULL) {
	    if ((*ri)->ref == ref && (*ri)->offset == offset) {
		return;	/* already in set */
	    }
	    ri = &(*ri)->next;
	}
    }

    *ri = create(c, ref, ruleno, offset, *ri);
}

/*
 * load an item
 */
Item *Item::load(ItChunk **c, unsigned short n, char **buf, char *grammar)
{
    char *p;
    Item **ri;
    Item *it;
    char *ref;
    unsigned short ruleno;

    ri = &it;
    p = *buf;
    do {
	ref = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	p += 2;
	ruleno = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	p += 2;
	*ri = create(c, ref, ruleno, UCHAR(*p++), (Item *) NULL);
	ri = &(*ri)->next;
    } while (--n != 0);
    *buf = p;

    return it;
}

/*
 * save an item
 */
char *Item::save(Item *it, char *buf, char *grammar)
{
    int offset;

    while (it != (Item *) NULL) {
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


class SrpState {
public:
    char *load(char *buf, char **rbuf);
    char *save(char *buf, char **rbuf);

    static unsigned short hash(unsigned short *htab, Uint htabsize,
			       SrpState *states, unsigned short idx);

    Item *items;		/* rules and offsets */
    union {
	char e[4];		/* 1 */
	char *a;		/* > 1 */
    };				/* reductions */
    unsigned short nitem;	/* # items */
    short nred;			/* # reductions, -1 if unexpanded */
    Int shoffset;		/* offset for shifts */
    Int gtoffset;		/* offset for gotos */
    unsigned short shcheck;	/* shift offset check */
    unsigned short next;	/* next in linked list */
    bool alloc;			/* reductions allocated? */
};

# define REDA(state)   (((state)->nred == 1) ? (state)->e : (state)->a)
# define UNEXPANDED	-1
# define NOSHIFT	((Int) 0xff800000L)

/*
 * put a new state in the hash table, or return an old one
 */
unsigned short SrpState::hash(unsigned short *htab, Uint htabsize,
			      SrpState *states, unsigned short idx)
{
    Uint h;
    SrpState *newstate;
    Item *it, *it2;
    SrpState *s;
    unsigned short *sr;

    /* hash on items */
    newstate = &states[idx];
    h = 0;
    for (it = newstate->items; it != (Item *) NULL; it = it->next) {
	h ^= (uintptr_t) it->ref;
	h = (h >> 3) ^ (h << 29) ^ it->offset;
    }

    /* check state hash table */
    sr = &htab[(Uint) h % htabsize];
    s = &states[*sr];
    while (s != states) {
	it = newstate->items;
	it2 = s->items;
	while (it != (Item *) NULL && it2 != (Item *) NULL &&
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
 * load a srpstate
 */
char *SrpState::load(char *buf, char **rbuf)
{
    items = (Item *) NULL;
    nitem = (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    buf += 2;
    nred = (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    buf += 2;
    shoffset = ((Int) SCHAR(buf[0]) << 16) + (UCHAR(buf[1]) << 8) +
	       UCHAR(buf[2]);
    buf += 3;
    gtoffset = ((Int) SCHAR(buf[0]) << 16) + (UCHAR(buf[1]) << 8) +
	       UCHAR(buf[2]);
    buf += 3;
    shcheck = (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    buf += 2;

    if (nred > 0) {
	if (nred == 1) {
	    memcpy(e, *rbuf, 4);
	} else {
	    a = *rbuf;
	}
	*rbuf += 4 * nred;
    }
    alloc = FALSE;

    return buf;
}

/*
 * save a srpstate
 */
char *SrpState::save(char *buf, char **rbuf)
{
    *buf++ = nitem >> 8;
    *buf++ = nitem;
    *buf++ = nred >> 8;
    *buf++ = nred;
    *buf++ = shoffset >> 16;
    *buf++ = shoffset >> 8;
    *buf++ = shoffset;
    *buf++ = gtoffset >> 16;
    *buf++ = gtoffset >> 8;
    *buf++ = gtoffset;
    *buf++ = shcheck >> 8;
    *buf++ = shcheck;

    if (nred > 0) {
	memcpy(*rbuf, REDA(this), nred * 4);
	*rbuf += nred * 4;
    }

    return buf;
}


class ShLink : public ChunkAllocated {
public:
    static ShLink *hash(ShLink **htab, Uint htabsize, class SlChunk **c,
			char *shtab, char *shifts, Uint n);

    Int shifts;			/* offset in shift table */
    ShLink *next;		/* next in linked list */
};

# define SLCHUNKSZ	64

class SlChunk : public Chunk<ShLink, SLCHUNKSZ> {
};

/*
 * put a new shlink in the hash table, or return an old one
 */
ShLink *ShLink::hash(ShLink **htab, Uint htabsize, SlChunk **c, char *shtab,
		     char *shifts, Uint n)
{
    unsigned long h;
    Uint i;
    ShLink **ssl, *sl;

    /* search in hash table */
    shifts += 5;
    n -= 5;
    h = 0;
    for (i = n; i > 0; --i) {
	h = (h >> 3) ^ (h << 29) ^ UCHAR(*shifts++);
    }
    shifts -= n;
    ssl = &htab[h % htabsize];
    while (*ssl != (ShLink *) NULL) {
	if (memcmp(shtab + (*ssl)->shifts + 5, shifts, n) == 0) {
	    /* seen this one before */
	    return *ssl;
	}
	ssl = &(*ssl)->next;
    }

    if (*c == (SlChunk *) NULL) {
	*c = new SlChunk;
    }
    sl = chunknew (**c) ShLink;
    sl->next = *ssl;
    *ssl = sl;
    sl->shifts = NOSHIFT;

    return sl;
}


# define SRP_VERSION	1

Srp::Srp(char *grammar)
{
    this->grammar = grammar;
    nsstring = (UCHAR(grammar[9]) << 8) + UCHAR(grammar[10]);
    ntoken = nsstring +
	     ((UCHAR(grammar[5]) + UCHAR(grammar[11])) << 8) +
	     UCHAR(grammar[6]) + UCHAR(grammar[12]);
    nprod = (UCHAR(grammar[13]) << 8) + UCHAR(grammar[14]);
}

/*
 * delete shift/reduce parser
 */
Srp::~Srp()
{
    unsigned short i;
    SrpState *state;

    if (allocated) {
	FREE(srpstr);
    }
    delete itc;
    if (sthtab != (unsigned short *) NULL) {
	FREE(sthtab);
    }
    for (i = nstates, state = states; i > 0; --i, state++) {
	if (state->alloc) {
	    FREE(state->a);
	}
    }
    FREE(states);
    if (alloc) {
	FREE(data);
	FREE(check);
    }
    delete slc;
    if (shtab != (char *) NULL) {
	FREE(shtab);
    }
    if (shhtab != (ShLink **) NULL) {
	FREE(shhtab);
    }
}

/*
 * create new shift/reduce parser
 */
Srp *Srp::create(char *grammar)
{
    Srp *lr;
    char *p;
    Uint nrule;

    lr = new Srp(grammar);

    /* grammar info */
    nrule = (UCHAR(grammar[15]) << 8) + UCHAR(grammar[16]);

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
    lr->states = ALLOC(SrpState, lr->sttsize);
    lr->sthtab = ALLOC(unsigned short, lr->sthsize);
    memset(lr->sthtab, '\0', lr->sthsize * sizeof(unsigned short));
    lr->itc = (ItChunk *) NULL;

    /* state 0 */
    p = grammar + 17 + ((lr->ntoken + lr->nsstring) << 1);
    p = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
    lr->states[0].items = Item::create(&lr->itc, p + 2, lr->ntoken, 0,
				       (Item *) NULL);
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
    lr->slc = (SlChunk *) NULL;
    lr->nshift = 0;
    lr->shtsize = lr->mapsize;
    lr->shhsize = nrule << 2;
    lr->shtab = ALLOC(char, lr->shtsize);
    lr->shhtab = ALLOC(ShLink*, lr->shhsize);
    memset(lr->shhtab, '\0', lr->shhsize * sizeof(ShLink*));

    return lr;
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
 * shift	[...]		shift table
 */

/*
 * load a shift/reduce parser from string
 */
Srp *Srp::load(char *grammar, char *str, Uint len)
{
    Srp *lr;
    char *buf;
    Uint i;
    SrpState *state;
    char *rbuf;

    if (UCHAR(str[0]) != SRP_VERSION) {
	return create(grammar);
    }

    lr = new Srp(grammar);

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
    lr->states = ALLOC(SrpState, lr->sttsize);
    lr->sthtab = (unsigned short *) NULL;
    lr->itc = (ItChunk *) NULL;

    /* load states */
    rbuf = buf + lr->nstates * 12;
    for (i = lr->nstates, state = lr->states; i > 0; --i, state++) {
	buf = state->load(buf, &rbuf);
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
    lr->srpsize = (intptr_t) buf - (intptr_t) str;
    lr->tmpsize = len - lr->srpsize;
    lr->modified = lr->allocated = FALSE;

    /* shift hash table */
    lr->slc = (SlChunk *) NULL;
    lr->nshift = 0;
    lr->shtsize = 0;
    lr->shhsize = 0;
    lr->shtab = (char *) NULL;
    lr->shhtab = (ShLink **) NULL;

    return lr;
}

/*
 * load the temporary data for a shift/reduce parser
 */
void Srp::loadtmp()
{
    Uint i, n;
    SrpState *state;
    char *p;
    Uint nrule;
    char *buf;

    nrule = (UCHAR(grammar[15]) << 8) + UCHAR(grammar[16]);

    buf = tmpstr;
    nitem = ((Uint) UCHAR(buf[1]) << 16) + (UCHAR(buf[2]) << 8) + UCHAR(buf[3]);
    nshift = ((Uint) UCHAR(buf[4]) << 16) + (UCHAR(buf[5]) << 8) +
	     UCHAR(buf[6]);
    buf += 7;

    /* states */
    sthsize = nrule << 2;
    sthtab = ALLOC(unsigned short, sthsize);
    memset(sthtab, '\0', sthsize * sizeof(unsigned short));
    for (i = 0, state = states; i < nstates; i++, state++) {
	if (state->nitem != 0) {
	    state->items = Item::load(&itc, state->nitem, &buf, grammar);
	}
	SrpState::hash(sthtab, sthsize, states, (unsigned short) i);
    }

    /* shifts */
    shtsize = nshift * 2;
    shhsize = nrule << 2;
    shtab = ALLOC(char, shtsize);
    memcpy(shtab, buf, nshift);
    shhtab = ALLOC(ShLink*, shhsize);
    memset(shhtab, '\0', shhsize * sizeof(ShLink*));
    for (i = 0, p = buf; i != nshift; i += n, p += n) {
	n = (Uint) 4 * ((UCHAR(p[5]) << 8) + UCHAR(p[6])) + 7;
	ShLink::hash(shhtab, shhsize, &slc, shtab, p, n)->shifts
						= (intptr_t) p - (intptr_t) buf;
    }
}

/*
 * save a shift/reduce parser to string
 */
bool Srp::save(char **str, Uint *len)
{
    char *buf;
    unsigned short i;
    SrpState *state;
    char *rbuf;

    if (!modified) {
	*str = srpstr;
	*len = srpsize + tmpsize;
	return FALSE;
    }

    if (nstates == nexpanded) {
	tmpsize = 0;
    }
    if (allocated) {
	FREE(srpstr);
    }
    srpstr = buf = *str = ALLOC(char, *len = srpsize + tmpsize);

    /* header */
    *buf++ = SRP_VERSION;
    *buf++ = nstates >> 8;
    *buf++ = nstates;
    *buf++ = nexpanded >> 8;
    *buf++ = nexpanded;
    *buf++ = nred >> 16;
    *buf++ = nred >> 8;
    *buf++ = nred;
    *buf++ = gap >> 16;
    *buf++ = gap >> 8;
    *buf++ = gap;
    *buf++ = spread >> 16;
    *buf++ = spread >> 8;
    *buf++ = spread;

    /* save states */
    rbuf = buf + nstates * 12;
    for (i = nstates, state = states; i > 0; --i, state++) {
	buf = state->save(buf, &rbuf);
    }
    buf = rbuf;

    /* save packed mapping */
    memcpy(buf, data, spread + 2);
    buf += spread + 2;
    memcpy(buf, check, spread + 2);
    buf += spread + 2;

    modified = FALSE;
    allocated = TRUE;
    if (tmpsize == 0) {
	/* no tmp data */
	return TRUE;
    }

    tmpstr = buf;

    /* tmp header */
    *buf++ = 0;
    *buf++ = nitem >> 16;
    *buf++ = nitem >> 8;
    *buf++ = nitem;
    *buf++ = nshift >> 16;
    *buf++ = nshift >> 8;
    *buf++ = nshift;

    /* save items */
    for (i = nstates, state = states; i > 0; --i, state++) {
	buf = Item::save(state->items, buf, grammar);
    }

    /* shift data */
    memcpy(buf, shtab, nshift);

    return TRUE;
}

/*
 * add a new set of shifts and gotos to the packed mapping
 */
Int Srp::pack(unsigned short *check, unsigned short *from, unsigned short *to,
	      unsigned short n)
{
    Uint i, j;
    char *p;
    char *shifts;
    ShLink *sl;
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
    sl = ShLink::hash(shhtab, shhsize, &slc, shtab, shifts, j);
    if (sl->shifts != NOSHIFT) {
	/* same as before */
	AFREE(shifts);
	p = shtab + sl->shifts;
	*check = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	return ((Int) SCHAR(p[2]) << 16) + ((UCHAR(p[3]) << 8) + UCHAR(p[4]));
    }

    /* not in hash table */
    if (nshift + j > shtsize) {
	/* grow shift table */
	i = (nshift + j) * 2;
	shtab = REALLOC(shtab, char, shtsize, i);
	shtsize = i;
    }
    sl->shifts = nshift;
    nshift += j;
    tmpsize += j;
    memcpy(shtab + sl->shifts, shifts, j);
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
    for (i = gap, p = &this->check[i];
	 UCHAR(p[0]) != 0xff || UCHAR(p[1]) != 0xff;
	 i += 2, p += 2) ;
    gap = i;

next:
    if (i + range >= mapsize) {
	/* grow tables */
	j = (i + range) << 1;
	if (alloc) {
	    data = REALLOC(data, char, mapsize, j);
	    this->check = REALLOC(this->check, char, mapsize, j);
	} else {
	    char *table;

	    table = ALLOC(char, j);
	    memcpy(table, data, mapsize);
	    data = table;
	    table = ALLOC(char, j);
	    memcpy(table, this->check, mapsize);
	    this->check = table;
	    alloc = TRUE;
	}
	memset(data + mapsize, '\0', j - mapsize);
	memset(this->check + mapsize, '\xff', j - mapsize);
	mapsize = j;
    }

    /* match each symbol with free slot */
    for (j = 1; j < n; j++) {
	p += offstab[j];
	if (UCHAR(p[0]) != 0xff || UCHAR(p[1]) != 0xff) {
	    p = &this->check[i];
	    do {
		i += 2;
		p += 2;
	    } while (UCHAR(p[0]) != 0xff || UCHAR(p[1]) != 0xff);
	    goto next;
	}
    }
    AFREE(offstab);

    /* free slots found: adjust spread */
    if (i + range > spread) {
	srpsize += 2 * (i + range - spread);
	spread = i + range;
    }

    /* add to packed mapping */
    offset = i - offset;
    do {
	j = from[--n] * 2 + offset;
	p = &data[j];
	*p++ = to[n] >> 8;
	*p = to[n];
	p = &this->check[j];
	*p++ = *check >> 8;
	*p = *check;
    } while (n != 0);

    p = shtab + sl->shifts;
    offset /= 2;
    *p++ = *check >> 8;
    *p++ = *check;
    *p++ = offset >> 16;
    *p++ = offset >> 8;
    *p = offset;
    return offset;
}

/*
 * compare two unsigned shorts
 */
static int cmp(cvoid *sh1, cvoid *sh2)
{
    return (*(unsigned short *) sh1 < *(unsigned short *) sh2) ?
	    -1 : (*(unsigned short *) sh1 == *(unsigned short *) sh2) ? 0 : 1;
}

/*
 * expand a state
 */
SrpState *Srp::expand(SrpState *state)
{
    unsigned short i, n;
    char *p;
    Item *it;
    Item **itemtab, *next;
    unsigned short *tokens, *symbols, *targets;
    SrpState *newstate;
    unsigned short nred, nshift, ngoto;

    modified = TRUE;
    if (state - states == 1) {
	/* final state */
	state->nred = 0;
	nexpanded++;
	return state;
    }

    if (sthtab == (unsigned short *) NULL) {
	loadtmp();	/* load tmp info */
    }

    n = ntoken + nprod;
    itemtab = ALLOCA(Item*, n);
    memset(itemtab, '\0', n * sizeof(Item*));
    symbols = ALLOCA(unsigned short, n);
    targets = ALLOCA(unsigned short, n);
    tokens = ALLOCA(unsigned short, ntoken);
    nred = nshift = ngoto = 0;

    /*
     * compute closure of kernel item set
     */
    for (it = state->items; it != (Item *) NULL; it = it->next) {
	i = it->offset;
	p = it->ref + 1;
	if (i == UCHAR(*p++)) {
	    /* end of production */
	    nred++;
	} else {
	    p += i;
	    n = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    if (n >= ntoken) {
		p = grammar + 17 + ((n + nsstring) << 1);
		p = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
		for (i = (UCHAR(p[0]) << 8) + UCHAR(p[1]), p += 2; i > 0; --i) {
		    Item::add(&itc, &state->items, p, n, 0, FALSE);
		    p += UCHAR(p[1]) + 2;
		}
	    }
	}
    }

    state->nred = nred;
    if (nred != 0) {
	if (nred > 1) {
	    state->a = ALLOC(char, (Uint) nred << 2);
	    state->alloc = TRUE;
	}
	this->nred += nred;
	srpsize += (Uint) nred << 2;
	nred = 0;
    }

    /*
     * compute reductions and shifts
     */
    if (state == states) {
	symbols[ngoto++] = ntoken;
    }
    for (it = state->items; it != (Item *) NULL; it = it->next) {
	p = it->ref;
	if (it->offset == UCHAR(p[1])) {
	    /* reduction */
	    n = p - grammar;
	    p = &REDA(state)[(Uint) nred++ << 2];
	    *p++ = n >> 8;
	    *p++ = n;
	    *p++ = it->ruleno >> 8;
	    *p = it->ruleno;
	} else {
	    /* shift/goto */
	    p += 2 + it->offset;
	    n = (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    if (itemtab[n] == (Item *) NULL) {
		if (n < ntoken) {
		    tokens[nshift++] = n;
		} else {
		    symbols[ngoto++] = n;
		}
	    }
	    Item::add(&itc, &itemtab[n], it->ref, it->ruleno, it->offset + 2,
		      TRUE);
	}
    }

    /*
     * delete non-kernel items
     */
    for (it = state->items, i = state->nitem; --i > 0; it = it->next) ;
    next = it->next;
    it->next = (Item *) NULL;
    for (it = next; it != (Item *) NULL; it = it->del()) ;

    /*
     * sort and merge token and goto tables
     */
    std::qsort(symbols, ngoto, sizeof(unsigned short), cmp);
    memcpy(symbols + ngoto, tokens, nshift * sizeof(unsigned short));
    AFREE(tokens);
    tokens = symbols + ngoto;
    std::qsort(tokens, nshift, sizeof(unsigned short), cmp);

    /*
     * create target table
     */
    for (i = 0; i < nshift + ngoto; i++) {
	newstate = &states[nstates];
	newstate->items = itemtab[symbols[i]];

	n = SrpState::hash(sthtab, sthsize, states, (unsigned short) nstates);
	targets[i] = n;
	if (n == nstates) {
	    /*
	     * new state
	     */
	    n = 0;
	    for (it = newstate->items; it != (Item *) NULL; it = it->next) {
		n++;
	    }
	    srpsize += 12;
	    nitem += n;
	    tmpsize += (Uint) 5 * n;
	    newstate->nitem = n;
	    newstate->nred = UNEXPANDED;
	    newstate->shoffset = NOSHIFT;
	    newstate->gtoffset = 0;
	    newstate->shcheck = 0;
	    newstate->alloc = FALSE;

	    if (++nstates == sttsize) {
		unsigned short save;

		/* grow table */
		save = state - states;
		states = REALLOC(states, SrpState, nstates, sttsize <<= 1);
		state = states + save;
	    }
	}
    }

    /*
     * add shifts and gotos to packed mapping
     */
    if (nshift != 0) {
	state->shcheck = state - states;
	state->shoffset = pack(&state->shcheck, tokens, targets + ngoto,
			       nshift);
    }
    if (ngoto != 0) {
	unsigned short dummy;

	dummy = -258;
	state->gtoffset = pack(&dummy, symbols, targets, ngoto);
    }
    AFREE(targets);
    AFREE(symbols);
    AFREE(itemtab);

    nexpanded++;
    return state;
}

/*
 * fetch reductions for a given state, possibly first expanding it
 */
short Srp::reduce(unsigned int num, unsigned short *nredp, char **redp)
{
    SrpState *state;

    state = &states[num];
    if (state->nred < 0) {
	state = expand(state);
	if (srpsize + tmpsize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
	    unsigned short save;

	    /*
	     * too much temporary data: attempt to expand all states
	     */
	    save = state - states;
	    for (state = states; nstates != nexpanded; state++) {
		if (nstates > SHRT_MAX ||
		    srpsize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
		    return -1;	/* too big */
		}
		if (state->nred < 0) {
		    state = expand(state);
		}
	    }
	    state = &states[save];
	}
	if (nstates > SHRT_MAX || srpsize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
	    return -1;	/* too big */
	}
    }

    *nredp = state->nred;
    *redp = REDA(state);
    return nstates;
}

/*
 * shift to a new state, if possible
 */
short Srp::shift(unsigned int num, unsigned int token)
{
    Int n;
    char *p;
    SrpState *state;

    state = &states[num];
    n = state->shoffset;
    if (n != NOSHIFT) {
	n = (n + (Int) token) * 2;
	if (n >= 0 && n < mapsize) {
	    p = &check[n];
	    if ((UCHAR(p[0]) << 8) + UCHAR(p[1]) == state->shcheck) {
		/* shift works: return new state */
		p = &data[n];
		return (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    }
	}
    }

    /* shift failed */
    return -1;
}

/*
 * goto a new state
 */
short Srp::_goto(unsigned int num, unsigned int symb)
{
    char *p;

    p = &data[(states[num].gtoffset + symb) * 2];
    return (UCHAR(p[0]) << 8) + UCHAR(p[1]);
}
