/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
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

# define INCLUDE_FILE_IO
# define INCLUDE_CTYPE
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"

struct objplane;

struct objpatch : public ChunkAllocated {
    objplane *plane;			/* plane that patch is on */
    objpatch *prev;			/* previous patch */
    objpatch *next;			/* next in linked list */
    Object obj;				/* new object value */
};

# define OPCHUNKSZ		32

class optable : public Allocated {
public:
    /*
     * initialize objpatch table
     */
    optable() {
	memset(&op, '\0', OBJPATCHHTABSZ * sizeof(objpatch*));
    }

    Chunk<objpatch, OPCHUNKSZ> chunk; 	/* object patch chunk */
    objpatch *op[OBJPATCHHTABSZ];	/* hash table of object patches */
};

struct objplane {
    Hashtab *htab;		/* object name hash table */
    optable *optab;		/* object patch table */
    uintptr_t clean;		/* list of objects to clean */
    uintptr_t upgrade;		/* list of upgrade objects */
    uindex destruct;		/* destructed object list */
    uindex free;		/* free object list */
    uindex nobjects;		/* number of objects in object table */
    uindex nfreeobjs;		/* number of objects in free list */
    Uint ocount;		/* object creation count */
    bool swap, dump, incr, stop, boot; /* state vars */
    objplane *prev;		/* previous object plane */
};

Object *Object::otable;		/* object table */
Uint *Object::ocmap;		/* object change map */
bool Object::obase;		/* object base plane flag */
bool Object::swap, Object::dump, Object::incr, Object::stop, Object::boot;
				/* global state vars */
static bool rcount;		/* object counts recalculated? */
static uindex otabsize;		/* size of object table */
static uindex uobjects;		/* objects to check for upgrades */
static objplane baseplane;	/* base object plane */
static objplane *oplane;	/* current object plane */
static Uint *omap;		/* object dump bitmap */
static Uint *counttab;		/* object count table */
static Object *upgradeList;	/* list of upgraded objects */
static uindex ndobject, dobject;/* objects to copy */
static uindex mobjects;		/* max objects to copy */
static uindex dchunksz;		/* copy chunk size */
static Uint dinterval;		/* copy interval */
static Uint dtime;		/* time copying started */
Uint Object::odcount;		/* objects destructed count */

/*
 * initialize the object tables
 */
void Object::init(unsigned int n, Uint interval)
{
    otable = ALLOC(Object, otabsize = n);
    memset(otable, '\0', n * sizeof(Object));
    ocmap = ALLOC(Uint, BMAP(n));
    memset(ocmap, '\0', BMAP(n) * sizeof(Uint));
    for (n = 4; n < otabsize; n <<= 1) ;
    baseplane.htab = Hashtab::create(n >> 2, OBJHASHSZ, FALSE);
    baseplane.optab = (optable *) NULL;
    baseplane.upgrade = baseplane.clean = OBJ_NONE;
    baseplane.destruct = baseplane.free = OBJ_NONE;
    baseplane.nobjects = 0;
    baseplane.nfreeobjs = 0;
    baseplane.ocount = 3;
    baseplane.swap = baseplane.dump = baseplane.incr = baseplane.stop =
		     baseplane.boot = FALSE;
    oplane = &baseplane;
    omap = ALLOC(Uint, BMAP(n));
    memset(omap, '\0', BMAP(n) * sizeof(Uint));
    counttab = ALLOC(Uint, n);
    upgradeList = (Object *) NULL;
    uobjects = ndobject = mobjects = 0;
    dinterval = ((interval + 1) * 19) / 20;
    odcount = 1;
    obase = TRUE;
    rcount = TRUE;
}


/*
 * NAME:	objpatch->new()
 * DESCRIPTION:	create a new object patch
 */
static objpatch *op_new(objplane *plane, objpatch **o, objpatch *prev, Object *obj)
{
    objpatch *op;

    /* allocate */
    op = chunknew (plane->optab->chunk) objpatch;

    /* initialize */
    op->plane = plane;
    op->prev = prev;
    op->obj = *obj;

    /* add to hash table */
    op->next = *o;
    return *o = op;
}

/*
 * NAME:	objpatch->del()
 * DESCRIPTION:	delete an Object patch
 */
static void op_del(objplane *plane, objpatch **o)
{
    objpatch *op;

    /* remove from hash table */
    op = *o;
    *o = op->next;

    /* add to free list */
    delete op;
}


/*
 * access to object from atomic code
 */
