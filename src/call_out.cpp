/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "data.h"
# include "interpret.h"
# include "call_out.h"

# define CYCBUF_SIZE	128		/* cyclic buffer size, power of 2 */
# define CYCBUF_MASK	(CYCBUF_SIZE - 1) /* cyclic buffer mask */
# define SWPERIOD	60		/* swaprate buffer size */

# define last		prev

static CallOut *cotab;			/* callout table */
static cindex cotabsz;			/* callout table size */
static cindex queuebrk;			/* queue brk */
static cindex cycbrk;			/* cyclic buffer brk */
static cindex flist;			/* free list index */
static cindex nzero;			/* # immediate callouts */
static cindex nshort;			/* # short-term callouts, incl. nzero */
static cindex running;			/* running callouts */
static cindex immediate;		/* immediate callouts */
static cindex cycbuf[CYCBUF_SIZE];	/* cyclic buffer of callout lists */
static Uint timestamp;			/* cycbuf start time */
static Uint timeout;			/* time of first callout in cycbuf */
static Uint timediff;			/* stored/actual time difference */
static Uint cotime;			/* callout time */
static unsigned short comtime;		/* callout millisecond time */
static Uint swaptime;			/* last swap count timestamp */
static Uint swapped1[SWPERIOD];		/* swap info for last minute */
static Uint swapped5[SWPERIOD];		/* swap info for last five minutes */
static Uint swaprate1;			/* swaprate per minute */
static Uint swaprate5;			/* swaprate per 5 minutes */

/*
 * initialize callout handling
 */
bool CallOut::init(unsigned int max)
{
    if (max != 0) {
	/* only if callouts are enabled */
	cotab = ALLOC(CallOut, max + 1);
	cotab[0].time = 0;	/* sentinel for the heap */
	cotab++;
	flist = 0;
	timestamp = timeout = 0;
	timediff = 0;
    }
    running = immediate = 0;
    memset(cycbuf, '\0', sizeof(cycbuf));
    cycbrk = cotabsz = max;
    queuebrk = 0;
    nzero = nshort = 0;
    ::cotime = 0;

    swaptime = P_time();
    memset(swapped1, '\0', sizeof(swapped1));
    memset(swapped5, '\0', sizeof(swapped5));
    ::swaprate1 = ::swaprate5 = 0;

    return TRUE;
}

/*
 * put a callout in the queue
 */
CallOut *CallOut::enqueue(Uint t, unsigned short m)
{
    cindex i, j;
    CallOut *l;
    Time time;

    /*
     * create a free spot in the heap, and sift it upward
     */
# ifdef DEBUG
    if (queuebrk == cycbrk) {
	EC->fatal("callout table overflow");
    }
# endif
    i = ++queuebrk;
    l = cotab - 1;
    time = ((Time) t << 16) | m;
    for (j = i >> 1; l[j].time > time; i = j, j >>= 1) {
	l[i] = l[j];
    }

    l = &l[i];
    l->time = time;
    return l;
}

/*
 * remove a callout from the queue
 */
void CallOut::dequeue(cindex i)
{
    Time t;
    cindex j;
    CallOut *l;

    l = cotab - 1;
    i++;
    t = l[queuebrk].time;
    if (t < l[i].time) {
	/* sift upward */
	for (j = i >> 1; l[j].time > t; i = j, j >>= 1) {
	    l[i] = l[j];
	}
    } else if (i <= UINDEX_MAX / 2) {
	/* sift downward */
	for (j = i << 1; j < queuebrk; i = j, j <<= 1) {
	    if (l[j].time > l[j + 1].time) {
		j++;
	    }
	    if (t <= l[j].time) {
		break;
	    }
	    l[i] = l[j];
	}
    }
    /* put into place */
    l[i] = l[queuebrk--];
}

/*
 * allocate a new callout for the cyclic buffer
 */
