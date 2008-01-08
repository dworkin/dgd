# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"
# include "parse.h"
# include "csupport.h"


# define COP_ADD	0	/* add callout patch */
# define COP_REMOVE	1	/* remove callout patch */
# define COP_REPLACE	2	/* replace callout patch */

typedef struct _copatch_ {
    short type;			/* add, remove, replace */
    uindex handle;		/* callout handle */
    dataplane *plane;		/* dataplane */
    Uint time;			/* start time */
    unsigned short mtime;	/* start time millisec component */
    cbuf *queue;		/* callout queue */
    struct _copatch_ *next;	/* next in linked list */
    dcallout aco;		/* added callout */
    dcallout rco;		/* removed callout */
} copatch;

# define COPCHUNKSZ	32

typedef struct _copchunk_ {
    struct _copchunk_ *next;	/* next in linked list */
    copatch cop[COPCHUNKSZ];	/* callout patches */
} copchunk;

typedef struct _coptable_ {
    copchunk *chunk;			/* callout patch chunk */
    unsigned short chunksz;		/* size of callout patch chunk */
    copatch *flist;			/* free list of callout patches */
    copatch *cop[COPATCHHTABSZ];	/* hash table of callout patches */
} coptable;

typedef struct {
    array **itab;			/* imported array replacement table */
    Uint itabsz;			/* size of table */
    Uint narr;				/* # of arrays */
} arrimport;

static dataplane *plist;		/* list of dataplanes */
static uindex ncallout;			/* # callouts added */
static dataspace *ifirst;		/* list of dataspaces with imports */


/*
 * NAME:	ref_rhs()
 * DESCRIPTION:	reference the right-hand side in an assignment
 */
