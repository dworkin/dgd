/* a collection of file functions used in the auto object and elsewhere */

/*
 * NAME:	creator()
 * DESCRIPTION:	get creator of file
 */
private string creator(string file)
{
    return (sscanf(file, "/usr/%s/", file) != 0) ?
	    file :
	    (sscanf(file, "/kernel/%*s") != 0) ? "System" : 0;
}

/*
 * NAME:	reduce_path()
 * DESCRIPTION:	reduce a path to its minimal absolute form
 */
private string reduce_path(string file, string dir, string creator)
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
	    file = "/usr/" + creator + file[1 .. ];
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
