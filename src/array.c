# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"

# define ARR_CHUNK	128

typedef struct _arrchunk_ {
    array a[ARR_CHUNK];		/* chunk of arrays */
    struct _arrchunk_ *next;	/* next in list */
} arrchunk;

typedef struct _arrh_ {
    struct _arrh_ *next;	/* next in hash table chain */
    array *arr;			/* array entry */
    uindex index;		/* building index */
    struct _arrh_ **link;	/* next in list */
} arrh;

typedef struct _arrhchunk_ {
    arrh ah[ARR_CHUNK];		/* chunk of arrh entries */
    struct _arrhchunk_ *next;	/* next in list */
} arrhchunk;

# define MELT_CHUNK	128

typedef struct _mapelt_ {
    unsigned short hashval;	/* hash value of index */
    value idx;			/* index */
    value val;			/* value */
    struct _mapelt_ *next;	/* next in hash table */
} mapelt;

typedef struct _meltchunk_ {
    mapelt e[MELT_CHUNK];	/* chunk of mapelt entries */
    struct _meltchunk_ *next;	/* next in list */
} meltchunk;

typedef struct _maphash_ {
    unsigned short size;	/* # elements in hash table */
    unsigned short tablesize;	/* actual hash table size */
    mapelt *table[1];		/* hash table */
} maphash;

# define MTABLE_SIZE	16	/* most mappings are quite small */

static unsigned long max_size;	/* max. size of array and mapping */
static Uint tag;		/* current array tag */
static arrchunk *aclist;	/* linked list of all array chunks */
static int achunksz;		/* size of current array chunk */
static array *flist;		/* free array list */
static arrh **ht;		/* array merge table */
static arrh **alink;		/* linked list of merged arrays */
static arrhchunk *ahlist;	/* linked list of all arrh chunks */
static int ahchunksz;		/* size of current arrh chunk */
static mapelt *fmelt;		/* free mapelt list */
static meltchunk *meltlist;	/* linked list of all mapelt chunks */
static int meltchunksz;		/* size of current mapelt chunk */
static uindex idx;		/* current building index */

/*
 * NAME:	array->init()
 * DESCRIPTION:	initialize array handling
 */
void arr_init(size)
unsigned int size;
{
    max_size = size;
    tag = 0;
    ht = ALLOC(arrh*, ARRMERGETABSZ);
    memset(ht, '\0', ARRMERGETABSZ * sizeof(arrh *));
    achunksz = ARR_CHUNK;
    ahchunksz = ARR_CHUNK;
    meltchunksz = MELT_CHUNK;
}

/*
 * NAME:	array->alloc()
 * DESCRIPTION:	create a new array
 */
array *arr_alloc(size)
unsigned int size;
{
    register array *a;

    if (flist != (array *) NULL) {
	/* from free list */
	a = flist;
	flist = (array *) a->primary;
    } else {
	if (achunksz == ARR_CHUNK) {
	    register arrchunk *l;

	    /* new chunk */
	    l = ALLOC(arrchunk, 1);
	    l->next = aclist;
	    aclist = l;
	    achunksz = 0;
	}
	a = &aclist->a[achunksz++];
    }
    a->size = size;
    a->elts = (value *) NULL;
    a->ref = 0;
    a->odcount = 0;			/* if swapped in, check objects */
    a->hashed = (maphash *) NULL;	/* only used for mappings */

    return a;
}

/*
 * NAME:	array->new()
 * DESCRIPTION:	create a new array
 */
array *arr_new(data, size)
dataspace *data;
register long size;
{
    register array *a;

    if (size > max_size) {
	error("Array too large");
    }
    a = arr_alloc((unsigned short) size);
    if (size > 0) {
	a->elts = ALLOC(value, size);
    }
    a->tag = tag++;
    a->odcount = odcount;
    a->primary = &data->alocal;
    return a;
}

/*
 * NAME:	array->del()
 * DESCRIPTION:	remove a reference from an array or mapping.  If none are
 *		left, the array/mapping is removed.
 */
void arr_del(a)
register array *a;
{
    if (--(a->ref) == 0) {
	register value *v;
	register unsigned short i;

	if ((v=a->elts) != (value *) NULL) {
	    for (i = a->size; i > 0; --i) {
		i_del_value(v++);
	    }
	    FREE(a->elts);
	}

	if (a->hashed != (maphash *) NULL) {
	    register mapelt *e, *n, **t;

	    /*
	     * delete the hashtable of a mapping
	     */
	    for (i = a->hashed->size, t = a->hashed->table; i > 0; t++) {
		for (e = *t; e != (mapelt *) NULL; e = n) {
		    i_del_value(&e->idx);
		    i_del_value(&e->val);
		    n = e->next;
		    e->next = fmelt;
		    fmelt = e;
		    --i;
		}
	    }
	    FREE(a->hashed);
	}

	a->primary = (arrref *) flist;
	flist = a;
    }
}

/*
 * NAME:	array->freeall()
 * DESCRIPTION:	free all array chunks and mapping element chunks
 */
void arr_freeall()
{
# ifdef DEBUG
    register arrchunk *ac;
    register meltchunk *mc;

    /* free array chunks */
    for (ac = aclist; ac != (arrchunk *) NULL; ) {
	register arrchunk *f;

	f = ac;
	ac = ac->next;
	FREE(f);
    }
# endif
    aclist = (arrchunk *) NULL;
    achunksz = ARR_CHUNK;

    flist = (array *) NULL;

# ifdef DEBUG
    /* free mapping element chunks */
    for (mc = meltlist; mc != (meltchunk *) NULL; ) {
	register meltchunk *f;

	f = mc;
	mc = mc->next;
	FREE(f);
    }
# endif
    meltlist = (meltchunk *) NULL;
    meltchunksz = MELT_CHUNK;
    fmelt = (mapelt *) NULL;
}

