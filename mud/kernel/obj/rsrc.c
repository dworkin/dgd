# include <kernel/kernel.h>
# include <kernel/rsrc.h>
# include <type.h>
# include <status.h>

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
	resources = ([
			"stack" :	({   0, -1, 0 }),
			"ticks" :	({   0, -1, 0 }),
			"tick usage" :	({ 0.0, -1, 0 })
		    ]);
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
	resources[name] = nil;
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

	if ((rsrc=resources[name])) {
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
    decay = (float) (100 - (int) grsrc[GRSRC_DECAY]) / 100.0;
    period = grsrc[GRSRC_PERIOD];
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

    rlimits (-1; -1) {
	rsrc[RSRC_DECAYTIME] = t;
	rsrc[RSRC_USAGE] = floor(usage + 0.5);
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
	    return ({ ((int) grsrc[GRSRC_DECAY] == 0) ? 0 : 0.0,
		      grsrc[GRSRC_MAX], 0 }) +
		   grsrc[GRSRC_DECAY .. GRSRC_PERIOD];
	} else {
	    if ((int) grsrc[GRSRC_DECAY] != 0 &&
		(time=time()) - (int) rsrc[RSRC_DECAYTIME] >=
						    (int) grsrc[GRSRC_PERIOD]) {
		/* decay resource */
		decay_rsrc(rsrc, grsrc, time);
	    }
	    rsrc += grsrc[GRSRC_DECAY .. GRSRC_PERIOD];
	    if ((int) rsrc[RSRC_MAX] < 0) {
		rsrc[RSRC_MAX] = grsrc[GRSRC_MAX];
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
		   ((int) grsrc[GRSRC_DECAY] == 0) ? ({   0, -1,    0 }) :
						     ({ 0.0, -1, time });
	    max = grsrc[GRSRC_MAX];
	} else {
	    /* existing resource */
	    if ((int) grsrc[GRSRC_DECAY] != 0 &&
		time - (int) rsrc[RSRC_DECAYTIME] >= (int) grsrc[GRSRC_PERIOD])
	    {
		/* decay resource */
		decay_rsrc(rsrc, grsrc, time);
	    }

	    max = ((int) rsrc[RSRC_MAX] >= 0) ?
		   rsrc[RSRC_MAX] : grsrc[GRSRC_MAX];
	}

	if (!force && max >= 0 &&
	    (incr > max || (int) rsrc[RSRC_USAGE] > max - incr) && incr > 0) {
	    return FALSE;	/* would exceed limit */
	}

	if (incr != 0) {
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
			} else if (!rsrc[RSRC_INDEXED][index]) {
			    rsrc[RSRC_INDEXED][index] = incr;
			} else if (!(rsrc[RSRC_INDEXED][index] += incr)) {
			    rsrc[RSRC_INDEXED][index] = nil;
			}
		    } : {
			return FALSE;	/* error: increment failed */
		    }
		}
		if (typeof(rsrc[RSRC_USAGE]) == T_INT) {
		    /* normal resource */
		    rsrc[RSRC_USAGE] += incr;
		} else {
		    /*
		     * decaying resources are expected to be updated frequently,
		     * so handle it with a callout
		     */
		    call_out("rsrc_update", 0, rsrc, incr);
		}
	    }
	}

	return TRUE;
    }
}

/*
 * NAME:	rsrc_update()
 * DESCRIPTION:	update a decaying resource
 */
static rsrc_update(float *rsrc, int incr)
{
    rsrc[RSRC_USAGE] += (float) incr;
}

/*
 * NAME:	call_limits()
 * DESCRIPTION:	return stack & ticks limits
 */
mixed *call_limits(mixed *limits, int stack, int ticks,
		   int *rstack, int *rticks, int *usage)
{
    if (previous_object() == rsrcd) {
	int maxstack, maxticks;

	/* determine available stack */
	maxstack = resources["stack"][RSRC_MAX];
	if (maxstack < 0) {
	    maxstack = rstack[GRSRC_MAX];
	}
	if (maxstack > stack && stack >= 0) {
	    maxstack = stack;
	}
	if (maxstack >= 0) {
	    maxstack++;
	}

	/* determine available ticks */
	maxticks = resources["ticks"][RSRC_MAX];
	if (maxticks < 0) {
	    maxticks = rticks[GRSRC_MAX];
	}
	if (maxticks >= 0) {
	    mixed *rsrc;
	    int n;

	    rsrc = resources["tick usage"];
	    if ((n=time()) - (int) rsrc[RSRC_DECAYTIME] >=
						    usage[GRSRC_PERIOD]) {
		/* decay resource */
		decay_rsrc(rsrc, usage, n);
	    }
	    n = rsrc[RSRC_MAX];
	    if (n < 0) {
		n = usage[GRSRC_MAX];
	    }
	    if (n >= 0 && (int) rsrc[RSRC_USAGE] >= n >> 1) {
		maxticks = (int) ((float) maxticks *
				  ((float) n - rsrc[RSRC_USAGE]) /
				  (float) (n >> 1));
	    }
	    if (maxticks > ticks - 25 && ticks >= 0) {
		maxticks = ticks - 25;
	    }
	    if (maxticks <= 0) {
		maxticks = 1;
	    }
	}

	return ({ limits, owner, maxstack, maxticks, ticks });
    }
}

/*
 * NAME:	update_ticks()
 * DESCRIPTION:	update ticks for the current owner
 */
update_ticks(int ticks)
{
    if (previous_object() == rsrcd) {
	call_out("rsrc_update", 0, resources["tick usage"], ticks);
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
		(int) rsrc[RSRC_DECAYTIME] != 0) {
		rsrc[RSRC_DECAYTIME] += downtime;
	    }
	}
    }
}
