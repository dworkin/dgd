# include <kernel/kernel.h>
# include <kernel/user.h>

private object connection;	/* associated connection object */


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
 * DESCRIPTION:	establish a connection
 */
static connect(object conn)
{
    disconnect();
    connection = conn;
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
