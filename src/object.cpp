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

# define INCLUDE_FILE_IO
# define INCLUDE_CTYPE
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "control.h"
# include "data.h"
# include "interpret.h"
# include "ext.h"


class ObjPatch : public ChunkAllocated {
public:
    ObjPatch(class ObjPlane *plane, ObjPatch **o, ObjPatch *prev, Object *obj) :
	plane(plane), prev(prev), obj(*obj) {
	next = *o;
	*o = this;
    }

    class ObjPlane *plane;		/* plane that patch is on */
    ObjPatch *prev;			/* previous patch */
    ObjPatch *next;			/* next in linked list */
    Object obj;				/* new object value */
};

# define OPCHUNKSZ		32

class ObjPatchTable : public Allocated {
public:
    /*
     * initialize ObjPatch table
     */
    ObjPatchTable() {
	memset(&op, '\0', OBJPATCHHTABSZ * sizeof(ObjPatch*));
    }

    /*
     * find object patch, or create a new one
     */
    ObjPatch *patch(unsigned int index, int access, class ObjPlane *plane) {
	ObjPatch **oo, *o;

	oo = &op[index % OBJPATCHHTABSZ];
	for (o = *oo; o->obj.index != index; o = o->next) ;
	if (access == OACC_READ || o->plane == plane) {
	    return o;
	}

	/* create new patch on current plane */
	return chunknew (chunk) ObjPatch(plane, oo, o, &o->obj);
    }

    /*
     * add first patch for object
     */
    ObjPatch *addPatch(unsigned int index, class ObjPlane *plane) {
	return chunknew (chunk) ObjPatch(plane, &op[index % OBJPATCHHTABSZ],
					 (ObjPatch *) NULL, OBJ(index));
    }

    Chunk<ObjPatch, OPCHUNKSZ> chunk; 	/* object patch chunk */
    ObjPatch *op[OBJPATCHHTABSZ];	/* hash table of object patches */
};

class ObjPlane : public Allocated {
public:
    ObjPlane(ObjPlane *prev) {
	htab = (Hash::Hashtab *) NULL;
	optab = (ObjPatchTable *) NULL;

	if (prev != (ObjPlane *) NULL) {
	    if (prev->optab != (ObjPatchTable *) NULL) {
		htab = prev->htab;
		optab = prev->optab;
	    }
	    clean = prev->clean;
	    upgrade = prev->upgrade;
	    destruct = prev->destruct;
	    free = prev->free;
	    nobjects = prev->nobjects;
	    nfreeobjs = prev->nfreeobjs;
	    ocount = prev->ocount;
	    swap = prev->swap;
	    dump = prev->dump;
	    incr = prev->incr;
	    stop = prev->stop;
	    boot = prev->boot;
	}
	this->prev = prev;
    }

    virtual ~ObjPlane() {
	if (optab != (ObjPatchTable *) NULL) {
	    if (prev != (ObjPlane *) NULL && prev->prev != (ObjPlane *) NULL) {
		prev->htab = htab;
		prev->optab = optab;
	    } else {
		if (htab != (Hash::Hashtab *) NULL) {
		    delete htab;
		}
		delete optab;
	    }
	}
    }

    /*
     * commit to previous plane
     */
    void commit() {
	prev->clean = clean;
	prev->upgrade = upgrade;
	prev->destruct = destruct;
	prev->free = free;
	prev->nobjects = nobjects;
	prev->nfreeobjs = nfreeobjs;
	prev->ocount = ocount;
	prev->swap = swap;
	prev->dump = dump;
	prev->incr = incr;
	prev->stop = stop;
	prev->boot = boot;
    }

    Hash::Hashtab *htab;	/* object name hash table */
    ObjPatchTable *optab;	/* object patch table */
    uintptr_t clean;		/* list of objects to clean */
    uintptr_t upgrade;		/* list of upgrade objects */
    uindex destruct;		/* destructed object list */
    uindex free;		/* free object list */
    uindex nobjects;		/* number of objects in object table */
    uindex nfreeobjs;		/* number of objects in free list */
    Uint ocount;		/* object creation count */
    bool swap, dump, incr, stop, boot; /* state vars */
    ObjPlane *prev;		/* previous object plane */
};

