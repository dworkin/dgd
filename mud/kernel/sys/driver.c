# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/rsrc.h>
# include <kernel/access.h>
# include <kernel/user.h>
# ifdef SYS_NETWORKING
#  include <kernel/net.h>
# endif
# include <status.h>
# include <trace.h>

object rsrcd;		/* resource manager object */
object accessd;		/* access manager object */
object userd;		/* user manager object */
object initd;		/* init manager object */
object objectd;		/* object manager object */
object errord;		/* error manager object */
# ifdef SYS_NETWORKING
object telnet;		/* telnet port object */
object binary;		/* binary port object */
# endif
int auto;		/* handle implicit compile of auto object */
string file;		/* last file used in editor write operation */
int size;		/* size of file used in editor write operation */
string compiled;	/* object currently being compiled */
string *inherited;	/* list of inherited objects */
string error;		/* last error */

/*
 * NAME:	creator()
 * DESCRIPTION:	get creator of file
 */
string creator(string file)
{
    return (sscanf(file, "/kernel/%*s") != 0) ? "System" :
	    (sscanf(file, USR + "/%s/", file) != 0) ? file : 0;
}

/*
 * NAME:	normalize_path()
 * DESCRIPTION:	reduce a path to its minimal absolute form
 */
string normalize_path(string file, string dir, string creator)
{
    string *path;
    int i, j, sz;

    if (strlen(file) == 0) {
	file = dir;
    }
    switch (file[0]) {
    case '~':
	/* ~path */
	if (creator && (strlen(file) == 1 || file[1] == '/')) {
	    file = USR + "/" + creator + file[1 ..];
	} else {
	    file = USR + "/" + file[1 ..];
	}
	/* fall through */
    case '/':
	/* absolute path */
	if (sscanf(file, "%*s//") == 0 && sscanf(file, "%*s/.") == 0) {
	    return file;	/* no changes */
	}
	path = explode(file, "/");
	break;

    default:
	/* relative path */
	if (sscanf(file, "%*s//") == 0 && sscanf(file, "%*s/.") == 0 &&
	    sscanf(dir, "%*s/..") == 0) {
	    /*
	     * simple relative path
	     */
	    return dir + "/" + file;
	}
	/* fall through */
    case '.':
	/*
	 * complex relative path
	 */
	path = explode(dir + "/" + file, "/");
	break;
    }

    for (i = 0, j = -1, sz = sizeof(path); i < sz; i++) {
	switch (path[i]) {
	case "..":
	    if (j >= 0) {
		--j;
	    }
	    /* fall through */
	case "":
	case ".":
	    continue;
	}
	path[++j] = path[i];
    }

    return "/" + implode(path[.. j], "/");
}

/*
 * NAME:	dir_size()
 * DESCRIPTION:	get the size of all files in a directory
 */
