# define INCLUDE_TELNET
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "comm.h"

typedef struct _user_ {
    union {
	object *obj;		/* associated object */
	struct _user_ *next;	/* next in free list */
    } u;
    int inbufsz;		/* bytes in input buffer */
    int outbufsz;		/* bytes in output buffer */
    char flags;			/* connection flags */
    char state;			/* telnet state */
    connection *conn;		/* connection */
    char inbuf[INBUF_SIZE];	/* input buffer */
    char outbuf[OUTBUF_SIZE];	/* output buffer */
} user;

# define CF_ECHO	0x01
# define CF_TELNET	0x02
# define CF_GA		0x04
# define CF_SEENCR	0x08

# define TS_DATA	0
# define TS_IAC		1
# define TS_DO		2
# define TS_DONT	3
# define TS_IGNORE	4

static user **users;		/* array of users */
static int maxusers;		/* max # of users */
static int nusers;		/* # of users */
static int newlines;		/* # of newlines in all input buffers */
static long binchars;		/* # characters in binary buffers */
static object *this_user;	/* current user */

/*
 * NAME:	comm->init()
 * DESCRIPTION:	initialize communications
 */
void comm_init(nusers, telnet_port, binary_port)
int nusers, telnet_port, binary_port;
{
    register int i;
    register user **usr;

    conn_init(nusers, telnet_port, binary_port);
    users = ALLOC(user*, maxusers = nusers);
    for (i = nusers, usr = users; i > 0; --i, usr++) {
	*usr = (user *) NULL;
    }
}

/*
 * NAME:	comm->finish()
 * DESCRIPTION:	terminate connections
 */
void comm_finish()
{
    comm_flush(FALSE);
    conn_finish();
}

/*
 * NAME:	comm->new()
 * DESCRIPTION:	accept a new connection
 */
static void comm_new(obj, conn, telnet)
object *obj;
connection *conn;
bool telnet;
{
    static char echo_on[] = { IAC, WONT, TELOPT_ECHO };
    register user **usr;

    if (obj->flags & (O_USER | O_EDITOR)) {
	error("user object is already used for user or editor");
    }
    for (usr = users; *usr != (user *) NULL; usr++) ;
    mstatic();
    *usr = ALLOC(user, 1);
    mdynamic();
    (*usr)->u.obj = obj;
    obj->flags |= O_USER;
    obj->eduser = usr - users;
    (*usr)->inbufsz = 0;
    (*usr)->outbufsz = 0;
    (*usr)->conn = conn;
    if (telnet) {
	/* start with echo on */
	conn_write(conn, echo_on, 3);
	(*usr)->flags = CF_TELNET | CF_ECHO;
	(*usr)->state = TS_DATA;
    } else {
	(*usr)->flags = 0;
    }
    nusers++;
    this_user = obj;
}

/*
 * NAME:	comm->del()
 * DESCRIPTION:	delete a connection
 */
static void comm_del(usr)
register user **usr;
{
    register char *p, *q;
    register int n;
    object *obj, *olduser;

    conn_del((*usr)->conn);
    n = (*usr)->inbufsz;
    if ((*usr)->flags & CF_TELNET) {
	p = (*usr)->inbuf;
	while (n > 0 && (q=(char *) memchr(p, LF, n)) != (char *) NULL) {
	    --newlines;
	    q++;
	    n -= q - p;
	    p = q;
	}
    } else {
	binchars -= n;
    }
    obj = (*usr)->u.obj;
    obj->flags &= ~O_USER;
    FREE(*usr);
    *usr = (user *) NULL;
    --nusers;

    olduser = this_user;
    this_user = obj;
    if (i_call(obj, "close", TRUE, 0)) {
	i_del_value(sp++);
    }
    if (obj == olduser) {
	this_user = (object *) NULL;
    } else {
	this_user = olduser;
    }
}

/*
 * NAME:	comm->send()
 * DESCRIPTION:	send a message to a user
 */
