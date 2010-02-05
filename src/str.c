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
# include "hash.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"

# define STR_CHUNK	128

typedef struct _strh_ {
    hte chain;			/* hash table chain */
    string *str;		/* string entry */
    Uint index;			/* building index */
} strh;

typedef struct _strhchunk_ {
    struct _strhchunk_ *next;	/* next in list */
    strh sh[STR_CHUNK];		/* chunk of strh entries */
} strhchunk;

static hashtab *sht;		/* string merge table */
static strhchunk *shlist;	/* list of all strh chunks */
static int strhchunksz;		/* size of current strh chunk */


/*
 * NAME:	string->alloc()
 * DESCRIPTION:	Create a new string. The text can be a NULL pointer, in which
 *		case it must be filled in later.
 */
string *str_alloc(text, len)
char *text;
register long len;
{
    register string *s;
    string dummy;

    /* allocate string struct & text in one block */
    s = (string *) ALLOC(char, dummy.text - (char *) &dummy + 1 + len);
    if (text != (char *) NULL && len > 0) {
	memcpy(s->text, text, (unsigned int) len);
    }
    s->text[s->len = len] = '\0';
    s->ref = 0;
    s->primary = (strref *) NULL;

    return s;
}

/*
 * NAME:	string->new()
 * DESCRIPTION:	create a new string with size check
 */
string *str_new(text, len)
char *text;
long len;
{
    if (len > (unsigned long) MAX_STRLEN) {
	error("String too long");
    }
    return str_alloc(text, len);
}

/*
 * NAME:	string->del()
 * DESCRIPTION:	remove a reference from a string. If there are none left, the
 *		string is removed.
 */
void str_del(s)
register string *s;
{
    if (--(s->ref) == 0) {
	FREE(s);
    }
}

/*
 * NAME:	string->merge()
 * DESCRIPTION:	prepare string merge
 */
void str_merge()
{
    sht = ht_new(STRMERGETABSZ, STRMERGEHASHSZ, FALSE);
    strhchunksz = STR_CHUNK;
}

/*
 * NAME:	string->put()
 * DESCRIPTION:	put a string in the string merge table
 */
Uint str_put(str, n)
register string *str;
register Uint n;
{
    register strh **h;

    h = (strh **) ht_lookup(sht, str->text, FALSE);
    for (;;) {
	/*
	 * The hasher doesn't handle \0 in strings, and so may not have
	 * found the proper string. Follow the hash table chain until
	 * the end is reached, or until a match is found using str_cmp().
	 */
	if (*h == (strh *) NULL) {
	    register strh *s;

	    /*
	     * Not in the hash table. Make a new entry.
	     */
	    if (strhchunksz == STR_CHUNK) {
		register strhchunk *l;

		l = ALLOC(strhchunk, 1);
		l->next = shlist;
		shlist = l;
		strhchunksz = 0;
	    }
	    s = *h = &shlist->sh[strhchunksz++];
	    s->chain.next = (hte *) NULL;
	    s->chain.name = str->text;
	    s->str = str;
	    s->index = n;

	    return n;
	} else if (str_cmp(str, (*h)->str) == 0) {
	    /* already in the hash table */
	    return (*h)->index;
	}
	h = (strh **) &(*h)->chain.next;
    }
}

/*
 * NAME:	string->clear()
 * DESCRIPTION:	clear the string merge table
 */
void str_clear()
{
    if (sht != (hashtab *) NULL) {
	register strhchunk *l;

	ht_del(sht);

	for (l = shlist; l != (strhchunk *) NULL; ) {
	    register strhchunk *f;

	    f = l;
	    l = l->next;
	    FREE(f);
	}

	sht = (hashtab *) NULL;
	shlist = (strhchunk *) NULL;
    }
}


/*
 * NAME:	string->cmp()
 * DESCRIPTION:	compare two strings
 */
int str_cmp(s1, s2)
string *s1, *s2;
{
    if (s1 == s2) {
	return 0;
    } else {
	register ssizet len;
	register char *p, *q;
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
 * NAME:	string->add()
 * DESCRIPTION:	add two strings
 */
string *str_add(s1, s2)
register string *s1, *s2;
{
    register string *s;

    s = str_new((char *) NULL, (long) s1->len + s2->len);
    memcpy(s->text, s1->text, s1->len);
    memcpy(s->text + s1->len, s2->text, s2->len);

    return s;
}

/*
 * NAME:	string->index()
 * DESCRIPTION:	index a string
 */
ssizet str_index(s, l)
string *s;
register long l;
{
    if (l < 0 || l >= (long) s->len) {
	error("String index out of range");
    }

    return l;
}

/*
 * NAME:	string->ckrange()
 * DESCRIPTION:	check a string subrange
 */
void str_ckrange(s, l1, l2)
string *s;
register long l1, l2;
{
    if (l1 < 0 || l1 > l2 + 1 || l2 >= (long) s->len) {
	error("Invalid string range");
    }
}

/*
 * NAME:	string->range()
 * DESCRIPTION:	return a subrange of a string
 */
string *str_range(s, l1, l2)
register string *s;
register long l1, l2;
{
    if (l1 < 0 || l1 > l2 + 1 || l2 >= (long) s->len) {
	error("Invalid string range");
    }

    return str_new(s->text + l1, l2 - l1 + 1);
}