private int dir_size(string file)
{
    mixed **info;
    int *sizes, size, i;

    info = get_dir(file + "/*");
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
varargs int file_size(string file, int dir)
{
    if (KERNEL() || SYSTEM()) {
	mixed **info;
	string *files, name;
	int i, sz;

	info = get_dir(file);
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
	return (i > 0) ?
		(i + 1023) >> 10 :
		(i == 0) ? 1 : (i == -2 && dir) ? dir_size(file) : 0;
    }
}

/*
 * NAME:	set_object_manager()
 * DESCRIPTION:	set the object manager
 */
set_object_manager(object obj)
{
    if (SYSTEM()) {
	objectd = obj;
    }
}

/*
 * NAME:	set_error_manager()
 * DESCRIPTION:	set the error manager
 */
set_error_manager(object obj)
{
    if (SYSTEM()) {
	errord = obj;
    }
}

/*
 * NAME:	compiling()
 * DESCRIPTION:	object being compiled
 */
compiling(string path)
{
    if (previous_program() == AUTO) {
	if (auto) {
	    auto = FALSE;
	    if (path != AUTO || find_object(AUTO)) {
		rsrcd->rsrc_incr("System", "objects", 0, 1, TRUE);
	    }
	}
	compiled = path;
	inherited = ({ });
	if (objectd) {
	    objectd->compiling(path);
	}
    }
}

/*
 * NAME:	compile()
 * DESCRIPTION:	object compiled
 */
compile(object obj, string owner)
{
    if (objectd && previous_program() == AUTO) {
	if (inherited) {
	    objectd->compile(owner, obj, inherited...);
	    inherited = 0;
	} else {
	    objectd->compile(owner, obj);
	}
    }
}

/*
 * NAME:	compile_lib()
 * DESCRIPTION:	inherited object compiled
 */
compile_lib(string path, string owner)
{
    if (objectd && previous_program() == AUTO) {
	if (inherited) {
	    objectd->compile_lib(owner, path, inherited...);
	    inherited = 0;
	} else {
	    objectd->compile_lib(owner, path);
	}
    }
}

/*
 * NAME:	clone()
 * DESCRIPTION:	object cloned
 */
clone(object obj, string owner)
{
    if (objectd && previous_program() == AUTO) {
	objectd->clone(owner, obj);
    }
}

/*
 * NAME:	destruct()
 * DESCRIPTION:	object about to be destructed
 */
destruct(object obj, string owner)
{
    if (objectd && previous_program() == AUTO) {
	objectd->destruct(owner, obj);
    }
}

/*
 * NAME:	destruct_lib()
 * DESCRIPTION:	inherited object about to be destructed
 */
destruct_lib(string path, string owner)
{
    if (previous_program() == AUTO) {
	if (path == AUTO) {
	    if (auto) {
		rsrcd->rsrc_incr("System", "objects", 0, 1, TRUE);
	    }
	    auto = TRUE;
	}
	if (objectd) {
	    objectd->destruct_lib(owner, path);
	}
    }
}

/*
 * NAME:	query_owner()
 * DESCRIPTION:	return owner of driver object
 */
string query_owner()
{
    return "System";
}


/*
 * NAME:	message()
 * DESCRIPTION:	show message
 */
message(string str)
{
    if (KERNEL() || SYSTEM()) {
	send_message(ctime(time())[4 .. 18] + " ** " + str);
    }
}

/*
 * NAME:	load()
 * DESCRIPTION:	find or compile object
 */
private object load(string path)
{
    object obj;

    obj = find_object(path);
    return (obj) ? obj : compile_object(path);
}

/*
 * NAME:	initialize()
 * DESCRIPTION:	called once at system startup
 */
static initialize()
{
    string *users;
    int i;
# ifdef SYS_NETWORKING
    object port;
# endif

    message("DGD " + status()[ST_VERSION] + "\n");
    message("Initializing...\n");

    /* load initial objects */
    load(AUTO);
    call_other(load(OBJREGD), "???");
    call_other(rsrcd = load(RSRCD), "???");

    /* initialize some resources */
    rsrcd->set_rsrc("stack",	        50, 0, 0);
    rsrcd->set_rsrc("ticks",	    250000, 0, 0);
    rsrcd->set_rsrc("create stack",      5, 0, 0);
    rsrcd->set_rsrc("create ticks",  10000, 0, 0);

    load(RSRCOBJ);

    /* create initial resource owners */
    rsrcd->add_owner("System");
    rsrcd->rsrc_incr("System", "filequota", 0,
		     dir_size("/kernel") + file_size(USR + "/System", TRUE));
    rsrcd->add_owner(0);	/* Ecru */
    rsrcd->rsrc_incr(0, "filequota", 0,
		     file_size("/doc", TRUE) + file_size("/include", TRUE));

    /* load remainder of manager objects */
    call_other(accessd = load(ACCESSD), "???");
    call_other(userd = load(USERD), "???");
    call_other(load(DEFAULT_WIZTOOL), "???");
# ifdef SYS_NETWORKING
    call_other(port = load(PORT_OBJECT), "???");
    telnet = clone_object(port);
    binary = clone_object(port);
# endif
    catch {
	initd = load(USR + "/System/initd");
    }

    /* initialize other users as resource owners */
    users = (accessd->query_users() - ({ "System" })) | ({ "admin" });
    for (i = sizeof(users); --i >= 0; ) {
	rsrcd->add_owner(users[i]);
	rsrcd->rsrc_incr(users[i], "filequota", 0,
			 file_size(USR + "/" + users[i], TRUE));
    }

    /* correct object count */
    rsrcd->rsrc_incr("System", "objects", 0,
		     status()[ST_NOBJECTS] -
			     rsrcd->rsrc_get("System", "objects")[RSRC_USAGE],
		     1);

    /* system-specific initialization */
    if (initd) {
	call_other(initd, "???");
# ifdef SYS_NETWORKING
    } else {
	telnet->listen("telnet", TELNET_PORT);
	binary->listen("tcp", BINARY_PORT);
# endif
    }

    message("Initialization complete.\n\n");
}

/*
 * NAME:	prepare_reboot()
 * DESCRIPTION:	prepare for a state dump
 */
prepare_reboot()
{
    if (KERNEL()) {
	rsrcd->prepare_reboot();
	userd->prepare_reboot();
	if (initd) {
	    initd->prepare_reboot();
	}
    }
}

/*
 * NAME:	restored()
 * DESCRIPTION:	re-initialize the system after a restore
 */
static restored()
{
    message("DGD " + status()[ST_VERSION] + "\n");

    rsrcd->reboot();
    userd->reboot();
    if (initd) {
	initd->reboot();
    }
# ifdef SYS_NETWORKING
    if (telnet) {
	telnet->listen("telnet", TELNET_PORT);
    }
    if (binary) {
	binary->listen("tcp", BINARY_PORT);
    }
# endif

    message("State restored.\n\n");
}

/*
 * NAME:	path_read()
 * DESCRIPTION:	handle an editor read path
 */
static string path_read(string path)
{
    string oname, creator;

    catch {
	path = previous_object()->path_read(path);
    }
    if (path) {
	creator = creator(oname = object_name(previous_object()));
	path = normalize_path(path, oname, creator);
	return ((creator == "System" ||
		 accessd->access(oname, path, READ_ACCESS)) ? path : 0);
    }
    return 0;
}

/*
 * NAME:	path_write()
 * DESCRIPTION:	handle an editor write path
 */
static string path_write(string path)
{
    string oname, creator;

    catch {
	path = previous_object()->path_write(path);
    }
    if (path) {
	creator = creator(oname = object_name(previous_object()));
	path = normalize_path(path, oname, creator);
	if (sscanf(path, "/kernel/%*s") == 0 &&
	    sscanf(path, "/include/kernel/%*s") == 0 &&
	    (creator == "System" || accessd->access(oname, path, WRITE_ACCESS)))
	{
	    size = file_size(file = path);
	    return path;
	}
    }
    return 0;
}

/*
 * NAME:	query_wfile()
 * DESCRIPTION:	get information about the editor file last written
 */
mixed *query_wfile()
{
    mixed *info;

    if (file) {
	info = ({ file, size });
	file = 0;
	return info;
    }
    return 0;
}

/*
 * NAME:	call_object()
 * DESCRIPTION:	get an object for call_other's first (string) argument
 */
static object call_object(string path)
{
    string oname;

    oname = object_name(previous_object());
    path = normalize_path(path, oname + "/..", creator(oname));
    if (sscanf(path, "%*s/lib/") != 0) {
	error("Illegal use of call_other");
    }
    return find_object(path);
}

/*
 * NAME:	inherit_program()
 * DESCRIPTION:	inherit a program, compiling it if needed
 */
static object inherit_program(string from, string path)
{
    string creator;
    object obj;

    path = normalize_path(path, from + "/..", creator = creator(from));
    if (sscanf(path, "%*s/lib/") == 0 ||
	(sscanf(path, "/kernel/%*s") != 0 && creator != "System") ||
	!accessd->access(from, path, READ_ACCESS)) {
	return 0;
    }

    obj = find_object(path);
    if (!obj) {
	int *rsrc;
	string saved;

	creator = creator(path);
	rsrc = rsrcd->rsrc_get(creator, "objects");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("Too many objects");
	}

	saved = compiled;
	compiled = path;
	inherited = ({ });
	if (objectd) {
	    objectd->compiling(path);
	}
	obj = compile_object(path);
	rsrcd->rsrc_incr(creator, "objects", 0, 1, TRUE);
	if (objectd) {
	    objectd->compile_lib(creator, path, inherited...);
	}
	compiled = saved;
	inherited = ({ });
    } else if (inherited) {
	inherited += ({ path });
    }
    return obj;
}

