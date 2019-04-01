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
# include "control.h"
# include "data.h"
# include "interpret.h"
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


static Dataspace *dhead, *dtail;	/* list of dataspace blocks */
static Dataspace *gcdata;		/* next dataspace to garbage collect */
static Sector ndata;			/* # dataspace blocks */

/*
 * NAME:	data->alloc_dataspace()
 * DESCRIPTION:	allocate a new dataspace block
 */
static Dataspace *d_alloc_dataspace(Object *obj)
{
    Dataspace *data;

    data = ALLOC(Dataspace, 1);
    if (dhead != (Dataspace *) NULL) {
	/* insert at beginning of list */
	dhead->prev = data;
	data->prev = (Dataspace *) NULL;
	data->next = dhead;
	dhead = data;
	data->gcprev = gcdata->gcprev;
	data->gcnext = gcdata;
	data->gcprev->gcnext = data;
	gcdata->gcprev = data;
    } else {
	/* list was empty */
	data->prev = data->next = (Dataspace *) NULL;
	dhead = dtail = data;
	gcdata = data;
	data->gcprev = data->gcnext = data;
    }
    ndata++;

    data->iprev = (Dataspace *) NULL;
    data->inext = (Dataspace *) NULL;
    data->flags = 0;

    data->oindex = obj->index;
    data->ctrl = (Control *) NULL;

    /* sectors */
    data->nsectors = 0;
    data->sectors = (Sector *) NULL;

    /* variables */
    data->nvariables = 0;
    data->variables = (Value *) NULL;
    data->svariables = (svalue *) NULL;

    /* arrays */
    data->narrays = 0;
    data->eltsize = 0;
    data->sarrays = (sarray *) NULL;
    data->saindex = (Uint *) NULL;
    data->selts = (svalue *) NULL;
    data->alist.prev = data->alist.next = &data->alist;

    /* strings */
    data->nstrings = 0;
    data->strsize = 0;
    data->sstrings = (sstring *) NULL;
    data->ssindex = (Uint *) NULL;
    data->stext = (char *) NULL;

    /* callouts */
    data->ncallouts = 0;
    data->fcallouts = 0;
    data->callouts = (dcallout *) NULL;
    data->scallouts = (scallout *) NULL;

    /* value plane */
    data->base.level = 0;
    data->base.flags = 0;
    data->base.schange = 0;
    data->base.achange = 0;
    data->base.imports = 0;
    data->base.alocal.arr = (Array *) NULL;
    data->base.alocal.plane = &data->base;
    data->base.alocal.data = data;
    data->base.alocal.state = AR_CHANGED;
    data->base.arrays = (arrref *) NULL;
    data->base.strings = (strref *) NULL;
    data->base.coptab = (class coptable *) NULL;
    data->base.prev = (Dataplane *) NULL;
    data->base.plist = (Dataplane *) NULL;
    data->plane = &data->base;

    /* parse_string data */
    data->parser = (struct parser *) NULL;

    return data;
}

/*
 * NAME:	data->new_dataspace()
 * DESCRIPTION:	create a new dataspace block
 */
Dataspace *d_new_dataspace(Object *obj)
{
    Dataspace *data;

    data = d_alloc_dataspace(obj);
    data->base.flags = MOD_VARIABLE;
    data->ctrl = obj->control();
    data->ctrl->ndata++;
    data->nvariables = data->ctrl->nvariables + 1;

    return data;
}

/*
 * NAME:	data->ref_dataspace()
 * DESCRIPTION:	reference data block
 */
void d_ref_dataspace(Dataspace *data)
{
    if (data != dhead) {
	/* move to head of list */
	data->prev->next = data->next;
	if (data->next != (Dataspace *) NULL) {
	    data->next->prev = data->prev;
	} else {
	    dtail = data->prev;
	}
	data->prev = (Dataspace *) NULL;
	data->next = dhead;
	dhead->prev = data;
	dhead = data;
    }
}

/*
 * NAME:	data->free_values()
 * DESCRIPTION:	free values in a dataspace block
 */
static void d_free_values(Dataspace *data)
{
    Uint i;

    /* free parse_string data */
    if (data->parser != (struct parser *) NULL) {
	ps_del(data->parser);
	data->parser = (struct parser *) NULL;
    }

    /* free variables */
    if (data->variables != (Value *) NULL) {
	Value *v;

	for (i = data->nvariables, v = data->variables; i > 0; --i, v++) {
	    i_del_value(v);
	}

	FREE(data->variables);
	data->variables = (Value *) NULL;
    }

    /* free callouts */
    if (data->callouts != (dcallout *) NULL) {
	dcallout *co;
	Value *v;
	int j;

	for (i = data->ncallouts, co = data->callouts; i > 0; --i, co++) {
	    v = co->val;
	    if (v->type == T_STRING) {
		j = 1 + co->nargs;
		if (j > 4) {
		    j = 4;
		}
		do {
		    i_del_value(v++);
		} while (--j > 0);
	    }
	}

	FREE(data->callouts);
	data->callouts = (dcallout *) NULL;
    }

    /* free arrays */
    if (data->base.arrays != (arrref *) NULL) {
	arrref *a;

	for (i = data->narrays, a = data->base.arrays; i > 0; --i, a++) {
	    if (a->arr != (Array *) NULL) {
		a->arr->del();
	    }
	}

	FREE(data->base.arrays);
	data->base.arrays = (arrref *) NULL;
    }

    /* free strings */
    if (data->base.strings != (strref *) NULL) {
	strref *s;

	for (i = data->nstrings, s = data->base.strings; i > 0; --i, s++) {
	    if (s->str != (String *) NULL) {
		s->str->primary = (strref *) NULL;
		s->str->del();
	    }
	}

	FREE(data->base.strings);
	data->base.strings = (strref *) NULL;
    }

    /* free any left-over arrays */
    if (data->alist.next != &data->alist) {
	data->alist.prev->next = data->alist.next;
	data->alist.next->prev = data->alist.prev;
	data->alist.next->freelist();
	data->alist.prev = data->alist.next = &data->alist;
    }
}

/*
 * NAME:	data->free_dataspace()
 * DESCRIPTION:	remove the dataspace block from memory
 */
