# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/access.h>
# include <kernel/rsrc.h>
# include <kernel/user.h>
# include <type.h>
# include <trace.h>

# define BADARG(n, func)	error("bad argument " + (n) + \
				      " for function " + #func)

private object prev, next;	/* previous and next in linked list */
private string creator, owner;	/* creator and owner of this object */
private mapping resources;	/* resources associated with this object */
private mapping events;		/* events for this object */

nomask _F_prev(object obj)  { if (previous_program() == OBJREGD) prev = obj; }
nomask _F_next(object obj)  { if (previous_program() == OBJREGD) next = obj; }
nomask object _Q_prev()	    { if (SYSTEM()) return prev; }
nomask object _Q_next()	    { if (SYSTEM()) return next; }

/*
 * NAME:	query_owner()
 * DESCRIPTION:	query the owner of an object
 */
nomask string query_owner()
{
    if (owner[0] == '/') {
	string str;

	/*
	 * temporary owner for cloned object
	 */
	str = owner;
	owner = "System";
	return str[1 ..];
    } else {
	return owner;
    }
}

/*
 * NAME:	_F_rsrc_incr()
 * DESCRIPTION:	increase/decrease a resource associated with this object
 */
nomask _F_rsrc_incr(string rsrc, int incr)
{
    if (KERNEL()) {
	if (!resources) {
	    resources = ([ rsrc : incr ]);
	} else {
	    resources[rsrc] += incr;
	}
    }
}

/*
 * NAME:	_F_destruct()
 * DESCRIPTION:	prepare object for being destructed
 */
nomask _F_destruct()
{
    if (KERNEL()) {
	object rsrcd;
	int i, j;

	rsrcd = ::find_object(RSRCD);
	if (events) {
	    object **evtlist, *objlist, obj;

	    /*
	     * decrease resources of other objects subscribed to events
	     */
	    evtlist = map_values(events);
	    i = sizeof(evtlist);
	    while (--i >= 0) {
		j = sizeof(objlist = evtlist[i] - ({ 0 }));
		while (--j >= 0) {
		    obj = objlist[j];
		    if (obj != this_object()) {
			rsrcd->rsrc_incr(obj->query_owner(), "events", obj, -1);
		    }
		}
	    }
	}

	if (resources) {
	    string *names;
	    int *values;

	    /*
	     * decrease resources associated with object
	     */
	    names = map_indices(resources);
	    values = map_values(resources);
	    i = sizeof(names);
	    while (--i >= 0) {
		rsrcd->rsrc_incr(owner, names[i], this_object(), -values[i]);
	    }
	}

	if (sscanf(object_name(this_object()), "%*s#") != 0) {
	    /* non-clones are handled by driver->remove_program() */
	    rsrcd->rsrc_incr(owner, "objects", 0, -1);
	}
    }
}


# include "file.c"


/*
 * NAME:	_F_create()
 * DESCRIPTION:	kernel creator function
 */
nomask _F_create()
{
    if (!creator) {
	rlimits (-1; -1) {
	    string oname;

	    /*
	     * set creator and owner
	     */
	    oname = object_name(this_object());
	    creator = creator(oname);
	    if (!creator || sscanf(oname, "%s#", oname) != 0) {
		owner = previous_object()->query_owner();
	    } else {
		owner = creator;
	    }

	    /*
	     * register object
	     */
	    if (oname != OBJREGD) {
		::find_object(OBJREGD)->link(this_object(), owner);
	    }
	}
	/* call higher-level creator function */
	this_object()->create();
    }
}

/*
 * NAME:	find_object()
 * DESCRIPTION:	find an object
 */
static object find_object(string path)
{
    if (!path) {
	BADARG(1, find_object);
    }

    path = reduce_path(path, object_name(this_object()), creator);
    if (sscanf(path, "%*s/lib/") != 0) {
	return 0;	/* library object */
    }
    return ::find_object(path);
}

/*
 * NAME:	destruct_object()
 * DESCRIPTION:	destruct an object, if you can
 */
