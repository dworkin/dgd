# define INCLUDE_TELNET
# include "dgd.h"
# include <sgtty.h>
# include "str.h"
# include "array.h"
# include "object.h"
# include "comm.h"

struct _connection_ {
    char dummy;
};

static struct sgttyb tty;			/* current terminal state */
static char buffer[INBUF_SIZE + 1];		/* input buffer */
static int inbuf;				/* # chars in input buffer */
static char state;				/* current output state */

# define TS_DATA	0
# define TS_IAC		1
# define TS_IGNORE	2

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connections
 */
void conn_init(nusers, telnet_port, binary_port)
int nusers;
unsigned short telnet_port, binary_port;
{
    ioctl(1, TIOCGETP, &tty);
    inbuf = -1;
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish()
{
    tty.sg_flags |= ECHO;
    ioctl(1, TIOCSETP, &tty);
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew()
{
    char buffer[100];
    static connection conn;

    if (inbuf < 0 && read(0, buffer, 100) > 0) {
	write(1, "<connected>\n", 12);
	inbuf = 0;
	return &conn;
    }
    return (connection *) NULL;
}

/*
 * NAME:	conn->bnew()
 * DESCRIPTION:	accept a new binary connection
 */
connection *conn_bnew()
{
    return (connection *) NULL;
}

/*
 * NAME:	conn->del()
 * DESCRIPTION:	delete a connection
 */
void conn_del(conn)
connection *conn;
{
    inbuf = -1;
    write(1, "<disconnected>\n", 15);
}

/*
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(wait)
bool wait;
{
    if (inbuf < 0) {
	return 0;
    }
    if (!wait) {
	return (inbuf > 0);
    }
    if ((inbuf=read(0, buffer, INBUF_SIZE)) < 0) {
	inbuf = 0;
	return 0;
    }
    return 1;
}

/*
 * NAME:	conn->read()
 * DESCRIPTION:	read from a connection
 */
int conn_read(conn, buf, size)
connection *conn;
char *buf;
int size;
{
    if (inbuf != 0) {
	if (size > inbuf) {
	    size = inbuf;
	}
	memcpy(buf, buffer, size);
	inbuf -= size;
	if (inbuf != 0) {
	    memcpy(buffer, buffer + size, inbuf);
	}
	return size;
    }
    return 0;
}

/*
 * NAME:	conn->write()
 * DESCRIPTION:	write to a connection
 */
void conn_write(conn, buf, size)
connection *conn;
char *buf;
register int size;
{
    register char *p, *q;
    
    for (p = buf, q = buf; size > 0; p++, --size) {
	switch (state) {
	case TS_DATA:
	    if (UCHAR(*p) == IAC) {
		state = TS_IAC;
		break;
	    }
	    *q++ = *p;
	    break;

	case TS_IAC:
	    if (UCHAR(*p) == IAC) {
		*q++ = *p;
		state = TS_DATA;
	    } else {
		if (UCHAR(*p) == GA) {
		    state = TS_DATA;
		    break;
		} else if (UCHAR(*p) == WONT) {
		    tty.sg_flags |= ECHO;
		} else {
		    tty.sg_flags &= ~ECHO;
		}
		ioctl(1, TIOCSETP, &tty);
		state = TS_IGNORE;
	    }
	    break;

	case TS_IGNORE:
	    state = TS_DATA;
	    break;
	}
    }
    size = q - buf;
    if (size > 0) {
	write(1, buf, size);
    }
}

/*
 * NAME:	conn->ipnum()
 * DESCRIPTION:	return the ip number of a connection
 */
char *conn_ipnum(conn)
connection *conn;
{
    return "127.0.0.1";
}
