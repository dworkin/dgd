# include <kernel/kernel.h>
# include <kernel/user.h>

object userd;		/* user manager object */
string porttype;	/* telnet or binary */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize port object
 */
static create(int clone)
{
    if (clone) {
	userd = find_object(USERD);
    }
}

/*
 * NAME:	listen()
 * DESCRIPTION:	start listening on a port
 */
listen(string protocol, int port)
{
    if (previous_program() == DRIVER) {
	porttype = (protocol == "tcp") ? "binary" : "telnet";
	catch {
	    open_port(protocol, port);
	} : {
	    previous_object()->message("open_port(" + protocol + ", " + port +
				       ") failed!\n");
	    shutdown();
	}
    }
}

/*
 * NAME:	connection()
 * DESCRIPTION:	return an appropriate connection object
 */
static object connection()
{
    return call_other(userd, porttype + "_connection");
}
