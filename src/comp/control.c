# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "fcontrol.h"
# include "hash.h"
# include "table.h"
# include "node.h"
# include "compile.h"
# include "control.h"

typedef struct _oh_ {		/* object hash table */
    hte chain;			/* hash table chain */
    object *obj;		/* object */
    short index;		/* -1: direct; 0: new; 1: indirect */
    struct _oh_ **next;		/* next in linked list */
} oh;

static hashtab *otab;		/* object hash table */
static oh **olist;		/* list of all object hash table entries */

/*
 * NAME:	oh->init()
 * DESCRIPTION:	initialize the object hash table
 */
static void oh_init()
{
    otab = ht_new(OMERGETABSZ, OBJHASHSZ);
}

/*
 * NAME:	oh->new()
 * DESCRIPTION:	put an object in the hash table
 */
static oh *oh_new(name)
char *name;
{
    register oh **h;

    h = (oh **) ht_lookup(otab, name);
    if (*h == (oh *) NULL) {
	/*
	 * new object
	 */
	*h = ALLOC(oh, 1);
	(*h)->chain.next = (hte *) NULL;
	(*h)->chain.name = name;
	(*h)->index = 0;		/* new object */
	(*h)->next = olist;
	olist = h;
    }

    return *h;
}

/*
 * NAME:	oh->clear()
 * DESCRIPTION:	clear the object hash table
 */
static void oh_clear()
{
    register oh **h, *f;

    for (h = olist; h != (oh **) NULL; ) {
	f = *h;
	h = f->next;
	FREE(f);
    }
    olist = (oh **) NULL;

    ht_del(otab);
}


# define VFH_CHUNK	64

typedef struct _vfh_ {		/* variable/function hash table */
    hte chain;			/* hash table chain */
    string *str;		/* name string */
    oh *ohash;			/* controlling object hash table entry */
    unsigned short ct;		/* function call, or variable type */
    short index;		/* definition table index */
    struct _vfh_ **next;	/* next in list */
} vfh;

typedef struct _vfhchunk_ {
    vfh vf[VFH_CHUNK];		/* vfh chunk */
    struct _vfhchunk_ *next;	/* next in linked list */
} vfhchunk;

static vfh **vfhlist;		/* linked list of all vfh entries */
static vfhchunk *vfhclist;	/* linked list of all vfh chunks */
static int vfhchunksz = VFH_CHUNK;	/* size of current vfh chunk */

/*
 * NAME:	vfh->new()
 * DESCRIPTION:	create a new vfh table element
 */
static void vfh_new(str, ohash, ct, idx, addr)
string *str;
oh *ohash;
unsigned short ct;
short idx;
vfh **addr;
{
    register vfh *h;

    if (vfhchunksz == VFH_CHUNK) {
	register vfhchunk *l;

	l = ALLOC(vfhchunk, 1);
	l->next = vfhclist;
	vfhclist = l;
	vfhchunksz = 0;
    }
    h = &vfhclist->vf[vfhchunksz++];
    h->chain.next = (hte *) *addr;
    h->chain.name = str->text;
    str_ref(h->str = str);
    h->ohash = ohash;
    h->ct = ct;
    h->index = idx;
    h->next = vfhlist;
    vfhlist = addr;
    *addr = h;
}

/*
 * NAME:	vfh->clear()
 * DESCRIPTION:	clear the vfh tables
 */
static void vfh_clear()
{
    register vfh **h;
    register vfhchunk *l;

    for (h = vfhlist; h != (vfh **) NULL; ) {
	register vfh *f;

	f = *h;
	*h = (vfh *) f->chain.next;
	h = f->next;
	str_del(f->str);
    }
    vfhlist = (vfh **) NULL;

    for (l = vfhclist; l != (vfhchunk *) NULL; ) {
	register vfhchunk *f;

	f = l;
	l = l->next;
	FREE(f);
    }
    vfhclist = (vfhchunk *) NULL;
    vfhchunksz = VFH_CHUNK;
}


typedef struct _lab_ {
    string *str;		/* label */
    oh *ohash;			/* entry in hash table */
    struct _lab_ *next;		/* next label */
} lab;

static lab *labels;		/* list of labeled inherited objects */

/*
 * NAME:	lab->new()
 * DESCRIPTION:	declare a new inheritance label
 */
static void lab_new(str, ohash)
string *str;
oh *ohash;
{
    register lab *l;

    l = ALLOC(lab, 1);
    str_ref(l->str = str);
    l->ohash = ohash;
    l->next = labels;
    labels = l;
}

/*
 * NAME:	lab->find()
 * DESCRIPTION:	find a labeled object in the list
 */
static oh *lab_find(name)
char *name;
{
    register lab *l;

    for (l = labels; l != (lab *) NULL; l = l->next) {
	if (strcmp(l->str->text, name) == 0) {
	    return l->ohash;
	}
    }
    return (oh *) NULL;
}

/*
 * NAME:	lab->clear()
 * DESCRIPTION:	wipe out all inheritance label declarations
 */
static void lab_clear()
{
    register lab *l, *f;

    l = labels;
    while (l != (lab *) NULL) {
	str_del(l->str);
	f = l;
	l = l->next;
	FREE(f);
    }
    labels = (lab *) NULL;
}


/*
 * NAME:	cmp_proto()
 * DESCRIPTION:	Compare two prototypes. Return TRUE if equal.
 */
static bool cmp_proto(prot1, prot2)
register char *prot1, *prot2;
{
    register int i;

    /* check if either prototype is implicit */
    if (PROTO_FTYPE(prot1) == T_IMPLICIT || PROTO_FTYPE(prot2) == T_IMPLICIT) {
	return TRUE;
    }

    /* check if classes are equal */
    if ((PROTO_CLASS(prot1) ^ PROTO_CLASS(prot2)) &
	UCHAR(~(C_COMPILED | C_LOCAL | C_UNDEFINED))) {
	return FALSE;
    }

    /* compare return type */
    if (PROTO_FTYPE(prot1) != PROTO_FTYPE(prot2)) {
	return FALSE;
    }

    /* check if the number of arguments is equal */
    if ((i=PROTO_NARGS(prot1)) != PROTO_NARGS(prot2)) {
	return FALSE;
    }

    /* compare argument types */
    for (prot1 = PROTO_ARGS(prot1), prot2 = PROTO_ARGS(prot2); i > 0; --i) {
	if (*prot1++ != *prot2++) {
	    return FALSE;
	}
    }

    return TRUE;	/* equal */
}


