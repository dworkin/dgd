# define INCLUDE_TELNET
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "comm.h"

# ifndef TELOPT_LINEMODE
# define TELOPT_LINEMODE	34	/* linemode option */
# define LM_MODE		1
# define MODE_EDIT		0x01
# endif

typedef struct _user_ {
    union {
	object *obj;		/* associated object */
	struct _user_ *next;	/* next in free list */
    } u;
    int inbufsz;		/* bytes in input buffer */
    int outbufsz;		/* bytes in output buffer */
    char flags;			/* connection flags */
    char state;			/* telnet state */
    short newlines;		/* # of newlines in input buffer */
    connection *conn;		/* connection */
    char *inbuf;		/* input buffer */
    char *outbuf;		/* output buffer */
    int osoffset;		/* offset in output string */
} user;

/* flags */
# define CF_ECHO	0x01	/* client echoes input */
# define CF_TELNET	0x02	/* telnet connection */
# define CF_GA		0x04	/* send GA after prompt */
# define CF_SEENCR	0x08	/* just seen a CR */

/* state */
# define TS_DATA	0
# define TS_IAC		1
# define TS_DO		2
# define TS_DONT	3
# define TS_WILL	4
# define TS_WONT	5
# define TS_SB		6
# define TS_SE		7

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
    static char init[] = { (char) IAC, (char) WONT, (char) TELOPT_ECHO,
			   (char) IAC, (char) DO,   (char) TELOPT_LINEMODE };
    register user **usr;

    if (obj->flags & (O_USER | O_EDITOR)) {
	error("user object is already used for user or editor");
    }
    for (usr = users; *usr != (user *) NULL; usr++) ;
    m_static();
    *usr = ALLOC(user, 1);
    m_dynamic();
    (*usr)->u.obj = obj;
    obj->flags |= O_USER;
    obj->etabi = usr - users;
    (*usr)->inbufsz = 0;
    (*usr)->outbufsz = 0;
    (*usr)->conn = conn;
    if (telnet) {
	/* initialize connection */
	conn_write(conn, init, sizeof(init));
	(*usr)->flags = CF_TELNET | CF_ECHO;
	(*usr)->state = TS_DATA;
	(*usr)->newlines = 0;
	m_static();
	(*usr)->inbuf = ALLOC(char, INBUF_SIZE);
	(*usr)->outbuf = ALLOC(char, OUTBUF_SIZE);
	m_dynamic();
    } else {
	(*usr)->flags = 0;
	m_static();
	(*usr)->inbuf = ALLOC(char, BINBUF_SIZE);
	m_dynamic();
    }
    (*usr)->osoffset = -1;
    d_extravar(o_dataspace(obj), TRUE);	/* add space for output buffer */
    nusers++;
    this_user = obj;
}

/*
 * NAME:	comm->del()
 * DESCRIPTION:	delete a connection
 */
static void comm_del(usr, force)
register user **usr;
bool force;
{
    object *obj, *olduser;

    conn_del((*usr)->conn);
    if ((*usr)->flags & CF_TELNET) {
	newlines -= (*usr)->newlines;
	FREE((*usr)->outbuf);
    } else {
	binchars -= (*usr)->inbufsz;
    }
    FREE((*usr)->inbuf);
    obj = (*usr)->u.obj;
    obj->flags &= ~(O_USER | O_PENDIO);
    d_extravar(o_dataspace(obj), FALSE);	/* remove output buffer */
    FREE(*usr);
    *usr = (user *) NULL;
    --nusers;

    olduser = this_user;
    this_user = obj;
    if (ec_push((ec_ftn) NULL)) {
	if (obj == olduser) {
	    this_user = (object *) NULL;
	} else {
	    this_user = olduser;
	}
	error((char *) NULL);
    } else {
	(--sp)->type = T_INT;
	sp->u.number = force;
	if (i_call(obj, "close", TRUE, 1)) {
	    i_del_value(sp++);
	}
	if (obj == olduser) {
	    this_user = (object *) NULL;
	} else {
	    this_user = olduser;
	}
	ec_pop();
    }
}

/*
 * NAME:	comm->send()
 * DESCRIPTION:	send a message to a user
 */
