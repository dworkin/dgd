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

# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"
# include "parse.h"


# define COP_ADD	0	/* add callout patch */
# define COP_REMOVE	1	/* remove callout patch */
# define COP_REPLACE	2	/* replace callout patch */

struct copatch : public ChunkAllocated {
    short type;			/* add, remove, replace */
    uindex handle;		/* callout handle */
    Dataplane *plane;		/* dataplane */
    Uint time;			/* start time */
    unsigned short mtime;	/* start time millisec component */
    uindex *queue;		/* callout queue */
    copatch *next;		/* next in linked list */
    dcallout aco;		/* added callout */
    dcallout rco;		/* removed callout */
};

# define COPCHUNKSZ	32

class coptable : public Allocated {
public:
    /*
     * NAME:		coptable()
     * DESCRIPTION:	initialize copatch table
     */
    coptable() {
	memset(&cop, '\0', COPATCHHTABSZ * sizeof(copatch*));
    }

    Chunk<copatch, COPCHUNKSZ> chunk;	/* callout patch chunk */
    copatch *cop[COPATCHHTABSZ];	/* hash table of callout patches */
};

struct arrimport {
    Array **itab;			/* imported array replacement table */
    Uint itabsz;			/* size of table */
    Uint narr;				/* # of arrays */
};

static Dataplane *plist;		/* list of dataplanes */
static uindex ncallout;			/* # callouts added */
static Dataspace *ifirst;		/* list of dataspaces with imports */


/*
 * NAME:	ref_rhs()
 * DESCRIPTION:	reference the right-hand side in an assignment
 */
