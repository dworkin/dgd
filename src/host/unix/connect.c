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
static fd_set waitfds;			/* file descriptor wait-write bitmap */
static fd_set readfds;			/* file descriptor read bitmap */
static fd_set writefds;			/* file descriptor write map */
static int maxfd;			/* largest fd opened yet */

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connection handling
 */
bool conn_init(maxusers, telnet_port, binary_port)
int maxusers;
unsigned int telnet_port, binary_port;
{
    struct sockaddr_in sin;
    struct hostent *host;
    register int n;
    register connection *conn;
    char buffer[256];
    int on;

    nusers = 0;

    gethostname(buffer, sizeof(buffer));
    host = gethostbyname(buffer);
    if (host == (struct hostent *) NULL) {
	perror("gethostbyname");
	return FALSE;
    }

    telnet = socket(host->h_addrtype, SOCK_STREAM, 0);
    binary = socket(host->h_addrtype, SOCK_STREAM, 0);
    if (telnet < 0 || binary < 0) {
	perror("socket");
	return FALSE;
    }
    on = 1;
    if (setsockopt(telnet, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
# ifdef SO_OOBINLINE
    on = 1;
    if (setsockopt(telnet, SOL_SOCKET, SO_OOBINLINE, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
# endif
    on = 1;
    if (setsockopt(binary, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
# ifdef SO_OOBINLINE
    on = 1;
    if (setsockopt(binary, SOL_SOCKET, SO_OOBINLINE, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
# endif

    memset(&sin, '\0', sizeof(sin));
    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
    sin.sin_port = htons(telnet_port);
    sin.sin_family = host->h_addrtype;
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(telnet, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
	perror("telnet bind");
	return FALSE;
    }
    sin.sin_port = htons(binary_port);
    if (bind(binary, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
	perror("binary bind");
	return FALSE;
    }

    flist = (connection *) NULL;
    connections = ALLOC(connection, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = -1;
	conn->next = flist;
	flist = conn;
    }

    FD_ZERO(&fds);
    FD_ZERO(&waitfds);
    FD_SET(telnet, &fds);
    FD_SET(binary, &fds);
    maxfd = (telnet < binary) ? binary : telnet;

    return TRUE;
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish()
{
    register int n;
    register connection *conn;

    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	if (conn->fd >= 0) {
	    close(conn->fd);
	}
    }
    close(telnet);
    close(binary);
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen()
{
    if (listen(telnet, 64) < 0 || listen(binary, 64) < 0) {
	perror("listen");
    } else if (fcntl(telnet, F_SETFL, FNDELAY) < 0 ||
	       fcntl(binary, F_SETFL, FNDELAY) < 0) {
	perror("fcntl");
    } else {
	return;
    }
    fatal("conn_listen failed");
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

    if (!FD_ISSET(telnet, &readfds)) {
	return (connection *) NULL;
    }
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
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);
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

    if (!FD_ISSET(binary, &readfds)) {
	return (connection *) NULL;
    }
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
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);
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
	FD_CLR(conn->fd, &waitfds);
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
int wait;
{
    struct timeval timeout;
    int retval;

    /*
     * First, check readability and writability for binary sockets with pending
     * data only.
     */
    memcpy(&readfds, &fds, sizeof(fd_set));
    memcpy(&writefds, &waitfds, sizeof(fd_set));
    timeout.tv_sec = (int) wait;
    timeout.tv_usec = 0;
    retval = select(maxfd + 1, &readfds, &writefds, (fd_set *) NULL, &timeout);
    if (retval < 0) {
	FD_ZERO(&readfds);
    }
    /*
     * Now check writability for all sockets in a polling call.
     */
    memcpy(&writefds, &fds, sizeof(fd_set));
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    select(maxfd + 1, (fd_set *) NULL, &writefds, (fd_set *) NULL, &timeout);
    return retval;
}

/*
 * NAME:	conn->read()
 * DESCRIPTION:	read from a connection
 */
int conn_read(conn, buf, len)
connection *conn;
char *buf;
unsigned int len;
{
    int size;

    if (conn->fd < 0) {
	return -1;
    }
    if (!FD_ISSET(conn->fd, &readfds)) {
	return 0;
    }
    size = read(conn->fd, buf, len);
    return (size == 0) ? -1 : size;
}

/*
 * NAME:	conn->write()
 * DESCRIPTION:	write to a connection; return the amount of bytes written
 */
int conn_write(conn, buf, len)
register connection *conn;
char *buf;
unsigned int len;
{
    int size;

    if (conn->fd >= 0) {
	FD_CLR(conn->fd, &waitfds);
	if (len == 0) {
	    return 0;	/* send_message("") can be used to flush buffer */
	}
	if (!FD_ISSET(conn->fd, &writefds)) {
	    /* the write would fail */
	    FD_SET(conn->fd, &waitfds);
	    return -1;
	}
	if ((size=write(conn->fd, buf, len)) < 0 && errno != EWOULDBLOCK) {
	    close(conn->fd);
	    FD_CLR(conn->fd, &fds);
	    conn->fd = -1;
	} else if (size != len) {
	    /* waiting for wrdone */
	    FD_SET(conn->fd, &waitfds);
	    FD_CLR(conn->fd, &writefds);
	}
	return size;
    }
    return 0;
}

/*
 * NAME:	conn->wrdone()
 * DESCRIPTION:	return TRUE if a connection is ready for output
 */
bool conn_wrdone(conn)
connection *conn;
{
    if (conn->fd < 0 || !FD_ISSET(conn->fd, &waitfds)) {
	return TRUE;
    }
    if (FD_ISSET(conn->fd, &writefds)) {
	FD_CLR(conn->fd, &waitfds);
	return TRUE;
    }
    return FALSE;
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
