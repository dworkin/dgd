# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/access.h>
# include <kernel/rsrc.h>
# include <type.h>
# include <trace.h>

# define BADARG(n, func)	error("bad argument " + (n) + \
				      " for function " + #func)

private object prev, next;	/* previous and next in linked list */
private string creator, owner;	/* creator and owner of this object */
private mapping resources;	/* resources associated with this object */
private mapping events;		/* events for this object */

nomask _F_prev(object obj)	{ if (PRIV0()) prev = obj; }
nomask _F_next(object obj)	{ if (PRIV0()) next = obj; }
nomask object _Q_prev()		{ return prev; }
nomask object _Q_next()		{ return next; }

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
    if (PRIV0()) {
	if (!resources) {
	    resources = ([ rsrc : incr ]);
	} else {
	    resources[rsrc] += incr;
	}
    }
}

/*
 * NAME:	_Q_rsrcs()
 * DESCRIPTION:	get list of associated objects
 */
nomask mapping _Q_rsrcs()
{
    if (PRIV0()) {
	return resources;
    }
}


# include "file.c"	/* file functions */


/*
 * NAME:	_F_create()
 * DESCRIPTION:	kernel creator functon
 */
nomask _F_create()
{
    if (creator) {
	return;
    }
    rlimits (-1; -1) {
	string oname;

	/*
	 * set creator and owner
	 */
	oname = object_name(this_object());
	creator = creator(oname);
	if (!creator || sscanf(oname, "%s#", oname) != 0) {
	    owner = previous_object()->query_owner();
	    if (!creator) {
		creator = owner;
	    }
	} else {
	    owner = creator;
	}

	/*
	 * register object
	 */
	if (oname != OBJREGD && oname != RSRCD && oname != RSRCOBJ) {
	    ::find_object(OBJREGD)->link(this_object(), owner);
	}
    }
    /* call higher-level creator function */
    this_object()->create();
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
    string oname, lib;
    mapping rsrcs;

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
    oname = object_name(obj);
    if (((owner != obj->query_owner() ||
	  sscanf(oname, "/kernel/%s/%*s", lib) != 0) && !PRIV1()) ||
	(lib && lib != "lib")) {
	/*
	 * kernel objects cannot be destructed at all
	 */
	error("Cannot destruct object: not owner");
    }

    rlimits (-1; -1) {
	if (oname != OBJREGD && oname != RSRCD && oname != RSRCOBJ) {
	    ::find_object(OBJREGD)->unlink(obj, owner);
	}

	rsrcs = obj->_Q_rsrcs();
	if (rsrcs && map_sizeof(rsrcs) != 0) {
	    string *indices;
	    int *values, i;
	    object rsrcd;

	    /*
	     * decrease resources associated with object
	     */
	    indices = map_indices(rsrcs);
	    values = map_values(rsrcs);
	    rsrcd = ::find_object(RSRCD);
	    i = sizeof(indices);
	    while (--i >= 0) {
		rsrcd->rsrc_incr(owner, indices[i], this_object(), -values[i]);
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
    string str, uid;
    object rsrcd, obj;
    int *rsrc;

    if (!path) {
	BADARG(1, compile_object);
    }

    /*
     * check permission
     */
    str = object_name(this_object());
    sscanf(str, "%s#", str);
    path = reduce_path(path, str, creator);
    if (creator != "System" &&
	!::find_object(ACCESSD)->access(creator, path, READ_ACCESS)) {
	error("Access denied");
    }

    obj = ::find_object(path);
    if (!obj) {
	/*
	 * check resource usage
	 */
	uid = creator(path);
	if (!uid) {
	    uid = owner;
	}
	rsrcd = ::find_object(RSRCD);
	rsrc = rsrcd->rsrc_get(uid, "objects");
	if (objrsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("Too many objects");
	}
    }

    if (sscanf(path, "%*s/lib/") != 0) {
	/*
	 * library object
	 */
	rlimits (-1; -1) {
	    ::compile_object(path);
	    if (!obj) {
		/* new object */
		rsrcd->rsrc_incr(uid, "objects", path, 1, 1);
	    }
	}
	return 0;
    } else if (obj) {
	/*
	 * recompile of usable object
	 */
	rlimits (-1; -1) {
	    return ::compile_object(path);
	}
    } else {
	int *status;

	/*
	 * new usable object
	 */
	status = ::status();
	rlimits (-1; -1) {
	    object configd;

	    configd = ::find_object(CONFIGD);
	    if ((status[ST_STACKDEPTH] >= 0 &&
		 status[ST_STACKDEPTH] < configd->query_stack_depth()) ||
		(status[ST_TICKS] >= 0 &&
		 status[ST_TICKS] < configd->query_ticks())) {
		error("Insufficient stack or ticks to create object");
	    }
	    obj = ::compile_object(path);
	}
	call_other(obj, "???");	/* initialize & register */
	return obj;
    }
}

/*
 * NAME:	clone_object()
 * DESCRIPTION:	clone an object
 */
static varargs object clone_object(string path, string oowner)
{
    string oname, str;
    object rsrcd, obj;
    int *rsrc, *status;

    if (!path) {
	BADARG(1, clone_object);
    }
    if (owner != "System" || !oowner) {
	oowner = owner;
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

    rsrcd = ::find_object(RSRCD);
    obj = ::find_object(path);
    if (!obj) {
	/* master object not compiled yet */
	str = creator(path);
	if (path != RSRCOBJ) {
	    rsrc = rsrcd->rsrc_get(str, "objects");
	    if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
		error("Too many objects");
	    }
	}
	rlimits (-1; -1) {
	    obj = ::compile_object(path);
	    if (path != RSRCOBJ) {
		rsrcd->rsrc_incr(str, "objects", path, 1, 1);
	    }
	}
    }

    if (path != RSRCOBJ) {
	rsrc = rsrcd->rsrc_get(oowner, "objects");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("Too many objects");
	}
    }

    status = ::status();
    rlimits (-1; -1) {
	object configd;

	configd = ::find_object(CONFIGD);
	if ((status[ST_STACKDEPTH] >= 0 &&
	     status[ST_STACKDEPTH] < configd->query_stack_depth()) &&
	    (status[ST_TICKS] >= 0 &&
	     status[ST_TICKS] < configd->query_ticks())) {
	    error("Insufficient stack or ticks to create object");
	}
	if (oowner != owner) {
	    owner = "/" + oowner;
	}
    }
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
	    if ((call=sizeof(trace[i])) > TRACE_FIRSTARG &&
		owner != creator(call[TRACE_PROGRAM])) {
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
		callouts[i] = ({ co[CO_HANDLE], co[CO_FIRSTARG],
				 co[CO_DELAY] });
	    } while (i != 0);
	} else {
	    do {
		--i;
		co = callouts[i];
		callouts[i] = ({ co[CO_HANDLE], co[CO_FIRSTARG],
				 co[CO_DELAY] }) + co[CO_FIRSTXARG + 1 ..];
	    } while (i != 0);
	}
    }
    return status;
}