# define MAX_INHERITS		255

static oh *directs[MAX_INHERITS];	/* direct inherit table */
static int ndirects;			/* # directly inh. objects */
static int ninherits;			/* # inherited objects */
static char *auto_name;			/* name of auto object */
static char *driver_name;		/* name of driver object */
static hashtab *vtab;			/* variable merge table */
static hashtab *ftab;			/* function merge table */
static unsigned short nvars;		/* # variables */
static unsigned short nfloats;		/* # float variables */
static unsigned short nsymbs;		/* # symbols */
static int nfclash;			/* # prototype clashes */
static uindex nfcalls;			/* # function calls */

/*
 * NAME:	control->init()
 * DESCRIPTION:	initialize control block construction
 */
void ctrl_init(auto_obj, driver_obj)
char *auto_obj, *driver_obj;
{
    auto_name = auto_obj;
    driver_name = driver_obj;
    oh_init();
    vtab = ht_new(VFMERGETABSZ, VFMERGEHASHSZ);
    ftab = ht_new(VFMERGETABSZ, VFMERGEHASHSZ);
}

/*
 * NAME:	control->vardefs()
 * DESCRIPTION:	put variable definitions from an inherited object into the
 *		variable merge table
 */
static void ctrl_vardefs(ohash, ctrl)
oh *ohash;
register control *ctrl;
{
    register dvardef *v;
    register int n;
    register string *str;
    register vfh **h;

    v = d_get_vardefs(ctrl);
    for (n = 0; n < ctrl->nvardefs; n++) {
	if (v->type == T_FLOAT) {
	    nfloats++;	/* inherited float variable */
	}
	/*
	 * Add only non-private variables, and check if a variable with the
	 * same name hasn't been inherited already.
	 */
	if (!(v->class & C_PRIVATE)) {
	    str = d_get_strconst(ctrl, v->inherit, v->index);
	    h = (vfh **) ht_lookup(vtab, str->text);
	    if (*h == (vfh *) NULL) {
		/* new variable */
		vfh_new(str, ohash, v->type, n, h);
	    } else {
	       /* duplicate variable */
	       c_error("multiple inheritance of variable %s (/%s, /%s)",
		       str->text, (*h)->ohash->chain.name, ohash->chain.name);
	    }
	}
	v++;
    }
}

/*
 * NAME:	control->funcdef()
 * DESCRIPTION:	put a function definition from an inherited object into
 *		the function merge table
 */
static void ctrl_funcdef(ctrl, idx, ohash)
register control *ctrl;
register int idx;
oh *ohash;
{
    register vfh **h, **l;
    register dfuncdef *f;
    string *str;

    f = &ctrl->funcdefs[idx];
    str = d_get_strconst(ctrl, f->inherit, f->index);
    h = (vfh **) ht_lookup(ftab, str->text);
    if (*h == (vfh *) NULL) {
	/*
	 * New function (-1: no calls to it yet)
	 */
	vfh_new(str, ohash, -1, idx, h);
	if (ctrl->ninherits != 1 ||
	    (f->class & (C_STATIC | C_UNDEFINED)) != C_STATIC) {
	    /* don't count static functions from the auto object */
	    nsymbs++;
	}
    } else if ((*h)->ohash != ohash) {
	register dinherit *inh;
	register int n;
	object *o;
	char *prot1, *prot2;
	vfh **marker;

	/*
	 * Different prototypes.
	 */
	if ((*h)->ohash == (oh *) NULL) {
	    /*
	     * Skip old clash marker.
	     */
	    marker = h;
	    h = (vfh **) &(*h)->chain.next;
	} else {
	    marker = (vfh **) NULL;
	}

	prot1 = ctrl->prog + f->offset;
	/*
	 * compare with the other prototypes
	 */
	for (l = h;
	     *l != (vfh *) NULL && strcmp((*l)->chain.name, str->text) == 0;
	     l = (vfh **) &(*l)->chain.next) {
	    /*
	     * First check if the function in the merge table is in
	     * an object inherited by the currently inherited object.
	     */
	    o = (*l)->ohash->obj;
	    ctrl = ohash->obj->ctrl;
	    inh = ctrl->inherits;
	    n = ctrl->ninherits;
	    ctrl = o->ctrl;
	    while (--n != 0) {
		if (o == inh->obj) {
		    /*
		     * redefined inherited function
		     */
		    if (marker == (vfh **) NULL && ctrl->ninherits == 1 &&
			(ctrl->funcdefs[(*l)->index].class &
				  (C_STATIC | C_UNDEFINED)) == C_STATIC) {
			/*
			 * redefine static function in auto object
			 */
			nsymbs++;
		    }
		    (*l)->ohash = ohash;
		    (*l)->index = idx;
		    return;
		}
		inh++;
	    }

	    /*
	     * Now check if the function in the currently inherited object is
	     * inherited by the object that defines the function in the merge
	     * table.
	     */
	    o = inh->obj;
	    inh = ctrl->inherits;
	    n = ctrl->ninherits;
	    while (--n != 0) {
		if (o == inh->obj) {
		    /*
		     * previously inherited, and redefined function
		     */
		    return;
		}
		inh++;
	    }

	    /*
	     * Check for clashes between the prototypes.
	     */
	    prot2 = ctrl->prog + ctrl->funcdefs[(*l)->index].offset;
	    if (((f->class | PROTO_CLASS(prot2)) & (C_NOMASK | C_UNDEFINED)) ==
								    C_NOMASK) {
		/*
		 * a nomask function is inherited more than once
		 */
		c_error("multiple inheritance of nomask function %s (/%s, /%s)",
			str->text, (*l)->ohash->chain.name, ohash->chain.name);
		return;
	    }
	    if (((f->class | PROTO_CLASS(prot2)) & C_UNDEFINED) &&
		!cmp_proto(prot1, prot2)) {
		/*
		 * prototype conflict
		 */
		c_error("unequal prototypes for function %s (/%s, /%s)",
			str->text, (*l)->ohash->chain.name, ohash->chain.name);
		return;
	    }
	}

	ctrl = (*h)->ohash->obj->ctrl;
	prot2 = ctrl->prog + ctrl->funcdefs[(*h)->index].offset;
	if (PROTO_CLASS(prot2) & C_UNDEFINED) {
	    /*
	     * replace undefined function
	     */
	    vfh_new(str, ohash, -1, idx, h);
	    return;
	}
	if (f->class & C_UNDEFINED) {
	    /*
	     * add undefined prototype at the end
	     */
	    vfh_new(str, ohash, -1, idx, l);
	    return;
	}

	if (marker != (vfh **) NULL) {
	    /*
	     * replace clash marker with function
	     */
	    h = marker;
	    (*h)->ohash = ohash;
	    (*h)->ct = -1;
	    (*h)->index = idx;
	} else {
	    /*
	     * first clash: insert before old prototype
	     */
	    nfclash++;
	    vfh_new(str, ohash, -1, idx, h);
	}

	/* insert new clash marker */
	vfh_new(str, (oh *) NULL, 0, 0, h);
    }
}

