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
    object *obj;		/* associated object */
    struct _user_ *prev;	/* preceding user */
    struct _user_ *next;	/* next user */
    unsigned int inbufsz;	/* bytes in input buffer */
    unsigned int outbufsz;	/* bytes in output buffer */
    char flags;			/* connection flags */
    char state;			/* telnet state */
    short newlines;		/* # of newlines in input buffer */
    connection *conn;		/* connection */
    char *inbuf;		/* input buffer */
    char *outbuf;		/* output buffer */
    unsigned int osoffset;	/* offset in output string */
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

static user *users;		/* array of users */
static user *lastuser;		/* last user checked */
static user *freeuser;		/* linked list of free users */
static int maxusers;		/* max # of users */
static int nusers;		/* # of users */
static long newlines;		/* # of newlines in all input buffers */
static object *this_user;	/* current user */
static bool flush;		/* do telnet output buffers need flushing? */

/*
 * NAME:	comm->init()
 * DESCRIPTION:	initialize communications
 */
void comm_init(nusers, telnet_port, binary_port)
int nusers;
unsigned int telnet_port, binary_port;
{
    register int i;
    register user *usr;

    conn_init(nusers, telnet_port, binary_port);
    users = ALLOC(user, maxusers = nusers);
    for (i = nusers, usr = users + i; i > 0; --i) {
	--usr;
	usr->obj = (object *) NULL;
	usr->next = usr + 1;
    }
    users[nusers - 1].next = (user *) NULL;
    freeuser = usr;
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
    register user *usr;

    if (obj->flags & (O_USER | O_EDITOR)) {
	error("user object is already used for user or editor");
    }

    usr = freeuser;
    freeuser = usr->next;
    if (lastuser != (user *) NULL) {
	usr->prev = lastuser->prev;
	usr->prev->next = usr;
	usr->next = lastuser;
	lastuser->prev = usr;
    } else {
	usr->prev = usr;
	usr->next = usr;
	lastuser = usr;
    }

    usr->obj = obj;
    obj->flags |= O_USER;
    obj->etabi = usr - users;
    usr->inbufsz = 0;
    usr->conn = conn;
    if (telnet) {
	/* initialize connection */
	usr->flags = CF_TELNET | CF_ECHO;
	usr->state = TS_DATA;
	usr->newlines = 0;
	m_static();
	usr->inbuf = ALLOC(char, INBUF_SIZE);
	usr->outbuf = ALLOC(char, OUTBUF_SIZE);
	m_dynamic();
	memcpy(usr->outbuf, init, usr->outbufsz = sizeof(init));
	flush = TRUE;
    } else {
	usr->flags = 0;
    }
    nusers++;
}

/*
 * NAME:	comm->del()
 * DESCRIPTION:	delete a connection
 */
