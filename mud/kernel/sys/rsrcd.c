# include <kernel/kernel.h>
# include <kernel/rsrc.h>
# include <type.h>

mapping resources;		/* registered resources */
mapping owners;			/* resource owners */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize resource mappings
 */
static create()
{
    /* initial resources */
    resources = ([
      "objects" :		({ 0, -1, 0, 0, 0 }),
      "events" :		({ 0, -1, 0, 0, 0 }),
      "callouts" :		({ 0, -1, 0, 0, 0 }),
      "stackdepth" :		({ 0, -1, 0, 0, 0 }),
      "ticks" :			({ 0, -1, 0, 0, 0 }),
      "tick usage" :		({ 0, -1, time(), 10, 3600 }),
      "filequota" :		({ 0, -1, 0, 0, 0 }),
      "editors" :		({ 0, -1, 0, 0, 0 }),

      /* reasonable starting values */
      "create depth" :		({ 0, 5, 0, 0, 0 }),
      "create ticks" :		({ 0, 5000, 0, 0, 0 }),
    ]);

    owners = ([ ]);		/* no resource owners yet */
}

/*
 * NAME:	set_rsrc()
 * DESCRIPTION:	set the maximum, decay percentage and decay period of a
 *		resource
 */
set_rsrc(string name, int max, int decay, int period)
{
    if (KERNEL()) {
	int time;
	mixed *rsrc;

	time = (decay != 0) ? time() : 0;
	rsrc = resources[name];
	if (rsrc != 0) {
	    /*
	     * existing resource
	     */
	    rlimits (-1; -1) {
		rsrc[RSRC_DECAYTIME] = time;
		rsrc[RSRC_MAX] = max;
		rsrc[RSRC_DECAY] = decay;
		rsrc[RSRC_PERIOD] = period;
	    }
	} else {
	    /* new resource */
	    resources[name] = ({ 0, max, time, decay, period });
	}
    }
}

/*
 * NAME:	del_rsrc()
 * DESCRIPTION:	delete a resource
 */
del_rsrc(string name)
{
    if (KERNEL()) {
	object *objlist;
	int i;

	objlist = map_values(owners);
	i = sizeof(objlist);
	rlimits (-1; -1) {
	    while (i != 0) {
		objlist[--i]->del_rsrc(name);
	    }
	    resources[name] = 0;
	}
    }
}

/*
 * NAME:	query_rsrc()
 * DESCRIPTION:	get usage and limits of a resource
 */
mixed *query_rsrc(string name)
{
    if (SYSTEM()) {
	return resources[name];
    }
}

/*
 * NAME:	query_rsrc_list()
 * DESCRIPTION:	get a list of resources
 */
string *query_rsrc_list()
{
    if (SYSTEM()) {
	return map_indices(resources);
    }
}


/*
 * NAME:	rsrc_set_limit()
 * DESCRIPTION:	set individual resource limit
 */
rsrc_set_limit(string owner, string name, int max)
{
    if (SYSTEM()) {
	owners[owner]->rsrc_set_limit(name, max);
    }
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
mixed *rsrc_get(string owner, string name)
{
    if (SYSTEM()) {
	return owners[owner]->rsrc_get(name, resources[name]);
    }
}

/*
 * NAME:	rsrc_incr()
 * DESCRIPTION:	increment or decrement a resource, returning 1 if succeeded,
 *		0 if failed
 */
varargs int rsrc_incr(string owner, string name, mixed index, int incr,
		      int force)
{
    if (KERNEL()) {
	return owners[owner]->rsrc_incr(name, index, incr, resources[name],
					force);
    }
}


/*
 * NAME:	add_owner()
 * DESCRIPTION:	add a (possibly already existing) resource owner
 */
add_owner(string owner)
{
    if (KERNEL() && !owners[owner]) {
	object obj;

	rlimits (-1; -1) {
	    obj = clone_object(RSRCOBJ);
	    catch {
		owners[owner] = obj;
		obj->set_owner("System");
		rsrc_incr("System", "objects", 0, 1);
	    } : {
		destruct_object(obj);
	    }
	}
	if (!obj) {
	    error("Too many resource owners");
	}
    }
}

/*
 * NAME:	query_owner_list()
 * DESCRIPTION:	get a list of resource owners
 */
string *query_owner_list()
{
    if (SYSTEM()) {
	return map_indices(owners);
    }
}
