# define INCLUDE_TIME
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"

# define CIRCBUF_SIZE	64		/* circular buffer size, power of 2 */
# define CIRCBUF_MASK	(CIRCBUF_SIZE - 1) /* circular buffer mask */
# define SWPERIOD	60		/* swaprate buffer size */

typedef struct {
    uindex handle;	/* call_out handle */
    uindex oindex;	/* index in object table */
    Int objcnt;		/* object count */
    union {
	time_t time;	/* when to call */
	uindex next;	/* index of next in list */
    } u;
} call_out;

typedef struct {
    uindex list;	/* list */
    uindex last;	/* last in list */
} cbuf;

static call_out *cotab;			/* call_out table */
static uindex cotabsz;			/* call_out table size */
static uindex queuebrk;			/* queue brk */
static uindex circbrk;			/* circular buffer brk */
static uindex flist;			/* free list index */
static cbuf circbuf[CIRCBUF_SIZE];	/* circular buffer of call_out lists */
static time_t timestamp;		/* circbuf start time */
static time_t timeout;			/* time the alarm came */
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
	cotab[0].u.time = 0;	/* sentinel for the heap */
	cotab++;
	flist = 0;
	/* only if call_outs are enabled */
	timestamp = _time();
	_alarm(1);
    }
    circbrk = cotabsz = max;
    queuebrk = 0;

    fragment = frag;
}

/*
 * NAME:	enqueue()
 * DESCRIPTION:	put a call_out in the queue
 */
