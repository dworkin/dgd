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
    cs[0] ^= 0xffffffffL;
    cs[1] ^= 0xffffffffL;
    cs[2] ^= 0xffffffffL;
    cs[3] ^= 0xffffffffL;
    cs[4] ^= 0xffffffffL;
    cs[5] ^= 0xffffffffL;
    cs[6] ^= 0xffffffffL;
    cs[7] ^= 0xffffffffL;
}

/*
 * NAME:	charset->and()
 * DESCRIPTION:	and two charsets
 */
static void cs_and(cs1, cs2)
register Uint *cs1, *cs2;
{
    cs1[0] &= cs2[0];
    cs1[1] &= cs2[1];
    cs1[2] &= cs2[2];
    cs1[3] &= cs2[3];
    cs1[4] &= cs2[4];
    cs1[5] &= cs2[5];
    cs1[6] &= cs2[6];
    cs1[7] &= cs2[7];
}

/*
 * NAME:	charset->or()
 * DESCRIPTION:	or two charsets
 */
static void cs_or(cs1, cs2)
register Uint *cs1, *cs2;
{
    cs1[0] |= cs2[0];
    cs1[1] |= cs2[1];
    cs1[2] |= cs2[2];
    cs1[3] |= cs2[3];
    cs1[4] |= cs2[4];
    cs1[5] |= cs2[5];
    cs1[6] |= cs2[6];
    cs1[7] |= cs2[7];
}

/*
 * NAME:	charset->sub()
 * DESCRIPTION:	subtract a charset from another one
 */
static void cs_sub(cs1, cs2)
register Uint *cs1, *cs2;
{
    cs1[0] &= ~cs2[0];
    cs1[1] &= ~cs2[1];
    cs1[2] &= ~cs2[2];
    cs1[3] &= ~cs2[3];
    cs1[4] &= ~cs2[4];
    cs1[5] &= ~cs2[5];
    cs1[6] &= ~cs2[6];
    cs1[7] &= ~cs2[7];
}

/*
 * NAME:	charset->intersect()
 * DESCRIPTION:	return TRUE if two character sets intersect, FALSE otherwise
 */
static bool cs_intersect(cs1, cs2)
register Uint *cs1, *cs2;
{
    register Uint i;

    i  = cs1[0] & cs2[0];
    i |= cs1[1] & cs2[1];
    i |= cs1[2] & cs2[2];
    i |= cs1[3] & cs2[3];
    i |= cs1[4] & cs2[4];
    i |= cs1[5] & cs2[5];
    i |= cs1[6] & cs2[6];
    i |= cs1[7] & cs2[7];

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

    s3  = cs3[0] = cs1[0] & cs2[0];	s4  = cs4[0] = cs1[0] & ~cs3[0];
    s3 |= cs3[1] = cs1[1] & cs2[1];	s4 |= cs4[1] = cs1[1] & ~cs3[1];
    s3 |= cs3[2] = cs1[2] & cs2[2];	s4 |= cs4[2] = cs1[2] & ~cs3[2];
    s3 |= cs3[3] = cs1[3] & cs2[3];	s4 |= cs4[3] = cs1[3] & ~cs3[3];
    s3 |= cs3[4] = cs1[4] & cs2[4];	s4 |= cs4[4] = cs1[4] & ~cs3[4];
    s3 |= cs3[5] = cs1[5] & cs2[5];	s4 |= cs4[5] = cs1[5] & ~cs3[5];
    s3 |= cs3[6] = cs1[6] & cs2[6];	s4 |= cs4[6] = cs1[6] & ~cs3[6];
    s3 |= cs3[7] = cs1[7] & cs2[7];	s4 |= cs4[7] = cs1[7] & ~cs3[7];

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
    short size;			/* size of position (length of string - 2) */
    short nposn;		/* position number */
    short ruleno;		/* the rule this position is in */
    bool final;			/* final position? */
    bool alloc;			/* position allocated separately? */
} rgxposn;

# define RPCHUNKSZ	32

typedef struct _rpchunk_ {
    rgxposn rp[RPCHUNKSZ];	/* rgxposn chunk */
    int chunksz;		/* size of chunk */
    struct _rpchunk_ *next;	/* next in linked list */
} rpchunk;

