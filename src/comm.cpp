/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2025 DGD Authors (see the commit log for details)
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
# include "xfloat.h"
# include "data.h"
# include "interpret.h"
# include "comm.h"
# include "version.h"
# include <errno.h>

#ifdef NETWORK_EXTENSIONS
# error network extensions are not currently supported
#endif

# ifndef TELOPT_LINEMODE
# define TELOPT_LINEMODE	34	/* linemode option */
# define LM_MODE		1
# define MODE_EDIT		0x01
# endif

# define MAXIACSEQLEN		7	/* longest IAC sequence sent */

class User {
public:
    void addtoflush(Array *arr);
    Array *setup(Frame *f, Object *obj);
    void del(Frame *f, Object *obj, bool destruct);
    int write(Object *obj, String *str, char *text, unsigned int len);
    void uflush(Object *obj, Dataspace *data, Array *arr);

    static User *create(Frame *f, Object *obj, Connection *conn, int flags);

    uindex oindex;		/* associated object index */
    User *prev;			/* preceding user */
    User *next;			/* next user */
    User *flush;		/* next in flush list */
    short flags;		/* connection flags */
    char state;			/* telnet state */
    short newlines;		/* # of newlines in input buffer */
    Connection *conn;		/* connection */
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
# define CF_STOPPED	0x0200	/* output stopped */

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

static User *users;		/* array of users */
static User *lastuser;		/* last user checked */
static User *freeuser;		/* linked list of free users */
static User *flush;		/* flush list */
static Uint nusers;		/* # of users */
static int odone;		/* # of users with output done */
static uindex this_user;	/* current user */

/*
 * accept a new connection
 */
User *User::create(Frame *f, Object *obj, Connection *conn, int flags)
{
    static char init[] = { (char) IAC, (char) WONT, (char) TELOPT_ECHO,
			   (char) IAC, (char) DO,   (char) TELOPT_LINEMODE };
    User *usr;
    Array *arr;
    Value val;

    if (obj->flags & O_SPECIAL) {
	EC->error("User object is already special purpose");
    }

    if (obj->flags & O_DRIVER) {
	EC->error("Cannot use driver object as user object");
    }

    usr = freeuser;
    freeuser = usr->next;
    if (lastuser != (User *) NULL) {
	usr->prev = lastuser->prev;
	usr->prev->next = usr;
	usr->next = lastuser;
	lastuser->prev = usr;
    } else {
	usr->prev = usr;
	usr->next = usr;
	lastuser = usr;
    }

    arr = usr->setup(f, obj);
    usr->conn = conn;
    usr->flags = flags;
    if (flags & CF_TELNET) {
	/* initialize connection */
	usr->flags = CF_TELNET | CF_ECHO | CF_OUTPUT;
	usr->state = TS_DATA;
	usr->newlines = 0;
	usr->inbufsz = 0;
	MM->staticMode();
	usr->inbuf = ALLOC(char, INBUF_SIZE + 1);
	*usr->inbuf++ = LF;	/* sentinel */
	MM->dynamicMode();

	arr->elts[0].number = CF_ECHO;
	PUT_STRVAL_NOREF(&val, String::create(init, sizeof(init)));
	obj->data->assignElt(arr, &arr->elts[1], &val);
    }
    nusers++;

    return usr;
}

/*
 * add a user to the flush list
 */
void User::addtoflush(Array *arr)
{
    flags |= CF_FLUSH;
    flush = ::flush;
    ::flush = this;
    extra = arr;
    extra->ref();

    /* remember initial buffer */
    if (Dataspace::elts(arr)[1].type == T_STRING) {
	outbuf = arr->elts[1].string;
	outbuf->ref();
    }
}

/*
 * setup a user
 */
Array *User::setup(Frame *f, Object *obj)
{
    Dataspace *data;
    Array *arr;
    Value val;

    if (obj->flags & O_DRIVER) {
	EC->error("Cannot use driver object as user object");
    }

    /* initialize dataspace before the object receives the user role */
    if (!O_HASDATA(obj) &&
	f->call(obj, (LWO *) NULL, (char *) NULL, 0, TRUE, 0)) {
	(f->sp++)->del();
    }

    Dataspace::wipeExtra(data = obj->dataspace());
    arr = Array::create(data, 3);
    arr->elts[0] = zeroInt;
    arr->elts[1] = arr->elts[2] = nil;
    PUT_ARRVAL_NOREF(&val, arr);
    Dataspace::setExtra(data, &val);

    oindex = obj->index;
    obj->flags |= O_USER;
    obj->etabi = this - users;
    conn = NULL;
    outbuf = (String *) NULL;
    osdone = 0;
    flags = 0;

    return arr;
}

/*
 * delete a connection
 */
void User::del(Frame *f, Object *obj, bool destruct)
{
    Dataspace *data;
    uindex olduser;

    data = obj->dataspace();
    if (!destruct) {
	/* if not destructing, make sure the connection terminates */
	if (!(flags & CF_FLUSH)) {
	    addtoflush(Dataspace::extra(data)->array);
	}
	obj->flags &= ~O_USER;
    }
    olduser = this_user;
    try {
	EC->push();
	this_user = obj->index;
	PUSH_INTVAL(f, destruct);
	if (f->call(obj, (LWO *) NULL, "close", 5, TRUE, 1)) {
	    (f->sp++)->del();
	}
	this_user = olduser;
	EC->pop();
    } catch (const char*) {
	this_user = olduser;
	EC->error((char *) NULL);
    }
    if (destruct) {
	/* if destructing, don't disconnect if there's an error in close() */
	if (!(flags & CF_FLUSH)) {
	    addtoflush(Dataspace::extra(data)->array);
	}
	obj->flags &= ~O_USER;
    }
}

/*
 * add bytes to output buffer
 */
int User::write(Object *obj, String *str, char *text, unsigned int len)
{
    Dataspace *data;
    Array *arr;
    Value *v;
    ssizet osdone, olen;
    Value val;

    arr = Dataspace::extra(data = obj->dataspace())->array;
    if (!(flags & CF_FLUSH)) {
	addtoflush(arr);
    }

    v = arr->elts + 1;
    if (v->type == T_STRING) {
	/* append to existing buffer */
	osdone = (outbuf == v->string) ? this->osdone : 0;
	olen = v->string->len - osdone;
	if (olen + len > MAX_STRLEN) {
	    len = MAX_STRLEN - olen;
	    if (len == 0 ||
		((flags & CF_TELNET) && text[0] == (char) IAC &&
		 len < MAXIACSEQLEN)) {
		return 0;
	    }
	}
	str = String::create((char *) NULL, (long) olen + len);
	memcpy(str->text, v->string->text + osdone, olen);
	memcpy(str->text + olen, text, len);
    } else {
	/* create new buffer */
	if (flags & CF_ODONE) {
	    flags &= ~CF_ODONE;
	    --odone;
	}
	flags |= CF_OUTPUT;
	if (str == (String *) NULL) {
	    str = String::create(text, len);
	}
    }

    PUT_STRVAL_NOREF(&val, str);
    data->assignElt(arr, v, &val);
    return len;
}

/*
 * flush output buffers for a single user only
 */
void User::uflush(Object *obj, Dataspace *data, Array *arr)
{
    Value *v;
    int n;

    UNREFERENCED_PARAMETER(obj);

    v = Dataspace::elts(arr);

    if (v[1].type == T_STRING) {
	if (conn->wrdone()) {
	    n = conn->write(v[1].string->text + osdone,
			    v[1].string->len - osdone);
	    if (n >= 0) {
		n += osdone;
		if (n == v[1].string->len) {
		    /* buffer fully drained */
		    n = 0;
		    flags &= ~CF_OUTPUT;
		    flags |= CF_ODONE;
		    odone++;
		    data->assignElt(arr, &v[1], &nil);
		}
		osdone = n;
	    } else {
		/* wait for conn_read() to discover the problem */
		flags &= ~CF_OUTPUT;
	    }
	}
    } else {
	/* just a datagram */
	flags &= ~CF_OUTPUT;
    }

    if (v[2].type == T_STRING) {
	if (flags & CF_UDPDATA) {
	    conn->writeUdp(v[2].string->text, v[2].string->len);
	} else if (conn->udp(v[2].string->text, v[2].string->len)) {
	    flags |= CF_UDP;
	}
	data->assignElt(arr, &v[2], &nil);
    }
}


static User *outbound;		/* pending outbound list */
static Uint maxusers;		/* max # of users */
static Uint maxdgram;		/* max # of datagram users */
static Uint ndgram;		/* # of datagram users */
static long newlines;		/* # of newlines in all input buffers */
static int ntport, nbport;	/* # telnet/binary ports */
static int ndport;		/* # datagram ports */
static int nexttport;		/* next telnet port to check */
static int nextbport;		/* next binary port to check */
static int nextdport;		/* next datagram port to check */
static char ayt[22];		/* are you there? */

/*
 * initialize communications
 */
bool Comm::init(int n, int p, char **thosts, char **bhosts, char **dhosts,
		unsigned short *tports, unsigned short *bports,
		unsigned short *dports, int ntelnet, int nbinary, int ndatagram)
{
    int i;
    User *usr;

    n += p;
    maxusers = n;
    maxdgram = p;
    ndgram = 0;
    users = ALLOC(User, n);
    for (i = n, usr = users + i; i > 0; --i) {
	--usr;
	usr->oindex = OBJ_NONE;
	usr->next = usr + 1;
    }
    users[n - 1].next = (User *) NULL;

    freeuser = usr;
    lastuser = (User *) NULL;
    ::flush = outbound = (User *) NULL;
    nusers = odone = newlines = 0;
    this_user = OBJ_NONE;

    snprintf(ayt, sizeof(ayt), "\15\12[%s]\15\12", VERSION);

    nexttport = nextbport = nextdport = 0;

    return Connection::init(n, thosts, bhosts, dhosts, tports, bports, dports,
			    ntport = ntelnet, nbport = nbinary,
			    ndport = ndatagram);
}

/*
 * clean up connections
 */
void Comm::clear()
{
    Connection::clear();
}

/*
 * terminate connections
 */
void Comm::finish()
{
    Connection::finish();
}

/*
 * start listening on telnet port and binary port
 */
void Comm::listen()
{
    Connection::listen();
}

/*
 * attempt to establish an outbound connection
 */
void Comm::connect(Frame *f, Object *obj, char *addr, unsigned short port)
{
    void *host;
    int len;
    User *usr;
    Array *arr;
    Value val;

    if (nusers >= maxusers)
	EC->error("Max number of connection objects exceeded");

    host = Connection::host(addr, port, &len);
    if (host == (void *) NULL) {
	EC->error("Unknown address");
    }

    for (usr = outbound; ; usr = usr->flush) {
	if (usr == (User *) NULL) {
	    usr = User::create(f, obj, (Connection *) NULL, 0);
	    arr = Dataspace::extra(obj->data)->array;
	    usr->flush = outbound;
	    outbound = usr;
	    break;
	}
	if ((OBJR(usr->oindex)->flags & O_SPECIAL) != O_USER) {
	    /*
	     * a previous outbound connection was undone, reuse it
	     */
	    usr->extra->del();
	    arr = usr->setup(f, obj);
	    break;
	}
    }

    PUT_INTVAL(&val, -1);
    obj->data->assignElt(arr, &arr->elts[0], &val);
    PUT_STRVAL_NOREF(&val, String::create((char *) host, len));
    obj->data->assignElt(arr, &arr->elts[1], &val);
    usr->flags |= CF_FLUSH;
    usr->extra = arr;
    usr->extra->ref();
    usr->flags |= CF_OPENDING;
}

/*
 * attempt to establish an outbound datagram connection
 */
void Comm::connectDgram(Frame *f, Object *obj, int uport, char *addr,
			unsigned short port)
{
    void *host;
    int len;
    User *usr;
    Array *arr;
    Value val;

    if (ndgram >= maxdgram) {
	EC->error("Max number of connection objects exceeded");
    }

    if (uport < 0 || uport >= ndport) {
	EC->error("No such datagram port");
    }
    host = Connection::host(addr, port, &len);
    if (host == (void *) NULL) {
	EC->error("Unknown address");
    }

    for (usr = outbound; ; usr = usr->flush) {
	if (usr == (User *) NULL) {
	    usr = User::create(f, obj, (Connection *) NULL, 0);
	    arr = Dataspace::extra(obj->data)->array;
	    usr->flush = outbound;
	    outbound = usr;
	    break;
	}
	if ((OBJR(usr->oindex)->flags & O_SPECIAL) != O_USER) {
	    /*
	     * a previous outbound connection was undone, reuse it
	     */
	    usr->extra->del();
	    arr = usr->setup(f, obj);
	    break;
	}
    }

    PUT_INTVAL(&val, uport);
    obj->data->assignElt(arr, &arr->elts[0], &val);
    PUT_STRVAL_NOREF(&val, String::create((char *) host, len));
    obj->data->assignElt(arr, &arr->elts[1], &val);
    usr->flags |= CF_UDPDATA | CF_FLUSH;
    usr->extra = arr;
    usr->extra->ref();
    usr->flags |= CF_OPENDING;
}

/*
 * set the UDP challenge for a binary connection
 */
void Comm::challenge(Object *obj, String *str)
{
    User *usr;
    Dataspace *data;
    Array *arr;
    Value *v;
    Value val;

    usr = &users[obj->etabi];
    if (usr->flags & CF_TELNET || !usr->conn->attach()) {
	EC->error("Datagram channel not available");
    }
    if (usr->flags & CF_UDPDATA) {
	EC->error("Datagram channel already established");
    }
    arr = Dataspace::extra(data = obj->data)->array;
    if (!(usr->flags & CF_FLUSH)) {
	usr->addtoflush(arr);
    }

    v = arr->elts + 2;
    if ((usr->flags & CF_UDP) || v->type == T_STRING) {
	EC->error("Datagram challenge already set");
    }
    usr->flags |= CF_OUTPUT;
    PUT_STRVAL_NOREF(&val, str);
    data->assignElt(arr, v, &val);
}

/*
 * send a message to a user
 */
int Comm::send(Object *obj, String *str)
{
    Value *v;
    User *usr;

    v = Dataspace::elts(Dataspace::extra(obj->data)->array);
    if (v->number & CF_STOPPED) {
	EC->error("Output channel closed");
    }
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
		n = usr->write(obj, (String *) NULL, outbuf, size);
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
	    EC->error("Message channel not enabled");
	}

