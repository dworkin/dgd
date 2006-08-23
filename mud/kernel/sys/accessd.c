# include <kernel/kernel.h>
# include <kernel/access.h>
# include <type.h>

mapping uaccess;		/* user access */
mapping gaccess;		/* read access under /usr for everyone */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize the access daemon
 */
static void create()
{
    uaccess = ([ ]);
    gaccess = ([ ]);
    catch {
	restore_object(ACCESSDATA);
    }
}

/*
 * NAME:	filter_from_file()
 * DESCRIPTION:	filter access from a file
 */
private mapping filter_from_file(mapping access, string file)
{
    if (file == "/") {
	return access;
    }
    return access[file .. file] +
	   (access[file + "/" .. file + "0"] - ({ file + "0" }));
}

/*
 * NAME:	filter_to_file()
 * DESCRIPTION:	filter access to a file
 */
private mapping filter_to_file(mapping access, string file)
{
    mapping result;
    string *path;
    int i, sz, type;

    if (file == "/") {
	/* special case */
	return ([ "/" : access["/"] ]);
    }

    result = ([ ]);
    path = explode(file, "/");
    file = "";
    for (i = 0, sz = sizeof(path); i < sz; i++) {
	file += "/" + path[i];
	result[file] = access[file];
    }

    return result;
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

	sscanf(file, USR_DIR + "/%s/%s/", dir, str);
	if (type == READ_ACCESS && (!dir || gaccess[dir] || str == "open") &&
	    sscanf(file, "/kernel/data/%*s") == 0) {
	    /*
	     * read access outside /usr, in /usr/foo/open and in selected
	     * other directories in /usr
	     */
	    return TRUE;
	}
	if (user == dir ||
	    (user && sscanf(user, USR_DIR + "/%s/", str) != 0 && str == dir)) {
	    /*
	     * full access to own/owner directory
	     */
	    return TRUE;
	}
	if (user == "admin") {
	    /*
	     * admin always has access (hardcoded)
	     */
	    return TRUE;
	}

	/*
	 * check special access
	 */
	access = uaccess[user];
	if (typeof(access) == T_MAPPING) {
	    if (access["/"] && access["/"] >= type) {
		return TRUE;
	    }

	    path = explode(file, "/");
	    file = "";
	    for (i = 0, sz = sizeof(path) - 1; i < sz; i++) {
		file += "/" + path[i];
		if (access[file] && access[file] >= type) {
		    return TRUE;
		}
	    }
	}
    }
    return FALSE;
}

/*
 * NAME:	add_user()
 * DESCRIPTION:	add a new user
 */
void add_user(string user)
{
    if (previous_program() == API_ACCESS && !uaccess[user]) {
	rlimits (-1; -1) {
	    uaccess[user] = TRUE;
# ifndef SYS_PERSISTENT
	    save_object(ACCESSDATA);
# endif
	}
    }
}

/*
 * NAME:	remove_user()
 * DESCRIPTION:	remove a user
 */
void remove_user(string user)
{
    if (previous_program() == API_ACCESS) {
	if (uaccess[user]) {
	    rlimits (-1; -1) {
		string *users;
		mixed *values, access;
		int i;

		uaccess[user] = nil;
		users = map_indices(uaccess);
		values = map_values(uaccess);
		user = USR_DIR + "/" + user;
		for (i = sizeof(values); --i >= 0; ) {
		    access = values[i];
		    if (typeof(access) == T_MAPPING) {
			access = filter_from_file(access, user);
			if (map_sizeof(access) != 0) {
			    uaccess[users[i]] -= map_indices(access);
			}
		    }
		}
# ifndef SYS_PERSISTENT
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
 * NAME:	save()
 * DESCRIPTION:	force a save of this object, even in a persistent system
 */
void save()
{
    if (SYSTEM()) {
	save_object(ACCESSDATA);
    }
}

/*
 * NAME:	set_access()
 * DESCRIPTION:	set access
 */
void set_access(string user, string file, int type)
{
    if (previous_program() == API_ACCESS) {
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
		if (access(user, file + "/", type)) {
		    return;	/* access already exists */
		}

		if (typeof(access) != T_MAPPING) {
		    /* first special access for this user */
		    uaccess[user] = ([ file : type ]);
		} else {
		    /* remove existing lesser access */
		    indices = map_indices(filter_from_file(access, file));
		    for (i = sizeof(indices); --i >= 0; ) {
			if (access[indices[i]] <= type) {
			    access[indices[i]] = nil;
			}
		    }
		    /* set access */
		    access[file] = (type != 0) ? type : nil;
		}
	    } else if (typeof(access) == T_MAPPING) {
		/*
		 * remove access
		 */
		if (!access[file]) {
		    /* remove all subdir access */
		    indices = map_indices(filter_from_file(access, file));
		    for (i = sizeof(indices); --i >= 0; ) {
			access[indices[i]] = nil;
		    }
		} else {
		    /* remove specific access */
		    access[file] = nil;
		}
		if (map_sizeof(access) == 0) {
		    uaccess[user] = TRUE;
		}
	    }
# ifndef SYS_PERSISTENT
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
    if (previous_program() == API_ACCESS) {
	mixed access;

	access = uaccess[user];
	return (typeof(access) == T_MAPPING) ? access[..] : ([ ]);
    }
}

/*
 * NAME:	query_file_access()
 * DESCRIPTION:	get all access to a file
 */
mapping query_file_access(string file)
{
    if (previous_program() == API_ACCESS) {
	mapping result;
	mixed *values, access;
	string *users;
	int i, sz;

	result = ([ ]);
	users = map_indices(uaccess);
	values = map_values(uaccess);
	for (i = 0, sz = sizeof(values); i < sz; i++) {
	    access = values[i];
	    if (typeof(access) == T_MAPPING) {
		access = filter_to_file(access, file);
		if (map_sizeof(access) != 0) {
		    result[users[i]] = access;
		}
	    }
	}
	return result;
    }
}

/*
 * NAME:	set_global_access()
 * DESCRIPTION:	set global read access for a directory
 */
void set_global_access(string dir, int flag)
{
    if (previous_program() == API_ACCESS) {
	rlimits (-1; -1) {
	    gaccess[dir] = (flag != 0) ? flag : nil;
# ifndef SYS_PERSISTENT
	    save_object(ACCESSDATA);
# endif
	}
    }
}

/*
 * NAME:	query_global_access()
 * DESCRIPTION:	return the directories under /usr where everyone has read
 *		access
 */
string *query_global_access()
{
    if (previous_program() == API_ACCESS) {
	return map_indices(gaccess);
    }
}