/*
 * NAME:	control->funcdefs()
 * DESCRIPTION:	put function definitions from an inherited object into
 *		the function merge table
 */
static void ctrl_funcdefs(ctrl)
register control *ctrl;
{
    register unsigned short n;

    if (ctrl->ninherits == 1) {
	register dfuncdef *f;
	register oh *ohash;

	/*
	 * For the auto object, add non-private functions from the function
	 * definition table.
	 */
	f = ctrl->funcdefs;
	ohash = oh_new(ctrl->inherits->obj->chain.name);
	for (n = ctrl->nfuncdefs, f = ctrl->funcdefs + n; n > 0; ) {
	    --n;
	    --f;
	    if (!(f->class & C_PRIVATE)) {
		ctrl_funcdef(ctrl, n, ohash);
	    }
	}
    } else {
	register object *o;
	register dsymbol *symb;

	/*
	 * The symbol table rather than the function definition table is
	 * used here.
	 */
	for (n = ctrl->nsymbols, symb = d_get_symbols(ctrl); n > 0; --n, symb++)
	{
	    o = ctrl->inherits[UCHAR(symb->inherit)].obj;
	    ctrl_funcdef(o->ctrl, UCHAR(symb->index), oh_new(o->chain.name));
	}
    }
}

/*
 * NAME:	control->inherit()
 * DESCRIPTION:	inherit an object
 */
bool ctrl_inherit(obj, label)
object *obj;
string *label;
{
    register oh *ohash;

    ohash = oh_new(obj->chain.name);
    if (label != (string *) NULL) {
	/*
	 * use a label
	 */
	if (lab_find(label->text) != (oh *) NULL) {
	    c_error("redeclaration of label %s", label->text);
	}
	lab_new(label, ohash);
    }

    if (ohash->index == 0) {
	register control *ctrl;
	register object *o;
	dinherit *inh;
	register int i, n;

	/*
	 * new inherited object
	 */
	ctrl = o_control(obj);
	for (i = ctrl->ninherits, inh = ctrl->inherits; i > 0; --i) {
	    /*
	     * check all the objects inherited by the object now inherited
	     */
	    o = (inh++)->obj;
	    if (o->count == 0) {
		/*
		 * This object inherits an object that has been destructed.
		 * Give the driver object a chance to destruct it.
		 */
		i_check_stack(1);
		(--sp)->type = T_OBJECT;
		sp->oindex = obj->index;
		sp->u.objcnt = obj->count;
		call_driver_object("recompile", 1);
		i_del_value(sp++);
		if (obj->count == 0) {
		    return FALSE;	/* recompile this object */
		}
	    }

	    ohash = oh_new(o->chain.name);
	    if (ohash->index == 0) {
		/*
		 * inherit a new object
		 */
		ohash->obj = o;
		ohash->index = 1;
		ctrl = o_control(o);

		/*
		 * put variables in variable merge table
		 */
		ctrl_vardefs(ohash, ctrl);
		/*
		 * ensure that relevant parts are loaded
		 */
		d_get_prog(ctrl);
		d_get_funcdefs(ctrl);
	    } else if (ohash->obj != o) {
		/*
		 * inherited two different objects with same name
		 */
		c_error("inherited different instances of /%s",
			o->chain.name);
		return TRUE;
	    } else if (ohash->index < 0 && ohash->obj->ctrl->ninherits > 1) {
		/*
		 * Inherit an object which previously was inherited
		 * directly (but is not the auto object). Mark it as
		 * indirect now.
		 */
		ohash->index = 1;
		n = ohash->obj->ctrl->ninherits - 1;
		ninherits -= n;
	    }
	}

	n = ctrl->ninherits;
	if (n > 1) {
	    /*
	     * Don't count the auto object, unless it is the auto object
	     * only.
	     */
	    --n;
	}
	ninherits += n;
	directs[ndirects++] = ohash;
	ohash->index = -1;	/* direct */

	/*
	 * put functions in function merge table
	 */
	ctrl_funcdefs(ctrl);

    } else if (ohash->obj != obj) {
	/*
	 * inherited two objects with same name
	 */
	c_error("inherited different instances of /%s", obj->chain.name);
    }

    if (ninherits >= MAX_INHERITS) {
	c_error("too many objects inherited");
    }

    return TRUE;
}


# define STRING_CHUNK	64

typedef struct _strchunk_ {
    string *s[STRING_CHUNK];		/* chunk of strings */
    struct _strchunk_ *next;		/* next in string chunk list */
} strchunk;

# define FCALL_CHUNK	64

typedef struct _fcchunk_ {
    char *f[FCALL_CHUNK];		/* function reference */
    struct _fcchunk_ *next;		/* next in fcall chunk list */
} fcchunk;