	/*
	 * binary connection
	 */
	return usr->write(obj, str, str->text, str->len);
    }
}

/*
 * send a message on the UDP channel of a binary connection
 */
int Comm::udpsend(Object *obj, String *str)
{
    User *usr;
    Dataspace *data;
    Array *arr;
    Value *v;
    Value val;

    usr = &users[EINDEX(obj->etabi)];
    if ((usr->flags & (CF_TELNET | CF_UDPDATA)) != CF_UDPDATA) {
	EC->error("Datagram channel not established");
    }

    arr = Dataspace::extra(data = obj->data)->array;
    if (!(usr->flags & CF_FLUSH)) {
	usr->addtoflush(arr);
    }

    v = arr->elts + 2;
    if (v->type == T_STRING) {
	return 0;	/* datagram queued already */
    }
    usr->flags |= CF_OUTPUT;
    PUT_STRVAL_NOREF(&val, str);
    data->assignElt(arr, v, &val);

    return str->len;
}

/*
 * turn on/off input echoing for a user
 */
bool Comm::echo(Object *obj, int echo)
{
    User *usr;
    Dataspace *data;
    Array *arr;
    Value *v;

    usr = &users[EINDEX(obj->etabi)];
    if (usr->flags & CF_TELNET) {
	arr = Dataspace::extra(data = obj->data)->array;
	v = Dataspace::elts(arr);
	if (echo != (v->number & CF_ECHO) >> 1) {
	    Value val;

	    if (!(usr->flags & CF_FLUSH)) {
		usr->addtoflush(arr);
	    }
	    val = *v;
	    val.number ^= CF_ECHO;
	    data->assignElt(arr, v, &val);
	}
	return TRUE;
    }
    return FALSE;
}

