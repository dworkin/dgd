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

# include "dgd.h"
# include <dirent.h>

static DIR *d;

/*
 * open a directory
 */
bool P_opendir(const char *dir)
{
    d = opendir(dir);
    return (d != (DIR *) NULL);
}

/*
 * read a directory, skipping . and ..
 */
char *P_readdir()
{
    struct dirent *de;

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
 * close a directory
 */
void P_closedir()
{
    closedir(d);
}
