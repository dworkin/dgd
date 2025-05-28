/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2025 DGD Authors (see the commit log for details)
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
# include "xfloat.h"
# include "data.h"

# define STR_CHUNK	128

struct StrHash : public Hash::Entry, public ChunkAllocated {
    String *str;		/* string entry */
    Uint index;			/* building index */
};

static Chunk<String, STR_CHUNK> schunk;
static Chunk<StrHash, STR_CHUNK> hchunk;

static Hash::Hashtab *sht;		/* string merge table */


String::String(const char *text, long len)
{
    this->text = ALLOC(char, len + 1);
    if (text != (char *) NULL && len > 0) {
	memcpy(this->text, text, (unsigned int) len);
    }
    this->text[this->len = len] = '\0';
    refCount = 0;
    primary = (StrRef *) NULL;
}

String::~String()
{
    FREE(text);
}

/*
 * Create a new string. The text can be a NULL pointer, in which case it must
 * be filled in later.
 */
String *String::alloc(const char *text, long len)
{
    return chunknew (schunk) String(text, len);
}

/*
 * create a new string with size check
 */
String *String::create(const char *text, LPCint len)
{
    if (len > (LPCint) MAX_STRLEN) {
	EC->error("String too long");
    }
    return alloc(text, len);
}

/*
 * Remove a reference from a string. If there are none left, the string is
 * removed.
 */
void String::del()
{
    if (--refCount == 0) {
	delete this;
    }
}

/*
 * remove string chunks from memory
 */
void String::clean()
{
    schunk.clean();
}

/*
 * prepare string merge
 */
void String::merge()
{
    sht = HM->create(STRMERGETABSZ, STRMERGEHASHSZ, FALSE);
}

/*
 * put a string in the string merge table
 */
Uint String::put(Uint n)
{
    StrHash **h;

    h = (StrHash **) sht->lookup(text, FALSE);
    for (;;) {
	/*
	 * The hasher doesn't handle \0 in strings, and so may not have
	 * found the proper string. Follow the hash table chain until
	 * the end is reached, or until a match is found using cmp().
	 */
	if (*h == (StrHash *) NULL) {
	    StrHash *s;

	    /*
	     * Not in the hash table. Make a new entry.
	     */
	    s = *h = chunknew (hchunk) StrHash;
	    s->next = (Hash::Entry *) NULL;
	    s->name = text;
	    s->str = this;
	    s->index = n;

	    return n;
	} else if (cmp((*h)->str) == 0) {
	    /* already in the hash table */
	    return (*h)->index;
	}
	h = (StrHash **) &(*h)->next;
    }
}

/*
 * clear the string merge table
 */
void String::clear()
{
    if (sht != (Hash::Hashtab *) NULL) {
	delete sht;

	hchunk.clean();
	sht = (Hash::Hashtab *) NULL;
    }
}


/*
 * compare two strings
 */
int String::cmp(String *str)
{
    if (this == str) {
	return 0;
    } else {
	ssizet length;
	char *p, *q;
	long cmplen;
	int cmp;

	cmplen = (long) len - str->len;
	if (cmplen > 0) {
	    /* s1 longer */
	    cmplen = 1;
	    length = str->len;
	} else {
	    /* str longer or equally long */
	    if (cmplen < 0) {
		cmplen = -1;
	    }
	    length = len;
	}
	for (p = text, q = str->text; length > 0 && *p == *q;
	     p++, q++, --length)
	    ;
	cmp = UCHAR(*p) - UCHAR(*q);
	return (cmp != 0) ? cmp : cmplen;
    }
}

/*
 * add two strings
 */
String *String::add(String *str)
{
    String *s;

    s = create((char *) NULL, (LPCint) len + str->len);
    memcpy(s->text, text, len);
    memcpy(s->text + len, str->text, str->len);

    return s;
}

/*
 * index a string
 */
ssizet String::index(LPCint l)
{
    if (l < 0 || l >= (LPCint) len) {
	EC->error("String index out of range");
    }

    return l;
}

/*
 * check a string subrange
 */
void String::checkRange(LPCint l1, LPCint l2)
{
    if (l1 < 0 || l1 > l2 + 1 || l2 >= (LPCint) len) {
	EC->error("Invalid string range");
    }
}

/*
 * return a subrange of a string
 */
String *String::range(LPCint l1, LPCint l2)
{
    if (l1 < 0 || l1 > l2 + 1 || l2 >= (LPCint) len) {
	EC->error("Invalid string range");
    }

    return create(text + l1, l2 - l1 + 1);
}
