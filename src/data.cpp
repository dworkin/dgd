/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2022 DGD Authors (see the commit log for details)
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


Value Value::zeroInt = { T_INT, TRUE };
Value Value::zeroFloat = { T_FLOAT, TRUE };
Value Value::nil = { T_NIL, TRUE };

/*
 * initialize nil value
 */
void Value::init(bool stricttc)
{
    Value::nil.type = (stricttc) ? T_NIL : T_INT;
}

/*
 * reference a value
 */
void Value::ref()
{
    switch (type) {
    case T_STRING:
	string->ref();
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	array->ref();
	break;
    }
}

/*
 * dereference a value
 */
void Value::del()
{
    switch (type) {
    case T_STRING:
	string->del();
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	array->del();
	break;
    }
}

/*
 * copy values from one place to another
 */
void Value::copy(Value *v, Value *w, unsigned int len)
{
    Value *o;

    for ( ; len != 0; --len) {
	switch (w->type) {
	case T_STRING:
	    w->string->ref();
	    break;

	case T_OBJECT:
	    if (DESTRUCTED(w)) {
		*v++ = Value::nil;
		w++;
		continue;
	    }
	    break;

	case T_LWOBJECT:
	    o = Dataspace::elts(w->array);
	    if (o->type == T_OBJECT && DESTRUCTED(o)) {
		*v++ = Value::nil;
		w++;
		continue;
	    }
	    /* fall through */
	case T_ARRAY:
	case T_MAPPING:
	    w->array->ref();
	    break;
	}
	*v++ = *w++;
    }
}

/*
 * return the name of the type
 */
char *Value::typeName(char *buf, unsigned int type)
{
    static const char *name[] = TYPENAMES;

    if ((type & T_TYPE) == T_CLASS) {
	type = (type & T_REF) | T_OBJECT;
    }
    strcpy(buf, name[type & T_TYPE]);
    type &= T_REF;
    type >>= REFSHIFT;
    if (type > 0) {
	char *p;

	p = buf + strlen(buf);
	*p++ = ' ';
	do {
	    *p++ = '*';
	} while (--type > 0);
	*p = '\0';
    }
    return buf;
}


# define COP_ADD	0	/* add callout patch */
# define COP_REMOVE	1	/* remove callout patch */
# define COP_REPLACE	2	/* replace callout patch */

class COPatch : public ChunkAllocated {
public:
    COPatch(Dataplane *plane, COPatch **c, int type, unsigned int handle,
	    DCallOut *co, Uint time, unsigned int mtime, uindex *q) :
	type(type), handle(handle), plane(plane), time(time), mtime(mtime),
	queue(q) {
	int i;
	Value *v;

	/* initialize */
	if (type == COP_ADD) {
	    aco = *co;
	} else {
	    rco = *co;
	}
	for (i = (co->nargs > 3) ? 4 : co->nargs + 1, v = co->val; i > 0; --i) {
	    (v++)->ref();
	}

	/* add to hash table */
	next = *c;
	*c = this;
    }

    /*
     * delete a callout patch
     */
    static void del(COPatch **c, bool deref) {
	COPatch *cop;
	DCallOut *co;
	int i;
	Value *v;

	/* remove from hash table */
	cop = *c;
	*c = cop->next;

	if (deref) {
	    /* free referenced callout */
	    co = (cop->type == COP_ADD) ? &cop->aco : &cop->rco;
	    v = co->val;
	    for (i = (co->nargs > 3) ? 4 : co->nargs + 1; i > 0; --i) {
		(v++)->del();
	    }
	}

	/* add to free list */
	delete cop;
    }

    /*
     * replace one callout patch with another
     */
    void replace(DCallOut *co, Uint time, unsigned int mtime, uindex *q) {
	int i;
	Value *v;

	type = COP_REPLACE;
	aco = *co;
	for (i = (co->nargs > 3) ? 4 : co->nargs + 1, v = co->val; i > 0; --i) {
	    (v++)->ref();
	}
	this->time = time;
	this->mtime = mtime;
	queue = q;
    }

    /*
     * commit a callout replacement
     */
    void commit() {
	int i;
	Value *v;

	type = COP_ADD;
	for (i = (rco.nargs > 3) ? 4 : rco.nargs + 1, v = rco.val; i > 0; --i) {
	    (v++)->del();
	}
    }

    /*
     * remove a callout replacement
     */
    void release() {
	int i;
	Value *v;

	type = COP_REMOVE;
	for (i = (aco.nargs > 3) ? 4 : aco.nargs + 1, v = aco.val; i > 0; --i) {
	    (v++)->del();
	}
    }

    /*
     * discard replacement
     */
    void discard()
    {
	/* force unref of proper component later */
	type = COP_ADD;
    }

    short type;			/* add, remove, replace */
    uindex handle;		/* callout handle */
    Dataplane *plane;		/* dataplane */
    Uint time;			/* start time */
    unsigned short mtime;	/* start time millisec component */
    uindex *queue;		/* callout queue */
    COPatch *next;		/* next in linked list */
    DCallOut aco;		/* added callout */
    DCallOut rco;		/* removed callout */
};

# define COPCHUNKSZ	32

class COPTable : public Allocated {
public:
    COPTable() {
	memset(&cop, '\0', COPATCHHTABSZ * sizeof(COPatch*));
    }

    /*
     * create new callout patch
     */
    COPatch *patch(Dataplane *plane, COPatch **c, int type, unsigned int handle,
	       DCallOut *co, Uint time, unsigned int mtime, uindex *q) {
	/* allocate */
	return chunknew (chunk) COPatch(plane, c, type, handle, co, time, mtime,
					q);
    }

    COPatch *cop[COPATCHHTABSZ];	/* hash table of callout patches */

private:
    Chunk<COPatch, COPCHUNKSZ> chunk;	/* callout patch chunk */
};


static Dataplane *plist;		/* list of dataplanes */
static uindex ncallout;			/* # callouts added */

/*
 * create the base dataplane
 */
Dataplane::Dataplane(Dataspace *data)
{
    level = 0;
    flags = 0;
    schange = 0;
    achange = 0;
    imports = 0;
    alocal.arr = (Array *) NULL;
    alocal.plane = this;
    alocal.data = data;
    alocal.state = AR_CHANGED;
    arrays = (ArrRef *) NULL;
    strings = (StrRef *) NULL;
    coptab = (COPTable *) NULL;
    prev = (Dataplane *) NULL;
    plist = (Dataplane *) NULL;
}

/*
 * create a new dataplane
 */
Dataplane::Dataplane(Dataspace *data, LPCint level) : level(level)
{
    Uint i;

    flags = data->plane->flags;
    schange = data->plane->schange;
    achange = data->plane->achange;
    imports = data->plane->imports;

    /* copy value information from previous plane */
    original = (Value *) NULL;
    alocal.arr = (Array *) NULL;
    alocal.plane = this;
    alocal.data = data;
    alocal.state = AR_CHANGED;
    coptab = data->plane->coptab;

    if (data->plane->arrays != (ArrRef *) NULL) {
	ArrRef *a, *b;

	arrays = ALLOC(ArrRef, i = data->narrays);
	for (a = arrays, b = data->plane->arrays; i != 0; a++, b++, --i) {
	    if (b->arr != (Array *) NULL) {
		*a = *b;
		a->arr->primary = a;
		a->arr->ref();
	    } else {
		a->arr = (Array *) NULL;
	    }
	}
    } else {
	arrays = (ArrRef *) NULL;
    }
    achunk = (Array::Backup *) NULL;

    if (data->plane->strings != (StrRef *) NULL) {
	StrRef *s, *t;

	strings = ALLOC(StrRef, i = data->nstrings);
	for (s = strings, t = data->plane->strings; i != 0; s++, t++, --i) {
	    if (t->str != (String *) NULL) {
		*s = *t;
		s->str->primary = s;
		s->str->ref();
	    } else {
		s->str = (String *) NULL;
	    }
	}
    } else {
	strings = (StrRef *) NULL;
    }

    prev = data->plane;
    data->plane = this;
    plist = ::plist;
    ::plist = this;
}

/*
 * commit non-swapped arrays among the values
 */
