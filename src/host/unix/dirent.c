# include "dgd.h"
# include <dirent.h>

static DIR *d;

/*
 * NAME:	_opendir()
 * DESCRIPTION:	open a directory
 */
bool _opendir(dir)
char *dir;
{
    d = opendir(dir);
    return (d != (DIR *) NULL);
}

/*
 * NAME:	_readdir()
 * DESCRIPTION:	read a directory, skipping . and ..
 */
char *_readdir()
{
    register struct dirent *de;

    do {
	de = readdir(d);
	if (de == (struct dirent *) NULL) {
	    return (char *) NULL;
	}
    } while (de->d_name[0] == '.' && (de->d_name[1] == '\0' ||
	     (de->d_name[1] == '.' && de->d_name[2] == '\0')));
    return de->d_name;
}

/*
 * NAME:	_closedir()
 * DESCRIPTION:	close a directory
 */
void _closedir()
{
    closedir(d);
}
