# include <kernel/kernel.h>
# include <kernel/rsrc.h>
# include <type.h>

mapping resources;		/* registered resources */
mapping owners;			/* resource owners */
int downtime;			/* shutdown time */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize resource mappings
 */
static create()
{
    /* initial resources */
    resources = ([
      "objects" :	({   0, -1,  0,    0 }),
      "events" :	({   0, -1,  0,    0 }),
      "callouts" :	({   0, -1,  0,    0 }),
      "timers" :	({   0, -1,  0,    0 }),
      "stack" :		({   0, -1,  0,    0 }),
      "ticks" :		({   0, -1,  0,    0 }),
      "tick usage" :	({ 0.0, -1, 10, 3600 }),
      "filequota" :	({   0, -1,  0,    0 }),
      "editors" :	({   0, -1,  0,    0 }),
      "create stack" :	({   0, -1,  0,    0 }),
      "create ticks" :	({   0, -1,  0,    0 }),
    ]);

    owners = ([ ]);		/* no resource owners yet */
}

/*
 * NAME:	add_owner()
 * DESCRIPTION:	add a new resource owner
 */
add_owner(string owner)
{
    if (KERNEL() && !owners[owner]) {
	object obj;

	rlimits (-1; -1) {
	    obj = clone_object(RSRCOBJ);
	    catch {
		owners[owner] = obj;
		obj->set_owner(owner);
		owners["System"]->rsrc_incr("objects", 0, 1,
					    resources["objects"], 1);
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
 * NAME:	remove_owner()
 * DESCRIPTION:	remove a resource owner
 */
remove_owner(string owner)
{
    object obj;
    string *names;
    mixed **rsrcs, *rsrc, *usage;
    int i, sz;

    if (previous_program() == API_RSRC && (obj=owners[owner])) {
	names = map_indices(resources);
	rsrcs = map_values(resources);
	usage = allocate(sz = sizeof(rsrcs));
	for (i = sz; --i >= 0; ) {
	    rsrc = obj->get_rsrc(names[i], rsrcs[i]);
	    if (rsrc[RSRC_DECAY] == 0 && rsrc[RSRC_USAGE] != 0) {
		error("Removing owner with non-zero resources");
	    }
	    usage[i] = rsrc[RSRC_USAGE];
	}

	rlimits (-1; -1) {
	    for (i = sz; --i >= 0; ) {
		rsrcs[i][RSRC_USAGE] -= usage[i];
	    }
	    destruct_object(obj);
	}
    }
}

/*
 * NAME:	query_owners()
 * DESCRIPTION:	return a list of resource owners
 */
string *query_owners()
{
    if (previous_program() == API_RSRC) {
	return map_indices(owners);
    }
}


/*
 * NAME:	set_rsrc()
 * DESCRIPTION:	set the maximum, decay percentage and decay period of a
 *		resource
 */
set_rsrc(string name, int max, int decay, int period)
{
    if (KERNEL()) {
	mixed *rsrc;

	rsrc = resources[name];
	if (rsrc != 0) {
	    /*
	     * existing resource
	     */
	    if ((rsrc[RSRC_DECAY - 1] == 0) != (decay == 0)) {
		error("Cannot change resource decay");
	    }
	    rlimits (-1; -1) {
		rsrc[RSRC_MAX] = max;
		rsrc[RSRC_DECAY - 1] = decay;
		rsrc[RSRC_PERIOD - 1] = period;
	    }
	} else {
	    /* new resource */
	    resources[name] = ({ (decay == 0) ? 0 : 0.0, max, decay, period });
	}
    }
}

/*
 * NAME:	remove_rsrc()
 * DESCRIPTION:	remove a resource
 */
remove_rsrc(string name)
{
    int *rsrc, i;
    object *objects;

    if (previous_program() == API_RSRC && (rsrc=resources[name])) {
	if (rsrc[RSRC_DECAY - 1] == 0 && rsrc[RSRC_USAGE] != 0) {
	    error("Removing non-zero resource");
	}

	objects = map_values(owners);
	i = sizeof(objects);
	rlimits (-1; -1) {
	    while (i != 0) {
		objects[--i]->remove_rsrc(name);
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
    mixed *rsrc;

    if (previous_program() == API_RSRC) {
	rsrc = resources[name];
	return rsrc[RSRC_USAGE .. RSRC_MAX] + ({ 0 }) +
	       rsrc[RSRC_DECAY - 1 .. RSRC_PERIOD - 1];
    }
}

/*
 * NAME:	query_resources()
 * DESCRIPTION:	return a list of resources
 */
string *query_resources()
{
    if (previous_program() == API_RSRC) {
	return map_indices(resources);
    }
}


/*
 * NAME:	rsrc_set_limit()
 * DESCRIPTION:	set individual resource limit
 */
rsrc_set_limit(string owner, string name, int max)
{
    if (previous_program() == API_RSRC) {
	owners[owner]->rsrc_set_limit(name, max,
				      resources[name][RSRC_DECAY - 1]);
    }
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
mixed *rsrc_get(string owner, string name)
{
    if (KERNEL()) {
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
 * NAME:	prepare_reboot()
 * DESCRIPTION:	prepare for a reboot
 */
prepare_reboot()
{
    if (previous_program() == DRIVER) {
	downtime = time();
    }
}

/*
 * NAME:	reboot()
 * DESCRIPTION:	recover from a reboot
 */
reboot()
{
    if (previous_program() == DRIVER) {
	object *objects;
	int i;

	downtime = time() - downtime;
	objects = map_values(owners);
	for (i = sizeof(objects); --i >= 0; ) {
	    objects[i]->reboot(downtime);
	}
    }
}
