# include <kernel/kernel.h>
# include <kernel/user.h>
# include <status.h>

object *telnet_users, *binary_users;		/* user arrays */
object telnet_manager, binary_manager;		/* user object managers */
mapping names;

/*
 * NAME:	create()
 * DESCRIPTION:	initialize object
 */
static create()
{
    /* compile essential objects */
    compile_object(TELNET_CONN);
    compile_object(BINARY_CONN);
    compile_object(DEFAULT_USER);

    /* initialize user arrays */
    telnet_users = ({ });
    binary_users = ({ });
    names = ([ ]);
}

/*
 * NAME:	telnet_connection()
 * DESCRIPTION:	return a new telnet connection object
 */
object telnet_connection()
{
    if (KERNEL()) {
	return clone_object(TELNET_CONN);
    }
}

/*
 * NAME:	binary_connection()
 * DESCRIPTION:	return a new binary connection object
 */
object binary_connection()
{
    if (KERNEL()) {
	return clone_object(BINARY_CONN);
    }
}

/*
 * NAME:	set_telnet_manager()
 * DESCRIPTION:	set the telnet manager object, which determines what the
 *		user object is, based on the first line of input
 */
set_telnet_manager(object manager)
{
    if (SYSTEM()) {
	telnet_manager = manager;
    }
}

/*
 * NAME:	set_binary_manager()
 * DESCRIPTION:	set the binary manager object, which determines what the
 *		user object is, based on the first line of input
 */
set_binary_manager(object manager)
{
    if (SYSTEM()) {
	binary_manager = manager;
    }
}

/*
 * NAME:	telnet_user()
 * DESCRIPTION:	select user object for telnet connection, based on line of
 *		input
 */
object telnet_user(string str)
{
    if (KERNEL()) {
	object user;

	user = (telnet_manager) ?
		telnet_manager->select(str) : clone_object(DEFAULT_USER);
	if (!user) {
	    return 0;
	}
	if (function_object("query_conn", user) != LIB_USER) {
	    error("Invalid user object");
	}
	telnet_users += ({ user });
	return user;
    }
}

/*
 * NAME:	binary_user()
 * DESCRIPTION:	select user object for binary connection, based on line of
 *		input
 */
object binary_user(string str)
{
    if (KERNEL()) {
	object user;

	user = (binary_manager && str != "admin") ?
		binary_manager->select(str) : clone_object(DEFAULT_USER);
	if (!user) {
	    return 0;
	}
	if (function_object("query_conn", user) != LIB_USER) {
	    error("Invalid user object");
	}
	binary_users += ({ user });
	return user;
    }
}

/*
 * NAME:	set_name()
 * DESCRIPTION:	set the name of a connection object
 */
set_name(string name, object conn)
{
    if (KERNEL()) {
	names[name] = conn;
    }
}

object *query_users()  { if (SYSTEM()) { return telnet_users + binary_users; } }
object *query_telnet_users()	{ if (SYSTEM()) { return telnet_users[..]; } }
object *query_binary_users()	{ if (SYSTEM()) { return binary_users[..]; } }

/*
 * NAME:	query_user()
 * DESCRIPTION:	find the user associated with a certain name
 */
object query_user(string name)
{
    object obj;

    obj = names[name];
    return (obj) ? obj->query_user() : 0;
}

/*
 * NAME:	query_telnet_timeout()
 * DESCRIPTION:	return the current telnet connection timeout
 */
int query_telnet_timeout(string ipnum)
{
    return (telnet_manager) ?
	    telnet_manager->query_timeout(ipnum) : DEFAULT_TIMEOUT;
}

/*
 * NAME:	query_binary_timeout()
 * DESCRIPTION:	return the current binary connection timeout
 */
int query_binary_timeout(string ipnum)
{
    return (binary_manager) ?
	    binary_manager->query_timeout(ipnum) : DEFAULT_TIMEOUT;
}

/*
 * NAME:	query_telnet_banner()
 * DESCRIPTION:	return the current telnet login banner
 */
string query_telnet_banner()
{
    return (telnet_manager) ?
	    telnet_manager->query_banner() :
	    "\nDGD " + status()[ST_VERSION] + " (telnet)\n\nlogin: ";
}

/*
 * NAME:	query_binary_banner()
 * DESCRIPTION:	return the current binary login banner
 */
string query_binary_banner()
{
    return (binary_manager) ?
	    binary_manager->query_banner() :
	    "\r\nDGD " + status()[ST_VERSION] + " (binary)\r\n\r\nlogin: ";
}

/*
 * NAME:	telnet_disconnect()
 * DESCRIPTION:	disconnect telnet user
 */
telnet_disconnect(object user)
{
    if (KERNEL()) {
	telnet_users -= ({ user });
    }
}

/*
 * NAME:	binary_disconnect()
 * DESCRIPTION:	disconnect binary user
 */
binary_disconnect(object user)
{
    if (KERNEL()) {
	binary_users -= ({ user });
    }
}