static void ref_rhs(Dataspace *data, Value *rhs)
{
    String *str;
    Array *arr;

    switch (rhs->type) {
    case T_STRING:
	str = rhs->string;
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
	arr = rhs->array;
	if (arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->arr != (Array *) NULL) {
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
		data->iprev == (Dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (Dataspace *) NULL;
		data->inext = ifirst;
		if (ifirst != (Dataspace *) NULL) {
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
static void del_lhs(Dataspace *data, Value *lhs)
{
    String *str;
    Array *arr;

    switch (lhs->type) {
    case T_STRING:
	str = lhs->string;
	if (str->primary != (strref *) NULL && str->primary->data == data) {
	    /* in this object */
	    if (--(str->primary->ref) == 0) {
		str->primary->str = (String *) NULL;
		str->primary = (strref *) NULL;
		str->del();
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
	arr = lhs->array;
	if (arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->arr != (Array *) NULL) {
		/* swapped in */
		data->plane->flags |= MOD_ARRAYREF;
		if ((--(arr->primary->ref) & ~ARR_MOD) == 0) {
		    d_get_elts(arr);
		    arr->primary->arr = (Array *) NULL;
		    arr->primary = &arr->primary->plane->alocal;
		    arr->del();
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
static uindex d_alloc_call_out(Dataspace *data, uindex handle, Uint time,
	unsigned short mtime, int nargs, Value *v)
{
    dcallout *co;

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
	/* fall through */
    case 2:
	ref_rhs(data, &v[2]);
	/* fall through */
    case 1:
	ref_rhs(data, &v[1]);
	/* fall through */
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
static void d_free_call_out(Dataspace *data, unsigned int handle)
{
    dcallout *co;
    Value *v;
    uindex n;

    co = &data->callouts[handle - 1];
    v = co->val;
    switch (co->nargs) {
    default:
	del_lhs(data, &v[3]);
	i_del_value(&v[3]);
	/* fall through */
    case 2:
	del_lhs(data, &v[2]);
	i_del_value(&v[2]);
	/* fall through */
    case 1:
	del_lhs(data, &v[1]);
	i_del_value(&v[1]);
	/* fall through */
    case 0:
	del_lhs(data, &v[0]);
	v[0].string->del();
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
 * NAME:	copatch->new()
 * DESCRIPTION:	create a new callout patch
 */
static copatch *cop_new(Dataplane *plane, copatch **c, int type,
	unsigned int handle, dcallout *co, Uint time, unsigned int mtime,
	uindex *q)
{
    copatch *cop;
    int i;
    Value *v;

    /* allocate */
    cop = chunknew (plane->coptab->chunk) copatch;

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
static void cop_del(copatch **c, bool del)
{
    copatch *cop;
    dcallout *co;
    int i;
    Value *v;

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
    delete cop;
}

/*
 * NAME:	copatch->replace()
 * DESCRIPTION:	replace one callout patch with another
 */
static void cop_replace(copatch *cop, dcallout *co, Uint time,
	unsigned int mtime, uindex *q)
{
    int i;
    Value *v;

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
static void cop_commit(copatch *cop)
{
    int i;
    Value *v;

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
static void cop_release(copatch *cop)
{
    int i;
    Value *v;

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
static void cop_discard(copatch *cop)
{
    /* force unref of proper component later */
    cop->type = COP_ADD;
}


/*
 * NAME:	data->new_plane()
 * DESCRIPTION:	create a new dataplane
 */
void d_new_plane(Dataspace *data, Int level)
{
    Dataplane *p;
    Uint i;

    p = ALLOC(Dataplane, 1);

    p->level = level;
    p->flags = data->plane->flags;
    p->schange = data->plane->schange;
    p->achange = data->plane->achange;
    p->imports = data->plane->imports;

    /* copy value information from previous plane */
    p->original = (Value *) NULL;
    p->alocal.arr = (Array *) NULL;
    p->alocal.plane = p;
    p->alocal.data = data;
    p->alocal.state = AR_CHANGED;
    p->coptab = data->plane->coptab;

    if (data->plane->arrays != (arrref *) NULL) {
	arrref *a, *b;

	p->arrays = ALLOC(arrref, i = data->narrays);
	for (a = p->arrays, b = data->plane->arrays; i != 0; a++, b++, --i) {
	    if (b->arr != (Array *) NULL) {
		*a = *b;
		a->arr->primary = a;
		a->arr->ref();
	    } else {
		a->arr = (Array *) NULL;
	    }
	}
    } else {
	p->arrays = (arrref *) NULL;
    }
    p->achunk = (Array::Backup *) NULL;

    if (data->plane->strings != (strref *) NULL) {
	strref *s, *t;

	p->strings = ALLOC(strref, i = data->nstrings);
	for (s = p->strings, t = data->plane->strings; i != 0; s++, t++, --i) {
	    if (t->str != (String *) NULL) {
		*s = *t;
		s->str->primary = s;
		s->str->ref();
	    } else {
		s->str = (String *) NULL;
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
static void commit_values(Value *v, unsigned int n, Int level)
{
    Array *list, *arr;

    list = (Array *) NULL;
    for (;;) {
	while (n != 0) {
	    if (T_INDEXED(v->type)) {
		arr = v->array;
		if (arr->primary->arr == (Array *) NULL &&
		    arr->primary->plane->level > level) {
		    if (arr->hashmod) {
			arr->mapCompact(arr->primary->data);
		    }
		    arr->primary = &arr->primary->plane->prev->alocal;
		    if (arr->size != 0) {
			arr->prev->next = arr->next;
			arr->next->prev = arr->prev;
			arr->next = list;
			list = arr;
		    }
		}

	    }
	    v++;
	    --n;
	}

	if (list == (Array *) NULL) {
	    break;
	}

	arr = list;
	list = arr->next;
	arr->prev = &arr->primary->data->alist;
	arr->next = arr->prev->next;
	arr->next->prev = arr;
	arr->prev->next = arr;
	v = arr->elts;
	n = arr->size;
    }
}

/*
 * NAME:	commit_callouts()
 * DESCRIPTION:	commit callout patches to previous plane
 */
static void commit_callouts(Dataplane *plane, bool merge)
{
    Dataplane *prev;
    copatch **c, **n, *cop;
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
		    cop_del(c, TRUE);
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
				    cop_del(c, TRUE);
				    break;

				case COP_REMOVE:
				    if ((*n)->type == COP_REPLACE) {
					/* turn replace back into remove */
					cop_release(*n);
				    } else {
					/* del old */
					cop_del(n, TRUE);
				    }
				    /* del new */
				    if (next == &cop->next) {
					next = c;
				    }
				    cop_del(c, TRUE);
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
					cop_del(c, TRUE);
				    } else {
					/* make replace into add, remove old */
					cop_del(n, TRUE);
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
void d_commit_plane(Int level, Value *retval)
{
    Dataplane *p, *commit, **r, **cr;
    Dataspace *data;
    Value *v;
    Uint i;
    Dataplane *clist;

    /*
     * pass 1: construct commit planes
     */
    clist = (Dataplane *) NULL;
    cr = &clist;
    for (r = &plist, p = *r; p != (Dataplane *) NULL && p->level == level;
	 r = &p->plist, p = *r) {
	if (p->prev->level != level - 1) {
	    /* insert commit plane */
	    commit = ALLOC(Dataplane, 1);
	    commit->level = level - 1;
	    commit->original = (Value *) NULL;
	    commit->alocal.arr = (Array *) NULL;
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
    if (clist != (Dataplane *) NULL) {
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
	if (p->original != (Value *) NULL) {
	    if (p->level == 1 || p->prev->original != (Value *) NULL) {
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
	    commit_callouts(p, (p->flags & PLANE_MERGE) != 0);
	    if (p->level == 1) {
		delete p->coptab;
		p->coptab = (coptable *) NULL;
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	Array::commit(&p->achunk, p->prev, (p->flags & PLANE_MERGE) != 0);
	if (p->flags & PLANE_MERGE) {
	    if (p->arrays != (arrref *) NULL) {
		arrref *a;

		/* remove old array refs */
		for (a = p->prev->arrays, i = data->narrays; i != 0; a++, --i) {
		    if (a->arr != (Array *) NULL) {
			if (a->arr->primary == &p->alocal) {
			    a->arr->primary = &p->prev->alocal;
			}
			a->arr->del();
		    }
		}
		FREE(p->prev->arrays);
		p->prev->arrays = p->arrays;
	    }

	    if (p->strings != (strref *) NULL) {
		strref *s;

		/* remove old string refs */
		for (s = p->prev->strings, i = data->nstrings; i != 0; s++, --i)
		{
		    if (s->str != (String *) NULL) {
			s->str->del();
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
	p->prev->flags = (p->flags & MOD_ALL) | MOD_SAVE;
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
static void discard_callouts(Dataplane *plane)
{
    copatch *cop, **c, **t;
    Dataspace *data;
    int i;

    data = plane->alocal.data;
    for (i = COPATCHHTABSZ, t = plane->coptab->cop; --i >= 0; t++) {
	c = t;
	while (*c != (copatch *) NULL && (*c)->plane == plane) {
	    cop = *c;
	    switch (cop->type) {
	    case COP_ADD:
		d_free_call_out(data, cop->handle);
		cop_del(c, TRUE);
		--ncallout;
		break;

	    case COP_REMOVE:
		d_alloc_call_out(data, cop->handle, cop->rco.time,
				 cop->rco.mtime, cop->rco.nargs, cop->rco.val);
		cop_del(c, FALSE);
		ncallout++;
		break;

	    case COP_REPLACE:
		d_free_call_out(data, cop->handle);
		d_alloc_call_out(data, cop->handle, cop->rco.time,
				 cop->rco.mtime, cop->rco.nargs, cop->rco.val);
		cop_discard(cop);
		cop_del(c, TRUE);
		break;
	    }
	}
    }
}

/*
 * NAME:	data->discard_plane()
 * DESCRIPTION:	discard the current data plane without committing it
 */
void d_discard_plane(Int level)
{
    Dataplane *p;
    Dataspace *data;
    Value *v;
    Uint i;

    for (p = plist; p != (Dataplane *) NULL && p->level == level; p = plist) {
	/*
	 * discard changes except for callout mods
	 */
	p->prev->flags |= p->flags & (MOD_CALLOUT | MOD_NEWCALLOUT);

	data = p->alocal.data;
	if (p->original != (Value *) NULL) {
	    /* restore original variable values */
	    for (v = data->variables, i = data->nvariables; i != 0; --i, v++) {
		i_del_value(v);
	    }
	    memcpy(data->variables, p->original,
		   data->nvariables * sizeof(Value));
	    FREE(p->original);
	}

	if (p->coptab != (coptable *) NULL) {
	    /* undo callout changes */
	    discard_callouts(p);
	    if (p->prev == &data->base) {
		delete p->coptab;
		p->coptab = (coptable *) NULL;
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	Array::discard(&p->achunk);
	if (p->arrays != (arrref *) NULL) {
	    arrref *a;

	    /* delete new array refs */
	    for (a = p->arrays, i = data->narrays; i != 0; a++, --i) {
		if (a->arr != (Array *) NULL) {
		    a->arr->del();
		}
	    }
	    FREE(p->arrays);
	    /* fix old ones */
	    for (a = p->prev->arrays, i = data->narrays; i != 0; a++, --i) {
		if (a->arr != (Array *) NULL) {
		    a->arr->primary = a;
		}
	    }
	}

	if (p->strings != (strref *) NULL) {
	    strref *s;

	    /* delete new string refs */
	    for (s = p->strings, i = data->nstrings; i != 0; s++, --i) {
		if (s->str != (String *) NULL) {
		    s->str->del();
		}
	    }
	    FREE(p->strings);
	    /* fix old ones */
	    for (s = p->prev->strings, i = data->nstrings; i != 0; s++, --i) {
		if (s->str != (String *) NULL) {
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
Array::Backup **d_commit_arr(Array *arr, Dataplane *prev, Dataplane *old)
{
    if (arr->primary->plane != prev) {
	if (arr->hashmod) {
	    arr->mapCompact(arr->primary->data);
	}

	if (arr->primary->arr == (Array *) NULL) {
	    arr->primary = &prev->alocal;
	} else {
	    arr->primary->plane = prev;
	}
	commit_values(arr->elts, arr->size, prev->level);
    }

    return (prev == old) ? (Array::Backup **) NULL : &prev->achunk;
}

/*
 * NAME:	data->discard_arr()
 * DESCRIPTION:	restore array to previous plane
 */
void d_discard_arr(Array *arr, Dataplane *plane)
{
    /* swapped-in arrays will be fixed later */
    arr->primary = &plane->alocal;
}


/*
 * NAME:	data->ref_imports()
 * DESCRIPTION:	check the elements of an array for imports
 */
void d_ref_imports(Array *arr)
{
    Dataspace *data;
    unsigned short n;
    Value *v;

    data = arr->primary->data;
    for (n = arr->size, v = arr->elts; n > 0; --n, v++) {
	if (T_INDEXED(v->type) && data != v->array->primary->data) {
	    /* mark as imported */
	    if (data->plane->imports++ == 0 && ifirst != data &&
		data->iprev == (Dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (Dataspace *) NULL;
		data->inext = ifirst;
		if (ifirst != (Dataspace *) NULL) {
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
void d_assign_var(Dataspace *data, Value *var, Value *val)
{
    if (var >= data->variables && var < data->variables + data->nvariables) {
	if (data->plane->level != 0 &&
	    data->plane->original == (Value *) NULL) {
	    /*
	     * back up variables
	     */
	    i_copy(data->plane->original = ALLOC(Value, data->nvariables),
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
Value *d_get_extravar(Dataspace *data)
{
    return d_get_variable(data, data->nvariables - 1);
}

/*
 * NAME:	data->set_extravar()
 * DESCRIPTION:	set an object's special value
 */
void d_set_extravar(Dataspace *data, Value *val)
{
    d_assign_var(data, d_get_variable(data, data->nvariables - 1), val);
}

/*
 * NAME:	data->wipe_extravar()
 * DESCRIPTION:	wipe out an object's special value
 */
void d_wipe_extravar(Dataspace *data)
{
    d_assign_var(data, d_get_variable(data, data->nvariables - 1), &nil_value);

    if (data->parser != (struct parser *) NULL) {
	/*
	 * get rid of the parser, too
	 */
	ps_del(data->parser);
	data->parser = (struct parser *) NULL;
    }
}

/*
 * NAME:	data->assign_elt()
 * DESCRIPTION:	assign a value to an array element
 */
void d_assign_elt(Dataspace *data, Array *arr, Value *elt, Value *val)
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
	arr->backup(&data->plane->achunk);
	if (arr->primary->arr != (Array *) NULL) {
	    arr->primary->plane = data->plane;
	} else {
	    arr->primary = &data->plane->alocal;
	}
    }

    if (arr->primary->arr != (Array *) NULL) {
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
	if (T_INDEXED(val->type) && data != val->array->primary->data) {
	    /* mark as imported */
	    if (data->plane->imports++ == 0 && ifirst != data &&
		data->iprev == (Dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (Dataspace *) NULL;
		data->inext = ifirst;
		if (ifirst != (Dataspace *) NULL) {
		    ifirst->iprev = data;
		}
		ifirst = data;
	    }
	}
	if (T_INDEXED(elt->type) && data != elt->array->primary->data) {
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
void d_change_map(Array *map)
{
    arrref *a;

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
uindex d_new_call_out(Dataspace *data, String *func, Int delay,
	unsigned int mdelay, Frame *f, int nargs)
{
    Uint ct, t;
    unsigned short m;
    uindex *q;
    Value v[4];
    uindex handle;

    ct = co_check(ncallout, delay, mdelay, &t, &m, &q);
    if (ct == 0 && q == (uindex *) NULL) {
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
	PUT_ARRVAL(&v[3], Array::create(data, nargs - 2));
	memcpy(v[3].array->elts, f->sp + 2, (nargs - 2) * sizeof(Value));
	d_ref_imports(v[3].array);
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
	Dataplane *plane;
	copatch **c, *cop;
	dcallout *co;
	copatch **cc;

	/*
	 * add callout patch
	 */
	plane = data->plane;
	if (plane->coptab == (coptable *) NULL) {
	    plane->coptab = new coptable;
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
Int d_del_call_out(Dataspace *data, Uint handle, unsigned short *mtime)
{
    dcallout *co;
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
	Dataplane *plane;
	copatch **c, *cop;
	copatch **cc;

	/*
	 * add/remove callout patch
	 */
	--ncallout;

	plane = data->plane;
	if (plane->coptab == (coptable *) NULL) {
	    plane->coptab = new coptable;
	}
	cc = c = &plane->coptab->cop[handle % COPATCHHTABSZ];
	for (;;) {
	    cop = *c;
	    if (cop == (copatch *) NULL || cop->plane != plane) {
		/* delete new */
		cop_new(plane, cc, COP_REMOVE, (uindex) handle, co, (Uint) 0, 0,
			(uindex *) NULL);
		break;
	    }
	    if (cop->handle == handle) {
		/* delete existing */
		if (cop->type == COP_REPLACE) {
		    cop_release(cop);
		} else {
		    cop_del(c, TRUE);
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
String *d_get_call_out(Dataspace *data, unsigned int handle, Frame *f,
	int *nargs)
{
    String *str;
    dcallout *co;
    Value *v, *o;
    uindex n;

    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    co = &data->callouts[handle - 1];
    v = co->val;
    del_lhs(data, &v[0]);
    str = v[0].string;

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
	memcpy(f->sp, d_get_elts(v[3].array), n * sizeof(Value));
	del_lhs(data, &v[3]);
	FREE(v[3].array->elts);
	v[3].array->elts = (Value *) NULL;
	v[3].array->del();
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
	    o = d_get_elts(v->array);
	    if (o->type == T_OBJECT && DESTRUCTED(o)) {
		v->array->del();
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
Array *d_list_callouts(Dataspace *host, Dataspace *data)
{
    uindex n, count, size;
    dcallout *co;
    Value *v, *v2, *elts;
    Array *list, *a;
    uindex max_args;
    Float flt;

    if (data->ncallouts == 0) {
	return Array::create(host, 0);
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
	return (Array *) NULL;
    }

    list = Array::create(host, count);
    elts = list->elts;
    max_args = conf_array_size() - 3;

    for (co = data->callouts; count > 0; co++) {
	if (co->val[0].type == T_STRING) {
	    size = co->nargs;
	    if (size > max_args) {
		/* unlikely, but possible */
		size = max_args;
	    }
	    a = Array::create(host, size + 3L);
	    v = a->elts;

	    /* handle */
	    PUT_INTVAL(v, co - data->callouts + 1);
	    v++;
	    /* function */
	    PUT_STRVAL(v, co->val[0].string);
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
		for (v2 = d_get_elts(co->val[3].array) + n; n > 0; --n) {
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
void d_set_varmap(Control *ctrl, unsigned short *vmap)
{
    ctrl->vmapsize = ctrl->nvariables + 1;
    ctrl->vmap = vmap;
}

/*
 * NAME:	data->get_varmap()
 * DESCRIPTION:	get the variable mapping for an object
 */
static unsigned short *d_get_varmap(Object **obj, Uint update, unsigned short *nvariables)
{
    Object *tmpl;
    unsigned short nvar, *vmap;

    tmpl = OBJ((*obj)->prev);
    if (O_UPGRADING(*obj)) {
	/* in the middle of an upgrade */
	tmpl = OBJ(tmpl->prev);
    }
    vmap = tmpl->control()->vmap;
    nvar = tmpl->ctrl->vmapsize;

    if (tmpl->update != update) {
	unsigned short *m1, *m2, n;

	m1 = vmap;
	vmap = ALLOC(unsigned short, n = nvar);
	do {
	    tmpl = OBJ(tmpl->prev);
	    m2 = tmpl->control()->vmap;
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
void d_upgrade_data(Dataspace *data, unsigned int nvar, unsigned short *vmap,
	Object *tmpl)
{
    Value *v;
    unsigned short n;
    Value *vars;

    /* make sure variables are in memory */
    vars = d_get_variable(data, 0);

    /* map variables */
    for (n = nvar, v = ALLOC(Value, n); n > 0; --n) {
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
	if (data->svariables != (struct svalue *) NULL) {
	    FREE(data->svariables);
	    data->svariables = (struct svalue *) NULL;
	}
	data->nvariables = nvar;
	data->base.achange++;	/* force rebuild on swapout */
    }

    OBJ(data->oindex)->upgraded(tmpl);
}

/*
 * NAME:	data->upgrade_clone()
 * DESCRIPTION:	upgrade a clone object
 */
void d_upgrade_clone(Dataspace *data)
{
    Object *obj;
    unsigned short *vmap, nvar;
    Uint update;

    /*
     * the program for the clone was upgraded since last swapin
     */
    obj = OBJ(data->oindex);
    update = obj->update;
    obj = OBJ(obj->master);
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
Object *d_upgrade_lwobj(Array *lwobj, Object *obj)
{
    arrref *a;
    unsigned short n;
    Value *v;
    Uint update;
    unsigned short nvar, *vmap;
    Value *vars;

    a = lwobj->primary;
    update = obj->update;
    vmap = d_get_varmap(&obj, (Uint) lwobj->elts[1].number, &nvar);
    --nvar;

    /* map variables */
    v = ALLOC(Value, nvar + 2);
    *v++ = lwobj->elts[0];
    *v = lwobj->elts[1];
    (v++)->objcnt = update;

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
	    if (a->arr != (Array *) NULL) {
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
    if (a->arr != (Array *) NULL) {
	/* swapped-in */
	if (a->state == AR_UNCHANGED) {
	    Dataplane *p;

	    a->state = AR_CHANGED;
	    for (p = a->data->plane; p != (Dataplane *) NULL; p = p->prev) {
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
static void d_import(arrimport *imp, Dataspace *data, Value *val,
	unsigned short n)
{
    Array *import, *a;

    import = (Array *) NULL;
    for (;;) {
	while (n > 0) {
	    if (T_INDEXED(val->type)) {
		Uint i, j;

		a = val->array;
		if (a->primary->data != data) {
		    /*
		     * imported array
		     */
		    i = a->put(imp->narr);
		    if (i == imp->narr) {
			/*
			 * first time encountered
			 */
			imp->narr++;

			if (a->hashed != (MapHash *) NULL) {
			    a->mapRemoveHash();
			}

			if (a->refCount == 2) {	/* + 1 for array merge table */
			    /*
			     * move array to new dataspace
			     */
			    a->prev->next = a->next;
			    a->next->prev = a->prev;
			} else {
			    /*
			     * make new array
			     */
			    a = Array::alloc(a->size);
			    a->tag = val->array->tag;
			    a->objDestrCount = val->array->objDestrCount;

			    if (a->size > 0) {
				/*
				 * copy elements
				 */
				i_copy(a->elts = ALLOC(Value, a->size),
				       d_get_elts(val->array), a->size);
			    }

			    /*
			     * replace
			     */
			    val->array->del();
			    val->array = a;
			    val->array->ref();

			    /*
			     * store in itab
			     */
			    if (i >= imp->itabsz) {
				/*
				 * increase size of itab
				 */
				for (j = imp->itabsz; j <= i; j += j) ;
				imp->itab = REALLOC(imp->itab, Array*,
						    imp->itabsz, j);
				imp->itabsz = j;
			    }
			    imp->itab[i] = a;
			    a->put(imp->narr++);
			}

			a->primary = &data->base.alocal;
			if (a->size == 0) {
			    /*
			     * put empty array in dataspace
			     */
			    a->prev = &data->alist;
			    a->next = data->alist.next;
			    a->next->prev = a;
			    data->alist.next = a;
			} else {
			    /*
			     * import elements too
			     */
			    a->next = import;
			    import = a;
			}
		    } else {
			/*
			 * array was previously replaced
			 */
			a = imp->itab[i];
			a->ref();
			val->array->del();
			val->array = a;
		    }
		} else if (a->put(imp->narr) == imp->narr) {
		    /*
		     * not previously encountered mapping or array
		     */
		    imp->narr++;
		    if (a->hashed != (MapHash *) NULL) {
			a->mapRemoveHash();
			a->prev->next = a->next;
			a->next->prev = a->prev;
			a->next = import;
			import = a;
		    } else if (a->elts != (Value *) NULL) {
			a->prev->next = a->next;
			a->next->prev = a->prev;
			a->next = import;
			import = a;
		    }
		}
	    }
	    val++;
	    --n;
	}

	/*
	 * retrieve next array from the import list
	 */
	if (import == (Array *) NULL) {
	    break;
	}
	a = import;
	import = import->next;
	a->prev = &data->alist;
	a->next = data->alist.next;
	a->next->prev = a;
	data->alist.next = a;

	/* import array elements */
	val = a->elts;
	n = a->size;
    }
}

/*
 * NAME:	data->export()
 * DESCRIPTION:	handle exporting of arrays shared by more than one object
 */
void d_export()
{
    Dataspace *data;
    Uint n;
    arrimport imp;

    if (ifirst != (Dataspace *) NULL) {
	imp.itab = ALLOC(Array*, imp.itabsz = 64);

	for (data = ifirst; data != (Dataspace *) NULL; data = data->inext) {
	    if (data->base.imports != 0) {
		data->base.imports = 0;
		Array::merge();
		imp.narr = 0;

		if (data->variables != (Value *) NULL) {
		    d_import(&imp, data, data->variables, data->nvariables);
		}
		if (data->base.arrays != (arrref *) NULL) {
		    arrref *a;

		    for (n = data->narrays, a = data->base.arrays; n > 0;
			 --n, a++) {
			if (a->arr != (Array *) NULL) {
			    if (a->arr->hashed != (MapHash *) NULL) {
				/* mapping */
				a->arr->mapRemoveHash();
				d_import(&imp, data, a->arr->elts,
					 a->arr->size);
			    } else if (a->arr->elts != (Value *) NULL) {
				d_import(&imp, data, a->arr->elts,
					 a->arr->size);
			    }
			}
		    }
		}
		if (data->callouts != (dcallout *) NULL) {
		    dcallout *co;

		    co = data->callouts;
		    for (n = data->ncallouts; n > 0; --n) {
			if (co->val[0].type == T_STRING) {
			    d_import(&imp, data, co->val,
				     (co->nargs > 3) ? 4 : co->nargs + 1);
			}
			co++;
		    }
		}
		Array::clear();	/* clear merge table */
	    }
	    data->iprev = (Dataspace *) NULL;
	}
	ifirst = (Dataspace *) NULL;

	FREE(imp.itab);
    }
}


/*
 * NAME:	data->del_control()
 * DESCRIPTION:	delete a control block from swap and memory
 */
void d_del_control(Control *ctrl)
{
    if (ctrl->sectors != (Sector *) NULL) {
	Swap::wipev(ctrl->sectors, ctrl->nsectors);
	Swap::delv(ctrl->sectors, ctrl->nsectors);
    }
    d_free_control(ctrl);
}

/*
 * NAME:	data->del_dataspace()
 * DESCRIPTION:	delete a dataspace block from swap and memory
 */
void d_del_dataspace(Dataspace *data)
{
    if (data->iprev != (Dataspace *) NULL) {
	data->iprev->inext = data->inext;
	if (data->inext != (Dataspace *) NULL) {
	    data->inext->iprev = data->iprev;
	}
    } else if (ifirst == data) {
	ifirst = data->inext;
	if (ifirst != (Dataspace *) NULL) {
	    ifirst->iprev = (Dataspace *) NULL;
	}
    }

    if (data->ncallouts != 0) {
	Uint n;
	dcallout *co;
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
    if (data->sectors != (Sector *) NULL) {
	Swap::wipev(data->sectors, data->nsectors);
	Swap::delv(data->sectors, data->nsectors);
    }
    d_free_dataspace(data);
}
