# include <Devices.h>
# include <MacTCP.h>
# include <OSUtils.h>
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "comm.h"


# define TCPBUFSZ	8192

struct _connection_ {
    connection *next;			/* next in list */
    short qType;			/* queue type */
    connection *prev;			/* prev in list */
    char dflag;				/* data arrival flag */
    char cflags;			/* closing/closed flags */
    char sflags;			/* status flags */
    long addr;				/* internet address of connection */
    int ssize;				/* send size */
    TCPiopb iobuf;			/* I/O parameter buffer */
    struct wdsEntry wds[2];		/* WDS */
};

# define TCP_DATA	0x01		/* data available */
# define TCP_CLOSING	0x01		/* shutdown on other side */
# define TCP_TERMINATED	0x02		/* terminated */
# define TCP_OPEN	0x01		/* open */
# define TCP_SEND	0x02		/* writing data */
# define TCP_WAIT	0x04		/* waiting for data to be written */
# define TCP_RELEASED	0x08		/* (about to be) released */

static connection *connlist;		/* list of open connections */
static QHdr flist;			/* free connection queue */
static QHdr telnet;			/* telnet accept queue */
static QHdr binary;			/* binary accept queue */
static int tcpbufsz;			/* TCP buffer size */
static unsigned int telnet_port;	/* telnet port number */
static unsigned int binary_port;	/* binary port number */
static short mactcp;			/* MacTCP driver handle */


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
     * urgent data
     */
    }
}

/*
 * NAME:	conn->start()
 * DESCRIPTION:	start listening on  a TCP stream
 */
static void conn_start(connection *conn, unsigned int port)
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
 * NAME:	completion()
 * DESCRIPTION:	conn start completion
 */
static void completion(struct TCPiopb *iobuf)
{
    connection *conn;
    QHdr *queue;

    conn = (connection *) iobuf->csParam.open.userDataPtr;
    if (conn->sflags & TCP_RELEASED) {
	return;
    }
    if (iobuf->ioResult == noErr) {
	queue = (iobuf->csParam.open.localPort == telnet_port) ?
		 &telnet : &binary;
	conn->sflags = TCP_OPEN;	/* opened */
	if (flist.qHead == NULL) {
	    /* cannot start listening on another one right away, alas */
	    queue->qFlags = FALSE;
	    return;
	}
	conn = (connection *) flist.qHead;
	Dequeue((QElemPtr) conn, &flist);
	Enqueue((QElemPtr) conn, queue);
    }

    /* (re)start */
    conn_start(conn, iobuf->csParam.open.localPort);
}

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connections
 */
