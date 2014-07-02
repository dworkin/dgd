/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2013 DGD Authors (see the commit log for details)
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

typedef struct _objplane_ objplane;

typedef struct _objpatch_ {
    objplane *plane;			/* plane that patch is on */
    struct _objpatch_ *prev;		/* previous patch */
    struct _objpatch_ *next;		/* next in linked list */
    object obj;				/* new object value */
} objpatch;

# define OPCHUNKSZ		32

typedef struct _opchunk_ {
    struct _opchunk_ *next;		/* next in linked list */
    objpatch op[OPCHUNKSZ];		/* object patches */
} opchunk;

typedef struct {
    opchunk *chunk;			/* object patch chunk */
    unsigned short chunksz;		/* size of object patch chunk */
    objpatch *flist;			/* free list of object patches */
    objpatch *op[OBJPATCHHTABSZ];	/* hash table of object patches */
} optable;

struct _objplane_ {
    hashtab *htab;		/* object name hash table */
    optable *optab;		/* object patch table */
    uintptr_t clean;		/* list of objects to clean */
    uintptr_t upgrade;		/* list of upgrade objects */
    uindex destruct;		/* destructed object list */
    uindex free;		/* free object list */
    uindex nobjects;		/* number of objects in object table */
    uindex nfreeobjs;		/* number of objects in free list */
    Uint ocount;		/* object creation count */
    bool swap, dump, incr, stop, boot; /* state vars */
    struct _objplane_ *prev;	/* previous object plane */
};

object *otable;			/* object table */
Uint *ocmap;			/* object change map */
bool obase;			/* object base plane flag */
bool swap, dump, incr, stop, boot; /* global state vars */
static bool recount;		/* object counts recalculated? */
static uindex otabsize;		/* size of object table */
static uindex uobjects;		/* objects to check for upgrades */
static objplane baseplane;	/* base object plane */
static objplane *oplane;	/* current object plane */
static Uint *omap;		/* object dump bitmap */
static Uint *counttab;		/* object count table */
static object *upgraded;	/* list of upgraded objects */
static uindex dobjects, dobject;/* objects to copy */
static uindex mobjects;		/* max objects to copy */
static uindex dchunksz;		/* copy chunk size */
static Uint dinterval;		/* copy interval */
static Uint dtime;		/* time copying started */
Uint odcount;			/* objects destructed count */
static uindex rotabsize;	/* size of object table at restore */

/*
 * NAME:	object->init()
 * DESCRIPTION:	initialize the object tables
 */
void o_init(unsigned int n, Uint interval)
{
    otable = ALLOC(object, otabsize = n);
    memset(otable, '\0', n * sizeof(object));
    ocmap = ALLOC(Uint, BMAP(n));
    memset(ocmap, '\0', BMAP(n) * sizeof(Uint));
    for (n = 4; n < otabsize; n <<= 1) ;
    baseplane.htab = ht_new(n >> 2, OBJHASHSZ, FALSE);
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
    upgraded = (object *) NULL;
    uobjects = dobjects = mobjects = 0;
    dinterval = (interval * 19) / 20;
    odcount = 1;
    obase = TRUE;
    recount = TRUE;
}


/*
 * NAME:	objpatch->init()
 * DESCRIPTION:	initialize objpatch table
 */
static void op_init(objplane *plane)
{
    memset(plane->optab = ALLOC(optable, 1), '\0', sizeof(optable));
}

/*
 * NAME:	objpatch->clean()
 * DESCRIPTION:	free objpatch table
 */