typedef struct _cfunc_ {
    dfuncdef func;			/* function name/type */
    char *proto;			/* function prototype */
    char *prog;				/* fuction program */
    unsigned short progsize;		/* fuction program size */
} cfunc;

static char *name;			/* name of object compiled */
static bool is_auto;			/* true if compiling the auto object */
static control *newctrl;		/* the new control block */
static oh *newohash;			/* fake ohash entry for new object */
static strchunk *strlist;		/* list of string chunks */
static int strchunksz = STRING_CHUNK;	/* size of current string chunk */
static unsigned short nstrs;		/* # of strings in all string chunks */
static fcchunk *fclist;			/* list of fcall chunks */
static int fcchunksz = FCALL_CHUNK;	/* size of current fcall chunk */
static cfunc *functions;		/* defined functions table */
static int nfdefs, fdef;		/* # defined functions, current func */
static unsigned short progsize;		/* size of all programs and protos */
static dvardef *variables;		/* defined variables */

/*
 * NAME:	control->create()
 * DESCRIPTION:	make an initial control block
 */
void ctrl_create(file)
char *file;
{
    register dinherit *new;
    register control *ctrl;
    register unsigned short n;
    register int i, count;

    name = file;
    is_auto = (strcmp(file, auto_name) == 0);

    /*
     * create a new control block
     */
    newohash = oh_new(file);
    newohash->index = count = ninherits;
    newctrl = d_new_control();
    new = newctrl->inherits = ALLOC(dinherit, newctrl->ninherits = count + 1);
    new += count;
    nfcalls = 0;
    nvars = 0;

    if (ninherits > 0) {
	/*
	 * initialize the virtually inherited objects
	 */
	for (n = ndirects; n > 0; ) {
	    register oh *ohash;
	    register dinherit *old;

	    ohash = directs[--n];
	    if (ohash->index < 0) {		/* directly inherited */
		ctrl = ohash->obj->ctrl;
		i = ctrl->ninherits - 1;
		old = ctrl->inherits + i;
		/*
		 * do this ctrl->ninherits - 1 times, but at least once
		 */
		do {
		    ohash = oh_new(old->obj->chain.name);
		    --old;
		    (--new)->obj = ohash->obj;
		    ohash->index = --count;	/* may happen more than once */
		} while (--i > 0);
	    }
	}

	/*
	 * Fix function offsets and variable offsets, and collect all string
	 * constants from inherited objects and put them in the string merge
	 * table.
	 */
	for (count = 0; count < ninherits; count++) {
	    i = oh_new(new->obj->chain.name)->index;
	    if (i == count) {
		ctrl = new->obj->ctrl;
		i = ctrl->ninherits - 1;
		new->funcoffset = nfcalls;
		nfcalls += ctrl->nfuncalls - ctrl->inherits[i].funcoffset;
		new->varoffset = nvars;
		nvars += ctrl->nvardefs;

		for (n = ctrl->nstrings; n > 0; ) {
		    --n;
		    str_put(d_get_strconst(ctrl, i, n),
			    ((long) count << 16) | n);
		}
	    } else {
		new->funcoffset = newctrl->inherits[i].funcoffset;
		new->varoffset = newctrl->inherits[i].varoffset;
	    }
	    new++;
	}
    }

    /*
     * stats for new object
     */
    new->obj = (object *) NULL;
    new->funcoffset = nfcalls;
    new->varoffset = newctrl->nvariables = nvars;
    newctrl->nfloats = nfloats;

    /*
     * prepare for construction of a new control block
     */
    functions = ALLOC(cfunc, 256);
    variables = ALLOC(dvardef, 256);
    progsize = 0;
    nstrs = 0;
    nfdefs = 0;
    nfcalls = 0;
    nvars = 0;
    nfloats = 0;
}

/*
 * NAME:	control->dstring()
 * DESCRIPTION:	define a new (?) string constant
 */
long ctrl_dstring(str)
string *str;
{
    register long desc, new;

    desc = str_put(str, new = ((long) ninherits << 16) | nstrs);
    if (desc == new) {
	/*
	 * it is really a new string
	 */
	if (strchunksz == STRING_CHUNK) {
	    register strchunk *l;

	    l = ALLOC(strchunk, 1);
	    l->next = strlist;
	    strlist = l;
	    strchunksz = 0;
	}
	str_ref(strlist->s[strchunksz++] = str);
	nstrs++;
    }
    if (desc >> 16 == ninherits) {
	desc |= 0x01000000L;	/* mark it as new */
    }
    return desc;
}

/*
 * NAME:	control->dproto()
 * DESCRIPTION:	define a new function prototype
 */
