# include <kernel/OS.h>
# include <sys/time.h>
# include <socket.h>
# include <netdb.h>
# include <errno.h>
# define INCLUDE_FILE_IO
# include "dgd.h"
# include "hash.h"
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
static void ipa_start(int sock, bool any)
{
    if (out < 0) {
	struct sockaddr_in addr;
	int len;

	len = sizeof(struct sockaddr_in);
	getsockname(sock, (struct sockaddr *) &addr, &len);
	if (any) {
	    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	}
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
    hte chain;				/* UDP challenge hash chain */
    int fd;				/* file descriptor */
    int npkts;				/* # packets in buffer */
    int bufsz;				/* # bytes in buffer */
    char *udpbuf;			/* datagram buffer */
    ipaddr *addr;			/* internet address of connection */
    unsigned short uport;		/* UDP port of connection */
    unsigned short at;			/* port connection was accepted at */
};

static int nusers;			/* # of users */
static connection *connections;		/* connections array */
static connection *flist;		/* list of free connections */
static connection **udphtab;		/* UDP hash table */
static int udphtabsz;			/* UDP hash table size */
static hashtab *chtab;			/* challenge hash table */
static int *tdescs, *bdescs;		/* telnet & binary descriptor arrays */
static int ntdescs, nbdescs;		/* # telnet & binary ports */
static int *udescs;			/* UDP port descriptor array */
static fd_set infds;			/* file descriptor input bitmap */
static fd_set outfds;			/* file descriptor output bitmap */
static fd_set waitfds;			/* file descriptor wait-write bitmap */
static fd_set readfds;			/* file descriptor read bitmap */
static fd_set writefds;			/* file descriptor write map */
static int maxfd;			/* largest fd opened yet */
static int npackets;			/* # packets buffered */
static int closed;			/* #fds closed in write */
static int ipa;				/* ipa socket */
static bool any;			/* ipa socket bound to INADDR_ANY */

/*
 * NAME:	conn->port()
 * DESCRIPTION:	open an IPv4 port
 */
static int conn_port(int *fd, int type, struct sockaddr_in *sin,
		     unsigned short port)
{
    int on;

    if ((*fd=socket(AF_INET, type, 0)) < 0) {
	perror("socket");
	return FALSE;
    }
    if (*fd > maxfd) {
	maxfd = *fd;
    }
    on = TRUE;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0)
    {
	perror("setsockopt");
	return FALSE;
    }
    sin->sin_port = htons(port);
    if (bind(*fd, (struct sockaddr *) sin, sizeof(struct sockaddr_in)) < 0) {
	perror("bind");
	return FALSE;
    }

    FD_SET(*fd, &infds);
    return TRUE;
}

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connection handling
 */
