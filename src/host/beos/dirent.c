# include <dirent.h>
# include "dgd.h"

static DIR *d;

/*
 * NAME:	P->opendir()
 * DESCRIPTION:	open a directory
 */
bool P_opendir(char *dir)
{
    d = opendir(dir);
    return (d != (DIR *) NULL);
}

/*
 * NAME:	P->readdir()
 * DESCRIPTION:	read a directory, skipping . and ..
 */
char *P_readdir()
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
 * NAME:	P->closedir()
 * DESCRIPTION:	close a directory
 */
void P_closedir()
{
    closedir(d);
}
