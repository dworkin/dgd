# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "comm.h"
# include "call_out.h"

# define CYCBUF_SIZE	128		/* cyclic buffer size, power of 2 */
# define CYCBUF_MASK	(CYCBUF_SIZE - 1) /* cyclic buffer mask */
# define SWPERIOD	60		/* swaprate buffer size */

typedef struct {
    uindex handle;	/* callout handle */
    uindex oindex;	/* index in object table */
    Uint time;		/* when to call */
} call_out;

# define prev		oindex
# define next		time

static char co_layout[] = "uui";

typedef struct {
    uindex list;	/* list */
    uindex last;	/* last in list */
} cbuf;

static char cb_layout[] = "uu";

static call_out *cotab;			/* callout table */
static uindex cotabsz;			/* callout table size */
static uindex queuebrk;			/* queue brk */
static uindex cycbrk;			/* cyclic buffer brk */
static uindex flist;			/* free list index */
static uindex nzero;			/* # immediate callouts */
static uindex nshort;			/* # short-term callouts, incl. nzero */
static cbuf c0first, c0next;		/* immediate callouts */
static cbuf cycbuf[CYCBUF_SIZE];	/* cyclic buffer of callout lists */
static Uint timestamp;			/* cycbuf start time */
static Uint timeout;			/* time the last alarm came */
static Uint timediff;			/* stored/actual time difference */
static int fragment;			/* swap fragment */
static uindex swapped1[SWPERIOD];	/* swap info for last minute */
static uindex swapped5[SWPERIOD];	/* swap info for last five minutes */
static int swapidx1, swapidx5;		/* index in swap info arrays */
static int incr5;			/* 5 second period counter */
static uindex swaps5;			/* swapout info for last 5 seconds */
static Uint swaprate1;			/* swaprate per minute */
static Uint swaprate5;			/* swaprate per 5 minutes */

/*
 * NAME:	call_out->init()
 * DESCRIPTION:	initialize callout handling
 */
void co_init(max, frag)
unsigned int max;
int frag;
{
    if (max != 0) {
	cotab = ALLOC(call_out, max + 1);
	cotab[0].time = 0;	/* sentinel for the heap */
	cotab++;
	flist = 0;
	/* only if callouts are enabled */
	timeout = timestamp = P_time();
	timediff = 0;
	P_alarm(1);
    }
    c0first.list = c0next.list = 0;
    memset(cycbuf, '\0', sizeof(cycbuf));
    cycbrk = cotabsz = max;
    queuebrk = 0;
    nzero = nshort = 0;

    fragment = frag;
    memset(swapped1, '\0', sizeof(swapped1));
    memset(swapped5, '\0', sizeof(swapped5));
    swapidx1 = swapidx5 = swaps5 = 0;
    incr5 = 5;
    swaprate1 = swaprate5 = 0;
}

/*
 * NAME:	enqueue()
 * DESCRIPTION:	put a callout in the queue
 */
static uindex enqueue(t)
register Uint t;
{
    register uindex i, j;
    register call_out *l;

    if (queuebrk == cycbrk) {
	error("Too many callouts");
    }

    /*
     * create a free spot in the heap, and sift it upward
     */
    i = ++queuebrk;
    l = cotab - 1;
    for (j = i >> 1; l[j].time > t; i = j, j >>= 1) {
	l[i] = l[j];
    }
    /* return index of free spot */
    return i - 1;
}

/*
 * NAME:	dequeue()
 * DESCRIPTION:	remove a callout from the queue
 */