Object *Object::access(unsigned int index, int access)
{
    objpatch *o, **oo;
    Object *obj;

    if (BTST(ocmap, index)) {
	/*
	 * object already patched
	 */
	oo = &oplane->optab->op[index % OBJPATCHHTABSZ];
	for (o = *oo; o->obj.index != index; o = o->next) ;
	if (access == OACC_READ || o->plane == oplane) {
	    return &o->obj;
	}

	/* create new patch on current plane */
	o = op_new(oplane, oo, o, obj = &o->obj);
	if (obj->name != (char *) NULL && obj->count != 0) {
	    *oplane->htab->lookup(obj->name, FALSE) = &o->obj;
	}
	return &o->obj;
    } else {
	/*
	 * first patch for object
	 */
	BSET(ocmap, index);
	if (oplane->optab == (optable *) NULL) {
	    oplane->optab = new optable;
	}
	oo = &oplane->optab->op[index % OBJPATCHHTABSZ];
	obj = &op_new(oplane, oo, (objpatch *) NULL, OBJ(index))->obj;
	if (obj->name != (char *) NULL) {
	    char *name;
	    Hashtab::Entry **h;

	    /* copy object name to higher plane */
	    strcpy(name = ALLOC(char, strlen(obj->name) + 1), obj->name);
	    obj->name = name;
	    if (obj->count != 0) {
		if (oplane->htab == (Hashtab *) NULL) {
		    oplane->htab = Hashtab::create(OBJPATCHHTABSZ, OBJHASHSZ,
						   FALSE);
		}
		h = oplane->htab->lookup(name, FALSE);
		obj->next = *h;
		*h = obj;
	    }
	}
	return obj;
    }
}

/*
 * read access to object in patch table
 */
Object *Object::oread(unsigned int index)
{
    return access(index, OACC_READ);
}

/*
 * write access to object in atomic code
 */
Object *Object::owrite(unsigned int index)
{
    return access(index, OACC_MODIFY);
}

/*
 * check if there's space for another object
 */
bool Object::space()
{
    return (oplane->free != OBJ_NONE) ? TRUE : (oplane->nobjects != otabsize);
}

/*
 * NAME:	Object->alloc()
 * DESCRIPTION:	allocate a new object
 */
static Object *o_alloc()
{
    uindex n;
    Object *obj;

    if (oplane->free != OBJ_NONE) {
	/* get space from free object list */
	n = oplane->free;
	obj = OBJW(n);
	oplane->free = obj->prev;
	--oplane->nfreeobjs;
    } else {
	/* use new space in object table */
	if (oplane->nobjects == otabsize) {
	    error("Too many objects");
	}
	n = oplane->nobjects++;
	obj = OBJW(n);
    }

    OBJ(n)->index = OBJ_NONE;
    obj->index = n;
    obj->ctrl = (Control *) NULL;
    obj->data = (Dataspace *) NULL;
    obj->cfirst = SW_UNUSED;
    obj->dfirst = SW_UNUSED;
    counttab[n] = 0;

    return obj;
}


/*
 * create a new object plane
 */
void Object::newPlane()
{
    objplane *p;

    p = ALLOC(objplane, 1);

    if (oplane->optab == (optable *) NULL) {
	p->htab = (Hashtab *) NULL;
	p->optab = (optable *) NULL;
    } else {
	p->htab = oplane->htab;
	p->optab = oplane->optab;
    }
    p->clean = oplane->clean;
    p->upgrade = oplane->upgrade;
    p->destruct = oplane->destruct;
    p->free = oplane->free;
    p->nobjects = oplane->nobjects;
    p->nfreeobjs = oplane->nfreeobjs;
    p->ocount = oplane->ocount;
    p->swap = oplane->swap;
    p->dump = oplane->dump;
    p->incr = oplane->incr;
    p->stop = oplane->stop;
    p->boot = oplane->boot;
    p->prev = oplane;
    oplane = p;

    obase = FALSE;
}

/*
 * commit the current object plane
 */
void Object::commitPlane()
{
    objplane *prev;
    objpatch **t, **o, *op;
    int i;
    Object *obj;


    prev = oplane->prev;
    if (oplane->optab != (optable *) NULL) {
	for (i = OBJPATCHHTABSZ, t = oplane->optab->op; --i >= 0; t++) {
	    o = t;
	    while (*o != (objpatch *) NULL && (*o)->plane == oplane) {
		op = *o;
		if (op->prev != (objpatch *) NULL) {
		    obj = &op->prev->obj;
		} else {
		    obj = OBJ(op->obj.index);
		}
		if (op->obj.count == 0 && obj->count != 0) {
		    /* remove object from stackframe above atomic function */
		    i_odest(cframe, obj);
		}

		if (prev == &baseplane) {
		    /*
		     * commit to base plane
		     */
		    if (op->obj.name != (char *) NULL) {
			Hashtab::Entry **h;

			if (obj->name == (char *) NULL) {
			    char *name;

			    /*
			     * make object name static
			     */
			    m_static();
			    name = ALLOC(char, strlen(op->obj.name) + 1);
			    m_dynamic();
			    strcpy(name, op->obj.name);
			    FREE(op->obj.name);
			    op->obj.name = name;
			    if (op->obj.count != 0) {
				/* put name in static hash table */
				h = prev->htab->lookup(name, FALSE);
				op->obj.next = *h;
				*h = obj;
			    }
			} else {
			    /*
			     * same name
			     */
			    FREE(op->obj.name);
			    op->obj.name = obj->name;
			    if (op->obj.count != 0) {
				/* keep this name */
				op->obj.next = obj->next;
			    } else if (obj->count != 0) {
				/* remove from hash table */
				h = prev->htab->lookup(obj->name, FALSE);
				if (*h != obj) {
				    /* new object was compiled also */
				    h = &(*h)->next;
				}
				*h = obj->next;
			    }
			}
		    }
		    if (obj->count != 0) {
			op->obj.update = obj->update;
		    }
		    BCLR(ocmap, op->obj.index);
		    *obj = op->obj;
		    op_del(oplane, o);
		} else {
		    /*
		     * commit to previous plane
		     */
		    if (op->prev == (objpatch *) NULL ||
			op->prev->plane != prev) {
			/* move to previous plane */
			op->plane = prev;
			o = &op->next;
		    } else {
			/*
			 * copy onto previous plane
			 */
			if (op->obj.name != (char *) NULL && op->obj.count != 0)
			{
			    /* move name to previous plane */
			    *oplane->htab->lookup(op->obj.name, FALSE) = obj;
			}
			*obj = op->obj;
			op_del(oplane, o);
		    }
		}
	    }
	}

	if (prev != &baseplane) {
	    prev->htab = oplane->htab;
	    prev->optab = oplane->optab;
	} else {
	    if (oplane->htab != (Hashtab *) NULL) {
		delete oplane->htab;
	    }
	    delete oplane->optab;
	}
    }

    prev->clean = oplane->clean;
    prev->upgrade = oplane->upgrade;
    prev->destruct = oplane->destruct;
    prev->free = oplane->free;
    prev->nobjects = oplane->nobjects;
    prev->nfreeobjs = oplane->nfreeobjs;
    prev->ocount = oplane->ocount;
    prev->swap = oplane->swap;
    prev->dump = oplane->dump;
    prev->incr = oplane->incr;
    prev->stop = oplane->stop;
    prev->boot = oplane->boot;
    FREE(oplane);
    oplane = prev;

    obase = (prev == &baseplane);
}

