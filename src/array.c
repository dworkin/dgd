# include "dgd.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"

# define ARR_CHUNK	128

typedef struct _arrchunk_ {
    array a[ARR_CHUNK];		/* chunk of arrays */
    struct _arrchunk_ *next;	/* next in list */
} arrchunk;

typedef struct _arrh_ {
    struct _arrh_ *next;	/* next in hash table chain */
    array *arr;			/* array entry */
    int index;			/* building index */
    struct _arrh_ **link;	/* next in list */
} arrh;

typedef struct _arrhchunk_ {
    arrh ah[ARR_CHUNK];		/* chunk of arrh entries */
    struct _arrhchunk_ *next;	/* next in list */
} arrhchunk;

static long tag;		/* current array tag */
static array *list;		/* linked list of all arrays */
static arrchunk *alist;		/* linked list of all array chunks */
static int arrchunksz;		/* size of current array chunk */
static array *flist;		/* free arrays */
static arrh **ht;		/* array merge table */
static int htsize;		/* array merge table size */
static arrh **link;		/* linked list of merged arrays */
static arrhchunk *ahlist;	/* linked list of all arrh chunks */
static int arrhchunksz;		/* size of current arrh chunk */
static uindex index;		/* current building index */

/*
 * NAME:	array->init()
 * DESCRIPTION:	initialize array handling
 */
void arr_init()
{
    ht = ALLOC(arrh*, htsize = ARRMERGETABSZ);
    memset(ht, '\0', ARRMERGETABSZ * sizeof(arrh *));
    arrchunksz = ARR_CHUNK;
    arrhchunksz = ARR_CHUNK;
}

/*
 * NAME:	a_new()
 * DESCRIPTION:	create a new array
 */
static array *a_new(size)
unsigned short size;
{
    register array *a;
    register value *v;

    if (flist != (array *) NULL) {
	a = flist;
	flist = a->next;
    } else {
	if (arrchunksz == ARR_CHUNK) {
	    register arrchunk *l;

	    l = ALLOC(arrchunk, 1);
	    l->next = alist;
	    alist = l;
	    arrchunksz = 0;
	}
	a = &alist->a[arrchunksz++];
    }
    a->size = size;
    if (size > 0) {
	a->elts = ALLOC(value, size);
    } else {
	a->elts = (value *) NULL;
    }
    a->ref = 0;
    a->tag = tag++;
    a->primary = (arrref *) NULL;
    a->prev = (array *) NULL;
    a->next = list;
    if (list != (array *) NULL) {
	list->prev = a;
    }

    return list = a;
}

/*
 * NAME:	array->new()
 * DESCRIPTION:	create a new array
 */
array *arr_new(size)
long size;
{
    if (size > MAX_ARRAY_SIZE) {
	error("Array too large");
    }
    return a_new((unsigned short) size);
}

/*
 * NAME:	array->del()
 * DESCRIPTION:	remove a reference from an array.  If none are left, the
 *		array is removed.
 */
void arr_del(a)
register array *a;
{
    if (--(a->ref) == 0) {
	register value *v;
	register unsigned short i;

	v = a->elts;
	for (i = a->size; i > 0; --i) {
	    switch (v->type) {
	    case T_STRING:
		str_del(v->u.string);
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		arr_del(v->u.array);
		break;
	    }
	    v++;
	}

	if (a == list) {
	    list = a->next;
	} else {
	    a->prev->next = a->next;
	}
	if (a->next != (array *) NULL) {
	    a->next->prev = a->prev;
	}

	if (a->elts != (value *) NULL) {
	    FREE(a->elts);
	}

	a->next = flist;
	flist = a;
    }
}

/*
 * NAME:	array->freeall()
 * DESCRIPTION:	free all arrays still left. This can be used to free arrays
 *		with circular references that have no outside reference
 *		left.
 */
