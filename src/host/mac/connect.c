# include <Files.h>
# include <Folders.h>
# include <Errors.h>
# include <Resources.h>
# include <Memory.h>
# include <Devices.h>
# include <MacTCP.h>
# include <OSUtils.h>
# include "dgd.h"
# include "hash.h"
# include "comm.h"

# define OPENRESOLVER	1L
# define CLOSERESOLVER	2L
# define ADDRTONAME	6L

# define NUM_ALT_ADDRS	4

struct hostInfo {
    int rtnCode;			/* call return code */
    char cname[255];			/* host name */
    unsigned long addr[NUM_ALT_ADDRS];	/* addresses */
};
typedef pascal void (*ResultProcPtr)(struct hostInfo *host, char *data);
typedef OSErr (*dnrfunc)(long, ...);

static Handle code;	/* DNR code resource */
static dnrfunc dnr;	/* DNR function pointer */

/*
 * NAME:	findcdev()
 * DESCRIPTION:	find TCP/IP control panel with the given creator
 */
static short findcdev(long creator, short vref, long dirid)
{
    HFileInfo buf;
    Str255 str;
    short rsrc;

    buf.ioNamePtr = str;
    buf.ioVRefNum = vref;
    buf.ioDirID = dirid;
    buf.ioFDirIndex = 1;

    while (PBGetCatInfoSync((CInfoPBPtr) &buf) == noErr) {
	if (buf.ioFlFndrInfo.fdType == 'cdev' &&
	    buf.ioFlFndrInfo.fdCreator == creator) {
	    rsrc = HOpenResFile(vref, dirid, str, fsRdPerm);
	    if (GetIndResource('dnrp', 1) == NULL) {
		CloseResFile(rsrc);	/* failed */
	    } else {
		return rsrc;	/* found TCP/IP cdev */
	    }
	}
	buf.ioFDirIndex++;
	buf.ioDirID = dirid;
    }

    return -1;
}

/*
 * NAME:	opendnrp()
 * DESCRIPTION:	open the 'dnrp' resource in the TCP/IP control panel
 */
static short opendnrp(void)
{
    short rsrc;
    short vref;
    long dirid;

    /* search for MacTCP 1.1, 2.0.x */
    FindFolder(kOnSystemDisk, kControlPanelFolderType, kDontCreateFolder,
	       &vref, &dirid);
    rsrc = findcdev('ztcp', vref, dirid);
    if (rsrc >= 0) {
	return rsrc;
    }

    /* search for MacTCP 1.0.x */
    FindFolder(kOnSystemDisk, kSystemFolderType, kDontCreateFolder,
	       &vref, &dirid);
    rsrc = findcdev('mtcp', vref, dirid);
    if (rsrc >= 0) {
	return rsrc;
    }

    /* search for MacTCP 1.0.x */
    FindFolder(kOnSystemDisk, kControlPanelFolderType, kDontCreateFolder,
	       &vref, &dirid);
    rsrc = findcdev('mtcp', vref, dirid);
    if (rsrc >= 0) {
	return rsrc;
    }

    return -1;	/* not found */
}

/*
 * NAME:	OpenResolver()
 * DESCRIPTION:	MacTCP: open DNR
 */
static OSErr OpenResolver(char *filename)
{
    short rsrc;
    OSErr result;

    if (dnr != NULL) {
	return noErr;	/* already open */
    }

    /* open 'dnrp' resource in TCP/IP control panel */
    rsrc = opendnrp();
    code = GetIndResource('dnrp', 1);
    if (code == NULL) {
	return ResError();	/* failed; rsrc < 0 */
    }
    DetachResource(code);
    if (rsrc >= 0) {
	CloseResFile(rsrc);
    }
    HLock(code);
    dnr = (dnrfunc) *code;

    /* initialize DNR */
    result = (*dnr)(OPENRESOLVER, filename);
    if (result != noErr) {
	/* init failed, unload DNR */
	HUnlock(code);
	DisposeHandle(code);
	dnr = NULL;
    }
    return result;
}

/*
 * NAME:	CloseResolver()
 * DESCRIPTION:	MacTCP: close DNR
 */
static OSErr CloseResolver(void)
{
    if (dnr == NULL) {
	return notOpenErr;
    }

    /* close & unload */
    (*dnr)(CLOSERESOLVER);
    dnr = NULL;
    HUnlock(code);
    DisposeHandle(code);

    return noErr;
}

/*
 * NAME:	AddrToName()
 * DESCRIPTION:	MacTCP: gethostbyaddr()
 */
static OSErr AddrToName(unsigned long addr, struct hostInfo *host,
			ResultProcPtr report, char *data)
{
    if (dnr == NULL) {
	return notOpenErr;
    }

    return (*dnr)(ADDRTONAME, addr, host, report, data);
}


