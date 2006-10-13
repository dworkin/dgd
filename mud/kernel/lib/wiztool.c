# include <kernel/kernel.h>
# include <kernel/access.h>
# include <kernel/rsrc.h>
# include <kernel/user.h>
# include <type.h>
# include <status.h>

inherit access	API_ACCESS;
inherit rsrc	API_RSRC;
inherit user	API_USER;


# define SPACE16	"                "

private string owner;		/* owner of this object */
private mixed *history;		/* expression history */
private int hsize, hindex, hmax; /* expression table size and index */
private string directory;	/* current directory */
private object driver;		/* driver object */

static void message(string str);

/*
 * NAME:	create()
 * DESCRIPTION:	initialize variables
 */
static void create(int size)
{
    access::create();
    rsrc::create();
    user::create();

    owner = query_owner();

    history = allocate(hsize = size);
    hindex = hmax = 0;
    directory = USR_DIR + "/" + owner;

    driver = find_object(DRIVER);
}

/*
 * NAME:	query_directory()
 * DESCRIPTION:	return the current directory
 */
string query_directory()
{
    return directory;
}

/*
 * NAME:	query_history()
 * DESCRIPTION:	return the current value history
 */
static mixed *query_history()
{
    return history[..];
}


/*
 * NAME:	access()
 * DESCRIPTION:	check access
 */
static nomask int access(string user, string file, int type)
{
    return ::access(user, file, type);
}

/*
 * NAME:	add_user()
 * DESCRIPTION:	add a new user
 */
static void add_user(string user)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Insufficient access granting privileges.\n");
    } else {
	::add_user(user);
    }
}

/*
 * NAME:	remove_user()
 * DESCRIPTION:	remove a user
 */
static void remove_user(string user)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Insufficient access granting privileges.\n");
    } else {
	::remove_user(user);
    }
}

/*
 * NAME:	set_access()
 * DESCRIPTION:	set access
 */
static void set_access(string user, string file, int type)
{
    if (!access(owner, (type == FULL_ACCESS) ? "/" : file + "/*", FULL_ACCESS))
    {
	message("Insufficient access granting privileges.\n");
    } else {
	::set_access(user, file, type);
    }
}

/*
 * NAME:	set_global_access()
 * DESCRIPTION:	set global read access for a directory
 */
static void set_global_access(string dir, int flag)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Insufficient access granting privileges.\n");
    } else {
	::set_global_access(dir, flag);
    }
}


/*
 * NAME:	add_owner()
 * DESCRIPTION:	add a new resource owner
 */
static void add_owner(string rowner)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::add_owner(rowner);
    }
}

/*
 * NAME:	remove_owner()
 * DESCRIPTION:	remove a resource owner
 */
static void remove_owner(string rowner)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::remove_owner(rowner);
    }
}

/*
 * NAME:	set_rsrc()
 * DESCRIPTION:	set the maximum, decay percentage and decay period of a
 *		resource
 */
static void set_rsrc(string name, int max, int decay, int period)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::set_rsrc(name, max, decay, period);
    }
}

/*
 * NAME:	remove_rsrc()
 * DESCRIPTION:	remove a resource
 */
static void remove_rsrc(string name)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::remove_rsrc(name);
    }
}

/*
 * NAME:	query_rsrc()
 * DESCRIPTION:	query a resource
 */
static mixed *query_rsrc(string name)
{
    mixed *rsrc;

    rsrc = ::query_rsrc(name);
    if (name == "objects") {
	rsrc[RSRC_USAGE] += sizeof(query_connections());
    }
    return rsrc;
}

/*
 * NAME:	rsrc_set_limit()
 * DESCRIPTION:	set individual resource limit
 */
static void rsrc_set_limit(string rowner, string name, int max)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::rsrc_set_limit(rowner, name, max);
    }
}

/*
 * NAME:	rsrc_get()
 * DESCRIPTION:	get individual resource usage
 */
static mixed *rsrc_get(string owner, string name)
{
    mixed *rsrc;

    rsrc = ::rsrc_get(owner, name);
    if (owner == "System" && name == "objects") {
	rsrc[RSRC_USAGE] += sizeof(query_connections());
    }
    return rsrc;
}

/*
 * NAME:	rsrc_incr()
 * DESCRIPTION:	increment or decrement a resource, returning TRUE if succeeded,
 *		FALSE if failed
 */
static int rsrc_incr(string rowner, string name, mixed index, int incr,
		     varargs int force)
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
	return FALSE;
    } else {
	return ::rsrc_incr(rowner, name, index, incr, force);
    }
}


/*
 * NAME:	compile_object()
 * DESCRIPTION:	compile_object wrapper
 */
static object compile_object(string path, varargs string code)
{
    path = driver->normalize_path(path, directory, owner);
    if (!access(owner, path,
		(sscanf(path, "/kernel/%*s") == 0 &&
		 sscanf(path, "%*s" + INHERITABLE_SUBDIR) != 0) ?
		 READ_ACCESS : WRITE_ACCESS)) {
	message(path + ": Permission denied.\n");
	return nil;
    }
    return (code) ? ::compile_object(path, code) : ::compile_object(path);
}

/*
 * NAME:	clone_object()
 * DESCRIPTION:	clone_object wrapper
 */
static object clone_object(string path)
{
    path = driver->normalize_path(path, directory, owner);
    if (sscanf(path, "/kernel/%*s") != 0 || !access(owner, path, READ_ACCESS)) {
	message(path + ": Permission denied.\n");
	return nil;
    }
    return ::clone_object(path);
}

/*
 * NAME:	destruct_object()
 * DESCRIPTION:	destruct_object wrapper
 */
static int destruct_object(mixed obj)
{
    string path, oowner;
    int lib;

    switch (typeof(obj)) {
    case T_STRING:
	path = obj = driver->normalize_path(obj, directory, owner);
	lib = sscanf(path, "%*s" + INHERITABLE_SUBDIR);
	if (lib) {
	    oowner = driver->creator(path);
	} else {
	    obj = find_object(path);
	    if (!obj) {
		return FALSE;
	    }
	    oowner = obj->query_owner();
	}
	break;

    case T_OBJECT:
	path = object_name(obj);
	lib = sscanf(path, "%*s" + INHERITABLE_SUBDIR);
	oowner = obj->query_owner();
	break;
    }

    if (path && owner != oowner &&
	((sscanf(path, "/kernel/%*s") != 0 && !lib) ||
	 !access(owner, path, WRITE_ACCESS))) {
	message(path + ": Permission denied.\n");
	return -1;
    }
    return ::destruct_object(obj);
}

