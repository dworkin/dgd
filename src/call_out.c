# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"

# define CYCBUF_SIZE	128		/* cyclic buffer size, power of 2 */
# define CYCBUF_MASK	(CYCBUF_SIZE - 1) /* cyclic buffer mask */
# define SWPERIOD	60		/* swaprate buffer size */

typedef struct {
    uindex handle;	/* call_out handle */
    uindex oindex;	/* index in object table */
    Int objcnt;		/* object count */
    unsigned long time;	/* when to call */
} call_out;

# define prev		oindex
# define next		time

typedef struct {
    uindex list;	/* list */
    uindex last;	/* last in list */
} cbuf;

static call_out *cotab;			/* call_out table */
static uindex cotabsz;			/* call_out table size */
static uindex queuebrk;			/* queue brk */
static uindex cycbrk;			/* cyclic buffer brk */
static uindex flist;			/* free list index */
static uindex ncallouts;		/* # call_outs */
static cbuf cycbuf[CYCBUF_SIZE];	/* cyclic buffer of call_out lists */
static unsigned long timestamp;		/* cycbuf start time */
static unsigned long timeout;		/* time the alarm came */
static unsigned long timestart;		/* time the prev alarm came */
static int fragment;			/* swap fragment */
static uindex swapped[SWPERIOD];	/* swapped objects per second */
static int swapcnt;			/* index in swapped buffer */
static unsigned long swaprate;		/* swaprate */

/*
 * NAME:	call_out->init()
 * DESCRIPTION:	initialize call_out handling
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
	/* only if call_outs are enabled */
	timestart = timestamp = P_time();
	P_alarm(1);
    }
    cycbrk = cotabsz = max;
    queuebrk = 0;

    fragment = frag;
}

/*
 * NAME:	enqueue()
 * DESCRIPTION:	put a call_out in the queue
 */
