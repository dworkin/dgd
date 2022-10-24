/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2017 DGD Authors (see the commit log for details)
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

# define INCLUDE_FILE_IO
# define INCLUDE_TELNET
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "comm.h"
# include "version.h"
# include <errno.h>

# ifndef TELOPT_LINEMODE
# define TELOPT_LINEMODE	34	/* linemode option */
# define LM_MODE		1
# define MODE_EDIT		0x01
# endif

# define MAXIACSEQLEN		7	/* longest IAC sequence sent */

struct user {
    uindex oindex;		/* associated object index */
    user *prev;			/* preceding user */
    user *next;			/* next user */
    user *flush;		/* next in flush list */
    short flags;		/* connection flags */
    char state;			/* telnet state */
    short newlines;		/* # of newlines in input buffer */
    connection *conn;		/* connection */
    char *inbuf;		/* input buffer */
    Array *extra;		/* object's extra value */
    String *outbuf;		/* output buffer string */
    ssizet inbufsz;		/* bytes in input buffer */
    ssizet osdone;		/* bytes of output string done */
};

/* flags */
# define CF_BINARY	0x0000	/* binary connection */
# define  CF_UDP	0x0002	/* receive UDP datagrams */
# define  CF_UDPDATA	0x0004	/* UDP data received */
# define CF_TELNET	0x0001	/* telnet connection */
# define  CF_ECHO	0x0002	/* client echoes input */
# define  CF_GA		0x0004	/* send GA after prompt */
# define  CF_PROMPT	0x0008	/* prompt in telnet output */
# define CF_BLOCKED	0x0010	/* input blocked */
# define CF_FLUSH	0x0020	/* in flush list */
# define CF_OUTPUT	0x0040	/* pending output */
# define CF_ODONE	0x0080	/* output done */
# define CF_OPENDING	0x0100	/* waiting for connect() to complete */

#ifdef NETWORK_EXTENSIONS
# define CF_PORT	0x0200	/* port (listening) connection */
# define CF_DATAGRAM	0x0400	/* independent UDP socket */
#endif

/* state */
# define TS_DATA	0
# define TS_CRDATA	1
# define TS_IAC		2
# define TS_DO		3
# define TS_DONT	4
# define TS_WILL	5
# define TS_WONT	6
# define TS_SB		7
# define TS_SE		8

static user *users;		/* array of users */
static user *lastuser;		/* last user checked */
static user *freeuser;		/* linked list of free users */
static user *flush;		/* flush list */
static user *outbound;		/* pending outbound list */
static int maxusers;		/* max # of users */
#ifdef NETWORK_EXTENSIONS
static int maxports;		/* max # of ports */
static int nports;		/* # of ports */
#else
static int maxdgram;		/* max # of datagram users */
static int ndgram;		/* # of datagram users */
#endif
static int nusers;		/* # of users */
static int odone;		/* # of users with output done */
static long newlines;		/* # of newlines in all input buffers */
static uindex this_user;	/* current user */
static int ntport, nbport;	/* # telnet/binary ports */
static int ndport;		/* # datagram ports */
static int nexttport;		/* next telnet port to check */
static int nextbport;		/* next binary port to check */
static int nextdport;		/* next datagram port to check */
static char ayt[22];		/* are you there? */

/*
 * NAME:	comm->init()
 * DESCRIPTION:	initialize communications
 */
bool comm_init(int n, int p, char **thosts, char **bhosts, char **dhosts,
	unsigned short *tports, unsigned short *bports, unsigned short *dports,
	int ntelnet, int nbinary, int ndatagram)
{
    int i;
    user *usr;

#ifdef NETWORK_EXTENSIONS
    maxusers = n;
    maxports = p;
    n += p;
    nports = 0;
#else
    n += p;
    maxusers = n;
    maxdgram = p;
    ndgram = 0;
#endif
    users = ALLOC(user, n);
    for (i = n, usr = users + i; i > 0; --i) {
	--usr;
	usr->oindex = OBJ_NONE;
	usr->next = usr + 1;
    }
    users[n - 1].next = (user *) NULL;

    freeuser = usr;
    lastuser = (user *) NULL;
    flush = outbound = (user *) NULL;
    nusers = odone = newlines = 0;
    this_user = OBJ_NONE;

    sprintf(ayt, "\15\12[%s]\15\12", VERSION);

    nexttport = nextbport = nextdport = 0;

    return conn_init(n, thosts, bhosts, dhosts, tports, bports, dports,
		     ntport = ntelnet, nbport = nbinary, ndport = ndatagram);
}

#ifdef NETWORK_EXTENSIONS
void comm_openport(Frame *f, Object *obj, unsigned char protocol,
	unsigned short portnr)
{
    connection *conn;
    uindex olduser;
    short flags;
    user *usr;
    Dataspace *data;
    Array *arr;
    Value val;

    flags = 0;

    if (nports >= maxports)
	error("Max number of port objects exceeded");

    switch (protocol)
    {
    case P_TELNET:
	flags = CF_TELNET | CF_PORT;
	protocol = P_TCP;
	break;
    case P_UDP:
	flags = CF_DATAGRAM | CF_PORT;
	break;
    case P_TCP:
	flags = CF_PORT;
	break;
    default:
	error("Unknown protocol");
    }

    conn = (connection *) conn_openlisten(protocol, portnr);
    if (conn == (connection *) NULL)
	error("Can't open port");

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
    d_wipe_extravar(data = o_dataspace(obj));
    switch (protocol)
    {
    case P_TCP:
	arr = arr_new(data, 3L);
	arr->elts[0] = zero_int;
	arr->elts[1] = arr->elts[2] = nil_value;
	PUT_ARRVAL_NOREF(&val, arr);
	d_set_extravar(data, &val);
	break;
    case P_UDP:
	arr = arr_new(data, 4L);
	arr->elts[0] = zero_int;
	arr->elts[1] = nil_value;
	arr->elts[2] = nil_value;
	arr->elts[3] = nil_value;
	PUT_ARRVAL_NOREF(&val, arr);
	d_set_extravar(data, &val);
	break;
    }
    usr->oindex = obj->index;
    obj->flags |= O_USER;
    obj->etabi = usr-users;
    usr->conn = conn;
    usr->outbuf = (String *) NULL;
    usr->osdone = 0;
    usr->flags = flags;
    olduser = this_user;
    this_user = obj->index;
    (--f->sp)->type = T_INT;
    f->sp->u.number = conn_at(conn);
    if (i_call(f, obj, (Array *) NULL, "open", 4, TRUE, 1)) {
	i_del_value(f->sp++);
    }
    nports++;
    this_user = olduser;
}
#endif

/*
 * NAME:	comm->clear()
 * DESCRIPTION:	clean up connections
 */
void comm_clear()
{
    conn_clear();
}

/*
 * NAME:	comm->finish()
 * DESCRIPTION:	terminate connections
 */
void comm_finish()
{
    conn_finish();
}

