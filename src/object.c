# define INCLUDE_FILE_IO
# define INCLUDE_CTYPE
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"

# define OBJ_NONE		UINDEX_MAX

static object *otab;		/* object table */
static uindex otabsize;		/* size of object table */
static hashtab *htab;		/* object name hash table */
static object *clean_obj;	/* list of objects to clean */
static object *upgrade_obj;	/* list of upgrade objects */
static object *dest_obj;	/* destructed object list */
static object *free_obj;	/* free object list */
static uindex nobjects;		/* number of objects in object table */
static uindex nfreeobjs;	/* number of objects in free list */
static Uint count;		/* object creation count */

/*
 * NAME:	object->init()
 * DESCRIPTION:	initialize the object tables
 */
void o_init(n)
register unsigned int n;
{
    otab = ALLOC(object, otabsize = n);
    for (n = 4; n < otabsize; n <<= 1) ;
    htab = ht_new(n >> 2, OBJHASHSZ);
    free_obj = dest_obj = upgrade_obj = clean_obj = (object *) NULL;
    nobjects = 0;
    nfreeobjs = 0;
    count = 0;
}

/*
 * NAME:	object->alloc()
 * DESCRIPTION:	allocate a new object
 */
static object *o_alloc()
{
    object *o;

    if (free_obj != (object *) NULL) {
	/* get space from free object list */
	o = free_obj;
	free_obj = (object *) o->chain.next;
	--nfreeobjs;
    } else {
	/* use new space in object table */
	if (nobjects == otabsize) {
	    fatal("too many objects");
	}
	o = &otab[nobjects++];
    }

    o->data = (dataspace *) NULL;
    o->cfirst = SW_UNUSED;
    o->dfirst = SW_UNUSED;

    return o;
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
    m_static();
    strcpy(o->chain.name = ALLOC(char, strlen(name) + 1), name);
    m_dynamic();
    h = ht_lookup(htab, name, FALSE);
    o->chain.next = *h;
    *h = (hte *) o;

    o->flags = O_MASTER;
    o->cref = 0;
    o->prev = OBJ_NONE;
    o->index = o - otab;
    o->count = ++count;
    o->update = 0;
    o->ctrl = ctrl;
    ctrl->inherits[ctrl->ninherits - 1].obj = ctrl->obj = o;

