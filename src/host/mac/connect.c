# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "comm.h"
# include <Devices.h>
# include <MacTCP.h>


# define TCPBUFSZ	8192

struct _connection_ {
    connection *prev, *next;		/* previous and next in list */
    short flags;			/* connection flags */
    long addr;				/* internet address of connection */
    int ssize;				/* send size */
    TCPiopb iobuf;			/* I/O parameter buffer */
    struct wdsEntry wds[2];		/* WDS */
    char *recvbuf;			/* receive buffer */
    char *sendbuf;			/* send buffer */
};

# define TCP_OPEN	0x01		/* opened */
# define TCP_CLOSED	0x02		/* closed */
# define TCP_DATA	0x04		/* data to read */
# define TCP_WAIT	0x08		/* waiting for data to be written */

static connection *connections;		/* list of open connections */
static connection *flist;		/* list of free connections */
static connection *telnet;		/* telnet listener */
static connection *binary;		/* binary listener */
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
    /*
     * currently ignored:
     * ULP timeout
     * urgent data
     */
    case TCPDataArrival:
	conn->flags |= TCP_DATA;
	break;

    case TCPTerminate:
    case TCPClosing:
	conn->flags |= TCP_CLOSED;
	break;
    }
}

/*
 * NAME:	conn->new()
 * DESCRIPTION:	create a new TCP stream
 */
static connection *conn_new(void)
{
    connection *conn;

    conn = flist;
    if (conn == NULL) {
	return NULL;
    }

    /* allocate receive buffer */
    m_static();
    conn->recvbuf = ALLOC(char, tcpbufsz);
    m_dynamic();
    conn->sendbuf = NULL;

    /* these values never change */
    conn->iobuf.ioCompletion = NULL;
    conn->iobuf.ioCRefNum = mactcp;

    /* open TCP stream */
    conn->iobuf.csCode = TCPCreate;
    conn->iobuf.csParam.create.rcvBuff = (Ptr) conn->recvbuf;
    conn->iobuf.csParam.create.rcvBuffLen = tcpbufsz;
    conn->iobuf.csParam.create.notifyProc = NewTCPNotifyProc(asr);
    conn->iobuf.csParam.create.userDataPtr = (Ptr) conn;
    if (PBControlSync((ParmBlkPtr) &conn->iobuf) != noErr) {
	/* failed (too many TCP streams?) */
	FREE(conn->recvbuf);
	return NULL;
    }

    /* opened */
    flist = conn->next;
    return conn;
}

/*
 * NAME:	conn->del()
 * DESCRIPTION:	delete a connection
 */