Object *objTable;		/* object table */
Uint *ocmap;			/* object change map */
bool obase;			/* object base plane flag */
bool swap, dump, incr, stop, boot; /* global state vars */
static bool rcount;		/* object counts recalculated? */
static uindex otabsize;		/* size of object table */
static uindex uobjects;		/* objects to check for upgrades */
static ObjPlane baseplane(NULL);/* base object plane */
static ObjPlane *oplane;	/* current object plane */
static Uint *omap;		/* object dump bitmap */
static Uint *counttab;		/* object count table */
static Uint *insttab;		/* object instance table */
static Object *upgradeList;	/* list of upgraded objects */
static uindex ndobject, dobject;/* objects to copy */
static uindex mobjects;		/* max objects to copy */
static uindex dchunksz;		/* copy chunk size */
static Uint dinterval;		/* copy interval */
static Uint dtime;		/* time copying started */
Uint objDestrCount;		/* objects destructed count */

/*
 * initialize the object tables
 */
void Object::init(unsigned int n, Uint interval)
{
    objTable = ALLOC(Object, otabsize = n);
    memset(objTable, '\0', n * sizeof(Object));
    ocmap = ALLOC(Uint, BMAP(n));
    memset(ocmap, '\0', BMAP(n) * sizeof(Uint));
    for (n = 4; n < otabsize; n <<= 1) ;
    baseplane.htab = HM->create(n >> 2, OBJHASHSZ, FALSE);
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
    insttab = ALLOC(Uint, n);
    do {
	insttab[--n] = 1;
    } while (n != 0);
    upgradeList = (Object *) NULL;
    uobjects = ndobject = mobjects = 0;
    dinterval = ((interval + 1) * 19) / 20;
    objDestrCount = 1;
    obase = TRUE;
    rcount = TRUE;
}

/*
 * access to object from atomic code
 */
Object *Object::access(unsigned int index, int access)
{
    Object *obj;

    if (BTST(ocmap, index)) {
	/*
	 * object already patched
	 */
	obj = &oplane->optab->patch(index, access, oplane)->obj;
	if (obj->name != (char *) NULL && obj->count != 0) {
	    *oplane->htab->lookup(obj->name, FALSE) = obj;
	}
    } else {
	/*
	 * first patch for object
	 */
	BSET(ocmap, index);
	if (oplane->optab == (ObjPatchTable *) NULL) {
	    oplane->optab = new ObjPatchTable;
	}
	obj = &oplane->optab->addPatch(index, oplane)->obj;
	if (obj->name != (char *) NULL) {
	    char *name;
	    Hash::Entry **h;

	    /* copy object name to higher plane */
	    strcpy(name = ALLOC(char, strlen(obj->name) + 1), obj->name);
	    obj->name = name;
	    if (obj->count != 0) {
		if (oplane->htab == (Hash::Hashtab *) NULL) {
		    oplane->htab = HM->create(OBJPATCHHTABSZ, OBJHASHSZ, FALSE);
		}
		h = oplane->htab->lookup(name, FALSE);
		obj->next = *h;
		*h = obj;
	    }
	}
    }
    return obj;
}

/*
 * check if there's space for another object
 */
bool Object::space()
{
    return (oplane->free != OBJ_NONE) ? TRUE : (oplane->nobjects != otabsize);
}

/*
 * allocate a new object
 */
Object *Object::alloc()
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
	    EC->error("Too many objects");
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
    oplane = new ObjPlane(oplane);
    obase = FALSE;
}

/*
 * commit the current object plane
 */
