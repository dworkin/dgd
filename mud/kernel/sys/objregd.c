# include <kernel/kernel.h>
# include <kernel/objreg.h>

mapping links;		/* owner : first object */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize global vars
 */
static create()
{
    links = ([ "System" : this_object() ]);
    _F_prev(this_object());
    _F_next(this_object());
}

/*
 * NAME:	link()
 * DESCRIPTION:	link in a new object in per-owner linked list
 */
link(object obj, string owner)
{
    if (previous_program() == AUTO) {
	rlimits (-1; -1) {
	    object link, next;

	    link = links[owner];
	    if (!link) {
		/* first object for this owner */
		links[owner] = obj;
		obj->_F_prev(obj);
		obj->_F_next(obj);
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
}

/*
 * NAME:	unlink()
 * DESCRIPTION:	remove object from per-owner linked list
 */
unlink(object obj, string owner)
{
    if (previous_program() == AUTO) {
	rlimits (-1; -1) {
	    object prev, next;

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
}

/*
 * NAME:	query_link()
 * DESCRIPTION:	query first object in linked list
 */
object query_link(string owner)
{
    if (SYSTEM()) {
	return links[owner];
    }
}
