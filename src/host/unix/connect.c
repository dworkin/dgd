# include <sys/time.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <signal.h>
# include <errno.h>
# define INCLUDE_FILE_IO
# include "dgd.h"
# include "hash.h"
# include "comm.h"

# ifdef INET6		/* INET6 defined */
#  if INET6 == 0
#   undef INET6		/* ... but turned off */
#  endif
# else
#  ifdef AF_INET6	/* define INET6 if AT_INET6 exists */
#   define INET6
#  endif
# endif

# ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN	256
# endif

# ifndef INADDR_NONE
# define INADDR_NONE	0xffffffffL
# endif

# define NFREE		32

typedef struct {
    union {
# ifdef INET6
	struct in6_addr addr6;		/* IPv6 addr */
# endif
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

static int in = -1, out = -1;		/* pipe to/from name resolver */
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
    char buf[sizeof(in46addr)];
    struct hostent *host;
    register int len;

    signal(SIGINT, SIG_IGN);
    signal(SIGTRAP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    while (read(in, buf, sizeof(in46addr)) > 0) {
	/* lookup host */
# ifdef INET6
	if (((in46addr *) &buf)->ipv6) {
	    host = gethostbyaddr(buf, sizeof(struct in6_addr), AF_INET6);
	    if (host == (struct hostent *) NULL) {
		sleep(2);
		host = gethostbyaddr(buf, sizeof(struct in6_addr), AF_INET6);
	    }
	} else
# endif
	{
	    host = gethostbyaddr(buf, sizeof(struct in_addr), AF_INET);
	    if (host == (struct hostent *) NULL) {
		sleep(2);
		host = gethostbyaddr(buf, sizeof(struct in_addr), AF_INET);
	    }
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

    exit(0);	/* pipe closed */
}

/*
 * NAME:	ipaddr->init()
 * DESCRIPTION:	initialize name lookup
 */
static bool ipa_init(maxusers)
int maxusers;
{
    if (in < 0) {
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
    } else if (busy) {
	char buf[MAXHOSTNAMELEN];

	/* discard ip name */
	read(in, buf, MAXHOSTNAMELEN);
    }

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
}

/*
 * NAME:	ipaddr->new()
 * DESCRIPTION:	return a new ipaddr
 */
static ipaddr *ipa_new(ipnum)
in46addr *ipnum;
{
    register ipaddr *ipa, **hash;

    /* check hash table */
# ifdef INET6
    if (ipnum->ipv6) {
	hash = &ipahtab[hashmem((char *) ipnum,
			sizeof(struct in6_addr)) % ipahtabsz];
    } else
# endif
    {
	hash = &ipahtab[(Uint) ipnum->in.addr.s_addr % ipahtabsz];
    }
    while (*hash != (ipaddr *) NULL) {
	ipa = *hash;
# ifdef INET6
	if (ipnum->ipv6 == ipa->ipnum.ipv6 &&
	    ((ipnum->ipv6) ?
	      memcmp(&ipnum->in.addr6, &ipa->ipnum.in.addr6,
		     sizeof(struct in6_addr)) == 0 :
	      ipnum->in.addr.s_addr == ipa->ipnum.in.addr.s_addr)) {
# else
	if (ipnum->in.addr.s_addr == ipa->ipnum.in.addr.s_addr) {
# endif
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
		    write(out, (char *) ipnum, sizeof(in46addr));
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
	register ipaddr **h;

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
# ifdef INET6
	    if (ipa->ipnum.ipv6) {
		h = &ipahtab[hashmem((char *) &ipa->ipnum,
				     sizeof(struct in6_addr)) % ipahtabsz];
	    } else
# endif
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
	write(out, (char *) ipnum, sizeof(in46addr));
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
	write(out, (char *) &ipa->ipnum, sizeof(in46addr));
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
    struct _connection_ *next;		/* next in list */
};

typedef struct {
    int in6;				/* IPv6 port descriptor */
    int in4;				/* IPv4 port descriptor */
} portdesc;

static int nusers;			/* # of users */
static connection *connections;		/* connections array */
static connection *flist;		/* list of free connections */
static connection **udphtab;		/* UDP hash table */
static int udphtabsz;			/* UDP hash table size */
static hashtab *chtab;			/* challenge hash table */
static portdesc *tdescs, *bdescs;	/* telnet & binary descriptor arrays */
static int ntdescs, nbdescs;		/* # telnet & binary ports */
static portdesc *udescs;		/* UDP port descriptor array */
static fd_set infds;			/* file descriptor input bitmap */
static fd_set outfds;			/* file descriptor output bitmap */
static fd_set waitfds;			/* file descriptor wait-write bitmap */
static fd_set readfds;			/* file descriptor read bitmap */
static fd_set writefds;			/* file descriptor write map */
static int maxfd;			/* largest fd opened yet */
static int npackets;			/* # packets buffered */
static int closed;			/* #fds closed in write */

# ifdef INET6
/*
 * NAME:	conn->port6()
 * DESCRIPTION:	open an IPv6 port
 */
static int conn_port6(fd, type, sin6, port)
register int *fd;
int type;
struct sockaddr_in6 *sin6;
unsigned int port;
{
    int on;

    if ((*fd=socket(AF_INET6, type, 0)) < 0) {
	perror("socket IPv6");
	return FALSE;
    }
    if (*fd > maxfd) {
	maxfd = *fd;
    }
    on = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
# ifdef SO_OOBINLINE
    if (type == SOCK_STREAM) {
	on = 1;
	if (setsockopt(*fd, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on)) < 0) {
	    perror("setsockopt");
	    return FALSE;
	}
    }
# endif
    sin6->sin6_port = htons(port);
    if (bind(*fd, (struct sockaddr *) sin6, sizeof(struct sockaddr_in6)) < 0) {
	perror("bind");
	return FALSE;
    }

    FD_SET(*fd, &infds);
    return TRUE;
}
# endif

/*
 * NAME:	conn->port()
 * DESCRIPTION:	open an IPv4 port
 */
static int conn_port(fd, type, sin, port)
register int *fd;
int type;
struct sockaddr_in *sin;
unsigned int port;
{
    int on;

    if ((*fd=socket(AF_INET, type, 0)) < 0) {
	perror("socket");
	return FALSE;
    }
    if (*fd > maxfd) {
	maxfd = *fd;
    }
    on = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
# ifdef SO_OOBINLINE
    if (type == SOCK_STREAM) {
	on = 1;
	if (setsockopt(*fd, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on)) < 0) {
	    perror("setsockopt");
	    return FALSE;
	}
    }
# endif
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
bool conn_init(maxusers, thosts, bhosts, tports, bports, ntports, nbports)
int maxusers, ntports, nbports;
char **thosts, **bhosts;
unsigned short *tports, *bports;
{
# ifdef INET6
    struct sockaddr_in6 sin6;
# endif
    struct sockaddr_in sin;
    struct hostent *host;
    register int n;
    register connection *conn;
    bool ipv6, ipv4;
    int err;

    if (!ipa_init(maxusers)) {
	return FALSE;
    }

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
	tdescs = ALLOC(portdesc, ntports);
	memset(tdescs, -1, ntports * sizeof(portdesc));
    }
    nbdescs = nbports;
    if (nbports != 0) {
	bdescs = ALLOC(portdesc, nbports);
	memset(bdescs, -1, nbports * sizeof(portdesc));
	udescs = ALLOC(portdesc, nbports);
	memset(udescs, -1, nbports * sizeof(portdesc));
    }

# ifdef INET6
    memset(&sin6, '\0', sizeof(sin6));
    sin6.sin6_family = AF_INET6;
# endif
    memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;

    for (n = 0; n < ntdescs; n++) {
	/* telnet ports */
	ipv6 = FALSE;
	ipv4 = FALSE;
	if (thosts[n] == (char *) NULL) {
# ifdef INET6
	    sin6.sin6_addr = in6addr_any;
	    ipv6 = TRUE;
# endif
	    sin.sin_addr.s_addr = INADDR_ANY;
	    ipv4 = TRUE;
	} else {
# ifdef INET6
	    if (inet_pton(AF_INET6, thosts[n], &sin6) > 0) {
		ipv6 = TRUE;
	    } else {
# ifdef AI_DEFAULT
		host = getipnodebyname(thosts[n], AF_INET6, 0, &err);
		if (host != (struct hostent *) NULL) {
		    memcpy(&sin6.sin6_addr, host->h_addr, host->h_length);
		    ipv6 = TRUE;
		    freehostent(host);
		}
# else
		host = gethostbyname2(thosts[n], AF_INET6);
		if (host != (struct hostent *) NULL) {
		    memcpy(&sin6.sin6_addr, host->h_addr, host->h_length);
		    ipv6 = TRUE;
		}
# endif
	    }
# endif
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

# ifdef INET6
	if (ipv6 && !conn_port6(&tdescs[n].in6, SOCK_STREAM, &sin6, tports[n]))
	{
	    return FALSE;
	}
# endif
	if (ipv4 && !conn_port(&tdescs[n].in4, SOCK_STREAM, &sin, tports[n])) {
	    return FALSE;
	}
    }

    for (n = 0; n < nbdescs; n++) {
	/* binary ports */
	ipv6 = FALSE;
	ipv4 = FALSE;
	if (bhosts[n] == (char *) NULL) {
# ifdef INET6
	    sin6.sin6_addr = in6addr_any;
	    ipv6 = TRUE;
# endif
	    sin.sin_addr.s_addr = INADDR_ANY;
	    ipv4 = TRUE;
	} else {
# ifdef INET6
	    if (inet_pton(AF_INET6, bhosts[n], &sin6) > 0) {
		ipv6 = TRUE;
	    } else {
# ifdef AI_DEFAULT
		host = getipnodebyname(bhosts[n], AF_INET6, 0, &err);
		if (host != (struct hostent *) NULL) {
		    memcpy(&sin6.sin6_addr, host->h_addr, host->h_length);
		    ipv6 = TRUE;
		    freehostent(host);
		}
# else
		host = gethostbyname2(bhosts[n], AF_INET6);
		if (host != (struct hostent *) NULL) {
		    memcpy(&sin6.sin6_addr, host->h_addr, host->h_length);
		    ipv6 = TRUE;
		}
# endif
	    }
# endif
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

# ifdef INET6
	if (ipv6) {
	    if (!conn_port6(&bdescs[n].in6, SOCK_STREAM, &sin6, bports[n])) {
		return FALSE;
	    }
	    if (!conn_port6(&udescs[n].in6, SOCK_DGRAM, &sin6, bports[n])) {
		return FALSE;
	    }
	}
# endif
	if (ipv4) {
	    if (!conn_port(&bdescs[n].in4, SOCK_STREAM, &sin, bports[n])) {
		return FALSE;
	    }
	    if (!conn_port(&udescs[n].in4, SOCK_DGRAM, &sin, bports[n])) {
		return FALSE;
	    }
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
    register int n;
    register connection *conn;

    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	if (conn->fd >= 0) {
	    close(conn->fd);
	}
    }
    for (n = 0; n < ntdescs; n++) {
	if (tdescs[n].in6 >= 0) {
	    close(tdescs[n].in6);
	}
	if (tdescs[n].in4 >= 0) {
	    close(tdescs[n].in4);
	}
    }
    for (n = 0; n < nbdescs; n++) {
	if (bdescs[n].in6 >= 0) {
	    close(bdescs[n].in6);
	}
	if (bdescs[n].in4 >= 0) {
	    close(bdescs[n].in4);
	}
	if (udescs[n].in6 >= 0) {
	    close(udescs[n].in6);
	}
	if (udescs[n].in4 >= 0) {
	    close(udescs[n].in4);
	}
    }

    ipa_finish();
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen()
{
    register int n;

    for (n = 0; n < ntdescs; n++) {
	if (tdescs[n].in6 >= 0) {
	    if (listen(tdescs[n].in6, 64) < 0) {
		perror("listen");
	    } else if (fcntl(tdescs[n].in6, F_SETFL, FNDELAY) < 0) {
		perror("fcntl");
	    } else {
		continue;
	    }
	    fatal("conn_listen failed");
	}
    }
    for (n = 0; n < ntdescs; n++) {
	if (tdescs[n].in4 >= 0) {
	    if (listen(tdescs[n].in4, 64) < 0) {
# ifdef INET6
		close(tdescs[n].in4);
		FD_CLR(tdescs[n].in4, &infds);
		tdescs[n].in4 = -1;
		continue;
# else
		perror("listen");
# endif
	    } else if (fcntl(tdescs[n].in4, F_SETFL, FNDELAY) < 0) {
		perror("fcntl");
	    } else {
		continue;
	    }
	    fatal("conn_listen failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	if (bdescs[n].in6 >= 0) {
	    if (listen(bdescs[n].in6, 64) < 0) {
		perror("listen");
	    } else if (fcntl(bdescs[n].in6, F_SETFL, FNDELAY) < 0) {
		perror("fcntl");
	    } else if (fcntl(udescs[n].in6, F_SETFL, FNDELAY) < 0) {
		perror("fcntl");
	    } else {
		continue;
	    }
	    fatal("conn_listen failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	if (bdescs[n].in4 >= 0) {
	    if (listen(bdescs[n].in4, 64) < 0) {
# ifdef INET6
		close(bdescs[n].in4);
		FD_CLR(bdescs[n].in4, &infds);
		bdescs[n].in4 = -1;
		continue;
# else
		perror("listen");
# endif
	    } else if (fcntl(bdescs[n].in4, F_SETFL, FNDELAY) < 0) {
		perror("fcntl");
	    } else if (fcntl(udescs[n].in4, F_SETFL, FNDELAY) < 0) {
		perror("fcntl");
	    } else {
		continue;
	    }
	    fatal("conn_listen failed");
	}
    }
}

# ifdef INET6
/*
 * NAME:	conn->accept6()
 * DESCRIPTION:	accept a new ipv6 connection
 */
static connection *conn_accept6(portfd, port)
int portfd, port;
{
    int fd, len;
    struct sockaddr_in6 sin6;
    in46addr addr;
    register connection *conn;

    if (!FD_ISSET(portfd, &readfds)) {
	return (connection *) NULL;
    }
    len = sizeof(sin6);
    fd = accept(portfd, (struct sockaddr *) &sin6, &len);
    if (fd < 0) {
	FD_CLR(portfd, &readfds);
	return (connection *) NULL;
    }
    fcntl(fd, F_SETFL, FNDELAY);

    conn = flist;
    flist = (connection *) conn->chain.next;
    conn->chain.name = (char *) NULL;
    conn->fd = fd;
    conn->udpbuf = (char *) NULL;
    if (IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) {
	/* convert to IPv4 address */
	addr.in.addr = *(struct in_addr *) &sin6.sin6_addr.s6_addr[12];
	addr.ipv6 = FALSE;
    } else {
	addr.in.addr6 = sin6.sin6_addr;
	addr.ipv6 = TRUE;
    }
    conn->addr = ipa_new(&addr);
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
# endif

/*
 * NAME:	conn->accept()
 * DESCRIPTION:	accept a new ipv4 connection
 */
static connection *conn_accept(portfd, port)
int portfd, port;
{
    int fd, len;
    struct sockaddr_in sin;
    in46addr addr;
    register connection *conn;

    if (!FD_ISSET(portfd, &readfds)) {
	return (connection *) NULL;
    }
    len = sizeof(sin);
    fd = accept(portfd, (struct sockaddr *) &sin, &len);
    if (fd < 0) {
	FD_CLR(portfd, &readfds);
	return (connection *) NULL;
    }
    fcntl(fd, F_SETFL, FNDELAY);

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
    if (fd > maxfd) {
	maxfd = fd;
    }

    return conn;
}

/*
 * NAME:	conn->tnew6()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew6(int port)
{
# ifdef INET6
    int fd;

    fd = tdescs[port].in6;
    if (fd >= 0) {
	return conn_accept6(fd, port);
    }
# endif
    return (connection *) NULL;
}

/*
 * NAME:	conn->bnew6()
 * DESCRIPTION:	accept a new binary connection
 */
connection *conn_bnew6(int port)
{
# ifdef INET6
    int fd;

    fd = bdescs[port].in6;
    if (fd >= 0) {
	return conn_accept6(fd, port);
    }
# endif
    return (connection *) NULL;
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew(int port)
{
    int fd;

    fd = tdescs[port].in4;
    if (fd >= 0) {
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
    int fd;

    fd = bdescs[port].in4;
    if (fd >= 0) {
	return conn_accept(fd, port);
    }
    return (connection *) NULL;
}

/*
 * NAME:	conn->udp()
 * DESCRIPTION:	set the challenge for attaching a UDP channel
 */
bool conn_udp(conn, challenge, len)
register connection *conn;
char *challenge;
register unsigned int len;
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
	if (conn->chain.name != (char *) NULL) {
	    hash = (connection **) ht_lookup(chtab, conn->chain.name, FALSE);
# ifdef INET6
	} else if (conn->addr->ipnum.ipv6) {
	    hash = &udphtab[(hashmem((char *) &conn->addr->ipnum,
			sizeof(struct in6_addr)) ^ conn->uport) % udphtabsz];
# endif
	} else {
	    hash = &udphtab[(((Uint) conn->addr->ipnum.in.addr.s_addr) ^
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

# ifdef INET6
/*
 * NAME:	conn->udprecv6()
 * DESCRIPTION:	receive an UDP packet
 */
static void conn_udprecv6(n)
int n;
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
# endif

/*
 * NAME:	conn->udprecv()
 * DESCRIPTION:	receive an UDP packet
 */
static void conn_udprecv(n)
int n;
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
int conn_select(t, mtime)
Uint t;
unsigned int mtime;
{
    struct timeval timeout;
    int retval;
    register int n;

    /*
     * First, check readability and writability for binary sockets with pending
     * data only.
     */
    memcpy(&readfds, &infds, sizeof(fd_set));
    if (flist == (connection *) NULL) {
	/* can't accept new connections, so don't check for them */
	for (n = ntdescs; n != 0; ) {
	    --n;
	    if (tdescs[n].in6 >= 0) {
		FD_CLR(tdescs[n].in6, &readfds);
	    }
	    if (tdescs[n].in4 >= 0) {
		FD_CLR(tdescs[n].in4, &readfds);
	    }
	}
	for (n = nbdescs; n != 0; ) {
	    --n;
	    if (bdescs[n].in6 >= 0) {
		FD_CLR(bdescs[n].in6, &readfds);
	    }
	    if (bdescs[n].in4 >= 0) {
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

    /* check for UDP packets */
    for (n = 0; n < nbdescs; n++) {
# ifdef INET6
	if (udescs[n].in6 >= 0 && FD_ISSET(udescs[n].in6, &readfds)) {
	    conn_udprecv6(n);
	}
# endif
	if (udescs[n].in4 >= 0 && FD_ISSET(udescs[n].in4, &readfds)) {
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
bool conn_udpcheck(conn)
connection *conn;
{
    return (conn->chain.name == (char *) NULL);
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
    if (size < 0) {
	close(conn->fd);
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
int conn_udpread(conn, buf, len)
register connection *conn;
char *buf;
unsigned int len;
{
    register unsigned short size, n;
    register char *p, *q;

    while (conn->bufsz != 0) {
	/* udp buffer is not empty */
	size = (UCHAR(conn->udpbuf[0]) << 8) | UCHAR(conn->udpbuf[1]);
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
int conn_write(conn, buf, len)
register connection *conn;
char *buf;
unsigned int len;
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
int conn_udpwrite(conn, buf, len)
register connection *conn;
char *buf;
unsigned int len;
{
    if (conn->fd >= 0) {
# ifdef INET6
	if (conn->addr->ipnum.ipv6) {
	    struct sockaddr_in6 to;
 
	    memset(&to, '\0', sizeof(struct sockaddr_in6));
	    to.sin6_family = AF_INET6;
	    memcpy(&to.sin6_addr, &conn->addr->ipnum.in.addr6,
		   sizeof(struct in6_addr));
	    to.sin6_port = conn->uport;
	    return sendto(udescs[conn->at].in6, buf, len, 0,
			  (struct sockaddr *) &to, sizeof(struct sockaddr_in6));
	} else
# endif
	{
	    struct sockaddr_in to;
 
	    memset(&to, '\0', sizeof(struct sockaddr_in));
	    to.sin_family = AF_INET;
	    to.sin_addr = conn->addr->ipnum.in.addr;
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
void conn_ipnum(conn, buf)
connection *conn;
char *buf;
{
# ifdef INET6
    /* IPv6: maxlen 39 */
    if (conn->addr->ipnum.ipv6) {
	inet_ntop(AF_INET6, &conn->addr->ipnum, buf,
		  sizeof(struct sockaddr_in6));
    } else
# endif
    {
	strcpy(buf, inet_ntoa(conn->addr->ipnum.in.addr));
    }

}

/*
 * NAME:	conn->ipname()
 * DESCRIPTION:	return the ip name of a connection
 */
void conn_ipname(conn, buf)
connection *conn;
char *buf;
{
    if (conn->addr->name[0] != '\0') {
	strcpy(buf, conn->addr->name);
    } else {
	conn_ipnum(conn, buf);
    }
}