void arr_freeall()
{
    register array *a;
    register arrchunk *c;

    for (a = list; a != (array *) NULL; ) {
	register value *v;
	register unsigned short i;

	if ((v=a->elts) != (value *) NULL) {
	    for (i = a->size; i > 0; --i) {
		/*
		 * We can't use arr_del() here, because a recursive deletion
		 * would delete this array again, etc. Therefore delete the
		 * arrays without regard of reference count.
		 */
		if (v->type == T_STRING) {
		    str_del(v->u.string);
		}
		v++;
	    }
	    FREE(a->elts);
	}
	a = a->next;
    }
    list = (array *) NULL;
    flist = (array *) NULL;

    for (c = alist; c != (arrchunk *) NULL; ) {
	register arrchunk *f;

	f = c;
	c = c->next;
	FREE(f);
    }
    alist = (arrchunk *) NULL;
    arrchunksz = ARR_CHUNK;
}

/*
 * NAME:	array->put()
 * DESCRIPTION:	Put an array in the merge table, and return its "index".
 */
uindex arr_put(a)
register array *a;
{
    register arrh **h;

    for (h = &ht[(long) a % htsize]; *h != (arrh *) NULL; h = &(*h)->next) {
	if ((*h)->arr == a) {
	    return (*h)->index;
	}
    }
    /*
     * Add a new entry to the hash table.
     */
    if (arrhchunksz == ARR_CHUNK) {
	register arrhchunk *l;

	l = ALLOC(arrhchunk, 1);
	l->next = ahlist;
	ahlist = l;
	arrhchunksz = 0;
    }
    *h = &ahlist->ah[arrhchunksz++];
    (*h)->next = (arrh *) NULL;
    (*h)->arr = a;
    (*h)->index = index;
    (*h)->link = link;
    link = h;

    return index++;
}

/*
 * NAME:	array->clear()
 * DESCRIPTION:	clear the array merge table
 */
void arr_clear()
{
    register arrh **h;
    register arrhchunk *l;

    for (h = link; h != (arrh **) NULL; ) {
	register arrh *f;

	f = *h;
	*h = (arrh *) NULL;
	h = f->link;
    }
    link = (arrh **) NULL;
    index = 0;

    for (l = ahlist; l != (arrhchunk *) NULL; ) {
	register arrhchunk *f;

	f = l;
	l = l->next;
	FREE(f);
    }
    ahlist = (arrhchunk *) NULL;
    arrhchunksz = ARR_CHUNK;
}

/*
 * NAME:	copy()
 * DESCRIPTION:	copy a number of values
 */
static void copy(v1, v2, n)
register value *v1, *v2;
register unsigned short n;
{
    while (n > 0) {
	/*
	 * No check for destructed objects is made here.
	 */
	switch (v2->type) {
	case T_STRING:
	    str_ref(v2->u.string);
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_ref(v2->u.array);
	    break;
	}
	*v1++ = *v2++;
	--n;
    }
}

/*
 * NAME:	array->add()
 * DESCRIPTION:	add two arrays
 */
array *arr_add(a1, a2)
register array *a1, *a2;
{
    register array *a;

    /*
     * Any destructed objects in a1 and a2 will be copied to the result array.
     */
    a = arr_new((long) a1->size + a2->size);
    copy(a->elts, d_get_elts(a1), a1->size);
    copy(a->elts + a1->size, d_get_elts(a2), a2->size);

    return a;
}

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two values
 */
static int cmp(v1, v2)
register value *v1, *v2;
{
    register int i;
    register long l;

    i = v1->type - v2->type;
    if (i != 0) {
	return i;	/* order by type */
    }

    /*
     * No special check for destructed objects is made here; if desired,
     * that should be done in advance.
     */
    switch (v1->type) {
    case T_NUMBER:
	l = v1->u.number - v2->u.number;
	break;

    case T_OBJECT:
	return v1->u.object.index - v2->u.object.index;

    case T_STRING:
	return str_cmp(v1->u.string, v2->u.string);

    case T_ARRAY:
    case T_MAPPING:
	l = v1->u.array->tag - v2->u.array->tag;
	break;
    }
    /*
     * Unfortunately, since longs may be larger than integers, we cannot just
     * subtract one long from another and return the result as an int.
     */
    return (l > 0) ? 1 : l >> 16;
}