void Dataplane::commitValues(Value *v, unsigned int n, LPCint level)
{
    Array *list, *arr;

    list = (Array *) NULL;
    for (;;) {
	while (n != 0) {
	    if (T_INDEXED(v->type)) {
		arr = v->array;
		if (arr->primary->arr == (Array *) NULL &&
		    arr->primary->plane->level > level) {
		    arr->canonicalize();
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
 * commit callout patches to previous plane
 */
void Dataplane::commitCallouts(bool merge)
{
    COPatch **c, **n, *cop;
    COPatch **t, **next;
    int i;

    for (i = COPATCHHTABSZ, t = coptab->cop; --i >= 0; t++) {
	if (*t != (COPatch *) NULL && (*t)->plane == this) {
	    /*
	     * find previous plane in hash chain
	     */
	    next = t;
	    do {
		next = &(*next)->next;
	    } while (*next != (COPatch *) NULL && (*next)->plane == this);

	    c = t;
	    do {
		cop = *c;
		if (cop->type != COP_REMOVE) {
		    commitValues(cop->aco.val + 1,
				 (cop->aco.nargs > 3) ? 3 : cop->aco.nargs,
				 prev->level);
		}

		if (prev->level == 0) {
		    /*
		     * commit to last plane
		     */
		    switch (cop->type) {
		    case COP_ADD:
			CallOut::create(alocal.data->oindex, cop->handle,
					cop->time, cop->mtime, cop->queue);
			--ncallout;
			break;

		    case COP_REMOVE:
			CallOut::del(alocal.data->oindex, cop->handle,
				     cop->rco.time, cop->rco.mtime);
			ncallout++;
			break;

		    case COP_REPLACE:
			CallOut::del(alocal.data->oindex, cop->handle,
				     cop->rco.time, cop->rco.mtime);
			CallOut::create(alocal.data->oindex, cop->handle,
					cop->time, cop->mtime, cop->queue);
			cop->commit();
			break;
		    }

		    if (next == &cop->next) {
			next = c;
		    }
		    COPatch::del(c, TRUE);
		} else {
		    /*
		     * commit to previous plane
		     */
		    cop->plane = prev;
		    if (merge) {
			for (n = next;
			     *n != (COPatch *) NULL && (*n)->plane == prev;
			     n = &(*n)->next) {
			    if (cop->handle == (*n)->handle) {
				switch (cop->type) {
				case COP_ADD:
				    /* turn old remove into replace, del new */
				    (*n)->replace(&cop->aco, cop->time,
						  cop->mtime, cop->queue);
				    if (next == &cop->next) {
					next = c;
				    }
				    COPatch::del(c, TRUE);
				    break;

				case COP_REMOVE:
				    if ((*n)->type == COP_REPLACE) {
					/* turn replace back into remove */
					(*n)->release();
				    } else {
					/* del old */
					COPatch::del(n, TRUE);
				    }
				    /* del new */
				    if (next == &cop->next) {
					next = c;
				    }
				    COPatch::del(c, TRUE);
				    break;

				case COP_REPLACE:
				    if ((*n)->type == COP_REPLACE) {
					/* merge replaces into old, del new */
					(*n)->release();
					(*n)->replace(&cop->aco, cop->time,
						      cop->mtime, cop->queue);
					if (next == &cop->next) {
					    next = c;
					}
					COPatch::del(c, TRUE);
				    } else {
					/* make replace into add, remove old */
					COPatch::del(n, TRUE);
					cop->commit();
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
 * commit the current data planes
 */
void Dataplane::commit(LPCint level, Value *retval)
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
    for (r = &::plist, p = *r; p != (Dataplane *) NULL && p->level == level;
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
    for (p = ::plist; p != clist; p = p->plist) {
	/*
	 * commit changes to previous plane
	 */
	data = p->alocal.data;
	if (p->original != (Value *) NULL) {
	    if (p->level == 1 || p->prev->original != (Value *) NULL) {
		/* free backed-up variable values */
		for (v = p->original, i = data->nvariables; i != 0; v++, --i) {
		    v->del();
		}
		FREE(p->original);
	    } else {
		/* move originals to previous plane */
		p->prev->original = p->original;
	    }
	    commitValues(data->variables, data->nvariables, level - 1);
	}

	if (p->coptab != (COPTable *) NULL) {
	    /* commit callout changes */
	    p->commitCallouts((p->flags & PLANE_MERGE) != 0);
	    if (p->level == 1) {
		delete p->coptab;
		p->coptab = (COPTable *) NULL;
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	Array::commit(&p->achunk, p->prev, (p->flags & PLANE_MERGE) != 0);
	if (p->flags & PLANE_MERGE) {
	    if (p->arrays != (ArrRef *) NULL) {
		ArrRef *a;

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

	    if (p->strings != (StrRef *) NULL) {
		StrRef *s;

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
    commitValues(retval, 1, level - 1);

    /*
     * pass 3: deallocate
     */
    for (p = ::plist; p != clist; p = ::plist) {
	p->prev->flags = (p->flags & MOD_ALL) | MOD_SAVE;
	p->prev->schange = p->schange;
	p->prev->achange = p->achange;
	p->prev->imports = p->imports;
	p->alocal.data->plane = p->prev;
	::plist = p->plist;
	delete p;
    }
}

/*
 * discard callout patches on current plane, restoring old callouts
 */
void Dataplane::discardCallouts()
{
    COPatch *cop, **c, **t;
    Dataspace *data;
    int i;

    data = alocal.data;
    for (i = COPATCHHTABSZ, t = coptab->cop; --i >= 0; t++) {
	c = t;
	while (*c != (COPatch *) NULL && (*c)->plane == this) {
	    cop = *c;
	    switch (cop->type) {
	    case COP_ADD:
		data->freeCallOut(cop->handle);
		COPatch::del(c, TRUE);
		--ncallout;
		break;

	    case COP_REMOVE:
		data->allocCallOut(cop->handle, cop->rco.time, cop->rco.mtime,
				   cop->rco.nargs, cop->rco.val);
		COPatch::del(c, FALSE);
		ncallout++;
		break;

	    case COP_REPLACE:
		data->freeCallOut(cop->handle);
		data->allocCallOut(cop->handle, cop->rco.time, cop->rco.mtime,
				   cop->rco.nargs, cop->rco.val);
		cop->discard();
		COPatch::del(c, TRUE);
		break;
	    }
	}
    }
}

/*
 * discard the current data plane without committing it
 */
void Dataplane::discard(LPCint level)
{
    Dataplane *p;
    Dataspace *data;
    Value *v;
    Uint i;

    for (p = ::plist; p != (Dataplane *) NULL && p->level == level; p = ::plist)
    {
	/*
	 * discard changes except for callout mods
	 */
	p->prev->flags |= p->flags & (MOD_CALLOUT | MOD_NEWCALLOUT);

	data = p->alocal.data;
	if (p->original != (Value *) NULL) {
	    /* restore original variable values */
	    for (v = data->variables, i = data->nvariables; i != 0; --i, v++) {
		v->del();
	    }
	    memcpy(data->variables, p->original,
		   data->nvariables * sizeof(Value));
	    FREE(p->original);
	}

	if (p->coptab != (COPTable *) NULL) {
	    /* undo callout changes */
	    p->discardCallouts();
	    if (p->prev == &data->base) {
		delete p->coptab;
		p->coptab = (COPTable *) NULL;
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	Array::discard(&p->achunk);
	if (p->arrays != (ArrRef *) NULL) {
	    ArrRef *a;

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

	if (p->strings != (StrRef *) NULL) {
	    StrRef *s;

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
	::plist = p->plist;
	delete p;
    }
}

/*
 * commit array to previous plane
 */
Array::Backup **Dataplane::commitArray(Array *arr, Dataplane *old)
{
    if (arr->primary->plane != this) {
	arr->canonicalize();

	if (arr->primary->arr == (Array *) NULL) {
	    arr->primary = &alocal;
	} else {
	    arr->primary->plane = this;
	}
	commitValues(arr->elts, arr->size, level);
    }

    return (this == old) ? (Array::Backup **) NULL : &achunk;
}

/*
 * restore array to previous plane
 */
void Dataplane::discardArray(Array *arr)
{
    /* swapped-in arrays will be fixed later */
    arr->primary = &alocal;
}


static Dataspace *dhead, *dtail;	/* list of dataspace blocks */
static Dataspace *gcdata;		/* next dataspace to garbage collect */
static Dataspace *ifirst;		/* list of dataspaces with imports */
static Sector ndata;			/* # dataspace blocks */

/*
 * allocate a new dataspace block
 */
Dataspace::Dataspace(Object *obj) : base(this)
{
    if (dhead != (Dataspace *) NULL) {
	/* insert at beginning of list */
	dhead->prev = this;
	prev = (Dataspace *) NULL;
	next = dhead;
	dhead = this;
	gcprev = gcdata->gcprev;
	gcnext = gcdata;
	gcprev->gcnext = this;
	gcdata->gcprev = this;
    } else {
	/* list was empty */
	prev = next = (Dataspace *) NULL;
	dhead = dtail = this;
	gcdata = this;
	gcprev = gcnext = this;
    }
    ndata++;

    iprev = (Dataspace *) NULL;
    inext = (Dataspace *) NULL;
    flags = 0;

    oindex = obj->index;
    ctrl = (Control *) NULL;

    /* sectors */
    nsectors = 0;
    sectors = (Sector *) NULL;

    /* variables */
    nvariables = 0;
    variables = (Value *) NULL;
    svariables = (SValue *) NULL;

    /* arrays */
    narrays = 0;
    eltsize = 0;
    sarrays = (SArray *) NULL;
    saindex = (Uint *) NULL;
    selts = (SValue *) NULL;

    /* strings */
    nstrings = 0;
    strsize = 0;
    sstrings = (SString *) NULL;
    ssindex = (Uint *) NULL;
    stext = (char *) NULL;

    /* callouts */
    ncallouts = 0;
    fcallouts = 0;
    callouts = (DCallOut *) NULL;
    scallouts = (SCallOut *) NULL;

    /* value plane */
    plane = &base;

    /* parse_string data */
    parser = (Parser *) NULL;
}

/*
 * create a new dataspace block
 */
Dataspace *Dataspace::create(Object *obj)
{
    Dataspace *data;

    data = new Dataspace(obj);
    data->base.flags = MOD_VARIABLE;
    data->ctrl = obj->control();
    data->ctrl->ndata++;
    data->nvariables = data->ctrl->nvariables + 1;

    return data;
}

/*
 * reference data block
 */
void Dataspace::ref()
{
    if (this != dhead) {
	/* move to head of list */
	prev->next = next;
	if (next != (Dataspace *) NULL) {
	    next->prev = prev;
	} else {
	    dtail = prev;
	}
	prev = (Dataspace *) NULL;
	next = dhead;
	dhead->prev = this;
	dhead = this;
    }
}

/*
 * dereference data block
 */
void Dataspace::deref()
{
    /* swap this out first */
    if (this != dtail) {
	if (dhead == this) {
	    dhead = next;
	    dhead->prev = (Dataspace *) NULL;
	} else {
	    prev->next = next;
	    next->prev = prev;
	}
	dtail->next = this;
	prev = dtail;
	next = (Dataspace *) NULL;
	dtail = this;
    }
}

/*
 * free values in a dataspace block
 */
void Dataspace::freeValues()
{
    Uint i;

    /* free parse_string data */
    if (parser != (Parser *) NULL) {
	delete parser;
	parser = (Parser *) NULL;
    }

    /* free variables */
    if (variables != (Value *) NULL) {
	Value *v;

	for (i = nvariables, v = variables; i > 0; --i, v++) {
	    v->del();
	}

	FREE(variables);
	variables = (Value *) NULL;
    }

    /* free callouts */
    if (callouts != (DCallOut *) NULL) {
	DCallOut *co;
	Value *v;
	int j;

	for (i = ncallouts, co = callouts; i > 0; --i, co++) {
	    v = co->val;
	    if (v->type == T_STRING) {
		j = 1 + co->nargs;
		if (j > 4) {
		    j = 4;
		}
		do {
		    (v++)->del();
		} while (--j > 0);
	    }
	}

	FREE(callouts);
	callouts = (DCallOut *) NULL;
    }

    /* free arrays */
    if (base.arrays != (ArrRef *) NULL) {
	ArrRef *a;

	for (i = narrays, a = base.arrays; i > 0; --i, a++) {
	    if (a->arr != (Array *) NULL) {
		a->arr->del();
	    }
	}

	FREE(base.arrays);
	base.arrays = (ArrRef *) NULL;
    }

    /* free strings */
    if (base.strings != (StrRef *) NULL) {
	StrRef *s;

	for (i = nstrings, s = base.strings; i > 0; --i, s++) {
	    if (s->str != (String *) NULL) {
		s->str->primary = (StrRef *) NULL;
		s->str->del();
	    }
	}

	FREE(base.strings);
	base.strings = (StrRef *) NULL;
    }

    /* free any left-over arrays */
    if (alist.next != &alist) {
	alist.prev->next = alist.next;
	alist.next->prev = alist.prev;
	alist.next->freelist();
	alist.prev = alist.next = &alist;
    }
}

/*
 * remove the dataspace block from memory
 */
Dataspace::~Dataspace()
{
    /* free values */
    freeValues();

    /* delete sectors */
    if (sectors != (Sector *) NULL) {
	FREE(sectors);
    }

    /* free scallouts */
    if (scallouts != (SCallOut *) NULL) {
	FREE(scallouts);
    }

    /* free sarrays */
    if (sarrays != (SArray *) NULL) {
	if (selts != (SValue *) NULL) {
	    FREE(selts);
	}
	if (saindex != (Uint *) NULL) {
	    FREE(saindex);
	}
	FREE(sarrays);
    }

    /* free sstrings */
    if (sstrings != (SString *) NULL) {
	if (stext != (char *) NULL) {
	    FREE(stext);
	}
	if (ssindex != (Uint *) NULL) {
	    FREE(ssindex);
	}
	FREE(sstrings);
    }

    /* free svariables */
    if (svariables != (SValue *) NULL) {
	FREE(svariables);
    }

    if (ctrl != (Control *) NULL) {
	ctrl->ndata--;
    }

    if (this != dhead) {
	prev->next = next;
    } else {
	dhead = next;
	if (dhead != (Dataspace *) NULL) {
	    dhead->prev = (Dataspace *) NULL;
	}
    }
    if (this != dtail) {
	next->prev = prev;
    } else {
	dtail = prev;
	if (dtail != (Dataspace *) NULL) {
	    dtail->next = (Dataspace *) NULL;
	}
    }
    gcprev->gcnext = gcnext;
    gcnext->gcprev = gcprev;
    if (this == gcdata) {
	gcdata = (this != gcnext) ? gcnext : (Dataspace *) NULL;
    }
    --ndata;
}

/*
 * delete a dataspace block from swap and memory
 */
void Dataspace::del()
{
    if (iprev != (Dataspace *) NULL) {
	iprev->inext = inext;
	if (inext != (Dataspace *) NULL) {
	    inext->iprev = iprev;
	}
    } else if (ifirst == this) {
	ifirst = inext;
	if (ifirst != (Dataspace *) NULL) {
	    ifirst->iprev = (Dataspace *) NULL;
	}
    }

    if (ncallouts != 0) {
	Uint n;
	DCallOut *co;
	unsigned short dummy;

	/*
	 * remove callouts from callout table
	 */
	if (callouts == (DCallOut *) NULL) {
	    loadCallouts();
	}
	for (n = ncallouts, co = callouts + n; n > 0; --n) {
	    if ((--co)->val[0].type == T_STRING) {
		delCallOut(n, &dummy);
	    }
	}
    }
    if (sectors != (Sector *) NULL) {
	Swap::wipev(sectors, nsectors);
	Swap::delv(sectors, nsectors);
    }
    delete this;
}


struct SDataspace {
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

struct SValue {
    char type;			/* object, number, string, array */
    char pad;			/* 0 */
    uindex oindex;		/* index in object table */
    union {
	LPCint number;		/* number */
	LPCuint string;		/* string */
	LPCuint objcnt;		/* object creation count */
	LPCuint array;		/* array */
    };
};

static char sv_layout[] = "ccuI";

struct SArray {
    Uint tag;			/* unique value for each array */
    Uint ref;			/* refcount */
    char type;			/* array type */
    unsigned short size;	/* size of array */
};

static char sa_layout[] = "iics";

struct SArray0 {
    Uint index;			/* index in array value table */
    char type;			/* array type */
    unsigned short size;	/* size of array */
    Uint ref;			/* refcount */
    Uint tag;			/* unique value for each array */
};

static char sa0_layout[] = "icsii";

struct SString {
    Uint ref;			/* refcount */
    ssizet len;			/* length of string */
};

static char ss_layout[] = "it";

struct SString0 {
    Uint index;			/* index in string text table */
    ssizet len;			/* length of string */
    Uint ref;			/* refcount */
};

static char ss0_layout[] = "iti";

struct SCallOut {
    Time time;			/* time of call */
    uindex nargs;		/* number of arguments */
    SValue val[4];		/* function name, 3 direct arguments */
};

static char sco_layout[] = "lu[ccui][ccui][ccui][ccui]";

struct SCallOut0 {
    Uint time;			/* time of call */
    unsigned short htime;	/* time of call, high word */
    unsigned short mtime;	/* time of call milliseconds */
    uindex nargs;		/* number of arguments */
    SValue val[4];		/* function name, 3 direct arguments */
};

static char sco0_layout[] = "issu[ccui][ccui][ccui][ccui]";

# define co_prev	time
# define co_next	nargs

static bool conv_14;			/* convert arrays & strings? */
static bool conv_16;			/* convert callouts? */
static bool convDone;			/* conversion complete? */

/*
 * load the dataspace header block
 */
Dataspace *Dataspace::load(Object *obj,
			   void (*readv) (char*, Sector*, Uint, Uint))
{
    SDataspace header;
    Dataspace *data;
    Uint size;

    data = new Dataspace(obj);
    data->ctrl = obj->control();
    data->ctrl->ndata++;

    /* header */
    (*readv)((char *) &header, &obj->dfirst, (Uint) sizeof(SDataspace),
	     (Uint) 0);
    data->nsectors = header.nsectors;
    data->sectors = ALLOC(Sector, header.nsectors);
    data->sectors[0] = obj->dfirst;
    size = header.nsectors * (Uint) sizeof(Sector);
    if (header.nsectors > 1) {
	(*readv)((char *) data->sectors, data->sectors, size,
		 (Uint) sizeof(SDataspace));
    }
    size += sizeof(SDataspace);

    data->flags = header.flags;

    /* variables */
    data->varoffset = size;
    data->nvariables = header.nvariables;
    size += data->nvariables * (Uint) sizeof(SValue);

    /* arrays */
    data->arroffset = size;
    data->narrays = header.narrays;
    data->eltsize = header.eltsize;
    size += header.narrays * (Uint) sizeof(SArray) +
	    header.eltsize * sizeof(SValue);

    /* strings */
    data->stroffset = size;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    size += header.nstrings * sizeof(SString) + header.strsize;

    /* callouts */
    data->cooffset = size;
    data->ncallouts = header.ncallouts;
    data->fcallouts = header.fcallouts;

    return data;
}

/*
 * load the dataspace header block of an object from swap
 */
Dataspace *Dataspace::load(Object *obj)
{
    Dataspace *data;

    data = load(obj, Swap::readv);

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->master)->update &&
	obj->count != 0) {
	data->upgradeClone();
    }

    return data;
}

/*
 * convert old sarrays
 */
Uint Dataspace::convSArray0(SArray *sa, Sector *s, Uint n, Uint offset,
			    void (*readv) (char*, Sector*, Uint, Uint))
{
    SArray0 *osa;
    Uint i;

    osa = ALLOC(SArray0, n);
    offset = Swap::convert((char *) osa, s, sa0_layout, n, offset, readv);
    for (i = 0; i < n; i++) {
	sa->tag = osa->tag;
	sa->ref = osa->ref;
	sa->type = osa->type;
	(sa++)->size = (osa++)->size;
    }
    FREE(osa - n);
    return offset;
}

/*
 * convert old sstrings
 */
Uint Dataspace::convSString0(SString *ss, Sector *s, Uint n, Uint offset,
			     void (*readv) (char*, Sector*, Uint, Uint))
{
    SString0 *oss;
    Uint i;

    oss = ALLOC(SString0, n);
    offset = Swap::convert((char *) oss, s, ss0_layout, n, offset, readv);
    for (i = 0; i < n; i++) {
	ss->ref = oss->ref;
	(ss++)->len = (oss++)->len;
    }
    FREE(oss - n);
    return offset;
}

/*
 * convert old scallouts
 */
void Dataspace::convSCallOut0(SCallOut *sco, Sector *s, Uint n, Uint offset,
			      void (*readv) (char*, Sector*, Uint, Uint))
{
    SCallOut0 *sco0;
    Uint i;

    sco0 = ALLOC(SCallOut0, n);
    Swap::convert((char *) sco0, s, sco0_layout, n, offset, readv);
    for (i = 0; i < n; i++) {
	if (sco0->val[0].type == T_STRING) {
	    sco->time = ((Time) sco0->time << 16) |
			 ((sco0->mtime == 0xffff) ? TIME_INT : sco0->mtime);
	} else {
	    sco->time = sco0->time;
	}
	sco->nargs = sco0->nargs;
	sco->val[0] = sco0->val[0];
	sco->val[1] = sco0->val[1];
	sco->val[2] = sco0->val[2];
	(sco++)->val[3] = (sco0++)->val[3];
    }
    FREE(sco0 - n);
}

/*
 * convert dataspace
 */
Dataspace *Dataspace::conv(Object *obj, Uint *counttab,
			   void (*readv) (char*, Sector*, Uint, Uint))
{
    SDataspace header;
    Dataspace *data;
    Uint size;
    unsigned int n;

    UNREFERENCED_PARAMETER(counttab);

    data = new Dataspace(obj);

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
    data->svariables = ALLOC(SValue, header.nvariables);
    size += Swap::convert((char *) data->svariables, data->sectors, sv_layout,
			  (Uint) header.nvariables, size, readv);

    if (header.narrays != 0) {
	/* arrays */
	data->sarrays = ALLOC(SArray, header.narrays);
	if (conv_14) {
	    size += convSArray0(data->sarrays, data->sectors, header.narrays,
				size, readv);
	} else {
	    size += Swap::convert((char *) data->sarrays, data->sectors,
				  sa_layout, header.narrays, size, readv);
	}
	if (header.eltsize != 0) {
	    data->selts = ALLOC(SValue, header.eltsize);
	    size += Swap::convert((char *) data->selts, data->sectors,
				  sv_layout, header.eltsize, size, readv);
	}
    }

    if (header.nstrings != 0) {
	/* strings */
	data->sstrings = ALLOC(SString, header.nstrings);
	if (conv_14) {
	    size += convSString0(data->sstrings, data->sectors,
				 (Uint) header.nstrings, size, readv);
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
	CallOut::cotime(&dummy);
	data->scallouts = ALLOC(SCallOut, header.ncallouts);
	if (conv_16) {
	    convSCallOut0(data->scallouts, data->sectors,
			  (Uint) header.ncallouts, size, readv);
	} else {
	    Swap::convert((char *) data->scallouts, data->sectors, sco_layout,
			  (Uint) header.ncallouts, size, readv);
	}
    }

    data->ctrl = obj->control();
    data->ctrl->ndata++;

    return data;
}

/*
 * load strings for dataspace
 */
void Dataspace::loadStrings(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (nstrings != 0) {
	/* load strings */
	sstrings = ALLOC(SString, nstrings);
	(*readv)((char *) sstrings, sectors, nstrings * sizeof(SString),
		 stroffset);
	if (strsize > 0) {
	    /* load strings text */
	    if (flags & DATA_STRCMP) {
		stext = Swap::decompress(sectors, readv, strsize,
				         stroffset + nstrings * sizeof(SString),
					 &strsize);
	    } else {
		stext = ALLOC(char, strsize);
		(*readv)(stext, sectors, strsize,
			 stroffset + nstrings * sizeof(SString));
	    }
	}
    }
}

/*
 * get a string from the dataspace
 */
String *Dataspace::string(Uint idx)
{
    if (plane->strings == (StrRef *) NULL ||
	plane->strings[idx].str == (String *) NULL) {
	String *str;
	StrRef *s;
	Dataplane *p;
	Uint i;

	if (sstrings == (SString *) NULL) {
	    loadStrings(Swap::readv);
	}
	if (ssindex == (Uint *) NULL) {
	    Uint size;

	    ssindex = ALLOC(Uint, nstrings);
	    for (size = 0, i = 0; i < nstrings; i++) {
		ssindex[i] = size;
		size += sstrings[i].len;
	    }
	}

	str = String::alloc(stext + ssindex[idx], sstrings[idx].len);
	p = plane;

	do {
	    if (p->strings == (StrRef *) NULL) {
		/* initialize string pointers */
		s = p->strings = ALLOC(StrRef, nstrings);
		for (i = nstrings; i > 0; --i) {
		    (s++)->str = (String *) NULL;
		}
	    }
	    s = &p->strings[idx];
	    s->str = str;
	    s->str->ref();
	    s->data = this;
	    s->ref = sstrings[idx].ref;
	    p = p->prev;
	} while (p != (Dataplane *) NULL);

	str->primary = &plane->strings[idx];
	return str;
    }
    return plane->strings[idx].str;
}

/*
 * load arrays for dataspace
 */
void Dataspace::loadArrays(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (narrays != 0) {
	/* load arrays */
	sarrays = ALLOC(SArray, narrays);
	(*readv)((char *) sarrays, sectors, narrays * (Uint) sizeof(SArray),
		 arroffset);
    }
}

/*
 * get an array from the dataspace
 */
Array *Dataspace::array(Uint idx, int type)
{
    if (plane->arrays == (ArrRef *) NULL ||
	plane->arrays[idx].arr == (Array *) NULL) {
	Array *arr;
	ArrRef *a;
	Dataplane *p;
	Uint i;

	if (sarrays == (SArray *) NULL) {
	    /* load arrays */
	    loadArrays(Swap::readv);
	}

	switch (type) {
	case T_ARRAY:
	    arr = Array::alloc(sarrays[idx].size);
	    break;

	case T_MAPPING:
	    arr = Mapping::alloc(sarrays[idx].size);
	    break;

	case T_LWOBJECT:
	    arr = LWO::alloc(sarrays[idx].size);
	    break;
	}
	arr->tag = sarrays[idx].tag;
	p = plane;

	do {
	    if (p->arrays == (ArrRef *) NULL) {
		/* create array pointers */
		a = p->arrays = ALLOC(ArrRef, narrays);
		for (i = narrays; i > 0; --i) {
		    (a++)->arr = (Array *) NULL;
		}
	    }
	    a = &p->arrays[idx];
	    a->arr = arr;
	    a->arr->ref();
	    a->plane = &base;
	    a->data = this;
	    a->state = AR_UNCHANGED;
	    a->ref = sarrays[idx].ref;
	    p = p->prev;
	} while (p != (Dataplane *) NULL);

	arr->primary = &plane->arrays[idx];
	arr->prev = &alist;
	arr->next = alist.next;
	arr->next->prev = arr;
	alist.next = arr;
	return arr;
    }
    return plane->arrays[idx].arr;
}

/*
 * get values from the Dataspace
 */
void Dataspace::loadValues(SValue *sv, Value *v, int n)
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
	    v->string = string(sv->string);
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
	    v->array = array(sv->array, v->type);
	    v->array->ref();
	    break;
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * initialize variables in a dataspace block
 */
void Dataspace::newVars(Control *ctrl, Value *val)
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
	    val->type = Value::nil.type;
	}
	val++;
    }
}

/*
 * load variables
 */
void Dataspace::loadVars(void (*readv) (char*, Sector*, Uint, Uint))
{
    svariables = ALLOC(SValue, nvariables);
    (*readv)((char *) svariables, sectors, nvariables * (Uint) sizeof(SValue),
	     varoffset);
}

/*
 * get a variable from the dataspace
 */
Value *Dataspace::variable(unsigned int idx)
{
    if (variables == (Value *) NULL) {
	/* create room for variables */
	variables = ALLOC(Value, nvariables);
	if (nsectors == 0 && svariables == (SValue *) NULL) {
	    /* new datablock */
	    newVars(ctrl, variables);
	    variables[nvariables - 1] = Value::nil;	/* extra var */
	} else {
	    /*
	     * variables must be loaded from swap
	     */
	    if (svariables == (SValue *) NULL) {
		/* load svalues */
		loadVars(Swap::readv);
	    }
	    loadValues(svariables, variables, nvariables);
	}
    }

    return &variables[idx];
}

/*
 * load elements
 */
void Dataspace::loadElts(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (eltsize != 0) {
	/* load array elements */
	selts = ALLOC(SValue, eltsize);
	(*readv)((char *) selts, sectors, eltsize * sizeof(SValue),
		 arroffset + narrays * sizeof(SArray));
    }
}

/*
 * get the elements of an array
 */
Value *Dataspace::elts(Array *arr)
{
    Value *v;

    v = arr->elts;
    if (v == (Value *) NULL && arr->size != 0) {
	Dataspace *data;
	Uint idx;

	data = arr->primary->data;
	if (data->selts == (SValue *) NULL) {
	    data->loadElts(Swap::readv);
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
	data->loadValues(&data->selts[idx], v, arr->size);
    }

    return v;
}

/*
 * load callouts from swap
 */
void Dataspace::loadCallouts(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (ncallouts != 0) {
	scallouts = ALLOC(SCallOut, ncallouts);
	(*readv)((char *) scallouts, sectors,
		 ncallouts * (Uint) sizeof(SCallOut), cooffset);
    }
}

/*
 * load callouts from swap
 */
void Dataspace::loadCallouts()
{
    SCallOut *sco;
    DCallOut *co;
    uindex n;

    if (scallouts == (SCallOut *) NULL) {
	loadCallouts(Swap::readv);
    }
    sco = scallouts;
    co = callouts = ALLOC(DCallOut, ncallouts);

    for (n = ncallouts; n > 0; --n) {
	co->time = sco->time >> 16;
	co->mtime = sco->time;
	co->nargs = sco->nargs;
	if (sco->val[0].type == T_STRING) {
	    loadValues(sco->val, co->val,
		       (sco->nargs > 3) ? 4 : sco->nargs + 1);
	} else {
	    co->val[0] = Value::nil;
	}
	sco++;
	co++;
    }
}


class SaveData {
public:
    SaveData() {
	narr = 0;
	nstr = 0;
	arrsize = 0;
	strsize = 0;
    }

    /*
     * count the number of arrays and strings in an object
     */
    void count(Value *v, Uint n) {
	Object *obj;
	Value *elts;
	Uint count;

	while (n > 0) {
	    switch (v->type) {
	    case T_STRING:
		if (v->string->put(nstr) == nstr) {
		    nstr++;
		    strsize += v->string->len;
		}
		break;

	    case T_ARRAY:
		if (v->array->put(narr) == narr) {
		    arrCount(v->array);
		}
		break;

	    case T_MAPPING:
		if (v->array->put(narr) == narr) {
		    v->array->canonicalize();
		    arrCount(v->array);
		}
		break;

	    case T_LWOBJECT:
		elts = Dataspace::elts(v->array);
		if (elts->type == T_OBJECT) {
		    obj = OBJ(elts->oindex);
		    count = obj->count;
		    if (elts[1].type == T_INT) {
			/* convert to new LWO type */
			elts[1].type = T_FLOAT;
			elts[1].oindex = FALSE;
		    }
		    if (v->array->put(narr) == narr) {
			if (elts->objcnt == count &&
			    elts[1].objcnt != obj->update) {
			    Dataspace::upgradeLWO(dynamic_cast<LWO *>(v->array),
						  obj);
			}
			arrCount(v->array);
		    }
		} else if (v->array->put(narr) == narr) {
		    arrCount(v->array);
		}
		break;
	    }

	    v++;
	    --n;
	}
    }

    /*
     * save the values in an object
     */
    void save(SValue *sv, Value *v, unsigned short n) {
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
		i = v->string->put(nstr);
		sv->oindex = 0;
		sv->string = i;
		if (sstrings[i].ref++ == 0) {
		    /* new string value */
		    sstrings[i].len = v->string->len;
		    memcpy(stext + strsize, v->string->text, v->string->len);
		    strsize += v->string->len;
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
		i = v->array->put(narr);
		sv->oindex = 0;
		sv->array = i;
		if (sarrays[i].ref++ == 0) {
		    /* new array value */
		    sarrays[i].type = sv->type;
		}
		break;
	    }
	    sv++;
	    v++;
	    --n;
	}
    }

    Uint narr;				/* # of arrays */
    Uint nstr;				/* # of strings */
    Uint arrsize;			/* # of array elements */
    Uint strsize;			/* total string size */
    SArray *sarrays;			/* save arrays */
    SValue *selts;			/* save array elements */
    SString *sstrings;			/* save strings */
    char *stext;			/* save string elements */
    Array alist;			/* linked list sentinel */

private:
    /*
     * count the number of arrays and strings in an array
     */
    void arrCount(Array *arr) {
	arr->prev->next = arr->next;
	arr->next->prev = arr->prev;
	arr->prev = &alist;
	arr->next = alist.next;
	arr->next->prev = arr;
	alist.next = arr;
	narr++;
    }
};

/*
 * save modified values as svalues
 */
void Dataspace::saveValues(SValue *sv, Value *v, unsigned short n)
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
		sv->string = v->string->primary - base.strings;
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
		sv->array = v->array->primary - base.arrays;
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
 * save all values in a dataspace block
 */
bool Dataspace::save(bool swap)
{
    SDataspace header;
    Uint n;

    if (parser != (Parser *) NULL && !(OBJ(oindex)->flags & O_SPECIAL)) {
	parser->save();
    }
    if (swap && (base.flags & MOD_SAVE)) {
	base.flags |= MOD_ALL;
    } else if (!(base.flags & MOD_ALL)) {
	return FALSE;
    }

    if (svariables != (SValue *) NULL && base.achange == 0 &&
	base.schange == 0 && !(base.flags & MOD_NEWCALLOUT)) {
	bool mod;

	/*
	 * No strings/arrays added or deleted. Check individual variables and
	 * array elements.
	 */
	if (base.flags & MOD_VARIABLE) {
	    /*
	     * variables changed
	     */
	    saveValues(svariables, variables, nvariables);
	    if (swap) {
		Swap::writev((char *) svariables, sectors,
			     nvariables * (Uint) sizeof(SValue), varoffset);
	    }
	}
	if (base.flags & MOD_ARRAYREF) {
	    SArray *sa;
	    ArrRef *a;

	    /*
	     * references to arrays changed
	     */
	    sa = sarrays;
	    a = base.arrays;
	    mod = FALSE;
	    for (n = narrays; n > 0; --n) {
		if (a->arr != (Array *) NULL && sa->ref != (a->ref & ~ARR_MOD))
		{
		    sa->ref = a->ref & ~ARR_MOD;
		    mod = TRUE;
		}
		sa++;
		a++;
	    }
	    if (mod && swap) {
		Swap::writev((char *) sarrays, sectors,
			     narrays * sizeof(SArray), arroffset);
	    }
	}
	if (base.flags & MOD_ARRAY) {
	    ArrRef *a;
	    Uint idx;

	    /*
	     * array elements changed
	     */
	    a = base.arrays;
	    for (n = 0; n < narrays; n++) {
		if (a->arr != (Array *) NULL && (a->ref & ARR_MOD)) {
		    a->ref &= ~ARR_MOD;
		    idx = saindex[n];
		    saveValues(&selts[idx], a->arr->elts, a->arr->size);
		    if (swap) {
			Swap::writev((char *) &selts[idx], sectors,
				     a->arr->size * (Uint) sizeof(SValue),
				     arroffset + narrays * sizeof(SArray) +
							  idx * sizeof(SValue));
		    }
		}
		a++;
	    }
	}
	if (base.flags & MOD_STRINGREF) {
	    SString *ss;
	    StrRef *s;

	    /*
	     * string references changed
	     */
	    ss = sstrings;
	    s = base.strings;
	    mod = FALSE;
	    for (n = nstrings; n > 0; --n) {
		if (s->str != (String *) NULL && ss->ref != s->ref) {
		    ss->ref = s->ref;
		    mod = TRUE;
		}
		ss++;
		s++;
	    }
	    if (mod && swap) {
		Swap::writev((char *) sstrings, sectors,
			     nstrings * sizeof(SString), stroffset);
	    }
	}
	if (base.flags & MOD_CALLOUT) {
	    SCallOut *sco;
	    DCallOut *co;

	    sco = scallouts;
	    co = callouts;
	    for (n = ncallouts; n > 0; --n) {
		sco->time = ((Time) co->time << 16) | co->mtime;
		sco->nargs = co->nargs;
		if (co->val[0].type == T_STRING) {
		    co->val[0].modified = TRUE;
		    co->val[1].modified = TRUE;
		    co->val[2].modified = TRUE;
		    co->val[3].modified = TRUE;
		    saveValues(sco->val, co->val,
			       (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
		}
		sco++;
		co++;
	    }

	    if (swap) {
		/* save new (?) fcallouts value */
		Swap::writev((char *) &fcallouts, sectors,
			     (Uint) sizeof(uindex),
			  (Uint) ((char *)&header.fcallouts - (char *)&header));

		/* save scallouts */
		Swap::writev((char *) scallouts, sectors,
			     ncallouts * (Uint) sizeof(SCallOut), cooffset);
	    }
	}
    } else {
	SaveData save;
	char *text;
	Uint size;
	Array *arr;
	SArray *sarr;

	/*
	 * count the number and sizes of strings and arrays
	 */
	Array::merge();
	String::merge();

	variable(0);
	if (svariables == (SValue *) NULL) {
	    svariables = ALLOC(SValue, nvariables);
	}
	save.count(variables, nvariables);

	if (ncallouts > 0) {
	    DCallOut *co;

	    if (callouts == (DCallOut *) NULL) {
		loadCallouts();
	    }
	    /* remove empty callouts at the end */
	    for (n = ncallouts, co = callouts + n; n > 0; --n) {
		if ((--co)->val[0].type == T_STRING) {
		    break;
		}
		if (fcallouts == n) {
		    /* first callout in the free list */
		    fcallouts = co->co_next;
		} else {
		    /* connect previous to next */
		    callouts[co->co_prev - 1].co_next = co->co_next;
		    if (co->co_next != 0) {
			/* connect next to previous */
			callouts[co->co_next - 1].co_prev = co->co_prev;
		    }
		}
	    }
	    ncallouts = n;
	    if (n == 0) {
		/* all callouts removed */
		FREE(callouts);
		callouts = (DCallOut *) NULL;
	    } else {
		/* process callouts */
		for (co = callouts; n > 0; --n, co++) {
		    if (co->val[0].type == T_STRING) {
			save.count(co->val,
				   (co->nargs > 3) ? 4 : co->nargs + 1);
		    }
		}
	    }
	}

	for (arr = save.alist.prev; arr != &save.alist; arr = arr->prev) {
	    save.arrsize += arr->size;
	    save.count(elts(arr), arr->size);
	}

	/* fill in header */
	header.flags = 0;
	header.nvariables = nvariables;
	header.narrays = save.narr;
	header.eltsize = save.arrsize;
	header.nstrings = save.nstr;
	header.strsize = save.strsize;
	header.ncallouts = ncallouts;
	header.fcallouts = fcallouts;

	/*
	 * put everything in a saveable form
	 */
	save.sstrings = sstrings =
			REALLOC(sstrings, SString, 0, header.nstrings);
	memset(save.sstrings, '\0', save.nstr * sizeof(SString));
	save.stext = stext = REALLOC(stext, char, 0, header.strsize);
	save.sarrays = sarrays = REALLOC(sarrays, SArray, 0, header.narrays);
	memset(save.sarrays, '\0', save.narr * sizeof(SArray));
	save.selts = selts = REALLOC(selts, SValue, 0, header.eltsize);
	save.narr = 0;
	save.nstr = 0;
	save.arrsize = 0;
	save.strsize = 0;
	scallouts = REALLOC(scallouts, SCallOut, 0, header.ncallouts);

	save.save(svariables, variables, nvariables);
	if (header.ncallouts > 0) {
	    SCallOut *sco;
	    DCallOut *co;

	    sco = scallouts;
	    co = callouts;
	    for (n = ncallouts; n > 0; --n) {
		sco->time = ((Time) co->time << 16) | co->mtime;
		sco->nargs = co->nargs;
		if (co->val[0].type == T_STRING) {
		    save.save(sco->val, co->val,
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
	    save.save(save.selts + save.arrsize, arr->elts, arr->size);
	    save.arrsize += arr->size;
	}
	if (arr->next != &save.alist) {
	    alist.next->prev = arr->prev;
	    arr->prev->next = alist.next;
	    alist.next = arr->next;
	    arr->next->prev = &alist;
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
	    size = sizeof(SDataspace) +
		   (header.nvariables + header.eltsize) * sizeof(SValue) +
		   header.narrays * sizeof(SArray) +
		   header.nstrings * sizeof(SString) +
		   header.strsize +
		   header.ncallouts * (Uint) sizeof(SCallOut);
	    header.nsectors = Swap::alloc(size, nsectors, &sectors);
	    nsectors = header.nsectors;
	    OBJ(oindex)->dfirst = sectors[0];

	    /* save header */
	    size = sizeof(SDataspace);
	    Swap::writev((char *) &header, sectors, size, (Uint) 0);
	    Swap::writev((char *) sectors, sectors,
			 header.nsectors * (Uint) sizeof(Sector), size);
	    size += header.nsectors * (Uint) sizeof(Sector);

	    /* save variables */
	    varoffset = size;
	    Swap::writev((char *) svariables, sectors,
			 nvariables * (Uint) sizeof(SValue), size);
	    size += nvariables * (Uint) sizeof(SValue);

	    /* save arrays */
	    arroffset = size;
	    if (header.narrays > 0) {
		Swap::writev((char *) save.sarrays, sectors,
			     header.narrays * sizeof(SArray), size);
		size += header.narrays * sizeof(SArray);
		if (header.eltsize > 0) {
		    Swap::writev((char *) save.selts, sectors,
				 header.eltsize * sizeof(SValue), size);
		    size += header.eltsize * sizeof(SValue);
		}
	    }

	    /* save strings */
	    stroffset = size;
	    if (header.nstrings > 0) {
		Swap::writev((char *) save.sstrings, sectors,
			     header.nstrings * sizeof(SString), size);
		size += header.nstrings * sizeof(SString);
		if (header.strsize > 0) {
		    Swap::writev(text, sectors, header.strsize, size);
		    size += header.strsize;
		    if (text != save.stext) {
			FREE(text);
		    }
		}
	    }

	    /* save callouts */
	    cooffset = size;
	    if (header.ncallouts > 0) {
		Swap::writev((char *) scallouts, sectors,
			     header.ncallouts * (Uint) sizeof(SCallOut), size);
	    }
	}

	freeValues();
	if (saindex != (Uint *) NULL) {
	    FREE(saindex);
	    saindex = NULL;
	}
	if (ssindex != (Uint *) NULL) {
	    FREE(ssindex);
	    ssindex = NULL;
	}

	flags = header.flags;
	narrays = header.narrays;
	eltsize = header.eltsize;
	nstrings = header.nstrings;
	strsize = save.strsize;

	base.schange = 0;
	base.achange = 0;
    }

    if (swap) {
	base.flags = 0;
    } else {
	base.flags = MOD_SAVE;
    }
    return TRUE;
}

/*
 * fix objects in dataspace
 */
void Dataspace::fixObjs(SValue *v, Uint n, Uint *ctab)
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
 * fix a dataspace
 */
void Dataspace::fix(Uint *counttab)
{
    SCallOut *sco;
    unsigned int n;

    fixObjs(svariables, (Uint) nvariables, counttab);
    fixObjs(selts, eltsize, counttab);
    for (n = ncallouts, sco = scallouts; n > 0; --n, sco++) {
	if (sco->val[0].type == T_STRING) {
	    if (sco->nargs > 3) {
		fixObjs(sco->val, (Uint) 4, counttab);
	    } else {
		fixObjs(sco->val, sco->nargs + (Uint) 1, counttab);
	    }
	}
    }
}

/*
 * restore a dataspace
 */
Dataspace *Dataspace::restore(Object *obj, Uint *counttab,
			      void (*readv) (char*, Sector*, Uint, Uint))
{
    Dataspace *data;

    data = (Dataspace *) NULL;
    if (OBJ(obj->index)->count != 0 && OBJ(obj->index)->dfirst != SW_UNUSED) {
	if (!convDone) {
	    data = conv(obj, counttab, readv);
	} else {
	    data = load(obj, readv);
	    data->loadVars(readv);
	    data->loadArrays(readv);
	    data->loadElts(readv);
	    data->loadStrings(readv);
	    data->loadCallouts(readv);
	}
	obj->data = data;
	if (counttab != (Uint *) NULL) {
	    data->fix(counttab);
	}

	if (!(obj->flags & O_MASTER) &&
	    obj->update != OBJ(obj->master)->update) {
	    /* handle object upgrading right away */
	    data->upgradeClone();
	}
	data->base.flags |= MOD_ALL;
    }

    return data;
}


/*
 * reference the right-hand side in an assignment
 */
void Dataspace::refRhs(Value *rhs)
{
    String *str;
    Array *arr;

    switch (rhs->type) {
    case T_STRING:
	str = rhs->string;
	if (str->primary != (StrRef *) NULL && str->primary->data == this) {
	    /* in this object */
	    str->primary->ref++;
	    plane->flags |= MOD_STRINGREF;
	} else {
	    /* not in this object: ref imported string */
	    plane->schange++;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr = rhs->array;
	if (arr->primary->data == this) {
	    /* in this object */
	    if (arr->primary->arr != (Array *) NULL) {
		/* swapped in */
		arr->primary->ref++;
		plane->flags |= MOD_ARRAYREF;
	    } else {
		/* ref new array */
		plane->achange++;
	    }
	} else {
	    /* not in this object: ref imported array */
	    if (plane->imports++ == 0 && ifirst != this &&
		iprev == (Dataspace *) NULL) {
		/* add to imports list */
		iprev = (Dataspace *) NULL;
		inext = ifirst;
		if (ifirst != (Dataspace *) NULL) {
		    ifirst->iprev = this;
		}
		ifirst = this;
	    }
	    plane->achange++;
	}
	break;
    }
}

/*
 * delete the left-hand side in an assignment
 */
void Dataspace::delLhs(Value *lhs)
{
    String *str;
    Array *arr;

    switch (lhs->type) {
    case T_STRING:
	str = lhs->string;
	if (str->primary != (StrRef *) NULL && str->primary->data == this) {
	    /* in this object */
	    if (--(str->primary->ref) == 0) {
		str->primary->str = (String *) NULL;
		str->primary = (StrRef *) NULL;
		str->del();
		plane->schange++;	/* last reference removed */
	    }
	    plane->flags |= MOD_STRINGREF;
	} else {
	    /* not in this object: deref imported string */
	    plane->schange--;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr = lhs->array;
	if (arr->primary->data == this) {
	    /* in this object */
	    if (arr->primary->arr != (Array *) NULL) {
		/* swapped in */
		plane->flags |= MOD_ARRAYREF;
		if ((--(arr->primary->ref) & ~ARR_MOD) == 0) {
		    elts(arr);
		    arr->primary->arr = (Array *) NULL;
		    arr->primary = &arr->primary->plane->alocal;
		    arr->del();
		    plane->achange++;
		}
	    } else {
		/* deref new array */
		plane->achange--;
	    }
	} else {
	    /* not in this object: deref imported array */
	    plane->imports--;
	    plane->achange--;
	}
	break;
    }
}

/*
 * check the elements of an array for imports
 */
void Dataspace::refImports(Array *arr)
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
 * assign a value to a variable
 */
void Dataspace::assignVar(Value *var, Value *val)
{
    if (var >= variables && var < variables + nvariables) {
	if (plane->level != 0 && plane->original == (Value *) NULL) {
	    /*
	     * back up variables
	     */
	    Value::copy(plane->original = ALLOC(Value, nvariables), variables,
			nvariables);
	}
	refRhs(val);
	delLhs(var);
	plane->flags |= MOD_VARIABLE;
    }

    val->ref();
    var->del();

    *var = *val;
    var->modified = TRUE;
}

/*
 * get an object's special value
 */
Value *Dataspace::extra(Dataspace *data)
{
    return data->variable(data->nvariables - 1);
}

/*
 * set an object's special value
 */
void Dataspace::setExtra(Dataspace *data, Value *val)
{
    data->assignVar(data->variable(data->nvariables - 1), val);
}

/*
 * wipe out an object's special value
 */
void Dataspace::wipeExtra(Dataspace *data)
{
    data->assignVar(data->variable(data->nvariables - 1), &Value::nil);

    if (data->parser != (Parser *) NULL) {
	/*
	 * get rid of the parser, too
	 */
	delete data->parser;
	data->parser = (Parser *) NULL;
    }
}

/*
 * assign a value to an array element
 */
void Dataspace::assignElt(Array *arr, Value *elt, Value *val)
{
    Dataspace *data;

    if (plane->level != arr->primary->data->plane->level) {
	/*
	 * bring dataspace of imported array up to the current plane level
	 */
	new Dataplane(arr->primary->data, plane->level);
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
	data->refRhs(val);
	data->delLhs(elt);
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

    val->ref();
    elt->del();

    *elt = *val;
    elt->modified = TRUE;
}

/*
 * mark a mapping as changed in size
 */
void Dataspace::changeMap(Mapping *map)
{
    ArrRef *a;

    a = map->primary;
    if (a->state == AR_UNCHANGED) {
	a->plane->achange++;
	a->state = AR_CHANGED;
    }
}


/*
 * allocate a new callout
 */
uindex Dataspace::allocCallOut(uindex handle, Uint time, unsigned short mtime,
			       int nargs, Value *v)
{
    DCallOut *co;

    if (ncallouts == 0) {
	/*
	 * the first in this object
	 */
	co = callouts = ALLOC(DCallOut, 1);
	ncallouts = handle = 1;
	plane->flags |= MOD_NEWCALLOUT;
    } else {
	if (callouts == (DCallOut *) NULL) {
	    loadCallouts();
	}
	if (handle != 0) {
	    /*
	     * get a specific callout from the free list
	     */
	    co = &callouts[handle - 1];
	    if (handle == fcallouts) {
		fcallouts = co->co_next;
	    } else {
		callouts[co->co_prev - 1].co_next = co->co_next;
		if (co->co_next != 0) {
		    callouts[co->co_next - 1].co_prev = co->co_prev;
		}
	    }
	} else {
	    handle = fcallouts;
	    if (handle != 0) {
		/*
		 * from free list
		 */
		co = &callouts[handle - 1];
		if (co->co_next == 0 || co->co_next > handle) {
		    /* take 1st free callout */
		    fcallouts = co->co_next;
		} else {
		    /* take 2nd free callout */
		    co = &callouts[co->co_next - 1];
		    callouts[handle - 1].co_next = co->co_next;
		    if (co->co_next != 0) {
			callouts[co->co_next - 1].co_prev = handle;
		    }
		    handle = co - callouts + 1;
		}
		plane->flags |= MOD_CALLOUT;
	    } else {
		/*
		 * add new callout
		 */
		handle = ncallouts;
		co = callouts = REALLOC(callouts, DCallOut, handle, handle + 1);
		co += handle;
		ncallouts = ++handle;
		plane->flags |= MOD_NEWCALLOUT;
	    }
	}
    }

    co->time = time;
    co->mtime = mtime;
    co->nargs = nargs;
    memcpy(co->val, v, sizeof(co->val));
    switch (nargs) {
    default:
	refRhs(&v[3]);
	/* fall through */
    case 2:
	refRhs(&v[2]);
	/* fall through */
    case 1:
	refRhs(&v[1]);
	/* fall through */
    case 0:
	refRhs(&v[0]);
	break;
    }

    return handle;
}

/*
 * free a callout
 */
void Dataspace::freeCallOut(unsigned int handle)
{
    DCallOut *co;
    Value *v;
    uindex n;

    co = &callouts[handle - 1];
    v = co->val;
    switch (co->nargs) {
    default:
	delLhs(&v[3]);
	v[3].del();
	/* fall through */
    case 2:
	delLhs(&v[2]);
	v[2].del();
	/* fall through */
    case 1:
	delLhs(&v[1]);
	v[1].del();
	/* fall through */
    case 0:
	delLhs(&v[0]);
	v[0].string->del();
	break;
    }
    v[0] = Value::nil;

    n = fcallouts;
    if (n != 0) {
	callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    fcallouts = handle;

    plane->flags |= MOD_CALLOUT;
}

/*
 * add a new callout
 */
uindex Dataspace::newCallOut(String *func, LPCint delay, unsigned int mdelay,
			     Frame *f, int nargs)
{
    Uint ct, t;
    unsigned short m;
    uindex *q;
    Value v[4];
    uindex handle;

    ct = CallOut::check(ncallout, delay, mdelay, &t, &m, &q);
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
	PUT_ARRVAL(&v[3], Array::create(this, nargs - 2));
	memcpy(v[3].array->elts, f->sp + 2, (nargs - 2) * sizeof(Value));
	refImports(v[3].array);
	break;
    }
    f->sp += nargs;
    handle = allocCallOut(0, ct, m, nargs, v);

    if (plane->level == 0) {
	/*
	 * add normal callout
	 */
	CallOut::create(oindex, handle, t, m, q);
    } else {
	COPatch **c, *cop;
	DCallOut *co;
	COPatch **cc;

	/*
	 * add callout patch
	 */
	if (plane->coptab == (COPTable *) NULL) {
	    plane->coptab = new COPTable;
	}
	co = &callouts[handle - 1];
	cc = c = &plane->coptab->cop[handle % COPATCHHTABSZ];
	for (;;) {
	    cop = *c;
	    if (cop == (COPatch *) NULL || cop->plane != plane) {
		/* add new */
		plane->coptab->patch(plane, cc, COP_ADD, handle, co, t, m, q);
		break;
	    }

	    if (cop->handle == handle) {
		/* replace removed */
		cop->replace(co, t, m, q);
		break;
	    }

	    c = &cop->next;
	}

	ncallout++;
    }

    return handle;
}

/*
 * remove a callout
 */
LPCint Dataspace::delCallOut(Uint handle, unsigned short *mtime)
{
    DCallOut *co;
    LPCint t;

    *mtime = TIME_INT;
    if (handle == 0 || handle > ncallouts) {
	/* no such callout */
	return -1;
    }
    if (callouts == (DCallOut *) NULL) {
	loadCallouts();
    }

    co = &callouts[handle - 1];
    if (co->val[0].type != T_STRING) {
	/* invalid callout */
	return -1;
    }

    *mtime = co->mtime;
    t = CallOut::remaining(co->time, mtime);
    if (plane->level == 0) {
	/*
	 * remove normal callout
	 */
	CallOut::del(oindex, (uindex) handle, co->time, co->mtime);
    } else {
	COPatch **c, *cop;
	COPatch **cc;

	/*
	 * add/remove callout patch
	 */
	--ncallout;

	if (plane->coptab == (COPTable *) NULL) {
	    plane->coptab = new COPTable;
	}
	cc = c = &plane->coptab->cop[handle % COPATCHHTABSZ];
	for (;;) {
	    cop = *c;
	    if (cop == (COPatch *) NULL || cop->plane != plane) {
		/* delete new */
		plane->coptab->patch(plane, cc, COP_REMOVE, (uindex) handle, co,
				     (Uint) 0, 0, (uindex *) NULL);
		break;
	    }
	    if (cop->handle == handle) {
		/* delete existing */
		if (cop->type == COP_REPLACE) {
		    cop->release();
		} else {
		    COPatch::del(c, TRUE);
		}
		break;
	    }
	    c = &cop->next;
	}
    }
    freeCallOut((uindex) handle);

    return t;
}

/*
 * get a callout
 */
String *Dataspace::callOut(unsigned int handle, Frame *f, int *nargs)
{
    String *str;
    DCallOut *co;
    Value *v, *o;
    uindex n;

    if (callouts == (DCallOut *) NULL) {
	loadCallouts();
    }

    co = &callouts[handle - 1];
    v = co->val;
    delLhs(&v[0]);
    str = v[0].string;

    f->growStack((*nargs = co->nargs) + 1);
    *--f->sp = v[0];

    switch (co->nargs) {
    case 3:
	delLhs(&v[3]);
	*--f->sp = v[3];
    case 2:
	delLhs(&v[2]);
	*--f->sp = v[2];
    case 1:
	delLhs(&v[1]);
	*--f->sp = v[1];
    case 0:
	break;

    default:
	n = co->nargs - 2;
	f->sp -= n;
	memcpy(f->sp, elts(v[3].array), n * sizeof(Value));
	delLhs(&v[3]);
	FREE(v[3].array->elts);
	v[3].array->elts = (Value *) NULL;
	v[3].array->del();
	delLhs(&v[2]);
	*--f->sp = v[2];
	delLhs(&v[1]);
	*--f->sp = v[1];
	break;
    }

    /* wipe out destructed objects */
    for (n = co->nargs, v = f->sp; n > 0; --n, v++) {
	switch (v->type) {
	case T_OBJECT:
	    if (DESTRUCTED(v)) {
		*v = Value::nil;
	    }
	    break;

	case T_LWOBJECT:
	    o = Dataspace::elts(v->array);
	    if (o->type == T_OBJECT && DESTRUCTED(o)) {
		v->array->del();
		*v = Value::nil;
	    }
	    break;
	}
    }

    co->val[0] = Value::nil;
    n = fcallouts;
    if (n != 0) {
	callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    fcallouts = handle;

    plane->flags |= MOD_CALLOUT;
    return str;
}

/*
 * list all call_outs in an object
 */
Array *Dataspace::listCallouts(Dataspace *data)
{
    uindex n, count, size;
    DCallOut *co;
    Value *v, *v2, *elts;
    Array *list, *a;
    uindex max_args;
    Float flt;

    if (ncallouts == 0) {
	return Array::create(data, 0);
    }
    if (callouts == (DCallOut *) NULL) {
	loadCallouts();
    }

    /* get the number of callouts in this object */
    count = ncallouts;
    for (n = fcallouts; n != 0; n = callouts[n - 1].co_next) {
	--count;
    }
    if (count > Config::arraySize()) {
	return (Array *) NULL;
    }

    list = Array::create(data, count);
    elts = list->elts;
    max_args = Config::arraySize() - 3;

    for (co = callouts; count > 0; co++) {
	if (co->val[0].type == T_STRING) {
	    size = co->nargs;
	    if (size > max_args) {
		/* unlikely, but possible */
		size = max_args;
	    }
	    a = Array::create(data, size + 3L);
	    v = a->elts;

	    /* handle */
	    PUT_INTVAL(v, co - callouts + 1);
	    v++;
	    /* function */
	    PUT_STRVAL(v, co->val[0].string);
	    v++;
	    /* time */
	    if (co->mtime == TIME_INT) {
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
		for (v2 = this->elts(co->val[3].array) + n; n > 0; --n) {
		    *v++ = *--v2;
		}
		*v++ = co->val[2];
		*v++ = co->val[1];
		break;
	    }
	    while (size > 0) {
		(--v)->ref();
		--size;
	    }
	    refImports(a);

	    /* put in list */
	    PUT_ARRVAL(elts, a);
	    elts++;
	    --count;
	}
    }
    CallOut::list(list);

    return list;
}


/*
 * get the variable mapping for an object
 */
unsigned short *Dataspace::varmap(Object **obj, Uint update,
				  unsigned short *nvariables)
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
 * upgrade the dataspace for one object
 */
void Dataspace::upgrade(unsigned int nvar, unsigned short *vmap, Object *tmpl)
{
    Value *v;
    unsigned short n;
    Value *vars;

    /* make sure variables are in memory */
    vars = variable(0);

    /* map variables */
    for (n = nvar, v = ALLOC(Value, n); n > 0; --n) {
	switch (*vmap) {
	case NEW_INT:
	    *v++ = Value::zeroInt;
	    break;

	case NEW_FLOAT:
	    *v++ = Value::zeroFloat;
	    break;

	case NEW_POINTER:
	    *v++ = Value::nil;
	    break;

	default:
	    *v = vars[*vmap];
	    v->ref();
	    v->modified = TRUE;
	    refRhs(v++);
	    break;
	}
	vmap++;
    }
    vars = v - nvar;

    /* deref old values */
    v = variables;
    for (n = nvariables; n > 0; --n) {
	delLhs(v);
	(v++)->del();
    }

    /* replace old with new */
    FREE(variables);
    variables = vars;

    base.flags |= MOD_VARIABLE;
    if (nvariables != nvar) {
	if (svariables != (SValue *) NULL) {
	    FREE(svariables);
	    svariables = (SValue *) NULL;
	}
	nvariables = nvar;
	base.achange++;	/* force rebuild on swapout */
    }

    OBJ(oindex)->upgraded(tmpl);
}

/*
 * upgrade a clone object
 */
void Dataspace::upgradeClone()
{
    Object *obj;
    unsigned short *vmap, nvar;
    Uint update;

    /*
     * the program for the clone was upgraded since last swapin
     */
    obj = OBJ(oindex);
    update = obj->update;
    obj = OBJ(obj->master);
    vmap = varmap(&obj, update, &nvar);
    upgrade(nvar, vmap, obj);
    if (vmap != obj->ctrl->vmap) {
	FREE(vmap);
    }
}

/*
 * upgrade a non-persistent object
 */
Object *Dataspace::upgradeLWO(LWO *lwobj, Object *obj)
{
    ArrRef *a;
    unsigned short n;
    Value *v;
    Uint update;
    unsigned short nvar, *vmap;
    Value *vars;

    a = lwobj->primary;
    update = obj->update;
    vmap = varmap(&obj, (Uint) lwobj->elts[1].number, &nvar);
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
	    *v++ = Value::zeroInt;
	    break;

	case NEW_FLOAT:
	    *v++ = Value::zeroFloat;
	    break;

	case NEW_POINTER:
	    *v++ = Value::nil;
	    break;

	default:
	    *v = vars[*vmap];
	    if (a->arr != (Array *) NULL) {
		a->data->refRhs(v);
	    }
	    v->ref();
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
	    a->data->delLhs(v);
	    (v++)->del();
	}
    } else {
	/* deref old values */
	for (n = lwobj->size - 2; n > 0; --n) {
	    (v++)->del();
	}
    }

    /* replace old with new */
    lwobj->size = nvar + 2;
    FREE(lwobj->elts);
    lwobj->elts = vars;

    return obj;
}

struct ArrImport {
    Array **itab;			/* imported array replacement table */
    Uint itabsz;			/* size of table */
    Uint narr;				/* # of arrays */
};

/*
 * copy imported arrays to current dataspace
 */
void Dataspace::import(ArrImport *imp, Value *val, unsigned short n)
{
    Array *import, *a;

    import = (Array *) NULL;
    for (;;) {
	while (n > 0) {
	    if (T_INDEXED(val->type)) {
		Uint i, j;

		a = val->array;
		if (a->primary->data != this) {
		    /*
		     * imported array
		     */
		    i = a->put(imp->narr);
		    if (i == imp->narr) {
			/*
			 * first time encountered
			 */
			imp->narr++;

			a->trim();

			if (a->refCount == 2) {	/* + 1 for array merge table */
			    /*
			     * move array to new dataspace
			     */
			    a->prev->next = a->next;
			    a->next->prev = a->prev;
			} else {
			    /*
			     * make copy
			     */
			    switch (val->type) {
			    case T_ARRAY:
				a = Array::alloc(a->size);
				break;

			    case T_MAPPING:
				a = Mapping::alloc(a->size);
				break;

			    case T_LWOBJECT:
				a = LWO::alloc(a->size);
				break;
			    }
			    a->tag = val->array->tag;
			    a->objDestrCount = val->array->objDestrCount;

			    if (a->size > 0) {
				/*
				 * copy elements
				 */
				Value::copy(a->elts = ALLOC(Value, a->size),
					    elts(val->array), a->size);
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

			a->primary = &base.alocal;
			if (a->size == 0) {
			    /*
			     * put empty array in dataspace
			     */
			    a->prev = &alist;
			    a->next = alist.next;
			    a->next->prev = a;
			    alist.next = a;
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
		    if (a->trim() || a->elts != (Value *) NULL) {
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
	a->prev = &alist;
	a->next = alist.next;
	a->next->prev = a;
	alist.next = a;

	/* import array elements */
	val = a->elts;
	n = a->size;
    }
}

/*
 * handle exporting of arrays shared by more than one object
 */
void Dataspace::xport()
{
    Dataspace *data;
    Uint n;
    ArrImport imp;

    if (ifirst != (Dataspace *) NULL) {
	imp.itab = ALLOC(Array*, imp.itabsz = 64);

	for (data = ifirst; data != (Dataspace *) NULL; data = data->inext) {
	    if (data->base.imports != 0) {
		data->base.imports = 0;
		Array::merge();
		imp.narr = 0;

		if (data->variables != (Value *) NULL) {
		    data->import(&imp, data->variables, data->nvariables);
		}
		if (data->base.arrays != (ArrRef *) NULL) {
		    ArrRef *a;

		    for (n = data->narrays, a = data->base.arrays; n > 0;
			 --n, a++) {
			if (a->arr != (Array *) NULL) {
			    if (a->arr->trim() ||
				a->arr->elts != (Value *) NULL) {
				data->import(&imp, a->arr->elts, a->arr->size);
			    }
			}
		    }
		}
		if (data->callouts != (DCallOut *) NULL) {
		    DCallOut *co;

		    co = data->callouts;
		    for (n = data->ncallouts; n > 0; --n) {
			if (co->val[0].type == T_STRING) {
			    data->import(&imp, co->val,
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
 * initialize swapped data handling
 */
void Dataspace::init()
{
    dhead = dtail = (Dataspace *) NULL;
    gcdata = (Dataspace *) NULL;
    ndata = 0;
    conv_14 = conv_16 = FALSE;
    convDone = FALSE;
}

/*
 * prepare for conversions
 */
void Dataspace::initConv(bool c14, bool c16)
{
    conv_14 = c14;
    conv_16 = c16;
}

/*
 * snapshot conversion is complete
 */
void Dataspace::converted()
{
    convDone = TRUE;
}

/*
 * Swap out a portion of the control and dataspace blocks in
 * memory.  Return the number of dataspace blocks swapped out.
 */
Sector Dataspace::swapout(unsigned int frag)
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
	    if (data->save(TRUE)) {
		count++;
	    }
	    OBJ(data->oindex)->data = (Dataspace *) NULL;
	    delete data;
	    data = prev;
	}

	Control::swapout(frag);
    }

    /* perform garbage collection for one dataspace */
    if (gcdata != (Dataspace *) NULL) {
	if (gcdata->save(frag != 0) && frag != 0) {
	    count++;
	}
	gcdata = gcdata->gcnext;
    }

    return count;
}

/*
 * upgrade all obj and all objects cloned from obj that have
 * dataspaces in memory
 */
void Dataspace::upgradeMemory(Object *tmpl, Object *newob)
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
		data->upgrade(nvar, vmap, tmpl);
	    }
	    data->ctrl->ndata--;
	    data->ctrl = newob->ctrl;
	    data->ctrl->ndata++;
	}
    }
}

/*
 * restore an object
 */
void Dataspace::restoreObject(Object *obj, Uint instance, Uint *counttab,
			      bool cactive, bool dactive)
{
    Control *ctrl;
    Dataspace *data;

    if (!convDone) {
	ctrl = Control::restore(obj, instance, Swap::conv);
	data = Dataspace::restore(obj, counttab, Swap::conv);
    } else {
	ctrl = Control::restore(obj, instance, Swap::dreadv);
	data = Dataspace::restore(obj, counttab, Swap::dreadv);
    }

    if (!cactive && ctrl != (Control *) NULL) {
	ctrl->deref();
    }
    if (!dactive && data != (Dataspace *) NULL) {
	data->deref();
    }
}