void ctrl_dproto(str, proto)
register string *str;
register char *proto;
{
    register vfh **h;
    register dfuncdef *func;
    register int i;
    long s;

    i = -1;	/* default: no calls yet */

    /* first check if prototype exists already */
    h = (vfh **) ht_lookup(ftab, str->text);
    if (*h != (vfh *) NULL) {
	register char *proto2;
	register control *ctrl;

	/*
	 * redefinition
	 */
	if ((*h)->ohash == newohash) {
	    /*
	     * redefinition of new function
	     */
	    proto2 = functions[(*h)->index].proto;
	    if (!((PROTO_CLASS(proto) | PROTO_CLASS(proto2)) & C_UNDEFINED)) {
		/*
		 * both prototypes are from functions
		 */
		c_error("multiple declaration of function %s", str->text);
	    } else if (!cmp_proto(proto, proto2)) {
		if ((PROTO_CLASS(proto) ^ PROTO_CLASS(proto2)) & C_UNDEFINED) {
		    /*
		     * declaration does not match prototype
		     */
		    c_error("declaration does not match prototype of %s",
			    str->text);
		} else {
		    /*
		     * unequal prototypes
		     */
		    c_error("unequal prototypes for function %s", str->text);
		}
	    } else if (!(PROTO_CLASS(proto) & C_UNDEFINED) ||
		       PROTO_FTYPE(proto2) == T_IMPLICIT) {
		/*
		 * replace undefined prototype
		 */
		progsize -= PROTO_SIZE(proto2);
		FREE(proto2);
		if (is_auto &&
		    (PROTO_CLASS(proto) & (C_STATIC | C_PRIVATE | C_UNDEFINED))
								== C_STATIC) {
		    /* static function in auto object replaces prototype */
		    PROTO_CLASS(proto) |= C_LOCAL;
		    --nsymbs;
		}
		i = PROTO_SIZE(proto);
		functions[fdef = (*h)->index].proto =
				    (char *) memcpy(ALLOC(char, i), proto, i);
		functions[fdef].func.class = PROTO_CLASS(proto);
		progsize += PROTO_SIZE(proto);
	    }
	    return;
	}

	/*
	 * redefinition of inherited function
	 */
	if ((*h)->ohash == (oh *) NULL) {
	    if (!(PROTO_CLASS(proto) & C_PRIVATE)) {
		/*
		 * mask inherited functions
		 */
		--nfclash;
	    }
	} else {
	    ctrl = (*h)->ohash->obj->ctrl;
	    proto2 = ctrl->prog + ctrl->funcdefs[(*h)->index].offset;
	    if ((PROTO_CLASS(proto2) & (C_NOMASK | C_UNDEFINED)) == C_NOMASK) {
		/*
		 * attempt to redefine nomask function
		 */
		c_error("redeclaration of nomask function %s (/%s)",
			str->text, (*h)->ohash->chain.name);
	    } else if ((PROTO_CLASS(proto2) & C_UNDEFINED) &&
		       !cmp_proto(proto, proto2)) {
		/*
		 * declaration does not match inherited prototype
		 */
		c_error("declaration does not match prototype of %s (/%s)",
			str->text, (*h)->ohash->chain.name);
	    } else if ((PROTO_CLASS(proto) & C_UNDEFINED) &&
		       PROTO_FTYPE(proto2) != T_IMPLICIT &&
		       cmp_proto(proto, proto2)) {
		/*
		 * there is no point in replacing an identical prototype
		 */
		return;
	    }

	    if (!(PROTO_CLASS(proto) & C_PRIVATE) && ctrl->ninherits == 1 &&
		(PROTO_CLASS(proto2) & (C_STATIC | C_UNDEFINED)) == C_STATIC) {
		/*
		 * replace static function in auto object by non-private
		 * function
		 */
		nsymbs++;
	    }
	    i = (*h)->ct;	/* take old call index */
	}
	/*
	 * insert the definition before the old one in the hash table
	 */
    } else if (!(PROTO_CLASS(proto) & C_PRIVATE) && (!is_auto ||
	        (PROTO_CLASS(proto) & (C_STATIC | C_UNDEFINED)) != C_STATIC)) {
	/*
	 * add new prototype to symbol table
	 */
	nsymbs++;
    } else {
	/* won't hurt for private functions */
	PROTO_CLASS(proto) |= C_LOCAL;
    }

    if (nfdefs == 256) {
	c_error("too many functions declared");
	return;
    }

    /*
     * Actual definition.
     */
    vfh_new(str, newohash, i, nfdefs, h);
    s = ctrl_dstring(str);
    i = PROTO_SIZE(proto);
    functions[nfdefs].proto = (char *) memcpy(ALLOC(char, i), proto, i);
    functions[nfdefs].progsize = 0;
    progsize += i;
    func = &functions[nfdefs++].func;
    func->class = PROTO_CLASS(proto);
    func->inherit = s >> 16;
    func->index = s;
}

/*
 * NAME:	control->dfunc()
 * DESCRIPTION:	define a new function
 */
void ctrl_dfunc(str, proto)
string *str;
char *proto;
{
    fdef = nfdefs;
    ctrl_dproto(str, proto);
}

/*
 * NAME:	control->dprogram()
 * DESCRIPTION:	define a function body
 */
void ctrl_dprogram(prog, size)
char *prog;
unsigned short size;
{
    functions[fdef].prog = prog;
    functions[fdef].progsize = size;
    progsize += size;
}

/*
 * NAME:	control->dvar()
 * DESCRIPTION:	define a variable
 */
void ctrl_dvar(str, class, type)
string *str;
unsigned short class, type;
{
    register vfh **h;
    register dvardef *var;
    register long s;

    h = (vfh **) ht_lookup(vtab, str->text);
    if (*h != (vfh *) NULL) {
	if ((*h)->ohash == newohash) {
	    c_error("redeclaration of variable %s", str->text);
	    return;
	} else if (!(class & C_PRIVATE)) {
	    /*
	     * non-private redeclaration of a variable
	     */
	    c_error("redeclaration of variable %s (/%s)", str->text,
		    (*h)->ohash->chain.name);
	    return;
	}
    }
    if (nvars == 256) {
	c_error("too many variables declared");
	return;
    }

    /* actually define the variable */
    vfh_new(str, newohash, type, nvars, h);
    s = ctrl_dstring(str);
    var = &variables[nvars++];
    var->class = class;
    var->inherit = s >> 16;
    var->index = s;
    var->type = type;

    if (type == T_FLOAT) {
	nfloats++;
    }
}

/*
 * NAME:	control->ifcall()
 * DESCRIPTION:	call an inherited function
 */
