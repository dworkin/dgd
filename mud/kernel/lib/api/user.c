# include <kernel/kernel.h>
# include <kernel/user.h>

private object userd;		/* user manager */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize API
 */
static void create()
{
    userd = find_object(USERD);
}

/*
 * NAME:	query_connections()
 * DESCRIPTION:	return the current active connections
 */
static object *query_connections()
{
    return userd->query_connections();
}

/*
 * NAME:	find_user()
 * DESCRIPTION:	find a user by name
 */
static object find_user(string name)
{
    if (!name) {
	error("Bad argument for find_user");
    }
    return userd->find_user(name);
}