/*
 * NAME:	dump_state()
 * DESCRIPTION:	create state dump
 */
static dump_state()
{
    if (PRIV1()) {
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
    if (PRIV1()) {
	::shutdown();
    }
}


/*
 * NAME:	call_limited()
 * DESCRIPTION:	call a function with limited stack depth and ticks
 */
private mixed call_limited(mixed what, string function, mixed *args)
{
    object obj, rsrcd;
    int *status, stackdepth, rstack, ticks, rticks;
    mixed result;

    rlimits (-1; -1) {
	if (typeof(what) == T_STRING) {
	    what = find_object(reduce_path(what, oject_name(this_object()),
					   creator));
	}

	status = ::status();
	rsrcd = ::find_object(RSRCD);
	obj = what;
	what = obj->query_owner();

	/* determine available stack */
	stackdepth = status[ST_ASTACKDEPTH];
	rstack = rsrcd->rsrc_get(what, "stackdepth")[RSRC_MAX];
	if (rstack > stackdepth && stackdepth >= 0) {
	    rstack = stackdepth;
	}

	/* determine available ticks */
	ticks = status[ST_ATICKS];
	rticks = rsrcd->rsrc_get(what, "ticks")[RSRC_MAX];
	if (rticks >= 0) {
	    int *rusage, max;

	    rusage = rsrcd->rsrc_get(what, "tick usage");
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
    }

    rlimits (rstack; rticks) {
	result = call_other(o, function, args...);
    }

    rlimits (-1; -1) {
	if (rticks >= 0) {
	    status = ::status();
	    rsrcd->rsrc_incr(what, "tick usage", 0, foo[2] - status[ST_TICKS],
			     1);
	}
	return result;
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

    rsrcd = ::find_object(RSRCD);
    rlimits (-1; -1) {
	handle = ::call_out("_F_callout", delay, function, args...);
	if (!rsrcd->rsrc_incr(owner, "callouts", this_object(), 1)) {
	    ::remove_call_out(handle);
	    error("Too many callouts");
	}
	if (!rsrcd->rsrc_incr(owner, "callout starts", 0, 1)) {
	    rsrcd->rsrc_incr(owner, "callouts", this_object(), -1);
	    ::remove_call_out(handle);
	    error("Too many callouts");
	}
	return handle;
    }
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
    if (PRIV0()) {
	::find_object(RSRCD)->rsrc_incr(owner, "callouts", this_object(), -1);
	call_limited(this_object(), function, args);
    }
}

/*
 * NAME:	new_event()
 * DESCRIPTION:	add a new event type
 */
static new_event(string name)
{
    if (!name) {
	BADARG(1, new_event);
    }

    if (!events) {
	events = ([ ]);
    }
    if (!events[name]) {
	events[name] = ([ ]);
    }
}

/*
 * NAME:	remove_event()
 * DESCRIPTION:	remove an event type
 */
static remove_event(string name)
{
    mapping event;

    if (!string) {
	BADARG(1, remove_event);
    }

    if (events && (event=events[name])) {
	rlimits (-1; -1) {
	    object rsrcd, *indices;
	    int i, sz;

	    rsrcd = ::find_object(RSRCD);
	    indices = map_indices(event);
	    for (i = 0, sz = sizeof(indices); i < sz; i++) {
		rsrcd->rsrc_incr(indices[i]->query_owner(), "events",
				 indices[i], -1);
	    }
	    events[name] = 0;
	}
    }
}

/*
 * NAME:	_F_subscribe_event()
 * DESCRIPTION:	subscribe to an event
 */
nomask _F_subscribe_event(string name, string function)
{
    if (PRIV0()) {
	mapping event;
	object obj;

	if (!events || !(event=events[name])) {
	    error("No such event");
	}

	obj = previous_object();
	if (function) {
	    /* subscribe */
	    if (!event[obj] &&
		!rsrcd->incr_rsrc(obj->query_owner(), "events", obj, 1)) {
		error("Too many events");
	    }
	} else {
	    /* unsubscribe */
	    if (!event[obj]) {
		error("Not subscribed to event");
	    }
	    rsrcd->incr_rsrc(obj->query_owner(), "events", obj, -1);
	}
	event[obj] = function;
    }
}

/*
 * NAME:	subscribe_event()
 * DESCRIPTION:	subscribe a function to an event
 */
static subscribe_event(object obj, string name, string function)
{
    if (!obj) {
	BADARG(1, subscribe_event);
    }
    if (!name) {
	BADARG(2, subscribe_event);
    }
    if (!function || strlen(function) < 4 || function[0 .. 3] != "evt_") {
	BADARG(3, subscribe_event);
    }

    if (!obj->query_subscribe_event(obj) || !obj) {
	error("Cannot subscribe to event");
    }
    rlimits (-1; -1) {
	obj->_F_subscribe_event(name, function);
    }
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

    rlimits (-1; -1) {
	obj->_F_subscribe_event(name, 0);
    }
}

/*
 * NAME:	call_event()
 * DESCRIPTION:	cause an event
 */
static varargs int call_event(string name, mixed args...)
{
    mapping event;
    object *indices;
    string *values;
    int i, sz, recipients;

    if (!name) {
	BADARG(1, call_event);
    }

    if (!events || !(event=events[name])) {
	error("No such event");
    }
    indices = map_indices(event);
    values = map_values(event);
    recipients = 0;
    for (i = 0, sz = sizeof(event); i < sz; i++) {
	if (indices[i]) {
	    catch {
		call_limited(indices[i], values[i], name, args);
	    }
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
	rlimits (-1; -1) {
	    rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	}
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }
    size = file_size(path);
    rlimits (-1; -1) {
	result = ::write_file(path, str, offset);
	if (result != 0 && (size=file_size(path) - size) != 0) {
	    rsrcd->rsrc_incr(fcreator, "filequota", 0, size, 1);
	}
    }

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
    rlimits (-1; -1) {
	result = ::remove_file(path);
	if (result != 0 && size != 0) {
	    ::find_object(RSRCD)->rsrc_incr(creator(path), "filequota", 0,
					    -size);
	}
    }
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

    from = reduce_path(from, oname = object_name(this_object()), creator);
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
	rlimits (-1; -1) {
	    rsrc = rsrcd->rsrc_get(tcreator, "filequota");
	}
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }
    rlimits (-1; -1) {
	result = ::rename_file(from, to);
	if (result != 0 && fcreator != tcreator) {
	    rsrcd->rsrc_incr(fcreator, "filequota", 0, -size);
	    rsrcd->rsrc_incr(tcreator, "filequota", 0, size, 1);
	}
    }
    return result;
}

/*
 * NAME:	get_dir()
 * DESCRIPTION:	get a directory listing
 */
static mixed **get_dir(string path)
{
    string oname;

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
    return ::get_dir(path);
    /*
     * TODO: add 4th array with objects
     */
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
	rlimits (-1; -1) {
	    rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	}
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }
    rlimits (-1; -1) {
	result = ::make_dir(path);
	if (result != 0) {
	    rsrcd->rsrc_incr(fcreator, "filequota", 0, 1, 1);
	}
    }
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
    rlimits (-1; -1) {
	result = ::remove_dir(path);
	if (result != 0) {
	    ::find_object(RSRCD)->rsrc_incr(creator(path), "filequota", 0, -1);
	}
    }
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
	rlimits (-1; -1) {
	    rsrc = rsrcd->rsrc_get(fcreator, "filequota");
	}
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("File quota exceeded");
	}
    }
    size = file_size(path);
    rlimits (-1; -1) {
	::save_object(path);
	size = file_size(path) - size;
	if (size != 0) {
	    rsrcd->rsrc_incr(fcreator, "filequota", 0, size, 1);
	}
    }
}

/*
 * NAME:	editor()
 * DESCRIPTION:	pass a command to the editor
 */
static editor(string cmd)
{
    object rsrcd;
    mixed *info;

    if (!cmd) {
	BADARG(1, editor);
    }

    rlimits (-1; -1) {
	rsrcd = ::find_object(RSRCD);
	if (!query_editor(this_object()) &&
	    !rsrcd->rsrc_incr(owner, "editors", this_object(), 1)) {
	    error("Too many editors");
	}
	::editor(cmd);
	if (!query_editor(this_object())) {
	    rsrcd->rsrc_incr(owner, "editors", this_object(), -1);
	}
	info = ::find_object(DRIVER)->query_wfile();
	if (info) {
	    rsrcd->rsrc_incr(creator(info[0]), "filequota", 0,
			     file_size(info[0]) - info[1], 1);
	}
    }
}
