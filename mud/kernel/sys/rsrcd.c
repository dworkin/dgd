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
    resources = ([ ]);		/* no resources yet */
    owners = ([ ]);		/* and no resource owners */
}

/*
 * NAME:	robj()
 * DESCRIPTION:	get a resource owner object, making a new one if it doesn't
 *		exist yet
 */
private object robj(string name)
{
    object owner;

    owner = owners[name];
    if (owner == 0) {
	return owners[name] = clone_object(RSRCOBJ);
    }
    return owner;
}

/*
 * NAME:	set_rsrc()
 * DESCRIPTION:	set the maximum, decay percentage and decay period of a
 *		resource
 */
set_rsrc(string name, int max, int decay, int period)
{
    if (PRIV0()) {
	mixed *rsrc;

	rsrc = resources[name];
	if (rsrc != 0) {
	    /*
	     * existing resource
	     */
	    rsrc[RSRC_MAX] = max;
	    rsrc[RSRC_DECAY] = decay;
	    rsrc[RSRC_PERIOD] = period;
	} else {
	    /* new resource */
	    resources[name] = ({ 0, max, 0, decay, period });
	}
    }
}

/*
 * NAME:	query_rsrc()
 * DESCRIPTION:	get resource usage
 */
mixed *query_rsrc(string name)
{
    if (PRIV0()) {
	return resources[name][ .. ];
    }
}

/*
 * NAME:	query_rsrc_list()
 * DESCRIPTION:	get a list of resources
 */
string *query_rsrc_list()
{
    return map_indices(resources);
}

/*
 * NAME:	query_owner_list()
 * DESCRIPTION:	get a list of resources
 */
string *query_owner_list()
{
    return map_indices(owners);
}

/*
 * NAME:	del_rsrc()
 * DESCRIPTION:	delete a resource
 */
del_rsrc(string name)
{
    if (PRIV0()) {
	object *robjs;
	int i;

	resources[name] = 0;
	robjs = map_values(owners);
	i = sizeof(robjs);
	while (--i >= 0) {
	    robjs[i]->del_rsrc(name);
	}
    }
}

/*
 * NAME:	rsrc_set_limit()
 * DESCRIPTION:	set individual resource limit
 */
rsrc_set_limit(string owner, string name, int max)
{
    if (PRIV0()) {
	robj(owner)->rsrc_set_limit(name, max);
    }
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
int *rsrc_get(string owner, string name)
{
    if (PRIV0()) {
	mixed *rsrc;

	rsrc = robj(owner)->rsrc_get(name) +
	       resources[name][RSRC_DECAY .. RSRC_PERIOD - 1];
	if (typeof(rsrc[RSRC_INDEXED]) == T_MAPPING) {
	    /* replace indexed resource by copy */
	    rsrc[RSRC_INDEXED] += ([ ]);
	}
	return rsrc;
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
    if (PRIV0()) {
	int *rsrc;

	rsrc = resources[name];
	if (robj(owner)->rsrc_incr(name, index, incr, rsrc, force)) {
	    /* increment succeeded */
	    rsrc[RSRC_USAGE] += incr;
	    return 1;
	} else {
	    /* increment failed */
	    return 0;
	}
    }
}
