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
    unsigned long clean;	/* list of objects to clean */
    unsigned long upgrade;	/* list of upgrade objects */
    uindex destruct;		/* destructed object list */
    uindex free;		/* free object list */
    uindex nobjects;		/* number of objects in object table */
    uindex nfreeobjs;		/* number of objects in free list */
    Uint ocount;		/* object creation count */
    bool swap, dump, stop;	/* state vars */
    struct _objplane_ *prev;	/* previous object plane */
};

object *otable;			/* object table */
char *ocmap;			/* object change map */
bool obase;			/* object base plane flag */
bool swap, dump, stop;		/* global state vars */
static uindex otabsize;		/* size of object table */
static objplane baseplane;	/* base object plane */
static objplane *oplane;	/* current object plane */
static object *upgraded;	/* list of upgraded objects */
Uint odcount;			/* objects destructed count */

/*
 * NAME:	object->init()
 * DESCRIPTION:	initialize the object tables
 */
void o_init(n)
register unsigned int n;
{
    otable = ALLOC(object, otabsize = n);
    memset(otable, '\0', n * sizeof(object));
    ocmap = ALLOC(char, (n + 7) >> 3);
    memset(ocmap, '\0', (n + 7) >> 3);
    for (n = 4; n < otabsize; n <<= 1) ;
    baseplane.htab = ht_new(n >> 2, OBJHASHSZ, FALSE);
    baseplane.optab = (optable *) NULL;
    baseplane.upgrade = baseplane.clean = OBJ_NONE;
    baseplane.destruct = baseplane.free = OBJ_NONE;
    baseplane.nobjects = 0;
    baseplane.nfreeobjs = 0;
    baseplane.ocount = 2;
    baseplane.swap = baseplane.dump = baseplane.stop = FALSE;
    oplane = &baseplane;
    upgraded = (object *) NULL;
    odcount = 1;
    obase = TRUE;
}


/*
 * NAME:	objpatch->init()
 * DESCRIPTION:	initialize objpatch table
 */
static void op_init(plane)
objplane *plane;
{
    memset(plane->optab = ALLOC(optable, 1), '\0', sizeof(optable));
}

/*
 * NAME:	objpatch->clean()
 * DESCRIPTION:	free objpatch table
 */