    /* add reference to all inherited objects */
    o->u.ref = 0;	/* increased to 1 in following loop */
    inh = ctrl->inherits;
    for (i = ctrl->ninherits; i > 0; --i) {
	(inh++)->obj->u.ref++;
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
    o->index = o - otab;
    o->count = ++count;
    o->update = (o->u.master = master)->update;
    o->ctrl = (control *) NULL;
    o->data = d_new_dataspace(o);	/* clones always have a dataspace */

    /* add reference to master object */
    master->cref++;
    master->u.ref++;

    return o;
}

/*
 * NAME:	object->delete()
 * DESCRIPTION:	the last reference to a master object was removed
 */
static void o_delete(o)
register object *o;
{
    /* put in deleted list */
    o->u.master = dest_obj;
    dest_obj = o;

    /* callback to the system */
    (--sp)->type = T_STRING;
    str_ref(sp->u.string = str_new(NULL, strlen(o->chain.name) + 1));
    sp->u.string->text[0] = '/';
    strcpy(sp->u.string->text + 1, o->chain.name);
    (--sp)->type = T_INT;
    sp->u.number = o_control(o)->compiled;
    if (i_call_critical("remove_program", 2, TRUE)) {
	i_del_value(sp++);
    }

    if (!O_UPGRADING(o)) {
	register dinherit *inh;
	register int i;

	/* remove references to inherited objects too */
	inh = o->ctrl->inherits;
	i = o->ctrl->ninherits;
	while (--i > 0) {
	    o = (inh++)->obj;
	    if (--(o->u.ref) == 0) {
		o_delete(o);
	    }
	}
    }
}

/*
 * NAME:	object->upgrade()
 * DESCRIPTION:	upgrade an object to a new program
 */
void o_upgrade(obj, ctrl)
object *obj;
control *ctrl;
{
    register object *o;
    register dinherit *inh;
    register int i;

    /* allocate upgrade object */
    o = o_alloc();
    o->chain.name = (char *) NULL;
    o->flags = O_MASTER;
    o->count = 0;
    o->u.master = obj;
    o->ctrl = ctrl;
    ctrl->inherits[ctrl->ninherits - 1].obj = obj;

    /* add reference to inherited objects */
    inh = ctrl->inherits;
    i = ctrl->ninherits;
    while (--i > 0) {
	(inh++)->obj->u.ref++;
    }

    /* add to upgrades list */
    o->chain.next = (hte *) upgrade_obj;
    upgrade_obj = o;

    /* mark as upgrading */
    obj->cref += 2;

    /* remove references to old inherited objects */
    ctrl = o_control(obj);
    inh = ctrl->inherits;
    i = ctrl->ninherits;
    while (--i > 0) {
	o = (inh++)->obj;
	if (--(o->u.ref) == 0) {
	    o_delete(o);
	}
    }
}

/*
 * NAME:	object->upgraded()
 * DESCRIPTION:	an object has been upgraded
 */
void o_upgraded(old, new)
register object *old, *new;
{
    if (new->count != 0) {
	if (!(new->flags & O_MASTER)) {
	    new->update = new->u.master->update;
	    new = new->u.master;
	    new->cref++;
	    new->u.ref++;
	}
	while (--(old->u.ref) == 0) {
	    /* put in deleted list */
	    old->u.master = dest_obj;
	    dest_obj = old;

# ifdef DEBUG
	    if (old->prev != OBJ_NONE) {
		fatal("removing issue in middle of list");
	    }
# endif
	    /* remove from issue list */
	    old = &otab[old->cref];
	    old->prev = OBJ_NONE;
	    if (old == new) {
		new->cref--;
		if (--(new->u.ref) == 0) {
		    o_delete(new);
		}
		break;
	    }
	}
    } else if (!(new->flags & O_MASTER)) {
	new->update = new->u.master->update;
    }
}

/*
 * NAME:	object->del()
 * DESCRIPTION:	delete an object
 */
void o_del(o)
register object *o;
{
# ifdef DEBUG
    if (o->count == 0) {
	fatal("destructing destructed object");
    }
# endif
    o->count = 0;

    if (o->flags & O_MASTER) {
	/* remove from object name hash table */
	*ht_lookup(htab, o->chain.name, FALSE) = o->chain.next;

	if (--(o->u.ref) == 0) {
	    o_delete(o);
	}
    } else {
	register object *m;

	/* clone */
	m = o->u.master;
	if (m->update == o->update) {
	    m->cref--;
	    if (--(m->u.ref) == 0) {
		o_delete(m);
	    }
	} else {
	    /* non-upgraded clone of old issue */
	    do {
		m = &otab[m->prev];
	    } while (m->update != o->update);
	    if (--(m->u.ref) == 0) {
		/* put old issue in deleted list */
		m->u.master = dest_obj;
		dest_obj = m;

		m = o->u.master;
		if (--(m->u.ref) == 0) {
		    o_delete(m);
		}
	    }
	}
    }

    /* put in clean list */
    o->chain.next = (hte *) clean_obj;
    clean_obj = o;
}

/*
 * NAME:	object->object()
 * DESCRIPTION:	get an object, given index and count
 */
object *o_object(idx, count)
unsigned int idx;
Uint count;
{
    register object *o;

    o = &otab[idx];
    return (o->count == count) ? o : (object *) NULL;
}

/*
 * NAME:	object->objref()
 * DESCRIPTION:	get a (possibly destructed) object, given the object index only
 */
object *o_objref(idx)
unsigned int idx;
{
    return &otab[idx];
}

/*
 * NAME:	object->name()
 * DESCRIPTION:	return the name of an object
 */
char *o_name(o)
register object *o;
{
    if (o->chain.name != (char *) NULL) {
	return o->chain.name;
    } else {
	static char name[STRINGSZ + 12];

	/*
	 * return the name of the master object with the index appended
	 */
	sprintf(name, "%s#%u", o->u.master->chain.name, o->index);
	return name;
    }
}

/*
 * NAME:	object->find()
 * DESCRIPTION:	find an object by name
 */
object *o_find(name)
char *name;
{
    char *hash;

    hash = strchr(name, '#');
    if (hash != (char *) NULL) {
	register char *p;
	register unsigned long number;
	register object *o;

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
	    if (number >= nobjects) {
		return (object *) NULL;
	    }
	} while (*p != '\0');

	o = &otab[number];
	if (o->count == 0 || (o->flags & O_MASTER) ||
	    strncmp(name, o->u.master->chain.name, hash - name) != 0 ||
	    o->u.master->chain.name[hash - name] != '\0') {
	    /*
	     * no entry, not a clone, or object name doesn't match
	     */
	    return (object *) NULL;
	}
	return o;
    } else {
	/* look it up in the hash table */
	return (object *) *ht_lookup(htab, name, TRUE);
    }
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
	o = o->u.master;	/* get control block of master object */
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
	if (o->dfirst == SW_UNUSED) {
	    /* create new dataspace block */
	    o->data = d_new_dataspace(o);
	} else {
	    /* load dataspace block */
	    o->data = d_load_dataspace(o);
	}
    } else {
	d_ref_dataspace(o->data);
    }
    return o->data;
}

