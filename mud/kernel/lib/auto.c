# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/access.h>
# include <kernel/rsrc.h>
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
    if (previous_program() == RSRCOBJ) {
	if (!resources) {
	    resources = ([ rsrc : incr ]);
	} else {
	    resources[rsrc] += incr;
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
	}
	/* call higher-level creator function */
	this_object()->create(clone);
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

	    if (resources["callouts"] != 0) {
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
	    rsrcd->rsrc_incr(owner, "objects", 0, -1);
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
	return 0;
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
	return 0;
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
    if ((sscanf(oname, "/kernel/%*s") != 0 && !lib &&
	 sscanf(object_name(this_object()), "/kernel/%*s") == 0) ||
	(creator != "System" && oowner && owner != oowner)) {
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
    int *rsrc, *status, lib, init;

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
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS)) {
	error("Access denied");
    }

    /*
     * check resource usage
     */
    rsrcd = ::find_object(RSRCD);
    rsrc = rsrcd->rsrc_get(uid = driver->creator(path), "objects");
    if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	error("Too many objects");
    }

    /*
     * do the compiling
     */
    lib = sscanf(path, "%*s/lib/");
    init = !(lib || ::find_object(path));
    if (init) {
	status = ::status();
    }
    catch {
	rlimits (-1; -1) {
	    if (init) {
		int stack, ticks;

		stack = status[ST_STACKDEPTH];
		ticks = status[ST_TICKS];
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
	    if (init) {
		rsrcd->rsrc_incr(uid, "objects", 0, 1, TRUE);
	    }
	    if (lib) {
		driver->compile_lib(path, uid);
	    } else {
		driver->compile(obj, uid);
	    }
	}
    } : error(driver->query_error());
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
    object driver, rsrcd, obj;
    int *rsrc, *status;

    CHECKARG(path, 1, "clone_object");
    if (uid) {
	CHECKARG(owner == "System", 1, "clone_object");
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
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if ((sscanf(path, "/kernel/%*s") != 0 &&
	 sscanf(oname, "/kernel/%*s") == 0) ||
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
    status = ::status();
    catch {
	rlimits (-1; -1) {
	    int stack, ticks;

	    stack = status[ST_STACKDEPTH];
	    ticks = status[ST_TICKS];
	    if ((stack >= 0 &&
		 stack - 2 < rsrcd->rsrc_get(uid, "create stack")[RSRC_MAX]) ||
		(ticks >= 0 &&
		 ticks < rsrcd->rsrc_get(uid, "create ticks")[RSRC_MAX])) {
		error("Insufficient stack or ticks to create object");
	    }
	    if (path != RSRCOBJ) {
		rsrcd->rsrc_incr(uid, "objects", 0, 1, TRUE);
	    }
	    if (uid != owner) {
		owner = "/" + uid;
	    }
	}
    } : error(driver->query_error());
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
varargs mixed *status(mixed obj)
{
    object driver;
    mixed *status, **callouts, *co;
    int i;

    if (!this_object()) {
	return 0;
    }
    if (!obj) {
	return ::status();
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
	    return 0;
	}
    }
    CHECKARG(typeof(obj) == T_OBJECT, 1, "status");

    status = ::status(obj);
    callouts = status[O_CALLOUTS];
    if ((i=sizeof(callouts)) != 0) {
	if (creator != "System" &&
	    (!owner || owner != driver->creator(object_name(obj)))) {
	    /* remove arguments from callouts */
	    do {
		--i;
		co = callouts[i];
		callouts[i] = ({ co[CO_HANDLE], co[CO_FIRSTXARG],
				 co[CO_FIRSTXARG + 1] ? 0 : co[CO_DELAY] });
	    } while (i != 0);
	} else {
	    do {
		--i;
		co = callouts[i];
		callouts[i] = ({ co[CO_HANDLE], co[CO_FIRSTXARG],
				 co[CO_FIRSTXARG + 1] ? 0 : co[CO_DELAY] }) +
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
    return (::this_user()) ? ::this_user()->query_user() : 0;
}

/*
 * NAME:	users()
 * DESCRIPTION:	return an array with the current user objects
 */
static object *users()
{
    if (!this_object()) {
	return 0;
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
nomask _F_call_limited(string function, mixed *args)
{
    if (previous_program() == AUTO) {
	object rsrcd;
	int *status;
	mixed *limits;

	status = ::status();
	rlimits (-1; -1) {
	    rsrcd = ::find_object(RSRCD);
	    limits = rsrcd->call_limits(owner, status);
	}

	rlimits (limits[2]; limits[3]) {
	    call_other(this_object(), function, args...);

	    status = ::status();
	    rlimits (-1; -1) {
		rsrcd->update_ticks(status[ST_TICKS]);
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

    CHECKARG(function, 1, "call_out");
    if (!this_object()) {
	return 0;
    }
    CHECKARG(function_object(function, this_object()) != AUTO, 1, "call_out");

    /*
     * add callout
     */
    rsrcd = ::find_object(RSRCD);
    if (this_object() == rsrcd) {
	return ::call_out(function, delay, function, FALSE, ({ }));
    }
    catch {
	rlimits (-1; -1) {
	    handle = ::call_out("_F_callout", delay, function, FALSE, args);
	    if (rsrcd->rsrc_incr(owner, "callouts", this_object(), 1)) {
		return handle;
	    }
	    ::remove_call_out(handle);
	    error("Too many callouts");
	}
    } : error(::find_object(DRIVER)->query_error());
}

/*
 * NAME:	remove_call_out()
 * DESCRIPTION:	remove a callout
 */
static int remove_call_out(int handle)
{
    rlimits (-1; -1) {
	int delay;

	if ((delay=::remove_call_out(handle)) >= 0 &&
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
nomask varargs _F_callout(string function, int suspended, mixed *args)
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
nomask _F_release(int handle)
{
    if (previous_program() == RSRCD) {
	int i;
	mixed **callouts;

	callouts = ::status(this_object())[O_CALLOUTS];
	::remove_call_out(handle);
	for (i = sizeof(callouts); callouts[--i][CO_HANDLE] != handle; ) ;
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
	    objlist = objlist - ({ 0 }) + ({ obj });
	    catch {
		rlimits (-1; -1) {
		    if (!rsrcd->rsrc_incr(oowner, "events", obj, 1)) {
			error("Too many events");
		    }
		    events[name] = objlist;
		}
	    } : error(::find_object(DRIVER)->query_error());
	} else {
	    if (sizeof(objlist & ({ obj })) == 0) {
		error("Not subscribed to event");
	    }
	    rlimits (-1; -1) {
		rsrcd->rsrc_incr(oowner, "events", obj, -1);
		events[name] -= ({ 0, obj });
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
    return objlist - ({ 0 });
}

/*
 * NAME:	event()
 * DESCRIPTION:	cause an event
 */
static varargs event(string name, mixed args...)
{
    object *objlist;
    string *names;
    int dest, i, sz;

    CHECKARG(name, 1, "event");
    if (!events || !(objlist=events[name])) {
	error("No such event");
    }
    if (!this_object()) {
	return;
    }

    name = "evt_" + name;
    dest = FALSE;
    for (i = 0, sz = sizeof(objlist); i < sz; i++) {
	if (objlist[i]) {
	    objlist[i]->_F_call_limited(name, args);
	} else {
	    dest = TRUE;
	}
    }

    if (dest && events[name]) {
	events[name] -= ({ 0 });
    }
}


/*
 * NAME:	read_file()
 * DESCRIPTION:	read a string from a file
 */
static varargs string read_file(string path, int offset, int size)
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
static varargs int write_file(string path, string str, int offset)
{
    string oname, fcreator;
    object driver, rsrcd;
    int *rsrc, size, result;

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
    rsrc = rsrcd->rsrc_get(fcreator, "filequota");
    if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	error("File quota exceeded");
    }

    size = driver->file_size(path);
    catch {
	rlimits (-1; -1) {
	    result = ::write_file(path, str, offset);
	    if (result != 0 && (size=driver->file_size(path) - size) != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", 0, size, TRUE);
	    }
	}
    } : error(driver->query_error());

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
						"filequota", 0, -size);
	    }
	}
    } : error(driver->query_error());
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
    int size, *rsrc, result;

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
    if (size != 0 && fcreator != tcreator) {
	rsrc = rsrcd->rsrc_get(tcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }

    catch {
	rlimits (-1; -1) {
	    result = ::rename_file(from, to);
	    if (result != 0 && fcreator != tcreator) {
		rsrcd->rsrc_incr(tcreator, "filequota", 0, size, TRUE);
		rsrcd->rsrc_incr(fcreator, "filequota", 0, -size);
	    }
	}
    } : error(driver->query_error());
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
	    if ((len=strlen(path)) >= 2 && path[len - 2 ..] == ".c" &&
		::find_object(path[.. len - 3])) {
		olist[i] = TRUE;
	    }
	}
    } else {
	/* ordinary objects */
	for (i = sz; --i >= 0; ) {
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
    object driver, rsrcd;
    int *rsrc, result;

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
    rsrc = rsrcd->rsrc_get(fcreator, "filequota");
    if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	error("File quota exceeded");
    }

    catch {
	rlimits (-1; -1) {
	    result = ::make_dir(path);
	    if (result != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", 0, 1, TRUE);
	    }
	}
    } : error(driver->query_error());
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
						"filequota", 0, -1);
	    }
	}
    } : error(driver->query_error());
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
    int size, *rsrc;

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
    rsrc = rsrcd->rsrc_get(fcreator, "filequota");
    if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	error("File quota exceeded");
    }

    size = driver->file_size(path);
    catch {
	rlimits (-1; -1) {
	    ::save_object(path);
	    if ((size=driver->file_size(path) - size) != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", 0, size, TRUE);
	    }
	}
    } : error(driver->query_error());
}

/*
 * NAME:	editor()
 * DESCRIPTION:	pass a command to the editor
 */
static varargs string editor(string cmd)
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
		rsrcd->rsrc_incr(driver->creator(info[0]), "filequota", 0,
				 driver->file_size(info[0]) - info[1], TRUE);
	    }
	}
    } : error(driver->query_error());
    return result;
}


# ifdef __ICHAT__
/*
 * NAME:	execute_program()
 * DESCRIPTION:	execute external program
 */
static execute_program(string cmdline)
{
    CHECKARG(cmdline, 1, "execute_program");

    if (creator == "System" && this_object()) {
	::execute_program(cmdline);
    }
}

/*
 * NAME:	gethostbyname()
 * DESCRIPTION:	get host ip number from host name, blocking the server during
 *		the call
 */
static string gethostbyname(string name)
{
    CHECKARG(name, 1, "name");

    if (creator == "System" && this_object()) {
	return ::gethostbyname(name);
    }
}

/*
 * NAME:	gethostbyaddr()
 * DESCRIPTION:	get host name from host ip number, blocking the server during
 *		the call
 */
static string gethostbyaddr(string addr)
{
    CHECKARG(addr, 1, "addr");

    if (creator == "System" && this_object()) {
	return ::gethostbyaddr(addr);
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
    CHECKARG(destination, 1, "connect");

    if (creator == "System" && this_object()) {
	::connect(destination, port);
    }
}

/*
 * NAME:	open_port()
 * DESCRIPTION:	open a port to listen on
 */
static open_port(string protocol, int port)
{
    CHECKARG(protocol, 1, "open_port");

    if (creator == "System" && this_object()) {
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