void conn_init(int nusers, unsigned int t_port, unsigned int b_port)
{
    IOParam device;
    GetAddrParamBlock addr;
    UDPiopb iobuf;
    connection *conn;

    /* initialize MacTCP */
    device.ioNamePtr = "\p.IPP";
    device.ioVRefNum = 0;
    device.ioVersNum = 0;
    device.ioPermssn = fsCurPerm;
    device.ioMisc = 0;
    if (PBOpenSync((ParmBlkPtr) &device) != noErr) {
	P_message("Config error: cannot initialize MacTCP\012"); /* LF */
	exit(1);
    }
    mactcp = device.ioRefNum;
    addr.ioCRefNum = mactcp;
    addr.csCode = ipctlGetAddr;
    if (PBControlSync((ParmBlkPtr) &addr) != noErr) {
	P_message("Config error: cannot get host address\012");	/* LF */
	exit(1);
    }
    iobuf.ioCRefNum = mactcp;
    iobuf.csCode = UDPMaxMTUSize;
    iobuf.csParam.mtu.remoteHost = addr.ourAddress;
    if (PBControlSync((ParmBlkPtr) &iobuf) != noErr) {
	P_message("Config error: cannot get MTU size\012");	/* LF */
	exit(1);
    }
    tcpbufsz = 4 * iobuf.csParam.mtu.mtuSize + 1024;
    if (tcpbufsz < TCPBUFSZ) {
	tcpbufsz = TCPBUFSZ;
    }

    /* initialize TCP streams */
    if (nusers < 2) {
	nusers = 2;
    }
    conn = ALLOC(connection, nusers);
    while (--nusers >= 0) {
	/* open TCP stream */
	conn->iobuf.ioCompletion = NewTCPIOCompletionProc(completion);
	conn->iobuf.ioCRefNum = mactcp;
	conn->iobuf.csCode = TCPCreate;
	conn->iobuf.csParam.create.rcvBuff = (Ptr) ALLOC(char, tcpbufsz);
	conn->iobuf.csParam.create.rcvBuffLen = tcpbufsz;
	conn->iobuf.csParam.create.notifyProc = NewTCPNotifyProc(asr);
	conn->iobuf.csParam.create.userDataPtr = (Ptr) conn;
	if (PBControlSync((ParmBlkPtr) &conn->iobuf) != noErr) {
	    /* failed (too many TCP streams?) */
	    FREE(conn->iobuf.csParam.create.rcvBuff);
	    break;
	}
	conn->iobuf.ioCompletion = NewTCPIOCompletionProc(completion);
	conn->qType = 0;
	Enqueue((QElemPtr) conn, &flist);
	conn++;
    }

    telnet_port = t_port;
    binary_port = b_port;
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
    /* remove all existing connections */
    conn_release(connlist);
    conn_release((connection *) telnet.qHead);
    conn_release((connection *) binary.qHead);
    conn_release((connection *) flist.qHead);
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen(void)
{
    connection *tconn, *bconn;

    tconn = (connection *) flist.qHead;
    Dequeue((QElemPtr) tconn, &flist);
    bconn = (connection *) flist.qHead;
    Dequeue((QElemPtr) bconn, &flist);

    /* start listening on telnet port */
    Enqueue((QElemPtr) tconn, &telnet);
    telnet.qFlags = TRUE;
    conn_start(tconn, telnet_port);

    /* start listening on binary port */
    Enqueue((QElemPtr) bconn, &binary);
    binary.qFlags = TRUE;
    conn_start(bconn, binary_port);
}

/*
 * NAME:	conn->accept()
 * DESCRIPTION:	accept a new connection on a port
 */
static connection *conn_accept(QHdr *queue, unsigned int portno)
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
	DisposeRoutineDescriptor(conn->iobuf.ioCompletion);
	conn->iobuf.ioCompletion = NULL;
	conn->addr = conn->iobuf.csParam.open.remoteHost;
	conn->ssize = 0;
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
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew(void)
{
    return conn_accept(&telnet, telnet_port);
}

/*
 * NAME:	conn->bnew()
 * DESCRIPTION:	accept a new binary connection
 */
connection *conn_bnew(void)
{
    return conn_accept(&binary, binary_port);
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
    conn->iobuf.ioCompletion = NewTCPIOCompletionProc(completion);

    /*
     * put in free list
     */
    if (conn->prev != NULL) {
	conn->prev->next = conn->next;
    } else {
	connlist = conn->next;
    }
    if (conn->next != NULL) {
	conn->next->prev = conn->prev;
    }
    Enqueue((QElemPtr) conn, &flist);

    /*
     * see if connection can be re-used right away
     */
    if (!telnet.qFlags) {
	if (Dequeue((QElemPtr) conn, &flist) == noErr) {
	    telnet.qFlags = TRUE;
	    Enqueue((QElemPtr) conn, &telnet);
	    conn_start(conn, telnet_port);
	}
    } else if (!binary.qFlags) {
	 if (Dequeue((QElemPtr) conn, &flist) == noErr) {
	    binary.qFlags = TRUE;
	    Enqueue((QElemPtr) conn, &binary);
	    conn_start(conn, binary_port);
	}
    }
}

/*
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(int wait)
{
    long ticks;
    bool stop;
    connection *conn, *next;

    ticks = TickCount() + wait * 60;
    stop = FALSE;
    do {
	getevent();

	for (conn = connlist; conn != NULL; conn = next) {
	    next = conn->next;
	    if (conn->sflags & TCP_OPEN) {
		conn_flush(conn);
		if (conn->dflag || conn->cflags ||
		    ((conn->sflags & TCP_WAIT) &&
		      conn->wds[0].length + conn->ssize != tcpbufsz)) {
		    stop = TRUE;
		}
	    } else {
	    	conn_del(conn);
	    }
	}
	if (stop) {
	    return 1;
	}

	conn = (connection *) telnet.qHead;
	if (conn != NULL && conn->sflags == TCP_OPEN) {
	    return 1;	/* new telnet connection */
	}
	conn = (connection *) binary.qHead;
	if (conn != NULL && conn->sflags == TCP_OPEN) {
	    return 1;	/* new binary connection */
	}
    } while (TickCount() - ticks < 0);

    return 0;
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
    if (!(conn->dflag)) {
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
 * NAME:	conn->write()
 * DESCRIPTION:	write to a connection; return the amount of bytes written
 */
int conn_write(connection *conn, char *buf, unsigned int len)
{
    int size;

    if (conn->cflags) {
	return 0;
    }
    conn->sflags &= ~TCP_WAIT;
    if (len == 0) {
	return 0;	/* send_message("") can be used to flush buffer */
    }
    size = tcpbufsz - (conn->wds[0].length + conn->ssize);
    if (size == 0) {
	conn->sflags |= TCP_WAIT;
	return -1;
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
char *conn_ipnum(connection *conn)
{
    static char buf[16];

    sprintf(buf, "%d.%d.%d.%d", UCHAR(conn->addr >> 24),
	    UCHAR(conn->addr >> 16), UCHAR(conn->addr >> 8), UCHAR(conn->addr));
    return buf;
}
