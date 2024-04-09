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

# include <sys/time.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <signal.h>
# include <pthread.h>
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
#  ifdef AF_INET6	/* define INET6 if AF_INET6 exists */
#   define INET6
#  endif
# endif

# ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN	1025
# endif

# ifndef INADDR_NONE
# define INADDR_NONE	0xffffffffL
# endif

struct In46Addr {
    union {
# ifdef INET6
	struct in6_addr addr6;		/* IPv6 addr */
# endif
	struct in_addr addr;		/* IPv4 addr */
    };
    bool ipv6;				/* IPv6? */
};

struct Pipes {
    int in;				/* input file descriptor */
    int out;				/* output file descriptor */
};

extern "C" {

/*
 * host name lookup thread
 */
static void *ipa_run(void *arg)
{
    char buf[sizeof(In46Addr)];
    struct Pipes *inout;
    struct hostent *host;
    int len;

    inout = (Pipes *) arg;

    while (read(inout->in, buf, sizeof(In46Addr)) > 0) {
	/* lookup host */
# ifdef INET6
	if (((In46Addr *) &buf)->ipv6) {
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
	    (void) write(inout->out, name, len);
	    name[0] = '\0';
	} else {
	    (void) write(inout->out, "", 1);	/* failure */
	}
    }

    close(inout->in);
    close(inout->out);
    return NULL;
}

}

class IpAddr {
public:
    void del();

