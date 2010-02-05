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

extern void srand48	P((long));
extern long lrand48	P((void));

/*
 * NAME:	P->srandom()
 * DESCRIPTION:	set the random seed
 */
void P_srandom(s)
long s;
{
    srand48(s);
}

/*
 * NAME:	P->random()
 * DESCRIPTION:	return a long random number
 */
long P_random()
{
    return lrand48();
}