/*
 * NAME:	search()
 * DESCRIPTION:	search for a value in an array
 */
static int search(v1, v2, h, step, flag)
register value *v1, *v2;
register unsigned short h;
register int step;		/* 1 for arrays, 2 for mappings */
bool flag;
{
    register unsigned short l, m;
    register int c;
    register value *v3;
    register unsigned short mask;

    mask = -step;
    l = 0;
    while (l < h) {
	m = ((l + h) >> 1) & mask;
	v3 = v2 + m;
	c = cmp(v1, v3);
	if (c == 0) {
	    if (v1->type >= T_ARRAY && v1->u.array != v3->u.array) {
		/*
		 * It is possible for one object to export an array, both
		 * objects being swapped out after that, and the other object
		 * exporting the array back again.  This gives two arrays with
		 * identical tags, which do not point to the same actual values
		 * and are not guaranteed to contain the same values, either.
		 * A possible way out is to give new tags to imported arrays
		 * and to resort all mappings before swapping them out.
		 * The solution used here is to check every array with this tag,
		 * and hope this kind of thing doesn't occur too often...
		 */
		/* search forward */
		for (;;) {
		    m += step;
		    v3 += step;
		    if (m == h || v3->type < T_ARRAY) {
			break;	/* out of range */
		    }
		    if (v1->u.array == v3->u.array) {
			return m;	/* found the right one */
		    }
		    if (v1->u.array->tag != v3->u.array->tag) {
			break;		/* wrong tag */
		    }
		}
		/* search backward */
		m = ((l + h) >> 1) & mask;
		v3 = v2 + m;
		for (;;) {
		    m -= step;
		    v3 -= step;
		    if (m < l || v3->type < T_ARRAY) {
			break;	/* out of range */
		    }
		    if (v1->u.array == v3->u.array) {
			return m;	/* found the right one */
		    }
		    if (v1->u.array->tag != v3->u.array->tag) {
			break;		/* wrong tag */
		    }
		}
		break;
	    }
	    return m;
	} else if (c < 0) {
	    h = m;		/* search in lower half */
	} else {
	    l = m + step;	/* search in upper half */
	}
    }
    /*
     * not found: return either the place where it should be, or -1
     */
    return (flag) ? m : -1;
}

static value zero_value = { T_NUMBER, TRUE };	/* the zero value */

/*
 * NAME:	array->sub()
 * DESCRIPTION:	subtract one array from another
 */
array *arr_sub(a1, a2)
array *a1, *a2;
{
    register value *v1, *v2, *v3;
    register array *a3;
    register unsigned short n, size, count;

    if (a2->size == 0) {
	/*
	 * array - ({ })
	 * Return a copy of the first array.
	 */
	a3 = a_new(a1->size);
	copy(a3->elts, d_get_elts(a1), a1->size);
	return a3;
    }

    /*
     * If destructed objects are found, they will be replaced by 0 in the
     * original arrays.
     */

    /* create new array */
    a3 = a_new(a1->size);
    size = a2->size;

    /* copy values of subtrahend */
    v1 = d_get_elts(a2);
    v2 = ALLOC(value, size);
    for (n = size; n > 0; --n) {
	if (v1->type == T_OBJECT && o_object(&v1->u.object) == (object *) NULL)
	{
	    /* replace destructed object by 0 */
	    d_assign_elt(a2, v1 - a2->elts, &zero_value);
	}
	*v2++ = *v1++;
    }
    /* sort values */
    qsort(v2 -= size, size, sizeof(value), cmp);

    count = 0;
    v1 = d_get_elts(a1);
    v3 = a3->elts;
    for (n = a1->size; n > 0; --n) {
	if (v1->type == T_OBJECT && o_object(&v1->u.object) == (object *) NULL)
	{
	    /* replace destructed object by 0 */
	    d_assign_elt(a1, v1 - a1->elts, &zero_value);
	}
	if (search(v1, v2, size, 1, FALSE) < 0) {
	    /*
	     * not found in subtrahend: copy to result array
	     */
	    switch (v1->type) {
	    case T_STRING:
		str_ref(v1->u.string);
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		arr_ref(v1->u.array);
		break;
	    }
	    *v3++ = *v1;
	    count++;
	}
	v1++;
    }
    FREE(v2);	/* free copy of values of subtrahend */

    if (count == 0 && a3->size > 0) {
	FREE(a3->elts);
	a3->elts = (value *) NULL;
    }
    a3->size = count;
    return a3;
}

