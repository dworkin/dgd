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
    long index;			/* building index */
    struct _strh_ **link;	/* next in list */
} strh;

typedef struct _strhchunk_ {
    strh sh[STR_CHUNK];		/* chunk of strh entries */
    struct _strhchunk_ *next;	/* next in list */
} strhchunk;

static hashtab *ht;		/* string merge table */
static strh **slink;		/* linked list of merged strings */
static strhchunk *shlist;	/* list of all strh chunks */
static int strhchunksz;		/* size of current strh chunk */

/*
 * NAME:	string->init()
 * DESCRIPTION:	initialize string handling
 */
void str_init()
{
    ht = ht_new(STRMERGETABSZ, STRMERGEHASHSZ);
    strhchunksz = STR_CHUNK;
}

/*
 * NAME:	string->new()
 * DESCRIPTION:	create a new string. The text can be a NULL pointer, in which
 *		case it must be filled in later.
 *		Note that strings are not placed in the hash table by default.
 */
string *str_new(text, len)
char *text;
register long len;
{
    register string *s;
    string dummy;

    if (len > (unsigned long) USHRT_MAX - sizeof(string)) {
	error("String too long");
    }

    /* allocate string struct & text in one block */
    s = (string *) ALLOC(char, dummy.text - (char *) &dummy + 1 + len);
    if (text != (char *) NULL && len > 0) {
	memcpy(s->text, text, (unsigned int) len);
    }
    s->text[s->len = len] = '\0';
    s->ref = 0;
    s->u.primary = (struct _strref_ *) NULL;

    return s;
}

/*
 * NAME:	string->del()
 * DESCRIPTION:	remove a reference from a string. If there are none left, the
 *		string is removed.
 */
void str_del(s)
register string *s;
{
    if ((--(s->ref) & STR_REF) == 0) {
	FREE(s);
    }
}

/*
 * NAME:	string->put()
 * DESCRIPTION:	put a string in the string merge table
 */
long str_put(str, n)
register string *str;
register long n;
{
    register strh **h;

    h = (strh **) ht_lookup(ht, str->text);
    for (;;) {
	if (*h == (strh *) NULL || strcmp(str->text, (*h)->str->text) != 0) {
	    register strh *s;
	    hte *next;

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
	    next = (hte *) *h;
	    s = *h = &shlist->sh[strhchunksz++];
	    s->chain.next = next;
	    s->chain.name = str->text;
	    s->str = str;
	    s->index = n;
	    s->link = slink;
	    slink = h;

	    return n;
	} else if (str_cmp(str, (*h)->str) == 0) {
	    register long i;

	    /*
	     * found it
	     */
	    i = (*h)->index;
	    if (n < i) {
		/*
		 * It was in the hash table, but give it a new index anyway.
		 */
		(*h)->index = n;
		if (n >= 0) {
		    return n;
		}
	    }
	    return i;	/* return the previous index */
	}
	h = (strh **) &(*h)->chain.next;
    }
}

/*
 * NAME:	string->clear()
 * DESCRIPTION:	clear the string merge table. All entries are in a separate
 *		linked list so this is simple. Note that this routine makes
 *		assumptions about how the hash table is constructed.
 */
void str_clear()
{
    register strh **h;
    register strhchunk *l;

    for (h = slink; h != (strh **) NULL; ) {
	register strh *f;

	f = *h;
	*h = (strh *) NULL;
	h = f->link;
    }
    slink = (strh **) NULL;

    for (l = shlist; l != (strhchunk *) NULL; ) {
	register strhchunk *f;

	f = l;
	l = l->next;
	FREE(f);
    }
    shlist = (strhchunk *) NULL;
    strhchunksz = STR_CHUNK;
}


/*
 * NAME:	string->cmp()
 * DESCRIPTION:	compare two strings
 */
int str_cmp(s1, s2)
register string *s1, *s2;
{
    if (s1 == s2) {
	return 0;
    } else {
	register long cmplen;
	register unsigned short len;
	register int cmp;

	cmplen = (long) s1->len - s2->len;
	if (cmplen > 0) {
	    /* s1 longer */
	    cmplen = 1;
	    len = s2->len;
	} else {
	    /* s2 longer or equally long */
	    cmplen >>= 16;
	    len = s1->len;
	}
	cmp = memcmp(s1->text, s2->text, len);
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
unsigned short str_index(s, l)
register string *s;
register long l;
{
    if (l < 0 || l >= (long) s->len) {
	error("String index out of range");
    }

    return l;
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
