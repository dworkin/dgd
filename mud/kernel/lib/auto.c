# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/access.h>
# include <kernel/rsrc.h>
# include <kernel/user.h>
# include <kernel/timer.h>
# include <config.h>
# include <type.h>
# include <trace.h>

# define CHECKARG(arg, n, func)	if (!(arg)) badarg((n), (func))
# define CHECKOBJ(func)		if (!this_object()) badobj((func))

/*
 * NAME:	badarg()
 * DESCRIPTION:	called when an argument check failed
 */
private badarg(int n, string func)
{
    error("Bad argument " + n + " for function " + func);
}

/*
 * NAME:	badobj()
 * DESCRIPTION:	called when the current object is destructed
 */
private badobj(string func)
{
    error("Function " + func + " called from destructed object");
}


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
	    creator = ::find_object(DRIVER)->creator(oname);
	    if (sscanf(oname, "%s#", oname) != 0) {
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
 * NAME:	_F_destruct()
 * DESCRIPTION:	prepare object for being destructed
 */
nomask _F_destruct()
{
    if (KERNEL()) {
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

	    if (resources["timers"]) {
		::find_object(TIMERD)->remove_timers(this_object());
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
    CHECKOBJ("find_object");
    CHECKARG(path, 1, "find_object");

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
 * DESCRIPTION:	destruct an object, if you can
 */
static destruct_object(mixed obj)
{
    object driver;
    string oname;
    int lib;

    CHECKOBJ("destruct_object");

    /* check and translate argument */
    driver = ::find_object(DRIVER);
    if (typeof(obj) == T_STRING) {
	obj = ::find_object(driver->normalize_path(obj,
						   object_name(this_object()) +
						   "/..",
						   creator));
    }
    CHECKARG(typeof(obj) == T_OBJECT, 1, "destruct_object");

    /*
     * check privileges
     */
    oname = object_name(obj);
    lib = sscanf(oname, "%*s/lib/");
    if ((sscanf(oname, "/kernel/%*s") != 0 && !lib &&
	 sscanf(object_name(this_object()), "/kernel/%*s") == 0) ||
	owner != ((lib) ? driver->creator(oname) : obj->query_owner())) {
	/*
	 * kernel:
	 *  - non-lib objects can only be destructed by kernel objects
	 * other: owner of this object must be equal to
	 *  - lib objects: creator of object
	 *  - other objects: owner of object
	 */
	error("Cannot destruct object: not owner");
    }

    rlimits (-1; -1) {
	if (!lib) {
	    obj->_F_destruct();
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
    object driver, rsrcd, obj;
    int *rsrc, *status, lib, init;

    CHECKOBJ("compile_object");
    CHECKARG(path, 1, "compile_object");

    /*
     * check permission; compiling requires write access
     */
    sscanf(oname = object_name(this_object()), "%s#", oname);
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
		     stack < rsrcd->rsrc_get(uid, "create stack")[RSRC_MAX]) ||
		    (ticks >= 0 &&
		     ticks < rsrcd->rsrc_get(uid, "create ticks")[RSRC_MAX])) {
		    error("Insufficient stack or ticks to create object");
		}
	    }
	    obj = ::compile_object(path);
	    rsrcd->rsrc_incr(uid, "objects", 0, 1, 1);
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

    CHECKOBJ("clone_object");
    CHECKARG(path, 1, "clone_object");
    if (uid) {
	CHECKARG(owner == "System", 1, "clone_object");
    } else {
	uid = owner;
    }

    /*
     * check permissions
     */
    sscanf(oname = object_name(this_object()), "%s#", oname);
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
    obj = ::find_object(path);
    if (!obj || sscanf(path, "%*s/obj/") == 0 || sscanf(path, "%*s/lib/") != 0)
    {
	/* master object not compiled, or not path of clonable */
	error("Cannot clone " + path);	/* not path of clonable */
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
		 stack < rsrcd->rsrc_get(uid, "create stack")[RSRC_MAX]) ||
		(ticks >= 0 &&
		 ticks < rsrcd->rsrc_get(uid, "create ticks")[RSRC_MAX])) {
		error("Insufficient stack or ticks to create object");
	    }
	    if (path != RSRCOBJ) {
		rsrcd->rsrc_incr(uid, "objects", 0, 1, 1);
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
    mixed **trace;
    int i, sz;
    mixed *call;
    object driver;

    trace = ::call_trace();
    trace = trace[.. sizeof(trace) - 2];	/* skip last */
    if (owner != "System") {
	driver = ::find_object(DRIVER);
	for (i = 0, sz = sizeof(trace); i < sz; i++) {
	    if (sizeof(call = trace[i]) > TRACE_FIRSTARG &&
		owner != driver->creator(call[TRACE_PROGNAME])) {
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
    string oname;
    object driver;
    mixed *status, **callouts, *co;
    int i;

    if (!obj) {
	return ::status();
    }

    /*
     * check arguments
     */
    CHECKOBJ("status");
    oname = object_name(this_object());
    driver = ::find_object(DRIVER);
    if (typeof(obj) == T_STRING) {
	/* get corresponding object */
	obj = ::find_object(driver->normalize_path(obj, oname + "/..",
						   creator));
    }
    CHECKARG(typeof(obj) == T_OBJECT, 1, "status");

    status = ::status(obj);
    callouts = status[O_CALLOUTS];
    if ((i=sizeof(callouts)) != 0 && oname != TIMERD) {
	if (owner != "System" && owner != driver->creator(obj)) {
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

/*
 * NAME:	dump_state()
 * DESCRIPTION:	create state dump
 */
static dump_state()
{
    if (creator == "System") {
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
    if (creator == "System") {
# ifdef SYS_CONTINUOUS
	::find_object(DRIVER)->prepare_statedump();
	::dump_state();
# endif
	::shutdown();
    }
}


/*
 * NAME:	_F_call_limited()
 * DESCRIPTION:	call a function with limited stack depth and ticks
 */
nomask _F_call_limited(mixed what, string function, mixed *args)
{
    if (KERNEL()) {
	object obj, rsrcd;
	int *status, stack, rstack, ticks, rticks;
	string prev;

	status = ::status();
	rlimits (-1; -1) {
	    rsrcd = ::find_object(RSRCD);

	    /* determine available stack */
	    stack = status[ST_STACKDEPTH];
	    rstack = rsrcd->rsrc_get(owner, "stack")[RSRC_MAX];
	    if (rstack > stack && stack >= 0) {
		rstack = stack;
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

	rlimits (rstack; rticks) {
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

    CHECKOBJ("call_out");
    CHECKARG(function && function_object(function, this_object()) != AUTO,
	     1, "call_out");

    /*
     * add callout
     */
    if (object_name(this_object()) == TIMERD) {
	return ::call_out(function, delay, args...);
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
    } : error(::find_object(DRIVER)->query_error());
}

/*
 * NAME:	remove_call_out()
 * DESCRIPTION:	remove a callout
 */
static int remove_call_out(int handle)
{
    rlimits (-1; -1) {
	handle = ::remove_call_out(handle);
	if (handle >= 0 && this_object() &&
	    object_name(this_object()) != TIMERD) {
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
    if ((!previous_program() || previous_program() == TIMERD) &&
	!::find_object(TIMERD)->suspended(function, args)) {
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
 * NAME:	_F_subscribe_event()
 * DESCRIPTION:	subscribe to an event
 */
nomask _F_subscribe_event(string name, string oowner, int subscribe)
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
		    if (!rsrcd->rsrc_incr(oowner, "events", obj, 1)) {
			error("Too many events");
		    }
		    events[name] = objlist;
		}
	    } : error(::find_object(DRIVER)->query_error());
	} else {
	    if (!sizeof(objlist & ({ obj }))) {
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
 * DESCRIPTION:	subscribe a function to an event
 */
static subscribe_event(object obj, string name)
{
    CHECKOBJ("subscribe_event");
    CHECKARG(obj, 1, "subscribe_event");
    CHECKARG(name, 2, "subscribe_event");

    if (!obj->allow_subscribe_event(this_object(), name) || !obj) {
	error("Cannot subscribe to event");
    }
    obj->_F_subscribe_event(name, owner, 1);
}

/*
 * NAME:	unsubscribe_event()
 * DESCRIPTION:	unsubscribe an object from an event
 */
static unsubscribe_event(object obj, string name)
{
    CHECKARG(obj, 1, "unsubscribe_event");
    CHECKARG(name, 2, "unsubscribe_event");

    obj->_F_subscribe_event(name, owner, 0);
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

    CHECKARG(name, 1, "event");
    if (!events || !(objlist=events[name])) {
	error("No such event");
    }
    if (!this_object()) {
	return 0;
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
 * NAME:	read_file()
 * DESCRIPTION:	read a string from a file
 */
static varargs string read_file(string path, int offset, int size)
{
    string oname;

    CHECKOBJ("read_file");
    CHECKARG(path, 1, "read_file");

    sscanf(oname = object_name(this_object()), "%s#", oname);
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

    CHECKOBJ("write_file");
    CHECKARG(path, 1, "write_file");
    CHECKARG(str, 2, "write_file");

    sscanf(oname = object_name(this_object()), "%s#", oname);
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if (sscanf(path, "/kernel/%*s", path) != 0 ||
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
		rsrcd->rsrc_incr(fcreator, "filequota", 0, size, 1);
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

    CHECKOBJ("remove_file");
    CHECKARG(path, 1, "remove_file");

    sscanf(oname = object_name(this_object()), "%s#", oname);
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if (sscanf(path, "/kernel/%*s") != 0 ||
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

    CHECKOBJ("rename_file");
    CHECKARG(from, 1, "rename_file");
    CHECKARG(to, 2, "rename_file");

    sscanf(oname = object_name(this_object()), "%s#", oname);
    driver = ::find_object(DRIVER);
    from = driver->normalize_path(from, oname + "/..", creator);
    to = driver->normalize_path(to, oname + "/..", creator);
    accessd = ::find_object(ACCESSD);
    if (sscanf(from, "/kernel/%*s") != 0 || sscanf(to, "/kernel/%*s") != 0 ||
	(creator != "System" &&
	 (!accessd->access(oname, from, WRITE_ACCESS) ||
	  !accessd->access(oname, to, WRITE_ACCESS)))) {
	error("Access denied");
    }

    fcreator = driver->creator(from);
    tcreator = driver->creator(to);
    size = driver->file_size(from, 1);
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
		rsrcd->rsrc_incr(fcreator, "filequota", 0, -size);
		rsrcd->rsrc_incr(tcreator, "filequota", 0, size, 1);
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

    CHECKOBJ("get_dir");
    CHECKARG(path, 1, "get_dir");

    sscanf(oname = object_name(this_object()), "%s#", oname);
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
    object driver, rsrcd;
    int *rsrc, result;

    CHECKOBJ("make_dir");
    CHECKARG(path, 1, "make_dir");

    sscanf(oname = object_name(this_object()), "%s#", oname);
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if (sscanf(path, "/kernel/%*s") != 0 ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS))) {
	error("Access denied");
    }

    fcreator = driver->creator(path);
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

    CHECKOBJ("remove_dir");
    CHECKARG(path, 1, "remove_dir");

    sscanf(oname = object_name(this_object()), "%s#", oname);
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if (sscanf(path, "/kernel/%*s") != 0 ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS))) {
	error("Access denied");
    }

    catch {
	rlimits (-1; -1) {
	    result = ::remove_dir(path);
	    if (result != 0) {
		::find_object(RSRCD)->rsrc_incr(driver->creator(path),
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

    CHECKOBJ("restore_object");
    CHECKARG(path, 1, "restore_object");

    sscanf(oname = object_name(this_object()), "%s#", oname);
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

    CHECKOBJ("save_object");
    CHECKARG(path, 1, "save_object");

    sscanf(oname = object_name(this_object()), "%s#", oname);
    driver = ::find_object(DRIVER);
    path = driver->normalize_path(path, oname + "/..", creator);
    if ((sscanf(path, "/kernel/%*s") != 0 &&
	 sscanf(path, "/kernel/data/%*s") == 0) ||
	(creator != "System" &&
	 !::find_object(ACCESSD)->access(oname, path, WRITE_ACCESS))) {
	error("Access denied");
    }

    fcreator = driver->creator(path);
    rsrcd = ::find_object(RSRCD);
    if (creator != "System") {
	rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }

    size = driver->file_size(path);
    catch {
	rlimits (-1; -1) {
	    ::save_object(path);
	    size = driver->file_size(path) - size;
	    if (size != 0) {
		rsrcd->rsrc_incr(fcreator, "filequota", 0, size, 1);
	    }
	}
    } : error(driver->query_error());
}

/*
 * NAME:	editor()
 * DESCRIPTION:	pass a command to the editor
 */
static string editor(string cmd)
{
    object rsrcd, driver;
    string result;
    mixed *info;

    CHECKOBJ("editor");
    CHECKARG(cmd, 1, "editor");

    catch {
	rlimits (-1; -1) {
	    driver = ::find_object(DRIVER);
	    rsrcd = ::find_object(RSRCD);
	    if (!query_editor(this_object()) &&
		!rsrcd->rsrc_incr(owner, "editors", this_object(), 1)) {
		error("Too many editors");
	    }

	    result = ::editor(cmd);

	    if (!query_editor(this_object())) {
		rsrcd->rsrc_incr(owner, "editors", this_object(), -1);
	    }
	    info = driver->query_wfile();
	    if (info) {
		rsrcd->rsrc_incr(driver->creator(info[0]), "filequota", 0,
				 driver->file_size(info[0]) - info[1], 1);
	    }
	}
    } : error(driver->query_error());
    return result;
}