/*
 * NAME:	array->put()
 * DESCRIPTION:	Put an array in the merge table, and return its "index".
 */
uindex arr_put(a)
register array *a;
{
    register arrh **h;

    for (h = &ht[(unsigned long) a % ARRMERGETABSZ]; *h != (arrh *) NULL;
	 h = &(*h)->next) {
	if ((*h)->arr == a) {
	    return (*h)->index;
	}
    }
    /*
     * Add a new entry to the hash table.
     */
    if (ahchunksz == ARR_CHUNK) {
	register arrhchunk *l;

	l = ALLOC(arrhchunk, 1);
	l->next = ahlist;
	ahlist = l;
	ahchunksz = 0;
    }
    *h = &ahlist->ah[ahchunksz++];
    (*h)->next = (arrh *) NULL;
    arr_ref((*h)->arr = a);
    (*h)->index = idx;
    (*h)->link = alink;
    alink = h;

    return idx++;
}

/*
 * NAME:	array->clear()
 * DESCRIPTION:	clear the array merge table
 */
void arr_clear()
{
    register arrh **h;
    register arrhchunk *l;

    /* clear hash table */
    for (h = alink; h != (arrh **) NULL; ) {
	register arrh *f;

	f = *h;
	*h = (arrh *) NULL;
	arr_del(f->arr);
	h = f->link;
    }
    alink = (arrh **) NULL;
    idx = 0;

    /* free array hash chunks */
    for (l = ahlist; l != (arrhchunk *) NULL; ) {
	register arrhchunk *f;

	f = l;
	l = l->next;
	FREE(f);
    }
    ahlist = (arrhchunk *) NULL;
    ahchunksz = ARR_CHUNK;
}

/*
 * NAME:	copytmp()
 * DESCRIPTION:	make temporary copies of values
 */
static void copytmp(v1, a)
register value *v1;
register array *a;
{
    register value *v2;
    register unsigned short n;

    v2 = d_get_elts(a);
    if (a->odcount == odcount) {
	/*
	 * no need to check for destructed objects
	 */
	memcpy(v1, v2, a->size * sizeof(value));
    } else {
	/*
	 * Copy and check for destructed objects.  If destructed objects are
	 * found, they will be replaced by 0 in the original array.
	 */
	a->odcount = odcount;
	for (n = a->size; n != 0; --n) {
	    if (v2->type == T_OBJECT && DESTRUCTED(v2)) {
		*v2 = nil_value;
	    }
	    *v1++ = *v2++;
	}
    }
}

/*
 * NAME:	array->copy()
 * DESCRIPTION:	copy the elements of an array or mapping
 */
static void arr_copy(v, a)
value *v;
array *a;
{
    i_copy(v, d_get_elts(a), a->size);
    a->odcount = odcount;
}

/*
 * NAME:	array->add()
 * DESCRIPTION:	add two arrays
 */
array *arr_add(data, a1, a2)
dataspace *data;
register array *a1, *a2;
{
    register array *a;

    a = arr_new(data, (long) a1->size + a2->size);
    arr_copy(a->elts, a1);
    arr_copy(a->elts + a1->size, a2);
    d_ref_imports(a);

    return a;
}

static int cmp P((cvoid*, cvoid*));

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two values
 */
static int cmp(cv1, cv2)
cvoid *cv1, *cv2;
{
    register value *v1, *v2;
    register int i;
    xfloat f1, f2;

    v1 = (value *) cv1;
    v2 = (value *) cv2;
    i = v1->type - v2->type;
    if (i != 0) {
	return i;	/* order by type */
    }

    switch (v1->type) {
    case T_NIL:
	return 0;

    case T_INT:
	return (v1->u.number <= v2->u.number) ?
		(v1->u.number < v2->u.number) ? -1 : 0 :
		1;

    case T_FLOAT:
	VFLT_GET(v1, f1);
	VFLT_GET(v2, f2);
	return flt_cmp(&f1, &f2);

    case T_STRING:
	return str_cmp(v1->u.string, v2->u.string);

    case T_OBJECT:
	return (v1->oindex <= v2->oindex) ?
		(v1->oindex < v2->oindex) ? -1 : 0 :
		1;

    case T_ARRAY:
    case T_MAPPING:
	return (v1->u.array->tag <= v2->u.array->tag) ?
		(v1->u.array->tag < v2->u.array->tag) ? -1 : 0 :
		1;
    }
}

/*
 * NAME:	search()
 * DESCRIPTION:	search for a value in an array
 */
static int search(v1, v2, h, step, place)
register value *v1, *v2;
register unsigned short h;
register int step;		/* 1 for arrays, 2 for mappings */
bool place;
{
    register unsigned short l, m;
    register Int c;
    register value *v3;
    register unsigned short mask;

    mask = -step;
    l = 0;
    while (l < h) {
	m = ((l + h) >> 1) & mask;
	v3 = v2 + m;
	c = cmp(v1, v3);
	if (c == 0) {
	    if (T_INDEXED(v1->type) && v1->u.array != v3->u.array) {
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
		    if (m == h || !T_INDEXED(v3->type)) {
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
		    v3 -= step;
		    if (m == l || !T_INDEXED(v3->type)) {
			break;	/* out of range */
		    }
		    m -= step;
		    if (v1->u.array == v3->u.array) {
			return m;	/* found the right one */
		    }
		    if (v1->u.array->tag != v3->u.array->tag) {
			break;		/* wrong tag */
		    }
		}
		break;		/* not found */
	    }
	    return m;		/* found */
	} else if (c < 0) {
	    h = m;		/* search in lower half */
	} else {
	    l = m + step;	/* search in upper half */
	}
    }
    /*
     * not found
     */
    return (place) ? l : -1;
}