#ifndef NETWORK_EXTENSIONS
/*
 * NAME:	comm->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void comm_listen()
{
    conn_listen();
}
#endif

/*
 * NAME:	addtoflush()
 * DESCRIPTION:	add a user to the flush list
 */
static void addtoflush(user *usr, Array *arr)
{
    usr->flags |= CF_FLUSH;
    usr->flush = flush;
    flush = usr;
    arr_ref(usr->extra = arr);

    /* remember initial buffer */
    if (d_get_elts(arr)[1].type == T_STRING) {
	str_ref(usr->outbuf = arr->elts[1].u.string);
    }
}

/*
 * NAME:	comm->setup()
 * DESCRIPTION:	setup a user
 */
static Array *comm_setup(user *usr, Frame *f, Object *obj)
{
    Dataspace *data;
    Array *arr;
    Value val;

    if (obj->flags & O_DRIVER) {
	error("Cannot use driver object as user object");
    }

    /* initialize dataspace before the object receives the user role */
    if (!O_HASDATA(obj) &&
	i_call(f, obj, (Array *) NULL, (char *) NULL, 0, TRUE, 0)) {
	i_del_value(f->sp++);
    }

    d_wipe_extravar(data = o_dataspace(obj));
    arr = arr_new(data, 3L);
    arr->elts[0] = zero_int;
    arr->elts[1] = arr->elts[2] = nil_value;
    PUT_ARRVAL_NOREF(&val, arr);
    d_set_extravar(data, &val);

    usr->oindex = obj->index;
    obj->flags |= O_USER;
    obj->etabi = usr - users;
    usr->conn = NULL;
    usr->outbuf = (String *) NULL;
    usr->osdone = 0;
    usr->flags = 0;

    return arr;
}

/*
 * NAME:	comm->new()
 * DESCRIPTION:	accept a new connection
 */