static destruct_object(mixed obj)
{
    string ocreator, oname;
    int lib;

    if (typeof(obj) == T_STRING) {
	obj = find_object(reduce_path(obj, object_name(this_object()),
				      creator));
    }
    if (typeof(obj) != T_OBJECT) {
	BADARG(1, destruct_object);
    }

    /*
     * check privileges
     */
    ocreator = creator(object_name(obj));
    oname = object_name(obj);
    lib = sscanf(oname, "%*s/lib/");
    if ((sscanf(oname, "/kernel/%*s") != 0) ?
	 ((lib) ?
	   creator != "System" :
	   sscanf(object_name(this_object()), "/kernel/%*s") == 0) :
	 ocreator && owner != ((lib) ? ocreator : obj->query_owner())) {
	/*
	 * kernel objects can only be destructed by kernel objects
	 */
	error("Cannot destruct object: not owner");
    }

    rlimits (-1; -1) {
	if (!lib) {
	    obj->_F_destruct();
	    if (oname != OBJREGD) {
		::find_object(OBJREGD)->unlink(obj, owner);
	    }
	}
	::destruct_object(obj);
    }
}

/*
 * NAME:	compile_object()
 * DESCRIPTION:	compile a master object
 */
static object compile_object(string path)
{
    string oname, uid;
    object rsrcd, obj;
    int *rsrc, *status, lib, init;

    if (!path) {
	BADARG(1, compile_object);
    }

    /*
     * check permission
     */
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS)) {
	error("Access denied");
    }

    /*
     * check resource usage
     */
    uid = creator(path);
    if (!uid) {
	uid = owner;
    }
    rsrcd = ::find_object(RSRCD);
    rsrc = rsrcd->rsrc_get(uid, "objects");
    if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	error("Too many objects");
    }

    lib = sscanf(path, "%*s/lib/");
    init = !lib && !::find_object(path);
    if (init) {
	status = ::status();
    }
    catch {
	rlimits (-1; -1) {
	    if (init) {
		int depth, ticks;

		depth = status[ST_STACKDEPTH];
		ticks = status[ST_TICKS];
		if ((depth >= 0 &&
		     depth < rsrcd->rsrc_get(uid, "create depth")[RSRC_MAX]) ||
		    (ticks >= 0 &&
		     ticks < rsrcd->rsrc_get(uid, "create ticks")[RSRC_MAX])) {
		    error("Insufficient stack or ticks to create object");
		}
	    }
	    obj = ::compile_object(path);
	    if (!rsrcd->rsrc_incr(uid, "objects", (lib) ? path : 0, 1, 1)) {
		/* will only happen to lib objects */
		::destruct_object(obj);
		error("Out of resources");
	    }
	}
    } : error("");
    if (init) {
	call_other(obj, "???");	/* initialize & register */
    }

    return (lib) ? 0 : obj;
}

/*
 * NAME:	clone_object()
 * DESCRIPTION:	clone an object
 */
static varargs object clone_object(string path, string uid)
{
    string oname, str;
    object rsrcd, obj;
    int *rsrc, *status;

    if (!path) {
	BADARG(1, clone_object);
    }
    if (owner != "System" || !uid) {
	uid = owner;
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);

    if (sscanf(path, "%*s/obj/") == 0 || sscanf(path, "%*s/lib/") != 0) {
	error("Cannot clone " + path);	/* not path of clonable */
    }

    if (creator != "System" &&
	(sscanf(path, "/kernel/%*s") != 0 ||
	 !::find_object(ACCESSD)->access(oname, path, READ_ACCESS))) {
	error("Access denied");
    }

    obj = ::find_object(path);
    if (!obj) {
	/* master object not compiled */
	error("No such object");
    }

    rsrcd = ::find_object(RSRCD);
    if (path != RSRCOBJ) {
	rsrc = rsrcd->rsrc_get(uid, "objects");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("Too many objects");
	}
    }

    status = ::status();
    catch {
	rlimits (-1; -1) {
	    int depth, ticks;

	    depth = status[ST_STACKDEPTH];
	    ticks = status[ST_TICKS];
	    if ((depth >= 0 &&
		 depth < rsrcd->rsrc_get(uid, "create depth")[RSRC_MAX]) ||
		(ticks >= 0 &&
		 ticks < rsrcd->rsrc_get(uid, "create ticks")[RSRC_MAX])) {
		error("Insufficient stack or ticks to create object");
	    }
	    if (path != RSRCOBJ && !rsrcd->rsrc_incr(uid, "objects", 0, 1, 1)) {
		error("Out of resources");
	    }
	    if (uid != owner) {
		owner = "/" + uid;
	    }
	}
    } : error("");
    return ::clone_object(obj);
}

/*
 * NAME:	call_trace()
 * DESCRIPTION:	call trace
 */