CallOut *CallOut::newcallout(cindex *list, Uint t)
{
    cindex i;
    CallOut *co, *first, *last;

    if (flist != 0) {
	/* get callout from free list */
	i = flist;
	flist = cotab[i].r.next;
    } else {
	/* allocate new callout */
# ifdef DEBUG
	if (cycbrk == queuebrk || cycbrk == 1) {
	    EC->fatal("callout table overflow");
	}
# endif
	i = --cycbrk;
    }
    nshort++;
    if (t == 0) {
	nzero++;
    }

    co = &cotab[i];
    if (*list == 0) {
	/* first one in list */
	*list = i;
	co->r.count = 1;

	if (t != 0 && (timeout == 0 || t < timeout)) {
	    timeout = t;
	}
    } else {
	/* add to list */
	first = &cotab[*list];
	last = (first->r.count == 1) ? first : &cotab[first->r.last];
	last->r.next = i;
	first->r.count++;
	first->r.last = i;
    }
    co->r.prev = co->r.next = 0;

    return co;
}

/*
 * remove a callout from the cyclic buffer
 */
void CallOut::freecallout(cindex *cyc, cindex j, cindex i, Uint t)
{
    CallOut *l, *first;

    --nshort;
    if (t == 0) {
	--nzero;
    }

    l = cotab;
    first = &l[*cyc];
    if (i == j) {
	if (first->r.count == 1) {
	    *cyc = 0;

	    if (t != 0 && t == timeout) {
		if (nshort != nzero) {
		    while (cycbuf[t & CYCBUF_MASK] == 0) {
			t++;
		    }
		    timeout = t;
		} else {
		    timeout = 0;
		}
	    }
	} else {
	    *cyc = first->r.next;
	    l[first->r.next].r.count = first->r.count - 1;
	    if (first->r.count != 2) {
		l[first->r.next].r.last = first->r.last;
	    }
	}
    } else {
	--first->r.count;
	if (i == first->r.last) {
	    l[j].r.prev = l[j].r.next = 0;
	    if (first->r.count != 1) {
		first->r.last = j;
	    }
	} else {
	    l[j].r.next = l[i].r.next;
	}
    }

    l += i;
    l->handle = 0;	/* mark as unused */
    if (i == cycbrk) {
	/*
	 * callout at the edge
	 */
	while (++cycbrk != cotabsz && (++l)->handle == 0) {
	    /* followed by free callout */
	    if (cycbrk == flist) {
		/* first in the free list */
		flist = l->r.next;
	    } else {
		/* connect previous to next */
		cotab[l->r.prev].r.next = l->r.next;
		if (l->r.next != 0) {
		    /* connect next to previous */
		    cotab[l->r.next].r.prev = l->r.prev;
		}
	    }
	}
    } else {
	/* add to free list */
	if (flist != 0) {
	    /* link next to current */
	    cotab[flist].r.prev = i;
	}
	/* link to next */
	l->r.next = flist;
	flist = i;
    }
}

/*
 * get the current (adjusted) time
 */
Uint CallOut::cotime(unsigned short *mtime)
{
    Uint t;

    if (::cotime != 0) {
	*mtime = comtime;
	return ::cotime;
    }

    t = P_mtime(mtime) - timediff;
    if (t < timestamp) {
	/* clock turned back? */
	t = timestamp;
	*mtime = 0;
    } else if (timestamp < t) {
	if (running == 0) {
	    if (timeout == 0 || timeout > t) {
		timestamp = t;
	    } else if (timestamp < timeout) {
		timestamp = timeout - 1;
	    }
	}
	if (t > timestamp + 60) {
	    /* lot of lag? */
	    t = timestamp + 60;
	    *mtime = 0;
	}
    }

    comtime = *mtime;
    return ::cotime = t + timediff;
}

/*
 * check if, and how, a new callout can be added
 */