static uindex enqueue(t)
register time_t t;
{
    register unsigned int i, j;
    register call_out *l;

    if (queuebrk == circbrk) {
	error("Too many call_outs");
    }

    /*
     * create a free spot in the heap, and sift it upward
     */
    i = ++queuebrk;
    l = cotab - 1;
    for (j = i >> 1; l[j].u.time > t; i = j, j >>= 1) {
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
    register time_t t;
    register unsigned int j;
    register call_out *l;

    l = cotab - 1;
    i++;
    t = l[queuebrk].u.time;
    if (t < l[i].u.time) {
	/* sift upward */
	for (j = i >> 1; l[j].u.time > t; i = j, j >>= 1) {
	    l[i] = l[j];
	}
    } else {
	/* sift downward */
	for (j = i << 1; j < queuebrk; i = j, j <<= 1) {
	    if (l[j].u.time > l[j + 1].u.time) {
		j++;
	    }
	    if (t <= l[j].u.time) {
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
 * DESCRIPTION:	get a new call_out for the circular buffer
 */
static uindex newcallout(t)
time_t t;
{
    register uindex i;

    if (flist != 0) {
	/* get call_out from free list */
	i = flist;
	flist = cotab[i].u.next;
    } else {
	/* allocate new call_out */
	if (circbrk == queuebrk || circbrk == 1) {
	    error("Too many call_outs");
	}
	i = --circbrk;
    }

    return i;
}

/*
 * NAME:	freecallout()
 * DESCRIPTION:	remove a call_out from the circular buffer
 */
static void freecallout(i)
register uindex i;
{
    register call_out *l;

    l = &cotab[i];
    l->handle = 0;	/* mark as unused */
    if (i == circbrk) {
	/*
	 * call_out at the edge
	 */
	while (++circbrk != cotabsz && (++l)->handle == 0) {
	    /* followed by free call_out */
	    if (circbrk == flist) {
		/* first in the free list */
		flist = l->u.next;
	    } else {
		/* connect previous to next */
		cotab[l->oindex].u.next = l->u.next;
		if (l->u.next != 0) {
		    /* connect next to previous */
		    cotab[l->u.next].oindex = l->oindex;
		}
	    }
	}
    } else {
	/* add to free list */
	if (flist != 0) {
	    /* link next to current */
	    cotab[flist].oindex = i;
	}
	/* link to next */
	l->u.next = flist;
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
    time_t t;
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
    t = _time() + delay;

    if (t < timestamp + CIRCBUF_SIZE) {
	register cbuf *circ;

	/* add to circular buffer */
	i = newcallout(t);
	circ = &circbuf[t & CIRCBUF_MASK];
	if (circ->list == 0) {
	    /* first one in list */
	    circ->list = i;
	} else {
	    /* add to list */
	    cotab[circ->last].u.next = i;
	}
	co = &cotab[circ->last = i];
	co->u.next = 0;
    } else {
	/* put in queue */
	i = enqueue(t);
	co = &cotab[i];
	co->u.time = t;
    }

    co->handle = d_new_call_out(o_dataspace(obj), str, (unsigned long) t, 
				nargs);
    co->oindex = obj->index;
    co->objcnt = obj->count;

    return TRUE;
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	remove a call_out (costly!)
 */
long co_del(obj, str)
object *obj;
string *str;
{
    register uindex handle, i, j, k;
    register call_out *l;
    register cbuf *circ;
    time_t t, t2;
    int nargs;

    /* d_get_call_out will need space */
    i_check_stack(MAX_LOCALS - 2);

    /*
     * try to find the call_out
     */
    handle = d_find_call_out(o_dataspace(obj), str);
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

    /*
     * Try to find the call_out in the circular buffer
     */
    l = cotab;
    for (i = 0; i < CIRCBUF_SIZE; i++) {
	circ = &circbuf[(timestamp + i) & CIRCBUF_MASK];
	if (circ->list != 0) {
	    /*
	     * this time-slot is in use
	     */
	    k = circ->list;
	    if (l[k].objcnt == obj->count && l[k].handle == handle) {
		/* first element in list */
		circ->list = l[k].u.next;
		freecallout(k);
		t = timestamp + i;
		t2 = _time();
		return (t < t2) ? 0 : t - t2;
	    }
	    if (circ->list != circ->last) {
		/*
		 * list contains more than 1 element
		 */
		j = circ->list;
		k = l[j].u.next;
		do {
		    if (l[k].objcnt == obj->count && l[k].handle == handle) {
			/* found it */
			if (k == circ->last) {
			    /* last element of the list */
			    l[circ->last = j].u.next = 0;
			} else {
			    /* connect previous to next */
			    l[j].u.next = l[k].u.next;
			}
			freecallout(k);
			t = timestamp + i;
			t2 = _time();
			return (t < t2) ? 0 : t - t2;
		    }
		    j = k;
		} while ((k=l[j].u.next) != 0);
	    }
	}
    }

    /*
     * Not found in the circular buffer; it <must> be in the queue.
     */
    for (;;) {
	if (l->objcnt == obj->count && l->handle == handle) {
	    t = l->u.time;
	    dequeue(l - cotab);
	    t2 = _time();
	    return (t < t2) ? 0 : t - t2;
	}
	l++;
    }
}

/*
 * NAME:	call_out->timeout()
 * DESCRIPTION:	it is time for the next call_outs to be done
 */
void co_timeout()
{
    timeout = _time();
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
	for (;;) {
	    if (timestamp <= timeout &&
		(i=circbuf[timestamp & CIRCBUF_MASK].list) != 0) {
		/*
		 * next from circular buffer list
		 */
		circbuf[timestamp & CIRCBUF_MASK].list = cotab[i].u.next;
		handle = cotab[i].handle;
		obj = o_object(cotab[i].oindex, cotab[i].objcnt);
		freecallout(i);
	    } else if (timestamp < timeout) {
		/*
		 * check next list
		 */
		timestamp++;
		continue;
	    } else if (queuebrk > 0 && cotab[0].u.time <= timeout) {
		/*
		 * next queued call_out
		 */
		handle = cotab[0].handle;
		obj = o_object(cotab[0].oindex, cotab[0].objcnt);
		dequeue(0);
	    } else {
		/*
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

		_alarm(1);
		return;
	    }

	    if (obj == (object *) NULL) {
		/* object destructed */
		continue;
	    }

	    func = d_get_call_out(o_dataspace(obj), handle, &nargs);
	    if (i_call(obj, func, TRUE, nargs)) {
		i_del_value(sp++);
	    }
	}
    }
}

/*
 * NAME:	co_count
 * DESCRIPTION:	return the number of call_outs
 */
uindex co_count()
{
    return queuebrk + cotabsz - circbrk;
}

/*
 * NAME:	co_swaprate
 * DESCRIPTION:	return the number of objects swapped out per minute
 */
long co_swaprate()
{
    return swaprate;
}