/*
 * NAME:	array->intersect()
 * DESCRIPTION:	A - (A - B).  If A and B are sets, the result is a set also.
 */
array *arr_intersect(a1, a2)
array *a1, *a2;
{
    register value *v1, *v2, *v3;
    register array *a3;
    register unsigned short n, size, count;

    if (a1->size == 0 || a2->size == 0) {
	/* array & ({ }) */
	return a_new(0);
    }

    /* create new array */
    a3 = a_new(a1->size);
    size = a2->size;

    /*
     * If destructed objects are found, they will be replaced by 0 in the
     * original arrays.
     */

    /* copy values of 2nd array */
    v1 = d_get_elts(a2);
    v2 = ALLOC(value, size);
    for (n = size; n > 0; --n) {
	if (v1->type == T_OBJECT && o_object(&v1->u.object) == (object *) NULL)
	{
	    /* replace destructed object by 0 */
	    d_assign_elt(a2, v1 - a2->elts, &zero_value);
	}
	*v2++ = *v1++;
    }
    /* sort values */
    qsort(v2 -= size, size, sizeof(value), cmp);

    count = 0;
    v1 = d_get_elts(a1);
    v3 = a3->elts;
    for (n = a1->size; n > 0; --n) {
	if (v1->type == T_OBJECT && o_object(&v1->u.object) == (object *) NULL)
	{
	    /* replace destructed object by 0 */
	    d_assign_elt(a1, v1 - a1->elts, &zero_value);
	}
	if (search(v1, v2, a2->size, 1, FALSE) >= 0) {
	    /*
	     * element is in both arrays: copy to result array
	     */
	    switch (v1->type) {
	    case T_STRING:
		str_ref(v1->u.string);
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		arr_ref(v1->u.array);
		break;
	    }
	    *v3++ = *v1;
	    count++;
	}
	v1++;
    }
    FREE(v2);	/* free copy of values of 2nd array */

    if (count == 0) {
	FREE(a3->elts);
	a3->elts = (value *) NULL;
    }
    a3->size = count;
    return a3;
}

/*
 * NAME:	array->index()
 * DESCRIPTION:	index an array
 */
int arr_index(a, l)
register array *a;
register Int l;
{
    if (l < 0) {
	l += a->size;
    }
    if (l < 0 || l >= a->size) {
	error("Array index out of range");
    }
    return l;
}

/*
 * NAME:	array->range()
 * DESCRIPTION:	return a subrange of an array
 */
array *arr_range(a, l1, l2)
register array *a;
register Int l1, l2;
{
    register array *range;

    if (l1 < 0) {
	l1 += a->size;
    }
    if (l2 < 0) {
	l2 += a->size;
    }
    if (l1 < 0 || l1 > l2 || l2 >= a->size) {
	error("Invalid array range");
    }

    /*
     * No attempt is made to replace destructed objects by 0.
     */
    range = a_new((unsigned short) (l2 - l1 + 1));
    copy(range->elts, d_get_elts(a) + l1, (unsigned short) (l2 - l1 + 1));
    return range;
}


/*
 * NAME:	mapcmp()
 * DESCRIPTION:	compare two mapping indices
 */