/*
 * NAME:	new_object()
 * DESCRIPTION:	new_object wrapper
 */
static object new_object(string path)
{
    path = driver->normalize_path(path, directory, owner);
    if (sscanf(path, "/kernel/%*s") != 0 || !access(owner, path, READ_ACCESS)) {
	message(path + ": Permission denied.\n");
	return nil;
    }
    return ::new_object(path);
}

/*
 * NAME:	read_file()
 * DESCRIPTION:	read_file wrapper
 */
static mixed read_file(string path, varargs int offset, int size)
{
    string result, err;

    path = driver->normalize_path(path, directory, owner);
    if (!access(owner, path, READ_ACCESS)) {
	message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::read_file(path, offset, size));
    if (err) {
	message(path + ": " + err + ".\n");
	return -1;
    }
    return result;
}

/*
 * NAME:	write_file()
 * DESCRIPTION:	write_file wrapper
 */
static int write_file(string path, string str, varargs int offset)
{
    int result;
    string err;

    path = driver->normalize_path(path, directory, owner);
    if (!access(owner, path, WRITE_ACCESS)) {
	message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::write_file(path, str, offset));
    if (err) {
	message(path + ": " + err + ".\n");
	return -1;
    }
    return result;
}

/*
 * NAME:	remove_file()
 * DESCRIPTION:	remove_file wrapper
 */
static int remove_file(string path)
{
    int result;
    string err;

    path = driver->normalize_path(path, directory, owner);
    if (!access(owner, path, WRITE_ACCESS)) {
	message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::remove_file(path));
    if (err) {
	message(path + ": " + err + ".\n");
	return -1;
    }
    return result;
}

/*
 * NAME:	rename_file()
 * DESCRIPTION:	rename_file wrapper
 */
static int rename_file(string from, string to)
{
    int result;
    string err;

    from = driver->normalize_path(from, directory, owner);
    if (!access(owner, from, WRITE_ACCESS)) {
	message(from + ": Access denied.\n");
	return -1;
    }
    to = driver->normalize_path(to, directory, owner);
    if (!access(owner, to, WRITE_ACCESS)) {
	message(to + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::rename_file(from, to));
    if (err) {
	message(to + ": " + err + ".\n");
	return -1;
    }
    return result;
}

/*
 * NAME:	get_dir()
 * DESCRIPTION:	get_dir wrapper
 */
static mixed **get_dir(string path)
{
    path = driver->normalize_path(path, directory, owner);
    if (!access(owner, path, READ_ACCESS)) {
	return nil;
    }
    return ::get_dir(path);
}

/*
 * NAME:	make_dir()
 * DESCRIPTION:	make_dir wrapper
 */
static int make_dir(string path)
{
    int result;
    string err;

    path = driver->normalize_path(path, directory, owner);
    if (!access(owner, path, WRITE_ACCESS)) {
	message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::make_dir(path));
    if (err) {
	message(path + ": " + err + ".\n");
	return -1;
    }
    return result;
}

/*
 * NAME:	remove_dir()
 * DESCRIPTION:	remove_dir wrapper
 */
static int remove_dir(string path)
{
    int result;
    string err;

    path = driver->normalize_path(path, directory, owner);
    if (!access(owner, path, WRITE_ACCESS)) {
	message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::remove_dir(path));
    if (err) {
	message(path + ": " + err + ".\n");
	return -1;
    }
    return result;
}

/*
 * NAME:	path_read()
 * DESCRIPTION:	determine editor read access
 */
nomask string path_read(string path)
{
    path = driver->normalize_path(path, directory, owner);
    return (access(owner, path, READ_ACCESS)) ? path : nil;
}

/*
 * NAME:	path_write()
 * DESCRIPTION:	determine editor write access
 */
nomask string path_write(string path)
{
    path = driver->normalize_path(path, directory, owner);
    return (access(owner, path, WRITE_ACCESS)) ? path : nil;
}

/*
 * NAME:	swapout()
 * DESCRIPTION:	swap out all objects
 */
static void swapout()
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::swapout();
    }
}

/*
 * NAME:	dump_state()
 * DESCRIPTION:	create a state dump
 */
static void dump_state()
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::dump_state();
    }
}

/*
 * NAME:	shutdown()
 * DESCRIPTION:	shut down the system
 */
static void shutdown()
{
    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::shutdown();
    }
}


/*
 * NAME:	dump_value()
 * DESCRIPTION:	return a string describing a value
 */
static string dump_value(mixed value, mapping seen)
{
    string str;
    int i, sz;
    mixed *indices, *values;

    switch (typeof(value)) {
    case T_FLOAT:
	str = (string) value;
	if (sscanf(str, "%*s.") == 0 && sscanf(str, "%*se") == 0) {
	    str += ".0";
	}
	return str;

    case T_INT:
	return (string) value;

    case T_STRING:
	str = value;
	if (sscanf(str, "%*s\\") != 0) {
	    str = implode(explode("\\" + str + "\\", "\\"), "\\\\");
	}
	if (sscanf(str, "%*s\"") != 0) {
	    str = implode(explode("\"" + str + "\"", "\""), "\\\"");
	}
	if (sscanf(str, "%*s\n") != 0) {
	    str = implode(explode("\n" + str + "\n", "\n"), "\\n");
	}
	if (sscanf(str, "%*s\t") != 0) {
	    str = implode(explode("\t" + str + "\t", "\t"), "\\t");
	}
	return "\"" + str + "\"";

    case T_OBJECT:
	return "<" + object_name(value) + ">";

    case T_ARRAY:
	if (seen[value]) {
	    return "#" + (seen[value] - 1);
	}

	seen[value] = map_sizeof(seen) + 1;
	sz = sizeof(value);
	if (sz == 0) {
	    return "({ })";
	}

	str = "({ ";
	for (i = 0, --sz; i < sz; i++) {
	    str += dump_value(value[i], seen) + ", ";
	}
	return str + dump_value(value[i], seen) + " })";

    case T_MAPPING:
	if (seen[value]) {
	    return "@" + (seen[value] - 1);
	}

	seen[value] = map_sizeof(seen) + 1;
	sz = map_sizeof(value);
	if (sz == 0) {
	    return "([ ])";
	}

	str = "([ ";
	indices = map_indices(value);
	values = map_values(value);
	for (i = 0, --sz; i < sz; i++) {
	    str += dump_value(indices[i], seen) + ":" +
		   dump_value(values[i], seen) + ", ";
	}
	return str + dump_value(indices[i], seen) + ":" +
		     dump_value(values[i], seen) + " ])";

    case T_NIL:
	return "nil";
    }
}