static void comm_del(usr, force)
register user *usr;
bool force;
{
    object *obj, *olduser;
    dataspace *data;

    obj = usr->obj;
    conn_del(usr->conn);
    if (usr->flags & CF_TELNET) {
	newlines -= usr->newlines;
	FREE(usr->outbuf);
	FREE(usr->inbuf);
    } else if (obj->flags & O_PENDIO) {
	/* remove old buffer */
	data = o_dataspace(obj);
	d_assign_var(data, d_get_variable(data, data->nvariables - 1),
		     &zero_value);
    }
    obj->flags &= ~(O_USER | O_PENDIO);

    usr->obj = (object *) NULL;
    if (usr->next == usr) {
	lastuser = (user *) NULL;
    } else {
	usr->next->prev = usr->prev;
	usr->prev->next = usr->next;
	if (usr == lastuser) {
	    lastuser = usr->next;
	}
    }
    usr->next = freeuser;
    freeuser = usr;
    --nusers;

    olduser = this_user;
    if (ec_push((ec_ftn) NULL)) {
	if (obj == olduser) {
	    this_user = (object *) NULL;
	} else {
	    this_user = olduser;
	}
	error((char *) NULL);
    } else {
	this_user = obj;
	(--sp)->type = T_INT;
	sp->u.number = force;
	if (i_call(obj, "close", 5, TRUE, 1)) {
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
    register unsigned int len;
    register int size;

    usr = &users[UCHAR(obj->etabi)];
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
		    conn_write(usr->conn, q = usr->outbuf, size, FALSE);
		    size = 0;
		}
		*q++ = (char) IAC;
		size++;
	    } else if (*p == LF) {
		/*
		 * insert CR before LF
		 */
		if (size == OUTBUF_SIZE) {
		    conn_write(usr->conn, q = usr->outbuf, size, FALSE);
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
		conn_write(usr->conn, q = usr->outbuf, size, FALSE);
		size = 0;
	    }
	    *q++ = *p++;
	    --len;
	    size++;
	}
	usr->outbufsz = size;
	flush = TRUE;
	return str->len;	/* always */
    } else {
	dataspace *data;

	/*
	 * binary connection: initially, no buffering
	 */
	if (obj->flags & O_PENDIO) {
	    /* remove old buffer */
	    obj->flags &= ~O_PENDIO;
	    data = o_dataspace(obj);
	    d_assign_var(data, d_get_variable(data, data->nvariables - 1),
			 &zero_value);
	}
	size = conn_write(usr->conn, p, len, TRUE);
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
	    data = o_dataspace(obj);
	    v.type = T_STRING;
	    v.u.string = str;
	    d_assign_var(data, d_get_variable(data, data->nvariables - 1), &v);
	}
	return size;
    }
}

/*
 * NAME:	comm_telnet()
 * DESCRIPTION:	add telnet data to output buffer
 */
