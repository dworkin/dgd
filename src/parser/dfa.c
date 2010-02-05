/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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
# include "hash.h"
# include "str.h"
# include "dfa.h"

/*
 * NAME:	charset->neg()
 * DESCRIPTION:	negate a charset
 */
static void cs_neg(cs)
register Uint *cs;
{
    *cs++ ^= 0xffffffffL;
    *cs++ ^= 0xffffffffL;
    *cs++ ^= 0xffffffffL;
    *cs++ ^= 0xffffffffL;
    *cs++ ^= 0xffffffffL;
    *cs++ ^= 0xffffffffL;
    *cs++ ^= 0xffffffffL;
    *cs   ^= 0xffffffffL;
}

/*
 * NAME:	charset->or()
 * DESCRIPTION:	or two charsets
 */
static void cs_or(cs1, cs2)
register Uint *cs1, *cs2;
{
    *cs1++ |= *cs2++;
    *cs1++ |= *cs2++;
    *cs1++ |= *cs2++;
    *cs1++ |= *cs2++;
    *cs1++ |= *cs2++;
    *cs1++ |= *cs2++;
    *cs1++ |= *cs2++;
    *cs1   |= *cs2;
}

/*
 * NAME:	charset->sub()
 * DESCRIPTION:	subtract a charset from another one
 */
static void cs_sub(cs1, cs2)
register Uint *cs1, *cs2;
{
    *cs1++ &= ~*cs2++;
    *cs1++ &= ~*cs2++;
    *cs1++ &= ~*cs2++;
    *cs1++ &= ~*cs2++;
    *cs1++ &= ~*cs2++;
    *cs1++ &= ~*cs2++;
    *cs1++ &= ~*cs2++;
    *cs1   &= ~*cs2;
}

/*
 * NAME:	charset->intersect()
 * DESCRIPTION:	return TRUE if two character sets intersect, FALSE otherwise
 */
static bool cs_intersect(cs1, cs2)
register Uint *cs1, *cs2;
{
    register Uint i;

    i  = *cs1++ & *cs2++;
    i |= *cs1++ & *cs2++;
    i |= *cs1++ & *cs2++;
    i |= *cs1++ & *cs2++;
    i |= *cs1++ & *cs2++;
    i |= *cs1++ & *cs2++;
    i |= *cs1++ & *cs2++;
    i |= *cs1   & *cs2;

    return (i != 0);
}

/*
 * NAME:	charset->overlap()
 * DESCRIPTION:	Check if two character sets overlap.  Return TRUE if they do,
 *		or if the first set contains the second one.
 */
static bool cs_overlap(cs1, cs2, cs3, cs4)
register Uint *cs1, *cs2, *cs3, *cs4;
{
    register Uint s3, s4;

    s3  = *cs3 = *cs1 & *cs2++;  s4  = *cs4++ = *cs1++ & ~*cs3++;
    s3 |= *cs3 = *cs1 & *cs2++;  s4 |= *cs4++ = *cs1++ & ~*cs3++;
    s3 |= *cs3 = *cs1 & *cs2++;  s4 |= *cs4++ = *cs1++ & ~*cs3++;
    s3 |= *cs3 = *cs1 & *cs2++;  s4 |= *cs4++ = *cs1++ & ~*cs3++;
    s3 |= *cs3 = *cs1 & *cs2++;  s4 |= *cs4++ = *cs1++ & ~*cs3++;
    s3 |= *cs3 = *cs1 & *cs2++;  s4 |= *cs4++ = *cs1++ & ~*cs3++;
    s3 |= *cs3 = *cs1 & *cs2++;  s4 |= *cs4++ = *cs1++ & ~*cs3++;
    s3 |= *cs3 = *cs1 & *cs2;    s4 |= *cs4   = *cs1   & ~*cs3;

    return (s3 != 0 && s4 != 0);
}

/*
 * NAME:	charset->firstc()
 * DESCRIPTION:	find the first char in a charset
 */
static int cs_firstc(cset, c)
register Uint *cset;
register int c;
{
    register Uint x;

    while (c < 256) {
	if ((x=cset[c >> 5] >> (c & 31)) != 0) {
	    while ((x & 0xff) == 0) {
		x >>= 8;
		c += 8;
	    }
	    while ((x & 1) == 0) {
		x >>= 1;
		c++;
	    }
	    return c;
	}
	c += 32;
	c &= ~31;
    }

    /* not found */
    return -1;
}

/*
 * NAME:	charset->eclass()
 * DESCRIPTION:	convert a charset into an equivalence class
 */
static int cs_eclass(cset, eclass, class)
Uint *cset;
char *eclass;
int class;
{
    register int n, c;
    register Uint x;

    n = 0;
    for (c = cs_firstc(cset, 0); c < 256; c += 31, c &= ~31) {
	x = cset[c >> 5] >> (c & 31);
	if (x != 0) {
	    do {
		while ((x & 0xff) == 0) {
		    x >>= 8;
		    c += 8;
		}
		if (x & 1) {
		    eclass[c] = class;
		    n++;
		}
		x >>= 1;
		c++;
	    } while (x != 0);
	} else {
	    c++;
	}
    }

    return n;
}


typedef struct {
    hte chain;			/* hash table chain */
    char *rgx;			/* regular expression this position is in */
    unsigned short size;	/* size of position (length of string - 2) */
    unsigned short ruleno;	/* the rule this position is in */
    Uint nposn;			/* position number */
    bool final;			/* final position? */
    bool alloc;			/* position allocated separately? */
} rgxposn;

# define RPCHUNKSZ	32

typedef struct _rpchunk_ {
    int chunksz;		/* size of chunk */
    struct _rpchunk_ *next;	/* next in linked list */
    rgxposn rp[RPCHUNKSZ];	/* rgxposn chunk */
} rpchunk;

/*
 * NAME:	rgxposn->alloc()
 * DESCRIPTION:	allocate a new rgxposn (or return an old one)
 */