/*
 * suspend or release input from a user
 */
void Comm::block(Object *obj, int block)
{
    User *usr;
    Dataspace *data;
    Array *arr;
    Value *v;

    usr = &users[EINDEX(obj->etabi)];
    arr = Dataspace::extra(data = obj->data)->array;
    v = Dataspace::elts(arr);
    if (block != (v->number & CF_BLOCKED) >> 4) {
	Value val;

	if (!(usr->flags & CF_FLUSH)) {
	    usr->addtoflush(arr);
	}
	val = *v;
	val.number ^= CF_BLOCKED;
	data->assignElt(arr, v, &val);
    }
}

/*
 * close output channel
 */
void Comm::stop(Object *obj)
{
    User *usr;
    Dataspace *data;
    Array *arr;
    Value *v;

    usr = &users[EINDEX(obj->etabi)];
    if (!(usr->flags & CF_TELNET) &&
	(usr->flags & (CF_UDP | CF_UDPDATA)) == CF_UDPDATA) {
	EC->error("Message channel not enabled");
    }
    arr = Dataspace::extra(data = obj->data)->array;
    v = Dataspace::elts(arr);
    if (v->number & CF_STOPPED) {
	EC->error("Output channel already closed");
    } else {
	Value val;

	if (!(usr->flags & CF_FLUSH)) {
	    usr->addtoflush(arr);
	}
	val = *v;
	val.number |= CF_STOPPED;
	data->assignElt(arr, v, &val);
    }
}

