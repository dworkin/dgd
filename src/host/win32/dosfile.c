# include <ctype.h>
# include <io.h>
# include <direct.h>
# include <fcntl.h>
# include <sys\stat.h>
# define INCLUDE_FILE_IO
# include "dgd.h"

/*
 * NAME:	path_native()
 * DESCRIPTION:	deal with a path that's already native
 */
char *path_native(char *to, char *from)
{
    to[0] = '/';	/* mark as native */
    strncpy(to + 1, from, STRINGSZ - 1);
    to[STRINGSZ - 1] = '\0';
    return to;
}

/*
 * NAME:	path_file()
 * DESCRIPTION:	translate a path into a native file name
 */
static char *path_file(char *buf, char *path)
{
    bool valid;

    if (path[0] == '/') {
	/* already native */
	strcpy(buf, path + 1);
    } else if (strpbrk(path, ":\\") != (char *) NULL ||
	       strlen(path) >= _MAX_PATH) {
	return (char *) NULL;
    } else {
	strcpy(buf, path);
	valid = FALSE;
	for (path = buf; *path != '\0'; path++) {
	    if (*path == '/') {
		if (!valid) {
		    return (char *) NULL;
		}
		*path = '\\';
		valid = FALSE;
	    } else if (*path != '.') {
		valid = TRUE;
	    }
	}
	if (!valid && strcmp(buf, ".") != 0) {
	    return (char *) NULL;
	}
    }
    return buf;
}

/*
 * NAME:	P->open()
 * DESCRIPTION:	open a file
 */
int P_open(char *file, int flags, int mode)
{
    char buf[STRINGSZ];

    if (path_file(buf, file) == (char *) NULL) {
	return -1;
    }
    return _open(buf, flags, mode);
}

/*
 * NAME:	P->close()
 * DESCRIPTION:	close a file
 */
int P_close(int fd)
{
    return _close(fd);
}

/*
 * NAME:	P->read()
 * DESCRIPTION:	read from a file
 */
int P_read(int fd, char *buf, int nbytes)
{
    return _read(fd, buf, nbytes);
}

/*
 * NAME:	P_write()
 * DESCRIPTION:	write to a file
 */
int P_write(int fd, char *buf, int nbytes)
{
    return _write(fd, buf, nbytes);
}

/*
 * NAME:	P->lseek()
 * DESCRIPTION:	seek on a file
 */
long P_lseek(int fd, long offset, int whence)
{
    return _lseek(fd, offset, whence);
}

/*
 * NAME:	P->stat()
 * DESCRIPTION:	get information about a file
 */
int P_stat(char *path, struct stat *sb)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }
    return _stat(buf, (struct _stat *) sb);
}

/*
 * NAME:	P->fstat()
 * DESCRIPTION:	get information about an open file
 */
int P_fstat(int fd, struct stat *sb)
{
    return _fstat(fd, (struct _stat *) sb);
}

/*
 * NAME:	P->unlink()
 * DESCRIPTION:	remove a file (but not a directory)
 */
int P_unlink(char *path)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }

    return _unlink(buf);
}

/*
 * NAME:	P->rename()
 * DESCRIPTION:	rename a file
 */
int P_rename(char *from, char *to)
{
    char buf1[STRINGSZ], buf2[STRINGSZ];

    if (path_file(buf1, from) == (char *) NULL ||
	path_file(buf2, to) == (char *) NULL) {
	return -1;
    }
    return rename(buf1, buf2);	/* has no underscore for some reason */
}

/*
 * NAME:	P_access()
 * DESCRIPTION:	check access on a file
 */
int P_access(char *path, int mode)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }
    return _access(buf, mode);
}

/*
 * NAME:	P->mkdir()
 * DESCRIPTION:	create a directory
 */
int P_mkdir(char *path, int mode)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }

    return _mkdir(buf);
}

/*
 * NAME:	P_rmdir()
 * DESCRIPTION:	remove an empty directory
 */
int P_rmdir(char *path)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }

    return _rmdir(buf);
}

/*
 * NAME:	P->chdir()
 * DESCRIPTION:	change the current directory (and drive)
 */
int P_chdir(char *dir)
{
    char buf[STRINGSZ];

    if (path_file(buf, dir) == (char *) NULL || _chdir(buf) < 0) {
	return -1;
    }
    if (buf[1] == ':' && _chdrive(toupper(buf[0]) - 'A' + 1) < 0) {
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

    if (path_file(path, dir) == (char *) NULL) {
	return FALSE;
    }
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