void comm_send(obj, str)
object *obj;
string *str;
{
    register user *usr;
    register char *p, *q;
    register unsigned short len, size;

    usr = users[UCHAR(obj->eduser)];
    p = str->text;
    len = str->len;
    size = usr->outbufsz;
    q = usr->outbuf + size;
    if (usr->flags & CF_TELNET) {
	/*
	 * telnet connection
	 */
	while (len != 0) {
	    if (UCHAR(*p) == IAC) {
		/*
		 * double the telnet IAC character
		 */
		if (size == OUTBUF_SIZE) {
		    conn_write(usr->conn, q = usr->outbuf, size);
		    size = 0;
		}
		*q++ = IAC;
		size++;
	    } else if (*p == LF) {
		/*
		 * insert CR before LF
		 */
		if (size == OUTBUF_SIZE) {
		    conn_write(usr->conn, q = usr->outbuf, size);
		    size = 0;
		}
		*q++ = CR;
		size++;
	    } else if ((*p & 0x7f) < ' ' && *p != HT && *p != BEL && *p != BS) {
		/*
		 * illegal character
		 */
		p++;
		--len;
		continue;
	    }
	    if (size == OUTBUF_SIZE) {
		conn_write(usr->conn, q = usr->outbuf, size);
		size = 0;
	    }
	    *q++ = *p++;
	    --len;
	    size++;
	}
    } else {
	/*
	 * binary connection
	 */
	while (size + len > OUTBUF_SIZE) {
	    memcpy(q, p, OUTBUF_SIZE - size);
	    p += OUTBUF_SIZE - size;
	    len -= OUTBUF_SIZE - size;
	    conn_write(usr->conn, q = usr->outbuf, OUTBUF_SIZE);
	    size = 0;
	}
	memcpy(q, p, len);
	size += len;
    }
    usr->outbufsz = size;
}

/*
 * NAME:	comm->echo()
 * DESCRIPTION:	turn on/off input echoing for a user
 */
void comm_echo(obj, echo)
object *obj;
bool echo;
{
    register user *usr;
    char buf[3];

    usr = users[UCHAR(obj->eduser)];
    if ((usr->flags & CF_TELNET) && echo != (usr->flags & CF_ECHO)) {
	buf[0] = IAC;
	buf[1] = (echo) ? WONT : WILL;
	buf[2] = TELOPT_ECHO;
	conn_write(usr->conn, buf, 3);
	usr->flags ^= CF_ECHO;
    }
}

/*
 * NAME:	comm->flush()
 * DESCRIPTION:	flush output to all users
 */
void comm_flush(prompt)
bool prompt;
{
    register user **usr;
    register int i, size;
    register char *p;

    for (usr = users, i = maxusers; i > 0; usr++, --i) {
	if (*usr != (user *) NULL && (size=(*usr)->outbufsz) > 0) {
	    if (prompt && (*usr)->u.obj == this_user &&
		((*usr)->flags & (CF_TELNET | CF_GA)) == (CF_TELNET | CF_GA)) {
		/*
		 * append "go ahead" to indicate that the prompt has been sent
		 */
		if (size >= OUTBUF_SIZE - 2) {
		    conn_write((*usr)->conn, (*usr)->outbuf, size);
		    size = 0;
		}
		p = (*usr)->outbuf + size;
		*p++ = IAC;
		*p++ = GA;
		size += 2;
	    }
	    conn_write((*usr)->conn, (*usr)->outbuf, size);
	    (*usr)->outbufsz = 0;
	}
    }
    this_user = (object *) NULL;
}

/*
 * NAME:	comm->receive()
 * DESCRIPTION:	receive a message from a user
 */
