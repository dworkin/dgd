# define INCLUDE_FILE_IO
# include "dgd.h"
# include <ctype.h>
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"

static object *otab;		/* object table */
static uindex otabsize;		/* size of object table */
static hashtab *htab;		/* object name hash table */
static object *clean_obj;	/* list of objects to clean */
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
unsigned int n;
{
    otab = ALLOC(object, otabsize = n);
    htab = ht_new(OBJTABSZ, OBJHASHSZ);
    free_obj = (object *) NULL;
    nobjects = 0;
    nfreeobjs = 0;
    count = 0;
}

/*
 * NAME:	object->new()
 * DESCRIPTION:	create a new object. If the master argument is non-NULL, the
 *		new object is a clone of this master object; otherwise it is
 *		a new master object.
 */
object *o_new(name, master, ctrl)
char *name;
object *master;
control *ctrl;
{
    register object *o;

    /* allocate object */
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
	o = &otab[nobjects];
	o->index = nobjects++;
    }
    o->count = ++count;

    if (master == (object *) NULL) {
	register dinherit *inh;
	register int i;
	hte **h;

	/* put object in object name hash table */
	m_static();
	strcpy(o->chain.name = ALLOC(char, strlen(name) + 1), name);
	m_dynamic();
	h = ht_lookup(htab, name);
	o->chain.next = *h;
	*h = (hte *) o;
	o->flags = O_MASTER;
	o->ctrl = ctrl;
	ctrl->inherits[ctrl->ninherits - 1].obj = o;

	/* add reference to all inherited objects */
	o->u.ref = 0;	/* increased to 1 in following loop */
	inh = ctrl->inherits;
	for (i = ctrl->ninherits; i > 0; --i) {
	    (inh++)->obj->u.ref++;
	}
    } else {
	/* ignore the object name */
	o->chain.name = (char *) NULL;

	/* add reference to master object */
	o->flags = 0;
	(o->u.master = master)->u.ref++;
	o->ctrl = (control *) NULL;
    }
    o->data = (dataspace *) NULL;
    o->dfirst = SW_UNUSED;

    return o;
}

/*
 * NAME:	object->delete()
 * DESCRIPTION:	delete a reference to an object, and put it in the deleted
 *		list if it was the last reference
 */
static void o_delete(o)
register object *o;
{
    register int i;
    register dinherit *inh;

    if (--(o->u.ref) != 0) {
	/* last reference not removed yet */
	return;
    }

    /* callback to the system */
    (--sp)->type = T_STRING;
    str_ref(sp->u.string = str_new(NULL, strlen(o->chain.name) + 1));
    sp->u.string->text[0] = '/';
    strcpy(sp->u.string->text + 1, o->chain.name);
    if (i_call_critical("remove_program", 1, TRUE)) {
	i_del_value(sp++);
    }

    /* remove references to inherited objects too */
    if (o->ctrl == (control *) NULL) {
	o->ctrl = d_load_control(o);
    }
    inh = o->ctrl->inherits;
    i = o->ctrl->ninherits;
    while (--i > 0) {
	o_delete((inh++)->obj);
    }

    /* put in deleted list */
    o->u.master = dest_obj;
    dest_obj = o;
}

/*
 * NAME:	object->del()
 * DESCRIPTION:	delete an object
 */
void o_del(o)
register object *o;
{
    if (o->count == 0) {
	/* objects can only be destructed once */
	error("Destructing destructed object");
    }
    o->count = 0;

    if (o->flags & O_MASTER) {
	/* remove from object name hash table */
	*ht_lookup(htab, o->chain.name) = o->chain.next;

	o_delete(o);
    } else {
	o_delete(o->u.master);
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
	return (object *) *ht_lookup(htab, name);
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
 * DESCRIPTION:	deal with destructed objects
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

    for (o = dest_obj; o != (object *) NULL; o = o->u.master) {
	/* free control block */
	d_del_control(o_control(o));

	/* free object name */
	FREE(o->chain.name);
	o->chain.name = (char *) NULL;

	/* put master object in free list */
	o->chain.next = (hte *) free_obj;
	free_obj = o;
	nfreeobjs++;
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
	*ht_lookup(htab, o->chain.name) = o->chain.next;
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

		h = ht_lookup(htab, p);
		o->chain.next = *h;
		*h = (hte *) o;
	    }
	    p += len;
	    buflen -= len;
	}

	if (o->count != 0) {
	    /* there are no user or editor objects after a restore */
	    o->flags &= ~(O_USER | O_EDITOR);
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
	if (offset != 0 && !(o->flags & O_COMPILED)) {
	    if (o->flags & O_MASTER) {
		/* patch inherits */
		d_patch_ctrl(o->ctrl = d_load_control(o), offset);
		d_swapout(1);
	    } else {
		/* patch master object reference */
		o->u.master = (object *) ((char *) o->u.master + offset);
	    }
	}

	/* check memory */
	if (!m_check()) {
	    m_purge();
	}
    }
}
