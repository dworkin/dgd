# include <kernel/kernel.h>
# include <kernel/user.h>

private object userd;		/* user manager */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize API
 */
static create()
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
 * NAME:	query_telnet_users()
 * DESCRIPTION:	return the users logged in on the telnet port
 */
static object *query_telnet_users()
{
    return userd->query_telnet_users();
}

/*
 * NAME:	query_binary_users()
 * DESCRIPTION:	return the users logged in on the binary port
 */
static object *query_binary_users()
{
    return userd->query_binary_users();
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