static rgxposn *rp_alloc(htab, posn, size, c, rgx, nposn, ruleno, final)
hashtab *htab;
char *posn, *rgx;
unsigned short size, ruleno;
Uint nposn;
rpchunk **c;
bool final;
{
    register rgxposn **rrp, *rp;

    rrp = (rgxposn **) ht_lookup(htab, posn, TRUE);
    if (*rrp != (rgxposn *) NULL) {
	return *rrp;	/* already exists */
    }

    if (*c == (rpchunk *) NULL || (*c)->chunksz == RPCHUNKSZ) {
	register rpchunk *x;

	x = ALLOC(rpchunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }
    rp = &(*c)->rp[(*c)->chunksz++];
    rp->chain.next = (hte *) *rrp;
    *rrp = rp;
    rp->chain.name = posn;
    rp->rgx = rgx;
    rp->size = size;
    rp->nposn = nposn;
    rp->ruleno = ruleno;
    rp->final = final;
    rp->alloc = FALSE;

    return rp;
}

/*
 * NAME:	rgxposn->new()
 * DESCRIPTION:	create a new rgxposn
 */
static rgxposn *rp_new(htab, posn, size, c, rgx, nposn, ruleno, final)
hashtab *htab;
char *posn, *rgx;
unsigned short size, ruleno;
Uint nposn;
rpchunk **c;
bool final;
{
    register rgxposn *rp;

    rp = rp_alloc(htab, posn, size, c, rgx, nposn, ruleno, final);
    if (rp->nposn == nposn) {
	strcpy(rp->chain.name = ALLOC(char, size + 3), posn);
	rp->alloc = TRUE;
    }
    return rp;
}

/*
 * NAME:	rgxposn->clear()
 * DESCRIPTION:	free all rgxposns
 */
static void rp_clear(c)
register rpchunk *c;
{
    register rpchunk *f;
    register rgxposn *rp;
    register int i;

    while (c != (rpchunk *) NULL) {
	for (rp = c->rp, i = c->chunksz; i != 0; rp++, --i) {
	    if (rp->alloc) {
		FREE(rp->chain.name);
	    }
	}
	f = c;
	c = c->next;
	FREE(f);
    }
}

/*
 * NAME:	rgxposn->transposn()
 * DESCRIPTION:	convert a transition into a position
 */
static bool rp_transposn(rgx, trans, buf, buflen)
char *rgx, *trans, *buf;
unsigned short *buflen;
{
    char a[256], b[256], c[256], heap[256];
    register char *p, *q;
    register int n, place;
    register unsigned short i, j, len;

    memset(a, '\0', 256);
    heap[0] = len = 0;

    /* from transitions to places */
    if (trans == (char *) NULL) {
	n = 1;
	b[0] = 1;
    } else {
	n = 0;
	for (p = trans; *p != '\0'; p++) {
	    for (i = UCHAR(*p); ; i = place + 1) {
		place = UCHAR(rgx[i]) + 1;
		if (!a[place]) {
		    a[place] = TRUE;
		    if (place != UCHAR(rgx[0])) {
			switch (rgx[place]) {
			case '|':
			    /* branch */
			    b[n++] = place + 2;
			    continue;

			case '+':
			    /* pattern+ */
			    b[n++] = place + 2;
			    if (place < i) {
				continue;
			    }
			    break;

			default:
			    /* add to heap */
			    for (i = ++len, j = i >> 1;
				 UCHAR(heap[j]) > place;
				 i = j, j >>= 1) {
				heap[i] = heap[j];
			    }
			    heap[i] = place;
			    break;
			}
		    }
		}
		break;
	    }
	}
    }
    b[n] = '\0';

    /* closure: alternate between b and c */
    for (p = b, q = c; *p != '\0'; p = q, q = (q == b) ? c : b) {
	n = 0;
	do {
	    place = UCHAR(*p++);
	    if (!a[place]) {
		a[place] = TRUE;
		if (place != UCHAR(rgx[0])) {
		    switch (rgx[place]) {
		    case '|':
			/* branch */
			q[n++] = place + 2;
			q[n++] = UCHAR(rgx[place + 1]) + 1;
			continue;

		    case '+':
			/* pattern+ */
			q[n++] = place + 2;
			continue;
		    }

		    /* add to heap */
		    for (i = ++len, j = i >> 1;
			 UCHAR(heap[j]) > place;
			 i = j, j >>= 1) {
			heap[i] = heap[j];
		    }
		    heap[i] = place;
		}
	    }
	} while (*p != '\0');
	q[n] = '\0';
    }

    /* from heap to buf */
    *buflen = len;
    for (p = buf; len != 0; --len) {
	*p++ = heap[1];
	n = UCHAR(heap[len]);
	for (i = 1, j = 2; j < len; i = j, j <<= 1) {
	    if (UCHAR(heap[j]) > UCHAR(heap[j + 1])) {
		j++;
	    }
	    if (n <= UCHAR(heap[j])) {
		break;
	    }
	    heap[i] = heap[j];
	}
	heap[i] = n;
    }
    *p = '\0';

    return a[UCHAR(rgx[0])];	/* final? */
}

static Uint bits[] = {
    0x00000001L, 0x00000003L, 0x00000007L, 0x0000000fL,
    0x0000001fL, 0x0000003fL, 0x0000007fL, 0x000000ffL,
    0x000001ffL, 0x000003ffL, 0x000007ffL, 0x00000fffL,
    0x00001fffL, 0x00003fffL, 0x00007fffL, 0x0000ffffL,
    0x0001ffffL, 0x0003ffffL, 0x0007ffffL, 0x000fffffL,
    0x001fffffL, 0x003fffffL, 0x007fffffL, 0x00ffffffL,
    0x01ffffffL, 0x03ffffffL, 0x07ffffffL, 0x0fffffffL,
    0x1fffffffL, 0x3fffffffL, 0x7fffffffL, 0xffffffffL
};

/*
 * NAME:	rgxposn->cset()
 * DESCRIPTION:	create input sets for a position
 */
static void rp_cset(rp, cset)
rgxposn *rp;
register Uint *cset;
{
    register char *p, *q;
    register int c, n, x;
    bool negate;

    for (q = rp->chain.name + 2; *q != '\0'; q++) {
	memset(cset, '\0', 32);
	p = rp->rgx + UCHAR(*q);
	switch (*p) {
	case '[':
	    /* character class */
	    p++;
	    if (*p == '^') {
		negate = TRUE;
		p++;
	    } else {
		negate = FALSE;
	    }
	    do {
		if (*p == '\\') {
		    p++;
		}
		c = UCHAR(*p++);
		cset[c >> 5] |= 1 << (c & 31);
		if (p[0] == '-' && p[1] != ']') {
		    n = UCHAR(p[1]) - c;
		    if (n != 0) {
			x = 32 - (++c & 31);
			if (x > n) {
			    x = n;
			}
			cset[c >> 5] |= bits[x - 1] << (c & 31);
			c += x;
			n -= x;
			while (n >= 32) {
			    cset[c >> 5] |= 0xffffffffL;
			    c += 32;
			    n -= 32;
			}
			if (n != 0) {
			    cset[c >> 5] |= bits[n - 1];
			}
		    }
		    p += 2;
		}
	    } while (*p != ']');
	    if (negate) {
		cs_neg(cset);
	    }
	    break;

	case '.':
	    /* anything */
	    memset(cset, -1, 32);
	    break;

	case '\\':
	    /* escaped char */
	    p++;
	default:
	    /* normal char */
	    c = UCHAR(*p);
	    cset[c >> 5] |= 1 << (c & 31);
	    break;
	}

	cset += 8;
    }
}

/*
 * NAME:	rgxposn->trans()
 * DESCRIPTION:	perform a transition on a position, given an input set
 */
static bool rp_trans(rp, cset, posn, size)
rgxposn *rp;
Uint *cset;
char *posn;
unsigned short *size;
{
    char trans[256];
    register char *p, *q;
    register int c, n, x;
    char *t;
    Uint found;
    bool negate;

    t = trans;
    for (q = rp->chain.name + 2; *q != '\0'; q++) {
	p = rp->rgx + UCHAR(*q);
	found = 0;
	switch (*p) {
	case '[':
	    /* character class */
	    p++;
	    if (*p == '^') {
		negate = TRUE;
		p++;
	    } else {
		negate = FALSE;
	    }
	    do {
		if (*p == '\\') {
		    p++;
		}
		c = UCHAR(*p++);
		found |= cset[c >> 5] & 1 << (c & 31);
		if (p[0] == '-' && p[1] != ']') {
		    n = UCHAR(p[1]) - c;
		    if (n != 0) {
			x = 32 - (++c & 31);
			if (x > n) {
			    x = n;
			}
			found |= cset[c >> 5] & (bits[x - 1] << (c & 31));
			c += x;
			n -= x;
			while (n >= 32) {
			    found |= cset[c >> 5] & 0xffffffffL;
			    c += 32;
			    n -= 32;
			}
			if (n != 0) {
			    found |= cset[c >> 5] & bits[n - 1];
			}
		    }
		    p += 2;
		}
	    } while (*p != ']');
	    if (negate) {
		found = !found;
	    }
	    break;

	case '.':
	    /* anything */
	    found = 1;
	    break;

	case '\\':
	    /* escaped char */
	    p++;
	default:
	    /* normal char */
	    c = UCHAR(*p);
	    found = cset[c >> 5] & (1 << (c & 31));
	    break;
	}
	if (found != 0) {
	    *t++ = p - rp->rgx + 1;
	}
    }
    *t = '\0';

    return rp_transposn(rp->rgx, trans, posn, size);
}

/*
 * NAME:	rgxposn->load()
 * DESCRIPTION:	load a rgxposn from a buffer
 */
static rgxposn *rp_load(htab, c, nposn, buf, grammar)
hashtab *htab;
rpchunk **c;
Uint nposn;
register char *buf;
char *grammar;
{
    char *rgx;
    unsigned short ruleno, size;
    bool final;

    rgx = grammar + (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    ruleno = (UCHAR(buf[2]) << 8) + UCHAR(buf[3]);
    buf += 4;
    if (*buf == '\0') {
	final = TRUE;
	buf++;
    } else {
	final = FALSE;
    }
    size = UCHAR(*buf++);

    return rp_alloc(htab, buf, size, c, rgx, nposn, ruleno, final);
}

/*
 * NAME:	rgxposn->save()
 * DESCRIPTION:	save a rgxposn to a buffer
 */
static char *rp_save(rp, buf, grammar)
register rgxposn *rp;
register char *buf;
char *grammar;
{
    unsigned short rgx;

    rgx = rp->rgx - grammar;
    *buf++ = rgx >> 8;
    *buf++ = rgx;
    *buf++ = rp->ruleno >> 8;
    *buf++ = rp->ruleno;
    if (rp->final) {
	*buf++ = '\0';
    }
    *buf++ = rp->size;
    memcpy(buf, rp->chain.name, rp->size + 3);
    return buf + rp->size + 3;
}


typedef struct {
    union {			/* regexp positions */
	rgxposn *e;		/* 1 */
	rgxposn **a;		/* > 1 */
    } posn;
    union {			/* strings */
	unsigned short e[2];	/* 1, 2 */
	unsigned short *a;	/* > 2 */
    } str;
    char *trans;		/* transitions */
    unsigned short nposn;	/* number of positions */
    unsigned short nstr;	/* number of string positions */
    unsigned short len;		/* string length */
    unsigned short ntrans;	/* number of transitions */
    short final;		/* rule number, -1: not final */
    unsigned short next;	/* next in hash chain */
    bool alloc;			/* transitions allocated? */
} dfastate;

# define POSNA(state)	(((state)->nposn == 1) ? \
			  &(state)->posn.e : (state)->posn.a)
# define STRA(state)	(((state)->nstr <= 2) ? \
			  (state)->str.e : (state)->str.a)

