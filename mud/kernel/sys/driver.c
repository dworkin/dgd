# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/rsrc.h>
# include <kernel/access.h>
# include <kernel/timer.h>
# include <kernel/user.h>
# include <status.h>
# include <trace.h>

object rsrcd;		/* resource manager object */
object accessd;		/* access manager object */
object timerd;		/* time manager object */
object userd;		/* user manager object */
object initd;		/* init manager object */
string file;		/* last file used in editor write operation */
int size;		/* size of file used in editor write operation */
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
    if (KERNEL()) {
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
 * NAME:	init_filequota()
 * DESCRIPTION:	set initial file quota for user
 */
init_filequota(string owner)
{
    if (previous_program() == DRIVER || SYSTEM()) {
	rsrcd->rsrc_incr(owner, "filequota", 0,
			 file_size(USR + "/" + owner, 1) -
			     rsrcd->rsrc_get(owner, "filequota")[RSRC_USAGE],
			 1);
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
 * NAME:	initialize()
 * DESCRIPTION:	called once at system startup
 */
static initialize()
{
    compile_object(AUTO);

    call_other(compile_object(OBJREGD), "???");
    call_other(rsrcd = compile_object(RSRCD), "???");

    rsrcd->set_rsrc("stack",	        50, 0, 0);
    rsrcd->set_rsrc("ticks",	    500000, 0, 0);
    rsrcd->set_rsrc("create stack",      5, 0, 0);
    rsrcd->set_rsrc("create ticks",  10000, 0, 0);

    compile_object(RSRCOBJ);
    rsrcd->add_owner("System");
    init_filequota("System");
    rsrcd->rsrc_incr("System", "filequota", 0, dir_size("/kernel"));

    call_other(accessd = compile_object(ACCESSD), "???");
    call_other(timerd = compile_object(TIMERD), "???");
    call_other(userd = compile_object(USERD), "???");
    call_other(compile_object(DEFAULT_WIZTOOL), "???");

    rsrcd->rsrc_incr("System", "objects", 0, 9);

    rsrcd->add_owner(0);	/* Ecru */
    rsrcd->rsrc_incr(0, "filequota", 0, dir_size("/include"));

    rsrcd->add_owner("admin");
    init_filequota("admin");

    catch {
	initd = compile_object(USR + "/System/initialize");
	rsrcd->rsrc_incr("System", "objects", 0, 1);
	call_other(initd, "???");
    }
}

/*
 * NAME:	prepare_reboot()
 * DESCRIPTION:	prepare for a state dump
 */
prepare_reboot()
{
    if (previous_program() == AUTO) {
	rsrcd->prepare_reboot();
	timerd->prepare_reboot();
	userd->prepare_reboot();
	if (initd) {
	    catch {
		initd->prepare_reboot();
	    }
	}
    }
}

/*
 * NAME:	restored()
 * DESCRIPTION:	re-initialize the system after a restore
 */
static restored()
{
    rsrcd->reboot();
    timerd->reboot();
    userd->reboot();
    if (initd) {
	catch {
	    initd->reboot();
	}
    }
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
	    file = path;
	    size = file_size(path);
	    return path;
	}
    }
}

/*
 * NAME:	query_wfile()
 * DESCRIPTION:	get information about the editor file last written
 */
mixed *query_wfile()
{
    if (file == 0) {
	return 0;
    } else {
	mixed *info;

	info = ({ file, size });
	file = 0;
	return info;
    }
}

/*
 * NAME:	call_object()
 * DESCRIPTION:	get an object for call_other's first (string) argument
 */
static object call_object(string path)
{
    string oname;

    oname = object_name(previous_object());
    path = normalize_path(path, oname, creator(oname));
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

    path = normalize_path(path, from, creator = creator(from));
    if (sscanf(path, "%*s/lib/") == 0 ||
	(sscanf(path, "/kernel/%*s") != 0 && creator != "System") ||
	!accessd->access(from, path, READ_ACCESS)) {
	return 0;
    }

    obj = find_object(path);
    if (obj == 0) {
	int *rsrc;

	creator = creator(path);
	rsrc = rsrcd->rsrc_get(creator, "objects");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("Too many objects");
	}
	catch {
	    rlimits (-1; -1) {
		obj = compile_object(path);
		rsrcd->rsrc_incr(creator, "objects", 0, 1, 1);
	    }
	} : error(error);
    }
    return obj;
}

/*
 * NAME:	path_include()
 * DESCRIPTION:	translate an include path
 */
static string path_include(string from, string path)
{
    string creator;

    creator = creator(from);
    if ((sscanf(from, "/include/%*s") != 0 || creator == "System") &&
	path[0] != '~') {
	return (path[0] == '/') ? path : from + "/../" + path;
    }
    path = normalize_path(path, from, creator);
    return (accessd->access(from, path, READ_ACCESS)) ? path : 0;
}

/*
 * NAME:	remove_program()
 * DESCRIPTION:	the last reference to a program is removed
 */
static remove_program(string path, int timestamp, int index)
{
    if (path != RSRCOBJ) {
	rsrcd->rsrc_incr(creator(path), "objects", 0, -1);
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
    shutdown();
# ifdef SYS_CONTINUOUS
    prepare_statedump();
    dump_state();
# endif
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
    mixed **trace, *what;
    string line, function, progname, objname;
    int i, sz, spent, len;
    object obj;

    if (caught) {
	if (ticks < 0) {
	    error = str;
	    return;
	}
	str += " [caught]";
    }

    trace = call_trace();
    i = sz = sizeof(trace) - 1;

    if (ticks >= 0) {
	while (--i >= 0) {
	    if (trace[i][TRACE_FUNCTION] == "_F_call_limited" &&
		trace[i][TRACE_PROGNAME] == AUTO) {
		what = trace[i][TRACE_FIRSTARG];
		spent = what[2] - ticks;
		rsrcd->rsrc_incr(what[0], "tick usage", 0, spent, 1);
		what[2] = ticks;
		ticks = what[1] - spent;
		if (ticks < 0) {
		    break;
		}
	    }
	}
    }

    str = ctime(time())[4 .. 18] + " ** " + str + "\n";

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
	if (len > 3 && progname == AUTO &&
	    (function[.. 2] == "_F_" || function[.. 2] == "_Q_")) {
	    continue;
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
    send_message(str);
    if (!caught && this_user() && (obj=this_user()->query_user())) {
	obj->message(str);
    }
}

/*
 * NAME:	compile_error()
 * DESCRIPTION:	deal with a compilation error
 */
static compile_error(string file, int line, string err)
{
    object obj;

    send_message(file += ", " + line + ": " + err + "\n");
    if (this_user() && (obj=this_user()->query_user())) {
	obj->message(file);
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

# ifdef DEBUG
trace(string s) { send_message("TRACE: " + s + "\n"); }
# endif