static void comm_telnet(usr, buf, size)
register user *usr;
char *buf;
register unsigned int size;
{
    if (usr->outbufsz > OUTBUF_SIZE - size) {
	conn_write(usr->conn, usr->outbuf, usr->outbufsz, FALSE);
	usr->outbufsz = 0;
    }
    memcpy(usr->outbuf + usr->outbufsz, buf, size);
    usr->outbufsz += size;
    flush = TRUE;
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

    usr = &users[UCHAR(obj->etabi)];
    if ((usr->flags & CF_TELNET) && echo != (usr->flags & CF_ECHO)) {
	buf[0] = (char) IAC;
	buf[1] = (echo) ? WONT : WILL;
	buf[2] = TELOPT_ECHO;
	comm_telnet(usr, buf, 3);
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
    register user *usr;
    register int i;
    register unsigned int size;
    register char *p;

    if (!flush) {
	return;
    }
    flush = FALSE;

    for (usr = lastuser, i = nusers; i > 0; usr = usr->next, --i) {
	if ((usr->flags & CF_TELNET) && (size=usr->outbufsz) > 0) {
	    if (prompt && (usr->flags & CF_GA) &&
		(usr->obj == this_user || usr->outbuf[size - 1] != LF)) {
		if (size > OUTBUF_SIZE - 2) {
		    conn_write(usr->conn, usr->outbuf, size, FALSE);
		    size = 0;
		}
		p = usr->outbuf + size;
		*p++ = (char) IAC;
		*p++ = (char) GA;
		size += 2;
	    }
	    conn_write(usr->conn, usr->outbuf, size, FALSE);
	    usr->outbufsz = 0;
	}
    }
}

/*
 * NAME:	comm->receive()
 * DESCRIPTION:	receive a message from a user
 */
void comm_receive()
{
    static char intr[] =	{ '\177' };
    static char brk[] =		{ '\034' };
    static char ayt[] =		{ CR, LF, '[', 'Y', 'e', 's', ']', CR, LF };
    static char tm[] =		{ (char) IAC, (char) WONT, (char) TELOPT_TM };
    static char will_sga[] =	{ (char) IAC, (char) WILL, (char) TELOPT_SGA };
    static char wont_sga[] =	{ (char) IAC, (char) WONT, (char) TELOPT_SGA };
    static char mode_edit[] =	{ (char) IAC, (char) SB,
				  (char) TELOPT_LINEMODE, (char) LM_MODE,
				  (char) MODE_EDIT, (char) IAC, (char) SE };
    char buffer[BINBUF_SIZE];
    connection *conn;
    object *o;
    register user *usr;
    register int n, i, state, flags, nls;
    register char *p, *q;

    n = conn_select(newlines == 0);
    if (n <= 0 && newlines == 0) {
	/*
	 * call_out to do, or timeout
	 */
	return;
    }

    if (ec_push((ec_ftn) NULL)) {
	this_user = (object *) NULL;
	error((char *) NULL);		/* pass on error */
    }

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
	    o = &otable[sp->oindex];
	    sp++;
	    endthread();
	    comm_new(o, conn, TRUE);
	    ec_pop();

	    this_user = o;
	    if (i_call(o, "open", 4, TRUE, 0)) {
		i_del_value(sp++);
		endthread();
		comm_flush(TRUE);
	    }
	    this_user = (object *) NULL;
	}
    }

    while (nusers < maxusers) {
	/*
	 * accept new binary connection
	 */
	conn = conn_bnew();
	if (conn == (connection *) NULL) {
	    break;
	}

	if (ec_push((ec_ftn) NULL)) {
	    conn_del(conn);		/* delete connection */
	    error((char *) NULL);	/* pass on error */
	}
	call_driver_object("binary_connect", 0);
	if (sp->type != T_OBJECT) {
	    fatal("driver->binary_connect() did not return an object");
	}
	o = &otable[sp->oindex];
	sp++;
	endthread();
	comm_new(o, conn, FALSE);
	ec_pop();

	this_user = o;
	if (i_call(o, "open", 4, TRUE, 0)) {
	    i_del_value(sp++);
	    endthread();
	    comm_flush(TRUE);
	}
	this_user = (object *) NULL;
    }

    for (i = nusers; i > 0; --i) {
	usr = lastuser;
	lastuser = usr->next;

	if ((usr->obj->flags & O_PENDIO) && conn_wrdone(usr->conn)) {
	    dataspace *data;
	    value *v;

	    data = o_dataspace(usr->obj);
	    v = d_get_variable(data, data->nvariables - 1);
	    if (usr->outbufsz != 0) {
		/* write next chunk */
		n = conn_write(usr->conn, v->u.string->text + usr->osoffset,
			       usr->outbufsz, TRUE);
		if (n > 0) {
		    usr->osoffset += n;
		    usr->outbufsz -= n;
		}
	    }
	    if (usr->outbufsz == 0) {
		/* remove buffer */
		usr->obj->flags &= ~O_PENDIO;
		d_assign_var(data, v, &zero_value);
		/* callback */
		this_user = usr->obj;
		if (i_call(this_user, "message_done", 12, TRUE, 0)) {
		    i_del_value(sp++);
		    endthread();
		    comm_flush(TRUE);
		}
		this_user = (object *) NULL;
		break;
	    }
	}

	if (usr->flags & CF_TELNET) {
	    /*
	     * telnet connection
	     */
	    if (usr->inbufsz != INBUF_SIZE) {
		p = usr->inbuf + usr->inbufsz;
		n = conn_read(usr->conn, p, INBUF_SIZE - usr->inbufsz);
		if (n < 0) {
		    /*
		     * bad connection
		     */
		    comm_del(usr, FALSE);
		    endthread();	/* this cannot be in comm_del() */
		    comm_flush(FALSE);
		    continue;
		}

		flags = usr->flags;
		state = usr->state;
		nls = usr->newlines;
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
			    if (q != usr->inbuf && q[-1] != LF) {
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
			    comm_telnet(usr, intr, sizeof(intr));
			    state = TS_DATA;
			    break;

			case BREAK:
			    comm_telnet(usr, brk, sizeof(brk));
			    state = TS_DATA;
			    break;

			case AYT:
			    comm_telnet(usr, ayt, sizeof(ayt));
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
			    comm_telnet(usr, tm, sizeof(tm));
			} else if (UCHAR(*p) == TELOPT_SGA) {
			    flags &= ~CF_GA;
			    comm_telnet(usr, will_sga, sizeof(will_sga));
			}
			state = TS_DATA;
			break;

		    case TS_DONT:
			if (UCHAR(*p) == TELOPT_SGA) {
			    flags |= CF_GA;
			    comm_telnet(usr, wont_sga, sizeof(wont_sga));
			}
			state = TS_DATA;
			break;

		    case TS_WILL:
			if (UCHAR(*p) == TELOPT_LINEMODE) {
			    /* linemode confirmed; now request editing */
			    comm_telnet(usr, mode_edit, sizeof(mode_edit));
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
		usr->flags = flags;
		usr->state = state;
		usr->newlines = nls;
		usr->inbufsz = q - usr->inbuf;
		if (nls == 0) {
		    continue;
		}

		/*
		 * input terminated by \n
		 */
		p = (char *) memchr(q = usr->inbuf, LF, usr->inbufsz);
		usr->newlines--;
		--newlines;
		n = p - usr->inbuf;
		p++;			/* skip \n */
		usr->inbufsz -= n + 1;

		(--sp)->type = T_STRING;
		str_ref(sp->u.string = str_new(usr->inbuf, (long) n));
		for (n = usr->inbufsz; n != 0; --n) {
		    *q++ = *p++;
		}
	    } else {
		/*
		 * input buffer full
		 */
		n = usr->inbufsz;
		usr->inbufsz = 0;
		(--sp)->type = T_STRING;
		str_ref(sp->u.string = str_new(usr->inbuf, (long) n));
	    }
	} else {
	    /*
	     * binary connection
	     */
	    n = conn_read(usr->conn, p = buffer, BINBUF_SIZE);
	    if (n <= 0) {
		if (n < 0) {
		    /*
		     * bad connection
		     */
		    comm_del(usr, FALSE);
		    endthread();	/* this cannot be in comm_del() */
		    comm_flush(FALSE);
		}
		continue;
	    }

	    (--sp)->type = T_STRING;
	    str_ref(sp->u.string = str_new(buffer, (long) n));
	}

	this_user = usr->obj;
	if (i_call(usr->obj, "receive_message", 15, TRUE, 1)) {
	    i_del_value(sp++);
	    endthread();
	    comm_flush(TRUE);
	}
	this_user = (object *) NULL;
	break;
    }

    ec_pop();
}