Uint CallOut::check(unsigned int n, LPCint delay, unsigned int mdelay, Uint *tp,
		    unsigned short *mp, cindex **qp)
{
    Uint t;
    unsigned short m;

    if (cotabsz == 0) {
	/*
	 * call_outs are disabled
	 */
	*qp = (cindex *) NULL;
	return 0;
    }

    if (queuebrk + (cindex) n == cycbrk || cycbrk - (cindex) n == 1) {
	EC->error("Too many callouts");
    }

    if (delay == 0 && (mdelay == 0 || mdelay == TIME_INT)) {
	/*
	 * immediate callout
	 */
	if (nshort == 0 && queuebrk == 0 && n == 0) {
	    cotime(mp);	/* initialize timestamp */
	}
	*qp = &immediate;
	*tp = t = 0;
	*mp = TIME_INT;
    } else {
	/*
	 * delayed callout
	 */
	t = cotime(mp) - timediff;
	if (t + delay + 1 <= t) {
	    EC->error("Too long delay");
	}
	t += delay;
	if (mdelay != TIME_INT) {
	    m = *mp + mdelay;
	    if (m >= 1000) {
		m -= 1000;
		t++;
	    }
	} else {
	    m = TIME_INT;
	}

	if (mdelay == TIME_INT && t < timestamp + CYCBUF_SIZE) {
	    /* use cyclic buffer */
	    *qp = &cycbuf[t & CYCBUF_MASK];
	} else {
	    /* use queue */
	    *qp = (cindex *) NULL;
	}
	*tp = t;
	*mp = m;
    }

    return t;
}

/*
 * add a callout
 */
void CallOut::create(unsigned int oindex, unsigned int handle, Uint t,
		     unsigned int m, cindex *q)
{
    CallOut *co;

    if (q != (cindex *) NULL) {
	co = newcallout(q, t);
    } else {
	if (m == TIME_INT) {
	    m = 0;
	}
	co = enqueue(t, m);
    }
    co->handle = handle;
    co->oindex = oindex;
}

/*
 * remove a short-term callout
 */
bool CallOut::rmshort(cindex *cyc, uindex i, uindex handle, Uint t)
{
    cindex j, k;
    CallOut *l;

    k = *cyc;
    if (k != 0) {
	/*
	 * this time-slot is in use
	 */
	l = cotab;
	if (l[k].oindex == i && l[k].handle == handle) {
	    /* first element in list */
	    freecallout(cyc, k, k, t);
	    return TRUE;
	}
	if (l[*cyc].r.count != 1) {
	    /*
	     * list contains more than 1 element
	     */
	    j = k;
	    k = l[j].r.next;
	    do {
		if (l[k].oindex == i && l[k].handle == handle) {
		    /* found it */
		    freecallout(cyc, j, k, t);
		    return TRUE;
		}
		j = k;
	    } while ((k = l[j].r.next) != 0);
	}
    }
    return FALSE;
}

/*
 * return the time remaining before a callout expires
 */
LPCint CallOut::remaining(Uint t, unsigned short *m)
{
    Uint time;
    unsigned short mtime;

    time = cotime(&mtime);

    if (t != 0) {
	t += timediff;
	if (*m == TIME_INT) {
	    if (t > time) {
		return t - time;
	    }
	} else if (t == time && *m > mtime) {
	    *m -= mtime;
	} else if (t > time) {
	    if (*m < mtime) {
		--t;
		*m += 1000;
	    }
	    *m -= mtime;
	    return t - time;
	} else {
	    *m = TIME_INT;
	}
    }

    return 0;
}

/*
 * remove a callout
 */
void CallOut::del(unsigned int oindex, unsigned int handle, Uint t,
		  unsigned int m)
{
    CallOut *l;

    if (m == TIME_INT) {
	/*
	 * try to find the callout in the cyclic buffer
	 */
	if (t > timestamp && t < timestamp + CYCBUF_SIZE &&
	    rmshort(&cycbuf[t & CYCBUF_MASK], oindex, handle, t)) {
	    return;
	}
    }

    if (t <= timestamp) {
	/*
	 * possible immediate callout
	 */
	if (rmshort(&immediate, oindex, handle, 0) ||
	    rmshort(&running, oindex, handle, 0)) {
	    return;
	}
    }

    /*
     * Not found in the cyclic buffer; it <must> be in the queue.
     */
    l = cotab;
    for (;;) {
# ifdef DEBUG
	if (l == cotab + queuebrk) {
	    EC->fatal("failed to remove callout");
	}
# endif
	if (l->oindex == oindex && l->handle == handle) {
	    dequeue(l - cotab);
	    return;
	}
	l++;
    }
}

