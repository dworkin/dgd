# include <kernel/kernel.h>
# include <kernel/user.h>
# ifdef SYS_NETWORKING
#  include <kernel/net.h>
#  define PORT	LIB_PORT
# else
#  define PORT	DRIVER
# endif
# include <status.h>


object *users;				/* user mappings */
object telnet_manager, binary_manager;	/* user object managers */
mapping names;				/* name : connection object */
object *connections;			/* saved connections */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize object
 */
static void create()
{
    /* load essential objects */
    if (!find_object(TELNET_CONN)) { compile_object(TELNET_CONN); }
    if (!find_object(BINARY_CONN)) { compile_object(BINARY_CONN); }
    if (!find_object(DEFAULT_USER)) { compile_object(DEFAULT_USER); }

    /* initialize user arrays */
    users = ({ });
    names = ([ ]);
}

/*
 * NAME:	telnet_connection()
 * DESCRIPTION:	return a new telnet connection object
 */
object telnet_connection(int port)
{
    if (previous_program() == PORT) {
	object conn;

	conn = clone_object(TELNET_CONN);
	conn->set_port(port);
	return conn;
    }
}

/*
 * NAME:	binary_connection()
 * DESCRIPTION:	return a new binary connection object
 */
object binary_connection(int port)
{
    if (previous_program() == PORT) {
	object conn;

	conn = clone_object(BINARY_CONN);
	conn->set_port(port);
	return conn;
    }
}

/*
 * NAME:	set_telnet_manager()
 * DESCRIPTION:	set the telnet manager object, which determines what the
 *		user object is, based on the first line of input
 */
void set_telnet_manager(object manager)
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
void set_binary_manager(object manager)
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

	user = names[str];
	if (!user) {
	    if (telnet_manager) {
		user = telnet_manager->select(str);
		if (function_object("query_conn", user) != LIB_USER) {
		    error("Invalid user object");
		}
	    } else {
		user = clone_object(DEFAULT_USER);
	    }
	}
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

	user = names[str];
	if (!user) {
	    if (binary_manager &&
		(str != "admin" || previous_object()->query_port() != 0)) {
		user = binary_manager->select(str);
		if (function_object("query_conn", user) != LIB_USER) {
		    error("Invalid user object");
		}
	    } else {
		user = clone_object(DEFAULT_USER);
	    }
	}
	return user;
    }
}

/*
 * NAME:	query_telnet_timeout()
 * DESCRIPTION:	return the current telnet connection timeout
 */
int query_telnet_timeout()
{
    if (previous_program() == LIB_CONN) {
	return (telnet_manager) ?
		telnet_manager->query_timeout(previous_object()) :
		DEFAULT_TIMEOUT;
    }
}

/*
 * NAME:	query_binary_timeout()
 * DESCRIPTION:	return the current binary connection timeout
 */
int query_binary_timeout()
{
    if (previous_program() == LIB_CONN) {
	return (binary_manager) ?
		binary_manager->query_timeout(previous_object()) :
		DEFAULT_TIMEOUT;
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
		telnet_manager->query_banner(previous_object()) :
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
		binary_manager->query_banner(previous_object()) :
		"\r\nDGD " + status()[ST_VERSION] + " (binary)\r\n\r\nlogin: ";
    }
}


/*
 * NAME:	login()
 * DESCRIPTION:	login user
 */
void login(object user, string name)
{
    if (previous_program() == LIB_USER) {
	users = (users - ({ nil })) | ({ user });
	names[name] = user;
    }
}

/*
 * NAME:	logout()
 * DESCRIPTION:	log user out
 */
void logout(object user, string name)
{
    if (previous_program() == LIB_USER) {
	users -= ({ user });
	names[name] = nil;
    }
}


/*
 * NAME:	query_users()
 * DESCRIPTION:	return the current telnet and binary users
 */
object *query_users()
{
    if (previous_program() == AUTO) {
	object *usr;
	int i, changed;

	usr = users - ({ nil });
	changed = FALSE;
	for (i = sizeof(usr); --i >= 0; ) {
	    if (!usr[i]->query_conn()) {
		usr[i] = nil;
		changed = TRUE;
	    }
	}
	return (changed) ? usr - ({ nil }) : usr;
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
 * NAME:	find_user()
 * DESCRIPTION:	find the user associated with a certain name
 */
object find_user(string name)
{
    if (previous_program() == API_USER) {
	return names[name];
    }
}


/*
 * NAME:	prepare_reboot()
 * DESCRIPTION:	prepare for a reboot
 */
void prepare_reboot()
{
    if (previous_program() == DRIVER) {
	connections = users();
    }
}

/*
 * NAME:	reboot()
 * DESCRIPTION:	handle a reboot
 */
void reboot()
{
    if (previous_program() == DRIVER) {
	int i;

	for (i = sizeof(connections); --i >= 0; ) {
	    connections[i]->reboot();
	}

	connections = nil;
	users = ({ });
	names = ([ ]);
    }
}