void conn_del(connection *conn)
{
    TCPiopb iobuf;

    iobuf = conn->iobuf;
    iobuf.csCode = TCPRelease;
    PBControlSync((ParmBlkPtr) &iobuf);	/* might cause term notification */

    FREE(conn->recvbuf);
    if (conn->flags & TCP_OPEN) {
	/* has been opened */
	FREE(conn->sendbuf);
	if (conn->prev != NULL) {
	    conn->prev->next = conn->next;
	} else {
	    connections = conn->next;
	}
	if (conn->next != NULL) {
	    conn->next->prev = conn->prev;
	}
    }
    conn->flags = 0;
    conn->next = flist;
    flist = conn;
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	open a TCP stream
 */
static void conn_listen(connection *conn, unsigned int port)
{
    /*
     * listen on a socket
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

    PBControlAsync((ParmBlkPtr) &conn->iobuf);
}

/*
 * NAME:	conn->accept()
 * DESCRIPTION:	accept a new connection on a port
 */
static connection *conn_accept(connection **c, unsigned int portno)
{
    connection *conn;

    conn = NULL;
    if (*c != NULL) {
	/*
	 * check if a connection can be accepted
	 */
	switch ((*c)->iobuf.ioResult) {
	case inProgress:
	    /* continue listening */
	    break;

	case noErr:
	    /* accept connection */
	    conn = *c;
	    *c = NULL;
	    conn->flags |= TCP_OPEN;
	    conn->prev = NULL;
	    conn->next = connections;
	    if (connections != NULL) {
		connections->prev = conn;
	    }
	    connections = conn;

	    /* fill in address and allocate send buffer */
	    conn->addr = conn->iobuf.csParam.open.remoteHost;
	    m_static();
	    conn->sendbuf = ALLOC(char, tcpbufsz);
	    m_dynamic();
	    conn->ssize = 0;
	    conn->wds[0].length = 0;
	    conn->wds[0].ptr = (Ptr) conn->sendbuf;
	    conn->wds[1].length = 0;
	    conn->wds[1].ptr = NULL;
	    break;

	default:
	    /* openFailed, etc */
	    conn_listen(*c, portno);
	    break;
	}
    }

    if (*c == NULL) {
	/*
	 * check if a new connection can be listened for
	 */
	*c = conn_new();
	if (*c != NULL) {
	    conn_listen(*c, portno);
	}
    }

    return conn;
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
    int n;

    /* initialize MacTCP */
    device.ioNamePtr = "\p.IPP";
    device.ioVRefNum = 0;
    device.ioVersNum = 0;
    device.ioPermssn = fsCurPerm;
    device.ioMisc = 0;
    if (PBOpenSync((ParmBlkPtr) &device) != noErr) {
	message("Config error: cannot initialize MacTCP\012");	/* LF */
	exit(1);
    }
    mactcp = device.ioRefNum;
    addr.ioCRefNum = mactcp;
    addr.csCode = ipctlGetAddr;
    if (PBControlSync((ParmBlkPtr) &addr) != noErr) {
	message("Config error: cannot get host address\012");	/* LF */
	exit(1);
    }
    iobuf.ioCRefNum = mactcp;
    iobuf.csCode = UDPMaxMTUSize;
    iobuf.csParam.mtu.remoteHost = addr.ourAddress;
    if (PBControlSync((ParmBlkPtr) &iobuf) != noErr) {
	message("Config error: cannot get MTU size\012");	/* LF */
	exit(1);
    }
    tcpbufsz = (iobuf.csParam.mtu.mtuSize > TCPBUFSZ) ?
		iobuf.csParam.mtu.mtuSize : TCPBUFSZ;
    

    connections = ALLOC(connection, nusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->flags = 0;
	conn->next = flist;
	flist = conn;
    }
    connections = NULL;

    /* prepare for allocating large blocks of static memory */
    d_swapout(1);
    arr_freeall();
    m_purge();

    /* start listening on telnet port */
    telnet = NULL;
    conn_accept(&telnet, telnet_port = t_port);
    if (!m_check()) {
	m_purge();
    }

    /* start listening on binary port */
    binary = NULL;
    conn_accept(&binary, binary_port = b_port);
    if (!m_check()) {
	m_purge();
    }
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish(void)
{
    /* remove all existing connections */
    while (connections != NULL) {
	conn_del(connections);
    }
    if (telnet != NULL) {
	conn_del(telnet);
    }
    if (binary != NULL) {
	conn_del(binary);
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
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(int wait)
{
    long ticks;
    connection *conn;

    ticks = TickCount() + wait * 60;
    do {
	getevent();
	if (telnet != NULL && telnet->iobuf.ioResult != inProgress) {
	    /* new telnet connection */
	    return 1;
	}
	if (binary != NULL && binary->iobuf.ioResult != inProgress) {
	    /* new binary connection */
	    return 1;
	}
	for (conn = connections; conn != NULL; conn = conn->next) {
	    if (conn->flags & (TCP_DATA | TCP_CLOSED)) {
		/* data arrived or connection closed */
		return 1;
	    }
	    if ((conn->flags & TCP_WAIT) && conn->iobuf.ioResult != inProgress)
	    {
		/* data written */
		return 1;
	    }
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

    if (conn->flags & TCP_CLOSED) {
	return -1;	/* terminated */
    }
    if (!(conn->flags & TCP_DATA)) {
	return 0;	/* no data */
    }
    conn->flags &= ~TCP_DATA;

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
 * NAME:	conn->flush()
 * DESCRIPTION:	flush data on a connection
 */
static void conn_flush(connection *conn)
{
    if (conn->flags & TCP_WAIT) {
	if (conn->iobuf.ioResult == inProgress) {
	    return;	/* send still in progress */
	}
	if (conn->iobuf.ioResult != noErr) {
	    /* send failed */
	    conn->flags |= TCP_CLOSED;
	    return;
	}
	conn->flags &= ~TCP_WAIT;
	if (conn->ssize != 0) {
	    /* depends on how memcpy works (use supplied memstr.c) */
	    memcpy(conn->sendbuf, conn->sendbuf + conn->wds[0].length,
		   conn->ssize);
	}
	conn->wds[0].length = 0;
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
	conn->iobuf.csParam.send.wdsPtr = (Ptr) conn->wds;
	if (PBControlAsync((ParmBlkPtr) &conn->iobuf) != noErr) {
	    conn->flags |= TCP_CLOSED;	/* error */
	} else {
	    conn->flags |= TCP_WAIT;
	}
	conn->ssize = 0;
    }
}

/*
 * NAME:	conn->write()
 * DESCRIPTION:	write to a connection; return the amount of bytes written
 */
int conn_write(connection *conn, char *buf, unsigned int len)
{
    int size;

    if (conn->flags & TCP_CLOSED) {
	return 0;
    }
    if (conn->flags & TCP_WAIT) {
	conn_flush(conn);
    }
    if (len == 0) {
	return 0;	/* send_message("") can be used to flush buffer */
    }

    size = tcpbufsz - (conn->wds[0].length + conn->ssize);
    if (size != 0) {
	/* room left in buffer */
	if (size > len) {
	    size = len;
	}
	memcpy(conn->sendbuf + conn->wds[0].length + conn->ssize, buf, size);
	conn->ssize += size;
	conn_flush(conn);
    }
    return size;
}

/*
 * NAME:	conn->wrdone()
 * DESCRIPTION:	return TRUE if a connection is ready for output
 */
bool conn_wrdone(connection *conn)
{
    if (conn->flags & TCP_CLOSED) {
	return TRUE;
    }
    conn_flush(conn);
    return (conn->wds[0].length + conn->ssize != tcpbufsz);
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