/*
 * NAME:	dfastate->hash()
 * DESCRIPTION:	put a new state in the hash table, or return an old one
 */
static unsigned short ds_hash(htab, htabsize, states, idx)
unsigned short *htab, idx;
Uint htabsize;
dfastate *states;
{
    register unsigned long x;
    register rgxposn **posn;
    register unsigned short n, *str;
    register dfastate *newstate, *ds;
    unsigned short *dds;

    /* hash on position and string pointers */
    newstate = &states[idx];
    x = newstate->len ^ newstate->final;
    for (n = newstate->nposn, posn = POSNA(newstate); n > 0; --n) {
	x = (x >> 3) ^ (x << 29) ^ (unsigned long) *posn++;
    }
    for (n = newstate->nstr, str = STRA(newstate); n > 0; --n) {
	x = (x >> 3) ^ (x << 29) ^ (unsigned long) *str++;
    }

    /* check state hash table */
    posn = POSNA(newstate);
    str = STRA(newstate);
    dds = &htab[(Uint) x % htabsize];
    ds = &states[*dds];
    while (ds != states) {
	if (newstate->len == ds->len && newstate->final == ds->final &&
	    newstate->nposn == ds->nposn && newstate->nstr == ds->nstr &&
	    memcmp(posn, POSNA(ds), newstate->nposn * sizeof(rgxposn*)) == 0 &&
	    memcmp(str, STRA(ds), newstate->nstr * sizeof(unsigned short)) == 0)
	{
	    /* state already exists */
	    return *dds;
	}
	dds = &ds->next;
	ds = &states[*dds];
    }

    newstate->next = *dds;
    return *dds = idx;
}

# define TRANS_NONE	0	/* no transitions */
# define TRANS_ZERO	1	/* all transitions to state 0 */
# define TRANS_STATES	2	/* normal transitions */

/*
 * NAME:	dfastate->load()
 * DESCRIPTION:	load a dfastate from a buffer
 */