int comm_send(obj, str)
object *obj;
string *str;
{
    register user *usr;
    register char *p, *q;
    register int len, size;

    usr = users[UCHAR(obj->etabi)];
    p = str->text;
    len = str->len;
    if (usr->flags & CF_TELNET) {
	/*
	 * telnet connection
	 */
	size = usr->outbufsz;
	q = usr->outbuf + size;
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
	usr->outbufsz = size;
	return str->len;	/* always */
    } else {
	dataspace *data;

	/*
	 * binary connection: initially, no buffering
	 */
	if (usr->osoffset >= 0) {
	    /* remove old buffer */
	    obj->flags &= ~O_PENDIO;
	    usr->osoffset = -1;
	    data = o_dataspace(obj);
	    d_assign_var(data, d_get_variable(data, data->nvariables - 1),
			 &zero_value);
	}
	size = conn_write(usr->conn, p, len);
	if (size != len) {
	    value v;

	    /*
	     * buffer the remainder of the string
	     */
	    if (size > 0) {
		usr->osoffset = size;
		usr->outbufsz = len - size;
	    } else {
		usr->osoffset = 0;
		usr->outbufsz = len;
	    }
	    obj->flags |= O_PENDIO;
	    v.type = T_STRING;
	    v.u.string = str;
	    data = o_dataspace(obj);
	    d_assign_var(data, d_get_variable(data, data->nvariables - 1), &v);
	}
	return size;
    }
}

/*
 * NAME:	comm->echo()
 * DESCRIPTION:	turn on/off input echoing for a user
 */