/*
 * NAME:	array->sub()
 * DESCRIPTION:	subtract one array from another
 */
array *arr_sub(data, a1, a2)
dataspace *data;
array *a1, *a2;
{
    register value *v1, *v2, *v3;
    register array *a3;
    register unsigned short n, size;

    if (a2->size == 0) {
	/*
	 * array - ({ })
	 * Return a copy of the first array.
	 */
	a3 = arr_new(data, (long) a1->size);
	arr_copy(a3->elts, a1);
	d_ref_imports(a3);
	return a3;
    }

    /* create new array */
    a3 = arr_new(data, (long) a1->size);
    if (a3->size == 0) {
	/* subtract from empty array */
	return a3;
    }
    size = a2->size;

    /* copy and sort values of subtrahend */
    copytmp(v2 = ALLOCA(value, size), a2);
    qsort(v2, size, sizeof(value), cmp);

    v1 = d_get_elts(a1);
    v3 = a3->elts;
    if (a1->odcount == odcount) {
	for (n = a1->size; n > 0; --n) {
	    if (search(v1, v2, size, 1, FALSE) < 0) {
		/*
		 * not found in subtrahend: copy to result array
		 */
		i_ref_value(v1);
		*v3++ = *v1;
	    }
	    v1++;
	}
    } else {
	a1->odcount = odcount;
	for (n = a1->size; n > 0; --n) {
	    if (v1->type == T_OBJECT && DESTRUCTED(v1)) {
		/* replace destructed object by nil */
		*v1 = nil_value;
	    }
	    if (search(v1, v2, size, 1, FALSE) < 0) {
		/*
		 * not found in subtrahend: copy to result array
		 */
		i_ref_value(v1);
		*v3++ = *v1;
	    }
	    v1++;
	}
    }
    AFREE(v2);	/* free copy of values of subtrahend */

    a3->size = v3 - a3->elts;
    if (a3->size == 0) {
	FREE(a3->elts);
	a3->elts = (value *) NULL;
    }

    d_ref_imports(a3);
    return a3;
}

/*
 * NAME:	array->intersect()
 * DESCRIPTION:	A - (A - B).  If A and B are sets, the result is a set also.
 */
array *arr_intersect(data, a1, a2)
dataspace *data;
array *a1, *a2;
{
    register value *v1, *v2, *v3;
    register array *a3;
    register unsigned short n, size;

    if (a1->size == 0 || a2->size == 0) {
	/* array & ({ }) */
	return arr_new(data, 0L);
    }

    /* create new array */
    a3 = arr_new(data, (long) a1->size);
    size = a2->size;

    /* copy and sort values of 2nd array */
    copytmp(v2 = ALLOCA(value, size), a2);
    qsort(v2, size, sizeof(value), cmp);

    v1 = d_get_elts(a1);
    v3 = a3->elts;
    if (a1->odcount == odcount) {
	for (n = a1->size; n > 0; --n) {
	    if (search(v1, v2, a2->size, 1, FALSE) >= 0) {
		/*
		 * element is in both arrays: copy to result array
		 */
		i_ref_value(v1);
		*v3++ = *v1;
	    }
	    v1++;
	}
    } else {
	a1->odcount = odcount;
	for (n = a1->size; n > 0; --n) {
	    if (v1->type == T_OBJECT && DESTRUCTED(v1)) {
		/* replace destructed object by nil */
		*v1 = nil_value;
	    }
	    if (search(v1, v2, a2->size, 1, FALSE) >= 0) {
		/*
		 * element is in both arrays: copy to result array
		 */
		i_ref_value(v1);
		*v3++ = *v1;
	    }
	    v1++;
	}
    }
    AFREE(v2);	/* free copy of values of 2nd array */

    a3->size = v3 - a3->elts;
    if (a3->size == 0) {
	FREE(a3->elts);
	a3->elts = (value *) NULL;
    }

    d_ref_imports(a3);
    return a3;
}

/*
 * NAME:	array->setadd()
 * DESCRIPTION:	A + (B - A).  If A and B are sets, the result is a set also.
 */
array *arr_setadd(data, a1, a2)
dataspace *data;
array *a1, *a2;
{
    register value *v, *v1, *v2;
    value *v3;
    register array *a3;
    register unsigned short n, size;

    if (a1->size == 0) {
	/* ({ }) | array */
	a3 = arr_new(data, (long) a2->size);
	arr_copy(a3->elts, a2);
	d_ref_imports(a3);
	return a3;
    }
    if (a2->size == 0) {
	/* array | ({ }) */
	a3 = arr_new(data, (long) a1->size);
	arr_copy(a3->elts, a1);
	d_ref_imports(a3);
	return a3;
    }

    /* make room for elements to add */
    v3 = ALLOCA(value, a2->size);

    /* copy and sort values of 1st array */
    copytmp(v1 = ALLOCA(value, size = a1->size), a1);
    qsort(v1, size, sizeof(value), cmp);

    v = v3;
    v2 = d_get_elts(a2);
    if (a2->odcount == odcount) {
	for (n = a2->size; n > 0; --n) {
	    if (search(v2, v1, size, 1, FALSE) < 0) {
		/*
		 * element is only in second array: copy to result array
		 */
		*v++ = *v2;
	    }
	    v2++;
	}
    } else {
	a2->odcount = odcount;
	for (n = a2->size; n > 0; --n) {
	    if (v2->type == T_OBJECT && DESTRUCTED(v2)) {
		/* replace destructed object by nil */
		*v2 = nil_value;
	    }
	    if (search(v2, v1, size, 1, FALSE) < 0) {
		/*
		 * element is only in second array: copy to result array
		 */
		*v++ = *v2;
	    }
	    v2++;
	}
    }
    AFREE(v1);	/* free copy of values of 1st array */

    n = v - v3;
    if ((long) size + n > max_size) {
	AFREE(v3);
	error("Array too large");
    }

    a3 = arr_new(data, (long) size + n);
    i_copy(a3->elts, a1->elts, size);
    i_copy(a3->elts + size, v3, n);
    AFREE(v3);

    d_ref_imports(a3);
    return a3;
}

