# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/access.h>
# include <kernel/rsrc.h>
# include <kernel/rlimits.h>
# include <type.h>

private object prev, next;		/* previous and next in linked list */
private string creator, owner;		/* creator and owner of this object */
private mapping resources;		/* resources associated with this */

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
	if (resources == 0) {
	    resources = ([ ]);
	}
	resources[rsrc] += incr;
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


/*
 * NAME:	creator()
 * DESCRIPTION:	get creator of file
 */
private string creator(string file)
{
    return (sscanf(file, "/usr/%s/", file) != 0) ?
	    file :
	    (sscanf(file, "/kernel/%*s") != 0) ? "System" : 0;
}

/*
 * NAME:	reduce_path()
 * DESCRIPTION:	reduce a path to its minimal absolute form
 */
private string reduce_path(string file, string dir)
{
    string *path;
    int i, j, sz;

    switch (file[0]) {
    case '/':
	/* absolute path */
	if (sscanf(file, "%*s//") == 0 &&
	    sscanf(file, "%*s/./") == 0 && sscanf(file, "%*s/../") == 0) {
	    return file;	/* no changes */
	}
	path = explode(file + "/", "/");
	break;

    case '~':
	/* ~path */
	if (strlen(file) == 1 || file[1] == '/') {
	    file = "/usr/" + creator + file[1 .. ];
	} else {
	    file = "/usr/" + file[1 .. ];
	}
	/* fall through */
    default:
	/* relative path */
	if (sscanf(file, "%*s//") == 0 &&
	    sscanf(file, "%*s/./") == 0 && sscanf(file, "%*s/../") == 0) {
	    /* simple relative path */
	    path = explode(dir, "/");
	    i = sizeof(path) - 1;
	    if (sscanf(file, "../%s", file) != 0 && --i < 0) {
		/* ../path */
		i = 0;
	    }
	    return "/" + implode(path[0 .. i - 1], "/") + "/" + file;
	}
	/* complex relative path */
	path = explode(dir + "/../" + file + "/", "/");
	break;
    }

    for (i = 0, j = 0, sz = sizeof(path); i < sz; i++) {
	switch (path[i]) {
	case "":
	    /* // */
	    if (i == sz - 1) {
		break;	/* path/ is a special case */
	    }
	    /* fall through */
	case ".":
	    /* /./ */
	    continue;

	case "..":
	    /* .. */
	    if (--j < 0) {
		j = 0;
	    }
	    continue;
	}
	path[j++] = path[i];
    }

    return "/" + implode(path[0 .. j - 1], "/");
}

/*
 * NAME:	dir_size()
 * DESCRIPTION:	get the size of all files in a directory
 */
private int dir_size(string file)
{
    mixed **info;
    int *sizes, size, i;

    info = ::get_dir(file + "/*");
    sizes = info[1];
    size = 1;		/* 1K for directory itself */
    i = sizeof(sizes);
    while (--i >= 0) {
	size += (sizes[i] < 0) ?
		 dir_size(file + "/" + info[0][i]) :
		 (sizes[i] + 1023) >> 10;
    }

    return size;
}

/*
 * NAME:	file_size()
 * DESCRIPTION:	get the size of a file in K, or 0 if the file doesn't exist
 */
private int file_size(string file)
{
    mixed **info;
    string *files, name;
    int i, sz;

    info = ::get_dir(file);
    files = explode(file, "/");
    name = files[sizeof(files) - 1];
    files = info[0];
    i = 0;
    sz = sizeof(files);

    if (sz <= 1) {
	if (sz == 0 || files[0] != name) {
	    return 0;	/* file does not exist */
	}
    } else {
	/* name is a pattern: find in file list */
	while (name != files[i]) {
	    if (++i == sz) {
		return 0;	/* file does not exist */
	    }
	}
    }

    i = info[1][i];
    return (i < 0) ? dir_size(file) : (i + 1023) >> 10;
}


/*
 * NAME:	_F_create()
 * DESCRIPTION:	kernel creator functon
 */