static mixed **call_trace()
{
    mixed **trace;
    int i, sz;
    mixed *call;

    trace = ::call_trace();
    trace = trace[.. sizeof(trace) - 2];	/* skip last */
    if (creator != "System") {
	for (i = 0, sz = sizeof(trace); i < sz; i++) {
	    if (sizeof(call = trace[i]) > TRACE_FIRSTARG &&
		owner != creator(call[TRACE_PROGNAME])) {
		/* remove arguments */
		trace[i] = call[.. TRACE_FIRSTARG - 1];
	    }
	}
    }

    return trace;
}

/*
 * NAME:	status()
 * DESCRIPTION:	get information about an object
 */
varargs mixed *status(mixed obj)
{
    mixed *status, **callouts, *co;
    int i;

    if (!obj) {
	return ::status();
    }

    if (typeof(obj) == T_STRING) {
	/* get corresponding object */
	obj = ::find_object(reduce_path(obj, object_name(this_object()),
					creator));
    }
    if (typeof(obj) != T_OBJECT) {
	BADARG(1, status);
    }

    status = ::status(obj);
    callouts = status[O_CALLOUTS];
    if ((i=sizeof(callouts)) != 0) {
	if (creator != "System" && owner != obj->query_owner()) {
	    /* remove arguments from callouts */
	    do {
		--i;
		co = callouts[i];
		callouts[i] = ({ co[CO_HANDLE], co[CO_FIRSTXARG],
				 co[CO_DELAY] });
	    } while (i != 0);
	} else {
	    do {
		--i;
		co = callouts[i];
		callouts[i] = ({ co[CO_HANDLE], co[CO_FIRSTXARG],
				 co[CO_DELAY] }) + co[CO_FIRSTXARG + 1 ..];
	    } while (i != 0);
	}
    }
    return status;
}

/*
 * NAME:	this_user()
 * DESCRIPTION:	return the user object and not the connection object
 */
static object this_user()
{
    return (this_user()) ? this_user()->query_user() : 0;
}

/*
 * NAME:	users()
 * DESCRIPTION:	return an array with the current user objects
 */
static object *users()
{
    return ::find_object(USERD)->query_users();
}

static string query_ip_number(object user)
{
    if (!user || function_object("query_conn", user) != CONNECTION) {
	BADARG(1, query_ip_number());
    }
    user = user->query_conn();
    if (!user) {
	BADARG(1, query_ip_number());
    }

    return ::query_ip_number(user);
}

/*
 * NAME:	dump_state()
 * DESCRIPTION:	create state dump
 */
static dump_state()
{
    if (SYSTEM()) {
	::find_object(DRIVER)->prepare_statedump();
	::dump_state();
    }
}

/*
 * NAME:	shutdown()
 * DESCRIPTION:	shutdown the system
 */
static shutdown()
{
    if (SYSTEM()) {
	::shutdown();
    }
}


/*
 * NAME:	_F_call_limited()
 * DESCRIPTION:	call a function with limited stack depth and ticks
 */
nomask _F_call_limited(mixed what, string function, mixed *args)
{
    if (previous_program() == AUTO) {
	object obj, rsrcd;
	int *status, depth, rdepth, ticks, rticks;
	string prev;

	if (owner == what) {
	    call_other(this_object(), function, args...);
	    return;
	}

	status = ::status();
	rlimits (-1; -1) {
	    rsrcd = ::find_object(RSRCD);

	    /* determine available stack */
	    depth = status[ST_STACKDEPTH];
	    rdepth = rsrcd->rsrc_get(owner, "stackdepth")[RSRC_MAX];
	    if (rdepth > depth && depth >= 0) {
		rdepth = depth;
	    }

	    /* determine available ticks */
	    ticks = status[ST_TICKS];
	    rticks = rsrcd->rsrc_get(owner, "ticks")[RSRC_MAX];
	    if (rticks >= 0) {
		int *rusage, max;

		rusage = rsrcd->rsrc_get(owner, "tick usage");
		max = rusage[RSRC_MAX];
		if (max >= 0 && rusage[RSRC_USAGE] >= max >> 1) {
		    rticks = rticks * (max - rusage[RSRC_USAGE]) / (max >> 1);
		}
		if (rticks > ticks - 25 && ticks >= 0) {
		    rticks = ticks - 25;
		}
		if (rticks <= 0) {
		    rticks = 1;
		}
	    }

	    prev = what;
	    what = ({ owner, ticks, rticks });
	}

	rlimits (rdepth; rticks) {
	    call_other(this_object(), function, args...);

	    status = ::status();
	    rlimits (-1; -1) {
		if (rticks >= 0 && prev != owner) {
		    rticks = what[2] - status[ST_TICKS];
		    if (ticks > 0) {
			rsrcd->rsrc_incr(prev, "tick usage", 0, -rticks);
		    }
		    rsrcd->rsrc_incr(owner, "tick usage", 0, rticks, 1);
		}
		return;
	    }
	}
    }
}