/*
 * adjust callout delays in array
 */
void CallOut::list(Array *a)
{
    Value *v, *w;
    unsigned short i;
    Uint t;
    unsigned short m;
    Float flt1, flt2;

    for (i = a->size, v = a->elts; i != 0; --i, v++) {
	w = &v->array->elts[2];
	if (w->type == T_INT) {
	    t = w->number;
	    m = TIME_INT;
	} else {
	    GET_FLT(w, flt1);
	    t = flt1.low;
	    m = flt1.high;
	}
	t = remaining(t, &m);
	if (m == TIME_INT) {
	    PUT_INTVAL(w, t);
	} else {
	    Float::itof(t, &flt1);
	    Float::itof(m, &flt2);
	    flt2.div(thousand);
	    flt1.add(flt2);
	    PUT_FLTVAL(w, flt1);
	}
    }
}

/*
 * collect callouts to run next
 */
void CallOut::expire()
{
    CallOut *co, *first, *last;
    uindex handle, oindex;
    cindex i, *cyc;
    Uint t;
    unsigned short m;
    Time time;

    t = P_mtime(&m) - timediff;
    time = ((Time) t << 16) | m;
    if ((timeout != 0 && timeout <= t) ||
	(queuebrk != 0 && cotab[0].time <= time)) {
	while (timestamp < t) {
	    timestamp++;

	    /*
	     * from queue
	     */
	    while (queuebrk != 0 && (Uint) (cotab[0].time >> 16) < timestamp) {
		handle = cotab[0].handle;
		oindex = cotab[0].oindex;
		dequeue(0);
		co = newcallout(&immediate, 0);
		co->handle = handle;
		co->oindex = oindex;
	    }

	    /*
	     * from cyclic buffer list
	     */
	    cyc = &cycbuf[timestamp & CYCBUF_MASK];
	    i = *cyc;
	    if (i != 0) {
		*cyc = 0;
		if (immediate == 0) {
		    immediate = i;
		} else {
		    first = &cotab[immediate];
		    last = (first->r.count == 1) ?
			    first : &cotab[first->r.last];
		    last->r.next = i;
		    first->r.count += cotab[i].r.count;
		    first->r.last = (cotab[i].r.count == 1) ?
				     i : cotab[i].r.last;
		}
		nzero += cotab[i].r.count;
	    }
	}

	/*
	 * from queue
	 */
	while (queuebrk != 0 && cotab[0].time <= time) {
	    handle = cotab[0].handle;
	    oindex = cotab[0].oindex;
	    dequeue(0);
	    co = newcallout(&immediate, 0);
	    co->handle = handle;
	    co->oindex = oindex;
	}

	if (timeout <= timestamp) {
	    if (nshort != nzero) {
		for (t = timestamp; cycbuf[t & CYCBUF_MASK] == 0; t++) ;
		timeout = t;
	    } else {
		timeout = 0;
	    }
	}
    }

    /* handle swaprate */
    while (swaptime < t) {
	++swaptime;
	::swaprate1 -= swapped1[swaptime % SWPERIOD];
	swapped1[swaptime % SWPERIOD] = 0;
	if (swaptime % 5 == 0) {
	    ::swaprate5 -= swapped5[swaptime % (5 * SWPERIOD) / 5];
	    swapped5[swaptime % (5 * SWPERIOD) / 5] = 0;
	}
    }
}

/*
 * call expired callouts
 */
