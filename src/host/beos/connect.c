# include <kernel/OS.h>
# include <sys/time.h>
# include <socket.h>
# include <netdb.h>
# include <errno.h>
# define INCLUDE_FILE_IO
# include "dgd.h"
# include "comm.h"

# define NFREE		32

typedef struct _ipaddr_ {
    struct _ipaddr_ *link;		/* next in hash table */
    struct _ipaddr_ *prev;		/* previous in linked list */
    struct _ipaddr_ *next;		/* next in linked list */
    Uint ref;				/* reference count */
    struct in_addr ipnum;		/* ip number */
    char name[MAXHOSTNAMELEN];		/* ip name */
} ipaddr;

static int in = -1, out = -1;		/* name resolver sockets */
static ipaddr **ipahtab;		/* ip address hash table */
static unsigned int ipahtabsz;		/* hash table size */
static ipaddr *qhead, *qtail;		/* request queue */
static ipaddr *ffirst, *flast;		/* free list */
static int nfree;			/* # in free list */
static ipaddr *lastreq;			/* last request */
static bool busy;			/* name resolver busy */


/*
 * NAME:	ipaddr->run()
 * DESCRIPTION:	host name lookup sub-program
 */
static int32 ipa_run(void *arg)
{
    char buf[sizeof(struct in_addr)];
    struct hostent *host;
    char hname[MAXHOSTNAMELEN + 1];
    int len;

    while (recv(out, buf, sizeof(struct in_addr), 0) > 0) {
	/* lookup host */
	host = gethostbyaddr(buf, sizeof(struct in_addr), AF_INET);
	if (host == (struct hostent *) NULL) {
	    sleep(2);
	    host = gethostbyaddr(buf, sizeof(struct in_addr), AF_INET);
	}

	if (host != (struct hostent *) NULL) {
	    /* send host name */
	    len = strlen(host->h_name);
	    if (len >= MAXHOSTNAMELEN) {
		len = MAXHOSTNAMELEN - 1;
	    }
	    memcpy(hname + 1, host->h_name, hname[0] = len);
	    send(out, hname, len + 1, 0);
	} else {
	    send(out, "\1", 2, 0);	/* failure */
	}
    }

    return 0;
}

/*
 * NAME:	ipaddr->init()
 * DESCRIPTION:	initialize name lookup
 */
static bool ipa_init(int maxusers)
{
    if (in < 0) {
	in = socket(AF_INET, SOCK_STREAM, 0);
	if (in < 0) {
	    return FALSE;
	}
    } else if (busy) {
	char buf[MAXHOSTNAMELEN + 2];

	/* discard ip name */
	recv(in, buf, MAXHOSTNAMELEN + 2, 0);
    }

    ipahtab = ALLOC(ipaddr*, ipahtabsz = maxusers);
    memset(ipahtab, '\0', ipahtabsz * sizeof(ipaddr*));
    qhead = qtail = ffirst = flast = lastreq = (ipaddr *) NULL;
    nfree = 0;
    busy = FALSE;

    return TRUE;
}

/*
 * NAME:	ipaddr->start()
 * DESCRIPTION:	start name resolver thread
 */
static void ipa_start(int sock)
{
    if (out < 0) {
	struct sockaddr_in addr;
	int len;

	len = sizeof(struct sockaddr_in);
	getsockname(sock, (struct sockaddr *) &addr, &len);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	connect(in, (struct sockaddr *) &addr, len);
	out = accept(sock, (struct sockaddr *) &addr, &len);
	resume_thread(spawn_thread(ipa_run, "name_lookup", B_NORMAL_PRIORITY,
				   NULL));
    }
}

/*
 * NAME:	ipaddr->finish()
 * DESCRIPTION:	stop name lookup
 */
static void ipa_finish(void)
{
}

/*
 * NAME:	ipaddr->new()
 * DESCRIPTION:	return a new ipaddr
 */