/*
 * NAME:	call_out()
 * DESCRIPTION:	start a callout
 */
static varargs int call_out(string function, int delay, mixed args...)
{
    object rsrcd;
    int handle;

    if (!function || function_object(function, this_object()) == AUTO) {
	BADARG(1, call_out);
    }

    rsrcd = ::find_object(RSRCD);
    catch {
	rlimits (-1; -1) {
	    handle = ::call_out("_F_callout", delay, function, args...);
	    if (rsrcd->rsrc_incr(owner, "callouts", this_object(), 1)) {
		return handle;
	    }
	    ::remove_call_out(handle);
	    error("Too many callouts");
	}
    } : error("");
}

/*
 * NAME:	remove_call_out()
 * DESCRIPTION:	remove a callout
 */
static int remove_call_out(int handle)
{
    rlimits (-1; -1) {
	handle = ::remove_call_out(handle);
	if (handle >= 0) {
	    ::find_object(RSRCD)->rsrc_incr(owner, "callouts", this_object(),
					    -1);
	}
	return handle;
    }
}

/*
 * NAME:	_F_callout()
 * DESCRIPTION:	callout gate
 */
nomask varargs _F_callout(string function, mixed args...)
{
    if (!previous_program()) {
	::find_object(RSRCD)->rsrc_incr(owner, "callouts", this_object(), -1);
	_F_call_limited(owner, function, args);
    }
}

/*
 * NAME:	add_event()
 * DESCRIPTION:	add a new event type
 */
static add_event(string name)
{
    if (!name) {
	BADARG(1, new_event);
    }

    if (!events) {
	events = ([ ]);
    }
    if (!events[name]) {
	events[name] = ({ });
    }
}

/*
 * NAME:	remove_event()
 * DESCRIPTION:	remove an event type
 */
static remove_event(string name)
{
    object *objlist, rsrcd;
    int i;

    if (!name) {
	BADARG(1, remove_event);
    }

    if (events && (objlist=events[name])) {
	rsrcd = ::find_object(RSRCD);
	i = sizeof(objlist -= ({ 0 }));
	rlimits (-1; -1) {
	    while (--i >= 0) {
		rsrcd->rsrc_incr(objlist[i]->query_owner(), "events",
				 objlist[i], -1);
	    }
	    events[name] = 0;
	}
    }
}

/*
 * NAME:	_F_subscribe_event()
 * DESCRIPTION:	subscribe to an event
 */
nomask _F_subscribe_event(string name, int subscribe)
{
    if (KERNEL()) {
	object *objlist, obj, rsrcd;

	if (!events || !(objlist=events[name])) {
	    error("No such event");
	}

	obj = previous_object();
	rsrcd = ::find_object(RSRCD);
	if (subscribe) {
	    if (sizeof(objlist & ({ obj }))) {
		error("Already subscribed to event");
	    }
	    objlist = objlist - ({ 0 }) + ({ obj });
	    catch {
		rlimits (-1; -1) {
		    if (!rsrcd->rsrc_incr(obj->query_owner(), "events", obj, 1))
		    {
			error("Too many events");
		    }
		    events[name] = objlist;
		}
	    } : error("");
	} else {
	    if (!sizeof(objlist & ({ obj }))) {
		error("Not subscribed to event");
	    }
	    rlimits (-1; -1) {
		rsrcd->rsrc_incr(obj->query_owner(), "events", obj, -1);
		events[name] -= ({ 0, obj });
	    }
	}
    }
}

/*
 * NAME:	subscribe_event()
 * DESCRIPTION:	subscribe a function to an event
 */
static subscribe_event(object obj, string name)
{
    if (!obj) {
	BADARG(1, subscribe_event);
    }
    if (!name) {
	BADARG(2, subscribe_event);
    }

    if (!obj->allow_subscribe_event(this_object(), name) || !obj) {
	error("Cannot subscribe to event");
    }
    obj->_F_subscribe_event(name, 1);
}

