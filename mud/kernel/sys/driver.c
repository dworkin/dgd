# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/rsrc.h>
# include <kernel/access.h>
# include <kernel/timer.h>
# include <kernel/user.h>
# include <status.h>
# include <trace.h>

object rsrcd;		/* resource daemon object */
object accessd;		/* access daemon object */
object timerd;		/* time daemon object */
object userd;		/* user daemon object */
string file;		/* last file used in editor write operation */
int size;		/* size of file used in editor write operation */

/*
 * NAME:	query_owner()
 * DESCRIPTION:	return owner of driver object
 */
string query_owner()
{
    return "System";
}


# include "/kernel/lib/file.c"


/*
 * NAME:	initialize()
 * DESCRIPTION:	called once at system startup
 */
static initialize()
{
    /* object registry daemon */
    call_other(compile_object(OBJREGD), "???");
send_message("objregd\n");

    /* resource daemon */
    call_other(rsrcd = compile_object(RSRCD), "???");
send_message("rsrcd\n");

    compile_object(RSRCOBJ);
    rsrcd->add_owner("System");
    rsrcd->rsrc_incr("System", "objects", 0, 3);
send_message("rsrcd2\n");

    /* access daemon */
    call_other(accessd = compile_object(ACCESSD), "???");
send_message("accessd\n");

    /* timer daemon */
    call_other(timerd = compile_object(TIMERD), "???");
send_message("timerd\n");

    /* user daemon */
    call_other(userd = compile_object(USERD), "???");
send_message("userd\n");
}

/*
 * NAME:	prepare_statedump()
 * DESCRIPTION:	prepare for a state dump
 */
prepare_statedump()
{
}

/*
 * NAME:	restored()
 * DESCRIPTION:	re-initialize the system after a restore
 */
static restored()
{
    timerd->restored();
    userd->restored();
}

/*
 * NAME:	path_read()
 * DESCRIPTION:	handle an editor read path
 */
static string path_read(string path)
{
    string oname, creator;

    oname = object_name(previous_object());
    creator = creator(oname);
    path = reduce_path(path, oname, creator);
    return (creator == "System" ||
	    accessd->access(oname, path, READ_ACCESS)) ? path : 0;
}

/*
 * NAME:	path_write()
 * DESCRIPTION:	handle an editor write path
 */
static string path_write(string path)
{
    string oname, creator;

    oname = object_name(previous_object());
    creator = creator(oname);
    path = reduce_path(path, oname, creator);
    if (creator == "System" || accessd->access(oname, path, WRITE_ACCESS)) {
	file = path;
	size = file_size(path);
	return path;
    }
    return 0;
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
	info = 0;
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
    path = reduce_path(path, oname, creator(oname));
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

    creator = creator(from);
    path = reduce_path(path, from, creator);
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
		rsrcd->rsrc_incr(creator, "objects", path, 1, 1);
	    }
	} : error("");
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
    path = reduce_path(path, from, creator);
    return (accessd->access(from, path, READ_ACCESS)) ? path : 0;
}

/*
 * NAME:	remove_program()
 * DESCRIPTION:	the last reference to a program is removed
 */
static remove_program(string path, int timestamp)
{
    if (path != RSRCOBJ) {
	rsrcd->rsrc_incr(creator(path), "objects",
			 (sscanf(path, "%*s/lib/") != 0) ? path : 0,
			 -1);
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
    return userd->new_telnet_user();
}

/*
 * NAME:	binary_connect()
 * DESCRIPTION:	return a binary connection user object
 */
static object binary_connect()
{
    return userd->new_binary_user();
}

/*
 * NAME:	interrupt
 * DESCRIPTION:	called when a kill signal is sent to the server
 */
static interrupt()
{
    shutdown();
}

string last_error;

/*
 * NAME:	runtime_error()
 * DESCRIPTION:	log a runtime error
 */
static runtime_error(string str, int caught, int ticks)
{
    mixed **trace, *what;
    string line, function, progname, objname;
    int i, sz, spent, len;

    if (caught && ticks < 0) {
	last_error = str;
	return;
    }
    if (str == "") {
	str = last_error;
    }

    str = ctime(time())[4 .. 18] + " ** " + str + "\n";

    trace = call_trace();
    i = sz = sizeof(trace) - 1;

    if (ticks >= 0) {
	while (--i >= 0) {
	    if (trace[i][TRACE_FUNCTION] == "call_limited" &&
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

    for (i = 0; i < sz; i++) {
	progname = trace[i][TRACE_PROGNAME];
	if (progname != AUTO + DRIVER) {	/* FOO */
	    len      = trace[i][TRACE_LINE];
	    if (len == 0) {
		line = "    ";
	    } else {
		line = "    " + len;
		line = line[strlen(line) - 4 ..];
	    }

	    function = trace[i][TRACE_FUNCTION];
	    len = strlen(function);
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
		str += line + " " + function + " " + progname +
		       " (" + objname + ")\n";
	    } else {
		str += line + " " + function + " " + progname + "\n";
	    }
	}
    }
    send_message(str);
}

/*
 * NAME:	compile_error()
 * DESCRIPTION:	deal with a compilation error
 */
compile_error(string file, int line, string err)
{
    send_message(file + ", " + line + ": " + err + "\n");
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
    return (sscanf(object_name(obj), "/usr/System/%*s") != 0 &&
	    depth == 0 && ticks < 0);
}

# ifdef DEBUG
trace(string s) { send_message("TRACE: " + s + "\n"); }
# endif
