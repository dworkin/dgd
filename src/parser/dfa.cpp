/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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

class Charset {
public:
    void neg();
    void merge(Charset *cs);
    void sub(Charset *cs);
    bool intersect(Charset *cs);
    bool overlap(Charset *cs1, Charset *cs2, Charset *cs3);
    int firstc(int c);
    int eclass(char *eclass, int sclass);

    Uint chars[8];
};

/*
 * negate a charset
 */
void Charset::neg()
{
    chars[0] ^= 0xffffffffL;
    chars[1] ^= 0xffffffffL;
    chars[2] ^= 0xffffffffL;
    chars[3] ^= 0xffffffffL;
    chars[4] ^= 0xffffffffL;
    chars[5] ^= 0xffffffffL;
    chars[6] ^= 0xffffffffL;
    chars[7] ^= 0xffffffffL;
}

/*
 * merge two charsets
 */
void Charset::merge(Charset *cs)
{
    chars[0] |= cs->chars[0];
    chars[1] |= cs->chars[1];
    chars[2] |= cs->chars[2];
    chars[3] |= cs->chars[3];
    chars[4] |= cs->chars[4];
    chars[5] |= cs->chars[5];
    chars[6] |= cs->chars[6];
    chars[7] |= cs->chars[7];
}

/*
 * subtract a charset from another one
 */
void Charset::sub(Charset *cs)
{
    chars[0] &= ~cs->chars[0];
    chars[1] &= ~cs->chars[1];
    chars[2] &= ~cs->chars[2];
    chars[3] &= ~cs->chars[3];
    chars[4] &= ~cs->chars[4];
    chars[5] &= ~cs->chars[5];
    chars[6] &= ~cs->chars[6];
    chars[7] &= ~cs->chars[7];
}

/*
 * return TRUE if two character sets intersect, FALSE otherwise
 */
bool Charset::intersect(Charset *cs)
{
    Uint i;

    i  = chars[0] & cs->chars[0];
    i |= chars[1] & cs->chars[1];
    i |= chars[2] & cs->chars[2];
    i |= chars[3] & cs->chars[3];
    i |= chars[4] & cs->chars[4];
    i |= chars[5] & cs->chars[5];
    i |= chars[6] & cs->chars[6];
    i |= chars[7] & cs->chars[7];

    return (i != 0);
}

/*
 * Check if two character sets overlap.  Return TRUE if they do,
 * or if the first set contains the second one.
 */
bool Charset::overlap(Charset *cs1, Charset *cs2, Charset *cs3)
{
    Uint s2, s3;

    s2  = cs2->chars[0] = chars[0] & cs1->chars[0];
    s2 |= cs2->chars[1] = chars[1] & cs1->chars[1];
    s2 |= cs2->chars[2] = chars[2] & cs1->chars[2];
    s2 |= cs2->chars[3] = chars[3] & cs1->chars[3];
    s2 |= cs2->chars[4] = chars[4] & cs1->chars[4];
    s2 |= cs2->chars[5] = chars[5] & cs1->chars[5];
    s2 |= cs2->chars[6] = chars[6] & cs1->chars[6];
    s2 |= cs2->chars[7] = chars[7] & cs1->chars[7];

    s3  = cs3->chars[0] = chars[0] & ~cs2->chars[0];
    s3 |= cs3->chars[1] = chars[1] & ~cs2->chars[1];
    s3 |= cs3->chars[2] = chars[2] & ~cs2->chars[2];
    s3 |= cs3->chars[3] = chars[3] & ~cs2->chars[3];
    s3 |= cs3->chars[4] = chars[4] & ~cs2->chars[4];
    s3 |= cs3->chars[5] = chars[5] & ~cs2->chars[5];
    s3 |= cs3->chars[6] = chars[6] & ~cs2->chars[6];
    s3 |= cs3->chars[7] = chars[7] & ~cs2->chars[7];

    return (s2 != 0 && s3 != 0);
}

/*
 * find the first char in a charset
 */