/*
 * NAME:	object->clean()
 * DESCRIPTION:	clean up the object table
 */
void o_clean()
{
    register object *o, *next;

    for (o = clean_obj; o != (object *) NULL; o = next) {
	/* free dataspace block (if it exists) */
	if (o->data == (dataspace *) NULL && o->dfirst != SW_UNUSED) {
	    /* reload dataspace block (sectors are needed) */
	    o->data = d_load_dataspace(o);
	}
	if (o->data != (dataspace *) NULL) {
	    d_del_dataspace(o->data);
	}
	next = (object *) o->chain.next;

	if (!(o->flags & O_MASTER)) {
	    /* put clone in free list */
	    o->chain.next = (hte *) free_obj;
	    free_obj = o;
	    nfreeobjs++;
	}
    }
    clean_obj = (object *) NULL;

    for (o = upgrade_obj; o != (object *) NULL; o = (object *) o->chain.next) {
	register object *up;
	register control *ctrl;

	up = o->u.master;
	up->cref -= 2;

	if (up->count == 0 && up->cref == 0) {
	    /* also remove upgrader */
	    o->u.master = dest_obj;
	    dest_obj = o;
	} else {
	    /* upgrade objects */
	    up->flags &= ~O_COMPILED;
	    o->u.ref = up->u.ref;
	    ctrl = up->ctrl;

	    if (ctrl->vmapsize != 0 &&
		(up->data != (dataspace *) NULL || up->dfirst != SW_UNUSED ||
		 up->count == 0 || --(o->u.ref) != 0)) {
		/* upgrade variables */
		o->cref = o->index = up->index;
		o->prev = up->prev;
		if (o->prev != SW_UNUSED) {
		    otab[o->prev].cref = o - otab;
		}
		o->update = up->update;

		up->prev = o - otab;
		up->cref = 1;
		up->u.ref = 1;
		up->update++;
		if (up->count != 0) {
		    up->u.ref++;
		    if (up->data == (dataspace *) NULL &&
			up->dfirst != SW_UNUSED) {
			/* load dataspace (with old control block) */
			up->data = d_load_dataspace(up);
		    }
		}
	    } else {
		/* no variable upgrading */
		o->u.master = dest_obj;
		dest_obj = o;
	    }

	    /* swap control blocks */
	    up->ctrl = o->ctrl;
	    up->ctrl->obj = up;
	    o->ctrl = ctrl;
	    ctrl->obj = o;
	    o->cfirst = up->cfirst;
	    up->cfirst = SW_UNUSED;

	    if (ctrl->ndata != 0) {
		/* upgrade all dataspaces in memory */
		d_upgrade_all(o, up);
	    }
	}
    }
    upgrade_obj = (object *) NULL;

    for (o = dest_obj; o != (object *) NULL; o = next) {
	/* free control block */
	d_del_control(o_control(o));

	if (o->chain.name != (char *) NULL) {
	    /* free object name */
	    FREE(o->chain.name);
	    o->chain.name = (char *) NULL;
	}

	/* put master object in free list */
	o->chain.next = (hte *) free_obj;
	free_obj = o;
	nfreeobjs++;

	next = o->u.master;
	o->u.ref = 0;
    }
    dest_obj = (object *) NULL;
}