/*
 * NAME:	path_include()
 * DESCRIPTION:	translate an include path
 */
static string path_include(string from, string path)
{
    if (path == "AUTO" && from == "/include/std.h" && objectd &&
	creator(compiled) != "System") {
	/*
	 * special object-dependant include file
	 */
	path = objectd->path_special(compiled);
    } else if (strlen(path) != 0 && path[0] != '~' &&
	       (sscanf(path, "/include/%*s") != 0 ||
		sscanf(path, "%*s/") == 0) && sscanf(path, "%*s/../") == 0) {
	/*
	 * safe include: return immediately
	 */
	if (path[0] == '/') {
	    if (objectd) {
		objectd->include(from, path);
	    }
	    return path;
	} else {
	    if (objectd) {
		objectd->include(from, normalize_path(path, from + "/..", 0));
	    }
	    return from + "/../" + path;
	}
    } else {
	path = normalize_path(path, from + "/..", creator(from));
    }

    if (accessd->access(from, path, READ_ACCESS)) {
	if (objectd) {
	    objectd->include(from, path);
	}
	return path;
    }
    return 0;
}

/*
 * NAME:	remove_program()
 * DESCRIPTION:	the last reference to a program is removed
 */
static remove_program(string path, int timestamp, int index)
{
    string creator;

    creator = creator(path);
    if (path != RSRCOBJ) {
	rsrcd->rsrc_incr(creator, "objects", 0, -1);
    }
    if (objectd) {
	objectd->remove_program(creator, path, timestamp, index);
    }
}