static uindex enqueue(t)
register unsigned long t;
{
    register unsigned int i, j;
    register call_out *l;

    if (queuebrk == cycbrk) {
	error("Too many call_outs");
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
 * DESCRIPTION:	remove a call_out from the queue
 */
static void dequeue(i)
register unsigned int i;
{
    register unsigned long t;
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
}

/*
 * NAME:	newcallout()
 * DESCRIPTION:	get a new call_out for the cyclic buffer
 */
static uindex newcallout()
{
    register uindex i;

    if (flist != 0) {
	/* get call_out from free list */
	i = flist;
	flist = cotab[i].next;
    } else {
	/* allocate new call_out */
	if (cycbrk == queuebrk || cycbrk == 1) {
	    error("Too many call_outs");
	}
	i = --cycbrk;
    }

    return i;
}

/*
 * NAME:	freecallout()
 * DESCRIPTION:	remove a call_out from the cyclic buffer
 */
static void freecallout(i)
register uindex i;
{
    register call_out *l;

    l = &cotab[i];
    l->handle = 0;	/* mark as unused */
    if (i == cycbrk) {
	/*
	 * call_out at the edge
	 */
	while (++cycbrk != cotabsz && (++l)->handle == 0) {
	    /* followed by free call_out */
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
 * NAME:	call_out->new()
 * DESCRIPTION:	add a new call_out
 */
bool co_new(obj, str, delay, nargs)
object *obj;
string *str;
long delay;
int nargs;
{
    unsigned long t;
    uindex i;
    register call_out *co;

    if (cotabsz == 0) {
	/*
	 * Call_outs are disabled.  Return immediately.
	 */
	return FALSE;
    }

    if (delay <= 0) {
	delay = 1;
    }
    t = timestart + delay;

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
    ncallouts++;

    return TRUE;
}

/*
 * NAME:	call_out->find()
 * DESCRIPTION:	find a call_out
 */
long co_find(obj, str)
object *obj;
string *str;
{
    unsigned long t;

    /*
     * try to find the call_out
     */
    if (d_find_call_out(o_dataspace(obj), str, &t) == 0) {
	/* call_out didn't exist */
	return -1;
    }

    return t - timestart;
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	remove a call_out (can be expensive)
 */
long co_del(obj, str)
object *obj;
string *str;
{
    register uindex handle, j, k;
    register call_out *l;
    register cbuf *cyc;
    unsigned long t;
    int nargs;

    /*
     * try to find the call_out
     */
    handle = d_find_call_out(o_dataspace(obj), str, &t);
    if (handle == 0) {
	/* call_out didn't exist */
	return -1;
    }

    /*
     * get the call_out
     */
    d_get_call_out(obj->data, handle, &nargs);
    if (nargs > 0) {
	i_pop(nargs);
    }

    --ncallouts;
    l = cotab;
    if (t < timestamp + CYCBUF_SIZE) {
	/*
	 * Try to find the call_out in the cyclic buffer
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
		return t - timestart;
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
			return t - timestart;
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
	    return t - timestart;
	}
	l++;
# ifdef DEBUG
	if (l == cotab + queuebrk) {
	    fatal("failed to remove call_out");
	}
# endif
    }
}

/*
 * NAME:	call_out->timeout()
 * DESCRIPTION:	it is time for the next call_outs to be done
 */
void co_timeout()
{
    timeout = P_time();
}

/*
 * NAME:	call_out->call()
 * DESCRIPTION:	call expired call_outs
 */
void co_call()
{
    register uindex i, handle;
    object *obj;
    char *func;
    int nargs;

    if (timeout != 0) {
	timestart = timeout;
	for (;;) {
	    if (timestamp <= timeout &&
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
	    } else if (queuebrk > 0 && cotab[0].time <= timeout) {
		/*
		 * next queued call_out
		 */
		handle = cotab[0].handle;
		obj = o_object(cotab[0].oindex, cotab[0].objcnt);
		dequeue(0);
	    } else {
		/*
		 * no more call_outs to do:
		 * set the alarm for the next round
		 */
		timeout = 0;
		comm_flush(FALSE);


		if (fragment > 0) {
		    uindex swaps;

		    /*
		     * swap out a fragment of all control and data blocks
		     */
		    swaps = d_swapout(fragment);
		    swaprate = swaprate - swapped[swapcnt] + swaps;
		    swapped[swapcnt++] = swaps;
		    if (swapcnt == SWPERIOD) {
			swapcnt = 0;
		    }
		}

		P_alarm(1);
		return;
	    }
	    --ncallouts;

	    if (obj != (object *) NULL) {
		/* object exists */
		func = d_get_call_out(o_dataspace(obj), handle, &nargs);
		if (i_call(obj, func, TRUE, nargs)) {
		    i_del_value(sp++);
		}
	    }
	}
    }
}

/*
 * NAME:	call_out->count
 * DESCRIPTION:	return the number of call_outs
 */
uindex co_count()
{
    return ncallouts;
}

/*
 * NAME:	call_out->swaprate
 * DESCRIPTION:	return the number of objects swapped out per minute
 */
long co_swaprate()
{
    return swaprate;
}


typedef struct {
    uindex cotabsz;		/* call_out table size */
    uindex queuebrk;		/* queue brk */
    uindex cycbrk;		/* cyclic buffer brk */
    uindex flist;		/* free list index */
    uindex ncallouts;		/* # of call_outs */
    unsigned long timestamp;	/* time the last alarm came */
} dump_header;

/*
 * NAME:	call_out->dump
 * DESCRIPTION:	dump call_out table
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
    dh.ncallouts = ncallouts;
    dh.timestamp = timestamp;

    /* write header and call_outs */
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
 * DESCRIPTION:	restore call_out table
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
    ncallouts = dh.ncallouts;

    /* patch call_outs in queue */
    for (i = queuebrk, co = cotab; i > 0; --i, co++) {
	co->time += t;
    }

    /* cycle around cyclic buffer */
    timestamp = dh.timestamp + t;
    t &= CYCBUF_MASK;
    memcpy(cycbuf + t, buffer, (unsigned int) (CYCBUF_SIZE - t) * sizeof(cbuf));
    memcpy(cycbuf, buffer + CYCBUF_SIZE - t, (unsigned int) t * sizeof(cbuf));

    if (offset != 0) {
	/* patch call_out references */
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