/*
 * discard the current object plane without committing it
 */
void Object::discardPlane()
{
    objpatch **o, *op;
    int i;
    Object *obj, *clist;
    objplane *p;

    if (oplane->optab != (optable *) NULL) {
	clist = (Object *) NULL;
	for (i = OBJPATCHHTABSZ, o = oplane->optab->op; --i >= 0; o++) {
	    while (*o != (objpatch *) NULL && (*o)->plane == oplane) {
		op = *o;
		if (op->prev != (objpatch *) NULL) {
		    obj = &op->prev->obj;
		} else {
		    BCLR(ocmap, op->obj.index);
		    obj = OBJ(op->obj.index);
		}

		if (op->obj.name != (char *) NULL) {
		    if (obj->name == (char *) NULL ||
			op->prev == (objpatch *) NULL) {
			/*
			 * remove new name
			 */
			if (op->obj.count != 0) {
			    /* remove from hash table */
			    *oplane->htab->lookup(op->obj.name, FALSE)
								= op->obj.next;
			}
			FREE(op->obj.name);
		    } else {
			Hashtab::Entry **h;

			if (op->obj.count != 0) {
			    /*
			     * move name to previous plane
			     */
			    h = oplane->htab->lookup(obj->name, FALSE);
			    obj->next = op->obj.next;
			    *h = obj;
			} else if (obj->count != 0) {
			    /*
			     * put name back in hashtable
			     */
			    h = oplane->htab->lookup(obj->name, FALSE);
			    obj->next = *h;
			    *h = obj;
			}
		    }
		}

		if (obj->index == OBJ_NONE) {
		    /*
		     * discard newly created object
		     */
		    if ((op->obj.flags & O_MASTER) &&
			op->obj.ctrl != (Control *) NULL) {
			op->obj.next = clist;
			clist = &op->obj;
		    }
		    if (op->obj.data != (Dataspace *) NULL) {
			/* discard new data block */
			d_del_dataspace(op->obj.data);
		    }
		    obj->index = op->obj.index;
		} else {
		    /* pass on control block and dataspace */
		    obj->ctrl = op->obj.ctrl;
		    if (obj->data != op->obj.data) {
			if (obj->dfirst != SW_UNUSED) {
			    obj->data = op->obj.data;
			} else {
			    /* discard new initialized data block */
			    d_del_dataspace(op->obj.data);
			}
		    }
		}
		op_del(oplane, o);
	    }
	}

	/* discard new control blocks */
	while (clist != (Object *) NULL) {
	    obj = clist;
	    clist = (Object *) obj->next;
	    d_del_control(obj->ctrl);
	}
    }

    p = oplane;
    oplane = p->prev;
    if (p->optab != (optable *) NULL) {
	if (oplane != &baseplane) {
	    oplane->htab = p->htab;
	    oplane->optab = p->optab;
	} else {
	    if (p->htab != (Hashtab *) NULL) {
		delete p->htab;
	    }
	    delete p->optab;
	}
    }
    FREE(p);

    obase = (oplane == &baseplane);
}


/*
 * create a new object
 */
Object *Object::create(char *name, Control *ctrl)
{
    Object *o;
    dinherit *inh;
    int i;
    Hashtab::Entry **h;

    /* allocate object */
    o = o_alloc();

    /* put object in object name hash table */
    if (obase) {
	m_static();
    }
    o->name = strcpy(ALLOC(char, strlen(name) + 1), name);
    if (obase) {
	m_dynamic();
    } else if (oplane->htab == (Hashtab *) NULL) {
	oplane->htab = Hashtab::create(OBJPATCHHTABSZ, OBJHASHSZ, FALSE);
    }
    h = oplane->htab->lookup(name, FALSE);
    o->next = *h;
    *h = o;

    o->flags = O_MASTER;
    o->cref = 0;
    o->prev = OBJ_NONE;
    o->count = ++oplane->ocount;
    o->update = 0;
    o->ctrl = ctrl;
    ctrl->inherits[ctrl->ninherits - 1].oindex = ctrl->oindex = o->index;

    /* add reference to all inherited objects */
    o->ref = 0;	/* increased to 1 in following loop */
    for (i = ctrl->ninherits, inh = ctrl->inherits; i > 0; --i, inh++) {
	OBJW(inh->oindex)->ref++;
    }

    return o;
}