/*
 * NAME:	rgxposn->alloc()
 * DESCRIPTION:	allocate a new rgxposn (or return an old one)
 */
static rgxposn *rp_alloc(htab, posn, size, c, rgx, nposn, ruleno, final)
hashtab *htab;
char *posn, *rgx;
int size, nposn, ruleno;
rpchunk **c;
bool final;
{
    register rgxposn **rrp, *rp;

    rrp = (rgxposn **) ht_lookup(htab, posn, TRUE);
    if (*rrp != (rgxposn *) NULL) {
	return *rrp;	/* already exists */
    }

    if (*c == (rpchunk *) NULL || (*c)->chunksz == RPCHUNKSZ) {
	rpchunk *x;

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
int size, nposn, ruleno;
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
int *buflen;
{
    char a[256], b[256], c[256], heap[256];
    register char *p, *q;
    register int n, len, place;
    register unsigned int i, j;

    memset(a, '\0', 256);
    heap[0] = len = 0;

    /* from transitions to places */
    if (trans == (char *) NULL) {
	n = 1;
	b[0] = 1;
    } else {
	n = 0;
	for (p = trans; *p != '\0'; p++) {
	    place = rgx[UCHAR(*p)] + 1;
	    if (!a[place]) {
		a[place] = TRUE;
		if (place != UCHAR(rgx[0])) {
		    switch (rgx[place]) {
		    case '|':
			/* branch */
			b[n++] = place + 2;
			b[n++] = UCHAR(rgx[place + 1]) + 1;
			continue;

		    case '+':
			/* pattern+ */
			b[n++] = place + 2;
			if (place < UCHAR(*p)) {
			    b[n++] = UCHAR(rgx[place + 1]) + 1;
			}
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
 * DESCRIPTION:	create an input set for a position
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
		    n = p[1] - c;
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
int *size;
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
		    n = p[1] - c;
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
int nposn;
register char *buf;
char *grammar;
{
    char *rgx;
    unsigned int ruleno, size;
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
    unsigned int rgx;

    rgx = rp->rgx - grammar;
    buf[0] = rgx >> 8;
    buf[1] = rgx;
    buf[2] = rp->ruleno >> 8;
    buf[3] = rp->ruleno;
    buf += 4;
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
static short ds_hash(htab, htabsize, states, idx)
short *htab;
int htabsize, idx;
dfastate *states;
{
    register unsigned long x;
    register int n;
    register rgxposn **posn;
    register short *str;
    register dfastate *newstate, *ds;
    short *dds;

    /* hash on position and string pointers */
    newstate = &states[idx];
    x = newstate->len ^ newstate->final;
    for (n = newstate->nposn, posn = POSNA(newstate); --n >= 0; ) {
	x = (x >> 3) ^ (x << 29) ^ (unsigned long) *posn++;
    }
    for (n = newstate->nstr, str = STRA(newstate); --n >= 0; ) {
	x = (x >> 3) ^ (x << 29) ^ (unsigned long) *str++;
    }
    x = (Uint) x % htabsize;

    /* check state hash table */
    posn = POSNA(newstate);
    str = STRA(newstate);
    dds = &htab[x];
    ds = &states[*dds];
    while (ds != states &&
	   (newstate->len != ds->len || newstate->final != ds->final ||
	    newstate->nposn != ds->nposn || newstate->nstr != ds->nstr ||
	    memcmp(posn, POSNA(ds), newstate->nposn * sizeof(rgxposn*)) != 0 ||
	    memcmp(str, STRA(ds), newstate->nstr * sizeof(short)) != 0)) {
	dds = &ds->next;
	ds = &states[*dds];
    }

    if (ds != states) {
	return *dds;	/* state already exists */
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
register unsigned int ntrans;
char *zerotrans;
{
    state->posn.a = (rgxposn **) NULL;
    state->str.a = (short *) NULL;
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
short *nposn;
{
    register int i;
    register rgxposn *rp, **rrp;
    register short *s;
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
	for (i = state->nposn; --i >= 0; ) {
	    posn = pbuf + (UCHAR(sbuf[0]) << 8) + UCHAR(sbuf[1]);
	    sbuf += 2;
	    rp = *rrp++ = rp_load(htab, c, *nposn, posn, grammar);
	    if (rp->nposn == *nposn) {
		(*nposn)++;
	    }
	}
    }
    if (state->nstr != 0) {
	if (state->nstr > 2) {
	    s = state->str.a = ALLOC(short, state->nstr);
	} else {
	    s = state->str.e;
	}
	for (i = state->nstr; --i >= 0; ) {
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
char *sbuf, **pbuf, *pbase, *grammar;
short *ptab, *nposn;
{
    register char *p;
    register rgxposn *rp, **rrp;
    register int i;
    register unsigned short *s;
    unsigned short n;

    p = sbuf;
    p[0] = state->nposn >> 8;
    p[1] = state->nposn;
    p[2] = state->nstr >> 8;
    p[3] = state->nstr;
    p[4] = state->len;
    sbuf += 5;

    p = *pbuf;
    rrp = POSNA(state);
    for (i = state->nposn; --i >= 0; ) {
	rp = *rrp++;
	if (rp->nposn == *nposn) {
	    ptab[(*nposn)++] = p - pbase;
	    p = rp_save(rp, p, grammar);
	}
	n = ptab[rp->nposn];
	sbuf[0] = n >> 8;
	sbuf[1] = n;
	sbuf += 2;
    }
    *pbuf = p;

    p = sbuf;
    s = STRA(state);
    for (i = state->nstr; --i >= 0; ) {
	p[0] = *s >> 8;
	p[1] = *s++;
	p += 2;
    }

    return p;
}


struct _dfa_ {
    char *grammar;		/* reference grammar */
    char *strings;		/* offset of strings in grammar */
    bool whitespace;		/* true if token 0 is whitespace */

    bool dfachanged;		/* dfa needs saving */
    bool tmpchanged;		/* temporary data needs saving */
    Uint dfasize;		/* size of state machine */
    Uint tmpssize;		/* size of temporary state data */
    Uint tmppsize;		/* size of temporary posn data */
    string *dfasaved;		/* saved dfa */
    string *tmpsaved;		/* saved temporary data */

    unsigned short nregexp;	/* # regexps */
    unsigned short nposn;	/* number of unique positions */
    rpchunk *rpc;		/* current rgxposn chunk */
    hashtab *posnhtab;		/* position hash table */

    unsigned short nstates;	/* # states */
    unsigned short expanded;	/* # expanded states */
    unsigned short endstates;	/* # states with no valid transitions */
    unsigned short sttsize;	/* state table size */
    unsigned short sthsize;	/* size of state hash table */
    dfastate *states;		/* dfa states */
    unsigned short *sthtab;	/* state hash table */

    unsigned short ecnum;	/* number of equivalence classes */
    char eclass[256];		/* equivalence classes */
    char *ecsplit;		/* equivalence class split history */
    char *ecmembers;		/* members per equivalence class */
    Uint *ecset;		/* equivalence class sets */

    char zerotrans[2 * 256];	/* shared zero transitions */
};

/*
 * NAME:	dfa->new()
 * DESCRIPTION:	create new dfa instance
 */
dfa *dfa_new(grammar)
register char *grammar;
{
    char posn[258];
    unsigned int nstrings;
    register dfa *da;
    register dfastate *state;
    bool final;

    da = ALLOC(dfa, 1);

    /* grammar info */
    da->grammar = grammar;
    da->nregexp = (UCHAR(grammar[2]) << 8) + UCHAR(grammar[3]);
    nstrings = (UCHAR(grammar[6]) << 8) + UCHAR(grammar[7]);
    da->strings = grammar + 10 + (da->nregexp << 1);
    da->whitespace = grammar[1];

    /* size info */
    da->dfachanged = FALSE;
    da->tmpchanged = FALSE;
    da->dfasize = 8 + 256 + 3;
    da->tmpssize = 3 + 1 + 5 + 5;
    da->tmppsize = 0;
    da->dfasaved = (string *) NULL;
    da->tmpsaved = (string *) NULL;

    /* positions */
    da->nposn = (UCHAR(grammar[4]) << 8) + UCHAR(grammar[5]);
    da->rpc = (rpchunk *) NULL;
    da->posnhtab = ht_new((da->nposn + 1) << 2, 257);

    /* states */
    da->nstates = 2;
    da->sttsize = (da->nposn + nstrings + 1) << 1;
    da->sthsize = da->sttsize << 1;
    da->expanded = 0;
    da->endstates = 1;
    da->states = ALLOC(dfastate, da->sttsize);
    da->sthtab = ALLOC(short, da->sthsize);
    memset(da->sthtab, '\0', sizeof(short) * da->sthsize);

    /* initial states */
    state = &da->states[0];
    state->posn = (rgxposn **) NULL;
    state->str = (unsigned short *) NULL;
    state->trans = (char *) NULL;
    state->nposn = state->nstr = 0;
    state->ntrans = state->len = 0;
    (state++)->final = -1;
    state->posn = (da->nposn != 0) ?
		   ALLOC(rgxposn*, da->nposn) : (rgxposn **) NULL;
    state->str = (nstrings != 0) ?
		  ALLOC(unsigned short, nstrings) : (unsigned short *) NULL;
    state->trans = (char *) NULL;
    state->nposn = da->nposn;
    state->nstr = nstrings;
    state->ntrans = state->len = 0;
    state->final = -1;
    state->alloc = FALSE;
    grammar += 10;
    /* initial positions */
    if (state->nposn == 0 && state->nstr == 0) {
	/* no valid transitions from initial state */
	state->ntrans = 256;
	state->trans = da->zerotrans;
	da->endstates++;
    } else {
	register rgxposn **rrp;
	register int i, j, n;
	register unsigned short *s;
	register char *rgx;
	int size;

	rrp = POSNA(state);
	for (i = j = 0; i < da->nregexp; i++) {
	    rgx = da->grammar + (UCHAR(grammar[0]) << 8) + UCHAR(grammar[1]);
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
		*rrp++ = rp_new(da->posnhtab, posn, size, &da->rpc, rgx, j++, i,
				final);
		da->tmpssize += 2;
		da->tmppsize += 8 + size + final;
		rgx += UCHAR(rgx[0]) + 1;
	    }
	}
	/* initial strings */
	for (i = 0, s = STRA(state); i < nstrings; i++) {
	    *s++ = i;
	}
	da->tmpssize += nstrings << 1;
    }
    /* add to hashtable */
    ds_hash(da->sthtab, da->sthsize, da->states, 1);

    /* equivalence classes */
    da->ecnum = 1;
    da->ecsplit = ALLOC(char, 256 + 256 + 32 * 256);
    da->ecmembers = da->ecsplit + 256;
    da->ecset = (Uint *) (da->ecmembers + 256);
    memset(da->eclass, '\0', 256);
    memset(da->ecmembers, '\0', 256);
    memset(da->ecset, -1, 32);
    memset(da->ecset + 8, '\0', 32 * 255);

    /* zero transitions */
    memset(da->zerotrans, '\0', 2 * 256);

    return da;
}

/*
 * NAME:	dfa->del()
 * DESCRIPTION:	delete a dfa instance
 */
void dfa_del(da)
register dfa *da;
{
    register dfastate *state;
    register int i;

    if (da->dfasaved != (string *) NULL) {
	str_del(da->dfasaved);
    }
    if (da->tmpsaved != (string *) NULL) {
	str_del(da->tmpsaved);
    }
    if (da->rpc != (rpchunk *) NULL) {
	rp_clear(da->rpc);
    }
    if (da->posnhtab != (hashtab *) NULL) {
	ht_del(da->posnhtab);
    }
    for (i = da->nstates, state = &da->states[1]; --i > 0; state++) {
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
    FREE(da->states);
    if (da->sthtab != (unsigned short *) NULL) {
	FREE(da->sthtab);
    }
    if (da->ecsplit != (char *) NULL) {
	FREE(da->ecsplit);
    }
    FREE(da);
}

/*
 * NAME:	dfa->ecsplit()
 * DESCRIPTION:	split up equivalence classes along the borders of character
 *		sets
 */
static void dfa_ecsplit(da, iset, cset, ncset)
register dfa *da;
Uint *iset, *cset;
int ncset;
{
    Uint ec1[8], ec2[8];
    register int i, n, c;

    for (c = cs_firstc(iset, 0); c >= 0; c = cs_firstc(iset, c + 1)) {
	for (i = 0; i < ncset; i++) {
	    /*
	     * get the equivalence class of the first char in the input set
	     */
	    n = UCHAR(da->eclass[c]);
	    if (da->ecmembers[n] == 1) {
		break;	/* only one character left */
	    }
	    if (cs_overlap(da->ecset + (n << 3), cset, ec1, ec2)) {
		/*
		 * create new equivalence class
		 */
		memcpy(da->ecset + (n << 3), ec1, sizeof(ec1));
		memcpy(da->ecset + (da->ecnum << 3), ec2, sizeof(ec2));
		da->ecsplit[da->ecnum] = n;
		da->ecmembers[n] -= da->ecmembers[da->ecnum] =
				    cs_eclass(ec2, da->eclass, da->ecnum);
		da->ecnum++;
		da->dfasize += da->expanded << 1;
		da->tmpssize++;
	    }
	    cset += 8;
	}
	cset -= i << 3;

	/* remove from input set */
	cs_sub(iset, da->ecset + (UCHAR(da->eclass[c]) << 3));
    }
}

/*
 * NAME:	dfa->newstate()
 * DESCRIPTION:	get the positions and strings for a new state
 */
static int dfa_newstate(da, state, newstate, ecset, cset)
dfa *da;
register dfastate *state, *newstate;
Uint *ecset, *cset;
{
    char posn[130];
    register int i, n;
    register rgxposn *rp, **rrp;
    register char *p;
    register unsigned short *s;
    int size, posnsize;
    bool final;

    newstate->trans = (char *) NULL;
    newstate->nposn = newstate->nstr = newstate->ntrans = 0;
    newstate->len = state->len + 1;
    newstate->final = -1;
    newstate->alloc = FALSE;
    posnsize = 0;

    /* positions */
    for (i = state->nposn, rrp = POSNA(state); --i >= 0; rrp++) {
	rp = *rrp;
	for (n = rp->size; n > 0; --n) {
	    if (cs_intersect(ecset, cset)) {
		final = rp_trans(rp, ecset, posn + 2, &size);
		if (size != 0) {
		    posn[0] = rp->chain.name[0];
		    posn[1] = rp->chain.name[1];
		    rp = rp_new(da->posnhtab, posn, size, &da->rpc, rp->rgx,
				da->nposn, rp->ruleno, final);
		    if (rp->nposn == da->nposn) {
			/* new position */
			da->nposn++;
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
    for (i = state->nstr, s = STRA(state); --i >= 0; s++) {
	p = da->strings + (*s << 1);
	p = da->grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	n = UCHAR(p[newstate->len]);
	if (ecset[n >> 5] & (1 << (n & 31))) {
	    if (newstate->len == UCHAR(p[0])) {
		/* end of string */
		newstate->final = da->nregexp + *s;
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
static dfastate *dfa_expand(da, state)
dfa *da;
dfastate *state;
{
    Uint iset[8];
    register Uint *cset, *ecset;
    register rgxposn **rrp;
    register int ncset, i, n;
    register char *p;
    register unsigned short *s;
    dfastate *newstate;
    rgxposn **newposn;
    short *newstr;
    int size;

    memset(iset, '\0', sizeof(iset));

    /* allocate character sets for strings and positions */
    ncset = state->nstr;
    for (i = state->nposn, rrp = POSNA(state); --i >= 0; rrp++) {
	ncset += (*rrp)->size;
    }
    cset = ALLOCA(Uint, ncset << 3);

    /* construct character sets for all string chars */
    for (i = state->nstr, s = STRA(state); --i >= 0; s++) {
	p = da->strings + (*s << 1);
	p = da->grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	n = UCHAR(p[1 + state->len]);
	memset(cset, '\0', 32);
	cset[n >> 5] |= 1 << (n & 31);
	iset[n >> 5] |= 1 << (n & 31);	/* also add to input set */
	cset += 8;
    }

    /* construct character sets for all positions */
    for (i = state->nposn, rrp = POSNA(state); --i >= 0; rrp++) {
	rp_cset(*rrp, cset);
	for (n = (*rrp)->size; --n >= 0; ) {
	    cs_or(iset, cset);		/* add to input set */
	    cset += 8;
	}
    }
    cset -= ncset << 3;

    /*
     * adjust equivalence classes
     */
    dfa_ecsplit(da, iset, cset, ncset);

    /*
     * for all equivalence classes, compute transition states
     */
    if (state->nposn != 0) {
	newposn = ALLOCA(rgxposn*, state->nposn);
    }
    if (state->nstr != 0) {
	newstr = ALLOCA(short, state->nstr);
    }
    p = state->trans = ALLOC(char, 2 * 256);
    state->ntrans = da->ecnum;
    state->alloc = TRUE;
    cset += state->nstr << 3;
    for (i = da->ecnum, ecset = da->ecset; --i >= 0; ecset += 8) {
	/* prepare new state */
	newstate = &da->states[da->nstates];

	/* flesh out new state */
	newstate->posn.a = newposn;
	newstate->str.a = newstr;
	size = dfa_newstate(da, state, newstate, ecset, cset);

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
		    newstate->str.a = (short *) NULL;
		    newstate->len = 0;
		} else {
		    memcpy(newstate->str.e, newstr, 2 * sizeof(short));
		}
	    }

	    n = ds_hash(da->sthtab, da->sthsize, da->states, da->nstates);
	    if (n == da->nstates) {
		/*
		 * genuinely new state
		 */
		if (newstate->nposn > 1) {
		    newstate->posn.a = ALLOC(rgxposn*, newstate->nposn);
		    memcpy(newstate->posn.a, newposn,
			   newstate->nposn * sizeof(rgxposn*));
		}
		if (newstate->nstr > 2) {
		    newstate->str.a = ALLOC(short, newstate->nstr);
		    memcpy(newstate->str.a, newstr,
			   newstate->nstr * sizeof(short));
		}
		da->dfasize += 3;
		da->tmpssize += 5 + ((newstate->nposn + newstate->nstr) << 1);
		da->tmppsize += size;
		da->nstates++;
		if (da->nstates == da->sttsize) {
		    dfastate *table;

		    /* grow table */
		    table = ALLOC(dfastate, da->sttsize <<= 1);
		    memcpy(table, da->states, da->nstates * sizeof(dfastate));
		    state = &table[state - da->states];
		    FREE(da->states);
		    da->states = table;
		}

		if (newstate->nposn == 0 && newstate->nstr == 0) {
		    newstate->ntrans = 256;
		    newstate->trans = da->zerotrans;
		    da->endstates++;
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
    AFREE(cset - (state->nstr << 3));

    da->dfachanged = TRUE;
    da->tmpchanged = TRUE;
    da->expanded++;
    da->dfasize += da->ecnum << 1;
    return state;
}

/*
 * NAME:	dfa->extend()
 * DESCRIPTION:	extend transition table
 */
static void dfa_extend(da, state, limit)
register dfa *da;
register dfastate *state;
register int limit;
{
    register char *p, *q;
    register unsigned int i;

    /* extend transition table */
    if (!state->alloc) {
	p = ALLOC(char, 2 * 256);
	memcpy(p, state->trans, state->ntrans << 1);
	state->trans = p;
	state->alloc = TRUE;
    }
    p = state->trans + (state->ntrans << 1);
    for (i = state->ntrans; i <= limit; i++) {
	q = &state->trans[UCHAR(da->ecsplit[i]) << 1];
	*p++ = q[0];
	*p++ = q[1];
    }
    state->ntrans = i;
}

/*
 * state & eclass format:
 *
 * header	[0]	version number
 *		[x][y]	# states
 *		[x][y]	# expanded states
 *		[x]	# equivalence classes
 * eclass	[...]	256 equivalence classes
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
 *		[...]   position data			}
 *		[...]	string data			}
 *
 * position	[x][y]	regexp				}
 *		[x][y]	ruleno				}
 *		[0]	optional: final position	} ...
 *		[x]	size				}
 *		[...]	position data			}
 */

/*
 * NAME:	dfa->load()
 * DESCRIPTION:	load dfa from strings
 */
dfa *dfa_load(grammar, s1, s2)
char *grammar;
string *s1, *s2;
{
    int nstrings, nposn;
    register dfa *da;
    register dfastate *state;
    register int i, c;
    register char *buf, *tmpbuf;

    da = ALLOC(dfa, 1);
    str_ref(da->dfasaved = s1);
    buf = s1->text;

    /* grammar info */
    da->grammar = grammar;
    da->nregexp = (UCHAR(grammar[2]) << 8) + UCHAR(grammar[3]);
    nstrings = (UCHAR(grammar[6]) << 8) + UCHAR(grammar[7]);
    da->strings = grammar + 10 + (da->nregexp << 1);
    da->whitespace = grammar[1];

    /* states */
    da->nstates = (UCHAR(buf[1]) << 8) + UCHAR(buf[2]);
    da->expanded = (UCHAR(buf[3]) << 8) + UCHAR(buf[4]);
    da->endstates = (UCHAR(buf[5]) << 8) + UCHAR(buf[6]);

    /* equivalence classes */
    da->ecnum = UCHAR(buf[7]);
    buf += 8;
    memcpy(da->eclass, buf, 256);
    buf += 256;

    if (da->nstates == da->expanded + da->endstates) {
	/*
	 * dfa is complete
	 */
	da->tmpsaved = (string *) NULL;

	/* positions */
	da->nposn = 0;
	da->rpc = (rpchunk *) NULL;
	da->posnhtab = (hashtab *) NULL;

	/* equivalence classes */
	da->ecsplit = (char *) NULL;
	da->ecmembers = (char *) NULL;
	da->ecset = (Uint *) NULL;

	/* states */
	da->sttsize = da->nstates;
	da->sthsize = 0;
	da->states = ALLOC(dfastate, da->sttsize);
	da->sthtab = (short *) NULL;

	for (i = da->nstates, state = &da->states[1]; --i > 0; state++) {
	    buf = ds_load(state, buf, da->ecnum, da->zerotrans);
	}

	/* size info */
	da->tmpssize = 0;
	da->tmppsize = 0;
    } else {
	/*
	 * dfa is incomplete
	 */
	str_ref(da->tmpsaved = s2);
	tmpbuf = s2->text;
	nposn = (UCHAR(tmpbuf[1]) << 8) + UCHAR(tmpbuf[2]);
	tmpbuf += 3;

	/* positions */
	da->nposn = 0;
	da->rpc = (rpchunk *) NULL;
	da->posnhtab = ht_new((nposn + 1) << 2, 257);

	/* equivalence classes */
	da->ecsplit = ALLOC(char, 256 + 256 + 32 * 256);
	da->ecmembers = da->ecsplit + 256;
	da->ecset = (Uint *) (da->ecmembers + 256);
	memcpy(da->ecsplit, tmpbuf, da->ecnum);
	tmpbuf += da->ecnum;
	memset(da->ecmembers, '\0', 256);
	memset(da->ecset, '\0', 32 * 256);
	for (i = 256; --i >= 0; ) {
	    c = UCHAR(da->eclass[i]);
	    da->ecmembers[c]++;
	    da->ecset[(c << 3) + (i >> 5)] |= 1 << (i & 31);
	}

	/* states */
	da->sttsize = (da->nposn + nstrings + 1) << 1;
	if (da->sttsize <= da->nstates) {
	    da->sttsize = da->nstates + 1;
	}
	da->sthsize = da->sttsize << 1;
	da->states = ALLOC(dfastate, da->sttsize);
	da->sthtab = ALLOC(short, da->sthsize);
	memset(da->sthtab, '\0', sizeof(short) * da->sthsize);

	for (i = 1, state = &da->states[1]; i < da->nstates; i++, state++) {
	    buf = ds_load(state, buf, da->ecnum, da->zerotrans);
	    tmpbuf = ds_loadtmp(state, tmpbuf, s2->text, da->posnhtab,
				&da->rpc, &da->nposn, grammar);
	    ds_hash(da->sthtab, da->sthsize, da->states, i);
	}

	/* size info */
	da->tmpssize = tmpbuf - s2->text;
	da->tmppsize = s2->len - da->tmpssize;
    }
    da->dfasize = s1->len;
    da->dfachanged = da->tmpchanged = FALSE;

    /* zero state */
    da->states[0].posn = (rgxposn **) NULL;
    da->states[0].str = (unsigned short *) NULL;
    da->states[0].trans = (char *) NULL;
    da->states[0].nposn = da->states[0].nstr = 0;
    da->states[0].ntrans = da->states[0].len = 0;
    da->states[0].final = -1;

    /* zero transitions */
    memset(da->zerotrans, '\0', 2 * 256);

    return da;
}

/*
 * NAME:	dfa->save()
 * DESCRIPTION:	save dfa in strings
 */
bool dfa_save(da, s1, s2)
register dfa *da;
string **s1, **s2;
{
    register int i;
    register char *buf, *pbase;
    register dfastate *state;
    char *pbuf;
    short *ptab, *nposn;

    if (!da->dfachanged) {
	return FALSE;
    }

    *s1 = str_new((char *) NULL, (long) da->dfasize);
    buf = (*s1)->text;
    buf[0] = 0;
    buf[1] = da->nstates >> 8;
    buf[2] = da->nstates;
    buf[3] = da->expanded >> 8;
    buf[4] = da->expanded;
    buf[5] = da->endstates >> 8;
    buf[6] = da->endstates;
    buf[7] = da->ecnum;
    buf += 8;
    memcpy(buf, da->eclass, 256);
    buf += 256;

    for (i = da->nstates, state = &da->states[1]; --i >= 0; state++) {
	if (state->ntrans != 0 && state->ntrans < da->ecnum) {
	    dfa_extend(da, state, da->ecnum - 1);
	}
	buf = ds_save(state, buf);
    }

    if (da->nstates == da->expanded + da->endstates) {
	*s2 = (string *) NULL;
	return TRUE;
    }
    if (!da->tmpchanged) {
	*s2 = da->tmpsaved;
	return TRUE;
    }

    *s2 = str_new((char *) NULL, (long) (da->tmpssize + da->tmppsize));
    buf = (*s2)->text;
    buf[0] = 0;
    buf[1] = da->nposn >> 8;
    buf[2] = da->nposn;
    pbase = buf;
    pbuf = buf + da->tmpssize;
    buf += 3;
    memcpy(buf, da->ecsplit, da->ecnum);
    buf += da->ecnum;

    ptab = ALLOCA(short, da->nposn);
    nposn = 0;
    for (i = da->nstates, state = &da->states[1]; --i >= 0; state++) {
	buf = ds_savetmp(state, buf, &pbuf, (*s2)->text, ptab, &nposn,
			 da->grammar);
    }
    AFREE(ptab);

    return TRUE;
}

/*
 * NAME:	dfa->lazyscan()
 * DESCRIPTION:	Scan input, while lazily constructing a DFA.
 *		Return values:	[0 ..>	token
 *				-1	end of string
 *				-2	Invalid token
 *				-3	DFA too large (deallocate)
 */
int dfa_lazyscan(da, str, strlen)
register dfa *da;
string *str;
unsigned int *strlen;
{
    register unsigned int size, eclass;
    register char *p, *q;
    register dfastate *state;
    int final;
    unsigned int fsize;

    size = *strlen;

    while (size != 0) {
	state = &da->states[1];
	final = -1;
	p = str->text + str->len - size;

	while (size != 0) {
	    eclass = UCHAR(da->eclass[UCHAR(*p)]);
	    if (state->ntrans <= eclass) {
		if (state->ntrans == 0) {
		    /* expand state */
		    if (state == da->states) {
			break;	/* stuck in state 0 */
		    }
		    state = dfa_expand(da, state);
		    if (da->tmpssize + da->tmppsize > USHRT_MAX) {
			int save;

			/*
			 * too much temporary data: attempt to expand
			 * all states
			 */
			save = state - da->states;
			for (state = &da->states[1];
			     da->nstates != da->expanded + da->endstates;
			     state++) {
			    if (da->dfasize > USHRT_MAX) {
				return -3;	/* DFA too big */
			    }
			    if (state->ntrans == 0) {
				state = dfa_expand(da, state);
			    }
			}
			state = &da->states[save];
		    }
		    if (da->dfasize > USHRT_MAX) {
			return -3;	/* DFA too big */
		    }
		    eclass = UCHAR(da->eclass[UCHAR(*p)]);
		} else {
		    /* extend transition table */
		    dfa_extend(da, state, eclass);
		}
	    }

	    /* transition */
	    --size;
	    p++;
	    q = &state->trans[eclass << 1];
	    state = &da->states[(UCHAR(q[0]) << 8) + UCHAR(q[1])];

	    /* check if final state */
	    if (state->final >= 0) {
		final = state->final;
		fsize = size;
	    }
	}

	if (final >= 0) {
	    /* in a final state */
	    size = fsize;
	    if (final != 0 || !da->whitespace) {
		*strlen = size;
		return final;
	    }
	    /* else whitespace: continue */
	} else {
	    return -2;	/* reject */
	}
    }

    return -1;	/* end of string */
}