char *ctrl_ifcall(str, label, call)
string *str;
char *label;
long *call;
{
    register control *ctrl;
    register oh *ohash;
    register short index;

    if (label != (char *) NULL) {
	register dsymbol *symb;

	/* first check if the label exists */
	ohash = lab_find(label);
	if (ohash == (oh *) NULL) {
	    c_error("undefined label %s", label);
	    return (char *) NULL;
	}
	symb = ctrl_symb(ohash->obj->ctrl, str->text);
	if (symb == (dsymbol *) NULL) {
	    /*
	     * It may seem strange to allow label::kfun, but remember that they
	     * are supposed to be inherited in the auto object.
	     */
	    index = kf_func(str->text);
	    if (index >= 0) {
		/* kfun call */
		*call = ((long) KFCALL << 24) | index;
		return KFUN(index).proto;
	    }
	    c_error("undefined function %s::%s", label, str->text);
	    return (char *) NULL;
	}
	ctrl = ohash->obj->ctrl;
	ohash = oh_new(ctrl->inherits[UCHAR(symb->inherit)].obj->chain.name);
	index = symb->index;
    } else {
	register vfh *h;

	/* check if the function exists */
	h = *(vfh **) ht_lookup(ftab, str->text);
	if (h == (vfh *) NULL || (h->ohash == newohash &&
	    ((h=(vfh *) h->chain.next) == (vfh *) NULL ||
	     strcmp(h->chain.name, str->text) != 0))) {

	    index = kf_func(str->text);
	    if (index >= 0) {
		/* kfun call */
		*call = ((long) KFCALL << 24) | index;
		return KFUN(index).proto;
	    }
	    c_error("undefined function ::%s", str->text);
	    return (char *) NULL;
	}
	ohash = h->ohash;
	if (ohash == (oh *) NULL) {
	    /*
	     * call to multiple inherited function
	     */
	    c_error("ambiguous function ::%s", str->text);
	    return (char *) NULL;
	}
	index = h->index;
	label = "";
    }

    ctrl = ohash->obj->ctrl;
    if (ctrl->funcdefs[index].class & C_UNDEFINED) {
	c_error("undefined function %s::%s", label, str->text);
	return (char *) NULL;
    }
    *call = ((long) DFCALL << 24) | ((long) ohash->index << 8) | index;
    return ctrl->prog + ctrl->funcdefs[index].offset;
}

/*
 * NAME:	control->fcall()
 * DESCRIPTION:	call a function
 */
char *ctrl_fcall(str, call, typechecking)
string *str;
long *call;
bool typechecking;
{
    register vfh *h;
    char *proto;

    h = *(vfh **) ht_lookup(ftab, str->text);
    if (h == (vfh *) NULL) {
	static char uproto[] = { C_UNDEFINED, T_IMPLICIT, 0 };
	register short kf;

	/*
	 * undefined function
	 */
	kf = kf_func(str->text);
	if (kf >= 0) {
	    /* kfun call */
	    *call = ((long) KFCALL << 24) | kf;
	    return KFUN(kf).proto;
	}

	/* created an undefined prototype for the function */
	if (nfcalls == 256) {
	    c_error("too many undefined functions");
	    return (char *) NULL;
	}
	ctrl_dproto(str, proto = uproto);
	h = *(vfh **) ht_lookup(ftab, str->text);
    } else if (h->ohash == newohash) {
	/*
	 * call to new function
	 */
	proto = functions[h->index].proto;
    } else if (h->ohash == (oh *) NULL) {
	/*
	 * call to multiple inherited function
	 */
	c_error("ambiguous function %s", str->text);
	return (char *) NULL;
    } else {
	register control *ctrl;

	/*
	 * call to inherited function
	 */
	ctrl = h->ohash->obj->ctrl;
	proto = ctrl->prog + ctrl->funcdefs[h->index].offset;
    }

    if (typechecking && PROTO_FTYPE(proto) == T_IMPLICIT) {
	/* don't allow calls to implicit prototypes when typechecking */
	c_error("undefined function %s", str->text);
	return (char *) NULL;
    }

    if (PROTO_CLASS(proto) & C_LOCAL) {
	/* direct call */
	*call = ((long) DFCALL << 24) | ((long) h->ohash->index << 8) |
		h->index;
    } else {
	/* ordinary function call */
	if (h->ct == (unsigned short) -1) {
	    /*
	     * add to function call table
	     */
	    if (fcchunksz == FCALL_CHUNK) {
		register fcchunk *l;

		l = ALLOC(fcchunk, 1);
		l->next = fclist;
		fclist = l;
		fcchunksz = 0;
	    }
	    fclist->f[fcchunksz++] = h->chain.name;
	    h->ct = nfcalls++;
	}
	*call = ((long) FCALL << 24) | h->ct;
    }
    return proto;
}

/*
 * NAME:	control->var()
 * DESCRIPTION:	handle a variable reference
 */
unsigned short ctrl_var(str, ref)
string *str;
long *ref;
{
    register vfh *h;

    /* check if the variable exists */
    h = *(vfh **) ht_lookup(vtab, str->text);
    if (h == (vfh *) NULL) {
	c_error("undefined variable %s", str->text);
	if (nvars < 256) {
	    /* don't repeat this error */
	    ctrl_dvar(str, 0, T_MIXED);
	}
	return T_MIXED;
    }

    *ref = ((long) h->ohash->index << 8) | h->index;
    return h->ct;	/* the variable type */
}


/*
 * NAME:	vfh->lclear()
 * DESCRIPTION:	clear the vfh list, meanwhile listing multiple definitions
 */
static void vfh_lclear()
{
    register vfh **h, *f, *t;

    for (h = vfhlist; h != (vfh **) NULL; h = f->next) {
	f = *h;
	*h = t = (vfh *) f->chain.next;
	if (f->ohash == (oh *) NULL) {
	    /*
	     * list a clash
	     */
	    message("  %s (/%s", f->chain.name, t->ohash->chain.name);
	    while ((t=(vfh *) t->chain.next) != (vfh *) NULL &&
		   strcmp(t->str->text, f->str->text) == 0 &&
		   !(t->ohash->obj->ctrl->funcdefs[t->index].class &
		     C_UNDEFINED)) {
		message(", /%s", t->ohash->chain.name);
	    }
	    message(")\012");	/* LF */
	} else if (t != (vfh *) NULL && t->ohash == (oh *) NULL &&
		   strcmp(t->str->text, f->str->text) == 0 &&
		   !(PROTO_CLASS(functions[f->index].proto) & C_PRIVATE)) {
	    /*
	     * this function was redefined, and must not be listed
	     */
	    t->ohash = f->ohash;
	}
	str_del(f->str);
    }
    vfhlist = (vfh **) NULL;
}

/*
 * NAME:	control->chkfuncs()
 * DESCRIPTION:
 */
bool ctrl_chkfuncs(file)
char *file;
{
    if (nfclash != 0) {
	message("\"/%s.c\": inherited multiple instances of:\012",	/* LF */
		file);
	vfh_lclear();
	return FALSE;
    }
    return TRUE;
}


/*
 * NAME:	control->mkstrings()
 * DESCRIPTION:	create the string table for the new control block
 */