object *comm_receive(buf, size)
char *buf;
int *size;
{
    static int lastuser;
    connection *conn;
    object *o;
    register int n, i, state, flags;
    register char *p, *q;

    if (nusers < maxusers) {
	/*
	 * accept new telnet connection
	 */
	conn = conn_tnew();
	if (conn != (connection *) NULL) {
	    if (ec_push()) {
		conn_del(conn);		/* delete connection */
		error((char *) NULL);	/* pass on error */
	    }
	    call_driver_object("telnet_connect", 0);
	    ec_pop();
	    if (sp->type != T_OBJECT) {
		fatal("driver->telnet_connect() did not return an object");
	    }
	    comm_new(o = o_object(sp->oindex, sp->u.objcnt), conn, TRUE);
	    sp++;
	    if (i_call(o, "open", TRUE, 0)) {
		i_del_value(sp++);
	    }
	    comm_flush(TRUE);
	}
    }

    if (nusers < maxusers) {
	/*
	 * accept new binary connection
	 */
	conn = conn_bnew();
	if (conn != (connection *) NULL) {
	    if (ec_push()) {
		conn_del(conn);		/* delete connection */
		error((char *) NULL);	/* pass on error */
	    }
	    call_driver_object("binary_connect", 0);
	    ec_pop();
	    if (sp->type != T_OBJECT) {
		fatal("driver->binary_connect() did not return an object");
	    }
	    comm_new(o = o_object(sp->oindex, sp->u.objcnt), conn, FALSE);
	    sp++;
	    if (i_call(o, "open", TRUE, 0)) {
		i_del_value(sp++);
	    }
	    comm_flush(TRUE);
	}
    }

    /*
     * read input from users
     */
    this_user = (object *) NULL;
    n = conn_select(newlines == 0 && binchars == 0);
    if (n <= 0) {
	/*
	 * call_out to do, or timeout
	 */
	if (newlines == 0 && binchars == 0) {
	    return (object *) NULL;
	}
    } else {
	static char intr[] = { '\177' };
	static char brk[] = { '\034' };
	static char tm[] = { IAC, WILL, TELOPT_TM };
	static char will_sga[] = { IAC, WILL, TELOPT_SGA };
	static char wont_sga[] = { IAC, WONT, TELOPT_SGA };
	register user **usr;

	for (i = maxusers, usr = users; i > 0; --i, usr++) {
	    if (*usr != (user *) NULL && (*usr)->inbufsz != INBUF_SIZE) {
		p = (*usr)->inbuf + (*usr)->inbufsz;
		n = conn_read((*usr)->conn, p, INBUF_SIZE - (*usr)->inbufsz);
		if (n < 0) {
		    /*
		     * bad connection
		     */
		    comm_del(usr);
		} else if ((*usr)->flags & CF_TELNET) {
		    /*
		     * telnet mode
		     */
		    flags = (*usr)->flags;
		    state = (*usr)->state;
		    q = p;
		    while (n > 0) {
			switch (state) {
			case TS_DATA:
			    switch (UCHAR(*p)) {
			    case IAC:
				state = TS_IAC;
				break;

			    case BS:
			    case 0x7f:
				if (q != (*usr)->inbuf && q[-1] != LF) {
				    --q;
				}
				flags &= ~CF_SEENCR;
				break;

			    case CR:
				newlines++;
				*q++ = LF;
				flags |= CF_SEENCR;
				break;

			    case LF:
				if ((flags & CF_SEENCR) != 0) {
				    flags &= ~CF_SEENCR;
				    break;
				}
				newlines++;
				/* fall through */
			    default:
				*q++ = *p;
				flags &= ~CF_SEENCR;
				break;
			    }
			    break;

			case TS_IAC:
			    switch (UCHAR(*p)) {
			    case IAC:
				*q++ = *p;
				state = TS_DATA;
				break;

			    case DO:
				state = TS_DO;
				break;

			    case DONT:
				state = TS_DONT;
				break;

			    case WILL:
			    case WONT:
				state = TS_IGNORE;
				break;

			    case IP:
				conn_write((*usr)->conn, intr, 1);
				state = TS_DATA;
				break;

			    case BREAK:
				conn_write((*usr)->conn, brk, 1);
				state = TS_DATA;
				break;

			    default:
				state = TS_DATA;
				break;
			    }
			    break;

			case TS_DO:
			    if (UCHAR(*p) == TELOPT_TM) {
				conn_write((*usr)->conn, tm, 3);
			    } else if (UCHAR(*p) == TELOPT_SGA) {
				flags &= ~CF_GA;
				conn_write((*usr)->conn, will_sga, 3);
			    }
			    state = TS_DATA;
			    break;

			case TS_DONT:
			    if (UCHAR(*p) == TELOPT_SGA) {
				flags |= CF_GA;
				conn_write((*usr)->conn, wont_sga, 3);
			    }
			    /* fall through */
			case TS_IGNORE:
			    state = TS_DATA;
			    break;
			}
			p++;
			--n;
		    }
		    (*usr)->flags = flags;
		    (*usr)->state = state;
		    (*usr)->inbufsz = q - (*usr)->inbuf;
		} else {
		    /*
		     * binary mode
		     */
		    (*usr)->inbufsz += n;
		    binchars += n;
		}
	    }
	}
    }

    if (newlines != 0 || binchars != 0) {
	register user *usr;

	n = lastuser;
	for (;;) {
	    n = (n + 1) % maxusers;
	    usr = users[n];
	    if (usr != (user *) NULL && usr->inbufsz != 0) {
		if (usr->flags & CF_TELNET) {
		    /*
		     * telnet connection
		     */
		    p = (char *) memchr(usr->inbuf, LF, usr->inbufsz);
		    if (p != (char *) NULL) {
			/*
			 * input terminated by \n
			 */
			--newlines;
			lastuser = n;
			*size = n = p - usr->inbuf;
			if (n != 0) {
			    memcpy(buf, usr->inbuf, n);
			}
			p++;	/* skip \n */
			n++;
			usr->inbufsz -= n;
			if (usr->inbufsz != 0) {
			    /* can't rely on memcpy */
			    for (q = usr->inbuf, n = usr->inbufsz; n > 0; --n) {
				*q++ = *p++;
			    }
			}
			return this_user = usr->u.obj;
		    } else if (usr->inbufsz == INBUF_SIZE) {
			/*
			 * input buffer full
			 */
			lastuser = n;
			memcpy(buf, usr->inbuf, *size = INBUF_SIZE);
			usr->inbufsz = 0;
			return this_user = usr->u.obj;
		    }
		} else {
		    /*
		     * binary connection
		     */
		    lastuser = n;
		    binchars -= usr->inbufsz;
		    memcpy(buf, usr->inbuf, *size = usr->inbufsz);
		    usr->inbufsz = 0;
		    return this_user = usr->u.obj;
		}
	    }
	}
    }

    return (object *) NULL;
}