static int mapcmp(v1, v2)
value *v1, *v2;
{
    register int c;

    c = cmp(v1, v2);
    if (c == 0 && (v1->type < T_ARRAY || v1->u.array == v2->u.array)) {
	error("Identical indices in mapping");
    }
    return c;
}

/*
 * NAME:	mapping->sort()
 * DESCRIPTION:	sort a mapping
 */
void map_sort(m)
array *m;
{
    qsort(m->elts, m->size >> 1, 2 * sizeof(value), mapcmp);
}

/*
 * NAME:	mapping->new()
 * DESCRIPTION:	create a new mapping
 */
array *map_new(size)
long size;
{
    if (size > MAX_ARRAY_SIZE * 2) {
	error("Mapping too large");
    }
    return a_new((unsigned short) size);
}

/*
 * NAME:	mapping->make()
 * DESCRIPTION:	construct a new mapping
 */
array *map_make(indices, values)
array *indices, *values;
{
    register value *v1, *v2, *v3;
    register unsigned short n, skip;
    array *map;

    if ((n=indices->size) != values->size) {
	error("Unequal array sizes in mapping constructor");
    }

    map = a_new(n << 1);

    /*
     * Destructed objects are replaced by 0 in the original arrays.
     */

    v1 = map->elts;
    v2 = d_get_elts(indices);
    v3 = d_get_elts(values);
    skip = 0;
    while (n > 0) {
	switch (v2->type) {
	case T_STRING:
	    str_ref(v2->u.string);
	    break;

	case T_OBJECT:
	    if (o_object(&v2->u.object) == (object *) NULL) {
		d_assign_elt(indices, v2 - indices->elts, &zero_value);
	    }
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_ref(v2->u.array);
	    break;
	}

	switch (v3->type) {
	case T_NUMBER:
	    if (v3->u.number == 0) {
		v2++;
		v3++;
		skip += 2;
		--n;
		continue;
	    }
	    break;

	case T_STRING:
	    str_ref(v3->u.string);
	    break;

	case T_OBJECT:
	    if (o_object(&v3->u.object) == (object *) NULL) {
		d_assign_elt(values, v3 - values->elts, &zero_value);
		v2++;
		v3++;
		skip += 2;
		--n;
		continue;
	    }
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_ref(v3->u.array);
	    break;
	}
	*v1++ = *v2++;
	*v1++ = *v3++;
	--n;
    }

    map->size -= skip;
    if (skip != 0 && map->size == 0) {
	FREE(map->elts);
	map->elts = (value *) NULL;
    } else {
	if (ec_push()) {
	    /*
	     * An error occurred. Delete the mapping, and pass on the error.
	     */
	    arr_del(map);
	    error((char *) NULL);
	}
	map_sort(map);
	ec_pop();
    }

    return map;
}

/*
 * NAME:	mapping->add()
 * DESCRIPTION:	add two mappings
 */