bool conn_init(int maxusers, char **thosts, char **bhosts,
	       unsigned short *tports, unsigned short *bports, int ntports,
	       int nbports)
{
    struct sockaddr_in sin;
    struct hostent *host;
    connection *conn;
    int n;

    if (!ipa_init(maxusers)) {
	return FALSE;
    }
    ipa = -1;
    any = FALSE;

    nusers = 0;
    maxfd = 0;
    FD_ZERO(&infds);
    FD_ZERO(&outfds);
    FD_ZERO(&waitfds);
    FD_SET(in, &infds);
    npackets = 0;
    closed = 0;

    ntdescs = ntports;
    if (ntports != 0) {
	tdescs = ALLOC(int, ntports);
	memset(tdescs, -1, ntports * sizeof(int));
    }
    nbdescs = nbports;
    if (nbports != 0) {
	bdescs = ALLOC(int, nbports);
	memset(bdescs, -1, nbports * sizeof(int));
	udescs = ALLOC(int, nbports);
	memset(udescs, -1, nbports * sizeof(int));
    }

    memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;

    for (n = 0; n < ntdescs; n++) {
	/* telnet ports */
	if (thosts[n] == (char *) NULL) {
	    sin.sin_addr.s_addr = INADDR_ANY;
	} else if ((sin.sin_addr.s_addr=inet_addr(thosts[n])) == 0xffffffff) {
	    host = gethostbyname(thosts[n]);
	    if (host == (struct hostent *) NULL) {
		message("unknown host %s\n", thosts[n]);
		return FALSE;
	    }
	    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
	}

	if (!conn_port(&tdescs[n], SOCK_STREAM, &sin, tports[n])) {
	    return FALSE;
	}
	if (ipa < 0) {
	    ipa = tdescs[n];
	    any = (thosts[n] == (char *) NULL);
	}
    }

    for (n = 0; n < nbdescs; n++) {
	/* binary ports */
	if (bhosts[n] == (char *) NULL) {
	    sin.sin_addr.s_addr = INADDR_ANY;
	} else if ((sin.sin_addr.s_addr=inet_addr(bhosts[n])) == 0xffffffff) {
	    host = gethostbyname(bhosts[n]);
	    if (host == (struct hostent *) NULL) {
		message("unknown host %s\n", bhosts[n]);
		return FALSE;
	    }
	    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
	}
	if (!conn_port(&bdescs[n], SOCK_STREAM, &sin, bports[n])) {
	    return FALSE;
	}
	if (!conn_port(&udescs[n], SOCK_DGRAM, &sin, bports[n])) {
	    return FALSE;
	}
	if (ipa < 0) {
	    ipa = bdescs[n];
	    any = (bhosts[n] == (char *) NULL);
	}
    }

    flist = (connection *) NULL;
    connections = ALLOC(connection, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = -1;
	conn->chain.next = (hte *) flist;
	flist = conn;
    }

    udphtab = ALLOC(connection*, udphtabsz = maxusers);
    memset(udphtab, '\0', udphtabsz * sizeof(connection*));
    chtab = ht_new(maxusers, UDPHASHSZ, TRUE);

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
    for (n = 0; n < ntdescs; n++) {
	close(tdescs[n]);
    }
    for (n = 0; n < nbdescs; n++) {
	close(bdescs[n]);
	close(udescs[n]);
    }

    ipa_finish();
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen()
{
    int n, on;

    for (n = 0; n < ntdescs; n++) {
	if (listen(tdescs[n], 64) < 0) {
	    perror("listen");
	    fatal("listen failed");
	}
	on = TRUE;
	if (setsockopt(tdescs[n], SOL_SOCKET, SO_NONBLOCK, (char *) &on,
		       sizeof(on)) < 0) {
	    perror("setsockopt");
	    fatal("setsockopt failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	if (listen(bdescs[n], 64) < 0) {
	    perror("listen");
	    fatal("listen failed");
	}
	on = TRUE;
	if (setsockopt(bdescs[n], SOL_SOCKET, SO_NONBLOCK, (char *) &on,
		       sizeof(on)) < 0) {
	    perror("setsockopt");
	    fatal("setsockopt failed");
	}
	on = TRUE;
	if (setsockopt(udescs[n], SOL_SOCKET, SO_NONBLOCK, (char *) &on,
		       sizeof(on)) < 0) {
	    perror("setsockopt");
	    fatal("setsockopt failed");
	}
    }

    if (ipa >= 0) {
	ipa_start(ipa, any);
    }
}

/*
 * NAME:	conn->accept()
 * DESCRIPTION:	accept a new connection
 */
static connection *conn_accept(int portfd, int port)
{
    int fd, n;
    struct sockaddr_in sin;
    connection *conn;

    if (!FD_ISSET(portfd, &readfds)) {
	return (connection *) NULL;
    }
    n = sizeof(sin);
    fd = accept(portfd, (struct sockaddr *) &sin, &n);
    if (fd < 0) {
	FD_CLR(portfd, &readfds);
	return (connection *) NULL;
    }
    n = TRUE;
    setsockopt(fd, SOL_SOCKET, SO_NONBLOCK, (char *) &n, sizeof(n));

    conn = flist;
    flist = (connection *) conn->chain.next;
    conn->chain.name = (char *) NULL;
    conn->fd = fd;
    conn->udpbuf = (char *) NULL;
    conn->addr = ipa_new(&sin.sin_addr);
    conn->at = port;
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
 * NAME:	conn->tnew6()
 * DESCRIPTION:	don't accept IPv6 connections
 */
connection *conn_tnew6(int port)
{
    return (connection *) NULL;
}

/*
 * NAME:	conn->bnew6()
 * DESCRIPTION:	don't accept IPv6 connections
 */
connection *conn_bnew6(int port)
{
    return (connection *) NULL;
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew(int port)
{
    return conn_accept(tdescs[port], port);
}

/*
 * NAME:	conn->bnew()
 * DESCRIPTION:	accept a new binary connection
 */
connection *conn_bnew(int port)
{
    return conn_accept(bdescs[port], port);
}

/*
 * NAME:	conn->udp()
 * DESCRIPTION:	set the challenge for attaching a UDP channel
 */
bool conn_udp(connection *conn, char *challenge, unsigned int len)
{
    char buffer[UDPHASHSZ];
    connection **hash;

    if (len == 0 || len > BINBUF_SIZE || conn->udpbuf != (char *) NULL) {
	return FALSE;	/* invalid challenge */
    }

    if (len >= UDPHASHSZ) {
	memcpy(buffer, challenge, UDPHASHSZ);
    } else {
	memset(buffer, '\0', UDPHASHSZ);
	memcpy(buffer, challenge, len);
    }
    hash = (connection **) ht_lookup(chtab, buffer, FALSE);
    while (*hash != (connection *) NULL &&
	   memcmp((*hash)->chain.name, buffer, UDPHASHSZ) == 0) {
	if ((*hash)->bufsz == len &&
	    memcmp((*hash)->udpbuf, challenge, len) == 0) {
	    return FALSE;	/* duplicate challenge */
	}
    }

    conn->chain.next = (hte *) *hash;
    *hash = conn;
    conn->npkts = 0;
    m_static();
    conn->udpbuf = ALLOC(char, BINBUF_SIZE);
    m_dynamic();
    memset(conn->udpbuf, '\0', UDPHASHSZ);
    memcpy(conn->chain.name = conn->udpbuf, challenge, conn->bufsz = len);

    return TRUE;
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
	if (conn->chain.name != (char *) NULL) {
	    hash = (connection **) ht_lookup(chtab, conn->chain.name, FALSE);
	} else {
	    hash = &udphtab[(((Uint) conn->addr->ipnum.s_addr) ^
						    conn->uport) % udphtabsz];
	}
	while (*hash != conn) {
	    hash = (connection **) &(*hash)->chain.next;
	}
	*hash = (connection *) conn->chain.next;
	npackets -= conn->npkts;
	FREE(conn->udpbuf);
    }
    ipa_del(conn->addr);
    conn->chain.next = (hte *) flist;
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
 * NAME:	conn->udprecv()
 * DESCRIPTION:	receive an UDP packet
 */
static void conn_udprecv(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in from;
    int fromlen;
    int size;
    connection **hash, *conn;
    char *p;

    for (;;) {
	memset(buffer, '\0', UDPHASHSZ);
	fromlen = sizeof(struct sockaddr_in);
	size = recvfrom(udescs[n], buffer, BINBUF_SIZE, 0,
			(struct sockaddr *) &from, &fromlen);
	if (size < 0) {
	    return;
	}

	hash = &udphtab[((Uint) from.sin_addr.s_addr ^ from.sin_port) %
								    udphtabsz];
	for (;;) {
	    conn = *hash;
	    if (conn == (connection *) NULL) {
		/*
		 * see if the packet matches an outstanding challenge
		 */
		hash = (connection **) ht_lookup(chtab, buffer, FALSE);
		while ((conn=*hash) != (connection *) NULL &&
		       memcmp((*hash)->chain.name, buffer, UDPHASHSZ) == 0) {
		    if (conn->bufsz == size &&
			memcmp(conn->udpbuf, buffer, size) == 0 &&
			conn->addr->ipnum.s_addr == from.sin_addr.s_addr) {
			/*
			 * attach new UDP channel
			 */
			*hash = (connection *) conn->chain.next;
			conn->chain.name = (char *) NULL;
			conn->bufsz = 0;
			conn->uport = from.sin_port;
			hash = &udphtab[((Uint) from.sin_addr.s_addr ^
						    conn->uport) % udphtabsz];
			conn->chain.next = (hte *) *hash;
			*hash = conn;

			break;
		    }
		    hash = (connection **) &conn->chain.next;
		}
		break;
	    }

	    if (conn->at == n &&
		conn->addr->ipnum.s_addr == from.sin_addr.s_addr &&
		conn->uport == from.sin_port) {
		/*
		 * packet from known correspondent
		 */
		if (conn->bufsz + size <= BINBUF_SIZE - 2) {
		    p = conn->udpbuf + conn->bufsz;
		    *p++ = size >> 8;
		    *p++ = size;
		    memcpy(p, buffer, size);
		    conn->bufsz += size + 2;
		    conn->npkts++;
		    npackets++;
		}
		break;
	    }
	    hash = (connection **) &conn->chain.next;
	}
    }
}

/*
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(Uint t, unsigned int mtime)
{
    struct timeval timeout;
    int retval;
    int n;

    /*
     * First, check readability and writability for binary sockets with pending
     * data only.
     */
    memcpy(&readfds, &infds, sizeof(fd_set));
    if (flist == (connection *) NULL) {
	/* can't accept new connections, so don't check for them */
	for (n = ntdescs; n != 0; ) {
	    --n;
	    FD_CLR(tdescs[n], &readfds);
	}
	for (n = nbdescs; n != 0; ) {
	    --n;
	    FD_CLR(bdescs[n], &readfds);
	}
    }
    memcpy(&writefds, &waitfds, sizeof(fd_set));
    if (npackets + closed != 0) {
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

    /* check for UDP packets */
    for (n = 0; n < nbdescs; n++) {
	if (FD_ISSET(udescs[n], &readfds)) {
	    conn_udprecv(n);
	}
    }
    retval += npackets + closed;

    /*
     * Now check writability for all sockets in a polling call.
     */
    memcpy(&writefds, &outfds, sizeof(fd_set));
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    select(maxfd + 1, (fd_set *) NULL, &writefds, (fd_set *) NULL, &timeout);

    /* handle ip name lookup */
    if (FD_ISSET(in, &readfds)) {
	ipa_lookup();
    }
    return retval;
}

/*
 * NAME:	conn->udpcheck()
 * DESCRIPTION:	check if UDP challenge met
 */
bool conn_udpcheck(connection *conn)
{
    return (conn->chain.name == (char *) NULL);
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
    unsigned short size, n;
    char *p, *q;

    while (conn->bufsz != 0) {
	/* udp buffer is not empty */
	size = ((unsigned char) conn->udpbuf[0] << 8) |
	       (unsigned char) conn->udpbuf[1];
	if (size <= len) {
	    memcpy(buf, conn->udpbuf + 2, len = size);
	}
	--conn->npkts;
	--npackets;
	conn->bufsz -= size + 2;
	for (p = conn->udpbuf, q = p + size + 2, n = conn->bufsz; n != 0; --n) {
	    *p++ = *q++;
	}
	if (len == size) {
	    return len;
	}
    }
    return -1;
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
	to.sin_port = conn->uport;
	return sendto(udescs[conn->at], buf, len, 0, (struct sockaddr *) &to,
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
void conn_ipnum(connection *conn, char *buf)
{
    strcpy(buf, inet_ntoa(conn->addr->ipnum));
}

/*
 * NAME:	conn->ipname()
 * DESCRIPTION:	return the ip name of a connection
 */
void conn_ipname(connection *conn, char *buf)
{
    if (conn->addr->name[0] != '\0') {
	strcpy(buf, conn->addr->name);
    } else {
	conn_ipnum(conn, buf);
    }
}