/*
 * clone an object
 */
Object *Object::clone()
{
    Object *o;

    /* allocate object */
    o = o_alloc();
    o->name = (char *) NULL;
    o->flags = 0;
    o->count = ++oplane->ocount;
    o->update = update;
    o->master = index;

    /* add reference to master object */
    cref++;
    ref++;

    return o;
}

/*
 * create light-weight instance of object
 */
void Object::lightWeight()
{
    flags |= O_LWOBJ;
}

/*
 * NAME:	Object->delete()
 * DESCRIPTION:	the last reference to a master object was removed
 */
static void o_delete(Object *o, Frame *f)
{
    Control *ctrl;
    dinherit *inh;
    int i;

    ctrl = (O_UPGRADING(o)) ? OBJR(o->prev)->ctrl : o->control();

    /* put in deleted list */
    o->cref = oplane->destruct;
    oplane->destruct = o->index;

    /* callback to the system */
    PUSH_STRVAL(f, str_new(NULL, strlen(o->name) + 1L));
    f->sp->u.string->text[0] = '/';
    strcpy(f->sp->u.string->text + 1, o->name);
    PUSH_INTVAL(f, ctrl->compiled);
    PUSH_INTVAL(f, o->index);
    if (i_call_critical(f, "remove_program", 3, TRUE)) {
	i_del_value(f->sp++);
    }

    /* remove references to inherited objects too */
    for (i = ctrl->ninherits, inh = ctrl->inherits; --i > 0; inh++) {
	o = OBJW(inh->oindex);
	if (--(o->ref) == 0) {
	    o_delete(o, f);
	}
    }
}

/*
 * upgrade an object to a new program
 */
void Object::upgrade(Control *ctrl, Frame *f)
{
    Object *obj;
    dinherit *inh;
    int i;

    /* allocate upgrade object */
    obj = o_alloc();
    obj->name = (char *) NULL;
    obj->flags = O_MASTER;
    obj->count = 0;
    obj->update = update;
    obj->ctrl = ctrl;
    ctrl->inherits[ctrl->ninherits - 1].oindex = obj->cref = index;

    /* add reference to inherited objects */
    for (i = ctrl->ninherits, inh = ctrl->inherits; --i > 0; inh++) {
	OBJW(inh->oindex)->ref++;
    }

    /* add to upgrades list */
    obj->next = (Hashtab::Entry *) oplane->upgrade;
    oplane->upgrade = obj->index;

    /* mark as upgrading */
    cref += 2;
    obj->prev = prev;
    prev = obj->index;

    /* remove references to old inherited objects */
    ctrl = control();
    for (i = ctrl->ninherits, inh = ctrl->inherits; --i > 0; inh++) {
	obj = OBJW(inh->oindex);
	if (--(obj->ref) == 0) {
	    o_delete(obj, f);
	}
    }
}

/*
 * an object has been upgraded
 */
void Object::upgraded(Object *tmpl)
{
# ifdef DEBUG
    if (count == 0) {
	fatal("upgrading destructed object");
    }
# endif
    if (!(flags & O_MASTER)) {
	update = OBJ(master)->update;
    }
    if (tmpl->count == 0) {
	tmpl->next = upgradeList;
	upgradeList = tmpl;
    }
    tmpl->count++;
}

/*
 * delete an object
 */
void Object::del(Frame *f)
{
    if (count == 0) {
	/* can happen if object selfdestructs in close()-on-destruct */
	error("Destructing destructed object");
    }
    i_odest(f, this);	/* wipe out occurrences on the stack */
    if (data == (Dataspace *) NULL && dfirst != SW_UNUSED) {
	dataspace();	/* load dataspace now */
    }
    count = 0;
    odcount++;

    if (flags & O_MASTER) {
	/* remove from object name hash table */
	*oplane->htab->lookup(name, FALSE) = next;

	if (--ref == 0 && !O_UPGRADING(this)) {
	    o_delete(this, f);
	}
    } else {
	Object *master;

	master = OBJW(this->master);
	master->cref--;
	if (--(master->ref) == 0 && !O_UPGRADING(master)) {
	    o_delete(master, f);
	}
    }

    /* put in clean list */
    next = (Hashtab::Entry *) oplane->clean;
    oplane->clean = index;
}


/*
 * return the name of an object
 */
const char *Object::objName(char *name)
{
    if (this->name != (char *) NULL) {
	return this->name;
    } else {
	char num[12];
	char *p;
	uindex n;

	/*
	 * return the name of the master object with the index appended
	 */
	n = index;
	p = num + 11;
	*p = '\0';
	do {
	    *--p = '0' + n % 10;
	    n /= 10;
	} while (n != 0);
	*--p = '#';

	strcpy(name, OBJR(master)->name);
	strcat(name, p);
	return name;
    }
}

/*
 * return the base name of a builtin type
 */
