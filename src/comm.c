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
    unsigned int osleft;	/* bytes of output string left */
} user;

/* flags */
# define CF_ECHO	0x01	/* client echoes input */
# define CF_TELNET	0x02	/* telnet connection */
# define CF_GA		0x04	/* send GA after prompt */
# define CF_SEENCR	0x08	/* just seen a CR */
# define CF_NOPROMPT	0x10	/* no prompt in telnet output */

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
bool comm_init(n, telnet_port, binary_port)
int n;
unsigned int telnet_port, binary_port;
{
    register int i;
    register user *usr;

    users = ALLOC(user, maxusers = n);
    for (i = n, usr = users + i; i > 0; --i) {
	--usr;
	usr->obj = (object *) NULL;
	usr->next = usr + 1;
    }
    users[n - 1].next = (user *) NULL;
    freeuser = usr;
    lastuser = (user *) NULL;
    nusers = newlines = 0;
    flush = FALSE;

    return conn_init(n, telnet_port, binary_port);
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
 * NAME:	comm->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void comm_listen()
{
    conn_listen();
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
	usr->outbuf = ALLOC(char, OUTBUF_SIZE + TELBUF_SIZE);
	m_dynamic();
	memcpy(usr->outbuf, init, usr->outbufsz = sizeof(init));
	flush = TRUE;
    } else {
	usr->flags = 0;
	usr->outbufsz = 0;
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
    }
    if ((obj->flags & O_PENDIO) && usr->osleft != 0) {
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
 * NAME:	comm->write()
 * DESCRIPTION:	write more data to a socket; return bytes written (binary) or
 *		bytes left in output buffer (telnet)
 */
static int comm_write(usr, str, prompt)
register user *usr;
string *str;
int prompt;
{
    register char *p, *q;
    register unsigned int len;
    register int size, n;
    dataspace *data;

    data = o_dataspace(usr->obj);
    if (str != (string *) NULL) {
	/* new string to write */
	if (usr->obj->flags & O_PENDIO) {
	    if (usr->flags & CF_TELNET) {
		/*
		 * discard new data as long as buffer not flushed, to avoid
		 * half-written telnet escape sequences
		 */
		usr->flags |= CF_NOPROMPT;
		return usr->outbufsz;
	    } else {
		/*
		 * binary connection: discard old data to make room for new
		 */
		usr->obj->flags &= ~O_PENDIO;
		d_assign_var(data, d_get_variable(data, data->nvariables - 1),
			     &zero_value);
	    }
	}
	len = str->len;
	p = str->text;
    } else {
	/* flush */
	len = usr->osleft;
	if (len != 0) {
	    /* fetch pending output string */
	    str = d_get_variable(data, data->nvariables - 1)->u.string;
	    p = str->text + str->len - len;
	}
    }

    if (usr->flags & CF_TELNET) {
	/*
	 * telnet connection
	 */
	size = usr->outbufsz;
	q = usr->outbuf + size;
	while (len != 0) {
	    if (size >= OUTBUF_SIZE - 1) {
		n = conn_write(usr->conn, q = usr->outbuf, size);
		if (n != size) {
		    if (n > 0) {
			size -= n;
			for (p = q + n, n = size; n > 0; --n) {
			    *q++ = *p++;
			}
		    }
		    break;
		}
		size = 0;
	    }
	    if (UCHAR(*p) == IAC) {
		/*
		 * double the telnet IAC character
		 */
		*q++ = (char) IAC;
		size++;
	    } else if (*p == LF) {
		/*
		 * insert CR before LF
		 */
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
	    *q++ = *p++;
	    --len;
	    size++;
	}
	if (prompt && (usr->flags & (CF_GA | CF_NOPROMPT)) == CF_GA &&
	    len == 0 && (usr->obj == this_user || usr->outbuf[size - 1] != LF))
	{
	    /*
	     * If no output has been discarded (which would include the prompt),
	     * add go-ahead, for which there is always space at this point.
	     */
	    *q++ = (char) IAC;
	    *q++ = (char) GA;
	    size += 2;
	    usr->flags |= CF_NOPROMPT;	/* send go-ahead only once */
	}

	if (str == (string *) NULL) {
	    /*
	     * try to flush the buffer
	     */
	    n = conn_write(usr->conn, q = usr->outbuf, size);
	    if (n > 0) {
		size -= n;
		for (p = q + n, n = size; n > 0; --n) {
		    *q++ = *p++;
		}
	    }
	}
	usr->outbufsz = size;
    } else {
	/*
	 * binary connection
	 */
	size = conn_write(usr->conn, p, len);
	if (size > 0) {
	    len -= size;
	}
    }

    if (usr->obj->flags & O_PENDIO) {
	/* old pending output */
	if (len == 0) {
	    if (usr->osleft != 0) {
		/* get rid of output buffer */
		d_assign_var(data, d_get_variable(data, data->nvariables - 1),
			     &zero_value);
	    }
	    if (usr->outbufsz == 0) {
		/* no more pending output */
		usr->obj->flags &= ~O_PENDIO;
		usr->flags &= ~CF_NOPROMPT;
	    }
	}
    } else if (len != 0 || (str == (string *) NULL && usr->outbufsz != 0)) {
	/* leftover output */
	usr->obj->flags |= O_PENDIO;
	if (len != 0 && str != (string *) NULL) {
	    value v;

	    /*
	     * buffer the remainder of the string
	     */
	    v.type = T_STRING;
	    v.u.string = str;
	    d_assign_var(data, d_get_variable(data, data->nvariables - 1), &v);
	}
    } else {
	/* no delayed output at all */
	usr->flags &= ~CF_NOPROMPT;
    }
    usr->osleft = len;

    return size;
}

/*
 * NAME:	comm->send()
 * DESCRIPTION:	send a message to a user
 */
int comm_send(obj, str)
object *obj;
string *str;
{
    user *usr;
    int size;

    usr = &users[UCHAR(obj->etabi)];
    size = comm_write(usr, str, FALSE);
    if (usr->flags & CF_TELNET) {
	flush = TRUE;
	return str->len;
    } else {
	return size;
    }
}

/*
 * NAME:	comm_telnet()
 * DESCRIPTION:	add telnet data to output buffer
 */
static void comm_telnet(usr, buf, len)
register user *usr;
char *buf;
register unsigned int len;
{
    register int size;
    register char *p, *q;

    if (usr->outbufsz > OUTBUF_SIZE + TELBUF_SIZE - 2 - len) {
	/*
	 * attempt to flush the buffer before adding telnet data
	 */
	size = conn_write(usr->conn, usr->outbuf, usr->outbufsz);
	if (size <= 0 ||
	    usr->outbufsz - size > OUTBUF_SIZE + TELBUF_SIZE - 2 - len) {
	    return;	/* failed to make room; abort */
	}

	/* shift buffer to make room for new data */
	usr->outbufsz -= size;
	for (p = usr->outbuf, q = p + size; size > 0; --size) {
	    *p++ = *q++;
	}
    }

    memcpy(usr->outbuf + usr->outbufsz, buf, len);
    usr->outbufsz += len;
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

    if (!flush) {
	return;
    }
    flush = FALSE;

    for (usr = lastuser, i = nusers; i != 0; usr = usr->next, --i) {
	if (usr->outbufsz != 0 && comm_write(usr, (string *) NULL, prompt) != 0)
	{
	    /* couldn't flush everything */
	    flush = TRUE;
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
	    comm_write(usr, (string *) NULL, FALSE);
	    if (!(usr->obj->flags & O_PENDIO) && !(usr->flags & CF_TELNET)) {
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
		    break;
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
		    break;
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
    if (usr->outbufsz != 0) {
	/*
	 * flush last bit of output
	 */
	comm_write(usr, (string *) NULL, FALSE);
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