static void dequeue(i)
register uindex i;
{
    register Uint t;
    register uindex j;
    register call_out *l;

    l = cotab - 1;
    i++;
    t = l[queuebrk].time;
    if (t < l[i].time) {
	/* sift upward */
	for (j = i >> 1; l[j].time > t; i = j, j >>= 1) {
	    l[i] = l[j];
	}
    } else {
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
 * NAME:	newcallout()
 * DESCRIPTION:	get a new callout for the cyclic buffer
 */
static uindex newcallout()
{
    register uindex i;

    if (flist != 0) {
	/* get callout from free list */
	i = flist;
	flist = cotab[i].next;
    } else {
	/* allocate new callout */
	if (cycbrk == queuebrk || cycbrk == 1) {
	    error("Too many callouts");
	}
	i = --cycbrk;
    }

    nshort++;
    return i;
}

/*
 * NAME:	freecallout()
 * DESCRIPTION:	remove a callout from the cyclic buffer
 */
static void freecallout(i)
register uindex i;
{
    register call_out *l;

    l = &cotab[i];
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

    --nshort;
}

/*
 * NAME:	call_out->new()
 * DESCRIPTION:	add a new callout
 */
uindex co_new(obj, str, delay, f, nargs)
object *obj;
string *str;
Int delay;
frame *f;
int nargs;
{
    Uint t;
    uindex i;
    register call_out *co;

    if (cotabsz == 0) {
	/*
	 * Call_outs are disabled.  Return immediately.
	 */
	return 0;
    }

    if (obj->data->ncallouts >= conf_array_size()) {
	/*
	 * A secondary effect of the max array size.  This is not very neat,
	 * but an error here is better than when listing the callouts in
	 * the object.
	 */
	error("Too many callouts in object");
    }

    if (delay == 0) {
	/*
	 * immediate callout
	 */
	i = newcallout();
	if (c0next.list == 0) {
	    /* first one in list */
	    c0next.list = i;
	} else {
	    /* add to list */
	    cotab[c0next.last].next = i;
	}
	co = &cotab[c0next.last = i];
	co->next = 0;
	nzero++;

	t = 0;
    } else {
	/*
	 * delayed callout
	 */
	t = P_time();
	if (t + delay < t) {
	    error("Too long delay");
	}
	t += delay;
	if (t <= timeout) {
	    t = timeout + 1;
	}

	if (t < timestamp + CYCBUF_SIZE) {
	    register cbuf *cyc;

	    /* add to cyclic buffer */
	    i = newcallout();
	    cyc = &cycbuf[t & CYCBUF_MASK];
	    if (cyc->list == 0) {
		/* first one in list */
		cyc->list = i;
	    } else {
		/* add to list */
		cotab[cyc->last].next = i;
	    }
	    co = &cotab[cyc->last = i];
	    co->next = 0;
	} else {
	    /* put in queue */
	    i = enqueue(t);
	    co = &cotab[i];
	    co->time = t;
	}

	t -= timediff;
    }

    co->handle = d_new_call_out(obj->data, str, t, f, nargs);
    co->oindex = obj->index;

    return co->handle;
}

/*
 * NAME:	rmshort()
 * DESCRIPTION:	remove a short-term callout
 */
bool rmshort(cyc, i, handle)
register cbuf *cyc;
register uindex i, handle;
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
	    cyc->list = l[k].next;
	    freecallout(k);
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
		    if (k == cyc->last) {
			/* last element of the list */
			l[cyc->last = j].next = 0;
		    } else {
			/* connect previous to next */
			l[j].next = l[k].next;
		    }
		    freecallout(k);
		    return TRUE;
		}
		j = k;
	    } while ((k=l[j].next) != 0);
	}
    }
    return FALSE;
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	remove a callout
 */
Int co_del(obj, handle)
object *obj;
register unsigned int handle;
{
    register uindex i;
    register call_out *l;
    Uint t;
    int nargs;

    /*
     * get the callout
     */
    if (d_get_call_out(obj->data, handle, &t, (frame *) NULL, &nargs) ==
							    (string *) NULL) {
	/* no such callout */
	return -1;
    }

    i = obj->index;
    if (t == 0) {
	/*
	 * immediate callout
	 */
	rmshort(&c0first, i, handle) || rmshort(&c0next, i, handle);
	--nzero;
	return 0;
    }

    t += timediff;
    if (t < timestamp + CYCBUF_SIZE) {
	/*
	 * try to find the callout in the cyclic buffer
	 */
	if (rmshort(&cycbuf[t & CYCBUF_MASK], i, handle)) {
	    return t - timeout;
	}
    }

    /*
     * Not found in the cyclic buffer; it <must> be in the queue.
     */
    l = cotab;
    for (;;) {
	if (l->oindex == i && l->handle == handle) {
	    dequeue(l - cotab);
	    return t - timeout;
	}
	l++;
# ifdef DEBUG
	if (l == cotab + queuebrk) {
	    fatal("failed to remove callout");
	}
# endif
    }
}

