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

# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"

# define CYCBUF_SIZE	128		/* cyclic buffer size, power of 2 */
# define CYCBUF_MASK	(CYCBUF_SIZE - 1) /* cyclic buffer mask */
# define SWPERIOD	60		/* swaprate buffer size */

typedef struct {
    uindex handle;	/* callout handle */
    uindex oindex;	/* index in object table */
    Uint time;		/* when to call */
    uindex mtime;	/* when to call in milliseconds */
} call_out;

static char co_layout[] = "uuiu";

# define prev		oindex
# define next		time
# define count		mtime

struct _cbuf_ {
    uindex list;	/* list */
    uindex last;	/* last in list */
};

static char cb_layout[] = "uu";

static call_out *cotab;			/* callout table */
static uindex cotabsz;			/* callout table size */
static uindex queuebrk;			/* queue brk */
static uindex cycbrk;			/* cyclic buffer brk */
static uindex flist;			/* free list index */
static uindex nzero;			/* # immediate callouts */
static uindex nshort;			/* # short-term callouts, incl. nzero */
static cbuf running;			/* running callouts */
static cbuf immediate;			/* immediate callouts */
static cbuf cycbuf[CYCBUF_SIZE];	/* cyclic buffer of callout lists */
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
 * NAME:	call_out->init()
 * DESCRIPTION:	initialize callout handling
 */
bool co_init(max)
unsigned int max;
{
    if (max != 0) {
	/* only if callouts are enabled */
	cotab = ALLOC(call_out, max + 1);
	cotab[0].time = 0;	/* sentinel for the heap */
	cotab[0].mtime = 0;
	cotab++;
	flist = 0;
	timestamp = timeout = 0;
	timediff = 0;
    }
    running.list = immediate.list = 0;
    memset(cycbuf, '\0', sizeof(cycbuf));
    cycbrk = cotabsz = max;
    queuebrk = 0;
    nzero = nshort = 0;
    cotime = 0;

    swaptime = P_time();
    memset(swapped1, '\0', sizeof(swapped1));
    memset(swapped5, '\0', sizeof(swapped5));
    swaprate1 = swaprate5 = 0;

    return TRUE;
}

/*
 * NAME:	enqueue()
 * DESCRIPTION:	put a callout in the queue
 */
static call_out *enqueue(t, m)
register Uint t;
unsigned short m;
{
    register uindex i, j;
    register call_out *l;

    /*
     * create a free spot in the heap, and sift it upward
     */
# ifdef DEBUG
    if (queuebrk == cycbrk) {
	fatal("callout table overflow");
    }
# endif
    i = ++queuebrk;
    l = cotab - 1;
    for (j = i >> 1; l[j].time > t || (l[j].time == t && l[j].mtime > m);
	 i = j, j >>= 1) {
	l[i] = l[j];
    }

    l = &l[i];
    l->time = t;
    l->mtime = m;
    return l;
}

/*
 * NAME:	dequeue()
 * DESCRIPTION:	remove a callout from the queue
 */
static void dequeue(i)
register uindex i;
{
    register Uint t;
    register short m;
    register uindex j;
    register call_out *l;

    l = cotab - 1;
    i++;
    t = l[queuebrk].time;
    m = l[queuebrk].mtime;
    if (t < l[i].time) {
	/* sift upward */
	for (j = i >> 1; l[j].time > t || (l[j].time == t && l[j].mtime > m);
	     i = j, j >>= 1) {
	    l[i] = l[j];
	}
    } else if (i <= UINDEX_MAX / 2) {
	/* sift downward */
	for (j = i << 1; j < queuebrk; i = j, j <<= 1) {
	    if (l[j].time > l[j + 1].time ||
		(l[j].time == l[j + 1].time && l[j].mtime > l[j + 1].mtime)) {
		j++;
	    }
	    if (t < l[j].time || (t == l[j].time && m <= l[j].mtime)) {
		break;
	    }
	    l[i] = l[j];
	}
    }
    /* put into place */
    l[i] = l[queuebrk--];
}

