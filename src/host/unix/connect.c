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

# ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN	256
# endif

# define NFREE		32

typedef struct _ipaddr_ {
    struct _ipaddr_ *link;		/* next in hash table */
    struct _ipaddr_ *prev;		/* previous in linked list */
    struct _ipaddr_ *next;		/* next in linked list */
    Uint ref;				/* reference count */
    struct in_addr ipnum;		/* ip number */
    char name[MAXHOSTNAMELEN];		/* ip name */
} ipaddr;

static int in = -1, out = -1;		/* pipe to/from name resolver */
static int addrtype;			/* network address family */
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
static void ipa_run(in, out)
register int in, out;
{
    char buf[sizeof(struct in_addr)];
    struct hostent *host;
    register int len;

    for (;;) {
	/* read request */
	if (read(in, buf, sizeof(struct in_addr)) <= 0) {
	    exit(0);	/* pipe closed */
	}

	/* lookup host */
	host = gethostbyaddr(buf, sizeof(struct in_addr), addrtype);
	if (host == (struct hostent *) NULL) {
	    sleep(5);
	    host = gethostbyaddr(buf, sizeof(struct in_addr), addrtype);
	}

	if (host != (struct hostent *) NULL) {
	    /* write host name */
	    len = strlen(host->h_name);
	    if (len >= MAXHOSTNAMELEN) {
		len = MAXHOSTNAMELEN - 1;
	    }
	    write(out, host->h_name, len);
	} else {
	    write(out, "", 1);	/* failure */
	}
    }
}

/*
 * NAME:	ipaddr->init()
 * DESCRIPTION:	initialize name lookup
 */
static bool ipa_init(maxusers)
int maxusers;
{
    int fd[4], pid;

    if (pipe(fd) < 0) {
	perror("pipe");
	return FALSE;
    }
    if (pipe(fd + 2) < 0) {
	perror("pipe");
	close(fd[0]);
	close(fd[1]);
	return FALSE;
    }
    pid = fork();
    if (pid < 0) {
	perror("fork");
	close(fd[0]);
	close(fd[1]);
	close(fd[2]);
	close(fd[3]);
	return FALSE;
    }
    if (pid == 0) {
	/* child process */
	close(fd[1]);
	close(fd[2]);
	ipa_run(fd[0], fd[3]);
    }
    close(fd[0]);
    close(fd[3]);
    in = fd[2];
    out = fd[1];

    ipahtab = ALLOC(ipaddr*, ipahtabsz = maxusers);
    memset(ipahtab, '\0', ipahtabsz * sizeof(ipaddr*));
    qhead = qtail = ffirst = flast = lastreq = (ipaddr *) NULL;
    nfree = 0;
    busy = FALSE;

    return TRUE;
}

/*
 * NAME:	ipaddr->finish()
 * DESCRIPTION:	stop name lookup
 */
static void ipa_finish()
{
    close(in);
    close(out);
    in = -1;
    out = -1;
}

/*
 * NAME:	ipaddr->new()
 * DESCRIPTION:	return a new ipaddr
 */
static ipaddr *ipa_new(ipnum)
struct in_addr *ipnum;
{
    register ipaddr *ipa, **hash;

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
	    return ipa;
	}
	hash = &ipa->link;
    }

    if (nfree >= NFREE) {
	register ipaddr **h;

	/*
	 * use first ipaddr in free list
	 */
	ipa = ffirst;
	ffirst = ipa->next;
	ffirst->prev = (ipaddr *) NULL;
	--nfree;

	/* remove from hash table */
	for (h = &ipahtab[(Uint) ipa->ipnum.s_addr % ipahtabsz];
	     *h != ipa;
	     h = &(*h)->link) ;
	*h = ipa->link;

	if (ipa == lastreq) {
	    lastreq = (ipaddr *) NULL;
	}
    } else {
	/*
	 * allocate new ipaddr
	 */
	m_static();
	ipa = ALLOC(ipaddr, 1);
	m_dynamic();
    }

    /* put in hash table */
    ipa->link = *hash;
    *hash = ipa;
    ipa->ref = 1;
    ipa->ipnum = *ipnum;
    ipa->name[0] = '\0';

    if (!busy) {
	/* send query to name resolver */
	write(out, (char *) ipnum, sizeof(struct in_addr));
	ipa->prev = ipa->next = (ipaddr *) NULL;
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
	ipa->next = (ipaddr *) NULL;
    }

    return ipa;
}

/*
 * NAME:	ipaddr->del()
 * DESCRIPTION:	delete an ipaddr
 */
