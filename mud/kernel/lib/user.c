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
static disconnect()
{
    if (connection) {
	destruct_object(connection);
    }
}

/*
 * NAME:	connect()
 * DESCRIPTION:	establish connection
 */
static connect(object conn)
{
    disconnect();
    connection = conn;
}

/*
 * NAME:	login()
 * DESCRIPTION:	log this user in
 */
static login(string str)
{
    if (!name || name == str) {
	USERD->login(this_object(), name = str);
    }
}

/*
 * NAME:	logout()
 * DESCRIPTION:	logout this user
 */
static logout()
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
message(string arg)
{
    if (connection) {
	connection->message(arg);
    }
}
