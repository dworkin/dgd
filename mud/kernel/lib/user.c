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
 * NAME:	connection()
 * DESCRIPTION:	establish connection
 */
static void connection(object LIB_CONN conn)
{
    if (conn) {
	disconnect();
	connection = conn;
    }
}

/*
 * NAME:	redirect()
 * DESCRIPTION:	direct connection to a different user object
 */
static void redirect(object LIB_USER user, string str)
{
    object conn;

    if (!user || !connection) {
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

/*
 * NAME:	message_done()
 * DESCRIPTION:	placeholder function which does no buffering
 */
int message_done()
{
    return MODE_NOCHANGE;
}

/*
 * NAME:	datagram_challenge()
 * DESCRIPTION:	set the challenge for the datagram channel
 */
static void datagram_challenge(string str)
{
    if (!str) {
	error("Bad argument 1 for function datagram_challenge");
    }
    if (connection) {
	connection->datagram_challenge(str);
    }
}

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
