# include <kernel/kernel.h>
# include <kernel/user.h>
# include <kernel/net.h>

private object driver;		/* driver object */
private object userd;		/* user manager object */
private string porttype;	/* telnet or binary */
private object udpport;		/* optional associated UDP port */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize port object
 */
static void create()
{
    driver = find_object(DRIVER);
    userd = find_object(USERD);
}

/*
 * NAME:	open_port()
 * DESCRIPTION:	start listening on a port
 */
static int open_port(string protocol, int port, varargs int udp)
{
    rlimits (-1; -1) {
	catch {
	    ::open_port(protocol, port);
	    porttype = (protocol == "telnet") ? "telnet" : "binary";
	    if (protocol == "tcp" && udp) {
		udpport = clone_object(PORT_UDP);
		catch {
		    udpport->listen(port);
		} : {
		    destruct_object(udpport);
		    return 0;
		}
	    }
	    return 1;
	} : {
	    return 0;
	}
    }
}

/*
 * NAME:	close()
 * DESCRIPTION:	close associated UDP port, if there is any
 */
static void close()
{
    if (udpport) {
	rlimits (-1; -1) {
	    destruct_object(udpport);
	}
    }
}


static object open_connection(string host, int port);

/*
 * NAME:	_connection()
 * DESCRIPTION:	internal version of connection()
 */
private object _connection(mixed *tls, string host, int port)
{
    object conn, user;

    conn = call_other(userd, porttype + "_connection");
    if (udpport) {
	conn->set_udpchannel(udpport, host, port);
    }
    user = open_connection(host, port);
    if (user) {
	if (function_object("query_conn", user) != LIB_USER) {
	    error("Invalid user object");
	}
	conn->set_user(user, nil);
    }
    return conn;
}

/*
 * NAME:	connection()
 * DESCRIPTION:	return an appropriate connection object
 */
static nomask object connection(string host, int port)
{
    if (!previous_program()) {
	return _connection(allocate(driver->query_tls_size()), host, port);
    }
}


static void recv_datagram(string str, string host, int port);

/*
 * NAME:	_receive_datagram()
 * DESCRIPTION:	internal version of receive_datagram()
 */
private void
_receive_datagram(mixed *tls, string str, string host, int port)
{
    recv_datagram(str, host, port);
}

/*
 * NAME:	receive_datagram()
 * DESCRIPTION:	receive datagram
 */
static nomask void receive_datagram(string str, string host, int port)
{
    if (!previous_program()) {
	_receive_datagram(allocate(driver->query_tls_size()), str, host, port);
    }
}

/*
 * NAME:	datagram()
 * DESCRIPTION:	send a datagram
 */
static int datagram(string str, string host, int port)
{
    return send_datagram(str, host, port);
}