array *map_add(m1, m2)
array *m1, *m2;
{
    register value *v1, *v2, *v3;
    register unsigned short n1, n2, n3, c;
    array *m3;

    m3 = map_new((long) m1->size + m2->size);
    v1 = d_get_elts(m1);
    v2 = d_get_elts(m2);
    for (n1 = m1->size, n2 = m2->size, n3 = 0; n1 > 0 && n2 > 0; ) {
	if ((v1[0].type == T_OBJECT &&
	     o_object(&v1[0].u.object) == (object *) NULL) ||
	    (v1[1].type == T_OBJECT &&
	     o_object(&v1[1].u.object) == (object *) NULL)) {
	    v1 += 2; n1 -= 2;
	    continue;
	}
	if ((v2[0].type == T_OBJECT &&
	     o_object(&v2[0].u.object) == (object *) NULL) ||
	    (v2[1].type == T_OBJECT &&
	     o_object(&v2[1].u.object) == (object *) NULL)) {
	    v2 += 2; n2 -= 2;
	    continue;
	}
	c = cmp(v1, v2);
	if (c < 0) {
	    /* the smaller element is in m1 */
	    copy(v3, v1, 2);
	    v1 += 2; n1 -= 2;
	    v3 += 2; n3 += 2;
	} else {
	    /* the smaller - or overriding - element is in m2 */
	    copy(v3, v2, 2);
	    v2 += 2; n2 -= 2;
	    v3 += 2; n3 += 2;
	    if (c == 0) {
		/* equal elements? */
		if (v1->type >= T_ARRAY && v1->u.array != v2->u.array) {
		    register value *v;
		    register int n;

		    /*
		     * The array tags are the same, but the arrays are not.
		     * Check ahead to see if the array is somewhere else
		     * in m2; if not, copy the element from m1 as well.
		     */
		    v = v2;
		    n = n2;
		    for (;;) {
			v += 2;
			n -= 2;
			if (n <= 0 || v->type < T_ARRAY ||
			    v->u.array->tag != v1->u.array->tag) {
			    copy(v3, v1, 2);
			    v3 += 2; n3 += 2;
			    break;
			}
			if (v->u.array == v1->u.array) {
			    break;
			}
		    }
		}
		/* skip m1 */
		v1 += 2; n1 -= 2;
	    }
	}
    }

    /* copy tail part of m1 */
    while (n1 > 0) {
	if ((v1[0].type != T_OBJECT ||
	     o_object(&v1[0].u.object) != (object *) NULL) &&
	    (v1[1].type != T_OBJECT ||
	     o_object(&v1[1].u.object) != (object *) NULL)) {
	    copy(v3, v1, 2);
	    v3 += 2; n3 += 2;
	}
	v1 += 2; n1 -= 2;
    }
    /* copy tail part of m2 */
    while (n2 > 0) {
	if ((v2[0].type != T_OBJECT ||
	     o_object(&v2[0].u.object) != (object *) NULL) &&
	    (v2[1].type != T_OBJECT ||
	     o_object(&v2[1].u.object) != (object *) NULL)) {
	    copy(v3, v2, 2);
	    v3 += 2; n3 += 2;
	}
	v2 += 2; n2 -= 2;
    }

    if (n3 == 0 && m3->size > 0) {
	FREE(m3->elts);
    }
    m3->size = n3;
    return m3;
}

/*
 * NAME:	mapping->index()
 * DESCRIPTION:	Index a mapping with a value.
 */
int map_index(m, val, elt)
register array *m;
value *val, *elt;
{
    register int n;
    register unsigned short i, size;
    register value *v1, *v2;
    bool grow;

    grow = elt != (value *) NULL &&
	   (elt->type != T_NUMBER || elt->u.number != 0);
    n = search(val, d_get_elts(m), m->size, 2, grow);
    if (n < 0) {
	/* not in the mapping, and the caller will not modify the result */
	return n;
    }

    v1 = &m->elts[n];
    if ((elt == (value *) NULL || grow) && n < m->size && cmp(val, v1) == 0 &&
	(val->type < T_ARRAY || val->u.array == v1->u.array)) {
	/*
	 * Found the right place, and it is not going to be deleted.
	 */
	return n + 1;
    }
    if (grow) {
	/* the mapping is to be extended  */
	if (m->size == MAX_ARRAY_SIZE * 2) {
	    /* ...but there is no room */
	    error("Mapping too large to grow");
	}
	v1 = ALLOC(value, m->size + 2);
    } else if (m->size > 2) {
	/* the mapping is to be shrunk */
	v1 = ALLOC(value, m->size - 2);
    } else {
	/* delete last contents */
	v1 = (value *) NULL;
    }

    elt = v1;
    v2 = m->elts;
    for (i = 0; i < n; i += 2) {
	if ((v2->type == T_OBJECT &&
	     o_object(&v2->u.object) == (object *) NULL) ||
	    (v2[1].type == T_OBJECT &&
	     o_object(&v2[1].u.object) == (object *) NULL)) {
	    /*
	     * Either the index or the value is a destructed object
	     */
	    v2 += 2;
	} else {
	    *v1++ = *v2++;
	    *v1++ = *v2++;
	}
    }

    if (grow) {
	*v1++ = *val;
	n = v1 - elt;
	*v1++ = zero_value;	/* filled in later */
	size = m->size;
    } else {
	n = -1;		/* so nothing in the mapping will be changed */
	switch (v2->type) {
	case T_STRING:
	    str_del(v2->u.string);
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_del(v2->u.array);
	    break;
	}
	v2++;
	switch (v2->type) {
	case T_STRING:
	    str_del(v2->u.string);
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_del(v2->u.array);
	    break;
	}
	v2++;
	size = m->size - 2;
    }

    for (i += 2; i < size; i += 2) {
	if ((v2->type == T_OBJECT &&
	     o_object(&v2->u.object) == (object *) NULL) ||
	    (v2[1].type == T_OBJECT &&
	     o_object(&v2[1].u.object) == (object *) NULL)) {
	    v2 += 2;
	} else {
	    *v1++ = *v2++;
	    *v1++ = *v2++;
	}
    }

    FREE(m->elts);
    if ((m->size = v1 - elt) == 0 && elt != (value *) NULL) {
	FREE(elt);
	m->elts = (value *) NULL;
    } else {
	m->elts = elt;
    }
    d_change_map(m);

    return n;
}