/*
 * NAME:	store()
 * DESCRIPTION:	store a value in the history table
 */
static void store(mixed value)
{
    if (hindex == hsize) {
	hindex = 0;
    }
    message("$" + hindex + " = " + dump_value(value, ([ ])) + "\n");
    history[hindex] = value;
    if (++hindex > hmax) {
	hmax = hindex;
    }
}

/*
 * NAME:	fetch()
 * DESCRIPTION:	fetch a value from the history table
 */
static mixed fetch(int num)
{
    if (num < 0 || num >= hmax) {
	error("$num out of range");
    }

    return history[num];
}

/*
 * NAME:	ident()
 * DESCRIPTION:	return the (object) value of $identifier or 0
 */
static object ident(string str)
{
    return find_user(str);
}

/*
 * NAME:	parse_code()
 * DESCRIPTION:	parse the argument of code, replacing $num by the proper
 *		historic reference
 */
static mixed *parse_code(string str)
{
    mixed *argv;
    int argc, len, i, c;
    string result, head, tail, tmp;

    argv = ({ nil });
    argc = 0;
    result = "";

    /* search for $ */
    while (sscanf(str, "%s$%s", head, str) != 0) {
	/*
	 * see if the $ is preceded by a quote
	 */
	if (sscanf(head, "%s\"%s", tmp, tail) != 0 ||
	    sscanf(head, "%s'%s", tmp, tail) != 0) {

	    result += head[.. strlen(tmp)];
	    str = tail + "$" + str;
	    len = strlen(tmp);
	    tmp = head[len .. len];	/* " or ' */

	    /* search finishing quote */
	    do {
		if (sscanf(str, "%s" + tmp + "%s", head, str) == 0) {
		    /* error; let the compiler do the complaining */
		    argv[0] = result + str;
		    return argv;
		}

		result += head + tmp;

		/*
		 * doesn't count if it's preceded by an odd number of
		 * backslashes
		 */
		len = strlen(head);
		while (len != 0 && head[len - 1] == '\\') {
		    if (len == 1 || head[len - 2] != '\\') {
			break;
		    }
		    len -= 2;
		}
	    } while (len != 0 && head[len - 1] == '\\');

	} else {
	    /*
	     * $ not enclosed in quotes: interpret it
	     */
	    result += head + "(argv[" + argc + "])";
	    argv += ({ nil });
	    argc++;

	    if (sscanf(str, "%d%s", i, str) != 0) {
		/* $num */
		if (i < 0 || i >= hmax) {
		    message("Error: $num out of range.\n");
		    return nil;
		}
		argv[argc] = history[i];
	    } else {
		len = strlen(str);
		if (len != 0 && (((c=str[0]) >= 'a' && c <= 'z') || c == '_' ||
				 c == '.' || (c >= 'A' && c <= 'Z'))) {
		    /* $identifier */
		    for (i = 1;
			 i < len && (((c=str[i]) >= 'a' && c <= 'z') ||
				     c == '_' || (c >= '0' && c <= '9') ||
				     c == '.' || (c >= 'A' && c <= 'Z'));
			 i++) ;
		    tmp = str[.. i - 1];
		    str = str[i ..];
		    if (!(argv[argc]=ident(tmp))) {
			message("Error: Unknown $ident.\n");
			return nil;
		    }
		}
	    }
	}
    }

    result += str;
    len = strlen(result);
    if (len != 0 && result[len - 1] != ';' && result[len - 1] != '}') {
	result = "return " + result + ";";
    }
    argv[0] = result;
    return argv;
}

/*
 * NAME:	parse_obj()
 * DESCRIPTION:	get an object from a string
 */
static mixed parse_obj(string str)
{
    int i;
    mixed obj;

    if (!str) {
	return 0;
    }
    if (sscanf(str, "$%d%s", i, str) != 0) {
	if (str != "") {
	    return 0;
	}
	if (i < 0 || i >= hmax) {
	    message("$num out of range.\n");
	    return nil;
	}
	obj = history[i];
	if (typeof(obj) != T_OBJECT) {
	    message("Not an object.\n");
	    return nil;
	}
    } else if (sscanf(str, "$%s", str) != 0) {
	obj = ident(str);
	if (!obj) {
	    message("Unknown $ident.\n");
	    return nil;
	}
    } else {
	obj = driver->normalize_path(str, directory, owner);
    }

    return obj;
}

/*
 * NAME:	expand()
 * DESCRIPTION:	expand file name(s)
 */
static mixed *expand(string files, int exist, int full)
{
    mixed *all, *dir;
    string str, file, *strs;
    int i, sz;

    all = ({ ({ }), ({ }), ({ }), ({ }), 0 });

    do {
	str = files;
	files = nil;
	sscanf(str, "%s %s", str, files);
	if (strlen(str) != 0) {
	    file = driver->normalize_path(str, directory, owner);
	    dir = get_dir(file);

	    if (full || (i=strlen(str)) == 0 || (i=strlen(file) - i) < 0 ||
		file[i ..] != str) {
		str = file;
		strs = explode(file, "/");
		file = "/" + implode(strs[.. sizeof(strs) - 2], "/");
		if (file != "/") {
		    file += "/";
		}
	    } else {
		strs = explode("/" + str, "/");
		if (sizeof(strs) <= 1) {
		    file = "";
		} else {
		    file = implode(strs[.. sizeof(strs) - 2], "/") + "/";
		}
	    }

	    if (!dir) {
		if (exist != 0 || files) {
		    message(str + ": Access denied.\n");
		    all[4]++;
		    continue;
		}
		sz = 0;
		dir = ::get_dir(str);
		if (sizeof(dir) != 1 || dir[1][0] != -2) {
		    dir = ({ ({ str }), ({ -1 }), ({ 0 }), ({ nil }) });
		}
	    } else if ((sz=sizeof(strs=dir[0])) == 0) {
		if (exist > 0 || (exist == 0 && files)) {
		    message(str + ": No such file or directory.\n");
		    all[4]++;
		    continue;
		}
		dir = ({ ({ str }), ({ -1 }), ({ 0 }), ({ nil }) });
	    }

	    for (i = 0; i < sz; i++) {
		str = strs[i];
		strs[i] = (str == ".") ? "/" : file + str;
	    }

	    all[0] += dir[0];
	    all[1] += dir[1];
	    all[2] += dir[2];
	    all[3] += dir[3];
	    all[4] += sizeof(dir[0]);
	}
    } while (files);

    return all;
}


/*
 * NAME:	cmd_code()
 * DESCRIPTION:	implementation of the code command
 */
