# include <kernel/kernel.h>
# include <kernel/user.h>
# include <kernel/net.h>

inherit LIB_PORT;


object driver;		/* driver object */
mapping connection;	/* address->connection mapping */
mapping address;	/* connection->address mapping */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize port object
 */
static void create(int clone)
{
    if (clone) {
	::create();
	driver = find_object(DRIVER);
	connection = ([ ]);
	address = ([ ]);
    }
}

/*
 * NAME:	listen()
 * DESCRIPTION:	listen on a UDP port
 */
void listen(int port)
{
    if (previous_program() == LIB_PORT) {
	open_port("udp", port);
    }
}

/*
 * NAME:	add_connection()
 * DESCRIPTION:	add connection object to the mapping
 */
void add_connection(object conn, string host, int port)
{
    if (previous_program() == BINARY_CONN) {
	host += ":" + port;
	connection[host] = conn;
	address[conn] = host;
    }
}

/*
 * NAME:	recv_datagram()
 * DESCRIPTION:	receive a datagram
 */
static void recv_datagram(string str, string host, int port)
{
    object conn;

    conn = connection[host + ":" + port];
    if (conn) {
	conn->receive_datagram(allocate(driver->query_tls_size()), str);
    }
}

/*
 * NAME:	datagram()
 * DESCRIPTION:	transmit a datagram
 */
int datagram(string str)
{
    if (previous_program() == BINARY_CONN) {
	string host;
	int port;

	host = address[previous_object()];
	if (host) {
	    sscanf(host, "%s:%d", host, port);
	    return ::datagram(str, host, port);
	}
    }
}
