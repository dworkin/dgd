# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"
# include "comm.h"

# define CYCBUF_SIZE	128		/* cyclic buffer size, power of 2 */
# define CYCBUF_MASK	(CYCBUF_SIZE - 1) /* cyclic buffer mask */
# define SWPERIOD	60		/* swaprate buffer size */

typedef struct {
    uindex handle;	/* callout handle */
    uindex oindex;	/* index in object table */
    Uint objcnt;	/* object count */
    Uint time;		/* when to call */
} call_out;

# define prev		oindex
# define next		time

typedef struct {
    uindex list;	/* list */
    uindex last;	/* last in list */
} cbuf;

static call_out *cotab;			/* callout table */
static uindex cotabsz;			/* callout table size */
static uindex queuebrk;			/* queue brk */
static uindex cycbrk;			/* cyclic buffer brk */
static uindex flist;			/* free list index */
static uindex nshort;			/* # short-term callouts */
static uindex nlong;			/* # long-term callouts */
static cbuf cycbuf[CYCBUF_SIZE];	/* cyclic buffer of callout lists */
static Uint timestamp;			/* cycbuf start time */
static Uint timeout;			/* time the last alarm came */
static int fragment;			/* swap fragment */
static Uint swaprate1;			/* swaprate per minute */
static Uint swaprate5;			/* swaprate per 5 minutes */

/*
 * NAME:	call_out->init()
 * DESCRIPTION:	initialize callout handling
 */
void co_init(max, frag)
uindex max;
int frag;
{
    if (max != 0) {
	cotab = ALLOC(call_out, max + 1);
	cotab[0].time = 0;	/* sentinel for the heap */
	cotab++;
	flist = 0;
	/* only if callout are enabled */
	timeout = timestamp = P_time();
	P_alarm(1);
    }
    cycbrk = cotabsz = max;
    queuebrk = 0;

    fragment = frag;
}

/*
 * NAME:	enqueue()
 * DESCRIPTION:	put a callout in the queue
 */
static uindex enqueue(t)
register Uint t;
{
    register unsigned int i, j;
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
    nlong++;
    /* return index of free spot */
    return i - 1;
}

/*
 * NAME:	dequeue()
 * DESCRIPTION:	remove a callout from the queue
 */
