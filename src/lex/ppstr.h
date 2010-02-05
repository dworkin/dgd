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

typedef struct {
    char *buffer;	/* string buffer */
    int size;		/* size of buffer */
    int len;		/* lenth of string (-1 if illegal) */
} str;

extern void pps_init	P((void));
extern void pps_clear	P((void));
extern str *pps_new	P((char*, int));
extern void pps_del	P((str*));
extern int  pps_scat	P((str*, char*));
extern int  pps_ccat	P((str*, int));
