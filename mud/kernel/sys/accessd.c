# include <kernel/kernel.h>
# include <kernel/config.h>
# include <kernel/access.h>
# include <type.h>

/*
 * Interface:
 *
 *	int	access(string user, string file, int type);
 *		set_access(string user, string file, int type);
 *		remove_user_access(string user);
 *	mapping	get_user_access(string user);
 *	mapping	get_file_access(string file);
 *		set_global_access(string dir, int flag);
 *	string *get_global_access();
 */

mapping uaccess;		/* user access */
mapping gaccess;		/* read access under /usr for everyone */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize the access daemon
 */
static create()
{
    uaccess = ([ "admin" : ([ "/" : FULL_ACCESS ]) ]);
    gaccess = ([ "System" : 1 ]);
# ifndef SYS_CONTINUOUS
    restore_object(ACCESSDATA);
# endif
}

/*
 * NAME:	access()
 * DESCRIPTION:	check access
 */
int access(string user, string file, int type)
{
    string dir, str, *path;
    mixed access;
    int i, sz;

    if (type == READ_ACCESS &&
	((sscanf(file, "/usr/%s/%s/", dir, str) == 2 &&
	  (gaccess[dir] || str == "open")) ||
	 (sscanf(file, "/usr/%*s") == 0
# ifndef SYS_CONTINUOUS
	  && sscanf(file, "/data/%*s") == 0	/* no access in /data */
# endif
	 ))) {
	/*
	 * read access outside /usr, in /usr/*/open and in selected
	 * other directories in /usr
	 */
	return 1;
    } else if (sscanf(user, "/usr/%s/", str) != 0 &&
	       strlen(file) >= strlen(str) + 6 &&
	       file[0 .. strlen(str) + 5] == "/usr/" + str + "/") {
	/*
	 * full access to owner directory
	 */
	return 1;
    } else if (strlen(file) >= strlen(user) + 6 &&
	       file[0 .. strlen(user) + 5] == "/usr/" + user + "/") {
	/*
	 * full access to own directory
	 */
	return 1;
    }

    /*
     * check special access
     */
    access = uaccess[user];
    if (access != 0) {
	if (access["/"] >= type) {
	    return 1;
	}

	path = explode(file + "/", "/");
	i = 0;
	sz = sizeof(path);
	file = "";
	do {
	    file += "/" + path[i];
	    if (access[file] >= type) {
		return 1;
	    }
	} while (++i < sz);
    }
    return 0;
}

/*
 * NAME:	filter_access()
 * DESCRIPTION:	filter access mapping for a file or files
 */
private mapping filter_access(mapping access, string file)
{
    mapping filtered;
    string fileslash, *indices, str;
    int *values, len, i, l, h;

    if (file == "/") {
	return access + ([ ]);
    }
    filtered = ([ ]);
    indices = map_indices(access);
    values = map_values(access);

    /*
     * find file in mapping
     */
    fileslash = file + "/";
    len = strlen(file);
    l = 0;
    h = sizeof(indices);
    while (l < h) {
	i = (l + h) / 2;
	str = indices[i];
	if (str < file) {
	    l = i + 1;	/* try again in upper half */
	} else if (str == file ||
		   (strlen(str) > len && str[0 .. len] == fileslash)) {
	    /*
	     * Found it.
	     * Now search forwards and backwards for files with the same
	     * initial path.
	     */
	    l = i;
	    h = sizeof(indices);
	    do {
		filtered[str] = values[l];
	    } while (++l < h && strlen(str=indices[l]) > len &&
		     str[0 .. len] == fileslash);

	    while (--i >= 0 &&
		   ((str=indices[i]) == file ||
		    (strlen(str) > len && str[0 .. len] == fileslash))) {
		filtered[str] = values[i];
	    }
	    break;
	} else {
	    h = i;	/* try again in lower half */
	}
    }

    return filtered;
}

/*
 * NAME:	set_access()
 * DESCRIPTION:	set access
 */
set_access(string user, string file, int type)
{
    if (PRIV1()) {
	if (user == 0 || file == 0 || type < 0 || type > FULL_ACCESS) {
	    error("Bad argument to set_access");
	}

	rlimits (-1; -1) {
	    mapping access;
	    mixed *indices;
	    int i;

	    if (type != 0) {
		/*
		 * add access
		 */
		if (access(user, file, type)) {
		    return;	/* access already exists */
		}

		access = uaccess[user];
		if (access == 0) {
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
	    } else {
		/*
		 * remove access
		 */
		access = uaccess[user];
		if (access != 0) {
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
	    }
# ifndef SYS_CONTINUOUS
	    save_object(ACCESSDATA);
# endif
	}
    }
}

/*
 * NAME:	remove_user_access()
 * DESCRIPTION:	remove all access for a user
 */
remove_user_access(string user)
{
    if (PRIV1()) {
	if (uaccess[user] != 0) {
	    rlimits (-1; -1) {
		uaccess[user] = 0;
# ifndef SYS_CONTINUOUS
		save_object(ACCESSDATA);
# endif
	    }
	}
    }
}

/*
 * NAME:	get_user_access()
 * DESCRIPTION:	get all access for a user
 */
mapping get_user_access(string user)
{
    mapping access;

    access = uaccess[user];
    if (access == 0) {
	return ([ ]);
    } else {
	return access + ([ ]);
    }
}

/*
 * NAME:	get_file_access()
 * DESCRIPTION:	get all access to a path
 */
mapping get_file_access(string path)
{
    mapping access, filtered, *values;
    string *indices;
    int i, sz;

    access = ([ ]);
    indices = map_indices(uaccess);
    values = map_values(uaccess);
    for (i = 0, sz = sizeof(indices); i < sz; i++) {
	filtered = filter_access(values[i], path);
	if (map_sizeof(filtered) != 0) {
	    access[indices[i]] = filtered;
	}
    }

    return access;
}

/*
 * NAME:	set_global_access()
 * DESCRIPTION:	set global read access for a directory
 */
set_global_access(string dir, int flag)
{
    if (PRIV1()) {
	if (dir == 0 || (flag & ~1)) {
	    error("Bad argument to set_global_access");
	}
	gaccess[dir] = flag;
    }
}

/*
 * NAME:	get_global_access()
 * DESCRIPTION:	return the directories under /usr where everyone has read
 *		access
 */
string *get_global_access()
{
    return map_indices(gaccess);
}