static void ctrl_mkstrings()
{
    register string **s;
    register strchunk *l, *f;
    register unsigned short i;
    register long strsize;

    strsize = 0;
    if ((newctrl->nstrings = nstrs) != 0) {
	newctrl->strings = ALLOC(string*, newctrl->nstrings);
	s = newctrl->strings + nstrs;
	i = strchunksz;
	for (l = strlist; l != (strchunk *) NULL; ) {
	    while (i > 0) {
		*--s = l->s[--i];	/* already referenced */
		(*s)->ref |= STR_CONST;
		(*s)->u.strconst = s;
		strsize += (*s)->len;
	    }
	    i = STRING_CHUNK;
	    f = l;
	    l = l->next;
	    FREE(f);
	}
	strlist = (strchunk *) NULL;
	strchunksz = i;
    }
    newctrl->strsize = strsize;
}

/*
 * NAME:	control->mkfuncs()
 * DESCRIPTION:	make the function definition table for the control block
 */
static void ctrl_mkfuncs()
{
    register char *p;
    register dfuncdef *d;
    register cfunc *f;
    register int i, len;

    newctrl->progsize = progsize;
    if ((newctrl->nfuncdefs = nfdefs) != 0) {
	p = newctrl->prog = ALLOC(char, progsize);
	d = newctrl->funcdefs = ALLOC(dfuncdef, nfdefs);
	f = functions;
	for (i = nfdefs; i > 0; --i) {
	    *d = f->func;
	    d->offset = p - newctrl->prog;
	    memcpy(p, f->proto, len = PROTO_SIZE(f->proto));
	    p += len;
	    if (f->progsize != 0) {
		/* more than just a prototype */
		memcpy(p, f->prog, f->progsize);
		p += f->progsize;
	    }
	    d++;
	    f++;
	}
    }
}

/*
 * NAME:	control->mkvars()
 * DESCRIPTION:	make the variable definition table for the control block
 */
static void ctrl_mkvars()
{
    if ((newctrl->nvardefs = nvars) != 0) {
	newctrl->vardefs = ALLOC(dvardef, nvars);
	memcpy(newctrl->vardefs, variables, nvars * sizeof(dvardef));
    }
}

/*
 * NAME:	control->mkfcalls()
 * DESCRIPTION:	make the function call table for the control block
 */
static void ctrl_mkfcalls()
{
    register char *fc;
    register int i;
    register vfh *h;
    register fcchunk *l;

    newctrl->nfuncalls = newctrl->inherits[newctrl->ninherits - 1].funcoffset +
			 nfcalls;
    if (newctrl->nfuncalls == 0) {
	return;
    }
    fc = newctrl->funcalls = ALLOC(char, 2 * newctrl->nfuncalls);
    for (i = 0; i < ninherits; i++) {
	/*
	 * Go down the inherited objects, starting with the ai object, and
	 * fill in the function call table segment for each object once.
	 */
	if (oh_new(newctrl->inherits[i].obj->chain.name)->index == i) {
	    register char *ofc;
	    register dfuncdef *f;
	    register control *ctrl, *ctrl2;
	    register uindex j, n;

	    /*
	     * build the function call segment, based on the function call
	     * table of the inherited object
	     */
	    ctrl = newctrl->inherits[i].obj->ctrl;
	    j = ctrl->ninherits - 1;
	    ofc = d_get_funcalls(ctrl) + 2L * ctrl->inherits[j].funcoffset;
	    for (n = ctrl->nfuncalls - ctrl->inherits[j].funcoffset; n > 0; --n)
	    {
		ctrl2 = ctrl->inherits[UCHAR(ofc[0])].obj->ctrl;
		f = &ctrl2->funcdefs[UCHAR(ofc[1])];
		if (f->class & C_LOCAL) {
		    /*
		     * keep old call
		     */
		    *fc++ = (ofc[0] == 0) ? 0 : ofc[0] + i - j;
		    *fc++ = ofc[1];
		} else {
		    h = *(vfh **) ht_lookup(ftab,
					    d_get_strconst(ctrl2, f->inherit,
							   f->index)->text);
		    if (h->ohash->index == ninherits &&
			(functions[h->index].func.class & C_PRIVATE)) {
			/*
			 * private redefinition of (guaranteed non-private)
			 * inherited function
			 */
			h = (vfh *) h->chain.next;
		    }
		    *fc++ = h->ohash->index;
		    *fc++ = h->index;
		}
		ofc += 2;
	    }
	}
    }

    /*
     * Now fill in the function call entries for the object just compiled.
     */
    fc += 2L * nfcalls;
    i = fcchunksz;
    for (l = fclist; l != (fcchunk *) NULL; l = l->next) {
	do {
	    h = *(vfh **) ht_lookup(ftab, l->f[--i]);
	    *--fc = h->index;
	    *--fc = h->ohash->index;
	} while (i != 0);
	i = FCALL_CHUNK;
    }
}

/*
 * NAME:	control->mksymbs()
 * DESCRIPTION:	make the symbol table for the control block
 */