static void cmd_code(object user, string cmd, string str)
{
    mixed *parsed, result;
    object obj;
    string name;

    if (!str) {
	message("Usage: " + cmd + " <LPC-code>\n");
	return;
    }

    parsed = parse_code(str);
    name = USR_DIR + "/" + owner + "/_code";
    obj = find_object(name);
    if (obj) {
	destruct_object(obj);
    }
    if (!parsed) {
	return;
    }

    str = USR_DIR + "/" + owner + "/include/code.h";
    if (file_info(str)) {
	str = "# include \"~/include/code.h\"\n";
    } else {
	str = "";
    }
    str = "# include <float.h>\n# include <limits.h>\n" +
	  "# include <status.h>\n# include <trace.h>\n" +
	  "# include <type.h>\n" + str + "\n" +
	  "mixed exec(object user, mixed argv...) {\n" +
	  "    mixed a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z;\n\n" +
	  "    " + parsed[0] + "\n}\n";
    str = catch(obj = compile_object(name, str),
		result = obj->exec(user, parsed[1 ..]...));
    if (str) {
	message("Error: " + str + ".\n");
    } else {
	store(result);
    }

    if (obj) {
	destruct_object(obj);
    }
}

/*
 * NAME:	cmd_history()
 * DESCRIPTION:	show command history
 */
static void cmd_history(object user, string cmd, string str)
{
    int num, i;

    num = 10;
    if (str && (sscanf(str, "%d%s", num, str) == 0 || num <= 0 ||
		strlen(str) != 0)) {
	message("Usage: " + cmd + " [<num>]\n");
	return;
    }

    if (num > hmax) {
	num = hmax;
    }
    for (i = hsize + hindex - num; --num >= 0; i++) {
	if (i >= hmax) {
	    i -= hsize;
	}
	message("$" + i + " = " + dump_value(history[i], ([ ])) + "\n");
    }
}

/*
 * NAME:	cmd_clear()
 * DESCRIPTION:	clear command history
 */
static void cmd_clear(object user, string cmd, string str)
{
    if (str) {
	message("Usage: " + cmd + "\n");
	return;
    }

    history = allocate(hsize);
    hindex = hmax = 0;
    message("Code history cleared.\n");
}

/*
 * NAME:	cmd_compile()
 * DESCRIPTION:	compile an object
 */
static void cmd_compile(object user, string cmd, string str)
{
    mixed *files;
    string *names;
    int num, i, len;
    object obj;

    if (!str) {
	message("Usage: " + cmd + " <file> [<file> ...]\n");
	return;
    }

    files = expand(str, 1, TRUE);	/* must exist, full filenames */
    names = files[0];
    num = sizeof(names);

    for (i = 0; i < num; i++) {
	str = names[i];
	len = strlen(str);
	if (len < 2 || str[len - 2 ..] != ".c") {
	    message("Not an LPC source file: " + str + "\n");
	} else {
	    obj = compile_object(str[ .. len - 3]);
	    if (obj) {
		store(obj);
	    }
	}
    }
}

/*
 * NAME:	cmd_clone()
 * DESCRIPTION:	clone an object
 */
static void cmd_clone(object user, string cmd, string str)
{
    mixed obj;

    obj = parse_obj(str);
    switch (typeof(obj)) {
    case T_INT:
	message("Usage: " + cmd + " <obj> | $<ident>\n");
    case T_NIL:
	return;

    case T_STRING:
	str = obj;
	obj = find_object(str);
	break;

    case T_OBJECT:
	str = object_name(obj);
	break;
    }
	
    if (sscanf(str, "%*s" + CLONABLE_SUBDIR + "%*s#") != 1) {
	message("Not a master object.\n");
    } else if (!obj) {
	message("No such object.\n");
    } else {
	str = catch(obj = clone_object(str));
	if (str) {
	    message(str + ".\n");
	} else if (obj) {
	    store(obj);
	}
    }
}

/*
 * NAME:	cmd_destruct()
 * DESCRIPTION:	destruct an object
 */
static void cmd_destruct(object user, string cmd, string str)
{
    mixed obj;
    int flag;

    obj = parse_obj(str);
    switch (typeof(obj)) {
    case T_INT:
	message("Usage: " + cmd + " <obj> | $<ident>\n");
    case T_NIL:
	return;
    }

    str = catch(flag = destruct_object(obj));
    if (str) {
	message(str + ".\n");
    } else if (flag == 0) {
	message("No such object.\n");
    }
}


/*
 * NAME:	cmd_cd()
 * DESCRIPTION:	change the current directory
 */
static void cmd_cd(object user, string cmd, string str)
{
    mixed *info;

    if (!str) {
	str = "~";
    }

    info = expand(str, -1, TRUE);	/* may not exist, full filenames */
    if (info[4] == 1) {
	if (sizeof(info[0]) == 1) {
	    str = info[0][0];
	    info = file_info(str);
	    if (!info) {
		message(str + ": No such file or directory.\n");
	    } else if (info[0] < 0) {
		directory = (str == "/") ? "" : str;
		message(str + "\n");
	    } else {
		message(str + ": Not a directory.\n");
	    }
	}
    } else {
	message("Usage: " + cmd + " <directory>\n");
    }
}

/*
 * NAME:	cmd_pwd()
 * DESCRIPTION:	print current directory
 */
static void cmd_pwd(object user, string cmd, string str)
{
    if (str) {
	message("Usage: " + cmd + "\n");
	return;
    }

    message(((directory == "") ? "/" : directory) + "\n");
}

/*
 * NAME:	cmd_ls()
 * DESCRIPTION:	list files
 */