static void ref_rhs(data, rhs)
register dataspace *data;
register value *rhs;
{
    register string *str;
    register array *arr;

    switch (rhs->type) {
    case T_STRING:
	str = rhs->u.string;
	if (str->primary != (strref *) NULL && str->primary->data == data) {
	    /* in this object */
	    str->primary->ref++;
	    data->plane->flags |= MOD_STRINGREF;
	} else {
	    /* not in this object: ref imported string */
	    data->plane->schange++;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr = rhs->u.array;
	if (arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->arr != (array *) NULL) {
		/* swapped in */
		arr->primary->ref++;
		data->plane->flags |= MOD_ARRAYREF;
	    } else {
		/* ref new array */
		data->plane->achange++;
	    }
	} else {
	    /* not in this object: ref imported array */
	    if (data->plane->imports++ == 0 && ifirst != data &&
		data->iprev == (dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (dataspace *) NULL;
		data->inext = ifirst;
		if (ifirst != (dataspace *) NULL) {
		    ifirst->iprev = data;
		}
		ifirst = data;
	    }
	    data->plane->achange++;
	}
	break;
    }
}

/*
 * NAME:	del_lhs()
 * DESCRIPTION:	delete the left-hand side in an assignment
 */
static void del_lhs(data, lhs)
register dataspace *data;
register value *lhs;
{
    register string *str;
    register array *arr;

    switch (lhs->type) {
    case T_STRING:
	str = lhs->u.string;
	if (str->primary != (strref *) NULL && str->primary->data == data) {
	    /* in this object */
	    if (--(str->primary->ref) == 0) {
		str->primary->str = (string *) NULL;
		str->primary = (strref *) NULL;
		str_del(str);
		data->plane->schange++;	/* last reference removed */
	    }
	    data->plane->flags |= MOD_STRINGREF;
	} else {
	    /* not in this object: deref imported string */
	    data->plane->schange--;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr = lhs->u.array;
	if (arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->arr != (array *) NULL) {
		/* swapped in */
		data->plane->flags |= MOD_ARRAYREF;
		if ((--(arr->primary->ref) & ~ARR_MOD) == 0) {
		    d_get_elts(arr);
		    arr->primary->arr = (array *) NULL;
		    arr->primary = &arr->primary->plane->alocal;
		    arr_del(arr);
		    data->plane->achange++;
		}
	    } else {
		/* deref new array */
		data->plane->achange--;
	    }
	} else {
	    /* not in this object: deref imported array */
	    data->plane->imports--;
	    data->plane->achange--;
	}
	break;
    }
}


/*
 * NAME:	data->alloc_call_out()
 * DESCRIPTION:	allocate a new callout
 */
static uindex d_alloc_call_out(data, handle, time, mtime, nargs, v)
register dataspace *data;
register uindex handle;
Uint time;
unsigned short mtime;
int nargs;
register value *v;
{
    register dcallout *co;

    if (data->ncallouts == 0) {
	/*
	 * the first in this object
	 */
	co = data->callouts = ALLOC(dcallout, 1);
	data->ncallouts = handle = 1;
	data->plane->flags |= MOD_NEWCALLOUT;
    } else {
	if (data->callouts == (dcallout *) NULL) {
	    d_get_callouts(data);
	}
	if (handle != 0) {
	    /*
	     * get a specific callout from the free list
	     */
	    co = &data->callouts[handle - 1];
	    if (handle == data->fcallouts) {
		data->fcallouts = co->co_next;
	    } else {
		data->callouts[co->co_prev - 1].co_next = co->co_next;
		if (co->co_next != 0) {
		    data->callouts[co->co_next - 1].co_prev = co->co_prev;
		}
	    }
	} else {
	    handle = data->fcallouts;
	    if (handle != 0) {
		/*
		 * from free list
		 */
		co = &data->callouts[handle - 1];
		if (co->co_next == 0 || co->co_next > handle) {
		    /* take 1st free callout */
		    data->fcallouts = co->co_next;
		} else {
		    /* take 2nd free callout */
		    co = &data->callouts[co->co_next - 1];
		    data->callouts[handle - 1].co_next = co->co_next;
		    if (co->co_next != 0) {
			data->callouts[co->co_next - 1].co_prev = handle;
		    }
		    handle = co - data->callouts + 1;
		}
		data->plane->flags |= MOD_CALLOUT;
	    } else {
		/*
		 * add new callout
		 */
		handle = data->ncallouts;
		co = data->callouts = REALLOC(data->callouts, dcallout, handle,
					      handle + 1);
		co += handle;
		data->ncallouts = ++handle;
		data->plane->flags |= MOD_NEWCALLOUT;
	    }
	}
    }

    co->time = time;
    co->mtime = mtime;
    co->nargs = nargs;
    memcpy(co->val, v, sizeof(co->val));
    switch (nargs) {
    default:
	ref_rhs(data, &v[3]);
    case 2:
	ref_rhs(data, &v[2]);
    case 1:
	ref_rhs(data, &v[1]);
    case 0:
	ref_rhs(data, &v[0]);
	break;
    }

    return handle;
}

/*
 * NAME:	data->free_call_out()
 * DESCRIPTION:	free a callout
 */
static void d_free_call_out(data, handle)
register dataspace *data;
unsigned int handle;
{
    register dcallout *co;
    register value *v;
    uindex n;

    co = &data->callouts[handle - 1];
    v = co->val;
    switch (co->nargs) {
    default:
	del_lhs(data, &v[3]);
	i_del_value(&v[3]);
    case 2:
	del_lhs(data, &v[2]);
	i_del_value(&v[2]);
    case 1:
	del_lhs(data, &v[1]);
	i_del_value(&v[1]);
    case 0:
	del_lhs(data, &v[0]);
	str_del(v[0].u.string);
	break;
    }
    v[0] = nil_value;

    n = data->fcallouts;
    if (n != 0) {
	data->callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    data->fcallouts = handle;

    data->plane->flags |= MOD_CALLOUT;
}


/*
 * NAME:	copatch->init()
 * DESCRIPTION:	initialize copatch table
 */
static void cop_init(plane)
dataplane *plane;
{
    memset(plane->coptab = ALLOC(coptable, 1), '\0', sizeof(coptable));
}

/*
 * NAME:	copatch->clean()
 * DESCRIPTION:	free copatch table
 */
static void cop_clean(plane)
dataplane *plane;
{
    register copchunk *c, *f;

    c = plane->coptab->chunk;
    while (c != (copchunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }

    FREE(plane->coptab);
    plane->coptab = (coptable *) NULL;
}

/*
 * NAME:	copatch->new()
 * DESCRIPTION:	create a new callout patch
 */
static copatch *cop_new(plane, c, type, handle, co, time, mtime, q)
dataplane *plane;
copatch **c;
int type;
unsigned int handle, mtime;
register dcallout *co;
Uint time;
cbuf *q;
{
    register coptable *tab;
    register copatch *cop;
    register int i;
    register value *v;

    /* allocate */
    tab = plane->coptab;
    if (tab->flist != (copatch *) NULL) {
	/* from free list */
	cop = tab->flist;
	tab->flist = cop->next;
    } else {
	/* newly allocated */
	if (tab->chunk == (copchunk *) NULL || tab->chunksz == COPCHUNKSZ) {
	    register copchunk *cc;

	    /* create new chunk */
	    cc = ALLOC(copchunk, 1);
	    cc->next = tab->chunk;
	    tab->chunk = cc;
	    tab->chunksz = 0;
	}

	cop = &tab->chunk->cop[tab->chunksz++];
    }

    /* initialize */
    cop->type = type;
    cop->handle = handle;
    if (type == COP_ADD) {
	cop->aco = *co;
    } else {
	cop->rco = *co;
    }
    for (i = (co->nargs > 3) ? 4 : co->nargs + 1, v = co->val; i > 0; --i) {
	i_ref_value(v++);
    }
    cop->time = time;
    cop->mtime = mtime;
    cop->plane = plane;
    cop->queue = q;

    /* add to hash table */
    cop->next = *c;
    return *c = cop;
}

/*
 * NAME:	copatch->del()
 * DESCRIPTION:	delete a callout patch
 */
static void cop_del(plane, c, del)
dataplane *plane;
copatch **c;
bool del;
{
    register copatch *cop;
    register dcallout *co;
    register int i;
    register value *v;
    coptable *tab;

    /* remove from hash table */
    cop = *c;
    *c = cop->next;

    if (del) {
	/* free referenced callout */
	co = (cop->type == COP_ADD) ? &cop->aco : &cop->rco;
	v = co->val;
	for (i = (co->nargs > 3) ? 4 : co->nargs + 1; i > 0; --i) {
	    i_del_value(v++);
	}
    }

    /* add to free list */
    tab = plane->coptab;
    cop->next = tab->flist;
    tab->flist = cop;
}

/*
 * NAME:	copatch->replace()
 * DESCRIPTION:	replace one callout patch with another
 */
static void cop_replace(cop, co, time, mtime, q)
register copatch *cop;
register dcallout *co;
Uint time;
unsigned int mtime;
cbuf *q;
{
    register int i;
    register value *v;

    cop->type = COP_REPLACE;
    cop->aco = *co;
    for (i = (co->nargs > 3) ? 4 : co->nargs + 1, v = co->val; i > 0; --i) {
	i_ref_value(v++);
    }
    cop->time = time;
    cop->mtime = mtime;
    cop->queue = q;
}

/*
 * NAME:	copatch->commit()
 * DESCRIPTION:	commit a callout replacement
 */
static void cop_commit(cop)
register copatch *cop;
{
    register int i;
    register value *v;

    cop->type = COP_ADD;
    for (i = (cop->rco.nargs > 3) ? 4 : cop->rco.nargs + 1, v = cop->rco.val;
	 i > 0; --i) {
	i_del_value(v++);
    }
}

/*
 * NAME:	copatch->release()
 * DESCRIPTION:	remove a callout replacement
 */
static void cop_release(cop)
register copatch *cop;
{
    register int i;
    register value *v;

    cop->type = COP_REMOVE;
    for (i = (cop->aco.nargs > 3) ? 4 : cop->aco.nargs + 1, v = cop->aco.val;
	 i > 0; --i) {
	i_del_value(v++);
    }
}

/*
 * NAME:	copatch->discard()
 * DESCRIPTION:	discard replacement
 */
static void cop_discard(cop)
copatch *cop;
{
    /* force unref of proper component later */
    cop->type = COP_ADD;
}


/*
 * NAME:	data->new_plane()
 * DESCRIPTION:	create a new dataplane
 */
void d_new_plane(data, level)
register dataspace *data;
Int level;
{
    register dataplane *p;
    register Uint i;

    p = ALLOC(dataplane, 1);

    p->level = level;
    p->flags = data->plane->flags;
    p->schange = data->plane->schange;
    p->achange = data->plane->achange;
    p->imports = data->plane->imports;

    /* copy value information from previous plane */
    p->original = (value *) NULL;
    p->alocal.arr = (array *) NULL;
    p->alocal.plane = p;
    p->alocal.data = data;
    p->alocal.state = AR_CHANGED;
    p->coptab = data->plane->coptab;

    if (data->plane->arrays != (arrref *) NULL) {
	register arrref *a, *b;

	p->arrays = ALLOC(arrref, i = data->narrays);
	for (a = p->arrays, b = data->plane->arrays; i != 0; a++, b++, --i) {
	    if (b->arr != (array *) NULL) {
		*a = *b;
		a->arr->primary = a;
		arr_ref(a->arr);
	    } else {
		a->arr = (array *) NULL;
	    }
	}
    } else {
	p->arrays = (arrref *) NULL;
    }
    p->achunk = (abchunk *) NULL;

    if (data->plane->strings != (strref *) NULL) {
	register strref *s, *t;

	p->strings = ALLOC(strref, i = data->nstrings);
	for (s = p->strings, t = data->plane->strings; i != 0; s++, t++, --i) {
	    if (t->str != (string *) NULL) {
		*s = *t;
		s->str->primary = s;
		str_ref(s->str);
	    } else {
		s->str = (string *) NULL;
	    }
	}
    } else {
	p->strings = (strref *) NULL;
    }

    p->prev = data->plane;
    data->plane = p;
    p->plist = plist;
    plist = p;
}

/*
 * NAME:	commit_values()
 * DESCRIPTION:	commit non-swapped arrays among the values
 */
static void commit_values(v, n, level)
register value *v;
register unsigned int n;
register Int level;
{
    register array *arr;

    while (n != 0) {
	if (T_INDEXED(v->type)) {
	    arr = v->u.array;
	    if (arr->primary->arr == (array *) NULL &&
		arr->primary->plane->level > level) {
		if (arr->hashmod) {
		    map_compact(arr->primary->data, arr);
		}
		arr->primary = &arr->primary->plane->prev->alocal;
		commit_values(arr->elts, arr->size, level);
	    }

	}
	v++;
	--n;
    }
}

/*
 * NAME:	commit_callouts()
 * DESCRIPTION:	commit callout patches to previous plane
 */
static void commit_callouts(plane, merge)
register dataplane *plane;
bool merge;
{
    register dataplane *prev;
    register copatch **c, **n, *cop;
    copatch **t, **next;
    int i;

    prev = plane->prev;
    for (i = COPATCHHTABSZ, t = plane->coptab->cop; --i >= 0; t++) {
	if (*t != (copatch *) NULL && (*t)->plane == plane) {
	    /*
	     * find previous plane in hash chain
	     */
	    next = t;
	    do {
		next = &(*next)->next;
	    } while (*next != (copatch *) NULL && (*next)->plane == plane);

	    c = t;
	    do {
		cop = *c;
		if (cop->type != COP_REMOVE) {
		    commit_values(cop->aco.val + 1,
				  (cop->aco.nargs > 3) ? 3 : cop->aco.nargs,
				  prev->level);
		}

		if (prev->level == 0) {
		    /*
		     * commit to last plane
		     */
		    switch (cop->type) {
		    case COP_ADD:
			co_new(plane->alocal.data->oindex, cop->handle,
			       cop->time, cop->mtime, cop->queue);
			--ncallout;
			break;

		    case COP_REMOVE:
			co_del(plane->alocal.data->oindex, cop->handle,
			       cop->rco.time, cop->rco.mtime);
			ncallout++;
			break;

		    case COP_REPLACE:
			co_del(plane->alocal.data->oindex, cop->handle,
			       cop->rco.time, cop->rco.mtime);
			co_new(plane->alocal.data->oindex, cop->handle,
			       cop->time, cop->mtime, cop->queue);
			cop_commit(cop);
			break;
		    }

		    if (next == &cop->next) {
			next = c;
		    }
		    cop_del(plane, c, TRUE);
		} else {
		    /*
		     * commit to previous plane
		     */
		    cop->plane = prev;
		    if (merge) {
			for (n = next;
			     *n != (copatch *) NULL && (*n)->plane == prev;
			     n = &(*n)->next) {
			    if (cop->handle == (*n)->handle) {
				switch (cop->type) {
				case COP_ADD:
				    /* turn old remove into replace, del new */
				    cop_replace(*n, &cop->aco, cop->time,
						cop->mtime, cop->queue);
				    if (next == &cop->next) {
					next = c;
				    }
				    cop_del(prev, c, TRUE);
				    break;

				case COP_REMOVE:
				    if ((*n)->type == COP_REPLACE) {
					/* turn replace back into remove */
					cop_release(*n);
				    } else {
					/* del old */
					cop_del(prev, n, TRUE);
				    }
				    /* del new */
				    if (next == &cop->next) {
					next = c;
				    }
				    cop_del(prev, c, TRUE);
				    break;

				case COP_REPLACE:
				    if ((*n)->type == COP_REPLACE) {
					/* merge replaces into old, del new */
					cop_release(*n);
					cop_replace(*n, &cop->aco, cop->time,
						    cop->mtime, cop->queue);
					if (next == &cop->next) {
					    next = c;
					}
					cop_del(prev, c, TRUE);
				    } else {
					/* make replace into add, remove old */
					cop_del(prev, n, TRUE);
					cop_commit(cop);
				    }
				    break;
				}
				break;
			    }
			}
		    }

		    if (*c == cop) {
			c = &cop->next;
		    }
		}
	    } while (c != next);
	}
    }
}

/*
 * NAME:	data->commit_plane()
 * DESCRIPTION:	commit the current data plane
 */
void d_commit_plane(level, retval)
Int level;
value *retval;
{
    register dataplane *p, *commit, **r, **cr;
    register dataspace *data;
    register value *v;
    register Uint i;
    dataplane *clist;

    /*
     * pass 1: construct commit planes
     */
    clist = (dataplane *) NULL;
    cr = &clist;
    for (r = &plist, p = *r; p != (dataplane *) NULL && p->level == level;
	 r = &p->plist, p = *r) {
	if (p->prev->level != level - 1) {
	    /* insert commit plane */
	    commit = ALLOC(dataplane, 1);
	    commit->level = level - 1;
	    commit->original = (value *) NULL;
	    commit->alocal.arr = (array *) NULL;
	    commit->alocal.plane = commit;
	    commit->alocal.data = p->alocal.data;
	    commit->alocal.state = AR_CHANGED;
	    commit->arrays = p->arrays;
	    commit->achunk = p->achunk;
	    commit->strings = p->strings;
	    commit->coptab = p->coptab;
	    commit->prev = p->prev;
	    *cr = commit;
	    cr = &commit->plist;

	    p->prev = commit;
	} else {
	    p->flags |= PLANE_MERGE;
	}
    }
    if (clist != (dataplane *) NULL) {
	/* insert commit planes in plane list */
	*cr = p;
	*r = clist;
    }
    clist = *r;	/* sentinel */

    /*
     * pass 2: commit
     */
    for (p = plist; p != clist; p = p->plist) {
	/*
	 * commit changes to previous plane
	 */
	data = p->alocal.data;
	if (p->original != (value *) NULL) {
	    if (p->level == 1 || p->prev->original != (value *) NULL) {
		/* free backed-up variable values */
		for (v = p->original, i = data->nvariables; i != 0; v++, --i) {
		    i_del_value(v);
		}
		FREE(p->original);
	    } else {
		/* move originals to previous plane */
		p->prev->original = p->original;
	    }
	    commit_values(data->variables, data->nvariables, level - 1);
	}

	if (p->coptab != (coptable *) NULL) {
	    /* commit callout changes */
	    commit_callouts(p, p->flags & PLANE_MERGE);
	    if (p->level == 1) {
		cop_clean(p);
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	arr_commit(&p->achunk, p->prev, p->flags & PLANE_MERGE);
	if (p->flags & PLANE_MERGE) {
	    if (p->arrays != (arrref *) NULL) {
		register arrref *a;

		/* remove old array refs */
		for (a = p->prev->arrays, i = data->narrays; i != 0; a++, --i) {
		    if (a->arr != (array *) NULL) {
			if (a->arr->primary == &p->alocal) {
			    a->arr->primary = &p->prev->alocal;
			}
			arr_del(a->arr);
		    }
		}
		FREE(p->prev->arrays);
		p->prev->arrays = p->arrays;
	    }

	    if (p->strings != (strref *) NULL) {
		register strref *s;

		/* remove old string refs */
		for (s = p->prev->strings, i = data->nstrings; i != 0; s++, --i)
		{
		    if (s->str != (string *) NULL) {
			str_del(s->str);
		    }
		}
		FREE(p->prev->strings);
		p->prev->strings = p->strings;
	    }
	}
    }
    commit_values(retval, 1, level - 1);

    /*
     * pass 3: deallocate
     */
    for (p = plist; p != clist; p = plist) {
	p->prev->flags = p->flags & MOD_ALL | MOD_SAVE;
	p->prev->schange = p->schange;
	p->prev->achange = p->achange;
	p->prev->imports = p->imports;
	p->alocal.data->plane = p->prev;
	plist = p->plist;
	FREE(p);
    }
}

/*
 * NAME:	discard_callouts()
 * DESCRIPTION:	discard callout patches on current plane, restoring old callouts
 */
static void discard_callouts(plane)
register dataplane *plane;
{
    register copatch *cop, **c, **t;
    register dataspace *data;
    register int i;

    data = plane->alocal.data;
    for (i = COPATCHHTABSZ, t = plane->coptab->cop; --i >= 0; t++) {
	c = t;
	while (*c != (copatch *) NULL && (*c)->plane == plane) {
	    cop = *c;
	    switch (cop->type) {
	    case COP_ADD:
		d_free_call_out(data, cop->handle);
		cop_del(plane, c, TRUE);
		--ncallout;
		break;

	    case COP_REMOVE:
		d_alloc_call_out(data, cop->handle, cop->rco.time,
				 cop->rco.mtime, cop->rco.nargs, cop->rco.val);
		cop_del(plane, c, FALSE);
		ncallout++;
		break;

	    case COP_REPLACE:
		d_free_call_out(data, cop->handle);
		d_alloc_call_out(data, cop->handle, cop->rco.time,
				 cop->rco.mtime, cop->rco.nargs, cop->rco.val);
		cop_discard(cop);
		cop_del(plane, c, TRUE);
		break;
	    }
	}
    }
}

/*
 * NAME:	data->discard_plane()
 * DESCRIPTION:	discard the current data plane without committing it
 */
void d_discard_plane(level)
Int level;
{
    register dataplane *p;
    register dataspace *data;
    register value *v;
    register Uint i;

    for (p = plist; p != (dataplane *) NULL && p->level == level; p = p->plist)
    {
	/*
	 * discard changes except for callout mods
	 */
	p->prev->flags |= p->flags & (MOD_CALLOUT | MOD_NEWCALLOUT);

	data = p->alocal.data;
	if (p->original != (value *) NULL) {
	    /* restore original variable values */
	    for (v = data->variables, i = data->nvariables; i != 0; --i, v++) {
		i_del_value(v);
	    }
	    memcpy(data->variables, p->original,
		   data->nvariables * sizeof(value));
	    FREE(p->original);
	}

	if (p->coptab != (coptable *) NULL) {
	    /* undo callout changes */
	    discard_callouts(p);
	    if (p->prev == &data->base) {
		cop_clean(p);
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	arr_discard(&p->achunk);
	if (p->arrays != (arrref *) NULL) {
	    register arrref *a;

	    /* delete new array refs */
	    for (a = p->arrays, i = data->narrays; i != 0; a++, --i) {
		if (a->arr != (array *) NULL) {
		    arr_del(a->arr);
		}
	    }
	    FREE(p->arrays);
	    /* fix old ones */
	    for (a = p->prev->arrays, i = data->narrays; i != 0; a++, --i) {
		if (a->arr != (array *) NULL) {
		    a->arr->primary = a;
		}
	    }
	}

	if (p->strings != (strref *) NULL) {
	    register strref *s;

	    /* delete new string refs */
	    for (s = p->strings, i = data->nstrings; i != 0; s++, --i) {
		if (s->str != (string *) NULL) {
		    str_del(s->str);
		}
	    }
	    FREE(p->strings);
	    /* fix old ones */
	    for (s = p->prev->strings, i = data->nstrings; i != 0; s++, --i) {
		if (s->str != (string *) NULL) {
		    s->str->primary = s;
		}
	    }
	}

	data->plane = p->prev;
	plist = p->plist;
	FREE(p);
    }
}


/*
 * NAME:	data->commit_arr()
 * DESCRIPTION:	commit array to previous plane
 */
abchunk **d_commit_arr(arr, prev, old)
register array *arr;
dataplane *prev, *old;
{
    if (arr->primary->plane != prev) {
	if (arr->hashmod) {
	    map_compact(arr->primary->data, arr);
	}

	if (arr->primary->arr == (array *) NULL) {
	    arr->primary = &prev->alocal;
	} else {
	    arr->primary->plane = prev;
	}
	commit_values(arr->elts, arr->size, prev->level);
    }

    return (prev == old) ? (abchunk **) NULL : &prev->achunk;
}

/*
 * NAME:	data->discard_arr()
 * DESCRIPTION:	restore array to previous plane
 */
void d_discard_arr(arr, plane)
array *arr;
dataplane *plane;
{
    /* swapped-in arrays will be fixed later */
    arr->primary = &plane->alocal;
}


/*
 * NAME:	data->ref_imports()
 * DESCRIPTION:	check the elements of an array for imports
 */
void d_ref_imports(arr)
array *arr;
{
    register dataspace *data;
    register unsigned short n;
    register value *v;

    data = arr->primary->data;
    for (n = arr->size, v = arr->elts; n > 0; --n, v++) {
	if (T_INDEXED(v->type) && data != v->u.array->primary->data) {
	    /* mark as imported */
	    if (data->plane->imports++ == 0 && ifirst != data &&
		data->iprev == (dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (dataspace *) NULL;
		data->inext = ifirst;
		if (ifirst != (dataspace *) NULL) {
		    ifirst->iprev = data;
		}
		ifirst = data;
	    }
	}
    }
}

/*
 * NAME:	data->assign_var()
 * DESCRIPTION:	assign a value to a variable
 */
void d_assign_var(data, var, val)
register dataspace *data;
register value *var;
register value *val;
{
    if (var >= data->variables && var < data->variables + data->nvariables) {
	if (data->plane->level != 0 &&
	    data->plane->original == (value *) NULL) {
	    /*
	     * back up variables
	     */
	    i_copy(data->plane->original = ALLOC(value, data->nvariables),
		   data->variables, data->nvariables);
	}
	ref_rhs(data, val);
	del_lhs(data, var);
	data->plane->flags |= MOD_VARIABLE;
    }

    i_ref_value(val);
    i_del_value(var);

    *var = *val;
    var->modified = TRUE;
}

/*
 * NAME:	data->get_extravar()
 * DESCRIPTION:	get an object's special value
 */
value *d_get_extravar(data)
dataspace *data;
{
    return d_get_variable(data, data->nvariables - 1);
}

/*
 * NAME:	data->set_extravar()
 * DESCRIPTION:	set an object's special value
 */
void d_set_extravar(data, val)
register dataspace *data;
value *val;
{
    d_assign_var(data, d_get_variable(data, data->nvariables - 1), val);
}

/*
 * NAME:	data->wipe_extravar()
 * DESCRIPTION:	wipe out an object's special value
 */
void d_wipe_extravar(data)
register dataspace *data;
{
    d_assign_var(data, d_get_variable(data, data->nvariables - 1), &nil_value);

    if (data->parser != (struct _parser_ *) NULL) {
	/*
	 * get rid of the parser, too
	 */
	ps_del(data->parser);
	data->parser = (struct _parser_ *) NULL;
    }
}

/*
 * NAME:	data->assign_elt()
 * DESCRIPTION:	assign a value to an array element
 */
void d_assign_elt(data, arr, elt, val)
register dataspace *data;
register array *arr;
register value *elt, *val;
{
    if (data->plane->level != arr->primary->data->plane->level) {
	/*
	 * bring dataspace of imported array up to the current plane level
	 */
	d_new_plane(arr->primary->data, data->plane->level);
    }

    data = arr->primary->data;
    if (arr->primary->plane != data->plane) {
	/*
	 * backup array's current elements
	 */
	arr_backup(&data->plane->achunk, arr);
	if (arr->primary->arr != (array *) NULL) {
	    arr->primary->plane = data->plane;
	} else {
	    arr->primary = &data->plane->alocal;
	}
    }

    if (arr->primary->arr != (array *) NULL) {
	/*
	 * the array is in the loaded dataspace of some object
	 */
	if ((arr->primary->ref & ARR_MOD) == 0) {
	    arr->primary->ref |= ARR_MOD;
	    data->plane->flags |= MOD_ARRAY;
	}
	ref_rhs(data, val);
	del_lhs(data, elt);
    } else {
	if (T_INDEXED(val->type) && data != val->u.array->primary->data) {
	    /* mark as imported */
	    if (data->plane->imports++ == 0 && ifirst != data &&
		data->iprev == (dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (dataspace *) NULL;
		data->inext = ifirst;
		if (ifirst != (dataspace *) NULL) {
		    ifirst->iprev = data;
		}
		ifirst = data;
	    }
	}
	if (T_INDEXED(elt->type) && data != elt->u.array->primary->data) {
	    /* mark as unimported */
	    data->plane->imports--;
	}
    }

    i_ref_value(val);
    i_del_value(elt);

    *elt = *val;
    elt->modified = TRUE;
}

/*
 * NAME:	data->change_map()
 * DESCRIPTION:	mark a mapping as changed in size
 */
void d_change_map(map)
array *map;
{
    register arrref *a;

    a = map->primary;
    if (a->state == AR_UNCHANGED) {
	a->plane->achange++;
	a->state = AR_CHANGED;
    }
}


/*
 * NAME:	data->new_call_out()
 * DESCRIPTION:	add a new callout
 */
uindex d_new_call_out(data, func, delay, mdelay, f, nargs)
register dataspace *data;
string *func;
Int delay;
unsigned int mdelay;
register frame *f;
int nargs;
{
    Uint ct, t;
    unsigned short m;
    cbuf *q;
    value v[4];
    uindex handle;

    ct = co_check(ncallout, delay, mdelay, &t, &m, &q);
    if (ct == 0 && q == (cbuf *) NULL) {
	/* callouts are disabled */
	return 0;
    }

    PUT_STRVAL(&v[0], func);
    switch (nargs) {
    case 3:
	v[3] = f->sp[2];
    case 2:
	v[2] = f->sp[1];
    case 1:
	v[1] = f->sp[0];
    case 0:
	break;

    default:
	v[1] = f->sp[0];
	v[2] = f->sp[1];
	PUT_ARRVAL(&v[3], arr_new(data, nargs - 2L));
	memcpy(v[3].u.array->elts, f->sp + 2, (nargs - 2) * sizeof(value));
	d_ref_imports(v[3].u.array);
	break;
    }
    f->sp += nargs;
    handle = d_alloc_call_out(data, 0, ct, m, nargs, v);

    if (data->plane->level == 0) {
	/*
	 * add normal callout
	 */
	co_new(data->oindex, handle, t, m, q);
    } else {
	register dataplane *plane;
	register copatch **c, *cop;
	dcallout *co;
	copatch **cc;

	/*
	 * add callout patch
	 */
	plane = data->plane;
	if (plane->coptab == (coptable *) NULL) {
	    cop_init(plane);
	}
	co = &data->callouts[handle - 1];
	cc = c = &plane->coptab->cop[handle % COPATCHHTABSZ];
	for (;;) {
	    cop = *c;
	    if (cop == (copatch *) NULL || cop->plane != plane) {
		/* add new */
		cop_new(plane, cc, COP_ADD, handle, co, t, m, q);
		break;
	    }

	    if (cop->handle == handle) {
		/* replace removed */
		cop_replace(cop, co, t, m, q);
		break;
	    }

	    c = &cop->next;
	}

	ncallout++;
    }

    return handle;
}

/*
 * NAME:	data->del_call_out()
 * DESCRIPTION:	remove a callout
 */
Int d_del_call_out(data, handle, mtime)
dataspace *data;
Uint handle;
unsigned short *mtime;
{
    register dcallout *co;
    Int t;

    *mtime = 0xffff;
    if (handle == 0 || handle > data->ncallouts) {
	/* no such callout */
	return -1;
    }
    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    co = &data->callouts[handle - 1];
    if (co->val[0].type != T_STRING) {
	/* invalid callout */
	return -1;
    }

    *mtime = co->mtime;
    t = co_remaining(co->time, mtime);
    if (data->plane->level == 0) {
	/*
	 * remove normal callout
	 */
	co_del(data->oindex, (uindex) handle, co->time, co->mtime);
    } else {
	register dataplane *plane;
	register copatch **c, *cop;
	copatch **cc;

	/*
	 * add/remove callout patch
	 */
	--ncallout;

	plane = data->plane;
	if (plane->coptab == (coptable *) NULL) {
	    cop_init(plane);
	}
	cc = c = &plane->coptab->cop[handle % COPATCHHTABSZ];
	for (;;) {
	    cop = *c;
	    if (cop == (copatch *) NULL || cop->plane != plane) {
		/* delete new */
		cop_new(plane, cc, COP_REMOVE, (uindex) handle, co, (Uint) 0, 0,
			(cbuf *) NULL);
		break;
	    }
	    if (cop->handle == handle) {
		/* delete existing */
		if (cop->type == COP_REPLACE) {
		    cop_release(cop);
		} else {
		    cop_del(plane, c, TRUE);
		}
		break;
	    }
	    c = &cop->next;
	}
    }
    d_free_call_out(data, (uindex) handle);

    return t;
}

/*
 * NAME:	data->get_call_out()
 * DESCRIPTION:	get a callout
 */
string *d_get_call_out(data, handle, f, nargs)
dataspace *data;
unsigned int handle;
register frame *f;
int *nargs;
{
    string *str;
    register dcallout *co;
    register value *v, *o;
    register uindex n;

    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    co = &data->callouts[handle - 1];
    v = co->val;
    del_lhs(data, &v[0]);
    str = v[0].u.string;

    i_grow_stack(f, (*nargs = co->nargs) + 1);
    *--f->sp = v[0];

    switch (co->nargs) {
    case 3:
	del_lhs(data, &v[3]);
	*--f->sp = v[3];
    case 2:
	del_lhs(data, &v[2]);
	*--f->sp = v[2];
    case 1:
	del_lhs(data, &v[1]);
	*--f->sp = v[1];
    case 0:
	break;

    default:
	n = co->nargs - 2;
	f->sp -= n;
	memcpy(f->sp, d_get_elts(v[3].u.array), n * sizeof(value));
	del_lhs(data, &v[3]);
	FREE(v[3].u.array->elts);
	v[3].u.array->elts = (value *) NULL;
	arr_del(v[3].u.array);
	del_lhs(data, &v[2]);
	*--f->sp = v[2];
	del_lhs(data, &v[1]);
	*--f->sp = v[1];
	break;
    }

    /* wipe out destructed objects */
    for (n = co->nargs, v = f->sp; n > 0; --n, v++) {
	switch (v->type) {
	case T_OBJECT:
	    if (DESTRUCTED(v)) {
		*v = nil_value;
	    }
	    break;

	case T_LWOBJECT:
	    o = d_get_elts(v->u.array);
	    if (DESTRUCTED(o)) {
		arr_del(v->u.array);
		*v = nil_value;
	    }
	    break;
	}
    }

    co->val[0] = nil_value;
    n = data->fcallouts;
    if (n != 0) {
	data->callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    data->fcallouts = handle;

    data->plane->flags |= MOD_CALLOUT;
    return str;
}

/*
 * NAME:	data->list_callouts()
 * DESCRIPTION:	list all call_outs in an object
 */
array *d_list_callouts(host, data)
dataspace *host;
register dataspace *data;
{
    register uindex n, count, size;
    register dcallout *co;
    register value *v, *v2, *elts;
    array *list, *a;
    uindex max_args;
    xfloat flt;

    if (data->ncallouts == 0) {
	return arr_new(host, 0L);
    }
    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    /* get the number of callouts in this object */
    count = data->ncallouts;
    for (n = data->fcallouts; n != 0; n = data->callouts[n - 1].co_next) {
	--count;
    }
    if (count > conf_array_size()) {
	return (array *) NULL;
    }

    list = arr_new(host, (long) count);
    elts = list->elts;
    max_args = conf_array_size() - 3;

    for (co = data->callouts; count > 0; co++) {
	if (co->val[0].type == T_STRING) {
	    size = co->nargs;
	    if (size > max_args) {
		/* unlikely, but possible */
		size = max_args;
	    }
	    a = arr_new(host, size + 3L);
	    v = a->elts;

	    /* handle */
	    PUT_INTVAL(v, co - data->callouts + 1);
	    v++;
	    /* function */
	    PUT_STRVAL(v, co->val[0].u.string);
	    v++;
	    /* time */
	    if (co->mtime == 0xffff) {
		PUT_INTVAL(v, co->time);
	    } else {
		flt.low = co->time;
		flt.high = co->mtime;
		PUT_FLTVAL(v, flt);
	    }
	    v++;

	    /* copy arguments */
	    switch (size) {
	    case 3:
		*v++ = co->val[3];
	    case 2:
		*v++ = co->val[2];
	    case 1:
		*v++ = co->val[1];
	    case 0:
		break;

	    default:
		n = size - 2;
		for (v2 = d_get_elts(co->val[3].u.array) + n; n > 0; --n) {
		    *v++ = *--v2;
		}
		*v++ = co->val[2];
		*v++ = co->val[1];
		break;
	    }
	    while (size > 0) {
		i_ref_value(--v);
		--size;
	    }
	    d_ref_imports(a);

	    /* put in list */
	    PUT_ARRVAL(elts, a);
	    elts++;
	    --count;
	}
    }
    co_list(list);

    return list;
}


/*
 * NAME:	data->set_varmap()
 * DESCRIPTION:	add a variable mapping to a control block
 */
void d_set_varmap(ctrl, nvar, vmap)
register control *ctrl;
unsigned int nvar;
unsigned short *vmap;
{
    ctrl->vmapsize = nvar;
    ctrl->vmap = vmap;

    /* varmap modified */
    ctrl->flags |= CTRL_VARMAP;
}

/*
 * NAME:	data->get_varmap()
 * DESCRIPTION:	get the variable mapping for an object
 */
static unsigned short *d_get_varmap(obj, update, nvariables)
object **obj;
register Uint update;
unsigned short *nvariables;
{
    register object *tmpl;
    register unsigned short nvar, *vmap;

    tmpl = OBJ((*obj)->prev);
    if (O_UPGRADING(*obj)) {
	/* in the middle of an upgrade */
	tmpl = OBJ(tmpl->prev);
    }
    vmap = o_control(tmpl)->vmap;
    nvar = tmpl->ctrl->vmapsize;

    if (tmpl->update != update) {
	register unsigned short *m1, *m2, n;

	m1 = vmap;
	vmap = ALLOC(unsigned short, n = nvar);
	do {
	    tmpl = OBJ(tmpl->prev);
	    m2 = o_control(tmpl)->vmap;
	    while (n > 0) {
		*vmap++ = (NEW_VAR(*m1)) ? *m1++ : m2[*m1++];
		--n;
	    }
	    n = nvar;
	    vmap -= n;
	    m1 = vmap;
	} while (tmpl->update != update);
    }

    *obj = tmpl;
    *nvariables = nvar;
    return vmap;
}

/*
 * NAME:	data->upgrade_data()
 * DESCRIPTION:	upgrade the dataspace for one object
 */
void d_upgrade_data(data, nvar, vmap, tmpl)
register dataspace *data;
register unsigned int nvar;
register unsigned short *vmap;
object *tmpl;
{
    register value *v;
    register unsigned short n;
    value *vars;

    /* make sure variables are in memory */
    vars = d_get_variable(data, 0);

    /* map variables */
    for (n = nvar, v = ALLOC(value, n); n > 0; --n) {
	switch (*vmap) {
	case NEW_INT:
	    *v++ = zero_int;
	    break;

	case NEW_FLOAT:
	    *v++ = zero_float;
	    break;

	case NEW_POINTER:
	    *v++ = nil_value;
	    break;

	default:
	    *v = vars[*vmap];
	    i_ref_value(v);
	    v->modified = TRUE;
	    ref_rhs(data, v++);
	    break;
	}
	vmap++;
    }
    vars = v - nvar;

    /* deref old values */
    v = data->variables;
    for (n = data->nvariables; n > 0; --n) {
	del_lhs(data, v);
	i_del_value(v++);
    }

    /* replace old with new */
    FREE(data->variables);
    data->variables = vars;

    data->base.flags |= MOD_VARIABLE;
    if (data->nvariables != nvar) {
	if (data->svariables != (svalue *) NULL) {
	    FREE(data->svariables);
	    data->svariables = (svalue *) NULL;
	}
	data->nvariables = nvar;
	data->base.achange++;	/* force rebuild on swapout */
    }

    o_upgraded(tmpl, OBJ(data->oindex));
}

/*
 * NAME:	data->upgrade_clone()
 * DESCRIPTION:	upgrade a clone object
 */
void d_upgrade_clone(data)
dataspace *data;
{
    object *obj;
    unsigned short *vmap, nvar;
    Uint update;

    /*
     * the program for the clone was upgraded since last swapin
     */
    obj = OBJ(data->oindex);
    update = obj->update;
    obj = OBJ(obj->u_master);
    vmap = d_get_varmap(&obj, update, &nvar);
    d_upgrade_data(data, nvar, vmap, obj);
    if (vmap != obj->ctrl->vmap) {
	FREE(vmap);
    }
}

/*
 * NAME:	data->upgrade_lwobj()
 * DESCRIPTION:	upgrade a non-persistent object
 */
object *d_upgrade_lwobj(lwobj, obj)
register array *lwobj;
object *obj;
{
    register arrref *a;
    register unsigned short n;
    register value *v;
    Uint update;
    unsigned short nvar, *vmap;
    value *vars;

    a = lwobj->primary;
    update = obj->update;
    vmap = d_get_varmap(&obj, (Uint) lwobj->elts[1].u.number, &nvar);
    --nvar;

    /* map variables */
    v = ALLOC(value, nvar + 2);
    *v++ = lwobj->elts[0];
    *v = lwobj->elts[1];
    (v++)->u.objcnt = update;

    vars = lwobj->elts + 2;
    for (n = nvar; n > 0; --n) {
	switch (*vmap) {
	case NEW_INT:
	    *v++ = zero_int;
	    break;

	case NEW_FLOAT:
	    *v++ = zero_float;
	    break;

	case NEW_POINTER:
	    *v++ = nil_value;
	    break;

	default:
	    *v = vars[*vmap];
	    if (a->arr != (array *) NULL) {
		ref_rhs(a->data, v);
	    }
	    i_ref_value(v);
	    (v++)->modified = TRUE;
	    break;
	}
	vmap++;
    }
    vars = v - (nvar + 2);

    vmap -= nvar;
    if (vmap != obj->ctrl->vmap) {
	FREE(vmap);  
    }

    v = lwobj->elts + 2;
    if (a->arr != (array *) NULL) {
	/* swapped-in */
	if (a->state == AR_UNCHANGED) {
	    register dataplane *p;

	    a->state = AR_CHANGED;
	    for (p = a->data->plane; p != (dataplane *) NULL; p = p->prev) {
		p->achange++;
	    }
	}

	/* deref old values */
	for (n = lwobj->size - 2; n > 0; --n) {
	    del_lhs(a->data, v);
	    i_del_value(v++);
	}
    } else {
	/* deref old values */
	for (n = lwobj->size - 2; n > 0; --n) {
	    i_del_value(v++);
	}
    }

    /* replace old with new */
    lwobj->size = nvar + 2;
    FREE(lwobj->elts);
    lwobj->elts = vars;

    return obj;
}

/*
 * NAME:	data->import()
 * DESCRIPTION:	copy imported arrays to current dataspace
 */
static void d_import(imp, data, val, n)
register arrimport *imp;
register dataspace *data;
register value *val;
register unsigned short n;
{
    while (n > 0) {
	if (T_INDEXED(val->type)) {
	    register array *a;
	    register Uint i, j;

	    a = val->u.array;
	    if (a->primary->data != data) {
		/*
		 * imported array
		 */
		i = arr_put(a, imp->narr);
		if (i == imp->narr) {
		    /*
		     * first time encountered
		     */
		    imp->narr++;

		    if (a->hashed != (struct _maphash_ *) NULL) {
			map_rmhash(a);
		    }

		    if (a->ref == 2) {	/* + 1 for array merge table */
			/*
			 * move array to new dataspace
			 */
			a->prev->next = a->next;
			a->next->prev = a->prev;
		    } else {
			/*
			 * make new array
			 */
			a = arr_alloc(a->size);
			a->tag = val->u.array->tag;
			a->odcount = val->u.array->odcount;

			if (a->size > 0) {
			    /*
			     * copy elements
			     */
			    i_copy(a->elts = ALLOC(value, a->size),
				   d_get_elts(val->u.array), a->size);
			}

			/*
			 * replace
			 */
			arr_del(val->u.array);
			arr_ref(val->u.array = a);

			/*
			 * store in itab
			 */
			if (i >= imp->itabsz) {
			    /*
			     * increase size of itab
			     */
			    for (j = imp->itabsz; j <= i; j += j) ;
			    imp->itab = REALLOC(imp->itab, array*, imp->itabsz,
						j);
			    imp->itabsz = j;
			}
			arr_put(imp->itab[i] = a, imp->narr++);
		    }

		    a->primary = &data->base.alocal;
		    a->prev = &data->alist;
		    a->next = data->alist.next;
		    a->next->prev = a;
		    data->alist.next = a;

		    if (a->size > 0) {
			/*
			 * import elements too
			 */
			d_import(imp, data, a->elts, a->size);
		    }
		} else {
		    /*
		     * array was previously replaced
		     */
		    arr_ref(a = imp->itab[i]);
		    arr_del(val->u.array);
		    val->u.array = a;
		}
	    } else if (arr_put(a, imp->narr) == imp->narr) {
		/*
		 * not previously encountered mapping or array
		 */
		imp->narr++;
		if (a->hashed != (struct _maphash_ *) NULL) {
		    map_rmhash(a);
		    d_import(imp, data, a->elts, a->size);
		} else if (a->elts != (value *) NULL) {
		    d_import(imp, data, a->elts, a->size);
		}
	    }
	}
	val++;
	--n;
    }
}

/*
 * NAME:	data->export()
 * DESCRIPTION:	handle exporting of arrays shared by more than one object
 */
void d_export()
{
    register dataspace *data;
    register Uint n;
    arrimport imp;

    if (ifirst != (dataspace *) NULL) {
	imp.itab = ALLOC(array*, imp.itabsz = 64);

	for (data = ifirst; data != (dataspace *) NULL; data = data->inext) {
	    if (data->base.imports != 0) {
		data->base.imports = 0;
		arr_merge();
		imp.narr = 0;

		if (data->variables != (value *) NULL) {
		    d_import(&imp, data, data->variables, data->nvariables);
		}
		if (data->base.arrays != (arrref *) NULL) {
		    register arrref *a;

		    for (n = data->narrays, a = data->base.arrays; n > 0;
			 --n, a++) {
			if (a->arr != (array *) NULL) {
			    if (a->arr->hashed != (struct _maphash_ *) NULL) {
				/* mapping */
				map_rmhash(a->arr);
				d_import(&imp, data, a->arr->elts,
					 a->arr->size);
			    } else if (a->arr->elts != (value *) NULL) {
				d_import(&imp, data, a->arr->elts,
					 a->arr->size);
			    }
			}
		    }
		}
		if (data->callouts != (dcallout *) NULL) {
		    register dcallout *co;

		    co = data->callouts;
		    for (n = data->ncallouts; n > 0; --n) {
			if (co->val[0].type == T_STRING) {
			    d_import(&imp, data, co->val,
				     (co->nargs > 3) ? 4 : co->nargs + 1);
			}
			co++;
		    }
		}
		arr_clear();	/* clear merge table */
	    }
	    data->iprev = (dataspace *) NULL;
	}
	ifirst = (dataspace *) NULL;

	FREE(imp.itab);
    }
}


/*
 * NAME:	data->del_control()
 * DESCRIPTION:	delete a control block from swap and memory
 */
void d_del_control(ctrl)
register control *ctrl;
{
    if (ctrl->sectors != (sector *) NULL) {
	sw_wipev(ctrl->sectors, ctrl->nsectors);
	sw_delv(ctrl->sectors, ctrl->nsectors);
    }
    d_free_control(ctrl);
}

/*
 * NAME:	data->del_dataspace()
 * DESCRIPTION:	delete a dataspace block from swap and memory
 */
void d_del_dataspace(data)
register dataspace *data;
{
    if (data->iprev != (dataspace *) NULL) {
	data->iprev->inext = data->inext;
	if (data->inext != (dataspace *) NULL) {
	    data->inext->iprev = data->iprev;
	}
    } else if (ifirst == data) {
	ifirst = data->inext;
	if (ifirst != (dataspace *) NULL) {
	    ifirst->iprev = (dataspace *) NULL;
	}
    }

    if (data->ncallouts != 0) {
	register Uint n;
	register dcallout *co;
	unsigned short dummy;

	/*
	 * remove callouts from callout table
	 */
	if (data->callouts == (dcallout *) NULL) {
	    d_get_callouts(data);
	}
	for (n = data->ncallouts, co = data->callouts + n; n > 0; --n) {
	    if ((--co)->val[0].type == T_STRING) {
		d_del_call_out(data, n, &dummy);
	    }
	}
    }
    if (data->sectors != (sector *) NULL) {
	sw_wipev(data->sectors, data->nsectors);
	sw_delv(data->sectors, data->nsectors);
    }
    d_free_dataspace(data);
}
