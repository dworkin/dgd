/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2019 DGD Authors (see the commit log for details)
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

# include "lex.h"
# include "ppstr.h"

/*
 * A string utility for the preprocessor.
 */

# define SCHUNKSZ	8

static Chunk<Str, SCHUNKSZ> schunk;

/*
 * finish string handling
 */
void Str::clear()
{
    schunk.clean();
}

/*
 * constructor
 */
Str::Str(char *buf, int sz)
{
    buffer = buf;
    buffer[0] = '\0';
    size = sz;
    len = 0;
}

/*
 * make a new string with length 0.
 */
Str *Str::create(char *buf, int sz)
{
    return chunknew (schunk) Str(buf, sz);
}

/*
 * append a string. The length becomes -1 if the result is too long
 */
int Str::append(const char *s)
{
    int l;

    if (len < 0 || len + (l = strlen(s)) >= size) {
	return len = -1;
    }
    strcpy(buffer + len, s);
    return len += l;
}

/*
 * append a char. The length becomes -1 if the result is too long
 */
int Str::append(int c)
{
    if (len < 0 || c == '\0' || len + 1 >= size) {
	return len = -1;
    }
    buffer[len++] = c;
    buffer[len] = '\0';
    return len;
}
