# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "hash.h"
# include "table.h"
# include "node.h"
# include "compile.h"
# include "control.h"

typedef struct _oh_ {		/* object hash table */
    hte chain;			/* hash table chain */
    object *obj;		/* object */
    short index;		/* -1: direct; 0: new; 1: indirect */
    short priv;			/* 1: direct private, 2: indirect private */
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

    h = (oh **) ht_lookup(otab, name, FALSE);
    if (*h == (oh *) NULL) {
	/*
	 * new object
	 */
	*h = ALLOC(oh, 1);
	(*h)->chain.next = (hte *) NULL;
	(*h)->chain.name = name;
	(*h)->index = 0;		/* new object */
	(*h)->priv = 0;
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

    if (otab != (hashtab *) NULL) {
	ht_del(otab);
	otab = (hashtab *) NULL;
    }
}


# define VFH_CHUNK	64

typedef struct _vfh_ {		/* variable/function hash table */
    hte chain;			/* hash table chain */
    string *str;		/* name string */
    oh *ohash;			/* controlling object hash table entry */
    unsigned short ct;		/* function call, or variable type */
    short index;		/* definition table index */
} vfh;

typedef struct _vfhchunk_ {
    vfh vf[VFH_CHUNK];		/* vfh chunk */
    struct _vfhchunk_ *next;	/* next in linked list */
} vfhchunk;

static vfhchunk *vfhclist;	/* linked list of all vfh chunks */
static int vfhchunksz = VFH_CHUNK; /* size of current vfh chunk */

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
    *addr = h;
    h->chain.name = str->text;
    str_ref(h->str = str);
    h->ohash = ohash;
    h->ct = ct;
    h->index = idx;
}

/*
 * NAME:	vfh->clear()
 * DESCRIPTION:	clear the vfh tables
 */
