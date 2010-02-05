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
# include <time.h>
# include <sys/time.h>

/*
 * NAME:	P->time()
 * DESCRIPTION:	return the current time
 */
Uint P_time()
{
    return (Uint) time((time_t *) NULL);
}

/*
 * NAME:	P->mtime()
 * DESCRIPTION:	return the current time in milliseconds
 */
Uint P_mtime(milli)
unsigned short *milli;
{
    struct timeval time;

    gettimeofday(&time, (struct timezone *) NULL);
    *milli = time.tv_usec / 1000;
    return (Uint) time.tv_sec;
}

/*
 * NAME:	P->ctime()
 * DESCRIPTION:	convert the given time to a string
 */
char *P_ctime(buf, time)
char *buf;
Uint time;
{
    register int offset;
    time_t t;

    offset = 0;
    for (offset = 0; time >= 2147397248L; time -= 883612800L, offset += 28) ;
    t = time;
    memcpy(buf, ctime(&t), 26);
    if (offset != 0) {
	long year;

	year = strtol(buf + 20, (char **) NULL, 10) + offset;
	if (year > 2100 ||
	    (year == 2100 && (buf[4] != 'J' || buf[5] != 'a') &&
	     (buf[4] != 'F' || (buf[8] == '2' && buf[9] == '9')))) {
	    /* 2100 is not a leap year */
	    t -= 378604800L;
	    offset += 12;
	    memcpy(buf, ctime(&t), 26);
	    year = strtol(buf + 20, (char **) NULL, 10) + offset;
	}
	sprintf(buf + 20, "%ld\012", year);
    }
    return buf;
}
