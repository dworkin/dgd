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

# include "lex.h"
# include "ppstr.h"

/*
 * A string utility for the preprocessor.
 */

# define SCHUNKSZ	8

typedef struct _schunk_ {
    struct _schunk_ *next;	/* next in list */
    str s[SCHUNKSZ];		/* chunk of pp strings */
} schunk;

static schunk *slist;		/* list of pps string chunks */
static int schunksz;		/* size of current chunk */
static str *flist;		/* list of free pp strings */

/*
 * NAME:	str->init()
 * DESCRIPTION:	initialize string handling
 */
void pps_init()
{
    slist = (schunk *) NULL;
    schunksz = SCHUNKSZ;
    flist = (str *) NULL;
}

/*
 * NAME:	str->clear()
 * DESCRIPTION:	finish string handling
 */
void pps_clear()
{
    register schunk *l, *f;

    for (l = slist; l != (schunk *) NULL; ) {
	f = l;
	l = l->next;
	FREE(f);
    }
    slist = (schunk *) NULL;
}

/*
 * NAME:	str->new()
 * DESCRIPTION:	make a new string with length 0.
 */
str *pps_new(buf, sz)
char *buf;
int sz;
{
    register str *sb;

    if (flist != (str *) NULL) {
	/* from free list */
	sb = flist;
	flist = (str *) sb->buffer;
    } else {
	/* allocate new string */
	if (schunksz == SCHUNKSZ) {
	    register schunk *l;

	    l = ALLOC(schunk, 1);
	    l->next = slist;
	    slist = l;
	    schunksz = 0;
	}
	sb = &slist->s[schunksz++];
    }
    sb->buffer = buf;
    sb->buffer[0] = '\0';
    sb->size = sz;
    sb->len = 0;

    return sb;
}

/*
 * NAME:	str->del()
 * DESCRIPTION:	delete a string
 */
void pps_del(sb)
str *sb;
{
    sb->buffer = (char *) flist;
    flist = sb;
}

/*
 * NAME:	str->scat()
 * DESCRIPTION:	append a string. The length becomes -1 if the result is too long
 */
int pps_scat(sb, s)
register str *sb;
char *s;
{
    register int l;

    if (sb->len < 0 || sb->len + (l = strlen(s)) >= sb->size) {
	return sb->len = -1;
    }
    strcpy(sb->buffer + sb->len, s);
    return sb->len += l;
}

/*
 * NAME:	str->ccat()
 * DESCRIPTION:	append a char. The length becomes -1 if the result is too long
 */
int pps_ccat(sb, c)
register str *sb;
int c;
{
    if (sb->len < 0 || c == '\0' || sb->len + 1 >= sb->size) {
	return sb->len = -1;
    }
    sb->buffer[sb->len++] = c;
    sb->buffer[sb->len] = '\0';
    return sb->len;
}
