# include <kernel/kernel.h>
# include <kernel/rsrc.h>

private object rsrcd;		/* resource manager */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize API
 */
static create()
{
    rsrcd = find_object(RSRCD);
}

/*
 * NAME:	add_owner()
 * DESCRIPTION:	add a new resource owner
 */
static add_owner(string owner)
{
    rsrcd->add_owner(owner);
}

/*
 * NAME:	remove_owner()
 * DESCRIPTION:	remove a resource owner
 */
static remove_owner(string owner)
{
    rsrcd->remove_owner(owner);
}

/*
 * NAME:	query_owners()
 * DESCRIPTION:	return a list of resource owners
 */
static string *query_owners()
{
    return rsrcd->query_owners();
}


/*
 * NAME:	set_rsrc()
 * DESCRIPTION:	set the maximum, decay percentage and decay period of a
 *		resource
 */
static set_rsrc(string name, int max, int decay, int period)
{
    if (!name || max < -1 || decay < 0 || decay > 100 || period < 0 ||
	!decay != !period) {
	error("Bad arguments for set_rsrc");
    }
    rsrcd->set_rsrc(name, max, decay, period);
}

/*
 * NAME:	remove_rsrc()
 * DESCRIPTION:	remove a resource
 */
static remove_rsrc(string name)
{
    if (!name) {
	error("Bad argument for remove_rsrc");
    }
    rsrcd->remove_rsrc(name);
}

/*
 * NAME:	query_rsrc()
 * DESCRIPTION:	get usage and limits of a resource
 */
static mixed *query_rsrc(string name)
{
    if (!name) {
	error("Bad argument for query_rsrc");
    }
    return rsrcd->query_rsrc(name);
}

/*
 * NAME:	query_resources()
 * DESCRIPTION:	return a list of resources
 */
static string *query_resources()
{
    return rsrcd->query_resources();
}


/*
 * NAME:	rsrc_set_limit()
 * DESCRIPTION:	set individual resource limit
 */
static rsrc_set_limit(string owner, string name, int max)
{
    if (!name || max < -1) {
	error("Bad arguments for rsrc_set_limit");
    }
    rsrcd->rsrc_set_limit(owner, name, max);
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
static mixed *rsrc_get(string owner, string name)
{
    if (!name) {
	error("Bad arguments for rsrc_get");
    }
    return rsrcd->rsrc_get(owner, name);
}

/*
 * NAME:	rsrc_incr()
 * DESCRIPTION:	increment or decrement a resource, returning 1 if succeeded,
 *		0 if failed
 */
static int rsrc_incr(string owner, string name, mixed index, int incr,
		     varargs int force)
{
    if (!name) {
	error("Bad arguments for rsrc_incr");
    }
    return rsrcd->rsrc_incr(owner, name, index, incr, force);
}