/*
 * NAME:	array->setxadd()
 * DESCRIPTION:	(A - B) + (B - A).  If A and B are sets, the result is a set
 *		also.
 */
array *arr_setxadd(data, a1, a2)
dataspace *data;
array *a1, *a2;
{
    register value *v, *w, *v1, *v2;
    value *v3;
    register array *a3;
    register unsigned short n, size;
    unsigned short num;

    if (a1->size == 0) {
	/* ({ }) ^ array */
	a3 = arr_new(data, (long) a2->size);
	arr_copy(a3->elts, a2);
	d_ref_imports(a3);
	return a3;
    }
    if (a2->size == 0) {
	/* array ^ ({ }) */
	a3 = arr_new(data, (long) a1->size);
	arr_copy(a3->elts, a1);
	d_ref_imports(a3);
	return a3;
    }

    /* copy values of 1st array */
    copytmp(v1 = ALLOCA(value, size = a1->size), a1);

    /* copy and sort values of 2nd array */
    copytmp(v2 = ALLOCA(value, size = a2->size), a2);
    qsort(v2, size, sizeof(value), cmp);

    /* room for first half of result */
    v3 = ALLOCA(value, a1->size);

    v = v3;
    w = v1;
    for (n = a1->size; n > 0; --n) {
	if (search(v1, v2, size, 1, FALSE) < 0) {
	    /*
	     * element is only in first array: copy to result array
	     */
	    *v++ = *v1;
	} else {
	    /*
	     * element is in both: keep it for the next round
	     */
	    *w++ = *v1;
	}
	v1++;
    }
    num = v - v3;

    /* sort copy of 1st array */
    v1 -= a1->size;
    qsort(v1, size = w - v1, sizeof(value), cmp);

    v = v2;
    w = a2->elts;
    for (n = a2->size; n > 0; --n) {
	if (search(w, v1, size, 1, FALSE) < 0) {
	    /*
	     * element is only in second array: copy to 2nd result array
	     */
	    *v++ = *w;
	}
	w++;
    }

    n = v - v2;
    if ((long) num + n > max_size) {
	AFREE(v3);
	AFREE(v2);
	AFREE(v1);
	error("Array too large");
    }

    a3 = arr_new(data, (long) num + n);
    i_copy(a3->elts, v3, num);
    i_copy(a3->elts + num, v2, n);
    AFREE(v3);
    AFREE(v2);
    AFREE(v1);

    d_ref_imports(a3);
    return a3;
}

/*
 * NAME:	array->index()
 * DESCRIPTION:	index an array
 */
unsigned short arr_index(a, l)
register array *a;
register long l;
{
    if (l < 0 || l >= (long) a->size) {
	error("Array index out of range");
    }
    return l;
}

/*
 * NAME:	array->ckrange()
 * DESCRIPTION:	check an array subrange
 */
void arr_ckrange(a, l1, l2)
array *a;
register long l1, l2;
{
    if (l1 < 0 || l1 > l2 + 1 || l2 >= (long) a->size) {
	error("Invalid array range");
    }
}

/*
 * NAME:	array->range()
 * DESCRIPTION:	return a subrange of an array
 */
array *arr_range(data, a, l1, l2)
dataspace *data;
register array *a;
register long l1, l2;
{
    register array *range;

    if (l1 < 0 || l1 > l2 + 1 || l2 >= (long) a->size) {
	error("Invalid array range");
    }

    range = arr_new(data, l2 - l1 + 1);
    i_copy(range->elts, d_get_elts(a) + l1, (unsigned short) (l2 - l1 + 1));
    d_ref_imports(range);
    return range;
}


/*
 * NAME:	mapping->new()
 * DESCRIPTION:	create a new mapping
 */
array *map_new(data, size)
dataspace *data;
register long size;
{
    array *m;

    if (size > max_size << 1) {
	error("Mapping too large");
    }
    m = arr_alloc((unsigned short) size);
    if (size > 0) {
	m->elts = ALLOC(value, size);
    }
    m->tag = tag++;
    m->odcount = odcount;
    m->primary = &data->alocal;
    return m;
}

/*
 * NAME:	mapping->sort()
 * DESCRIPTION:	prune and sort a mapping
 */
void map_sort(m)
register array *m;
{
    register unsigned short i, sz;
    register value *v, *w;

    for (i = m->size, sz = 0, v = w = m->elts; i > 0; i -= 2) {
	if (!VAL_NIL(v + 1)) {
	    *w++ = *v++;
	    *w++ = *v++;
	    sz += 2;
	} else {
	    /* delete index and skip zero value */
	    i_del_value(v);
	    v += 2;
	}
    }

    if (sz != 0) {
	qsort(v = m->elts, i = sz >> 1, 2 * sizeof(value), cmp);
	while (--i != 0) {
	    if (cmp((cvoid *) v, (cvoid *) &v[2]) == 0 &&
		(!T_INDEXED(v->type) || v->u.array == v[2].u.array)) {
		error("Identical indices in mapping");
	    }
	    v += 2;
	}
    } else if (m->size > 0) {
	FREE(m->elts);
	m->elts = (value *) NULL;
    }
    m->size = sz;
}

