# include <kernel/kernel.h>
# include <kernel/user.h>

private object connection;	/* associated connection object */
private string name;		/* name of user */


/*
 * NAME:	query_conn()
 * DESCRIPTION:	query the associated connection
 */
nomask object query_conn()
{
    return connection;
}

/*
 * NAME:	disconnect()
 * DESCRIPTION:	terminate the connection
 */
static void disconnect()
{
    if (connection) {
	connection->disconnect();
    }
}

/*
 * NAME:	connect()
 * DESCRIPTION:	establish connection
 */
static void connect(object conn)
{
    disconnect();
    connection = conn;
}

/*
 * NAME:	redirect()
 * DESCRIPTION:	direct connection to a different user object
 */
static void redirect(object user, string str)
{
    object conn;

    if (!connection || function_object("query_conn", user) != LIB_USER) {
	error("Bad redirect");
    }
    conn = connection;
    connection = nil;
    conn->set_user(user, str);
}

/*
 * NAME:	login()
 * DESCRIPTION:	log this user in
 */
static void login(string str)
{
    if (!name || name == str) {
	USERD->login(this_object(), name = str);
    }
}

/*
 * NAME:	logout()
 * DESCRIPTION:	logout this user
 */
static void logout()
{
    USERD->logout(this_object(), name);
}

/*
 * NAME:	query_name()
 * DESCRIPTION:	return this user's name (if any)
 */
string query_name()
{
    return name;
}

/*
 * NAME:	message()
 * DESCRIPTION:	forward a message to the connection object
 */
int message(string str)
{
    if (!str) {
	error("Bad argument 1 for function message");
    }
    if (connection) {
	return connection->message(str);
    }
    return 0;
}

# ifdef SYS_DATAGRAMS
/*
 * NAME:	datagram()
 * DESCRIPTION:	forward a datagram to the connection object
 */
int datagram(string str)
{
    if (!str) {
	error("Bad argument 1 for function datagram");
    }
    if (connection) {
	return connection->datagram(str);
    }
    return 0;
}
# endif
