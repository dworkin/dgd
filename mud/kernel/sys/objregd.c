# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/rsrc.h>

mapping links;		/* owner : first object */
object rsrcd;		/* resource daemon */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize global vars
 */
static create()
{
    links = ([ ]);
    rsrcd = find_object(RSRCD);		/* must have been compiled before */
}

/*
 * NAME:	link()
 * DESCRIPTION:	link in a new object in per-owner linked list
 */
link(object obj, string owner)
{
    if (PRIV0()) {
	object link, next;

	rsrcd->rsrc_incr(owner, "objects", 0, 1, 1);	/* must succeed */
	link = links[owner];
	if (link == 0) {
	    /* first object for this owner */
	    links[owner] = obj;
	    obj->_F_next(obj);
	    obj->_F_prev(obj);
	} else {
	    /* add to list */
	    next = link->_Q_next();
	    link->_F_next(obj);
	    next->_F_prev(obj);
	    obj->_F_prev(link);
	    obj->_F_next(next);
	}
    }
}

/*
 * NAME:	unlink()
 * DESCRIPTION:	remove object from per-owner linked list
 */
unlink(object obj, string owner)
{
    if (PRIV0()) {
	object prev, next;

	if (sscanf(object_name(obj), "%*s#") != 0) {
	    /* clones only: others handled by driver->remove_program() */
	    rsrcd->rsrc_incr(owner, "objects", 0, -1);
	}
	prev = obj->_Q_prev();
	if (prev == obj) {
	    links[owner] = 0;	/* no more objects left */
	} else {
	    next = obj->_Q_next();
	    prev->_F_next(next);
	    next->_F_prev(prev);
	    if (obj == links[owner]) {
		links[owner] = next;	/* replace reference object */
	    }
	}
    }
}