void comm_echo(obj, echo)
object *obj;
int echo;
{
    register user *usr;
    char buf[3];

    usr = users[UCHAR(obj->etabi)];
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
int prompt;
{
    register user **usr;
    register int i, size;
    register char *p;

    for (usr = users, i = nusers; i > 0; usr++) {
	if (*usr != (user *) NULL) {
	    --i;
	    if (((*usr)->flags & CF_TELNET) && (size=(*usr)->outbufsz) > 0) {
		if (prompt &&
		    ((*usr)->u.obj == this_user ||
		     (*usr)->outbuf[size - 1] != LF) &&
		    ((*usr)->flags & CF_GA)) {
		    /*
		     * append "go ahead" to indicate that the prompt has been
		     * sent
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
    register int n, i, state, flags, nls;
    register char *p, *q;

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
	static char intr[] =	 { '\177' };
	static char brk[] =	 { '\034' };
	static char ayt[] =	 { CR, LF, '[', 'Y', 'e', 's', ']', CR, LF };
	static char tm[] =	 { (char) IAC, (char) WONT, (char) TELOPT_TM };
	static char will_sga[] = { (char) IAC, (char) WILL, (char) TELOPT_SGA };
	static char wont_sga[] = { (char) IAC, (char) WONT, (char) TELOPT_SGA };
	static char mode_edit[]= { (char) IAC, (char) SB,
				   (char) TELOPT_LINEMODE, (char) LM_MODE,
				   (char) MODE_EDIT, (char) IAC, (char) SE };
	register user **usr;

	if (nusers < maxusers) {
	    /*
	     * accept new telnet connection
	     */
	    conn = conn_tnew();
	    if (conn != (connection *) NULL) {
		if (ec_push((ec_ftn) NULL)) {
		    conn_del(conn);		/* delete connection */
		    error((char *) NULL);	/* pass on error */
		}
		call_driver_object("telnet_connect", 0);
		if (sp->type != T_OBJECT) {
		    fatal("driver->telnet_connect() did not return an object");
		}
		d_export();
		comm_new(o = o_object(sp->oindex, sp->u.objcnt), conn, TRUE);
		sp++;
		ec_pop();
		if (i_call(o, "open", TRUE, 0)) {
		    i_del_value(sp++);
		    d_export();
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
		if (ec_push((ec_ftn) NULL)) {
		    conn_del(conn);		/* delete connection */
		    error((char *) NULL);	/* pass on error */
		}
		call_driver_object("binary_connect", 0);
		if (sp->type != T_OBJECT) {
		    fatal("driver->binary_connect() did not return an object");
		}
		d_export();
		comm_new(o = o_object(sp->oindex, sp->u.objcnt), conn, FALSE);
		sp++;
		ec_pop();
		if (i_call(o, "open", TRUE, 0)) {
		    i_del_value(sp++);
		    d_export();
		}
		comm_flush(TRUE);
	    }
	}

	for (i = nusers, usr = users; i > 0; usr++) {
	    if (*usr == (user *) NULL) {
		continue;	/* avoid even worse indentation */
	    }
	    --i;
	    n = ((*usr)->flags & CF_TELNET) ? INBUF_SIZE : BINBUF_SIZE;
	    if ((*usr)->inbufsz != n) {
		p = (*usr)->inbuf + (*usr)->inbufsz;
		n = conn_read((*usr)->conn, p, n - (*usr)->inbufsz);
		if (n < 0) {
		    /*
		     * bad connection
		     */
		    comm_del(usr, FALSE);
		    d_export();	/* this cannot be in comm_del() */
		    continue;
		} else if ((*usr)->flags & CF_TELNET) {
		    /*
		     * telnet mode
		     */
		    flags = (*usr)->flags;
		    state = (*usr)->state;
		    nls = (*usr)->newlines;
		    q = p;
		    while (n > 0) {
			switch (state) {
			case TS_DATA:
			    switch (UCHAR(*p)) {
			    case '\0':
				flags &= ~CF_SEENCR;
				break;

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
				nls++;
				newlines++;
				*q++ = LF;
				flags |= CF_SEENCR;
				break;

			    case LF:
				if ((flags & CF_SEENCR) != 0) {
				    flags &= ~CF_SEENCR;
				    break;
				}
				nls++;
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
				state = TS_WILL;
				break;

			    case WONT:
				state = TS_WONT;
				break;

			    case SB:
				state = TS_SB;
				break;

			    case IP:
				conn_write((*usr)->conn, intr, 1);
				state = TS_DATA;
				break;

			    case BREAK:
				conn_write((*usr)->conn, brk, 1);
				state = TS_DATA;
				break;

			    case AYT:
				conn_write((*usr)->conn, ayt, 9);
				state = TS_DATA;
				break;

			    default:
				/* let's hope it wasn't important */
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
			    state = TS_DATA;
			    break;

			case TS_WILL:
			    if (UCHAR(*p) == TELOPT_LINEMODE) {
				/* linemode confirmed; now request editing */
				conn_write((*usr)->conn, mode_edit, 7);
			    }
			    /* fall through */
			case TS_WONT:
			    state = TS_DATA;
			    break;

			case TS_SB:
			    /* skip to the end */
			    if (UCHAR(*p) == IAC) {
				state = TS_SE;
			    }
			    break;

			case TS_SE:
			    if (UCHAR(*p) == SE) {
				/* end of subnegotiation */
				state = TS_DATA;
			    } else {
				state = TS_SB;
			    }
			    break;
			}
			p++;
			--n;
		    }
		    (*usr)->flags = flags;
		    (*usr)->state = state;
		    (*usr)->newlines = nls;
		    (*usr)->inbufsz = q - (*usr)->inbuf;
		} else {
		    /*
		     * binary mode
		     */
		    (*usr)->inbufsz += n;
		    binchars += n;
		}
	    }
	    if ((*usr)->osoffset >= 0 && conn_wrdone((*usr)->conn)) {
		dataspace *data;
		value *v;

		if ((*usr)->outbufsz != 0) {
		    /* write next chunk */
		    data = o_dataspace((*usr)->u.obj);
		    v = d_get_variable(data, data->nvariables - 1);
		    n = conn_write((*usr)->conn,
				   v->u.string->text + (*usr)->osoffset,
				   (*usr)->outbufsz);
		    if (n > 0) {
			(*usr)->osoffset += n;
			(*usr)->outbufsz -= n;
		    }
		} else {
		    /* remove buffer */
		    (*usr)->u.obj->flags &= ~O_PENDIO;
		    (*usr)->osoffset = -1;
		    d_assign_var(data, v, &zero_value);
		    /* callback */
		    this_user = (*usr)->u.obj;
		    if (i_call(this_user, "message_done", TRUE, 0)) {
			i_del_value(sp++);
			d_export();
		    }
		    this_user = (object *) NULL;
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
		    if (usr->newlines != 0) {
			/*
			 * input terminated by \n
			 */
			p = (char *) memchr(usr->inbuf, LF, usr->inbufsz);
			usr->newlines--;
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

    ipnum = conn_ipnum(users[UCHAR(obj->etabi)]->conn);
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

    usr = &users[UCHAR(obj->etabi)];
    if (((*usr)->flags & CF_TELNET) && (*usr)->outbufsz != 0) {
	/*
	 * flush last bit of output
	 */
	conn_write((*usr)->conn, (*usr)->outbuf, (*usr)->outbufsz);
    }
    comm_del(usr, TRUE);
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
