# include <kernel/kernel.h>
# include <kernel/user.h>
# include <status.h>

mapping telnet_users, binary_users;		/* user mappings */
object telnet_manager, binary_manager;		/* user object managers */
mapping names;					/* name : connection object */
object *connections;				/* saved connections */

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
    telnet_users = ([ ]);
    binary_users = ([ ]);
    names = ([ ]);
}

/*
 * NAME:	telnet_connection()
 * DESCRIPTION:	return a new telnet connection object
 */
object telnet_connection()
{
    if (previous_program() == DRIVER) {
	return clone_object(TELNET_CONN);
    }
}

/*
 * NAME:	binary_connection()
 * DESCRIPTION:	return a new binary connection object
 */
object binary_connection()
{
    if (previous_program() == DRIVER) {
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
    if (previous_program() == LIB_CONN) {
	object user;

	if (telnet_manager) {
	    user = telnet_manager->select(str);
	    if (!user) {
		return 0;
	    }
	} else {
	    user = names[str];
	    if (user) {
		user = user->query_user();
	    } else {
		user = clone_object(DEFAULT_USER);
	    }
	}
	if (function_object("query_conn", user) != LIB_USER) {
	    error("Invalid user object");
	}
	telnet_users[user]++;
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
    if (previous_program() == LIB_CONN) {
	object user;

	if (binary_manager && str != "admin") {
	    user = binary_manager->select(str);
	    if (!user) {
		return 0;
	    }
	} else {
	    user = names[str];
	    if (user) {
		user = user->query_user();
	    } else {
		user = clone_object(DEFAULT_USER);
	    }
	}
	if (function_object("query_conn", user) != LIB_USER) {
	    error("Invalid user object");
	}
	binary_users[user]++;
	return user;
    }
}

/*
 * NAME:	set_name()
 * DESCRIPTION:	set the name of a connection object
 */
set_name(string name, object conn)
{
    if (previous_program() == LIB_CONN) {
	names[name] = conn;
    }
}

/*
 * NAME:	query_telnet_timeout()
 * DESCRIPTION:	return the current telnet connection timeout
 */
int query_telnet_timeout(string ipnum)
{
    if (previous_program() == LIB_CONN) {
	return (telnet_manager) ?
		telnet_manager->query_timeout(ipnum) : DEFAULT_TIMEOUT;
    }
}

/*
 * NAME:	query_binary_timeout()
 * DESCRIPTION:	return the current binary connection timeout
 */
int query_binary_timeout(string ipnum)
{
    if (previous_program() == LIB_CONN) {
	return (binary_manager) ?
		binary_manager->query_timeout(ipnum) : DEFAULT_TIMEOUT;
    }
}

/*
 * NAME:	query_telnet_banner()
 * DESCRIPTION:	return the current telnet login banner
 */
string query_telnet_banner()
{
    if (previous_program() == LIB_CONN) {
	return (telnet_manager) ?
		telnet_manager->query_banner() :
		"\nDGD " + status()[ST_VERSION] + " (telnet)\n\nlogin: ";
    }
}

/*
 * NAME:	query_binary_banner()
 * DESCRIPTION:	return the current binary login banner
 */
string query_binary_banner()
{
    if (previous_program() == LIB_CONN) {
	return (binary_manager) ?
		binary_manager->query_banner() :
		"\r\nDGD " + status()[ST_VERSION] + " (binary)\r\n\r\nlogin: ";
    }
}

/*
 * NAME:	telnet_disconnect()
 * DESCRIPTION:	disconnect telnet user
 */
telnet_disconnect(object user)
{
    if (previous_program() == LIB_CONN) {
	telnet_users[user]--;
    }
}

/*
 * NAME:	binary_disconnect()
 * DESCRIPTION:	disconnect binary user
 */
binary_disconnect(object user)
{
    if (previous_program() == LIB_CONN) {
	binary_users[user]--;
    }
}


/*
 * NAME:	query_users()
 * DESCRIPTION:	return the current telnet and binary users
 */
object *query_users()
{
    if (previous_program() == AUTO) {
	return map_indices(telnet_users) + map_indices(binary_users);
    }
}

/*
 * NAME:	query_connections()
 * DESCRIPTION:	return the current connections
 */
object *query_connections()
{
    if (previous_program() == API_USER) {
	return users();
    }
}

/*
 * NAME:	query_telnet_users()
 * DESCRIPTION:	return the current telnet users
 */
object *query_telnet_users()
{
    if (previous_program() == API_USER) {
	return map_indices(telnet_users);
    }
}

/*
 * NAME:	query_binary_users()
 * DESCRIPTION:	return the current binary users
 */
object *query_binary_users()
{
    if (previous_program() == API_USER) {
	return map_indices(binary_users);
    }
}

/*
 * NAME:	find_user()
 * DESCRIPTION:	find the user associated with a certain name
 */
object find_user(string name)
{
    if (previous_program() == API_USER) {
	object obj;

	obj = names[name];
	return (obj) ? obj->query_user() : 0;
    }
}


/*
 * NAME:	prepare_reboot()
 * DESCRIPTION:	prepare for a reboot
 */
prepare_reboot()
{
    if (previous_program() == DRIVER) {
	connections = users();
    }
}

/*
 * NAME:	reboot()
 * DESCRIPTION:	handle a reboot
 */
reboot()
{
    if (previous_program() == DRIVER) {
	int i;

	for (i = sizeof(connections); --i >= 0; ) {
	    connections[i]->reboot();
	}
    }
}
