/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2020 DGD Authors (see the commit log for details)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

# include <ctype.h>
# include <io.h>
# include <direct.h>
# include <fcntl.h>
# include <sys\stat.h>
# define INCLUDE_FILE_IO
# include "dgd.h"

/*
 * deal with a path that's already native
 */
char *path_native(char *to, const char *from)
{
    to[0] = '/';	/* mark as native */
    strncpy(to + 1, from, STRINGSZ - 1);
    to[STRINGSZ - 1] = '\0';
    return to;
}

/*
 * translate a path into a native file name
 */
static char *path_file(char *buf, const char *path)
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
	for (char *p = buf; *p != '\0'; p++) {
	    if (*p == '/') {
		if (!valid) {
		    return (char *) NULL;
		}
		*p = '\\';
		valid = FALSE;
	    } else if (*p != '.') {
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
 * open a file
 */
int P_open(const char *file, int flags, int mode)
{
    char buf[STRINGSZ];

    if (path_file(buf, file) == (char *) NULL) {
	return -1;
    }
    return _open(buf, flags, mode);
}

/*
 * close a file
 */
int P_close(int fd)
{
    return _close(fd);
}

/*
 * read from a file
 */
int P_read(int fd, char *buf, int nbytes)
{
    return _read(fd, buf, nbytes);
}

/*
 * write to a file
 */
int P_write(int fd, const char *buf, int nbytes)
{
    return _write(fd, buf, nbytes);
}

/*
 * seek on a file
 */
long P_lseek(int fd, long offset, int whence)
{
    return _lseek(fd, offset, whence);
}

/*
 * get information about a file
 */
int P_stat(const char *path, struct stat *sb)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }
    return _stat(buf, (struct _stat *) sb);
}

/*
 * get information about an open file
 */
int P_fstat(int fd, struct stat *sb)
{
    return _fstat(fd, (struct _stat *) sb);
}

/*
 * remove a file (but not a directory)
 */
int P_unlink(const char *path)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }

    return _unlink(buf);
}

/*
 * rename a file
 */
int P_rename(const char *from, const char *to)
{
    char buf1[STRINGSZ], buf2[STRINGSZ];

    if (path_file(buf1, from) == (char *) NULL ||
	path_file(buf2, to) == (char *) NULL) {
	return -1;
    }
    return rename(buf1, buf2);	/* has no underscore for some reason */
}

/*
 * check access on a file
 */
int P_access(const char *path, int mode)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }
    return _access(buf, mode);
}

/*
 * create a directory
 */
int P_mkdir(const char *path, int mode)
{
    char buf[STRINGSZ];

    UNREFERENCED_PARAMETER(mode);

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }

    return _mkdir(buf);
}

/*
 * remove an empty directory
 */
int P_rmdir(const char *path)
{
    char buf[STRINGSZ];

    if (path_file(buf, path) == (char *) NULL) {
	return -1;
    }

    return _rmdir(buf);
}

/*
 * change the current directory (and drive)
 */
int P_chdir(const char *dir)
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

static intptr_t d;
static struct _finddata_t fdata;

/*
 * open a directory
 */
bool P_opendir(const char *dir)
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
 * read a directory
 */
char *P_readdir()
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
 * close a directory
 */
void P_closedir()
{
    if (d != -1) {
	_findclose(d);
    }
}

/*
 * execute a program
 */
int P_execv(const char *path, char **argv)
{
    UNREFERENCED_PARAMETER(path);
    UNREFERENCED_PARAMETER(argv);
    P_message("Hotbooting not supported on Windows\012");	/* LF */
    return -1;
}
