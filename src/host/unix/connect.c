# define INCLUDE_FILE_IO
# include "dgd.h"
# include <sys/time.h>
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
static int telnet;			/* telnet port socket descriptor */
static int binary;			/* binary port socket descriptor */
static fd_set fds;			/* file descriptor bitmap */
static fd_set readfds;			/* file descriptor read bitmap */
static int maxfd;			/* largest fd opened yet */

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connections
 */
void conn_init(maxusers, telnet_port, binary_port)
int maxusers;
unsigned short telnet_port, binary_port;
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

    telnet = socket(host->h_addrtype, SOCK_STREAM, 0);
    binary = socket(host->h_addrtype, SOCK_STREAM, 0);
    if (telnet < 0 || binary < 0) {
	perror("socket");
	exit(2);
    }
    on = 1;
    if (setsockopt(telnet, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	exit(2);
    }
    on = 1;
    if (setsockopt(binary, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	exit(2);
    }

    memset(&sin, '\0', sizeof(sin));
    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
    sin.sin_port = htons(telnet_port);
    sin.sin_family = host->h_addrtype;
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(telnet, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
	perror("bind");
	exit(2);
    }
    sin.sin_port = htons(binary_port);
    if (bind(binary, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
	perror("bind");
	exit(2);
    }

    if (listen(telnet, 5) < 0 || listen(binary, 5) < 0) {
	perror("listen");
	exit(2);
    }

    if (fcntl(telnet, F_SETFL, FNDELAY) < 0 ||
	fcntl(binary, F_SETFL, FNDELAY) < 0) {
	perror("fcntl");
	exit(2);
    }

    connections = ALLOC(connection, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = -1;
	conn->next = flist;
	flist = conn;
    }

    FD_ZERO(&fds);
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish()
{
    close(telnet);
    close(binary);
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew()
{
    int fd, len;
    struct sockaddr_in sin;
    register connection *conn;

    len = sizeof(sin);
    fd = accept(telnet, (struct sockaddr *) &sin, &len);
    if (fd < 0) {
	return (connection *) NULL;
    }

    conn = flist;
    flist = conn->next;
    conn->fd = fd;
    memcpy(&conn->addr, (char *) &sin, len);
    FD_SET(fd, &fds);
    if (fd > maxfd) {
	maxfd = fd;
    }

    return conn;
}

/*
 * NAME:	conn->bnew()
 * DESCRIPTION:	accept a new binary connection
 */
connection *conn_bnew()
{
    int fd, len;
    struct sockaddr_in sin;
    register connection *conn;

    len = sizeof(sin);
    fd = accept(binary, (struct sockaddr *) &sin, &len);
    if (fd < 0) {
	return (connection *) NULL;
    }

    conn = flist;
    flist = conn->next;
    conn->fd = fd;
    memcpy(&conn->addr, (char *) &sin, len);
    FD_SET(fd, &fds);
    if (fd > maxfd) {
	maxfd = fd;
    }

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
	FD_CLR(conn->fd, &fds);
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
    struct timeval timeout;

    memcpy(&readfds, &fds, sizeof(fd_set));
    timeout.tv_sec = (int) wait;
    timeout.tv_usec = 0;
    return select(maxfd + 1, &readfds, (fd_set *) NULL, (fd_set *) NULL,
		  &timeout);
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
    if (!FD_ISSET(conn->fd, &readfds)) {
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
    if (conn->fd >= 0) {
	if (write(conn->fd, buf, size) < 0 && errno != EWOULDBLOCK) {
	    close(conn->fd);
	    FD_CLR(conn->fd, &fds);
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