static void cmd_ls(object user, string cmd, string str)
{
    mixed *files, *objects;
    string *names, timestr, dirlist;
    int *sizes, *times, long, ancient, i, j, sz, max, len, rows, time;

    if (!str) {
	str = ".";
    } else if (sscanf(str, "-%s", str) != 0) {
	long = 1;
	if (str == "l") {
	    str = ".";
	} else if (sscanf(str, "l %s", str) == 0) {
	    message("Usage: " + cmd + " [-l] [<file> ...]\n");
	    return;
	}
    }

    files = expand(str, 1, FALSE);	/* must exist, short file names */

    if (files[4] == 1 && sizeof(files[0]) == 1 && files[1][0] == -2) {
	str = files[0][0];
	if (str[0] != '/') {
	    str = directory + "/" + str;
	}
	files = get_dir(str + "/*");
	if (!files) {
	    message(str + ": Access denied.\n");
	    return;
	}
    }

    names = files[0];
    sz = sizeof(names);
    if (sz == 0) {
	return;
    }
    sizes = files[1];
    times = files[2];
    objects = files[3];

    for (i = 0; i < sz; i++) {
	j = strlen(names[i]);
	if (j > max) {
	    max = j;
	}
	j = sizes[i];
	if (j > len) {
	    len = j;
	}
    }
    if (long) {
	len = strlen((string) len) + 1;
	max += len + 14;
	ancient = time() - 6 * 30 * 24 * 60 * 60;
    }

    max += 2;
    j = (79 + 2) / (max + 1);
    if (j == 0) {
	rows = sz;
    } else {
	rows = (sz + j - 1) / j;
    }

    dirlist = "";
    for (i = 0; i < rows; i++) {
	j = i;
	for (;;) {
	    if (long) {
		str = "            ";
		if (sizes[j] >= 0) {
		    str += (string) sizes[j];
		}

		time = times[j];
		timestr = ctime(time);
		if (time >= ancient) {
		    timestr = timestr[3 .. 15];
		} else {
		    timestr = timestr[3 .. 10] + timestr[19 .. 23];
		}
		str = str[strlen(str) - len ..] + timestr + " " + names[j];
	    } else {
		str = names[j];
	    }

	    if (sizes[j] < 0) {
		str += "/";
	    } else if (objects[j]) {
		str += "*";
	    }
	    j += rows;
	    if (j >= sz) {
		dirlist += str + "\n";
		break;
	    }
	    dirlist += (str + "                                        ")
		       [0 .. max];
	}
    }
    message(dirlist);
}

/*
 * NAME:	cmd_cp()
 * DESCRIPTION:	copy file(s)
 */
