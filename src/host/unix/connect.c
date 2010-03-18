/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

#ifdef NETWORK_EXTENSIONS
#undef INET6
#undef AF_INET6
#endif

# ifdef INET6		/* INET6 defined */
#  if INET6 == 0
#   undef INET6		/* ... but turned off */
#  endif
# else
#  ifdef AF_INET6	/* define INET6 if AF_INET6 exists */
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
#ifdef NETWORK_EXTENSIONS
static int addrtype;                    /* network address family */
#endif
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
static void ipa_run(int in, int out)
{
    char buf[sizeof(in46addr)];
    struct hostent *host;
    int len;

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
	    char *name;

	    /* write host name */
	    name = (char *) host->h_name;
	    len = strlen(name);
	    if (len >= MAXHOSTNAMELEN) {
		len = MAXHOSTNAMELEN - 1;
	    }
	    write(out, name, len);
	    name[0] = '\0';
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
static bool ipa_init(int maxusers)
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
static ipaddr *ipa_new(in46addr *ipnum)
{
    ipaddr *ipa, **hash;

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
static int conn_port6(int *fd, int type, struct sockaddr_in6 *sin6, unsigned int port)
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
static int conn_port(int *fd, int type, struct sockaddr_in *sin, unsigned int port)
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
bool conn_init(int maxusers, char **thosts, char **bhosts, 
	unsigned short *tports, unsigned short *bports, int ntports, 
	int nbports)
{
# ifdef INET6
    struct sockaddr_in6 sin6;
# endif
    struct sockaddr_in sin;
    struct hostent *host;
    int n;
    connection *conn;
    bool ipv6, ipv4;
    int err;

    if (!ipa_init(maxusers)) {
	return FALSE;
    }

#ifdef NETWORK_EXTENSIONS
    addrtype = PF_INET;
#endif

    nusers = 0;
    
    maxfd = 0;
    FD_ZERO(&infds);
    FD_ZERO(&outfds);
    FD_ZERO(&waitfds);
    FD_SET(in, &infds);
    npackets = 0;
    closed = 0;

#ifndef NETWORK_EXTENSIONS
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
#endif

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
#ifndef NETWORK_EXTENSIONS
    connections = ALLOC(connection, nusers = maxusers);
#else
    connections = ALLOC(connection, nusers = maxusers+1);
#endif
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = -1;
	conn->chain.next = (hte *) flist;
	flist = conn;
    }

#ifndef NETWORK_EXTENSIONS
    udphtab = ALLOC(connection*, udphtabsz = maxusers);
    memset(udphtab, '\0', udphtabsz * sizeof(connection*));
    chtab = ht_new(maxusers, UDPHASHSZ, TRUE);
#endif

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

#ifndef NETWORK_EXTENSIONS
/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen()
{
    int n;

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
static connection *conn_accept6(int portfd, int port)
{
    int fd;
    unsigned int len;
    struct sockaddr_in6 sin6;
    in46addr addr;
    connection *conn;

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

/* #ifndef NETWORK_EXTENSIONS */
/*
 * NAME:	conn->accept()
 * DESCRIPTION:	accept a new ipv4 connection
 */
static connection *conn_accept(int portfd, int port)
{
    int fd;
    unsigned int len;
    struct sockaddr_in sin;
    in46addr addr;
    connection *conn;

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
/* #endif */

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
#endif

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
#ifndef NETWORK_EXTENSIONS
    ipa_del(conn->addr);
#else
    if (conn->addr != (ipaddr *) NULL)
    {
      ipa_del(conn->addr);
    }
#endif
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

# ifdef INET6
/*
 * NAME:	conn->udprecv6()
 * DESCRIPTION:	receive an UDP packet
 */
static void conn_udprecv6(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in6 from;
    unsigned int fromlen;
    int size;
    connection **hash, *conn;
    char *p;

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
# endif

/*
 * NAME:	conn->udprecv()
 * DESCRIPTION:	receive an UDP packet
 */
static void conn_udprecv(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in from;
    unsigned int fromlen;
    int size;
    connection **hash, *conn;
    char *p;

    memset(buffer, '\0', UDPHASHSZ);
    fromlen = sizeof(struct sockaddr_in);
    size = recvfrom(udescs[n].in4, buffer, BINBUF_SIZE, 0,
		    (struct sockaddr *) &from, &fromlen);
    if (size < 0) {
	return;
    }

    hash = &udphtab[((Uint) from.sin_addr.s_addr ^ from.sin_port) % udphtabsz];
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
		    conn->addr->ipnum.in.addr.s_addr == from.sin_addr.s_addr) {
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
int conn_udpread(connection *conn, char *buf, unsigned int len)
{
    unsigned short size, n;
    char *p, *q;

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
int conn_udpwrite(connection *conn, char *buf, unsigned int len)
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

#ifdef NETWORK_EXTENSIONS
int conn_udpsend(connection *conn, char *buf, unsigned int len, char *addr, 
	unsigned short port)
{
    struct sockaddr_in to;

    to.sin_family=addrtype;
    inet_aton(addr, &to.sin_addr); /* should have been checked for valid 
				      addresses already, so it should not
				      fail */
    to.sin_port = htons(port);
    if (!sendto(conn->fd, buf, len, 0, (struct sockaddr *) &to,
		sizeof(struct sockaddr_in)))
    {
	if (errno==EAGAIN) {
	    return -1;
	}
	perror("sendto");
	return -2;
    }
    return 0;
}

/*
 * NAME:	conn->checkaddr()
 * DESCRIPTION:	checks for valid ip address
 */
int conn_checkaddr(char *ip)
{
    struct in_addr dummy;
    return inet_aton(ip, &dummy);
}
#endif

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
# ifdef INET6
    /* IPv6: maxlen 39 */
    if (conn->addr->ipnum.ipv6) {
	inet_ntop(AF_INET6, &conn->addr->ipnum, buf, 40);
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
void conn_ipname(connection *conn, char *buf)
{
    if (conn->addr->name[0] != '\0') {
	strcpy(buf, conn->addr->name);
    } else {
	conn_ipnum(conn, buf);
    }
}

#ifdef NETWORK_EXTENSIONS
 /*
  * Name:        conn->openlisten()
  * DESCRIPTION: open a new listening connection
  */
 connection *
 conn_openlisten(unsigned char protocol, unsigned short port)
 {
     struct sockaddr_in sin;  
     connection *conn;
     int on, n, sock;
     unsigned int sz;
     
 
     switch (protocol){
     case P_TCP:
 	sock=socket(addrtype, SOCK_STREAM, 0);
 	if (sock<0){
 	    perror("socket");
 	    return NULL;
 	}
 	on=1;
 	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
 		       sizeof(on))<0){
 	    perror("setsockopt");
 	    close(sock);
 	    return NULL;
 	}
 #ifdef SO_OOBINLINE
 	on=1;
 	if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *) &on,
 		       sizeof(on))<0) {
 	    perror("setsockopt");
 	    close(sock);
 	    return NULL;
 	}
 #endif
 	memset(&sin, '\0', sizeof(sin));
 	sin.sin_port = htons(port);
 	sin.sin_family = addrtype;
 	sin.sin_addr.s_addr = INADDR_ANY;
 	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
 	    perror("bind");
 	    close(sock);
 	    return NULL;
 	}
 
 	if (listen(sock,64)) {
 	    perror("listen");
 	    close(sock);
 	    return NULL;
 	}
 
 	FD_SET(sock, &infds);
 	if (maxfd < sock) {
 	    maxfd = sock;
 	}
 	
 	conn=flist;
 	flist = (connection *) conn->chain.next;
 	conn->fd=sock;
 	sz=sizeof(sin);
 	getsockname(conn->fd, (struct sockaddr *) &sin, &sz);
 	conn->at=ntohs(sin.sin_port);
 	return conn;
     case P_UDP:
 	sock=socket(addrtype, SOCK_DGRAM, 0);
 	if (sock<0) {
 	    perror("socket");
 	    return NULL;
 	}
 	on=0;
 	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
 		       sizeof(on))<0) {
 	    perror("setsockopt");
 	    close(sock);
 	    return NULL;
 	}
 	memset(&sin, '\0', sizeof(sin));
 	sin.sin_port=htons(port);
 	sin.sin_family=addrtype;
 	sin.sin_addr.s_addr=INADDR_ANY;
 	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
 	    perror("bind");
 	    close(sock);
 	    return NULL;
 	}
 	if (fcntl(sock, F_SETFL, FNDELAY)) {
 	    perror("fcntl");
 	    close(sock);
 	    return NULL;
 	}
 	FD_SET(sock, &infds);
 	if (maxfd < sock) {
 	    maxfd=sock;
 	}
 	conn=flist;
 	flist=(connection *) conn->chain.next;
 	conn->fd=sock;
 	sz=sizeof(sin);
 	getsockname(conn->fd, (struct sockaddr *) &sin, &sz);
 	conn->at=ntohs(sin.sin_port);
 	return conn;
     default:
 	return NULL;
     }
   
 }
 /*
  * NAME:	conn->port()
  * DESCRIPTION:	return the port number of a connection
  */
 int conn_at(connection *conn)
 {
     return conn->at;
 }
 
 /*
  * NAME:	conn->accept()
  * DESCRIPTION:	return a new connction structure
  */
 connection *conn_accept(connection *conn)
 {
     int fd;
     unsigned int len;
     struct sockaddr_in sin;
     in46addr addr;
     connection *newconn;
 
     if (!FD_ISSET(conn->fd, &readfds)) {
 	return (connection *) NULL;
     }
     
     len = sizeof(sin);
     fd = accept(conn->fd, (struct sockaddr *) &sin, &len);
     if (fd < 0) {
 	return (connection *) NULL;
     }
     if (fcntl(fd, F_SETFL, FNDELAY)) {
 	perror("fcntl");
 	close(fd);
 	return NULL;
     }
 
     newconn=flist;
     flist=(connection *)newconn->chain.next;
     newconn->fd=fd;
     newconn->chain.name = (char *) NULL;
     newconn->udpbuf=(char *) NULL;
     addr.in.addr = sin.sin_addr;
     addr.ipv6 = FALSE;
     newconn->addr = ipa_new(&addr);
     /* newconn->addr=ipa_new(&sin.sin_addr);  */
     newconn->at=ntohs(sin.sin_port);
     FD_SET(fd, &infds);
     FD_SET(fd, &outfds);
     FD_CLR(fd, &readfds);
     FD_SET(fd, &writefds);
     if (fd > maxfd) {
 	maxfd=fd;
     }

     return newconn;
 }
 
 
 connection *conn_connect(char *addr, unsigned short port)
 {
     connection * conn;
     int sock;
     int on;
     long arg;

     struct sockaddr_in sin;
     in46addr inaddr;
 
     if(flist == (connection *) NULL) {
        return NULL;
     }

     sock=socket(addrtype, SOCK_STREAM, 0);
     if (sock<0) {
 	perror("socket");
 	return NULL;
     }
     on=1;
     if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on, 
 		   sizeof(on))<0) {
 	perror("setsockopt");
 	return NULL;
     }
 #ifdef SO_OOBINLINE
     on=1;
     if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *) &on,
 		   sizeof(on))<0) {
 	perror("setsockopt");
 	return NULL;
     }
 #endif
     if( (arg = fcntl(sock, F_GETFL, NULL)) < 0) {
        perror("fcntl");
        return NULL;
     }
     arg |= O_NONBLOCK;
     if( fcntl(sock, F_SETFL, arg) < 0) {
        perror("fcntl");
        return NULL;
     }

     memset(&sin, '\0', sizeof(sin));
     sin.sin_port = htons(port);
     sin.sin_family = addrtype;
     if (!inet_aton(addr, &sin.sin_addr)) {
 	perror("inet_aton");
 	return NULL;
     }
     if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0 &&
                    errno != EINPROGRESS) {
 	perror("connect");
 	return NULL;
     }
     
     conn=flist;
     flist=(connection *)conn->chain.next;
     conn->fd=sock;
     conn->chain.name = (char *) NULL;
     conn->udpbuf=(char *) NULL;
     inaddr.in.addr = sin.sin_addr;
     inaddr.ipv6 = FALSE;
     conn->addr = ipa_new(&inaddr);
     conn->at=sin.sin_port;
     FD_SET(sock, &infds);
     FD_SET(sock, &outfds);
     FD_CLR(sock, &readfds);
     FD_SET(sock, &writefds);
     if (sock > maxfd) {
 	maxfd=sock;
     }
     return conn;
 }
 
 int conn_udpreceive(connection *conn, char *buffer, int size, char **host, 
	 int *port)
 {
     if (FD_ISSET(conn->fd, &readfds)) {
 	struct sockaddr_in from;
 	unsigned int fromlen;
	int sz;
 
 	fromlen=sizeof(struct sockaddr_in);
 	sz=recvfrom(conn->fd, buffer, size, 0, (struct sockaddr *) &from,
 		    &fromlen);
 	if (sz<0) {
 	    perror("recvfrom");
 	    return sz;
 	}
 	*host=inet_ntoa(from.sin_addr);
 	*port=ntohs(from.sin_port);
 	return sz;
     }
     return -1;
 }


 /*
  * check for a connection in pending state and see if it is connected.
  */
 int conn_check_connected(connection *conn)
 {
     Uint t;
     unsigned int mtime;
     int retval;
     int optval;
     fd_set fdwrite;
     socklen_t lon;
     struct timeval timeout;

     t = 0;
     mtime = 0;

     /*
      * indicate that our fd became invalid.
      */
     if(conn->fd < 0) {
       return -2;
     }

     FD_ZERO(&fdwrite);
     FD_SET(conn->fd,&fdwrite);

     timeout.tv_sec = t;
     timeout.tv_usec = mtime * 1000L;

     retval = select(conn->fd + 1, NULL, &fdwrite, NULL, &timeout);

     /*
      * Delayed connect completed, check for errors
      */
     if(retval > 0) {
         lon = sizeof(int);
         /*
          * Get error state for the socket
          */
         if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, (void*)(&optval), &lon) < 0) {
             return -1;
         }
         if (optval != 0) {
             errno = optval;
             return -1;
         } else {
             errno = 0;
             return 1;
         }
     } else if(retval < 0) {
         return -1;
     }
     return 0;
 }

 #endif
