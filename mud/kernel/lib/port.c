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
    porttype = (protocol == "telnet") ? "telnet" : "binary";
    catch {
	::open_port(protocol, port);
	return 1;
    } : {
	return 0;
    }
}

static object open_connection();

/*
 * NAME:	connection()
 * DESCRIPTION:	return an appropriate connection object
 */
static nomask object connection()
{
    if (!previous_program()) {
	object conn, user;

	conn = call_other(userd, porttype + "_connection");
	user = open_connection();
	if (user) {
	    conn->set_user(user);
	}
	return conn;
    }
}