/*
 * NAME:	mapping->compact()
 * DESCRIPTION:	compact a mapping by removing indices/values of destructed
 *		objects
 */
void map_compact(m)
array *m;
{
    register value *v1, *v2;
    register unsigned short n;

    if ((n=m->size) == 0) {
	/* skip empty mapping */
	return;
    }

    v1 = v2 = d_get_elts(m);
    do {
	if (v2[0].type == T_OBJECT &&
	    o_object(&v2[0].u.object) == (object *) NULL) {
	    /*
	     * index is destructed object
	     */
	    switch (v2[1].type) {
	    case T_STRING:
		str_del(v2[1].u.string);
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		arr_del(v2[1].u.array);
		break;
	    }
	} else if (v2[1].type == T_OBJECT &&
		   o_object(&v2[1].u.object) == (object *) NULL) {
	    switch (v2[0].type) {
	    /*
	     * value is destructed object
	     */
	    case T_STRING:
		str_del(v2[0].u.string);
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		arr_del(v2[0].u.array);
		break;
	    }
	} else {
	    /*
	     * keep index/value pair
	     */
	    *v1++ = v2[0];
	    *v1++ = v2[1];
	}
	v2 += 2;
    } while ((n -= 2) != 0);

    if (v1 != v2) {
	/*
	 * mapping has changed
	 */
	m->size = v1 - m->elts;
	if (v1 == m->elts) {
	    /*
	     * nothing left
	     */
	    FREE(m->elts);
	    m->elts = (value *) NULL;
	}
	d_change_map(m);
    }
}

/*
 * NAME:	mapping->indices()
 * DESCRIPTION:	return the indices of a mapping
 */
array *map_indices(m)
array *m;
{
    register array *indices;
    register value *v1, *v2;
    register unsigned short n;

    map_compact(m);
    indices = a_new(n = m->size >> 1);
    v1 = indices->elts;
    for (v2 = m->elts; n > 0; v2 += 2, --n) {
	switch (v2->type) {
	case T_STRING:
	    str_ref(v2->u.string);
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_ref(v2->u.array);
	    break;
	}
	*v1++ = *v2;
    }

    return indices;
}

/*
 * NAME:	mapping->values()
 * DESCRIPTION:	return the values of a mapping
 */
array *map_values(m)
array *m;
{
    register array *values;
    register value *v1, *v2;
    register unsigned short n;

    map_compact(m);
    values = a_new(n = m->size >> 1);
    v1 = values->elts;
    for (v2 = m->elts + 1; n > 0; v2 += 2, --n) {
	switch (v2->type) {
	case T_STRING:
	    str_ref(v2->u.string);
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_ref(v2->u.array);
	    break;
	}
	*v1++ = *v2;
    }

    return values;
}
