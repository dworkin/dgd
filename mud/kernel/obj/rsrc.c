# include <kernel/kernel.h>
# include <kernel/rsrc.h>
# include <type.h>

object rsrcd;		/* resource manager */
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
 * NAME:	remove_rsrc()
 * DESCRIPTION:	remove a resource
 */
remove_rsrc(string name)
{
    if (previous_object() == rsrcd) {
	resources[name] = 0;
    }
}

/*
 * NAME:	rsrc_set_limit()
 * DESCRIPTION:	set individual resource limit
 */
rsrc_set_limit(string name, int max, int decay)
{
    if (previous_object() == rsrcd) {
	mixed *rsrc;

	rsrc = resources[name];
	if (rsrc) {
	    rsrc[RSRC_MAX] = max;
	} else {
	    resources[name] = ({ (decay == 0) ? 0 : 0.0, max, 0 });
	}
    }
}

/*
 * NAME:	decay_rsrc()
 * DESCRIPTION:	decay a resource
 */
private decay_rsrc(mixed *rsrc, mixed *grsrc, int time)
{
    float usage, decay;
    int period, t;

    usage = rsrc[RSRC_USAGE];
    decay = (float) (100 - (int) grsrc[RSRC_DECAY - 1]) / 100.0;
    period = grsrc[RSRC_PERIOD - 1];
    time -= period;
    t = rsrc[RSRC_DECAYTIME];

    do {
	usage *= decay;
	if (usage < 0.5) {
	    t = time;
	    break;
	}
	t += period;
    } while (time >= t);
    usage = floor(usage + 0.5);

    rlimits (-1; -1) {
	rsrc[RSRC_DECAYTIME] = t;
	grsrc[RSRC_USAGE] -= rsrc[RSRC_USAGE] - usage;
	rsrc[RSRC_USAGE] = usage;
    }
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
int *rsrc_get(string name, mixed *grsrc)
{
    if (previous_object() == rsrcd) {
	mixed *rsrc;
	int time;

	rsrc = resources[name];
	if (!rsrc) {
	    return ({ ((int) grsrc[RSRC_DECAY - 1] == 0) ? 0 : 0.0,
		      grsrc[RSRC_MAX], 0 }) +
		   grsrc[RSRC_DECAY - 1 .. RSRC_PERIOD - 1];
	} else {
	    if ((int) grsrc[RSRC_DECAY - 1] != 0 &&
		(time=time()) - (int) rsrc[RSRC_DECAYTIME] >=
						(int) grsrc[RSRC_PERIOD - 1]) {
		/* decay resource */
		decay_rsrc(rsrc, grsrc, time);
	    }
	    rsrc += grsrc[RSRC_DECAY - 1 .. RSRC_PERIOD - 1];
	    if ((int) rsrc[RSRC_MAX] < 0) {
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
int rsrc_incr(string name, mixed index, int incr, mixed *grsrc, int force)
{
    if (previous_object() == rsrcd) {
	mixed *rsrc;
	int time, max;

	rsrc = resources[name];
	time = time();
	if (!rsrc) {
	    /* new resource */
	    rsrc = resources[name] =
		   ((int) grsrc[RSRC_DECAY - 1] == 0) ? ({   0, -1,    0 }) :
							({ 0.0, -1, time });
	    max = grsrc[RSRC_MAX];
	} else {
	    /* existing resource */
	    if ((int) grsrc[RSRC_DECAY - 1] != 0 &&
		time - (int) rsrc[RSRC_DECAYTIME] >=
						(int) grsrc[RSRC_PERIOD - 1]) {
		/* decay resource */
		decay_rsrc(rsrc, grsrc, time);
	    }

	    max = (((int) rsrc[RSRC_MAX] >= 0) ? rsrc : grsrc)[RSRC_MAX];
	}

	if (!force && max >= 0 &&
	    (incr > max || (int) rsrc[RSRC_USAGE] > max - incr) && incr > 0) {
	    return 0;	/* would exceed limit */
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
	    if (typeof(rsrc[RSRC_USAGE]) == T_INT) {
		/* normal resource */
		rsrc[RSRC_USAGE] += incr;
		grsrc[RSRC_USAGE] += incr;
TRACE(owner + ": " + name + " from " + ((int) rsrc[RSRC_USAGE] - incr) + " to " + (int) rsrc[RSRC_USAGE]);
	    } else {
		/* decaying resource */
		index = (float) incr;
		rsrc[RSRC_USAGE] += index;
		grsrc[RSRC_USAGE] += index;
TRACE(owner + ": " + name + " from " + ((float) rsrc[RSRC_USAGE] - index) + " to " + (float) rsrc[RSRC_USAGE]);
	    }
	}

	return 1;
    }
}


/*
 * NAME:	reboot()
 * DESCRIPTION:	recover from a reboot
 */
reboot(int downtime)
{
    if (previous_object() == rsrcd) {
	mixed **rsrcs, *rsrc;
	int i;

	rsrcs = map_values(resources);
	for (i = sizeof(rsrcs); --i >= 0; ) {
	    rsrc = rsrcs[i];
	    if (typeof(rsrc[RSRC_DECAYTIME]) == T_INT &&
		(int) rsrc[RSRC_DECAYTIME] > 0) {
		rsrc[RSRC_DECAYTIME] += downtime;
	    }
	}
    }
}
