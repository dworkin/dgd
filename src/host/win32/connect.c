# define FD_SETSIZE   1024
# include <winsock2.h>
# include <ws2tcpip.h>
# include <process.h>
# define INCLUDE_FILE_IO
# include "dgd.h"
# include "hash.h"
# include "comm.h"

# define MAXHOSTNAMELEN	256

# define NFREE		32

typedef struct {
    union {
	struct in6_addr addr6;		/* IPv6 addr */
	struct in_addr addr;		/* IPv4 addr */
    } in;
    bool ipv6;				/* IPv6? */
} in46addr;

typedef struct _ipaddr_ {
    struct _ipaddr_ *link;		/* next in hash table */
    struct _ipaddr_ *prev;		/* previous in linked list */
    struct _ipaddr_ *next;		/* next in linked list */
    Uint ref;				/* reference count */
    in46addr ipnum;			/* ip number */
    char name[MAXHOSTNAMELEN];		/* ip name */
} ipaddr;

static SOCKET in = INVALID_SOCKET;	/* connection from name resolver */
static SOCKET out = INVALID_SOCKET;	/* connection to name resolver */
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
static void ipa_run(void *dummy)
{
    char buf[sizeof(in46addr)];
    struct hostent *host;
    int len;

    while (recv(out, buf, sizeof(in46addr), 0) > 0) {
	/* lookup host */
	if (((in46addr *) &buf)->ipv6) {
	    host = gethostbyaddr(buf, sizeof(struct in6_addr), AF_INET6);
	    if (host == (struct hostent *) NULL) {
		Sleep(2000);
		host = gethostbyaddr(buf, sizeof(struct in6_addr), AF_INET6);
	    }
	} else {
	    host = gethostbyaddr(buf, sizeof(struct in_addr), AF_INET);
	    if (host == (struct hostent *) NULL) {
		Sleep(2000);
		host = gethostbyaddr(buf, sizeof(struct in_addr), AF_INET);
	    }
	}

	if (host != (struct hostent *) NULL) {
	    /* write host name */
	    len = strlen(host->h_name);
	    if (len >= MAXHOSTNAMELEN) {
		len = MAXHOSTNAMELEN - 1;
	    }
	    send(out, host->h_name, len, 0);
	} else {
	    send(out, "", 1, 0);	/* failure */
	}
    }
}

/*
 * NAME:	ipaddr->init()
 * DESCRIPTION:	initialize name lookup
 */
static bool ipa_init(int maxusers)
{
    ipahtab = ALLOC(ipaddr*, ipahtabsz = maxusers);
    memset(ipahtab, '\0', ipahtabsz * sizeof(ipaddr*));
    qhead = qtail = ffirst = flast = lastreq = (ipaddr *) NULL;
    nfree = 0;
    busy = FALSE;

    return TRUE;
}

/*
 * NAME:	ipadd->start()
 * DESCRIPTION:	start name resolver thread
 */
static void ipa_start(SOCKET fd_in, SOCKET fd_out)
{
    in = fd_in;
    out = fd_out;
    _beginthread(ipa_run, 0, NULL);
}

/*
 * NAME:	ipaddr->finish()
 * DESCRIPTION:	stop name lookup
 */
static void ipa_finish(void)
{
    closesocket(in);
    in = INVALID_SOCKET;
    out = INVALID_SOCKET;
}

/*
 * NAME:	ipaddr->new()
 * DESCRIPTION:	return a new ipaddr
 */