void d_free_dataspace(Dataspace *data)
{
    /* free values */
    d_free_values(data);

    /* delete sectors */
    if (data->sectors != (Sector *) NULL) {
	FREE(data->sectors);
    }

    /* free scallouts */
    if (data->scallouts != (scallout *) NULL) {
	FREE(data->scallouts);
    }

    /* free sarrays */
    if (data->sarrays != (sarray *) NULL) {
	if (data->selts != (svalue *) NULL) {
	    FREE(data->selts);
	}
	if (data->saindex != (Uint *) NULL) {
	    FREE(data->saindex);
	}
	FREE(data->sarrays);
    }

    /* free sstrings */
    if (data->sstrings != (sstring *) NULL) {
	if (data->stext != (char *) NULL) {
	    FREE(data->stext);
	}
	if (data->ssindex != (Uint *) NULL) {
	    FREE(data->ssindex);
	}
	FREE(data->sstrings);
    }

    /* free svariables */
    if (data->svariables != (svalue *) NULL) {
	FREE(data->svariables);
    }

    if (data->ctrl != (Control *) NULL) {
	data->ctrl->ndata--;
    }

    if (data != dhead) {
	data->prev->next = data->next;
    } else {
	dhead = data->next;
	if (dhead != (Dataspace *) NULL) {
	    dhead->prev = (Dataspace *) NULL;
	}
    }
    if (data != dtail) {
	data->next->prev = data->prev;
    } else {
	dtail = data->prev;
	if (dtail != (Dataspace *) NULL) {
	    dtail->next = (Dataspace *) NULL;
	}
    }
    data->gcprev->gcnext = data->gcnext;
    data->gcnext->gcprev = data->gcprev;
    if (data == gcdata) {
	gcdata = (data != data->gcnext) ? data->gcnext : (Dataspace *) NULL;
    }
    --ndata;

    FREE(data);
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


struct sdataspace {
    Sector nsectors;		/* number of sectors in data space */
    short flags;		/* dataspace flags: compression */
    unsigned short nvariables;	/* number of variables */
    Uint narrays;		/* number of array values */
    Uint eltsize;		/* total size of array elements */
    Uint nstrings;		/* number of strings */
    Uint strsize;		/* total size of strings */
    uindex ncallouts;		/* number of callouts */
    uindex fcallouts;		/* first free callout */
};

static char sd_layout[] = "dssiiiiuu";

struct svalue {
    char type;			/* object, number, string, array */
    char pad;			/* 0 */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Uint string;		/* string */
	Uint objcnt;		/* object creation count */
	Uint array;		/* array */
    };
};

static char sv_layout[] = "ccui";

struct sarray {
    Uint tag;			/* unique value for each array */
    Uint ref;			/* refcount */
    char type;			/* array type */
    unsigned short size;	/* size of array */
};

static char sa_layout[] = "iics";

struct sarray0 {
    Uint index;			/* index in array value table */
    char type;			/* array type */
    unsigned short size;	/* size of array */
    Uint ref;			/* refcount */
    Uint tag;			/* unique value for each array */
};

static char sa0_layout[] = "icsii";

struct sstring {
    Uint ref;			/* refcount */
    ssizet len;			/* length of string */
};

static char ss_layout[] = "it";

struct sstring0 {
    Uint index;			/* index in string text table */
    ssizet len;			/* length of string */
    Uint ref;			/* refcount */
};

static char ss0_layout[] = "iti";

struct scallout {
    Uint time;			/* time of call */
    unsigned short htime;	/* time of call, high word */
    unsigned short mtime;	/* time of call milliseconds */
    uindex nargs;		/* number of arguments */
    svalue val[4];		/* function name, 3 direct arguments */
};

static char sco_layout[] = "issu[ccui][ccui][ccui][ccui]";

class savedata {
public:
    savedata() : alist(0) { }

    Uint narr;				/* # of arrays */
    Uint nstr;				/* # of strings */
    Uint arrsize;			/* # of array elements */
    Uint strsize;			/* total string size */
    sarray *sarrays;			/* save arrays */
    svalue *selts;			/* save array elements */
    sstring *sstrings;			/* save strings */
    char *stext;			/* save string elements */
    Array alist;			/* linked list sentinel */
};

static bool conv_14;			/* convert arrays & strings? */
static bool converted;			/* conversion complete? */

/*
 * NAME:	data->init()
 * DESCRIPTION:	initialize swapped data handling
 */
void d_init()
{
    dhead = dtail = (Dataspace *) NULL;
    gcdata = (Dataspace *) NULL;
    ndata = 0;
    conv_14 = FALSE;
    converted = FALSE;
}

/*
 * NAME:	data->init_conv()
 * DESCRIPTION:	prepare for conversions
 */
void d_init_conv(bool c14)
{
    conv_14 = c14;
}

/*
 * NAME:	load_dataspace()
 * DESCRIPTION:	load the dataspace header block
 */
static Dataspace *load_dataspace(Object *obj, void (*readv) (char*, Sector*, Uint, Uint))
{
    sdataspace header;
    Dataspace *data;
    Uint size;

    data = d_alloc_dataspace(obj);
    data->ctrl = obj->control();
    data->ctrl->ndata++;

    /* header */
    (*readv)((char *) &header, &obj->dfirst, (Uint) sizeof(sdataspace),
	     (Uint) 0);
    data->nsectors = header.nsectors;
    data->sectors = ALLOC(Sector, header.nsectors);
    data->sectors[0] = obj->dfirst;
    size = header.nsectors * (Uint) sizeof(Sector);
    if (header.nsectors > 1) {
	(*readv)((char *) data->sectors, data->sectors, size,
		 (Uint) sizeof(sdataspace));
    }
    size += sizeof(sdataspace);

    data->flags = header.flags;

    /* variables */
    data->varoffset = size;
    data->nvariables = header.nvariables;
    size += data->nvariables * (Uint) sizeof(svalue);

    /* arrays */
    data->arroffset = size;
    data->narrays = header.narrays;
    data->eltsize = header.eltsize;
    size += header.narrays * (Uint) sizeof(sarray) +
	    header.eltsize * sizeof(svalue);

    /* strings */
    data->stroffset = size;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    size += header.nstrings * sizeof(sstring) + header.strsize;

    /* callouts */
    data->cooffset = size;
    data->ncallouts = header.ncallouts;
    data->fcallouts = header.fcallouts;

    return data;
}