static char *ds_load(state, buf, ntrans, zerotrans)
register dfastate *state;
register char *buf;
register unsigned short ntrans;
char *zerotrans;
{
    state->posn.a = (rgxposn **) NULL;
    state->str.a = (unsigned short *) NULL;
    state->nposn = state->nstr = state->len = 0;
    state->alloc = FALSE;
    state->final = (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    buf += 2;
    switch (*buf++) {
    case TRANS_NONE:
	state->ntrans = 0;
	break;

    case TRANS_ZERO:
	state->ntrans = 256;
	state->trans = zerotrans;
	break;

    case TRANS_STATES:
	state->ntrans = ntrans;
	state->trans = buf;
	buf += ntrans << 1;
	break;
    }

    return buf;
}

/*
 * NAME:	dfastate->loadtmp()
 * DESCRIPTION:	load dfastate temporary data from a buffer
 */
static char *ds_loadtmp(state, sbuf, pbuf, htab, c, nposn, grammar)
register dfastate *state;
register char *sbuf;
char *pbuf, *grammar;
hashtab *htab;
rpchunk **c;
Uint *nposn;
{
    register rgxposn *rp, **rrp;
    register unsigned short i, *s;
    char *posn;

    state->nposn = (UCHAR(sbuf[0]) << 8) + UCHAR(sbuf[1]);
    state->nstr = (UCHAR(sbuf[2]) << 8) + UCHAR(sbuf[3]);
    sbuf += 4;
    state->len = UCHAR(*sbuf++);

    if (state->nposn != 0) {
	if (state->nposn != 1) {
	    rrp = state->posn.a = ALLOC(rgxposn*, state->nposn);
	} else {
	    rrp = &state->posn.e;
	}
	for (i = state->nposn; i > 0; --i) {
	    posn = pbuf + ((Uint) UCHAR(sbuf[0]) << 16) +
		   (UCHAR(sbuf[1]) << 8) + UCHAR(sbuf[2]);
	    sbuf += 3;
	    rp = *rrp++ = rp_load(htab, c, *nposn, posn, grammar);
	    if (rp->nposn == *nposn) {
		(*nposn)++;
	    }
	}
    }
    if (state->nstr != 0) {
	if (state->nstr > 2) {
	    s = state->str.a = ALLOC(unsigned short, state->nstr);
	} else {
	    s = state->str.e;
	}
	for (i = state->nstr; i > 0; --i) {
	    *s++ = (UCHAR(sbuf[0]) << 8) + UCHAR(sbuf[1]);
	    sbuf += 2;
	}
    }

    return sbuf;
}

/*
 * NAME:	dfastate->save()
 * DESCRIPTION:	save a dfastate to a buffer
 */
static char *ds_save(state, buf)
register dfastate *state;
register char *buf;
{
    buf[0] = state->final >> 8;
    buf[1] = state->final;
    buf += 2;
    if (state->ntrans == 0) {
	*buf++ = TRANS_NONE;
    } else if (state->nposn == 0 && state->nstr == 0) {
	*buf++ = TRANS_ZERO;
    } else {
	*buf++ = TRANS_STATES;
	memcpy(buf, state->trans, state->ntrans << 1);
	buf += state->ntrans << 1;
    }

    return buf;
}

/*
 * NAME:	dfastate->savetmp()
 * DESCRIPTION:	save dfastate temporary data to a buffer
 */
static char *ds_savetmp(state, sbuf, pbuf, pbase, ptab, nposn, grammar)
register dfastate *state;
register char *sbuf;
char **pbuf, *pbase, *grammar;
Uint *ptab, *nposn;
{
    register rgxposn *rp, **rrp;
    register unsigned short i, *s;
    Uint n;

    *sbuf++ = state->nposn >> 8;
    *sbuf++ = state->nposn;
    *sbuf++ = state->nstr >> 8;
    *sbuf++ = state->nstr;
    *sbuf++ = state->len;

    rrp = POSNA(state);
    for (i = state->nposn; i > 0; --i) {
	rp = *rrp++;
	if (rp->nposn == *nposn) {
	    ptab[(*nposn)++] = (long) *pbuf - (long) pbase;
	    *pbuf = rp_save(rp, *pbuf, grammar);
	}
	n = ptab[rp->nposn];
	*sbuf++ = n >> 16;
	*sbuf++ = n >> 8;
	*sbuf++ = n;
    }

    s = STRA(state);
    for (i = state->nstr; i > 0; --i) {
	*sbuf++ = *s >> 8;
	*sbuf++ = *s++;
    }

    return sbuf;
}


struct _dfa_ {
    char *source;		/* source grammar */
    char *grammar;		/* reference grammar */
    char *strings;		/* offset of strings in grammar */
    unsigned short nsstrings;	/* # strings in source grammar */
    short whitespace;		/* whitespace rule or -1 */
    short nomatch;		/* nomatch rule or -1 */

    bool modified;		/* dfa modified */
    bool allocated;		/* dfa strings allocated locally */
    Uint dfasize;		/* size of state machine */
    Uint tmpssize;		/* size of temporary state data */
    Uint tmppsize;		/* size of temporary posn data */
    char *dfastr;		/* saved dfa */
    char *tmpstr;		/* saved temporary data */

    unsigned short nregexp;	/* # regexps */
    Uint nposn;			/* number of unique positions */
    rpchunk *rpc;		/* current rgxposn chunk */
    hashtab *posnhtab;		/* position hash table */

    unsigned short nstates;	/* # states */
    unsigned short nexpanded;	/* # expanded states */
    unsigned short endstates;	/* # states with no valid transitions */
    Uint sttsize;		/* state table size */
    Uint sthsize;		/* size of state hash table */
    dfastate *states;		/* dfa states */
    unsigned short *sthtab;	/* state hash table */

    unsigned short ecnum;	/* number of equivalence classes */
    char *ecsplit;		/* equivalence class split history */
    char *ecmembers;		/* members per equivalence class */
    Uint *ecset;		/* equivalence class sets */
    char eclass[256];		/* equivalence classes */

    char zerotrans[2 * 256];	/* shared zero transitions */
};

# define DFA_VERSION	1

/*
 * NAME:	dfa->new()
 * DESCRIPTION:	create new dfa instance
 */
dfa *dfa_new(source, grammar)
char *source;
register char *grammar;
{
    char posn[258];
    unsigned short nstrings;
    register dfa *fa;
    register dfastate *state;
    bool final;

    fa = ALLOC(dfa, 1);

    /* grammar info */
    fa->source = source;
    fa->grammar = grammar;
    fa->whitespace = (UCHAR(grammar[1]) << 8) + UCHAR(grammar[2]);
    fa->nomatch = (UCHAR(grammar[3]) << 8) + UCHAR(grammar[4]);
    fa->nregexp = (UCHAR(grammar[5]) << 8) + UCHAR(grammar[6]);
    fa->nsstrings = (UCHAR(grammar[9]) << 8) + UCHAR(grammar[10]);
    nstrings = fa->nsstrings + (UCHAR(grammar[11]) << 8) + UCHAR(grammar[12]);
    fa->strings = grammar + 17 + (fa->nregexp << 1);

    /* size info */
    fa->modified = TRUE;
    fa->allocated = FALSE;
    fa->dfasize = 8 + 256 + 3;		/* header + eclasses + state 1 */
    fa->tmpssize = 4 + 1 + 5;		/* header + ecsplit + state 1 */
    fa->tmppsize = 0;
    fa->dfastr = (char *) NULL;
    fa->tmpstr = (char *) NULL;

    /* equivalence classes */
    fa->ecnum = 1;
    fa->ecsplit = ALLOC(char, 256 + 256 + 32 * 256);
    fa->ecmembers = fa->ecsplit + 256;
    fa->ecset = (Uint *) (fa->ecmembers + 256);
    memset(fa->eclass, '\0', 256);
    memset(fa->ecmembers, '\0', 256);
    memset(fa->ecset, -1, 32);
    memset(fa->ecset + 8, '\0', 32 * 255);

    /* positions */
    fa->nposn = (UCHAR(grammar[7]) << 8) + UCHAR(grammar[8]);
    fa->rpc = (rpchunk *) NULL;
    fa->posnhtab = ht_new((fa->nposn + 1) << 2, 257, FALSE);

    /* states */
    fa->nstates = 2;
    fa->sttsize = (Uint) (fa->nposn + nstrings + 1) << 1;
    fa->sthsize = (Uint) fa->sttsize << 1;
    fa->nexpanded = 0;
    fa->endstates = 1;
    fa->states = ALLOC(dfastate, fa->sttsize);
    fa->sthtab = ALLOC(unsigned short, fa->sthsize);
    memset(fa->sthtab, '\0', sizeof(unsigned short) * fa->sthsize);

    /* initial states */
    state = &fa->states[0];
    state->posn.a = (rgxposn **) NULL;
    state->str.a = (unsigned short *) NULL;
    state->trans = (char *) NULL;
    state->nposn = state->nstr = 0;
    state->ntrans = state->len = 0;
    (state++)->final = -1;
    state->posn.a = (fa->nposn > 1) ?
		     ALLOC(rgxposn*, fa->nposn) : (rgxposn **) NULL;
    state->str.a = (nstrings > 2) ?
		    ALLOC(unsigned short, nstrings) : (unsigned short *) NULL;
    state->trans = (char *) NULL;
    state->nposn = fa->nposn;
    state->nstr = nstrings;
    state->ntrans = state->len = 0;
    state->final = -1;
    state->alloc = FALSE;
    grammar += 17;
    /* initial positions */
    if (state->nposn == 0 && state->nstr == 0) {
	/* no valid transitions from initial state */
	state->ntrans = 256;
	state->trans = fa->zerotrans;
	fa->endstates++;
    } else {
	register rgxposn **rrp;
	register unsigned short i, j, n, *s;
	register char *rgx;
	unsigned short size;

	rrp = POSNA(state);
	for (i = j = 0; i < fa->nregexp; i++) {
	    rgx = fa->grammar + (UCHAR(grammar[0]) << 8) + UCHAR(grammar[1]);
	    grammar += 2;
	    n = j + (UCHAR(rgx[0]) << 8) + UCHAR(rgx[1]);
	    rgx += 2;
	    while (j < n) {
		final = rp_transposn(rgx, (char *) NULL, posn + 2, &size);
		if (final && state->final < 0) {
		    state->final = i;
		}
		posn[0] = 1 + j / 255;
		posn[1] = 1 + j % 255;
		*rrp++ = rp_new(fa->posnhtab, posn, size, &fa->rpc, rgx,
				(Uint) j++, i, final);
		fa->tmpssize += 3;
		fa->tmppsize += 8 + size + final;
		rgx += UCHAR(rgx[0]) + 1;
	    }
	}
	/* initial strings */
	for (i = 0, s = STRA(state); i < nstrings; i++) {
	    *s++ = i;
	}
	fa->tmpssize += nstrings << 1;
    }
    /* add to hashtable */
    ds_hash(fa->sthtab, fa->sthsize, fa->states, 1);

    /* zero transitions */
    memset(fa->zerotrans, '\0', 2 * 256);

    return fa;
}

/*
 * NAME:	dfa->del()
 * DESCRIPTION:	delete a dfa instance
 */
void dfa_del(fa)
register dfa *fa;
{
    register dfastate *state;
    register unsigned short i;

    if (fa->allocated) {
	FREE(fa->dfastr);
    }
    if (fa->ecsplit != (char *) NULL) {
	FREE(fa->ecsplit);
    }
    if (fa->rpc != (rpchunk *) NULL) {
	rp_clear(fa->rpc);
    }
    if (fa->posnhtab != (hashtab *) NULL) {
	ht_del(fa->posnhtab);
    }
    for (i = fa->nstates, state = &fa->states[1]; --i > 0; state++) {
	if (state->nposn > 1) {
	    FREE(state->posn.a);
	}
	if (state->nstr > 2) {
	    FREE(state->str.a);
	}
	if (state->alloc) {
	    FREE(state->trans);
	}
    }
    FREE(fa->states);
    if (fa->sthtab != (unsigned short *) NULL) {
	FREE(fa->sthtab);
    }
    FREE(fa);
}

/*
 * NAME:	dfa->extend()
 * DESCRIPTION:	extend transition table
 */
static void dfa_extend(fa, state, limit)
register dfa *fa;
register dfastate *state;
register unsigned short limit;
{
    register char *p, *q;
    register unsigned short i;

    /* extend transition table */
    if (!state->alloc) {
	p = ALLOC(char, 2 * 256);
	memcpy(p, state->trans, state->ntrans << 1);
	state->trans = p;
	state->alloc = TRUE;
    }
    p = state->trans + (state->ntrans << 1);
    for (i = state->ntrans; i <= limit; i++) {
	q = &state->trans[UCHAR(fa->ecsplit[i]) << 1];
	*p++ = *q++;
	*p++ = *q;
    }
    state->ntrans = i;
}

/*
 * state & eclass format:
 *
 * header	[0]	version number
 *		[x][y]	# states
 *		[x][y]	# expanded states
 *		[x][y]	# end states
 *		[x]	# equivalence classes
 * eclass	[...]	1 - 256 equivalence classes
 *
 * state 	[x][y]	final				} ...
 *		[...]	optional: transitions		}
 *
 *
 * temporary data format:
 *
 * header	[0]	version number
 *		[x][y]	number of positions
 * ecsplit	[...]	256 ecsplit data
 *
 * state	[x][y]	# positions			}
 *		[x][y]	# strings			}
 * 		[x]	len				} ...
 *		[...]	position data			}
 *		[...]	string data			}
 *
 * position	[x][y]	regexp			}
 *		[x][y]	ruleno			}
 *		[0]	optional: final position	} ...
 *		[x]	size				}
 *		[...]	position data			}
 */

/*
 * NAME:	dfa->load()
 * DESCRIPTION:	load dfa from string
 */
dfa *dfa_load(source, grammar, str, len)
char *source, *grammar, *str;
Uint len;
{
    register dfa *fa;
    register dfastate *state;
    register unsigned short i;
    register char *buf;
    unsigned short nstrings;

    if (str[0] != DFA_VERSION) {
	return dfa_new(source, grammar);
    }

    fa = ALLOC(dfa, 1);
    fa->dfastr = buf = str;

    /* grammar info */
    fa->source = source;
    fa->grammar = grammar;
    fa->whitespace = (UCHAR(grammar[1]) << 8) + UCHAR(grammar[2]);
    fa->nomatch = (UCHAR(grammar[3]) << 8) + UCHAR(grammar[4]);
    fa->nregexp = (UCHAR(grammar[5]) << 8) + UCHAR(grammar[6]);
    fa->nsstrings = (UCHAR(grammar[9]) << 8) + UCHAR(grammar[10]);
    nstrings = fa->nsstrings + (UCHAR(grammar[11]) << 8) + UCHAR(grammar[12]);
    fa->strings = grammar + 17 + (fa->nregexp << 1);

    /* positions */
    fa->nposn = (UCHAR(grammar[7]) << 8) + UCHAR(grammar[8]);
    fa->rpc = (rpchunk *) NULL;
    fa->posnhtab = (hashtab *) NULL;

    /* states 1 */
    fa->nstates = (UCHAR(buf[1]) << 8) + UCHAR(buf[2]);
    fa->nexpanded = (UCHAR(buf[3]) << 8) + UCHAR(buf[4]);
    fa->endstates = (UCHAR(buf[5]) << 8) + UCHAR(buf[6]);
    fa->sttsize = fa->nstates + 1;
    fa->sthsize = (Uint) (fa->nposn + nstrings + 1) << 2;
    fa->states = ALLOC(dfastate, fa->sttsize);
    fa->sthtab = (unsigned short *) NULL;

    /* equivalence classes */
    fa->ecnum = UCHAR(buf[7]) + 1;
    buf += 8;
    memcpy(fa->eclass, buf, 256);
    buf += 256;
    fa->ecsplit = (char *) NULL;
    fa->ecmembers = (char *) NULL;
    fa->ecset = (Uint *) NULL;

    /* states 2 */
    fa->states[0].posn.a = (rgxposn **) NULL;
    fa->states[0].str.a = (unsigned short *) NULL;
    fa->states[0].trans = (char *) NULL;
    fa->states[0].nposn = fa->states[0].nstr = 0;
    fa->states[0].ntrans = fa->states[0].len = 0;
    fa->states[0].final = -1;
    for (i = fa->nstates, state = &fa->states[1]; --i > 0; state++) {
	buf = ds_load(state, buf, fa->ecnum, fa->zerotrans);
    }

    /* temporary data */
    fa->tmpstr = buf;

    /* size info */
    fa->dfasize = (long) buf - (long) str;
    fa->tmpssize = 0;
    fa->tmppsize = len - fa->dfasize;
    fa->modified = fa->allocated = FALSE;

    /* zero transitions */
    memset(fa->zerotrans, '\0', 2 * 256);

    return fa;
}

/*
 * NAME:	dfa->loadtmp()
 * DESCRIPTION:	load dfa tmp info
 */
static void dfa_loadtmp(fa)
register dfa *fa;
{
    register dfastate *state;
    register unsigned short i;
    register int c;
    register char *buf;

    buf = fa->tmpstr;
    fa->nposn = ((Uint) UCHAR(buf[1]) << 16) + (UCHAR(buf[2]) << 8) +
		UCHAR(buf[3]);
    buf += 4;

    /* equivalence classes */
    fa->ecsplit = ALLOC(char, 256 + 256 + 32 * 256);
    fa->ecmembers = fa->ecsplit + 256;
    fa->ecset = (Uint *) (fa->ecmembers + 256);
    memcpy(fa->ecsplit, buf, fa->ecnum);
    buf += fa->ecnum;
    memset(fa->ecmembers, '\0', 256);
    memset(fa->ecset, '\0', 32 * 256);
    for (i = 256; i > 0; ) {
	--i;
	c = UCHAR(fa->eclass[i]);
	fa->ecmembers[c]++;
	fa->ecset[(c << 3) + (i >> 5)] |= 1 << (i & 31);
    }

    /* positions */
    fa->posnhtab = ht_new((fa->nposn + 1) << 2, 257, FALSE);

    /* states */
    fa->sthtab = ALLOC(unsigned short, fa->sthsize);
    memset(fa->sthtab, '\0', sizeof(unsigned short) * fa->sthsize);

    fa->nposn = 0;
    for (i = 1, state = &fa->states[1]; i < fa->nstates; i++, state++) {
	buf = ds_loadtmp(state, buf, fa->tmpstr, fa->posnhtab, &fa->rpc,
			 &fa->nposn, fa->grammar);
	ds_hash(fa->sthtab, fa->sthsize, fa->states, i);
    }

    /* size info */
    fa->tmpssize = (long) buf - (long) fa->tmpstr;
    fa->tmppsize -= fa->tmpssize;
}

/*
 * NAME:	dfa->save()
 * DESCRIPTION:	save dfa to string
 */
bool dfa_save(fa, str, len)
register dfa *fa;
char **str;
Uint *len;
{
    register unsigned short i;
    register char *buf;
    register dfastate *state;
    char *pbuf;
    Uint *ptab, nposn;

    if (!fa->modified) {
	*str = fa->dfastr;
	*len = fa->dfasize + fa->tmpssize + fa->tmppsize;
	return FALSE;
    }

    if (fa->nstates == fa->nexpanded + fa->endstates) {
	fa->tmpssize = fa->tmppsize = 0;
    }
    if (fa->allocated) {
	FREE(fa->dfastr);
    }
    fa->dfastr = buf = *str =
		 ALLOC(char, *len = fa->dfasize + fa->tmpssize + fa->tmppsize);
    *buf++ = DFA_VERSION;
    *buf++ = fa->nstates >> 8;
    *buf++ = fa->nstates;
    *buf++ = fa->nexpanded >> 8;
    *buf++ = fa->nexpanded;
    *buf++ = fa->endstates >> 8;
    *buf++ = fa->endstates;
    *buf++ = fa->ecnum - 1;
    memcpy(buf, fa->eclass, 256);
    buf += 256;

    for (i = fa->nstates, state = &fa->states[1]; --i > 0; state++) {
	if (state->ntrans != 0 && state->ntrans < fa->ecnum) {
	    dfa_extend(fa, state, fa->ecnum - 1);
	}
	buf = ds_save(state, buf);
    }

    fa->modified = FALSE;
    fa->allocated = TRUE;
    if (fa->tmpssize + fa->tmppsize == 0) {
	/* no tmp data */
	return TRUE;
    }

    fa->tmpstr = buf;
    pbuf = buf + fa->tmpssize;
    *buf++ = 0;
    *buf++ = fa->nposn >> 16;
    *buf++ = fa->nposn >> 8;
    *buf++ = fa->nposn;
    memcpy(buf, fa->ecsplit, fa->ecnum);
    buf += fa->ecnum;

    ptab = ALLOCA(Uint, fa->nposn);
    nposn = 0;
    for (i = fa->nstates, state = &fa->states[1]; --i > 0; state++) {
	buf = ds_savetmp(state, buf, &pbuf, fa->tmpstr, ptab, &nposn,
			 fa->grammar);
    }
    AFREE(ptab);

    return TRUE;
}

/*
 * NAME:	dfa->ecsplit()
 * DESCRIPTION:	split up equivalence classes along the borders of character
 *		sets
 */
static void dfa_ecsplit(fa, iset, cset, ncset)
register dfa *fa;
Uint *iset, *cset, ncset;
{
    Uint ec1[8], ec2[8];
    register Uint i;
    register int n, c;

    for (c = cs_firstc(iset, 0); c >= 0; c = cs_firstc(iset, c + 1)) {
	for (i = 0; i < ncset; i++) {
	    /*
	     * get the equivalence class of the first char in the input set
	     */
	    n = UCHAR(fa->eclass[c]);
	    if (fa->ecmembers[n] == 1) {
		break;	/* only one character left */
	    }
	    if (cs_overlap(fa->ecset + (n << 3), cset, ec1, ec2)) {
		/*
		 * create new equivalence class
		 */
		memcpy(fa->ecset + (n << 3), ec1, sizeof(ec1));
		memcpy(fa->ecset + (fa->ecnum << 3), ec2, sizeof(ec2));
		fa->ecsplit[fa->ecnum] = n;
		fa->ecmembers[n] -= fa->ecmembers[fa->ecnum] =
				    cs_eclass(ec2, fa->eclass, fa->ecnum);
		fa->ecnum++;
		fa->dfasize += fa->nexpanded << 1;
		fa->tmpssize++;
	    }
	    cset += 8;
	}
	cset -= i << 3;

	/* remove from input set */
	cs_sub(iset, fa->ecset + (UCHAR(fa->eclass[c]) << 3));
    }
}

/*
 * NAME:	dfa->newstate()
 * DESCRIPTION:	get the positions and strings for a new state
 */
static unsigned short dfa_newstate(fa, state, newstate, ecset, cset)
dfa *fa;
register dfastate *state, *newstate;
Uint *ecset, *cset;
{
    char posn[130];
    register unsigned short i, n, *s;
    register rgxposn *rp, **rrp;
    register char *p;
    unsigned short size;
    Uint posnsize;
    bool final;

    newstate->trans = (char *) NULL;
    newstate->nposn = newstate->nstr = newstate->ntrans = 0;
    newstate->len = state->len + 1;
    newstate->final = -1;
    newstate->alloc = FALSE;
    posnsize = 0;

    /* positions */
    for (i = state->nposn, rrp = POSNA(state); i > 0; --i, rrp++) {
	rp = *rrp;
	for (n = rp->size; n > 0; --n) {
	    if (cs_intersect(ecset, cset)) {
		final = rp_trans(rp, ecset, posn + 2, &size);
		if (size != 0) {
		    posn[0] = rp->chain.name[0];
		    posn[1] = rp->chain.name[1];
		    rp = rp_new(fa->posnhtab, posn, size, &fa->rpc, rp->rgx,
				fa->nposn, rp->ruleno, final);
		    if (rp->nposn == fa->nposn) {
			/* new position */
			fa->nposn++;
			posnsize += 8 + rp->size + final;
		    }
		    newstate->posn.a[newstate->nposn++] = rp;
		}
		if (final && newstate->final < 0) {
		    newstate->final = rp->ruleno;
		}
		cset += n << 3;
		break;
	    }
	    cset += 8;
	}
    }

    /* strings */
    for (i = state->nstr, s = STRA(state); i > 0; --i, s++) {
	if (*s < fa->nsstrings) {
	    p = fa->strings + (*s << 2);
	    n = UCHAR(fa->source[(UCHAR(p[0]) << 16) + (UCHAR(p[1]) << 8) +
				 UCHAR(p[2]) + state->len]);
	    p += 3;
	} else {
	    p = fa->strings + (fa->nsstrings << 2) +
		((*s - fa->nsstrings) << 1);
	    p = fa->grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    n = UCHAR(p[newstate->len]);
	}
	if (ecset[n >> 5] & (1 << (n & 31))) {
	    if (newstate->len == UCHAR(p[0])) {
		/* end of string */
		newstate->final = fa->nregexp + *s;
	    } else {
		/* add string */
		newstate->str.a[newstate->nstr++] = *s;
	    }
	}
    }

    return posnsize;
}

/*
 * NAME:	dfa->expand()
 * DESCRIPTION:	expand a state
 */
static dfastate *dfa_expand(fa, state)
dfa *fa;
dfastate *state;
{
    Uint iset[8];
    register Uint *cset, *ecset, ncset;
    register rgxposn **rrp;
    register unsigned short i, n, *s;
    register char *p;
    dfastate *newstate;
    rgxposn **newposn;
    unsigned short *newstr;
    Uint size;

    if (fa->posnhtab == (hashtab *) NULL) {
	dfa_loadtmp(fa);	/* load tmp info */
    }

    memset(iset, '\0', sizeof(iset));

    /* allocate character sets for strings and positions */
    ncset = state->nstr;
    for (i = state->nposn, rrp = POSNA(state); i > 0; --i, rrp++) {
	ncset += (*rrp)->size;
    }
    cset = ALLOCA(Uint, ncset << 3);

    /* construct character sets for all string chars */
    for (i = state->nstr, s = STRA(state); i > 0; --i, s++) {
	if (*s < fa->nsstrings) {
	    p = fa->strings + (*s << 2);
	    p = fa->source + (UCHAR(p[0]) << 16) + (UCHAR(p[1]) << 8) +
		UCHAR(p[2]);
	} else {
	    p = fa->strings + (fa->nsstrings << 2) +
		((*s - fa->nsstrings) << 1);
	    p = fa->grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]) + 1;
	}
	n = UCHAR(p[state->len]);
	memset(cset, '\0', 32);
	cset[n >> 5] |= 1 << (n & 31);
	iset[n >> 5] |= 1 << (n & 31);	/* also add to input set */
	cset += 8;
    }

    /* construct character sets for all positions */
    for (i = state->nposn, rrp = POSNA(state); i > 0; --i, rrp++) {
	rp_cset(*rrp, cset);
	for (n = (*rrp)->size; n > 0; --n) {
	    cs_or(iset, cset);		/* add to input set */
	    cset += 8;
	}
    }
    cset -= ncset << 3;

    /*
     * adjust equivalence classes
     */
    dfa_ecsplit(fa, iset, cset, ncset);

    /*
     * for all equivalence classes, compute transition states
     */
    if (state->nposn != 0) {
	newposn = ALLOCA(rgxposn*, state->nposn);
    }
    if (state->nstr != 0) {
	newstr = ALLOCA(unsigned short, state->nstr);
    }
    p = state->trans = ALLOC(char, 2 * 256);
    state->ntrans = fa->ecnum;
    state->alloc = TRUE;
    cset += (Uint) state->nstr << 3;
    for (i = fa->ecnum, ecset = fa->ecset; i > 0; --i, ecset += 8) {
	/* prepare new state */
	newstate = &fa->states[fa->nstates];

	/* flesh out new state */
	newstate->posn.a = newposn;
	newstate->str.a = newstr;
	size = dfa_newstate(fa, state, newstate, ecset, cset);

	if (newstate->nposn == 0 && newstate->nstr == 0 && newstate->final < 0)
	{
	    /* stuck in state 0 */
	    n = 0;
	} else {
	    if (newstate->nposn <= 1) {
		if (newstate->nposn == 0) {
		    newstate->posn.a = (rgxposn **) NULL;
		} else {
		    newstate->posn.e = newposn[0];
		}
	    }
	    if (newstate->nstr <= 2) {
		if (newstate->nstr == 0) {
		    newstate->str.a = (unsigned short *) NULL;
		    newstate->len = 0;
		} else {
		    memcpy(newstate->str.e, newstr, 2 * sizeof(unsigned short));
		}
	    }

	    n = ds_hash(fa->sthtab, fa->sthsize, fa->states,
			(unsigned short) fa->nstates);
	    if (n == fa->nstates) {
		/*
		 * genuinely new state
		 */
		if (newstate->nposn > 1) {
		    newstate->posn.a = ALLOC(rgxposn*, newstate->nposn);
		    memcpy(newstate->posn.a, newposn,
			   newstate->nposn * sizeof(rgxposn*));
		}
		if (newstate->nstr > 2) {
		    newstate->str.a = ALLOC(unsigned short, newstate->nstr);
		    memcpy(newstate->str.a, newstr,
			   newstate->nstr * sizeof(unsigned short));
		}
		if (newstate->nposn == 0 && newstate->nstr == 0) {
		    newstate->ntrans = 256;
		    newstate->trans = fa->zerotrans;
		    fa->endstates++;
		}
		fa->dfasize += 3;
		fa->tmpssize += 5 + newstate->nposn * 3 + newstate->nstr * 2;
		fa->tmppsize += size;

		if (++fa->nstates == fa->sttsize) {
		    /* grow table */
		    size = state - fa->states;
		    fa->states = REALLOC(fa->states, dfastate, fa->nstates,
					 fa->sttsize <<= 1);
		    state = fa->states + size;
		}
	    }
	}

	*p++ = n >> 8;
	*p++ = n;
    }

    if (state->nstr != 0) {
	AFREE(newstr);
    }
    if (state->nposn != 0) {
	AFREE(newposn);
    }
    AFREE(cset - ((Uint) state->nstr << 3));

    fa->modified = TRUE;
    fa->nexpanded++;
    fa->dfasize += fa->ecnum << 1;
    return state;
}

