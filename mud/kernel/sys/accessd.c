# include <kernel/kernel.h>
# include <kernel/access.h>
# include <config.h>
# include <type.h>

mapping uaccess;		/* user access */
mapping gaccess;		/* read access under /usr for everyone */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize the access daemon
 */
static create()
{
    uaccess = ([ ]);
    gaccess = ([ ]);
# ifndef SYS_CONTINUOUS
    restore_object(ACCESSDATA);
# endif
}

/*
 * NAME:	filter_access()
 * DESCRIPTION:	filter access mapping for a file or files
 */
private mapping filter_access(mapping access, string file)
{
    if (file == "/") {
	return access[..];
    }
    return access[file .. file] +
	   (access[file + "/" .. file + "0"] - ({ file + "0" }));
}

/*
 * NAME:	access()
 * DESCRIPTION:	check access
 */
int access(string user, string file, int type)
{
    if (KERNEL()) {
	string dir, str, *path;
	mixed access;
	int i, sz;

	sscanf(file, USR + "/%s/%s/", dir, str);
	if (type == READ_ACCESS && (!dir || gaccess[dir] || str == "open") &&
	    sscanf(file, "/kernel/data/%*s") == 0 &&
	    sscanf(file, "/save/%*s") == 0) {
	    /*
	     * read access outside /usr, in /usr/foo/open and in selected
	     * other directories in /usr
	     */
	    return 1;
	}
	if (user == dir || (sscanf(user, USR + "/%s/", str) != 0 && str == dir))
	{
	    /*
	     * full access to own/owner directory
	     */
	    return 1;
	}
	if (user == "admin") {
	    /*
	     * admin always has access (hardcoded)
	     */
	    return 1;
	}

	/*
	 * check special access
	 */
	access = uaccess[user];
	if (typeof(access) == T_MAPPING) {
	    if (access["/"] >= type) {
		return 1;
	    }

	    path = explode(file + "/", "/");
	    file = "";
	    for (i = 0, sz = sizeof(path); i < sz; i++) {
		file += "/" + path[i];
		if (access[file] >= type) {
		    return 1;
		}
	    } while (++i < sz);
	}
    }
}

/*
 * NAME:	add_user()
 * DESCRIPTION:	add a new user
 */
add_user(string user)
{
    if (previous_program() == LIB_ACCESS && !uaccess[user]) {
	rlimits (-1; -1) {
	    uaccess[user] = 1;
# ifndef SYS_CONTINUOUS
	    save_object(ACCESSDATA);
# endif
	}
    }
}

/*
 * NAME:	remove_user()
 * DESCRIPTION:	remove a user
 */
remove_user(string user)
{
    if (previous_program() == LIB_ACCESS) {
	if (uaccess[user]) {
	    rlimits (-1; -1) {
		string *users;
		mapping *values, access;
		int i;

		uaccess[user] = 0;
		users = map_indices(uaccess);
		values = map_values(uaccess);
		user = USR + "/" + user;
		for (i = sizeof(values); --i >= 0; ) {
		    access = filter_access(values[i], user);
		    if (map_sizeof(access) != 0) {
			uaccess[users[i]] -= map_indices(access);
		    }
		}
# ifndef SYS_CONTINUOUS
		save_object(ACCESSDATA);
# endif
	    }
	}
    }
}

/*
 * NAME:	query_users()
 * DESCRIPTION:	return list of users
 */
string *query_users()
{
    if (KERNEL()) {
	return map_indices(uaccess);
    }
}

/*
 * NAME:	set_access()
 * DESCRIPTION:	set access
 */
set_access(string user, string file, int type)
{
    if (previous_program() == LIB_ACCESS) {
	mixed access, *indices;
	int i;

	access = uaccess[user];
	if (!access) {
	    error("No such user");
	}

	rlimits (-1; -1) {
	    if (type != 0) {
		/*
		 * add access
		 */
		if (access(user, file, type)) {
		    return;	/* access already exists */
		}

		if (typeof(access) != T_MAPPING) {
		    /* first special access for this user */
		    uaccess[user] = ([ file : type ]);
		} else {
		    /* remove existing lesser access */
		    indices = map_indices(filter_access(access, file));
		    for (i = sizeof(indices); --i >= 0; ) {
			if (access[indices[i]] <= type) {
			    access[indices[i]] = 0;
			}
		    }
		    /* set access */
		    access[file] = type;
		}
	    } else if (typeof(access) == T_MAPPING) {
		/*
		 * remove access
		 */
		if (access[file] == 0) {
		    /* remove all subdir access */
		    indices = map_indices(filter_access(access, file));
		    for (i = sizeof(indices); --i >= 0; ) {
			access[indices[i]] = 0;
		    }
		} else {
		    /* remove specific access */
		    access[file] = 0;
		}
		if (map_sizeof(access) == 0) {
		    uaccess[user] = 0;
		}
	    }
# ifndef SYS_CONTINUOUS
	    save_object(ACCESSDATA);
# endif
	}
    }
}

/*
 * NAME:	query_user_access()
 * DESCRIPTION:	get all access for a user
 */
mapping query_user_access(string user)
{
    if (previous_program() == LIB_ACCESS) {
	mixed access;

	access = uaccess[user];
	return (typeof(access) == T_MAPPING) ? access[..] : ([ ]);
    }
}

/*
 * NAME:	query_file_access()
 * DESCRIPTION:	get all access to a path
 */
mapping query_file_access(string path)
{
    if (previous_program() == LIB_ACCESS) {
	mapping access, *values;
	string *users;
	int i, sz;

	access = ([ ]);
	users = map_indices(uaccess);
	values = map_values(uaccess);
	for (i = 0, sz = sizeof(users); i < sz; i++) {
	    access[users[i]] = filter_access(values[i], path);
	}

	return access;
    }
}

/*
 * NAME:	set_global_access()
 * DESCRIPTION:	set global read access for a directory
 */
set_global_access(string dir, int flag)
{
    if (previous_program() == LIB_ACCESS) {
	gaccess[dir] = flag;
    }
}

/*
 * NAME:	query_global_access()
 * DESCRIPTION:	return the directories under /usr where everyone has read
 *		access
 */
string *query_global_access()
{
    if (previous_program() == LIB_ACCESS) {
	return map_indices(gaccess);
    }
}