/*
 * NAME:	object->count()
 * DESCRIPTION:	return the number of objects in use
 */
uindex o_count()
{
    return nobjects - nfreeobjs;
}


typedef struct {
    object *otab;	/* object table pointer */
    object *free_obj;	/* free object list */
    uindex nobjects;	/* # objects */
    uindex nfreeobjs;	/* # free objects */
    Int count;		/* object count */
    long onamelen;	/* length of all object names */
} dump_header;

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
    register int len, buflen;
    dump_header dh;
    char buffer[CHUNKSZ];

    /* prepare header */
    dh.otab = otab;
    dh.free_obj = free_obj;
    dh.nobjects = nobjects;
    dh.nfreeobjs = nfreeobjs;
    dh.count = count;
    dh.onamelen = 0;
    for (i = nobjects, o = otab; i > 0; --i, o++) {
	if (o->chain.name != (char *) NULL) {
	    dh.onamelen += strlen(o->chain.name) + 1;
	}
    }

    /* write header and objects */
    if (write(fd, (char *) &dh, sizeof(dump_header)) < 0 ||
	write(fd, (char *) otab, nobjects * sizeof(object)) < 0) {
	return FALSE;
    }

    /* write object names */
    buflen = 0;
    for (i = nobjects, o = otab; i > 0; --i, o++) {
	if (o->chain.name != (char *) NULL) {
	    len = strlen(o->chain.name) + 1;
	    if (buflen + len > CHUNKSZ) {
		if (write(fd, buffer, buflen) < 0) {
		    return FALSE;
		}
		buflen = 0;
	    }
	    memcpy(buffer + buflen, o->chain.name, len);
	    buflen += len;
	}
    }
    return (buflen == 0 || write(fd, buffer, buflen) >= 0);
}

/*
 * NAME:	object->restore()
 * DESCRIPTION:	restore the object table
 */
void o_restore(fd)
int fd;
{
    register uindex i;
    register object *o;
    register int len, buflen;
    register char *p;
    dump_header dh;
    long offset;
    char buffer[CHUNKSZ];

    /*
     * Free object names of precompiled objects.
     */
    for (i = nobjects, o = otab; i > 0; --i, o++) {
	*ht_lookup(htab, o->chain.name, FALSE) = o->chain.next;
	FREE(o->chain.name);
    }

    /* deal with header */
    if (read(fd, (char *) &dh, sizeof(dump_header)) != sizeof(dump_header) ||
	dh.nobjects > otabsize ||
	read(fd, (char *) otab, dh.nobjects * sizeof(object)) !=
						dh.nobjects * sizeof(object)) {
	fatal("cannot restore objects");
    }
    offset = (long) otab - (long) dh.otab;
    if (dh.free_obj == (object *) NULL) {
	free_obj = (object *) NULL;
    } else {
	free_obj = (object *) ((char *) dh.free_obj + offset);
    }
    nobjects = dh.nobjects;
    nfreeobjs = dh.nfreeobjs;
    count = dh.count;

    /* read object names, and patch all objects and control blocks */
    buflen = 0;
    for (i = nobjects, o = otab; i > 0; --i, o++) {
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
		if (read(fd, buffer + buflen, len) != len) {
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

		h = ht_lookup(htab, p, FALSE);
		o->chain.next = *h;
		*h = (hte *) o;
	    }
	    p += len;
	    buflen -= len;
	}

	if (o->count != 0) {
	    /* there are no user or editor objects after a restore */
	    o->flags &= ~(O_USER | O_EDITOR | O_PENDIO);
	} else if (!(o->flags & O_MASTER) || o->u.ref == 0) {
	    /* free object slot */
	    if (offset != 0 && o->chain.next != (hte *) NULL) {
		/* patch free object list pointer */
		o->chain.next = (hte *) ((char *) o->chain.next + offset);
	    }
	    continue;
	}

	o->ctrl = (control *) NULL;
	o->data = (dataspace *) NULL;
	if (offset != 0 && !(o->flags & O_MASTER)) {
	    /* patch master object reference */
	    o->u.master = (object *) ((char *) o->u.master + offset);
	}

	/* check memory */
	if (!m_check()) {
	    m_purge();
	}
    }
}