/*
 * NAME:	mapping->clean()
 * DESCRIPTION:	remove destructed objects from mapping
 */
static void map_clean(m)
register array *m;
{
    register value *v1, *v2;
    register unsigned short i, size;

    if (m->odcount == odcount) {
	return;	/* no destructed objects */
    }

    /*
     * remove destructed objects in the array
     */
    if (m->size != 0) {
	size = 0;
	v1 = v2 = d_get_elts(m);
	for (i = m->size; i > 0; i -= 2) {
	    if (v2->type == T_OBJECT && DESTRUCTED(v2)) {
		/*
		 * index is destructed object
		 */
		d_assign_elt(m, v2 + 1, &nil_value);
		v2 += 2;
	    } else if (v2[1].type == T_OBJECT && DESTRUCTED(&v2[1])) {
		/*
		 * value is destructed object
		 */
		d_assign_elt(m, v2, &nil_value);
		v2 += 2;
	    } else {
		*v1++ = *v2++;
		*v1++ = *v2++;
		size += 2;
	    }
	}
	if (size == 0) {
	    FREE(m->elts);
	    m->elts = (value *) NULL;
	}
	if (size != m->size) {
	    d_change_map(m);
	}
	m->size = size;
    }

    /*
     * remove destructed objects in the hash table
     */
    if (m->hashed != (maphash *) NULL && m->hashed->size != 0) {
	register mapelt *e, **p, **t;

	size = 0;
	t = m->hashed->table;
	for (i = m->hashed->size; i > 0; ) {
	    for (p = t++; (e=*p) != (mapelt *) NULL; --i) {
		if (e->idx.type == T_OBJECT && DESTRUCTED(&e->idx)) {
		    /*
		     * index is destructed object
		     */
		    d_assign_elt(m, &e->val, &nil_value);
		} else if (e->val.type == T_OBJECT && DESTRUCTED(&e->val)) {
		    /*
		     * value is destructed object
		     */
		    d_assign_elt(m, &e->idx, &nil_value);
		} else {
		    size++;
		    p = &e->next;
		    continue;
		}
		*p = e->next;
		e->next = fmelt;
		fmelt = e;
	    }
	}
	m->hashed->size = size;
    }

    m->odcount = odcount;	/* update */
}

/*
 * NAME:	mapping->compact()
 * DESCRIPTION:	compact a mapping: put elements from the hash table into
 *		the array, and remove destructed objects
 */
void map_compact(m)
register array *m;
{
    register value *v1, *v2;
    register unsigned short i, arrsize, hashsize;

    if ((m->size == 0 || m->odcount == odcount) &&
	(m->hashed == (maphash *) NULL || m->hashed->size == 0)) {
	/* skip empty or unchanged mapping */
	return;
    }

    arrsize = 0;
    if (m->size > 0) {
	v1 = v2 = d_get_elts(m);
	if (m->odcount != odcount) {
	    /*
	     * remove destructed objects in the array
	     */
	    for (i = m->size; i > 0; i -= 2) {
		if (v2->type == T_OBJECT && DESTRUCTED(v2)) {
		    /*
		     * index is destructed object
		     */
		    d_assign_elt(m, v2 + 1, &nil_value);
		    v2 += 2;
		} else if (v2[1].type == T_OBJECT && DESTRUCTED(&v2[1])) {
		    /*
		     * value is destructed object
		     */
		    d_assign_elt(m, v2, &nil_value);
		    v2 += 2;
		} else {
		    *v1++ = *v2++;
		    *v1++ = *v2++;
		    arrsize += 2;
		}
	    }
	} else {
	    arrsize = m->size;
	}
    }

    /*
     * convert hashtable into sorted array
     */
    hashsize = 0;
    if (m->hashed != (maphash *) NULL) {
	if (m->hashed->size != 0) {
	    register mapelt *e, *n, **t;

	    v2 = ALLOCA(value, m->hashed->size << 1);
	    t = m->hashed->table;
	    if (m->odcount == odcount) {
		for (i = m->hashed->size; i > 0; ) {
		    for (e = *t++; e != (mapelt *) NULL; --i, e = n) {
			*v2++ = e->idx;
			*v2++ = e->val;
			n = e->next;
			e->next = fmelt;
			fmelt = e;
		    }
		}
		hashsize = m->hashed->size << 1;
	    } else {
		for (i = m->hashed->size; i > 0; ) {
		    for (e = *t++; e != (mapelt *) NULL; --i, e = n) {
			if (e->idx.type == T_OBJECT && DESTRUCTED(&e->idx)) {
			    /*
			     * index is destructed object
			     */
			    d_assign_elt(m, &e->val, &nil_value);
			} else if (e->val.type == T_OBJECT &&
				   DESTRUCTED(&e->val)) {
			    /*
			     * value is destructed object
			     */
			    d_assign_elt(m, &e->idx, &nil_value);
			} else {
			    /*
			     * copy to array
			     */
			    *v2++ = e->idx;
			    *v2++ = e->val;
			    hashsize += 2;
			}
			n = e->next;
			e->next = fmelt;
			fmelt = e;
		    }
		}
	    }
	    if (hashsize == 0) {
		AFREE(v2);	/* nothing in the hash table */
	    } else {
		v2 -= hashsize;
		qsort(v2, hashsize >> 1, 2 * sizeof(value), cmp);
	    }
	}
	FREE(m->hashed);
	m->hashed = (maphash *) NULL;
    }

    m->odcount = odcount;	/* update */

    if (hashsize > 0) {
	register value *v3;
	register unsigned short j;

	/*
	 * merge the two value arrays
	 */
	v1 = m->elts;
	v3 = ALLOC(value, arrsize + hashsize);
	for (i = arrsize, j = hashsize; i > 0 && j > 0; ) {
	    if (cmp(v1, v2) <= 0) {
		*v3++ = *v1++;
		*v3++ = *v1++;
		i -= 2;
	    } else {
		*v3++ = *v2++;
		*v3++ = *v2++;
		j -= 2;
	    }
	}

	/*
	 * copy tails of arrays
	 */
	memcpy(v3, v1, i * sizeof(value));
	v3 += i;
	memcpy(v3, v2, j * sizeof(value));
	v3 += j;

	AFREE(v2 - (hashsize - j));
	if (m->size > 0) {
	    FREE(m->elts);
	}
	m->size = arrsize + hashsize;
	m->elts = v3 - m->size;
    } else if (arrsize != m->size) {
	/*
	 * destructed objects were removed
	 */
	if (arrsize == 0) {
	    FREE(m->elts);
	    m->elts = (value *) NULL;
	}
	m->size = arrsize;
	d_change_map(m);
    }
}