static ipaddr *ipa_new(struct in_addr *ipnum)
{
    ipaddr *ipa, **hash;

    /* check hash table */
    hash = &ipahtab[(Uint) ipnum->s_addr % ipahtabsz];
    while (*hash != (ipaddr *) NULL) {
	ipa = *hash;
	if (ipnum->s_addr == ipa->ipnum.s_addr) {
	    /*
	     * found it
	     */
	    if (ipa->ref == 0) {
		/* remove from free list */
		if (ipa->prev == (ipaddr *) NULL) {
		    ffirst = ipa->next;
		} else {
		    ipa->prev->next = ipa->next;
		}
		if (ipa->next == (ipaddr *) NULL) {
		    flast = ipa->prev;
		} else {
		    ipa->next->prev = ipa->prev;
		}
		ipa->prev = ipa->next = (ipaddr *) NULL;
		--nfree;
	    }
	    ipa->ref++;

	    if (ipa->name[0] == '\0' && ipa != lastreq &&
		ipa->prev == (ipaddr *) NULL && ipa != qhead) {
		if (!busy) {
		    /* send query to name resolver */
		    send(in, (char *) ipnum, sizeof(struct in_addr), 0);
		    lastreq = ipa;
		    busy = TRUE;
		} else {
		    /* put in request queue */
		    ipa->prev = qtail;
		    if (qtail == (ipaddr *) NULL) {
			qhead = ipa;
		    } else {
			qtail->next = ipa;
		    }
		    qtail = ipa;
		}
	    }
	    return ipa;
	}
	hash = &ipa->link;
    }

    if (nfree >= NFREE) {
	ipaddr **h;

	/*
	 * use first ipaddr in free list
	 */
	ipa = ffirst;
	ffirst = ipa->next;
	ffirst->prev = (ipaddr *) NULL;
	--nfree;

	if (ipa == lastreq) {
	    lastreq = (ipaddr *) NULL;
	}

	if (hash != &ipa->link) {
	    /* remove from hash table */
	    for (h = &ipahtab[(Uint) ipa->ipnum.s_addr % ipahtabsz];
		 *h != ipa;
		 h = &(*h)->link) ;
	    *h = ipa->link;

	    /* put in hash table */
	    ipa->link = *hash;
	    *hash = ipa;
	}
    } else {
	/*
	 * allocate new ipaddr
	 */
	m_static();
	ipa = ALLOC(ipaddr, 1);
	m_dynamic();

	/* put in hash table */
	ipa->link = *hash;
	*hash = ipa;
    }

    ipa->ref = 1;
    ipa->ipnum = *ipnum;
    ipa->name[0] = '\0';
    ipa->prev = ipa->next = (ipaddr *) NULL;

    if (!busy) {
	/* send query to name resolver */
	send(in, (char *) ipnum, sizeof(struct in_addr), 0);
	lastreq = ipa;
	busy = TRUE;
    } else {
	/* put in request queue */
	ipa->prev = qtail;
	if (qtail == (ipaddr *) NULL) {
	    qhead = ipa;
	} else {
	    qtail->next = ipa;
	}
	qtail = ipa;
    }

    return ipa;
}

/*
 * NAME:	ipaddr->del()
 * DESCRIPTION:	delete an ipaddr
 */
static void ipa_del(ipaddr *ipa)
{
    if (--ipa->ref == 0) {
	if (ipa->prev != (ipaddr *) NULL || qhead == ipa) {
	    /* remove from queue */
	    if (ipa->prev != (ipaddr *) NULL) {
		ipa->prev->next = ipa->next;
	    } else {
		qhead = ipa->next;
	    }
	    if (ipa->next != (ipaddr *) NULL) {
		ipa->next->prev = ipa->prev;
	    } else {
		qtail = ipa->prev;
	    }
	}

	/* add to free list */
	if (flast != (ipaddr *) NULL) {
	    flast->next = ipa;
	    ipa->prev = flast;
	    flast = ipa;
	} else {
	    ffirst = flast = ipa;
	    ipa->prev = (ipaddr *) NULL;
	}
	ipa->next = (ipaddr *) NULL;
	nfree++;
    }
}

/*
 * NAME:	ipaddr->lookup()
 * DESCRIPTION:	lookup another ip name
 */
static void ipa_lookup()
{
    ipaddr *ipa;

    if (lastreq != (ipaddr *) NULL) {
	unsigned char len;

	/* read ip name */
	recv(in, &len, 1, 0);
	if (len == 0) {
	    return;	/* interrupt */
	}
	recv(in, lastreq->name, len, 0);
	lastreq->name[len] = '\0';
    } else {
	char buf[MAXHOSTNAMELEN + 2];

	/* discard ip name */
	recv(in, buf, MAXHOSTNAMELEN + 2, 0);
    }

    /* if request queue not empty, write new query */
    if (qhead != (ipaddr *) NULL) {
	ipa = qhead;
	send(in, (char *) &ipa->ipnum, sizeof(struct in_addr), 0);
	qhead = ipa->next;
	if (qhead == (ipaddr *) NULL) {
	    qtail = (ipaddr *) NULL;
	} else {
	    qhead->prev = (ipaddr *) NULL;
	}
	ipa->prev = ipa->next = (ipaddr *) NULL;
	lastreq = ipa;
	busy = TRUE;
    } else {
	lastreq = (ipaddr *) NULL;
	busy = FALSE;
    }
}


