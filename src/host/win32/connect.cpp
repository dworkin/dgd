/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
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

# define FD_SETSIZE   1024
# include <winsock2.h>
# include <ws2tcpip.h>
# include <process.h>
# define INCLUDE_FILE_IO
# include "dgd.h"
# include "hash.h"
# include "comm.h"

# define MAXHOSTNAMELEN	1025

# define NFREE		32

struct in46addr {
    union {
	struct in6_addr addr6;		/* IPv6 addr */
	struct in_addr addr;		/* IPv4 addr */
    };
    bool ipv6;				/* IPv6? */
};

struct ipaddr {
    ipaddr *link;			/* next in hash table */
    ipaddr *prev;			/* previous in linked list */
    ipaddr *next;			/* next in linked list */
    Uint ref;				/* reference count */
    in46addr ipnum;			/* ip number */
    char name[MAXHOSTNAMELEN];		/* ip name */
};

static SOCKET in = INVALID_SOCKET;	/* connection from name resolver */
static SOCKET out = INVALID_SOCKET;	/* connection to name resolver */
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
static void ipa_run(void *dummy)
{
    char buf[sizeof(in46addr)];
    struct hostent *host;
    int len;

    UNREFERENCED_PARAMETER(dummy);

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
    closesocket(out);
    out = INVALID_SOCKET;
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
static void ipa_finish()
{
    closesocket(in);
    in = INVALID_SOCKET;
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
	hash = &ipahtab[Hashtab::hashmem((char *) ipnum,
					 sizeof(struct in6_addr)) % ipahtabsz];
    } else {
	hash = &ipahtab[(Uint) ipnum->addr.s_addr % ipahtabsz];
    }
    while (*hash != (ipaddr *) NULL) {
	ipa = *hash;
	if (ipnum->ipv6 == ipa->ipnum.ipv6 &&
	    ((ipnum->ipv6) ?
	      memcmp(&ipnum->addr6, &ipa->ipnum.addr6,
		     sizeof(struct in6_addr)) == 0 :
	      ipnum->addr.s_addr == ipa->ipnum.addr.s_addr)) {
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
		h = &ipahtab[Hashtab::hashmem((char *) &ipa->ipnum,
					      sizeof(struct in6_addr)) %
								    ipahtabsz];
	    } else
	    {
		h = &ipahtab[(Uint) ipa->ipnum.addr.s_addr % ipahtabsz];
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
	Alloc::staticMode();
	ipa = ALLOC(ipaddr, 1);
	Alloc::dynamicMode();

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
static void ipa_lookup()
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


struct connection : public Hashtab::Entry {
    SOCKET fd;				/* file descriptor */
    bool udp;				/* datagram only? */
    int bufsz;				/* # bytes in buffer */
    int npkts;				/* # packets in buffer */
    char *udpbuf;			/* datagram buffer */
    ipaddr *addr;			/* internet address of connection */
    unsigned short port;		/* UDP port of connection */
    short at;				/* port connection was accepted at */
};

struct portdesc {
    SOCKET in6;				/* IPv6 socket */
    SOCKET in4;				/* IPv4 socket */
};

struct udpdesc {
    struct portdesc fd;			/* port descriptors */
    in46addr addr;			/* source of new packet */
    unsigned short port;		/* source port of new datagram */
    bool accept;			/* datagram ready to accept? */
    unsigned short hashval;		/* address hash */
    int size;				/* size in buffer */
    char buffer[BINBUF_SIZE];		/* buffer */

};

static connection **udphtab;		/* UDP hash table */
static int udphtabsz;			/* UDP hash table size */
static Hashtab *chtab;			/* challenge hash table */
static udpdesc *udescs;			/* UDP port descriptor array */
static int nudescs;			/* # datagram ports */
static SOCKET inpkts, outpkts;		/* UDP packet notification pip */
static CRITICAL_SECTION udpmutex;	/* UDP mutex */
static bool udpstop;			/* stop UDP thread? */

/*
 * NAME:	udp->recv6()
 * DESCRIPTION:	receive an UDP packet
 */
static void udp_recv6(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in6 from;
    int fromlen;
    int size;
    unsigned short hashval;
    connection **hash, *conn;
    char *p;

    memset(buffer, '\0', UDPHASHSZ);
    fromlen = sizeof(struct sockaddr_in6);
    size = recvfrom(udescs[n].fd.in6, buffer, BINBUF_SIZE, 0,
		    (struct sockaddr *) &from, &fromlen);
    if (size < 0) {
	return;
    }

    hashval = (Hashtab::hashmem((char *) &from.sin6_addr,
			        sizeof(struct in6_addr)) ^ from.sin6_port) %
								    udphtabsz;
    hash = &udphtab[hashval];
    EnterCriticalSection(&udpmutex);
    for (;;) {
	conn = *hash;
	if (conn == (connection *) NULL) {
	    if (!conf_attach(n)) {
		if (!udescs[n].accept) {
		    udescs[n].addr.addr6 = from.sin6_addr;
		    udescs[n].addr.ipv6 = TRUE;
		    udescs[n].port = from.sin6_port;
		    udescs[n].hashval = hashval;
		    udescs[n].size = size;
		    memcpy(udescs[n].buffer, buffer, size);
		    udescs[n].accept = TRUE;
		    send(outpkts, buffer, 1, 0);
		}
		break;
	    }

	    /*
	     * see if the packet matches an outstanding challenge
	     */
	    hash = (connection **) chtab->lookup(buffer, FALSE);
	    while ((conn=*hash) != (connection *) NULL &&
		   memcmp(conn->name, buffer, UDPHASHSZ) == 0) {
		if (conn->bufsz == size &&
		    memcmp(conn->udpbuf, buffer, size) == 0 &&
		    conn->addr->ipnum.ipv6 &&
		    memcmp(&conn->addr->ipnum, &from.sin6_addr,
			   sizeof(struct in6_addr)) == 0) {
		    /*
		     * attach new UDP channel
		     */
		    *hash = (connection *) conn->next;
		    conn->name = (char *) NULL;
		    conn->bufsz = 0;
		    conn->port = from.sin6_port;
		    hash = &udphtab[hashval];
		    conn->next = *hash;
		    *hash = conn;

		    break;
		}
		hash = (connection **) &conn->next;
	    }
	    break;
	}

	if (conn->at == n && conn->port == from.sin6_port &&
	    memcmp(&conn->addr->ipnum, &from.sin6_addr,
		   sizeof(struct in6_addr)) == 0) {
	    /*
	     * packet from known correspondent
	     */
	    if (conn->bufsz + size <= BINBUF_SIZE) {
		p = conn->udpbuf + conn->bufsz;
		*p++ = size >> 8;
		*p++ = size;
		memcpy(p, buffer, size);
		conn->bufsz += size + 2;
		conn->npkts++;
		send(outpkts, buffer, 1, 0);
	    }
	    break;
	}
	hash = (connection **) &conn->next;
    }
    LeaveCriticalSection(&udpmutex);
}

/*
 * NAME:	udp->recv()
 * DESCRIPTION:	receive an UDP packet
 */
static void udp_recv(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in from;
    int fromlen;
    int size;
    unsigned short hashval;
    connection **hash, *conn;
    char *p;

    memset(buffer, '\0', UDPHASHSZ);
    fromlen = sizeof(struct sockaddr_in);
    size = recvfrom(udescs[n].fd.in4, buffer, BINBUF_SIZE, 0,
		    (struct sockaddr *) &from, &fromlen);
    if (size < 0) {
	return;
    }

    hashval = ((Uint) from.sin_addr.s_addr ^ from.sin_port) % udphtabsz;
    hash = &udphtab[hashval];
    EnterCriticalSection(&udpmutex);
    for (;;) {
	conn = *hash;
	if (conn == (connection *) NULL) {
	    if (!conf_attach(n)) {
		if (!udescs[n].accept) {
		    udescs[n].addr.addr = from.sin_addr;
		    udescs[n].addr.ipv6 = FALSE;
		    udescs[n].port = from.sin_port;
		    udescs[n].hashval = hashval;
		    udescs[n].size = size;
		    memcpy(udescs[n].buffer, buffer, size);
		    udescs[n].accept = TRUE;
		    send(outpkts, buffer, 1, 0);
		}
		break;
	    }

	    /*
	     * see if the packet matches an outstanding challenge
	     */
	    hash = (connection **) chtab->lookup(buffer, FALSE);
	    while ((conn=*hash) != (connection *) NULL &&
		   memcmp((*hash)->name, buffer, UDPHASHSZ) == 0) {
		if (conn->bufsz == size &&
		    memcmp(conn->udpbuf, buffer, size) == 0 &&
		    !conn->addr->ipnum.ipv6 &&
		    conn->addr->ipnum.addr.s_addr == from.sin_addr.s_addr) {
		    /*
		     * attach new UDP channel
		     */
		    *hash = (connection *) conn->next;
		    conn->name = (char *) NULL;
		    conn->bufsz = 0;
		    conn->port = from.sin_port;
		    hash = &udphtab[hashval];
		    conn->next = *hash;
		    *hash = conn;

		    break;
		}
		hash = (connection **) &conn->next;
	    }
	    break;
	}

	if (conn->at == n &&
	    conn->addr->ipnum.addr.s_addr == from.sin_addr.s_addr &&
	    conn->port == from.sin_port) {
	    /*
	     * packet from known correspondent
	     */
	    if (conn->bufsz + size <= BINBUF_SIZE) {
		p = conn->udpbuf + conn->bufsz;
		*p++ = size >> 8;
		*p++ = size;
		memcpy(p, buffer, size);
		conn->bufsz += size + 2;
		conn->npkts++;
		send(outpkts, buffer, 1, 0);
	    }
	    break;
	}
	hash = (connection **) &conn->next;
    }
    LeaveCriticalSection(&udpmutex);
}

/*
 * NAME:	udp->run()
 * DESCRIPTION:	UDP thread
 */
static void udp_run(void *arg)
{
    fd_set udpfds;
    fd_set readfds;
    fd_set errorfds;
    int n, retval;

    UNREFERENCED_PARAMETER(arg);

    FD_ZERO(&udpfds);
    for (n = 0; n < nudescs; n++) {
	if (udescs[n].fd.in6 >= 0) {
	    FD_SET(udescs[n].fd.in6, &udpfds);
	}
	if (udescs[n].fd.in4 >= 0) {
	    FD_SET(udescs[n].fd.in4, &udpfds);
	}
    }

    for (;;) {
	memcpy(&readfds, &udpfds, sizeof(fd_set));
	memcpy(&errorfds, &udpfds, sizeof(fd_set));
	retval = select(0, &readfds, (fd_set *) NULL, &errorfds,
			(struct timeval *) NULL);
	if (udpstop) {
	    break;
	}

	if (retval > 0) {
	    for (n = 0; n < nudescs; n++) {
		if (udescs[n].fd.in6 >= 0 &&
		    FD_ISSET(udescs[n].fd.in6, &readfds)) {
		    udp_recv6(n);
		}
		if (udescs[n].fd.in4 >= 0 &&
		    FD_ISSET(udescs[n].fd.in4, &readfds)) {
		    udp_recv(n);
		}
	    }
	}
    }

    DeleteCriticalSection(&udpmutex);
}


static int nusers;			/* # of users */
static connection *connections;		/* connections array */
static connection *flist;		/* list of free connections */
static portdesc *tdescs, *bdescs;	/* telnet & binary descriptor arrays */
static int ntdescs, nbdescs;		/* # telnet & binary ports */
static fd_set infds;			/* file descriptor input bitmap */
static fd_set outfds;			/* file descriptor output bitmap */
static fd_set waitfds;			/* file descriptor wait-write bitmap */
static fd_set readfds;			/* file descriptor read bitmap */
static fd_set writefds;			/* file descriptor write map */
static int closed;			/* #fds closed in write */
static SOCKET self;			/* socket to self */
static bool self6;			/* self socket IPv6? */
static SOCKET cintr;			/* interrupt socket */

/*
 * NAME:	conn->intr()
 * DESCRIPTION:	interrupt conn->select()
 */
void conn_intr()
{
    send(cintr, "", 1, 0);
}

/*
 * NAME:	conn->port6()
 * DESCRIPTION:	open an IPv6 port
 */
static int conn_port6(SOCKET *fd, int type, struct sockaddr_in6 *sin6,
	unsigned short port)
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

    if (type == SOCK_STREAM) {
	FD_SET(*fd, &infds);
    }
    return TRUE;
}

/*
 * NAME:	conn->port()
 * DESCRIPTION:	open an IPv4 port
 */
static int conn_port(SOCKET *fd, int type, struct sockaddr_in *sin,
	unsigned short port)
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

    if (type == SOCK_STREAM) {
	FD_SET(*fd, &infds);
    }
    return TRUE;
}

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connection handling
 */
bool conn_init(int maxusers, char **thosts, char **bhosts, char **dhosts,
	       unsigned short *tports, unsigned short *bports,
	       unsigned short *dports, int ntports, int nbports, int ndports)
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

    addrtype = PF_INET;

    nusers = 0;
    FD_ZERO(&infds);
    FD_ZERO(&outfds);
    FD_ZERO(&waitfds);
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
	for (n = 0; n < nbdescs; n++) {
	    bdescs[n].in6 = INVALID_SOCKET;
	    bdescs[n].in4 = INVALID_SOCKET;
	}
    }
    nudescs = ndports;
    if (ndports != 0) {
	udescs = ALLOC(udpdesc, ndports);
	for (n = 0; n < nudescs; n++) {
	    udescs[n].fd.in6 = INVALID_SOCKET;
	    udescs[n].fd.in4 = INVALID_SOCKET;
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
	sin6.sin6_family = AF_INET6;
	if (thosts[n] == (char *) NULL) {
	    sin6.sin6_addr = in6addr_any;
	    sin.sin_addr.s_addr = htonl(INADDR_ANY);
	    ipv6 = ipv4 = TRUE;
	} else {
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
	sin6.sin6_family = AF_INET6;
	if (bhosts[n] == (char *) NULL) {
	    sin6.sin6_addr = in6addr_any;
	    sin.sin_addr.s_addr = htonl(INADDR_ANY);
	    ipv6 = ipv4 = TRUE;
	} else {
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

	if (ipv6 &&
	    !conn_port6(&bdescs[n].in6, SOCK_STREAM, &sin6, bports[n]) &&
	    bdescs[n].in6 != INVALID_SOCKET) {
	    return FALSE;
	}
	if (ipv4 && !conn_port(&bdescs[n].in4, SOCK_STREAM, &sin, bports[n])) {
	    return FALSE;
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

    for (n = 0; n < nudescs; n++) {
	/* datagram ports */
	ipv6 = FALSE;
	ipv4 = FALSE;
	sin6.sin6_family = AF_INET6;
	if (dhosts[n] == (char *) NULL) {
	    sin6.sin6_addr = in6addr_any;
	    sin.sin_addr.s_addr = htonl(INADDR_ANY);
	    ipv6 = ipv4 = TRUE;
	} else {
	    length = sizeof(struct sockaddr_in6);
	    if (WSAStringToAddress(dhosts[n], AF_INET6,
				   (LPWSAPROTOCOL_INFO) NULL,
				   (struct sockaddr *) &sin6, &length) == 0) {
		ipv6 = TRUE;
	    } else {
		struct addrinfo hints, *addr;

		memset(&hints, '\0', sizeof(struct addrinfo));
		hints.ai_family = PF_INET6;
		addr = (struct addrinfo *) NULL;
		getaddrinfo(dhosts[n], (char *) NULL, &hints, &addr);
		if (addr != (struct addrinfo *) NULL) {
		    memcpy(&sin6, addr->ai_addr, addr->ai_addrlen);
		    ipv6 = TRUE;
		    freeaddrinfo(addr);
		}
	    }
	    if ((sin.sin_addr.s_addr=inet_addr(dhosts[n])) != INADDR_NONE) {
		ipv4 = TRUE;
	    } else {
		host = gethostbyname(dhosts[n]);
		if (host != (struct hostent *) NULL) {
		    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
		    ipv4 = TRUE;
		}
	    }
	}

	if (!ipv6 && !ipv4) {
	    message("unknown host %s\012", dhosts[n]);	/* LF */
	    return FALSE;
	}

	if (ipv6 &&
	    !conn_port6(&udescs[n].fd.in6, SOCK_DGRAM, &sin6, dports[n]) &&
	    udescs[n].fd.in6 != INVALID_SOCKET) {
	    return FALSE;
	}
	if (ipv4 &&
	    !conn_port(&udescs[n].fd.in4, SOCK_DGRAM, &sin, dports[n])) {
	    return FALSE;
	}

	udescs[n].accept = FALSE;
    }

    flist = (connection *) NULL;
    connections = ALLOC(connection, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = INVALID_SOCKET;
	conn->next = flist;
	flist = conn;
    }

    udphtab = ALLOC(connection *, udphtabsz = maxusers);
    memset(udphtab, '\0', udphtabsz * sizeof(connection *));
    chtab = Hashtab::create(maxusers, UDPHASHSZ, TRUE);

    return TRUE;
}

/*
 * NAME:	conn->clear()
 * DESCRIPTION:	clean up connections
 */
void conn_clear()
{
    ipa_finish();
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish()
{
    udpstop = TRUE;
    WSACleanup();
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen()
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
	    inpkts = socket(AF_INET6, SOCK_STREAM, 0);
	    connect(inpkts, (struct sockaddr *) &addr, len);
	    outpkts = accept(self, (struct sockaddr *) &dummy, &len);
	} else {
	    struct sockaddr_in addr;
	    struct sockaddr_in dummy;

	    len = sizeof(struct sockaddr_in);
	    getsockname(self, (struct sockaddr *) &addr, &len);
	    if (addr.sin_addr.s_addr == htonl(INADDR_ANY)) {
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	    }
	    in = socket(AF_INET, SOCK_STREAM, 0);
	    FD_SET(in, &infds);
	    connect(in, (struct sockaddr *) &addr, len);
	    ipa_start(in, accept(self, (struct sockaddr *) &dummy, &len));
	    fd = socket(AF_INET, SOCK_STREAM, 0);
	    connect(fd, (struct sockaddr *) &addr, len);
	    cintr = accept(self, (struct sockaddr *) &dummy, &len);
	    inpkts = socket(AF_INET, SOCK_STREAM, 0);
	    connect(inpkts, (struct sockaddr *) &addr, len);
	    outpkts = accept(self, (struct sockaddr *) &dummy, &len);
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
    }
    if (nudescs != 0) {
	udpstop = FALSE;
	InitializeCriticalSection(&udpmutex);
	_beginthread(udp_run, 0, NULL);
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
    flist = (connection *) conn->next;
    conn->name = (char *) NULL;
    conn->fd = fd;
    conn->udp = FALSE;
    conn->udpbuf = (char *) NULL;
    addr.addr6 = sin6.sin6_addr;
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
    flist = (connection *) conn->next;
    conn->name = (char *) NULL;
    conn->fd = fd;
    conn->udp = FALSE;
    conn->udpbuf = (char *) NULL;
    addr.addr = sin.sin_addr;
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
 * NAME:	conn->udpaccept()
 * DESCRIPTION:	accept a new UDP connection
 */
static connection *conn_udpaccept(int port)
{
    connection *conn, **hash;

    conn = flist;
    flist = (connection *) conn->next;
    conn->name = (char *) NULL;
    Alloc::staticMode();
    conn->udpbuf = ALLOC(char, BINBUF_SIZE + 2);
    Alloc::dynamicMode();
    hash = &udphtab[udescs[port].hashval];
    EnterCriticalSection(&udpmutex);
    conn->next = *hash;
    *hash = conn;
    conn->fd = INVALID_SOCKET;
    conn->udp = TRUE;
    conn->addr = ipa_new(&udescs[port].addr);
    conn->port = udescs[port].port;
    conn->at = port;
    conn->udpbuf[0] = udescs[port].size >> 8;
    conn->udpbuf[1] = udescs[port].size;
    memcpy(conn->udpbuf + 2, udescs[port].buffer, udescs[port].size);
    conn->bufsz = udescs[port].size + 2;
    conn->npkts = 1;
    udescs[port].accept = FALSE;
    LeaveCriticalSection(&udpmutex);

    return conn;
}

/*
 * NAME:	conn->tnew6()
 * DESCRIPTION:	accept a new telnet connection
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
 * DESCRIPTION:	accept a new binary connection
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
 * NAME:	conn->dnew6()
 * DESCRIPTION:	accept a new datagram connection
 */
connection *conn_dnew6(int port)
{
    if (udescs[port].accept) {
	return conn_udpaccept(port);
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
 * NAME:	conn->dnew()
 * DESCRIPTION:	accept a new datagram connection
 */
connection *conn_dnew(int port)
{
    if (udescs[port].accept) {
	return conn_udpaccept(port);
    }
    return (connection *) NULL;
}

/*
 * NAME:	conn->attach()
 * DESCRIPTION:	can datagram channel be attached to this connection?
 */
bool conn_attach(connection *conn)
{
    return conf_attach(conn->at);
}

/*
 * NAME:	conn->udp()
 * DESCRIPTION:	set the challenge for attaching a UDP channel
 */
bool conn_udp(connection *conn, char *challenge,
	      unsigned int len)
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
    EnterCriticalSection(&udpmutex);
    hash = (connection **) chtab->lookup(buffer, FALSE);
    while (*hash != (connection *) NULL &&
	   memcmp((*hash)->name, buffer, UDPHASHSZ) == 0) {
	if ((*hash)->bufsz == (int) len &&
	    memcmp((*hash)->udpbuf, challenge, len) == 0) {
	    LeaveCriticalSection(&udpmutex);
	    return FALSE;	/* duplicate challenge */
	}
    }

    conn->next = *hash;
    *hash = conn;
    conn->npkts = 0;
    Alloc::staticMode();
    conn->udpbuf = ALLOC(char, BINBUF_SIZE + 2);
    Alloc::dynamicMode();
    memset(conn->udpbuf, '\0', UDPHASHSZ);
    conn->name = (const char *) memcpy(conn->udpbuf, challenge, conn->bufsz = len);
    LeaveCriticalSection(&udpmutex);

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
	shutdown(conn->fd, SD_SEND);
	closesocket(conn->fd);
	FD_CLR(conn->fd, &infds);
	FD_CLR(conn->fd, &outfds);
	FD_CLR(conn->fd, &waitfds);
	conn->fd = INVALID_SOCKET;
    } else if (!conn->udp) {
	--closed;
    }
    if (conn->udpbuf != (char *) NULL) {
	EnterCriticalSection(&udpmutex);
	if (conn->name != (char *) NULL) {
	    hash = (connection **) chtab->lookup(conn->name, FALSE);
	} else if (conn->addr->ipnum.ipv6) {
	    hash = &udphtab[(Hashtab::hashmem((char *) &conn->addr->ipnum,
			    sizeof(struct in6_addr)) ^ conn->port) % udphtabsz];
	} else {
	    hash = &udphtab[((Uint) conn->addr->ipnum.addr.s_addr ^
						    conn->port) % udphtabsz];
	}
	while (*hash != conn) {
	    hash = (connection **) &(*hash)->next;
	}
	*hash = (connection *) conn->next;
	if (conn->npkts != 0) {
	    recv(inpkts, conn->udpbuf, conn->npkts, 0);
	}
	LeaveCriticalSection(&udpmutex);
	FREE(conn->udpbuf);
    }
    if (conn->addr != (ipaddr *) NULL) {
      ipa_del(conn->addr);
    }
    conn->next = flist;
    flist = conn;
}

/*
 * NAME:	conn->block()
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
    if (closed != 0) {
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
	if (WSAGetLastError() == WSAEINVAL) {
	    /* The select() call didn't wait because there are no open
	       fds, so we force a sleep to keep from pegging the cpu */
	    Sleep(1000);
	}

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
    return (conn->name == (char *) NULL);
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
int conn_udpread(connection *conn, char *buf, unsigned int len)
{
    unsigned short size, n;
    char *p, *q, discard;

    EnterCriticalSection(&udpmutex);
    while (conn->bufsz != 0) {
	/* udp buffer is not empty */
	size = ((unsigned char) conn->udpbuf[0] << 8) |
		(unsigned char) conn->udpbuf[1];
	if (size <= len) {
	    memcpy(buf, conn->udpbuf + 2, len = size);
	}
	--conn->npkts;
	recv(inpkts, &discard, 1, 0);
	conn->bufsz -= size + 2;
	for (p = conn->udpbuf, q = p + size + 2, n = conn->bufsz; n != 0; --n) {
	    *p++ = *q++;
	}
	if (len == size) {
	    LeaveCriticalSection(&udpmutex);
	    return len;
	}
    }
    LeaveCriticalSection(&udpmutex);
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
    if ((size = send(conn->fd, buf, len, 0)) == SOCKET_ERROR &&
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
    if (conn->fd != INVALID_SOCKET || conn->udp) {
	if (conn->addr->ipnum.ipv6) {
	    struct sockaddr_in6 to;

	    memset(&to, '\0', sizeof(struct sockaddr_in6));
	    to.sin6_family = AF_INET6;
	    memcpy(&to.sin6_addr, &conn->addr->ipnum.addr6,
		   sizeof(struct in6_addr));
	    to.sin6_port = conn->port;
	    return sendto(udescs[conn->at].fd.in6, buf, len, 0,
			  (struct sockaddr *) &to, sizeof(struct sockaddr_in6));
	} else {
	    struct sockaddr_in to;

	    to.sin_family = AF_INET;
	    to.sin_addr.s_addr = conn->addr->ipnum.addr.s_addr;
	    to.sin_port = conn->port;
	    return sendto(udescs[conn->at].fd.in4, buf, len, 0,
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
	sin6.sin6_addr = conn->addr->ipnum.addr6;
	WSAAddressToString((struct sockaddr *) &sin6,
			   sizeof(struct sockaddr_in6),
			   (LPWSAPROTOCOL_INFO) NULL, buf, &length);
    } else {
	strcpy(buf, inet_ntoa(conn->addr->ipnum.addr));
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

/*
 * NAME:	conn->host()
 * DESCRIPTION:	look up a host
 */
void *conn_host(char *addr, unsigned short port, int *len)
{
    struct hostent *host;
    static union {
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
    } inaddr;

    memset(&inaddr.sin, '\0', sizeof(struct sockaddr_in));
    inaddr.sin.sin_family = AF_INET;
    inaddr.sin.sin_port = htons(port);
    *len = sizeof(struct sockaddr_in);
    if ((inaddr.sin.sin_addr.s_addr=inet_addr(addr)) != INADDR_NONE) {
	return &inaddr;
    } else {
	host = gethostbyname(addr);
	if (host != (struct hostent *) NULL) {
	    memcpy(&inaddr.sin.sin_addr, host->h_addr, host->h_length);
	    return &inaddr;
	}
    }

    memset(&inaddr.sin6, '\0', sizeof(struct sockaddr_in6));
    inaddr.sin6.sin6_family = AF_INET6;
    inaddr.sin6.sin6_port = htons(port);
    *len = sizeof(struct sockaddr_in6);
    if (WSAStringToAddress(addr, AF_INET, (LPWSAPROTOCOL_INFO) NULL,
			   (struct sockaddr *) &inaddr.sin6, len) == 0) {
	return &inaddr;
    } else {
	struct addrinfo hints, *ai;

	memset(&hints, '\0', sizeof(struct addrinfo));
	hints.ai_family = PF_INET6;
	ai = (struct addrinfo *) NULL;
	getaddrinfo(addr, (char *) NULL, &hints, &ai);
	if (ai != (struct addrinfo *) NULL) {
	    memcpy(&inaddr.sin6, ai->ai_addr, ai->ai_addrlen);
	    freeaddrinfo(ai);
	    *len = sizeof(struct sockaddr_in6);
	    return &inaddr;
	}
    }

    return (void *) NULL;
}

/*
 * NAME:	conn->connect()
 * DESCRIPTION: establish an oubound connection
 */
connection *conn_connect(void *addr, int len)
{
    connection *conn;
    int sock;
    int on;
    unsigned long nonblock;

    if (flist == (connection *) NULL) {
       return NULL;
    }

    sock = socket(((struct sockaddr_in *) addr)->sin_family, SOCK_STREAM, 0);
    if (sock < 0) {
	P_message("socket");
	return NULL;
    }
    on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) < 0) {
	P_message("setsockopt");
	closesocket(sock);
	return NULL;
    }
    on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *) &on,
		   sizeof(on)) < 0) {
	P_message("setsockopt");
	closesocket(sock);
	return NULL;
    }
    nonblock = TRUE;
    if (ioctlsocket(sock, FIONBIO, &nonblock) != 0) {
	P_message("ioctlsocket");
	closesocket(sock);
	return NULL;
    }

    connect(sock, (struct sockaddr *) addr, len);

    conn = flist;
    flist = (connection *)conn->next;
    conn->fd = sock;
    conn->name = (char *) NULL;
    conn->udpbuf = (char *) NULL;
    conn->addr = (ipaddr *) NULL;
    conn->at = -1;
    FD_SET(sock, &infds);
    FD_SET(sock, &outfds);
    FD_CLR(sock, &readfds);
    FD_CLR(sock, &writefds);
    FD_SET(sock, &waitfds);
    return conn;
}

/*
 * check for a connection in pending state and see if it is connected.
 */
int conn_check_connected(connection *conn, int *errcode)
{
    int optval;
     socklen_t lon;

    /*
     * indicate that our fd became invalid.
     */
    if (conn->fd < 0) {
	return -2;
    }

    if (!FD_ISSET(conn->fd, &writefds)) {
	return 0;
    }
    FD_CLR(conn->fd, &waitfds);

    /*
     * Delayed connect completed, check for errors
     */
    lon = sizeof(int);
    /*
     * Get error state for the socket
     */
    *errcode = 0;
    if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, (char*)(&optval), &lon) < 0) {
	return -1;
    }
    if (optval != 0) {
	switch (optval) {
	case WSAECONNREFUSED:
	case ERROR_CONNECTION_REFUSED:
	    *errcode = 1;
	    break;

	case WSAEHOSTUNREACH:
	case ERROR_HOST_UNREACHABLE:
	    *errcode = 2;
	    break;

	case WSAENETUNREACH:
	case ERROR_NETWORK_UNREACHABLE:
	    *errcode = 3;
	    break;

	case WSAETIMEDOUT:
	case ERROR_SEM_TIMEOUT:
	    *errcode = 4;
	    break;
	}
	errno = optval;
	return -1;
    } else {
	struct sockaddr_in6 sin;
	socklen_t len;
	in46addr inaddr;

	len = sizeof(sin);
	getpeername(conn->fd, (struct sockaddr *) &sin, &len);
	inaddr.ipv6 = FALSE;
	if (sin.sin6_family == AF_INET6) {
	    inaddr.addr6 = sin.sin6_addr;
	    inaddr.ipv6 = TRUE;
	} else {
	    inaddr.addr = ((struct sockaddr_in *) &sin)->sin_addr;
	}
	conn->addr = ipa_new(&inaddr);
	errno = 0;
	return 1;
    }
}


/*
 * NAME:	conn->export()
 * DESCRIPTION:	export a connection
 */
bool conn_export(connection *conn, int *fd, char *addr, unsigned short *port,
		 short *at, int *npkts, int *bufsz, char **buf, char *flags)
{
    UNREFERENCED_PARAMETER(conn);
    UNREFERENCED_PARAMETER(fd);
    UNREFERENCED_PARAMETER(addr);
    UNREFERENCED_PARAMETER(port);
    UNREFERENCED_PARAMETER(at);
    UNREFERENCED_PARAMETER(npkts);
    UNREFERENCED_PARAMETER(bufsz);
    UNREFERENCED_PARAMETER(buf);
    UNREFERENCED_PARAMETER(flags);
    return FALSE;
}

/*
 * NAME:	conn->import()
 * DESCRIPTION:	import a connection
 */
connection *conn_import(int fd, char *addr, unsigned short port, short at,
			int npkts, int bufsz, char *buf, char flags,
			bool telnet)
{
    UNREFERENCED_PARAMETER(fd);
    UNREFERENCED_PARAMETER(addr);
    UNREFERENCED_PARAMETER(port);
    UNREFERENCED_PARAMETER(at);
    UNREFERENCED_PARAMETER(npkts);
    UNREFERENCED_PARAMETER(bufsz);
    UNREFERENCED_PARAMETER(buf);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(telnet);
    return (connection *) NULL;
}

/*
 * NAME:	conn->fdcount()
 * DESCRIPTION:	return the number of restored connections
 */
int conn_fdcount()
{
    return 0;
}

/*
 * NAME:	conn->fdlist()
 * DESCRIPTION:	pass on a list of connection descriptors
 */
void conn_fdlist(int *list)
{
}

/*
 * NAME:	Connection->fdclose()
 * DESCRIPTION:	close a list of connection descriptors
 */
void conn_fdclose(int *list, int n)
{
}
