# include <kernel/kernel.h>
# include <kernel/rsrc.h>
# include <type.h>

mapping resources;		/* registered resources */
mapping owners;			/* resource owners */
mixed *limits;			/* limits for current owner */
int downtime;			/* shutdown time */
mapping suspended;		/* suspended callouts */
mixed *first_suspended;		/* first suspended callout */
mixed *last_suspended;		/* last suspended callout */
object suspender;		/* object that suspended callouts */
int suspend;			/* releaser callout or -1 for suspending */

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
					    resources["objects"], TRUE);
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
	    rsrc = obj->rsrc_get(names[i], rsrcs[i]);
	    if (rsrc[RSRC_DECAY] == 0 && (int) rsrc[RSRC_USAGE] != 0) {
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
	if (rsrc) {
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
	if (rsrc[RSRC_DECAY - 1] == 0 && (int) rsrc[RSRC_USAGE] != 0) {
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
	object obj;

	if (!(obj=owners[owner])) {
	    error("No such resource owner");
	}
	obj->rsrc_set_limit(name, max, resources[name][RSRC_DECAY - 1]);
    }
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
mixed *rsrc_get(string owner, string name)
{
    if (KERNEL()) {
	object obj;

	if (!(obj=owners[owner])) {
	    error("No such resource owner");
	}
	return obj->rsrc_get(name, resources[name]);
    }
}

/*
 * NAME:	rsrc_incr()
 * DESCRIPTION:	increment or decrement a resource, returning TRUE if succeeded,
 *		FALSE if failed
 */
varargs int rsrc_incr(string owner, string name, mixed index, int incr,
		      int force)
{
    if (KERNEL()) {
	object obj;

	if (!(obj=owners[owner])) {
	    error("No such resource owner");
	}
	return obj->rsrc_incr(name, index, incr, resources[name], force);
    }
}

/*
 * NAME:	call_limits()
 * DESCRIPTION:	handle stack and tick limits for _F_call_limited
 */
mixed *call_limits(string owner, mixed *status)
{
    if (previous_program() == AUTO) {
	return limits = owners[owner]->call_limits(limits, status,
						   resources["stack"],
						   resources["ticks"],
						   resources["tick usage"]);
    }
}

/*
 * NAME:	update_ticks()
 * DESCRIPTION:	update ticks after execution
 */
int update_ticks(int ticks)
{
    if (KERNEL()) {
	if (limits[3] > 0 && (!limits[0] || limits[1] != limits[0][1])) {
	    owners[limits[1]]->update_ticks(ticks = limits[3] - ticks);
	    resources["tick usage"][RSRC_USAGE] += (float) ticks;
	    ticks = (limits[4] >= 0) ? limits[0][3] -= ticks : -1;
	}
	limits = limits[0];
	return ticks;
    }
}


/*
 * NAME:	suspend_callouts()
 * DESCRIPTION:	suspend all callouts
 */
suspend_callouts()
{
    if (SYSTEM() && suspend >= 0) {
	rlimits (-1; -1) {
	    if (!suspended) {
		suspended = ([ ]);
	    }
	    suspender = previous_object();
	    if (suspend != 0) {
		remove_call_out(suspend);
	    }
	    suspend = -1;
	}
    }
}

/*
 * NAME:	release_callouts()
 * DESCRIPTION:	release suspended callouts
 */
release_callouts()
{
    if (SYSTEM() && suspend < 0) {
	rlimits (-1; -1) {
	    suspender = 0;
	    if (first_suspended) {
		suspend = call_out("release", 0);
	    } else {
		suspended = 0;
		suspend = 0;
	    }
	}
    }
}


/*
 * NAME:	suspended()
 * DESCRIPTION:	return TRUE if callouts are suspended, otherwise return FALSE
 *		and decrease # of callouts by 1
 */
int suspended(object obj, string owner)
{
    if (previous_program() == AUTO) {
	if (suspend < 0 && obj != suspender) {
	    return TRUE;
	}
	owners[owner]->rsrc_incr("callouts", obj, -1, resources["callouts"]);
	return FALSE;
    }
}

/*
 * NAME:	suspend()
 * DESCRIPTION:	suspend a callout
 */
suspend(object obj, string owner, int handle)
{
    if (previous_program() == AUTO) {
	mixed *callout;

	callout = ({ obj, owner, handle, last_suspended, 0 });
	if (last_suspended) {
	    last_suspended[4] = callout;
	} else {
	    first_suspended = callout;
	}
	last_suspended = callout;
	if (suspended[obj]) {
	    suspended[obj][handle] = callout;
	} else {
	    suspended[obj] = ([ handle : callout ]);
	}
    }
}

/*
 * NAME:	remove_callout()
 * DESCRIPTION:	decrease amount of callouts, and possibly remove callout from
 *		list of suspended calls
 */
int remove_callout(object obj, string owner, int handle)
{
    if (previous_program() == AUTO && obj != this_object()) {
	mapping callouts;
	mixed *callout;

	owners[owner]->rsrc_incr("callouts", obj, -1, resources["callouts"]);

	if (suspended && (callouts=suspended[obj]) &&
	    (callout=callouts[handle])) {
	    if (callout != first_suspended) {
		callout[3][4] = callout[4];
	    } else {
		first_suspended = callout[4];
	    }
	    if (callout != last_suspended) {
		callout[4][3] = callout[3];
	    } else {
		last_suspended = callout[3];
	    }
	    callout[3] = callout[4] = 0;
	    callouts[handle] = 0;
	    return TRUE;	/* delayed call */
	}
    }
    return FALSE;
}

/*
 * NAME:	remove_callouts()
 * DESCRIPTION:	remove callouts from an object about to be destructed
 */
remove_callouts(object obj, string owner, int n)
{
    if (previous_program() == AUTO) {
	mixed **callouts, *callout;
	int i;

	owners[owner]->rsrc_incr("callouts", obj, -n, resources["callouts"]);

	if (suspended && suspended[obj]) {
	    callouts = map_values(suspended[obj]);
	    for (i = sizeof(callouts); --i >= 0; ) {
		callout = callouts[i];
		if (callout != first_suspended) {
		    callout[3][4] = callout[4];
		} else {
		    first_suspended = callout[4];
		}
		if (callout != last_suspended) {
		    callout[4][3] = callout[3];
		} else {
		    last_suspended = callout[3];
		}
		callout[3] = callout[4] = 0;
	    }
	    suspended[obj] = 0;
	}
    }
}

/*
 * NAME:	release()
 * DESCRIPTION:	release callouts
 */
static release()
{
    mixed *callout;
    object obj;
    int handle;

    suspend = 0;
    while (first_suspended) {
	callout = first_suspended;
	if (!(first_suspended=callout[4])) {
	    last_suspended = 0;
	}
	callout[3] = callout[4] = 0;
	suspended[obj=callout[0]][handle=callout[2]] = 0;
	owners[callout[1]]->rsrc_incr("callouts", obj, -1,
				      resources["callouts"], FALSE);
	catch {
	    obj->_F_release(handle);
	}
	if (suspend != 0) {
	    if (suspend > 0) {
		remove_call_out(suspend);
		suspend = 0;
	    } else {
		return;
	    }
	}
    }
    suspended = 0;
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