/*
 * NAME:	newcallout()
 * DESCRIPTION:	allocate a new callout for the cyclic buffer
 */
static call_out *newcallout(list, t)
register cbuf *list;
Uint t;
{
    register uindex i;
    register call_out *co;

    if (flist != 0) {
	/* get callout from free list */
	i = flist;
	flist = cotab[i].next;
    } else {
	/* allocate new callout */
# ifdef DEBUG
	if (cycbrk == queuebrk || cycbrk == 1) {
	    fatal("callout table overflow");
	}
# endif
	i = --cycbrk;
    }
    nshort++;
    if (t == 0) {
	nzero++;
    }

    co = &cotab[i];
    if (list->list == 0) {
	/* first one in list */
	list->list = i;
	co->count = 1;

	if (t != 0 && (timeout == 0 || t < timeout)) {
	    timeout = t;
	}
    } else {
	/* add to list */
	cotab[list->list].count++;
	cotab[list->last].next = i;
    }
    list->last = i;
    co->next = 0;

    return co;
}

/*
 * NAME:	freecallout()
 * DESCRIPTION:	remove a callout from the cyclic buffer
 */
static void freecallout(cyc, j, i, t)
register cbuf *cyc;
register uindex j, i;
register Uint t;
{
    register call_out *l;

    --nshort;
    if (t == 0) {
	--nzero;
    }

    l = cotab;
    if (i == j) {
	cyc->list = l[i].next;
	if (cyc->list != 0) {
	    l[cyc->list].count = l[i].count - 1;
	} else if (t != 0 && t == timeout) {
	    if (nshort != nzero) {
		while (cycbuf[t & CYCBUF_MASK].list == 0) {
		    t++;
		}
		timeout = t;
	    } else {
		timeout = 0;
	    }
	}
    } else {
	if (i == cyc->last) {
	    /* last element of the list */
	    l[cyc->last = j].next = 0;
	} else {
	    /* connect previous to next */
	    l[j].next = l[i].next;
	}
	--l[cyc->list].count;
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
		flist = l->next;
	    } else {
		/* connect previous to next */
		cotab[l->prev].next = l->next;
		if (l->next != 0) {
		    /* connect next to previous */
		    cotab[l->next].prev = l->prev;
		}
	    }
	}
    } else {
	/* add to free list */
	if (flist != 0) {
	    /* link next to current */
	    cotab[flist].prev = i;
	}
	/* link to next */
	l->next = flist;
	flist = i;
    }
}

/*
 * NAME:	call_out->decode()
 * DESCRIPTION:	decode millisecond time
 */
Uint co_decode(time, mtime)
register Uint time;
unsigned short *mtime;
{
    *mtime = time & 0xffff;
    time = ((timestamp - timediff) & 0xffffff00L) + ((time >> 16) & 0xff);
    if (time + timediff < timestamp) {
	time += 0x100;
    }
    return time;
}

/*
 * NAME:	call_out->time()
 * DESCRIPTION:	get the current (adjusted) time
 */