/*
 * NAME:	unsubscribe_event()
 * DESCRIPTION:	unsubscribe an object from an event
 */
static unsubscribe_event(object obj, string name)
{
    if (!obj) {
	BADARG(1, unsubscribe_event);
    }
    if (!name) {
	BADARG(2, unsubscribe_event);
    }

    obj->_F_subscribe_event(name, 0);
}

/*
 * NAME:	event()
 * DESCRIPTION:	cause an event
 */
static varargs int event(string name, mixed args...)
{
    object *objlist;
    string *names;
    int i, sz, recipients;

    if (!name) {
	BADARG(1, call_event);
    }

    if (!events || !(objlist=events[name])) {
	error("No such event");
    }
    name = "evt_" + name;
    recipients = 0;
    for (i = 0, sz = sizeof(objlist); i < sz; i++) {
	if (objlist[i]) {
	    objlist[i]->_F_call_limited(owner, name, args);
	    recipients++;
	}
    }

    return recipients;
}


/*
 * NAME:	resolve_path()
 * DESCRIPTION:	resolve a file path
 */
static string resolve_path(string path, string dir, string creator)
{
    if (!path) {
	BADARG(1, resolve_path);
    }
    if (!dir) {
	BADARG(2, resolve_path);
    }
    if (!creator) {
	BADARG(3, resolve_path);
    }
    return reduce_path(path, dir, creator);
}

/*
 * NAME:	read_file()
 * DESCRIPTION:	read a string from a file
 */
static varargs string read_file(string path, int offset, int size)
{
    string oname;

    if (!path) {
	BADARG(1, read_file);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, READ_ACCESS)) {
	error("Access denied");
    }
    return ::read_file(path, offset, size);
}

/*
 * NAME:	write_file()
 * DESCRIPTION:	write a string to a file
 */
static varargs int write_file(string path, string str, int offset)
{
    string oname, fcreator;
    object rsrcd;
    int *rsrc, size, result;

    if (!path) {
	BADARG(1, write_file);
    }
    if (!str) {
	BADARG(2, write_file);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS)) {
	error("Access denied");
    }
    fcreator = creator(path);
    rsrcd = ::find_object(RSRCD);
    if (creator != "System") {
	rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }
    size = file_size(path);
    catch {
	rlimits (-1; -1) {
	    result = ::write_file(path, str, offset);
	    if (result != 0 && (size=file_size(path) - size) != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", 0, size, 1);
	    }
	}
    } : error("");

    return result;
}

/*
 * NAME:	remove_file()
 * DESCRIPTION:	remove a file
 */
static int remove_file(string path)
{
    string oname;
    int size, result;

    if (!path) {
	BADARG(1, remove_file);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS)) {
	error("Access denied");
    }
    size = file_size(path);
    catch {
	rlimits (-1; -1) {
	    result = ::remove_file(path);
	    if (result != 0 && size != 0) {
		::find_object(RSRCD)->rsrc_incr(creator(path), "filequota", 0,
						-size);
	    }
	}
    } : error("");
    return result;
}

/*
 * NAME:	rename_file()
 * DESCRIPTION:	rename a file
 */