/*
 * NAME:	dfa->scan()
 * DESCRIPTION:	Scan input, while lazily constructing a DFA.
 *		Return values:	[0 ..>	token
 *				-1	end of string
 *				-2	Invalid token
 *				-3	DFA too large (deallocate)
 */
short dfa_scan(fa, str, strlen, token, len)
register dfa *fa;
string *str;
ssizet *strlen, *len;
char **token;
{
    register ssizet size;
    register unsigned short eclass;
    register char *p, *q;
    register dfastate *state;
    short final;
    ssizet fsize, nomatch;

    nomatch = 0;
    size = *strlen;
    *token = str->text + str->len - size;

    while (size != 0) {
	state = &fa->states[1];
	final = -1;
	p = str->text + str->len - size;

	while (size != 0) {
	    eclass = UCHAR(fa->eclass[UCHAR(*p)]);
	    if (state->ntrans <= eclass) {
		if (state->ntrans == 0) {
		    /* expand state */
		    if (state == fa->states) {
			break;	/* stuck in state 0 */
		    }
		    state = dfa_expand(fa, state);
		    if (fa->dfasize + fa->tmpssize + fa->tmppsize >
					    (Uint) MAX_AUTOMSZ * USHRT_MAX) {
			unsigned short save;

			/*
			 * too much temporary data: attempt to expand
			 * all states
			 */
			save = state - fa->states;
			for (state = &fa->states[1];
			     fa->nstates != fa->nexpanded + fa->endstates;
			     state++) {
			    if (fa->nstates > USHRT_MAX - 256 ||
				fa->dfasize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
				return DFA_TOOBIG;
			    }
			    if (state->ntrans == 0) {
				state = dfa_expand(fa, state);
			    }
			}
			state = &fa->states[save];
		    }
		    if (fa->nstates > USHRT_MAX - 256 ||
			fa->dfasize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
			return DFA_TOOBIG;
		    }
		    eclass = UCHAR(fa->eclass[UCHAR(*p)]);
		} else {
		    /* extend transition table */
		    dfa_extend(fa, state, eclass);
		}
	    }

	    /* transition */
	    --size;
	    p++;
	    q = &state->trans[eclass << 1];
	    state = &fa->states[(UCHAR(q[0]) << 8) + UCHAR(q[1])];

	    /* check if final state */
	    if (state->final >= 0) {
		final = state->final;
		fsize = size;
	    }
	}

	if (final >= 0) {
	    if (nomatch != 0) {
		if (fa->nomatch != fa->whitespace) {
		    *len = nomatch;
		    return fa->nomatch;
		}
		*token += nomatch;
		nomatch = 0;
	    }

	    /* in a final state */
	    size = fsize;
	    if (final != fa->whitespace) {
		*len = *strlen - size;
		*strlen = size;
		return final;
	    }
	    /* else whitespace: continue */
	    *token = p - 1;
	    *strlen = size;
	} else if (fa->nomatch >= 0) {
	    nomatch++;
	    size = --*strlen;
	} else {
	    return DFA_REJECT;
	}
    }

    if (nomatch != 0 && fa->nomatch != fa->whitespace) {
	*len = nomatch;
	return fa->nomatch;
    }

    return DFA_EOS;
}
