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

    sscanf(file, USR + "/%s/%s/", dir, str);
    if (type == READ_ACCESS && (!dir || gaccess[dir] || str == "open") &&
	sscanf(file, "/kernel/data/%*s") == 0 && sscanf(file, "/save/%*s") == 0)
    {
	/*
	 * read access outside /usr, in /usr/foo/open and in selected
	 * other directories in /usr
	 */
	return 1;
    }
    if (user == dir || (sscanf(user, USR + "/%s/", str) != 0 && str == dir)) {
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
    if (file == "/") {
	return access[..];
    }
    return access[file .. file] +
	   (access[file + "/" .. file + "0"] - ({ file + "0" }));
}

/*
 * NAME:	set_access()
 * DESCRIPTION:	set access
 */
set_access(string user, string file, int type)
{
    if (SYSTEM()) {
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
    if (SYSTEM()) {
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
	return access[..];
    }
}

/*
 * NAME:	get_file_access()
 * DESCRIPTION:	get all access to a path
 */
mapping get_file_access(string path)
{
    mapping access, *values;
    string *indices;
    int i, sz;

    access = ([ ]);
    indices = map_indices(uaccess);
    values = map_values(uaccess);
    for (i = 0, sz = sizeof(indices); i < sz; i++) {
	access[indices[i]] = filter_access(values[i], path);
    }

    return access;
}

/*
 * NAME:	set_global_access()
 * DESCRIPTION:	set global read access for a directory
 */
set_global_access(string dir, int flag)
{
    if (SYSTEM()) {
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