static int rename_file(string from, string to)
{
    string oname, fcreator, tcreator;
    object accessd, rsrcd;
    int size, *rsrc, result;

    if (!from) {
	BADARG(1, rename_file);
    }
    if (!to) {
	BADARG(2, rename_file);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    from = reduce_path(from, oname, creator);
    to = reduce_path(to, oname, creator);
    accessd = ::find_object(ACCESSD);
    if (creator != "System" &&
	(!accessd->access(oname, from, WRITE_ACCESS) ||
	 !accessd->access(oname, to, WRITE_ACCESS))) {
	error("Access denied");
    }
    fcreator = creator(from);
    tcreator = creator(to);
    size = file_size(from);
    rsrcd = ::find_object(RSRCD);
    if (creator != "System" && size != 0 && fcreator != tcreator) {
	rsrc = rsrcd->rsrc_get(tcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }
    catch {
	rlimits (-1; -1) {
	    result = ::rename_file(from, to);
	    if (result != 0 && fcreator != tcreator) {
		rsrcd->rsrc_incr(fcreator, "filequota", 0, -size);
		rsrcd->rsrc_incr(tcreator, "filequota", 0, size, 1);
	    }
	}
    } : error("");
    return result;
}

/*
 * NAME:	get_dir()
 * DESCRIPTION:	get a directory listing
 */
static mixed **get_dir(string path)
{
    string oname, *names, dir;
    mixed **list, *olist;
    int i, sz, len;

    if (!path) {
	BADARG(1, get_dir);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, READ_ACCESS)) {
	error("Access denied");
    }
    list = ::get_dir(path);

    names = explode(path, "/");
    dir = implode(names[.. sizeof(names) - 2], "/");
    names = list[0];
    olist = allocate(sz = sizeof(names));
    if (sscanf(path, "%*s/lib/") != 0) {
	/* lib objects */
	for (i = 0; i < sz; i++) {
	    path = dir + "/" + names[i];
	    if ((len=strlen(path)) >= 2 && path[len - 2 ..] == ".c" &&
		::find_object(path[.. len - 3])) {
		olist[i] = 1;
	    }
	}
    } else {
	/* ordinary objects */
	for (i = 0; i < sz; i++) {
	    object obj;

	    path = dir + "/" + names[i];
	    if ((len=strlen(path)) >= 2 && path[len - 2 ..] == ".c" &&
		(obj=::find_object(path[.. len - 3]))) {
		olist[i] = obj;
	    }
	}
    }
    return list + ({ olist });
}

/*
 * NAME:	make_dir()
 * DESCRIPTION:	create a directory
 */
static int make_dir(string path)
{
    string oname, fcreator;
    object rsrcd;
    int *rsrc, result;

    if (!path) {
	BADARG(1, make_dir);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS)) {
	error("Access denied");
    }
    fcreator = creator(path);
    rsrcd = ::find_object(RSRCD);
    if (creator != "System") {
	rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }
    catch {
	rlimits (-1; -1) {
	    result = ::make_dir(path);
	    if (result != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", 0, 1, 1);
	    }
	}
    } : error("");
    return result;
}

/*
 * NAME:	remove_dir()
 * DESCRIPTION:	remove a directory
 */
static int remove_dir(string path)
{
    string oname;
    int result;

    if (!path) {
	BADARG(1, remove_dir);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS)) {
	error("Access denied");
    }
    catch {
	rlimits (-1; -1) {
	    result = ::remove_dir(path);
	    if (result != 0) {
		::find_object(RSRCD)->rsrc_incr(creator(path), "filequota", 0,
						-1);
	    }
	}
    } : error("");
    return result;
}

/*
 * NAME:	restore_object()
 * DESCRIPTION:	restore the state of an object
 */
static int restore_object(string path)
{
    string oname;

    if (!path) {
	BADARG(1, restore_object);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, READ_ACCESS)) {
	error("Access denied");
    }
    return ::restore_object(path);
}

/*
 * NAME:	save_object()
 * DESCRIPTION:	save the state of an object
 */
static save_object(string path)
{
    string oname, fcreator;
    object rsrcd;
    int size, *rsrc;

    if (!path) {
	BADARG(1, save_object);
    }

    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS)) {
	error("Access denied");
    }
    fcreator = creator(path);
    rsrcd = ::find_object(RSRCD);
    if (creator != "System") {
	rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }
    size = file_size(path);
    catch {
	rlimits (-1; -1) {
	    ::save_object(path);
	    size = file_size(path) - size;
	    if (size != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", 0, size, 1);
	    }
	}
    } : error("");
}

/*
 * NAME:	editor()
 * DESCRIPTION:	pass a command to the editor
 */
static string editor(string cmd)
{
    object rsrcd;
    string result;
    mixed *info;

    if (!cmd) {
	BADARG(1, editor);
    }

    catch {
	rlimits (-1; -1) {
	    rsrcd = ::find_object(RSRCD);
	    if (!query_editor(this_object()) &&
		!rsrcd->rsrc_incr(owner, "editors", this_object(), 1)) {
		error("Too many editors");
	    }
	    result = ::editor(cmd);
	    if (!query_editor(this_object())) {
		rsrcd->rsrc_incr(owner, "editors", this_object(), -1);
	    }
	    info = ::find_object(DRIVER)->query_wfile();
	    if (info) {
		rsrcd->rsrc_incr(creator(info[0]), "filequota", 0,
				 file_size(info[0]) - info[1], 1);
	    }
	}
    } : error("");
    return result;
}