const char *Object::builtinName(Int type)
{
    /*
     * builtin types have names like: /builtin/type#-1
     * the base name is then: builtin/type
     */
    switch (type) {
# ifdef CLOSURES
    case BUILTIN_FUNCTION:
	return BIPREFIX "function";
# endif

# ifdef DEBUG
    default:
	fatal("unknown builtin type %d", type);
# endif
    }
    return NULL;
}

/*
 * find an object by name
 */
Object *Object::find(char *name, int access)
{
    Object *o;
    unsigned long number;
    char *hash;

    hash = strchr(name, '#');
    if (hash != (char *) NULL) {
	char *p;
	Object *m;

	/*
	 * Look for a cloned object, which cannot be found directly in the
	 * object name hash table.
	 * The name must be of the form filename#1234, where 1234 is the
	 * decimal representation of the index in the object table.
	 */
	p = hash + 1;
	if (*p == '\0' || (p[0] == '0' && p[1] != '\0')) {
	    /* don't accept "filename#" or "filename#01" */
	    return (Object *) NULL;
	}

	/* convert the string to a number */
	number = 0;
	do {
	    if (!isdigit(*p)) {
		return (Object *) NULL;
	    }
	    number = number * 10 + *p++ - '0';
	    if (number >= oplane->nobjects) {
		return (Object *) NULL;
	    }
	} while (*p != '\0');

	o = OBJR(number);
	if (o->count == 0 || (o->flags & O_MASTER) ||
	    strncmp(name, (m=OBJR(o->master))->name, hash-name) != 0 ||
	    m->name[hash - name] != '\0') {
	    /*
	     * no entry, not a clone, or object name doesn't match
	     */
	    return (Object *) NULL;
	}
    } else {
	/* look it up in the hash table */
	if (oplane->htab == (Hashtab *) NULL ||
	    (o = (Object *) *oplane->htab->lookup(name, TRUE)) ==
							    (Object *) NULL) {
	    if (oplane != &baseplane) {
		o = (Object *) *baseplane.htab->lookup(name, FALSE);
		if (o != (Object *) NULL) {
		    number = o->index;
		    o = (access == OACC_READ)? OBJR(number) : OBJW(number);
		    if (o->count != 0) {
			return o;
		    }
		}
	    }
	    return (Object *) NULL;
	}
	number = o->index;
    }

    return (access == OACC_READ) ? o : OBJW(number);
}

/*
 * NAME:	Object->restore_object()
 * DESCRIPTION:	restore an object from the snapshot
 */
static void o_restore_obj(Object *obj, bool cactive, bool dactive)
{
    BCLR(omap, obj->index);
    --ndobject;
    d_restore_obj(obj, (rcount) ? counttab : (Uint *) NULL, cactive, dactive);
}

/*
 * return the control block for an object
 */
Control *Object::control()
{
    Object *o;

    o = this;
    if (!(o->flags & O_MASTER)) {
	/* get control block of master object */
	o = OBJR(o->master);
    }
    if (o->ctrl == (Control *) NULL) {
	if (BTST(omap, o->index)) {
	    o_restore_obj(o, TRUE, FALSE);
	} else {
	    o->ctrl = d_load_control(o);
	}
    } else {
	d_ref_control(o->ctrl);
    }
    return ctrl = o->ctrl;
}

/*
 * return the dataspace block for an object
 */
Dataspace *Object::dataspace()
{
    if (data == (Dataspace *) NULL) {
	if (BTST(omap, index)) {
	    o_restore_obj(this, TRUE, TRUE);
	} else {
	    data = d_load_dataspace(this);
	}
    } else {
	d_ref_dataspace(data);
    }
    return data;
}

/*
 * NAME:	Object->clean_upgrades()
 * DESCRIPTION:	clean up upgrade templates
 */
static void o_clean_upgrades()
{
    Object *o, *next;
    Uint count;

    while ((next=upgradeList) != (Object *) NULL) {
	upgradeList = (Object *) next->next;

	count = next->count;
	next->count = 0;
	do {
	    o = next;
	    next = OBJ(o->cref);
	    o->ref -= count;
	    if (o->ref == 0) {
# ifdef DEBUG
		if (o->prev != OBJ_NONE) {
		    fatal("removing issue in middle of list");
		}
# endif
		/* remove from template list */
		if (next->prev == o->index) {
		    next->prev = OBJ_NONE;
		} else {
		    OBJ(next->prev)->prev = OBJ_NONE;
		}

		/* put in delete list */
		o->cref = baseplane.destruct;
		baseplane.destruct = o->index;
	    }
	} while (next->name == (char *) NULL);
    }
}

/*
 * NAME:	Object->purge_upgrades()
 * DESCRIPTION:	purge the LW dross from upgrade templates
 */
static bool o_purge_upgrades(Object *o)
{
    bool purged;

    purged = FALSE;
    while (o->prev != OBJ_NONE && ((o=OBJ(o->prev))->flags & O_LWOBJ)) {
	o->flags &= ~O_LWOBJ;
	if (--o->ref == 0) {
	    o->cref = baseplane.destruct;
	    baseplane.destruct = o->index;
	    purged = TRUE;
	}
    }

    return purged;
}

/*
 * clean up the object table
 */
