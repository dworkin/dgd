# include <kernel/rsrc.h>
# include <type.h>

object rsrcd;		/* resource daemon */
mapping resources;	/* registered resources */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize resource mapping
 */
static create()
{
    resources = ([ ]);		/* no resources yet */
    rsrcd = find_object(RSRCD);
}

/*
 * NAME:	decay_rsrc()
 * DESCRIPTION:	decay a resource
 */
private decay_rsrc(int *rsrc, int *xrsrc)
{
    int usage, decay, period, i, time;

    usage = rsrc[RSRC_USAGE];
    decay = 100 - xrsrc[RSRC_DECAY];
    period = xrsrc[RSRC_PERIOD];
    i = rsrc[RSRC_DECAYTIME];

    time = time();
    do {
	usage = usage * decay / 100;
	i += period;
    } while (time - i >= period && usage != 0);

    rsrc[RSRC_DECAYTIME] = i;
    xrsrc[RSRC_USAGE] -= rsrc[RSRC_USAGE] - usage;
    rsrc[RSRC_USAGE] = usage;
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
	if (rsrc != 0) {
	    rsrc[RSRC_MAX] = max;
	} else {
	    resources[name] = ({ 0, max, time() });
	}
    }
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
int *rsrc_get(string name, mixed *xrsrc)
{
    if (previous_object() == rsrcd) {
	int *rsrc;

	rsrc = resources[name];
	if (rsrc == 0) {
	    return resources[name] = ({ 0, -1, time() });
	}
	if (typeof(xrsrc[RSRC_DECAY]) == T_INT && xrsrc[RSRC_DECAY] != 0 &&
	    time() - rsrc[RSRC_DECAYTIME] >= xrsrc[RSRC_PERIOD]) {
	    decay_rsrc(rsrc, xrsrc);
	}
	return rsrc;
    }
}

/*
 * NAME:	rsrc_incr()
 * DESCRIPTION:	increment or decrement a resource, return 1 if successful,
 *		0 if the maximum would be exceeded
 */
int rsrc_incr(string name, mixed index, int incr, mixed *xrsrc, int force)
{
    if (previous_object() == rsrcd) {
	mixed *rsrc;
	int max;

	rsrc = resources[name];
	if (rsrc == 0) {
	    /* new resource */
	    rsrc = resources[name] = ({ 0, -1, time() });
	    max = xrsrc[RSRC_MAX];
	} else {
	    /* existing resource */
	    if (typeof(xrsrc[RSRC_DECAY]) == T_INT && xrsrc[RSRC_DECAY] != 0 &&
		time() - rsrc[RSRC_DECAYTIME] >= xrsrc[RSRC_PERIOD]) {
		decay_rsrc(rsrc, xrsrc);
	    }
	    max = ((rsrc[RSRC_MAX] >= 0) ? rsrc : xrsrc)[RSRC_MAX];
	}

	if (!force && max >= 0 && rsrc[RSRC_USAGE] + incr > max && incr > 0) {
	    /* would exceed limit */
	    return 0;
	}
	rsrc[RSRC_USAGE] += incr;
	if (index != 0) {
	    /* indexed resource */
	    if (typeof(rsrc[RSRC_INDEXED]) != T_MAPPING) {
		rsrc[RSRC_INDEXED] = ([ index : incr ]);
	    } else {
		rsrc[RSRC_INDEXED][index] += incr;
	    }
	    if (typeof(index) == T_OBJECT) {
		/* let object keep track */
		index->_F_rsrc_incr(name, incr);
	    }
	}
	return 1;
    }
}