int Charset::firstc(int c)
{
    Uint x;

    while (c < 256) {
	if ((x=chars[c >> 5] >> (c & 31)) != 0) {
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
 * convert a charset into an equivalence class
 */
int Charset::eclass(char *eclass, int sclass)
{
    int n, c;
    Uint x;

    n = 0;
    for (c = firstc(0); c < 256; c += 31, c &= ~31) {
	x = chars[c >> 5] >> (c & 31);
	if (x != 0) {
	    do {
		while ((x & 0xff) == 0) {
		    x >>= 8;
		    c += 8;
		}
		if (x & 1) {
		    eclass[c] = sclass;
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


class RgxPosn : public Hash::Entry, public ChunkAllocated {
public:
    void cset(Charset *cset);
    bool trans(Charset *cset, char *posn, unsigned short *size);
    char *save(char *buf, char *grammar);

    static RgxPosn *alloc(Hash::Hashtab *htab, char *posn, unsigned short size,
			  class RpChunk **c, char *rgx, Uint nposn,
			  unsigned short ruleno, bool final);
    static RgxPosn *create(Hash::Hashtab *htab, char *posn, unsigned short size,
			   RpChunk **c, char *rgx, Uint nposn,
			   unsigned short ruleno, bool final);
    static bool transposn(char *rgx, char *trans, char *buf,
			  unsigned short *buflen);
    static RgxPosn *load(Hash::Hashtab *htab, RpChunk **c, Uint nposn,
			 char *buf, char *grammar);

    char *rgx;			/* regular expression this position is in */
    unsigned short size;	/* size of position (length of string - 2) */
    unsigned short ruleno;	/* the rule this position is in */
    Uint nposn;			/* position number */
    bool allocated;		/* position allocated separately? */

private:
    bool final;			/* final position? */
};

# define RPCHUNKSZ	32

class RpChunk : public Chunk<RgxPosn, RPCHUNKSZ> {
public:
    /*
     * iterate through items from destructor
     */
    virtual ~RpChunk() {
	items();
    }

    /*
     * free strings when iterating through items
     */
    virtual bool item(RgxPosn *rp) {
	if (rp->allocated) {
	    FREE(rp->name);
	}
	return TRUE;
    }
};

/*
 * allocate a new rgxposn (or return an old one)
 */
RgxPosn *RgxPosn::alloc(Hash::Hashtab *htab, char *posn, unsigned short size,
			RpChunk **c, char *rgx, Uint nposn,
			unsigned short ruleno, bool final)
{
    RgxPosn **rrp, *rp;

    rrp = (RgxPosn **) htab->lookup(posn, TRUE);
    if (*rrp != (RgxPosn *) NULL) {
	return *rrp;	/* already exists */
    }

    if (*c == (RpChunk *) NULL) {
	*c = new RpChunk;
    }
    rp = chunknew (**c) RgxPosn;
    rp->next = *rrp;
    *rrp = rp;
    rp->name = posn;
    rp->rgx = rgx;
    rp->size = size;
    rp->nposn = nposn;
    rp->ruleno = ruleno;
    rp->final = final;
    rp->allocated = FALSE;

    return rp;
}

/*
 * create a new rgxposn
 */
RgxPosn *RgxPosn::create(Hash::Hashtab *htab, char *posn, unsigned short size,
			 RpChunk **c, char *rgx, Uint nposn,
			 unsigned short ruleno, bool final)
{
    RgxPosn *rp;

    rp = alloc(htab, posn, size, c, rgx, nposn, ruleno, final);
    if (rp->nposn == nposn) {
	rp->name = strcpy(ALLOC(char, size + 3), posn);
	rp->allocated = TRUE;
    }
    return rp;
}

/*
 * convert a transition into a position
 */
bool RgxPosn::transposn(char *rgx, char *trans, char *buf,
			unsigned short *buflen)
{
    char a[256], b[256], c[256], heap[256];
    char *p, *q;
    int n, place;
    unsigned short i, j, len;

    memset(a, '\0', 256);
    heap[0] = 0;
    len = 0;

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

    return (a[UCHAR(rgx[0])] != '\0');	/* final? */
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
 * create input sets for a position
 */
void RgxPosn::cset(Charset *cset)
{
    char *p;
    const char *q;
    int c, n, x;
    bool negate;

    for (q = name + 2; *q != '\0'; q++) {
	memset(cset->chars, '\0', 32);
	p = rgx + UCHAR(*q);
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
		cset->chars[c >> 5] |= 1 << (c & 31);
		if (p[0] == '-' && p[1] != ']') {
		    n = UCHAR(p[1]) - c;
		    if (n != 0) {
			x = 32 - (++c & 31);
			if (x > n) {
			    x = n;
			}
			cset->chars[c >> 5] |= bits[x - 1] << (c & 31);
			c += x;
			n -= x;
			while (n >= 32) {
			    cset->chars[c >> 5] |= 0xffffffffL;
			    c += 32;
			    n -= 32;
			}
			if (n != 0) {
			    cset->chars[c >> 5] |= bits[n - 1];
			}
		    }
		    p += 2;
		}
	    } while (*p != ']');
	    if (negate) {
		cset->neg();
	    }
	    break;

	case '.':
	    /* anything */
	    memset(cset->chars, -1, 32);
	    break;

	case '\\':
	    /* escaped char */
	    p++;
	    /* fall through */
	default:
	    /* normal char */
	    c = UCHAR(*p);
	    cset->chars[c >> 5] |= 1 << (c & 31);
	    break;
	}

	cset++;
    }
}

/*
 * perform a transition on a position, given an input set
 */
bool RgxPosn::trans(Charset *cset, char *posn, unsigned short *size)
{
    char trans[256];
    char *p;
    const char *q;
    int c, n, x;
    char *t;
    Uint found;
    bool negate;

    t = trans;
    for (q = name + 2; *q != '\0'; q++) {
	p = rgx + UCHAR(*q);
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
		found |= cset->chars[c >> 5] & 1 << (c & 31);
		if (p[0] == '-' && p[1] != ']') {
		    n = UCHAR(p[1]) - c;
		    if (n != 0) {
			x = 32 - (++c & 31);
			if (x > n) {
			    x = n;
			}
			found |= cset->chars[c >> 5] & (bits[x - 1] <<
								    (c & 31));
			c += x;
			n -= x;
			while (n >= 32) {
			    found |= cset->chars[c >> 5] & 0xffffffffL;
			    c += 32;
			    n -= 32;
			}
			if (n != 0) {
			    found |= cset->chars[c >> 5] & bits[n - 1];
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
	    /* fall through */
	default:
	    /* normal char */
	    c = UCHAR(*p);
	    found = cset->chars[c >> 5] & (1 << (c & 31));
	    break;
	}
	if (found != 0) {
	    *t++ = p - rgx + 1;
	}
    }
    *t = '\0';

    return transposn(rgx, trans, posn, size);
}

/*
 * load a rgxposn from a buffer
 */
RgxPosn *RgxPosn::load(Hash::Hashtab *htab, RpChunk **c, Uint nposn, char *buf,
		       char *grammar)
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

    return alloc(htab, buf, size, c, rgx, nposn, ruleno, final);
}

/*
 * save a rgxposn to a buffer
 */
char *RgxPosn::save(char *buf, char *grammar)
{
    unsigned short rgx;

    rgx = this->rgx - grammar;
    *buf++ = rgx >> 8;
    *buf++ = rgx;
    *buf++ = ruleno >> 8;
    *buf++ = ruleno;
    if (final) {
	*buf++ = '\0';
    }
    *buf++ = size;
    memcpy(buf, name, size + 3);
    return buf + size + 3;
}


class DfaState {
public:
    char *load(char *buf, unsigned short ntrans, char *zerotrans);
    char *loadtmp(char *sbuf, char *pbuf, Hash::Hashtab *htab, RpChunk **c,
		  Uint *nposn, char *grammar);
    char *save(char *buf);
    char *savetmp(char *sbuf, char **pbuf, char *pbase, Uint *ptab, Uint *nposn,
		  char *grammar);

    static unsigned short hash(unsigned short *htab, Uint htabsize,
			       DfaState *states, unsigned short idx);

    union {			/* regexp positions */
	RgxPosn *e;		/* 1 */
	RgxPosn **a;		/* > 1 */
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
    bool alloc;			/* transitions allocated? */

private:
    unsigned short next;	/* next in hash chain */
};

# define POSNA(state)	(((state)->nposn == 1) ? \
			  &(state)->posn.e : (state)->posn.a)
# define STRA(state)	(((state)->nstr <= 2) ? \
			  (state)->str.e : (state)->str.a)

/*
 * put a new state in the hash table, or return an old one
 */
unsigned short DfaState::hash(unsigned short *htab, Uint htabsize,
			      DfaState *states, unsigned short idx)
{
    Uint x;
    RgxPosn **posn;
    unsigned short n, *str;
    DfaState *newstate, *ds;
    unsigned short *dds;

    /* hash on position and string pointers */
    newstate = &states[idx];
    x = newstate->len ^ newstate->final;
    for (n = newstate->nposn, posn = POSNA(newstate); n > 0; --n) {
	x = (x >> 3) ^ (x << 29) ^ (uintptr_t) *posn++;
    }
    for (n = newstate->nstr, str = STRA(newstate); n > 0; --n) {
	x = (x >> 3) ^ (x << 29) ^ (uintptr_t) *str++;
    }

    /* check state hash table */
    posn = POSNA(newstate);
    str = STRA(newstate);
    dds = &htab[(Uint) x % htabsize];
    ds = &states[*dds];
    while (ds != states) {
	if (newstate->len == ds->len && newstate->final == ds->final &&
	    newstate->nposn == ds->nposn && newstate->nstr == ds->nstr &&
	    memcmp(posn, POSNA(ds), newstate->nposn * sizeof(RgxPosn*)) == 0 &&
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
 * load a dfastate from a buffer
 */
char *DfaState::load(char *buf, unsigned short ntrans, char *zerotrans)
{
    posn.a = (RgxPosn **) NULL;
    str.a = (unsigned short *) NULL;
    nposn = nstr = len = 0;
    alloc = FALSE;
    final = (UCHAR(buf[0]) << 8) + UCHAR(buf[1]);
    buf += 2;
    switch (*buf++) {
    case TRANS_NONE:
	this->ntrans = 0;
	break;

    case TRANS_ZERO:
	this->ntrans = 256;
	trans = zerotrans;
	break;

    case TRANS_STATES:
	this->ntrans = ntrans;
	trans = buf;
	buf += ntrans << 1;
	break;
    }

    return buf;
}

/*
 * load dfastate temporary data from a buffer
 */
char *DfaState::loadtmp(char *sbuf, char *pbuf, Hash::Hashtab *htab,
			RpChunk **c, Uint *nposn, char *grammar)
{
    RgxPosn *rp, **rrp;
    unsigned short i, *s;
    char *posn;

    this->nposn = (UCHAR(sbuf[0]) << 8) + UCHAR(sbuf[1]);
    nstr = (UCHAR(sbuf[2]) << 8) + UCHAR(sbuf[3]);
    sbuf += 4;
    len = UCHAR(*sbuf++);

    if (this->nposn != 0) {
	if (this->nposn != 1) {
	    rrp = this->posn.a = ALLOC(RgxPosn*, this->nposn);
	} else {
	    rrp = &this->posn.e;
	}
	for (i = this->nposn; i > 0; --i) {
	    posn = pbuf + ((Uint) UCHAR(sbuf[0]) << 16) +
		   (UCHAR(sbuf[1]) << 8) + UCHAR(sbuf[2]);
	    sbuf += 3;
	    rp = *rrp++ = RgxPosn::load(htab, c, *nposn, posn, grammar);
	    if (rp->nposn == *nposn) {
		(*nposn)++;
	    }
	}
    }
    if (nstr != 0) {
	if (nstr > 2) {
	    s = str.a = ALLOC(unsigned short, nstr);
	} else {
	    s = str.e;
	}
	for (i = nstr; i > 0; --i) {
	    *s++ = (UCHAR(sbuf[0]) << 8) + UCHAR(sbuf[1]);
	    sbuf += 2;
	}
    }

    return sbuf;
}

/*
 * save a dfastate to a buffer
 */
char *DfaState::save(char *buf)
{
    buf[0] = final >> 8;
    buf[1] = final;
    buf += 2;
    if (ntrans == 0) {
	*buf++ = TRANS_NONE;
    } else if (nposn == 0 && nstr == 0) {
	*buf++ = TRANS_ZERO;
    } else {
	*buf++ = TRANS_STATES;
	memcpy(buf, trans, ntrans << 1);
	buf += ntrans << 1;
    }

    return buf;
}

/*
 * save dfastate temporary data to a buffer
 */
char *DfaState::savetmp(char *sbuf, char **pbuf, char *pbase, Uint *ptab,
			Uint *nposn, char *grammar)
{
    RgxPosn *rp, **rrp;
    unsigned short i, *s;
    Uint n;

    *sbuf++ = this->nposn >> 8;
    *sbuf++ = this->nposn;
    *sbuf++ = nstr >> 8;
    *sbuf++ = nstr;
    *sbuf++ = len;

    rrp = POSNA(this);
    for (i = this->nposn; i > 0; --i) {
	rp = *rrp++;
	if (rp->nposn == *nposn) {
	    ptab[(*nposn)++] = (intptr_t) *pbuf - (intptr_t) pbase;
	    *pbuf = rp->save(*pbuf, grammar);
	}
	n = ptab[rp->nposn];
	*sbuf++ = n >> 16;
	*sbuf++ = n >> 8;
	*sbuf++ = n;
    }

    s = STRA(this);
    for (i = nstr; i > 0; --i) {
	*sbuf++ = *s >> 8;
	*sbuf++ = *s++;
    }

    return sbuf;
}


# define DFA_VERSION	1

/*
 * construct a dfa instance
 */
Dfa::Dfa(char *source, char *grammar)
{
    this->source = source;
    this->grammar = grammar;
    whitespace = (UCHAR(grammar[1]) << 8) + UCHAR(grammar[2]);
    nomatch = (UCHAR(grammar[3]) << 8) + UCHAR(grammar[4]);
    nregexp = (UCHAR(grammar[5]) << 8) + UCHAR(grammar[6]);
    nsstrings = (UCHAR(grammar[9]) << 8) + UCHAR(grammar[10]);
    strings = grammar + 17 + (nregexp << 1);
    nposn = (UCHAR(grammar[7]) << 8) + UCHAR(grammar[8]);
}

/*
 * delete a dfa instance
 */
Dfa::~Dfa()
{
    DfaState *state;
    unsigned short i;

    if (allocated) {
	FREE(dfastr);
    }
    if (ecsplit != (char *) NULL) {
	FREE(ecsplit);
    }
    delete rpc;
    if (posnhtab != (Hash::Hashtab *) NULL) {
	delete posnhtab;
    }
    for (i = nstates, state = &states[1]; --i > 0; state++) {
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
    FREE(states);
    if (sthtab != (unsigned short *) NULL) {
	FREE(sthtab);
    }
}

/*
 * create new dfa instance
 */
Dfa *Dfa::create(char *source, char *grammar)
{
    char posn[258];
    unsigned short nstrings;
    Dfa *fa;
    DfaState *state;
    bool final;

    fa = new Dfa(source, grammar);

    /* grammar info */
    nstrings = fa->nsstrings + (UCHAR(grammar[11]) << 8) + UCHAR(grammar[12]);

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
    fa->ecset = (Charset *) (fa->ecmembers + 256);
    memset(fa->eclass, '\0', 256);
    memset(fa->ecmembers, '\0', 256);
    memset(fa->ecset, -1, 32);
    memset(fa->ecset + 1, '\0', 32 * 255);

    /* positions */
    fa->rpc = (RpChunk *) NULL;
    fa->posnhtab = HM->create((fa->nposn + 1) << 2, 257, FALSE);

    /* states */
    fa->nstates = 2;
    fa->sttsize = (Uint) (fa->nposn + nstrings + 1) << 1;
    fa->sthsize = (Uint) fa->sttsize << 1;
    fa->nexpanded = 0;
    fa->endstates = 1;
    fa->states = ALLOC(DfaState, fa->sttsize);
    fa->sthtab = ALLOC(unsigned short, fa->sthsize);
    memset(fa->sthtab, '\0', sizeof(unsigned short) * fa->sthsize);

    /* initial states */
    state = &fa->states[0];
    state->posn.a = (RgxPosn **) NULL;
    state->str.a = (unsigned short *) NULL;
    state->trans = (char *) NULL;
    state->nposn = state->nstr = 0;
    state->ntrans = state->len = 0;
    (state++)->final = -1;
    state->posn.a = (fa->nposn > 1) ?
		     ALLOC(RgxPosn*, fa->nposn) : (RgxPosn **) NULL;
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
	RgxPosn **rrp;
	unsigned short i, j, n, *s;
	char *rgx;
	unsigned short size;

	rrp = POSNA(state);
	for (i = j = 0; i < fa->nregexp; i++) {
	    rgx = fa->grammar + (UCHAR(grammar[0]) << 8) + UCHAR(grammar[1]);
	    grammar += 2;
	    n = j + (UCHAR(rgx[0]) << 8) + UCHAR(rgx[1]);
	    rgx += 2;
	    while (j < n) {
		final = RgxPosn::transposn(rgx, (char *) NULL, posn + 2, &size);
		if (final && state->final < 0) {
		    state->final = i;
		}
		posn[0] = 1 + j / 255;
		posn[1] = 1 + j % 255;
		*rrp++ = RgxPosn::create(fa->posnhtab, posn, size, &fa->rpc,
					 rgx, (Uint) j++, i, final);
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
    DfaState::hash(fa->sthtab, fa->sthsize, fa->states, 1);

    /* zero transitions */
    memset(fa->zerotrans, '\0', 2 * 256);

    return fa;
}

/*
 * extend transition table
 */
void Dfa::extend(DfaState *state, unsigned short limit)
{
    char *p, *q;
    unsigned short i;

    /* extend transition table */
    if (!state->alloc) {
	p = ALLOC(char, 2 * 256);
	memcpy(p, state->trans, state->ntrans << 1);
	state->trans = p;
	state->alloc = TRUE;
    }
    p = state->trans + (state->ntrans << 1);
    for (i = state->ntrans; i <= limit; i++) {
	q = &state->trans[UCHAR(ecsplit[i]) << 1];
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
 * state	[x][y]	final				} ...
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
 *		[x]	len				} ...
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
 * load dfa from string
 */
Dfa *Dfa::load(char *source, char *grammar, char *str, Uint len)
{
    Dfa *fa;
    DfaState *state;
    unsigned short i;
    char *buf;
    unsigned short nstrings;

    if (str[0] != DFA_VERSION) {
	return create(source, grammar);
    }

    fa = new Dfa(source, grammar);
    fa->dfastr = buf = str;

    /* grammar info */
    nstrings = fa->nsstrings + (UCHAR(grammar[11]) << 8) + UCHAR(grammar[12]);

    /* positions */
    fa->rpc = (RpChunk *) NULL;
    fa->posnhtab = (Hash::Hashtab *) NULL;

    /* states 1 */
    fa->nstates = (UCHAR(buf[1]) << 8) + UCHAR(buf[2]);
    fa->nexpanded = (UCHAR(buf[3]) << 8) + UCHAR(buf[4]);
    fa->endstates = (UCHAR(buf[5]) << 8) + UCHAR(buf[6]);
    fa->sttsize = fa->nstates + 1;
    fa->sthsize = (Uint) (fa->nposn + nstrings + 1) << 2;
    fa->states = ALLOC(DfaState, fa->sttsize);
    fa->sthtab = (unsigned short *) NULL;

    /* equivalence classes */
    fa->ecnum = UCHAR(buf[7]) + 1;
    buf += 8;
    memcpy(fa->eclass, buf, 256);
    buf += 256;
    fa->ecsplit = (char *) NULL;
    fa->ecmembers = (char *) NULL;
    fa->ecset = (Charset *) NULL;

    /* states 2 */
    fa->states[0].posn.a = (RgxPosn **) NULL;
    fa->states[0].str.a = (unsigned short *) NULL;
    fa->states[0].trans = (char *) NULL;
    fa->states[0].nposn = fa->states[0].nstr = 0;
    fa->states[0].ntrans = fa->states[0].len = 0;
    fa->states[0].final = -1;
    for (i = fa->nstates, state = &fa->states[1]; --i > 0; state++) {
	buf = state->load(buf, fa->ecnum, fa->zerotrans);
    }

    /* temporary data */
    fa->tmpstr = buf;

    /* size info */
    fa->dfasize = (intptr_t) buf - (intptr_t) str;
    fa->tmpssize = 0;
    fa->tmppsize = len - fa->dfasize;
    fa->modified = fa->allocated = FALSE;

    /* zero transitions */
    memset(fa->zerotrans, '\0', 2 * 256);

    return fa;
}

/*
 * load dfa tmp info
 */
void Dfa::loadtmp()
{
    DfaState *state;
    unsigned short i;
    int c;
    char *buf;

    buf = tmpstr;
    nposn = ((Uint) UCHAR(buf[1]) << 16) + (UCHAR(buf[2]) << 8) + UCHAR(buf[3]);
    buf += 4;

    /* equivalence classes */
    ecsplit = ALLOC(char, 256 + 256 + 32 * 256);
    ecmembers = ecsplit + 256;
    ecset = (Charset *) (ecmembers + 256);
    memcpy(ecsplit, buf, ecnum);
    buf += ecnum;
    memset(ecmembers, '\0', 256);
    memset(ecset, '\0', 32 * 256);
    for (i = 256; i > 0; ) {
	--i;
	c = UCHAR(eclass[i]);
	ecmembers[c]++;
	ecset[c].chars[i >> 5] |= 1 << (i & 31);
    }

    /* positions */
    posnhtab = HM->create((nposn + 1) << 2, 257, FALSE);

    /* states */
    sthtab = ALLOC(unsigned short, sthsize);
    memset(sthtab, '\0', sizeof(unsigned short) * sthsize);

    nposn = 0;
    for (i = 1, state = &states[1]; i < nstates; i++, state++) {
	buf = state->loadtmp(buf, tmpstr, posnhtab, &rpc, &nposn, grammar);
	DfaState::hash(sthtab, sthsize, states, i);
    }

    /* size info */
    tmpssize = (intptr_t) buf - (intptr_t) tmpstr;
    tmppsize -= tmpssize;
}

/*
 * save dfa to string
 */
bool Dfa::save(char **str, Uint *len)
{
    unsigned short i;
    char *buf;
    DfaState *state;
    char *pbuf;
    Uint *ptab, nposn;

    if (!modified) {
	*str = dfastr;
	*len = dfasize + tmpssize + tmppsize;
	return FALSE;
    }

    if (nstates == nexpanded + endstates) {
	tmpssize = tmppsize = 0;
    }
    if (allocated) {
	FREE(dfastr);
    }
    dfastr = buf = *str = ALLOC(char, *len = dfasize + tmpssize + tmppsize);
    *buf++ = DFA_VERSION;
    *buf++ = nstates >> 8;
    *buf++ = nstates;
    *buf++ = nexpanded >> 8;
    *buf++ = nexpanded;
    *buf++ = endstates >> 8;
    *buf++ = endstates;
    *buf++ = ecnum - 1;
    memcpy(buf, eclass, 256);
    buf += 256;

    for (i = nstates, state = &states[1]; --i > 0; state++) {
	if (state->ntrans != 0 && state->ntrans < ecnum) {
	    extend(state, ecnum - 1);
	}
	buf = state->save(buf);
    }

    modified = FALSE;
    allocated = TRUE;
    if (tmpssize + tmppsize == 0) {
	/* no tmp data */
	return TRUE;
    }

    tmpstr = buf;
    pbuf = buf + tmpssize;
    *buf++ = 0;
    *buf++ = this->nposn >> 16;
    *buf++ = this->nposn >> 8;
    *buf++ = this->nposn;
    memcpy(buf, ecsplit, ecnum);
    buf += ecnum;

    ptab = ALLOCA(Uint, this->nposn);
    nposn = 0;
    for (i = nstates, state = &states[1]; --i > 0; state++) {
	buf = state->savetmp(buf, &pbuf, tmpstr, ptab, &nposn, grammar);
    }
    AFREE(ptab);

    return TRUE;
}

/*
 * split up equivalence classes along the borders of character sets
 */
void Dfa::split(Charset *iset, Charset *cset, Uint ncset)
{
    Charset ec1, ec2;
    Uint i;
    int n, c;

    for (c = iset->firstc(0); c >= 0; c = iset->firstc(c + 1)) {
	for (i = 0; i < ncset; i++) {
	    /*
	     * get the equivalence class of the first char in the input set
	     */
	    n = UCHAR(eclass[c]);
	    if (ecmembers[n] == 1) {
		break;	/* only one character left */
	    }
	    if (ecset[n].overlap(cset, &ec1, &ec2)) {
		/*
		 * create new equivalence class
		 */
		memcpy(ecset + n, &ec1, sizeof(ec1));
		memcpy(ecset + ecnum, &ec2, sizeof(ec2));
		ecsplit[ecnum] = n;
		ecmembers[n] -= ecmembers[ecnum] = ec2.eclass(eclass, ecnum);
		ecnum++;
		dfasize += nexpanded << 1;
		tmpssize++;
	    }
	    cset++;
	}
	cset -= i;

	/* remove from input set */
	iset->sub(ecset + UCHAR(eclass[c]));
    }
}

/*
 * get the positions and strings for a new state
 */
unsigned short Dfa::newstate(DfaState *state, DfaState *newstate,
			     Charset *ecset, Charset *cset)
{
    char posn[130];
    unsigned short i, n, *s;
    RgxPosn *rp, **rrp;
    char *p;
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
	    if (ecset->intersect(cset)) {
		final = rp->trans(ecset, posn + 2, &size);
		if (size != 0) {
		    posn[0] = rp->name[0];
		    posn[1] = rp->name[1];
		    rp = RgxPosn::create(posnhtab, posn, size, &rpc, rp->rgx,
					 nposn, rp->ruleno, final);
		    if (rp->nposn == nposn) {
			/* new position */
			nposn++;
			posnsize += 8 + rp->size + final;
		    }
		    newstate->posn.a[newstate->nposn++] = rp;
		}
		if (final && newstate->final < 0) {
		    newstate->final = rp->ruleno;
		}
		cset += n;
		break;
	    }
	    cset++;
	}
    }

    /* strings */
    for (i = state->nstr, s = STRA(state); i > 0; --i, s++) {
	if (*s < nsstrings) {
	    p = strings + (*s << 2);
	    n = UCHAR(source[(UCHAR(p[0]) << 16) + (UCHAR(p[1]) << 8) +
			     UCHAR(p[2]) + state->len]);
	    p += 3;
	} else {
	    p = strings + (nsstrings << 2) + ((*s - nsstrings) << 1);
	    p = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]);
	    n = UCHAR(p[newstate->len]);
	}
	if (ecset->chars[n >> 5] & (1 << (n & 31))) {
	    if (newstate->len == UCHAR(p[0])) {
		/* end of string */
		newstate->final = nregexp + *s;
	    } else {
		/* add string */
		newstate->str.a[newstate->nstr++] = *s;
	    }
	}
    }

    return posnsize;
}

/*
 * expand a state
 */
DfaState *Dfa::expand(DfaState *state)
{
    Charset iset, *cset, *ecset;
    Uint ncset;
    RgxPosn **rrp;
    unsigned short i, n, *s;
    char *p;
    DfaState *newstate;
    RgxPosn **newposn;
    unsigned short *newstr;
    Uint size;

    newposn = NULL;
    newstr = 0;

    if (posnhtab == (Hash::Hashtab *) NULL) {
	loadtmp();	/* load tmp info */
    }

    memset(iset.chars, '\0', sizeof(iset));

    /* allocate character sets for strings and positions */
    ncset = state->nstr;
    for (i = state->nposn, rrp = POSNA(state); i > 0; --i, rrp++) {
	ncset += (*rrp)->size;
    }
    cset = ALLOCA(Charset, ncset);

    /* construct character sets for all string chars */
    for (i = state->nstr, s = STRA(state); i > 0; --i, s++) {
	if (*s < nsstrings) {
	    p = strings + (*s << 2);
	    p = source + (UCHAR(p[0]) << 16) + (UCHAR(p[1]) << 8) + UCHAR(p[2]);
	} else {
	    p = strings + (nsstrings << 2) + ((*s - nsstrings) << 1);
	    p = grammar + (UCHAR(p[0]) << 8) + UCHAR(p[1]) + 1;
	}
	n = UCHAR(p[state->len]);
	memset(cset->chars, '\0', 32);
	cset->chars[n >> 5] |= 1 << (n & 31);
	iset.chars[n >> 5] |= 1 << (n & 31);	/* also add to input set */
	cset++;
    }

    /* construct character sets for all positions */
    for (i = state->nposn, rrp = POSNA(state); i > 0; --i, rrp++) {
	(*rrp)->cset(cset);
	for (n = (*rrp)->size; n > 0; --n) {
	    iset.merge(cset++);		/* add to input set */
	}
    }
    cset -= ncset;

    /*
     * adjust equivalence classes
     */
    split(&iset, cset, ncset);

    /*
     * for all equivalence classes, compute transition states
     */
    if (state->nposn != 0) {
	newposn = ALLOCA(RgxPosn*, state->nposn);
    }
    if (state->nstr != 0) {
	newstr = ALLOCA(unsigned short, state->nstr);
    }
    p = state->trans = ALLOC(char, 2 * 256);
    state->ntrans = ecnum;
    state->alloc = TRUE;
    cset += (Uint) state->nstr;
    for (i = ecnum, ecset = this->ecset; i > 0; --i, ecset++) {
	/* prepare new state */
	newstate = &states[nstates];

	/* flesh out new state */
	newstate->posn.a = newposn;
	newstate->str.a = newstr;
	size = this->newstate(state, newstate, ecset, cset);

	if (newstate->nposn == 0 && newstate->nstr == 0 && newstate->final < 0)
	{
	    /* stuck in state 0 */
	    n = 0;
	} else {
	    if (newstate->nposn <= 1) {
		if (newstate->nposn == 0) {
		    newstate->posn.a = (RgxPosn **) NULL;
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

	    n = DfaState::hash(sthtab, sthsize, states,
			       (unsigned short) nstates);
	    if (n == nstates) {
		/*
		 * genuinely new state
		 */
		if (newstate->nposn > 1) {
		    newstate->posn.a = ALLOC(RgxPosn*, newstate->nposn);
		    memcpy(newstate->posn.a, newposn,
			   newstate->nposn * sizeof(RgxPosn*));
		}
		if (newstate->nstr > 2) {
		    newstate->str.a = ALLOC(unsigned short, newstate->nstr);
		    memcpy(newstate->str.a, newstr,
			   newstate->nstr * sizeof(unsigned short));
		}
		if (newstate->nposn == 0 && newstate->nstr == 0) {
		    newstate->ntrans = 256;
		    newstate->trans = zerotrans;
		    endstates++;
		}
		dfasize += 3;
		tmpssize += 5 + newstate->nposn * 3 + newstate->nstr * 2;
		tmppsize += size;

		if (++nstates == sttsize) {
		    /* grow table */
		    size = state - states;
		    states = REALLOC(states, DfaState, nstates, sttsize <<= 1);
		    state = states + size;
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
    AFREE(cset - (Uint) state->nstr);

    modified = TRUE;
    nexpanded++;
    dfasize += ecnum << 1;
    return state;
}

/*
 * Scan input, while lazily constructing a DFA.
 * Return values:	[0 ..>	token
 *			-1	end of string
 *			-2	Invalid token
 *			-3	DFA too large (deallocate)
 */
short Dfa::scan(String *str, ssizet *strlen, char **token, ssizet *len)
{
    ssizet size;
    unsigned short eclass;
    char *p, *q;
    DfaState *state;
    short final;
    ssizet fsize, nomatch;

    nomatch = 0;
    fsize = 0;
    size = *strlen;
    *token = str->text + str->len - size;

    while (size != 0) {
	state = &states[1];
	final = -1;
	p = str->text + str->len - size;

	while (size != 0) {
	    eclass = UCHAR(this->eclass[UCHAR(*p)]);
	    if (state->ntrans <= eclass) {
		if (state->ntrans == 0) {
		    /* expand state */
		    if (state == states) {
			break;	/* stuck in state 0 */
		    }
		    state = expand(state);
		    if (dfasize + tmpssize + tmppsize >
					    (Uint) MAX_AUTOMSZ * USHRT_MAX) {
			unsigned short save;

			/*
			 * too much temporary data: attempt to expand
			 * all states
			 */
			save = state - states;
			for (state = &states[1];
			     nstates != nexpanded + endstates;
			     state++) {
			    if (nstates > USHRT_MAX - 256 ||
				    dfasize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
				return DFA_TOOBIG;
			    }
			    if (state->ntrans == 0) {
				state = expand(state);
			    }
			}
			state = &states[save];
		    }
		    if (nstates > USHRT_MAX - 256 ||
				    dfasize > (Uint) MAX_AUTOMSZ * USHRT_MAX) {
			return DFA_TOOBIG;
		    }
		    eclass = UCHAR(this->eclass[UCHAR(*p)]);
		} else {
		    /* extend transition table */
		    extend(state, eclass);
		}
	    }

	    /* transition */
	    --size;
	    p++;
	    q = &state->trans[eclass << 1];
	    state = &states[(UCHAR(q[0]) << 8) + UCHAR(q[1])];

	    /* check if final state */
	    if (state->final >= 0) {
		final = state->final;
		fsize = size;
	    }
	}

	if (final >= 0) {
	    if (nomatch != 0) {
		if (this->nomatch != whitespace) {
		    *len = nomatch;
		    return this->nomatch;
		}
		*token += nomatch;
		nomatch = 0;
	    }

	    /* in a final state */
	    size = fsize;
	    if (final != whitespace) {
		*len = *strlen - size;
		*strlen = size;
		return final;
	    }
	    /* else whitespace: continue */
	    *token = p - 1;
	    *strlen = size;
	} else if (this->nomatch >= 0) {
	    nomatch++;
	    size = --*strlen;
	} else {
	    return DFA_REJECT;
	}
    }

    if (nomatch != 0 && this->nomatch != whitespace) {
	*len = nomatch;
	return this->nomatch;
    }

    return DFA_EOS;
}
