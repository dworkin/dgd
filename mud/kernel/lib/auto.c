# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/rsrc.h>
# include <kernel/access.h>
# include <kernel/user.h>
# include <status.h>
# include <type.h>
# include <trace.h>

# define LONG_TIME		(365 * 24 * 60 * 60)
# define CHECKARG(arg, n, func)	if (!(arg)) badarg((n), (func))

/*
 * NAME:	badarg()
 * DESCRIPTION:	called when an argument check failed
 */
private badarg(int n, string func)
{
    error("Bad argument " + n + " for function " + func);
}


private object prev, next;	/* previous and next in linked list */
private string creator, owner;	/* creator and owner of this object */
private mapping resources;	/* resources associated with this object */
private mapping events;		/* events for this object */

nomask _F_prev(object obj)  { if (previous_program() == OBJREGD) prev = obj; }
nomask _F_next(object obj)  { if (previous_program() == OBJREGD) next = obj; }
nomask object _Q_prev()	    { if (previous_program() == OBJREGD) return prev; }
nomask object _Q_next()	    { if (previous_program() == OBJREGD) return next; }

/*
 * NAME:	query_owner()
 * DESCRIPTION:	query the owner of an object
 */
nomask string query_owner()
{
    if (owner && owner[0] == '/') {
	string str;

	/*
	 * temporary owner for cloned object
	 */
	sscanf(owner, "/%s/%s", str, owner);
	return str;
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
    if (previous_program() == RSRCOBJ) {
	if (!resources) {
	    resources = ([ rsrc : incr ]);
	} else if (!resources[rsrc]) {
	    resources[rsrc] = incr;
	} else if (!(resources[rsrc] += incr)) {
	    resources[rsrc] = nil;
	}
    }
}

/*
 * NAME:	_F_create()
 * DESCRIPTION:	kernel creator function
 */
nomask _F_create()
{
    if (!prev) {
	string oname;
# ifdef CREATOR
	string cname;
# endif
	object driver;
	int clone;

	rlimits (-1; -1) {
	    /*
	     * set creator and owner
	     */
	    oname = object_name(this_object());
	    driver = ::find_object(DRIVER);
	    creator = driver->creator(oname);
	    clone = sscanf(oname, "%*s#");
	    if (clone) {
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

	    if (clone) {
		driver->clone(this_object(), owner);
	    }
# ifdef CREATOR
	    cname = function_object(CREATOR, this_object());
	    if (cname && sscanf(cname, USR + "/System/%*s") != 0) {
		/* extra initialisation function */
		call_other(this_object(), CREATOR, clone);
	    }
# endif
	}
	/* call higher-level creator function */
	if (sscanf(oname, "%*s/obj/") == 0) {
	    this_object()->create();
	} else {
	    this_object()->create(clone);
	}
    }
}

/*
 * NAME:	_F_destruct()
 * DESCRIPTION:	prepare object for being destructed
 */
nomask _F_destruct()
{
    if (previous_program() == AUTO) {
	object rsrcd;
	int i, j;

	::find_object(OBJREGD)->unlink(this_object(), owner);

	rsrcd = ::find_object(RSRCD);

	if (events) {
	    object **evtlist, *objlist, obj;

	    /*
	     * decrease resources of other objects subscribed to events
	     */
	    evtlist = map_values(events);
	    i = sizeof(evtlist);
	    while (--i >= 0) {
		j = sizeof(objlist = evtlist[i] - ({ nil }));
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

	    if (resources["callouts"]) {
		/*
		 * remove callouts
		 */
		rsrcd->remove_callouts(this_object(), owner,
				       resources["callouts"]);
	    }

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
	    /*
	     * non-clones are handled by driver->remove_program()
	     */
	    rsrcd->rsrc_incr(owner, "objects", nil, -1);
	}
    }
}


/*
 * NAME:	find_object()
 * DESCRIPTION:	find an object
 */
static object find_object(string path)
{
    CHECKARG(path, 1, "find_object");
    if (!this_object()) {
	return nil;
    }

    path = ::find_object(DRIVER)->normalize_path(path,
						 object_name(this_object()) +
						 "/..",
						 creator);
    if (sscanf(path, "%*s/lib/") != 0) {
	/*
	 * It is not possible to find a lib object by name, or to call a
	 * function in it.
	 */
	return nil;
    }
    return ::find_object(path);
}

/*
 * NAME:	destruct_object()
 * DESCRIPTION:	destruct an object
 */
static int destruct_object(mixed obj)
{
    object driver;
    string oname, oowner;
    int lib;

    /* check and translate argument */
    driver = ::find_object(DRIVER);
    if (typeof(obj) == T_STRING) {
	if (!this_object()) {
	    return FALSE;
	}
	obj = ::find_object(driver->normalize_path(obj,
						   object_name(this_object()) +
						   "/..",
						   creator));
	if (!obj) {
	    return FALSE;
	}
    } else {
	CHECKARG(typeof(obj) == T_OBJECT, 1, "destruct_object");
	if (!this_object()) {
	    return FALSE;
	}
    }

    /*
     * check privileges
     */
    oname = object_name(obj);
    lib = sscanf(oname, "%*s/lib/");
    oowner = (lib) ? driver->creator(oname) : obj->query_owner();
    if ((sscanf(oname, "/kernel/%*s") != 0 && !lib && !KERNEL()) ||
	(creator != "System" && owner != oowner)) {
	error("Cannot destruct object: not owner");
    }

    rlimits (-1; -1) {
	if (!lib) {
	    driver->destruct(obj, oowner);
	    obj->_F_destruct();
	} else {
	    driver->destruct_lib(object_name(obj), oowner);
	}
	::destruct_object(obj);
    }
    return TRUE;
}

/*
 * NAME:	compile_object()
 * DESCRIPTION:	compile a master object
 */
static object compile_object(string path)
{
    string oname, uid;
    object driver, rsrcd, obj;
    int *rsrc, lib, new, stack, ticks;

    CHECKARG(path, 1, "compile_object");
    if (!this_object()) {
	error("Access denied");
    }

    /*
     * check permission; compiling requires write access
     */
    oname = object_name(this_object());
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    uid = driver->creator(path);
    if (uid && creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS)) {
	error("Access denied");
    }

    /*
     * check resource usage
     */
    rsrcd = ::find_object(RSRCD);
    rsrc = rsrcd->rsrc_get(uid, "objects");
    if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	error("Too many objects");
    }

    /*
     * do the compiling
     */
    lib = sscanf(path, "%*s/lib/");
    new = !::find_object(path);
    stack = ::status()[ST_STACKDEPTH];
    ticks = ::status()[ST_TICKS];
    rlimits (-1; -1) {
	catch {
	    if (new && !lib) {
		if ((stack >= 0 &&
		     stack - 2 < rsrcd->rsrc_get(uid,
						 "create stack")[RSRC_MAX]) ||
		    (ticks >= 0 &&
		     ticks < rsrcd->rsrc_get(uid, "create ticks")[RSRC_MAX])) {
		    error("Insufficient stack or ticks to create object");
		}
	    }
	    driver->compiling(path);
	    obj = ::compile_object(path);
	    if (new) {
		rsrcd->rsrc_incr(uid, "objects", nil, 1, TRUE);
	    }
	    if (lib) {
		driver->compile_lib(path, uid);
	    } else {
		driver->compile(obj, uid);
	    }
	} : {
	    driver->compile_failed(path, uid);
	    rlimits (stack; ticks) {
		error(::call_trace()[1][TRACE_FIRSTARG][1]);
	    }
	}
    }
    if (new && !lib) {
	call_other(obj, "???");	/* initialize & register */
    }

    return (lib) ? nil : obj;
}

/*
 * NAME:	clone_object()
 * DESCRIPTION:	clone an object
 */
static object clone_object(string path, varargs string uid)
{
    string oname, str;
    object rsrcd, obj;
    int *rsrc, stack, ticks;

    CHECKARG(path, 1, "clone_object");
    if (uid) {
	CHECKARG(creator == "System", 1, "clone_object");
    } else {
	uid = owner;
    }
    if (!this_object()) {
	error("Access denied");
    }

    /*
     * check permissions
     */
    oname = object_name(this_object());
    path = ::find_object(DRIVER)->normalize_path(path, oname + "/..", creator);
    if ((sscanf(path, "/kernel/%*s") != 0 && !KERNEL()) ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, READ_ACCESS))) {
	/*
	 * kernel objects can only be cloned by kernel objects, and cloning
	 * in general requires read access
	 */
	error("Access denied");
    }

    /*
     * check if object can be cloned
     */
    if (!owner || !(obj=::find_object(path)) || sscanf(path, "%*s/obj/") == 0 ||
	sscanf(path, "%*s/lib/") != 0) {
	/*
	 * no owner for clone, master object not compiled, or not path of
	 * clonable
	 */
	error("Cannot clone " + path);
    }

    /*
     * check resource usage
     */
    rsrcd = ::find_object(RSRCD);
    if (path != RSRCOBJ) {
	rsrc = rsrcd->rsrc_get(uid, "objects");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("Too many objects");
	}
    }

    /*
     * do the cloning
     */
    stack = ::status()[ST_STACKDEPTH];
    ticks = ::status()[ST_TICKS];
    catch {
	rlimits (-1; -1) {
	    if ((stack >= 0 &&
		 stack - 2 < rsrcd->rsrc_get(uid, "create stack")[RSRC_MAX]) ||
		(ticks >= 0 &&
		 ticks < rsrcd->rsrc_get(uid, "create ticks")[RSRC_MAX])) {
		error("Insufficient stack or ticks to create object");
	    }
	    if (path != RSRCOBJ) {
		rsrcd->rsrc_incr(uid, "objects", nil, 1, TRUE);
	    }
	    if (uid != owner) {
		owner = "/" + uid + "/" + owner;
	    }
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
    return ::clone_object(obj);
}

/*
 * NAME:	call_trace()
 * DESCRIPTION:	call trace
 */
static mixed **call_trace()
{
    mixed **trace, *call;
    int i;
    object driver;

    trace = ::call_trace();
    if (creator != "System") {
	driver = ::find_object(DRIVER);
	for (i = sizeof(trace) - 1; --i >= 0; ) {
	    if (sizeof(call = trace[i]) > TRACE_FIRSTARG &&
		(!owner || owner != driver->creator(call[TRACE_PROGNAME]))) {
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
static mixed *status(varargs mixed obj)
{
    object driver;
    string oname;
    mixed *status, **callouts, *co;
    int i;

    if (!this_object()) {
	return nil;
    }
    if (!obj) {
	status = ::status();
	if (status[ST_STACKDEPTH] >= 0) {
	    status[ST_STACKDEPTH]++;
	}
	return status;
    }

    /*
     * check arguments
     */
    driver = ::find_object(DRIVER);
    if (typeof(obj) == T_STRING) {
	/* get corresponding object */
	obj = ::find_object(driver->normalize_path(obj,
						   object_name(this_object()) +
						   "/..",
						   creator));
	if (!obj) {
	    return nil;
	}
    }
    CHECKARG(typeof(obj) == T_OBJECT, 1, "status");

    status = ::status(obj);
    callouts = status[O_CALLOUTS];
    if ((i=sizeof(callouts)) != 0) {
	oname = object_name(obj);
	if (sscanf(oname, "/kernel/%*s") != 0) {
	    /* can't see callouts in kernel objects */
	    status[O_CALLOUTS] = ({ });
	} else if (creator != "System" &&
		   (!owner || owner != driver->creator(oname))) {
	    /* remove arguments from callouts */
	    do {
		--i;
		co = callouts[i];
		callouts[i] = ({ co[CO_HANDLE], co[CO_FIRSTXARG],
				 (co[CO_FIRSTXARG + 1]) ? 0 : co[CO_DELAY] });
	    } while (i != 0);
	} else {
	    do {
		--i;
		co = callouts[i];
		callouts[i] = ({ co[CO_HANDLE], co[CO_FIRSTXARG],
				 (co[CO_FIRSTXARG + 1]) ? 0 : co[CO_DELAY] }) +
			      co[CO_FIRSTXARG + 2];
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
    return (::this_user()) ? ::this_user()->query_user() : nil;
}

/*
 * NAME:	users()
 * DESCRIPTION:	return an array with the current user objects
 */
static object *users()
{
    if (!this_object()) {
	return nil;
    } else if (object_name(this_object()) == USERD) {
	/* connection objects */
	return ::users();
    } else {
	return ::find_object(USERD)->query_users();
    }
}

/*
 * NAME:	swapout()
 * DESCRIPTION:	swap out all objects
 */
static swapout()
{
    if (creator == "System" && this_object()) {
	::swapout();
    }
}

/*
 * NAME:	dump_state()
 * DESCRIPTION:	create state dump
 */
static dump_state()
{
    if (creator == "System" && this_object()) {
	rlimits (-1; -1) {
	    ::find_object(DRIVER)->prepare_reboot();
	    ::dump_state();
	}
    }
}

/*
 * NAME:	shutdown()
 * DESCRIPTION:	shutdown the system
 */
static shutdown()
{
    if (creator == "System" && this_object()) {
	::find_object(DRIVER)->message("System halted.\n");
	::shutdown();
    }
}


/*
 * NAME:	_F_call_limited()
 * DESCRIPTION:	call a function with limited stack depth and ticks
 */
nomask mixed _F_call_limited(mixed arg1, mixed *args)
{
    if (previous_program() == AUTO) {
	object rsrcd;
	int stack, ticks;
	string function;
	mixed tls, *limits, result;

	rsrcd = ::find_object(RSRCD);
	function = arg1;
	stack = ::status()[ST_STACKDEPTH];
	ticks = ::status()[ST_TICKS];
	rlimits (-1; -1) {
	    tls = ::call_trace()[1][TRACE_FIRSTARG];
	    if (tls == arg1) {
		tls = arg1 = allocate(TLS_SIZE);
	    }
	    limits = tls[0] = rsrcd->call_limits(tls[0], owner, stack, ticks);
	}

	rlimits (limits[LIM_MAXSTACK]; limits[LIM_MAXTICKS]) {
	    result = call_other(this_object(), function, args...);

	    ticks = ::status()[ST_TICKS];
	    rlimits (-1; -1) {
		rsrcd->update_ticks(limits, ticks);
		tls[0] = limits[LIM_NEXT];
	    }
	}

	return result;
    }
}

/*
 * NAME:	call_limited()
 * DESCRIPTION:	call a function with the current object owner's resource limits
 */
static mixed call_limited(string function, mixed args...)
{
    CHECKARG(function, 1, "call_limited");

    return _F_call_limited(function, args);
}

/*
 * NAME:	call_out()
 * DESCRIPTION:	start a callout
 */
static int call_out(string function, mixed delay, mixed args...)
{
    int handle;

    CHECKARG(function, 1, "call_out");
    handle = typeof(delay);
    CHECKARG(handle == T_INT || handle == T_FLOAT, 2, "call_out");
    if (!this_object()) {
	return 0;
    }
    CHECKARG(function_object(function, this_object()) != AUTO, 1, "call_out");

    /*
     * add callout
     */
    if (sscanf(object_name(this_object()), "/kernel/%*s/rsrc") != 0) {
	/* direct callouts for resource management objects */
	return ::call_out(function, delay, args...);
    }
    catch {
	rlimits (-1; -1) {
	    handle = ::call_out("_F_callout", delay, function, FALSE, args);
	    if (::find_object(RSRCD)->rsrc_incr(owner, "callouts",
						this_object(), 1)) {
		return handle;
	    }
	    ::remove_call_out(handle);
	    error("Too many callouts");
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
}

/*
 * NAME:	remove_call_out()
 * DESCRIPTION:	remove a callout
 */
static mixed remove_call_out(int handle)
{
    rlimits (-1; -1) {
	mixed delay;

	if ((delay=::remove_call_out(handle)) != -1 &&
	    ::find_object(RSRCD)->remove_callout(this_object(), owner, handle))
	{
	    return 0;
	}
	return delay;
    }
}

/*
 * NAME:	_F_callout()
 * DESCRIPTION:	callout gate
 */
nomask _F_callout(string function, int suspended, mixed *args)
{
    if (!previous_program()) {
	int handle;

	if (!suspended &&
	    !::find_object(RSRCD)->suspended(this_object(), owner)) {
	    _F_call_limited(function, args);
	} else {
	    handle = ::call_out("_F_callout", LONG_TIME, function, TRUE, args);
	    if (!suspended) {
		::find_object(RSRCD)->suspend(this_object(), owner, handle);
	    }
	}
    }
}

/*
 * NAME:	_F_release()
 * DESCRIPTION:	release a suspended callout
 */
nomask _F_release(mixed handle)
{
    if (previous_program() == RSRCD) {
	int i;
	mixed **callouts;

	callouts = ::status(this_object())[O_CALLOUTS];
	::remove_call_out(handle);
	for (i = sizeof(callouts); callouts[--i][CO_HANDLE] != handle; ) ;
	handle = allocate(TLS_SIZE);
	_F_call_limited(callouts[i][CO_FIRSTXARG],
			callouts[i][CO_FIRSTXARG + 2]);
    }
}

/*
 * NAME:	add_event()
 * DESCRIPTION:	add a new event type
 */
static add_event(string name)
{
    CHECKARG(name, 1, "add_event");

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

    CHECKARG(name, 1, "remove_event");

    if (events && (objlist=events[name])) {
	rsrcd = ::find_object(RSRCD);
	i = sizeof(objlist -= ({ nil }));
	rlimits (-1; -1) {
	    while (--i >= 0) {
		rsrcd->rsrc_incr(objlist[i]->query_owner(), "events",
				 objlist[i], -1);
	    }
	    events[name] = nil;
	}
    }
}

/*
 * NAME:	query_events()
 * DESCRIPTION:	return a list of existing events
 */
static string *query_events()
{
    if (events) {
	return map_indices(events);
    } else {
	return ({ });
    }
}

/*
 * NAME:	_F_subscribe_event()
 * DESCRIPTION:	subscribe to an event
 */
nomask _F_subscribe_event(object obj, string oowner, string name, int subscribe)
{
    if (previous_program() == AUTO) {
	object *objlist, rsrcd;

	if (!events || !(objlist=events[name])) {
	    error("No such event");
	}

	rsrcd = ::find_object(RSRCD);
	if (subscribe) {
	    if (sizeof(objlist & ({ obj })) != 0) {
		error("Already subscribed to event");
	    }
	    objlist = objlist - ({ nil }) + ({ obj });
	    catch {
		rlimits (-1; -1) {
		    if (!rsrcd->rsrc_incr(oowner, "events", obj, 1)) {
			error("Too many events");
		    }
		    events[name] = objlist;
		}
	    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
	} else {
	    if (sizeof(objlist & ({ obj })) == 0) {
		error("Not subscribed to event");
	    }
	    rlimits (-1; -1) {
		rsrcd->rsrc_incr(oowner, "events", obj, -1);
		events[name] -= ({ nil, obj });
	    }
	}
    }
}

/*
 * NAME:	subscribe_event()
 * DESCRIPTION:	subscribe to an event
 */
static subscribe_event(object obj, string name)
{
    CHECKARG(obj, 1, "subscribe_event");
    CHECKARG(name, 2, "subscribe_event");

    if (!obj->allow_subscribe(this_object(), name) || !obj) {
	error("Cannot subscribe to event");
    }
    obj->_F_subscribe_event(this_object(), owner, name, TRUE);
}

/*
 * NAME:	unsubscribe_event()
 * DESCRIPTION:	unsubscribe from an event
 */
static unsubscribe_event(object obj, string name)
{
    CHECKARG(obj, 1, "unsubscribe_event");
    CHECKARG(name, 2, "unsubscribe_event");

    obj->_F_subscribe_event(this_object(), owner, name, FALSE);
}

/*
 * NAME:	query_subscribed()
 * DESCRIPTION:	return a list of objects subscribed to an event
 */
static object *query_subscribed(string name)
{
    object *objlist;

    CHECKARG(name, 1, "query_subscribed");

    if (!events || !(objlist=events[name])) {
	error("No such event");
    }
    return objlist - ({ nil });
}

/*
 * NAME:	_F_event()
 * DESCRIPTION:	handle an event
 */
nomask _F_event(string name, mixed *args)
{
    if (!previous_program() && args[0]) {
	_F_call_limited(name, args);
    }
}

/*
 * NAME:	_F_start_event()
 * DESCRIPTION:	start an event in this object
 */
nomask _F_start_event(string name, mixed *args)
{
    if (previous_program() == AUTO) {
	::call_out("_F_event", 0, name, args);
    }
}

/*
 * NAME:	event()
 * DESCRIPTION:	cause an event
 */
static event(string name, mixed args...)
{
    object *objlist;
    string *names;
    int i, sz;

    CHECKARG(name, 1, "event");
    if (!events || !(objlist=events[name])) {
	error("No such event");
    }

    name = "evt_" + name;
    args = ({ this_object() }) + args;
    for (i = 0, sz = sizeof(objlist -= ({ nil })); i < sz; i++) {
	objlist[i]->_F_start_event(name, args);
    }
}


/*
 * NAME:	read_file()
 * DESCRIPTION:	read a string from a file
 */
static string read_file(string path, varargs int offset, int size)
{
    string oname;

    CHECKARG(path, 1, "read_file");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    path = ::find_object(DRIVER)->normalize_path(path, oname + "/..", creator);
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
static int write_file(string path, string str, varargs int offset)
{
    string oname, fcreator;
    object driver, rsrcd;
    int size, result;

    CHECKARG(path, 1, "write_file");
    CHECKARG(str, 2, "write_file");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if (sscanf(path, "/kernel/%*s") != 0 ||
	sscanf(path, "/include/kernel/%*s") != 0 ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS))) {
	error("Access denied");
    }

    fcreator = driver->creator(path);
    rsrcd = ::find_object(RSRCD);
    if (creator != "System") {
	int *rsrc;

	rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }

    size = driver->file_size(path);
    catch {
	rlimits (-1; -1) {
	    result = ::write_file(path, str, offset);
	    if (result != 0 && (size=driver->file_size(path) - size) != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", nil, size, TRUE);
	    }
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);

    return result;
}

/*
 * NAME:	remove_file()
 * DESCRIPTION:	remove a file
 */
static int remove_file(string path)
{
    string oname;
    object driver;
    int size, result;

    CHECKARG(path, 1, "remove_file");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if (sscanf(path, "/kernel/%*s") != 0 ||
	sscanf(path, "/include/kernel/%*s") != 0 ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS))) {
	error("Access denied");
    }

    size = driver->file_size(path);
    catch {
	rlimits (-1; -1) {
	    result = ::remove_file(path);
	    if (result != 0 && size != 0) {
		::find_object(RSRCD)->rsrc_incr(driver->creator(path),
						"filequota", nil, -size);
	    }
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
    return result;
}

/*
 * NAME:	rename_file()
 * DESCRIPTION:	rename a file
 */
static int rename_file(string from, string to)
{
    string oname, fcreator, tcreator;
    object driver, accessd, rsrcd;
    int size, result;

    CHECKARG(from, 1, "rename_file");
    CHECKARG(to, 2, "rename_file");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    driver = ::find_object(DRIVER);
    from = driver->normalize_path(from, oname + "/..", creator);
    to = driver->normalize_path(to, oname + "/..", creator);
    accessd = ::find_object(ACCESSD);
    if (sscanf(from + "/", "/kernel/%*s") != 0 ||
	sscanf(to, "/kernel/%*s") != 0 ||
	sscanf(from + "/", "/include/kernel/%*s") != 0 || from == "/include" ||
	sscanf(to, "/include/kernel/%*s") != 0 ||
	(creator != "System" &&
	 (!accessd->access(oname, from, WRITE_ACCESS) ||
	  !accessd->access(oname, to, WRITE_ACCESS)))) {
	error("Access denied");
    }

    fcreator = driver->creator(from);
    tcreator = driver->creator(to);
    size = driver->file_size(from, TRUE);
    rsrcd = ::find_object(RSRCD);
    if (size != 0 && fcreator != tcreator && creator != "System") {
	int *rsrc;

	rsrc = rsrcd->rsrc_get(tcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }

    catch {
	rlimits (-1; -1) {
	    result = ::rename_file(from, to);
	    if (result != 0 && fcreator != tcreator) {
		rsrcd->rsrc_incr(tcreator, "filequota", nil, size, TRUE);
		rsrcd->rsrc_incr(fcreator, "filequota", nil, -size);
	    }
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
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
    int i, sz;

    CHECKARG(path, 1, "get_dir");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    path = ::find_object(DRIVER)->normalize_path(path, oname + "/..", creator);
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
	for (i = sz; --i >= 0; ) {
	    path = dir + "/" + names[i];
	    if ((sz=strlen(path)) >= 2 && path[sz - 2 ..] == ".c" &&
		::find_object(path[.. sz - 3])) {
		olist[i] = TRUE;
	    }
	}
    } else {
	/* ordinary objects */
	for (i = sz; --i >= 0; ) {
	    object obj;

	    path = dir + "/" + names[i];
	    if ((sz=strlen(path)) >= 2 && path[sz - 2 ..] == ".c" &&
		(obj=::find_object(path[.. sz - 3]))) {
		olist[i] = obj;
	    }
	}
    }
    return list + ({ olist });
}

/*
 * NAME:	file_info()
 * DESCRIPTION:	get info for a single file
 */
static mixed *file_info(string path)
{
    string name, *files;
    mixed *info;
    int i, sz;
    object obj;

    CHECKARG(path, 1, "file_info");
    if (!this_object()) {
	error("Access denied");
    }

    name = object_name(this_object());
    path = ::find_object(DRIVER)->normalize_path(path, name + "/..", creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(name, path, READ_ACCESS)) {
	error("Access denied");
    }

    info = ::get_dir(path);
    files = explode(path, "/");
    name = files[sizeof(files) - 1];
    files = info[0];
    sz = sizeof(files);
    if (sz <= 1) {
	if (sz == 0 || files[0] != name) {
	    return nil;	/* file does not exist */
	}
    } else {
	/* name is a pattern: find in file list */
	for (i = 0; name != files[i]; ) {
	    if (++i == sz) {
		return nil;	/* file does not exist */
	    }
	}
    }
    info = ({ info[1][i], info[2][i], nil });
    if ((sz=strlen(path)) >= 2 && path[sz - 2 ..] == ".c" &&
	(obj=::find_object(path[.. sz - 3]))) {
	info[2] = (sscanf(path, "%*s/lib/") != 0) ? TRUE : obj;
    }
    return info;
}

/*
 * NAME:	make_dir()
 * DESCRIPTION:	create a directory
 */
static int make_dir(string path)
{
    string oname, fcreator;
    object driver, rsrcd;
    int result;

    CHECKARG(path, 1, "make_dir");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if (sscanf(path, "/kernel/%*s") != 0 ||
	sscanf(path, "/include/kernel/%*s") != 0 ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS))) {
	error("Access denied");
    }

    fcreator = driver->creator(path + "/");
    rsrcd = ::find_object(RSRCD);
    if (creator != "System") {
	int *rsrc;

	rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }

    catch {
	rlimits (-1; -1) {
	    result = ::make_dir(path);
	    if (result != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", nil, 1, TRUE);
	    }
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
    return result;
}

/*
 * NAME:	remove_dir()
 * DESCRIPTION:	remove a directory
 */
static int remove_dir(string path)
{
    string oname;
    object driver;
    int result;

    CHECKARG(path, 1, "remove_dir");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if (sscanf(path, "/kernel/%*s") != 0 ||
	sscanf(path, "/include/kernel/%*s") != 0 ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS))) {
	error("Access denied");
    }

    catch {
	rlimits (-1; -1) {
	    result = ::remove_dir(path);
	    if (result != 0) {
		::find_object(RSRCD)->rsrc_incr(driver->creator(path + "/"),
						"filequota", nil, -1);
	    }
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
    return result;
}

/*
 * NAME:	restore_object()
 * DESCRIPTION:	restore the state of an object
 */
static int restore_object(string path)
{
    string oname;

    CHECKARG(path, 1, "restore_object");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    path = ::find_object(DRIVER)->normalize_path(path, oname + "/..", creator);
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
    object driver, rsrcd;
    int size;

    CHECKARG(path, 1, "save_object");
    if (!this_object()) {
	error("Access denied");
    }

    oname = object_name(this_object());
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if ((sscanf(path, "/kernel/%*s") != 0 &&
	 sscanf(oname, "/kernel/%*s") == 0) ||
	sscanf(path, "/include/kernel/%*s") != 0 ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS))) {
	error("Access denied");
    }

    fcreator = driver->creator(path);
    rsrcd = ::find_object(RSRCD);
    if (creator != "System") {
	int *rsrc;

	rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }

    size = driver->file_size(path);
    catch {
	rlimits (-1; -1) {
	    ::save_object(path);
	    if ((size=driver->file_size(path) - size) != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", nil, size, TRUE);
	    }
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
}

/*
 * NAME:	editor()
 * DESCRIPTION:	pass a command to the editor
 */
static string editor(varargs string cmd)
{
    object rsrcd, driver;
    string result;
    mixed *info;

    if (creator != "System" || !this_object()) {
	error("Access denied");
    }

    catch {
	rlimits (-1; -1) {
	    rsrcd = ::find_object(RSRCD);
	    if (!query_editor(this_object()) &&
		!rsrcd->rsrc_incr(owner, "editors", this_object(), 1)) {
		error("Too many editors");
	    }
	    driver = ::find_object(DRIVER);

	    result = (cmd) ? ::editor(cmd) : ::editor();

	    if (!query_editor(this_object())) {
		rsrcd->rsrc_incr(owner, "editors", this_object(), -1);
	    }
	    info = driver->query_wfile();
	    if (info) {
		rsrcd->rsrc_incr(driver->creator(info[0]), "filequota", nil,
				 driver->file_size(info[0]) - info[1], TRUE);
	    }
	}
    } : error(::call_trace()[1][TRACE_FIRSTARG][1]);
    return result;
}


# ifdef __ICHAT__
/*
 * NAME:	execute_program()
 * DESCRIPTION:	execute external program
 */
static execute_program(string cmdline)
{
    object conn;
    int dedicated;

    CHECKARG(cmdline, 1, "execute_program");

    if (creator == "System" && this_object()) {
	if (function_object("query_conn", this_object()) != LIB_USER) {
	    error("Not a user object");
	}
	catch {
	    rlimits (-1; -1) {
		conn = this_object()->query_conn();
		if (!conn) {
		    conn = clone_object(BINARY_CONN);
		    dedicated = TRUE;
		} else {
		    dedicated = FALSE;
		}
		conn->execute_program(cmdline);
	    }
	} : {
	    rlimits (-1; -1) {
		if (dedicated) {
		    destruct_object(conn);
		}
	    }
	    error(::call_trace()[1][TRACE_FIRSTARG][1]);
	}
    }
}
# endif


# ifdef SYS_NETWORKING
/*
 * NAME:	connect()
 * DESCRIPTION:	open an outbound connection
 */
static connect(string destination, int port)
{
    object conn;

    CHECKARG(destination, 1, "connect");

    if (creator == "System" && this_object()) {
	if (function_object("query_conn", this_object()) != LIB_USER) {
	    error("Not a user object");
	}
	catch {
	    rlimits (-1; -1) {
		conn = clone_object(BINARY_CONN);
		conn->connect(destination, port);
	    }
	} : {
	    rlimits (-1; -1) {
		destruct_object(conn);
	    }
	    error(::call_trace()[1][TRACE_FIRSTARG][1]);
	}
    }
}

/*
 * NAME:	open_port()
 * DESCRIPTION:	open a port to listen on
 */
static open_port(string protocol, int port)
{
    CHECKARG(protocol, 1, "open_port");

    if (KERNEL() && this_object()) {
	::open_port(protocol, port);
    }
}

/*
 * NAME:	ports()
 * DESCRIPTION:	return list of open ports
 */
static object *ports()
{
    if (creator == "System" && this_object()) {
	return ::ports();
    }
}
# endif /* SYS_NETWORKING */