static void ctrl_mksymbs()
{
    register unsigned short i, n, x, ncoll;
    register dsymbol *symtab, *coll;

    if ((newctrl->nsymbols = nsymbs) == 0) {
	return;
    }

    /* initialize */
    symtab = newctrl->symbols = ALLOC(dsymbol, nsymbs);
    for (i = nsymbs; i > 0; --i) {
	symtab->next = -1;	/* mark as unused */
	symtab++;
    }
    symtab = newctrl->symbols;
    coll = ALLOCA(dsymbol, nsymbs);
    ncoll = 0;

    /*
     * Go down the inherited objects, adding the functions of each object
     * once.
     */
    for (i = 0; i <= ninherits; i++) {
	if (oh_new(newctrl->inherits[i].obj->chain.name)->index == i) {
	    register dfuncdef *f;
	    register control *ctrl;

	    ctrl = newctrl->inherits[i].obj->ctrl;
	    for (f = ctrl->funcdefs, n = 0; n < ctrl->nfuncdefs; f++, n++) {
		register vfh *h;
		register char *name;

		if ((f->class & C_PRIVATE) ||
		    ((f->class & (C_STATIC | C_UNDEFINED)) == C_STATIC &&
		     ctrl->ninherits == 1 &&
		     (is_auto ||
		      (ctrl->inherits->obj->flags & O_AUTO)))) {
		    continue;
		}
		name = d_get_strconst(ctrl, f->inherit, f->index)->text;
		h = *(vfh **) ht_lookup(ftab, name);
		if (h->ohash->index == ninherits &&
		    (functions[h->index].func.class & C_PRIVATE)) {
		    /*
		     * private redefinition of inherited function:
		     * use inherited function
		     */
		    h = (vfh *) h->chain.next;
		}
		if (i == h->ohash->index) {
		    /*
		     * all non-private functions are put into the hash table
		     */
		    x = hashstr(name, VFMERGEHASHSZ) % nsymbs;
		    if (symtab[x].next == (unsigned short) -1) {
			/*
			 * new entry
			 */
			symtab[x].inherit = i;
			symtab[x].index = n;
			symtab[x].next = x;
		    } else {
			/*
			 * collision
			 */
			coll[ncoll].inherit = i;
			coll[ncoll].index = n;
			coll[ncoll++].next = x;
		    }
		}
	    }
	}
    }

    /*
     * Now deal with the collisions.
     */
    n = 0;
    for (i = 0; i < ncoll; i++) {
	/* find a free slot */
	while (symtab[n].next != (unsigned short) -1) {
	    n++;
	}
	x = coll[i].next;
	/* add new entry to list */
	symtab[n] = symtab[x];
	if (symtab[n].next == x) {
	    symtab[n].next = n;	/* adjust list terminator */
	}
	symtab[x].inherit = coll[i].inherit;
	symtab[x].index = coll[i].index;
	symtab[x].next = n++;	/* link to previous slot */
    }

    AFREE(coll);
}

/*
 * NAME:	control->symb()
 * DESCRIPTION:	return the entry in the symbol table for func, or NULL
 */
dsymbol *ctrl_symb(ctrl, func)
register control *ctrl;
char *func;
{
    register dsymbol *symb;
    register dfuncdef *f;
    register unsigned int i, j;
    dsymbol *symtab, *symb1;
    dinherit *inherits;

    if ((i=ctrl->nsymbols) == 0) {
	return (dsymbol *) NULL;
    }

    inherits = ctrl->inherits;
    symtab = d_get_symbols(ctrl);
    i = hashstr(func, VFMERGEHASHSZ) % i;
    symb1 = symb = &symtab[i];
    ctrl = o_control(inherits[UCHAR(symb->inherit)].obj);
    f = d_get_funcdefs(ctrl) + UCHAR(symb->index);
    if (strcmp(func, d_get_strconst(ctrl, f->inherit, f->index)->text) == 0) {
	/* found it */
	return (f->class & C_UNDEFINED) ? (dsymbol *) NULL : symb1;
    }
    while (i != symb->next) {
	symb = &symtab[i = symb->next];
	ctrl = o_control(inherits[UCHAR(symb->inherit)].obj);
	f = d_get_funcdefs(ctrl) + UCHAR(symb->index);
	if (strcmp(func, d_get_strconst(ctrl, f->inherit, f->index)->text) == 0)
	{
	    /* found it: put symbol first in linked list */
	    i = symb1->inherit;
	    j = symb1->index;
	    symb1->inherit = symb->inherit;
	    symb1->index = symb->index;
	    symb->inherit = i;
	    symb->index = j;
	    return (f->class & C_UNDEFINED) ? (dsymbol *) NULL : symb1;
	}
    }
    return (dsymbol *) NULL;
}

/*
 * NAME:	control->construct()
 * DESCRIPTION:	construct and return a control block for the object just
 *		compiled
 */
control *ctrl_construct()
{
    register control *ctrl;
    register object *obj;

    ctrl = newctrl;
    ctrl->nvariables += nvars;

    obj = o_new(name, (object *) NULL, ctrl);
    ctrl_mkstrings();
    ctrl_mkfuncs();
    ctrl_mkvars();
    ctrl_mkfcalls();
    ctrl_mksymbs();
    ctrl->nfloatdefs = nfloats;
    ctrl->nfloats += nfloats;
    ctrl->compiled = P_time();

    if (strcmp(name, driver_name) == 0) {
	/*
	 * this is the driver object
	 */
	obj->flags |= O_DRIVER;
    } else if (is_auto) {
	/*
	 * this is the auto object
	 */
	obj->flags |= O_AUTO;
    }

    newctrl = (control *) NULL;
    return ctrl;
}

/*
 * NAME:	control->clear()
 * DESCRIPTION:	clean up
 */
void ctrl_clear()
{
    oh_clear();
    vfh_clear();
    ht_del(vtab);
    ht_del(ftab);
    lab_clear();

    ndirects = 0;
    ninherits = 0;
    nfloats = 0;
    nsymbs = 0;
    nfclash = 0;

    if (newctrl != (control *) NULL) {
	d_del_control(newctrl);
	newctrl = (control *) NULL;
    }
    str_clear();
    while (strlist != (strchunk *) NULL) {
	register strchunk *l;
	register string **s;

	l = strlist;
	s = &l->s[strchunksz];
	while (--strchunksz >= 0) {
	    str_del(*--s);
	}
	strchunksz = STRING_CHUNK;
	strlist = l->next;
	FREE(l);
    }
    while (fclist != (fcchunk *) NULL) {
	register fcchunk *l;

	l = fclist;
	fclist = l->next;
	FREE(l);
    }
    fcchunksz = FCALL_CHUNK;
    if (functions != (cfunc *) NULL) {
	register int i;
	register cfunc *f;

	for (i = nfdefs, f = functions; i > 0; --i, f++) {
	    FREE(f->proto);
	    if (f->progsize != 0) {
		FREE(f->prog);
	    }
	}
	FREE(functions);
	functions = (cfunc *) NULL;
    }
    if (variables != (dvardef *) NULL) {
	FREE(variables);
	variables = (dvardef *) NULL;
    }
}
