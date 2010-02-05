/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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
# include <sys/dir.h>

static DIR *d;

/*
 * NAME:	P->opendir()
 * DESCRIPTION:	open a directory
 */
bool P_opendir(dir)
char *dir;
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
    register struct direct *de;

    do {
	de = readdir(d);
	if (de == (struct direct *) NULL) {
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