static user *comm_new(Frame *f, Object *obj, connection *conn, int flags)
{
    static char init[] = { (char) IAC, (char) WONT, (char) TELOPT_ECHO,
			   (char) IAC, (char) DO,   (char) TELOPT_LINEMODE };
    user *usr;
    Array *arr;
    Value val;

    if (obj->flags & O_SPECIAL) {
	error("User object is already special purpose");
    }

    if (obj->flags & O_DRIVER) {
	error("Cannot use driver object as user object");
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

    arr = comm_setup(usr, f, obj);
    usr->conn = conn;
    usr->flags = flags;
    if (flags & CF_TELNET) {
	/* initialize connection */
	usr->flags = CF_TELNET | CF_ECHO | CF_OUTPUT;
	usr->state = TS_DATA;
	usr->newlines = 0;
	usr->inbufsz = 0;
	m_static();
	usr->inbuf = ALLOC(char, INBUF_SIZE + 1);
	*usr->inbuf++ = LF;	/* sentinel */
	m_dynamic();

	arr->elts[0].u.number = CF_ECHO;
	PUT_STRVAL_NOREF(&val, str_new(init, (long) sizeof(init)));
	d_assign_elt(obj->data, arr, &arr->elts[1], &val);
    }
    nusers++;

    return usr;
}

/*
 * NAME:	comm->connect()
 * DESCRIPTION:	attempt to establish an outbound connection
 */
void comm_connect(Frame *f, Object *obj, char *addr, unsigned char protocol,
	unsigned short port)
{
    void *host;
    int len;
    user *usr;
    Array *arr;
    Value val;

    if (nusers >= maxusers)
	error("Max number of connection objects exceeded");

    host = conn_host(addr, port, &len);
    if (host == (void *) NULL) {
	error("Unknown address");
    }

    for (usr = outbound; ; usr = usr->flush) {
	if (usr == (user *) NULL) {
	    usr = comm_new(f, obj, (connection *) NULL,
			   (protocol == P_TELNET) ? CF_TELNET : 0);
	    arr = d_get_extravar(obj->data)->u.array;
	    usr->flush = outbound;
	    outbound = usr;
	    break;
	}
	if ((OBJR(usr->oindex)->flags & O_SPECIAL) != O_USER) {
	    /*
	     * a previous outbound connection was undone, reuse it
	     */
	    arr_del(usr->extra);
	    arr = comm_setup(usr, f, obj);
	    break;
	}
    }

    PUT_STRVAL_NOREF(&val, str_new((char *) host, len));
    d_assign_elt(obj->data, arr, &arr->elts[1], &val);
    usr->flags |= CF_FLUSH;
    arr_ref(usr->extra = arr);
    usr->flags |= CF_OPENDING;
}

#ifdef NETWORK_EXTENSIONS
int comm_senddatagram(Object *obj, String *str, String *ip, int port)
{
    user *usr;
    Dataspace *data;
    Array *arr;
    Value *v1, *v2, *v3;
    Value val;

    usr = &users[EINDEX(obj->etabi)];

    if ((usr->flags & (CF_PORT | CF_DATAGRAM)) != (CF_PORT | CF_DATAGRAM)) {
	error("Object is not a udp port object");
    }
    if (!conn_checkaddr(ip->text)) {
	error("Not a IP address");
    }
    arr = d_get_extravar(data = obj->data)->u.array;
    if (!(usr->flags & CF_FLUSH)) {
	addtoflush(usr, arr);
    }

    v1 = arr->elts + 1;/* used for string */
    v2 = arr->elts + 2;/* used for ip */
    v3 = arr->elts + 3;/* used for port */
    usr->flags |= CF_OUTPUT;
    PUT_STRVAL_NOREF(&val, str);
    d_assign_elt(data, arr, v1, &val);
    PUT_STRVAL_NOREF(&val, ip);
    d_assign_elt(data, arr, v2, &val);
    PUT_INTVAL(&val, port);
    d_assign_elt(data, arr, v3, &val);
    return str->len;
}
#endif

/*
 * NAME:	comm->del()
 * DESCRIPTION:	delete a connection
 */
static void comm_del(Frame *f, user *usr, Object *obj, bool destruct)
{
    Dataspace *data;
    uindex olduser;

    data = o_dataspace(obj);
    if (!destruct) {
	/* if not destructing, make sure the connection terminates */
	if (!(usr->flags & CF_FLUSH)) {
	    addtoflush(usr, d_get_extravar(data)->u.array);
	}
	obj->flags &= ~O_USER;
    }
    olduser = this_user;
    try {
	ec_push((ec_ftn) NULL);
	this_user = obj->index;
	PUSH_INTVAL(f, destruct);
	if (i_call(f, obj, (Array *) NULL, "close", 5, TRUE, 1)) {
	    i_del_value(f->sp++);
	}
	this_user = olduser;
	ec_pop();
    } catch (...) {
	this_user = olduser;
	error((char *) NULL);
    }
    if (destruct) {
	/* if destructing, don't disconnect if there's an error in close() */
	if (!(usr->flags & CF_FLUSH)) {
	    addtoflush(usr, d_get_extravar(data)->u.array);
	}
	obj->flags &= ~O_USER;
    }
}

#ifndef NETWORK_EXTENSIONS
/*
 * NAME:	comm->challenge()
 * DESCRIPTION:	set the UDP challenge for a binary connection
 */
void comm_challenge(Object *obj, String *str)
{
    user *usr;
    Dataspace *data;
    Array *arr;
    Value *v;
    Value val;

    usr = &users[obj->etabi];
    if (usr->flags & CF_TELNET || !conn_attach(usr->conn)) {
	error("Datagram channel not available");
    }
    if (usr->flags & CF_UDPDATA) {
	error("Datagram channel already established");
    }
    arr = d_get_extravar(data = obj->data)->u.array;
    if (!(usr->flags & CF_FLUSH)) {
	addtoflush(usr, arr);
    }

    v = arr->elts + 2;
    if ((usr->flags & CF_UDP) || v->type == T_STRING) {
	error("Datagram challenge already set");
    }
    usr->flags |= CF_OUTPUT;
    PUT_STRVAL_NOREF(&val, str);
    d_assign_elt(data, arr, v, &val);
}
#endif

/*
 * NAME:	comm->write()
 * DESCRIPTION:	add bytes to output buffer
 */
static int comm_write(user *usr, Object *obj, String *str, char *text,
	unsigned int len)
{
    Dataspace *data;
    Array *arr;
    Value *v;
    ssizet osdone, olen;
    Value val;

    arr = d_get_extravar(data = o_dataspace(obj))->u.array;
    if (!(usr->flags & CF_FLUSH)) {
	addtoflush(usr, arr);
    }

    v = arr->elts + 1;
    if (v->type == T_STRING) {
	/* append to existing buffer */
	osdone = (usr->outbuf == v->u.string) ? usr->osdone : 0;
	olen = v->u.string->len - osdone;
	if (olen + len > MAX_STRLEN) {
	    len = MAX_STRLEN - olen;
	    if (len == 0 ||
		((usr->flags & CF_TELNET) && text[0] == (char) IAC &&
		 len < MAXIACSEQLEN)) {
		return 0;
	    }
	}
	str = str_new((char *) NULL, (long) olen + len);
	memcpy(str->text, v->u.string->text + osdone, olen);
	memcpy(str->text + olen, text, len);
    } else {
	/* create new buffer */
	if (usr->flags & CF_ODONE) {
	    usr->flags &= ~CF_ODONE;
	    --odone;
	}
	usr->flags |= CF_OUTPUT;
	if (str == (String *) NULL) {
	    str = str_new(text, (long) len);
	}
    }

    PUT_STRVAL_NOREF(&val, str);
    d_assign_elt(data, arr, v, &val);
    return len;
}

/*
 * NAME:	comm->send()
 * DESCRIPTION:	send a message to a user
 */
int comm_send(Object *obj, String *str)
{
    user *usr;

    usr = &users[EINDEX(obj->etabi)];
    if (usr->flags & CF_TELNET) {
	char outbuf[OUTBUF_SIZE];
	char *p, *q;
	unsigned int len, size, n;

	/*
	 * telnet connection
	 */
	p = str->text;
	len = str->len;
	q = outbuf;
	size = 0;
	for (;;) {
	    if (len == 0 || size >= OUTBUF_SIZE - 1 || UCHAR(*p) == IAC) {
		n = comm_write(usr, obj, (String *) NULL, outbuf, size);
		if (n != size) {
		    /*
		     * count how many bytes of original string were written
		     */
		    for (n = size - n; n != 0; --n) {
			if (*--p == *--q) {
			    len++;
			    if (UCHAR(*p) == IAC) {
				break;
			    }
			} else if (*q == CR) {
			    /* inserted CR */
			    p++;
			} else {
			    /* skipped char */
			    q++;
			    len++;
			}
		    }
		    return str->len - len;
		}
		if (len == 0) {
		    return str->len;
		}
		size = 0;
		q = outbuf;
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
	    }
	    *q++ = *p++;
	    --len;
	    size++;
	}
    } else {
	if ((usr->flags & (CF_UDP | CF_UDPDATA)) == CF_UDPDATA) {
	    error("Message channel not enabled");
	}

	/*
	 * binary connection
	 */
	return comm_write(usr, obj, str, str->text, str->len);
    }
}

/*
 * NAME:	comm->udpsend()
 * DESCRIPTION:	send a message on the UDP channel of a binary connection
 */
int comm_udpsend(Object *obj, String *str)
{
    user *usr;
    Dataspace *data;
    Array *arr;
    Value *v;
    Value val;

    usr = &users[EINDEX(obj->etabi)];
    if ((usr->flags & (CF_TELNET | CF_UDPDATA)) != CF_UDPDATA) {
	error("Datagram channel not established");
    }

    arr = d_get_extravar(data = obj->data)->u.array;
    if (!(usr->flags & CF_FLUSH)) {
	addtoflush(usr, arr);
    }

    v = arr->elts + 2;
    if (v->type == T_STRING) {
	return 0;	/* datagram queued already */
    }
    usr->flags |= CF_OUTPUT;
    PUT_STRVAL_NOREF(&val, str);
    d_assign_elt(data, arr, v, &val);

    return str->len;
}

/*
 * NAME:	comm->echo()
 * DESCRIPTION:	turn on/off input echoing for a user
 */
bool comm_echo(Object *obj, int echo)
{
    user *usr;
    Dataspace *data;
    Array *arr;
    Value *v;

    usr = &users[EINDEX(obj->etabi)];
    if (usr->flags & CF_TELNET) {
	arr = d_get_extravar(data = obj->data)->u.array;
	v = d_get_elts(arr);
	if (echo != (v->u.number & CF_ECHO) >> 1) {
	    Value val;

	    if (!(usr->flags & CF_FLUSH)) {
		addtoflush(usr, arr);
	    }
	    val = *v;
	    val.u.number ^= CF_ECHO;
	    d_assign_elt(data, arr, v, &val);
	}
	return TRUE;
    }
    return FALSE;
}

/*
 * NAME:	comm->block()
 * DESCRIPTION:	suspend or release input from a user
 */
void comm_block(Object *obj, int block)
{
    user *usr;
    Dataspace *data;
    Array *arr;
    Value *v;

    usr = &users[EINDEX(obj->etabi)];
    arr = d_get_extravar(data = obj->data)->u.array;
    v = d_get_elts(arr);
    if (block != (v->u.number & CF_BLOCKED) >> 4) {
	Value val;

	if (!(usr->flags & CF_FLUSH)) {
	    addtoflush(usr, arr);
	}
	val = *v;
	val.u.number ^= CF_BLOCKED;
	d_assign_elt(data, arr, v, &val);
    }
}

#ifdef NETWORK_EXTENSIONS
static void comm_udpflush(user *usr, Object *obj, Dataspace *data, Array *arr)
{
    Value *v;
    char *buf;
    int res;

    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(obj);

    v = d_get_elts(arr);
    if (!conn_wrdone(usr->conn)) {
	return;
    }
    buf = v[1].u.string->text;
    res = conn_udpsend(usr->conn, buf, strlen(buf), v[2].u.string->text,
	(unsigned short) v[3].u.number);
    if (res == -1) {
	 /* EAGAIN occured, datagram could not be sent */
    }
    usr->flags &= ~CF_OUTPUT;
    usr->flags |= CF_ODONE;
    odone++;
}
#endif

/*
 * NAME:	comm->uflush()
 * DESCRIPTION:	flush output buffers for a single user only
 */
static void comm_uflush(user *usr, Object *obj, Dataspace *data, Array *arr)
{
    Value *v;
    int n;

    UNREFERENCED_PARAMETER(obj);

    v = d_get_elts(arr);

    if (v[1].type == T_STRING) {
	if (conn_wrdone(usr->conn)) {
	    n = conn_write(usr->conn, v[1].u.string->text + usr->osdone,
			   v[1].u.string->len - usr->osdone);
	    if (n >= 0) {
		n += usr->osdone;
		if (n == v[1].u.string->len) {
		    /* buffer fully drained */
		    n = 0;
		    usr->flags &= ~CF_OUTPUT;
		    usr->flags |= CF_ODONE;
		    odone++;
		    d_assign_elt(data, arr, &v[1], &nil_value);
		}
		usr->osdone = n;
	    } else {
		/* wait for conn_read() to discover the problem */
		usr->flags &= ~CF_OUTPUT;
	    }
	}
    } else {
	/* just a datagram */
	usr->flags &= ~CF_OUTPUT;
    }

    if (v[2].type == T_STRING) {
	if (usr->flags & CF_UDPDATA) {
	    conn_udpwrite(usr->conn, v[2].u.string->text, v[2].u.string->len);
#ifndef NETWORK_EXTENSIONS
	} else if (conn_udp(usr->conn, v[2].u.string->text, v[2].u.string->len))
	{
	    usr->flags |= CF_UDP;
#endif
	}
	d_assign_elt(data, arr, &v[2], &nil_value);
    }
}

/*
 * NAME:	comm->flush()
 * DESCRIPTION:	flush state, output and connections
 */
void comm_flush()
{
    user *usr;
    Object *obj;
    Array *arr;
    Value *v;

    while (outbound != (user *) NULL) {
	usr = outbound;
	outbound = usr->flush;

	arr = usr->extra;
	obj = OBJ(usr->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER) {
	    /* connect */
	    usr->conn = conn_connect(arr->elts[1].u.string->text,
				     arr->elts[1].u.string->len);
	    if (usr->conn == (connection *) NULL) {
		fatal("can't connect to server");
	    }

	    d_assign_elt(obj->data, arr, &arr->elts[1], &nil_value);
	    arr_del(arr);
	    usr->flags &= ~CF_FLUSH;
	} else if (obj->count != 0) {
	    /* clean up */
	    usr->flush = flush;
	    flush = usr;
	} else {
	    /* discard */
	    usr->oindex = OBJ_NONE;
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
	}
    }

    while (flush != (user *) NULL) {
	usr = flush;
	flush = usr->flush;

	/*
	 * status change
	 */
	obj = OBJ(usr->oindex);
	arr = usr->extra;
	v = arr->elts;
	if (usr->flags & CF_TELNET) {
	    if ((v->u.number ^ usr->flags) & CF_ECHO) {
		char buf[3];

		/* change echo */
		buf[0] = (char) IAC;
		buf[1] = (v->u.number & CF_ECHO) ? (char) WONT : (char) WILL;
		buf[2] = TELOPT_ECHO;
		if (comm_write(usr, obj, (String *) NULL, buf, 3) != 0) {
		    usr->flags ^= CF_ECHO;
		}
	    }
	    if (usr->flags & CF_PROMPT) {
		usr->flags &= ~CF_PROMPT;
		if ((usr->flags & CF_GA) && v[1].type == T_STRING &&
		    usr->outbuf != v[1].u.string) {
		    static char ga[] = { (char) IAC, (char) GA };

		    /* append go-ahead */
		    comm_write(usr, obj, (String *) NULL, ga, 2);
		}
	    }
	}
	if ((v->u.number ^ usr->flags) & CF_BLOCKED) {
	    usr->flags ^= CF_BLOCKED;
	    conn_block(usr->conn, ((usr->flags & CF_BLOCKED) != 0));
	}

	/*
	 * write
	 */
	if (usr->outbuf != (String *) NULL) {
	    if (usr->outbuf != v[1].u.string) {
		usr->osdone = 0;	/* new mesg before buffer drained */
	    }
	    str_del(usr->outbuf);
	    usr->outbuf = (String *) NULL;
	}
	if (usr->flags & CF_OUTPUT) {
#ifdef NETWORK_EXTENSIONS
	    if ((usr->flags & CF_DATAGRAM)) {
		comm_udpflush(usr, obj, obj->data, arr);
	    } else
#endif
	    comm_uflush(usr, obj, obj->data, arr);
	}
	/*
	 * disconnect
	 */
	if ((obj->flags & O_SPECIAL) != O_USER) {
	    d_wipe_extravar(obj->data);
	    if (usr->conn != (connection *) NULL) {
		conn_del(usr->conn);
	    }
#ifdef NETWORK_EXTENSIONS
	    if ((usr->flags & (CF_TELNET | CF_PORT)) == CF_TELNET) {
#else
	    if (usr->flags & CF_TELNET) {
#endif
		newlines -= usr->newlines;
		FREE(usr->inbuf - 1);
	    }
	    if (usr->flags & CF_ODONE) {
		--odone;
	    }

	    usr->oindex = OBJ_NONE;
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
#ifdef NETWORK_EXTENSIONS
	    if (usr->flags & CF_PORT) {
		--nports;
	    } else {
		--nusers;
	    }
#else
	    if ((usr->flags & (CF_TELNET | CF_UDP | CF_UDPDATA)) == CF_UDPDATA)
	    {
		--ndgram;
	    }
	    --nusers;
#endif
	}

	arr_del(arr);
	usr->flags &= ~CF_FLUSH;
    }
}

/*
 * NAME:	comm->taccept()
 * DESCRIPTION:	accept a telnet connection
 */
static void comm_taccept(Frame *f, connection *conn, int port)
{
    user *usr;
    Object *obj;

    try {
	ec_push((ec_ftn) NULL);
	PUSH_INTVAL(f, port);
	call_driver_object(f, "telnet_connect", 1);
	if (f->sp->type != T_OBJECT) {
	    fatal("driver->telnet_connect() did not return persistent object");
	}
	obj = OBJ(f->sp->oindex);
	f->sp++;
	usr = comm_new(f, obj, conn, CF_TELNET);
	ec_pop();
    } catch (...) {
	conn_del(conn);		/* delete connection */
	error((char *) NULL);	/* pass on error */
    }

    usr->flags |= CF_PROMPT;
    addtoflush(usr, d_get_extravar(o_dataspace(obj))->u.array);
    this_user = obj->index;
    if (i_call(f, obj, (Array *) NULL, "open", 4, TRUE, 0)) {
	i_del_value(f->sp++);
    }
    endtask();
    this_user = OBJ_NONE;
}

/*
 * NAME:	comm->baccept()
 * DESCRIPTION:	accept a binary connection
 */
static void comm_baccept(Frame *f, connection *conn, int port)
{
    Object *obj;

    try {
	ec_push((ec_ftn) NULL);
	PUSH_INTVAL(f, port);
	call_driver_object(f, "binary_connect", 1);
	if (f->sp->type != T_OBJECT) {
	    fatal("driver->binary_connect() did not return persistent object");
	}
	obj = OBJ(f->sp->oindex);
	f->sp++;
	comm_new(f, obj, conn, 0);
	ec_pop();
    } catch (...) {
	conn_del(conn);		/* delete connection */
	error((char *) NULL);	/* pass on error */
    }

    this_user = obj->index;
    if (i_call(f, obj, (Array *) NULL, "open", 4, TRUE, 0)) {
	i_del_value(f->sp++);
    }
    endtask();
    this_user = OBJ_NONE;
}

#ifndef NETWORK_EXTENSIONS
/*
 * NAME:	comm->daccept()
 * DESCRIPTION:	accept a datagram connection
 */
static void comm_daccept(Frame *f, connection *conn, int port)
{
    Object *obj;

    try {
	ec_push((ec_ftn) NULL);
	PUSH_INTVAL(f, port);
	call_driver_object(f, "datagram_connect", 1);
	if (f->sp->type != T_OBJECT) {
	    fatal("driver->datagram_connect() did not return persistent object");
	}
	obj = OBJ(f->sp->oindex);
	f->sp++;
	comm_new(f, obj, conn, CF_UDPDATA);
	ndgram++;
	ec_pop();
    } catch (...) {
	conn_del(conn);		/* delete connection */
	error((char *) NULL);	/* pass on error */
    }

    this_user = obj->index;
    if (i_call(f, obj, (Array *) NULL, "open", 4, TRUE, 0)) {
	i_del_value(f->sp++);
    }
    endtask();
    this_user = OBJ_NONE;
}
#endif

/*
 * NAME:	comm->receive()
 * DESCRIPTION:	receive a message from a user
 */
void comm_receive(Frame *f, Uint timeout, unsigned int mtime)
{
    static char intr[] =	{ '\177' };
    static char brk[] =		{ '\034' };
    static char tm[] =		{ (char) IAC, (char) WONT, (char) TELOPT_TM };
    static char will_sga[] =	{ (char) IAC, (char) WILL, (char) TELOPT_SGA };
    static char wont_sga[] =	{ (char) IAC, (char) WONT, (char) TELOPT_SGA };
    static char mode_edit[] =	{ (char) IAC, (char) SB,
				  (char) TELOPT_LINEMODE, (char) LM_MODE,
				  (char) MODE_EDIT, (char) IAC, (char) SE };
    char buffer[BINBUF_SIZE];
    Object *obj;
    user *usr;
    int n, i, state, nls;
    char *p, *q;
#ifndef NETWORK_EXTENSIONS
    connection *conn;
#endif

    if (newlines != 0 || odone != 0) {
	timeout = mtime = 0;
    }
    n = conn_select(timeout, mtime);
    if ((n <= 0) && (newlines == 0) && (odone == 0)) {
	/*
	 * call_out to do, or timeout
	 */
	return;
    }

    try {
	ec_push(errhandler);
#ifndef NETWORK_EXTENSIONS
	if (ntport != 0 && nusers < maxusers) {
	    n = nexttport;
	    do {
		/*
		 * accept new telnet connection
		 */
		conn = conn_tnew6(n);
		if (conn != (connection *) NULL) {
		    comm_taccept(f, conn, n);
		    nexttport = (n + 1) % ntport;
		}
		if (nusers < maxusers) {
		    conn = conn_tnew(n);
		    if (conn != (connection *) NULL) {
			comm_taccept(f, conn, n);
			nexttport = (n + 1) % ntport;
		    }
		}

		n = (n + 1) % ntport;
	    } while (n != nexttport);
	}

	if (nbport != 0 && nusers < maxusers) {
	    n = nextbport;
	    do {
		/*
		 * accept new binary connection
		 */
		conn = conn_bnew6(n);
		if (conn != (connection *) NULL) {
		    comm_baccept(f, conn, n);
		}
		if (nusers < maxusers) {
		    conn = conn_bnew(n);
		    if (conn != (connection *) NULL) {
			comm_baccept(f, conn, n);
		    }
		}
		n = (n + 1) % nbport;
		if (nusers == maxusers) {
		    nextbport = n;
		    break;
		}
	    } while (n != nextbport);
	}

	if (ndport != 0 && ndgram < maxdgram) {
	    n = nextdport;
	    do {
		/*
		 * accept new datagram connection
		 */
		conn = conn_dnew6(n);
		if (conn != (connection *) NULL) {
		    comm_daccept(f, conn, n);
		}
		if (ndgram < maxdgram) {
		    conn = conn_dnew(n);
		    if (conn != (connection *) NULL) {
			comm_daccept(f, conn, n);
		    }
		}
		n = (n + 1) % ndport;
		if (ndgram == maxdgram) {
		    nextdport = n;
		    break;
		}
	    } while (n != nextdport);
	}

	for (i = nusers; lastuser != (user *) NULL && i > 0; --i) {
#else
	for (i = nusers + nports; lastuser != (user *) NULL && i > 0; --i) {
#endif
	    usr = lastuser;
	    lastuser = usr->next;

	    obj = OBJ(usr->oindex);


	    /*
	     * Check if we have an event pending from connect() and if so,
	     * handle it.
	     */
	    if (usr->flags & CF_OPENDING) {
		int retval, errcode;
		uindex old_user;
		retval = conn_check_connected(usr->conn, &errcode);
		/*
		 * Something happened to the connection..
		 * its either connected or in error state now.
		 */
		if (retval != 0) {
		    usr->flags &= ~CF_OPENDING;
		    if (!(usr->flags & CF_FLUSH)) {
			addtoflush(usr,
				   d_get_extravar(o_dataspace(obj))->u.array);
		    }
		    old_user = this_user;
		    this_user = obj->index;
		    /*
		     * Error, report it to the user object.
		     */
		    if (retval < 0) {
			obj->flags &= ~O_USER;
#ifdef NETWORK_EXTENSIONS
			if (retval == -1) {
			    PUSH_STRVAL(f, str_new(strerror(errno),
						   strlen(strerror(errno))));
			} else {
			    PUSH_STRVAL(f, str_new("socket unexpectedly closed",
						   26));
			}

			if (i_call(f, obj, (Array *) NULL, "receive_error", 13,
				   TRUE, 1)) {
			    i_del_value(f->sp++);
			}
#else
			PUSH_INTVAL(f, errcode);
			if (i_call(f, obj, (Array *) NULL, "unconnected", 11,
				   TRUE, 1)) {
			    i_del_value(f->sp++);
			}
#endif
			endtask();
		    /*
		     * Connection completed, call open in the user object.
		     */
		    } else if (retval > 0) {
			if (i_call(f, obj, (Array *) NULL, "open", 4, TRUE, 0))
			{
			    i_del_value(f->sp++);
			}
			endtask();
		    }
		    this_user = old_user;
		}
		/*
		 * Don't do anything else for user objects with pending
		 * connects.
		 */
		continue;
	    }
	    if (usr->flags & CF_OUTPUT) {
		Dataspace *data;

		data = o_dataspace(obj);
		comm_uflush(usr, obj, data, d_get_extravar(data)->u.array);
	    }
	    if (usr->flags & CF_ODONE) {
		/* callback */
		usr->flags &= ~CF_ODONE;
		--odone;
		this_user = obj->index;
#ifdef NETWORK_EXTENSIONS
		/*
		 * message_done for tcp, datagram_done for udp
		 */
		if (usr->flags & CF_DATAGRAM) {
		    if (i_call(f, obj, (Array *) NULL, "datagram_done", 13,
			       TRUE, 0)) {
			i_del_value(f->sp++);
			endtask();
		    }
		} else
#endif
		if (i_call(f, obj, (Array *) NULL, "message_done", 12, TRUE, 0))
		{
		    i_del_value(f->sp++);
		    endtask();
		}

		this_user = OBJ_NONE;
		if (obj->count == 0) {
		    break;	/* continue, unless the connection was closed */
		}
	    }

	    if (usr->flags & CF_BLOCKED) {
		continue;	/* no input on this connection */
	    }

#ifdef NETWORK_EXTENSIONS
	    if ((usr->flags & (CF_DATAGRAM | CF_PORT)) ==
						    (CF_PORT | CF_DATAGRAM)) {
		char *addr;
		int port;
		n = conn_udpreceive(usr->conn, buffer, BINBUF_SIZE, &addr,
				    &port);
		if (n >= 0) {
		    PUSH_STRVAL(f, str_new(buffer, (long) n));
		    PUSH_STRVAL(f, str_new(addr, strlen(addr)));
		    PUSH_INTVAL(f, port);
		    if (i_call(f, obj, (Array *) NULL, "receive_datagram", 16,
			       TRUE, 3)) {
			i_del_value(f->sp++);
			endtask();
		    }
		}
		continue;
	    }

	    if ((usr->flags & (CF_PORT | CF_DATAGRAM)) == CF_PORT) {
		if (nusers <= maxusers) {
		    connection *conn;
		    char ip[40];
		    user *newusr;
		    uindex olduser;

		    conn = (connection *) conn_accept(usr->conn);

		    if (conn == (connection *) NULL) {
			break;
		    }
		    if (nusers == maxusers) {
			conn_del(conn);
			error("Maximum number of users exceeded");
		    }

		    try {
			ec_push((ec_ftn) NULL);
			(--f->sp)->type = T_STRING;
			conn_ipnum(conn,ip);
			PUT_STRVAL(f->sp, str_new(ip, strlen(ip)));
			(--f->sp)->type = T_INT;
			f->sp->u.number = conn_at(conn);
			if (!i_call(f, OBJ(usr->oindex), (Array *) NULL,
				    "connection", 10, TRUE, 2)) {
			    conn_del(conn);
			    error("Missing connection()-function");
			}
			if (f->sp->type != T_OBJECT) {
			    conn_del(conn);
			    error("object->connection() did not return a persistent object");
			}
			obj = OBJ(f->sp->oindex);
			f->sp++;
			newusr = comm_new(f,obj, conn, usr->flags & CF_TELNET);
			ec_pop();
		    } catch (...) {
			conn_del(conn);
			error((char *) NULL);
		    }
		    endtask();

		    newusr->flags |= CF_PROMPT;
		    addtoflush(newusr,
			       d_get_extravar(o_dataspace(obj))->u.array);
		    olduser = this_user;
		    this_user = obj->index;
		    if (i_call(f, obj, (Array *) NULL, "open", 4, TRUE, 0)) {
			i_del_value(f->sp++);
			endtask();
		    }
		    this_user=olduser;
		}
		continue;

	    }

	    if ((obj->flags & O_USER) != O_USER) {
		continue;
	    }

#endif
	    if (usr->flags & CF_TELNET) {
		/*
		 * telnet connection
		 */
		if (usr->inbufsz != INBUF_SIZE) {
		    p = usr->inbuf + usr->inbufsz;
		    n = conn_read(usr->conn, p, INBUF_SIZE - usr->inbufsz);
		    if (n < 0) {
			if (usr->inbufsz != 0) {
			    if (p[-1] != LF) {
				/*
				 * add a newline at the end
				 */
				*p = LF;
				n = 1;
			    }
			} else if (!(usr->flags & CF_OUTPUT)) {
			    /*
			     * empty buffer, no more input, no pending output
			     */
			    comm_del(f, usr, obj, FALSE);
			    endtask();    /* this cannot be in comm_del() */
			    break;
			}
		    }

		    state = usr->state;
		    nls = usr->newlines;
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
				if (q[-1] != LF) {
				    --q;
				}
				break;

			    case CR:
				nls++;
				newlines++;
				*q++ = LF;
				state = TS_CRDATA;
				break;

			    case LF:
				nls++;
				newlines++;
				/* fall through */
			    default:
				*q++ = *p;
				/* fall through */
			    case '\0':
				break;
			    }
			    break;

			case TS_CRDATA:
			    switch (UCHAR(*p)) {
			    case IAC:
				state = TS_IAC;
				break;

			    case CR:
				nls++;
				newlines++;
				*q++ = LF;
				break;

			    default:
				*q++ = *p;
				/* fall through */
			    case '\0':
			    case LF:
			    case BS:
			    case 0x7f:
				state = TS_DATA;
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
				comm_write(usr, obj, (String *) NULL, intr,
					   sizeof(intr));
				state = TS_DATA;
				break;

			    case BREAK:
				comm_write(usr, obj, (String *) NULL, brk,
					   sizeof(brk));
				state = TS_DATA;
				break;

			    case AYT:
				comm_write(usr, obj, (String *) NULL, ayt,
					   strlen(ayt));
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
				comm_write(usr, obj, (String *) NULL, tm,
					   sizeof(tm));
			    } else if (UCHAR(*p) == TELOPT_SGA) {
				usr->flags &= ~CF_GA;
				comm_write(usr, obj, (String *) NULL, will_sga,
					   sizeof(will_sga));
			    }
			    state = TS_DATA;
			    break;

			case TS_DONT:
			    if (UCHAR(*p) == TELOPT_SGA) {
				usr->flags |= CF_GA;
				comm_write(usr, obj, (String *) NULL, wont_sga,
					   sizeof(wont_sga));
			    }
			    state = TS_DATA;
			    break;

			case TS_WILL:
			    if (UCHAR(*p) == TELOPT_LINEMODE) {
				/* linemode confirmed; now request editing */
				comm_write(usr, obj, (String *) NULL, mode_edit,
					   sizeof(mode_edit));
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

		    PUSH_STRVAL(f, str_new(usr->inbuf, (long) n));
		    for (n = usr->inbufsz; n != 0; --n) {
			*q++ = *p++;
		    }
		} else {
		    /*
		     * input buffer full
		     */
		    n = usr->inbufsz;
		    usr->inbufsz = 0;
		    PUSH_STRVAL(f, str_new(usr->inbuf, (long) n));
		}
		usr->flags |= CF_PROMPT;
		if (!(usr->flags & CF_FLUSH)) {
		    addtoflush(usr, d_get_extravar(o_dataspace(obj))->u.array);
		}
	    } else {
		/*
		 * binary connection
		 */
#ifndef NETWORK_EXTENSIONS
		if (usr->flags & CF_UDPDATA) {
		    n = conn_udpread(usr->conn, buffer, BINBUF_SIZE);
		    if (n >= 0) {
			/*
			 * received datagram
			 */
			PUSH_STRVAL(f, str_new(buffer, (long) n));
			this_user = obj->index;
			if (i_call(f, obj, (Array *) NULL, "receive_datagram",
				   16, TRUE, 1)) {
			    i_del_value(f->sp++);
			    endtask();
			}
			this_user = OBJ_NONE;
		    }
		    if (!(usr->flags & CF_UDP)) {
			continue;	/* datagram only */
		    }
		} else if ((usr->flags & CF_UDP) && conn_udpcheck(usr->conn)) {
		    usr->flags |= CF_UDPDATA;
		    this_user = obj->index;
		    if (i_call(f, obj, (Array *) NULL, "datagram_attach", 15,
			       TRUE, 0)) {
			i_del_value(f->sp++);
			endtask();
		    }
		    this_user = OBJ_NONE;
		}
#endif

		n = conn_read(usr->conn, p = buffer, BINBUF_SIZE);
		if (n <= 0) {
		    if (n < 0 && !(usr->flags & CF_OUTPUT)) {
			/*
			 * no more input and no pending output
			 */
			comm_del(f, usr, obj, FALSE);
			endtask();	/* this cannot be in comm_del() */
			break;
		    }
		    continue;
		}

		PUSH_STRVAL(f, str_new(buffer, (long) n));
	    }

	    this_user = obj->index;
	    if (i_call(f, obj, (Array *) NULL, "receive_message", 15, TRUE, 1))
	    {
		i_del_value(f->sp++);
		endtask();
	    }
	    this_user = OBJ_NONE;
	    break;
	}

	ec_pop();
    } catch (...) {
	endtask();
	this_user = OBJ_NONE;
	return;
    }

    comm_flush();
}

/*
 * NAME:	comm->ip_number()
 * DESCRIPTION:	return the ip number of a user (as a string)
 */
String *comm_ip_number(Object *obj)
{
    char ipnum[40];

    conn_ipnum(users[EINDEX(obj->etabi)].conn, ipnum);
    return str_new(ipnum, (long) strlen(ipnum));
}

/*
 * NAME:	comm->ip_name()
 * DESCRIPTION:	return the ip name of a user
 */
String *comm_ip_name(Object *obj)
{
    char ipname[1024];

    conn_ipname(users[EINDEX(obj->etabi)].conn, ipname);
    return str_new(ipname, (long) strlen(ipname));
}

/*
 * NAME:	comm->close()
 * DESCRIPTION:	remove a user
 */
void comm_close(Frame *f, Object *obj)
{
    comm_del(f, &users[EINDEX(obj->etabi)], obj, TRUE);
}

/*
 * NAME:	comm->user()
 * DESCRIPTION:	return the current user
 */
Object *comm_user()
{
    Object *obj;

    return (this_user != OBJ_NONE && (obj=OBJR(this_user))->count != 0) ?
	    obj : (Object *) NULL;
}

# ifdef NETWORK_EXTENSIONS
/*
 * NAME:	comm->ports()
 * DESCRIPTION:	return an array with all port objects
 */
Array *comm_ports(Dataspace *data)
{
    Array *a;
    int i, n;
    user *usr;
    Value *v;
    Object *obj;

    n = 0;
    for (i = nports, usr = users; i > 0; usr++) {
	if (usr->oindex != OBJ_NONE && (usr->flags & CF_PORT)) {
	    --i;
	    if (OBJR(usr->oindex)->count != 0) {
		n++;
	    }
	}
    }

    a = arr_new(data, (long) n);
    v = a->elts;
    for (usr = users; n > 0; usr++) {
	if (usr->oindex != OBJ_NONE && (usr->flags & CF_PORT) &&
	    (obj=OBJR(usr->oindex))->count != 0) {
	    PUT_OBJVAL(v, obj);
	    v++;
	    --n;
	}
    }
    return a;
}
# endif

/*
 * NAME:	comm->users()
 * DESCRIPTION:	return an array with all user objects
 */
Array *comm_users(Dataspace *data)
{
    Array *a;
    int i, n;
    user *usr;
    Value *v;
    Object *obj;

    n = 0;
    for (i = nusers, usr = users; i > 0; usr++) {
	if (usr->oindex != OBJ_NONE) {
#ifdef NETWORK_EXTENSIONS
	    if (usr->flags & CF_PORT) {
		continue;
	    }
#endif
	    --i;
	    if (!(usr->flags & CF_OPENDING)) {
		if (OBJR(usr->oindex)->count != 0) {
		    n++;
		}
	    }
	}
    }

    a = arr_new(data, (long) n);
    v = a->elts;
    for (usr = users; n > 0; usr++) {
	if (usr->oindex != OBJ_NONE && (obj=OBJR(usr->oindex))->count != 0) {
#ifdef NETWORK_EXTENSIONS
	    if (usr->flags & CF_PORT) {
		continue;
	    }
#endif
	    if (!(usr->flags & CF_OPENDING)) {
		PUT_OBJVAL(v, obj);
		v++;
		--n;
	    }
	}
    }
    return a;
}

/*
 * NAME:	comm->is_connection()
 * DESCRIPTION: is this REALLY a user object?
 */
bool comm_is_connection(Object *obj)
{
    user *usr;

    if ((obj->flags & O_SPECIAL) == O_USER) {
	usr = &users[EINDEX(obj->etabi)];
# ifdef NETWORK_EXTENSIONS
	if (usr->flags & CF_PORT) {
	    return FALSE;
	}
# endif
	if (!(usr->flags & CF_OPENDING)) {
	    return TRUE;
	}
    }
    return FALSE;
}

struct dump_header {
    short version;		/* hotboot version */
    Uint nusers;		/* # users */
    Uint tbufsz;		/* total telnet buffer size */
    Uint ubufsz;		/* total UDP buffer size */
};

static char dh_layout[] = "siii";

struct duser {
    char addr[24];		/* address */
    uindex oindex;		/* object index */
    short flags;		/* user flags */
    char state;			/* user state */
    char cflags;		/* connection flags */
    short newlines;		/* # newlines in input buffer */
    Uint tbufsz;		/* telnet buffer size */
    Uint osdone;		/* amount of output string done */
    Int fd;			/* file descriptor */
    Uint npkts;			/* # packets in UDP buffer */
    Uint ubufsz;		/* UDB buffer size */
    unsigned short port;	/* connection port */
    short at;			/* connected at */
};

static char du_layout[] = "ccccccccccccccccccccccccusccsiiiiiss";

/*
 * NAME:	comm->dump()
 * DESCRIPTION:	save users
 */
bool comm_dump(int fd)
{
    dump_header dh;
    duser *du;
    char **bufs, *tbuf, *ubuf;
    user *usr;
    int i;

    du = (duser *) NULL;
    bufs = (char **) NULL;
    tbuf = ubuf = (char *) NULL;

    /* header */
    dh.version = 1;
    dh.nusers = nusers;
    dh.tbufsz = 0;
    dh.ubufsz = 0;

    /*
     * gather information about users
     */
    if (nusers != 0) {
	du = ALLOC(duser, nusers);
	bufs = ALLOC(char*, 2 * nusers);

	for (i = nusers, usr = users; i > 0; usr++) {
	    if (usr->oindex != OBJ_NONE) {
		int npkts, ubufsz;

		du->oindex = usr->oindex;
		du->flags = usr->flags;
		du->state = usr->state;
		du->newlines = usr->newlines;
		du->tbufsz = usr->inbufsz;
		du->osdone = usr->osdone;
		*bufs++ = usr->inbuf;
		if (!conn_export(usr->conn, &du->fd, du->addr, &du->port,
				 &du->at, &npkts, &ubufsz, bufs++,
				 &du->cflags)) {
		    /* no hotbooting support */
		    FREE(du);
		    FREE(bufs - 2);
		    return FALSE;
		}
		du->npkts = npkts;
		du->ubufsz = ubufsz;
		dh.tbufsz += du->tbufsz;
		dh.ubufsz += du->ubufsz;

		du++;
		--i;
	    }
	}
	du -= nusers;
	bufs -= 2 * nusers;
    }

    /* write header */
    if (!sw_write(fd, &dh, sizeof(dump_header))) {
	fatal("failed to dump user header");
    }

    if (nusers != 0) {
	/*
	 * write users
	 */
	if (!sw_write(fd, du, nusers * sizeof(duser))) {
	    fatal("failed to dump users");
	}

	if (dh.tbufsz != 0) {
	    tbuf = ALLOC(char, dh.tbufsz);
	}
	if (dh.ubufsz != 0) {
	    ubuf = ALLOC(char, dh.ubufsz);
	}

	/*
	 * copy buffer content
	 */
	for (i = nusers; i > 0; --i, du++) {
	    if (du->tbufsz != 0) {
		memcpy(tbuf, *bufs, du->tbufsz);
		tbuf += du->tbufsz;
	    }
	    bufs++;
	    if (du->ubufsz != 0) {
		memcpy(ubuf, *bufs, du->ubufsz);
		ubuf += du->ubufsz;
	    }
	    bufs++;
	}
	tbuf -= dh.tbufsz;
	ubuf -= dh.ubufsz;

	/*
	 * write buffer content
	 */
	if (dh.tbufsz != 0) {
	    if (!sw_write(fd, tbuf, dh.tbufsz)) {
		fatal("failed to dump telnet buffers");
	    }
	    FREE(tbuf);
	}
	if (dh.ubufsz != 0) {
	    if (!sw_write(fd, ubuf, dh.ubufsz)) {
		fatal("failed to dump UDP buffers");
	    }
	    FREE(ubuf);
	}

	FREE(du - nusers);
	FREE(bufs - 2 * nusers);
    }

    return TRUE;
}

/*
 * NAME:	comm->restore()
 * DESCRIPTION:	restore users
 */
bool comm_restore(int fd)
{
    dump_header dh;
    duser *du;
    char *tbuf, *ubuf;
    int i;
    user *usr;
    connection *conn;

    tbuf = ubuf = (char *) NULL;

    /* read header */
    conf_dread(fd, (char *) &dh, dh_layout, 1);
    if (dh.nusers > maxusers) {
	fatal("too many users");
    }

    if (dh.nusers != 0) {
	/* read users and buffers */
	du = ALLOC(duser, dh.nusers);
	conf_dread(fd, (char *) du, du_layout, dh.nusers);
	if (dh.tbufsz != 0) {
	    tbuf = ALLOC(char, dh.tbufsz);
	    if (P_read(fd, tbuf, dh.tbufsz) != dh.tbufsz) {
		fatal("cannot read telnet buffer");
	    }
	}
	if (dh.ubufsz != 0) {
	    ubuf = ALLOC(char, dh.ubufsz);
	    if (P_read(fd, ubuf, dh.ubufsz) != dh.ubufsz) {
		fatal("cannot read UDP buffer");
	    }
	}

	for (i = dh.nusers; i > 0; --i) {
	    /* import connection */
	    conn = conn_import(du->fd, du->addr, du->port, du->at, du->npkts,
			       du->ubufsz, ubuf, du->cflags,
			       (du->flags & CF_TELNET) != 0);
	    if (conn == (connection *) NULL) {
		if (nusers == 0) {
		    if (dh.ubufsz != 0) {
			FREE(ubuf);
		    }
		    if (dh.tbufsz != 0) {
			FREE(tbuf);
		    }
		    FREE(du);
		    return FALSE;
		}
		fatal("cannot restore user");
	    }
	    ubuf += du->ubufsz;

	    /* allocate user */
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
	    nusers++;

	    /* initialize user */
	    usr->oindex = du->oindex;
	    OBJ(usr->oindex)->etabi = usr - users;
	    OBJ(usr->oindex)->flags |= O_USER;
	    usr->flags = du->flags;
	    if (usr->flags & CF_ODONE) {
		odone++;
	    }
	    usr->state = du->state;
	    usr->newlines = du->newlines;
	    newlines += usr->newlines;
	    usr->conn = conn;
	    if (usr->flags & CF_TELNET) {
		m_static();
		usr->inbuf = ALLOC(char, INBUF_SIZE + 1);
		*usr->inbuf++ = LF;	/* sentinel */
		m_dynamic();
	    } else {
		usr->inbuf = (char *) NULL;
	    }
	    usr->extra = (Array *) NULL;
	    usr->outbuf = (String *) NULL;
	    usr->inbufsz = du->tbufsz;
	    if (usr->inbufsz != 0) {
		memcpy(usr->inbuf, tbuf, usr->inbufsz);
		tbuf += usr->inbufsz;
	    }
	    usr->osdone = du->osdone;

	    du++;
	}
	if (dh.ubufsz != 0) {
	    FREE(ubuf - dh.ubufsz);
	}
	if (dh.tbufsz != 0) {
	    FREE(tbuf - dh.tbufsz);
	}
	FREE(du - dh.nusers);
    }

    return TRUE;
}