static void cmd_cp(object user, string cmd, string str)
{
    mixed *files, chunk;
    int *sizes, num, i, sz, offset, n;
    string *names, *path;

    if (!str) {
	message("Usage: " + cmd + " <file> [<file> ...] <target>\n");
	return;
    }

    files = expand(str, 0, TRUE);	/* last may not exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names) - 1;
    if (files[4] < 2 || (files[4] > 2 && sizes[num] != -2)) {
	message("Usage: " + cmd + " <file> [<file> ...] <target>\n");
	return;
    }

    for (i = 0; i < num; i++) {
	if (sizes[i] < 0) {
	    message(names[i] + ": Directory (not copied).\n");
	} else {
	    if (sizes[num] == -2) {
		path = explode(names[i], "/");
		str = names[num] + "/" + path[sizeof(path) - 1];
	    } else {
		str = names[num];
	    }

	    if (remove_file(str) < 0) {
		return;
	    }
	    offset = 0;
	    sz = sizes[i];
	    do {
		chunk = read_file(names[i], offset, 57344);
		if (typeof(chunk) != T_STRING) {
		    if (!chunk) {
			message(names[i] + ": No such file or directory.\n");
		    }
		    return;
		}
		n = write_file(str, chunk);
		if (n <= 0) {
		    if (n == 0) {
			message(str + ": No such file or directory.\n");
		    }
		    return;
		}
		offset += strlen(chunk);
		sz -= strlen(chunk);
	    } while (sz > 0 && strlen(chunk) != 0);
	}
    }
}

/*
 * NAME:	cmd_mv()
 * DESCRIPTION:	move file(s)
 */
static void cmd_mv(object user, string cmd, string str)
{
    mixed *files;
    int *sizes, num, i, n;
    string *names, *path;

    if (!str) {
	message("Usage: " + cmd + " <file> [<file> ...] <target>\n");
	return;
    }

    files = expand(str, 0, TRUE);	/* last may not exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names) - 1;
    if (files[4] < 2 || (files[4] > 2 && sizes[num] != -2)) {
	message("Usage: " + cmd + " <file> [<file> ...] <target>\n");
	return;
    }

    for (i = 0; i < num; i++) {
	if (sizes[num] == -2) {
	    path = explode(names[i], "/");
	    str = names[num] + "/" + path[sizeof(path) - 1];
	} else {
	    str = names[num];
	}

	if (remove_file(str) < 0) {
	    return;
	}
	n = rename_file(names[i], str);
	if (n <= 0) {
	    if (n == 0) {
		message(str + ": move failed.\n");
	    }
	    return;
	}
    }
}

/*
 * NAME:	cmd_rm()
 * DESCRIPTION:	remove file(s)
 */
static void cmd_rm(object user, string cmd, string str)
{
    mixed *files;
    string *names;
    int *sizes, num, i;

    if (!str) {
	message("Usage: " + cmd + " <file> [<file> ...]\n");
	return;
    }

    files = expand(str, 1, TRUE);		/* must exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names);

    for (i = 0; i < num; i++) {
	str = names[i];
	if (sizes[i] == -2) {
	    message(str + ": Directory.\n");
	} else {
	    remove_file(str);
	}
    }
}

/*
 * NAME:	cmd_mkdir()
 * DESCRIPTION:	make directory(s)
 */
static void cmd_mkdir(object user, string cmd, string str)
{
    mixed *files;
    string *names;
    int *sizes, num, i;

    if (!str) {
	message("Usage: " + cmd + " <directory> [<directory> ...]\n");
	return;
    }

    files = expand(str, -1, TRUE);	/* may not exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names);

    for (i = 0; i < num; i++) {
	str = names[i];
	if (sizes[i] != -1) {
	    message(str + ": File exists.\n");
	} else if (make_dir(str) == 0) {
	    message(str + ": No such file or directory.\n");
	}
    }
}

/*
 * NAME:	cmd_rmdir()
 * DESCRIPTION:	remove directory(s)
 */
static void cmd_rmdir(object user, string cmd, string str)
{
    mixed *files;
    string *names;
    int *sizes, num, i;

    if (!str) {
	message("Usage: " + cmd + " <directory> [<directory> ...]\n");
	return;
    }

    files = expand(str, 1, TRUE);	/* must exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names);

    for (i = 0; i < num; i++) {
	str = names[i];
	if (sizes[i] != -2) {
	    message(str + ": Not a directory.\n");
	} else if (remove_dir(str) == 0) {
	    message(str + ": Directory not empty.\n");
	}
    }
}

/*
 * NAME:	cmd_ed()
 * DESCRIPTION:	handle editor
 */
static void cmd_ed(object user, string cmd, string str)
{
    mixed *files;

    if (str) {
	files = expand(str, -1, FALSE);
	if (files[4] != 1) {
	    message("Usage: " + cmd + " [<file>]\n");
	    return;
	}
	if (sizeof(files[0]) == 0) {
	    return;
	}
	str = editor("e " + files[0][0]);
    } else {
	str = editor();
    }
    if (str) {
	message(str);
    }
}


/*
 * NAME:	list_access()
 * DESCRIPTION:	return an access listing in string form
 */
private string list_access(mapping access)
{
    string str, *files;
    int i, *values;

    files = map_indices(access);
    values = map_values(access);
    for (i = sizeof(files); --i >= 0; ) {
	switch (values[i]) {
	case READ_ACCESS:
	    files[i] += " [read-only]";
	    break;

	case FULL_ACCESS:
	    files[i] += " [full]";
	    break;
	}
    }

    return " " + implode(files, "\n ") + "\n";
}

/*
 * NAME:	cmd_access()
 * DESCRIPTION:	list special access
 */
static void cmd_access(object user, string cmd, string str)
{
    mapping access, *values;
    mixed *files;
    string *users;
    int i, sz;

    if (!str) {
	str = owner;
    }

    if (str == "global") {
	str = implode(query_global_access(), "\n " + USR_DIR + "/");
	if (strlen(str) != 0) {
	    message("Global read access:\n " + USR_DIR + "/" + str + "\n");
	}
    } else if (sizeof(query_users() & ({ str })) != 0) {
	access = query_user_access(str);
	switch (map_sizeof(access)) {
	case 0:
	    message(str + " has no special access.\n");
	    break;

	case 1:
	    message(str + " has access to:" + list_access(access));
	    break;

	default:
	    message(str + " has access to:\n" + list_access(access));
	    break;
	}
    } else {
	if (sscanf(str, "%*s ") != 0 || (files=expand(str, 0, TRUE))[4] != 1) {
	    message("Usage: " + cmd + " <user> | global | <directory>\n");
	    return;
	}
	str = files[0][0];
	access = query_file_access(str);
	users = map_indices(access);
	if (sizeof(users) != 0) {
	    values = map_values(access);
	    for (i = 0, sz = sizeof(users); i < sz; i++) {
		access = values[i];
		message(users[i] + ((map_sizeof(access) == 1) ?
				     " has access to:" : " has access to:\n") +
				   list_access(values[i]));
	    }
	} else {
	    message("No special access to " + str + ".\n");
	}
    }
}

/*
 * NAME:	cmd_grant()
 * DESCRIPTION:	grant access
 */
static void cmd_grant(object user, string cmd, string str)
{
    string who, dir;
    mixed type, *files;

    if (!str ||
	(sscanf(str, "%s %s %s", who, dir, type) != 3 &&
	 sscanf(str, "%s %s", who, dir) != 2) ||
	(who == "global" && type) ||
	((dir == "access") ? type : (files=expand(dir, 0, TRUE))[4] != 1)) {
	message(
	    "Usage: " + cmd + " <user> access\n" +
	    "       " + cmd + " <user> <directory> [read | write | full]\n" +
	    "       " + cmd + " global <directory>\n");
	return;
    }

    str = (dir == "access") ? dir : files[0][0];
    switch (type) {
    case "read":
	type = READ_ACCESS;
	break;

    case nil:
    case "write":
	type = WRITE_ACCESS;
	break;

    case "full":
	type = FULL_ACCESS;
	break;

    default:
	message(
	    "Usage: " + cmd + " <user> access\n" +
	    "       " + cmd + " <user> <directory> [read | write | full]\n" +
	    "       " + cmd + " global <directory>\n");
	return;
    }

    if (who == "global") {
	/*
	 * global access
	 */
	if (sscanf(str, USR_DIR + "/%s", str) == 0 || sscanf(str, "%*s/") != 0)
	{
	    message("Global read access is for directories under " + USR_DIR +
		    " only.\n");
	} else if (sizeof(query_global_access() & ({ str })) != 0) {
	    message("That global access already exists.\n");
	} else {
	    set_global_access(str, TRUE);
	}
    } else if (dir == "access") {
	/*
	 * file access
	 */
	if (sscanf(who, "%*s/") != 0) {
	    message("Invalid user name.\n");
	} else if (sizeof(query_users() & ({ who })) != 0) {
	    message(who + " already has file access.\n");
	} else if (!access(owner, "/", FULL_ACCESS)) {
	    message("Insufficient access granting privileges.\n");
	} else {
	    ::add_user(who);
	    ::add_owner(who);
	    ::make_dir(USR_DIR + "/" + who);
	}
    } else {
	/*
	 * special access
	 */
	if (sizeof(query_users() & ({ who })) == 0) {
	    message(who + " has no file access.\n");
	} else if (access(who, str + "/*", type)) {
	    message(who + " already has that access.\n");
	} else {
	    set_access(who, str, type);
	}
    }
}

/*
 * NAME:	cmd_ungrant()
 * DESCRIPTION:	remove access
 */
static void cmd_ungrant(object user, string cmd, string str)
{
    string who, dir;
    mixed *files;

    if (!str || sscanf(str, "%s %s", who, dir) != 2 ||
	(dir != "access" &&
	 (sscanf(dir, "%*s ") != 0 || (files=expand(dir, 0, TRUE))[4] != 1))) {
	message("Usage: " + cmd + " <user> access\n" +
		"       " + cmd + " <user> <directory>\n" +
		"       " + cmd + " global <directory>\n");
	return;
    }

    str = (dir == "access") ? dir : files[0][0];

    if (who == "global") {
	/*
	 * global access
	 */
	if (sscanf(str, USR_DIR + "/%s", str) == 0 || sscanf(str, "%*s/") != 0)
	{
	    message("Global read access is for directories under " + USR_DIR +
		    " only.\n");
	} else if (sizeof(query_global_access() & ({ str })) == 0) {
	    message("That global access does not exist.\n");
	} else {
	    set_global_access(str, FALSE);
	}
    } else if (dir == "access") {
	/*
	 * file access
	 */
	if (sizeof(query_users() & ({ who })) == 0) {
	    message(who + " has no file access.\n");
	} else {
	    remove_user(who);
	}
    } else {
	/*
	 * special access
	 */
	if (sizeof(query_users() & ({ who })) == 0) {
	    message(who + " has no file access.\n");
	} else if (!query_user_access(who)[str]) {
	    message(who + " has no such access.\n");
	} else {
	    set_access(who, str, 0);
	}
    }
}


/*
 * NAME:	ralign()
 * DESCRIPTION:	return a number as a right-aligned string
 */
private string ralign(mixed num, int width)
{
    string str;

    str = SPACE16 + (string) num;
    return str[strlen(str) - width ..];
}

/*
 * NAME:	list_resources()
 * DESCRIPTION:	create a listing of resource usage, limits etc
 */
private string list_resources(string name, string *names, mixed *resources)
{
    int i, n;
    mixed *rsrc;
    string str, unit;

    for (i = sizeof(names); --i >= 0; ) {
	rsrc = resources[i];
	str = (names[i] + SPACE16)[.. 15] + ralign(rsrc[RSRC_USAGE], 14) +
	      ralign(rsrc[RSRC_MAX], 13);
	if ((int) rsrc[RSRC_DECAY] != 0) {
	    str += ralign(rsrc[RSRC_DECAY], 6) + "%";
	}
	if ((int) rsrc[RSRC_PERIOD] != 0) {
	    switch (n = rsrc[RSRC_PERIOD]) {
	    case 1 .. 60 - 1:
		unit = "second";
		break;

	    case 60 .. 60 * 60 - 1:
		n /= 60;
		unit = "minute";
		break;

	    case 60 * 60 .. 24 * 60 * 60 - 1:
		n /= 60 * 60;
		unit = "hour";
		break;

	    default:
		n /= 24 * 60 * 60;
		unit = "day";
		break;
	    }
	    str += " per " + ((n == 1) ? unit : n + " " + unit + "s");
	}
	resources[i] = str;
    }

    return (name + SPACE16)[.. 15] +
			   "         usage          max  decay  period\n" +
	   "----------------+-------------+------------+------+---------\n" +
	   implode(resources, "\n") + "\n";
}

/*
 * NAME:	cmd_quota()
 * DESCRIPTION:	resource quota command
 */
static void cmd_quota(object user, string cmd, string str)
{
    int limit, i;
    string who, rsrc, *names;
    mixed **resources;

    if (!str) {
	/* list resource usage of owner */
	who = owner;
    } else if (sscanf(str, "%s %s", who, str) != 0) {
	if (who == "Ecru") {
	    who = nil;
	}
	if (sizeof(query_owners() & ({ who })) == 0) {
	    message("No such resource owner.\n");
	    return;
	}

	if (sscanf(str, "%s%d%s", rsrc, limit, str) == 0) {
	    /* show single resource */
	    message(list_resources("resource", ({ str }),
				   ({ rsrc_get(who, str) })));
	} else {
	    /* change resource */
	    if (strlen(str) != 0 || (i=strlen(rsrc)) == 0 || rsrc[i - 1] != ' ')
	    {
		message("Usage: " + cmd + " [<user> [<rsrc> [<limit>]]]\n");
		return;
	    }
	    rsrc = rsrc[.. i - 2];

	    if (sizeof(query_resources() & ({ rsrc })) == 0) {
		message("No such resource.\n");
		return;
	    }

	    str = catch(rsrc_set_limit(who, rsrc, limit));
	    if (str) {
		message(str + ".\n");
	    }
	}
	return;
    } else {
	/* list resource usage of given user */
	who = str;
    }

    if (who == "Ecru") {
	who = nil;
    }
    if (sizeof(query_owners() & ({ who })) == 0) {
	message("No such resource owner.\n");
	return;
    }

    names = query_resources();
    resources = allocate(i = sizeof(names));
    while (--i >= 0) {
	resources[i] = rsrc_get(who, names[i]);
    }
    message(list_resources("resources", names, resources));
}

/*
 * NAME:	cmd_rsrc()
 * DESCRIPTION:	deal with resources
 */
static void cmd_rsrc(object user, string cmd, string str)
{
    int i, sz, limit;
    string *names, name;
    mixed **resources, *rsrc;

    if (!str) {
	names = query_resources();
	resources = allocate(i = sizeof(names));
	while (--i >= 0) {
	    resources[i] = query_rsrc(names[i]);
	}
	message(list_resources("resources", names, resources));
    } else if (sscanf(str, "%s%d%s", name, limit, str) != 0) {
	if (strlen(str) != 0 || (i=strlen(name)) == 0 || name[i - 1] != ' ') {
	    message("Usage: " + cmd + " [<resource> [<limit>]]\n");
	    return;
	}
	name = name[.. i - 2];
	if (sizeof(query_resources() & ({ name })) == 0) {
	    message("No such resource.\n");
	    return;
	}

	rsrc = query_rsrc(name);
	str = catch(set_rsrc(name, limit, rsrc[RSRC_DECAY], rsrc[RSRC_PERIOD]));
	if (str) {
	    message(str + ".\n");
	}
    } else {
	if (sizeof(query_resources() & ({ str })) == 0) {
	    message("No such resource.\n");
	    return;
	}
	names = query_owners();
	resources = allocate(i = sz = sizeof(names));
	while (--i >= 0) {
	    resources[i] = rsrc_get(names[i], str);
	}
	if (sz != 0 && !names[0]) {
	    names[0] = "Ecru";
	}

	message(list_resources("owner", names, resources));
    }
}

/*
 * NAME:	cmd_people()
 * DESCRIPTION:	show users logged on via telnet, with ip numbers
 */
static void cmd_people(object user, string cmd, string str)
{
    object *users, usr;
    string *owners, name;
    int i, sz;

    if (str) {
	message("Usage: " + cmd + "\n");
	return;
    }

    str = "";
    users = users();
    owners = query_owners();
    for (i = 0, sz = sizeof(users); i < sz; i++) {
	usr = users[i];
	name = usr->query_name();
	while (usr <- LIB_USER) {
	    usr = usr->query_conn();
	}
	str += query_ip_number(usr) + "\t" +
	       ((sizeof(owners & ({ name })) == 0) ? " " : "*") +
	       name + "\n";
    }
    message(str);
}

/*
 * NAME:	swapnum()
 * DESCRIPTION:	return a swap average in X.XX format
 */
private string swapnum(int num, int div)
{
    string str;

    str = (string) ((float) num / (float) div);
    str += (sscanf(str, "%*s.") != 0) ? "00" : ".00";
    if (strlen(str) > 4) {
	str = (str[3] == '.') ? str[.. 2] : str[.. 3];
    }
    return str;
}

/*
 * NAME:	cmd_status()
 * DESCRIPTION:	show driver status
 */
static void cmd_status(object user, string cmd, string str)
{
    mixed *status;
    int uptime, hours, minutes, seconds;
    int short, long;
    int i;
    mixed obj;

    if (!str) {
	status = status();
	str =
"                                          Server:       " +
  (string) status[ST_VERSION] + "\n" +
"------------ Swap device -------------\n" +
"sectors:  " + ralign(status[ST_SWAPUSED], 9) + " / " +
	       ralign(status[ST_SWAPSIZE], 9) + " (" +
  ralign((int) status[ST_SWAPUSED] * 100 / (int) status[ST_SWAPSIZE], 3) +
  "%)    Start time:   " + ctime(status[ST_STARTTIME])[4 ..] + "\n" +
"sector size:   " + (((float) status[ST_SECTORSIZE] / 1024.0) + "K" +
		     SPACE16)[..15];
	if ((int) status[ST_STARTTIME] != (int) status[ST_BOOTTIME]) {
	    str += "           Last reboot:  " +
		   ctime(status[ST_BOOTTIME])[4 ..];
	}

	uptime = status[ST_UPTIME];
	seconds = uptime % 60;
	uptime /= 60;
	minutes = uptime % 60;
	uptime /= 60;
	hours = uptime % 24;
	uptime /= 24;
	short = status[ST_NCOSHORT];
	long = status[ST_NCOLONG];
	i = sizeof(query_connections());
	str += "\n" +
"swap average:  " + (swapnum(status[ST_SWAPRATE1], 60) + ", " +
		     swapnum(status[ST_SWAPRATE5], 300) + SPACE16)[.. 15] +
  "           Uptime:       " +
  ((uptime == 0) ? "" : uptime + ((uptime == 1) ? " day, " : " days, ")) +
  ralign("00" + hours, 2) + ":" + ralign("00" + minutes, 2) + ":" +
  ralign("00" + seconds, 2) + "\n\n" +
"--------------- Memory ---------------" +
  "    ------------ Callouts ------------\n" +
"static:   " + ralign(status[ST_SMEMUSED], 9) + " / " +
	       ralign(status[ST_SMEMSIZE], 9) + " (" +
  ralign((int) ((float) status[ST_SMEMUSED] * 100.0 /
		(float) status[ST_SMEMSIZE]), 3) +
  "%)    short term:   " + ralign(short, 5) + "         (" +
  ((short + long == 0) ? "  0" : ralign(100 - long * 100 / (short + long), 3)) +
  "%)\n" +
"dynamic:  " + ralign(status[ST_DMEMUSED], 9) + " / " +
	       ralign(status[ST_DMEMSIZE], 9) + " (" +
  ralign((int) ((float) status[ST_DMEMUSED] * 100.0 /
	 (float) status[ST_DMEMSIZE]), 3) +
  "%) +  other:        " + ralign(long, 5) + "         (" +
  ((short + long == 0) ? "  0" : ralign(long * 100 / (short + long), 3)) +
  "%) +\n" +
"          " +
  ralign((int) status[ST_SMEMUSED] + (int) status[ST_DMEMUSED], 9) + " / " +
  ralign((int) status[ST_SMEMSIZE] + (int) status[ST_DMEMSIZE], 9) + " (" +
  ralign((int) (((float) status[ST_SMEMUSED] +
		 (float) status[ST_DMEMUSED]) * 100.0 /
		((float) status[ST_SMEMSIZE] +
		 (float) status[ST_DMEMSIZE])), 3) +
  "%)                  " + ralign(short + long, 5) + " / " +
			   ralign(status[ST_COTABSIZE], 5) + " (" +
  ralign((short + long) * 100 / (int) status[ST_COTABSIZE], 3) + "%)\n\n" +
"Objects:  " + ralign(status[ST_NOBJECTS], 9) + " / " +
	       ralign(status[ST_OTABSIZE], 9) + " (" +
  ralign((int) status[ST_NOBJECTS] * 100 / (int) status[ST_OTABSIZE], 3) +
  "%)    Connections:  " + ralign(i, 5) + " / " +
			   ralign(status[ST_UTABSIZE], 5) + " (" +
  ralign(i * 100 / (int) status[ST_UTABSIZE], 3) + "%)\n\n";
    } else {
	i = -1;
	if (!str || (sscanf(str, "$%d%s", i, str) != 0 &&
		     (i < 0 || i >= hmax || str != ""))) {
	    message("Usage: " + cmd + " [<obj> | $<ident>]\n");
	    return;
	}

	if (i >= 0) {
	    obj = history[i];
	    if (typeof(obj) != T_OBJECT) {
		message("Not an object.\n");
		return;
	    }
	} else if (sscanf(str, "$%s", str) != 0) {
	    obj = ident(str);
	    if (!obj) {
		message("Unknown $ident.\n");
		return;
	    }
	} else {
	    obj = driver->normalize_path(str, directory, owner);
	}

	str = catch(status = status(obj));
	if (str) {
	    str += ".\n";
	} else if (!status) {
	    str = "No such object.\n";
	} else {
	    if (typeof(obj) == T_OBJECT) {
		obj = object_name(obj);
	    }
	    str = driver->creator(obj);
	    if (!str) {
		str = "Ecru";
	    }
	    str = "Object:      <" + obj + ">" +
"\nCompiled at: " + ctime(status[O_COMPILETIME])[4 ..] +
  "    Program size: " + (int) status[O_PROGSIZE] +
"\nCreator:     " + (str + SPACE16)[.. 16] +
  "       Variables:    " + (int) status[O_DATASIZE] +
"\nOwner:       " + (((obj=find_object(obj)) ?
		     (obj=obj->query_owner()) ? obj : "Ecru" :
		     str) + SPACE16)[.. 16] +
  "       Callouts:     " + sizeof(status[O_CALLOUTS]) +
"\nMaster ID:   " + ((int) status[O_INDEX] + SPACE16)[.. 16] +
  "       Sectors:      " + (int) status[O_NSECTORS] + "\n";
	}
    }

    message(str);
}

/*
 * NAME:	cmd_swapout()
 * DESCRIPTION:	swap out all objects
 */
static void cmd_swapout(object user, string cmd, string str)
{
    if (str) {
	message("Usage: " + cmd + "\n");
	return;
    }

    swapout();
}

/*
 * NAME:	cmd_statedump()
 * DESCRIPTION:	create a state dump
 */
static void cmd_statedump(object user, string cmd, string str)
{
    if (str) {
	message("Usage: " + cmd + "\n");
	return;
    }

    dump_state();
}

/*
 * NAME:	cmd_shutdown()
 * DESCRIPTION:	shut down the system
 */
static void cmd_shutdown(object user, string cmd, string str)
{
    if (str) {
	message("Usage: " + cmd + "\n");
	return;
    }

    shutdown();
}

/*
 * NAME:	cmd_reboot()
 * DESCRIPTION:	reboot system
 */
static void cmd_reboot(object user, string cmd, string str)
{
    if (str) {
	message("Usage: " + cmd + "\n");
	return;
    }

    if (!access(owner, "/", FULL_ACCESS)) {
	message("Permission denied.\n");
    } else {
	::dump_state();
	::shutdown();
    }
}