void CallOut::call(Frame *f)
{
    cindex i;
    uindex handle;
    Object *obj;
    String *str;
    int nargs;

    if (running == 0) {
	expire();
	running = immediate;
	immediate = 0;
    }

    if (running != 0) {
	/*
	 * callouts to do
	 */
	while ((i=running) != 0) {
	    handle = cotab[i].handle;
	    obj = OBJ(cotab[i].oindex);
	    freecallout(&running, i, i, 0);

	    try {
		EC->push(DGD::errHandler);
		str = obj->dataspace()->callOut(handle, f, &nargs);
		if (f->call(obj, (LWO *) NULL, str->text, str->len, TRUE,
			    nargs)) {
		    /* function exists */
		    (f->sp++)->del();
		}
		(f->sp++)->string->del();
		EC->pop();
	    } catch (const char*) { }
	    DGD::endTask();
	}
    }
}

/*
 * give information about callouts
 */
void CallOut::info(cindex *n1, cindex *n2)
{
    *n1 = nshort;
    *n2 = queuebrk;
}

/*
 * return the time until the next timeout
 */
Uint CallOut::delay(Uint rtime, unsigned short rmtime, unsigned short *mtime)
{
    Uint t;
    unsigned short m;

    if (nzero != 0) {
	/* immediate */
	*mtime = 0;
	return 0;
    }
    if ((rtime | timeout | queuebrk) == 0) {
	/* infinite */
	*mtime = 0xffff;
	return 0;
    }
    if (rtime != 0) {
	rtime -= timediff;
    }
    if (timeout != 0 && (rtime == 0 || timeout <= rtime)) {
	rtime = timeout;
	rmtime = 0;
    }
    if (queuebrk != 0 &&
	(rtime == 0 || cotab[0].time <= (((Time) rtime << 16) | rmtime))) {
	rtime = cotab[0].time >> 16;
	rmtime = cotab[0].time;
    }
    if (rtime != 0) {
	rtime += timediff;
    }

    t = cotime(&m);
    ::cotime = 0;
    if (t > rtime || (t == rtime && m >= rmtime)) {
	/* immediate */
	*mtime = 0;
	return 0;
    }
    if (m > rmtime) {
	m -= 1000;
	t++;
    }
    *mtime = rmtime - m;
    return rtime - t;
}

/*
 * keep track of the number of objects swapped out
 */
void CallOut::swapcount(unsigned int count)
{
    ::swaprate1 += count;
    ::swaprate5 += count;
    swapped1[swaptime % SWPERIOD] += count;
    swapped5[swaptime % (SWPERIOD * 5) / 5] += count;
    ::cotime = 0;
}

/*
 * return the number of objects swapped out per minute
 */
long CallOut::swaprate1()
{
    return ::swaprate1;
}

/*
 * return the number of objects swapped out per 5 minutes
 */
long CallOut::swaprate5()
{
    return ::swaprate5;
}


struct CallOut0 {
    uindex handle;		/* callout handle */
    uindex oindex;		/* index in object table */
    Uint time;			/* when to call */
    uindex htime;		/* when to call, high word */
    uindex mtime;		/* when to call, milliseconds */
};

# define CO0_LAYOUT	"uuiuu"

struct CallOutHeader {
    cindex cotabsz;		/* callout table size */
    cindex queuebrk;		/* queue brk */
    cindex cycbrk;		/* cyclic buffer brk */
    cindex flist;		/* free list index */
    cindex nshort;		/* # of short-term callouts */
    cindex running;		/* running callouts */
    cindex immediate;		/* immediate callouts list */
    unsigned short hstamp;	/* timestamp high word */
    unsigned short hdiff;	/* timediff high word */
    Uint timestamp;		/* time the last alarm came */
    Uint timediff;		/* accumulated time difference */
};

static char dh_layout[] = "fffffffssii";

/*
 * dump callout table
 */
