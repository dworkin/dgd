# include <kernel/kernel.h>
# include <kernel/user.h>

private object userd;		/* user manager object */
private string porttype;	/* telnet or binary */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize port object
 */
static create()
{
    userd = find_object(USERD);
}

/*
 * NAME:	open_port()
 * DESCRIPTION:	start listening on a port
 */
static int open_port(string protocol, int port)
{
    catch {
	::open_port(protocol, port);
	porttype = (protocol == "telnet") ? "telnet" : "binary";
	return 1;
    } : {
	return 0;
    }
}

static object open_connection(string ipaddr, int port);

/*
 * NAME:	_connection()
 * DESCRIPTION:	internal version of connection()
 */
private object _connection(mixed *tls, string ipaddr, int port)
{
    object conn, user;

    conn = call_other(userd, porttype + "_connection");
    user = open_connection(ipaddr, port);
    if (user) {
	if (function_object("query_conn", user) != LIB_USER) {
	    error("Invalid user object");
	}
	conn->set_user(user);
    }
    return conn;
}

/*
 * NAME:	connection()
 * DESCRIPTION:	return an appropriate connection object
 */
static nomask object connection(string ipaddr, int port)
{
    if (!previous_program()) {
	return _connection(allocate(TLS_SIZE), ipaddr, port);
    }
}