# define MAXHOSTNAMELEN	256
# define NFREE		32

typedef struct _ipaddr_ {
    struct _ipaddr_ *link;		/* next in hash table */
    struct _ipaddr_ *prev;		/* previous in linked list */
    struct _ipaddr_ *next;		/* next in linked list */
    Uint ref;				/* reference count */
    unsigned long ipnum;		/* ip number */
    char name[MAXHOSTNAMELEN];		/* ip name */
} ipaddr;

static ipaddr **ipahtab;		/* ip address hash table */
static unsigned int ipahtabsz;		/* hash table size */
static ipaddr *qhead, *qtail;		/* request queue */
static ipaddr *ffirst, *flast;		/* free list */
static int nfree;			/* # in free list */
static ipaddr *lastreq;			/* last request */
static struct hostInfo host;		/* host name etc. */
static bool lookup, busy;		/* name resolver activity */

/*
 * NAME:	ipaddr->report()
 * DESCRIPTION:	DNR reporting back
 */
static pascal void ipa_report(struct hostInfo *host, char *busy)
{
    *busy = FALSE;
}

/*
 * NAME:	ipaddr->init()
 * DESCRIPTION:	initialize name lookup
 */
static bool ipa_init(int maxusers)
{
    if (OpenResolver(NULL) != noErr) {
	return FALSE;
    }

    ipahtab = ALLOC(ipaddr*, ipahtabsz = maxusers);
    memset(ipahtab, '\0', ipahtabsz * sizeof(ipaddr*));
    qhead = qtail = ffirst = flast = lastreq = (ipaddr *) NULL;
    nfree = 0;
    lookup = busy = FALSE;

    return TRUE;
}

/*
 * NAME:	ipaddr->finish()
 * DESCRIPTION:	stop name lookup
 */
static void ipa_finish(void)
{
    CloseResolver();
}

/*
 * NAME:	ipaddr->new()
 * DESCRIPTION:	return a new ipaddr
 */