/*
 * NAME:	call_out->list()
 * DESCRIPTION:	return an array with the callouts of an object
 */
array *co_list(data, obj)
dataspace *data;
object *obj;
{
    return d_list_callouts(data, o_dataspace(obj), timeout - timediff);
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
    Uint t;
    int nargs;

    if (c0first.list == 0) {
	c0first = c0next;
	c0next.list = 0;
    }
    if (c0first.list != 0) {
	/*
	 * immediate callouts
	 */
	while (ec_push((ec_ftn) errhandler)) {
	    endthread();
	}
	while ((i=c0first.list) != 0) {
	    c0first.list = cotab[i].next;
	    handle = cotab[i].handle;
	    obj = &otable[cotab[i].oindex];
	    freecallout(i);
	    --nzero;

	    str = d_get_call_out(o_dataspace(obj), handle, &t, f, &nargs);
	    if (i_call(f, obj, str->text, str->len, TRUE, nargs)) {
		/* function exists */
		i_del_value(f->sp++);
		str_del((f->sp++)->u.string);
	    } else {
		/* function doesn't exist */
		str_del((f->sp++)->u.string);
	    }
	    endthread();
	}
	ec_pop();
    }

    if (P_timeout()) {
	/*
	 * delayed callouts
	 */
	if ((t=P_time()) <= timeout) {
	    return;
	}
	timeout = t;
	while (ec_push((ec_ftn) errhandler)) {
	    endthread();
	}
	for (;;) {
	    if (queuebrk > 0 && cotab[0].time <= timeout) {
		/*
		 * queued callout
		 */
		handle = cotab[0].handle;
		obj = &otable[cotab[0].oindex];
		dequeue(0);
	    } else if (timestamp <= timeout &&
		(i=cycbuf[timestamp & CYCBUF_MASK].list) != 0) {
		/*
		 * next from cyclic buffer list
		 */
		cycbuf[timestamp & CYCBUF_MASK].list = cotab[i].next;
		handle = cotab[i].handle;
		obj = &otable[cotab[i].oindex];
		freecallout(i);
	    } else if (timestamp < timeout) {
		/*
		 * check next list
		 */
		timestamp++;
		continue;
	    } else {
		/*
		 * no more callouts to do:
		 * set the alarm for the next round
		 */
		if (fragment > 0) {
		    uindex swaps1;

		    /*
		     * swap out a fragment of all control and data blocks
		     */
		    swaps1 = d_swapout(fragment);
		    swaprate1 += swaps1 - swapped1[swapidx1];
		    swapped1[swapidx1++] = swaps1;
		    if (swapidx1 == SWPERIOD) {
			swapidx1 = 0;
		    }
		    swaps5 += swaps1;
		    swaprate5 += swaps1;
		    if (--incr5 == 0) {
			swaprate5 -= swapped5[swapidx5];
			swapped5[swapidx5++] = swaps5;
			if (swapidx5 == SWPERIOD) {
			    swapidx5 = 0;
			}
			swaps5 = 0;
			incr5 = 5;
		    }
		}

		/* allow as much time for non-callouts as for callouts */
		t = P_time();
		P_alarm((t <= timeout || !comm_active()) ? 1 : t - timeout);
		break;
	    }

	    str = d_get_call_out(o_dataspace(obj), handle, &t, f, &nargs);
	    if (i_call(f, obj, str->text, str->len, TRUE, nargs)) {
		/* function exists */
		i_del_value(f->sp++);
		str_del((f->sp++)->u.string);
	    } else {
		/* function doesn't exist */
		str_del((f->sp++)->u.string);
	    }
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
 * NAME:	call_out->ready()
 * DESCRIPTION:	return TRUE if callouts are ready to run
 */
bool co_ready()
{
    return (nzero != 0);
}

/*
 * NAME:	call_out->swaprate1
 * DESCRIPTION:	return the number of objects swapped out per minute
 */
long co_swaprate1()
{
    return swaprate1;
}

/*
 * NAME:	call_out->swaprate5
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

/*
 * NAME:	call_out->dump
 * DESCRIPTION:	dump callout table
 */
bool co_dump(fd)
int fd;
{
    dump_header dh;
    register uindex list, last;
    register cbuf *cb;
    int ret;

    /* fill in header */
    dh.cotabsz = cotabsz;
    dh.queuebrk = queuebrk;
    dh.cycbrk = cycbrk;
    dh.flist = flist;
    dh.nshort = nshort;
    dh.nlong0 = queuebrk + nzero;
    dh.timestamp = timestamp;
    dh.timediff = timediff;

    /* deal with immediate callouts */
    if (nzero != 0) {
	if (c0first.list != 0) {
	    list = c0first.list;
	    if (c0next.list != 0) {
		cotab[c0first.last].next = c0next.list;
		last = c0next.last;
	    } else {
		last = c0first.last;
	    }
	} else {
	    list = c0next.list;
	    last = c0next.last;
	}
	cb = &cycbuf[timestamp & CYCBUF_MASK];
	cotab[last].next = cb->list;
	cb->list = list;
    }

    /* write header and callouts */
    ret = (P_write(fd, (char *) &dh, sizeof(dump_header)) > 0 &&
	   (queuebrk == 0 ||
	    P_write(fd, (char *) cotab, queuebrk * sizeof(call_out)) > 0) &&
	   (cycbrk == cotabsz ||
	    P_write(fd, (char *) (cotab + cycbrk),
		    (cotabsz - cycbrk) * sizeof(call_out)) > 0) &&
	   P_write(fd, (char *) cycbuf, CYCBUF_SIZE * sizeof(cbuf)) > 0);

    /* fix up immediate callouts */
    if (nzero != 0) {
	cb->list = cotab[last].next;
	if (c0first.list != 0) {
	    cotab[c0first.last].next = 0;
	}
	if (c0next.list != 0) {
	    cotab[c0next.last].next = 0;
	}
    }

    return ret;
}

/*
 * NAME:	call_out->restore
 * DESCRIPTION:	restore callout table
 */
void co_restore(fd, t)
int fd;
register Uint t;
{
    register uindex i, offset, last;
    register call_out *co;
    register cbuf *cb;
    dump_header dh;
    cbuf buffer[CYCBUF_SIZE];

    /* read and check header */
    conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);
    queuebrk = dh.queuebrk;
    offset = cotabsz - dh.cotabsz;
    cycbrk = dh.cycbrk + offset;
    if (queuebrk > cycbrk + offset || cycbrk == 0) {
	error("Restored too many callouts");
    }

    /* read tables */
    conf_dread(fd, (char *) cotab, co_layout, (Uint) queuebrk);
    conf_dread(fd, (char *) (cotab + cycbrk), co_layout,
	       (Uint) cotabsz - cycbrk);
    conf_dread(fd, (char *) buffer, cb_layout, (Uint) CYCBUF_SIZE);

    flist = dh.flist;
    nshort = dh.nshort;
    nzero = dh.nlong0 - dh.queuebrk;
    timestamp = t;
    t -= dh.timestamp;
    timediff = dh.timediff + t;

    /* patch callouts in queue */
    for (i = queuebrk, co = cotab; i > 0; --i, co++) {
	co->time += t;
    }

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
	c0next.list = cb->list;
	for (i = nzero - 1, last = cb->list; i != 0; --i) {
	    last = cotab[last].next;
	}
	cb->list = cotab[last].next;
	cotab[last].next = 0;
    }
}