/*
 * NAME:	data->load_dataspace()
 * DESCRIPTION:	load the dataspace header block of an object from swap
 */
Dataspace *d_load_dataspace(Object *obj)
{
    Dataspace *data;

    data = load_dataspace(obj, Swap::readv);

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->master)->update &&
	obj->count != 0) {
	d_upgrade_clone(data);
    }

    return data;
}

/*
 * NAME:	get_strings()
 * DESCRIPTION:	load strings for dataspace
 */
static void get_strings(Dataspace *data, void (*readv) (char*, Sector*, Uint, Uint))
{
    if (data->nstrings != 0) {
	/* load strings */
	data->sstrings = ALLOC(sstring, data->nstrings);
	(*readv)((char *) data->sstrings, data->sectors,
		 data->nstrings * sizeof(sstring), data->stroffset);
	if (data->strsize > 0) {
	    /* load strings text */
	    if (data->flags & DATA_STRCMP) {
		data->stext = Swap::decompress(data->sectors, readv,
					       data->strsize,
					       data->stroffset +
					       data->nstrings * sizeof(sstring),
					       &data->strsize);
	    } else {
		data->stext = ALLOC(char, data->strsize);
		(*readv)(data->stext, data->sectors, data->strsize,
			 data->stroffset + data->nstrings * sizeof(sstring));
	    }
	}
    }
}

/*
 * NAME:	data->get_string()
 * DESCRIPTION:	get a string from the dataspace
 */
static String *d_get_string(Dataspace *data, Uint idx)
{
    if (data->plane->strings == (strref *) NULL ||
	data->plane->strings[idx].str == (String *) NULL) {
	String *str;
	strref *s;
	Dataplane *p;
	Uint i;

	if (data->sstrings == (sstring *) NULL) {
	    get_strings(data, Swap::readv);
	}
	if (data->ssindex == (Uint *) NULL) {
	    Uint size;

	    data->ssindex = ALLOC(Uint, data->nstrings);
	    for (size = 0, i = 0; i < data->nstrings; i++) {
		data->ssindex[i] = size;
		size += data->sstrings[i].len;
	    }
	}

	str = String::alloc(data->stext + data->ssindex[idx],
			    data->sstrings[idx].len);
	p = data->plane;

	do {
	    if (p->strings == (strref *) NULL) {
		/* initialize string pointers */
		s = p->strings = ALLOC(strref, data->nstrings);
		for (i = data->nstrings; i > 0; --i) {
		    (s++)->str = (String *) NULL;
		}
	    }
	    s = &p->strings[idx];
	    s->str = str;
	    s->str->ref();
	    s->data = data;
	    s->ref = data->sstrings[idx].ref;
	    p = p->prev;
	} while (p != (Dataplane *) NULL);

	str->primary = &data->plane->strings[idx];
	return str;
    }
    return data->plane->strings[idx].str;
}

/*
 * NAME:	get_arrays()
 * DESCRIPTION:	load arrays for dataspace
 */
static void get_arrays(Dataspace *data, void (*readv) (char*, Sector*, Uint, Uint))
{
    if (data->narrays != 0) {
	/* load arrays */
	data->sarrays = ALLOC(sarray, data->narrays);
	(*readv)((char *) data->sarrays, data->sectors,
		 data->narrays * (Uint) sizeof(sarray), data->arroffset);
    }
}

/*
 * NAME:	data->get_array()
 * DESCRIPTION:	get an array from the dataspace
 */
static Array *d_get_array(Dataspace *data, Uint idx)
{
    if (data->plane->arrays == (arrref *) NULL ||
	data->plane->arrays[idx].arr == (Array *) NULL) {
	Array *arr;
	arrref *a;
	Dataplane *p;
	Uint i;

	if (data->sarrays == (sarray *) NULL) {
	    /* load arrays */
	    get_arrays(data, Swap::readv);
	}

	arr = Array::alloc(data->sarrays[idx].size);
	arr->tag = data->sarrays[idx].tag;
	p = data->plane;

	do {
	    if (p->arrays == (arrref *) NULL) {
		/* create array pointers */
		a = p->arrays = ALLOC(arrref, data->narrays);
		for (i = data->narrays; i > 0; --i) {
		    (a++)->arr = (Array *) NULL;
		}
	    }
	    a = &p->arrays[idx];
	    a->arr = arr;
	    a->arr->ref();
	    a->plane = &data->base;
	    a->data = data;
	    a->state = AR_UNCHANGED;
	    a->ref = data->sarrays[idx].ref;
	    p = p->prev;
	} while (p != (Dataplane *) NULL);

	arr->primary = &data->plane->arrays[idx];
	arr->prev = &data->alist;
	arr->next = data->alist.next;
	arr->next->prev = arr;
	data->alist.next = arr;
	return arr;
    }
    return data->plane->arrays[idx].arr;
}

/*
 * NAME:	data->get_values()
 * DESCRIPTION:	get values from the Dataspace
 */