bool CallOut::save(int fd)
{
    CallOutHeader dh;
    unsigned short m;

    /* update timestamp */
    cotime(&m);
    ::cotime = 0;

    /* fill in header */
    dh.cotabsz = cotabsz;
    dh.queuebrk = queuebrk;
    dh.cycbrk = cycbrk;
    dh.flist = flist;
    dh.nshort = nshort;
    dh.running = running;
    dh.immediate = immediate;
    dh.hstamp = 0;
    dh.hdiff = 0;
    dh.timestamp = timestamp;
    dh.timediff = timediff;

    /* write header and callouts */
    return (Swap::write(fd, &dh, sizeof(CallOutHeader)) &&
	    (queuebrk == 0 ||
	     Swap::write(fd, cotab, queuebrk * sizeof(CallOut))) &&
	    (cycbrk == cotabsz ||
	     Swap::write(fd, cotab + cycbrk,
			 (cotabsz - cycbrk) * sizeof(CallOut))) &&
	    Swap::write(fd, cycbuf, CYCBUF_SIZE * sizeof(cindex)));
}

/*
 * restore callout table
 */
void CallOut::restore(int fd, Uint t, bool conv16)
{
    CallOutHeader dh;
    cindex n, i, offset;
    CallOut *co;
    cindex *cb;

    /* read and check header */
    timediff = t;

    Config::dread(fd, (char *) &dh, dh_layout, (Uint) 1);
    queuebrk = dh.queuebrk;
    offset = cotabsz - dh.cotabsz;
    cycbrk = dh.cycbrk + offset;
    flist = dh.flist;
    nshort = dh.nshort;
    running = dh.running;
    immediate = dh.immediate;
    timestamp = dh.timestamp;

    timediff -= timestamp;
    if (queuebrk > cycbrk || cycbrk == 0) {
	EC->error("Restored too many callouts");
    }

    /* read tables */
    n = queuebrk + cotabsz - cycbrk;
    if (n != 0) {
	if (conv16) {
	    CallOut0 *co0;

	    co0 = ALLOC(CallOut0, n);
	    Config::dread(fd, (char *) co0, CO0_LAYOUT, (Uint) n);
	    for (i = 0; i < queuebrk; co0++, i++) {
		cotab[i].time = (((Time) co0->htime) << 48) |
				(((Time) co0->time) << 16) | co0->mtime;
		cotab[i].handle = co0->handle;
		cotab[i].oindex = co0->oindex;
	    }
	    for (i = cycbrk; i < cotabsz; co0++, i++) {
		cotab[i].r.count = co0->time;
		cotab[i].r.prev = co0->htime;
		cotab[i].r.next = co0->mtime;
		cotab[i].handle = co0->handle;
		cotab[i].oindex = co0->oindex;
	    }
	    FREE(co0 - queuebrk - cotabsz + cycbrk);
	} else {
	    Config::dread(fd, (char *) cotab, CO1_LAYOUT, (Uint) queuebrk);
	    Config::dread(fd, (char *) (cotab + cycbrk), CO2_LAYOUT,
			  (Uint) (cotabsz - cycbrk));
	}
    }
    Config::dread(fd, (char *) cycbuf, "f", (Uint) CYCBUF_SIZE);

    nzero = 0;
    if (running != 0) {
	running += offset;
	nzero += cotab[running].r.count;
    }
    if (immediate != 0) {
	immediate += offset;
	nzero += cotab[immediate].r.count;
    }

    if (offset != 0) {
	/* patch callout references */
	if (flist != 0) {
	    flist += offset;
	}
	for (i = CYCBUF_SIZE, cb = cycbuf; i > 0; --i, cb++) {
	    if (*cb != 0) {
		*cb += offset;
	    }
	}
	for (i = cotabsz - cycbrk, co = cotab + cycbrk; i > 0; --i, co++) {
	    if (co->r.prev != 0) {
		co->r.prev += offset;
	    }
	    if (co->r.next != 0) {
		co->r.next += offset;
	    }
	}
    }

    /* restart callouts */
    if (nshort != nzero) {
	for (t = timestamp; cycbuf[t & CYCBUF_MASK] == 0; t++) ;
	timeout = t;
    }
}