static ipaddr *ipa_new(unsigned long ipnum)
{
    ipaddr *ipa, **hash;

    /* check hash table */
    hash = &ipahtab[ipnum % ipahtabsz];
    while (*hash != (ipaddr *) NULL) {
	ipa = *hash;
	if (ipnum == ipa->ipnum) {
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
		    host.cname[0] = '\0';
		    lookup = busy = TRUE;
		    if (AddrToName(ipnum, &host, ipa_report, &busy) !=
								cacheFault) {
			busy = FALSE;
		    }
		    lastreq = ipa;
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
	    for (h = &ipahtab[ipa->ipnum % ipahtabsz];
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
    ipa->ipnum = ipnum;
    ipa->name[0] = '\0';
    ipa->prev = ipa->next = (ipaddr *) NULL;

    if (!busy) {
	/* send query to name resolver */
	host.cname[0] = '\0';
	lookup = busy = TRUE;
	if (AddrToName(ipnum, &host, ipa_report, &busy) != cacheFault) {
	    busy = FALSE;
	}
	lastreq = ipa;
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
    int i;
    ipaddr *ipa;

    lookup = FALSE;
    if (lastreq != (ipaddr *) NULL) {
	/* read ip name */
	if (host.rtnCode == noErr) {
	    i = strlen(host.cname) - 1;
	    if (host.cname[i] == '.') {
		host.cname[i] = '\0';
	    }
	    strcpy(lastreq->name, host.cname);
	} else {
	    lastreq->name[0] = '\0';
	}
    }

    /* if request queue not empty, write new query */
    if (qhead != (ipaddr *) NULL) {
	ipa = qhead;
	host.cname[0] = '\0';
	lookup = busy = TRUE;
	if (AddrToName(ipa->ipnum, &host, ipa_report, &busy) != cacheFault) {
	    busy = FALSE;
	}
	qhead = ipa->next;
	if (qhead == (ipaddr *) NULL) {
	    qtail = (ipaddr *) NULL;
	} else {
	    qhead->prev = (ipaddr *) NULL;
	}
	ipa->prev = ipa->next = (ipaddr *) NULL;
	lastreq = ipa;
    } else {
	lastreq = (ipaddr *) NULL;
	busy = FALSE;
    }
}



# define TCPBUFSZ	8192

struct _connection_ {
    connection *next;			/* next in queue/list */
    short qType;			/* queue type */
    connection *prev;			/* prev in list */
    connection *hash;			/* next in hashed list */
    char dflag;				/* data arrival flag */
    char cflags;			/* closing/closed flags */
    char sflags;			/* status flags */
    char binary;			/* telnet or binary */
    ipaddr *addr;			/* internet address of connection */
    unsigned short uport;		/* UDP port of connection */
    unsigned short at;			/* port index */
    int ssize;				/* send size */
    int bufsz;				/* UDP buffer size */
    char *udpbuf;			/* UDP read buffer */
    TCPiopb iobuf;			/* I/O parameter buffer */
    struct wdsEntry wds[2];		/* WDS */
};

# define TCP_DATA	0x01		/* data available */
# define TCP_CLOSING	0x01		/* shutdown on other side */
# define TCP_TERMINATED	0x02		/* terminated */
# define TCP_OPEN	0x01		/* open */
# define TCP_CLOSE	0x02		/* closing connection */
# define TCP_BLOCKED	0x04		/* input blocked */
# define TCP_SEND	0x08		/* writing data */
# define TCP_WAIT	0x10		/* waiting for data to be written */
# define TCP_RELEASED	0x20		/* (about to be) released */

static connection *connlist;		/* list of open connections */
static QHdr flist;			/* free connection queue */
static QHdr *telnet;			/* telnet accept queues */
static QHdr *binary;			/* binary accept queues */
static int tcpbufsz;			/* TCP buffer size */
static connection **chtab;		/* UDP challenge hash table */
static connection **udphtab;		/* UDP hash table */
static int udphtabsz;			/* UDP hash table size */
static unsigned short *tports;		/* telnet port numbers */
static unsigned short *bports;		/* binary port numbers */
static int ntports, nbports;		/* # telnet, binary ports */
static UDPiopb *udpbuf;			/* UDP I/O buffers */
static bool udpdata;			/* UDP data ready */


/*
 * NAME:	asr()
 * DESCRIPTION:	asynchronous notification
 */
static pascal void asr(StreamPtr stream, unsigned short event, Ptr userdata,
		       unsigned short term, struct ICMPReport *icmp)
{
    connection *conn;

    conn = (connection *) userdata;
    switch (event) {
    case TCPDataArrival:
    case TCPUrgent:
	conn->dflag = TCP_DATA;
	break;

    case TCPClosing:
	conn->cflags |= TCP_CLOSING;
	break;

    case TCPTerminate:
	conn->cflags |= TCP_TERMINATED;
	break;
    /*
     * currently ignored:
     * ULP timeout
     */
    }
}

/*
 * NAME:	conn->start()
 * DESCRIPTION:	start listening on  a TCP stream
 */
static void conn_start(connection *conn, unsigned short port)
{
    conn->dflag = 0;
    conn->cflags = 0;
    conn->sflags = 0;

    /*
     * start listening
     */
    conn->iobuf.ioResult = inProgress;
    conn->iobuf.csCode = TCPPassiveOpen;
    conn->iobuf.csParam.open.ulpTimeoutValue = 255;
    conn->iobuf.csParam.open.ulpTimeoutAction = 0;	/* report & repeat */
    conn->iobuf.csParam.open.validityFlags = timeoutValue | timeoutAction;
    conn->iobuf.csParam.open.commandTimeoutValue = 0;
    conn->iobuf.csParam.open.remoteHost = 0;
    conn->iobuf.csParam.open.remotePort = 0;
    conn->iobuf.csParam.open.localHost = 0;
    conn->iobuf.csParam.open.localPort = port;
    conn->iobuf.csParam.open.dontFrag = 0;
    conn->iobuf.csParam.open.timeToLive = 0;
    conn->iobuf.csParam.open.security = 0;
    conn->iobuf.csParam.open.optionCnt = 0;
    conn->iobuf.csParam.open.userDataPtr = (Ptr) conn;

    PBControlAsync((ParmBlkPtr) &conn->iobuf);
}

/*
 * NAME:	tcpcompletion()
 * DESCRIPTION:	conn start completion
 */
static void tcpcompletion(struct TCPiopb *iobuf)
{
    connection *conn;
    char type;
    int port;
    QHdr *queue;

    conn = (connection *) iobuf->csParam.open.userDataPtr;
    if (conn->sflags & TCP_RELEASED) {
	return;
    }
    if (iobuf->ioResult == noErr) {
	type = conn->binary;
	port = conn->at;
	queue = (type) ? &binary[port] : &telnet[port];
	conn->sflags = TCP_OPEN;	/* opened */
	if (flist.qHead == NULL) {
	    /* cannot start listening for another one right away, alas */
	    queue->qFlags = FALSE;
	    return;
	}
	conn = (connection *) flist.qHead;
	Dequeue((QElemPtr) conn, &flist);
	conn->binary = type;
	conn->at = port;
	Enqueue((QElemPtr) conn, queue);
    }

    /* (re)start */
    conn_start(conn, iobuf->csParam.open.localPort);
}

/*
 * NAME:	udpcompletion()
 * DESCRIPTION:	udp message received
 */
static void udpcompletion(struct UDPiopb *iobuf)
{
    udpdata = TRUE;
}

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connections
 */
bool conn_init(int nusers, char **thosts, char **bhosts, unsigned short *tp,
	       unsigned short *bp, int ntp, int nbp)
{
    IOParam device;
    GetAddrParamBlock addr;
    UDPiopb udp;
    int n;
    connection *conn;

    connlist = NULL;
    memset(&flist, '\0', sizeof(flist));
    ntports = ntp;
    if (ntp != 0) {
	for (n = 0; n < ntp; n++) {
	    if (thosts[n] != (char *) NULL) {
		P_message("Config error: cannot bind to address\012");	/* LF */
		return FALSE;
	    }
	}
	telnet = ALLOC(QHdr, ntp);
	memset(telnet, '\0', ntp * sizeof(QHdr));
	tports = tp;
    }
    nbports = nbp;
    if (nbp != 0) {
	for (n = 0; n < nbp; n++) {
	    if (bhosts[n] != (char *) NULL) {
		P_message("Config error: cannot bind to address\012");	/* LF */
		return FALSE;
	    }
	}
	binary = ALLOC(QHdr, nbp);
	memset(binary, '\0', nbp * sizeof(QHdr));
	bports = bp;
	udpbuf = ALLOC(UDPiopb, nbp);
    }
    if (ntp + nbp == 0) {
	return TRUE;	/* don't initialize MacTCP unless it's actually used */
    }

    /*
     * initialize MacTCP
     */
    device.ioNamePtr = "\p.IPP";
    device.ioVRefNum = 0;
    device.ioVersNum = 0;
    device.ioPermssn = fsCurPerm;
    device.ioMisc = 0;
    if (PBOpenSync((ParmBlkPtr) &device) != noErr) {
	P_message("Config error: cannot initialize MacTCP\012");	/* LF */
	return FALSE;
    }
    if (!ipa_init(nusers)) {
	P_message("Config error: cannot initialize DNR\012");	/* LF */
	return FALSE;
    }
    addr.ioCRefNum = device.ioRefNum;
    addr.csCode = ipctlGetAddr;
    if (PBControlSync((ParmBlkPtr) &addr) != noErr) {
	P_message("Config error: cannot get host address\012");	/* LF */
	return FALSE;
    }
    udp.ioCRefNum = device.ioRefNum;
    udp.csCode = UDPMaxMTUSize;
    udp.csParam.mtu.remoteHost = addr.ourAddress;
    if (PBControlSync((ParmBlkPtr) &udp) != noErr) {
	P_message("Config error: cannot get MTU size\012");	/* LF */
	return FALSE;
    }
    tcpbufsz = 4 * udp.csParam.mtu.mtuSize + 1024;
    if (tcpbufsz < TCPBUFSZ) {
	tcpbufsz = TCPBUFSZ;
    }

    /* open UDP ports */
    for (n = 0; n < nbp; n++) {
	udpbuf[n].ioCRefNum = device.ioRefNum;
	udpbuf[n].csCode = UDPCreate;
	udpbuf[n].csParam.create.rcvBuff = (Ptr) ALLOC(char, 32768);
	udpbuf[n].csParam.create.rcvBuffLen = 32768;
	udpbuf[n].csParam.create.notifyProc = NULL;
	udpbuf[n].csParam.create.localPort = bp[n];
	if (PBControlSync((ParmBlkPtr) &udpbuf[n]) != noErr) {
	    P_message("Config error: cannot open UDP port\012");	/* LF */
	    return FALSE;
	}
    }
    udphtab = ALLOC(connection*, udphtabsz = nusers);
    memset(udphtab, '\0', nusers * sizeof(connection*));
    chtab = ALLOC(connection*, nusers);
    memset(chtab, '\0', nusers * sizeof(connection*));

    /* initialize TCP streams */
    if (nusers < ntp + nbp) {
	nusers = ntp + nbp;
    }
    conn = ALLOC(connection, nusers);
    for (n = 0; n < nusers; n++) {
	/* open TCP stream */
	conn->iobuf.ioCRefNum = device.ioRefNum;
	conn->iobuf.csCode = TCPCreate;
	conn->iobuf.csParam.create.rcvBuff = (Ptr) ALLOC(char, tcpbufsz);
	conn->iobuf.csParam.create.rcvBuffLen = tcpbufsz;
	conn->iobuf.csParam.create.notifyProc = asr;
	conn->iobuf.csParam.create.userDataPtr = (Ptr) conn;
	if (PBControlSync((ParmBlkPtr) &conn->iobuf) != noErr) {
	    /* failed (too many TCP streams?) */
	    FREE(conn->iobuf.csParam.create.rcvBuff);
	    if (n < ntp + nbp) {
		P_message("Config error: cannot open TCP port\012");	/* LF */
		return FALSE;
	    }
	    break;
	}
	conn->iobuf.ioCompletion = tcpcompletion;
	conn->qType = 0;
	Enqueue((QElemPtr) conn, &flist);
	conn++;
    }

    return TRUE;
}

/*
 * NAME:	conn->release()
 * DESCRIPTION:	(forcibly) release all TCP streams in a list of connections
 */
static void conn_release(connection *conn)
{
    TCPiopb iobuf;

    while (conn != NULL) {
	conn->sflags |= TCP_RELEASED;
	iobuf = conn->iobuf;
	iobuf.csCode = TCPRelease;
	PBControlSync((ParmBlkPtr) &iobuf);
	conn = conn->next;
    }
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish(void)
{
    if (ntports + nbports != 0) {
	UDPiopb iobuf;
	int n;

	/* remove all existing connections */
	conn_release(connlist);
	for (n = 0; n < ntports; n++) {
	    conn_release((connection *) telnet[n].qHead);
	}
	for (n = 0; n < nbports; n++) {
	    conn_release((connection *) binary[n].qHead);
	    iobuf = udpbuf[n];
	    iobuf.csCode = UDPRelease;
	    PBControlSync((ParmBlkPtr) &iobuf);
	}
	conn_release((connection *) flist.qHead);

	ipa_finish();
    }
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen(void)
{
    connection *conn;
    int n;

    for (n = 0; n < ntports; n++) {
	/* start listening on telnet port */
	conn = (connection *) flist.qHead;
	Dequeue((QElemPtr) conn, &flist);
	telnet[n].qFlags = TRUE;
	conn->binary = FALSE;
	conn->at = n;
	Enqueue((QElemPtr) conn, &telnet[n]);
	conn_start(conn, tports[n]);
    }

    udpdata = FALSE;
    for (n = 0; n < nbports; n++) {
	/* start listening on binary port */
	conn = (connection *) flist.qHead;
	Dequeue((QElemPtr) conn, &flist);
	binary[n].qFlags = TRUE;
	conn->binary = TRUE;
	conn->at = n;
	Enqueue((QElemPtr) conn, &binary[n]);
	conn_start(conn, bports[n]);

	/* start reading on UDP port */
	udpbuf[n].ioCompletion = udpcompletion;
	udpbuf[n].csCode = UDPRead;
	udpbuf[n].csParam.receive.timeOut = 0;
	udpbuf[n].csParam.receive.secondTimeStamp = 0;
	PBControlAsync((ParmBlkPtr) &udpbuf[n]);
    }
}

/*
 * NAME:	conn->accept()
 * DESCRIPTION:	accept a new connection on a port
 */
static connection *conn_accept(QHdr *queue)
{
    connection *conn;

    conn = (connection *) queue->qHead;
    if (conn != NULL && conn->sflags == TCP_OPEN) {
	Dequeue((QElemPtr) conn, queue);
	conn->prev = NULL;
	conn->next = connlist;
	if (connlist != NULL) {
	    connlist->prev = conn;
	}
	connlist = conn;

	/* fully initialize connection struct */
	conn->iobuf.ioCompletion = NULL;
	conn->addr = ipa_new(conn->iobuf.csParam.open.remoteHost);
	conn->uport = 0;
	conn->ssize = 0;
	conn->udpbuf = (char *) NULL;
	conn->wds[0].length = 0;
	m_static();
	conn->wds[0].ptr = (Ptr) ALLOC(char, tcpbufsz);
	m_dynamic();
	conn->wds[1].length = 0;
	conn->wds[1].ptr = NULL;
	return conn;
    } else {
	return NULL;
    }
}

/*
 * NAME:	conn->tnew6()
 * DESCRIPTION:	can't accept an IPv6 connection
 */
connection *conn_tnew6(int n)
{
    return NULL;
}

/*
 * NAME:	conn->bnew6()
 * DESCRIPTION:	can't accept an IPv6 connection
 */
connection *conn_bnew6(int n)
{
    return NULL;
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew(int n)
{
    return conn_accept(&telnet[n]);
}

/*
 * NAME:	conn->bnew()
 * DESCRIPTION:	accept a new binary connection
 */
connection *conn_bnew(int n)
{
    return conn_accept(&binary[n]);
}

/*
 * NAME:        conn->udp()
 * DESCRIPTION: enable the UDP channel of a binary connection
 */
bool conn_udp(connection *conn, char *challenge, unsigned int len)
{
    char buffer[UDPHASHSZ];
    connection **hash;

    if (len == 0 || len > BINBUF_SIZE || conn->udpbuf != (char *) NULL) {
	return FALSE;   /* invalid challenge */
    }

    if (len >= UDPHASHSZ) {
	memcpy(buffer, challenge, UDPHASHSZ);
    } else {
	memset(buffer, '\0', UDPHASHSZ);
	memcpy(buffer, challenge, len);
    }
    hash = &chtab[hashmem(buffer, UDPHASHSZ) % udphtabsz];
    while (*hash != (connection *) NULL) {
	if ((*hash)->bufsz == len &&
	    memcmp((*hash)->udpbuf, challenge, len) == 0) {
	    return FALSE;       /* duplicate challenge */
	}
    }

    conn->hash = *hash;
    *hash = conn;
    m_static();
    conn->udpbuf = ALLOC(char, BINBUF_SIZE);
    m_dynamic();
    memset(conn->udpbuf, '\0', UDPHASHSZ);
    memcpy(conn->udpbuf, challenge, conn->bufsz = len);
    conn->binary++;

    return TRUE;
}

/*
 * NAME:	conn->flush()
 * DESCRIPTION:	flush data on a connection, return TRUE if done
 */
static bool conn_flush(connection *conn)
{
    if (conn->sflags & TCP_SEND) {
	if (conn->iobuf.ioResult == inProgress) {
	    return FALSE;	/* send still in progress */
	}
	if (conn->iobuf.ioResult != noErr) {
	    /* send failed */
	    return TRUE;	/* can't send any more */
	}
	conn->sflags &= ~TCP_SEND;
	if (conn->ssize != 0) {
	    /* copy overlapping block */
	    memcpy(conn->wds[0].ptr,
		   (char *) conn->wds[0].ptr + conn->wds[0].length,
		   conn->ssize);
	}
    }
    if (conn->ssize != 0) {
	/* more to send */
	conn->iobuf.ioResult = inProgress;
	conn->iobuf.csCode = TCPSend;
	conn->iobuf.csParam.send.ulpTimeoutValue = 120;
	conn->iobuf.csParam.send.ulpTimeoutAction = 1;	/* abort */
	conn->iobuf.csParam.send.validityFlags = timeoutValue | timeoutAction;
	conn->iobuf.csParam.send.pushFlag = 1;	/* send right away */
	conn->iobuf.csParam.send.urgentFlag = 0;
	conn->wds[0].length = conn->ssize;
	conn->ssize = 0;
	conn->iobuf.csParam.send.wdsPtr = (Ptr) conn->wds;
	PBControlAsync((ParmBlkPtr) &conn->iobuf);
	if (conn->iobuf.ioResult == inProgress) {
	    conn->sflags |= TCP_SEND;
	    return FALSE;
	}
    }
    conn->wds[0].length = 0;
    return TRUE;
}

/*
 * NAME:	conn->del()
 * DESCRIPTION:	delete a connection
 */
void conn_del(connection *conn)
{
    connection **hash;
    int n;

    conn->sflags |= TCP_CLOSE;
    if (!(conn->cflags & TCP_TERMINATED)) {
	if (!conn_flush(conn)) {
	    /* buffer not flushed */
	    return;
	}
	if (conn->sflags & TCP_OPEN) {
	    /* close connection */
	    conn->sflags &= ~TCP_OPEN;
	    conn->iobuf.ioResult = inProgress;
	    conn->iobuf.csCode = TCPClose;
	    conn->iobuf.csParam.close.ulpTimeoutValue = 60;
	    conn->iobuf.csParam.close.ulpTimeoutAction = 1;	/* abort */
	    conn->iobuf.csParam.close.validityFlags = timeoutValue |
						      timeoutAction;
	    PBControlAsync((ParmBlkPtr) &conn->iobuf);
	}
	if (conn->iobuf.ioResult == inProgress) {
	    return;
	}
	conn->iobuf.csCode = TCPAbort;
	PBControlSync((ParmBlkPtr) &conn->iobuf);
    }

    /* prepare for new use */
    FREE(conn->wds[0].ptr);
    conn->iobuf.ioCompletion = tcpcompletion;
    if (conn->udpbuf != (char *) NULL) {
	if (conn->binary > TRUE) {
	    hash = (connection **) &chtab[hashmem(conn->udpbuf,
						  UDPHASHSZ) % udphtabsz];
	} else {
	    hash = &udphtab[(conn->addr->ipnum ^ conn->uport) % udphtabsz];
	}       
	while (*hash != conn) {
	    hash = &(*hash)->hash;
	}
	*hash = conn->hash;
	FREE(conn->udpbuf);
    }
    if (conn->prev != NULL) {
	conn->prev->next = conn->next;
    } else {
	connlist = conn->next;
    }
    if (conn->next != NULL) {
	conn->next->prev = conn->prev;
    }
    ipa_del(conn->addr);

    /*
     * see if connection can be re-used right away
     */
    for (n = 0; n < nbports; n++) {
	if (!binary[n].qFlags) {
	    binary[n].qFlags = TRUE;
	    conn->binary = TRUE;
	    conn->at = n;
	    Enqueue((QElemPtr) conn, &binary[n]);
	    conn_start(conn, bports[n]);
	    return;
	}
    }
    for (n = 0; n < ntports; n++) {
	if (!telnet[n].qFlags) {
	    telnet[n].qFlags = TRUE;
	    conn->binary = FALSE;
	    conn->at = n;
	    Enqueue((QElemPtr) conn, &telnet[n]);
	    conn_start(conn, tports[n]);
	    return;
	}
    }

    /*
     * put in free list
     */
    Enqueue((QElemPtr) conn, &flist);
}

/*
 * NAME:	conn->block()
 * DESCRIPTION:	block or unblock input from a connection
 */
void conn_block(connection *conn, int flag)
{
    if (flag) {
	conn->sflags |= TCP_BLOCKED;
    } else {
	conn->sflags &= ~TCP_BLOCKED;
    }
}

/*
 * NAME:	conn->udprecv()
 * DESCRIPTION:	receive UDP packets
 */
static bool conn_udprecv(int n)
{
    char buffer[UDPHASHSZ];
    UDPiopb *udp;
    bool stop;
    unsigned int size;
    connection **hash, *conn;
    char *p;

    stop = FALSE;
    udp = &udpbuf[n];
    size = udp->csParam.receive.rcvBuffLen;
    hash = &udphtab[(udp->csParam.receive.remoteHost ^
			    udp->csParam.receive.remotePort) % udphtabsz];
    for (;;) {
	conn = *hash;
	if (conn == (connection *) NULL) {
	    if (size >= UDPHASHSZ) {
		memcpy(buffer, udp->csParam.receive.rcvBuff, UDPHASHSZ);
	    } else {
		memset(buffer, '\0', UDPHASHSZ);
		memcpy(buffer, udp->csParam.receive.rcvBuff, size);
	    }
	    hash = (connection **) &chtab[hashmem(buffer, UDPHASHSZ) %
								udphtabsz];
	    while ((conn=*hash) != (connection *) NULL) {
		if (conn->bufsz == size &&
		    memcmp(conn->udpbuf, udp->csParam.receive.rcvBuff,
			   size) == 0 &&
		    conn->addr->ipnum == udp->csParam.receive.remoteHost) {
		    /*
		     * attach new UDP channel
		     */
		    *hash = conn->hash;
		    --conn->binary;
		    conn->bufsz = 0;
		    conn->uport = udp->csParam.receive.remotePort;
		    hash = &udphtab[(udp->csParam.receive.remoteHost ^
						conn->uport) % udphtabsz];
		    conn->hash = *hash;
		    *hash = conn;

		    stop = TRUE;
		    break;
		}
		hash = &conn->hash;
	    }
	    break;
	}
    
	if (conn->at == n &&
	    conn->addr->ipnum == udp->csParam.receive.remoteHost &&
	    conn->uport == udp->csParam.receive.remotePort) {
	    /*
	     * packet from known correspondent
	     */
	    if (conn->bufsz + size <= BINBUF_SIZE - 2) {
		p = conn->udpbuf + conn->bufsz;
		*p++ = size >> 8;
		*p++ = size;
		memcpy(p, udp->csParam.receive.rcvBuff, size);
		conn->bufsz += size + 2;
		stop = TRUE;
	    }
	    break;
	}
	hash = &conn->hash;
    }
    /* else from unknown source: ignore */
    
    udp->csCode = UDPBfrReturn;
    PBControlSync((ParmBlkPtr) udp);
    udp->ioCompletion = udpcompletion;
    udp->csCode = UDPRead;
    PBControlAsync((ParmBlkPtr) udp);

    return stop;
}

/*
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(Uint t, unsigned int mtime)
{
    long ticks;
    bool stop;
    int n;
    connection *conn, *next, **hash;

    if (mtime != 0xffffL) {
	ticks = TickCount() + t * 60 + mtime * 100L / 1667;
    } else {
	ticks = 0x7fffffffL;
    }
    stop = FALSE;
    do {
	getevent();
	if (lookup && !busy) {
	    ipa_lookup();
	}
	if (udpdata) {
	    udpdata = FALSE;
	    for (n = 0; n < nbports; n++) {
		stop |= conn_udprecv(n);
	    }
	}
	for (conn = connlist; conn != NULL; conn = next) {
	    next = conn->next;
	    if (conn->sflags & TCP_CLOSE) {
		conn_del(conn);
	    } else {
		conn_flush(conn);
		if ((conn->dflag && !(conn->sflags & TCP_BLOCKED)) ||
		    conn->cflags ||
		    ((conn->sflags & TCP_WAIT) &&
		      conn->wds[0].length + conn->ssize != tcpbufsz)) {
		    stop = TRUE;
		}
	    }
	}
	if (stop) {
	    return 1;
	}

	for (n = 0; n < ntports; n++) {
	    conn = (connection *) telnet[n].qHead;
	    if (conn != NULL && conn->sflags == TCP_OPEN) {
		return 1;	/* new telnet connection */
	    }
	}
	for (n = 0; n < nbports; n++) {
	    conn = (connection *) binary[n].qHead;
	    if (conn != NULL && conn->sflags == TCP_OPEN) {
		return 1;	/* new binary connection */
	    }
	}
    } while (ticks - (long) TickCount() > 0 && !intr);

    return 0;
}

/*
 * NAME:	conn->udpcheck()
 * DESCRIPTION:	check if UDP challenge met
 */
bool conn_udpcheck(connection *conn)
{
    return (conn->binary == TRUE);
}

/*
 * NAME:	conn->read()
 * DESCRIPTION:	read from a connection
 */
int conn_read(connection *conn, char *buf, unsigned int len)
{
    TCPiopb iobuf;

    if (conn->cflags) {
	return -1;	/* terminated */
    }
    if (!(conn->dflag) || (conn->sflags & TCP_BLOCKED)) {
	return 0;	/* no data */
    }
    conn->dflag = 0;

    /* get amount of available data */
    iobuf = conn->iobuf;
    iobuf.csCode = TCPStatus;
    if (PBControlSync((ParmBlkPtr) &iobuf) != noErr) {
	return -1;	/* can't get status */
    }
    if (len > iobuf.csParam.status.amtUnreadData) {
	len = iobuf.csParam.status.amtUnreadData;
    }

    /* get available data */
    iobuf.csCode = TCPRcv;
    iobuf.csParam.receive.commandTimeoutValue = 0;
    iobuf.csParam.receive.rcvBuff = (Ptr) buf;
    iobuf.csParam.receive.rcvBuffLen = len;
    if (PBControlSync((ParmBlkPtr) &iobuf) != noErr ||
	len != iobuf.csParam.receive.rcvBuffLen) {
	return -1;
    }
    return len;
}

/*
 * NAME:        conn->udpread()
 * DESCRIPTION: read a message from a UDP channel
 */
int conn_udpread(connection *conn, char *buf, unsigned int len)
{
    unsigned short size;
    char *p, *q;

    while (conn->bufsz != 0) {  
	/* udp buffer is not empty */
	size = ((unsigned char) conn->udpbuf[0] << 8) |
		(unsigned char) conn->udpbuf[1];
	if (size <= len) {
	    memcpy(buf, conn->udpbuf + 2, len = size);
	}
	conn->bufsz -= size + 2;
	memcpy(conn->udpbuf, conn->udpbuf + size + 2, conn->bufsz);
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

    if (conn->cflags) {
	return -1;
    }
    if (len == 0) {
	return 0;
    }
    size = tcpbufsz - (conn->wds[0].length + conn->ssize);
    if (size == 0) {
	conn->sflags |= TCP_WAIT;
	return 0;
    }

    if (len > size) {
	len = size;
	conn->sflags |= TCP_WAIT;
    }
    memcpy((char *) conn->wds[0].ptr + conn->wds[0].length + conn->ssize, buf,
	   len);
    conn->ssize += len;
    conn_flush(conn);
    return len;
}

/*
 * NAME:        conn->udpwrite()
 * DESCRIPTION: write a message to a UDP channel
 */
int conn_udpwrite(connection *conn, char *buf, unsigned int len)
{
    struct wdsEntry wds[2];
    UDPiopb iobuf;

    if (conn->cflags) {
	return 0;
    }
    wds[0].ptr = (Ptr) buf;
    wds[0].length = len;
    wds[1].ptr = NULL;
    wds[1].length = 0;
    iobuf = udpbuf[conn->at];
    iobuf.csCode = UDPWrite;
    iobuf.csParam.send.remoteHost = conn->addr->ipnum;
    iobuf.csParam.send.remotePort = conn->uport;
    iobuf.csParam.send.wdsPtr = (Ptr) wds;
    iobuf.csParam.send.checkSum = 1;
    iobuf.csParam.send.sendLength = 0;
    return (PBControlSync((ParmBlkPtr) &iobuf) != noErr) ? -1 : len;
}

/*
 * NAME:	conn->wrdone()
 * DESCRIPTION:	return TRUE if a connection is ready for output
 */
bool conn_wrdone(connection *conn)
{
    if (conn->cflags || !(conn->sflags & TCP_WAIT)) {
	return TRUE;
    }
    if (conn->wds[0].length + conn->ssize != tcpbufsz) {
	conn->sflags &= ~TCP_WAIT;
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
    unsigned long ipnum;

    ipnum = conn->addr->ipnum;
    sprintf(buf, "%d.%d.%d.%d", (unsigned char) (ipnum >> 24),
	    (unsigned char) (ipnum >> 16), (unsigned char) (ipnum >> 8),
	    (unsigned char) (ipnum));
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