static void dequeue(i)
register unsigned int i;
{
    register Uint t;
    register unsigned int j;
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
    --nlong;
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
uindex co_new(obj, str, delay, nargs)
object *obj;
string *str;
Int delay;
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

    if (delay <= 0) {
	delay = 1;
    }
    t = timeout + delay;
    if (t < timeout) {
	error("Too long delay");
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

    co->handle = d_new_call_out(o_dataspace(obj), str, t, nargs);
    co->oindex = obj->index;
    co->objcnt = obj->count;

    return co->handle;
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	remove a callout
 */
Int co_del(obj, handle)
register object *obj;
register uindex handle;
{
    register uindex j, k;
    register call_out *l;
    register cbuf *cyc;
    Uint t;
    int nargs;

    /*
     * get the callout
     */
    if (d_get_call_out(o_dataspace(obj), handle, &t, &nargs) == (char *) NULL) {
	/* no such callout */
	return -1;
    }
    if (nargs > 0) {
	i_pop(nargs);
    }
    if (obj->count == 0) {
	/* destructed object */
	return t - timeout;
    }

    l = cotab;
    if (t < timestamp + CYCBUF_SIZE) {
	/*
	 * Try to find the callout in the cyclic buffer
	 */
	cyc = &cycbuf[t & CYCBUF_MASK];
	if (cyc->list != 0) {
	    /*
	     * this time-slot is in use
	     */
	    k = cyc->list;
	    if (l[k].objcnt == obj->count && l[k].handle == handle) {
		/* first element in list */
		cyc->list = l[k].next;
		freecallout(k);
		return t - timeout;
	    }
	    if (cyc->list != cyc->last) {
		/*
		 * list contains more than 1 element
		 */
		j = cyc->list;
		k = l[j].next;
		do {
		    if (l[k].objcnt == obj->count && l[k].handle == handle) {
			/* found it */
			if (k == cyc->last) {
			    /* last element of the list */
			    l[cyc->last = j].next = 0;
			} else {
			    /* connect previous to next */
			    l[j].next = l[k].next;
			}
			freecallout(k);
			return t - timeout;
		    }
		    j = k;
		} while ((k=l[j].next) != 0);
	    }
	}
    }

    /*
     * Not found in the cyclic buffer; it <must> be in the queue.
     */
    for (;;) {
	if (l->objcnt == obj->count && l->handle == handle) {
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
array *co_list(obj)
object *obj;
{
    return d_list_callouts(o_dataspace(obj), timeout);
}

/*
 * NAME:	call_out->call()
 * DESCRIPTION:	call expired callouts
 */
void co_call()
{
    register uindex i, handle;
    object *obj;
    char *func;
    Uint t;
    int nargs;

    if (P_timeout()) {
	timeout = P_time();
	for (;;) {
	    if (queuebrk > 0 && cotab[0].time <= timeout) {
		/*
		 * queued callout
		 */
		handle = cotab[0].handle;
		obj = o_object(cotab[0].oindex, cotab[0].objcnt);
		dequeue(0);
	    } else if (timestamp <= timeout &&
		(i=cycbuf[timestamp & CYCBUF_MASK].list) != 0) {
		/*
		 * next from cyclic buffer list
		 */
		cycbuf[timestamp & CYCBUF_MASK].list = cotab[i].next;
		handle = cotab[i].handle;
		obj = o_object(cotab[i].oindex, cotab[i].objcnt);
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
		comm_flush(FALSE);


		if (fragment > 0) {
		    static uindex swapped1[SWPERIOD], swapped5[SWPERIOD];
		    static int swapcnt1, swapcnt5;
		    uindex swaps1;
		    static uindex swaps5;
		    static int incr5 = 5;

		    /*
		     * swap out a fragment of all control and data blocks
		     */
		    swaps1 = d_swapout(fragment);
		    swaprate1 += swaps1;
		    swaprate1 -= swapped1[swapcnt1];
		    swapped1[swapcnt1++] = swaps1;
		    if (swapcnt1 == SWPERIOD) {
			swapcnt1 = 0;
		    }
		    swaps5 += swaps1;
		    if (--incr5 == 0) {
			swaprate5 += swaps5;
			swaprate5 -= swapped5[swapcnt5];
			swapped5[swapcnt5++] = swaps5;
			if (swapcnt5 == SWPERIOD) {
			    swapcnt5 = 0;
			}
			swaps5 = 0;
			incr5 = 5;
		    }
		}

		P_alarm(1);
		return;
	    }

	    if (obj != (object *) NULL) {
		/* object exists */
		func = d_get_call_out(o_dataspace(obj), handle, &t, &nargs);
		if (i_call(obj, func, TRUE, nargs)) {
		    i_del_value(sp++);
		}
	    }
	}
    }
}

/*
 * NAME:	call_out->info
 * DESCRIPTION:	give information about callouts
 */
void co_info(n1, n2)
uindex *n1, *n2;
{
    *n1 = nshort;
    *n2 = nlong;
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
    uindex nlong;		/* # of long-term callouts */
    Uint timestamp;		/* time the last alarm came */
} dump_header;

/*
 * NAME:	call_out->dump
 * DESCRIPTION:	dump callout table
 */
bool co_dump(fd)
int fd;
{
    dump_header dh;

    /* fill in header */
    dh.cotabsz = cotabsz;
    dh.queuebrk = queuebrk;
    dh.cycbrk = cycbrk;
    dh.flist = flist;
    dh.nshort = nshort;
    dh.nlong = nlong;
    dh.timestamp = timestamp;

    /* write header and callouts */
    return (write(fd, &dh, sizeof(dump_header)) >= 0 &&
	    (queuebrk == 0 ||
	     write(fd, cotab, queuebrk * sizeof(call_out)) >= 0) &&
	    (cycbrk == cotabsz ||
	     write(fd, cotab + cycbrk,
		   (cotabsz - cycbrk) * sizeof(call_out)) >= 0) &&
	    write(fd, cycbuf, CYCBUF_SIZE * sizeof(cbuf)) >= 0);
}

/*
 * NAME:	call_out->restore
 * DESCRIPTION:	restore callout table
 */
void co_restore(fd, t)
int fd;
register long t;
{
    register uindex i, offset;
    register call_out *co;
    register cbuf *cb;
    dump_header dh;
    cbuf buffer[CYCBUF_SIZE];

    /* read and check */
    if (read(fd, &dh, sizeof(dump_header)) != sizeof(dump_header) ||
	(queuebrk=dh.queuebrk) >
			(cycbrk=dh.cycbrk + (offset=cotabsz - dh.cotabsz)) ||
	cycbrk == 0 ||
	(queuebrk != 0 && read(fd, cotab, queuebrk * sizeof(call_out)) !=
						queuebrk * sizeof(call_out)) ||
	(cycbrk != cotabsz &&
	 read(fd, cotab + cycbrk, (cotabsz - cycbrk) * sizeof(call_out)) !=
				    (cotabsz - cycbrk) * sizeof(call_out)) ||
	read(fd, buffer, CYCBUF_SIZE * sizeof(cbuf)) !=
						CYCBUF_SIZE * sizeof(cbuf)) {
	fatal("cannot restore callouts");
    }
    flist = dh.flist;
    nshort = dh.nshort;
    nlong = dh.nlong;

    /* patch callouts in queue */
    for (i = queuebrk, co = cotab; i > 0; --i, co++) {
	co->time += t;
    }

    /* cycle around cyclic buffer */
    timestamp = dh.timestamp + t;
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
}