struct _connection_ {
    int fd;				/* file descriptor */
    int bufsz;				/* # bytes in buffer */
    char *udpbuf;			/* datagram buffer */
    ipaddr *addr;			/* internet address of connection */
    unsigned short port;		/* port of connection */
    struct _connection_ *next;		/* next in list */
};

static int nusers;			/* # of users */
static connection *connections;		/* connections array */
static connection *flist;		/* list of free connections */
static connection **udphtab;		/* UDP hash table */
static int udphtabsz;			/* UDP hash table size */
static int telnet;			/* telnet port socket descriptor */
static int binary;			/* binary port socket descriptor */
static int udp;				/* UDP port socket descriptor */
static fd_set infds;			/* file descriptor input bitmap */
static fd_set outfds;			/* file descriptor output bitmap */
static fd_set waitfds;			/* file descriptor wait-write bitmap */
static fd_set readfds;			/* file descriptor read bitmap */
static fd_set writefds;			/* file descriptor write map */
static int maxfd;			/* largest fd opened yet */
static int closed;			/* #fds closed in write */

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connection handling
 */
bool conn_init(int maxusers, unsigned int telnet_port, unsigned int binary_port)
{
    struct sockaddr_in sin;
    connection *conn;
    int n, on;

    if (!ipa_init(maxusers)) {
	return FALSE;
    }

    nusers = 0;

    telnet = socket(AF_INET, SOCK_STREAM, 0);
    binary = socket(AF_INET, SOCK_STREAM, 0);
    udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (telnet < 0 || binary < 0 || udp < 0) {
	perror("socket");
	return FALSE;
    }
    on = TRUE;
    if (setsockopt(telnet, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
    on = TRUE;
    if (setsockopt(binary, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
    on = TRUE;
    if (setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0)
    {
	perror("setsockopt");
	return FALSE;
    }

    memset(&sin, '\0', sizeof(sin));
    sin.sin_port = htons(telnet_port);
    sin.sin_family = AF_INET;
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
    if (bind(udp, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
	perror("udp bind");
	return FALSE;
    }

    flist = (connection *) NULL;
    connections = ALLOC(connection, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = -1;
	conn->next = flist;
	flist = conn;
    }

    udphtab = ALLOC(connection*, udphtabsz = maxusers);
    memset(udphtab, '\0', udphtabsz * sizeof(connection*));

    FD_ZERO(&infds);
    FD_ZERO(&outfds);
    FD_ZERO(&waitfds);
    FD_SET(telnet, &infds);
    FD_SET(binary, &infds);
    FD_SET(udp, &infds);
    FD_SET(in, &infds);
    maxfd = (telnet < binary) ? binary : telnet;
    if (maxfd < udp) {
	maxfd = udp;
    }
    if (maxfd < in) {
	maxfd = in;
    }

    closed = 0;

    return TRUE;
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish()
{
    int n;
    connection *conn;

    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	if (conn->fd >= 0) {
	    closesocket(conn->fd);
	}
    }
    closesocket(telnet);
    closesocket(binary);
    closesocket(udp);

    ipa_finish();
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen()
{
    int on;

    if (listen(telnet, 64) < 0 || listen(binary, 64) < 0) {
	perror("listen");
	fatal("conn_listen failed");
    }
    ipa_start(binary);

    on = TRUE;
    if (setsockopt(telnet, SOL_SOCKET, SO_NONBLOCK, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	fatal("setsockopt failed");
    }
    on = TRUE;
    if (setsockopt(binary, SOL_SOCKET, SO_NONBLOCK, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	fatal("setsockopt failed");
    }
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew()
{
    int fd, len, on;
    struct sockaddr_in sin;
    connection *conn;

    if (!FD_ISSET(telnet, &readfds)) {
	return (connection *) NULL;
    }
    len = sizeof(sin);
    fd = accept(telnet, (struct sockaddr *) &sin, &len);
    if (fd < 0) {
	return (connection *) NULL;
    }
    on = TRUE;
    setsockopt(fd, SOL_SOCKET, SO_NONBLOCK, (char *) &on, sizeof(on));

    conn = flist;
    flist = conn->next;
    conn->fd = fd;
    conn->udpbuf = (char *) NULL;
    conn->addr = ipa_new(&sin.sin_addr);
    conn->port = sin.sin_port;
    FD_SET(fd, &infds);
    FD_SET(fd, &outfds);
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
    int fd, len, on;
    struct sockaddr_in sin;
    connection *conn;

    if (!FD_ISSET(binary, &readfds)) {
	return (connection *) NULL;
    }
    len = sizeof(sin);
    fd = accept(binary, (struct sockaddr *) &sin, &len);
    if (fd < 0) {
	return (connection *) NULL;
    }
    on = TRUE;
    setsockopt(fd, SOL_SOCKET, SO_NONBLOCK, (char *) &on, sizeof(on));

    conn = flist;
    flist = conn->next;
    conn->fd = fd;
    conn->udpbuf = (char *) NULL;
    conn->addr = ipa_new(&sin.sin_addr);
    conn->port = sin.sin_port;
    FD_SET(fd, &infds);
    FD_SET(fd, &outfds);
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);
    if (fd > maxfd) {
	maxfd = fd;
    }

    return conn;
}

/*
 * NAME:	conn->udp()
 * DESCRIPTION:	enable the UDP channel of a binary connection
 */
void conn_udp(connection *conn)
{
    connection **hash;

    m_static();
    conn->udpbuf = ALLOC(char, BINBUF_SIZE);
    m_dynamic();
    conn->bufsz = 0;

    hash = &udphtab[((Uint) conn->addr->ipnum.s_addr ^ conn->port) % udphtabsz];
    conn->next = *hash;
    *hash = conn;
}

/*
 * NAME:	conn->del()
 * DESCRIPTION:	delete a connection
 */
void conn_del(connection *conn)
{
    connection **hash;

    if (conn->fd >= 0) {
	closesocket(conn->fd);
	FD_CLR(conn->fd, &infds);
	FD_CLR(conn->fd, &outfds);
	FD_CLR(conn->fd, &waitfds);
	conn->fd = -1;
    } else {
	--closed;
    }
    if (conn->udpbuf != (char *) NULL) {
	FREE(conn->udpbuf);

	for (hash = &udphtab[((Uint) conn->addr->ipnum.s_addr ^ conn->port) %
			     udphtabsz];
	     *hash != conn;
	     hash = &(*hash)->next) ;
	*hash = conn->next;
    }
    ipa_del(conn->addr);
    conn->next = flist;
    flist = conn;
}

/*
 * NAME:	conn->block()
 * DESCRIPTION:	block or unblock input from connection
 */
void conn_block(connection *conn, int flag)
{
    if (conn->fd >= 0) {
	if (flag) {
	    FD_CLR(conn->fd, &infds);
	    FD_CLR(conn->fd, &readfds);
	} else {
	    FD_SET(conn->fd, &infds);
	}
    }
}

/*
 * NAME:	conn->intr()
 * DESCRIPTION:	make sure select() is interrupted
 */
void conn_intr(void)
{
    char intr;

    intr = '\0';
    send(out, &intr, 1, 0);
}

/*
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(Uint t, unsigned int mtime)
{
    struct timeval timeout;
    int retval;

    /*
     * First, check readability and writability for binary sockets with pending
     * data only.
     */
    memcpy(&readfds, &infds, sizeof(fd_set));
    memcpy(&writefds, &waitfds, sizeof(fd_set));
    if (closed != 0) {
	t = 0;
	mtime = 0;
    }
    if (mtime != 0xffff) {
	timeout.tv_sec = t;
	timeout.tv_usec = mtime * 1000;
	retval = select(maxfd + 1, &readfds, &writefds, (fd_set *) NULL,
			&timeout);
    } else {
	retval = select(maxfd + 1, &readfds, &writefds, (fd_set *) NULL,
			(struct timeval *) NULL);
    }
    if (retval < 0) {
	FD_ZERO(&readfds);
	retval = 0;
    }
    retval += closed;

    /*
     * Now check writability for all sockets in a polling call.
     */
    memcpy(&writefds, &outfds, sizeof(fd_set));
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    select(maxfd + 1, (fd_set *) NULL, &writefds, (fd_set *) NULL, &timeout);

    /* check for UDP packet */
    if (FD_ISSET(udp, &readfds)) {
	char buffer[BINBUF_SIZE];
	struct sockaddr_in from;
	int fromlen, size;
	connection **hash;

	fromlen = sizeof(struct sockaddr_in);
	size = recvfrom(udp, buffer, BINBUF_SIZE, 0, (struct sockaddr *) &from,
			&fromlen);
	if (size > 0) {
	    hash = &udphtab[((Uint) from.sin_addr.s_addr ^ from.sin_port) %
			    udphtabsz];
	    while (*hash != (connection *) NULL) {
		if ((*hash)->addr->ipnum.s_addr == from.sin_addr.s_addr &&
		    (*hash)->port == from.sin_port) {
		    /*
		     * copy to connection's buffer
		     */
		    memcpy((*hash)->udpbuf, buffer, (*hash)->bufsz = size);
		    break;
		}
		hash = &(*hash)->next;
	    }
	    /* else from unknown source: ignore */
	}
    }

    /* handle ip name lookup */
    if (FD_ISSET(in, &readfds)) {
	ipa_lookup();
    }
    return retval;
}

/*
 * NAME:	conn->read()
 * DESCRIPTION:	read from a connection
 */
int conn_read(connection *conn, char *buf, unsigned int len)
{
    int size;

    if (conn->fd < 0) {
	return -1;
    }
    if (!FD_ISSET(conn->fd, &readfds)) {
	return 0;
    }
    size = recv(conn->fd, buf, len, 0);
    if (size < 0) {
	closesocket(conn->fd);
	FD_CLR(conn->fd, &infds);
	FD_CLR(conn->fd, &outfds);
	FD_CLR(conn->fd, &waitfds);
	conn->fd = -1;
	closed++;
    }
    return (size == 0) ? -1 : size;
}

/*
 * NAME:	conn->udpread()
 * DESCRIPTION:	read a message from a UDP channel
 */
int conn_udpread(connection *conn, char *buf, unsigned int len)
{
    if (conn->bufsz != 0) {
	/* udp buffer is not empty */
	if (conn->bufsz <= len) {
	    memcpy(buf, conn->udpbuf, len = conn->bufsz);
	} else {
	    len = 0;
	}
	conn->bufsz = 0;
	return len;
    }
    return 0;
}

/*
 * NAME:	conn->write()
 * DESCRIPTION:	write to a connection; return the amount of bytes written
 */
int conn_write(connection *conn, char *buf, unsigned int len)
{
    int size;

    if (conn->fd < 0) {
	return -1;
    }
    if (len == 0) {
	return 0;
    }
    if (!FD_ISSET(conn->fd, &writefds)) {
	/* the write would fail */
	FD_SET(conn->fd, &waitfds);
	return 0;
    }
    if ((size=send(conn->fd, buf, len, 0)) < 0 && errno != EWOULDBLOCK) {
	closesocket(conn->fd);
	FD_CLR(conn->fd, &infds);
	FD_CLR(conn->fd, &outfds);
	conn->fd = -1;
	closed++;
    } else if (size != len) {
	/* waiting for wrdone */
	FD_SET(conn->fd, &waitfds);
	FD_CLR(conn->fd, &writefds);
	if (size < 0) {
	    return 0;
	}
    }
    return size;
}

/*
 * NAME:	conn->udpwrite()
 * DESCRIPTION:	write a message to a UDP channel
 */
int conn_udpwrite(connection *conn, char *buf, unsigned int len)
{
    struct sockaddr_in to;

    if (conn->fd >= 0) {
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = conn->addr->ipnum.s_addr;
	to.sin_port = conn->port;
	return sendto(udp, buf, len, 0, (struct sockaddr *) &to,
		      sizeof(struct sockaddr_in));
    }
    return 0;
}

/*
 * NAME:	conn->wrdone()
 * DESCRIPTION:	return TRUE if a connection is ready for output
 */
bool conn_wrdone(connection *conn)
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
char *conn_ipnum(connection *conn)
{
    return inet_ntoa(conn->addr->ipnum);
}

/*
 * NAME:	conn->ipname()
 * DESCRIPTION:	return the ip name of a connection
 */
char *conn_ipname(connection *conn)
{
    return (conn->addr->name[0] != '\0') ?
	    conn->addr->name : inet_ntoa(conn->addr->ipnum);
}