/*
 * NAME:	mapping->size()
 * DESCRIPTION:	return the size of a mapping
 */
unsigned short map_size(m)
register array *m;
{
    unsigned short size;

    map_clean(m);
    size = m->size >> 1;
    if (m->hashed != (maphash *) NULL) {
	size += m->hashed->size;
    }
    return size;
}

/*
 * NAME:	mapping->add()
 * DESCRIPTION:	add two mappings
 */
array *map_add(data, m1, m2)
dataspace *data;
array *m1, *m2;
{
    register value *v1, *v2, *v3;
    register unsigned short n1, n2;
    register Int c;
    array *m3;

    map_compact(m1);
    map_compact(m2);
    m3 = map_new(data, (long) m1->size + m2->size);
    if (m3->size == 0) {
	/* add two empty mappings */
	return m3;
    }

    v1 = m1->elts;
    v2 = m2->elts;
    v3 = m3->elts;
    for (n1 = m1->size, n2 = m2->size; n1 > 0 && n2 > 0; ) {
	c = cmp(v1, v2);
	if (c < 0) {
	    /* the smaller element is in m1 */
	    i_copy(v3, v1, 2);
	    v1 += 2; v3 += 2; n1 -= 2;
	} else {
	    /* the smaller - or overriding - element is in m2 */
	    i_copy(v3, v2, 2);
	    v2 += 2; v3 += 2; n2 -= 2;
	    if (c == 0) {
		/* equal elements? */
		if (T_INDEXED(v1->type) && v1->u.array != v2->u.array) {
		    register value *v;
		    register unsigned short n;

		    /*
		     * The array tags are the same, but the arrays are not.
		     * Check ahead to see if the array is somewhere else
		     * in m2; if not, copy the element from m1 as well.
		     */
		    v = v2; n = n2;
		    for (;;) {
			v += 2; n -= 2;
			if (n == 0 || !T_INDEXED(v->type) ||
			    v->u.array->tag != v1->u.array->tag) {
			    /* not in m2 */
			    i_copy(v3, v1, 2);
			    v3 += 2;
			    break;
			}
			if (v->u.array == v1->u.array) {
			    /* also in m2 */
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
    i_copy(v3, v1, n1);
    v3 += n1;
    /* copy tail part of m2 */
    i_copy(v3, v2, n2);
    v3 += n2;

    m3->size = v3 - m3->elts;
    if (m3->size == 0) {
	FREE(m3->elts);
	m3->elts = (value *) NULL;
    }

    d_ref_imports(m3);
    return m3;
}

/*
 * NAME:	mapping->sub()
 * DESCRIPTION:	subtract an array from a mapping
 */
array *map_sub(data, m1, a2)
dataspace *data;
array *m1, *a2;
{
    register value *v1, *v2, *v3;
    register unsigned short n1, n2, size;
    register Int c;
    array *m3;

    map_compact(m1);
    m3 = map_new(data, (long) m1->size);
    if (m1->size == 0) {
	/* subtract from empty mapping */
	return m3;
    }
    if ((size=a2->size) == 0) {
	/* subtract empty array */
	arr_copy(m3->elts, m1);
	d_ref_imports(m3);
	return m3;
    }

    /* copy and sort values of array */
    copytmp(v2 = ALLOCA(value, size), a2);
    qsort(v2, size, sizeof(value), cmp);

    v1 = m1->elts;
    v3 = m3->elts;
    for (n1 = m1->size, n2 = size; n1 > 0 && n2 > 0; ) {
	c = cmp(v1, v2);
	if (c < 0) {
	    /* the smaller element is in m1 */
	    i_copy(v3, v1, 2);
	    v1 += 2; v3 += 2; n1 -= 2;
	} else if (c > 0) {
	    /* the smaller element is in a2 */
	    v2++; --n2;
	} else {
	    /* equal elements? */
	    if (T_INDEXED(v1->type) && v1->u.array != v2->u.array) {
		register value *v;
		register unsigned short n;

		/*
		 * The array tags are the same, but the arrays are not.
		 * Check ahead to see if the array is somewhere else
		 * in a2; if not, copy the element from m1.
		 */
		v = v2; n = n2;
		for (;;) {
		    v++; --n;
		    if (n == 0 || !T_INDEXED(v->type) ||
			v->u.array->tag != v1->u.array->tag) {
			/* not in a2 */
			i_copy(v3, v1, 2);
			v3 += 2;
			break;
		    }
		    if (v->u.array == v1->u.array) {
			/* also in a2 */
			break;
		    }
		}
	    }
	    /* skip m1 */
	    v1 += 2; n1 -= 2;
	}
    }
    AFREE(v2 - (size - n2));

    /* copy tail part of m1 */
    i_copy(v3, v1, n1);
    v3 += n1;

    m3->size = v3 - m3->elts;
    if (m3->size == 0) {
	FREE(m3->elts);
	m3->elts = (value *) NULL;
    }

    d_ref_imports(m3);
    return m3;
}

/*
 * NAME:	mapping->intersect()
 * DESCRIPTION:	intersect a mapping with an array
 */
array *map_intersect(data, m1, a2)
dataspace *data;
array *m1, *a2;
{
    register value *v1, *v2, *v3;
    register unsigned short n1, n2, size;
    register Int c;
    array *m3;

    map_compact(m1);
    if ((size=a2->size) == 0) {
	/* intersect with empty array */
	return map_new(data, 0L);
    }
    m3 = map_new(data, (long) m1->size);
    if (m1->size == 0) {
	/* intersect with empty mapping */
	return m3;
    }

    /* copy and sort values of array */
    copytmp(v2 = ALLOCA(value, size), a2);
    qsort(v2, size, sizeof(value), cmp);

    v1 = m1->elts;
    v3 = m3->elts;
    for (n1 = m1->size, n2 = size; n1 > 0 && n2 > 0; ) {
	c = cmp(v1, v2);
	if (c < 0) {
	    /* the smaller element is in m1 */
	    v1 += 2; n1 -= 2;
	} else if (c > 0) {
	    /* the smaller element is in a2 */
	    v2++; --n2;
	} else {
	    /* equal elements? */
	    if (T_INDEXED(v1->type) && v1->u.array != v2->u.array) {
		register value *v;
		register unsigned short n;

		/*
		 * The array tags are the same, but the arrays are not.
		 * Check ahead to see if the array is somewhere else
		 * in a2; if not, don't copy the element from m1.
		 */
		v = v2; n = n2;
		for (;;) {
		    v++; --n;
		    if (n == 0 || !T_INDEXED(v->type) ||
			v->u.array->tag != v1->u.array->tag) {
			/* not in a2 */
			break;
		    }
		    if (v->u.array == v1->u.array) {
			/* also in a2 */
			i_copy(v3, v1, 2);
			v3 += 2; v1 += 2; n1 -= 2;
			break;
		    }
		}
	    } else {
		/* equal */
		i_copy(v3, v1, 2);
		v3 += 2; v1 += 2; n1 -= 2;
	    }
	    v2++; --n2;
	}
    }
    AFREE(v2 - (size - n2));

    m3->size = v3 - m3->elts;
    if (m3->size == 0) {
	FREE(m3->elts);
	m3->elts = (value *) NULL;
    }

    d_ref_imports(m3);
    return m3;
}

/*
 * NAME:	mapping->grow()
 * DESCRIPTION:	add an element to a mapping
 */
static mapelt *map_grow(m, hashval)
register array *m;
unsigned short hashval;
{
    register maphash *h;
    register mapelt *e;
    register unsigned short i;

    h = m->hashed;
    if ((m->size >> 1) + ((h == (maphash *) NULL) ? 0 : h->size) >= max_size) {
	map_compact(m);
	if (m->size >> 1 >= max_size) {
	    error("Mapping too large to grow");
	}
	h = (maphash *) NULL;
    }

    if (h == (maphash *) NULL) {
	/*
	 * add hash table to this mapping
	 */
	m->hashed = h = (maphash *)
	    ALLOC(char, sizeof(maphash) + (MTABLE_SIZE - 1) * sizeof(mapelt*));
	h->size = 0;
	h->tablesize = MTABLE_SIZE;
	memset(h->table, '\0', MTABLE_SIZE * sizeof(mapelt*));
    } else if (h->size << 2 >= h->tablesize * 3) {
	register mapelt *n, **t;
	register unsigned short j;

	/*
	 * extend hash table for this mapping
	 */
	i = h->tablesize << 1;
	h = (maphash *) ALLOC(char,
			      sizeof(maphash) + (i - 1) * sizeof(mapelt*));
	h->size = m->hashed->size;
	h->tablesize = i;
	memset(h->table, '\0', i * sizeof(mapelt*));
	/*
	 * copy entries from old hashtable to new hashtable
	 */
	for (j = h->size, t = m->hashed->table; j > 0; t++) {
	    for (e = *t; e != (mapelt *) NULL; e = n) {
		n = e->next;
		i = e->hashval % h->tablesize;
		e->next = h->table[i];
		h->table[i] = e;
		--j;
	    }
	}
	FREE(m->hashed);
	m->hashed = h;
    }
    h->size++;

    if (fmelt != (mapelt *) NULL) {
	/* from free list */
	e = fmelt;
	fmelt = e->next;
    } else {
	if (meltchunksz == MELT_CHUNK) {
	    register meltchunk *l;

	    /* new chunk */
	    l = ALLOC(meltchunk, 1);
	    l->next = meltlist;
	    meltlist = l;
	    meltchunksz = 0;
	}
	e = &meltlist->e[meltchunksz++];
    }
    e->hashval = hashval;
    e->idx = nil_value;
    e->val = nil_value;
    i = hashval % h->tablesize;
    e->next = h->table[i];
    h->table[i] = e;

    return e;
}

/*
 * NAME:	mapping->index()
 * DESCRIPTION:	Index a mapping with a value. If a third argument is supplied,
 *		perform an assignment; otherwise return the indexed value.
 */
value *map_index(m, val, elt)
register array *m;
value *val, *elt;
{
    register unsigned short i;
    bool del;

    if (elt != (value *) NULL && VAL_NIL(elt)) {
	elt = (value *) NULL;
	del = TRUE;
    } else {
	del = FALSE;
    }

    if (m->size > 0) {
	register int n;

	n = search(val, d_get_elts(m), m->size, 2, FALSE);
	if (n >= 0) {
	    register value *v;

	    /*
	     * found in the array
	     */
	    v = &m->elts[n];
	    if (elt != (value *) NULL) {
		/*
		 * change the element
		 */
		if (val->type == T_OBJECT) {
		    v->modified = TRUE;
		    v->u.objcnt = val->u.objcnt;	/* refresh */
		}
		d_assign_elt(m, v + 1, elt);
	    } else if (del ||
		       (val->type == T_OBJECT &&
			val->u.objcnt != v->u.objcnt)) {
		/*
		 * delete the element
		 */
		d_assign_elt(m, v, &nil_value);
		d_assign_elt(m, v + 1, &nil_value);

		m->size -= 2;
		if (m->size == 0) {
		    /* last element removed */
		    FREE(m->elts);
		    m->elts = (value *) NULL;
		} else {
		    /* move tail */
		    memcpy(v, v + 2, (m->size - n) * sizeof(value));
		}
		d_change_map(m);
		return &nil_value;
	    }
	    return v + 1;
	}
    }

    switch (val->type) {
    case T_NIL:
	i = 4747;
	break;

    case T_INT:
	i = val->u.number;
	break;

    case T_FLOAT:
	i = VFLT_HASH(val);
	break;

    case T_STRING:
	i = hashstr(val->u.string->text, STRMAPHASHSZ) ^ val->u.string->len;
	break;

    case T_OBJECT:
	i = val->oindex;
	break;

    case T_ARRAY:
    case T_MAPPING:
	i = (unsigned short) ((unsigned long) val->u.array >> 3);
	break;
    }

    if (m->hashed != (maphash *) NULL) {
	register mapelt *e, **p;

	for (p = &m->hashed->table[i % m->hashed->tablesize];
	     (e=*p) != (mapelt *) NULL; p = &e->next) {
	    if (cmp(val, &e->idx) == 0 &&
		(!T_INDEXED(val->type) || val->u.array == e->idx.u.array)) {
		/*
		 * found in the hashtable
		 */
		if (elt != (value *) NULL) {
		    /*
		     * change element
		     */
		    if (val->type == T_OBJECT) {
			e->idx.u.objcnt = val->u.objcnt;	/* refresh */
		    }
		    d_assign_elt(m, &e->val, elt);
		} else if (del ||
			   (val->type == T_OBJECT &&
			    val->u.objcnt != e->idx.u.objcnt)) {
		    /*
		     * delete element
		     */
		    d_assign_elt(m, &e->idx, &nil_value);
		    d_assign_elt(m, &e->val, &nil_value);

		    *p = e->next;
		    e->next = fmelt;
		    fmelt = e;
		    m->hashed->size--;
		    return &nil_value;
		}
		return &e->val;
	    }
	}
    }

    if (elt != (value *) NULL) {
	register mapelt *e;

	/*
	 * extend mapping
	 */
	e = map_grow(m, i);
	d_change_map(m);

	d_assign_elt(m, &e->idx, val);
	d_assign_elt(m, &e->val, elt);
    }

    /*
     * not found
     */
    return &nil_value;
}

/*
 * NAME:	mapping->range()
 * DESCRIPTION:	return a mapping value subrange
 */
array *map_range(data, m, v1, v2)
dataspace *data;
array *m;
register value *v1, *v2;
{
    register unsigned short from, to;
    register array *range;

    map_compact(m);

    /* determine subrange */
    from = (v1 == (value *) NULL) ? 0 : search(v1, m->elts, m->size, 2, TRUE);
    if (v2 == (value *) NULL) {
	to = m->size;
    } else {
	to = search(v2, m->elts, m->size, 2, TRUE);
	if (to < m->size && cmp(v2, &m->elts[to]) == 0 &&
	    (!T_INDEXED(v2->type) || v2->u.array == m->elts[to].u.array)) {
	    /*
	     * include last element
	     */
	    to += 2;
	}
    }
    if (from >= to) {
	return map_new(data, 0L);	/* empty subrange */
    }

    /* copy subrange */
    range = map_new(data, (long) (to -= from));
    i_copy(range->elts, m->elts + from, to);

    d_ref_imports(range);
    return range;
}

/*
 * NAME:	mapping->indices()
 * DESCRIPTION:	return the indices of a mapping
 */
array *map_indices(data, m)
dataspace *data;
array *m;
{
    register array *indices;
    register value *v1, *v2;
    register unsigned short n;

    map_compact(m);
    indices = arr_new(data, (long) (n = m->size >> 1));
    v1 = indices->elts;
    for (v2 = m->elts; n > 0; v2 += 2, --n) {
	i_ref_value(v2);
	*v1++ = *v2;
    }

    d_ref_imports(indices);
    return indices;
}

/*
 * NAME:	mapping->values()
 * DESCRIPTION:	return the values of a mapping
 */
array *map_values(data, m)
dataspace *data;
array *m;
{
    register array *values;
    register value *v1, *v2;
    register unsigned short n;

    map_compact(m);
    values = arr_new(data, (long) (n = m->size >> 1));
    v1 = values->elts;
    for (v2 = m->elts + 1; n > 0; v2 += 2, --n) {
	i_ref_value(v2);
	*v1++ = *v2;
    }

    d_ref_imports(values);
    return values;
}