nomask _F_create()
{
    if (creator != 0) {
	return;
    }
    rlimits (-1; -1) {
	string oname;

	/*
	 * set creator and owner
	 */
	oname = object_name(this_object());
	creator = creator(oname);
	if (creator == 0 || sscanf(oname, "%s#", oname) != 0) {
	    owner = previous_object()->query_owner();
	    if (creator == 0) {
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
 * NAME:	destruct_object()
 * DESCRIPTION:	destruct an object, if you can
 */
static destruct_object(object obj)
{
    string oname;
    mapping rsrcs;

    if (obj == 0) {
	error("Bad argument 1 for function destruct_object");
    }

    /*
     * check privileges
     */
    oname = object_name(obj);
    if ((owner != obj->query_owner() || sscanf(oname, "/kernel/%*s") != 0) &&
	!PRIV1()) {
	/*
	 * The override check doesn't use creator, so destruct_object() in
	 * a system object is safe.
	 */
	error("Cannot destruct object: not owner");
    }

    rlimits (-1; -1) {
	if (oname != OBJREGD && oname != RSRCD && oname != RSRCOBJ) {
	    ::find_object(OBJREGD)->unlink(obj, owner);
	}

	rsrcs = obj->_Q_rsrcs();
	if (rsrcs != 0 && map_sizeof(rsrcs) != 0) {
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
 * NAME:	update_object()
 * DESCRIPTION:	destruct inherited object
 */
static int update_object(string path)
{
    string str;
    object obj;

    if (path == 0) {
	error("Bad argument 1 for function update_object");
    }

    /*
     * check if the object exists and is a lib object
     */
    path = reduce_path(path, object_name(this_object()));
    obj = ::find_object(path);
    str = path;
    while (sscanf(str, "%*s/lib/%s", str) != 0) ;
    if (str == path || sscanf(str, "%*s/") != 0 || obj == 0) {
	return 0;
    }

    /*
     * check permission
     */
    if (owner != creator(path) && !PRIV1()) {
	/*
	 * The override check doesn't use creator, so update_object() in
	 * a system object is safe.
	 */
	error("Cannot update object: not owner");
    }

    ::destruct_object(obj);
    return 1;
}

/*
 * NAME:	find_object()
 * DESCRIPTION:	find an object
 */
static object find_object(string path)
{
    string str;

    if (path == 0) {
	error("Bad argument 1 for function find_object");
    }

    path = reduce_path(path, object_name(this_object()));
    str = path;
    while (sscanf(str, "%*s/lib/%s", str) != 0) ;
    if (str != path && sscanf(str, "%*s/") == 0) {
	return 0;	/* library object */
    }
    return ::find_object(path);
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

    if (path == 0) {
	error("Bad argument 1 for function compile_object");
    }

    /*
     * check permission
     */
    str = object_name(this_object());
    sscanf(str, "%s#", str);
    path = reduce_path(path, str);
    if (creator != "System" &&
	::find_object(ACCESSD)->access(creator, path, READ_ACCESS) == 0) {
	error("Access denied");
    }

    obj = ::find_object(path);
    if (obj == 0) {
	/*
	 * check resource usage
	 */
	uid = creator(path);
	if (uid == 0) {
	    uid = owner;
	}
	rsrcd = ::find_object(RSRCD);
	rsrc = rsrcd->rsrc_get(uid, "objects");
	if (objrsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("Too many objects");
	}
    }

    str = path;
    while (sscanf(str, "%*s/lib/%s", str) != 0) ;
    if (str != path && sscanf(str, "%*s/") != 0) {
	/*
	 * library object
	 */
	rlimits (-1; -1) {
	    ::compile_object(path);
	    if (obj == 0) {
		/* new object */
		rsrcd->rsrc_incr(uid, "objects", path, 1, 1);
	    }
	}
	return 0;
    } else if (obj != 0) {
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
	    if ((status[ST_STACKDEPTH] < CREATE_STACKDEPTH &&
		 status[ST_STACKDEPTH] >= 0) ||
		(status[ST_TICKS] < CREATE_TICKS && status[ST_TICKS] >= 0)) {
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

    if (path == 0) {
	error("Bad argument 1 for function clone_object");
    }
    if (owner != "System" || oowner == 0) {
	oowner = owner;
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);

    str = path;
    while (sscanf(str, "%*s/obj/%s", str) != 0) ;
    if (path == str || sscanf(str, "%*s/lib/") != 0) {
	error("Cannot clone " + path);	/* not path of master object */
    }

    if (creator != "System" &&
	(sscanf(path, "/kernel/%*s") != 0 ||
	 !::find_object(ACCESSD)->access(oname, path, READ_ACCESS))) {
	error("Access denied");
    }

    rsrcd = ::find_object(RSRCD);
    obj = ::find_object(path);
    if (obj == 0) {
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
	if (status[ST_NOBJECTS] == status[ST_OTABSIZE]) {
	    error("Too many objects");
	}
	if ((status[ST_STACKDEPTH] < CREATE_STACKDEPTH &&
	     status[ST_STACKDEPTH] >= 0) &&
	    (status[ST_TICKS] < CREATE_TICKS && status[ST_TICKS] >= 0)) {
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
    object accessd;
    string oname, str;
    int i, sz, access;

    trace = ::call_trace();
    trace = trace[ .. sizeof(trace) - 2];	/* skip last */
    if (creator != "System") {
	accessd = ::find_object(ACCESSD);
	oname = object_name(this_object());
	sscanf(oname, "%s#", oname);

	for (i = 0, sz = sizeof(trace); i < sz; i++) {
	    if (trace[i][2]) {
		/* external call: check access to object */
		str = trace[i][0];
		sscanf(str, "%s#", str);
		access = accessd->access(oname, str, WRITE_ACCESS);
	    }
	    if (!access && sizeof(trace[i]) > 3) {
		trace[i] = trace[i][ .. 2];	/* remove arguments */
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
    mixed *status, **callouts;
    string oname, o2name;
    int i;

    switch (typeof(obj)) {
    case T_STRING:
	/* get corresponding object */
	obj = ::find_object(reduce_path(obj, object_name(this_object())));
	if (obj == 0) {
	    error("Bad argument 1 for function status");
	}
	/* fall through */
    case T_OBJECT:
	status = ::status(obj);
	callouts = status[O_CALLOUTS];
	if (sizeof(callouts) != 0) {
	    i = sizeof(callouts);
	    if (creator != "System") {
		oname = object_name(this_object());
		sscanf(oname, "%s#", oname);
		o2name = object_name(obj);
		sscanf(o2name, "%s#", o2name);
		if (!::find_object(ACCESSD)->access(oname, o2name,
						    WRITE_ACCESS)) {
		    /* remove arguments from callouts */
		    do {
			--i;
			callouts[i] = ({ callouts[i][2], callouts[i][1] });
		    } while (i != 0);
		    return status;
		}
	    }
	    /* ({ "_F_callout", delay, func... }) -> ({ func, delay... }) */
	    do {
		--i;
		callouts[i] = ({ callouts[i][2], callouts[i][1] }) +
			      callouts[i][3 ..];
	    } while (i != 0);
	}
	return status;

    case T_INT:
	if (obj == 0) {
	    return ::status();
	}
	/* fall through */
    default:
	error("Bad argument 1 for function status");
    }
}

/*
 * NAME:	this_user()
 * DESCRIPTION:	return current user
 */
static object this_user()
{
    return (::this_user() != 0) ? ::this_user()->query_user() : 0;
}

/*
 * NAME:	dump_state()
 * DESCRIPTION:	create state dump
 */
static dump_state()
{
    if (PRIV1()) {
	::find_object(DRIVER)->prepare_restore();
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
static varargs mixed call_limited(mixed foo, string function, mixed args...)
{
    object obj, rsrcd;
    int *status, stackdepth, rstack, ticks, rticks;
    string oowner;
    mixed result;

    if (typeof(foo) == T_STRING) {
	foo = find_object(foo);
    }
    if (typeof(foo) != T_OBJECT) {
	error("Bad argument 1 for function call_limited");
    }
    if (function == 0) {
	error("Bad argument 2 for function call_limited");
    }

    status = ::status();
    rlimits (-1; -1) {
	obj = foo;
	rsrcd = ::find_object(RSRCD);
	oowner = obj->query_owner();

	/* determine available stack */
	stackdepth = status[ST_STACKDEPTH];
	rstack = rsrcd->rsrc_get(oowner, "stackdepth")[RSRC_MAX];
	if (rstack > stackdepth && stackdepth >= 0) {
	    rstack = stackdepth;
	}

	/* determine available ticks */
	ticks = status[ST_TICKS];
	rticks = rsrcd->rsrc_get(oowner, "ticks")[RSRC_MAX];
	if (rticks >= 0) {
	    int *rusage, max;

	    rusage = rsrcd->rsrc_get(oowner, "tick usage");
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

	foo = ({ oowner, ticks, rticks });
    }

    rlimits (rstack; rticks) {
	result = call_other(obj, function, args...);
	status = ::status();
    }

    rlimits (-1; -1) {
	if (rticks >= 0) {
	    rsrcd->rsrc_incr(oowner, "tick usage", 0, foo[2] - status[ST_TICKS],
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
	call_limited(this_object(), function, args...);
    }
}


/*
 * NAME:	read_file()
 * DESCRIPTION:	read a string from a file
 */
static varargs string read_file(string path, int offset, int size)
{
    string oname;

    if (path == 0) {
	error("Bad argument 1 for function read_file");
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);
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

    if (path == 0) {
	error("Bad argument 1 for function write_file");
    }
    if (str == 0) {
	error("Bad argument 2 for function write_file");
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);
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

    if (path == 0) {
	error("Bad argument 1 for function remove_file");
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);
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

    if (from == 0) {
	error("Bad argument 1 for function rename_file");
    }
    if (to == 0) {
	error("Bad argument 2 for function rename_file");
    }
    from = reduce_path(from, oname = object_name(this_object()));
    to = reduce_path(to, oname);
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

    if (path == 0) {
	error("Bad argument 1 for function get_dir");
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);
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

    if (path == 0) {
	error("Bad argument 1 for function make_dir");
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);
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

    if (path == 0) {
	error("Bad argument 1 for function remove_dir");
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);
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

    if (path == 0) {
	error("Bad argument 1 for function restore_object");
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);
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

    if (path == 0) {
	error("Bad argument 1 for function save_object");
    }
    oname = object_name(this_object());
    sscanf(oname, "%s#", oname);
    path = reduce_path(path, oname);
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

    if (cmd == 0) {
	error("Bad argument 1 to function editor");
    }
    rlimits (-1; -1) {
	rsrcd = ::find_object(RSRCD);
	if (query_editor(this_object()) == 0 &&
	    rsrcd->rsrc_incr(owner, "editors", this_object(), 1) == 0) {
	    error("Too many editors");
	}
	::editor(cmd);
	if (query_editor(this_object()) == 0) {
	    rsrcd->rsrc_incr(owner, "editors", this_object(), -1);
	}
	info = ::find_object(DRIVER)->query_wfile();
	if (info != 0) {
	    rsrcd->rsrc_incr(creator(info[0]), "filequota", 0,
			     file_size(info[0]) - info[1], 1);
	}
    }
}
