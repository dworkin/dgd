# include "dgd.h"
# include <sys/time.h>
# include <fcntl.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <errno.h>
# include "str.h"
# include "array.h"
# include "object.h"
# include "comm.h"

struct _connection_ {
    int fd;				/* file descriptor */
    struct sockaddr_in addr;		/* internet address of connection */
    struct _connection_ *next;		/* next in list */
};

static int nusers;			/* # of users */
static connection *connections;		/* connections array */
static connection *flist;		/* list of free connections */
static int tcpip;			/* TCP/IP socket descriptor */
static fd_set fds;			/* socket file destriptors */

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connections
 */
void conn_init(maxusers, port)
int maxusers;
unsigned short port;
{
    struct sockaddr_in sin;
    struct hostent *host;
    register int n;
    register connection *conn;
    char buffer[256];
    int on;

    gethostname(buffer, sizeof(buffer));
    host = gethostbyname(buffer);
    if (host == (struct hostent *) NULL) {
	perror("gethostbyname");
	exit(2);
    }

    tcpip = socket(host->h_addrtype, SOCK_STREAM, 0);
    if (tcpip < 0) {
	perror("socket");
	exit(2);
    }
    on = 1;
    if (setsockopt(tcpip, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	exit(2);
    }

    memset(&sin, '\0', sizeof(sin));
    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
    sin.sin_port = htons(port);
    sin.sin_family = host->h_addrtype;
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(tcpip, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
	perror("bind");
	exit(2);
    }

    if (listen(tcpip, 5) < 0) {
	perror("listen");
	exit(2);
    }

    if (fcntl(tcpip, F_SETFL, FNDELAY) < 0) {
	perror("fcntl");
	exit(2);
    }

    connections = ALLOC(connection, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = -1;
	conn->next = flist;
	flist = conn;
    }
}

/*
 * NAME:	conn->new()
 * DESCRIPTION:	accept a new connection
 */
connection *conn_new()
{
    int fd, len;
    struct sockaddr_in sin;
    register connection *conn;

    len = sizeof(sin);
    fd = accept(tcpip, (struct sockaddr *) &sin, &len);
    if (fd < 0) {
	return (connection *) NULL;
    }

    conn = flist;
    flist = conn->next;
    conn->fd = fd;
    memcpy(&conn->addr, (char *) &sin, len);

    return conn;
}

/*
 * NAME:	conn->del()
 * DESCRIPTION:	delete a connection
 */
void conn_del(conn)
register connection *conn;
{
    if (conn->fd >= 0) {
	close(conn->fd);
	conn->fd = -1;
    }
    conn->next = flist;
    flist = conn;
}

/*
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(wait)
bool wait;
{
    register int i, max;
    register connection *conn;
    struct timeval timeout;

    FD_ZERO(&fds);
    max = 0;
    for (i = nusers, conn = connections; i > 0; --i, conn++) {
	if (conn->fd >= 0) {
	    FD_SET(conn->fd, &fds);
	    if (conn->fd > max) {
		max = conn->fd;
	    }
	}
    }

    timeout.tv_sec = (int) wait;
    timeout.tv_usec = 0;
    return select(max + 1, &fds, (fd_set *) NULL, (fd_set *) NULL, &timeout);
}

/*
 * NAME:	conn->read()
 * DESCRIPTION:	read from a connection
 */
int conn_read(conn, buf, size)
connection *conn;
char *buf;
int size;
{
    if (conn->fd < 0) {
	return -1;
    }
    if (!FD_ISSET(conn->fd, &fds)) {
	return 0;
    }
    size = read(conn->fd, buf, size);
    return (size == 0) ? -1 : size;
}

/*
 * NAME:	conn->write()
 * DESCRIPTION:	write to a connection
 */
void conn_write(conn, buf, size)
connection *conn;
char *buf;
register int size;
{
    char buffer[2 * OUTBUF_SIZE];
    register char *p, *q;

    if (conn->fd >= 0) {
	for (p = buf, q = buffer; size > 0; --size) {
	    if (*p == '\n') {
		*q++ = '\r';
	    }
	    *q++ = *p++;
	}
	if (write(conn->fd, buffer, q - buffer) < 0 && errno != EWOULDBLOCK) {
	    close(conn->fd);
	    conn->fd = -1;
	}
    }
}

/*
 * NAME:	conn->ipnum()
 * DESCRIPTION:	return the ip number of a connection
 */
char *conn_ipnum(conn)
connection *conn;
{
    return inet_ntoa(conn->addr.sin_addr);
}
