# include <kernel/rsrc.h>
# include <type.h>

object rsrcd;		/* resource daemon */
mapping resources;	/* registered resources */
string owner;		/* owner of these resources */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize resource mapping
 */
static create(int clone)
{
    if (clone) {
	resources = ([ ]);		/* no resources yet */
	rsrcd = find_object(RSRCD);
    }
}

/*
 * NAME:	set_owner()
 * DESCRIPTION:	set the owner of this resource
 */
set_owner(string name)
{
    if (previous_object() == rsrcd) {
	owner = name;
    }
}

/*
 * NAME:	del_rsrc()
 * DESCRIPTION:	delete a resource
 */
del_rsrc(string name)
{
    if (previous_object() == rsrcd) {
	resources[name] = 0;
    }
}

/*
 * NAME:	rsrc_set_limit()
 * DESCRIPTION:	set individual resource limit
 */
rsrc_set_limit(string name, int max)
{
    if (previous_object() == rsrcd) {
	int *rsrc;

	rsrc = resources[name];
	if (rsrc) {
	    rsrc[RSRC_MAX] = max;
	} else {
	    resources[name] = ({ 0, max, 0 });
	}
    }
}

/*
 * NAME:	decay_rsrc()
 * DESCRIPTION:	decay a resource
 */
private decay_rsrc(int *rsrc, int *grsrc, int time)
{
    int usage, decay, period, i;

    usage = rsrc[RSRC_USAGE];
    decay = 100 - grsrc[RSRC_DECAY - 1];
    period = grsrc[RSRC_PERIOD - 1];
    time -= period;
    i = rsrc[RSRC_DECAYTIME];

    do {
	usage = usage * decay / 100;
	if (usage == 0) {
	    i = time;
	    break;
	}
	i += period;
    } while (time >= i);

    rlimits (-1; -1) {
	rsrc[RSRC_DECAYTIME] = i;
	grsrc[RSRC_USAGE] -= rsrc[RSRC_USAGE] - usage;
	rsrc[RSRC_USAGE] = usage;
    }
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
int *rsrc_get(string name, int *grsrc)
{
    if (previous_object() == rsrcd) {
	mixed *rsrc;
	int time;

	rsrc = resources[name];
	if (!rsrc) {
	    return ({ 0, grsrc[RSRC_MAX], 0 }) +
		   grsrc[RSRC_DECAY - 1 .. RSRC_PERIOD - 2];
	} else {
	    if (grsrc[RSRC_DECAY - 1] != 0 &&
		(time=time()) - rsrc[RSRC_DECAYTIME] >= grsrc[RSRC_PERIOD - 1])
	    {
		/* decay resource */
		decay_rsrc(rsrc, grsrc, time);
	    }
	    rsrc += grsrc[RSRC_DECAY - 1 .. RSRC_PERIOD - 2];
	    if (rsrc[RSRC_MAX] < 0) {
		rsrc[RSRC_MAX] = grsrc[RSRC_MAX];
	    }
	    if (typeof(rsrc[RSRC_INDEXED]) == T_MAPPING) {
		rsrc[RSRC_INDEXED] = rsrc[RSRC_INDEXED][..];
	    }
	    return rsrc;
	}
    }
}

/*
 * NAME:	rsrc_incr()
 * DESCRIPTION:	increment or decrement a resource, return 1 if successful,
 *		0 if the maximum would be exceeded
 */
int rsrc_incr(string name, mixed index, int incr, int *grsrc, int force)
{
    if (previous_object() == rsrcd) {
	mixed *rsrc;
	int time, max;

	rsrc = resources[name];
	time = time();
	if (!rsrc) {
	    /* new resource */
	    rsrc = resources[name] = ({ 0, -1, 0 });
	    if (grsrc[RSRC_DECAY - 1] != 0) {
		rsrc[RSRC_DECAYTIME] = time;
	    }
	    max = grsrc[RSRC_MAX];
	} else {
	    /* existing resource */
	    if (grsrc[RSRC_DECAY - 1] != 0 &&
		time - rsrc[RSRC_DECAYTIME] >= grsrc[RSRC_PERIOD - 1]) {
		/* decay resource */
		decay_rsrc(rsrc, grsrc, time);
	    }

	    max = ((rsrc[RSRC_MAX] >= 0) ? rsrc : grsrc)[RSRC_MAX];
	}

	if (!force && max >= 0 && rsrc[RSRC_USAGE] + incr > max && incr > 0) {
	    /* would exceed limit */
	    return 0;
	}

	rlimits (-1; -1) {
	    if (index) {
		/*
		 * indexed resource
		 */
		catch {
		    if (typeof(index) == T_OBJECT) {
			/* let object keep track */
			index->_F_rsrc_incr(name, incr);
		    } else if (typeof(rsrc[RSRC_INDEXED]) != T_MAPPING) {
			rsrc[RSRC_INDEXED] = ([ index : incr ]);
		    } else {
			rsrc[RSRC_INDEXED][index] += incr;
		    }
		} : {
		    return 0;	/* error: increment failed */
		}
	    }
	    rsrc[RSRC_USAGE] += incr;
	    grsrc[RSRC_USAGE] += incr;
	}

TRACE(owner + ": " + name + " from " + (rsrc[RSRC_USAGE] - incr) + " to " + rsrc[RSRC_USAGE]);
	return 1;
    }
}