void Object::clean()
{
    Object *o;

    while (baseplane.clean != OBJ_NONE) {
	o = OBJ(baseplane.clean);
	baseplane.clean = (uintptr_t) o->next;

	/* free dataspace block (if it exists) */
	if (o->data != (Dataspace *) NULL) {
	    d_del_dataspace(o->data);
	}

	if (o->flags & O_MASTER) {
	    /* remove possible upgrade templates */
	    if (o_purge_upgrades(o)) {
		o->prev = OBJ_NONE;
	    }
	} else {
	    Object *tmpl;

	    /* check if clone still had to be upgraded */
	    tmpl = OBJW(o->master);
	    if (tmpl->update != o->update) {
		/* non-upgraded clone of old issue */
		do {
		    tmpl = OBJW(tmpl->prev);
		} while (tmpl->update != o->update);

		if (tmpl->count == 0) {
		    tmpl->next = upgradeList;
		    upgradeList = tmpl;
		}
		tmpl->count++;
	    }

	    /* put clone in free list */
	    o->prev = baseplane.free;
	    baseplane.free = o->index;
	    baseplane.nfreeobjs++;
	}
    }

    o_clean_upgrades();		/* 1st time */

    while (baseplane.upgrade != OBJ_NONE) {
	Object *up;
	Control *ctrl;

	o = OBJ(baseplane.upgrade);
	baseplane.upgrade = (uintptr_t) o->next;

	up = OBJ(o->cref);
	if (up->ref == 0) {
	    /* no more instances of object around */
	    o->cref = baseplane.destruct;
	    baseplane.destruct = o->index;
	} else {
	    /* upgrade objects */
	    up->cref -= 2;
	    o->ref = up->cref;
	    if (up->flags & O_LWOBJ) {
		o->flags |= O_LWOBJ;
		o->ref++;
	    }
	    if (up->count != 0 && O_HASDATA(up)) {
		o->ref++;
	    }
	    ctrl = up->ctrl;

	    if (o->ctrl->vmapsize != 0 && o->ref != 0) {
		/*
		 * upgrade variables
		 */
		if (o->prev != OBJ_NONE) {
		    OBJ(o->prev)->cref = o->index;
		}

		if (o->ref > (Uint) (up->count != 0)) {
		    up->update++;
		}
		if (up->count != 0 && up->data == (Dataspace *) NULL &&
		    up->dfirst != SW_UNUSED) {
		    /* load dataspace (with old control block) */
		    up->data = d_load_dataspace(up);
		}
	    } else {
		/* no variable upgrading */
		up->prev = o->prev;
		o->cref = baseplane.destruct;
		baseplane.destruct = o->index;
	    }

	    /* swap control blocks */
	    up->ctrl = o->ctrl;
	    up->ctrl->oindex = up->index;
	    o->ctrl = ctrl;
	    ctrl->oindex = o->index;
	    o->cfirst = up->cfirst;
	    up->cfirst = SW_UNUSED;

	    /* swap vmap back to template */
	    ctrl->vmap = up->ctrl->vmap;
	    ctrl->vmapsize = up->ctrl->vmapsize;
	    if (ctrl->vmapsize != 0) {
		ctrl->flags |= CTRL_VARMAP;
	    }
	    up->ctrl->vmap = (unsigned short *) NULL;
	    up->ctrl->vmapsize = 0;

	    if (ctrl->ndata != 0) {
		/* upgrade all dataspaces in memory */
		d_upgrade_mem(o, up);
	    }
	}
    }

    o_clean_upgrades();		/* 2nd time */

    while (baseplane.destruct != OBJ_NONE) {
	o = OBJ(baseplane.destruct);
	baseplane.destruct = o->cref;

	/* free control block */
	d_del_control(o->control());

	if (o->name != (char *) NULL) {
	    /* free object name */
	    FREE(o->name);
	    o->name = (char *) NULL;
	}
	o->ref = 0;

	/* put object in free list */
	o->prev = baseplane.free;
	baseplane.free = o->index;
	baseplane.nfreeobjs++;
    }

    swap = baseplane.swap;
    dump = baseplane.dump;
    incr = baseplane.incr;
    stop = baseplane.stop;
    boot = baseplane.boot;
    baseplane.swap = baseplane.dump = baseplane.incr = FALSE;
}

/*
 * return the number of objects in use
 */
uindex Object::ocount()
{
    return oplane->nobjects - oplane->nfreeobjs;
}

/*
 * return the number of objects left to copy
 */
uindex Object::dobjects()
{
    return ndobject;
}


struct dump_header {
    uindex free;	/* free object list */
    uindex nobjects;	/* # objects */
    uindex nfreeobjs;	/* # free objects */
    Uint onamelen;	/* length of all object names */
};

static char dh_layout[] = "uuui";

struct map_header {
    uindex nctrl;	/* objects left to copy */
    uindex ndata;
    uindex cobject;	/* object to copy */
    uindex dobject;
    Uint count;		/* object count */
};

static char mh_layout[] = "uuuui";

# define CHUNKSZ	16384

/*
 * sweep through the object table after a dump or restore
 */
