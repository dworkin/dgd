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
    struct _strh_ **link;	/* next in list */
} strh;

typedef struct _strhchunk_ {
    struct _strhchunk_ *next;	/* next in list */
    strh sh[STR_CHUNK];		/* chunk of strh entries */
} strhchunk;

struct _strmerge_ {
    hashtab *ht;		/* string merge table */
    strh **slink;		/* linked list of merged strings */
    strhchunk *shlist;		/* list of all strh chunks */
    int strhchunksz;		/* size of current strh chunk */
};


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
 * DESCRIPTION:	create a string merge table
 */
strmerge *str_merge()
{
    register strmerge *merge;

    merge = ALLOC(strmerge, 1);
    merge->ht = ht_new(STRMERGETABSZ, STRMERGEHASHSZ, FALSE);
    merge->slink = (strh **) NULL;
    merge->shlist = (strhchunk *) NULL;
    merge->strhchunksz = STR_CHUNK;

    return merge;
}

/*
 * NAME:	string->put()
 * DESCRIPTION:	put a string in a string merge table
 */
Uint str_put(merge, str, n)
register strmerge *merge;
register string *str;
register Uint n;
{
    register strh **h;

    h = (strh **) ht_lookup(merge->ht, str->text, FALSE);
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
	    if (merge->strhchunksz == STR_CHUNK) {
		register strhchunk *l;

		l = ALLOC(strhchunk, 1);
		l->next = merge->shlist;
		merge->shlist = l;
		merge->strhchunksz = 0;
	    }
	    s = *h = &merge->shlist->sh[merge->strhchunksz++];
	    s->chain.next = (hte *) NULL;
	    s->chain.name = str->text;
	    s->str = str;
	    s->index = n;
	    s->link = merge->slink;
	    merge->slink = h;

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
 * DESCRIPTION:	clear a string merge table
 */
void str_clear(merge)
strmerge *merge;
{
    register strh **h;
    register strhchunk *l;

    for (h = merge->slink; h != (strh **) NULL; ) {
	register strh *f;

	f = *h;
	*h = (strh *) NULL;
	h = f->link;
    }
    ht_del(merge->ht);

    for (l = merge->shlist; l != (strhchunk *) NULL; ) {
	register strhchunk *f;

	f = l;
	l = l->next;
	FREE(f);
    }

    FREE(merge);
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