static void ipa_del(ipa)
register ipaddr *ipa;
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
    register ipaddr *ipa;

    if (lastreq != (ipaddr *) NULL) {
	/* read ip name */
	lastreq->name[read(in, lastreq->name, MAXHOSTNAMELEN)] = '\0';
    } else {
	char buf[MAXHOSTNAMELEN];

	/* discard ip name */
	read(in, buf, MAXHOSTNAMELEN);
    }

    /* if request queue not empty, write new query */
    if (qhead != (ipaddr *) NULL) {
	ipa = qhead;
	write(out, (char *) &ipa->ipnum, sizeof(struct in_addr));
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
bool conn_init(maxusers, telnet_port, binary_port)
int maxusers;
unsigned int telnet_port, binary_port;
{
    struct sockaddr_in sin;
    struct hostent *host;
    register int n;
    register connection *conn;
    char buffer[MAXHOSTNAMELEN];
    int on;

    gethostname(buffer, MAXHOSTNAMELEN);
    host = gethostbyname(buffer);
    if (host == (struct hostent *) NULL) {
	perror("gethostbyname");
	return FALSE;
    }
    addrtype = host->h_addrtype;

    if (!ipa_init(maxusers)) {
	return FALSE;
    }

    nusers = 0;

    telnet = socket(host->h_addrtype, SOCK_STREAM, 0);
    binary = socket(host->h_addrtype, SOCK_STREAM, 0);
    udp = socket(host->h_addrtype, SOCK_DGRAM, 0);
    if (telnet < 0 || binary < 0 || udp < 0) {
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
    on = 1;
    if (setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0)
    {
	perror("setsockopt");
	return FALSE;
    }
# ifdef SO_OOBINLINE
    on = 1;
    if (setsockopt(udp, SOL_SOCKET, SO_OOBINLINE, (char *) &on, sizeof(on)) < 0)
    {
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
    maxfd = (telnet < binary) ? binary : telnet;
    if (maxfd < udp) {
	maxfd = udp;
    }

    FD_SET(in, &infds);
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
    register int n;
    register connection *conn;

    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	if (conn->fd >= 0) {
	    close(conn->fd);
	}
    }
    close(telnet);
    close(binary);
    close(udp);

    ipa_finish();
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
	       fcntl(binary, F_SETFL, FNDELAY) < 0 ||
	       fcntl(udp, F_SETFL, FNDELAY) < 0) {
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
    fcntl(fd, F_SETFL, FNDELAY);

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
    fcntl(fd, F_SETFL, FNDELAY);

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
void conn_udp(conn)
register connection *conn;
{
    register connection **hash;

    m_static();
    conn->udpbuf = ALLOC(char, BINBUF_SIZE);
    m_dynamic();
    conn->bufsz = -1;

    hash = &udphtab[((Uint) conn->addr->ipnum.s_addr ^ conn->port) % udphtabsz];
    conn->next = *hash;
    *hash = conn;
}

/*
 * NAME:	conn->del()
 * DESCRIPTION:	delete a connection
 */
void conn_del(conn)
register connection *conn;
{
    register connection **hash;

    if (conn->fd >= 0) {
	close(conn->fd);
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
void conn_block(conn, flag)
register connection *conn;
int flag;
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
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(t, mtime)
Uint t;
unsigned int mtime;
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
	timeout.tv_usec = mtime * 1000L;
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
	register connection **hash;

	fromlen = sizeof(struct sockaddr_in);
	size = recvfrom(udp, buffer, BINBUF_SIZE, 0, (struct sockaddr *) &from,
			&fromlen);
	if (size >= 0) {
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
 * NAME:	conn->udpread()
 * DESCRIPTION:	read a message from a UDP channel
 */
int conn_udpread(conn, buf, len)
register connection *conn;
char *buf;
unsigned int len;
{
    if (conn->bufsz >= 0) {
	/* udp buffer is not empty */
	if (conn->bufsz <= len) {
	    memcpy(buf, conn->udpbuf, len = conn->bufsz);
	} else {
	    len = 0;
	}
	conn->bufsz = -1;
	return len;
    }
    return -1;
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
	    FD_CLR(conn->fd, &infds);
	    FD_CLR(conn->fd, &outfds);
	    conn->fd = -1;
	    closed++;
	} else if (size != len) {
	    /* waiting for wrdone */
	    FD_SET(conn->fd, &waitfds);
	    FD_CLR(conn->fd, &writefds);
	}
	return size;
    }
    return len;
}

/*
 * NAME:	conn->udpwrite()
 * DESCRIPTION:	write a message to a UDP channel
 */
int conn_udpwrite(conn, buf, len)
register connection *conn;
char *buf;
unsigned int len;
{
    struct sockaddr_in to;

    if (conn->fd >= 0) {
	to.sin_family = addrtype;
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
    return inet_ntoa(conn->addr->ipnum);
}

/*
 * NAME:	conn->ipname()
 * DESCRIPTION:	return the ip name of a connection
 */
char *conn_ipname(conn)
connection *conn;
{
    return (conn->addr->name[0] != '\0') ?
	    conn->addr->name : inet_ntoa(conn->addr->ipnum);
}