static void op_clean(plane)
objplane *plane;
{
    register opchunk *c, *f;

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
static objpatch *op_new(plane, o, prev, obj)
objplane *plane;
objpatch **o, *prev;
object *obj;
{
    register optable *tab;
    register objpatch *op;

    /* allocate */
    tab = plane->optab;
    if (tab->flist != (objpatch *) NULL) {
	/* from free list */
	op = tab->flist;
	tab->flist = op->next;
    } else {
	/* newly allocated */
	if (tab->chunk == (opchunk *) NULL || tab->chunksz == OPCHUNKSZ) {
	    register opchunk *cc;

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
static void op_del(plane, o)
objplane *plane;
register objpatch **o;
{
    register objpatch *op;
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
static object *o_oaccess(index, access)
register unsigned int index;
int access;
{
    register objpatch *o, **oo;
    register object *obj;

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
	return &op_new(oplane, oo, o, &o->obj)->obj;
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
object *o_oread(index)
unsigned int index;
{
    return o_oaccess(index, OACC_READ);
}

/*
 * NAME:	object->owrite()
 * DESCRIPTION:	write access to object in atomic code
 */
object *o_owrite(index)
unsigned int index;
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
    register uindex n;
    register object *obj;

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

    return obj;
}


/*
 * NAME:	object->new_plane()
 * DESCRIPTION:	create a new object plane
 */
void o_new_plane()
{
    register objplane *p;

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
    p->stop = oplane->stop;
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
    register objplane *prev;
    register objpatch **t, **o, *op;
    register int i;
    register object *obj;


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
    prev->stop = oplane->stop;
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
    register objpatch **o, *op;
    register int i;
    register object *obj, *clist;
    register objplane *p;

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
object *o_new(name, ctrl)
char *name;
register control *ctrl;
{
    register object *o;
    register dinherit *inh;
    register int i;
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
object *o_clone(master)
register object *master;
{
    register object *o;

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
void o_lwobj(obj)
object *obj;
{
    obj->flags |= O_LWOBJ;
}

/*
 * NAME:	object->delete()
 * DESCRIPTION:	the last reference to a master object was removed
 */
static void o_delete(o, f)
register object *o;
register frame *f;
{
    register control *ctrl;
    register dinherit *inh;
    register int i;

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
	    o_delete(OBJW(inh->oindex), f);
	}
    }
}

/*
 * NAME:	object->upgrade()
 * DESCRIPTION:	upgrade an object to a new program
 */
void o_upgrade(obj, ctrl, f)
register object *obj;
control *ctrl;
register frame *f;
{
    register object *tmpl;
    register dinherit *inh;
    register int i;

    /* allocate upgrade object */
    tmpl = o_alloc();
    tmpl->chain.name = (char *) NULL;
    tmpl->flags = O_MASTER;
    tmpl->count = 0;
    tmpl->update = obj->update;
    tmpl->ctrl = ctrl;
    ctrl->inherits[ctrl->ninherits - 1].oindex = tmpl->dfirst = obj->index;

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
	    o_delete(OBJW(inh->oindex), f);
	}
    }
}

/*
 * NAME:	object->upgraded()
 * DESCRIPTION:	an object has been upgraded
 */
void o_upgraded(tmpl, new)
register object *tmpl, *new;
{
# ifdef DEBUG
    if (new->count == 0) {
	fatal("upgrading destructed object");
    }
# endif
    if (!(new->flags & O_MASTER)) {
	new->update = OBJ(new->u_master)->update;
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
void o_del(obj, f)
register object *obj;
frame *f;
{
    if (obj->count == 0) {
	/* can happen if object selfdestructs in close()-on-destruct */
	error("Destructing destructed object");
    }
    i_odest(f, obj);	/* wipe out occurrances on the stack */
    obj->count = 0;
    odcount++;

    if (obj->flags & O_MASTER) {
	/* remove from object name hash table */
	*ht_lookup(oplane->htab, obj->chain.name, FALSE) = obj->chain.next;

	if (--(obj->u_ref) == 0) {
	    o_delete(obj, f);
	}
    } else {
	object *master;

	master = OBJW(obj->u_master);
	master->cref--;
	if (--(master->u_ref) == 0) {
	    o_delete(OBJW(master->index), f);
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
char *o_name(name, o)
char *name;
register object *o;
{
    if (o->chain.name != (char *) NULL) {
	return o->chain.name;
    } else {
	char num[12];
	register char *p;
	register uindex n;

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
 * NAME:	object->find()
 * DESCRIPTION:	find an object by name
 */
object *o_find(name, access)
char *name;
int access;
{
    register object *o;
    register unsigned long number;
    char *hash;

    hash = strchr(name, '#');
    if (hash != (char *) NULL) {
	register char *p;
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
 * NAME:	object->control()
 * DESCRIPTION:	return the control block for an object
 */
control *o_control(obj)
object *obj;
{
    register object *o;

    o = obj;
    if (!(o->flags & O_MASTER)) {
	/* get control block of master object */
	o = OBJR(o->u_master);
    }
    if (o->ctrl == (control *) NULL) {
	o->ctrl = d_load_control(o);	/* reload */
    } else {
	d_ref_control(o->ctrl);
    }
    return obj->ctrl = o->ctrl;
}

/*
 * NAME:	object->dataspace()
 * DESCRIPTION:	return the dataspace block for an object
 */
dataspace *o_dataspace(o)
register object *o;
{
    if (o->data == (dataspace *) NULL) {
	/* load dataspace block */
	o->data = d_load_dataspace(o);
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
    register object *o, *next;
    register Uint count;

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
	} while (o->dfirst != next->index);
    }
}

/*
 * NAME:	object->purge_upgrades()
 * DESCRIPTION:	purge the LW dross from upgrade templates
 */
static bool o_purge_upgrades(o)
register object *o;
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
    register object *o;

    while (baseplane.clean != OBJ_NONE) {
	o = OBJ(baseplane.clean);
	baseplane.clean = (unsigned long) o->chain.next;

	/* free dataspace block (if it exists) */
	if (o->data == (dataspace *) NULL && o->dfirst != SW_UNUSED) {
	    /* reload dataspace block (sectors are needed) */
	    o->data = d_load_dataspace(o);
	}
	if (o->data != (dataspace *) NULL) {
	    d_del_dataspace(o->data);
	}

	if (o->flags & O_MASTER) {
	    /* remove possible upgrade templates */
	    if (o_purge_upgrades(o)) {
		o->prev = OBJ_NONE;
	    }
	} else {
	    register object *tmpl;

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
	register object *up;
	register control *ctrl;

	o = OBJ(baseplane.upgrade);
	baseplane.upgrade = (unsigned long) o->chain.next;

	up = OBJ(o->dfirst);
	if (up->u_ref == 0) {
	    /* no more instances of object around */
	    o->cref = baseplane.destruct;
	    baseplane.destruct = o->index;
	} else {
	    /* upgrade objects */
	    up->flags &= ~O_COMPILED;
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

	    if (ctrl->vmapsize != 0 && o->u_ref != 0) {
		/*
		 * upgrade variables
		 */
		o->cref = up->index;
		if (o->prev != OBJ_NONE) {
		    OBJ(o->prev)->cref = o->index;
		}

		up->update++;
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
    stop = baseplane.stop;
    baseplane.swap = baseplane.dump = baseplane.stop = FALSE;
}

/*
 * NAME:	object->count()
 * DESCRIPTION:	return the number of objects in use
 */
uindex o_count()
{
    return oplane->nobjects - oplane->nfreeobjs;
}


typedef struct {
    uindex free;	/* free object list */
    uindex nobjects;	/* # objects */
    uindex nfreeobjs;	/* # free objects */
    Uint onamelen;	/* length of all object names */
} dump_header;

static char dh_layout[] = "uuui";

# define CHUNKSZ	16384

/*
 * NAME:	object->dump()
 * DESCRIPTION:	dump the object table
 */
bool o_dump(fd)
int fd;
{
    register uindex i;
    register object *o;
    register unsigned int len, buflen;
    dump_header dh;
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
    return (buflen == 0 || P_write(fd, buffer, buflen) >= 0);
}

/*
 * NAME:	object->restore()
 * DESCRIPTION:	restore the object table
 */
void o_restore(fd, rlwobj)
int fd;
unsigned int rlwobj;
{
    register uindex i;
    register object *o;
    register unsigned int len, buflen;
    register char *p;
    dump_header dh;
    char buffer[CHUNKSZ];

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
	error("Too many objects in restore file");
    }
    conf_dread(fd, (char *) otable, OBJ_LAYOUT, (Uint) dh.nobjects);
    baseplane.free = dh.free;
    baseplane.nobjects = dh.nobjects;
    baseplane.nfreeobjs = dh.nfreeobjs;

    /* read object names, and patch all objects and control blocks */
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
		register hte **h;

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

	/* check memory */
	if (!m_check()) {
	    m_purge();
	}
    }
}

static int cmp P((cvoid*, cvoid*));

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two objects
 */
static int cmp(cv1, cv2)
cvoid *cv1, *cv2;
{
    register object *o1, *o2;

    /* non-objects first, then objects sorted by count */
    o1 = OBJ(*((Uint *) cv1));
    o2 = OBJ(*((Uint *) cv2));
    if (o1->count == 0) {
	if (o2->count == 0) {
	    return (o1 <= o2) ? (o1 < o2) ? -1 : 0 : 1;
	}
	return -1;
    } else if (o2->count == 0) {
	return 1;
    } else {
	return (o1->count <= o2->count) ? (o1->count < o2->count) ? -1 : 0 : 1;
    }
}

/*
 * NAME:	object->conv()
 * DESCRIPTION:	convert all objects, creating a new swap file
 */
void o_conv(conv_callouts, conv_lwos, conv_ctrls)
int conv_callouts, conv_lwos, conv_ctrls;
{
    register Uint *counts, *sorted;
    register uindex i;
    register object *o;

    if (baseplane.nobjects != 0) {
	/*
	 * create new object count table
	 */
	counts = ALLOCA(Uint, baseplane.nobjects);
	sorted = ALLOCA(Uint, baseplane.nobjects) + baseplane.nobjects;
	i = baseplane.nobjects;
	while (i != 0) {
	    *--sorted = --i;
	}
	/* sort by count */
	qsort(sorted, baseplane.nobjects, sizeof(Uint), cmp);
	/* skip destructed objects */
	for (i = 0; i < baseplane.nobjects; i++) {
	    if (OBJ(*sorted)->count != 0) {
		break;
	    }
	    sorted++;
	}
	/* fill in counts table */
	while (i < baseplane.nobjects) {
	    counts[*sorted++] = ++i + 1;
	}
	AFREE(sorted - i);
	baseplane.ocount = i + 2;

	/*
	 * convert all control blocks
	 */
	for (i = baseplane.nobjects, o = otable; i > 0; --i, o++) {
	    if ((o->count != 0 || ((o->flags & O_MASTER) && o->u_ref != 0)) &&
		o->cfirst != SW_UNUSED) {
		d_conv_control(o->index, conv_ctrls);
	    }
	}

	/*
	 * convert all data blocks
	 */
	for (i = baseplane.nobjects, o = otable; i > 0; --i, o++) {
	    if (o->count != 0 && o->dfirst != SW_UNUSED) {
		d_conv_dataspace(o, counts, conv_callouts);
		o_clean_upgrades();
		d_swapout(1);
	    }
	}

	/*
	 * clean up object upgrade templates
	 */
	for (i = baseplane.nobjects, o = otable; i > 0; --i, o++) {
	    if (o->count != 0 && o->cfirst != SW_UNUSED &&
		(o->flags & O_LWOBJ) && o_purge_upgrades(o)) {
		o->prev = OBJ_NONE;
	    }
	}
	o_clean();

	/*
	 * last pass over all objects:
	 * fix count and update fields, handle special objects
	 */
	for (i = baseplane.nobjects, o = otable; i > 0; --i, o++, counts++) {
	    if (o->count != 0) {
		o->count = *counts;
	    }
	    o->update = 0;
	    if ((o->flags & O_SPECIAL) == O_SPECIAL &&
		ext_restore != (void (*) P((object*))) NULL) {
		(*ext_restore)(o);
		d_swapout(1);
	    }
	}
	AFREE(counts - baseplane.nobjects);
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
void dump_state()
{
    oplane->dump = TRUE;
}

/*
 * NAME:	finish()
 * DESCRIPTION:	indicate that the program must finish
 */
void finish()
{
    oplane->stop = TRUE;
}