/*
 * NAME:	comm->ip_number()
 * DESCRIPTION:	return the ip number of a user (as a string)
 */
string *comm_ip_number(obj)
object *obj;
{
    char *ipnum;

    ipnum = conn_ipnum(users[UCHAR(obj->etabi)].conn);
    return str_new(ipnum, (long) strlen(ipnum));
}

/*
 * NAME:	comm->close()
 * DESCRIPTION:	remove a user
 */
void comm_close(obj)
object *obj;
{
    register user *usr;

    usr = &users[UCHAR(obj->etabi)];
    if ((usr->flags & CF_TELNET) && usr->outbufsz != 0) {
	/*
	 * flush last bit of output
	 */
	conn_write(usr->conn, usr->outbuf, usr->outbufsz, FALSE);
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
    register user *usr;
    register value *v;

    a = arr_new((long) (i = nusers));
    v = a->elts;
    for (usr = users; i > 0; usr++) {
	if (usr->obj != (object *) NULL) {
	    v->type = T_OBJECT;
	    v->oindex = usr->obj->index;
	    v->u.objcnt = usr->obj->count;
	    v++;
	    --i;
	}
    }
    return a;
}

/*
 * NAME:	comm->active()
 * DESCRIPTION:	return TRUE if there is any pending comm activity
 */
bool comm_active()
{
    return (newlines != 0 || conn_select(0) > 0);
}