void Object::commitPlane()
{
    ObjPlane *prev;
    ObjPatch **t, **o, *op;
    int i;
    Object *obj;

    prev = oplane->prev;
    if (oplane->optab != (ObjPatchTable *) NULL) {
	for (i = OBJPATCHHTABSZ, t = oplane->optab->op; --i >= 0; t++) {
	    o = t;
	    while (*o != (ObjPatch *) NULL && (*o)->plane == oplane) {
		op = *o;
		if (op->prev != (ObjPatch *) NULL) {
		    obj = &op->prev->obj;
		} else {
		    obj = OBJ(op->obj.index);
		}
		if (op->obj.count == 0 && obj->count != 0) {
		    /* remove object from stackframe above atomic function */
		    cframe->objDest(obj);
		}

		if (prev == &baseplane) {
		    /*
		     * commit to base plane
		     */
		    if (op->obj.name != (char *) NULL) {
			Hash::Entry **h;

			if (obj->name == (char *) NULL) {
			    char *name;

			    /*
			     * make object name static
			     */
			    MM->staticMode();
			    name = ALLOC(char, strlen(op->obj.name) + 1);
			    MM->dynamicMode();
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
		    op->obj.flags |= obj->flags & O_COMPILED;
		    *obj = op->obj;
		    *o = op->next;
		    delete op;
		} else {
		    /*
		     * commit to previous plane
		     */
		    if (op->prev == (ObjPatch *) NULL ||
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
			*o = op->next;
			delete op;
		    }
		}
	    }
	}
    }

    oplane->commit();
    delete oplane;
    oplane = prev;

    obase = (prev == &baseplane);
}

/*
 * discard the current object plane without committing it
 */
void Object::discardPlane()
{
    ObjPatch **o, *op, *clist;
    int i;
    Object *obj;
    ObjPlane *p;

    if (oplane->optab != (ObjPatchTable *) NULL) {
	clist = (ObjPatch *) NULL;
	for (i = OBJPATCHHTABSZ, o = oplane->optab->op; --i >= 0; o++) {
	    while (*o != (ObjPatch *) NULL && (*o)->plane == oplane) {
		op = *o;
		if (op->prev != (ObjPatch *) NULL) {
		    obj = &op->prev->obj;
		} else {
		    BCLR(ocmap, op->obj.index);
		    obj = OBJ(op->obj.index);
		}

		if (op->obj.name != (char *) NULL) {
		    if (obj->name == (char *) NULL ||
			op->prev == (ObjPatch *) NULL) {
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
			Hash::Entry **h;

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

		*o = op->next;
		if (obj->index == OBJ_NONE) {
		    /*
		     * discard newly created object
		     */
		    if (op->obj.data != (Dataspace *) NULL) {
			/* discard new data block */
			op->obj.data->del();
		    }
		    obj->index = op->obj.index;
		    if ((op->obj.flags & O_MASTER) &&
			op->obj.ctrl != (Control *) NULL) {
			op->next = clist;
			clist = op;
			continue;
		    }
		} else {
		    /* pass on control block and dataspace */
		    obj->ctrl = op->obj.ctrl;
		    if (obj->data != op->obj.data) {
			if (obj->dfirst != SW_UNUSED) {
			    obj->data = op->obj.data;
			} else {
			    /* discard new initialized data block */
			    op->obj.data->del();
			}
		    }
		}
		delete op;
	    }
	}

	/* discard new control blocks */
	while (clist != (ObjPatch *) NULL) {
	    obj = &clist->obj;
	    obj->ctrl->del();
	    op = clist;
	    clist = clist->next;
	    delete op;
	}
    }

    p = oplane;
    oplane = p->prev;
    delete p;

    obase = (oplane == &baseplane);
}


/*
 * create a new object
 */
Object *Object::create(char *name, Control *ctrl)
{
    Object *o;
    Inherit *inh;
    int i;
    Hash::Entry **h;

    /* allocate object */
    o = alloc();

    /* put object in object name hash table */
    if (obase) {
	MM->staticMode();
    }
    o->name = strcpy(ALLOC(char, strlen(name) + 1), name);
    if (obase) {
	MM->dynamicMode();
    } else if (oplane->htab == (Hash::Hashtab *) NULL) {
	oplane->htab = HM->create(OBJPATCHHTABSZ, OBJHASHSZ, FALSE);
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
    o = alloc();
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
 * the last reference to a master object was removed
 */
void Object::remove(Frame *f)
{
    Control *ctrl;
    Inherit *inh;
    int i;
    Object *o;

    ctrl = (O_UPGRADING(this)) ? OBJR(prev)->ctrl : control();

    /* put in deleted list */
    cref = oplane->destruct;
    oplane->destruct = index;

    /* callback to the system */
    PUSH_STRVAL(f, String::create(NULL, strlen(name) + 1L));
    f->sp->string->text[0] = '/';
    strcpy(f->sp->string->text + 1, name);
    PUSH_INTVAL(f, ctrl->compiled);
    PUSH_INTVAL(f, index);
    if (f->callCritical("remove_program", 3, TRUE)) {
	(f->sp++)->del();
    }

    /* remove references to inherited objects too */
    for (i = ctrl->ninherits, inh = ctrl->inherits; --i > 0; inh++) {
	o = OBJW(inh->oindex);
	if (--(o->ref) == 0) {
	    o->remove(f);
	}
    }
}

/*
 * upgrade an object to a new program
 */
void Object::upgrade(Control *ctrl, Frame *f)
{
    Object *obj;
    Inherit *inh;
    int i;

    /* allocate upgrade object */
    obj = alloc();
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
    obj->next = (Hash::Entry *) oplane->upgrade;
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
	    obj->remove(f);
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
	EC->fatal("upgrading destructed object");
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
	EC->error("Destructing destructed object");
    }
    f->objDest(this);	/* wipe out occurrences on the stack */
    if (data == (Dataspace *) NULL && dfirst != SW_UNUSED) {
	dataspace();	/* load dataspace now */
    }
    count = 0;
    objDestrCount++;

    if (flags & O_MASTER) {
	/* remove from object name hash table */
	*oplane->htab->lookup(name, FALSE) = next;

	if (--ref == 0) {
	    remove(f);
	}
    } else {
	Object *master;

	master = OBJW(this->master);
	master->cref--;
	if (--(master->ref) == 0) {
	    master->remove(f);
	}
    }

    /* put in clean list */
    next = (Hash::Entry *) oplane->clean;
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
const char *Object::builtinName(LPCint type)
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
	EC->fatal("unknown builtin type %d", type);
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
	if (oplane->htab == (Hash::Hashtab *) NULL ||
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
 * restore an object from the snapshot
 */
void Object::restoreObject(bool cactive, bool dactive)
{
    BCLR(omap, index);
    --ndobject;
    Dataspace::restoreObject(this, insttab[index],
			     (rcount) ? counttab : (Uint *) NULL, cactive,
			     dactive);
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
	    o->restoreObject(TRUE, FALSE);
	} else {
	    o->ctrl = Control::load(o, insttab[o->index]);
	}
    } else {
	o->ctrl->ref();
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
	    restoreObject(TRUE, TRUE);
	} else {
	    data = Dataspace::load(this);
	}
    } else {
	data->ref();
    }
    return data;
}

/*
 * clean up upgrade templates
 */
void Object::cleanUpgrades()
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
		    EC->fatal("removing issue in middle of list");
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
 * purge the LW dross from upgrade templates
 */
bool Object::purgeUpgrades()
{
    Object *o;
    bool purged;

    o = this;
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
	    o->data->del();
	}

	if (o->flags & O_MASTER) {
	    /* remove possible upgrade templates */
	    if (o->purgeUpgrades()) {
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

    cleanUpgrades();		/* 1st time */

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

		if (o->ref > (Uint) (up->count != 0 && O_HASDATA(up))) {
		    up->update++;
		}
		if (up->count != 0 && up->data == (Dataspace *) NULL &&
		    up->dfirst != SW_UNUSED) {
		    /* load dataspace (with old control block) */
		    up->data = Dataspace::load(up);
		}
	    } else {
		/* no variable upgrading */
		up->prev = o->prev;
		o->cref = baseplane.destruct;
		baseplane.destruct = o->index;
	    }

	    if (up->flags & O_COMPILED) {
		up->flags &= ~O_COMPILED;
		Ext::release(up->index, up->ctrl->instance);
	    }

	    /* swap control blocks */
	    up->ctrl = o->ctrl;
	    up->ctrl->oindex = up->index;
	    up->ctrl->instance = ++insttab[up->index];
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
		Dataspace::upgradeMemory(o, up);
	    }
	}
    }

    cleanUpgrades();		/* 2nd time */

    while (baseplane.destruct != OBJ_NONE) {
	o = OBJ(baseplane.destruct);
	baseplane.destruct = o->cref;

	if (o->flags & O_COMPILED) {
	    Ext::release(o->index, o->control()->instance);
	}

	/* free control block */
	o->control()->del();

	if (o->name != (char *) NULL) {
	    /* free object name */
	    FREE(o->name);
	    o->name = (char *) NULL;
	}
	o->ref = 0;
	insttab[o->index]++;

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


struct ObjectHeader {
    uindex free;	/* free object list */
    uindex nobjects;	/* # objects */
    uindex nfreeobjs;	/* # free objects */
    Uint onamelen;	/* length of all object names */
};

static char oh_layout[] = "uuui";

struct MapHeader {
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
    for (obj = objTable; n > 0; obj++, --n) {
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
    for (obj = objTable, ct = counttab; n > 0; obj++, ct++, --n) {
	if (obj->count != 0) {
	    *ct = obj->count;
	    obj->count = count++;
	} else {
	    *ct = 2;
	}
    }

    objDestrCount = 1;
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
    ObjectHeader dh;
    MapHeader mh;
    char buffer[CHUNKSZ];

    /* prepare header */
    dh.free = baseplane.free;
    dh.nobjects = baseplane.nobjects;
    dh.nfreeobjs = baseplane.nfreeobjs;
    dh.onamelen = 0;
    for (i = baseplane.nobjects, o = objTable; i > 0; --i, o++) {
	if (o->name != (char *) NULL) {
	    dh.onamelen += strlen(o->name) + 1;
	}
    }

    /* write header and objects */
    if (!Swap::write(fd, &dh, sizeof(ObjectHeader)) ||
	!Swap::write(fd, objTable, baseplane.nobjects * sizeof(Object))) {
	return FALSE;
    }

    /* write object names */
    buflen = 0;
    for (i = baseplane.nobjects, o = objTable; i > 0; --i, o++) {
	if (o->name != (char *) NULL) {
	    len = strlen(o->name) + 1;
	    if (buflen + len > CHUNKSZ) {
		if (!Swap::write(fd, buffer, buflen)) {
		    return FALSE;
		}
		buflen = 0;
	    }
	    memcpy(buffer + buflen, o->name, len);
	    buflen += len;
	}
    }
    if (buflen != 0 && !Swap::write(fd, buffer, buflen)) {
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
	if (!Swap::write(fd, &mh, sizeof(MapHeader)) ||
	    !Swap::write(fd, omap + BOFF(dobject),
			 (BMAP(dh.nobjects) - BOFF(dobject)) * sizeof(Uint)) ||
	    !Swap::write(fd, omap + BOFF(dobject),
			 (BMAP(dh.nobjects) - BOFF(dobject)) * sizeof(Uint)) ||
	    (mh.count != 0 &&
	     !Swap::write(fd, counttab, dh.nobjects * sizeof(Uint)))) {
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
    ObjectHeader dh;
    char buffer[CHUNKSZ];

    p = NULL;

    /* read header and object table */
    Config::dread(fd, (char *) &dh, oh_layout, (Uint) 1);

    if (dh.nobjects > otabsize) {
	EC->error("Too many objects in restore file (%u)", dh.nobjects);
    }

    Config::dread(fd, (char *) objTable, OBJ_LAYOUT, (Uint) dh.nobjects);
    baseplane.free = dh.free;
    baseplane.nobjects = dh.nobjects;
    baseplane.nfreeobjs = dh.nfreeobjs;

    /* read object names */
    buflen = 0;
    for (i = 0, o = objTable; i < baseplane.nobjects; i++, o++) {
	o->flags &= ~O_COMPILED;
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
		    EC->fatal("cannot restore object names");
		}
		dh.onamelen -= len;
		buflen += len;
		p = buffer;
	    }
	    MM->staticMode();
	    o->name = strcpy(ALLOC(char, len = strlen(p) + 1), p);
	    MM->dynamicMode();

	    if (o->count != 0) {
		Hash::Entry **h;

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
	MapHeader mh;
	off_t offset;
	Uint *cmap, *dmap;
	uindex nctrl, ndata;

	Config::dread(fd, (char *) &mh, mh_layout, (Uint) 1);
	nctrl = mh.nctrl;
	ndata = mh.ndata;
	count = mh.count;

	cmap = dmap = (Uint *) NULL;
	if (nctrl != 0) {
	    cmap = ALLOC(Uint, BMAP(dh.nobjects));
	    memset(cmap, '\0', BMAP(dh.nobjects) * sizeof(Uint));
	    Config::dread(fd, (char *) (cmap + BOFF(mh.cobject)), "i",
			  BMAP(dh.nobjects) - BOFF(mh.cobject));
	}
	if (ndata != 0) {
	    dmap = ALLOC(Uint, BMAP(dh.nobjects));
	    memset(dmap, '\0', BMAP(dh.nobjects) * sizeof(Uint));
	    Config::dread(fd, (char *) (dmap + BOFF(mh.dobject)), "i",
			  BMAP(dh.nobjects) - BOFF(mh.dobject));
	}

	if (count != 0) {
	    Config::dread(fd, (char *) counttab, "i", dh.nobjects);
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
		Control::restore(o, insttab[o->index], &Swap::conv2);
		Dataspace::swapout(1);
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
		Dataspace::restore(o, counttab, &Swap::conv2);
		Dataspace::swapout(1);
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
	    obj->restoreObject(FALSE, FALSE);
	    if (time == 0) {
		Object::clean();
		Dataspace::swapout(1);
	    }
	}
    }

    if (ndobject == 0) {
	for (n = uobjects, obj = objTable; n > 0; --n, obj++) {
	    if (obj->count != 0 && (obj->flags & O_LWOBJ)) {
		for (tmpl = obj; tmpl->prev != OBJ_NONE; tmpl = OBJ(tmpl->prev))
		{
		    if (!(OBJ(tmpl->prev)->flags & O_LWOBJ)) {
			break;
		    }
		    if (counttab[tmpl->prev] == 2) {
			if (tmpl->purgeUpgrades()) {
			    tmpl->prev = OBJ_NONE;
			}
			break;
		    }
		}
	    }
	}
	Object::clean();

	Control::converted();
	Dataspace::converted();
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
	EC->error("Hotbooting without snapshot");
    }
    oplane->stop = TRUE;
    oplane->boot = boot;
}