static void op_clean(objplane *plane)
{
    opchunk *c, *f;

    c = plane->optab->chunk;
    while (c != (opchunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }

    FREE(plane->optab);
    plane->optab = (optable *) NULL;
}

/*
 * NAME:	objpatch->new()
 * DESCRIPTION:	create a new object patch
 */
static objpatch *op_new(objplane *plane, objpatch **o, objpatch *prev, object *obj)
{
    optable *tab;
    objpatch *op;

    /* allocate */
    tab = plane->optab;
    if (tab->flist != (objpatch *) NULL) {
	/* from free list */
	op = tab->flist;
	tab->flist = op->next;
    } else {
	/* newly allocated */
	if (tab->chunk == (opchunk *) NULL || tab->chunksz == OPCHUNKSZ) {
	    opchunk *cc;

	    /* create new chunk */
	    cc = ALLOC(opchunk, 1);
	    cc->next = tab->chunk;
	    tab->chunk = cc;
	    tab->chunksz = 0;
	}

	op = &tab->chunk->op[tab->chunksz++];
    }

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
 * DESCRIPTION:	delete an object patch
 */
static void op_del(objplane *plane, objpatch **o)
{
    objpatch *op;
    optable *tab;

    /* remove from hash table */
    op = *o;
    *o = op->next;

    /* add to free list */
    tab = plane->optab;
    op->next = tab->flist;
    tab->flist = op;
}


/*
 * NAME:	object->oaccess()
 * DESCRIPTION:	access to object from atomic code
 */
static object *o_oaccess(unsigned int index, int access)
{
    objpatch *o, **oo;
    object *obj;

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
	if (obj->chain.name != (char *) NULL && obj->count != 0) {
	    *ht_lookup(oplane->htab, obj->chain.name, FALSE) = (hte *) &o->obj;
	}
	return &o->obj;
    } else {
	/*
	 * first patch for object
	 */
	BSET(ocmap, index);
	if (oplane->optab == (optable *) NULL) {
	    op_init(oplane);
	}
	oo = &oplane->optab->op[index % OBJPATCHHTABSZ];
	obj = &op_new(oplane, oo, (objpatch *) NULL, OBJ(index))->obj;
	if (obj->chain.name != (char *) NULL) {
	    char *name;
	    hte **h;

	    /* copy object name to higher plane */
	    strcpy(name = ALLOC(char, strlen(obj->chain.name) + 1),
		   obj->chain.name);
	    obj->chain.name = name;
	    if (obj->count != 0) {
		if (oplane->htab == (hashtab *) NULL) {
		    oplane->htab = ht_new(OBJPATCHHTABSZ, OBJHASHSZ, FALSE);
		}
		h = ht_lookup(oplane->htab, name, FALSE);
		obj->chain.next = *h;
		*h = (hte *) obj;
	    }
	}
	return obj;
    }
}

/*
 * NAME:	object->oread()
 * DESCRIPTION:	read access to object in patch table
 */
object *o_oread(unsigned int index)
{
    return o_oaccess(index, OACC_READ);
}

/*
 * NAME:	object->owrite()
 * DESCRIPTION:	write access to object in atomic code
 */
object *o_owrite(unsigned int index)
{
    return o_oaccess(index, OACC_MODIFY);
}

/*
 * NAME:	object->space()
 * DESCRIPTION:	check if there's space for another object
 */
bool o_space()
{
    return (oplane->free != OBJ_NONE) ? TRUE : (oplane->nobjects != otabsize);
}

/*
 * NAME:	object->alloc()
 * DESCRIPTION:	allocate a new object
 */
static object *o_alloc()
{
    uindex n;
    object *obj;

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
    obj->ctrl = (control *) NULL;
    obj->data = (dataspace *) NULL;
    obj->cfirst = SW_UNUSED;
    obj->dfirst = SW_UNUSED;
    counttab[n] = 0;

    return obj;
}


/*
 * NAME:	object->new_plane()
 * DESCRIPTION:	create a new object plane
 */
void o_new_plane()
{
    objplane *p;

    p = ALLOC(objplane, 1);

    if (oplane->optab == (optable *) NULL) {
	p->htab = (hashtab *) NULL;
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
 * NAME:	object->commit_plane()
 * DESCRIPTION:	commit the current object plane
 */
void o_commit_plane()
{
    objplane *prev;
    objpatch **t, **o, *op;
    int i;
    object *obj;


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
		    if (op->obj.chain.name != (char *) NULL) {
			hte **h;

			if (obj->chain.name == (char *) NULL) {
			    char *name;

			    /*
			     * make object name static
			     */
			    m_static();
			    name = ALLOC(char, strlen(op->obj.chain.name) + 1);
			    m_dynamic();
			    strcpy(name, op->obj.chain.name);
			    FREE(op->obj.chain.name);
			    op->obj.chain.name = name;
			    if (op->obj.count != 0) {
				/* put name in static hash table */
				h = ht_lookup(prev->htab, name, FALSE);
				op->obj.chain.next = *h;
				*h = (hte *) obj;
			    }
			} else {
			    /*
			     * same name
			     */
			    FREE(op->obj.chain.name);
			    op->obj.chain.name = obj->chain.name;
			    if (op->obj.count != 0) {
				/* keep this name */
				op->obj.chain.next = obj->chain.next;
			    } else if (obj->count != 0) {
				/* remove from hash table */
				h = ht_lookup(prev->htab, obj->chain.name,
					      FALSE);
				if (*h != (hte *) obj) {
				    /* new object was compiled also */
				    h = &(*h)->next;
				}
				*h = obj->chain.next;
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
			if (op->obj.chain.name != (char *) NULL &&
			    op->obj.count != 0) {
			    /* move name to previous plane */
			    *ht_lookup(oplane->htab, op->obj.chain.name, FALSE)
								= (hte *) obj;
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
	    if (oplane->htab != (hashtab *) NULL) {
		ht_del(oplane->htab);
	    }
	    op_clean(oplane);
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
 * NAME:	object->discard_plane()
 * DESCRIPTION:	discard the current object plane without committing it
 */
void o_discard_plane()
{
    objpatch **o, *op;
    int i;
    object *obj, *clist;
    objplane *p;

    if (oplane->optab != (optable *) NULL) {
	clist = (object *) NULL;
	for (i = OBJPATCHHTABSZ, o = oplane->optab->op; --i >= 0; o++) {
	    while (*o != (objpatch *) NULL && (*o)->plane == oplane) {
		op = *o;
		if (op->prev != (objpatch *) NULL) {
		    obj = &op->prev->obj;
		} else {
		    BCLR(ocmap, op->obj.index);
		    obj = OBJ(op->obj.index);
		}

		if (op->obj.chain.name != (char *) NULL) {
		    if (obj->chain.name == (char *) NULL ||
			op->prev == (objpatch *) NULL) {
			/*
			 * remove new name
			 */
			if (op->obj.count != 0) {
			    /* remove from hash table */
			    *ht_lookup(oplane->htab, op->obj.chain.name, FALSE)
							= op->obj.chain.next;
			}
			FREE(op->obj.chain.name);
		    } else {
			hte **h;

			if (op->obj.count != 0) {
			    /*
			     * move name to previous plane
			     */
			    h = ht_lookup(oplane->htab, obj->chain.name, FALSE);
			    obj->chain.next = op->obj.chain.next;
			    *h = (hte *) obj;
			} else if (obj->count != 0) {
			    /*
			     * put name back in hashtable
			     */
			    h = ht_lookup(oplane->htab, obj->chain.name, FALSE);
			    obj->chain.next = *h;
			    *h = (hte *) obj;
			}
		    }
		}

		if (obj->index == OBJ_NONE) {
		    /*
		     * discard newly created object
		     */
		    if ((op->obj.flags & O_MASTER) &&
			op->obj.ctrl != (control *) NULL) {
			op->obj.chain.next = (hte *) clist;
			clist = &op->obj;
		    }
		    if (op->obj.data != (dataspace *) NULL) {
			/* discard new data block */
			d_del_dataspace(op->obj.data);
		    }
		    obj->index = op->obj.index;
		} else {
		    /* pass on control block and dataspace */
		    obj->ctrl = op->obj.ctrl;
		    obj->data = op->obj.data;
		}
		op_del(oplane, o);
	    }
	}

	/* discard new control blocks */
	while (clist != (object *) NULL) {
	    obj = clist;
	    clist = (object *) obj->chain.next;
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
	    if (p->htab != (hashtab *) NULL) {
		ht_del(p->htab);
	    }
	    op_clean(p);
	}
    }
    FREE(p);

    obase = (oplane == &baseplane);
}


/*
 * NAME:	object->new()
 * DESCRIPTION:	create a new object
 */
object *o_new(char *name, control *ctrl)
{
    object *o;
    dinherit *inh;
    int i;
    hte **h;

    /* allocate object */
    o = o_alloc();

    /* put object in object name hash table */
    if (obase) {
	m_static();
    }
    strcpy(o->chain.name = ALLOC(char, strlen(name) + 1), name);
    if (obase) {
	m_dynamic();
    } else if (oplane->htab == (hashtab *) NULL) {
	oplane->htab = ht_new(OBJPATCHHTABSZ, OBJHASHSZ, FALSE);
    }
    h = ht_lookup(oplane->htab, name, FALSE);
    o->chain.next = *h;
    *h = (hte *) o;

    o->flags = O_MASTER;
    o->cref = 0;
    o->prev = OBJ_NONE;
    o->count = ++oplane->ocount;
    o->update = 0;
    o->ctrl = ctrl;
    ctrl->inherits[ctrl->ninherits - 1].oindex = ctrl->oindex = o->index;

    /* add reference to all inherited objects */
    o->u_ref = 0;	/* increased to 1 in following loop */
    inh = ctrl->inherits;
    for (i = ctrl->ninherits, inh = ctrl->inherits; i > 0; --i, inh++) {
	OBJW(inh->oindex)->u_ref++;
    }

    return o;
}

/*
 * NAME:	object->clone()
 * DESCRIPTION:	clone an object
 */
object *o_clone(object *master)
{
    object *o;

    /* allocate object */
    o = o_alloc();
    o->chain.name = (char *) NULL;
    o->flags = 0;
    o->count = ++oplane->ocount;
    o->update = master->update;
    o->u_master = master->index;

    /* add reference to master object */
    master->cref++;
    master->u_ref++;

    return o;
}

/*
 * NAME:	object->lwobj()
 * DESCRIPTION:	create light-weight instance of object
 */
void o_lwobj(object *obj)
{
    obj->flags |= O_LWOBJ;
}

/*
 * NAME:	object->delete()
 * DESCRIPTION:	the last reference to a master object was removed
 */
static void o_delete(object *o, frame *f)
{
    control *ctrl;
    dinherit *inh;
    int i;

    ctrl = (O_UPGRADING(o)) ? OBJR(o->prev)->ctrl : o_control(o);

    /* put in deleted list */
    o->cref = oplane->destruct;
    oplane->destruct = o->index;

    /* callback to the system */
    PUSH_STRVAL(f, str_new(NULL, strlen(o->chain.name) + 1L));
    f->sp->u.string->text[0] = '/';
    strcpy(f->sp->u.string->text + 1, o->chain.name);
    PUSH_INTVAL(f, ctrl->compiled);
    PUSH_INTVAL(f, o->index);
    if (i_call_critical(f, "remove_program", 3, TRUE)) {
	i_del_value(f->sp++);
    }

    /* remove references to inherited objects too */
    for (i = ctrl->ninherits, inh = ctrl->inherits; --i > 0; inh++) {
	o = OBJW(inh->oindex);
	if (--(o->u_ref) == 0) {
	    o_delete(o, f);
	}
    }
}

/*
 * NAME:	object->upgrade()
 * DESCRIPTION:	upgrade an object to a new program
 */
void o_upgrade(object *obj, control *ctrl, frame *f)
{
    object *tmpl;
    dinherit *inh;
    int i;

    /* allocate upgrade object */
    tmpl = o_alloc();
    tmpl->chain.name = (char *) NULL;
    tmpl->flags = O_MASTER;
    tmpl->count = 0;
    tmpl->update = obj->update;
    tmpl->ctrl = ctrl;
    ctrl->inherits[ctrl->ninherits - 1].oindex = tmpl->cref = obj->index;

    /* add reference to inherited objects */
    for (i = ctrl->ninherits, inh = ctrl->inherits; --i > 0; inh++) {
	OBJW(inh->oindex)->u_ref++;
    }

    /* add to upgrades list */
    tmpl->chain.next = (hte *) oplane->upgrade;
    oplane->upgrade = tmpl->index;

    /* mark as upgrading */
    obj->cref += 2;
    tmpl->prev = obj->prev;
    obj->prev = tmpl->index;

    /* remove references to old inherited objects */
    ctrl = o_control(obj);
    for (i = ctrl->ninherits, inh = ctrl->inherits; --i > 0; inh++) {
	obj = OBJW(inh->oindex);
	if (--(obj->u_ref) == 0) {
	    o_delete(obj, f);
	}
    }
}

/*
 * NAME:	object->upgraded()
 * DESCRIPTION:	an object has been upgraded
 */
void o_upgraded(object *tmpl, object *onew)
{
# ifdef DEBUG
    if (onew->count == 0) {
	fatal("upgrading destructed object");
    }
# endif
    if (!(onew->flags & O_MASTER)) {
	onew->update = OBJ(onew->u_master)->update;
    }
    if (tmpl->count == 0) {
	tmpl->chain.next = (hte *) upgraded;
	upgraded = tmpl;
    }
    tmpl->count++;
}

/*
 * NAME:	object->del()
 * DESCRIPTION:	delete an object
 */
void o_del(object *obj, frame *f)
{
    if (obj->count == 0) {
	/* can happen if object selfdestructs in close()-on-destruct */
	error("Destructing destructed object");
    }
    i_odest(f, obj);	/* wipe out occurrences on the stack */
    if (obj->data == (dataspace *) NULL && obj->dfirst != SW_UNUSED) {
	o_dataspace(obj);	/* load dataspace now */
    }
    obj->count = 0;
    odcount++;

    if (obj->flags & O_MASTER) {
	/* remove from object name hash table */
	*ht_lookup(oplane->htab, obj->chain.name, FALSE) = obj->chain.next;

	if (--(obj->u_ref) == 0 && !O_UPGRADING(obj)) {
	    o_delete(obj, f);
	}
    } else {
	object *master;

	master = OBJW(obj->u_master);
	master->cref--;
	if (--(master->u_ref) == 0 && !O_UPGRADING(master)) {
	    o_delete(master, f);
	}
    }

    /* put in clean list */
    obj->chain.next = (hte *) oplane->clean;
    oplane->clean = obj->index;
}


/*
 * NAME:	object->name()
 * DESCRIPTION:	return the name of an object
 */
char *o_name(char *name, object *o)
{
    if (o->chain.name != (char *) NULL) {
	return o->chain.name;
    } else {
	char num[12];
	char *p;
	uindex n;

	/*
	 * return the name of the master object with the index appended
	 */
	n = o->index;
	p = num + 11;
	*p = '\0';
	do {
	    *--p = '0' + n % 10;
	    n /= 10;
	} while (n != 0);
	*--p = '#';

	strcpy(name, OBJR(o->u_master)->chain.name);
	strcat(name, p);
	return name;
    }
}

/*
 * NAME:	object->builtin_name()
 * DESCRIPTION:	return the base name of a builtin type
 */
char *o_builtin_name(Int type)
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
 * NAME:	object->find()
 * DESCRIPTION:	find an object by name
 */
object *o_find(char *name, int access)
{
    object *o;
    unsigned long number;
    char *hash;

    hash = strchr(name, '#');
    if (hash != (char *) NULL) {
	char *p;
	object *m;

	/*
	 * Look for a cloned object, which cannot be found directly in the
	 * object name hash table.
	 * The name must be of the form filename#1234, where 1234 is the
	 * decimal representation of the index in the object table.
	 */
	p = hash + 1;
	if (*p == '\0' || (p[0] == '0' && p[1] != '\0')) {
	    /* don't accept "filename#" or "filename#01" */
	    return (object *) NULL;
	}

	/* convert the string to a number */
	number = 0;
	do {
	    if (!isdigit(*p)) {
		return (object *) NULL;
	    }
	    number = number * 10 + *p++ - '0';
	    if (number >= oplane->nobjects) {
		return (object *) NULL;
	    }
	} while (*p != '\0');

	o = OBJR(number);
	if (o->count == 0 || (o->flags & O_MASTER) ||
	    strncmp(name, (m=OBJR(o->u_master))->chain.name, hash-name) != 0 ||
	    m->chain.name[hash - name] != '\0') {
	    /*
	     * no entry, not a clone, or object name doesn't match
	     */
	    return (object *) NULL;
	}
    } else {
	/* look it up in the hash table */
	if (oplane->htab == (hashtab *) NULL ||
	    (o = (object *) *ht_lookup(oplane->htab, name, TRUE)) ==
							    (object *) NULL) {
	    if (oplane != &baseplane) {
		o = (object *) *ht_lookup(baseplane.htab, name, FALSE);
		if (o != (object *) NULL) {
		    number = o->index;
		    o = (access == OACC_READ)? OBJR(number) : OBJW(number);
		    if (o->count != 0) {
			return o;
		    }
		}
	    }
	    return (object *) NULL;
	}
	number = o->index;
    }

    return (access == OACC_READ) ? o : OBJW(number);
}

/*
 * NAME:	object->restore_object()
 * DESCRIPTION:	restore an object from the snapshot
 */
static void o_restore_obj(object *obj, bool cactive, bool dactive)
{
    BCLR(omap, obj->index);
    --dobjects;
    d_restore_obj(obj, (recount) ? counttab : (Uint *) NULL, cactive, dactive);
}

/*
 * NAME:	object->control()
 * DESCRIPTION:	return the control block for an object
 */
control *o_control(object *obj)
{
    object *o;

    o = obj;
    if (!(o->flags & O_MASTER)) {
	/* get control block of master object */
	o = OBJR(o->u_master);
    }
    if (o->ctrl == (control *) NULL) {
	if (BTST(omap, o->index)) {
	    o_restore_obj(o, TRUE, FALSE);
	} else {
	    o->ctrl = d_load_control(o);
	}
    } else {
	d_ref_control(o->ctrl);
    }
    return obj->ctrl = o->ctrl;
}

/*
 * NAME:	object->dataspace()
 * DESCRIPTION:	return the dataspace block for an object
 */
dataspace *o_dataspace(object *o)
{
    if (o->data == (dataspace *) NULL) {
	if (BTST(omap, o->index)) {
	    o_restore_obj(o, TRUE, TRUE);
	} else {
	    o->data = d_load_dataspace(o);
	}
    } else {
	d_ref_dataspace(o->data);
    }
    return o->data;
}

/*
 * NAME:	object->clean_upgrades()
 * DESCRIPTION:	clean up upgrade templates
 */
static void o_clean_upgrades()
{
    object *o, *next;
    Uint count;

    while ((next=upgraded) != (object *) NULL) {
	upgraded = (object *) next->chain.next;

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
	} while (next->chain.name == (char *) NULL);
    }
}

/*
 * NAME:	object->purge_upgrades()
 * DESCRIPTION:	purge the LW dross from upgrade templates
 */
static bool o_purge_upgrades(object *o)
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
 * NAME:	object->clean()
 * DESCRIPTION:	clean up the object table
 */
void o_clean()
{
    object *o;

    while (baseplane.clean != OBJ_NONE) {
	o = OBJ(baseplane.clean);
	baseplane.clean = (uintptr_t) o->chain.next;

	/* free dataspace block (if it exists) */
	if (o->data != (dataspace *) NULL) {
	    d_del_dataspace(o->data);
	}

	if (o->flags & O_MASTER) {
	    /* remove possible upgrade templates */
	    if (o_purge_upgrades(o)) {
		o->prev = OBJ_NONE;
	    }
	} else {
	    object *tmpl;

	    /* check if clone still had to be upgraded */
	    tmpl = OBJW(o->u_master);
	    if (tmpl->update != o->update) {
		/* non-upgraded clone of old issue */
		do {
		    tmpl = OBJW(tmpl->prev);
		} while (tmpl->update != o->update);

		if (tmpl->count == 0) {
		    tmpl->chain.next = (hte *) upgraded;
		    upgraded = tmpl;
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
	object *up;
	control *ctrl;

	o = OBJ(baseplane.upgrade);
	baseplane.upgrade = (uintptr_t) o->chain.next;

	up = OBJ(o->cref);
	if (up->u_ref == 0) {
	    /* no more instances of object around */
	    o->cref = baseplane.destruct;
	    baseplane.destruct = o->index;
	} else {
	    /* upgrade objects */
	    up->cref -= 2;
	    o->u_ref = up->cref;
	    if (up->flags & O_LWOBJ) {
		o->flags |= O_LWOBJ;
		o->u_ref++;
	    }
	    if (up->count != 0 && O_HASDATA(up)) {
		o->u_ref++;
	    }
	    ctrl = up->ctrl;

	    if (o->ctrl->vmapsize != 0 && o->u_ref != 0) {
		/*
		 * upgrade variables
		 */
		if (o->prev != OBJ_NONE) {
		    OBJ(o->prev)->cref = o->index;
		}

		if (o->u_ref > (up->count != 0)) {
		    up->update++;
		}
		if (up->count != 0 && up->data == (dataspace *) NULL &&
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
	d_del_control(o_control(o));

	if (o->chain.name != (char *) NULL) {
	    /* free object name */
	    FREE(o->chain.name);
	    o->chain.name = (char *) NULL;
	}
	o->u_ref = 0;

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
 * NAME:	object->count()
 * DESCRIPTION:	return the number of objects in use
 */
uindex o_count()
{
    return oplane->nobjects - oplane->nfreeobjs;
}

/*
 * NAME:	object->dobjects()
 * DESCRIPTION:	return the number of objects left to copy
 */
uindex o_dobjects()
{
    return dobjects;
}


typedef struct {
    uindex free;	/* free object list */
    uindex nobjects;	/* # objects */
    uindex nfreeobjs;	/* # free objects */
    Uint onamelen;	/* length of all object names */
} dump_header;

static char dh_layout[] = "uuui";

typedef struct {
    uindex nctrl;	/* objects left to copy */
    uindex ndata;
    uindex cobject;	/* object to copy */
    uindex dobject;
    Uint count;		/* object count */
} map_header;

static char mh_layout[] = "uuuui";

# define CHUNKSZ	16384

/*
 * NAME:	object->sweep()
 * DESCRIPTION:	sweep through the object table after a dump or restore
 */
static void o_sweep(uindex n)
{
    object *obj;

    uobjects = n;
    dobject = 0;
    for (obj = otable; n > 0; obj++, --n) {
	if (obj->count != 0) {
	    if (obj->cfirst != SW_UNUSED || obj->dfirst != SW_UNUSED) {
		BSET(omap, obj->index);
		dobjects++;
	    }
	} else if ((obj->flags & O_MASTER) && obj->u_ref != 0 &&
		   obj->cfirst != SW_UNUSED) {
	    BSET(omap, obj->index);
	    dobjects++;
	}
    }
    mobjects = dobjects;
}

/*
 * NAME:	object->recount()
 * DESCRIPTION:	update object counts
 */
static Uint o_recount(uindex n)
{
    Uint count, *ct;
    object *obj;

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
    recount = TRUE;
    return count;
}

/*
 * NAME:	object->dump()
 * DESCRIPTION:	dump the object table
 */
bool o_dump(int fd, bool incr)
{
    uindex i;
    object *o;
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
	if (o->chain.name != (char *) NULL) {
	    dh.onamelen += strlen(o->chain.name) + 1;
	}
    }

    /* write header and objects */
    if (P_write(fd, (char *) &dh, sizeof(dump_header)) < 0 ||
	P_write(fd, (char *) otable, baseplane.nobjects * sizeof(object)) < 0) {
	return FALSE;
    }

    /* write object names */
    buflen = 0;
    for (i = baseplane.nobjects, o = otable; i > 0; --i, o++) {
	if (o->chain.name != (char *) NULL) {
	    len = strlen(o->chain.name) + 1;
	    if (buflen + len > CHUNKSZ) {
		if (P_write(fd, buffer, buflen) < 0) {
		    return FALSE;
		}
		buflen = 0;
	    }
	    memcpy(buffer + buflen, o->chain.name, len);
	    buflen += len;
	}
    }
    if (buflen != 0 && P_write(fd, buffer, buflen) < 0) {
	return FALSE;
    }

    if (dobjects != 0) {
	/*
	 * partial snapshot: write bitmap and counts
	 */
	mh.nctrl = dobjects;
	mh.ndata = dobjects;
	mh.cobject = dobject;
	mh.dobject = dobject;
	mh.count = 0;
	if (recount) {
	    mh.count = baseplane.ocount;
	}
	if (P_write(fd, (char *) &mh, sizeof(map_header)) < 0 ||
	    P_write(fd, (char *) (omap + BOFF(dobject)),
		    (BMAP(dh.nobjects) - BOFF(dobject)) * sizeof(Uint)) < 0 ||
	    P_write(fd, (char *) (omap + BOFF(dobject)),
		    (BMAP(dh.nobjects) - BOFF(dobject)) * sizeof(Uint)) < 0 ||
	    (mh.count != 0 &&
	     P_write(fd, (char *) counttab, dh.nobjects * sizeof(Uint)) < 0)) {
	    return FALSE;
	}
    }

    if (!incr) {
	o_sweep(baseplane.nobjects);
	baseplane.ocount = o_recount(baseplane.nobjects);
	rotabsize = baseplane.nobjects;
    }

    return TRUE;
}

/*
 * NAME:	object->restore()
 * DESCRIPTION:	restore the object table
 */
void o_restore(int fd, unsigned int rlwobj, bool part)
{
    uindex i;
    object *o;
    Uint len, buflen, count;
    char *p;
    dump_header dh;
    char buffer[CHUNKSZ];

    p = NULL;

    /*
     * Free object names of precompiled objects.
     */
    for (i = baseplane.nobjects, o = otable; i > 0; --i, o++) {
	*ht_lookup(baseplane.htab, o->chain.name, FALSE) = o->chain.next;
	FREE(o->chain.name);
    }

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
	if (rlwobj != 0) {
	    o->flags &= ~O_LWOBJ;
	}
	if (o->chain.name != (char *) NULL) {
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
	    strcpy(o->chain.name = ALLOC(char, len = strlen(p) + 1), p);
	    m_dynamic();

	    if (o->count != 0) {
		hte **h;

		/* add name to lookup table */
		h = ht_lookup(baseplane.htab, p, FALSE);
		o->chain.next = *h;
		*h = (hte *) o;

		/* fix O_LWOBJ */
		if (o->cref & rlwobj) {
		    o->flags |= O_LWOBJ;
		    o->cref &= ~rlwobj;
		}
	    }
	    p += len;
	    buflen -= len;
	} else if (o->ref & rlwobj) {
	    o->flags |= O_LWOBJ;
	    o->ref &= ~rlwobj;
	    o->ref++;
	}

	if (o->count != 0) {
	    /* there are no user or editor objects after a restore */
	    if ((o->flags & O_SPECIAL) != O_SPECIAL) {
		o->flags &= ~O_SPECIAL;
	    }
	}
    }

    o_sweep(baseplane.nobjects);

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
	    recount = FALSE;
	} else {
	    count = o_recount(baseplane.nobjects);
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
		    --dobjects;
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
		    --dobjects;
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
	count = o_recount(baseplane.nobjects);
    }

    baseplane.ocount = count;
    rotabsize = baseplane.nobjects;
}

/*
 * NAME:	object->copy()
 * DESCRIPTION:	copy objects from dump to swap
 */
bool o_copy(Uint time)
{
    uindex n;
    object *obj, *tmpl;

    if (dobjects != 0) {
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

	while (dobjects > n) {
	    for (obj = OBJ(dobject); !BTST(omap, obj->index); obj++) ;
	    dobject = obj->index + 1;
	    o_restore_obj(obj, FALSE, FALSE);
	}
    }
    o_clean();

    if (dobjects == 0) {
	d_swapout(1);

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
	o_clean();

	d_converted();
	dtime = 0;
	return FALSE;
    } else {
	return TRUE;
    }
}


/*
 * NAME:	swapout()
 * DESCRIPTION:	indicate that objects are to be swapped out
 */
void swapout()
{
    oplane->swap = TRUE;
}

/*
 * NAME:	dump_state()
 * DESCRIPTION:	indicate that the state must be dumped
 */
void dump_state(bool incr)
{
    oplane->dump = TRUE;
    oplane->incr = incr;
}

/*
 * NAME:	finish()
 * DESCRIPTION:	indicate that the program must finish
 */
void finish(bool boot)
{
    if (boot && !oplane->dump) {
	error("Hotbooting without snapshot");
    }
    oplane->stop = TRUE;
    oplane->boot = boot;
}
