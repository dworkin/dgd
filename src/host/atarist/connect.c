# define INCLUDE_TELNET
# include "dgd.h"
# include <time.h>
# include <osbind.h>
# include "str.h"
# include "array.h"
# include "object.h"
# include "comm.h"

struct _connection_ {
    char dummy;
};

static int inbuf;				/* # chars in input buffer */
static bool echo;				/* input echoing */
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
    inbuf = -1;
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish()
{
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew()
{
    static connection conn;
    extern unsigned long stoptime;

    if (inbuf < 0) {
	if (stoptime == 0) {
	    if (Cconis() < 0) {
		Cnecin();
		Cconws("<connected>\r\n");
		inbuf = 0;
		return &conn;
	    }
	} else {
	    while (!P_timeout()) {
		usleep(40000);
		if (Cconis() < 0) {
		    Cnecin();
		    Cconws("<connected>\r\n");
		    inbuf = 0;
		    return &conn;
		}
	    }
	}
    }
    return (connection *) NULL;
}

/*
 * NAME:	conn->bnew()
 * DESCRIPTION:	(don't) accept a new binary connection
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
    Cconws("<disconnected>\r\n");
}

/*
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(wait)
bool wait;
{
    unsigned long start, time;

    if (inbuf < 0) {
	return 0;
    }
    if (!wait) {
	return (inbuf > 0);
    }
    start = clock();
    while ((time=clock()) < start + CLOCKS_PER_SEC && time >= start &&
	   !P_timeout()) {
	if (Cconis() < 0) {
	    inbuf = 1;
	    return 1;
	}
	usleep(40000);
    }
    return 0;
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
    register int num;

    if (inbuf != 0) {
	for (num = 0; size > 0 && Cconis() < 0; num++, --size) {
	    *buf = Cnecin();
	    if (*buf == '\r') {
		*buf = '\n';
	    }
	    if (echo) {
		if (*buf == '\n') {
		    Cconws("\r\n");
		} else if (*buf == '\b') {
		    Cconws("\b \b");
		} else {
		    Cconout(*buf);
		}
	    }
	    buf++;
	}
	inbuf = 0;
	return num;
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
    char buffer[OUTBUF_SIZE + 1];
    register char *p, *q;
    
    for (p = buf, q = buffer; size > 0; p++, --size) {
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
		    echo = TRUE;
		} else {
		    echo = FALSE;
		}
		state = TS_IGNORE;
	    }
	    break;

	case TS_IGNORE:
	    state = TS_DATA;
	    break;
	}
    }
    *q = '\0';

    if (q != buffer) {
	Cconws(buffer);
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
