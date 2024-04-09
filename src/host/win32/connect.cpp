/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

struct In46Addr {
    union {
	struct in6_addr addr6;		/* IPv6 addr */
	struct in_addr addr;		/* IPv4 addr */
    };
    bool ipv6;				/* IPv6? */
};

static SOCKET in = INVALID_SOCKET;	/* connection from name resolver */
static SOCKET out = INVALID_SOCKET;	/* connection to name resolver */

/*
 * host name lookup thread
 */
static void ipa_run(void *dummy)
{
    char buf[sizeof(In46Addr)];
    struct hostent *host;
    int len;

    UNREFERENCED_PARAMETER(dummy);

    while (recv(out, buf, sizeof(In46Addr), 0) > 0) {
	/* lookup host */
	if (((In46Addr *) &buf)->ipv6) {
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


class IpAddr {
public:
    void del();

    static bool init(int maxusers);
    static void start(SOCKET fd_in, SOCKET fd_out);
    static void finish();
    static IpAddr *create(In46Addr *ipnum);
    static void lookup();

    In46Addr ipnum;			/* ip number */
    char name[MAXHOSTNAMELEN];		/* ip name */

private:
    IpAddr *link;			/* next in hash table */
    IpAddr *prev;			/* previous in linked list */
    IpAddr *next;			/* next in linked list */
    Uint ref;				/* reference count */
};

# define NFREE		32

static int addrtype;			/* network address family */
static IpAddr **ipahtab;		/* ip address hash table */
static unsigned int ipahtabsz;		/* hash table size */
static IpAddr *qhead, *qtail;		/* request queue */
static IpAddr *ffirst, *flast;		/* free list */
static int nfree;			/* # in free list */
static IpAddr *lastreq;			/* last request */
static bool busy;			/* name resolver busy */

/*
 * initialize name lookup
 */
bool IpAddr::init(int maxusers)
{
    ipahtab = ALLOC(IpAddr*, ipahtabsz = maxusers);
    memset(ipahtab, '\0', ipahtabsz * sizeof(IpAddr*));
    qhead = qtail = ffirst = flast = lastreq = (IpAddr *) NULL;
    nfree = 0;
    busy = FALSE;

    return TRUE;
}

/*
 * start name resolver thread
 */
void IpAddr::start(SOCKET fd_in, SOCKET fd_out)
{
    in = fd_in;
    out = fd_out;
    _beginthread(ipa_run, 0, NULL);
}

/*
 * stop name lookup
 */
void IpAddr::finish()
{
    closesocket(in);
    in = INVALID_SOCKET;
}

/*
 * return a new ipaddr
 */
IpAddr *IpAddr::create(In46Addr *ipnum)
{
    IpAddr *ipa, **hash;

    /* check hash table */
    if (ipnum->ipv6) {
	hash = &ipahtab[HM->hashmem((char *) ipnum,
				    sizeof(struct in6_addr)) % ipahtabsz];
    } else {
	hash = &ipahtab[(Uint) ipnum->addr.s_addr % ipahtabsz];
    }
    while (*hash != (IpAddr *) NULL) {
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
		if (ipa->prev == (IpAddr *) NULL) {
		    ffirst = ipa->next;
		} else {
		    ipa->prev->next = ipa->next;
		}
		if (ipa->next == (IpAddr *) NULL) {
		    flast = ipa->prev;
		} else {
		    ipa->next->prev = ipa->prev;
		}
		ipa->prev = ipa->next = (IpAddr *) NULL;
		--nfree;
	    }
	    ipa->ref++;

	    if (ipa->name[0] == '\0' && ipa != lastreq &&
		ipa->prev == (IpAddr *) NULL && ipa != qhead) {
		if (!busy) {
		    /* send query to name resolver */
		    send(in, (char *) ipnum, sizeof(In46Addr), 0);
		    lastreq = ipa;
		    busy = TRUE;
		} else {
		    /* put in request queue */
		    ipa->prev = qtail;
		    if (qtail == (IpAddr *) NULL) {
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
	IpAddr **h;

	/*
	 * use first ipaddr in free list
	 */
	ipa = ffirst;
	ffirst = ipa->next;
	ffirst->prev = (IpAddr *) NULL;
	--nfree;

	if (ipa == lastreq) {
	    lastreq = (IpAddr *) NULL;
	}

	if (hash != &ipa->link) {
	    /* remove from hash table */
	    if (ipa->ipnum.ipv6) {
		h = &ipahtab[HM->hashmem((char *) &ipa->ipnum,
					 sizeof(struct in6_addr)) % ipahtabsz];
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
	MM->staticMode();
	ipa = ALLOC(IpAddr, 1);
	MM->dynamicMode();

	/* put in hash table */
	ipa->link = *hash;
	*hash = ipa;
    }

    ipa->ref = 1;
    ipa->ipnum = *ipnum;
    ipa->name[0] = '\0';
    ipa->prev = ipa->next = (IpAddr *) NULL;

    if (!busy) {
	/* send query to name resolver */
	send(in, (char *) ipnum, sizeof(In46Addr), 0);
	lastreq = ipa;
	busy = TRUE;
    } else {
	/* put in request queue */
	ipa->prev = qtail;
	if (qtail == (IpAddr *) NULL) {
	    qhead = ipa;
	} else {
	    qtail->next = ipa;
	}
	qtail = ipa;
    }

    return ipa;
}

/*
 * delete an ipaddr
 */
void IpAddr::del()
{
    if (--ref == 0) {
	if (prev != (IpAddr *) NULL || qhead == this) {
	    /* remove from queue */
	    if (prev != (IpAddr *) NULL) {
		prev->next = next;
	    } else {
		qhead = next;
	    }
	    if (next != (IpAddr *) NULL) {
		next->prev = prev;
	    } else {
		qtail = prev;
	    }
	}

	/* add to free list */
	if (flast != (IpAddr *) NULL) {
	    flast->next = this;
	    prev = flast;
	    flast = this;
	} else {
	    ffirst = flast = this;
	    prev = (IpAddr *) NULL;
	}
	next = (IpAddr *) NULL;
	nfree++;
    }
}

/*
 * lookup another ip name
 */
void IpAddr::lookup()
{
    IpAddr *ipa;

    if (lastreq != (IpAddr *) NULL) {
	/* read ip name */
	lastreq->name[recv(in, lastreq->name, MAXHOSTNAMELEN, 0)] = '\0';
    } else {
	char buf[MAXHOSTNAMELEN];

	/* discard ip name */
	recv(in, buf, MAXHOSTNAMELEN, 0);
    }

    /* if request queue not empty, write new query */
    if (qhead != (IpAddr *) NULL) {
	ipa = qhead;
	send(in, (char *) &ipa->ipnum, sizeof(In46Addr), 0);
	qhead = ipa->next;
	if (qhead == (IpAddr *) NULL) {
	    qtail = (IpAddr *) NULL;
	} else {
	    qhead->prev = (IpAddr *) NULL;
	}
	ipa->prev = ipa->next = (IpAddr *) NULL;
	lastreq = ipa;
	busy = TRUE;
    } else {
	lastreq = (IpAddr *) NULL;
	busy = FALSE;
    }
}

class XConnection : public Hash::Entry, public Connection, public Allocated {
public:
    XConnection() : fd(INVALID_SOCKET) { }

    virtual bool attach();
    virtual bool udp(char *challenge, unsigned int len);
    virtual void del();
    virtual void block(int flag);
    virtual void stop();
    virtual bool udpCheck();
    virtual int read(char *buf, unsigned int len);
    virtual int readUdp(char *buf, unsigned int len);
    virtual int write(char *buf, unsigned int len);
    virtual int writeUdp(char *buf, unsigned int len);
    virtual bool wrdone();
    virtual void ipnum(char *buf);
    virtual void ipname(char *buf);
    virtual int checkConnected(int *errcode);
    virtual bool cexport(int *fd, char *addr, unsigned short *port, short *at,
			 int *npkts, int *bufsz, char **buf, char *flags);

    static int port6(SOCKET *fd, int type, struct sockaddr_in6 *sin6,
		     unsigned int port);
    static int port4(SOCKET *fd, int type, struct sockaddr_in *sin,
		     unsigned int port);
    static XConnection *create6(SOCKET portfd, int port);
    static XConnection *create(SOCKET portfd, int port);
    static XConnection *createUdp(int port);

    SOCKET fd;				/* file descriptor */
    int npkts;				/* # packets in buffer */
    int bufsz;				/* # bytes in buffer */
    bool udpFlag;			/* datagram only? */
    int err;				/* state of outbound UDP connection */
    char *udpbuf;			/* datagram buffer */
    IpAddr *addr;			/* internet address of connection */
    unsigned short port;		/* UDP port of connection */
    short at;				/* port connection was accepted at */
};

struct PortDesc {
    SOCKET in6;				/* IPv6 socket */
    SOCKET in4;				/* IPv4 socket */
};

class Udp {
public:
    static void recv6(int n);
    static void recv(int n);

    struct PortDesc fd;			/* port descriptors */
    In46Addr addr;			/* source of new packet */
    unsigned short port;		/* source port of new datagram */
    bool accept;			/* datagram ready to accept? */
    unsigned short hashval;		/* address hash */
    int size;				/* size in buffer */
    char buffer[BINBUF_SIZE];		/* buffer */
};

static Hash::Entry **udphtab;		/* UDP hash table */
static int udphtabsz;			/* UDP hash table size */
static Hash::Hashtab *chtab;		/* challenge hash table */
static Udp *udescs;			/* UDP port descriptor array */
static int nudescs;			/* # datagram ports */
static SOCKET inpkts, outpkts;		/* UDP packet notification pip */
static CRITICAL_SECTION udpmutex;	/* UDP mutex */
static bool udpstop;			/* stop UDP thread? */

/*
 * receive an UDP packet
 */
void Udp::recv6(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in6 from;
    int fromlen;
    int size;
    unsigned short hashval;
    Hash::Entry **hash;
    XConnection *conn;
    char *p;

    memset(buffer, '\0', UDPHASHSZ);
    fromlen = sizeof(struct sockaddr_in6);
    size = recvfrom(udescs[n].fd.in6, buffer, BINBUF_SIZE, 0,
		    (struct sockaddr *) &from, &fromlen);
    if (size < 0) {
	return;
    }

    hashval = (HM->hashmem((char *) &from.sin6_addr,
			   sizeof(struct in6_addr)) ^ from.sin6_port) %
								    udphtabsz;
    hash = &udphtab[hashval];
    EnterCriticalSection(&udpmutex);
    for (;;) {
	conn = (XConnection *) *hash;
	if (conn == (XConnection *) NULL) {
	    if (!Config::attach(n)) {
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
	    hash = chtab->lookup(buffer, FALSE);
	    while ((conn=(XConnection *) *hash) != (XConnection *) NULL &&
		   memcmp(conn->name, buffer, UDPHASHSZ) == 0) {
		if (conn->bufsz == size &&
		    memcmp(conn->udpbuf, buffer, size) == 0 &&
		    conn->addr->ipnum.ipv6 &&
		    memcmp(&conn->addr->ipnum, &from.sin6_addr,
			   sizeof(struct in6_addr)) == 0) {
		    /*
		     * attach new UDP channel
		     */
		    *hash = conn->next;
		    conn->name = (char *) NULL;
		    conn->bufsz = 0;
		    conn->port = from.sin6_port;
		    hash = &udphtab[hashval];
		    conn->next = *hash;
		    *hash = conn;

		    break;
		}
		hash = &conn->next;
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
	hash = &conn->next;
    }
    LeaveCriticalSection(&udpmutex);
}

/*
 * receive an UDP packet
 */
void Udp::recv(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in from;
    int fromlen;
    int size;
    unsigned short hashval;
    Hash::Entry **hash;
    XConnection *conn;
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
	conn = (XConnection *) *hash;
	if (conn == (XConnection *) NULL) {
	    if (!Config::attach(n)) {
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
	    hash = chtab->lookup(buffer, FALSE);
	    while ((conn=(XConnection *) *hash) != (XConnection *) NULL &&
		   memcmp((*hash)->name, buffer, UDPHASHSZ) == 0) {
		if (conn->bufsz == size &&
		    memcmp(conn->udpbuf, buffer, size) == 0 &&
		    !conn->addr->ipnum.ipv6 &&
		    conn->addr->ipnum.addr.s_addr == from.sin_addr.s_addr) {
		    /*
		     * attach new UDP channel
		     */
		    *hash = conn->next;
		    conn->name = (char *) NULL;
		    conn->bufsz = 0;
		    conn->port = from.sin_port;
		    hash = &udphtab[hashval];
		    conn->next = *hash;
		    *hash = conn;

		    break;
		}
		hash = &conn->next;
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
	hash = &conn->next;
    }
    LeaveCriticalSection(&udpmutex);
}

/*
 * UDP thread
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
		    Udp::recv6(n);
		}
		if (udescs[n].fd.in4 >= 0 &&
		    FD_ISSET(udescs[n].fd.in4, &readfds)) {
		    Udp::recv(n);
		}
	    }
	}
    }

    DeleteCriticalSection(&udpmutex);
}


static int nusers;			/* # of users */
static XConnection **connections;	/* connections array */
static Hash::Entry *flist;		/* list of free connections */
static PortDesc *tdescs, *bdescs;	/* telnet & binary descriptor arrays */
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
 * interrupt Connection::select()
 */
void conn_intr()
{
    send(cintr, "", 1, 0);
}

/*
 * open an IPv6 port
 */
int XConnection::port6(SOCKET *fd, int type, struct sockaddr_in6 *sin6,
		       unsigned int port)
{
    int on;

    if ((*fd=socket(AF_INET6, type, 0)) == INVALID_SOCKET) {
	return FALSE;
    }
    on = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0)
    {
	EC->message("setsockopt() failed\n");
	return FALSE;
    }
    if (type == SOCK_STREAM) {
	on = 1;
	if (setsockopt(*fd, SOL_SOCKET, SO_OOBINLINE, (char *) &on, sizeof(on))
									< 0) {
	    EC->message("setsockopt() failed\n");
	    return FALSE;
	}
    }
    WSAHtons(*fd, port, &sin6->sin6_port);
    if (bind(*fd, (struct sockaddr *) sin6, sizeof(struct sockaddr_in6)) < 0) {
	EC->message("bind() failed\n");
	return FALSE;
    }

    if (type == SOCK_STREAM) {
	FD_SET(*fd, &infds);
    }
    return TRUE;
}

/*
 * open an IPv4 port
 */
int XConnection::port4(SOCKET *fd, int type, struct sockaddr_in *sin,
		       unsigned int port)
{
    int on;

    if ((*fd=socket(AF_INET, type, 0)) == INVALID_SOCKET) {
	EC->message("socket() failed\n");
	return FALSE;
    }
    on = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0)
    {
	EC->message("setsockopt() failed\n");
	return FALSE;
    }
    if (type == SOCK_STREAM) {
	on = 1;
	if (setsockopt(*fd, SOL_SOCKET, SO_OOBINLINE, (char *) &on, sizeof(on))
									< 0) {
	    EC->message("setsockopt() failed\n");
	    return FALSE;
	}
    }
    sin->sin_port = htons(port);
    if (bind(*fd, (struct sockaddr *) sin, sizeof(struct sockaddr_in)) < 0) {
	EC->message("bind() failed\n");
	return FALSE;
    }

    if (type == SOCK_STREAM) {
	FD_SET(*fd, &infds);
    }
    return TRUE;
}

/*
 * initialize connection handling
 */
bool Connection::init(int maxusers, char **thosts, char **bhosts, char **dhosts,
		      unsigned short *tports, unsigned short *bports,
		      unsigned short *dports, int ntports, int nbports,
		      int ndports)
{
    WSADATA wsadata;
    struct sockaddr_in6 sin6;
    struct sockaddr_in sin;
    struct hostent *host;
    int n, length;
    XConnection **conn;
    bool ipv6, ipv4;

    self = INVALID_SOCKET;
    cintr = INVALID_SOCKET;

    /* initialize winsock */
    if (WSAStartup(MAKEWORD(2, 0), &wsadata) != 0) {
	EC->message("WSAStartup failed (no winsock?)\n");
	return FALSE;
    }
    if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 0) {
	WSACleanup();
	EC->message("Winsock 2.0 not supported\n");
	return FALSE;
    }

    if (!IpAddr::init(maxusers)) {
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
	tdescs = ALLOC(PortDesc, ntports);
	for (n = 0; n < ntdescs; n++) {
	    tdescs[n].in6 = INVALID_SOCKET;
	    tdescs[n].in4 = INVALID_SOCKET;
	}
    }
    nbdescs = nbports;
    if (nbports != 0) {
	bdescs = ALLOC(PortDesc, nbports);
	for (n = 0; n < nbdescs; n++) {
	    bdescs[n].in6 = INVALID_SOCKET;
	    bdescs[n].in4 = INVALID_SOCKET;
	}
    }
    nudescs = ndports;
    if (ndports != 0) {
	udescs = ALLOC(Udp, ndports);
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
	    EC->message("unknown host %s\012", thosts[n]);	/* LF */
	    return FALSE;
	}

	if (ipv6 && !XConnection::port6(&tdescs[n].in6, SOCK_STREAM, &sin6,
					tports[n]) &&
	    tdescs[n].in6 != INVALID_SOCKET) {
	    return FALSE;
	}
	if (ipv4 &&
	    !XConnection::port4(&tdescs[n].in4, SOCK_STREAM, &sin, tports[n])) {
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
	    EC->message("unknown host %s\012", bhosts[n]);	/* LF */
	    return FALSE;
	}

	if (ipv6 && !XConnection::port6(&bdescs[n].in6, SOCK_STREAM, &sin6,
					bports[n]) &&
	    bdescs[n].in6 != INVALID_SOCKET) {
	    return FALSE;
	}
	if (ipv4 &&
	    !XConnection::port4(&bdescs[n].in4, SOCK_STREAM, &sin, bports[n])) {
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
	    EC->message("unknown host %s\012", dhosts[n]);	/* LF */
	    return FALSE;
	}

	if (ipv6 &&
	    !XConnection::port6(&udescs[n].fd.in6, SOCK_DGRAM, &sin6,
				dports[n]) &&
	    udescs[n].fd.in6 != INVALID_SOCKET) {
	    return FALSE;
	}
	if (ipv4 &&
	    !XConnection::port4(&udescs[n].fd.in4, SOCK_DGRAM, &sin, dports[n]))
	{
	    return FALSE;
	}

	udescs[n].accept = FALSE;
    }

    flist = (Hash::Entry *) NULL;
    connections = ALLOC(XConnection*, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	*conn = new XConnection();
	(*conn)->next = flist;
	flist = *conn;
    }

    udphtab = ALLOC(Hash::Entry*, udphtabsz = maxusers);
    memset(udphtab, '\0', udphtabsz * sizeof(Hash::Entry*));
    chtab = HM->create(maxusers, UDPHASHSZ, TRUE);

    return TRUE;
}

/*
 * clean up connections
 */
void Connection::clear()
{
    IpAddr::finish();
}

/*
 * terminate connections
 */
void Connection::finish()
{
    udpstop = TRUE;
    WSACleanup();
}

/*
 * start listening on telnet port and binary port
 */
void Connection::listen()
{
    int n;
    unsigned long nonblock;

    for (n = 0; n < ntdescs; n++) {
	if (tdescs[n].in6 != INVALID_SOCKET && ::listen(tdescs[n].in6, 64) != 0) {
	    EC->fatal("listen failed");
	}
	if (tdescs[n].in4 != INVALID_SOCKET && ::listen(tdescs[n].in4, 64) != 0) {
	    EC->fatal("listen failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	if (bdescs[n].in6 != INVALID_SOCKET && ::listen(bdescs[n].in6, 64) != 0) {
	    EC->fatal("listen failed");
	}
	if (bdescs[n].in4 != INVALID_SOCKET && ::listen(bdescs[n].in4, 64) != 0) {
	    EC->fatal("listen failed");
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
	    ::connect(in, (struct sockaddr *) &addr, len);
	    IpAddr::start(in, accept(self, (struct sockaddr *) &dummy, &len));
	    fd = socket(AF_INET6, SOCK_STREAM, 0);
	    ::connect(fd, (struct sockaddr *) &addr, len);
	    cintr = accept(self, (struct sockaddr *) &dummy, &len);
	    inpkts = socket(AF_INET6, SOCK_STREAM, 0);
	    ::connect(inpkts, (struct sockaddr *) &addr, len);
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
	    ::connect(in, (struct sockaddr *) &addr, len);
	    IpAddr::start(in, accept(self, (struct sockaddr *) &dummy, &len));
	    fd = socket(AF_INET, SOCK_STREAM, 0);
	    ::connect(fd, (struct sockaddr *) &addr, len);
	    cintr = accept(self, (struct sockaddr *) &dummy, &len);
	    inpkts = socket(AF_INET, SOCK_STREAM, 0);
	    ::connect(inpkts, (struct sockaddr *) &addr, len);
	    outpkts = accept(self, (struct sockaddr *) &dummy, &len);
	}
	FD_SET(fd, &infds);
    }

    for (n = 0; n < ntdescs; n++) {
	nonblock = TRUE;
	if (tdescs[n].in6 != INVALID_SOCKET &&
	    ioctlsocket(tdescs[n].in6, FIONBIO, &nonblock) != 0) {
	    EC->fatal("ioctlsocket failed");
	}
	nonblock = TRUE;
	if (tdescs[n].in4 != INVALID_SOCKET &&
	    ioctlsocket(tdescs[n].in4, FIONBIO, &nonblock) != 0) {
	    EC->fatal("ioctlsocket failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	nonblock = TRUE;
	if (bdescs[n].in6 != INVALID_SOCKET &&
	    ioctlsocket(bdescs[n].in6, FIONBIO, &nonblock) != 0) {
	    EC->fatal("ioctlsocket failed");
	}
	nonblock = TRUE;
	if (bdescs[n].in4 != INVALID_SOCKET &&
	    ioctlsocket(bdescs[n].in4, FIONBIO, &nonblock) != 0) {
	    EC->fatal("ioctlsocket failed");
	}
    }
    if (nudescs != 0) {
	udpstop = FALSE;
	InitializeCriticalSection(&udpmutex);
	_beginthread(udp_run, 0, NULL);
    }
}

/*
 * accept a new ipv6 connection
 */
XConnection *XConnection::create6(SOCKET portfd, int port)
{
    SOCKET fd;
    int len;
    struct sockaddr_in6 sin6;
    In46Addr addr;
    XConnection *conn;
    unsigned long nonblock;

    if (!FD_ISSET(portfd, &readfds)) {
	return (XConnection *) NULL;
    }
    len = sizeof(sin6);
    fd = accept(portfd, (struct sockaddr *) &sin6, &len);
    if (fd == INVALID_SOCKET) {
	FD_CLR(portfd, &readfds);
	return (XConnection *) NULL;
    }
    nonblock = TRUE;
    ioctlsocket(fd, FIONBIO, &nonblock);

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->name = (char *) NULL;
    conn->fd = fd;
    conn->udpFlag = FALSE;
    conn->udpbuf = (char *) NULL;
    addr.addr6 = sin6.sin6_addr;
    addr.ipv6 = TRUE;
    conn->addr = IpAddr::create(&addr);
    conn->at = port;
    FD_SET(fd, &infds);
    FD_SET(fd, &outfds);
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);

    return conn;
}

/*
 * accept a new ipv4 connection
 */
XConnection *XConnection::create(SOCKET portfd, int port)
{
    SOCKET fd;
    int len;
    struct sockaddr_in sin;
    In46Addr addr;
    XConnection *conn;
    unsigned long nonblock;

    if (!FD_ISSET(portfd, &readfds)) {
	return (XConnection *) NULL;
    }
    len = sizeof(sin);
    fd = accept(portfd, (struct sockaddr *) &sin, &len);
    if (fd == INVALID_SOCKET) {
	FD_CLR(portfd, &readfds);
	return (XConnection *) NULL;
    }
    nonblock = TRUE;
    ioctlsocket(fd, FIONBIO, &nonblock);

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->name = (char *) NULL;
    conn->fd = fd;
    conn->udpFlag = FALSE;
    conn->udpbuf = (char *) NULL;
    addr.addr = sin.sin_addr;
    addr.ipv6 = FALSE;
    conn->addr = IpAddr::create(&addr);
    conn->at = port;
    FD_SET(fd, &infds);
    FD_SET(fd, &outfds);
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);

    return conn;
}

/*
 * accept a new UDP connection
 */
XConnection *XConnection::createUdp(int port)
{
    XConnection *conn;
    Hash::Entry **hash;

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->name = (char *) NULL;
    MM->staticMode();
    conn->udpbuf = ALLOC(char, BINBUF_SIZE + 2);
    MM->dynamicMode();
    hash = &udphtab[udescs[port].hashval];
    EnterCriticalSection(&udpmutex);
    conn->next = *hash;
    *hash = conn;
    conn->fd = INVALID_SOCKET;
    conn->udpFlag = TRUE;
    conn->addr = IpAddr::create(&udescs[port].addr);
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
 * accept a new telnet connection
 */
Connection *Connection::createTelnet6(int port)
{
    SOCKET fd;

    fd = tdescs[port].in6;
    if (fd != INVALID_SOCKET) {
	return XConnection::create6(fd, port);
    }
    return (Connection *) NULL;
}

/*
 * accept a new binary connection
 */
Connection *Connection::create6(int port)
{
    SOCKET fd;

    fd = bdescs[port].in6;
    if (fd != INVALID_SOCKET) {
	return XConnection::create6(fd, port);
    }
    return (Connection *) NULL;
}

/*
 * accept a new datagram connection
 */
Connection *Connection::createDgram6(int port)
{
    if (udescs[port].accept) {
	return XConnection::createUdp(port);
    }
    return (Connection *) NULL;
}

/*
 * accept a new telnet connection
 */
Connection *Connection::createTelnet(int port)
{
    SOCKET fd;

    fd = tdescs[port].in4;
    if (fd != INVALID_SOCKET) {
	return XConnection::create(fd, port);
    }
    return (Connection *) NULL;
}

/*
 * accept a new binary connection
 */
Connection *Connection::create(int port)
{
    SOCKET fd;

    fd = bdescs[port].in4;
    if (fd != INVALID_SOCKET) {
	return XConnection::create(fd, port);
    }
    return (Connection *) NULL;
}

/*
 * accept a new datagram connection
 */
Connection *Connection::createDgram(int port)
{
    if (udescs[port].accept) {
	return XConnection::createUdp(port);
    }
    return (Connection *) NULL;
}

/*
 * can datagram channel be attached to this connection?
 */
bool XConnection::attach()
{
    return Config::attach(at);
}

/*
 * set the challenge for attaching a UDP channel
 */
bool XConnection::udp(char *challenge, unsigned int len)
{
    char buffer[UDPHASHSZ];
    Hash::Entry **hash;
    XConnection *conn;

    if (len == 0 || len > BINBUF_SIZE || udpbuf != (char *) NULL) {
	return FALSE;	/* invalid challenge */
    }

    if (len >= UDPHASHSZ) {
	memcpy(buffer, challenge, UDPHASHSZ);
    } else {
	memset(buffer, '\0', UDPHASHSZ);
	memcpy(buffer, challenge, len);
    }
    EnterCriticalSection(&udpmutex);
    hash = chtab->lookup(buffer, FALSE);
    while ((conn=(XConnection *) *hash) != (XConnection *) NULL &&
	   memcmp(conn->name, buffer, UDPHASHSZ) == 0) {
	if (conn->bufsz == (int) len &&
	    memcmp(conn->udpbuf, challenge, len) == 0) {
	    LeaveCriticalSection(&udpmutex);
	    return FALSE;	/* duplicate challenge */
	}
	hash = &conn->next;
    }

    next = conn;
    *hash = this;
    npkts = 0;
    MM->staticMode();
    udpbuf = ALLOC(char, BINBUF_SIZE + 2);
    MM->dynamicMode();
    memset(udpbuf, '\0', UDPHASHSZ);
    name = (const char *) memcpy(udpbuf, challenge, bufsz = len);
    LeaveCriticalSection(&udpmutex);

    return TRUE;
}

/*
 * delete a connection
 */
void XConnection::del()
{
    Hash::Entry **hash;

    if (fd != INVALID_SOCKET) {
	closesocket(fd);
	FD_CLR(fd, &infds);
	FD_CLR(fd, &outfds);
	FD_CLR(fd, &waitfds);
	fd = INVALID_SOCKET;
    } else if (!udpFlag) {
	--closed;
    }
    if (udpbuf != (char *) NULL) {
	EnterCriticalSection(&udpmutex);
	if (addr != (IpAddr *) NULL) {
	    if (name != (char *) NULL) {
		hash = chtab->lookup(name, FALSE);
	    } else if (addr->ipnum.ipv6) {
		hash = &udphtab[(HM->hashmem((char *) &addr->ipnum,
				 sizeof(struct in6_addr)) ^ port) % udphtabsz];
	    } else {
		hash = &udphtab[(((Uint) addr->ipnum.addr.s_addr) ^ port) %
								    udphtabsz];
	    }
	    while ((XConnection *) *hash != this) {
		hash = &(*hash)->next;
	    }
	    *hash = next;
	}
	if (npkts != 0) {
	    recv(inpkts, udpbuf, npkts, 0);
	}
	LeaveCriticalSection(&udpmutex);
	FREE(udpbuf);
    }
    if (addr != (IpAddr *) NULL) {
	addr->del();
    }
    next = flist;
    flist = this;
}

/*
 * block or unblock input from connection
 */
void XConnection::block(int flag)
{
    if (fd != INVALID_SOCKET) {
	if (flag) {
	    FD_CLR(fd, &infds);
	    FD_CLR(fd, &readfds);
	} else {
	    FD_SET(fd, &infds);
	}
    }
}

/*
 * close output channel
 */
void XConnection::stop()
{
    if (fd != INVALID_SOCKET) {
	shutdown(fd, SD_SEND);
    }
}

/*
 * wait for input from connections
 */
int Connection::select(Uint t, unsigned int mtime)
{
    struct timeval timeout;
    int retval, n;

    /*
     * First, check readability and writability for binary sockets with pending
     * data only.
     */
    memcpy(&readfds, &infds, sizeof(fd_set));
    if (flist == (Hash::Entry *) NULL) {
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
	retval = ::select(0, &readfds, &writefds, (fd_set *) NULL, &timeout);
    } else {
	retval = ::select(0, &readfds, &writefds, (fd_set *) NULL,
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
    ::select(0, (fd_set *) NULL, &writefds, (fd_set *) NULL, &timeout);

    /* handle ip name lookup */
    if (FD_ISSET(in, &readfds)) {
	IpAddr::lookup();
    }
    return retval;
}

/*
 * check if UDP challenge met
 */
bool XConnection::udpCheck()
{
    return (name == (char *) NULL);
}

/*
 * read from a connection
 */
int XConnection::read(char *buf, unsigned int len)
{
    int size;

    if (fd == INVALID_SOCKET) {
	return -1;
    }
    if (!FD_ISSET(fd, &readfds)) {
	return 0;
    }
    size = recv(fd, buf, len, 0);
    if (size == SOCKET_ERROR) {
	closesocket(fd);
	FD_CLR(fd, &infds);
	FD_CLR(fd, &outfds);
	FD_CLR(fd, &waitfds);
	fd = INVALID_SOCKET;
	closed++;
    }
    return (size == 0 || size == SOCKET_ERROR) ? -1 : size;
}

/*
 * read a message from a UDP channel
 */
int XConnection::readUdp(char *buf, unsigned int len)
{
    unsigned short size, n;
    char *p, *q, discard;

    EnterCriticalSection(&udpmutex);
    while (bufsz != 0) {
	/* udp buffer is not empty */
	size = (UCHAR(udpbuf[0]) << 8) | UCHAR(udpbuf[1]);
	if (size <= len) {
	    memcpy(buf, udpbuf + 2, len = size);
	}
	--npkts;
	recv(inpkts, &discard, 1, 0);
	bufsz -= size + 2;
	for (p = udpbuf, q = p + size + 2, n = bufsz; n != 0; --n) {
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
 * write to a connection; return the amount of bytes written
 */
int XConnection::write(char *buf, unsigned int len)
{
    int size;

    if (fd == INVALID_SOCKET) {
	return -1;
    }
    if (len == 0) {
	return 0;
    }
    if (!FD_ISSET(fd, &writefds)) {
	/* the write would fail */
	FD_SET(fd, &waitfds);
	return 0;
    }
    if ((size = send(fd, buf, len, 0)) == SOCKET_ERROR &&
	WSAGetLastError() != WSAEWOULDBLOCK) {
	closesocket(fd);
	FD_CLR(fd, &infds);
	FD_CLR(fd, &outfds);
	fd = INVALID_SOCKET;
	closed++;
    } else if ((unsigned int) size != len) {
	/* waiting for wrdone */
	FD_SET(fd, &waitfds);
	FD_CLR(fd, &writefds);
	if (size == SOCKET_ERROR) {
	    return 0;
	}
    }
    return (size == SOCKET_ERROR) ? -1 : size;
}

/*
 * write a message to a UDP channel
 */
int XConnection::writeUdp(char *buf, unsigned int len)
{
    if (fd != INVALID_SOCKET || udpFlag) {
	if (addr->ipnum.ipv6) {
	    struct sockaddr_in6 to;

	    memset(&to, '\0', sizeof(struct sockaddr_in6));
	    to.sin6_family = AF_INET6;
	    memcpy(&to.sin6_addr, &addr->ipnum.addr6, sizeof(struct in6_addr));
	    to.sin6_port = port;
	    return sendto(udescs[at].fd.in6, buf, len, 0,
			  (struct sockaddr *) &to, sizeof(struct sockaddr_in6));
	} else {
	    struct sockaddr_in to;

	    to.sin_family = AF_INET;
	    to.sin_addr.s_addr = addr->ipnum.addr.s_addr;
	    to.sin_port = port;
	    return sendto(udescs[at].fd.in4, buf, len, 0,
			  (struct sockaddr *) &to, sizeof(struct sockaddr_in));
	}
    }
    return -1;
}

/*
 * return TRUE if a connection is ready for output
 */
bool XConnection::wrdone()
{
    if (fd == INVALID_SOCKET || !FD_ISSET(fd, &waitfds)) {
	return TRUE;
    }
    if (FD_ISSET(fd, &writefds)) {
	FD_CLR(fd, &waitfds);
	return TRUE;
    }
    return FALSE;
}

/*
 * return the ip number of a connection
 */
void XConnection::ipnum(char *buf)
{
    if (addr->ipnum.ipv6) {
	struct sockaddr_in6 sin6;
	DWORD length;

	memset(&sin6, '\0', sizeof(struct sockaddr_in6));
	sin6.sin6_family = AF_INET6;
	length = 40;
	sin6.sin6_addr = addr->ipnum.addr6;
	WSAAddressToString((struct sockaddr *) &sin6,
			   sizeof(struct sockaddr_in6),
			   (LPWSAPROTOCOL_INFO) NULL, buf, &length);
    } else {
	strcpy(buf, inet_ntoa(addr->ipnum.addr));
    }
}

/*
 * return the ip name of a connection
 */
void XConnection::ipname(char *buf)
{
    if (addr->name[0] != '\0') {
	strcpy(buf, addr->name);
    } else {
	ipnum(buf);
    }
}

/*
 * look up a host
 */
void *Connection::host(char *addr, unsigned short port, int *len)
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
 * establish an oubound connection
 */
Connection *Connection::connect(void *addr, int len)
{
    XConnection *conn;
    int sock;
    int on;
    unsigned long nonblock;

    if (flist == (Hash::Entry *) NULL) {
       return NULL;
    }

    sock = socket(((struct sockaddr_in *) addr)->sin_family, SOCK_STREAM, 0);
    if (sock < 0) {
	EC->message("socket failed\n");
	return NULL;
    }
    on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) < 0) {
	EC->message("setsockopt failed\n");
	closesocket(sock);
	return NULL;
    }
    on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *) &on,
		   sizeof(on)) < 0) {
	EC->message("setsockopt failed\n");
	closesocket(sock);
	return NULL;
    }
    nonblock = TRUE;
    if (ioctlsocket(sock, FIONBIO, &nonblock) != 0) {
	EC->message("ioctlsocket failed\n");
	closesocket(sock);
	return NULL;
    }

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->fd = sock;
    conn->name = (char *) NULL;
    conn->udpbuf = (char *) NULL;
    conn->addr = (IpAddr *) NULL;
    conn->at = -1;

    ::connect(sock, (struct sockaddr *) addr, len);

    FD_SET(sock, &infds);
    FD_SET(sock, &outfds);
    FD_CLR(sock, &readfds);
    FD_CLR(sock, &writefds);
    FD_SET(sock, &waitfds);
    return conn;
}

/*
 * establish an oubound UDP connection
 */
Connection *Connection::connectDgram(int uport, void *addr, int len)
{
    In46Addr ipnum;
    XConnection *conn, *c;
    Hash::Entry **hash;
    unsigned short port, hashval;

    UNREFERENCED_PARAMETER(len);

    if (flist == (Hash::Entry *) NULL) {
       return NULL;
    }

    if (((sockaddr_in6 *) addr)->sin6_family == AF_INET6) {
	if (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *) addr)->sin6_addr)) {
	    ipnum.addr = *(struct in_addr *)
			&((struct sockaddr_in6 *) addr)->sin6_addr.s6_addr[12];
	    ipnum.ipv6 = FALSE;
	    port = ((struct sockaddr_in6 *) addr)->sin6_port;
	} else {
	    ipnum.addr6 = ((struct sockaddr_in6 *) addr)->sin6_addr;
	    ipnum.ipv6 = TRUE;
	    port = ((struct sockaddr_in6 *) addr)->sin6_port;
	}
    } else {
	ipnum.addr = ((struct sockaddr_in *) addr)->sin_addr;
	ipnum.ipv6 = FALSE;
	port = ((struct sockaddr_in *) addr)->sin_port;
    }

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->name = (char *) NULL;
    MM->staticMode();
    conn->udpbuf = ALLOC(char, BINBUF_SIZE + 2);
    MM->dynamicMode();
    conn->fd = INVALID_SOCKET;
    conn->udpFlag = TRUE;
    conn->addr = NULL;
    conn->port = port;
    conn->at = uport;
    conn->bufsz = 0;
    conn->npkts = 0;

    /*
     * check address family
     */
    if (ipnum.ipv6) {
	if (udescs[uport].fd.in6 == INVALID_SOCKET) {
	    conn->err = 3;
	    return conn;
	}
    } else if (udescs[uport].fd.in4 == INVALID_SOCKET) {
	conn->err = 3;
	return conn;
    }

    if (ipnum.ipv6) {
	hashval = (HM->hashmem((char *) &ipnum,
			       sizeof(struct in6_addr)) ^ port) % udphtabsz;
    } else {
	hashval = (((Uint) ipnum.addr.s_addr) ^ port) % udphtabsz;
    }
    hash = &udphtab[hashval];
    EnterCriticalSection(&udpmutex);
    for (;;) {
	c = (XConnection *) *hash;
	if (c == (XConnection *) NULL) {
	    /*
	     * establish connection
	     */
	    hash = &udphtab[hashval];
	    conn->next = *hash;
	    *hash = conn;
	    conn->err = 0;
	    conn->addr = IpAddr::create(&ipnum);
	    break;
	}
	if (c->at == uport && c->port == port && (
	     (ipnum.ipv6) ?
	      memcmp(&c->addr->ipnum, &ipnum.addr6,
		     sizeof(struct in6_addr)) == 0 :
	      c->addr->ipnum.addr.s_addr == ipnum.addr.s_addr)) {
	    /*
	     * already exists
	     */
	    conn->err = 5;
	    break;
	}
	hash = &c->next;
    }
    LeaveCriticalSection(&udpmutex);

    return conn;
}

/*
 * check for a connection in pending state and see if it is connected.
 */
int XConnection::checkConnected(int *errcode)
{
    int optval;
    socklen_t lon;

    if (udpFlag) {
	/*
	 * UDP connection
	 */
	if (err != 0) {
	    *errcode = err;
	    return -1;
	}
	return 1;
    }

    /*
     * indicate that our fd became invalid.
     */
    if (fd < 0) {
	return -2;
    }

    if (!FD_ISSET(fd, &writefds)) {
	return 0;
    }
    FD_CLR(fd, &waitfds);

    /*
     * Delayed connect completed, check for errors
     */
    lon = sizeof(int);
    /*
     * Get error state for the socket
     */
    *errcode = 0;
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)(&optval), &lon) < 0) {
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
	In46Addr inaddr;

	len = sizeof(sin);
	getpeername(fd, (struct sockaddr *) &sin, &len);
	inaddr.ipv6 = FALSE;
	if (sin.sin6_family == AF_INET6) {
	    inaddr.addr6 = sin.sin6_addr;
	    inaddr.ipv6 = TRUE;
	} else {
	    inaddr.addr = ((struct sockaddr_in *) &sin)->sin_addr;
	}
	addr = IpAddr::create(&inaddr);
	errno = 0;
	return 1;
    }
}


/*
 * export a connection
 */
bool XConnection::cexport(int *fd, char *addr, unsigned short *port, short *at,
			  int *npkts, int *bufsz, char **buf, char *flags)
{
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
 * import a connection
 */
Connection *Connection::import(int fd, char *addr, unsigned short port,
			       short at, int npkts, int bufsz, char *buf,
			       char flags, bool telnet)
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
    return (Connection *) NULL;
}

/*
 * return the number of restored connections
 */
int Connection::fdcount()
{
    return 0;
}

/*
 * pass on a list of connection descriptors
 */
void Connection::fdlist(int *list)
{
    UNREFERENCED_PARAMETER(list);
}

/*
 * close a list of connection descriptors
 */
void Connection::fdclose(int *list, int n)
{
    UNREFERENCED_PARAMETER(list);
    UNREFERENCED_PARAMETER(n);
}
