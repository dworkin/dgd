# include <kernel/kernel.h>
# include <kernel/user.h>

private object connection;

/*
 * NAME:	set_conn()
 * DESCRIPTION:	set the associated connection for this user object
 */
nomask set_conn(object conn)
{
    if (KERNEL()) {
	connection = conn;
	subscribe_event(conn, "login");
	subscribe_event(conn, "logout");
	subscribe_event(conn, "message");
    }
}

/*
 * NAME:	disconnect()
 * DESCRIPTION:	terminate the connection
 */
static disconnect()
{
    destruct_object(connection);
}

/*
 * NAME:	message()
 * DESCRIPTION:	forward a message to the connection object
 */
message(mixed arg)
{
    if (connection) {
	connection->message(arg);
    }
}
