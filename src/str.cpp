/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
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
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"

# define STR_CHUNK	128

struct strh : public Hashtab::Entry, public ChunkAllocated {
    String *str;		/* string entry */
    Uint index;			/* building index */
};

static Chunk<strh, STR_CHUNK> hchunk;

static Hashtab *sht;		/* string merge table */


/*
 * NAME:	String->alloc()
 * DESCRIPTION:	Create a new string. The text can be a NULL pointer, in which
 *		case it must be filled in later.
 */
String *str_alloc(const char *text, long len)
{
    String *s;
    String dummy;

    /* allocate string struct & text in one block */
    s = (String *) ALLOC(char, dummy.text - (char *) &dummy + 1 + len);
    if (text != (char *) NULL && len > 0) {
	memcpy(s->text, text, (unsigned int) len);
    }
    s->text[s->len = len] = '\0';
    s->ref = 0;
    s->primary = (strref *) NULL;

    return s;
}

/*
 * NAME:	String->new()
 * DESCRIPTION:	create a new string with size check
 */
String *str_new(const char *text, long len)
{
    if (len > (unsigned long) MAX_STRLEN) {
	error("String too long");
    }
    return str_alloc(text, len);
}

/*
 * NAME:	String->del()
 * DESCRIPTION:	remove a reference from a string. If there are none left, the
 *		string is removed.
 */
void str_del(String *s)
{
    if (--(s->ref) == 0) {
	FREE(s);
    }
}

/*
 * NAME:	String->merge()
 * DESCRIPTION:	prepare string merge
 */
void str_merge()
{
    sht = Hashtab::create(STRMERGETABSZ, STRMERGEHASHSZ, FALSE);
}

/*
 * NAME:	String->put()
 * DESCRIPTION:	put a string in the string merge table
 */
Uint str_put(String *str, Uint n)
{
    strh **h;

    h = (strh **) sht->lookup(str->text, FALSE);
    for (;;) {
	/*
	 * The hasher doesn't handle \0 in strings, and so may not have
	 * found the proper string. Follow the hash table chain until
	 * the end is reached, or until a match is found using str_cmp().
	 */
	if (*h == (strh *) NULL) {
	    strh *s;

	    /*
	     * Not in the hash table. Make a new entry.
	     */
	    s = *h = chunknew (hchunk) strh;
	    s->next = (Hashtab::Entry *) NULL;
	    s->name = str->text;
	    s->str = str;
	    s->index = n;

	    return n;
	} else if (str_cmp(str, (*h)->str) == 0) {
	    /* already in the hash table */
	    return (*h)->index;
	}
	h = (strh **) &(*h)->next;
    }
}

/*
 * NAME:	String->clear()
 * DESCRIPTION:	clear the string merge table
 */
void str_clear()
{
    if (sht != (Hashtab *) NULL) {
	delete sht;

	hchunk.clean();
	sht = (Hashtab *) NULL;
    }
}


/*
 * NAME:	String->cmp()
 * DESCRIPTION:	compare two strings
 */
int str_cmp(String *s1, String *s2)
{
    if (s1 == s2) {
	return 0;
    } else {
	ssizet len;
	char *p, *q;
	long cmplen;
	int cmp;

	cmplen = (long) s1->len - s2->len;
	if (cmplen > 0) {
	    /* s1 longer */
	    cmplen = 1;
	    len = s2->len;
	} else {
	    /* s2 longer or equally long */
	    if (cmplen < 0) {
		cmplen = -1;
	    }
	    len = s1->len;
	}
	for (p = s1->text, q = s2->text; len > 0 && *p == *q; p++, q++, --len) ;
	cmp = UCHAR(*p) - UCHAR(*q);
	return (cmp != 0) ? cmp : cmplen;
    }
}

/*
 * NAME:	String->add()
 * DESCRIPTION:	add two strings
 */
String *str_add(String *s1, String *s2)
{
    String *s;

    s = str_new((char *) NULL, (long) s1->len + s2->len);
    memcpy(s->text, s1->text, s1->len);
    memcpy(s->text + s1->len, s2->text, s2->len);

    return s;
}

/*
 * NAME:	String->index()
 * DESCRIPTION:	index a string
 */
ssizet str_index(String *s, long l)
{
    if (l < 0 || l >= (long) s->len) {
	error("String index out of range");
    }

    return l;
}

/*
 * NAME:	String->ckrange()
 * DESCRIPTION:	check a string subrange
 */
void str_ckrange(String *s, long l1, long l2)
{
    if (l1 < 0 || l1 > l2 + 1 || l2 >= (long) s->len) {
	error("Invalid string range");
    }
}

/*
 * NAME:	String->range()
 * DESCRIPTION:	return a subrange of a string
 */
String *str_range(String *s, long l1, long l2)
{
    if (l1 < 0 || l1 > l2 + 1 || l2 >= (long) s->len) {
	error("Invalid string range");
    }

    return str_new(s->text + l1, l2 - l1 + 1);
}
