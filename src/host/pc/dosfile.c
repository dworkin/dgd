# include "dgd.h"
# include <ctype.h>
# include <io.h>
# include <direct.h>

/*
 * NAME:	path->file()
 * DESCRIPTION:	translate a path into a local file name
 */
char *path_file(char *path)
{
    static char file[_MAX_PATH];

    if (path == (char *) NULL || strpbrk(path, ":\\") != (char *) NULL ||
	strlen(path) >= _MAX_PATH) {
	return (char *) NULL;
    }
    strcpy(file, path);
    for (path = file; *path != '\0'; path++) {
	if (*path == '/') {
	    *path = '\\';
	}
    }
    return file;
}

/*
 * NAME:	path->unfile()
 * DESCRIPTION:	translate a local file name into a path
 */
char *path_unfile(char *file)
{
    static char path[STRINGSZ];

    strncpy(path, file, STRINGSZ - 1);
    for (file = path; *file != '\0'; file++) {
	if (*file == '\\') {
	    *file = '/';
	}
    }
    return path;
}

/*
 * NAME:	P->chdir()
 * DESCRIPTION:	change the current directory (and drive)
 */
int P_chdir(char *dir)
{
    if (_chdir(dir) < 0) {
	return -1;
    }
    if (dir[1] == ':' && _chdrive(toupper(dir[0]) - 'A' + 1) < 0) {
	return -1;
    }
    return 0;
}

static long d;
static struct _finddata_t fdata;

/*
 * NAME:	P->opendir()
 * DESCRIPTION:	open a directory
 */
char P_opendir(char *dir)
{
    char path[_MAX_PATH + 2];

    strcpy(path, dir);
    strcat(path, "\\*");
    d = _findfirst(path, &fdata);
    return (d != -1);
}

/*
 * NAME:	P->readdir()
 * DESCRIPTION:	read a directory
 */
char *P_readdir(void)
{
    static struct _finddata_t fd;

    do {
	if (d == -1) {
	    return (char *) NULL;
	}
	fd = fdata;
	if (_findnext(d, &fdata) != 0) {
	    _findclose(d);
    	    d = -1;
	}
    } while (fd.name[0] == '.' &&
	     (fd.name[1] == '\0' ||
	      (fd.name[1] == '.' && fd.name[2] == '\0')));
    return fd.name;
}

/*
 * NAME:	P->closedir()
 * DESCRIPTION:	close a directory
 */
void P_closedir(void)
{
    if (d != -1) {
	_findclose(d);
    }
}