static void d_get_values(Dataspace *data, svalue *sv, Value *v, int n)
{
    while (n > 0) {
	v->modified = FALSE;
	switch (v->type = sv->type) {
	case T_NIL:
	    v->number = 0;
	    break;

	case T_INT:
	    v->number = sv->number;
	    break;

	case T_STRING:
	    v->string = d_get_string(data, sv->string);
	    v->string->ref();
	    break;

	case T_FLOAT:
	case T_OBJECT:
	    v->oindex = sv->oindex;
	    v->objcnt = sv->objcnt;
	    break;

	case T_ARRAY:
	case T_MAPPING:
	case T_LWOBJECT:
	    v->array = d_get_array(data, sv->array);
	    v->array->ref();
	    break;
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * NAME:	data->new_variables()
 * DESCRIPTION:	initialize variables in a dataspace block
 */
void d_new_variables(Control *ctrl, Value *val)
{
    unsigned short n;
    char *type;
    VarDef *var;

    memset(val, '\0', ctrl->nvariables * sizeof(Value));
    for (n = ctrl->nvariables - ctrl->nvardefs, type = ctrl->varTypes();
	 n != 0; --n, type++) {
	val->type = *type;
	val++;
    }
    for (n = ctrl->nvardefs, var = ctrl->vars(); n != 0; --n, var++) {
	if (T_ARITHMETIC(var->type)) {
	    val->type = var->type;
	} else {
	    val->type = nil_type;
	}
	val++;
    }
}

/*
 * NAME:	get_variables()
 * DESCRIPTION:	load variables
 */
static void get_variables(Dataspace *data, void (*readv) (char*, Sector*, Uint, Uint))
{
    data->svariables = ALLOC(svalue, data->nvariables);
    (*readv)((char *) data->svariables, data->sectors,
	     data->nvariables * (Uint) sizeof(svalue), data->varoffset);
}

/*
 * NAME:	data->get_variable()
 * DESCRIPTION:	get a variable from the dataspace
 */
Value *d_get_variable(Dataspace *data, unsigned int idx)
{
    if (data->variables == (Value *) NULL) {
	/* create room for variables */
	data->variables = ALLOC(Value, data->nvariables);
	if (data->nsectors == 0 && data->svariables == (svalue *) NULL) {
	    /* new datablock */
	    d_new_variables(data->ctrl, data->variables);
	    data->variables[data->nvariables - 1] = nil_value;	/* extra var */
	} else {
	    /*
	     * variables must be loaded from swap
	     */
	    if (data->svariables == (svalue *) NULL) {
		/* load svalues */
		get_variables(data, Swap::readv);
	    }
	    d_get_values(data, data->svariables, data->variables,
			 data->nvariables);
	}
    }

    return &data->variables[idx];
}

/*
 * NAME:	get_elts()
 * DESCRIPTION:	load elements
 */
static void get_elts(Dataspace *data, void (*readv) (char*, Sector*, Uint, Uint))
{
    if (data->eltsize != 0) {
	/* load array elements */
	data->selts = (svalue *) ALLOC(svalue, data->eltsize);
	(*readv)((char *) data->selts, data->sectors,
		 data->eltsize * sizeof(svalue),
		 data->arroffset + data->narrays * sizeof(sarray));
    }
}

/*
 * NAME:	data->get_elts()
 * DESCRIPTION:	get the elements of an array
 */
Value *d_get_elts(Array *arr)
{
    Value *v;

    v = arr->elts;
    if (v == (Value *) NULL && arr->size != 0) {
	Dataspace *data;
	Uint idx;

	data = arr->primary->data;
	if (data->selts == (svalue *) NULL) {
	    get_elts(data, Swap::readv);
	}
	if (data->saindex == (Uint *) NULL) {
	    Uint size;

	    data->saindex = ALLOC(Uint, data->narrays);
	    for (size = 0, idx = 0; idx < data->narrays; idx++) {
		data->saindex[idx] = size;
		size += data->sarrays[idx].size;
	    }
	}

	v = arr->elts = ALLOC(Value, arr->size);
	idx = data->saindex[arr->primary - data->plane->arrays];
	d_get_values(data, &data->selts[idx], v, arr->size);
    }

    return v;
}

/*
 * NAME:	get_callouts()
 * DESCRIPTION:	load callouts from swap
 */
static void get_callouts(Dataspace *data, void (*readv) (char*, Sector*, Uint, Uint))
{
    if (data->ncallouts != 0) {
	data->scallouts = ALLOC(scallout, data->ncallouts);
	(*readv)((char *) data->scallouts, data->sectors,
		 data->ncallouts * (Uint) sizeof(scallout), data->cooffset);
    }
}

/*
 * NAME:	data->get_callouts()
 * DESCRIPTION:	load callouts from swap
 */
void d_get_callouts(Dataspace *data)
{
    scallout *sco;
    dcallout *co;
    uindex n;

    if (data->scallouts == (scallout *) NULL) {
	get_callouts(data, Swap::readv);
    }
    sco = data->scallouts;
    co = data->callouts = ALLOC(dcallout, data->ncallouts);

    for (n = data->ncallouts; n > 0; --n) {
	co->time = sco->time;
	co->mtime = sco->mtime;
	co->nargs = sco->nargs;
	if (sco->val[0].type == T_STRING) {
	    d_get_values(data, sco->val, co->val,
			 (sco->nargs > 3) ? 4 : sco->nargs + 1);
	} else {
	    co->val[0] = nil_value;
	}
	sco++;
	co++;
    }
}

/*
 * NAME:	data->arrcount()
 * DESCRIPTION:	count the number of arrays and strings in an array
 */
static void d_arrcount(savedata *save, Array *arr)
{
    arr->prev->next = arr->next;
    arr->next->prev = arr->prev;
    arr->prev = &save->alist;
    arr->next = save->alist.next;
    arr->next->prev = arr;
    save->alist.next = arr;
    save->narr++;
}

/*
 * NAME:	data->count()
 * DESCRIPTION:	count the number of arrays and strings in an object
 */
static void d_count(savedata *save, Value *v, Uint n)
{
    Object *obj;
    Value *elts;
    Uint count;

    while (n > 0) {
	switch (v->type) {
	case T_STRING:
	    if (v->string->put(save->nstr) == save->nstr) {
		save->nstr++;
		save->strsize += v->string->len;
	    }
	    break;

	case T_ARRAY:
	    if (v->array->put(save->narr) == save->narr) {
		d_arrcount(save, v->array);
	    }
	    break;

	case T_MAPPING:
	    if (v->array->put(save->narr) == save->narr) {
		if (v->array->hashmod) {
		    v->array->mapCompact(v->array->primary->data);
		}
		d_arrcount(save, v->array);
	    }
	    break;

	case T_LWOBJECT:
	    elts = d_get_elts(v->array);
	    if (elts->type == T_OBJECT) {
		obj = OBJ(elts->oindex);
		count = obj->count;
		if (elts[1].type == T_INT) {
		    /* convert to new LWO type */
		    elts[1].type = T_FLOAT;
		    elts[1].oindex = FALSE;
		}
		if (v->array->put(save->narr) == save->narr) {
		    if (elts->objcnt == count && elts[1].objcnt != obj->update)
		    {
			d_upgrade_lwobj(v->array, obj);
		    }
		    d_arrcount(save, v->array);
		}
	    } else if (v->array->put(save->narr) == save->narr) {
		d_arrcount(save, v->array);
	    }
	    break;
	}

	v++;
	--n;
    }
}

/*
 * NAME:	data->save()
 * DESCRIPTION:	save the values in an object
 */
static void d_save(savedata *save, svalue *sv, Value *v, unsigned short n)
{
    Uint i;

    while (n > 0) {
	sv->pad = '\0';
	switch (sv->type = v->type) {
	case T_NIL:
	    sv->oindex = 0;
	    sv->number = 0;
	    break;

	case T_INT:
	    sv->oindex = 0;
	    sv->number = v->number;
	    break;

	case T_STRING:
	    i = v->string->put(save->nstr);
	    sv->oindex = 0;
	    sv->string = i;
	    if (save->sstrings[i].ref++ == 0) {
		/* new string value */
		save->sstrings[i].len = v->string->len;
		memcpy(save->stext + save->strsize, v->string->text,
		       v->string->len);
		save->strsize += v->string->len;
	    }
	    break;

	case T_FLOAT:
	case T_OBJECT:
	    sv->oindex = v->oindex;
	    sv->objcnt = v->objcnt;
	    break;

	case T_ARRAY:
	case T_MAPPING:
	case T_LWOBJECT:
	    i = v->array->put(save->narr);
	    sv->oindex = 0;
	    sv->array = i;
	    if (save->sarrays[i].ref++ == 0) {
		/* new array value */
		save->sarrays[i].type = sv->type;
	    }
	    break;
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * NAME:	data->put_values()
 * DESCRIPTION:	save modified values as svalues
 */
static void d_put_values(Dataspace *data, svalue *sv, Value *v, unsigned short n)
{
    while (n > 0) {
	if (v->modified) {
	    sv->pad = '\0';
	    switch (sv->type = v->type) {
	    case T_NIL:
		sv->oindex = 0;
		sv->number = 0;
		break;

	    case T_INT:
		sv->oindex = 0;
		sv->number = v->number;
		break;

	    case T_STRING:
		sv->oindex = 0;
		sv->string = v->string->primary - data->base.strings;
		break;

	    case T_FLOAT:
	    case T_OBJECT:
		sv->oindex = v->oindex;
		sv->objcnt = v->objcnt;
		break;

	    case T_ARRAY:
	    case T_MAPPING:
	    case T_LWOBJECT:
		sv->oindex = 0;
		sv->array = v->array->primary - data->base.arrays;
		break;
	    }
	    v->modified = FALSE;
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * NAME:	data->save_dataspace()
 * DESCRIPTION:	save all values in a dataspace block
 */
static bool d_save_dataspace(Dataspace *data, bool swap)
{
    sdataspace header;
    Uint n;

    if (data->parser != (struct parser *) NULL &&
	!(OBJ(data->oindex)->flags & O_SPECIAL)) {
	ps_save(data->parser);
    }
    if (swap && (data->base.flags & MOD_SAVE)) {
	data->base.flags |= MOD_ALL;
    } else if (!(data->base.flags & MOD_ALL)) {
	return FALSE;
    }

    if (data->svariables != (svalue *) NULL && data->base.achange == 0 &&
	data->base.schange == 0 && !(data->base.flags & MOD_NEWCALLOUT)) {
	bool mod;

	/*
	 * No strings/arrays added or deleted. Check individual variables and
	 * array elements.
	 */
	if (data->base.flags & MOD_VARIABLE) {
	    /*
	     * variables changed
	     */
	    d_put_values(data, data->svariables, data->variables,
			 data->nvariables);
	    if (swap) {
		Swap::writev((char *) data->svariables, data->sectors,
			     data->nvariables * (Uint) sizeof(svalue),
			     data->varoffset);
	    }
	}
	if (data->base.flags & MOD_ARRAYREF) {
	    sarray *sa;
	    arrref *a;

	    /*
	     * references to arrays changed
	     */
	    sa = data->sarrays;
	    a = data->base.arrays;
	    mod = FALSE;
	    for (n = data->narrays; n > 0; --n) {
		if (a->arr != (Array *) NULL && sa->ref != (a->ref & ~ARR_MOD))
		{
		    sa->ref = a->ref & ~ARR_MOD;
		    mod = TRUE;
		}
		sa++;
		a++;
	    }
	    if (mod && swap) {
		Swap::writev((char *) data->sarrays, data->sectors,
			     data->narrays * sizeof(sarray), data->arroffset);
	    }
	}
	if (data->base.flags & MOD_ARRAY) {
	    arrref *a;
	    Uint idx;

	    /*
	     * array elements changed
	     */
	    a = data->base.arrays;
	    for (n = 0; n < data->narrays; n++) {
		if (a->arr != (Array *) NULL && (a->ref & ARR_MOD)) {
		    a->ref &= ~ARR_MOD;
		    idx = data->saindex[n];
		    d_put_values(data, &data->selts[idx], a->arr->elts,
				 a->arr->size);
		    if (swap) {
			Swap::writev((char *) &data->selts[idx], data->sectors,
				     a->arr->size * (Uint) sizeof(svalue),
				     data->arroffset +
					      data->narrays * sizeof(sarray) +
					      idx * sizeof(svalue));
		    }
		}
		a++;
	    }
	}
	if (data->base.flags & MOD_STRINGREF) {
	    sstring *ss;
	    strref *s;

	    /*
	     * string references changed
	     */
	    ss = data->sstrings;
	    s = data->base.strings;
	    mod = FALSE;
	    for (n = data->nstrings; n > 0; --n) {
		if (s->str != (String *) NULL && ss->ref != s->ref) {
		    ss->ref = s->ref;
		    mod = TRUE;
		}
		ss++;
		s++;
	    }
	    if (mod && swap) {
		Swap::writev((char *) data->sstrings, data->sectors,
			     data->nstrings * sizeof(sstring),
			     data->stroffset);
	    }
	}
	if (data->base.flags & MOD_CALLOUT) {
	    scallout *sco;
	    dcallout *co;

	    sco = data->scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->htime = 0;
		sco->mtime = co->mtime;
		sco->nargs = co->nargs;
		if (co->val[0].type == T_STRING) {
		    co->val[0].modified = TRUE;
		    co->val[1].modified = TRUE;
		    co->val[2].modified = TRUE;
		    co->val[3].modified = TRUE;
		    d_put_values(data, sco->val, co->val,
				 (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
		}
		sco++;
		co++;
	    }

	    if (swap) {
		/* save new (?) fcallouts value */
		Swap::writev((char *) &data->fcallouts, data->sectors,
			     (Uint) sizeof(uindex),
			  (Uint) ((char *)&header.fcallouts - (char *)&header));

		/* save scallouts */
		Swap::writev((char *) data->scallouts, data->sectors,
			     data->ncallouts * (Uint) sizeof(scallout),
			     data->cooffset);
	    }
	}
    } else {
	savedata save;
	char *text;
	Uint size;
	Array *arr;
	sarray *sarr;

	/*
	 * count the number and sizes of strings and arrays
	 */
	Array::merge();
	String::merge();
	save.narr = 0;
	save.nstr = 0;
	save.arrsize = 0;
	save.strsize = 0;
	save.alist.prev = save.alist.next = &save.alist;

	d_get_variable(data, 0);
	if (data->svariables == (svalue *) NULL) {
	    data->svariables = ALLOC(svalue, data->nvariables);
	}
	d_count(&save, data->variables, data->nvariables);

	if (data->ncallouts > 0) {
	    dcallout *co;

	    if (data->callouts == (dcallout *) NULL) {
		d_get_callouts(data);
	    }
	    /* remove empty callouts at the end */
	    for (n = data->ncallouts, co = data->callouts + n; n > 0; --n) {
		if ((--co)->val[0].type == T_STRING) {
		    break;
		}
		if (data->fcallouts == n) {
		    /* first callout in the free list */
		    data->fcallouts = co->co_next;
		} else {
		    /* connect previous to next */
		    data->callouts[co->co_prev - 1].co_next = co->co_next;
		    if (co->co_next != 0) {
			/* connect next to previous */
			data->callouts[co->co_next - 1].co_prev = co->co_prev;
		    }
		}
	    }
	    data->ncallouts = n;
	    if (n == 0) {
		/* all callouts removed */
		FREE(data->callouts);
		data->callouts = (dcallout *) NULL;
	    } else {
		/* process callouts */
		for (co = data->callouts; n > 0; --n, co++) {
		    if (co->val[0].type == T_STRING) {
			d_count(&save, co->val,
				(co->nargs > 3) ? 4 : co->nargs + 1);
		    }
		}
	    }
	}

	for (arr = save.alist.prev; arr != &save.alist; arr = arr->prev) {
	    save.arrsize += arr->size;
	    d_count(&save, d_get_elts(arr), arr->size);
	}

	/* fill in header */
	header.flags = 0;
	header.nvariables = data->nvariables;
	header.narrays = save.narr;
	header.eltsize = save.arrsize;
	header.nstrings = save.nstr;
	header.strsize = save.strsize;
	header.ncallouts = data->ncallouts;
	header.fcallouts = data->fcallouts;

	/*
	 * put everything in a saveable form
	 */
	save.sstrings = data->sstrings =
			REALLOC(data->sstrings, sstring, 0, header.nstrings);
	memset(save.sstrings, '\0', save.nstr * sizeof(sstring));
	save.stext = data->stext =
		     REALLOC(data->stext, char, 0, header.strsize);
	save.sarrays = data->sarrays =
		       REALLOC(data->sarrays, sarray, 0, header.narrays);
	memset(save.sarrays, '\0', save.narr * sizeof(sarray));
	save.selts = data->selts =
		     REALLOC(data->selts, svalue, 0, header.eltsize);
	save.narr = 0;
	save.nstr = 0;
	save.arrsize = 0;
	save.strsize = 0;
	data->scallouts = REALLOC(data->scallouts, scallout, 0,
				  header.ncallouts);

	d_save(&save, data->svariables, data->variables, data->nvariables);
	if (header.ncallouts > 0) {
	    scallout *sco;
	    dcallout *co;

	    sco = data->scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->htime = 0;
		sco->mtime = co->mtime;
		sco->nargs = co->nargs;
		if (co->val[0].type == T_STRING) {
		    d_save(&save, sco->val, co->val,
			   (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
		}
		sco++;
		co++;
	    }
	}
	for (arr = save.alist.prev, sarr = save.sarrays; arr != &save.alist;
	     arr = arr->prev, sarr++) {
	    sarr->size = arr->size;
	    sarr->tag = arr->tag;
	    d_save(&save, save.selts + save.arrsize, arr->elts, arr->size);
	    save.arrsize += arr->size;
	}
	if (arr->next != &save.alist) {
	    data->alist.next->prev = arr->prev;
	    arr->prev->next = data->alist.next;
	    data->alist.next = arr->next;
	    arr->next->prev = &data->alist;
	}

	/* clear merge tables */
	Array::clear();
	String::clear();

	if (swap) {
	    text = save.stext;
	    if (header.strsize >= CMPLIMIT) {
		text = ALLOC(char, header.strsize);
		size = Swap::compress(text, save.stext, header.strsize);
		if (size != 0) {
		    header.flags |= CMP_PRED;
		    header.strsize = size;
		} else {
		    FREE(text);
		    text = save.stext;
		}
	    }

	    /* create sector space */
	    size = sizeof(sdataspace) +
		   (header.nvariables + header.eltsize) * sizeof(svalue) +
		   header.narrays * sizeof(sarray) +
		   header.nstrings * sizeof(sstring) +
		   header.strsize +
		   header.ncallouts * (Uint) sizeof(scallout);
	    header.nsectors = Swap::alloc(size, data->nsectors, &data->sectors);
	    data->nsectors = header.nsectors;
	    OBJ(data->oindex)->dfirst = data->sectors[0];

	    /* save header */
	    size = sizeof(sdataspace);
	    Swap::writev((char *) &header, data->sectors, size, (Uint) 0);
	    Swap::writev((char *) data->sectors, data->sectors,
			 header.nsectors * (Uint) sizeof(Sector), size);
	    size += header.nsectors * (Uint) sizeof(Sector);

	    /* save variables */
	    data->varoffset = size;
	    Swap::writev((char *) data->svariables, data->sectors,
			 data->nvariables * (Uint) sizeof(svalue), size);
	    size += data->nvariables * (Uint) sizeof(svalue);

	    /* save arrays */
	    data->arroffset = size;
	    if (header.narrays > 0) {
		Swap::writev((char *) save.sarrays, data->sectors,
			     header.narrays * sizeof(sarray), size);
		size += header.narrays * sizeof(sarray);
		if (header.eltsize > 0) {
		    Swap::writev((char *) save.selts, data->sectors,
				 header.eltsize * sizeof(svalue), size);
		    size += header.eltsize * sizeof(svalue);
		}
	    }

	    /* save strings */
	    data->stroffset = size;
	    if (header.nstrings > 0) {
		Swap::writev((char *) save.sstrings, data->sectors,
			     header.nstrings * sizeof(sstring), size);
		size += header.nstrings * sizeof(sstring);
		if (header.strsize > 0) {
		    Swap::writev(text, data->sectors, header.strsize, size);
		    size += header.strsize;
		    if (text != save.stext) {
			FREE(text);
		    }
		}
	    }

	    /* save callouts */
	    data->cooffset = size;
	    if (header.ncallouts > 0) {
		Swap::writev((char *) data->scallouts, data->sectors,
			     header.ncallouts * (Uint) sizeof(scallout), size);
	    }
	}

	d_free_values(data);
	if (data->saindex != (Uint *) NULL) {
	    FREE(data->saindex);
	    data->saindex = NULL;
	}
	if (data->ssindex != (Uint *) NULL) {
	    FREE(data->ssindex);
	    data->ssindex = NULL;
	}

	data->flags = header.flags;
	data->narrays = header.narrays;
	data->eltsize = header.eltsize;
	data->nstrings = header.nstrings;
	data->strsize = save.strsize;

	data->base.schange = 0;
	data->base.achange = 0;
    }

    if (swap) {
	data->base.flags = 0;
    } else {
	data->base.flags = MOD_SAVE;
    }
    return TRUE;
}

/*
 * NAME:	data->swapout()
 * DESCRIPTION:	Swap out a portion of the control and dataspace blocks in
 *		memory.  Return the number of dataspace blocks swapped out.
 */
Sector d_swapout(unsigned int frag)
{
    Sector n, count;
    Dataspace *data;

    count = 0;

    if (frag != 0) {
	/* swap out dataspace blocks */
	data = dtail;
	for (n = ndata / frag, n -= (n > 0 && frag != 1); n > 0; --n) {
	    Dataspace *prev;

	    prev = data->prev;
	    if (d_save_dataspace(data, TRUE)) {
		count++;
	    }
	    OBJ(data->oindex)->data = (Dataspace *) NULL;
	    d_free_dataspace(data);
	    data = prev;
	}

	Control::swapout(frag);
    }

    /* perform garbage collection for one dataspace */
    if (gcdata != (Dataspace *) NULL) {
	if (d_save_dataspace(gcdata, (frag != 0)) && frag != 0) {
	    count++;
	}
	gcdata = gcdata->gcnext;
    }

    return count;
}

/*
 * NAME:	data->upgrade_mem()
 * DESCRIPTION:	upgrade all obj and all objects cloned from obj that have
 *		dataspaces in memory
 */
void d_upgrade_mem(Object *tmpl, Object *newob)
{
    Dataspace *data;
    Uint nvar;
    unsigned short *vmap;
    Object *obj;

    nvar = tmpl->ctrl->vmapsize;
    vmap = tmpl->ctrl->vmap;

    for (data = dtail; data != (Dataspace *) NULL; data = data->prev) {
	obj = OBJ(data->oindex);
	if ((obj == newob ||
	     (!(obj->flags & O_MASTER) && obj->master == newob->index)) &&
	    obj->count != 0) {
	    /* upgrade clone */
	    if (nvar != 0) {
		d_upgrade_data(data, nvar, vmap, tmpl);
	    }
	    data->ctrl->ndata--;
	    data->ctrl = newob->ctrl;
	    data->ctrl->ndata++;
	}
    }
}

/*
 * NAME:	data->conv_sarray0()
 * DESCRIPTION:	convert old sarrays
 */
static Uint d_conv_sarray0(sarray *sa, Sector *s, Uint n, Uint size)
{
    sarray0 *osa;
    Uint i;

    osa = ALLOC(sarray0, n);
    size = Swap::convert((char *) osa, s, sa0_layout, n, size, &Swap::conv);
    for (i = 0; i < n; i++) {
	sa->tag = osa->tag;
	sa->ref = osa->ref;
	sa->type = osa->type;
	(sa++)->size = (osa++)->size;
    }
    FREE(osa - n);
    return size;
}

/*
 * NAME:	data->conv_sstring0()
 * DESCRIPTION:	convert old sstrings
 */
static Uint d_conv_sstring0(sstring *ss, Sector *s, Uint n, Uint size)
{
    sstring0 *oss;
    Uint i;

    oss = ALLOC(sstring0, n);
    size = Swap::convert((char *) oss, s, ss0_layout, n, size, &Swap::conv);
    for (i = 0; i < n; i++) {
	ss->ref = oss->ref;
	(ss++)->len = (oss++)->len;
    }
    FREE(oss - n);
    return size;
}

/*
 * NAME:	data->fixobjs()
 * DESCRIPTION:	fix objects in dataspace
 */
static void d_fixobjs(svalue *v, Uint n, Uint *ctab)
{
    while (n != 0) {
	if (v->type == T_OBJECT) {
	    if (v->objcnt == ctab[v->oindex] && OBJ(v->oindex)->count != 0) {
		/* fix object count */
		v->objcnt = OBJ(v->oindex)->count;
	    } else {
		/* destructed object; mark as invalid */
		v->oindex = 0;
		v->objcnt = 1;
	    }
	}
	v++;
	--n;
    }
}

/*
 * NAME:	data->fixdata()
 * DESCRIPTION:	fix a dataspace
 */
static void d_fixdata(Dataspace *data, Uint *counttab)
{
    scallout *sco;
    unsigned int n;

    d_fixobjs(data->svariables, (Uint) data->nvariables, counttab);
    d_fixobjs(data->selts, data->eltsize, counttab);
    for (n = data->ncallouts, sco = data->scallouts; n > 0; --n, sco++) {
	if (sco->val[0].type == T_STRING) {
	    if (sco->nargs > 3) {
		d_fixobjs(sco->val, (Uint) 4, counttab);
	    } else {
		d_fixobjs(sco->val, sco->nargs + (Uint) 1, counttab);
	    }
	}
    }
}

/*
 * NAME:	data->conv_datapace()
 * DESCRIPTION:	convert dataspace
 */
static Dataspace *d_conv_dataspace(Object *obj, Uint *counttab,
				   void (*readv) (char*, Sector*, Uint, Uint))
{
    sdataspace header;
    Dataspace *data;
    Uint size;
    unsigned int n;

    UNREFERENCED_PARAMETER(counttab);

    data = d_alloc_dataspace(obj);

    /*
     * restore from snapshot
     */
    size = Swap::convert((char *) &header, &obj->dfirst, sd_layout, (Uint) 1,
			 (Uint) 0, readv);
    data->nvariables = header.nvariables;
    data->narrays = header.narrays;
    data->eltsize = header.eltsize;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    data->ncallouts = header.ncallouts;
    data->fcallouts = header.fcallouts;

    /* sectors */
    data->sectors = ALLOC(Sector, data->nsectors = header.nsectors);
    data->sectors[0] = obj->dfirst;
    for (n = 0; n < header.nsectors; n++) {
	size += Swap::convert((char *) (data->sectors + n), data->sectors, "d",
			      (Uint) 1, size, readv);
    }

    /* variables */
    data->svariables = ALLOC(svalue, header.nvariables);
    size += Swap::convert((char *) data->svariables, data->sectors, sv_layout,
			  (Uint) header.nvariables, size, readv);

    if (header.narrays != 0) {
	/* arrays */
	data->sarrays = ALLOC(sarray, header.narrays);
	if (conv_14) {
	    size += d_conv_sarray0(data->sarrays, data->sectors,
				   header.narrays, size);
	} else {
	    size += Swap::convert((char *) data->sarrays, data->sectors,
				  sa_layout, header.narrays, size, readv);
	}
	if (header.eltsize != 0) {
	    data->selts = ALLOC(svalue, header.eltsize);
	    size += Swap::convert((char *) data->selts, data->sectors,
				  sv_layout, header.eltsize, size, readv);
	}
    }

    if (header.nstrings != 0) {
	/* strings */
	data->sstrings = ALLOC(sstring, header.nstrings);
	if (conv_14) {
	    size += d_conv_sstring0(data->sstrings, data->sectors,
				    (Uint) header.nstrings, size);
	} else {
	    size += Swap::convert((char *) data->sstrings, data->sectors,
				  ss_layout, header.nstrings, size, readv);
	}
	if (header.strsize != 0) {
	    if (header.flags & CMP_TYPE) {
		data->stext = Swap::decompress(data->sectors, readv,
					       header.strsize, size,
					       &data->strsize);
	    } else {
		data->stext = ALLOC(char, header.strsize);
		(*readv)(data->stext, data->sectors, header.strsize, size);
	    }
	    size += header.strsize;
	}
    }

    if (header.ncallouts != 0) {
	unsigned short dummy;

	/* callouts */
	co_time(&dummy);
	data->scallouts = ALLOC(scallout, header.ncallouts);
	Swap::convert((char *) data->scallouts, data->sectors, sco_layout,
		      (Uint) header.ncallouts, size, readv);
    }

    data->ctrl = obj->control();
    data->ctrl->ndata++;

    return data;
}

/*
 * NAME:	data->restore_data()
 * DESCRIPTION:	restore a dataspace
 */
Dataspace *d_restore_data(Object *obj, Uint *counttab,
			  void (*readv) (char*, Sector*, Uint, Uint))
{
    Dataspace *data;

    data = (Dataspace *) NULL;
    if (OBJ(obj->index)->count != 0 && OBJ(obj->index)->dfirst != SW_UNUSED) {
	if (!converted) {
	    data = d_conv_dataspace(obj, counttab, readv);
	} else {
	    data = load_dataspace(obj, readv);
	    get_variables(data, readv);
	    get_arrays(data, readv);
	    get_elts(data, readv);
	    get_strings(data, readv);
	    get_callouts(data, readv);
	}
	obj->data = data;
	if (counttab != (Uint *) NULL) {
	    d_fixdata(data, counttab);
	}

	if (!(obj->flags & O_MASTER) &&
	    obj->update != OBJ(obj->master)->update) {
	    /* handle object upgrading right away */
	    d_upgrade_clone(data);
	}
	data->base.flags |= MOD_ALL;
    }

    return data;
}

/*
 * NAME:	data->restore_obj()
 * DESCRIPTION:	restore an object
 */
void d_restore_obj(Object *obj, Uint instance, Uint *counttab, bool cactive,
		   bool dactive)
{
    Control *ctrl;
    Dataspace *data;

    if (!converted) {
	ctrl = Control::restore(obj, instance, Swap::conv);
	data = d_restore_data(obj, counttab, Swap::conv);
    } else {
	ctrl = Control::restore(obj, instance, Swap::dreadv);
	data = d_restore_data(obj, counttab, Swap::dreadv);
    }

    if (!cactive) {
	ctrl->deref();
    }
    if (!dactive) {
	/* swap this out first */
	if (data != (Dataspace *) NULL && data != dtail) {
	    if (dhead == data) {
		dhead = data->next;
		dhead->prev = (Dataspace *) NULL;
	    } else {
		data->prev->next = data->next;
		data->next->prev = data->prev;
	    }
	    dtail->next = data;
	    data->prev = dtail;
	    data->next = (Dataspace *) NULL;
	    dtail = data;
	}
    }
}

/*
 * NAME:	data->converted()
 * DESCRIPTION:	snapshot conversion is complete
 */
void d_converted()
{
    converted = TRUE;
}