void Object::sweep(uindex n)
{
    Object *obj;

    uobjects = n;
    dobject = 0;
    for (obj = otable; n > 0; obj++, --n) {
	if (obj->count != 0) {
	    if (obj->cfirst != SW_UNUSED || obj->dfirst != SW_UNUSED) {
		BSET(omap, obj->index);
		ndobject++;
	    }
	} else if ((obj->flags & O_MASTER) && obj->ref != 0 &&
		   obj->cfirst != SW_UNUSED) {
	    BSET(omap, obj->index);
	    ndobject++;
	}
    }
    mobjects = ndobject;
}

/*
 * update object counts
 */
Uint Object::recount(uindex n)
{
    Uint count, *ct;
    Object *obj;

    count = 3;
    for (obj = otable, ct = counttab; n > 0; obj++, ct++, --n) {
	if (obj->count != 0) {
	    *ct = obj->count;
	    obj->count = count++;
	} else {
	    *ct = 2;
	}
    }

    odcount = 1;
    rcount = TRUE;
    return count;
}

/*
 * save the object table
 */
bool Object::save(int fd, bool incr)
{
    uindex i;
    Object *o;
    unsigned int len, buflen;
    dump_header dh;
    map_header mh;
    char buffer[CHUNKSZ];

    /* prepare header */
    dh.free = baseplane.free;
    dh.nobjects = baseplane.nobjects;
    dh.nfreeobjs = baseplane.nfreeobjs;
    dh.onamelen = 0;
    for (i = baseplane.nobjects, o = otable; i > 0; --i, o++) {
	if (o->name != (char *) NULL) {
	    dh.onamelen += strlen(o->name) + 1;
	}
    }

    /* write header and objects */
    if (!sw_write(fd, &dh, sizeof(dump_header)) ||
	!sw_write(fd, otable, baseplane.nobjects * sizeof(Object))) {
	return FALSE;
    }

    /* write object names */
    buflen = 0;
    for (i = baseplane.nobjects, o = otable; i > 0; --i, o++) {
	if (o->name != (char *) NULL) {
	    len = strlen(o->name) + 1;
	    if (buflen + len > CHUNKSZ) {
		if (!sw_write(fd, buffer, buflen)) {
		    return FALSE;
		}
		buflen = 0;
	    }
	    memcpy(buffer + buflen, o->name, len);
	    buflen += len;
	}
    }
    if (buflen != 0 && !sw_write(fd, buffer, buflen)) {
	return FALSE;
    }

    if (ndobject != 0) {
	/*
	 * partial snapshot: write bitmap and counts
	 */
	mh.nctrl = ndobject;
	mh.ndata = ndobject;
	mh.cobject = dobject;
	mh.dobject = dobject;
	mh.count = 0;
	if (rcount) {
	    mh.count = baseplane.ocount;
	}
	if (!sw_write(fd, &mh, sizeof(map_header)) ||
	    !sw_write(fd, omap + BOFF(dobject),
		      (BMAP(dh.nobjects) - BOFF(dobject)) * sizeof(Uint)) ||
	    !sw_write(fd, omap + BOFF(dobject),
		      (BMAP(dh.nobjects) - BOFF(dobject)) * sizeof(Uint)) ||
	    (mh.count != 0 &&
	     !sw_write(fd, counttab, dh.nobjects * sizeof(Uint)))) {
	    return FALSE;
	}
    }

    if (!incr) {
	sweep(baseplane.nobjects);
	baseplane.ocount = recount(baseplane.nobjects);
    }

    return TRUE;
}

/*
 * restore the object table
 */