Uint co_time(mtime)
unsigned short *mtime;
{
    Uint t;

    if (cotime != 0) {
	*mtime = comtime;
	return cotime;
    }

    t = P_mtime(mtime);
    if (t < timestamp) {
	/* clock turned back? */
	t = timestamp;
	*mtime = 0;
    } else if (timestamp < t) {
	if (running.list == 0) {
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
    return cotime = t;
}

/*
 * NAME:	call_out->check()
 * DESCRIPTION:	check if, and how, a new callout can be added
 */
Uint co_check(n, delay, mdelay, tp, mp, qp)
unsigned int n, mdelay;
Int delay;
Uint *tp;
unsigned short *mp;
cbuf **qp;
{
    register Uint t;
    register unsigned short m;

    if (cotabsz == 0) {
	/*
	 * call_outs are disabled
	 */
	*qp = (cbuf *) NULL;
	return 0;
    }

    if (queuebrk + (uindex) n == cycbrk || cycbrk - (uindex) n == 1) {
	error("Too many callouts");
    }

    if (delay == 0 && (mdelay == 0 || mdelay == 0xffff)) {
	/*
	 * immediate callout
	 */
	if (nshort == 0 && queuebrk == 0 && n == 0) {
	    co_time(mp);	/* initialize timestamp */
	}
	*qp = &immediate;
	*tp = t = 0;
	*mp = 0xffff;
    } else {
	/*
	 * delayed callout
	 */
	t = co_time(mp);
	if (t + delay + 1 <= t) {
	    error("Too long delay");
	}
	t += delay;
	if (mdelay != 0xffff) {
	    m = *mp + mdelay;
	    if (m >= 1000) {
		m -= 1000;
		t++;
	    }
	} else {
	    m = 0xffff;
	}

	if (mdelay == 0xffff && t < timestamp + CYCBUF_SIZE) {
	    /* use cyclic buffer */
	    *qp = &cycbuf[t & CYCBUF_MASK];
	} else {
	    /* use queue */
	    *qp = (cbuf *) NULL;
	}
	*tp = t;
	*mp = m;

	t -= timediff;
    }

    return t;
}

/*
 * NAME:	call_out->new()
 * DESCRIPTION:	add a callout
 */
void co_new(oindex, handle, t, m, q)
unsigned int oindex, handle, m;
Uint t;
cbuf *q;
{
    register call_out *co;

    if (q != (cbuf *) NULL) {
	co = newcallout(q, t);
    } else {
	if (m == 0xffff) {
	    m = 0;
	}
	co = enqueue(t, m);
    }
    co->handle = handle;
    co->oindex = oindex;
}

/*
 * NAME:	rmshort()
 * DESCRIPTION:	remove a short-term callout
 */
static bool rmshort(cyc, i, handle, t)
register cbuf *cyc;
register uindex i, handle;
Uint t;
{
    register uindex j, k;
    register call_out *l;

    k = cyc->list;
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
	if (k != cyc->last) {
	    /*
	     * list contains more than 1 element
	     */
	    j = k;
	    k = l[j].next;
	    do {
		if (l[k].oindex == i && l[k].handle == handle) {
		    /* found it */
		    freecallout(cyc, j, k, t);
		    return TRUE;
		}
		j = k;
	    } while ((k=l[j].next) != 0);
	}
    }
    return FALSE;
}

/*
 * NAME:	call_out->remaining()
 * DESCRIPTION:	return the time remaining before a callout expires
 */
Int co_remaining(t, m)
register Uint t;
register unsigned short *m;
{
    Uint time;
    unsigned short mtime;

    time = co_time(&mtime);

    if (t != 0) {
	t += timediff;
	if (*m == 0xffff) {
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
	    *m = 0xffff;
	}
    }

    return 0;
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	remove a callout
 */
void co_del(oindex, handle, t, m)
register unsigned int oindex, handle, m;
Uint t;
{
    register call_out *l;

    if (t != 0) {
	t += timediff;
    }

    if (m == 0xffff) {
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
	    fatal("failed to remove callout");
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
 * NAME:	call_out->list()
 * DESCRIPTION:	adjust callout delays in array
 */
void co_list(a)
array *a;
{
    register value *v, *w;
    register unsigned short i;
    Uint t;
    unsigned short m;
    xfloat flt1, flt2;

    for (i = a->size, v = a->elts; i != 0; --i, v++) {
	w = &v->u.array->elts[2];
	if (w->type == T_INT) {
	    t = w->u.number;
	    m = 0xffff;
	} else {
	    GET_FLT(w, flt1);
	    t = flt1.low;
	    m = flt1.high;
	}
	t = co_remaining(t, &m);
	if (m == 0xffff) {
	    PUT_INTVAL(w, t);
	} else {
	    flt_itof(t, &flt1);
	    flt_itof(m, &flt2);
	    flt_mult(&flt2, &thousandth);
	    flt_add(&flt1, &flt2);
	    PUT_FLTVAL(w, flt1);
	}
    }
}

/*
 * NAME:	call_out->expire()
 * DESCRIPTION:	collect callouts to run next
 */
static void co_expire()
{
    register call_out *co;
    register uindex handle, oindex, i;
    register cbuf *cyc;
    Uint t;
    unsigned short m;

    t = P_mtime(&m);
    if ((timeout != 0 && timeout <= t) ||
	(queuebrk != 0 &&
	 (cotab[0].time < t || (cotab[0].time == t && cotab[0].mtime <= m)))) {
	while (timestamp < t) {
	    timestamp++;

	    /*
	     * from queue
	     */
	    while (queuebrk != 0 && cotab[0].time < timestamp) {
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
	    i = cyc->list;
	    if (i != 0) {
		cyc->list = 0;
		if (immediate.list == 0) {
		    immediate.list = i;
		} else {
		    cotab[immediate.last].next = i;
		}
		immediate.last = cyc->last;
		i = cotab[i].count;
		cotab[immediate.list].count += i;
		nzero += i;
	    }
	}

	/*
	 * from queue
	 */
	while (queuebrk != 0 &&
	       (cotab[0].time < t ||
		(cotab[0].time == t && cotab[0].mtime <= m))) {
	    handle = cotab[0].handle;
	    oindex = cotab[0].oindex;
	    dequeue(0);
	    co = newcallout(&immediate, 0);
	    co->handle = handle;
	    co->oindex = oindex;
	}

	if (timeout <= timestamp) {
	    if (nshort != nzero) {
		for (t = timestamp; cycbuf[t & CYCBUF_MASK].list == 0; t++) ;
		timeout = t;
	    } else {
		timeout = 0;
	    }
	}
    }

    /* handle swaprate */
    while (swaptime < t) {
	++swaptime;
	swaprate1 -= swapped1[swaptime % SWPERIOD];
	swapped1[swaptime % SWPERIOD] = 0;
	if (swaptime % 5 == 0) {
	    swaprate5 -= swapped5[swaptime % (5 * SWPERIOD) / 5];
	    swapped5[swaptime % (5 * SWPERIOD) / 5] = 0;
	}
    }
}

/*
 * NAME:	call_out->call()
 * DESCRIPTION:	call expired callouts
 */
void co_call(f)
frame *f;
{
    register uindex i, handle;
    object *obj;
    string *str;
    int nargs;

    co_expire();
    running = immediate;
    immediate.list = 0;

    if (running.list != 0) {
	/*
	 * callouts to do
	 */
	while (ec_push((ec_ftn) errhandler)) {
	    endthread();
	}
	while ((i=running.list) != 0) {
	    handle = cotab[i].handle;
	    obj = OBJ(cotab[i].oindex);
	    freecallout(&running, i, i, 0);

	    str = d_get_call_out(o_dataspace(obj), handle, f, &nargs);
	    if (i_call(f, obj, (array *) NULL, str->text, str->len, TRUE,
		       nargs)) {
		/* function exists */
		i_del_value(f->sp++);
	    }
	    str_del((f->sp++)->u.string);
	    endthread();
	}
	ec_pop();
    }
}

/*
 * NAME:	call_out->info()
 * DESCRIPTION:	give information about callouts
 */
void co_info(n1, n2)
uindex *n1, *n2;
{
    *n1 = nshort;
    *n2 = queuebrk;
}

/*
 * NAME:	call_out->delay()
 * DESCRIPTION:	return the time until the next timeout
 */
Uint co_delay(rtime, rmtime, mtime)
register Uint rtime;
register unsigned int rmtime;
unsigned short *mtime;
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
    if (timeout != 0 && (rtime == 0 || timeout <= rtime)) {
	rtime = timeout;
	rmtime = 0;
    }
    if (queuebrk != 0 &&
	(rtime == 0 || cotab[0].time < rtime ||
	 (cotab[0].time == rtime && cotab[0].mtime <= rmtime))) {
	rtime = cotab[0].time;
	rmtime = cotab[0].mtime;
    }

    t = co_time(&m);
    cotime = 0;
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
 * NAME:	call_out->swapcount()
 * DESCRIPTION:	keep track of the number of objects swapped out
 */
void co_swapcount(count)
unsigned int count;
{
    swaprate1 += count;
    swaprate5 += count;
    swapped1[swaptime % SWPERIOD] += count;
    swapped5[swaptime % (SWPERIOD * 5) / 5] += count;
    cotime = 0;
}

/*
 * NAME:	call_out->swaprate1()
 * DESCRIPTION:	return the number of objects swapped out per minute
 */
long co_swaprate1()
{
    return swaprate1;
}

/*
 * NAME:	call_out->swaprate5()
 * DESCRIPTION:	return the number of objects swapped out per 5 minutes
 */
long co_swaprate5()
{
    return swaprate5;
}


typedef struct {
    uindex cotabsz;		/* callout table size */
    uindex queuebrk;		/* queue brk */
    uindex cycbrk;		/* cyclic buffer brk */
    uindex flist;		/* free list index */
    uindex nshort;		/* # of short-term callouts */
    uindex nlong0;		/* # of long-term callouts and imm. callouts */
    Uint timestamp;		/* time the last alarm came */
    Uint timediff;		/* accumulated time difference */
} dump_header;

static char dh_layout[] = "uuuuuuii";

typedef struct {
    uindex handle;	/* callout handle */
    uindex oindex;	/* index in object table */
    Uint time;		/* when to call */
} dump_callout;

static char dco_layout[] = "uui";

/*
 * NAME:	call_out->dump()
 * DESCRIPTION:	dump callout table
 */
bool co_dump(fd)
int fd;
{
    dump_header dh;
    register uindex list, last;
    register call_out *co, *dc;
    register uindex n;
    register cbuf *cb;
    unsigned short m;
    bool ret;

    /* update timestamp */
    co_time(&m);
    cotime = 0;

    /* fill in header */
    dh.cotabsz = cotabsz;
    dh.queuebrk = queuebrk;
    dh.cycbrk = cycbrk;
    dh.flist = flist;
    dh.nshort = nshort;
    dh.nlong0 = queuebrk + nzero;
    dh.timestamp = timestamp;
    dh.timediff = timediff;

    /* copy callouts */
    n = queuebrk + cotabsz - cycbrk;
    if (n != 0) {
	dc = ALLOCA(call_out, n);
	for (co = cotab, n = queuebrk; n != 0; co++, --n) {
	    dc->handle = co->handle;
	    dc->oindex = co->oindex;
	    dc->time = co->time;
	    dc->mtime = co->mtime;
	    dc++;
	}
	for (co = cotab + cycbrk, n = cotabsz - cycbrk; n != 0; co++, --n) {
	    dc->handle = co->handle;
	    dc->oindex = co->oindex;
	    dc->time = co->time;
	    dc++;
	}
	n = queuebrk + cotabsz - cycbrk;
	dc -= n;
    }

    /* deal with immediate callouts */
    if (nzero != 0) {
	if (running.list != 0) {
	    list = running.list;
	    if (immediate.list != 0) {
		dc[running.last + n - cotabsz].next = immediate.list;
		last = immediate.last;
	    } else {
		last = running.last;
	    }
	} else {
	    list = immediate.list;
	    last = immediate.last;
	}
	cb = &cycbuf[timestamp & CYCBUF_MASK];
	last = dc[last + n - cotabsz].next = cb->list;
	cb->list = list;
    }

    /* write header and callouts */
    ret = (P_write(fd, (char *) &dh, sizeof(dump_header)) > 0 &&
	   (n == 0 || P_write(fd, (char *) dc, n * sizeof(call_out)) > 0) &&
	   P_write(fd, (char *) cycbuf, CYCBUF_SIZE * sizeof(cbuf)) > 0);

    if (n != 0) {
	AFREE(dc);
    }

    if (nzero != 0) {
	cb->list = last;
    }

    return ret;
}

/*
 * NAME:	call_out->restore()
 * DESCRIPTION:	restore callout table
 */
void co_restore(fd, t, conv)
int fd, conv;
register Uint t;
{
    register uindex n, i, offset, last;
    register call_out *co;
    register cbuf *cb;
    dump_header dh;
    cbuf buffer[CYCBUF_SIZE];
    unsigned short m;

    /* read and check header */
    conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);
    queuebrk = dh.queuebrk;
    offset = cotabsz - dh.cotabsz;
    cycbrk = dh.cycbrk + offset;
    if (queuebrk > cycbrk + offset || cycbrk == 0) {
	error("Restored too many callouts");
    }

    flist = dh.flist;
    nshort = dh.nshort;
    nzero = dh.nlong0 - dh.queuebrk;
    timestamp = t;
    t -= dh.timestamp;
    timediff = dh.timediff + t;

    /* read tables */
    n = queuebrk + cotabsz - cycbrk;
    if (n != 0) {
	if (conv) {
	    register dump_callout *dc;

	    dc = ALLOCA(dump_callout, n);
	    conf_dread(fd, (char *) dc, dco_layout, (Uint) n);

	    for (co = cotab, i = queuebrk; i != 0; co++, --i) {
		co->handle = dc->handle;
		co->oindex = dc->oindex;
		if (dc->time >> 24 == 1) {
		    co->time = co_decode(dc->time, &m) + timediff;
		    co->mtime = m;
		} else {
		    co->time = dc->time + t;
		    co->mtime = 0;
		}
		dc++;
	    }
	    for (co = cotab + cycbrk, i = cotabsz - cycbrk; i != 0; co++, --i) {
		co->handle = dc->handle;
		co->oindex = dc->oindex;
		co->time = dc->time;
		dc++;
	    }
	    AFREE(dc - n);
	} else {
	    register call_out *dc;

	    dc = ALLOCA(call_out, n);
	    conf_dread(fd, (char *) dc, co_layout, (Uint) n);

	    for (co = cotab, i = queuebrk; i != 0; co++, --i) {
		co->handle = dc->handle;
		co->oindex = dc->oindex;
		co->time = dc->time + t;
		co->mtime = dc->mtime;
		dc++;
	    }
	    for (co = cotab + cycbrk, i = cotabsz - cycbrk; i != 0; co++, --i) {
		co->handle = dc->handle;
		co->oindex = dc->oindex;
		co->time = dc->time;
		dc++;
	    }
	    AFREE(dc - n);
	}
    }
    conf_dread(fd, (char *) buffer, cb_layout, (Uint) CYCBUF_SIZE);

    /* cycle around cyclic buffer */
    t &= CYCBUF_MASK;
    memcpy(cycbuf + t, buffer, (unsigned int) (CYCBUF_SIZE - t) * sizeof(cbuf));
    memcpy(cycbuf, buffer + CYCBUF_SIZE - t, (unsigned int) t * sizeof(cbuf));

    if (offset != 0) {
	/* patch callout references */
	if (flist != 0) {
	    flist += offset;
	}
	for (i = CYCBUF_SIZE, cb = cycbuf; i > 0; --i, cb++) {
	    if (cb->list != 0) {
		cb->list += offset;
		cb->last += offset;
	    }
	}
	for (i = cotabsz - cycbrk, co = cotab + cycbrk; i > 0; --i, co++) {
	    if (co->handle == 0) {
		co->prev += offset;
	    }
	    if (co->next != 0) {
		co->next += offset;
	    }
	}
    }

    /* fix up immediate callouts */
    if (nzero != 0) {
	cb = &cycbuf[timestamp & CYCBUF_MASK];
	immediate.list = cb->list;
	for (i = nzero - 1, last = cb->list; i != 0; --i) {
	    last = cotab[last].next;
	}
	immediate.last = last;
	cotab[immediate.list].count = nzero;
	cb->list = cotab[last].next;
	cotab[last].next = 0;
    }

    /* add counts */
    for (i = CYCBUF_SIZE, cb = cycbuf; i != 0; --i, cb++) {
	if (cb->list != 0) {
	    n = 0;
	    last = cb->list;
	    do {
		last = cotab[last].next;
		n++;
	    } while (last != 0);
	    cotab[cb->list].count = n;
	}
    }

    /* restart callouts */
    if (nshort != nzero) {
	for (t = timestamp; cycbuf[t & CYCBUF_MASK].list == 0; t++) ;
	timeout = t;
    }
}
