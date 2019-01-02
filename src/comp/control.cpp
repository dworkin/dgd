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

# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "table.h"
# include "node.h"
# include "compile.h"
# include "control.h"

struct oh : public Hashtab::Entry { /* object hash table */
    Object *obj;		/* object */
    short index;		/* -1: new */
    short priv;			/* 1: direct private, 2: indirect private */
    oh **list;			/* next in linked list */
};

static Hashtab *otab;		/* object hash table */
static oh **olist;		/* list of all object hash table entries */

/*
 * NAME:	oh->init()
 * DESCRIPTION:	initialize the object hash table
 */
static void oh_init()
{
    otab = Hashtab::create(OMERGETABSZ, OBJHASHSZ, FALSE);
}

/*
 * NAME:	oh->new()
 * DESCRIPTION:	put an object in the hash table
 */
static oh *oh_new(const char *name)
{
    oh **h;

    h = (oh **) otab->lookup(name, FALSE);
    if (*h == (oh *) NULL) {
	/*
	 * new object
	 */
	*h = ALLOC(oh, 1);
	(*h)->next = (Hashtab::Entry *) NULL;
	(*h)->name = name;
	(*h)->index = -1;		/* new object */
	(*h)->priv = 0;
	(*h)->list = olist;
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
    oh **h, *f;

    for (h = olist; h != (oh **) NULL; ) {
	f = *h;
	h = f->list;
	FREE(f);
    }
    olist = (oh **) NULL;

    if (otab != (Hashtab *) NULL) {
	delete otab;
	otab = (Hashtab *) NULL;
    }
}


# define VFH_CHUNK	64

/* variable/function hash table */
struct vfh : public Hashtab::Entry, public ChunkAllocated {
    String *str;		/* name string */
    oh *ohash;			/* controlling object hash table entry */
    String *cvstr;		/* class variable string */
    unsigned short ct;		/* function call, or variable type */
    short index;		/* definition table index */
};

static class vfhchunk : public Chunk<vfh, VFH_CHUNK> {
public:
    /*
     * NAME:		item()
     * DESCRIPTION:	dereference strings when iterating through items
     */
    virtual bool item(vfh *h) {
	h->str->del();
	if (h->cvstr != (String *) NULL) {
	    h->cvstr->del();
	}
	return TRUE;
    }
} vchunk;

/*
 * NAME:	vfh->new()
 * DESCRIPTION:	create a new vfh table element
 */
static void vfh_new(String *str, oh *ohash, unsigned short ct,
	String *cvstr, short idx, vfh **addr)
{
    vfh *h;

    h = chunknew (vchunk) vfh;
    h->next = *addr;
    *addr = h;
    h->name = str->text;
    h->str = str;
    h->str->ref();
    h->ohash = ohash;
    h->cvstr = cvstr;
    if (cvstr != (String *) NULL) {
	cvstr->ref();
    }
    h->ct = ct;
    h->index = idx;
}

/*
 * NAME:	vfh->clear()
 * DESCRIPTION:	clear the vfh tables
 */
static void vfh_clear()
{
    vchunk.items();
    vchunk.clean();
}


struct lab {
    String *str;		/* label */
    oh *ohash;			/* entry in hash table */
    lab *next;			/* next label */
};

static lab *labels;		/* list of labeled inherited objects */

/*
 * NAME:	lab->new()
 * DESCRIPTION:	declare a new inheritance label
 */
static void lab_new(String *str, oh *ohash)
{
    lab *l;

    l = ALLOC(lab, 1);
    l->str = str;
    l->str->ref();
    l->ohash = ohash;
    l->next = labels;
    labels = l;
}

/*
 * NAME:	lab->find()
 * DESCRIPTION:	find a labeled object in the list
 */
static oh *lab_find(const char *name)
{
    lab *l;

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
    lab *l, *f;

    l = labels;
    while (l != (lab *) NULL) {
	l->str->del();
	f = l;
	l = l->next;
	FREE(f);
    }
    labels = (lab *) NULL;
}


# define MAX_INHERITS		255
# define MAX_VARIABLES		(USHRT_MAX - 2)

static oh *inherits[MAX_INHERITS * 2];	/* inherited objects */
static int ninherits;			/* # inherited objects */
static bool privinherit;		/* TRUE if private inheritance used */
static Hashtab *vtab;			/* variable merge table */
static Hashtab *ftab;			/* function merge table */
static unsigned short nvars;		/* # variables */
static unsigned short nsymbs;		/* # symbols */
static int nfclash;			/* # prototype clashes */
static Uint nifcalls;			/* # inherited function calls */

/*
 * NAME:	Control->init()
 * DESCRIPTION:	initialize control block construction
 */
void ctrl_init()
{
    oh_init();
    vtab = Hashtab::create(VFMERGETABSZ, VFMERGEHASHSZ, FALSE);
    ftab = Hashtab::create(VFMERGETABSZ, VFMERGEHASHSZ, FALSE);
}

/*
 * NAME:	Control->vardefs()
 * DESCRIPTION:	put variable definitions from an inherited object into the
 *		variable merge table
 */
static void ctrl_vardefs(oh *ohash, Control *ctrl)
{
    dvardef *v;
    int n;
    String *str, *cvstr;
    vfh **h;

    v = d_get_vardefs(ctrl);
    for (n = 0; n < ctrl->nvardefs; n++) {
	/*
	 * Add only non-private variables, and check if a variable with the
	 * same name hasn't been inherited already.
	 */
	if (!(v->sclass & C_PRIVATE)) {
	    str = d_get_strconst(ctrl, v->inherit, v->index);
	    h = (vfh **) vtab->lookup(str->text, FALSE);
	    if (*h == (vfh *) NULL) {
		/* new variable */
		if (ctrl->nclassvars != 0) {
		    cvstr = ctrl->cvstrings[n];
		} else {
		    cvstr = (String *) NULL;
		}
		vfh_new(str, ohash, v->type, cvstr, n, h);
	    } else {
	       /* duplicate variable */
	       c_error("multiple inheritance of variable %s (/%s, /%s)",
		       str->text, (*h)->ohash->name, ohash->name);
	    }
	}
	v++;
    }
}

/*
 * NAME:	comp_class()
 * DESCRIPTION:	compare two class strings
 */
static bool cmp_class(Control *ctrl1, Uint s1, Control *ctrl2, Uint s2)
{
    if (ctrl1 == ctrl2 && s1 == s2) {
	return TRUE;	/* the same */
    }
    if (ctrl1->compiled == 0 && (s1 >> 16) == ninherits) {
	return FALSE;	/* one is new, and therefore different */
    }
    if (ctrl2->compiled == 0 && (s2 >> 16) == ninherits) {
	return FALSE;	/* one is new, and therefore different */
    }
    return !d_get_strconst(ctrl1, s1 >> 16, s1 & 0xffff)->cmp(d_get_strconst(ctrl2, s2 >> 16, s2 & 0xffff));
}

/*
 * NAME:	cmp_proto()
 * DESCRIPTION:	Compare two prototypes. Return TRUE if equal.
 */
static bool cmp_proto(Control *ctrl1, char *prot1, Control *ctrl2, char *prot2)
{
    int i;
    char c1, c2;
    Uint s1, s2;

    /* check if either prototype is implicit */
    if (PROTO_FTYPE(prot1) == T_IMPLICIT || PROTO_FTYPE(prot2) == T_IMPLICIT) {
	return TRUE;
    }

    /* check if classes are compatible */
    c1 = *prot1++;
    c2 = *prot2++;
    if ((c1 ^ c2) & (C_PRIVATE | C_ELLIPSIS)) {
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

    /* check if the number of arguments is equal */
    if ((i=UCHAR(*prot1++)) != UCHAR(*prot2++)) {
	return FALSE;
    }
    if (*prot1 != *prot2) {
	return FALSE;
    }
    i += UCHAR(*prot1);

    /* compare return type & arguments */
    prot1 += 3;
    prot2 += 3;
    do {
	if (*prot1++ != *prot2) {
	    return FALSE;
	}
	if ((*prot2++ & T_TYPE) == T_CLASS) {
	    /* compare class strings */
	    FETCH3U(prot1, s1);
	    FETCH3U(prot2, s2);
	    if (!cmp_class(ctrl1, s1, ctrl2, s2)) {
		return FALSE;
	    }
	}
    } while (--i >= 0);

    return TRUE;	/* equal */
}

/*
 * NAME:	Control->funcdef()
 * DESCRIPTION:	put a function definition from an inherited object into
 *		the function merge table
 */
static void ctrl_funcdef(Control *ctrl, int idx, oh *ohash)
{
    vfh **h, **l;
    dfuncdef *f;
    String *str;

    f = &ctrl->funcdefs[idx];
    str = d_get_strconst(ctrl, f->inherit, f->index);
    if (ohash->priv != 0 && (f->sclass & C_NOMASK)) {
	/*
	 * privately inherited nomask function is not allowed
	 */
	c_error("private inherit of nomask function %s (/%s)", str->text,
		ohash->name);
	return;
    }

    h = (vfh **) ftab->lookup(str->text, FALSE);
    if (*h == (vfh *) NULL) {
	/*
	 * New function (-1: no calls to it yet)
	 */
	vfh_new(str, ohash, -1, (String *) NULL, idx, h);
	if (ohash->priv == 0 &&
	    (ctrl->ninherits != 1 ||
	     (f->sclass & (C_STATIC | C_UNDEFINED)) != C_STATIC)) {
	    /*
	     * don't count privately inherited functions, or static functions
	     * from the auto object
	     */
	    nsymbs++;
	}
    } else {
	dinherit *inh;
	int n;
	Object *o;
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
	     *l != (vfh *) NULL && strcmp((*l)->name, str->text) == 0;
	     l = (vfh **) &(*l)->next) {
	    if ((*l)->ohash == (oh *) NULL) {
		continue;
	    }

	    ctrl = (*l)->ohash->obj->ctrl;
	    inh = ctrl->inherits;
	    n = ctrl->ninherits;
	    ctrl = ohash->obj->ctrl;
	    while (--n != 0) {
		if (o->index == inh->oindex && !inh->priv) {
		    if (ohash->priv == 0 && (*l)->ohash->priv != 0 &&
			(ctrl->ninherits != 1 ||
			 (ctrl->funcdefs[idx].sclass &
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
	 * Now check if the functions in the merge table are in
	 * an object inherited by the currently inherited object.
	 */
	inhflag = firstsym = TRUE;
	nfunc = npriv = 0;
	l = h;
	while (*l != (vfh *) NULL && strcmp((*l)->name, str->text) == 0) {
	    if ((*l)->ohash == (oh *) NULL) {
		l = (vfh **) &(*l)->next;
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
			/*
			 * redefined inherited function
			 */
			if ((*l)->ohash != ohash && (*l)->ohash->priv == 0 &&
			    (ctrl->ninherits != 1 ||
			     (ctrl->funcdefs[(*l)->index].sclass &
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
				l = (vfh **) &(*l)->next;
				break;
			    }
			}
			*l = (vfh *) (*l)->next;
			break;
		    }
		} else {
		    /*
		     * not inherited: check for prototype clashes
		     */
		    if (((f->sclass | PROTO_CLASS(prot2)) &
					(C_NOMASK | C_UNDEFINED)) == C_NOMASK) {
			/*
			 * a nomask function is inherited more than once
			 */
			c_error("multiple inheritance of nomask function %s (/%s, /%s)",
				str->text, (*l)->ohash->name, ohash->name);
			return;
		    }
		    if (((f->sclass | PROTO_CLASS(prot2)) & C_UNDEFINED) &&
			!cmp_proto(ohash->obj->ctrl, prot1, ctrl, prot2)) {
			/*
			 * prototype conflict
			 */
			c_error("unequal prototypes for function %s (/%s, /%s)",
				str->text, (*l)->ohash->name, ohash->name);
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
		    l = (vfh **) &(*l)->next;
		    break;
		}
	    }
	}

	if (firstsym && ohash->priv == 0) {
	    nsymbs++;	/* first symbol */
	}

	if (inhflag) {
	    /* insert new prototype at the beginning */
	    vfh_new(str, ohash, -1, (String *) NULL, idx, h);
	    h = (vfh **) &(*h)->next;
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
	    h = (vfh **) &(*h)->next;
	}

	/* add/remove clash markers */
	if (*h != (vfh *) NULL &&
	    strcmp((*h)->name, str->text) == 0) {
	    /*
	     * there are other prototypes
	     */
	    if ((*h)->ohash == (oh *) NULL) {
		/* first entry is clash marker */
		if (nfunc + npriv <= 1) {
		    /* remove it */
		    *h = (vfh *) (*h)->next;
		    --nfclash;
		} else {
		    /* adjust it */
		    (*h)->index = nfunc;
		    h = (vfh **) &(*h)->next;
		}
	    } else if (nfunc + npriv > 1) {
		/* add new clash marker as first entry */
		vfh_new(str, (oh *) NULL, 0, (String *) NULL, nfunc, h);
		nfclash++;
		h = (vfh **) &(*h)->next;
	    }
	}

	/* add new prototype, undefined at the end */
	if (!inhflag) {
	    if (PROTO_CLASS(prot1) & C_UNDEFINED) {
		vfh_new(str, ohash, -1, (String *) NULL, idx, l);
	    } else {
		vfh_new(str, ohash, -1, (String *) NULL, idx, h);
	    }
	}
    }
}

/*
 * NAME:	Control->funcdefs()
 * DESCRIPTION:	put function definitions from an inherited object into
 *		the function merge table
 */
static void ctrl_funcdefs(oh *ohash, Control *ctrl)
{
    short n;
    dfuncdef *f;

    d_get_prog(ctrl);
    for (n = 0, f = d_get_funcdefs(ctrl); n < ctrl->nfuncdefs; n++, f++) {
	if (!(f->sclass & C_PRIVATE)) {
	    ctrl_funcdef(ctrl, n, ohash);
	}
    }
}

/*
 * NAME:	Control->inherit()
 * DESCRIPTION:	inherit an object
 */
bool ctrl_inherit(Frame *f, char *from, Object *obj, String *label, int priv)
{
    oh *ohash;
    Control *ctrl;
    dinherit *inh;
    int i;
    Object *o;

    if (!(obj->flags & O_MASTER)) {
	c_error("cannot inherit cloned object");
	return TRUE;
    }
    if (O_UPGRADING(obj)) {
	c_error("cannot inherit object being upgraded");
	return TRUE;
    }

    ohash = oh_new(obj->name);
    if (label != (String *) NULL) {
	/*
	 * use a label
	 */
	if (lab_find(label->text) != (oh *) NULL) {
	    c_error("redeclaration of label %s", label->text);
	}
	lab_new(label, ohash);
    }

    if (ohash->index < 0) {
	/*
	 * new inherited object
	 */
	ctrl = obj->control();
	inh = ctrl->inherits;
	if (ninherits != 0 && strcmp(OBJR(inh->oindex)->name,
				     inherits[0]->obj->name) != 0) {
	    c_error("inherited different auto objects");
	}

	for (i = ctrl->ninherits - 1, inh += i; i > 0; --i) {
	    /*
	     * check if object inherits destructed objects
	     */
	    --inh;
	    o = OBJR(inh->oindex);
	    if (o->count == 0) {
		Uint ocount;

		if (strcmp(o->name, from) == 0) {
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
		f->sp->objcnt = ocount = obj->count;
		call_driver_object(f, "recompile", 1);
		i_del_value(f->sp++);
		obj = OBJR(obj->index);
		if (obj->count != ocount) {
		    return FALSE;	/* recompile this object */
		}
	    }
	}

	for (i = ctrl->ninherits, inh += i; i > 0; --i) {
	    /*
	     * check if inherited objects have been inherited before
	     */
	    --inh;
	    o = OBJR(inh->oindex);
	    ohash = oh_new(o->name);
	    if (ohash->index < 0) {
		/*
		 * inherit a new object
		 */
		ohash->obj = o;
		o->control();		/* load the control block */
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
		c_error("inherited different instances of /%s", o->name);
		return TRUE;
	    } else if (!inh->priv && ohash->priv > priv) {
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

	for (i = ctrl->ninherits; i > 0; --i) {
	    /*
	     * add to the inherited array
	     */
	    ohash = oh_new(OBJR(inh->oindex)->name);
	    if (ohash->index < 0) {
		ohash->index = ninherits;
		inherits[ninherits++] = ohash;
	    }
	    inh++;
	}

	if (priv) {
	    privinherit = TRUE;
	}

    } else if (ohash->obj != obj) {
	/*
	 * inherited two objects with same name
	 */
	c_error("inherited different instances of /%s", obj->name);
    } else if (ohash->priv > priv) {
	/*
	 * previously inherited with greater privateness; process all
	 * objects inherited by this object
	 */
	ctrl = obj->control();
	for (i = ctrl->ninherits, inh = ctrl->inherits + i; i > 0; --i) {
	    --inh;
	    o = OBJR(inh->oindex);
	    ohash = oh_new(o->name);
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

    if (ninherits >= MAX_INHERITS) {
	c_error("too many objects inherited");
    }

    return TRUE;
}


# define STRING_CHUNK	64

struct strptr : public ChunkAllocated {
    String *str;
};

static class strchunk : public Chunk<strptr, STRING_CHUNK> {
public:
    /*
     * NAME:		item()
     * DESCRIPTION:	copy or dereference when iterating through items
     */
    virtual bool item(strptr *s) {
	if (copy != (String **) NULL) {
	    *--copy = s->str;
	    strsize += s->str->len;
	} else {
	    s->str->del();
	}
	return TRUE;
    }

    /*
     * NAME:		mkstrings()
     * DESCRIPTION:	build string constant table and clean up
     */
    long mkstrings(String **s) {
	copy = s;
	strsize = 0;
	items();
	Chunk<strptr, STRING_CHUNK>::clean();
	return strsize;
    }

    /*
     * NAME:		clean()
     * DESCRIPTION:	override Chunk::clean()
     */
    void clean()
    {
	copy = (String **) NULL;
	items();
	Chunk<strptr, STRING_CHUNK>::clean();
    }

private:
    String **copy;			/* string copy table or NULL */
    long strsize;			/* cumulative length of all strings */
} schunk;

# define FCALL_CHUNK	64

struct charptr : public ChunkAllocated {
    char *name;
};

static class fcchunk : public Chunk<charptr, FCALL_CHUNK> {
public:
    /*
     * NAME:		item()
     * DESCRIPTION:	build function call table when iterating through items
     */
    virtual bool item(charptr *name) {
	vfh *h;

	h = *(vfh **) ftab->lookup(name->name, FALSE);
	*--fcalls = h->index;
	*--fcalls = h->ohash->index;
	return TRUE;
    }

    /*
     * NAME:		mkfcalls()
     * DESCRIPTION:	build function call table
     */
    void mkfcalls(char *fc) {
	fcalls = fc;
	items();
    }

private:
    char *fcalls;			/* function call pointer */
} fchunk;

struct cfunc {
    dfuncdef func;			/* function name/type */
    char *name;				/* function name */
    char *proto;			/* function prototype */
    String *cfstr;			/* function class string */
    char *prog;				/* function program */
    unsigned short progsize;		/* function program size */
};

static Control *newctrl;		/* the new control block */
static oh *newohash;			/* fake ohash entry for new object */
static Uint nstrs;			/* # of strings in all string chunks */
static cfunc *functions;		/* defined functions table */
static int nfdefs, fdef;		/* # defined functions, current func */
static int nundefs;			/* # private undefined prototypes */
static Uint progsize;			/* size of all programs and protos */
static dvardef *variables;		/* defined variables */
static String **cvstrings;		/* variable class strings */
static char *classvars;			/* class variables */
static int nclassvars;			/* # classvars */
static Uint nfcalls;			/* # function calls */

/*
 * NAME:	Control->imap()
 * DESCRIPTION:	initialize inherit map
 */
static void ctrl_imap(Control *ctrl)
{
    dinherit *inh;
    int i, j, n, imapsz;
    Control *ctrl2;

    imapsz = ctrl->ninherits;
    for (n = imapsz - 1, inh = &ctrl->inherits[n]; n > 0; ) {
	--n;
	(--inh)->progoffset = imapsz;
	ctrl2 = OBJR(inh->oindex)->ctrl;
	for (i = 0; i < ctrl2->ninherits; i++) {
	    ctrl->imap[imapsz++] = oh_new(OBJR(ctrl2->inherits[UCHAR(ctrl2->imap[i])].oindex)->name)->index;
	}
	for (j = ctrl->ninherits - n; --j > 0; ) {
	    if (memcmp(ctrl->imap + inh->progoffset,
		       ctrl->imap + inh[j].progoffset, i) == 0) {
		/* merge with table of inheriting object */
		inh->progoffset = inh[j].progoffset;
		imapsz -= i;
		break;
	    }
	}
    }
    ctrl->imap = REALLOC(ctrl->imap, char, ctrl->imapsz, imapsz);
    ctrl->imapsz = imapsz;
}

/*
 * NAME:	Control->convert()
 * DESCRIPTION:	convert inherits
 */
void ctrl_convert(Control *ctrl)
{
    int n, imapsz;
    oh *ohash;
    dinherit *inh;
    Object *obj;
    Hashtab *xotab;
    oh **xolist;

    xotab = otab;
    xolist = olist;
    oh_init();
    olist = (oh **) NULL;

    imapsz = 0;
    for (n = 0, inh = ctrl->inherits; n < ctrl->ninherits; n++, inh++) {
	obj = OBJR(inh->oindex);
	ohash = oh_new(obj->name);
	if (ohash->index < 0) {
	    ohash->obj = obj;
	    ohash->index = n;
	}
	imapsz += obj->control()->ninherits;
    }
    ctrl->imap = ALLOC(char, ctrl->imapsz = imapsz);
    imapsz = 0;
    for (n = ctrl->ninherits, inh = ctrl->inherits; n > 0; --n, inh++) {
	ctrl->imap[imapsz++] = n;
    }
    ctrl->imap[0] = 0;
    ctrl_imap(ctrl);

    oh_clear();
    olist = xolist;
    otab = xotab;
}

/*
 * NAME:	Control->create()
 * DESCRIPTION:	make an initial control block
 */
void ctrl_create()
{
    dinherit *inh;
    Control *ctrl;
    unsigned short n;
    int i, count;
    oh *ohash;

    /*
     * create a new control block
     */
    newohash = oh_new("/");		/* unique name */
    newohash->index = ninherits;
    newctrl = d_new_control();
    inh = newctrl->inherits =
	  ALLOC(dinherit, newctrl->ninherits = ninherits + 1);
    newctrl->imap = ALLOC(char, (ninherits + 2) * (ninherits + 1) / 2);
    nvars = 0;
    String::merge();

    /*
     * Fix function offsets and variable offsets, and collect all string
     * constants from inherited objects and put them in the string merge
     * table.
     */
    for (count = 0; count < ninherits; count++) {
	newctrl->imap[count] = count;
	ohash = inherits[count];
	inh->oindex = ohash->obj->index;
	ctrl = ohash->obj->ctrl;
	i = ctrl->ninherits - 1;
	inh->funcoffset = nifcalls;
	n = ctrl->nfuncalls - ctrl->inherits[i].funcoffset;
	if (nifcalls > UINDEX_MAX - n) {
	    c_error("inherited too many function calls");
	}
	nifcalls += n;
	inh->varoffset = nvars;
	if (nvars > MAX_VARIABLES - ctrl->nvardefs) {
	    c_error("inherited too many variables");
	}
	nvars += ctrl->nvardefs;

	for (n = ctrl->nstrings; n > 0; ) {
	    --n;
	    d_get_strconst(ctrl, i, n)->put(((Uint) count << 16) | n);
	}
	inh->priv = (ohash->priv != 0);
	inh++;
    }
    newctrl->imap[count] = count;
    inh->oindex = UINDEX_MAX;
    inh->progoffset = 0;
    inh->funcoffset = nifcalls;
    inh->varoffset = newctrl->nvariables = nvars;
    inh->priv = FALSE;
    ctrl_imap(newctrl);

    /*
     * prepare for construction of a new control block
     */
    functions = ALLOC(cfunc, 256);
    variables = ALLOC(dvardef, 256);
    cvstrings = ALLOC(String*, 256 * sizeof(String*));
    classvars = ALLOC(char, 256 * 3);
    progsize = 0;
    nstrs = 0;
    nfdefs = 0;
    nvars = 0;
    nclassvars = 0;
    nfcalls = 0;
}

/*
 * NAME:	Control->dstring()
 * DESCRIPTION:	define a new (?) string constant
 */
long ctrl_dstring(String *str)
{
    Uint desc, ndesc;

    desc = str->put(ndesc = ((Uint) ninherits << 16) | nstrs);
    if (desc == ndesc) {
	/*
	 * it is really a new string
	 */
	(chunknew (schunk) strptr)->str = str;
	str->ref();
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
 * NAME:	Control->dproto()
 * DESCRIPTION:	define a new function prototype
 */
void ctrl_dproto(String *str, char *proto, String *sclass)
{
    vfh **h, **l;
    dfuncdef *func;
    char *proto2;
    Control *ctrl;
    int i;
    long s;

    /* first check if prototype exists already */
    h = l = (vfh **) ftab->lookup(str->text, FALSE);
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
	    } else if (!cmp_proto(newctrl, proto, newctrl, proto2)) {
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
		if ((PROTO_CLASS(proto2) & C_PRIVATE) &&
		    !(PROTO_CLASS(proto) & C_UNDEFINED)) {
		    /* replace private undefined prototype by declaration */
		    --nundefs;
		}

		i = PROTO_SIZE(proto);
		progsize += i - PROTO_SIZE(proto2);
		functions[fdef = (*h)->index].proto =
			(char *) memcpy(REALLOC(proto2, char, 0, i), proto, i);
		functions[fdef].func.sclass = PROTO_CLASS(proto);
		if (functions[fdef].cfstr != (String *) NULL) {
		    functions[fdef].cfstr->del();
		}
		functions[fdef].cfstr = sclass;
		if (sclass != (String *) NULL) {
		    sclass->ref();
		}
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
		!cmp_proto(newctrl, proto, ctrl, proto2)) {
		/*
		 * declaration does not match inherited prototype
		 */
		c_error("inherited different prototype for %s (/%s)",
			str->text, (*h)->ohash->name);
	    } else if ((PROTO_CLASS(proto) & C_UNDEFINED) &&
		       (*h)->ohash->priv == 0 &&
		       (ctrl->ninherits != 1 ||
			(PROTO_CLASS(proto2) & (C_STATIC | C_UNDEFINED)) !=
								    C_STATIC) &&
		       PROTO_FTYPE(proto2) != T_IMPLICIT &&
		       cmp_proto(newctrl, proto, ctrl, proto2)) {
		/*
		 * there is no point in replacing an identical prototype
		 * that is not a static function in the auto object
		 */
		return;
	    } else if ((PROTO_CLASS(proto2) & (C_NOMASK | C_UNDEFINED)) ==
								    C_NOMASK) {
		/*
		 * attempt to redefine nomask function
		 */
		c_error("redeclaration of nomask function %s (/%s)",
			str->text, (*h)->ohash->name);
	    }

	    if ((*l)->ohash->priv != 0) {
		l = (vfh **) &(*l)->next;	/* skip private function */
	    }
	}
    }

    if (!(PROTO_CLASS(proto) & C_PRIVATE)) {
	/*
	 * may be a new symbol
	 */
	if (*l == (vfh *) NULL || strcmp((*l)->name, str->text) != 0) {
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
    } else if (PROTO_CLASS(proto) & C_UNDEFINED) {
	nundefs++;		/* private undefined prototype */
    }

    if (nfdefs == 255) {
	c_error("too many functions declared");
    }

    /*
     * Actual definition.
     */
    vfh_new(str, newohash, -1, (String *) NULL, nfdefs, h);
    s = ctrl_dstring(str);
    i = PROTO_SIZE(proto);
    functions[nfdefs].name = str->text;
    functions[nfdefs].proto = (char *) memcpy(ALLOC(char, i), proto, i);
    functions[nfdefs].cfstr = sclass;
    if (sclass != (String *) NULL) {
	sclass->ref();
    }
    functions[nfdefs].progsize = 0;
    progsize += i;
    func = &functions[nfdefs++].func;
    func->sclass = PROTO_CLASS(proto);
    func->inherit = s >> 16;
    func->index = s;
}

/*
 * NAME:	Control->dfunc()
 * DESCRIPTION:	define a new function
 */
void ctrl_dfunc(String *str, char *proto, String *sclass)
{
    fdef = nfdefs;
    ctrl_dproto(str, proto, sclass);
}

/*
 * NAME:	Control->dprogram()
 * DESCRIPTION:	define a function body
 */
void ctrl_dprogram(char *prog, unsigned int size)
{
    functions[fdef].prog = prog;
    functions[fdef].progsize = size;
    progsize += size;
}

/*
 * NAME:	Control->dvar()
 * DESCRIPTION:	define a variable
 */
void ctrl_dvar(String *str, unsigned int sclass, unsigned int type, String *cvstr)
{
    vfh **h;
    dvardef *var;
    char *p;
    long s;

    h = (vfh **) vtab->lookup(str->text, FALSE);
    if (*h != (vfh *) NULL) {
	if ((*h)->ohash == newohash) {
	    c_error("redeclaration of variable %s", str->text);
	    return;
	} else if (!(sclass & C_PRIVATE)) {
	    /*
	     * non-private redeclaration of a variable
	     */
	    c_error("redeclaration of variable %s (/%s)", str->text,
		    (*h)->ohash->name);
	    return;
	}
    }
    if (nvars == 255 || newctrl->nvariables + nvars == MAX_VARIABLES) {
	c_error("too many variables declared");
    }

    /* actually define the variable */
    vfh_new(str, newohash, type, cvstr, nvars, h);
    s = ctrl_dstring(str);
    var = &variables[nvars];
    var->sclass = sclass;
    var->inherit = s >> 16;
    var->index = s;
    var->type = type;
    cvstrings[nvars++] = cvstr;
    if (cvstr != (String *) NULL) {
	cvstr->ref();
	s = ctrl_dstring(cvstr);
	p = classvars + nclassvars++ * 3;
	*p++ = s >> 16;
	*p++ = s >> 8;
	*p = s;
    }
}

/*
 * NAME:	Control->ifcall()
 * DESCRIPTION:	call an inherited function
 */
char *ctrl_ifcall(String *str, const char *label, String **cfstr, long *call)
{
    Control *ctrl;
    oh *ohash;
    short index;
    char *proto;

    *cfstr = (String *) NULL;

    if (label != (char *) NULL) {
	dsymbol *symb;

	/* first check if the label exists */
	ohash = lab_find(label);
	if (ohash == (oh *) NULL) {
	    c_error("undefined label %s", label);
	    return (char *) NULL;
	}
	symb = ctrl_symb(ctrl = ohash->obj->ctrl, str->text, str->len);
	if (symb == (dsymbol *) NULL) {
	    if (ctrl->ninherits != 1) {
		ohash = inherits[0];
		symb = ctrl_symb(ctrl = ohash->obj->ctrl, str->text, str->len);
	    }
	    if (symb == (dsymbol *) NULL) {
		/*
		 * It may seem strange to allow label::kfun, but remember that
		 * they are supposed to be inherited by the auto object.
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
	}
	ohash = oh_new(OBJR(ctrl->inherits[UCHAR(symb->inherit)].oindex)->name);
	index = UCHAR(symb->index);
    } else {
	vfh *h;

	/* check if the function exists */
	h = *(vfh **) ftab->lookup(str->text, FALSE);
	if (h == (vfh *) NULL || (h->ohash == newohash &&
	    ((h=(vfh *) h->next) == (vfh *) NULL ||
	     strcmp(h->name, str->text) != 0))) {

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
    if (ctrl->funcdefs[index].sclass & C_UNDEFINED) {
	c_error("undefined function %s::%s", label, str->text);
	return (char *) NULL;
    }
    *call = ((long) DFCALL << 24) | ((long) ohash->index << 8) | index;
    proto = ctrl->prog + ctrl->funcdefs[index].offset;

    if ((PROTO_FTYPE(proto) & T_TYPE) == T_CLASS) {
	char *p;
	Uint sclass;

	p = &PROTO_FTYPE(proto) + 1;
	FETCH3U(p, sclass);
	*cfstr = d_get_strconst(ctrl, sclass >> 16, sclass & 0xffff);
    }
    return proto;
}

/*
 * NAME:	Control->fcall()
 * DESCRIPTION:	call a function
 */
char *ctrl_fcall(String *str, String **cfstr, long *call, int typechecking)
{
    vfh *h;
    char *proto;

    *cfstr = (String *) NULL;

    h = *(vfh **) ftab->lookup(str->text, FALSE);
    if (h == (vfh *) NULL) {
	static char uproto[] = { (char) C_UNDEFINED, 0, 0, 0, 6, T_IMPLICIT };
	short kf;

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
	ctrl_dproto(str, proto = uproto, (String *) NULL);
	h = *(vfh **) ftab->lookup(str->text, FALSE);
    } else if (h->ohash == newohash) {
	/*
	 * call to new function
	 */
	proto = functions[h->index].proto;
	*cfstr = functions[h->index].cfstr;
    } else if (h->ohash == (oh *) NULL) {
	/*
	 * call to multiple inherited function
	 */
	c_error("ambiguous call to function %s", str->text);
	return (char *) NULL;
    } else {
	Control *ctrl;
	char *p;
	Uint sclass;

	/*
	 * call to inherited function
	 */
	ctrl = h->ohash->obj->ctrl;
	proto = ctrl->prog + ctrl->funcdefs[h->index].offset;
	if ((PROTO_FTYPE(proto) & T_TYPE) == T_CLASS) {
	    p = &PROTO_FTYPE(proto) + 1;
	    FETCH3U(p, sclass);
	    *cfstr = d_get_strconst(ctrl, sclass >> 16, sclass & 0xffff);
	}
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
	    *call = ((long) DFCALL << 24) | ((long) h->ohash->index << 8) | h->index;
	}
    } else {
	/* ordinary function call */
	*call = ((long) FCALL << 24) | ((long) h->ohash->index << 8) | h->index;
    }
    return proto;
}

/*
 * NAME:	Control->gencall()
 * DESCRIPTION:	generate a function call
 */
unsigned short ctrl_gencall(long call)
{
    vfh *h;
    char *name;
    short inherit, index;

    inherit = (call >> 8) & 0xff;
    index = call & 0xff;
    if (inherit == ninherits) {
	name = functions[index].name;
    } else {
	Control *ctrl;
	dfuncdef *f;

	ctrl = OBJR(newctrl->inherits[inherit].oindex)->ctrl;
	f = ctrl->funcdefs + index;
	name = d_get_strconst(ctrl, f->inherit, f->index)->text;
    }
    h = *(vfh **) ftab->lookup(name, FALSE);
    if (h->ct == (unsigned short) -1) {
	/*
	 * add to function call table
	 */
	(chunknew (fchunk) charptr)->name = name;
	if (nifcalls + nfcalls == UINDEX_MAX) {
	    c_error("too many function calls");
	}
	h->ct = nfcalls++;
    }
    return h->ct;
}

/*
 * NAME:	Control->var()
 * DESCRIPTION:	handle a variable reference
 */
unsigned short ctrl_var(String *str, long *ref, String **cvstr)
{
    vfh *h;

    /* check if the variable exists */
    h = *(vfh **) vtab->lookup(str->text, TRUE);
    if (h == (vfh *) NULL) {
	c_error("undeclared variable %s", str->text);
	if (nvars < 255) {
	    /* don't repeat this error */
	    ctrl_dvar(str, 0, T_MIXED, (String *) NULL);
	}
	*cvstr = (String *) NULL;
	*ref = 0;
	return T_MIXED;
    }

    if (h->ohash->index == 0 && ninherits != 0) {
	*ref = h->index;
    } else {
	*ref = ((long) h->ohash->index << 8) | h->index;
    }
    *cvstr = h->cvstr;
    return h->ct;	/* the variable type */
}

/*
 * NAME:	ctrl->ninherits()
 * DESCRIPTION:	return the number of objects inherited
 */
int ctrl_ninherits()
{
    return ninherits;
}


/*
 * NAME:	Control->chkfuncs()
 * DESCRIPTION:	check function definitions
 */
bool ctrl_chkfuncs()
{
    if (nundefs != 0) {
	cfunc *f;
	unsigned short i;

	/*
	 * private undefined prototypes
	 */
	c_error("undefined private functions:");
	for (f = functions, i = nundefs; i != 0; f++) {
	    if ((f->func.sclass & (C_PRIVATE | C_UNDEFINED)) ==
						    (C_PRIVATE | C_UNDEFINED)) {
		c_error("  %s", f->name);
		--i;
	    }
	}
	return FALSE;
    }

    if (nfclash != 0 || privinherit) {
	Hashtab::Entry **t;
	unsigned short sz;
	vfh **f, **n;
	bool clash;

	clash = FALSE;
	for (t = ftab->table(), sz = ftab->size(); sz > 0; t++, --sz) {
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
			*f = (vfh *) (*f)->next;
		    } else {
			/*
			 * list a clash (only the first two)
			 */
			if (!clash) {
			    clash = TRUE;
			    c_error("inherited multiple instances of:");
			}
			f = (vfh **) &(*f)->next;
			while ((*f)->ohash->priv != 0) {
			    f = (vfh **) &(*f)->next;
			}
			n = (vfh **) &(*f)->next;
			while ((*n)->ohash->priv != 0) {
			    n = (vfh **) &(*n)->next;
			}
			c_error("  %s (/%s, /%s)", (*f)->name,
				(*f)->ohash->name, (*n)->ohash->name);
			f = (vfh **) &(*n)->next;
		    }
		} else if ((*f)->ohash->priv != 0) {
		    /*
		     * skip privately inherited function
		     */
		    f = (vfh **) &(*f)->next;
		} else {
		    n = (vfh **) &(*f)->next;
		    if (*n != (vfh *) NULL && (*n)->ohash != (oh *) NULL &&
			(*n)->ohash->priv != 0) {
			/* skip privately inherited function */
			n = (vfh **) &(*n)->next;
		    }
		    if (*n != (vfh *) NULL && (*n)->ohash == (oh *) NULL &&
			strcmp((*n)->str->text, (*f)->str->text) == 0 &&
			!(PROTO_CLASS(functions[(*f)->index].proto) &C_PRIVATE))
		    {
			/*
			 * this function was redefined, skip the clash marker
			 */
			n = (vfh **) &(*n)->next;
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
 * NAME:	Control->mkstrings()
 * DESCRIPTION:	create the string table for the new control block
 */
static void ctrl_mkstrings()
{
    long strsize;

    strsize = 0;
    if ((newctrl->nstrings = nstrs) != 0) {
	newctrl->strings = ALLOC(String*, newctrl->nstrings);
	strsize = schunk.mkstrings(newctrl->strings + nstrs);
    }
    newctrl->strsize = strsize;
}

/*
 * NAME:	Control->mkfuncs()
 * DESCRIPTION:	make the function definition table for the control block
 */
static void ctrl_mkfuncs()
{
    char *p;
    dfuncdef *d;
    cfunc *f;
    int i;
    unsigned int len;

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
 * NAME:	Control->mkvars()
 * DESCRIPTION:	make the variable definition table for the control block
 */
static void ctrl_mkvars()
{
    if ((newctrl->nvardefs = nvars) != 0) {
	newctrl->vardefs = ALLOC(dvardef, nvars);
	memcpy(newctrl->vardefs, variables, nvars * sizeof(dvardef));
	if ((newctrl->nclassvars = nclassvars) != 0) {
	    unsigned short i;
	    String **s;

	    newctrl->cvstrings = ALLOC(String*, nvars * sizeof(String*));
	    memcpy(newctrl->cvstrings, cvstrings, nvars * sizeof(String*));
	    for (i = nvars, s = newctrl->cvstrings; i != 0; --i, s++) {
		if (*s != (String *) NULL) {
		    (*s)->ref();
		}
	    }
	    newctrl->classvars = ALLOC(char, nclassvars * 3);
	    memcpy(newctrl->classvars, classvars, nclassvars * 3);
	}
    }
}

/*
 * NAME:	Control->mkfcalls()
 * DESCRIPTION:	make the function call table for the control block
 */
static void ctrl_mkfcalls()
{
    char *fc;
    int i;
    vfh *h;
    dinherit *inh;
    oh *ohash;

    newctrl->nfuncalls = nifcalls + nfcalls;
    if (newctrl->nfuncalls == 0) {
	return;
    }
    fc = newctrl->funcalls = ALLOC(char, 2L * newctrl->nfuncalls);
    for (i = 0, inh = newctrl->inherits; i < ninherits; i++, inh++) {
	/*
	 * Walk through the list of inherited objects, starting with the auto
	 * object, and fill in the function call table segment for each object
	 * once.
	 */
	ohash = oh_new(OBJR(inh->oindex)->name);
	if (ohash->index == i) {
	    char *ofc;
	    dfuncdef *f;
	    Control *ctrl;
	    Object *obj;
	    uindex j, n;

	    /*
	     * build the function call segment, based on the function call
	     * table of the inherited object
	     */
	    ctrl = ohash->obj->ctrl;
	    j = ctrl->ninherits - 1;
	    ofc = d_get_funcalls(ctrl) + 2L * ctrl->inherits[j].funcoffset;
	    for (n = ctrl->nfuncalls - ctrl->inherits[j].funcoffset; n > 0; --n)
	    {
		j = UCHAR(ofc[0]);
		obj = OBJR(ctrl->inherits[j].oindex);
		f = &obj->ctrl->funcdefs[UCHAR(ofc[1])];
		if (inh->priv || (f->sclass & C_PRIVATE) ||
		    (f->sclass & (C_NOMASK | C_UNDEFINED)) == C_NOMASK ||
		    ((f->sclass & (C_STATIC | C_UNDEFINED)) == C_STATIC &&
		     j == 0)) {
		    /*
		     * keep old call
		     */
		    if (j != 0) {
			j = oh_new(obj->name)->index;
		    }
		    *fc++ = j;
		    *fc++ = ofc[1];
		} else {
		    h = *(vfh **) ftab->lookup(d_get_strconst(obj->ctrl,
							      f->inherit,
							      f->index)->text,
					    FALSE);
		    if (h->ohash->index == ninherits &&
			(functions[h->index].func.sclass & C_PRIVATE)) {
			/*
			 * private redefinition of (guaranteed non-private)
			 * inherited function
			 */
			h = (vfh *) h->next;
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
    fchunk.mkfcalls(fc + 2L * nfcalls);
}

/*
 * NAME:	Control->mksymbs()
 * DESCRIPTION:	make the symbol table for the control block
 */
static void ctrl_mksymbs()
{
    unsigned short i, n, x, ncoll;
    dsymbol *symtab, *coll;
    dinherit *inh;

    if ((newctrl->nsymbols = nsymbs) == 0) {
	return;
    }

    /* initialize */
    symtab = newctrl->symbols = ALLOC(dsymbol, nsymbs);
    for (i = 0; i < nsymbs; i++) {
	symtab->next = i;	/* mark as unused */
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
	dfuncdef *f;
	Control *ctrl;

	if (i == ninherits) {
	    ctrl = newctrl;
	} else if (!inh->priv &&
		   oh_new(OBJR(inh->oindex)->name)->index == i) {
	    ctrl = OBJR(inh->oindex)->ctrl;
	} else {
	    continue;
	}

	for (f = ctrl->funcdefs, n = 0; n < ctrl->nfuncdefs; f++, n++) {
	    vfh *h;
	    char *name;

	    if ((f->sclass & C_PRIVATE) ||
		(i == 0 && ninherits != 0 &&
		 (f->sclass & (C_STATIC | C_UNDEFINED)) == C_STATIC)) {
		continue;	/* not in symbol table */
	    }
	    name = d_get_strconst(ctrl, f->inherit, f->index)->text;
	    h = *(vfh **) ftab->lookup(name, FALSE);
	    if (h->ohash->index == ninherits &&
		(functions[h->index].func.sclass & C_PRIVATE)) {
		/*
		 * private redefinition of inherited function:
		 * use inherited function
		 */
		h = (vfh *) h->next;
	    }
	    while (h->ohash->priv != 0) {
		/*
		 * skip privately inherited function
		 */
		h = (vfh *) h->next;
	    }
	    if (i == h->ohash->index) {
		/*
		 * all non-private functions are put into the hash table
		 */
		x = Hashtab::hashstr(name, VFMERGEHASHSZ) % nsymbs;
		if (symtab[x].next == x) {
		    /*
		     * new entry
		     */
		    symtab[x].inherit = i;
		    symtab[x].index = n;
		    symtab[x].next = -1;
		} else {
		    /*
		     * collision
		     */
		    coll[ncoll].inherit = i;
		    coll[ncoll].index = n;
		    coll[ncoll++].next = x;
		}
		if (f->sclass & C_UNDEFINED) {
		    newctrl->flags |= CTRL_UNDEFINED;
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
	while (symtab[n].next != n) {
	    n++;
	}
	x = coll[i].next;
	/* add new entry to list */
	symtab[n] = symtab[x];
	symtab[x].inherit = coll[i].inherit;
	symtab[x].index = coll[i].index;
	symtab[x].next = n++;	/* link to previous slot */
    }

    AFREE(coll);
}

/*
 * NAME:	ctrl->mkvtypes()
 * DESCRIPTION:	make the variable type table for the control block
 */
void ctrl_mkvtypes(Control *ctrl)
{
    char *type;
    unsigned short max, nv, n;
    dinherit *inh;
    dvardef *var;

    max = ctrl->nvariables - ctrl->nvardefs;
    if (max == 0) {
	return;
    }

    ctrl->vtypes = type = ALLOC(char, max);
    for (nv = 0, inh = ctrl->inherits; nv != max; inh++) {
	if (inh->varoffset == nv) {
	    ctrl = OBJR(inh->oindex)->control();
	    for (n = ctrl->nvardefs, nv += n, var = d_get_vardefs(ctrl);
		 n != 0; --n, var++) {
		if (T_ARITHMETIC(var->type)) {
		    *type++ = var->type;
		} else {
		    *type++ = nil_value.type;
		}
	    }
	}
    }
}

/*
 * NAME:	Control->symb()
 * DESCRIPTION:	return the entry in the symbol table for func, or NULL
 */
dsymbol *ctrl_symb(Control *ctrl, const char *func, unsigned int len)
{
    dsymbol *symb;
    dfuncdef *f;
    unsigned int i, j;
    String *str;
    dsymbol *symtab, *symb1;
    dinherit *inherits;

    if ((i=ctrl->nsymbols) == 0) {
	return (dsymbol *) NULL;
    }

    inherits = ctrl->inherits;
    symtab = d_get_symbols(ctrl);
    i = Hashtab::hashstr(func, VFMERGEHASHSZ) % i;
    symb1 = symb = &symtab[i];
    ctrl = OBJR(inherits[UCHAR(symb->inherit)].oindex)->control();
    f = d_get_funcdefs(ctrl) + UCHAR(symb->index);
    str = d_get_strconst(ctrl, f->inherit, f->index);
    if (len == str->len && memcmp(func, str->text, len) == 0) {
	/* found it */
	return (f->sclass & C_UNDEFINED) ? (dsymbol *) NULL : symb1;
    }
    while (symb->next != i && symb->next != (unsigned short) -1) {
	symb = &symtab[i = symb->next];
	ctrl = OBJR(inherits[UCHAR(symb->inherit)].oindex)->control();
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
	    return (f->sclass & C_UNDEFINED) ? (dsymbol *) NULL : symb1;
	}
    }
    return (dsymbol *) NULL;
}

/*
 * NAME:	Control->construct()
 * DESCRIPTION:	construct and return a control block for the object just
 *		compiled
 */
Control *ctrl_construct()
{
    Control *ctrl;

    ctrl = newctrl;
    ctrl->nvariables += nvars;

    ctrl_mkstrings();
    ctrl_mkfuncs();
    ctrl_mkvars();
    ctrl_mkfcalls();
    ctrl_mksymbs();
    ctrl_mkvtypes(ctrl);
    ctrl->compiled = P_time();

    newctrl = (Control *) NULL;
    return ctrl;
}

/*
 * NAME:	Control->clear()
 * DESCRIPTION:	clean up
 */
void ctrl_clear()
{
    oh_clear();
    vfh_clear();
    if (vtab != (Hashtab *) NULL) {
	delete vtab;
	delete ftab;
	vtab = (Hashtab *) NULL;
	ftab = (Hashtab *) NULL;
    }
    lab_clear();

    ninherits = 0;
    privinherit = FALSE;
    nsymbs = 0;
    nfclash = 0;
    nifcalls = 0;
    nundefs = 0;

    if (newctrl != (Control *) NULL) {
	d_del_control(newctrl);
	newctrl = (Control *) NULL;
    }
    String::clear();
    schunk.clean();
    fchunk.clean();
    if (functions != (cfunc *) NULL) {
	int i;
	cfunc *f;

	for (i = nfdefs, f = functions; i > 0; --i, f++) {
	    FREE(f->proto);
	    if (f->progsize != 0) {
		FREE(f->prog);
	    }
	    if (f->cfstr != (String *) NULL) {
		f->cfstr->del();
	    }
	}
	FREE(functions);
	functions = (cfunc *) NULL;
    }
    if (variables != (dvardef *) NULL) {
	FREE(variables);
	variables = (dvardef *) NULL;
    }
    if (cvstrings != (String **) NULL) {
	unsigned short i;
	String **s;

	for (i = nvars, s = cvstrings; i != 0; --i, s++) {
	    if (*s != (String *) NULL) {
		(*s)->del();
	    }
	}
	FREE(cvstrings);
	cvstrings = (String **) NULL;
    }
    if (classvars != (char *) NULL) {
	FREE(classvars);
	classvars = (char *) NULL;
    }
}

/*
 * NAME:	Control->varmap()
 * DESCRIPTION:	create a variable mapping from the old control block to the new
 */
unsigned short *ctrl_varmap(Control *octrl, Control *nctrl)
{
    unsigned short j, k;
    dvardef *v;
    long n;
    unsigned short *vmap;
    dinherit *inh, *inh2;
    Control *ctrl, *ctrl2;
    unsigned short i, voffset;

    /*
     * make variable mapping from old to new, with new just compiled
     */

    vmap = ALLOC(unsigned short, nctrl->nvariables + 1);

    voffset = 0;
    for (i = nctrl->ninherits, inh = nctrl->inherits; i > 0; --i, inh++) {
	ctrl = (i == 1) ? nctrl : OBJR(inh->oindex)->ctrl;
	if (inh->varoffset < voffset || ctrl->nvardefs == 0) {
	    continue;
	}
	voffset = inh->varoffset + ctrl->nvardefs;

	j = octrl->ninherits;
	for (inh2 = octrl->inherits; ; inh2++) {
	    if (strcmp(OBJR(inh->oindex)->name, OBJR(inh2->oindex)->name) == 0)
	    {
		/*
		 * put var names from old control block in string merge table
		 */
		String::merge();
		ctrl2 = OBJR(inh2->oindex)->control();
		v = d_get_vardefs(ctrl2);
		for (k = 0; k < ctrl2->nvardefs; k++, v++) {
		    d_get_strconst(ctrl2, v->inherit, v->index)->put(((Uint) k << 8) | v->type);
		}

		/*
		 * map new variables to old ones
		 */
		for (k = 0, v = d_get_vardefs(ctrl); k < ctrl->nvardefs;
		     k++, v++) {
		    n = d_get_strconst(ctrl, v->inherit, v->index)->put(0);
		    if (n != 0 &&
			(((n & 0xff) == v->type &&
			  ((n & T_TYPE) != T_CLASS ||
			   ctrl->cvstrings[k]->cmp(ctrl2->cvstrings[n >> 8])
								    == 0)) ||
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
		String::clear();
		break;
	    }

	    if (--j == 0) {
		/*
		 * new inherited object
		 */
		for (k = 0, v = d_get_vardefs(ctrl); k < ctrl->nvardefs;
		     k++, v++) {
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
		    vmap++;
		}
		break;
	    }
	}
    }

    /*
     * check if any variable changed
     */
    *vmap = octrl->nvariables;
    vmap -= nctrl->nvariables;
    if (octrl->nvariables != nctrl->nvariables) {
	return vmap;		/* changed */
    }
    for (i = 0; i <= nctrl->nvariables; i++) {
	if (vmap[i] != i) {
	    return vmap;	/* changed */
	}
    }
    /* no variable remapping needed */
    FREE(vmap);
    return (unsigned short *) NULL;
}

/*
 * NAME:	Control->undefined()
 * DESCRIPTION:	list the undefined functions in a program
 */
Array *ctrl_undefined(Dataspace *data, Control *ctrl)
{
    struct ulist {
	short count;		/* number of undefined functions */
	short index;		/* index in inherits list */
    } *u, *list;
    short i;
    dsymbol *symb;
    dfuncdef *f;
    Value *v;
    Object *obj;
    dinherit *inherits;
    dsymbol *symtab;
    unsigned short nsymbols;
    long size;
    Array *m;

    list = ALLOCA(ulist, ctrl->ninherits);
    memset(list, '\0', ctrl->ninherits * sizeof(ulist));
    inherits = ctrl->inherits;
    symtab = d_get_symbols(ctrl);
    nsymbols = ctrl->nsymbols;
    size = 0;

    /*
     * count the number of undefined functions per program
     */
    for (i = nsymbols, symb = symtab; i != 0; --i, symb++) {
	obj = OBJR(inherits[UCHAR(symb->inherit)].oindex);
	ctrl = (O_UPGRADING(obj)) ? OBJR(obj->prev)->ctrl : obj->control();
	if ((d_get_funcdefs(ctrl)[UCHAR(symb->index)].sclass & C_UNDEFINED) &&
	    list[UCHAR(symb->inherit)].count++ == 0) {
	    list[UCHAR(symb->inherit)].index = size;
	    size += 2;
	}
    }

    m = (Array *) NULL;
    try {
	ErrorContext::push();
	m = Array::mapCreate(data, size);
	memset(m->elts, '\0', size * sizeof(Value));
	for (i = nsymbols, symb = symtab; i != 0; --i, symb++) {
	    obj = OBJR(inherits[UCHAR(symb->inherit)].oindex);
	    ctrl = (O_UPGRADING(obj)) ? OBJR(obj->prev)->ctrl : obj->control();
	    f = d_get_funcdefs(ctrl) + UCHAR(symb->index);
	    if (f->sclass & C_UNDEFINED) {
		u = &list[UCHAR(symb->inherit)];
		v = &m->elts[u->index];
		if (v->string == (String *) NULL) {
		    String *str;
		    unsigned short len;

		    len = strlen(obj->name);
		    str = String::create((char *) NULL, len + 1L);
		    str->text[0] = '/';
		    memcpy(str->text + 1, obj->name, len);
		    PUT_STRVAL(v, str);
		    PUT_ARRVAL(v + 1, Array::createNil(data, u->count));
		    u->count = 0;
		}
		v = &v[1].array->elts[u->count++];
		PUT_STRVAL(v, d_get_strconst(ctrl, f->inherit, f->index));
	    }
	}
	ErrorContext::pop();
    } catch (...) {
	if (m != (Array *) NULL) {
	    /* discard mapping */
	    m->ref();
	    m->del();
	}
	AFREE(list);
	error((char *) NULL);	/* pass on error */
    }
    AFREE(list);

    m->mapSort();
    return m;
}
