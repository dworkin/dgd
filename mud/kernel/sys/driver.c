# include <kernel/kernel.h>
# include <kernel/objreg.h>
# include <kernel/rsrc.h>
# include <kernel/access.h>
# include <kernel/timer.h>
# include <kernel/user.h>
# include <status.h>

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


/*
 * NAME:	creator()
 * DESCRIPTION:	get creator of file
 */
private string creator(string file)
{
    string creator;

    return (sscanf(file, "/usr/%s/", creator) == 0) ? "System" : creator;
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
	    file = "/usr/" + creator(dir) + file[1 .. ];
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
 * NAME:	initialize()
 * DESCRIPTION:	called once at system startup
 */
static initialize()
{
    /* resource daemon */
    call_other(rsrcd = compile_object(RSRCD), "???");

    /* object registry daemon */
    call_other(compile_object(OBJREGD), "???");

    /* initial resources */
    compile_object(RSRCOBJ);
    rsrcd->set_rsrc("objects", -1, 0, 0);
    rsrcd->set_rsrc("events", -1, 0, 0);
    rsrcd->set_rsrc("callouts", -1, 0, 0);
    rsrcd->set_rsrc("callout starts", -1, 0, 0);
    rsrcd->set_rsrc("stackdepth", -1, 0, 0);
    rsrcd->set_rsrc("ticks", -1, 0, 0);
    rsrcd->set_rsrc("tick usage", -1, 0, 0);
    rsrcd->set_rsrc("filequota", -1, 0, 0);
    rsrcd->set_rsrc("editors", -1, 0, 0);

    /* access daemon */
    call_other(accessd = compile_object(ACCESSD), "???");

    /* timer daemon */
    call_other(timerd = compile_object(TIMERD), "???");

    /* user daemon */
    call_other(userd = compile_object(USERD), "???");
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
    string oname;

    path = reduce_path(path, oname = object_name(previous_object()));
    return (creator(oname) == "System" ||
	    accessd->access(oname, path, READ_ACCESS)) ? path : 0;
}

/*
 * NAME:	path_write()
 * DESCRIPTION:	handle an editor write path
 */
static string path_write(string path)
{
    string oname;

    path = reduce_path(path, oname = object_name(previous_object()));
    if (creator(oname) == "System" ||
	accessd->access(oname, path, WRITE_ACCESS)) {
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
    path = reduce_path(path, object_name(previous_object()));
    if (sscanf(path, "%*s/lib/") != 0 || sscanf(path, "%*s/obj/%s#") == 1) {
	error("Illegal call_other");
    }
    return find_object(path);
}

/*
 * NAME:	inherit_program()
 * DESCRIPTION:	inherit a program, compiling it if needed
 */
static object inherit_program(string from, string path)
{
    string dir, str;
    object obj;

    path = reduce_path(path, from);
    if (sscanf(path, "/kernel/%*s") == 0 && sscanf(path, "/usr/%*s") == 0) {
	return 0;
    }
    str = path;
    while (sscanf(str, "%*s/lib/%s", str) != 0) ;
    if (str == path || sscanf(str, "%*s/obj/") != 0 ||
	!accessd->access(from, path, READ_ACCESS)) {
	return 0;
    }

    obj = find_object(path);
    if (obj == 0) {
	int *rsrc;

	str = creator(path);
	rsrc = rsrcd->rsrc_get(str, "objects");
	if (rsrc[RSRC_USAGE] >= rsrc[RSRC_MAX] && rsrc[RSRC_MAX] >= 0) {
	    error("Too many objects");
	}
	rlimits (-1; -1) {
	    obj = compile_object(path);
	    rsrcd->rsrc_incr(str, "objects", path, 1, 1);
	}
    }
    return obj;
}

/*
 * NAME:	path_include()
 * DESCRIPTION:	translate an include path
 */
static string path_include(string from, string path)
{
    if (creator(from) == "System" && path[0] != '~') {
	return (path[0] == '/') ? path : from + "/../" + path;
    }
    path = reduce_path(path, from);
    return (accessd->access(from, path, READ_ACCESS)) ? path : 0;
}

/*
 * NAME:	remove_program()
 * DESCRIPTION:	the last reference to a program is removed
 */
static remove_program(string path)
{
    if (path != RSRCOBJ) {
	rsrcd->rsrc_incr(creator(path), "objects",
			 (sscanf(path, "%*s/obj/") != 0 ||
			  sscanf(path, "%*s/lib/") != 0) ? path : 0,
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

/*
 * NAME:	runtime_error()
 * DESCRIPTION:	log a runtime error
 */
static runtime_error(string error, int caught)
{
    mixed **trace, *foo;
    string progname, objname, function, str;
    int i, sz, ticks, x, line, len;

    send_message(error + "\n");
    trace = call_trace();
    if ((sz=sizeof(trace) - 1) != 0) {
	ticks = status()[ST_ATICKS];
	if (ticks >= 0) {
	    i = sz;
	    while (--i >= 0) {
		if (trace[i][2] == "call_limited" && trace[i][1] == AUTO) {
		    foo = trace[i][5];
		    x = foo[2] - ticks;
		    rsrcd->rsrc_incr(foo[0], "tick usage", 0, x, 1);
		    foo[2] = ticks;
		    ticks = foo[1];
		    if (ticks < 0) {
			break;
		    }
		    ticks -= x;
		}
	    }
	}

	for (i = 0; i < sz; i++) {
	    objname  = trace[i][0];
	    progname = trace[i][1];
	    function = trace[i][2];
	    line     = trace[i][3];

	    if (line == 0) {
		str = "    ";
	    } else {
		str = "    " + line;
		str = str[strlen(str) - 4 ..];
	    }
	    str += " " + function + " ";
	    len = strlen(function);
	    if (len < 17) {
		str += "                 "[len ..];
	    }
	    str += progname;
	    if (progname != objname) {
		len = strlen(progname);
		if (len < strlen(objname) && progname == objname[.. len - 1]) {
		    str += " (" + objname[len ..] + ")";
		} else {
		    str += " (" + objname + ")";
		}
	    }
	    send_message(str + "\n");
	}
    }
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
	    depth >= 0 && depth <= status()[ST_STACKDEPTH]);
}

# ifdef DEBUG
trace(string s) { send_message(s + "\n"); }
# endif
