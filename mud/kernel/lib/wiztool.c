# include <kernel/kernel.h>
# include <kernel/access.h>
# include <kernel/user.h>
# include <config.h>
# include <type.h>


# define HISTORY 200		/* typical session is 100 or so */

private string owner;		/* owner of this object */
private mixed *history;		/* expression history */
private int hindex, hmax;	/* expression table index and size */
private string directory;	/* current directory */
private object driver;		/* driver object */
private object accessd;		/* access daemon */


object query_user();		/* function in inheriting object */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize variables
 */
static create()
{
    owner = query_owner();

    history = allocate(HISTORY);
    hindex = hmax = 0;
    directory = USR + "/" + owner;

    driver = find_object(DRIVER);
    accessd = find_object(ACCESSD);
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
 * NAME:	compile_object()
 * DESCRIPTION:	compile_object wrapper
 */
static object compile_object(string path)
{
    path = driver->normalize_path(path, directory, owner);
    if (!accessd->access(owner, path, WRITE_ACCESS)) {
	query_user()->message(path + ": Access denied.\n");
	return 0;
    }
    return ::compile_object(path);
}

/*
 * NAME:	clone_object()
 * DESCRIPTION:	clone_object wrapper
 */
static object clone_object(string path)
{
    path = driver->normalize_path(path, directory, owner);
    if (!accessd->access(owner, path, READ_ACCESS)) {
	query_user()->message(path + ": Access denied.\n");
	return 0;
    }
    return ::clone_object(path);
}

/*
 * NAME:	read_file()
 * DESCRIPTION:	read_file wrapper
 */
static varargs mixed read_file(string path, int offset, int size)
{
    string result, err;

    path = driver->normalize_path(path, directory, owner);
    if (!accessd->access(owner, path, READ_ACCESS)) {
	query_user()->message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::read_file(path, offset, size));
    if (err) {
	query_user()->message(path + ": " + err + ".\n");
	return -1;
    }
    return result;
}

/*
 * NAME:	write_file()
 * DESCRIPTION:	write_file wrapper
 */
static varargs int write_file(string path, string str, int offset)
{
    int result;
    string err;

    path = driver->normalize_path(path, directory, owner);
    if (!accessd->access(owner, path, WRITE_ACCESS)) {
	query_user()->message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::write_file(path, str, offset));
    if (err) {
	query_user()->message(path + ": " + err + ".\n");
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
    if (!accessd->access(owner, path, WRITE_ACCESS)) {
	query_user()->message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::remove_file(path));
    if (err) {
	query_user()->message(path + ": " + err + ".\n");
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
    if (!accessd->access(owner, from, WRITE_ACCESS)) {
	query_user()->message(from + ": Access denied.\n");
	return -1;
    }
    to = driver->normalize_path(to, directory, owner);
    if (!accessd->access(owner, to, WRITE_ACCESS)) {
	query_user()->message(to + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::rename_file(from, to));
    if (err) {
	query_user()->message(to + ": " + err + ".\n");
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
    if (!accessd->access(owner, path, READ_ACCESS)) {
	return 0;
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
    if (!accessd->access(owner, path, WRITE_ACCESS)) {
	query_user()->message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::make_dir(path));
    if (err) {
	query_user()->message(path + ": " + err + ".\n");
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
    if (!accessd->access(owner, path, READ_ACCESS)) {
	query_user()->message(path + ": Access denied.\n");
	return -1;
    }
    err = catch(result = ::remove_dir(path));
    if (err) {
	query_user()->message(path + ": " + err + ".\n");
	return -1;
    }
    return result;
}

/*
 * NAME:	editor()
 * DESCRIPTION:	editor wrapper
 */
static mixed editor(string cmd)
{
    string result, err;

    err = catch(result = ::editor(cmd));
    if (err) {
	query_user()->message(err + ".\n");
	return -1;
    }
    return result;
}


/*
 * NAME:	subst()
 * DESCRIPTION:	put the value of a $identifier in the argument array; the
 *		default implementation searches for a user by that name
 */
static int subst(string str, mixed *argv, int argc)
{
    object user;

    user = USERD->query_user(str);
    if (user) {
	argv[argc] = user;
	return 1;
    } else {
	return 0;
    }
}

/*
 * NAME:	parse()
 * DESCRIPTION:	parse the argument of code, replacing $num by the proper
 *		historic reference
 */
static mixed *parse(object user, string str)
{
    mixed *argv;
    int argc, len, i, c;
    string result, head, tail, tmp;

    argv = ({ 0 });
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
		    /* error; let DGD's compiler do the complaining */
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
	    argv += ({ 0 });
	    argc++;

	    if (sscanf(str, "%d%s", i, str) != 0) {
		/* $num */
		if (i < 0 || i >= hmax) {
		    user->message("Parse error: $num out of range\n");
		    return 0;
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
		    if (!subst(tmp, argv, argc)) {
			user->message("Parse error: unknown $identifier\n");
			return 0;
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
 * NAME:	dump_value()
 * DESCRIPTION:	return a string describing a value
 */
static string dump_value(mixed value, mapping seen)
{
    string str;
    int i, sz;
    mixed *indices, *values;

    switch (typeof(value)) {
    case T_INT:
    case T_FLOAT:
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
    }
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
	files = 0;
	sscanf(str, "%s %s", str, files);
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
		query_user()->message(str + ": Access denied.\n");
		all[4]++;
		continue;
	    }
	    sz = 0;
	    dir = ::get_dir(str);
	    if (sizeof(dir) != 1 || dir[1][0] != -2) {
		dir = ({ ({ str }), ({ -1 }), ({ 0 }), ({ 0 }) });
	    }
	} else if ((sz=sizeof(strs=dir[0])) == 0) {
	    if (exist > 0 || (exist == 0 && files)) {
		query_user()->message(str + ": No such file or directory.\n");
		all[4]++;
		continue;
	    }
	    dir = ({ ({ str }), ({ -1 }), ({ 0 }), ({ 0 }) });
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
    } while (files);

    return all;
}


/*
 * NAME:	cmd_code()
 * DESCTIPTION:	implementation of the code command
 */
static cmd_code(object user, string cmd, string str)
{
    mixed *parsed, result;
    object obj;
    string err;

    if (!str) {
	user->message("Usage: " + cmd + " LPC-code\n");
	return;
    }

    parsed = parse(user, str);
    str = USR + "/" + owner + "/_code";
    remove_file(str + ".c");
    if (parsed &&
	write_file(str + ".c",
		   "varargs mixed exec(object user, mixed argv...) {\n" +
		   "    mixed " +
		   "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z;\n\n" +
		   "    " + parsed[0] + "\n}\n") > 0) {
	err = catch(obj = compile_object(str),
		    result = obj->exec(user, parsed[1 ..]...));
	if (err) {
	    user->message(err + "\n");
	} else {
	    if (hindex == HISTORY) {
		hindex = 0;
	    }
	    user->message("$" + hindex + " = " + dump_value(result, ([ ])) +
			  "\n");
	    history[hindex] = result;
	    if (++hindex > hmax) {
		hmax = hindex;
	    }
	}

	if (obj) {
	    destruct_object(obj);
	}
	remove_file(str + ".c");
    }
}

/*
 * NAME:	cmd_clear()
 * DESCRIPTION:	clear command history
 */
static cmd_clear(object user, string cmd, string str)
{
    if (str) {
	user->message("Usage: " + cmd + "\n");
	return;
    }

    history = allocate(HISTORY);
    hindex = hmax = 0;
    user->message("Code history cleared.\n");
}

/*
 * NAME:	cmd_compile()
 * DESCRIPTION:	compile an object
 */
static cmd_compile(object user, string cmd, string str)
{
    mixed *files;
    string *names;
    int num, i, len;
    object obj;

    if (!str) {
	user->message("Usage: " + cmd + " file [file ...]\n");
	return;
    }

    files = expand(str, 1, 1);		/* must exist, full filenames */
    names = files[0];
    num = sizeof(names);

    for (i = 0; i < num; i++) {
	str = names[i];
	len = strlen(str);
	if (len < 2 || str[len - 2 ..] != ".c") {
	    user->message("Not an LPC source file: " + str + "\n");
	} else {
	    obj = compile_object(str[ .. len - 3]);
	    if (obj) {
		if (hindex == HISTORY) {
		    hindex = 0;
		}
		user->message("$" + hindex + " = <" + object_name(obj) + ">\n");
		history[hindex] = obj;
		if (++hindex > hmax) {
		    hmax = hindex;
		}
	    }
	}
    }
}

/*
 * NAME:	cmd_clone()
 * DESCRIPTION:	clone an object
 */
static cmd_clone(object user, string cmd, string str)
{
    mixed *files;
    object obj;

    if (!str) {
	user->message("Usage: " + cmd + " obj\n");
	return;
    }

    files = expand(str, -1, 1);		/* may not exist, full filenames */
    if (files[4] != 1) {
	user->message("Usage: " + cmd + " obj\n");
	return;
    }

    if (sizeof(files[0]) == 1) {
	obj = clone_object(files[0][0]);
	if (hindex == HISTORY) {
	    hindex = 0;
	}
	user->message("$" + hindex + " = <" + object_name(obj) + ">\n");
	history[hindex] = obj;
	if (++hindex > hmax) {
	    hmax = hindex;
	}
    }
}


/*
 * NAME:	cmd_cd()
 * DESCRIPTION:	change the current directory
 */
static cmd_cd(object user, string cmd, string str)
{
    mixed *files;

    if (!str) {
	str = "~";
    }

    files = expand(str, -1, 1);		/* may not exist, full filenames */
    if (files[4] == 1) {
	str = files[0][0];
	if (!accessd->access(owner, str + "/.", READ_ACCESS)) {
	    user->message(str + ": Access denied.\n");
	} else {
	    files = ::get_dir(str);
	    if (sizeof(files[0]) == 0) {
		user->message(str + ": No such file or directory.\n");
	    } else if (files[1][0] == -2) {
		user->message(str + "\n");
		if (str == "/") {
		    str = "";
		}
		directory = str;
	    } else {
		user->message(str + ": Not a directory.\n");
	    }
	}
    } else {
	user->message("Usage: " + cmd + " directory\n");
    }
}

/*
 * NAME:	cmd_pwd()
 * DESCRIPTION:	print current directory
 */
static cmd_pwd(object user, string cmd, string str)
{
    if (str) {
	user->message("Usage: " + cmd + "\n");
	return;
    }

    user->message(((directory == "") ? "/" : directory) + "\n");
}

/*
 * NAME:	cmd_ls()
 * DESCRIPTION:	list files
 */
static cmd_ls(object user, string cmd, string str)
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
	    user->message("Usage: " + cmd + " [file ...]\n");
	    return;
	}
    }

    files = expand(str, 1, 0);	/* must exist, short file names */

    if (files[4] == 1 && sizeof(files[0]) == 1 && files[1][0] == -2) {
	str = files[0][0];
	if (str[0] != '/') {
	    str = directory + "/" + str;
	}
	files = get_dir(str + "/*");
	if (!files) {
	    user->message(str + ": Access denied.\n");
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
    user->message(dirlist);
}

/*
 * NAME:	cmd_cp()
 * DESCRIPTION:	copy file(s)
 */
static cmd_cp(object user, string cmd, string str)
{
    mixed *files, chunk;
    int *sizes, num, i, sz, offset, n;
    string *names, *path;

    if (!str) {
	user->message("Usage: " + cmd + " file [file ...] target\n");
	return;
    }

    files = expand(str, 0, 1);	/* last may not exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names) - 1;
    if (files[4] < 2 || (files[4] > 2 && sizes[num] != -2)) {
	user->message("Usage: " + cmd + " file [file ...] target\n");
	return;
    }

    for (i = 0; i < num; i++) {
	if (sizes[i] < 0) {
	    user->message(names[i] + ": Directory (not copied).\n");
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
		    if (chunk == 0) {
			user->message(names[i] +
				      ": No such file or directory.\n");
		    }
		    return;
		}
		n = write_file(str, chunk);
		if (n <= 0) {
		    if (n == 0) {
			user->message(str + ": No such file or directory.\n");
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
static cmd_mv(object user, string cmd, string str)
{
    mixed *files;
    int *sizes, num, i, n;
    string *names, *path;

    if (!str) {
	user->message("Usage: " + cmd + " file [file ...] target\n");
	return;
    }

    files = expand(str, 0, 1);	/* last may not exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names) - 1;
    if (files[4] < 2 || (files[4] > 2 && sizes[num] != -2)) {
	user->message("Usage: " + cmd + " file [file ...] target\n");
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
		user->message(str + ": move failed.\n");
	    }
	    return;
	}
    }
}

/*
 * NAME:	cmd_rm()
 * DESCRIPTION:	remove file(s)
 */
static cmd_rm(object user, string cmd, string str)
{
    mixed *files;
    string *names;
    int *sizes, num, i;

    if (!str) {
	user->message("Usage: " + cmd + " file [file ...]\n");
	return;
    }

    files = expand(str, 1, 1);		/* must exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names);

    for (i = 0; i < num; i++) {
	str = names[i];
	if (sizes[i] == -2) {
	    user->message(str + ": Directory.\n");
	} else {
	    remove_file(str);
	}
    }
}

/*
 * NAME:	cmd_mkdir()
 * DESCRIPTION:	make directory(s)
 */
static cmd_mkdir(object user, string cmd, string str)
{
    mixed *files;
    string *names;
    int *sizes, num, i;

    if (!str) {
	user->message("Usage: " + cmd + " dir [dir ...]\n");
	return;
    }

    files = expand(str, -1, 1);		/* may not exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names);

    for (i = 0; i < num; i++) {
	str = names[i];
	if (sizes[i] != -1) {
	    user->message(str + ": File exists.\n");
	} else if (make_dir(str) == 0) {
	    user->message(str + ": No such file or directory.\n");
	}
    }
}

/*
 * NAME:	cmd_rmdir()
 * DESCRIPTION:	remove directory(s)
 */
static cmd_rmdir(object user, string cmd, string str)
{
    mixed *files;
    string *names;
    int *sizes, num, i;

    if (!str) {
	user->message("Usage: " + cmd + " dir [dir ...]\n");
	return;
    }

    files = expand(str, 1, 1);		/* must exist, full filenames */
    names = files[0];
    sizes = files[1];
    num = sizeof(names);

    for (i = 0; i < num; i++) {
	str = names[i];
	if (sizes[i] != -2) {
	    user->message(str + ": Not a directory.\n");
	} else if (remove_dir(str) == 0) {
	    user->message(str + ": Directory not empty.\n");
	}
    }
}

/*
code
clear (command history)
compile
clone
destruct

cd
pwd
ls
cp
mv
rm
mkdir
rmdir
ed

who
say
emote
tell

grant
ungrant
access
quota
shutdown
status
*/