    static bool init(int maxusers);
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

static int in = -1, out = -1;		/* pipe to/from name resolver */
static int addrtype;			/* network address family */
static IpAddr **ipahtab;		/* ip address hash table */
static unsigned int ipahtabsz;		/* hash table size */
static IpAddr *qhead, *qtail;		/* request queue */
static IpAddr *ffirst, *flast;		/* free list */
static int nfree;			/* # in free list */
static IpAddr *lastreq;			/* last request */
static bool busy;			/* name resolver busy */
static pthread_t lookup;		/* name lookup thread */


/*
 * initialize name lookup
 */
bool IpAddr::init(int maxusers)
{
    if (in < 0) {
	int fd[4];
	static Pipes inout;

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
	inout.in = fd[0];
	inout.out = fd[3];
	if (pthread_create(&::lookup, NULL, &ipa_run, &inout) < 0) {
	    perror("pthread_create");
	    close(fd[0]);
	    close(fd[1]);
	    close(fd[2]);
	    close(fd[3]);
	    return FALSE;
	}
	in = fd[2];
	out = fd[1];
    } else if (busy) {
	char buf[MAXHOSTNAMELEN];

	/* discard ip name */
	(void) read(in, buf, MAXHOSTNAMELEN);
    }

    ipahtab = ALLOC(IpAddr*, ipahtabsz = maxusers);
    memset(ipahtab, '\0', ipahtabsz * sizeof(IpAddr*));
    qhead = qtail = ffirst = flast = lastreq = (IpAddr *) NULL;
    nfree = 0;
    busy = FALSE;

    return TRUE;
}

/*
 * stop name lookup
 */
void IpAddr::finish()
{
    close(out);
    close(in);
}

/*
 * return a new ipaddr
 */
IpAddr *IpAddr::create(In46Addr *ipnum)
{
    IpAddr *ipa, **hash;

    /* check hash table */
# ifdef INET6
    if (ipnum->ipv6) {
	hash = &ipahtab[HM->hashmem((char *) ipnum,
				    sizeof(struct in6_addr)) % ipahtabsz];
    } else
# endif
    {
	hash = &ipahtab[(Uint) ipnum->addr.s_addr % ipahtabsz];
    }
    while (*hash != (IpAddr *) NULL) {
	ipa = *hash;
# ifdef INET6
	if (ipnum->ipv6 == ipa->ipnum.ipv6 &&
	    ((ipnum->ipv6) ?
	      memcmp(&ipnum->addr6, &ipa->ipnum.addr6,
		     sizeof(struct in6_addr)) == 0 :
	      ipnum->addr.s_addr == ipa->ipnum.addr.s_addr)) {
# else
	if (ipnum->addr.s_addr == ipa->ipnum.addr.s_addr) {
# endif
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
		    (void) write(out, (char *) ipnum, sizeof(In46Addr));
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
# ifdef INET6
	    if (ipa->ipnum.ipv6) {
		h = &ipahtab[HM->hashmem((char *) &ipa->ipnum,
					 sizeof(struct in6_addr)) % ipahtabsz];
	    } else
# endif
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
	(void) write(out, (char *) ipnum, sizeof(In46Addr));
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
	lastreq->name[read(in, lastreq->name, MAXHOSTNAMELEN)] = '\0';
    } else {
	char buf[MAXHOSTNAMELEN];

	/* discard ip name */
	(void) read(in, buf, MAXHOSTNAMELEN);
    }

    /* if request queue not empty, write new query */
    if (qhead != (IpAddr *) NULL) {
	ipa = qhead;
	(void) write(out, (char *) &ipa->ipnum, sizeof(In46Addr));
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
    XConnection() : fd(-1) { }

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

# ifdef INET6
    static int port6(int *fd, int type, struct sockaddr_in6 *sin6,
		     unsigned int port);
# endif
    static int port4(int *fd, int type, struct sockaddr_in *sin,
		     unsigned int port);
# ifdef INET6
    static XConnection *create6(int portfd, int port);
# endif
    static XConnection *create(int portfd, int port);
    static XConnection *createUdp(int port);

    int fd;				/* file descriptor */
    int npkts;				/* # packets in buffer */
    int bufsz;				/* # bytes in buffer */
    int err;				/* state of outbound connection */
    char *udpbuf;			/* datagram buffer */
    IpAddr *addr;			/* internet address of connection */
    unsigned short port;		/* UDP port of connection */
    short at;				/* port connection was accepted at */
};

struct PortDesc {
    int in6;				/* IPv6 port descriptor */
    int in4;				/* IPv4 port descriptor */
};

class Udp {
public:
# ifdef INET6
    static void recv6(int n);
# endif
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
static int inpkts, outpkts;		/* UDP packet notification pipe */
static pthread_t udp;			/* UDP thread */
static pthread_mutex_t udpmutex;	/* UDP mutex */
static bool udpstop;			/* stop UDP thread? */

# ifdef INET6
/*
 * receive an UDP packet
 */
void Udp::recv6(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in6 from;
    socklen_t fromlen;
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
    pthread_mutex_lock(&udpmutex);
    for (;;) {
	conn = (XConnection *) *hash;
	if (conn == (XConnection *) NULL) {
	    if (!Config::attach(n)) {
		if (!udescs[n].accept) {
		    if (IN6_IS_ADDR_V4MAPPED(&from.sin6_addr)) {
			/* convert to IPv4 address */
			udescs[n].addr.addr = *(struct in_addr *)
						    &from.sin6_addr.s6_addr[12];
			udescs[n].addr.ipv6 = FALSE;
		    } else {
			udescs[n].addr.addr6 = from.sin6_addr;
			udescs[n].addr.ipv6 = TRUE;
		    }
		    udescs[n].port = from.sin6_port;
		    udescs[n].hashval = hashval;
		    udescs[n].size = size;
		    memcpy(udescs[n].buffer, buffer, size);
		    udescs[n].accept = TRUE;
		    (void) write(outpkts, buffer, 1);
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
		(void) write(outpkts, buffer, 1);
	    }
	    break;
	}
	hash = &conn->next;
    }
    pthread_mutex_unlock(&udpmutex);
}
# endif

/*
 * receive an UDP packet
 */
void Udp::recv(int n)
{
    char buffer[BINBUF_SIZE];
    struct sockaddr_in from;
    socklen_t fromlen;
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
    pthread_mutex_lock(&udpmutex);
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
		    (void) write(outpkts, buffer, 1);
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
		(void) write(outpkts, buffer, 1);
	    }
	    break;
	}
	hash = &conn->next;
    }
    pthread_mutex_unlock(&udpmutex);
}

extern "C" {

/*
 * UDP thread
 */
static void *udp_run(void *arg)
{
    fd_set udpfds;
    fd_set readfds;
    fd_set errorfds;
    int maxufd, n, retval;

    FD_ZERO(&udpfds);
    maxufd = 0;
    for (n = 0; n < nudescs; n++) {
# ifdef INET6
	if (udescs[n].fd.in6 >= 0) {
	    FD_SET(udescs[n].fd.in6, &udpfds);
	    if (udescs[n].fd.in6 > maxufd) {
		maxufd = udescs[n].fd.in6;
	    }
	}
# endif
	if (udescs[n].fd.in4 >= 0) {
	    FD_SET(udescs[n].fd.in4, &udpfds);
	    if (udescs[n].fd.in4 > maxufd) {
		maxufd = udescs[n].fd.in4;
	    }
	}
    }

    for (;;) {
	memcpy(&readfds, &udpfds, sizeof(fd_set));
	memcpy(&errorfds, &udpfds, sizeof(fd_set));
	retval = select(maxufd + 1, &readfds, (fd_set *) NULL, &errorfds,
		        (struct timeval *) NULL);
	if (udpstop) {
	    break;
	}

	if (retval > 0) {
	    for (n = 0; n < nudescs; n++) {
# ifdef INET6
		if (udescs[n].fd.in6 >= 0 &&
		    FD_ISSET(udescs[n].fd.in6, &readfds)) {
		    Udp::recv6(n);
		}
# endif
		if (udescs[n].fd.in4 >= 0 &&
		    FD_ISSET(udescs[n].fd.in4, &readfds)) {
		    Udp::recv(n);
		}
	    }
	}
    }

    pthread_mutex_destroy(&udpmutex);
    close(inpkts);
    close(outpkts);
    return (void *) NULL;
}

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
static int maxfd;			/* largest fd opened yet */
static int closed;			/* #fds closed in write */

# ifdef INET6
/*
 * open an IPv6 port
 */
int XConnection::port6(int *fd, int type, struct sockaddr_in6 *sin6,
		       unsigned int port)
{
    int on;

    if ((*fd=socket(AF_INET6, type, 0)) < 0) {
	perror("socket IPv6");
	return FALSE;
    }
    on = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
# if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
    on = 1;
    if (setsockopt(*fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	return FALSE;
    }
# endif
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

    if (type == SOCK_STREAM) {
	if (*fd > maxfd) {
	    maxfd = *fd;
	}
	FD_SET(*fd, &infds);
    }
    return TRUE;
}
# endif

/*
 * open an IPv4 port
 */
int XConnection::port4(int *fd, int type, struct sockaddr_in *sin,
		       unsigned int port)
{
    int on;

    if ((*fd=socket(AF_INET, type, 0)) < 0) {
	perror("socket");
	return FALSE;
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

    if (type == SOCK_STREAM) {
	if (*fd > maxfd) {
	    maxfd = *fd;
	}
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
# ifdef INET6
    struct sockaddr_in6 sin6;
# endif
    struct sockaddr_in sin;
    struct hostent *host;
    int n, fds[2];
    XConnection **conn;
    bool ipv6, ipv4;
# ifdef AI_DEFAULT
    int err;
# endif

    if (!IpAddr::init(maxusers)) {
	return FALSE;
    }

    addrtype = PF_INET;

    nusers = 0;

    maxfd = 0;
    FD_ZERO(&infds);
    FD_ZERO(&outfds);
    FD_ZERO(&waitfds);
    FD_SET(in, &infds);
    closed = 0;

    (void) pipe(fds);
    inpkts = fds[0];
    outpkts = fds[1];
    FD_SET(inpkts, &infds);
    if (inpkts > maxfd) {
	maxfd = inpkts;
    }

    ntdescs = ntports;
    if (ntports != 0) {
	tdescs = ALLOC(PortDesc, ntports);
	memset(tdescs, -1, ntports * sizeof(PortDesc));
    }
    nbdescs = nbports;
    if (nbports != 0) {
	bdescs = ALLOC(PortDesc, nbports);
	memset(bdescs, -1, nbports * sizeof(PortDesc));
    }
    nudescs = ndports;
    if (ndports != 0) {
	udescs = ALLOC(Udp, ndports);
	memset(udescs, -1, ndports * sizeof(Udp));
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
	    sin.sin_addr.s_addr = htonl(INADDR_ANY);
	    ipv4 = TRUE;
	} else {
# ifdef INET6
	    if (inet_pton(AF_INET6, thosts[n], &sin6.sin6_addr) > 0) {
		sin6.sin6_family = AF_INET6;
		ipv6 = TRUE;
	    } else {
# ifdef AI_DEFAULT
		host = getipnodebyname(thosts[n], AF_INET6, 0, &err);
		if (host != (struct hostent *) NULL) {
		    if (host->h_length != 4) {
			memcpy(&sin6.sin6_addr, host->h_addr, host->h_length);
			ipv6 = TRUE;
		    }
		    freehostent(host);
		}
# else
		host = gethostbyname2(thosts[n], AF_INET6);
		if (host != (struct hostent *) NULL && host->h_length != 4) {
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
	    EC->message("unknown host %s\012", thosts[n]);	/* LF */
	    return FALSE;
	}

# ifdef INET6
	if (ipv6 &&
	    !XConnection::port6(&tdescs[n].in6, SOCK_STREAM, &sin6, tports[n]))
	{
	    return FALSE;
	}
# endif
	if (ipv4 &&
	    !XConnection::port4(&tdescs[n].in4, SOCK_STREAM, &sin, tports[n])) {
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
	    sin.sin_addr.s_addr = htonl(INADDR_ANY);
	    ipv4 = TRUE;
	} else {
# ifdef INET6
	    if (inet_pton(AF_INET6, bhosts[n], &sin6.sin6_addr) > 0) {
		sin6.sin6_family = AF_INET6;
		ipv6 = TRUE;
	    } else {
# ifdef AI_DEFAULT
		host = getipnodebyname(bhosts[n], AF_INET6, 0, &err);
		if (host != (struct hostent *) NULL) {
		    if (host->h_length != 4) {
			memcpy(&sin6.sin6_addr, host->h_addr, host->h_length);
			ipv6 = TRUE;
		    }
		    freehostent(host);
		}
# else
		host = gethostbyname2(bhosts[n], AF_INET6);
		if (host != (struct hostent *) NULL && host->h_length != 4) {
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
	    EC->message("unknown host %s\012", bhosts[n]);	/* LF */
	    return FALSE;
	}

# ifdef INET6
	if (ipv6 &&
	    !XConnection::port6(&bdescs[n].in6, SOCK_STREAM, &sin6, bports[n]))
	{
	    return FALSE;
	}
# endif
	if (ipv4 &&
	    !XConnection::port4(&bdescs[n].in4, SOCK_STREAM, &sin, bports[n])) {
	    return FALSE;
	}
    }

    for (n = 0; n < nudescs; n++) {
	/* datagram ports */
	ipv6 = FALSE;
	ipv4 = FALSE;
	if (dhosts[n] == (char *) NULL) {
# ifdef INET6
	    sin6.sin6_addr = in6addr_any;
	    ipv6 = TRUE;
# endif
	    sin.sin_addr.s_addr = htonl(INADDR_ANY);
	    ipv4 = TRUE;
	} else {
# ifdef INET6
	    if (inet_pton(AF_INET6, dhosts[n], &sin6.sin6_addr) > 0) {
		sin6.sin6_family = AF_INET6;
		ipv6 = TRUE;
	    } else {
# ifdef AI_DEFAULT
		host = getipnodebyname(dhosts[n], AF_INET6, 0, &err);
		if (host != (struct hostent *) NULL) {
		    if (host->h_length != 4) {
			memcpy(&sin6.sin6_addr, host->h_addr, host->h_length);
			ipv6 = TRUE;
		    }
		    freehostent(host);
		}
# else
		host = gethostbyname2(dhosts[n], AF_INET6);
		if (host != (struct hostent *) NULL && host->h_length != 4) {
		    memcpy(&sin6.sin6_addr, host->h_addr, host->h_length);
		    ipv6 = TRUE;
		}
# endif
	    }
# endif
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

# ifdef INET6
	if (ipv6 && !XConnection::port6(&udescs[n].fd.in6, SOCK_DGRAM, &sin6,
					dports[n])) {
	    return FALSE;
	}
# endif
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
    if (nudescs != 0) {
	udpstop = FALSE;
	pthread_mutex_init(&udpmutex, NULL);
	if (pthread_create(&::udp, NULL, &udp_run, (void *) NULL) < 0) {
	    perror("pthread_create");
	    return FALSE;
	}
    }

    return TRUE;
}

/*
 * clean up connections
 */
void Connection::clear()
{
    int n;

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
    }
    udpstop = TRUE;
    for (n = 0; n < nudescs; n++) {
	if (udescs[n].fd.in6 >= 0) {
	    close(udescs[n].fd.in6);
	}
	if (udescs[n].fd.in4 >= 0) {
	    close(udescs[n].fd.in4);
	}
    }

    IpAddr::finish();
}

/*
 * terminate connections
 */
void Connection::finish()
{
    int n;
    XConnection **conn;

    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	if ((*conn)->fd >= 0) {
	    close((*conn)->fd);
	}
    }
}

/*
 * start listening on telnet port and binary port
 */
void Connection::listen()
{
    int n;

    for (n = 0; n < ntdescs; n++) {
	if (tdescs[n].in6 >= 0) {
	    if (::listen(tdescs[n].in6, 64) < 0) {
		perror("listen");
	    } else if (fcntl(tdescs[n].in6, F_SETFL, FNDELAY) < 0) {
		perror("fcntl");
	    } else {
		continue;
	    }
	    EC->fatal("conn_listen failed");
	}
    }
    for (n = 0; n < ntdescs; n++) {
	if (tdescs[n].in4 >= 0) {
	    if (::listen(tdescs[n].in4, 64) < 0) {
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
	    EC->fatal("conn_listen failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	if (bdescs[n].in6 >= 0) {
	    if (::listen(bdescs[n].in6, 64) < 0) {
		perror("listen");
	    } else if (fcntl(bdescs[n].in6, F_SETFL, FNDELAY) < 0) {
		perror("fcntl");
	    } else {
		continue;
	    }
	    EC->fatal("conn_listen failed");
	}
    }
    for (n = 0; n < nbdescs; n++) {
	if (bdescs[n].in4 >= 0) {
	    if (::listen(bdescs[n].in4, 64) < 0) {
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
	    } else {
		continue;
	    }
	    EC->fatal("conn_listen failed");
	}
    }
}

# ifdef INET6
/*
 * accept a new ipv6 connection
 */
XConnection *XConnection::create6(int portfd, int port)
{
    int fd;
    socklen_t len;
    struct sockaddr_in6 sin6;
    In46Addr addr;
    XConnection *conn;

    if (!FD_ISSET(portfd, &readfds)) {
	return (XConnection *) NULL;
    }
    len = sizeof(sin6);
    fd = accept(portfd, (struct sockaddr *) &sin6, &len);
    if (fd < 0) {
	FD_CLR(portfd, &readfds);
	return (XConnection *) NULL;
    }
    fcntl(fd, F_SETFL, FNDELAY);

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->name = (char *) NULL;
    conn->fd = fd;
    conn->udpbuf = (char *) NULL;
    if (IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) {
	/* convert to IPv4 address */
	addr.addr = *(struct in_addr *) &sin6.sin6_addr.s6_addr[12];
	addr.ipv6 = FALSE;
    } else {
	addr.addr6 = sin6.sin6_addr;
	addr.ipv6 = TRUE;
    }
    conn->addr = IpAddr::create(&addr);
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
 * accept a new ipv4 connection
 */
XConnection *XConnection::create(int portfd, int port)
{
    int fd;
    socklen_t len;
    struct sockaddr_in sin;
    In46Addr addr;
    XConnection *conn;

    if (!FD_ISSET(portfd, &readfds)) {
	return (XConnection *) NULL;
    }
    len = sizeof(sin);
    fd = accept(portfd, (struct sockaddr *) &sin, &len);
    if (fd < 0) {
	FD_CLR(portfd, &readfds);
	return (XConnection *) NULL;
    }
    fcntl(fd, F_SETFL, FNDELAY);

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->name = (char *) NULL;
    conn->fd = fd;
    conn->udpbuf = (char *) NULL;
    addr.addr = sin.sin_addr;
    addr.ipv6 = FALSE;
    conn->addr = IpAddr::create(&addr);
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
    pthread_mutex_lock(&udpmutex);
    conn->next = *hash;
    *hash = conn;
    conn->fd = -2;
    conn->addr = IpAddr::create(&udescs[port].addr);
    conn->port = udescs[port].port;
    conn->at = port;
    conn->udpbuf[0] = udescs[port].size >> 8;
    conn->udpbuf[1] = udescs[port].size;
    memcpy(conn->udpbuf + 2, udescs[port].buffer, udescs[port].size);
    conn->bufsz = udescs[port].size + 2;
    conn->npkts = 1;
    udescs[port].accept = FALSE;
    pthread_mutex_unlock(&udpmutex);

    return conn;
}

/*
 * accept a new telnet connection
 */
Connection *Connection::createTelnet6(int port)
{
# ifdef INET6
    int fd;

    fd = tdescs[port].in6;
    if (fd >= 0) {
	return XConnection::create6(fd, port);
    }
# endif
    return (Connection *) NULL;
}

/*
 * accept a new binary connection
 */
Connection *Connection::create6(int port)
{
# ifdef INET6
    int fd;

    fd = bdescs[port].in6;
    if (fd >= 0) {
	return XConnection::create6(fd, port);
    }
# endif
    return (Connection *) NULL;
}

/*
 * accept a new datagram connection
 */
Connection *Connection::createDgram6(int port)
{
# ifdef INET6
    if (udescs[port].accept) {
	return XConnection::createUdp(port);
    }
# endif
    return (Connection *) NULL;
}

/*
 * accept a new telnet connection
 */
Connection *Connection::createTelnet(int port)
{
    int fd;

    fd = tdescs[port].in4;
    if (fd >= 0) {
	return XConnection::create(fd, port);
    }
    return (Connection *) NULL;
}

/*
 * accept a new binary connection
 */
Connection *Connection::create(int port)
{
    int fd;

    fd = bdescs[port].in4;
    if (fd >= 0) {
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
    pthread_mutex_lock(&udpmutex);
    hash = chtab->lookup(buffer, FALSE);
    while ((conn=(XConnection *) *hash) != (XConnection *) NULL &&
	   memcmp(conn->name, buffer, UDPHASHSZ) == 0) {
	if (conn->bufsz == len && memcmp(conn->udpbuf, challenge, len) == 0) {
	    pthread_mutex_unlock(&udpmutex);
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
    pthread_mutex_unlock(&udpmutex);

    return TRUE;
}

/*
 * delete a connection
 */
void XConnection::del()
{
    Hash::Entry **hash;

    if (fd >= 0) {
	close(fd);
	FD_CLR(fd, &infds);
	FD_CLR(fd, &outfds);
	FD_CLR(fd, &waitfds);
	fd = -1;
    } else if (fd == -1) {
	--closed;
    }
    if (udpbuf != (char *) NULL) {
	pthread_mutex_lock(&udpmutex);
	if (addr != (IpAddr *) NULL) {
	    if (name != (char *) NULL) {
		hash = chtab->lookup(name, FALSE);
# ifdef INET6
	    } else if (addr->ipnum.ipv6) {
		hash = &udphtab[(HM->hashmem((char *) &addr->ipnum,
				 sizeof(struct in6_addr)) ^ port) % udphtabsz];
# endif
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
	    (void) ::read(inpkts, udpbuf, npkts);
	}
	pthread_mutex_unlock(&udpmutex);
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
    if (fd >= 0) {
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
    if (fd >= 0) {
	shutdown(fd, SHUT_WR);
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
    if (closed != 0) {
	t = 0;
	mtime = 0;
    }
    if (mtime != 0xffff) {
	timeout.tv_sec = t;
	timeout.tv_usec = mtime * 1000L;
	retval = ::select(maxfd + 1, &readfds, &writefds, (fd_set *) NULL,
			  &timeout);
    } else {
	retval = ::select(maxfd + 1, &readfds, &writefds, (fd_set *) NULL,
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
    ::select(maxfd + 1, (fd_set *) NULL, &writefds, (fd_set *) NULL, &timeout);

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

    if (fd < 0) {
	return -1;
    }
    if (!FD_ISSET(fd, &readfds)) {
	return 0;
    }
    size = ::read(fd, buf, len);
    if (size < 0) {
	close(fd);
	FD_CLR(fd, &infds);
	FD_CLR(fd, &outfds);
	FD_CLR(fd, &waitfds);
	fd = -1;
	closed++;
    }
    return (size == 0) ? -1 : size;
}

/*
 * read a message from a UDP channel
 */
int XConnection::readUdp(char *buf, unsigned int len)
{
    unsigned short size, n;
    char *p, *q, discard;

    pthread_mutex_lock(&udpmutex);
    while (bufsz != 0) {
	/* udp buffer is not empty */
	size = (UCHAR(udpbuf[0]) << 8) | UCHAR(udpbuf[1]);
	if (size <= len) {
	    memcpy(buf, udpbuf + 2, len = size);
	}
	--npkts;
	(void) ::read(inpkts, &discard, 1);
	bufsz -= size + 2;
	for (p = udpbuf, q = p + size + 2, n = bufsz; n != 0; --n) {
	    *p++ = *q++;
	}
	if (len == size) {
	    pthread_mutex_unlock(&udpmutex);
	    return len;
	}
    }
    pthread_mutex_unlock(&udpmutex);
    return -1;
}

/*
 * write to a connection; return the amount of bytes written
 */
int XConnection::write(char *buf, unsigned int len)
{
    int size;

    if (fd < 0) {
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
    if ((size=::write(fd, buf, len)) < 0 && errno != EWOULDBLOCK) {
	close(fd);
	FD_CLR(fd, &infds);
	FD_CLR(fd, &outfds);
	fd = -1;
	closed++;
    } else if (size != len) {
	/* waiting for wrdone */
	FD_SET(fd, &waitfds);
	FD_CLR(fd, &writefds);
	if (size < 0) {
	    return 0;
	}
    }
    return size;
}

/*
 * write a message to a UDP channel
 */
int XConnection::writeUdp(char *buf, unsigned int len)
{
    if (fd != -1) {
# ifdef INET6
	if (addr->ipnum.ipv6) {
	    struct sockaddr_in6 to;

	    memset(&to, '\0', sizeof(struct sockaddr_in6));
	    to.sin6_family = AF_INET6;
	    memcpy(&to.sin6_addr, &addr->ipnum.addr6, sizeof(struct in6_addr));
	    to.sin6_port = port;
	    return sendto(udescs[at].fd.in6, buf, len, 0,
			  (struct sockaddr *) &to, sizeof(struct sockaddr_in6));
	} else
# endif
	{
	    struct sockaddr_in to;

	    memset(&to, '\0', sizeof(struct sockaddr_in));
	    to.sin_family = AF_INET;
	    to.sin_addr = addr->ipnum.addr;
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
    if (fd < 0 || !FD_ISSET(fd, &waitfds)) {
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
# ifdef INET6
    /* IPv6: maxlen 39 */
    if (addr->ipnum.ipv6) {
	inet_ntop(AF_INET6, &addr->ipnum, buf, 40);
    } else
# endif
    {
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
# ifdef INET6
	struct sockaddr_in6 sin6;
# endif
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

# ifdef INET6
    memset(&inaddr.sin6, '\0', sizeof(struct sockaddr_in6));
    inaddr.sin6.sin6_family = AF_INET6;
    *len = sizeof(struct sockaddr_in6);
    if (inet_pton(AF_INET6, addr, &inaddr.sin6) > 0) {
	inaddr.sin6.sin6_family = AF_INET6;
	inaddr.sin6.sin6_port = htons(port);
	return &inaddr;
    } else {
# ifdef AI_DEFAULT
	int err;

	host = getipnodebyname(addr, AF_INET6, 0, &err);
# else
	host = gethostbyname2(addr, AF_INET6);
# endif
	if (host != (struct hostent *) NULL) {
	    if (host->h_length != 4) {
		memcpy(&inaddr.sin6.sin6_addr, host->h_addr, host->h_length);
# ifdef AI_DEFAULT
		freehostent(host);
# endif
		inaddr.sin6.sin6_port = htons(port);
		return &inaddr;
	    }
# ifdef AI_DEFAULT
	    freehostent(host);
# endif
	}
    }
# endif

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
    long arg;

    if (flist == (Hash::Entry *) NULL) {
       return NULL;
    }

    sock = socket(((struct sockaddr_in *) addr)->sin_family, SOCK_STREAM, 0);
    if (sock < 0) {
	perror("socket");
	return NULL;
    }
    on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	close(sock);
	return NULL;
    }
#ifdef SO_OOBINLINE
    on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, (char *) &on,
		   sizeof(on)) < 0) {
	perror("setsockopt");
	close(sock);
	return NULL;
    }
#endif
    if ((arg = fcntl(sock, F_GETFL, NULL)) < 0) {
       perror("fcntl");
       close(sock);
       return NULL;
    }
    arg |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, arg) < 0) {
       perror("fcntl");
       close(sock);
       return NULL;
    }

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->fd = sock;
    conn->name = (char *) NULL;
    conn->udpbuf = (char *) NULL;
    conn->addr = (IpAddr *) NULL;
    conn->at = -1;

    if (::connect(sock, (struct sockaddr *) addr, len) == 0) {
	conn->err = 0;
    } else {
	conn->err = errno;
	if (conn->err == EINPROGRESS) {
	    conn->err = 0;
	}
    }

    FD_SET(sock, &infds);
    FD_SET(sock, &outfds);
    FD_CLR(sock, &readfds);
    FD_CLR(sock, &writefds);
    FD_SET(sock, &waitfds);
    if (sock > maxfd) {
	maxfd = sock;
    }
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

    if (flist == (Hash::Entry *) NULL) {
       return NULL;
    }

# ifdef INET6
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
    } else
# endif
    {
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
    conn->fd = -2;
    conn->addr = NULL;
    conn->port = port;
    conn->at = uport;
    conn->bufsz = 0;
    conn->npkts = 0;

# ifdef INET6
    /*
     * check address family
     */
    if (ipnum.ipv6) {
	if (udescs[uport].fd.in6 < 0) {
	    conn->err = 3;
	    return conn;
	}
    } else if (udescs[uport].fd.in4 < 0) {
	conn->err = 3;
	return conn;
    }

    if (ipnum.ipv6) {
	hashval = (HM->hashmem((char *) &ipnum,
				sizeof(struct in6_addr)) ^ port) % udphtabsz;
    } else
# endif
    {
	hashval = (((Uint) ipnum.addr.s_addr) ^ port) % udphtabsz;
    }
    hash = &udphtab[hashval];
    pthread_mutex_lock(&udpmutex);
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
# ifdef INET6
	     (ipnum.ipv6) ?
	      memcmp(&c->addr->ipnum, &ipnum.addr6,
		     sizeof(struct in6_addr)) == 0 :
# endif
	      c->addr->ipnum.addr.s_addr == ipnum.addr.s_addr)) {
	    /*
	     * already exists
	     */
	    conn->err = 5;
	    break;
	}
	hash = &c->next;
    }
    pthread_mutex_unlock(&udpmutex);

    return conn;
}

/*
 * check for a connection in pending state and see if it is connected.
 */
int XConnection::checkConnected(int *errcode)
{
    int optval;
    socklen_t lon;

    if (fd == -2) {
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

    if (err != 0) {
	optval = err;
    } else {
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
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)(&optval), &lon) < 0) {
	    return -1;
	}
    }
    *errcode = 0;
    if (optval != 0) {
	switch (optval) {
	case ECONNREFUSED:
	    *errcode = 1;
	    break;

	case EHOSTUNREACH:
	    *errcode = 2;
	    break;

	case ENETUNREACH:
	    *errcode = 3;
	    break;

	case ETIMEDOUT:
	    *errcode = 4;
	    break;
	}
	errno = optval;
	return -1;
    } else {
# ifdef INET6
	struct sockaddr_in6 sin;
# else
	struct sockaddr_in sin;
# endif
	socklen_t len;
	In46Addr inaddr;

	len = sizeof(sin);
	getpeername(fd, (struct sockaddr *) &sin, &len);
	inaddr.ipv6 = FALSE;
# ifdef INET6
	if (sin.sin6_family == AF_INET6) {
	    inaddr.addr6 = sin.sin6_addr;
	    inaddr.ipv6 = TRUE;
	} else
# endif
	inaddr.addr = ((struct sockaddr_in *) &sin)->sin_addr;
	addr = IpAddr::create(&inaddr);
	errno = 0;
	return 1;
    }
}


# define CONN_READF	0x01	/* read flag set */
# define CONN_WRITEF	0x02	/* write flag set */
# define CONN_WAITF	0x04	/* wait flag set */
# define CONN_UCHAL	0x08	/* UDP challenge issued */
# define CONN_UCHAN	0x10	/* UDP channel established */
# define CONN_ADDR	0x20	/* has an address */

/*
 * export a connection
 */
bool XConnection::cexport(int *fd, char *addr, unsigned short *port, short *at,
			  int *npkts, int *bufsz, char **buf, char *flags)
{
    *fd = this->fd;
    *port = this->port;
    if (this->fd != -1) {
	*flags = 0;
	*at = this->at;
	*npkts = this->npkts;
	*bufsz = this->bufsz;
	*buf = this->udpbuf;
	if (FD_ISSET(this->fd, &readfds)) {
	    *flags |= CONN_READF;
	}
	if (FD_ISSET(this->fd, &writefds)) {
	    *flags |= CONN_WRITEF;
	}
	if (FD_ISSET(this->fd, &waitfds)) {
	    *flags |= CONN_WAITF;
	}
	if (udpbuf != (char *) NULL) {
	    if (name != NULL) {
		*flags |= CONN_UCHAL;
	    } else {
		*flags |= CONN_UCHAN;
	    }
	}
	if (this->addr != (IpAddr *) NULL) {
	    memcpy(addr, &this->addr->ipnum, sizeof(In46Addr));
	    *flags |= CONN_ADDR;
	}
    }

    return TRUE;
}

/*
 * import a connection
 */
Connection *Connection::import(int fd, char *addr, unsigned short port,
			       short at, int npkts, int bufsz, char *buf,
			       char flags, bool telnet)
{
    In46Addr inaddr;
    XConnection *conn;

    conn = (XConnection *) flist;
    flist = conn->next;
    conn->fd = fd;
    conn->name = (char *) NULL;
    conn->udpbuf = (char *) NULL;
    conn->addr = (IpAddr *) NULL;
    conn->bufsz = 0;
    conn->npkts = 0;
    conn->port = port;
    conn->at = -1;

    if (fd >= 0) {
# ifdef INET6
	struct sockaddr_in6 sin;
# else
	struct sockaddr_in sin;
# endif
	socklen_t len;

	len = sizeof(sin);
	if (getpeername(fd, (struct sockaddr *) &sin, &len) < 0 &&
	    errno != ENOTCONN) {
	    return NULL;
	}

	FD_SET(fd, &infds);
	FD_SET(fd, &outfds);
	if (flags & CONN_READF) {
	    FD_SET(fd, &readfds);
	}
	if (flags & CONN_WRITEF) {
	    FD_SET(fd, &writefds);
	}
	if (flags & CONN_WAITF) {
	    FD_SET(fd, &waitfds);
	}
	if (fd > maxfd) {
	    maxfd = fd;
	}
    }

    if (fd != -1) {
	if (at >= 0 && at >= ((telnet) ? ntdescs : nbdescs)) {
	    at = -1;
	}
	conn->at = at;
	if (flags & CONN_ADDR) {
	    memcpy(&inaddr, addr, sizeof(In46Addr));
	    conn->addr = IpAddr::create(&inaddr);
	}

	if (at >= 0) {
	    if (flags & CONN_UCHAL) {
		conn->udp(buf, bufsz);
	    }
	    if (flags & CONN_UCHAN) {
		Hash::Entry **hash;

		conn->bufsz = bufsz;
		MM->staticMode();
		conn->udpbuf = ALLOC(char, BINBUF_SIZE + 2);
		MM->dynamicMode();
		memcpy(conn->udpbuf, buf, bufsz);
# ifdef INET6
		if (inaddr.ipv6) {
		    hash = &udphtab[(HM->hashmem((char *) &inaddr.addr6,
			   sizeof(struct in6_addr)) ^ conn->port) % udphtabsz];
		} else
# endif
		hash = &udphtab[((Uint) inaddr.addr.s_addr ^ conn->port) %
								    udphtabsz];
		conn->next = *hash;
		*hash = conn;
	    }
	    conn->npkts = npkts;
	    (void) ::write(outpkts, conn->udpbuf, npkts);
	}
    } else {
	closed++;
    }

    return conn;
}

/*
 * return the number of restored connections
 */
int Connection::fdcount()
{
    int count, n;
    XConnection **conn;

    count = 0;
    for (conn = connections, n = nusers; n > 0; conn++, --n) {
	if ((*conn)->fd >= 0) {
	    count++;
	}
    }
    return count;
}

/*
 * pass on a list of connection descriptors
 */
void Connection::fdlist(int *list)
{
    int n;
    XConnection **conn;

    for (conn = connections, n = nusers; n > 0; conn++, --n) {
	if ((*conn)->fd >= 0) {
	    *list++ = (*conn)->fd;
	}
    }
}

/*
 * close a list of connection descriptors
 */
void Connection::fdclose(int *list, int n)
{
    while (n > 0) {
	close(list[--n]);
    }
}