static void vfh_clear()
{
    register vfhchunk *l, *f;
    register vfh *vf;

    for (l = vfhclist; l != (vfhchunk *) NULL; ) {
	for (vf = l->vf; vfhchunksz != 0; vf++, --vfhchunksz) {
	    str_del(vf->str);
	}
	vfhchunksz = VFH_CHUNK;
	f = l;
	l = l->next;
	FREE(f);
    }
    vfhclist = (vfhchunk *) NULL;
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
    register char c1, c2;

    /* check if either prototype is implicit */
    if (PROTO_FTYPE(prot1) == T_IMPLICIT || PROTO_FTYPE(prot2) == T_IMPLICIT) {
	return TRUE;
    }

    /* check if classes are compatible */
    c1 = PROTO_CLASS(prot1);
    c2 = PROTO_CLASS(prot2);
    if ((c1 ^ c2) & (C_PRIVATE | C_VARARGS)) {
	return FALSE;		/* must agree on this much */
    } else if (c1 & c2 & C_UNDEFINED) {
	if ((c1 ^ c2) & ~C_TYPECHECKED) {
	    return FALSE;	/* 2 prototypes must be equal */
	}
    } else if (c1 & C_UNDEFINED) {
	if ((c1 ^ (c1 & c2)) & (C_STATIC | C_NOMASK | C_ATOMIC)) {
	    return FALSE;	/* everthing in prototype must be supported */
	}
    } else if (c2 & C_UNDEFINED) {
	if ((c2 ^ (c2 & c1)) & (C_STATIC | C_NOMASK | C_ATOMIC)) {
	    return FALSE;	/* everthing in prototype must be supported */
	}
    } else {
	return FALSE;		/* not compatible */
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
static bool countint;			/* TRUE if counting integer variables */
static bool privinherit;		/* TRUE if private inheritance used */
static hashtab *vtab;			/* variable merge table */
static hashtab *ftab;			/* function merge table */
static unsigned short nvars;		/* # variables */
static unsigned short nvinit;		/* # variables needing initialization */
static unsigned short nsymbs;		/* # symbols */
static int nfclash;			/* # prototype clashes */
static Uint nifcalls;			/* # inherited function calls */

/*
 * NAME:	control->init()
 * DESCRIPTION:	initialize control block construction
 */
void ctrl_init(flag)
bool flag;
{
    oh_init();
    vtab = ht_new(VFMERGETABSZ, VFMERGEHASHSZ);
    ftab = ht_new(VFMERGETABSZ, VFMERGEHASHSZ);
    countint = flag;
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
	/*
	 * Add only non-private variables, and check if a variable with the
	 * same name hasn't been inherited already.
	 */
	if (!(v->class & C_PRIVATE)) {
	    str = d_get_strconst(ctrl, v->inherit, v->index);
	    h = (vfh **) ht_lookup(vtab, str->text, FALSE);
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
    if (ohash->priv != 0 && (f->class & C_NOMASK)) {
	/*
	 * privately inherited nomask function is not allowed
	 */
	c_error("private inherit of nomask function %s (/%s)", str->text, 
		ohash->chain.name);
	return;
    }

    h = (vfh **) ht_lookup(ftab, str->text, FALSE);
    if (*h == (vfh *) NULL) {
	/*
	 * New function (-1: no calls to it yet)
	 */
	vfh_new(str, ohash, -1, idx, h);
	if (ohash->priv == 0 &&
	    (ctrl->ninherits != 1 ||
	     (f->class & (C_STATIC | C_UNDEFINED)) != C_STATIC)) {
	    /*
	     * don't count privately inherited functions, or static functions
	     * from the auto object
	     */
	    nsymbs++;
	}
    } else {
	register dinherit *inh;
	register int n;
	object *o;
	char *prot1, *prot2;
	bool privflag, inhflag, firstsym;
	int nfunc, npriv;

	/*
	 * prototype already exists
	 */
	prot1 = ctrl->prog + f->offset;

	/*
	 * First check if the new function's object is inherited by the
	 * object that defines the function in the merge table.
	 */
	privflag = FALSE;
	o = ohash->obj;
	for (l = h;
	     *l != (vfh *) NULL && strcmp((*l)->chain.name, str->text) == 0;
	     l = (vfh **) &(*l)->chain.next) {
	    if ((*l)->ohash == (oh *) NULL) {
		continue;
	    }

	    ctrl = (*l)->ohash->obj->ctrl;
	    inh = ctrl->inherits;
	    n = ctrl->ninherits;
	    ctrl = ohash->obj->ctrl;
	    while (--n != 0) {
		if (o->index == inh->oindex) {
		    if (ohash->priv == 0 && (*l)->ohash->priv != 0 &&
			(ctrl->ninherits != 1 ||
			 (ctrl->funcdefs[idx].class &
				       (C_STATIC | C_UNDEFINED)) != C_STATIC)) {
			/*
			 * private masks nonprivate function that isn't a
			 * static function in the auto object
			 */
			if (l == h) {
			    privflag = TRUE;
			}
			break;
		    } else {
			return;	/* no change */
		    }
		}
		inh++;
	    }
	}

	/*
	 * Now check if the function in the merge table is in
	 * an object inherited by the currently inherited object.
	 */
	inhflag = firstsym = TRUE;
	nfunc = npriv = 0;
	l = h;
	while (*l != (vfh *) NULL && strcmp((*l)->chain.name, str->text) == 0) {
	    if ((*l)->ohash == (oh *) NULL) {
		l = (vfh **) &(*l)->chain.next;
		continue;
	    }

	    o = (*l)->ohash->obj;
	    ctrl = ohash->obj->ctrl;
	    inh = ctrl->inherits;
	    n = ctrl->ninherits;
	    ctrl = o->ctrl;
	    prot2 = ctrl->prog + ctrl->funcdefs[(*l)->index].offset;
	    for (;;) {
		if (--n >= 0) {
		    if (o->index == (inh++)->oindex) {
			if ((*l)->ohash != ohash && (*l)->ohash->priv == 0 &&
			    (ctrl->ninherits != 1 ||
			     (ctrl->funcdefs[(*l)->index].class &
				       (C_STATIC | C_UNDEFINED)) != C_STATIC)) {
			    /*
			     * function in merge table is nonprivate and is
			     * not a static function in the auto object
			     */
			    firstsym = FALSE;
			    if (ohash->priv != 0) {
				/*
				 * masked by private function: leave it
				 */
				if (!(PROTO_CLASS(prot2) & C_UNDEFINED)) {
				    nfunc++;
				}
				l = (vfh **) &(*l)->chain.next;
				break;
			    }
			}

			/*
			 * redefined inherited function
			 */
			*l = (vfh *) (*l)->chain.next;
			break;
		    }
		} else {
		    /*
		     * check for prototype clashes
		     */
		    if (((f->class | PROTO_CLASS(prot2)) &
					(C_NOMASK | C_UNDEFINED)) == C_NOMASK) {
			/*
			 * a nomask function is inherited more than once
			 */
			c_error("multiple inheritance of nomask function %s (/%s, /%s)",
				str->text, (*l)->ohash->chain.name,
				ohash->chain.name);
			return;
		    }
		    if (((f->class | PROTO_CLASS(prot2)) & C_UNDEFINED) &&
			!cmp_proto(prot1, prot2)) {
			/*
			 * prototype conflict
			 */
			c_error("unequal prototypes for function %s (/%s, /%s)",
				str->text, (*l)->ohash->chain.name,
				ohash->chain.name);
			return;
		    }

		    if (!(PROTO_CLASS(prot2) & C_UNDEFINED)) {
			inhflag = FALSE;
			if ((*l)->ohash->priv == 0) {
			    nfunc++;
			} else {
			    npriv++;
			}
		    }

		    if ((*l)->ohash->priv == 0) {
			firstsym = FALSE;
		    }
		    l = (vfh **) &(*l)->chain.next;
		    break;
		}
	    }
	}

	if (firstsym && ohash->priv == 0) {
	    nsymbs++;	/* first symbol */
	}

	if (inhflag) {
	    /* insert new prototype at the beginning */
	    vfh_new(str, ohash, -1, idx, h);
	    h = (vfh **) &(*h)->chain.next;
	} else if (!(PROTO_CLASS(prot1) & C_UNDEFINED)) {
	    /* add the new prototype to the count */
	    if (ohash->priv == 0) {
		nfunc++;
	    } else {
		npriv++;
	    }
	}

	if (privflag) {
	    /* skip private function at the start */
	    h = (vfh **) &(*h)->chain.next;
	}

	/* add/remove clash markers */
	if (*h != (vfh *) NULL &&
	    strcmp((*h)->chain.name, str->text) == 0) {
	    /*
	     * there are other prototypes
	     */
	    if ((*h)->ohash == (oh *) NULL) {
		/* first entry is clash marker */
		if (nfunc + npriv <= 1) {
		    /* remove it */
		    *h = (vfh *) (*h)->chain.next;
		    --nfclash;
		} else {
		    /* adjust it */
		    (*h)->index = nfunc;
		    h = (vfh **) &(*h)->chain.next;
		}
	    } else if (nfunc + npriv > 1) {
		/* add new clash marker as first entry */
		vfh_new(str, (oh *) NULL, 0, nfunc, h);
		nfclash++;
		h = (vfh **) &(*h)->chain.next;
	    }
	}

	/* add new prototype, undefined at the end */
	if (!inhflag) {
	    if (PROTO_CLASS(prot1) & C_UNDEFINED) {
		vfh_new(str, ohash, -1, idx, l);
	    } else {
		vfh_new(str, ohash, -1, idx, h);
	    }
	}
    }
}

/*
 * NAME:	control->funcdefs()
 * DESCRIPTION:	put function definitions from an inherited object into
 *		the function merge table
 */
static void ctrl_funcdefs(ohash, ctrl)
register oh *ohash;
register control *ctrl;
{
    register short n;
    register dfuncdef *f;

    d_get_prog(ctrl);
    for (n = 0, f = d_get_funcdefs(ctrl); n < ctrl->nfuncdefs; n++, f++) {
	if (!(f->class & C_PRIVATE)) {
	    ctrl_funcdef(ctrl, n, ohash);
	}
    }
}

/*
 * NAME:	control->inherit()
 * DESCRIPTION:	inherit an object
 */
bool ctrl_inherit(f, from, obj, label, priv)
register frame *f;
char *from;
object *obj;
string *label;
int priv;
{
    register oh *ohash;
    register control *ctrl;
    dinherit *inh;
    register int i;
    register object *o;

    if (!(obj->flags & O_MASTER)) {
	c_error("cannot inherit cloned object");
	return TRUE;
    }
    if (O_UPGRADING(obj)) {
	c_error("cannot inherit object being upgraded");
	return TRUE;
    }

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
	/*
	 * new inherited object
	 */
	ctrl = o_control(obj);
	inh = ctrl->inherits;
	if (ndirects != 0 && strcmp(OBJ(inh->oindex)->chain.name,
				    directs[0]->obj->chain.name) != 0) {
	    c_error("inherited different auto objects");
	}
	for (i = ctrl->ninherits, inh += i; i > 0; --i) {
	    /*
	     * check all the objects inherited by the object now inherited
	     */
	    --inh;
	    o = OBJ(inh->oindex);
	    if (o->count == 0) {
		Uint ocount;

		if (strcmp(o->chain.name, from) == 0) {
		    /*
		     * inheriting old instance of the same object
		     */
		    c_error("cycle in inheritance");
		    return TRUE;
		}

		/*
		 * This object inherits an object that has been destructed.
		 * Give the driver object a chance to destruct it.
		 */
		(--f->sp)->type = T_OBJECT;
		f->sp->oindex = obj->index;
		f->sp->u.objcnt = ocount = obj->count;
		call_driver_object(f, "recompile", 1);
		i_del_value(f->sp++);
		if (obj->count != ocount) {
		    return FALSE;	/* recompile this object */
		}
	    }

	    ohash = oh_new(o->chain.name);
	    if (ohash->index == 0) {
		/*
		 * inherit a new object
		 */
		ohash->obj = o;
		ohash->index = 2;	/* indirect */
		nvinit += o_control(o)->nifdefs;
		if (inh->priv) {
		    ohash->priv = 2;	/* indirect private */
		} else {
		    ohash->priv = priv;
		    /*
		     * add functions and variables from this object
		     */
		    ctrl_funcdefs(ohash, o->ctrl);
		    ctrl_vardefs(ohash, o->ctrl);
		}
	    } else if (ohash->obj != o) {
		/*
		 * inherited two different objects with same name
		 */
		c_error("inherited different instances of /%s", o->chain.name);
		return TRUE;
	    } else {
		if (ohash->index < 0 && !(o->flags & O_AUTO)) {
		    /*
		     * Inherit an object which previously was inherited
		     * directly (but is not the auto object). Mark it as
		     * indirect now.
		     */
		    ohash->index = 1;	/* indirect, but immediate */
		    ninherits -= o->ctrl->ninherits - 1;
		}

		if (!inh->priv && ohash->priv == 2) {
		    /*
		     * previously indirectly privately inherited
		     */
		    ohash->priv = priv;
		    ctrl_funcdefs(ohash, o->ctrl);
		    ctrl_vardefs(ohash, o->ctrl);
		}
	    }
	}

	i = ctrl->ninherits;
	if (i > 1) {
	    /*
	     * Don't count the auto object, unless it is the auto object
	     * only.
	     */
	    --i;
	}
	ninherits += i;
	ohash = oh_new(obj->chain.name);
	directs[ndirects++] = ohash;
	ohash->index = -1;	/* direct */
	ohash->priv = priv;
	if (priv) {
	    privinherit = TRUE;
	}

    } else if (ohash->obj != obj) {
	/*
	 * inherited two objects with same name
	 */
	c_error("inherited different instances of /%s", obj->chain.name);
    } else {
	if (ohash->index == 2) {
	    /*
	     * not inherited directly before
	     */
	    directs[ndirects++] = ohash;
	    ohash->index = 1;	/* indirect, but immediate */
	}

	if (ohash->priv > priv) {
	    /*
	     * previously inherited with greater privateness; process all
	     * objects inherited by this object
	     */
	    ctrl = o_control(obj);
	    for (i = ctrl->ninherits, inh = ctrl->inherits + i; i > 0; --i) {
		--inh;
		o = OBJ(inh->oindex);
		ohash = oh_new(o->chain.name);
		if (!inh->priv && ohash->priv > priv) {
		    /*
		     * add to function and variable table
		     */
		    if (ohash->priv == 2) {
			ctrl_vardefs(ohash, o->ctrl);
		    }
		    ohash->priv = priv;
		    ctrl_funcdefs(ohash, o->ctrl);
		}
	    }
	}
    }

    if (ninherits >= MAX_INHERITS || ndirects == MAX_INHERITS) {
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
    char *name;				/* function name */
    char *proto;			/* function prototype */
    char *prog;				/* function program */
    unsigned short progsize;		/* function program size */
} cfunc;

static control *newctrl;		/* the new control block */
static oh *newohash;			/* fake ohash entry for new object */
static strchunk *str_list;		/* list of string chunks */
static int strchunksz = STRING_CHUNK;	/* size of current string chunk */
static Uint nstrs;			/* # of strings in all string chunks */
static fcchunk *fclist;			/* list of fcall chunks */
static int fcchunksz = FCALL_CHUNK;	/* size of current fcall chunk */
static cfunc *functions;		/* defined functions table */
static int nfdefs, fdef;		/* # defined functions, current func */
static Uint progsize;			/* size of all programs and protos */
static dvardef *variables;		/* defined variables */
static Uint nfcalls;			/* # function calls */

/*
 * NAME:	control->create()
 * DESCRIPTION:	make an initial control block
 */
void ctrl_create()
{
    register dinherit *new;
    register control *ctrl;
    register unsigned short n;
    register int i, count;

    /*
     * create a new control block
     */
    newohash = oh_new("/");		/* unique name */
    newohash->index = count = ninherits;
    newctrl = d_new_control();
    new = newctrl->inherits = ALLOC(dinherit, newctrl->ninherits = count + 1);
    new += count;
    nvars = 0;

    if (ninherits > 0) {
	register oh *ohash;

	/*
	 * initialize the virtually inherited objects
	 */
	for (n = ndirects; n > 0; ) {
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
		    ohash = oh_new(OBJ(old->oindex)->chain.name);
		    --old;
		    (--new)->oindex = ohash->obj->index;
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
	    ohash = oh_new(OBJ(new->oindex)->chain.name);
	    i = ohash->index;
	    if (i == count) {
		ctrl = OBJ(new->oindex)->ctrl;
		i = ctrl->ninherits - 1;
		new->funcoffset = nifcalls;
		n = ctrl->nfuncalls - ctrl->inherits[i].funcoffset;
		nifcalls += n;
		if (nifcalls > UINDEX_MAX && nifcalls - n <= UINDEX_MAX) {
		    c_error("inherited too many function calls");
		}
		new->varoffset = nvars;
		nvars += ctrl->nvardefs;
		if (nvars > 32767 && nvars - ctrl->nvardefs <= 32767) {
		    c_error("inherited too many variables");
		}

		for (n = ctrl->nstrings; n > 0; ) {
		    --n;
		    str_put(d_get_strconst(ctrl, i, n),
			    ((Uint) count << 16) | n);
		}
	    } else {
		new->funcoffset = newctrl->inherits[i].funcoffset;
		new->varoffset = newctrl->inherits[i].varoffset;
	    }
	    new->priv = (ohash->priv != 0);
	    new++;
	}
    }

    /*
     * stats for new object
     */
    new->oindex = UINDEX_MAX;
    new->funcoffset = nifcalls;
    new->varoffset = newctrl->nvariables = nvars;
    new->priv = FALSE;
    newctrl->nvinit = nvinit;

    /*
     * prepare for construction of a new control block
     */
    functions = ALLOC(cfunc, 256);
    variables = ALLOC(dvardef, 256);
    progsize = 0;
    nstrs = 0;
    nfdefs = 0;
    nvars = 0;
    nfcalls = 0;
    nvinit = 0;
}

/*
 * NAME:	control->dstring()
 * DESCRIPTION:	define a new (?) string constant
 */
long ctrl_dstring(str)
string *str;
{
    register Uint desc, new;

    desc = str_put(str, new = ((Uint) ninherits << 16) | nstrs);
    if (desc == new) {
	/*
	 * it is really a new string
	 */
	if (strchunksz == STRING_CHUNK) {
	    register strchunk *l;

	    l = ALLOC(strchunk, 1);
	    l->next = str_list;
	    str_list = l;
	    strchunksz = 0;
	}
	str_ref(str_list->s[strchunksz++] = str);
	if (nstrs == USHRT_MAX) {
	    c_error("too many string constants");
	}
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
    register vfh **h, **l;
    register dfuncdef *func;
    register char *proto2;
    register control *ctrl;
    int i;
    long s;

    i = -1;	/* default: no calls yet */

    /* first check if prototype exists already */
    h = l = (vfh **) ht_lookup(ftab, str->text, FALSE);
    if (*h != (vfh *) NULL) {
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
		if (PROTO_FTYPE(proto2) == T_IMPLICIT &&
		    (PROTO_CLASS(proto) & C_PRIVATE)) {
		    /* private function replaces implicit prototype */
		    --nsymbs;
		}
		i = PROTO_SIZE(proto);
		progsize += i - PROTO_SIZE(proto2);
		functions[fdef = (*h)->index].proto =
			(char *) memcpy(REALLOC(proto2, char, 0, i), proto, i);
		functions[fdef].func.class = PROTO_CLASS(proto);
	    }
	    return;
	}

	/*
	 * redefinition of inherited function
	 */
	if ((*h)->ohash != (oh *) NULL) {
	    ctrl = (*h)->ohash->obj->ctrl;
	    proto2 = ctrl->prog + ctrl->funcdefs[(*h)->index].offset;
	    if ((PROTO_CLASS(proto2) & C_UNDEFINED) &&
		!cmp_proto(proto, proto2)) {
		/*
		 * declaration does not match inherited prototype
		 */
		c_error("inherited different prototype for %s (/%s)",
			str->text, (*h)->ohash->chain.name);
	    } else if ((PROTO_CLASS(proto) & C_UNDEFINED) &&
		       (*h)->ohash->priv == 0 &&
		       PROTO_FTYPE(proto2) != T_IMPLICIT &&
		       cmp_proto(proto, proto2)) {
		/*
		 * there is no point in replacing an identical prototype
		 */
		return;
	    } else if ((PROTO_CLASS(proto2) & (C_NOMASK | C_UNDEFINED)) ==
								    C_NOMASK) {
		/*
		 * attempt to redefine nomask function
		 */
		c_error("redeclaration of nomask function %s (/%s)",
			str->text, (*h)->ohash->chain.name);
	    }

	    i = (*h)->ct;	/* take old call index */

	    if ((*l)->ohash->priv != 0) {
		l = (vfh **) &(*l)->chain.next;	/* skip private function */
	    }
	}
    }

    if (!(PROTO_CLASS(proto) & C_PRIVATE)) {
	/*
	 * may be a new symbol
	 */
	if (*l == (vfh *) NULL || strcmp((*l)->chain.name, str->text) != 0) {
	    nsymbs++;		/* no previous symbol */
	} else if ((*l)->ohash == (oh *) NULL) {
	    if ((*l)->index == 0) {
		nsymbs++;	/* previous functions all privately inherited */
	    }
	} else if ((*l)->ohash->priv != 0) {
	    nsymbs++;		/* replace private function */
	} else {
	    ctrl = (*l)->ohash->obj->ctrl;
	    proto2 = ctrl->prog + ctrl->funcdefs[(*l)->index].offset;
	    if (ctrl->ninherits == 1 &&
		(PROTO_CLASS(proto2) & (C_STATIC | C_UNDEFINED)) == C_STATIC) {
		nsymbs++;	/* mask static function in auto object */
	    }
	}
    }

    if (nfdefs == 255) {
	c_error("too many functions declared");
	return;
    }

    /*
     * Actual definition.
     */
    vfh_new(str, newohash, i, nfdefs, h);
    s = ctrl_dstring(str);
    i = PROTO_SIZE(proto);
    functions[nfdefs].name = str->text;
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
unsigned int size;
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
unsigned int class, type;
{
    register vfh **h;
    register dvardef *var;
    register long s;

    h = (vfh **) ht_lookup(vtab, str->text, FALSE);
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
    if (nvars == 255 || newctrl->nvariables + nvars == 32767) {
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

    if ((type == T_INT && countint) || type == T_FLOAT) {
	nvinit++;
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
    short inherit;

    if (label != (char *) NULL) {
	register dsymbol *symb;

	/* first check if the label exists */
	ohash = lab_find(label);
	if (ohash == (oh *) NULL) {
	    c_error("undefined label %s", label);
	    return (char *) NULL;
	}
	inherit = ohash->index;
	symb = ctrl_symb(ctrl = ohash->obj->ctrl, str->text, str->len);
	if (symb == (dsymbol *) NULL) {
	    /*
	     * It may seem strange to allow label::kfun, but remember that they
	     * are supposed to be inherited by the auto object.
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
	ohash = oh_new(OBJ(ctrl->inherits[UCHAR(symb->inherit)].oindex)->chain.name);
	index = UCHAR(symb->index);
    } else {
	register vfh *h;

	/* check if the function exists */
	inherit = ninherits;
	h = *(vfh **) ht_lookup(ftab, str->text, FALSE);
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
	    c_error("ambiguous call to function ::%s", str->text);
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
    inherit = (ohash->index == 0) ? 0 : ninherits + 1 - ohash->index;
    *call = ((long) DFCALL << 24) | ((long) inherit << 8) | index;
    return ctrl->prog + ctrl->funcdefs[index].offset;
}

/*
 * NAME:	control->fcall()
 * DESCRIPTION:	call a function
 */
char *ctrl_fcall(str, call, typechecking)
string *str;
long *call;
int typechecking;
{
    register vfh *h;
    char *proto;

    h = *(vfh **) ht_lookup(ftab, str->text, FALSE);
    if (h == (vfh *) NULL) {
	static char uproto[] = { (char) C_UNDEFINED, T_IMPLICIT, 0 };
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

	/* create an undefined prototype for the function */
	if (nfdefs == 255) {
	    c_error("too many undefined functions");
	    return (char *) NULL;
	}
	ctrl_dproto(str, proto = uproto);
	h = *(vfh **) ht_lookup(ftab, str->text, FALSE);
    } else if (h->ohash == newohash) {
	/*
	 * call to new function
	 */
	proto = functions[h->index].proto;
    } else if (h->ohash == (oh *) NULL) {
	/*
	 * call to multiple inherited function
	 */
	c_error("ambiguous call to function %s", str->text);
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

    if (h->ohash->priv != 0 || (PROTO_CLASS(proto) & C_PRIVATE) ||
	(PROTO_CLASS(proto) & (C_NOMASK | C_UNDEFINED)) == C_NOMASK ||
	((PROTO_CLASS(proto) & (C_STATIC | C_UNDEFINED)) == C_STATIC &&
	 h->ohash->index == 0)) {
	/* direct call */
	if (h->ohash->index == 0) {
	    *call = ((long) DFCALL << 24) | h->index;
	} else {
	    *call = ((long) DFCALL << 24) |
		    ((ninherits + 1L - h->ohash->index) << 8) |
		    h->index;
	}
    } else {
	/* ordinary function call */
	*call = ((long) FCALL << 24) | ((long) h->ohash->index << 8) | h->index;
    }
    return proto;
}

/*
 * NAME:	control->gencall()
 * DESCRIPTION:	generate a function call
 */
unsigned short ctrl_gencall(call)
long call;
{
    register vfh *h;
    char *name;
    short inherit, index;

    inherit = (call >> 8) & 0xff;
    index = call & 0xff;
    if (inherit == ninherits) {
	name = functions[index].name;
    } else {
	control *ctrl;
	dfuncdef *f;

	ctrl = OBJ(newctrl->inherits[inherit].oindex)->ctrl;
	f = ctrl->funcdefs + index;
	name = d_get_strconst(ctrl, f->inherit, f->index)->text;
    }
    h = *(vfh **) ht_lookup(ftab, name, FALSE);
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
	fclist->f[fcchunksz++] = name;
	if (nifcalls + nfcalls == UINDEX_MAX) {
	    c_error("too many function calls");
	}
	h->ct = nfcalls++;
    }
    return h->ct;
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
    h = *(vfh **) ht_lookup(vtab, str->text, TRUE);
    if (h == (vfh *) NULL) {
	c_error("undeclared variable %s", str->text);
	if (nvars < 255) {
	    /* don't repeat this error */
	    ctrl_dvar(str, 0, T_MIXED);
	}
	return T_MIXED;
    }

    if (h->ohash->index == 0 && ninherits != 0) {
	*ref = h->index;
    } else {
	*ref = ((ninherits + 1L - h->ohash->index) << 8) | h->index;
    }
    return h->ct;	/* the variable type */
}


/*
 * NAME:	control->chkfuncs()
 * DESCRIPTION:	check for multiple function definitions
 */
bool ctrl_chkfuncs()
{
    if (nfclash != 0 || privinherit) {
	register hte **t;
	register unsigned short sz;
	register vfh **f, **n;
	bool clash;

	clash = FALSE;
	for (t = ftab->table, sz = ftab->size; sz > 0; t++, --sz) {
	    for (f = (vfh **) t; *f != (vfh *) NULL; ) {
		if ((*f)->ohash == (oh *) NULL) {
		    /*
		     * clash marker found
		     */
		    if ((*f)->index <= 1) {
			/*
			 * erase clash which involves at most one function
			 * that isn't privately inherited
			 */
			*f = (vfh *) (*f)->chain.next;
		    } else {
			/*
			 * list a clash (only the first two)
			 */
			if (!clash) {
			    clash = TRUE;
			    c_error("inherited multiple instances of:");
			}
			f = (vfh **) &(*f)->chain.next;
			while ((*f)->ohash->priv != 0) {
			    f = (vfh **) &(*f)->chain.next;
			}
			n = (vfh **) &(*f)->chain.next;
			while ((*n)->ohash->priv != 0) {
			    n = (vfh **) &(*n)->chain.next;
			}
			c_error("  %s (/%s, /%s)", (*f)->chain.name,
				(*f)->ohash->chain.name,
				(*n)->ohash->chain.name);
			f = (vfh **) &(*n)->chain.next;
		    }
		} else if ((*f)->ohash->priv != 0) {
		    /*
		     * skip privately inherited function
		     */
		    f = (vfh **) &(*f)->chain.next;
		} else {
		    n = (vfh **) &(*f)->chain.next;
		    if (*n != (vfh *) NULL && (*n)->ohash != (oh *) NULL &&
			(*n)->ohash->priv != 0) {
			/* skip privately inherited function */
			n = (vfh **) &(*n)->chain.next;
		    }
		    if (*n != (vfh *) NULL && (*n)->ohash == (oh *) NULL &&
			strcmp((*n)->str->text, (*f)->str->text) == 0 &&
			!(PROTO_CLASS(functions[(*f)->index].proto) &C_PRIVATE))
		    {
			/*
			 * this function was redefined, skip the clash marker
			 */
			n = (vfh **) &(*n)->chain.next;
		    }
		    f = n;
		}
	    }
	}
	return !clash;
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
	for (l = str_list; l != (strchunk *) NULL; ) {
	    while (i > 0) {
		*--s = l->s[--i];	/* already referenced */
		strsize += (*s)->len;
	    }
	    i = STRING_CHUNK;
	    f = l;
	    l = l->next;
	    FREE(f);
	}
	str_list = (strchunk *) NULL;
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
    register int i;
    register unsigned int len;

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
    dinherit *inh;

    newctrl->nfuncalls = nifcalls + nfcalls;
    if (newctrl->nfuncalls == 0) {
	return;
    }
    fc = newctrl->funcalls = ALLOC(char, 2 * newctrl->nfuncalls);
    for (i = 0, inh = newctrl->inherits; i < ninherits; i++, inh++) {
	/*
	 * Walk through the list of inherited objects, starting with the auto
	 * object, and fill in the function call table segment for each object
	 * once.
	 */
	if (oh_new(OBJ(inh->oindex)->chain.name)->index == i) {
	    register char *ofc;
	    register dfuncdef *f;
	    register control *ctrl, *ctrl2;
	    register uindex j, n;

	    /*
	     * build the function call segment, based on the function call
	     * table of the inherited object
	     */
	    ctrl = OBJ(inh->oindex)->ctrl;
	    j = ctrl->ninherits - 1;
	    ofc = d_get_funcalls(ctrl) + 2L * ctrl->inherits[j].funcoffset;
	    for (n = ctrl->nfuncalls - ctrl->inherits[j].funcoffset; n > 0; --n)
	    {
		ctrl2 = OBJ(ctrl->inherits[UCHAR(ofc[0])].oindex)->ctrl;
		f = &ctrl2->funcdefs[UCHAR(ofc[1])];
		if (inh->priv || (f->class & C_PRIVATE) ||
		    (f->class & (C_NOMASK | C_UNDEFINED)) == C_NOMASK ||
		    ((f->class & (C_STATIC | C_UNDEFINED)) == C_STATIC &&
		     ofc[0] == 0)) {
		    /*
		     * keep old call
		     */
		    *fc++ = (ofc[0] == 0) ? 0 : ofc[0] + i - j;
		    *fc++ = ofc[1];
		} else {
		    h = *(vfh **) ht_lookup(ftab,
					    d_get_strconst(ctrl2, f->inherit,
							   f->index)->text,
					    FALSE);
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
	    h = *(vfh **) ht_lookup(ftab, l->f[--i], FALSE);
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
    dinherit *inh;

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
     * Go down the list of inherited objects, adding the functions of each
     * object once.
     */
    for (i = 0, inh = newctrl->inherits; i <= ninherits; i++, inh++) {
	register dfuncdef *f;
	register control *ctrl;

	if (i == ninherits) {
	    ctrl = newctrl;
	} else if (!inh->priv &&
		   oh_new(OBJ(inh->oindex)->chain.name)->index == i) {
	    ctrl = OBJ(inh->oindex)->ctrl;
	} else {
	    continue;
	}

	for (f = ctrl->funcdefs, n = 0; n < ctrl->nfuncdefs; f++, n++) {
	    register vfh *h;
	    register char *name;

	    if ((f->class & C_PRIVATE) ||
		(i == 0 && ninherits != 0 &&
		 (f->class & (C_STATIC | C_UNDEFINED)) == C_STATIC)) {
		continue;	/* not in symbol table */
	    }
	    name = d_get_strconst(ctrl, f->inherit, f->index)->text;
	    h = *(vfh **) ht_lookup(ftab, name, FALSE);
	    if (h->ohash->index == ninherits &&
		(functions[h->index].func.class & C_PRIVATE)) {
		/*
		 * private redefinition of inherited function:
		 * use inherited function
		 */
		h = (vfh *) h->chain.next;
	    }
	    while (h->ohash->priv != 0) {
		/*
		 * skip privately inherited function
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
dsymbol *ctrl_symb(ctrl, func, len)
register control *ctrl;
char *func;
unsigned int len;
{
    register dsymbol *symb;
    register dfuncdef *f;
    register unsigned int i, j;
    register string *str;
    dsymbol *symtab, *symb1;
    dinherit *inherits;

    if ((i=ctrl->nsymbols) == 0) {
	return (dsymbol *) NULL;
    }

    inherits = ctrl->inherits;
    symtab = d_get_symbols(ctrl);
    i = hashstr(func, VFMERGEHASHSZ) % i;
    symb1 = symb = &symtab[i];
    ctrl = o_control(OBJ(inherits[UCHAR(symb->inherit)].oindex));
    f = d_get_funcdefs(ctrl) + UCHAR(symb->index);
    str = d_get_strconst(ctrl, f->inherit, f->index);
    if (len == str->len && memcmp(func, str->text, len) == 0) {
	/* found it */
	return (f->class & C_UNDEFINED) ? (dsymbol *) NULL : symb1;
    }
    while (i != symb->next) {
	symb = &symtab[i = symb->next];
	ctrl = o_control(OBJ(inherits[UCHAR(symb->inherit)].oindex));
	f = d_get_funcdefs(ctrl) + UCHAR(symb->index);
	str = d_get_strconst(ctrl, f->inherit, f->index);
	if (len == str->len && memcmp(func, str->text, len) == 0) {
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

    ctrl = newctrl;
    ctrl->nvariables += nvars;

    ctrl_mkstrings();
    ctrl_mkfuncs();
    ctrl_mkvars();
    ctrl_mkfcalls();
    ctrl_mksymbs();
    ctrl->nifdefs = nvinit;
    ctrl->nvinit += nvinit;
    ctrl->compiled = P_time();

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
    if (vtab != (hashtab *) NULL) {
	ht_del(vtab);
	ht_del(ftab);
	vtab = (hashtab *) NULL;
	ftab = (hashtab *) NULL;
    }
    lab_clear();

    ndirects = 0;
    ninherits = 0;
    privinherit = FALSE;
    nvinit = 0;
    nsymbs = 0;
    nfclash = 0;
    nifcalls = 0;

    if (newctrl != (control *) NULL) {
	d_del_control(newctrl);
	newctrl = (control *) NULL;
    }
    str_clear();
    while (str_list != (strchunk *) NULL) {
	register strchunk *l;
	register string **s;

	l = str_list;
	s = &l->s[strchunksz];
	while (--strchunksz >= 0) {
	    str_del(*--s);
	}
	strchunksz = STRING_CHUNK;
	str_list = l->next;
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

/*
 * NAME:	control->varmap()
 * DESCRIPTION:	create a variable mapping from the old control block to the new
 */
unsigned short *ctrl_varmap(old, new)
register control *old, *new;
{
    register unsigned short j, k;
    register dvardef *v;
    register long n;
    register unsigned short *vmap;
    register dinherit *inh, *inh2;
    register control *ctrl, *ctrl2;
    unsigned short i, voffset;

    /*
     * make variable mapping from old to new, with new just compiled
     */

    vmap = ALLOC(unsigned short, new->nvariables + 1);

    voffset = 0;
    for (i = new->ninherits, inh = new->inherits; i > 0; --i, inh++) {
	ctrl = (i == 1) ? new : OBJ(inh->oindex)->ctrl;
	if (inh->varoffset < voffset || ctrl->nvardefs == 0) {
	    continue;
	}
	voffset = inh->varoffset + ctrl->nvardefs;

	for (j = old->ninherits, inh2 = old->inherits; j > 0; --j, inh2++) {
	    if (strcmp(OBJ(inh->oindex)->chain.name,
		       OBJ(inh2->oindex)->chain.name) == 0) {
		/*
		 * put var names from old control block in string merge table
		 */
		ctrl2 = o_control(OBJ(inh2->oindex));
		v = d_get_vardefs(ctrl2);
		for (k = 0; k < ctrl2->nvardefs; k++, v++) {
		    str_put(d_get_strconst(ctrl2, v->inherit, v->index),
			    ((Uint) k << 8) | v->type);
		}
	    } else if (j != 1) {
		continue;
	    }

	    /*
	     * map new variables to old ones
	     */
	    for (k = 0, v = ctrl->vardefs; k < ctrl->nvardefs; k++, v++) {
		n = str_put(d_get_strconst(ctrl, v->inherit, v->index),
			    (Uint) 0);
		if (n != 0 &&
		    ((n & 0xff) == v->type ||
		     ((v->type & T_REF) <= (n & T_REF) &&
		      (v->type & T_TYPE) == T_MIXED))) {
		    *vmap = inh2->varoffset + (n >> 8);
		} else {
		    switch (v->type) {
		    case T_INT:
			*vmap = NEW_INT;
			break;

		    case T_FLOAT:
			*vmap = NEW_FLOAT;
			break;

		    default:
			*vmap = NEW_POINTER;
			break;
		    }
		}
		vmap++;
	    }
	    str_clear();
	    break;
	}
    }

    /*
     * check if any variable changed
     */
    *vmap = old->nvariables;
    vmap -= new->nvariables;
    if (old->nvariables != new->nvariables) {
	return vmap;		/* changed */
    }
    for (i = 0; i <= new->nvariables; i++) {
	if (vmap[i] != i) {
	    return vmap;	/* changed */
	}
    }
    /* no variable remapping needed */
    FREE(vmap);
    return (unsigned short *) NULL;
}