/*
 * flush state, output and connections
 */
void Comm::flush()
{
    User *usr;
    Object *obj;
    Array *arr;
    Value *v;

    while (outbound != (User *) NULL) {
	usr = outbound;
	outbound = usr->flush;

	arr = usr->extra;
	obj = OBJ(usr->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER) {
	    /* connect */
	    if (arr->elts[0].number < 0) {
		usr->conn = Connection::connect(arr->elts[1].string->text,
						arr->elts[1].string->len);
	    } else {
		usr->conn = Connection::connectDgram(arr->elts[0].number,
						     arr->elts[1].string->text,
						     arr->elts[1].string->len);
	    }
	    if (usr->conn == (Connection *) NULL) {
		EC->fatal("can't connect to server");
	    }

	    obj->data->assignElt(arr, &arr->elts[0], &zeroInt);
	    obj->data->assignElt(arr, &arr->elts[1], &nil);
	    arr->del();
	    usr->flags &= ~CF_FLUSH;
	} else if (obj->count != 0) {
	    /* clean up */
	    usr->flush = ::flush;
	    ::flush = usr;
	} else {
	    /* discard */
	    usr->oindex = OBJ_NONE;
	    if (usr->next == usr) {
		lastuser = (User *) NULL;
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

    while (::flush != (User *) NULL) {
	usr = ::flush;
	::flush = usr->flush;

	/*
	 * status change
	 */
	obj = OBJ(usr->oindex);
	arr = usr->extra;
	v = arr->elts;
	if (usr->flags & CF_TELNET) {
	    if ((v->number ^ usr->flags) & CF_ECHO) {
		char buf[3];

		/* change echo */
		buf[0] = (char) IAC;
		buf[1] = (v->number & CF_ECHO) ? (char) WONT : (char) WILL;
		buf[2] = TELOPT_ECHO;
		if (usr->write(obj, (String *) NULL, buf, 3) != 0) {
		    usr->flags ^= CF_ECHO;
		}
	    }
	    if (usr->flags & CF_PROMPT) {
		usr->flags &= ~CF_PROMPT;
		if ((usr->flags & CF_GA) && v[1].type == T_STRING &&
		    usr->outbuf != v[1].string) {
		    static char ga[] = { (char) IAC, (char) GA };

		    /* append go-ahead */
		    usr->write(obj, (String *) NULL, ga, 2);
		}
	    }
	}
	if ((v->number ^ usr->flags) & CF_BLOCKED) {
	    usr->flags ^= CF_BLOCKED;
	    usr->conn->block(((usr->flags & CF_BLOCKED) != 0));
	}

	/*
	 * write
	 */
	if (usr->outbuf != (String *) NULL) {
	    if (usr->outbuf != v[1].string) {
		usr->osdone = 0;	/* new mesg before buffer drained */
	    }
	    usr->outbuf->del();
	    usr->outbuf = (String *) NULL;
	}
	if (usr->flags & CF_OUTPUT) {
	    usr->uflush(obj, obj->data, arr);
	}
	if ((v->number & CF_STOPPED) && !(usr->flags & CF_STOPPED)) {
	    obj->data->assignElt(arr, &v[1], &nil);
	    usr->flags |= CF_STOPPED;
	    usr->conn->stop();
	}
	/*
	 * disconnect
	 */
	if ((obj->flags & O_SPECIAL) != O_USER) {
	    Dataspace::wipeExtra(obj->data);
	    if (usr->conn != (Connection *) NULL) {
		usr->conn->del();
	    }
	    if (usr->flags & CF_TELNET) {
		newlines -= usr->newlines;
		FREE(usr->inbuf - 1);
	    }
	    if (usr->flags & CF_ODONE) {
		--odone;
	    }

	    usr->oindex = OBJ_NONE;
	    if (usr->next == usr) {
		lastuser = (User *) NULL;
	    } else {
		usr->next->prev = usr->prev;
		usr->prev->next = usr->next;
		if (usr == lastuser) {
		    lastuser = usr->next;
		}
	    }
	    usr->next = freeuser;
	    freeuser = usr;
	    if ((usr->flags & (CF_TELNET | CF_UDP | CF_UDPDATA)) == CF_UDPDATA)
	    {
		--ndgram;
	    }
	    --nusers;
	}

	arr->del();
	usr->flags &= ~CF_FLUSH;
    }
}

/*
 * accept a telnet connection
 */
void Comm::acceptTelnet(Frame *f, Connection *conn, int port)
{
    User *usr;
    Object *obj;

    try {
	EC->push();
	PUSH_INTVAL(f, port);
	DGD::callDriver(f, "telnet_connect", 1);
	if (f->sp->type != T_OBJECT) {
	    EC->fatal("driver->telnet_connect() did not return persistent object");
	}
	obj = OBJ(f->sp->oindex);
	f->sp++;
	usr = User::create(f, obj, conn, CF_TELNET);
	EC->pop();
    } catch (const char*) {
	conn->del();		/* delete connection */
	EC->error((char *) NULL);	/* pass on error */
    }

    usr->flags |= CF_PROMPT;
    usr->addtoflush(Dataspace::extra(obj->dataspace())->array);
    this_user = obj->index;
    if (f->call(obj, (LWO *) NULL, "open", 4, TRUE, 0)) {
	(f->sp++)->del();
    }
    DGD::endTask();
    this_user = OBJ_NONE;
}

/*
 * accept a binary connection
 */
void Comm::accept(Frame *f, Connection *conn, int port)
{
    Object *obj;

    try {
	EC->push();
	PUSH_INTVAL(f, port);
	DGD::callDriver(f, "binary_connect", 1);
	if (f->sp->type != T_OBJECT) {
	    EC->fatal("driver->binary_connect() did not return persistent object");
	}
	obj = OBJ(f->sp->oindex);
	f->sp++;
	User::create(f, obj, conn, 0);
	EC->pop();
    } catch (const char*) {
	conn->del();		/* delete connection */
	EC->error((char *) NULL);	/* pass on error */
    }

    this_user = obj->index;
    if (f->call(obj, (LWO *) NULL, "open", 4, TRUE, 0)) {
	(f->sp++)->del();
    }
    DGD::endTask();
    this_user = OBJ_NONE;
}

/*
 * accept a datagram connection
 */
void Comm::acceptDgram(Frame *f, Connection *conn, int port)
{
    Object *obj;

    try {
	EC->push();
	PUSH_INTVAL(f, port);
	DGD::callDriver(f, "datagram_connect", 1);
	if (f->sp->type != T_OBJECT) {
	    EC->fatal("driver->datagram_connect() did not return persistent object");
	}
	obj = OBJ(f->sp->oindex);
	f->sp++;
	User::create(f, obj, conn, CF_UDPDATA);
	ndgram++;
	EC->pop();
    } catch (const char*) {
	conn->del();		/* delete connection */
	EC->error((char *) NULL);	/* pass on error */
    }

    this_user = obj->index;
    if (f->call(obj, (LWO *) NULL, "open", 4, TRUE, 0)) {
	(f->sp++)->del();
    }
    DGD::endTask();
    this_user = OBJ_NONE;
}

/*
 * receive a message from a user
 */
void Comm::receive(Frame *f, Uint timeout, unsigned int mtime)
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
    User *usr;
    int n, i, state, nls;
    char *p, *q;
    Connection *conn;

    if (newlines != 0 || odone != 0) {
	timeout = mtime = 0;
    }
    n = Connection::select(timeout, mtime);
    if ((n <= 0) && (newlines == 0) && (odone == 0)) {
	/*
	 * call_out to do, or timeout
	 */
	return;
    }

    try {
	EC->push(DGD::errHandler);
	if (ntport != 0 && nusers < maxusers) {
	    n = nexttport;
	    do {
		/*
		 * accept new telnet connection
		 */
		conn = Connection::createTelnet6(n);
		if (conn != (Connection *) NULL) {
		    acceptTelnet(f, conn, n);
		    nexttport = (n + 1) % ntport;
		}
		if (nusers < maxusers) {
		    conn = Connection::createTelnet(n);
		    if (conn != (Connection *) NULL) {
			acceptTelnet(f, conn, n);
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
		conn = Connection::create6(n);
		if (conn != (Connection *) NULL) {
		    accept(f, conn, n);
		}
		if (nusers < maxusers) {
		    conn = Connection::create(n);
		    if (conn != (Connection *) NULL) {
			accept(f, conn, n);
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
		conn = Connection::createDgram6(n);
		if (conn != (Connection *) NULL) {
		    acceptDgram(f, conn, n);
		}
		if (ndgram < maxdgram) {
		    conn = Connection::createDgram(n);
		    if (conn != (Connection *) NULL) {
			acceptDgram(f, conn, n);
		    }
		}
		n = (n + 1) % ndport;
		if (ndgram == maxdgram) {
		    nextdport = n;
		    break;
		}
	    } while (n != nextdport);
	}

	for (i = nusers; lastuser != (User *) NULL && i > 0; --i) {
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
		retval = usr->conn->checkConnected(&errcode);
		/*
		 * Something happened to the connection..
		 * its either connected or in error state now.
		 */
		if (retval != 0) {
		    usr->flags &= ~CF_OPENDING;
		    if (!(usr->flags & CF_FLUSH)) {
			usr->addtoflush(Dataspace::extra(obj->dataspace())->array);
		    }
		    old_user = this_user;
		    this_user = obj->index;
		    /*
		     * Error, report it to the user object.
		     */
		    if (retval < 0) {
			obj->flags &= ~O_USER;
			PUSH_INTVAL(f, errcode);
			if (f->call(obj, (LWO *) NULL, "unconnected", 11, TRUE,
				    1)) {
			    (f->sp++)->del();
			}
			DGD::endTask();
		    } else if (retval > 0) {
			/*
			 * Connection completed, call open in the user object.
			 */
			if (f->call(obj, (LWO *) NULL, "open", 4, TRUE, 0)) {
			    (f->sp++)->del();
			}
			DGD::endTask();
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

		data = obj->dataspace();
		usr->uflush(obj, data, Dataspace::extra(data)->array);
	    }
	    if (usr->flags & CF_ODONE) {
		/* callback */
		usr->flags &= ~CF_ODONE;
		--odone;
		this_user = obj->index;
		if (f->call(obj, (LWO *) NULL, "message_done", 12, TRUE, 0)) {
		    (f->sp++)->del();
		    DGD::endTask();
		}

		this_user = OBJ_NONE;
		if (obj->count == 0) {
		    break;	/* continue, unless the connection was closed */
		}
	    }

	    if (usr->flags & CF_BLOCKED) {
		continue;	/* no input on this connection */
	    }

	    if (usr->flags & CF_TELNET) {
		/*
		 * telnet connection
		 */
		if (usr->inbufsz != INBUF_SIZE) {
		    p = usr->inbuf + usr->inbufsz;
		    n = usr->conn->read(p, INBUF_SIZE - usr->inbufsz);
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
			    usr->del(f, obj, FALSE);
			    DGD::endTask(); /* this cannot be in comm_del() */
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
				usr->write(obj, (String *) NULL, intr,
					   sizeof(intr));
				state = TS_DATA;
				break;

			    case BREAK:
				usr->write(obj, (String *) NULL, brk,
					   sizeof(brk));
				state = TS_DATA;
				break;

			    case AYT:
				usr->write(obj, (String *) NULL, ayt,
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
				usr->write(obj, (String *) NULL, tm,
					   sizeof(tm));
			    } else if (UCHAR(*p) == TELOPT_SGA) {
				usr->flags &= ~CF_GA;
				usr->write(obj, (String *) NULL, will_sga,
					   sizeof(will_sga));
			    }
			    state = TS_DATA;
			    break;

			case TS_DONT:
			    if (UCHAR(*p) == TELOPT_SGA) {
				usr->flags |= CF_GA;
				usr->write(obj, (String *) NULL, wont_sga,
					   sizeof(wont_sga));
			    }
			    state = TS_DATA;
			    break;

			case TS_WILL:
			    if (UCHAR(*p) == TELOPT_LINEMODE) {
				/* linemode confirmed; now request editing */
				usr->write(obj, (String *) NULL, mode_edit,
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

		    PUSH_STRVAL(f, String::create(usr->inbuf, n));
		    for (n = usr->inbufsz; n != 0; --n) {
			*q++ = *p++;
		    }
		} else {
		    /*
		     * input buffer full
		     */
		    n = usr->inbufsz;
		    usr->inbufsz = 0;
		    PUSH_STRVAL(f, String::create(usr->inbuf, n));
		}
		usr->flags |= CF_PROMPT;
		if (!(usr->flags & CF_FLUSH)) {
		    usr->addtoflush(Dataspace::extra(obj->dataspace())->array);
		}
	    } else {
		/*
		 * binary connection
		 */
		if (usr->flags & CF_UDPDATA) {
		    n = usr->conn->readUdp(buffer, BINBUF_SIZE);
		    if (n >= 0) {
			/*
			 * received datagram
			 */
			PUSH_STRVAL(f, String::create(buffer, n));
			this_user = obj->index;
			if (f->call(obj, (LWO *) NULL, "receive_datagram", 16,
				    TRUE, 1)) {
			    (f->sp++)->del();
			    DGD::endTask();
			}
			this_user = OBJ_NONE;
		    }
		    if (!(usr->flags & CF_UDP)) {
			continue;	/* datagram only */
		    }
		} else if ((usr->flags & CF_UDP) && usr->conn->udpCheck()) {
		    usr->flags |= CF_UDPDATA;
		    this_user = obj->index;
		    if (f->call(obj, (LWO *) NULL, "datagram_attach", 15, TRUE,
				0)) {
			(f->sp++)->del();
			DGD::endTask();
		    }
		    this_user = OBJ_NONE;
		}

		n = usr->conn->read(buffer, BINBUF_SIZE);
		if (n <= 0) {
		    if (n < 0 && !(usr->flags & CF_OUTPUT)) {
			/*
			 * no more input and no pending output
			 */
			usr->del(f, obj, FALSE);
			DGD::endTask();	/* this cannot be in comm_del() */
			break;
		    }
		    continue;
		}

		PUSH_STRVAL(f, String::create(buffer, n));
	    }

	    this_user = obj->index;
	    if (f->call(obj, (LWO *) NULL, "receive_message", 15, TRUE, 1)) {
		(f->sp++)->del();
		DGD::endTask();
	    }
	    this_user = OBJ_NONE;
	    break;
	}

	EC->pop();
    } catch (const char*) {
	DGD::endTask();
	this_user = OBJ_NONE;
	return;
    }

    flush();
}

/*
 * return the ip number of a user (as a string)
 */
String *Comm::ipNumber(Object *obj)
{
    char ipnum[40];

    users[EINDEX(obj->etabi)].conn->ipnum(ipnum);
    return String::create(ipnum, strlen(ipnum));
}

/*
 * return the ip name of a user
 */
String *Comm::ipName(Object *obj)
{
    char ipname[1024];

    users[EINDEX(obj->etabi)].conn->ipname(ipname);
    return String::create(ipname, strlen(ipname));
}

/*
 * remove a user
 */
void Comm::close(Frame *f, Object *obj)
{
    users[EINDEX(obj->etabi)].del(f, obj, TRUE);
}

/*
 * return the current user
 */
Object *Comm::user()
{
    Object *obj;

    return (this_user != OBJ_NONE && (obj=OBJR(this_user))->count != 0) ?
	    obj : (Object *) NULL;
}

/*
 * return the number of connections
 */
eindex Comm::numUsers()
{
    return nusers;
}

/*
 * return an array with all user objects
 */
Array *Comm::listUsers(Dataspace *data)
{
    Array *a;
    int i, n;
    User *usr;
    Value *v;
    Object *obj;

    n = 0;
    for (i = nusers, usr = users; i > 0; usr++) {
	if (usr->oindex != OBJ_NONE) {
	    --i;
	    if (!(usr->flags & CF_OPENDING)) {
		if (OBJR(usr->oindex)->count != 0) {
		    n++;
		}
	    }
	}
    }

    a = Array::create(data, n);
    v = a->elts;
    for (usr = users; n > 0; usr++) {
	if (usr->oindex != OBJ_NONE && (obj=OBJR(usr->oindex))->count != 0) {
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
 * is this REALLY a user object?
 */
bool Comm::isConnection(Object *obj)
{
    User *usr;

    if ((obj->flags & O_SPECIAL) == O_USER) {
	usr = &users[EINDEX(obj->etabi)];
	if (!(usr->flags & CF_OPENDING)) {
	    return TRUE;
	}
    }
    return FALSE;
}

struct CommHeader {
    short version;		/* hotboot version */
    Uint nusers;		/* # users */
    Uint tbufsz;		/* total telnet buffer size */
    Uint ubufsz;		/* total UDP buffer size */
};

static char dh_layout[] = "siii";

struct SaveUser {
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
 * save users
 */
bool Comm::save(int fd)
{
    CommHeader dh;
    SaveUser *du;
    char **bufs, *tbuf, *ubuf;
    User *usr;
    int i;

    du = (SaveUser *) NULL;
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
	du = ALLOC(SaveUser, nusers);
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
		if (!usr->conn->cexport(&du->fd, du->addr, &du->port,
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
    if (!Swap::write(fd, &dh, sizeof(CommHeader))) {
	EC->fatal("failed to dump user header");
    }

    if (nusers != 0) {
	/*
	 * write users
	 */
	if (!Swap::write(fd, du, nusers * sizeof(SaveUser))) {
	    EC->fatal("failed to dump users");
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
	    if (!Swap::write(fd, tbuf, dh.tbufsz)) {
		EC->fatal("failed to dump telnet buffers");
	    }
	    FREE(tbuf);
	}
	if (dh.ubufsz != 0) {
	    if (!Swap::write(fd, ubuf, dh.ubufsz)) {
		EC->fatal("failed to dump UDP buffers");
	    }
	    FREE(ubuf);
	}

	FREE(du - nusers);
	FREE(bufs - 2 * nusers);
    }

    return TRUE;
}

/*
 * restore users
 */
bool Comm::restore(int fd)
{
    CommHeader dh;
    SaveUser *du;
    char *tbuf, *ubuf;
    int i;
    User *usr;
    Connection *conn;

    tbuf = ubuf = (char *) NULL;

    /* read header */
    Config::dread(fd, (char *) &dh, dh_layout, 1);
    if (dh.nusers > maxusers) {
	EC->fatal("too many users");
    }

    if (dh.nusers != 0) {
	/* read users and buffers */
	du = ALLOC(SaveUser, dh.nusers);
	Config::dread(fd, (char *) du, du_layout, dh.nusers);
	if (dh.tbufsz != 0) {
	    tbuf = ALLOC(char, dh.tbufsz);
	    if (P_read(fd, tbuf, dh.tbufsz) != dh.tbufsz) {
		EC->fatal("cannot read telnet buffer");
	    }
	}
	if (dh.ubufsz != 0) {
	    ubuf = ALLOC(char, dh.ubufsz);
	    if (P_read(fd, ubuf, dh.ubufsz) != dh.ubufsz) {
		EC->fatal("cannot read UDP buffer");
	    }
	}

	for (i = dh.nusers; i > 0; --i) {
	    /* import connection */
	    conn = Connection::import(du->fd, du->addr, du->port, du->at,
				      du->npkts, du->ubufsz, ubuf, du->cflags,
				      (du->flags & CF_TELNET) != 0);
	    if (conn == (Connection *) NULL) {
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
		EC->fatal("cannot restore user");
	    }
	    ubuf += du->ubufsz;

	    /* allocate user */
	    usr = freeuser;
	    freeuser = usr->next;
	    if (lastuser != (User *) NULL) {
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
		MM->staticMode();
		usr->inbuf = ALLOC(char, INBUF_SIZE + 1);
		*usr->inbuf++ = LF;	/* sentinel */
		MM->dynamicMode();
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
