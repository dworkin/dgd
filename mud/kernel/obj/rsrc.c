# include <kernel/kernel.h>
# include <kernel/rsrc.h>
# include <type.h>
# include <status.h>

object rsrcd;		/* resource manager */
mapping resources;	/* registered resources */
string owner;		/* owner of these resources */
int maxticks;		/* maximum amount of ticks currently allowed */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize resource mapping
 */
static void create(int clone)
{
    if (clone) {
	resources = ([
			"stack" :	({   0, -1, 0 }),
			"ticks" :	({   0, -1, 0 }),
			"tick usage" :	({ 0.0, -1, 0 })
		    ]);
	maxticks = -1;
	rsrcd = find_object(RSRCD);
    }
}

/*
 * NAME:	set_owner()
 * DESCRIPTION:	set the owner of this resource
 */
void set_owner(string name)
{
    if (previous_object() == rsrcd) {
	owner = name;
    }
}

/*
 * NAME:	remove_rsrc()
 * DESCRIPTION:	remove a resource
 */
void remove_rsrc(string name)
{
    if (previous_object() == rsrcd) {
	resources[name] = nil;
    }
}

/*
 * NAME:	decay_rsrc()
 * DESCRIPTION:	decay a resource
 */
private void decay_rsrc(mixed *rsrc, mixed *grsrc, int time)
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
	    t = time + period;
	    break;
	}
	t += period;
    } while (time >= t);

    rsrc[RSRC_DECAYTIME] = t;
    rsrc[RSRC_USAGE] = floor(usage + 0.5);
}

/*
 * NAME:	set_rlimits()
 * DESCRIPTION:	set stack/tick limits
 */
private void set_rlimits(mixed *rsrc, int update)
{
    int max;

    max = rsrc[RSRC_MAX];
    max = (max < 0 || rsrc[RSRC_USAGE] < (float) max) ?
	   resources["ticks"][RSRC_MAX] : 1;
    if (update || maxticks != max) {
	maxticks = max;
	rsrcd->set_rlimits(owner,
			   ({
			      resources["stack"][RSRC_MAX],
			      max,
			      rsrc[RSRC_DECAYTIME]
			   }));
    }
}

/*
 * NAME:	rsrc_set_limit()
 * DESCRIPTION:	set individual resource limit
 */
void rsrc_set_limit(string name, int max, int decay)
{
    if (previous_object() == rsrcd) {
	mixed *rsrc;

	if ((rsrc=resources[name])) {
	    rsrc[RSRC_MAX] = max;
	    if (name == "stack" || name == "ticks" || name == "tick usage") {
		rlimits (-1; -1) {
		    set_rlimits(resources["tick usage"], TRUE);
		}
	    }
	} else {
	    resources[name] = ({ (decay == 0) ? 0 : 0.0, max, 0 });
	}
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
		rlimits (-1; -1) {
		    /* decay resource */
		    decay_rsrc(rsrc, grsrc, time);
		    if (name == "tick usage") {
			set_rlimits(rsrc, TRUE);
		    }
		}
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
    if (previous_program() == RSRCD) {
	if ((int) grsrc[GRSRC_DECAY] != 0) {
	    if (incr != 0) {
		call_out("decayed_incr", 0, name, incr, grsrc);
	    }
	} else {
	    mixed *rsrc;
	    int max;

	    rsrc = resources[name];
	    if (!rsrc) {
		/* new resource */
		rsrc = resources[name] = ({ 0, -1, 0 });
		max = grsrc[GRSRC_MAX];
	    } else {
		/* existing resource */
		max = ((int) rsrc[RSRC_MAX] >= 0) ?
		       rsrc[RSRC_MAX] : grsrc[GRSRC_MAX];
	    }

	    if (incr != 0) {
		if (!force && incr > 0 && max >= 0 &&
		    (incr > max || (int) rsrc[RSRC_USAGE] > max - incr)) {
		    return FALSE;	/* would exceed limit */
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
			    } else if (typeof(rsrc[RSRC_INDEXED]) != T_MAPPING)
			    {
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
		    rsrc[RSRC_USAGE] += incr;
		}
	    }
	}

	return TRUE;
    }
}

/*
 * NAME:	decayed_incr()
 * DESCRIPTION:	increment or decrement a decaying resource
 */
static void decayed_incr(string name, int incr, mixed *grsrc)
{
    mixed *rsrc;
    int time;

    rsrc = resources[name];
    time = time();
    if (!rsrc) {
	/* new resource */
	rsrc = resources[name] = ({ 0.0, -1, time });
    } else if (time - (int) rsrc[RSRC_DECAYTIME] >= (int) grsrc[GRSRC_PERIOD]) {
	/* decay resource */
	decay_rsrc(rsrc, grsrc, time);
	time = 0;
    }
    rsrc[RSRC_USAGE] += (float) incr;

    if (name == "tick usage") {
	set_rlimits(rsrc, time == 0);
    }
}

/*
 * NAME:	decay_ticks()
 * DESCRIPTION:	decay ticks
 */
void decay_ticks(int *limits, int time, mixed *grsrc)
{
    if (previous_object() == rsrcd) {
	mixed *rsrc;

	rlimits (-1; -1) {
	    rsrc = resources["tick usage"];
	    decay_rsrc(rsrc, grsrc, time);
	    maxticks = rsrc[RSRC_MAX];
	    maxticks = (maxticks < 0 || rsrc[RSRC_USAGE] < (float) maxticks) ?
			resources["ticks"][RSRC_MAX] : 1;
	    limits[LIM_MAX_TICKS] = maxticks;
	    limits[LIM_MAX_TIME] = rsrc[RSRC_DECAYTIME];
	}
    }
}

/*
 * NAME:	update_ticks()
 * DESCRIPTION:	update ticks for the current owner
 */
void update_ticks(int ticks, mixed *grsrc)
{
    if (previous_program() == RSRCD) {
	call_out("incr_ticks", 0, ticks, grsrc);
    }
}

/*
 * NAME:	incr_ticks()
 * DESCRIPTION:	increase ticks
 */
static void incr_ticks(int ticks, mixed *grsrc)
{
    mixed *rsrc;
    int time, max;

    rsrc = resources["tick usage"];
    time = time();
    if (time - (int) rsrc[RSRC_DECAYTIME] >= (int) grsrc[GRSRC_PERIOD]) {
	/* decay resource */
	decay_rsrc(rsrc, grsrc, time);
	time = 0;
    }

    rsrc[RSRC_USAGE] += (float) ticks;
    set_rlimits(rsrc, time == 0);
}

/*
 * NAME:	reboot()
 * DESCRIPTION:	recover from a reboot
 */
void reboot(int downtime)
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