/*
 * NAME:	recompile()
 * DESCRIPTION:	recompile an inherited object
 */
static recompile(object obj)
{
    destruct_object(obj);
}

/*
 * NAME:	telnet_connect()
 * DESCRIPTION:	return a telnet connection user object
 */
static object telnet_connect()
{
    return userd->telnet_connection();
}

/*
 * NAME:	binary_connect()
 * DESCRIPTION:	return a binary connection user object
 */
static object binary_connect()
{
    return userd->binary_connection();
}

/*
 * NAME:	interrupt
 * DESCRIPTION:	called when a kill signal is sent to the server
 */
static interrupt()
{
    message("Interrupt.\n");

# ifdef SYS_CONTINUOUS
    prepare_reboot();
    dump_state();
# endif
    shutdown();
}

/*
 * NAME:	query_error()
 * DESCRIPTION:	return the last errormessage
 */
string query_error()
{
    if (KERNEL() || SYSTEM()) {
	return error;
    }
}

/*
 * NAME:	runtime_error()
 * DESCRIPTION:	log a runtime error
 */
static runtime_error(string str, int caught, int ticks)
{
    mixed **trace;
    string line, function, progname, objname;
    int i, sz, len;
    object obj;

    if (caught == 1) {
	/* top-level catch: ignore */
	caught = 0;
    } else if (caught != 0 && ticks < 0) {
	error = str;
	return;
    }

    trace = call_trace();
    i = sz = sizeof(trace) - 1;

    if (ticks >= 0) {
	while (--i >= caught) {
	    if (trace[i][TRACE_FUNCTION] == "_F_call_limited" &&
		trace[i][TRACE_PROGNAME] == AUTO) {
		ticks = rsrcd->update_ticks(ticks);
		if (ticks < 0) {
		    break;
		}
	    }
	}
    }

    if (errord) {
	errord->runtime_error(str, caught, trace);
    } else {
	if (caught != 0) {
	    str += " [caught]";
	}
	str += "\n";

	for (i = 0; i < sz; i++) {
	    progname = trace[i][TRACE_PROGNAME];
	    len      = trace[i][TRACE_LINE];
	    if (len == 0) {
		line = "    ";
	    } else {
		line = "    " + len;
		line = line[strlen(line) - 4 ..];
	    }

	    function = trace[i][TRACE_FUNCTION];
	    len = strlen(function);
	    if (progname == AUTO && len > 3) {
		switch (function[.. 2]) {
		case "bad":
		case "_F_":
		case "_Q_":
		    continue;
		}
	    }
	    if (len < 17) {
		function += "                 "[len ..];
	    }

	    objname  = trace[i][TRACE_OBJNAME];
	    if (progname != objname) {
		len = strlen(progname);
		if (len < strlen(objname) && progname == objname[.. len - 1] &&
		    objname[len] == '#') {
		    objname = objname[len ..];
		}
		str += line + " " + function + " " + progname + " (" + objname +
		       ")\n";
	    } else {
		str += line + " " + function + " " + progname + "\n";
	    }
	}

	message(str);
	if (caught == 0 && this_user() && (obj=this_user()->query_user())) {
	    obj->message(str);
	}
    }
}

/*
 * NAME:	compile_error()
 * DESCRIPTION:	deal with a compilation error
 */
static compile_error(string file, int line, string err)
{
    object obj;

    if (errord) {
	errord->compile_error(file, line, err);
    } else {
	send_message(file += ", " + line + ": " + err + "\n");
	if (this_user() && (obj=this_user()->query_user())) {
	    obj->message(file);
	}
    }
}

/*
 * NAME:	compile_rlimits()
 * DESCRIPTION:	compile-time check on rlimits
 */
static int compile_rlimits(string objname)
{
    /* unlimited resource usage for kernel objects */
    return sscanf(objname, "/kernel/%*s");
}

/*
 * NAME:	runtime_rlimits()
 * DESCRIPTION:	runtime check on rlimits
 */
static int runtime_rlimits(object obj, int depth, int ticks)
{
    return (sscanf(object_name(obj), USR + "/System/%*s") != 0 &&
	    depth == 0 && ticks < 0);
}