static ipaddr *ipa_new(in46addr *ipnum)
{
    ipaddr *ipa, **hash;

    /* check hash table */
    if (ipnum->ipv6) {
	hash = &ipahtab[hashmem((char *) ipnum,
			sizeof(struct in6_addr)) % ipahtabsz];
    } else {
	hash = &ipahtab[(Uint) ipnum->in.addr.s_addr % ipahtabsz];
    }
    while (*hash != (ipaddr *) NULL) {
	ipa = *hash;
	if (ipnum->ipv6 == ipa->ipnum.ipv6 &&
	    ((ipnum->ipv6) ?
	      memcmp(&ipnum->in.addr6, &ipa->ipnum.in.addr6,
		     sizeof(struct in6_addr)) == 0 :
	      ipnum->in.addr.s_addr == ipa->ipnum.in.addr.s_addr)) {
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
		    send(in, (char *) ipnum, sizeof(in46addr), 0);
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
	    if (ipa->ipnum.ipv6) {
		h = &ipahtab[hashmem((char *) &ipa->ipnum,
				     sizeof(struct in6_addr)) % ipahtabsz];
	    } else
	    {
		h = &ipahtab[(Uint) ipa->ipnum.in.addr.s_addr % ipahtabsz];
	    }
	    while (*h != ipa) {
		h = &(*h)->link;
	    }
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
	send(in, (char *) ipnum, sizeof(in46addr), 0);
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
static void ipa_lookup(void)
{
    ipaddr *ipa;

    if (lastreq != (ipaddr *) NULL) {
	/* read ip name */
	lastreq->name[recv(in, lastreq->name, MAXHOSTNAMELEN, 0)] = '\0';
    } else {
	char buf[MAXHOSTNAMELEN];

	/* discard ip name */
	recv(in, buf, MAXHOSTNAMELEN, 0);
    }

    /* if request queue not empty, write new query */
    if (qhead != (ipaddr *) NULL) {
	ipa = qhead;
	send(in, (char *) &ipa->ipnum, sizeof(in46addr), 0);
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
    SOCKET fd;				/* file descriptor */
    int bufsz;				/* # bytes in buffer */
    int npkts;				/* # packets in buffer */
    char *udpbuf;			/* datagram buffer */
    ipaddr *addr;			/* internet address of connection */
    unsigned short uport;		/* UDP port of connection */
    unsigned short at;			/* port connection was accepted at */
    struct _connection_ *next;		/* next in list */
};

typedef struct {
    SOCKET in6;				/* IPv6 socket */
    SOCKET in4;				/* IPv4 socket */
} portdesc;

static int nusers;			/* # of users */
static connection *connections;		/* connections array */
static connection *flist;		/* list of free connections */
static connection **udphtab;		/* UDP hash table */
static hashtab *chtab;			/* challenge hash table */
static int udphtabsz;			/* UDP hash table size */
static portdesc *tdescs, *bdescs;	/* telnet & binary descriptor arrays */
static int ntdescs, nbdescs;		/* # telnet & binary ports */
static portdesc *udescs;		/* UDP port descriptor array */
static fd_set infds;			/* file descriptor input bitmap */
static fd_set outfds;			/* file descriptor output bitmap */
static fd_set waitfds;			/* file descriptor wait-write bitmap */
static fd_set readfds;			/* file descriptor read bitmap */
static fd_set writefds;			/* file descriptor write map */
static int npackets;			/* # packets buffered */
static int closed;			/* #fds closed in write */
static SOCKET self;			/* socket to self */
static bool self6;			/* self socket IPv6? */
static SOCKET cintr;			/* interrupt socket */

/*
 * NAME:	conn->intr()
 * DESCRIPTION:	interrupt conn->select()
 */
void conn_intr(void)
{
    send(cintr, "", 1, 0);
}

/*
 * NAME:	conn->port6()
 * DESCRIPTION:	open an IPv6 port
 */
static int conn_port6(fd, type, sin6, port)
register SOCKET *fd;
int type;
struct sockaddr_in6 *sin6;
unsigned short port;
{
    int on;

    if ((*fd=socket(AF_INET6, type, 0)) == INVALID_SOCKET) {
	return FALSE;
    }
    on = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0)
    {
	P_message("setsockopt() failed\n");
	return FALSE;
    }
    if (type == SOCK_STREAM) {
	on = 1;
	if (setsockopt(*fd, SOL_SOCKET, SO_OOBINLINE, (char *) &on, sizeof(on))
									< 0) {
	    P_message("setsockopt() failed\n");
	    return FALSE;
	}
    }
    WSAHtons(*fd, port, &sin6->sin6_port);
    if (bind(*fd, (struct sockaddr *) sin6, sizeof(struct sockaddr_in6)) < 0) {
	P_message("bind() failed\n");
	return FALSE;
    }

    FD_SET(*fd, &infds);
    return TRUE;
}

/*
 * NAME:	conn->port()
 * DESCRIPTION:	open an IPv4 port
 */
static int conn_port(fd, type, sin, port)
register SOCKET *fd;
int type;
struct sockaddr_in *sin;
unsigned short port;
{
    int on;

    if ((*fd=socket(AF_INET, type, 0)) == INVALID_SOCKET) {
	P_message("socket() failed\n");
	return FALSE;
    }
    on = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0)
    {
	P_message("setsockopt() failed\n");
	return FALSE;
    }
    if (type == SOCK_STREAM) {
	on = 1;
	if (setsockopt(*fd, SOL_SOCKET, SO_OOBINLINE, (char *) &on, sizeof(on))
									< 0) {
	    P_message("setsockopt() failed\n");
	    return FALSE;
	}
    }
    sin->sin_port = htons(port);
    if (bind(*fd, (struct sockaddr *) sin, sizeof(struct sockaddr_in)) < 0) {
	P_message("bind() failed\n");
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
	       unsigned short *tports, unsigned short *bports,
	       int ntports, int nbports)
{
    WSADATA wsadata;
    struct sockaddr_in6 sin6;
    struct sockaddr_in sin;
    int n, length;
    struct hostent *host;
    connection *conn;
    bool ipv6, ipv4;

    self = INVALID_SOCKET;
    cintr = INVALID_SOCKET;

    /* initialize winsock */
    if (WSAStartup(MAKEWORD(2, 0), &wsadata) != 0) {
	P_message("WSAStartup failed (no winsock?)\n");
	return FALSE;
    }
    if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 0) {
	WSACleanup();
	P_message("Winsock 2.0 not supported\n");
	return FALSE;
    }

    if (!ipa_init(maxusers)) {
	return FALSE;
    }

    nusers = 0;
    FD_ZERO(&infds);
    FD_ZERO(&outfds);
    FD_ZERO(&waitfds);
    npackets = 0;
    closed = 0;

    ntdescs = ntports;
    if (ntports != 0) {
	tdescs = ALLOC(portdesc, ntports);
	for (n = 0; n < ntdescs; n++) {
	    tdescs[n].in6 = INVALID_SOCKET;
	    tdescs[n].in4 = INVALID_SOCKET;
	}
    }
    nbdescs = nbports;
    if (nbports != 0) {
	bdescs = ALLOC(portdesc, nbports);
	udescs = ALLOC(portdesc, nbports);
	for (n = 0; n < nbdescs; n++) {
	    bdescs[n].in6 = INVALID_SOCKET;
	    bdescs[n].in4 = INVALID_SOCKET;
	    udescs[n].in6 = INVALID_SOCKET;
	    udescs[n].in4 = INVALID_SOCKET;
	}
    }

    memset(&sin6, '\0', sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;

    for (n = 0; n < ntdescs; n++) {
	/* telnet ports */
	ipv6 = FALSE;
	ipv4 = FALSE;
	if (thosts[n] == (char *) NULL) {
	    sin6.sin6_addr = in6addr_any;
	    sin.sin_addr.s_addr = INADDR_ANY;
	    ipv6 = ipv4 = TRUE;
	} else {
	    sin6.sin6_family = AF_INET6;
	    length = sizeof(struct sockaddr_in6);
	    if (WSAStringToAddress(thosts[n], AF_INET6,
				   (LPWSAPROTOCOL_INFO) NULL,
				   (struct sockaddr *) &sin6, &length) == 0) {
		ipv6 = TRUE;
	    } else {
		struct addrinfo hints, *addr;

		memset(&hints, '\0', sizeof(struct addrinfo));
		hints.ai_family = PF_INET6;
		addr = (struct addrinfo *) NULL;
		getaddrinfo(thosts[n], (char *) NULL, &hints, &addr);
		if (addr != (struct addrinfo *) NULL) {
		    memcpy(&sin6, addr->ai_addr, addr->ai_addrlen);
		    ipv6 = TRUE;
		    freeaddrinfo(addr);
		}
	    }
	    if ((sin.sin_addr.s_addr=inet_addr(thosts[n])) != INADDR_NONE) {
		ipv4 = TRUE;
	    } else {
		host = gethostbyname(thosts[n]);
		if (host != (struct hostent *) NULL) {
		    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
		    ipv4 = TRUE;
		}
	    }
	}

	if (!ipv6 && !ipv4) {
	    message("unknown host %s\012", thosts[n]);	/* LF */
	    return FALSE;
	}

	if (ipv6 &&
	    !conn_port6(&tdescs[n].in6, SOCK_STREAM, &sin6, tports[n]) &&
	    tdescs[n].in6 != INVALID_SOCKET) {
	    return FALSE;
	}
	if (ipv4 && !conn_port(&tdescs[n].in4, SOCK_STREAM, &sin, tports[n])) {
	    return FALSE;
	}

	if (self == INVALID_SOCKET) {
	    if (tdescs[n].in6 != INVALID_SOCKET) {
		self = tdescs[n].in6;
		self6 = TRUE;
	    } else {
		self = tdescs[n].in4;
		self6 = FALSE;
	    }
	}
    }

    for (n = 0; n < nbdescs; n++) {
	/* binary ports */
	ipv6 = FALSE;
	ipv4 = FALSE;
	if (bhosts[n] == (char *) NULL) {
	    sin6.sin6_addr = in6addr_any;
	    sin.sin_addr.s_addr = INADDR_ANY;
	    ipv6 = ipv4 = TRUE;
	} else {
	    sin6.sin6_family = AF_INET6;
	    length = sizeof(struct sockaddr_in6);
	    if (WSAStringToAddress(bhosts[n], AF_INET6,
				   (LPWSAPROTOCOL_INFO) NULL,
				   (struct sockaddr *) &sin6, &length) == 0) {
		ipv6 = TRUE;
	    } else {
		struct addrinfo hints, *addr;

		memset(&hints, '\0', sizeof(struct addrinfo));
		hints.ai_family = PF_INET6;
		addr = (struct addrinfo *) NULL;
		getaddrinfo(bhosts[n], (char *) NULL, &hints, &addr);
		if (addr != (struct addrinfo *) NULL) {
		    memcpy(&sin6, addr->ai_addr, addr->ai_addrlen);
		    ipv6 = TRUE;
		    freeaddrinfo(addr);
		}
	    }
	    if ((sin.sin_addr.s_addr=inet_addr(bhosts[n])) != INADDR_NONE) {
		ipv4 = TRUE;
	    } else {
		host = gethostbyname(bhosts[n]);
		if (host != (struct hostent *) NULL) {
		    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
		    ipv4 = TRUE;
		}
	    }
	}

	if (!ipv6 && !ipv4) {
	    message("unknown host %s\012", bhosts[n]);	/* LF */
	    return FALSE;
	}

	if (ipv6) {
	    if (!conn_port6(&bdescs[n].in6, SOCK_STREAM, &sin6, bports[n]) &&
		bdescs[n].in6 != INVALID_SOCKET) {
		return FALSE;
	    }
	    if (!conn_port6(&udescs[n].in6, SOCK_DGRAM, &sin6, bports[n]) &&
		udescs[n].in6 != INVALID_SOCKET) {
		return FALSE;
	    }
	}
	if (ipv4) {
	    if (!conn_port(&bdescs[n].in4, SOCK_STREAM, &sin, bports[n])) {
		return FALSE;
	    }
	    if (!conn_port(&udescs[n].in4, SOCK_DGRAM, &sin, bports[n])) {
		return FALSE;
	    }
	}

	if (self == INVALID_SOCKET) {
	    if (bdescs[n].in6 != INVALID_SOCKET) {
		self = bdescs[n].in6;
		self6 = TRUE;
	    } else {
		self = bdescs[n].in4;
		self6 = FALSE;
	    }
	}
    }

    flist = (connection *) NULL;
    connections = ALLOC(connection, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = INVALID_SOCKET;
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
void conn_finish(void)
{
    ipa_finish();

    WSACleanup();
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen(void)
{
    int n;
    unsigned long nonblock;

    for (n = 0; n < ntdescs; n++) {
	if (tdescs[n].in6 != INVALID_SOCKET && listen(tdescs[n].in6, 64) != 0) {
	    fatal("listen failed");
	}
	if (tdescs[n].in4 != INVALID_SOCKET && listen(tdescs[n].in4, 64) != 0) {
	    fatal("listen failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	if (bdescs[n].in6 != INVALID_SOCKET && listen(bdescs[n].in6, 64) != 0) {
	    fatal("listen failed");
	}
	if (bdescs[n].in4 != INVALID_SOCKET && listen(bdescs[n].in4, 64) != 0) {
	    fatal("listen failed");
	}
    }
    if (self != INVALID_SOCKET) {
	int len;
	SOCKET fd;

	if (self6) {
	    struct sockaddr_in6 addr;
	    struct sockaddr_in6 dummy;

	    len = sizeof(struct sockaddr_in6);
	    getsockname(self, (struct sockaddr *) &addr, &len);
	    if (IN6_IS_ADDR_UNSPECIFIED(&addr.sin6_addr)) {
		addr.sin6_addr.s6_addr[15] = 1;
	    }
	    in = socket(AF_INET6, SOCK_STREAM, 0);
	    FD_SET(in, &infds);
	    connect(in, (struct sockaddr *) &addr, len);
	    ipa_start(in, accept(self, (struct sockaddr *) &dummy, &len));
	    fd = socket(AF_INET6, SOCK_STREAM, 0);
	    connect(fd, (struct sockaddr *) &addr, len);
	    cintr = accept(self, (struct sockaddr *) &dummy, &len);
	} else {
	    struct sockaddr_in addr;
	    struct sockaddr_in dummy;

	    len = sizeof(struct sockaddr_in);
	    getsockname(self, (struct sockaddr *) &addr, &len);
	    if (addr.sin_addr.s_addr == INADDR_ANY) {
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	    }
	    in = socket(AF_INET, SOCK_STREAM, 0);
	    FD_SET(in, &infds);
	    connect(in, (struct sockaddr *) &addr, len);
	    ipa_start(in, accept(self, (struct sockaddr *) &dummy, &len));
	    fd = socket(AF_INET, SOCK_STREAM, 0);
	    connect(fd, (struct sockaddr *) &addr, len);
	    cintr = accept(self, (struct sockaddr *) &dummy, &len);
	}
	FD_SET(fd, &infds);
    }

    for (n = 0; n < ntdescs; n++) {
	nonblock = TRUE;
	if (tdescs[n].in6 != INVALID_SOCKET &&
	    ioctlsocket(tdescs[n].in6, FIONBIO, &nonblock) != 0) {
	    fatal("ioctlsocket failed");
	}
	nonblock = TRUE;
	if (tdescs[n].in4 != INVALID_SOCKET &&
	    ioctlsocket(tdescs[n].in4, FIONBIO, &nonblock) != 0) {
	    fatal("ioctlsocket failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	nonblock = TRUE;
	if (bdescs[n].in6 != INVALID_SOCKET &&
	    ioctlsocket(bdescs[n].in6, FIONBIO, &nonblock) != 0) {
	    fatal("ioctlsocket failed");
	}
	nonblock = TRUE;
	if (bdescs[n].in4 != INVALID_SOCKET &&
	    ioctlsocket(bdescs[n].in4, FIONBIO, &nonblock) != 0) {
	    fatal("ioctlsocket failed");
	}
	nonblock = TRUE;
	if (udescs[n].in6 != INVALID_SOCKET &&
	    ioctlsocket(udescs[n].in6, FIONBIO, &nonblock) != 0) {
	    fatal("ioctlsocket failed");
	}
	nonblock = TRUE;
	if (udescs[n].in4 != INVALID_SOCKET &&
	    ioctlsocket(udescs[n].in4, FIONBIO, &nonblock) != 0) {
	    fatal("ioctlsocket failed");
	}
    }
}

/*
 * NAME:	conn->accept6()
 * DESCRIPTION:	accept a new ipv6 connection
 */
static connection *conn_accept6(SOCKET portfd, int port)
{
    SOCKET fd;
    int len;
    struct sockaddr_in6 sin6;
    in46addr addr;
    connection *conn;
    unsigned long nonblock;

    if (!FD_ISSET(portfd, &readfds)) {
	return (connection *) NULL;
    }
    len = sizeof(sin6);
    fd = accept(portfd, (struct sockaddr *) &sin6, &len);
    if (fd == INVALID_SOCKET) {
	FD_CLR(portfd, &readfds);
	return (connection *) NULL;
    }
    nonblock = TRUE;
    ioctlsocket(fd, FIONBIO, &nonblock);

    conn = flist;
    flist = (connection *) conn->chain.next;
    conn->chain.name = (char *) NULL;
    conn->fd = fd;
    conn->udpbuf = (char *) NULL;
    addr.in.addr6 = sin6.sin6_addr;
    addr.ipv6 = TRUE;
    conn->addr = ipa_new(&addr);
    conn->at = port;
    FD_SET(fd, &infds);
    FD_SET(fd, &outfds);
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);

    return conn;
}

/*
 * NAME:	conn->accept()
 * DESCRIPTION:	accept a new ipv4 connection
 */
static connection *conn_accept(SOCKET portfd, int port)
{
    SOCKET fd;
    int len;
    struct sockaddr_in sin;
    in46addr addr;
    connection *conn;
    unsigned long nonblock;

    if (!FD_ISSET(portfd, &readfds)) {
	return (connection *) NULL;
    }
    len = sizeof(sin);
    fd = accept(portfd, (struct sockaddr *) &sin, &len);
    if (fd == INVALID_SOCKET) {
	FD_CLR(portfd, &readfds);
	return (connection *) NULL;
    }
    nonblock = TRUE;
    ioctlsocket(fd, FIONBIO, &nonblock);

    conn = flist;
    flist = (connection *) conn->chain.next;
    conn->chain.name = (char *) NULL;
    conn->fd = fd;
    conn->udpbuf = (char *) NULL;
    addr.in.addr = sin.sin_addr;
    addr.ipv6 = FALSE;
    conn->addr = ipa_new(&addr);
    conn->at = port;
    FD_SET(fd, &infds);
    FD_SET(fd, &outfds);
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);

    return conn;
}

/*
 * NAME:	conn->tnew6()
 * DESCRIPTION:	don't accept IPv6 connections yet
 */
connection *conn_tnew6(int port)
{
    SOCKET fd;

    fd = tdescs[port].in6;
    if (fd != INVALID_SOCKET) {
	return conn_accept6(fd, port);
    }
    return (connection *) NULL;
}
 
/*
 * NAME:	conn->bnew6()
 * DESCRIPTION:	don't accept IPv6 connections yet
 */
connection *conn_bnew6(int port)
{
    SOCKET fd;

    fd = bdescs[port].in6;
    if (fd != INVALID_SOCKET) {
	return conn_accept6(fd, port);
    }
    return (connection *) NULL;
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew(int port)
{
    SOCKET fd;

    fd = tdescs[port].in4;
    if (fd != INVALID_SOCKET) {
	return conn_accept(fd, port);
    }
    return (connection *) NULL;
}

/*
 * NAME:	conn->bnew()
 * DESCRIPTION:	accept a new binary connection
 */
connection *conn_bnew(int port)
{
    SOCKET fd;

    fd = bdescs[port].in4;
    if (fd != INVALID_SOCKET) {
	return conn_accept(fd, port);
    }
    return (connection *) NULL;
}

/*
 * NAME:	conn->udp()
 * DESCRIPTION:	set the challenge for attaching a UDP channel
 */
bool conn_udp(register connection *conn, char *challenge,
	      register unsigned int len)
{
    char buffer[UDPHASHSZ];
    register connection **hash;

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

    if (conn->fd != INVALID_SOCKET) {
	closesocket(conn->fd);
	FD_CLR(conn->fd, &infds);
	FD_CLR(conn->fd, &outfds);
	FD_CLR(conn->fd, &waitfds);
	conn->fd = INVALID_SOCKET;
    } else {
	--closed;
    }
    if (conn->udpbuf != (char *) NULL) {
	if (conn->chain.name != (char *) NULL) {
	    hash = (connection **) ht_lookup(chtab, conn->chain.name, FALSE);
	} else if (conn->addr->ipnum.ipv6) {
	    hash = &udphtab[(hashmem((char *) &conn->addr->ipnum,
			sizeof(struct in6_addr)) ^ conn->uport) % udphtabsz];
	} else {
	    hash = &udphtab[((Uint) conn->addr->ipnum.in.addr.s_addr ^
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
 * NAME:        conn->block()
 * DESCRIPTION: block or unblock input from connection
 */
void conn_block(connection *conn, int flag)
{
    if (conn->fd != INVALID_SOCKET) {
	if (flag) {
	    FD_CLR(conn->fd, &infds);
	    FD_CLR(conn->fd, &readfds);
	} else {
	    FD_SET(conn->fd, &infds);
	}
    }
}

/*
 * NAME:	conn->udprecv6()
 * DESCRIPTION:	receive an UDP packet
 */
static void conn_udprecv6(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in6 from;
    int fromlen;
    register int size;
    register connection **hash, *conn;
    register char *p;

    for (;;) {
	memset(buffer, '\0', UDPHASHSZ);
	fromlen = sizeof(struct sockaddr_in6);
	size = recvfrom(udescs[n].in6, buffer, BINBUF_SIZE, 0,
			(struct sockaddr *) &from, &fromlen);
	if (size < 0) {
	    return;
	}

	hash = &udphtab[(hashmem((char *) &from.sin6_addr,
				 sizeof(struct in6_addr)) ^
						from.sin6_port) % udphtabsz];
	for (;;) {
	    conn = *hash;
	    if (conn == (connection *) NULL) {
		/*
		 * see if the packet matches an outstanding challenge
		 */
		hash = (connection **) ht_lookup(chtab, buffer, FALSE);
		while ((conn=*hash) != (connection *) NULL &&
		       memcmp(conn->chain.name, buffer, UDPHASHSZ) == 0) {
		    if (conn->bufsz == size &&
			memcmp(conn->udpbuf, buffer, size) == 0 &&
			conn->addr->ipnum.ipv6 &&
			memcmp(&conn->addr->ipnum, &from.sin6_addr,
			       sizeof(struct in6_addr)) == 0) {
			/*
			 * attach new UDP channel
			 */
			*hash = (connection *) conn->chain.next;
			conn->chain.name = (char *) NULL;
			conn->bufsz = 0;
			conn->uport = from.sin6_port;
			hash = &udphtab[(hashmem((char *) &from.sin6_addr,
			   sizeof(struct in6_addr)) ^ conn->uport) % udphtabsz];
			conn->chain.next = (hte *) *hash;
			*hash = conn;

			break;
		    }
		    hash = (connection **) &conn->chain.next;
		}
		break;
	    }

	    if (conn->at == n && conn->uport == from.sin6_port &&
		memcmp(&conn->addr->ipnum, &from.sin6_addr,
		       sizeof(struct in6_addr)) == 0) {
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
 * NAME:	conn->udprecv()
 * DESCRIPTION:	receive an UDP packet
 */
static void conn_udprecv(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in from;
    int fromlen;
    register int size;
    register connection **hash, *conn;
    register char *p;

    for (;;) {
	memset(buffer, '\0', UDPHASHSZ);
	fromlen = sizeof(struct sockaddr_in);
	size = recvfrom(udescs[n].in4, buffer, BINBUF_SIZE, 0,
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
			!conn->addr->ipnum.ipv6 &&
			conn->addr->ipnum.in.addr.s_addr ==
							from.sin_addr.s_addr) {
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
		conn->addr->ipnum.in.addr.s_addr == from.sin_addr.s_addr &&
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
    int retval, n;

    /*
     * First, check readability and writability for binary sockets with pending
     * data only.
     */
    memcpy(&readfds, &infds, sizeof(fd_set));
    if (flist == (connection *) NULL) {
	/* can't accept new connections, so don't check for them */
	for (n = ntdescs; n != 0; ) {
	    --n;
	    if (tdescs[n].in6 != INVALID_SOCKET) {
		FD_CLR(tdescs[n].in6, &readfds);
	    }
	    if (tdescs[n].in4 != INVALID_SOCKET) {
		FD_CLR(tdescs[n].in4, &readfds);
	    }
	}
	for (n = nbdescs; n != 0; ) {
	    --n;
	    if (bdescs[n].in6 != INVALID_SOCKET) {
		FD_CLR(bdescs[n].in6, &readfds);
	    }
	    if (bdescs[n].in4 != INVALID_SOCKET) {
		FD_CLR(bdescs[n].in4, &readfds);
	    }
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
	retval = select(0, &readfds, &writefds, (fd_set *) NULL, &timeout);
    } else {    
	retval = select(0, &readfds, &writefds, (fd_set *) NULL,
			(struct timeval *) NULL);
    }
    if (retval == SOCKET_ERROR) {
	FD_ZERO(&readfds);
	retval = 0;
    }

    /* check for UDP packets */
    for (n = 0; n < nbdescs; n++) {
	if (udescs[n].in6 != INVALID_SOCKET &&
	    FD_ISSET(udescs[n].in6, &readfds)) {
	    conn_udprecv6(n);
	}
	if (udescs[n].in4 != INVALID_SOCKET &&
	    FD_ISSET(udescs[n].in4, &readfds)) {
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
    select(0, (fd_set *) NULL, &writefds, (fd_set *) NULL, &timeout);

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

    if (conn->fd == INVALID_SOCKET) {
	return -1;
    }
    if (!FD_ISSET(conn->fd, &readfds)) {
	return 0;
    }
    size = recv(conn->fd, buf, len, 0);
    if (size == SOCKET_ERROR) {
	closesocket(conn->fd);
	FD_CLR(conn->fd, &infds);
	FD_CLR(conn->fd, &outfds);
	FD_CLR(conn->fd, &waitfds);
	conn->fd = INVALID_SOCKET;
	closed++;
    }
    return (size == 0 || size == SOCKET_ERROR) ? -1 : size;
}

/*
 * NAME:	conn->udpread()
 * DESCRIPTION:	read a message from a UDP channel
 */
int conn_udpread(register connection *conn, char *buf, unsigned int len)
{
    register unsigned short size, n;
    register char *p, *q;

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

    if (conn->fd == INVALID_SOCKET) {
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
    if ((size=send(conn->fd, buf, len, 0)) == SOCKET_ERROR &&
	WSAGetLastError() != WSAEWOULDBLOCK) {
	closesocket(conn->fd);
	FD_CLR(conn->fd, &infds);
	FD_CLR(conn->fd, &outfds);
	conn->fd = INVALID_SOCKET;
	closed++;
    } else if ((unsigned int) size != len) {
	/* waiting for wrdone */
	FD_SET(conn->fd, &waitfds);
	FD_CLR(conn->fd, &writefds);
	if (size == SOCKET_ERROR) {
	    return 0;
	}
    }
    return (size == SOCKET_ERROR) ? -1 : size;
}

/*
 * NAME:	conn->udpwrite()
 * DESCRIPTION:	write a message to a UDP channel
 */
int conn_udpwrite(connection *conn, char *buf, unsigned int len)
{
    if (conn->fd >= 0) {
	if (conn->addr->ipnum.ipv6) {
	    struct sockaddr_in6 to;

	    memset(&to, '\0', sizeof(struct sockaddr_in6));
	    to.sin6_family = AF_INET6;
	    memcpy(&to.sin6_addr, &conn->addr->ipnum.in.addr6,
		   sizeof(struct in6_addr));
	    to.sin6_port = conn->uport;
	    return sendto(udescs[conn->at].in6, buf, len, 0,
			  (struct sockaddr *) &to, sizeof(struct sockaddr_in6));
	} else {
	    struct sockaddr_in to;

	    to.sin_family = AF_INET;
	    to.sin_addr.s_addr = conn->addr->ipnum.in.addr.s_addr;
	    to.sin_port = conn->uport;
	    return sendto(udescs[conn->at].in4, buf, len, 0,
			  (struct sockaddr *) &to, sizeof(struct sockaddr_in));
	}
    }
    return -1;
}

/*
 * NAME:	conn->wrdone()
 * DESCRIPTION:	return TRUE if a connection is ready for output
 */
bool conn_wrdone(connection *conn)
{
    if (conn->fd == INVALID_SOCKET || !FD_ISSET(conn->fd, &waitfds)) {
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
    if (conn->addr->ipnum.ipv6) {
	struct sockaddr_in6 sin6;
	DWORD length;

	memset(&sin6, '\0', sizeof(struct sockaddr_in6));
	sin6.sin6_family = AF_INET6;
	length = 40;
	sin6.sin6_addr = conn->addr->ipnum.in.addr6;
	WSAAddressToString((struct sockaddr *) &sin6,
			   sizeof(struct sockaddr_in6),
			   (LPWSAPROTOCOL_INFO) NULL, buf, &length);
    } else {
	strcpy(buf, inet_ntoa(conn->addr->ipnum.in.addr));
    }
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
