# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/rsrc.h>

mapping links;		/* owner : first object */
mapping editors;	/* editor : TRUE */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize global vars
 */
static void create()
{
    links = ([ "System" : this_object() ]);
    editors = ([ ]);
    _F_prev(this_object());
    _F_next(this_object());
}

/*
 * NAME:	link()
 * DESCRIPTION:	link in a new object in per-owner linked list
 */
void link(object obj, string owner)
{
    if (previous_program() == AUTO) {
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

/*
 * NAME:	unlink()
 * DESCRIPTION:	remove object from per-owner linked list
 */
void unlink(object obj, string owner)
{
    if (previous_program() == AUTO) {
	object prev, next;

	prev = obj->_Q_prev();
	if (prev == obj) {
	    links[owner] = nil;	/* no more objects left */
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

/*
 * NAME:	add_editor()
 * DESCRIPTION:	register an editor object
 */
void add_editor(object obj)
{
    if (previous_program() == AUTO) {
	editors[obj] = TRUE;
    }
}

/*
 * NAME:	remove_editor()
 * DESCRIPTION:	unregister an editor object
 */
void remove_editor(object obj)
{
    if (previous_program() == AUTO) {
	editors[obj] = nil;
    }
}

/*
 * NAME:	remove_editors()
 * DESCRIPTION:	unregister all editor objects, and return them in an array
 */
object *remove_editors()
{
    if (previous_program() == RSRCD) {
	object *objects;

	objects = map_indices(editors);
	editors = ([ ]);
	return objects;
    }
}

/*
 * NAME:	first_link()
 * DESCRIPTION:	return first object in linked list
 */
object first_link(string owner)
{
    if (previous_program() == API_OBJREG) {
	return links[owner];
    }
}

/*
 * NAME:	prev_link()
 * DESCRIPTION:	return prev object in linked list
 */
object prev_link(object obj)
{
    if (previous_program() == API_OBJREG) {
	return obj->_Q_prev();
    }
}

/*
 * NAME:	next_link()
 * DESCRIPTION:	return next object in linked list
 */
object next_link(object obj)
{
    if (previous_program() == API_OBJREG) {
	return obj->_Q_next();
    }
}