/*
 * NAME:	comm->ip_number()
 * DESCRIPTION:	return the ip number of a user (as a string)
 */
string *comm_ip_number(obj)
object *obj;
{
    char *ipnum;

    ipnum = conn_ipnum(users[UCHAR(obj->eduser)]->conn);
    return str_new(ipnum, (long) strlen(ipnum));
}

/*
 * NAME:	comm->close()
 * DESCRIPTION:	remove a user
 */
void comm_close(obj)
object *obj;
{
    register user **usr;

    usr = &users[UCHAR(obj->eduser)];
    if ((*usr)->outbufsz != 0) {
	/*
	 * flush last bit of output
	 */
	conn_write((*usr)->conn, (*usr)->outbuf, (*usr)->outbufsz);
    }
    comm_del(usr);
}

/*
 * NAME:	comm->user()
 * DESCRIPTION:	return the current user
 */
object *comm_user()
{
    return this_user;
}

/*
 * NAME:	comm->users()
 * DESCRIPTION:	return an array with all user objects
 */
array *comm_users()
{
    array *a;
    register int i;
    register user **usr;
    register value *v;

    a = arr_new((long) nusers);
    v = a->elts;
    for (i = nusers, usr = users; i > 0; usr++) {
	if (*usr != (user *) NULL) {
	    v->type = T_OBJECT;
	    v->oindex = (*usr)->u.obj->index;
	    v->u.objcnt = (*usr)->u.obj->count;
	    v++;
	    --i;
	}
    }
    return a;
}