void Object::restore(int fd, bool part)
{
    uindex i;
    Object *o;
    Uint len, buflen, count;
    char *p;
    dump_header dh;
    char buffer[CHUNKSZ];

    p = NULL;

    /* read header and object table */
    conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);

    if (dh.nobjects > otabsize) {
	error("Too many objects in restore file (%u)", dh.nobjects);
    }

    conf_dread(fd, (char *) otable, OBJ_LAYOUT, (Uint) dh.nobjects);
    baseplane.free = dh.free;
    baseplane.nobjects = dh.nobjects;
    baseplane.nfreeobjs = dh.nfreeobjs;

    /* read object names */
    buflen = 0;
    for (i = 0, o = otable; i < baseplane.nobjects; i++, o++) {
	if (o->name != (char *) NULL) {
	    /*
	     * restore name
	     */
	    if (buflen == 0 ||
		(char *) memchr(p, '\0', buflen) == (char *) NULL) {
		/* move remainder to beginning, and refill buffer */
		if (buflen != 0) {
		    memcpy(buffer, p, buflen);
		}
		len = (dh.onamelen > CHUNKSZ - buflen) ?
		       CHUNKSZ - buflen : dh.onamelen;
		if (P_read(fd, buffer + buflen, len) != len) {
		    fatal("cannot restore object names");
		}
		dh.onamelen -= len;
		buflen += len;
		p = buffer;
	    }
	    m_static();
	    o->name = strcpy(ALLOC(char, len = strlen(p) + 1), p);
	    m_dynamic();

	    if (o->count != 0) {
		Hashtab::Entry **h;

		/* add name to lookup table */
		h = baseplane.htab->lookup(p, FALSE);
		o->next = *h;
		*h = o;
	    }
	    p += len;
	    buflen -= len;
	}

	if (o->count != 0) {
	    /* there are no user or editor objects after a restore */
	    if ((o->flags & O_SPECIAL) != O_SPECIAL) {
		o->flags &= ~O_SPECIAL;
	    }
	}
    }

    sweep(baseplane.nobjects);

    if (part) {
	map_header mh;
	off_t offset;
	Uint *cmap, *dmap;
	uindex nctrl, ndata;

	conf_dread(fd, (char *) &mh, mh_layout, (Uint) 1);
	nctrl = mh.nctrl;
	ndata = mh.ndata;
	count = mh.count;

	cmap = dmap = (Uint *) NULL;
	if (nctrl != 0) {
	    cmap = ALLOC(Uint, BMAP(dh.nobjects));
	    memset(cmap, '\0', BMAP(dh.nobjects) * sizeof(Uint));
	    conf_dread(fd, (char *) (cmap + BOFF(mh.cobject)), "i",
		       BMAP(dh.nobjects) - BOFF(mh.cobject));
	}
	if (ndata != 0) {
	    dmap = ALLOC(Uint, BMAP(dh.nobjects));
	    memset(dmap, '\0', BMAP(dh.nobjects) * sizeof(Uint));
	    conf_dread(fd, (char *) (dmap + BOFF(mh.dobject)), "i",
		       BMAP(dh.nobjects) - BOFF(mh.dobject));
	}

	if (count != 0) {
	    conf_dread(fd, (char *) counttab, "i", dh.nobjects);
	    rcount = FALSE;
	} else {
	    count = recount(baseplane.nobjects);
	}

	/*
	 * copy all objects from the secondary restore file
	 */
	offset = P_lseek(fd, (off_t) 0, SEEK_CUR);
	i = mh.cobject;
	while (nctrl > 0) {
	    while (!BTST(cmap, i)) {
		i++;
	    }
	    BCLR(cmap, i);

	    o = OBJ(i);
	    if (o->cfirst != SW_UNUSED) {
		if (BTST(omap, i)) {
		    BCLR(omap, i);
		    --ndobject;
		}
		d_restore_ctrl(o, &sw_conv2);
		d_swapout(1);
	    }
	    i++;
	    --nctrl;
	}
	i = mh.dobject;
	while (ndata > 0) {
	    while (!BTST(dmap, i)) {
		i++;
	    }
	    BCLR(dmap, i);

	    o = OBJ(i);
	    if (o->dfirst != SW_UNUSED) {
		if (BTST(omap, i)) {
		    BCLR(omap, i);
		    --ndobject;
		}
		d_restore_data(o, counttab, &sw_conv2);
		d_swapout(1);
	    }
	    i++;
	    --ndata;
	}

	if (cmap != (Uint *) NULL) {
	    FREE(cmap);
	}
	if (dmap != (Uint *) NULL) {
	    FREE(dmap);
	}
	P_lseek(fd, offset, SEEK_SET);
    } else {
	count = recount(baseplane.nobjects);
    }

    baseplane.ocount = count;
}

/*
 * copy objects from dump to swap
 */
bool Object::copy(Uint time)
{
    uindex n;
    Object *obj, *tmpl;

    if (ndobject != 0) {
	if (time == 0) {
	    n = 0;  /* copy all objects */
	} else {
	    if (dtime == 0) {
		/* first copy */
		dtime = time - 1;
		if (dinterval == 0) {
		    dchunksz = SWAPCHUNKSZ;
		} else {
		    dchunksz = (mobjects + dinterval - 1) / dinterval;
		    if (dchunksz == 0) {
			dchunksz = 1;
		    }
		}
	    }

	    time -= dtime;
	    if (dinterval != 0 && time >= dinterval) {
		n = 0;      /* copy all objects */
	    } else if ((n = dchunksz * time) < mobjects) {
		/* copy a portion of remaining objects */
		n = mobjects - n;
	    } else {
		n = 0;      /* copy all objects */
	    }
	}

	while (ndobject > n) {
	    for (obj = OBJ(dobject); !BTST(omap, obj->index); obj++) ;
	    dobject = obj->index + 1;
	    o_restore_obj(obj, FALSE, FALSE);
	    if (time == 0) {
		Object::clean();
		d_swapout(1);
	    }
	}
    }

    if (ndobject == 0) {
	for (n = uobjects, obj = otable; n > 0; --n, obj++) {
	    if (obj->count != 0 && (obj->flags & O_LWOBJ)) {
		for (tmpl = obj; tmpl->prev != OBJ_NONE; tmpl = OBJ(tmpl->prev))
		{
		    if (!(OBJ(tmpl->prev)->flags & O_LWOBJ)) {
			break;
		    }
		    if (counttab[tmpl->prev] == 2) {
			if (o_purge_upgrades(tmpl)) {
			    tmpl->prev = OBJ_NONE;
			}
			break;
		    }
		}
	    }
	}
	Object::clean();

	d_converted();
	dtime = 0;
	return FALSE;
    } else {
	return TRUE;
    }
}


/*
 * indicate that objects are to be swapped out
 */
void Object::swapout()
{
    oplane->swap = TRUE;
}

/*
 * indicate that the state must be dumped
 */
void Object::dumpState(bool incr)
{
    oplane->dump = TRUE;
    oplane->incr = incr;
}

/*
 * indicate that the program must finish
 */
void Object::finish(bool boot)
{
    if (boot && !oplane->dump) {
	error("Hotbooting without snapshot");
    }
    oplane->stop = TRUE;
    oplane->boot = boot;
}
