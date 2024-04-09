/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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
# include "control.h"
# include "data.h"
# include "interpret.h"
# include "table.h"
# include "node.h"
# include "compile.h"

static Control *chead, *ctail;		/* list of control blocks */
static Sector nctrl;			/* # control blocks */
static Control *newctrl;		/* the new control block */

/*
 * create a new control block
 */
Control::Control()
{
    if (chead != (Control *) NULL) {
	/* insert at beginning of list */
	chead->prev = this;
	prev = (Control *) NULL;
	next = chead;
	chead = this;
    } else {
	/* list was empty */
	prev = next = (Control *) NULL;
	chead = ctail = this;
    }
    ndata = 0;
    nctrl++;

    flags = 0;
    version = VERSION_VM_MINOR;

    nsectors = 0;		/* nothing on swap device yet */
    sectors = (Sector *) NULL;
    oindex = UINDEX_MAX;
    instance = 0;
    ninherits = 0;
    inherits = (Inherit *) NULL;
    imapsz = 0;
    imap = (char *) NULL;
    compiled = 0;
    progsize = 0;
    prog = (char *) NULL;
    nstrings = 0;
    strings = (String **) NULL;
    sslength = (ssizet *) NULL;
    ssindex = (Uint *) NULL;
    stext = (char *) NULL;
    nfuncdefs = 0;
    funcdefs = (FuncDef *) NULL;
    nvardefs = 0;
    nclassvars = 0;
    vardefs = (VarDef *) NULL;
    cvstrings = (String **) NULL;
    classvars = (char *) NULL;
    nfuncalls = 0;
    funcalls = (char *) NULL;
    nsymbols = 0;
    symbols = (Symbol *) NULL;
    nvariables = 0;
    vtypes = (char *) NULL;
    vmapsize = 0;
    vmap = (unsigned short *) NULL;
}

/*
 * reference control block
 */
void Control::ref()
{
    if (this != chead) {
	/* move to head of list */
	prev->next = next;
	if (next != (Control *) NULL) {
	    next->prev = prev;
	} else {
	    ctail = prev;
	}
	prev = (Control *) NULL;
	next = chead;
	chead->prev = this;
	chead = this;
    }
}

/*
 * deref control block
 */
void Control::deref()
{
    /* swap this out first */
    if (this != ctail) {
	if (chead == this) {
	    chead = next;
	    chead->prev = (Control *) NULL;
	} else {
	    prev->next = next;
	    next->prev = prev;
	}
	ctail->next = this;
	prev = ctail;
	next = (Control *) NULL;
	ctail = this;
    }
}

/*
 * delete control block
 */
Control::~Control()
{
    String **strs;
    unsigned short i;

    /* delete strings */
    if (strings != (String **) NULL) {
	strs = strings;
	for (i = nstrings; i > 0; --i) {
	    if (*strs != (String *) NULL) {
		(*strs)->del();
	    }
	    strs++;
	}
	FREE(strings);
    }
    if (cvstrings != (String **) NULL) {
	strs = cvstrings;
	for (i = nvardefs; i > 0; --i) {
	    if (*strs != (String *) NULL) {
		(*strs)->del();
	    }
	    strs++;
	}
	FREE(cvstrings);
    }

    /* delete vmap */
    if (vmap != (unsigned short *) NULL) {
	FREE(vmap);
    }

    /* delete sectors */
    if (sectors != (Sector *) NULL) {
	FREE(sectors);
    }

    if (inherits != (Inherit *) NULL) {
	/* delete inherits */
	FREE(inherits);
    }

    if (prog != (char *) NULL) {
	FREE(prog);
    }

    /* delete inherit indices */
    if (imap != (char *) NULL) {
	FREE(imap);
    }

    /* delete string constants */
    if (sslength != (ssizet *) NULL) {
	FREE(sslength);
    }
    if (ssindex != (Uint *) NULL) {
	FREE(ssindex);
    }
    if (stext != (char *) NULL) {
	FREE(stext);
    }

    /* delete function definitions */
    if (funcdefs != (FuncDef *) NULL) {
	FREE(funcdefs);
    }

    /* delete variable definitions */
    if (vardefs != (VarDef *) NULL) {
	FREE(vardefs);
	if (classvars != (char *) NULL) {
	    FREE(classvars);
	}
    }

    /* delete function call table */
    if (funcalls != (char *) NULL) {
	FREE(funcalls);
    }

    /* delete symbol table */
    if (symbols != (Symbol *) NULL) {
	FREE(symbols);
    }

    /* delete variable types */
    if (vtypes != (char *) NULL) {
	FREE(vtypes);
    }

    if (this != chead) {
	prev->next = next;
    } else {
	chead = next;
	if (chead != (Control *) NULL) {
	    chead->prev = (Control *) NULL;
	}
    }
    if (this != ctail) {
	next->prev = prev;
    } else {
	ctail = prev;
	if (ctail != (Control *) NULL) {
	    ctail->next = (Control *) NULL;
	}
    }
    --nctrl;
}

/*
 * delete a control block from swap and memory
 */
void Control::del()
{
    if (sectors != (Sector *) NULL) {
	Swap::wipev(sectors, nsectors);
	Swap::delv(sectors, nsectors);
    }
    delete this;
}


class ObjHash : public Hash::Entry, public Allocated {
public:
    ObjHash(const char *name, ObjHash **h) {
	next = (Hash::Entry *) NULL;
	this->name = name;
	index = -1;		/* new object */
	priv = 0;
	list = olist;
	olist = h;
    }

    /*
     * put an object in the hash table
     */
    static ObjHash *create(const char *name) {
	ObjHash **h;

	h = (ObjHash **) otab->lookup(name, FALSE);
	if (*h == (ObjHash *) NULL) {
	    *h = new ObjHash(name, h);
	}

	return *h;
    }

    /*
     * initialize the object hash table
     */
    static void init() {
	otab = HM->create(OMERGETABSZ, OBJHASHSZ, FALSE);
    }

    /*
     * clear the object hash table
     */
    static void clear() {
	ObjHash **h, *f;

	for (h = olist; h != (ObjHash **) NULL; ) {
	    f = *h;
	    h = f->list;
	    delete f;
	}
	olist = (ObjHash **) NULL;

	if (otab != (Hash::Hashtab *) NULL) {
	    delete otab;
	    otab = (Hash::Hashtab *) NULL;
	}
    }

    Object *obj;		/* object */
    short index;		/* -1: new */
    short priv;			/* 1: direct private, 2: indirect private */

private:
    ObjHash **list;		/* next in linked list */

    static Hash::Hashtab *otab;	/* object hash table */
    static ObjHash **olist;	/* list of all object hash table entries */
};

Hash::Hashtab *ObjHash::otab;
ObjHash **ObjHash::olist;


# define VFH_CHUNK	64

/* variable/function hash table */
class VFH : public Hash::Entry, public ChunkAllocated {
public:
    static void create(String *str, ObjHash *ohash, unsigned short ct,
		       String *cvstr, short idx, VFH **addr);
    static void clear();

    String *str;		/* name string */
    ObjHash *ohash;		/* controlling object hash table entry */
    String *cvstr;		/* class variable string */
    unsigned short ct;		/* function call, or variable type */
    short index;		/* definition table index */

private:
    VFH(VFH **addr, char *name) {
	next = *addr;
	*addr = this;
	this->name = name;
    }
};

static class VFHChunk : public Chunk<VFH, VFH_CHUNK> {
public:
    /*
     * dereference strings when iterating through items
     */
    virtual bool item(VFH *h) {
	h->str->del();
	if (h->cvstr != (String *) NULL) {
	    h->cvstr->del();
	}
	return TRUE;
    }
} vchunk;

/*
 * create a new VFH table element
 */
void VFH::create(String *str, ObjHash *ohash, unsigned short ct, String *cvstr,
		 short idx, VFH **addr)
{
    VFH *h;

    h = chunknew (vchunk) VFH(addr, str->text);
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
 * clear the VFH tables
 */
void VFH::clear()
{
    vchunk.items();
    vchunk.clean();
}


class Label : public Allocated {
public:
    Label(String *str, ObjHash *ohash) : str(str), ohash(ohash) {
	str->ref();
	next = labels;
	labels = this;
    }

    virtual ~Label() {
	str->del();
    }

    /*
     * find a labeled object in the list
     */
    static ObjHash *find(const char *name)
    {
	Label *l;

	for (l = labels; l != (Label *) NULL; l = l->next) {
	    if (strcmp(l->str->text, name) == 0) {
		return l->ohash;
	    }
	}
	return (ObjHash *) NULL;
    }

    /*
     * wipe out all inheritance label declarations
     */
    static void clear()
    {
	Label *l, *f;

	l = labels;
	while (l != (Label *) NULL) {
	    f = l;
	    l = l->next;
	    delete f;
	}
	labels = (Label *) NULL;
    }

private:
    String *str;		/* label */
    ObjHash *ohash;		/* entry in hash table */
    Label *next;		/* next label */

    static Label *labels;	/* list of labeled inherited objects */
};

Label *Label::labels;


# define MAX_INHERITS		255
# define MAX_VARIABLES		(USHRT_MAX - 2)

static ObjHash *inherits[MAX_INHERITS * 2];	/* inherited objects */
static int ninherits;			/* # inherited objects */
static bool privinherit;		/* TRUE if private inheritance used */
static Hash::Hashtab *vtab;		/* variable merge table */
static Hash::Hashtab *ftab;		/* function merge table */
static unsigned short nvars;		/* # variables */
static unsigned short nsymbs;		/* # symbols */
static int nfclash;			/* # prototype clashes */
static Uint nifcalls;			/* # inherited function calls */

/*
 * initialize control block construction
 */
void Control::prepare()
{
    ObjHash::init();
    vtab = HM->create(VFMERGETABSZ, VFMERGEHASHSZ, FALSE);
    ftab = HM->create(VFMERGETABSZ, VFMERGEHASHSZ, FALSE);
}

/*
 * put variable definitions from an inherited object into the
 * variable merge table
 */
void Control::inheritVars(ObjHash *ohash)
{
    VarDef *v;
    int n;
    String *str, *cvstr;
    VFH **h;

    v = vars();
    for (n = 0; n < this->nvardefs; n++) {
	/*
	 * Add only non-private variables, and check if a variable with the
	 * same name hasn't been inherited already.
	 */
	if (!(v->sclass & C_PRIVATE)) {
	    str = strconst(v->inherit, v->index);
	    h = (VFH **) vtab->lookup(str->text, FALSE);
	    if (*h == (VFH *) NULL) {
		/* new variable */
		if (nclassvars != 0) {
		    cvstr = cvstrings[n];
		} else {
		    cvstr = (String *) NULL;
		}
		VFH::create(str, ohash, v->type, cvstr, n, h);
	    } else {
	       /* duplicate variable */
	       Compile::error("multiple inheritance of variable %s (/%s, /%s)",
			      str->text, (*h)->ohash->name, ohash->name);
	    }
	}
	v++;
    }
}

/*
 * compare two class strings
 */
bool Control::compareClass(Uint s1, Control *ctrl, Uint s2)
{
    if (this == ctrl && s1 == s2) {
	return TRUE;	/* the same */
    }
    if (compiled == 0 && (s1 >> 16) == ::ninherits) {
	return FALSE;	/* one is new, and therefore different */
    }
    if (ctrl->compiled == 0 && (s2 >> 16) == ::ninherits) {
	return FALSE;	/* one is new, and therefore different */
    }
    return !strconst(s1 >> 16, s1 & 0xffff)->cmp(ctrl->strconst(s2 >> 16,
								s2 & 0xffff));
}

/*
 * Compare two prototypes. Return TRUE if equal.
 */
bool Control::compareProto(char *prot1, Control *ctrl, char *prot2)
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
	    if (!compareClass(s1, ctrl, s2)) {
		return FALSE;
	    }
	}
    } while (--i >= 0);

    return TRUE;	/* equal */
}

/*
 * put a function definition from an inherited object into
 * the function merge table
 */
void Control::inheritFunc(int idx, ObjHash *ohash)
{
    VFH **h, **l;
    FuncDef *f;
    String *str;

    f = &funcdefs[idx];
    str = strconst(f->inherit, f->index);
    if (ohash->priv != 0 && (f->sclass & C_NOMASK)) {
	/*
	 * privately inherited nomask function is not allowed
	 */
	Compile::error("private inherit of nomask function %s (/%s)", str->text,
		       ohash->name);
	return;
    }

    h = (VFH **) ftab->lookup(str->text, FALSE);
    if (*h == (VFH *) NULL) {
	/*
	 * New function (-1: no calls to it yet)
	 */
	VFH::create(str, ohash, -1, (String *) NULL, idx, h);
	if (ohash->priv == 0 &&
	    (ninherits != 1 ||
	     (f->sclass & (C_STATIC | C_UNDEFINED)) != C_STATIC)) {
	    /*
	     * don't count privately inherited functions, or static functions
	     * from the auto object
	     */
	    nsymbs++;
	}
    } else {
	Inherit *inh;
	int n;
	Object *o;
	Control *ctrl;
	char *prot1, *prot2;
	bool privflag, inhflag, firstsym;
	int nfunc, npriv;

	/*
	 * prototype already exists
	 */
	prot1 = prog + f->offset;

	/*
	 * First check if the new function's object is inherited by the
	 * object that defines the function in the merge table.
	 */
	privflag = FALSE;
	o = ohash->obj;
	for (l = h;
	     *l != (VFH *) NULL && strcmp((*l)->name, str->text) == 0;
	     l = (VFH **) &(*l)->next) {
	    if ((*l)->ohash == (ObjHash *) NULL) {
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
	while (*l != (VFH *) NULL && strcmp((*l)->name, str->text) == 0) {
	    if ((*l)->ohash == (ObjHash *) NULL) {
		l = (VFH **) &(*l)->next;
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
				l = (VFH **) &(*l)->next;
				break;
			    }
			}
			*l = (VFH *) (*l)->next;
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
			Compile::error("multiple inheritance of nomask function %s (/%s, /%s)",
				       str->text, (*l)->ohash->name,
				       ohash->name);
			return;
		    }
		    if (((f->sclass | PROTO_CLASS(prot2)) & C_UNDEFINED) &&
			!ohash->obj->ctrl->compareProto(prot1, ctrl, prot2)) {
			/*
			 * prototype conflict
			 */
			Compile::error("unequal prototypes for function %s (/%s, /%s)",
				       str->text, (*l)->ohash->name,
				       ohash->name);
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
		    l = (VFH **) &(*l)->next;
		    break;
		}
	    }
	}

	if (firstsym && ohash->priv == 0) {
	    nsymbs++;	/* first symbol */
	}

	if (inhflag) {
	    /* insert new prototype at the beginning */
	    VFH::create(str, ohash, -1, (String *) NULL, idx, h);
	    h = (VFH **) &(*h)->next;
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
	    h = (VFH **) &(*h)->next;
	}

	/* add/remove clash markers */
	if (*h != (VFH *) NULL &&
	    strcmp((*h)->name, str->text) == 0) {
	    /*
	     * there are other prototypes
	     */
	    if ((*h)->ohash == (ObjHash *) NULL) {
		/* first entry is clash marker */
		if (nfunc + npriv <= 1) {
		    /* remove it */
		    *h = (VFH *) (*h)->next;
		    --nfclash;
		} else {
		    /* adjust it */
		    (*h)->index = nfunc;
		    h = (VFH **) &(*h)->next;
		}
	    } else if (nfunc + npriv > 1) {
		/* add new clash marker as first entry */
		VFH::create(str, (ObjHash *) NULL, 0, (String *) NULL, nfunc, h);
		nfclash++;
		h = (VFH **) &(*h)->next;
	    }
	}

	/* add new prototype, undefined at the end */
	if (!inhflag) {
	    if (PROTO_CLASS(prot1) & C_UNDEFINED) {
		VFH::create(str, ohash, -1, (String *) NULL, idx, l);
	    } else {
		VFH::create(str, ohash, -1, (String *) NULL, idx, h);
	    }
	}
    }
}

/*
 * put function definitions from an inherited object into
 * the function merge table
 */
void Control::inheritFuncs(ObjHash *ohash)
{
    short n;
    FuncDef *f;

    program();
    for (n = 0, f = funcs(); n < nfuncdefs; n++, f++) {
	if (!(f->sclass & C_PRIVATE)) {
	    inheritFunc(n, ohash);
	}
    }
}

/*
 * inherit an object
 */
bool Control::inherit(Frame *f, char *from, Object *obj, String *label,
		      int priv)
{
    ObjHash *ohash;
    Control *ctrl;
    Inherit *inh;
    int i;
    Object *o;

    if (!(obj->flags & O_MASTER)) {
	Compile::error("cannot inherit cloned object");
	return TRUE;
    }
    if (O_UPGRADING(obj)) {
	Compile::error("cannot inherit object being upgraded");
	return TRUE;
    }

    ohash = ObjHash::create(obj->name);
    if (label != (String *) NULL) {
	/*
	 * use a label
	 */
	if (Label::find(label->text) != (ObjHash *) NULL) {
	    Compile::error("redeclaration of label %s", label->text);
	}
	new Label(label, ohash);
    }

    if (ohash->index < 0) {
	/*
	 * new inherited object
	 */
	ctrl = obj->control();
	inh = ctrl->inherits;
	if (::ninherits != 0 && strcmp(OBJR(inh->oindex)->name,
				       ::inherits[0]->obj->name) != 0) {
	    Compile::error("inherited different auto objects");
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
		    Compile::error("cycle in inheritance");
		    return TRUE;
		}

		/*
		 * This object inherits an object that has been destructed.
		 * Give the driver object a chance to destruct it.
		 */
		(--f->sp)->type = T_OBJECT;
		f->sp->oindex = obj->index;
		f->sp->objcnt = ocount = obj->count;
		DGD::callDriver(f, "recompile", 1);
		(f->sp++)->del();
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
	    ohash = ObjHash::create(o->name);
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
		    o->ctrl->inheritFuncs(ohash);
		    o->ctrl->inheritVars(ohash);
		}
	    } else if (ohash->obj != o) {
		/*
		 * inherited two different objects with same name
		 */
		Compile::error("inherited different instances of /%s", o->name);
		return TRUE;
	    } else if (!inh->priv && ohash->priv > priv) {
		/*
		 * add to function and variable table
		 */
		if (ohash->priv == 2) {
		    o->ctrl->inheritVars(ohash);
		}
		ohash->priv = priv;
		o->ctrl->inheritFuncs(ohash);
	    }
	}

	for (i = ctrl->ninherits; i > 0; --i) {
	    /*
	     * add to the inherited array
	     */
	    ohash = ObjHash::create(OBJR(inh->oindex)->name);
	    if (ohash->index < 0) {
		ohash->index = ::ninherits;
		::inherits[::ninherits++] = ohash;
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
	Compile::error("inherited different instances of /%s", obj->name);
    } else if (ohash->priv > priv) {
	/*
	 * previously inherited with greater privateness; process all
	 * objects inherited by this object
	 */
	ctrl = obj->control();
	for (i = ctrl->ninherits, inh = ctrl->inherits + i; i > 0; --i) {
	    --inh;
	    o = OBJR(inh->oindex);
	    ohash = ObjHash::create(o->name);
	    if (!inh->priv && ohash->priv > priv) {
		/*
		 * add to function and variable table
		 */
		if (ohash->priv == 2) {
		    o->ctrl->inheritVars(ohash);
		}
		ohash->priv = priv;
		o->ctrl->inheritFuncs(ohash);
	    }
	}
    }

    if (::ninherits >= MAX_INHERITS) {
	Compile::error("too many objects inherited");
    }

    return TRUE;
}


# define STRING_CHUNK	64

struct StrPtr : public ChunkAllocated {
    String *str;
};

static class StrChunk : public Chunk<StrPtr, STRING_CHUNK> {
public:
    /*
     * copy or dereference when iterating through items
     */
    virtual bool item(StrPtr *s) {
	if (copy != (String **) NULL) {
	    *--copy = s->str;
	    strsize += s->str->len;
	} else {
	    s->str->del();
	}
	return TRUE;
    }

    /*
     * build string constant table and clean up
     */
    long makeTable(String **s) {
	copy = s;
	strsize = 0;
	items();
	Chunk<StrPtr, STRING_CHUNK>::clean();
	return strsize;
    }

    /*
     * override Chunk::clean()
     */
    void clean()
    {
	copy = (String **) NULL;
	items();
	Chunk<StrPtr, STRING_CHUNK>::clean();
    }

private:
    String **copy;			/* string copy table or NULL */
    long strsize;			/* cumulative length of all strings */
} schunk;

# define FCALL_CHUNK	64

struct CharPtr : public ChunkAllocated {
    char *name;
};

static class FunCallChunk : public Chunk<CharPtr, FCALL_CHUNK> {
public:
    /*
     * build function call table when iterating through items
     */
    virtual bool item(CharPtr *name) {
	VFH *h;

	h = *(VFH **) ftab->lookup(name->name, FALSE);
	*--fcalls = h->index;
	*--fcalls = h->ohash->index;
	return TRUE;
    }

    /*
     * build function call table
     */
    void makeTable(char *fc) {
	fcalls = fc;
	items();
    }

private:
    char *fcalls;			/* function call pointer */
} fchunk;

struct FuncInfo {
    FuncDef func;			/* function name/type */
    char *name;				/* function name */
    char *proto;			/* function prototype */
    String *cfstr;			/* function class string */
    char *prog;				/* function program */
    unsigned short progsize;		/* function program size */
};

static ObjHash *newohash;		/* fake ohash entry for new object */
static Uint nstrs;			/* # of strings in all string chunks */
static FuncInfo *functions;		/* defined functions table */
static int nfdefs, fdef;		/* # defined functions, current func */
static int nundefs;			/* # private undefined prototypes */
static Uint progsize;			/* size of all programs and protos */
static VarDef *variables;		/* defined variables */
static String **cvstrings;		/* variable class strings */
static char *classvars;			/* class variables */
static int nclassvars;			/* # classvars */
static Uint nfcalls;			/* # function calls */

/*
 * initialize inherit map
 */
void Control::inheritMap()
{
    Inherit *inh;
    int i, j, n, sz;
    Control *ctrl;

    sz = ninherits;
    for (n = sz - 1, inh = &inherits[n]; n > 0; ) {
	--n;
	(--inh)->progoffset = sz;
	ctrl = OBJR(inh->oindex)->ctrl;
	for (i = 0; i < ctrl->ninherits; i++) {
	    imap[sz++] = ObjHash::create(OBJR(ctrl->inherits[UCHAR(ctrl->imap[i])].oindex)->name)->index;
	}
	for (j = ninherits - n; --j > 0; ) {
	    if (memcmp(imap + inh->progoffset,
		       imap + inh[j].progoffset, i) == 0) {
		/* merge with table of inheriting object */
		inh->progoffset = inh[j].progoffset;
		sz -= i;
		break;
	    }
	}
    }
    imap = REALLOC(imap, char, imapsz, sz);
    imapsz = sz;
}

/*
 * make an initial control block
 */
void Control::create()
{
    Inherit *inh;
    Control *ctrl;
    unsigned short n;
    int i, count;
    ObjHash *ohash;

    /*
     * create a new control block
     */
    newohash = ObjHash::create("/");		/* unique name */
    newohash->index = ::ninherits;
    newctrl = new Control();
    inh = newctrl->inherits =
	  ALLOC(Inherit, newctrl->ninherits = ::ninherits + 1);
    newctrl->imap = ALLOC(char, (::ninherits + 2) * (::ninherits + 1) / 2);
    nvars = 0;
    String::merge();

    /*
     * Fix function offsets and variable offsets, and collect all string
     * constants from inherited objects and put them in the string merge
     * table.
     */
    for (count = 0; count < ::ninherits; count++) {
	newctrl->imap[count] = count;
	ohash = ::inherits[count];
	inh->oindex = ohash->obj->index;
	ctrl = ohash->obj->ctrl;
	i = ctrl->ninherits - 1;
	inh->funcoffset = nifcalls;
	n = ctrl->nfuncalls - ctrl->inherits[i].funcoffset;
	if (nifcalls > UINDEX_MAX - n) {
	    Compile::error("inherited too many function calls");
	}
	nifcalls += n;
	inh->varoffset = nvars;
	if (nvars > MAX_VARIABLES - ctrl->nvardefs) {
	    Compile::error("inherited too many variables");
	}
	nvars += ctrl->nvardefs;

	for (n = ctrl->nstrings; n > 0; ) {
	    --n;
	    ctrl->strconst(i, n)->put(((Uint) count << 16) | n);
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
    newctrl->inheritMap();

    /*
     * prepare for construction of a new control block
     */
    functions = ALLOC(FuncInfo, 256);
    variables = ALLOC(VarDef, 256);
    ::cvstrings = ALLOC(String*, 256 * sizeof(String*));
    ::classvars = ALLOC(char, 256 * 3);
    ::progsize = 0;
    nstrs = 0;
    nfdefs = 0;
    nvars = 0;
    ::nclassvars = 0;
    nfcalls = 0;
}

/*
 * define a new (?) string constant
 */
long Control::defString(String *str)
{
    Uint desc, ndesc;

    desc = str->put(ndesc = ((Uint) ::ninherits << 16) | nstrs);
    if (desc == ndesc) {
	/*
	 * it is really a new string
	 */
	(chunknew (schunk) StrPtr)->str = str;
	str->ref();
	if (nstrs == USHRT_MAX) {
	    Compile::error("too many string constants");
	}
	nstrs++;
    }
    if (desc >> 16 == ::ninherits) {
	desc |= 0x01000000L;	/* mark it as new */
    }
    return desc;
}

/*
 * define a new function prototype
 */
void Control::defProto(String *str, char *proto, String *sclass)
{
    VFH **h, **l;
    FuncDef *func;
    char *proto2;
    Control *ctrl;
    int i;
    long s;

    /* first check if prototype exists already */
    h = l = (VFH **) ftab->lookup(str->text, FALSE);
    if (*h != (VFH *) NULL) {
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
		Compile::error("multiple declaration of function %s",
			       str->text);
	    } else if (!newctrl->compareProto(proto, newctrl, proto2)) {
		if ((PROTO_CLASS(proto) ^ PROTO_CLASS(proto2)) & C_UNDEFINED) {
		    /*
		     * declaration does not match prototype
		     */
		    Compile::error("declaration does not match prototype of %s",
				   str->text);
		} else {
		    /*
		     * unequal prototypes
		     */
		    Compile::error("unequal prototypes for function %s",
				   str->text);
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
		::progsize += i - PROTO_SIZE(proto2);
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
	if ((*h)->ohash != (ObjHash *) NULL) {
	    ctrl = (*h)->ohash->obj->ctrl;
	    proto2 = ctrl->prog + ctrl->funcdefs[(*h)->index].offset;
	    if (!(PROTO_CLASS(proto) & C_UNDEFINED) &&
		(PROTO_CLASS(proto2) & C_UNDEFINED) &&
		!newctrl->compareProto(proto, ctrl, proto2)) {
		/*
		 * declaration does not match inherited prototype
		 */
		Compile::error("inherited different prototype for %s (/%s)",
			       str->text, (*h)->ohash->name);
	    } else if ((PROTO_CLASS(proto) & C_UNDEFINED) &&
		       (*h)->ohash->priv == 0 &&
		       (ctrl->ninherits != 1 ||
			(PROTO_CLASS(proto2) & (C_STATIC | C_UNDEFINED)) !=
								    C_STATIC) &&
		       PROTO_FTYPE(proto2) != T_IMPLICIT &&
		       newctrl->compareProto(proto, ctrl, proto2)) {
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
		Compile::error("redeclaration of nomask function %s (/%s)",
			       str->text, (*h)->ohash->name);
	    }

	    if ((*l)->ohash->priv != 0) {
		l = (VFH **) &(*l)->next;	/* skip private function */
	    }
	}
    }

    if (!(PROTO_CLASS(proto) & C_PRIVATE)) {
	/*
	 * may be a new symbol
	 */
	if (*l == (VFH *) NULL || strcmp((*l)->name, str->text) != 0) {
	    nsymbs++;		/* no previous symbol */
	} else if ((*l)->ohash == (ObjHash *) NULL) {
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
	Compile::error("too many functions declared");
    }

    /*
     * Actual definition.
     */
    VFH::create(str, newohash, -1, (String *) NULL, nfdefs, h);
    s = defString(str);
    i = PROTO_SIZE(proto);
    functions[nfdefs].name = str->text;
    functions[nfdefs].proto = (char *) memcpy(ALLOC(char, i), proto, i);
    functions[nfdefs].cfstr = sclass;
    if (sclass != (String *) NULL) {
	sclass->ref();
    }
    functions[nfdefs].progsize = 0;
    ::progsize += i;
    func = &functions[nfdefs++].func;
    func->sclass = PROTO_CLASS(proto);
    func->inherit = s >> 16;
    func->index = s;
}

/*
 * define a new function
 */
void Control::defFunc(String *str, char *proto, String *sclass)
{
    fdef = nfdefs;
    defProto(str, proto, sclass);
}

/*
 * define a function body
 */
void Control::defProgram(char *prog, unsigned int size)
{
    functions[fdef].prog = prog;
    functions[fdef].progsize = size;
    ::progsize += size;
}

/*
 * define a variable
 */
void Control::defVar(String *str, unsigned int sclass, unsigned int type,
		     String *cvstr)
{
    VFH **h;
    VarDef *var;
    char *p;
    long s;

    h = (VFH **) vtab->lookup(str->text, FALSE);
    if (*h != (VFH *) NULL) {
	if ((*h)->ohash == newohash) {
	    Compile::error("redeclaration of variable %s", str->text);
	    return;
	} else if (!(sclass & C_PRIVATE)) {
	    /*
	     * non-private redeclaration of a variable
	     */
	    Compile::error("redeclaration of variable %s (/%s)", str->text,
			   (*h)->ohash->name);
	    return;
	}
    }
    if (nvars == 255 || newctrl->nvariables + nvars == MAX_VARIABLES) {
	Compile::error("too many variables declared");
    }

    /* actually define the variable */
    VFH::create(str, newohash, type, cvstr, nvars, h);
    s = defString(str);
    var = &variables[nvars];
    var->sclass = sclass;
    var->inherit = s >> 16;
    var->index = s;
    var->type = type;
    ::cvstrings[nvars++] = cvstr;
    if (cvstr != (String *) NULL) {
	cvstr->ref();
	s = defString(cvstr);
	p = ::classvars + ::nclassvars++ * 3;
	*p++ = s >> 16;
	*p++ = s >> 8;
	*p = s;
    }
}

/*
 * call an inherited function
 */
char *Control::iFunCall(String *str, const char *label, String **cfstr,
			long *call)
{
    Control *ctrl;
    ObjHash *ohash;
    short index;
    char *proto;

    *cfstr = (String *) NULL;

    if (label != (char *) NULL) {
	Symbol *symb;

	/* first check if the label exists */
	ohash = Label::find(label);
	if (ohash == (ObjHash *) NULL) {
	    Compile::error("undefined label %s", label);
	    return (char *) NULL;
	}
	symb = (ctrl = ohash->obj->ctrl)->symb(str->text, str->len);
	if (symb == (Symbol *) NULL) {
	    if (ctrl->ninherits != 1) {
		ohash = ::inherits[0];
		symb = (ctrl = ohash->obj->ctrl)->symb(str->text, str->len);
	    }
	    if (symb == (Symbol *) NULL) {
		/*
		 * It may seem strange to allow label::kfun, but remember that
		 * they are supposed to be inherited by the auto object.
		 */
		index = KFun::kfunc(str->text);
		if (index >= 0) {
		    /* kfun call */
		    *call = ((long) KFCALL << 24) | index;
		    return KFUN(index).proto;
		}
		Compile::error("undefined function %s::%s", label, str->text);
		return (char *) NULL;
	    }
	}
	ohash = ObjHash::create(OBJR(ctrl->inherits[UCHAR(symb->inherit)].oindex)->name);
	index = UCHAR(symb->index);
    } else {
	VFH *h;

	/* check if the function exists */
	h = *(VFH **) ftab->lookup(str->text, FALSE);
	if (h == (VFH *) NULL || (h->ohash == newohash &&
	    ((h=(VFH *) h->next) == (VFH *) NULL ||
	     strcmp(h->name, str->text) != 0))) {

	    index = KFun::kfunc(str->text);
	    if (index >= 0) {
		/* kfun call */
		*call = ((long) KFCALL << 24) | index;
		return KFUN(index).proto;
	    }
	    Compile::error("undefined function ::%s", str->text);
	    return (char *) NULL;
	}
	ohash = h->ohash;
	if (ohash == (ObjHash *) NULL) {
	    /*
	     * call to multiple inherited function
	     */
	    Compile::error("ambiguous call to function ::%s", str->text);
	    return (char *) NULL;
	}
	index = h->index;
	label = "";
    }

    ctrl = ohash->obj->ctrl;
    if (ctrl->funcdefs[index].sclass & C_UNDEFINED) {
	Compile::error("undefined function %s::%s", label, str->text);
	return (char *) NULL;
    }
    *call = ((long) DFCALL << 24) | ((unsigned short) ohash->index << 8) |
	    index;
    proto = ctrl->prog + ctrl->funcdefs[index].offset;

    if ((PROTO_FTYPE(proto) & T_TYPE) == T_CLASS) {
	char *p;
	Uint sclass;

	p = &PROTO_FTYPE(proto) + 1;
	FETCH3U(p, sclass);
	*cfstr = ctrl->strconst(sclass >> 16, sclass & 0xffff);
    }
    return proto;
}

/*
 * call a function
 */
char *Control::funCall(String *str, String **cfstr, long *call,
		       int typechecking)
{
    VFH *h;
    char *proto;

    *cfstr = (String *) NULL;

    h = *(VFH **) ftab->lookup(str->text, FALSE);
    if (h == (VFH *) NULL) {
	static char uproto[] = { (char) C_UNDEFINED, 0, 0, 0, 6, T_IMPLICIT };
	short kf;

	/*
	 * undefined function
	 */
	kf = KFun::kfunc(str->text);
	if (kf >= 0) {
	    /* kfun call */
	    *call = ((long) KFCALL << 24) | kf;
	    return KFUN(kf).proto;
	}

	/* create an undefined prototype for the function */
	if (nfdefs == 255) {
	    Compile::error("too many undefined functions");
	    return (char *) NULL;
	}
	defProto(str, proto = uproto, (String *) NULL);
	h = *(VFH **) ftab->lookup(str->text, FALSE);
    } else if (h->ohash == newohash) {
	/*
	 * call to new function
	 */
	proto = functions[h->index].proto;
	*cfstr = functions[h->index].cfstr;
    } else if (h->ohash == (ObjHash *) NULL) {
	/*
	 * call to multiple inherited function
	 */
	Compile::error("ambiguous call to function %s", str->text);
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
	    *cfstr = ctrl->strconst(sclass >> 16, sclass & 0xffff);
	}
    }

    if (typechecking && PROTO_FTYPE(proto) == T_IMPLICIT) {
	/* don't allow calls to implicit prototypes when typechecking */
	Compile::error("undefined function %s", str->text);
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
		    ((unsigned short) h->ohash->index << 8) | h->index;
	}
    } else {
	/* ordinary function call */
	*call = ((long) FCALL << 24) | ((unsigned short) h->ohash->index << 8) |
		h->index;
    }
    return proto;
}

/*
 * generate a function call
 */
unsigned short Control::genCall(long call)
{
    VFH *h;
    char *name;
    short inherit, index;

    inherit = (call >> 8) & 0xff;
    index = call & 0xff;
    if (inherit == ::ninherits) {
	name = functions[index].name;
    } else {
	Control *ctrl;
	FuncDef *f;

	ctrl = OBJR(newctrl->inherits[inherit].oindex)->ctrl;
	f = ctrl->funcdefs + index;
	name = ctrl->strconst(f->inherit, f->index)->text;
    }
    h = *(VFH **) ftab->lookup(name, FALSE);
    if (h->ct == (unsigned short) -1) {
	/*
	 * add to function call table
	 */
	(chunknew (fchunk) CharPtr)->name = name;
	if (nifcalls + nfcalls == UINDEX_MAX) {
	    Compile::error("too many function calls");
	}
	h->ct = nfcalls++;
    }
    return h->ct;
}

/*
 * handle a variable reference
 */
unsigned short Control::var(String *str, long *ref, String **cvstr)
{
    VFH *h;

    /* check if the variable exists */
    h = *(VFH **) vtab->lookup(str->text, TRUE);
    if (h == (VFH *) NULL) {
	Compile::error("undeclared variable %s", str->text);
	if (nvars < 255) {
	    /* don't repeat this error */
	    defVar(str, 0, T_MIXED, (String *) NULL);
	}
	*cvstr = (String *) NULL;
	*ref = 0;
	return T_MIXED;
    }

    if (h->ohash->index == 0 && ::ninherits != 0) {
	*ref = h->index;
    } else {
	*ref = ((unsigned short) h->ohash->index << 8) | h->index;
    }
    *cvstr = h->cvstr;
    return h->ct;	/* the variable type */
}

/*
 * return the number of objects inherited
 */
int Control::nInherits()
{
    return ::ninherits;
}


/*
 * check function definitions
 */
bool Control::checkFuncs()
{
    if (nundefs != 0) {
	FuncInfo *f;
	unsigned short i;

	/*
	 * private undefined prototypes
	 */
	Compile::error("undefined private functions:");
	for (f = functions, i = nundefs; i != 0; f++) {
	    if ((f->func.sclass & (C_PRIVATE | C_UNDEFINED)) ==
						    (C_PRIVATE | C_UNDEFINED)) {
		Compile::error("  %s", f->name);
		--i;
	    }
	}
	return FALSE;
    }

    if (nfclash != 0 || privinherit) {
	Hash::Entry **t;
	unsigned short sz;
	VFH **f, **n;
	bool clash;

	clash = FALSE;
	for (t = ftab->table, sz = ftab->size; sz > 0; t++, --sz) {
	    for (f = (VFH **) t; *f != (VFH *) NULL; ) {
		if ((*f)->ohash == (ObjHash *) NULL) {
		    /*
		     * clash marker found
		     */
		    if ((*f)->index <= 1) {
			/*
			 * erase clash which involves at most one function
			 * that isn't privately inherited
			 */
			*f = (VFH *) (*f)->next;
		    } else {
			/*
			 * list a clash (only the first two)
			 */
			if (!clash) {
			    clash = TRUE;
			    Compile::error("inherited multiple instances of:");
			}
			f = (VFH **) &(*f)->next;
			while ((*f)->ohash->priv != 0) {
			    f = (VFH **) &(*f)->next;
			}
			n = (VFH **) &(*f)->next;
			while ((*n)->ohash->priv != 0) {
			    n = (VFH **) &(*n)->next;
			}
			Compile::error("  %s (/%s, /%s)", (*f)->name,
				       (*f)->ohash->name, (*n)->ohash->name);
			f = (VFH **) &(*n)->next;
		    }
		} else if ((*f)->ohash->priv != 0) {
		    /*
		     * skip privately inherited function
		     */
		    f = (VFH **) &(*f)->next;
		} else {
		    n = (VFH **) &(*f)->next;
		    if (*n != (VFH *) NULL && (*n)->ohash != (ObjHash *) NULL &&
			(*n)->ohash->priv != 0) {
			/* skip privately inherited function */
			n = (VFH **) &(*n)->next;
		    }
		    if (*n != (VFH *) NULL && (*n)->ohash == (ObjHash *) NULL &&
			strcmp((*n)->str->text, (*f)->str->text) == 0 &&
			!(PROTO_CLASS(functions[(*f)->index].proto) &C_PRIVATE))
		    {
			/*
			 * this function was redefined, skip the clash marker
			 */
			n = (VFH **) &(*n)->next;
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
 * create the string table for the new control block
 */
void Control::makeStrings()
{
    long strsize;

    strsize = 0;
    if ((newctrl->nstrings = nstrs) != 0) {
	newctrl->strings = ALLOC(String*, newctrl->nstrings);
	strsize = schunk.makeTable(newctrl->strings + nstrs);
    }
    newctrl->strsize = strsize;
}

/*
 * make the function definition table for the control block
 */
void Control::makeFuncs()
{
    char *p;
    FuncDef *d;
    FuncInfo *f;
    int i;
    unsigned int len;

    newctrl->progsize = ::progsize;
    if ((newctrl->nfuncdefs = nfdefs) != 0) {
	p = newctrl->prog = ALLOC(char, ::progsize);
	d = newctrl->funcdefs = ALLOC(FuncDef, nfdefs);
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
 * make the variable definition table for the control block
 */
void Control::makeVars()
{
    if ((newctrl->nvardefs = nvars) != 0) {
	newctrl->vardefs = ALLOC(VarDef, nvars);
	memcpy(newctrl->vardefs, variables, nvars * sizeof(VarDef));
	if ((newctrl->nclassvars = ::nclassvars) != 0) {
	    unsigned short i;
	    String **s;

	    newctrl->cvstrings = ALLOC(String*, nvars * sizeof(String*));
	    memcpy(newctrl->cvstrings, ::cvstrings, nvars * sizeof(String*));
	    for (i = nvars, s = newctrl->cvstrings; i != 0; --i, s++) {
		if (*s != (String *) NULL) {
		    (*s)->ref();
		}
	    }
	    newctrl->classvars = ALLOC(char, ::nclassvars * 3);
	    memcpy(newctrl->classvars, ::classvars, ::nclassvars * 3);
	}
    }
}

/*
 * make the function call table for the control block
 */
void Control::makeFunCalls()
{
    char *fc;
    int i;
    VFH *h;
    Inherit *inh;
    ObjHash *ohash;

    newctrl->nfuncalls = nifcalls + nfcalls;
    if (newctrl->nfuncalls == 0) {
	return;
    }
    fc = newctrl->funcalls = ALLOC(char, 2L * newctrl->nfuncalls);
    for (i = 0, inh = newctrl->inherits; i < ::ninherits; i++, inh++) {
	/*
	 * Walk through the list of inherited objects, starting with the auto
	 * object, and fill in the function call table segment for each object
	 * once.
	 */
	ohash = ObjHash::create(OBJR(inh->oindex)->name);
	if (ohash->index == i) {
	    char *ofc;
	    FuncDef *f;
	    Control *ctrl;
	    Object *obj;
	    uindex j, n;

	    /*
	     * build the function call segment, based on the function call
	     * table of the inherited object
	     */
	    ctrl = ohash->obj->ctrl;
	    j = ctrl->ninherits - 1;
	    ofc = ctrl->funCalls() + 2L * ctrl->inherits[j].funcoffset;
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
			j = ObjHash::create(obj->name)->index;
		    }
		    *fc++ = j;
		    *fc++ = ofc[1];
		} else {
		    h = *(VFH **) ftab->lookup(obj->ctrl->strconst(f->inherit,
							      f->index)->text,
					       FALSE);
		    if (h->ohash->index == ::ninherits &&
			(functions[h->index].func.sclass & C_PRIVATE)) {
			/*
			 * private redefinition of (guaranteed non-private)
			 * inherited function
			 */
			h = (VFH *) h->next;
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
    fchunk.makeTable(fc + 2L * nfcalls);
}

# define SYMBHASH	10	/* keep unchanged for snapshot compatibility */

/*
 * make the symbol table for the control block
 */
void Control::makeSymbols()
{
    unsigned short i, n, x, ncoll;
    Symbol *symtab, *coll;
    Inherit *inh;

    if ((newctrl->nsymbols = nsymbs) == 0) {
	return;
    }

    /* initialize */
    symtab = newctrl->symbols = ALLOC(Symbol, nsymbs);
    for (i = 0; i < nsymbs; i++) {
	symtab->next = i;	/* mark as unused */
	symtab++;
    }
    symtab = newctrl->symbols;
    coll = ALLOCA(Symbol, nsymbs);
    ncoll = 0;

    /*
     * Go down the list of inherited objects, adding the functions of each
     * object once.
     */
    for (i = 0, inh = newctrl->inherits; i <= ::ninherits; i++, inh++) {
	FuncDef *f;
	Control *ctrl;

	if (i == ::ninherits) {
	    ctrl = newctrl;
	} else if (!inh->priv &&
		   ObjHash::create(OBJR(inh->oindex)->name)->index == i) {
	    ctrl = OBJR(inh->oindex)->ctrl;
	} else {
	    continue;
	}

	for (f = ctrl->funcdefs, n = 0; n < ctrl->nfuncdefs; f++, n++) {
	    VFH *h;
	    char *name;

	    if ((f->sclass & C_PRIVATE) ||
		(i == 0 && ::ninherits != 0 &&
		 (f->sclass & (C_STATIC | C_UNDEFINED)) == C_STATIC)) {
		continue;	/* not in symbol table */
	    }
	    name = ctrl->strconst(f->inherit, f->index)->text;
	    h = *(VFH **) ftab->lookup(name, FALSE);
	    if (h->ohash->index == ::ninherits &&
		(functions[h->index].func.sclass & C_PRIVATE)) {
		/*
		 * private redefinition of inherited function:
		 * use inherited function
		 */
		h = (VFH *) h->next;
	    }
	    while (h->ohash->priv != 0) {
		/*
		 * skip privately inherited function
		 */
		h = (VFH *) h->next;
	    }
	    if (i == h->ohash->index) {
		/*
		 * all non-private functions are put into the hash table
		 */
		x = HM->hashstr(name, SYMBHASH) % nsymbs;
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
 * make the variable type table for the control block
 */
void Control::makeVarTypes()
{
    char *type;
    unsigned short max, nv, n;
    Inherit *inh;
    Control *ctrl;
    VarDef *var;

    max = newctrl->nvariables - newctrl->nvardefs;
    if (max == 0) {
	return;
    }

    newctrl->vtypes = type = ALLOC(char, max);
    for (nv = 0, inh = newctrl->inherits; nv != max; inh++) {
	if (inh->varoffset == nv) {
	    ctrl = OBJR(inh->oindex)->control();
	    for (n = ctrl->nvardefs, nv += n, var = ctrl->vars();
		 n != 0; --n, var++) {
		if (T_ARITHMETIC(var->type)) {
		    *type++ = var->type;
		} else {
		    *type++ = nil.type;
		}
	    }
	}
    }
}

/*
 * construct and return a control block for the object just compiled
 */
Control *Control::construct()
{
    Control *ctrl;

    ctrl = newctrl;
    ctrl->nvariables += nvars;

    makeStrings();
    makeFuncs();
    makeVars();
    makeFunCalls();
    makeSymbols();
    makeVarTypes();
    ctrl->compiled = P_time();

    newctrl = (Control *) NULL;
    return ctrl;
}

/*
 * clean up
 */
void Control::clear()
{
    ObjHash::clear();
    VFH::clear();
    if (vtab != (Hash::Hashtab *) NULL) {
	delete vtab;
	delete ftab;
	vtab = (Hash::Hashtab *) NULL;
	ftab = (Hash::Hashtab *) NULL;
    }
    Label::clear();

    ::ninherits = 0;
    privinherit = FALSE;
    nsymbs = 0;
    nfclash = 0;
    nifcalls = 0;
    nundefs = 0;

    if (newctrl != (Control *) NULL) {
	newctrl->del();
	newctrl = (Control *) NULL;
    }
    String::clear();
    schunk.clean();
    fchunk.clean();
    if (functions != (FuncInfo *) NULL) {
	int i;
	FuncInfo *f;

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
	functions = (FuncInfo *) NULL;
    }
    if (variables != (VarDef *) NULL) {
	FREE(variables);
	variables = (VarDef *) NULL;
    }
    if (::cvstrings != (String **) NULL) {
	unsigned short i;
	String **s;

	for (i = nvars, s = ::cvstrings; i != 0; --i, s++) {
	    if (*s != (String *) NULL) {
		(*s)->del();
	    }
	}
	FREE(::cvstrings);
	::cvstrings = (String **) NULL;
    }
    if (::classvars != (char *) NULL) {
	FREE(::classvars);
	::classvars = (char *) NULL;
    }
}

/*
 * create a variable mapping from the old control block to the new
 */
unsigned short *Control::varmap(Control *octrl)
{
    unsigned short j, k;
    VarDef *v;
    long n;
    unsigned short *vmap;
    Inherit *inh, *inh2;
    Control *ctrl, *ctrl2;
    unsigned short i, voffset;

    /*
     * make variable mapping from old to new, with new just compiled
     */

    vmap = ALLOC(unsigned short, nvariables + 1);

    voffset = 0;
    for (i = ninherits, inh = inherits; i > 0; --i, inh++) {
	ctrl = (i == 1) ? this : OBJR(inh->oindex)->ctrl;
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
		v = ctrl2->vars();
		for (k = 0; k < ctrl2->nvardefs; k++, v++) {
		    ctrl2->strconst(v->inherit, v->index)->put(((Uint) k << 8) |
								    v->type);
		}

		/*
		 * map new variables to old ones
		 */
		for (k = 0, v = ctrl->vars(); k < ctrl->nvardefs; k++, v++) {
		    n = ctrl->strconst(v->inherit, v->index)->put(0);
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
		for (k = 0, v = ctrl->vars(); k < ctrl->nvardefs; k++, v++) {
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
    vmap -= nvariables;
    if (octrl->nvariables != nvariables) {
	return vmap;		/* changed */
    }
    for (i = 0; i <= nvariables; i++) {
	if (vmap[i] != i) {
	    return vmap;	/* changed */
	}
    }
    /* no variable remapping needed */
    FREE(vmap);
    return (unsigned short *) NULL;
}

/*
 * add a variable mapping to a control block
 */
void Control::setVarmap(unsigned short *vmap)
{
    vmapsize = nvariables + 1;
    this->vmap = vmap;
}


struct SControl {
    Sector nsectors;		/* # sectors in part one */
    char flags;			/* control flags: compression */
    char version;		/* program version */
    short ninherits;		/* # objects in inherit table */
    uindex imapsz;		/* inherit map size */
    Uint progsize;		/* size of program code */
    Time compiled;		/* time of compilation */
    unsigned short nstrings;	/* # strings in string constant table */
    Uint strsize;		/* size of string constant table */
    char nfuncdefs;		/* # entries in function definition table */
    char nvardefs;		/* # entries in variable definition table */
    char nclassvars;		/* # class variables */
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
    unsigned short vmapsize;	/* size of variable map, or 0 for none */
};

static char sc_layout[] = "dccsuilsicccusss";

struct SControl0 {
    Sector nsectors;		/* # sectors in part one */
    short flags;		/* control flags: compression */
    short ninherits;		/* # objects in inherit table */
    uindex imapsz;		/* inherit map size */
    Uint progsize;		/* size of program code */
    Uint compiled;		/* time of compilation */
    unsigned short comphigh;	/* time of compilation high word */
    unsigned short nstrings;	/* # strings in string constant table */
    Uint strsize;		/* size of string constant table */
    char nfuncdefs;		/* # entries in function definition table */
    char nvardefs;		/* # entries in variable definition table */
    char nclassvars;		/* # class variables */
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
    unsigned short vmapsize;	/* size of variable map, or 0 for none */
};

static char sc0_layout[] = "dssuiissicccusss";

struct SControl1 {
    Sector nsectors;		/* # sectors in part one */
    char flags;			/* control flags: compression */
    char version;		/* program version */
    short ninherits;		/* # objects in inherit table */
    uindex imapsz;		/* inherit map size */
    Uint progsize;		/* size of program code */
    Uint compiled;		/* time of compilation */
    unsigned short comphigh;	/* time of compilation high word */
    unsigned short nstrings;	/* # strings in string constant table */
    Uint strsize;		/* size of string constant table */
    char nfuncdefs;		/* # entries in function definition table */
    char nvardefs;		/* # entries in variable definition table */
    char nclassvars;		/* # class variables */
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
    unsigned short vmapsize;	/* size of variable map, or 0 for none */
};

static char sc1_layout[] = "dccsuiissicccusss";

struct SInherit {
    uindex oindex;		/* index in object table */
    uindex progoffset;		/* program offset */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset */
    unsigned short flags;	/* bit flags */
};

static char si_layout[] = "uuuss";

struct StrConst0 {
    Uint index;			/* index in control block */
    ssizet len;			/* string length */
};

# define DSTR0_LAYOUT	"it"

static bool conv_14;			/* convert arrays & strings? */
static bool conv_15, conv_16;		/* convert control blocks? */
static bool convDone;			/* conversion complete? */

/*
 * load a control block
 */
Control *Control::load(Object *obj, Uint instance,
		       void (*readv) (char*, Sector*, Uint, Uint))
{
    Control *ctrl;
    SControl header;
    Uint size;

    ctrl = new Control();
    ctrl->oindex = obj->index;
    ctrl->instance = instance;

    /* header */
    (*readv)((char *) &header, &obj->cfirst, (Uint) sizeof(SControl), (Uint) 0);
    ctrl->nsectors = header.nsectors;
    ctrl->sectors = ALLOC(Sector, header.nsectors);
    ctrl->sectors[0] = obj->cfirst;
    size = header.nsectors * (Uint) sizeof(Sector);
    if (header.nsectors > 1) {
	(*readv)((char *) ctrl->sectors, ctrl->sectors, size,
		 (Uint) sizeof(SControl));
    }
    size += sizeof(SControl);

    ctrl->flags = header.flags;
    ctrl->version = header.version;

    /* inherits */
    ctrl->ninherits = header.ninherits;

    if (header.vmapsize != 0) {
	/*
	 * Control block for outdated issue; only vmap can be loaded.
	 * The load offsets will be invalid (and unused).
	 */
	ctrl->vmapsize = header.vmapsize;
	ctrl->vmap = ALLOC(unsigned short, header.vmapsize);
	(*readv)((char *) ctrl->vmap, ctrl->sectors,
		 header.vmapsize * (Uint) sizeof(unsigned short), size);
    } else {
	int n;
	Inherit *inherits;
	SInherit *sinherits;

	/* load inherits */
	n = header.ninherits; /* at least one */
	ctrl->inherits = inherits = ALLOC(Inherit, n);
	sinherits = ALLOCA(SInherit, n);
	(*readv)((char *) sinherits, ctrl->sectors, n * (Uint) sizeof(SInherit),
		 size);
	size += n * sizeof(SInherit);
	do {
	    inherits->oindex = sinherits->oindex;
	    inherits->progoffset = sinherits->progoffset;
	    inherits->funcoffset = sinherits->funcoffset;
	    inherits->varoffset = sinherits->varoffset;
	    (inherits++)->priv = (sinherits++)->flags;
	} while (--n > 0);
	AFREE(sinherits - header.ninherits);

	/* load iindices */
	ctrl->imapsz = header.imapsz;
	ctrl->imap = ALLOC(char, header.imapsz);
	(*readv)(ctrl->imap, ctrl->sectors, ctrl->imapsz, size);
	size += ctrl->imapsz;
    }

    /* compile time */
    ctrl->compiled = header.compiled >> 16;

    /* program */
    ctrl->progoffset = size;
    ctrl->progsize = header.progsize;
    size += header.progsize;

    /* string constants */
    ctrl->stroffset = size;
    ctrl->nstrings = header.nstrings;
    ctrl->strsize = header.strsize;
    size += header.nstrings * (Uint) sizeof(ssizet) + header.strsize;

    /* function definitions */
    ctrl->funcdoffset = size;
    ctrl->nfuncdefs = UCHAR(header.nfuncdefs);
    size += UCHAR(header.nfuncdefs) * (Uint) sizeof(FuncDef);

    /* variable definitions */
    ctrl->vardoffset = size;
    ctrl->nvardefs = UCHAR(header.nvardefs);
    ctrl->nclassvars = UCHAR(header.nclassvars);
    size += UCHAR(header.nvardefs) * (Uint) sizeof(VarDef) +
	    UCHAR(header.nclassvars) * (Uint) 3;

    /* function call table */
    ctrl->funccoffset = size;
    ctrl->nfuncalls = header.nfuncalls;
    size += header.nfuncalls * (Uint) 2;

    /* symbol table */
    ctrl->symboffset = size;
    ctrl->nsymbols = header.nsymbols;
    size += header.nsymbols * (Uint) sizeof(Symbol);

    /* # variables */
    ctrl->vtypeoffset = size;
    ctrl->nvariables = header.nvariables;

    return ctrl;
}

/*
 * load a control block from the swap device
 */
Control *Control::load(Object *obj, Uint instance)
{
    return load(obj, instance, Swap::readv);
}

/*
 * convert control block
 */
Control *Control::conv(Object *obj, Uint instance,
		       void (*readv) (char*, Sector*, Uint, Uint))
{
    SControl header;
    Control *ctrl;
    Uint size;
    unsigned int n;

    ctrl = new Control();
    ctrl->oindex = obj->index;
    ctrl->instance = instance;

    /*
     * restore from snapshot
     */
    if (conv_15) {
	SControl0 h0;

	size = Swap::convert((char *) &h0, &obj->cfirst, sc0_layout, (Uint) 1,
			     (Uint) 0, readv);
	header.nsectors = h0.nsectors;
	header.flags = h0.flags;
	header.version = 0;
	header.ninherits = h0.ninherits;
	header.imapsz = h0.imapsz;
	header.progsize = h0.progsize;
	header.compiled = (Time) h0.compiled << 16;
	header.nstrings = h0.nstrings;
	header.strsize = h0.strsize;
	header.nfuncdefs = h0.nfuncdefs;
	header.nvardefs = h0.nvardefs;
	header.nclassvars = h0.nclassvars;
	header.nfuncalls = h0.nfuncalls;
	header.nsymbols = h0.nsymbols;
	header.nvariables = h0.nvariables;
	header.vmapsize = h0.vmapsize;
    } else if (conv_16) {
	SControl1 h1;

	size = Swap::convert((char *) &h1, &obj->cfirst, sc1_layout, (Uint) 1,
			     (Uint) 0, readv);
	header.nsectors = h1.nsectors;
	header.flags = h1.flags;
	header.version = h1.version;
	header.ninherits = h1.ninherits;
	header.imapsz = h1.imapsz;
	header.progsize = h1.progsize;
	header.compiled = (Time) h1.compiled << 16;
	header.nstrings = h1.nstrings;
	header.strsize = h1.strsize;
	header.nfuncdefs = h1.nfuncdefs;
	header.nvardefs = h1.nvardefs;
	header.nclassvars = h1.nclassvars;
	header.nfuncalls = h1.nfuncalls;
	header.nsymbols = h1.nsymbols;
	header.nvariables = h1.nvariables;
	header.vmapsize = h1.vmapsize;
    } else {
	size = Swap::convert((char *) &header, &obj->cfirst, sc_layout,
			     (Uint) 1, (Uint) 0, readv);
    }
    ctrl->flags = header.flags;
    ctrl->version = header.version;
    ctrl->ninherits = header.ninherits;
    ctrl->imapsz = header.imapsz;
    ctrl->compiled = header.compiled >> 16;
    ctrl->progsize = header.progsize;
    ctrl->nstrings = header.nstrings;
    ctrl->strsize = header.strsize;
    ctrl->nfuncdefs = UCHAR(header.nfuncdefs);
    ctrl->nvardefs = UCHAR(header.nvardefs);
    ctrl->nclassvars = UCHAR(header.nclassvars);
    ctrl->nfuncalls = header.nfuncalls;
    ctrl->nsymbols = header.nsymbols;
    ctrl->nvariables = header.nvariables;
    ctrl->vmapsize = header.vmapsize;

    /* sectors */
    ctrl->sectors = ALLOC(Sector, ctrl->nsectors = header.nsectors);
    ctrl->sectors[0] = obj->cfirst;
    for (n = 0; n < header.nsectors; n++) {
	size += Swap::convert((char *) (ctrl->sectors + n), ctrl->sectors, "d",
			      (Uint) 1, size, readv);
    }

    if (header.vmapsize != 0) {
	/* only vmap */
	ctrl->vmap = ALLOC(unsigned short, header.vmapsize);
	Swap::convert((char *) ctrl->vmap, ctrl->sectors, "s",
		      (Uint) header.vmapsize, size, readv);
    } else {
	Inherit *inherits;
	SInherit *sinherits;

	/* inherits */
	n = header.ninherits; /* at least one */
	ctrl->inherits = inherits = ALLOC(Inherit, n);

	sinherits = ALLOCA(SInherit, n);
	size += Swap::convert((char *) sinherits, ctrl->sectors, si_layout,
			      (Uint) n, size, readv);
	do {
	    inherits->oindex = sinherits->oindex;
	    inherits->progoffset = sinherits->progoffset;
	    inherits->funcoffset = sinherits->funcoffset;
	    inherits->varoffset = sinherits->varoffset;
	    (inherits++)->priv = (sinherits++)->flags;
	} while (--n > 0);
	AFREE(sinherits - header.ninherits);

	ctrl->imap = ALLOC(char, header.imapsz);
	(*readv)(ctrl->imap, ctrl->sectors, header.imapsz, size);
	size += header.imapsz;

	if (header.progsize != 0) {
	    /* program */
	    if (header.flags & CMP_TYPE) {
		ctrl->prog = Swap::decompress(ctrl->sectors, readv,
					      header.progsize, size,
					      &ctrl->progsize);
	    } else {
		ctrl->prog = ALLOC(char, header.progsize);
		(*readv)(ctrl->prog, ctrl->sectors, header.progsize, size);
	    }
	    size += header.progsize;
	}

	if (header.nstrings != 0) {
	    /* strings */
	    ctrl->sslength = ALLOC(ssizet, header.nstrings);
	    if (conv_14) {
		StrConst0 *sstrings;
		unsigned short i;

		sstrings = ALLOCA(StrConst0, header.nstrings);
		size += Swap::convert((char *) sstrings, ctrl->sectors,
				      DSTR0_LAYOUT, (Uint) header.nstrings,
				      size, readv);
		for (i = 0; i < header.nstrings; i++) {
		    ctrl->sslength[i] = sstrings[i].len;
		}
		AFREE(sstrings);
	    } else {
		size += Swap::convert((char *) ctrl->sslength, ctrl->sectors,
				      "t", (Uint) header.nstrings, size, readv);
	    }
	    if (header.strsize != 0) {
		if (header.flags & (CMP_TYPE << 2)) {
		    ctrl->stext = Swap::decompress(ctrl->sectors, readv,
						   header.strsize, size,
						   &ctrl->strsize);
		} else {
		    ctrl->stext = ALLOC(char, header.strsize);
		    (*readv)(ctrl->stext, ctrl->sectors, header.strsize, size);
		}
		size += header.strsize;
	    }
	}

	if (header.nfuncdefs != 0) {
	    /* function definitions */
	    ctrl->funcdefs = ALLOC(FuncDef, UCHAR(header.nfuncdefs));
	    size += Swap::convert((char *) ctrl->funcdefs, ctrl->sectors,
				  DF_LAYOUT, (Uint) UCHAR(header.nfuncdefs),
				  size, readv);
	}

	if (header.nvardefs != 0) {
	    /* variable definitions */
	    ctrl->vardefs = ALLOC(VarDef, UCHAR(header.nvardefs));
	    size += Swap::convert((char *) ctrl->vardefs, ctrl->sectors,
				  DV_LAYOUT, (Uint) UCHAR(header.nvardefs),
				  size, readv);
	    if (ctrl->nclassvars != 0) {
		ctrl->classvars = ALLOC(char, ctrl->nclassvars * 3);
		(*readv)(ctrl->classvars, ctrl->sectors,
			 ctrl->nclassvars * (Uint) 3, size);
		size += ctrl->nclassvars * (Uint) 3;
	    }
	}

	if (header.nfuncalls != 0) {
	    /* function calls */
	    ctrl->funcalls = ALLOC(char, 2 * header.nfuncalls);
	    (*readv)(ctrl->funcalls, ctrl->sectors, header.nfuncalls * (Uint) 2,
		     size);
	    size += header.nfuncalls * (Uint) 2;
	}

	if (header.nsymbols != 0) {
	    /* symbol table */
	    ctrl->symbols = ALLOC(Symbol, header.nsymbols);
	    size += Swap::convert((char *) ctrl->symbols, ctrl->sectors,
				  DSYM_LAYOUT, (Uint) header.nsymbols, size,
				  readv);
	}

	if (header.nvariables > UCHAR(header.nvardefs)) {
	    /* variable types */
	    ctrl->vtypes = ALLOC(char, header.nvariables -
				       UCHAR(header.nvardefs));
	    (*readv)(ctrl->vtypes, ctrl->sectors,
		     header.nvariables - UCHAR(header.nvardefs), size);
	}
    }

    return ctrl;
}

/*
 * get the program
 */
void Control::loadProgram(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (progsize != 0) {
	if (flags & CTRL_PROGCMP) {
	    prog = Swap::decompress(sectors, readv, progsize, progoffset,
				    &progsize);
	} else {
	    prog = ALLOC(char, progsize);
	    (*readv)(prog, sectors, progsize, progoffset);
	}
    }
}

/*
 * get the program
 */
char *Control::program()
{
    if (prog == (char *) NULL && progsize != 0) {
	loadProgram(Swap::readv);
    }
    return prog;
}

/*
 * load strings text
 */
void Control::loadStext(void (*readv) (char*, Sector*, Uint, Uint))
{
    /* load strings text */
    if (flags & CTRL_STRCMP) {
	stext = Swap::decompress(sectors, readv, strsize,
				 stroffset + nstrings * sizeof(ssizet),
				 &strsize);
    } else {
	stext = ALLOC(char, strsize);
	(*readv)(stext, sectors, strsize,
		 stroffset + nstrings * (Uint) sizeof(ssizet));
    }
}

/*
 * load string constants
 */
void Control::loadStrconsts(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (nstrings != 0) {
	/* load strings */
	sslength = ALLOC(ssizet, nstrings);
	(*readv)((char *) sslength, sectors, nstrings * (Uint) sizeof(ssizet),
		 stroffset);
	if (strsize > 0 && stext == (char *) NULL) {
	    loadStext(readv);	/* load strings text */
	}
    }
}

/*
 * get a string constant
 */
String *Control::strconst(int inherit, Uint idx)
{
    if (UCHAR(inherit) < ninherits - 1) {
	/* get the proper control block */
	return OBJR(inherits[UCHAR(inherit)].oindex)->control()->strconst(inherit,
									  idx);
    }

    if (strings == (String **) NULL) {
	/* make string pointer block */
	strings = ALLOC(String*, nstrings);
	memset(strings, '\0', nstrings * sizeof(String *));

	if (sslength == (ssizet *) NULL) {
	    loadStrconsts(Swap::readv);
	}
	if (ssindex == (Uint *) NULL) {
	    Uint size;
	    unsigned short i;

	    ssindex = ALLOC(Uint, nstrings);
	    for (size = 0, i = 0; i < nstrings; i++) {
		ssindex[i] = size;
		size += sslength[i];
	    }
	}
    }

    if (strings[idx] == (String *) NULL) {
	String *str;

	str = String::alloc(stext + ssindex[idx], sslength[idx]);
	strings[idx] = str;
	str->ref();
    }

    return strings[idx];
}

/*
 * load function definitions
 */
void Control::loadFuncdefs(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (nfuncdefs != 0) {
	funcdefs = ALLOC(FuncDef, nfuncdefs);
	(*readv)((char *) funcdefs, sectors,
		 nfuncdefs * (Uint) sizeof(FuncDef), funcdoffset);
    }
}

/*
 * get function definitions
 */
FuncDef *Control::funcs()
{
    if (funcdefs == (FuncDef *) NULL && nfuncdefs != 0) {
	loadFuncdefs(Swap::readv);
    }
    return funcdefs;
}

/*
 * load variable definitions
 */
void Control::loadVardefs(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (nvardefs != 0) {
	vardefs = ALLOC(VarDef, nvardefs);
	(*readv)((char *) vardefs, sectors, nvardefs * (Uint) sizeof(VarDef),
		 vardoffset);
	if (nclassvars != 0) {
	    classvars = ALLOC(char, nclassvars * 3);
	    (*readv)(classvars, sectors, nclassvars * 3,
		     vardoffset + nvardefs * sizeof(VarDef));
	}
    }
}

/*
 * get variable definitions
 */
VarDef *Control::vars()
{
    if (vardefs == (VarDef *) NULL && nvardefs != 0) {
	loadVardefs(Swap::readv);
    }
    if (cvstrings == (String **) NULL && nclassvars != 0) {
	char *p;
	VarDef *vars;
	String **strs;
	unsigned short n, inherit, u;

	cvstrings = strs = ALLOC(String*, nvardefs);
	memset(strs, '\0', nvardefs * sizeof(String*));
	p = classvars;
	for (n = nclassvars, vars = vardefs; n != 0; vars++) {
	    if ((vars->type & T_TYPE) == T_CLASS) {
		inherit = FETCH1U(p);
		*strs = strconst(inherit, FETCH2U(p, u));
		(*strs)->ref();
		--n;
	    }
	    strs++;
	}
    }
    return vardefs;
}

/*
 * get function call table
 */
void Control::loadFuncalls(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (nfuncalls != 0) {
	funcalls = ALLOC(char, 2L * nfuncalls);
	(*readv)((char *) funcalls, sectors, nfuncalls * (Uint) 2, funccoffset);
    }
}

/*
 * get function call table
 */
char *Control::funCalls()
{
    if (funcalls == (char *) NULL && nfuncalls != 0) {
	loadFuncalls(Swap::readv);
    }
    return funcalls;
}

/*
 * get symbol table
 */
void Control::loadSymbols(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (nsymbols > 0) {
	symbols = ALLOC(Symbol, nsymbols);
	(*readv)((char *) symbols, sectors, nsymbols * (Uint) sizeof(Symbol),
		 symboffset);
    }
}

/*
 * get symbol table
 */
Symbol *Control::symbs()
{
    if (symbols == (Symbol *) NULL && nsymbols > 0) {
	loadSymbols(Swap::readv);
    }
    return symbols;
}

/*
 * get variable types
 */
void Control::loadVtypes(void (*readv) (char*, Sector*, Uint, Uint))
{
    if (nvariables > nvardefs) {
	vtypes = ALLOC(char, nvariables - nvardefs);
	(*readv)(vtypes, sectors, nvariables - nvardefs, vtypeoffset);
    }
}

/*
 * get variable types
 */
char *Control::varTypes()
{
    if (vtypes == (char *) NULL && nvariables > nvardefs) {
	loadVtypes(Swap::readv);
    }
    return vtypes;
}

/*
 * get the size of a control block
 */
Uint Control::progSize()
{
    if (progsize != 0 && prog == (char *) NULL && (flags & CTRL_PROGCMP)) {
	loadProgram(Swap::readv);	/* decompress program */
    }
    if (strsize != 0 && stext == (char *) NULL && (flags & CTRL_STRCMP)) {
	loadStext(Swap::readv);	/* decompress strings */
    }

    return ninherits * sizeof(Inherit) +
	   imapsz +
	   progsize +
	   nstrings * (Uint) sizeof(ssizet) +
	   strsize +
	   nfuncdefs * sizeof(FuncDef) +
	   nvardefs * sizeof(VarDef) +
	   nclassvars * (Uint) 3 +
	   nfuncalls * (Uint) 2 +
	   nsymbols * (Uint) sizeof(Symbol) +
	   nvariables - nvardefs;
}

/*
 * save the control block
 */
void Control::save()
{
    SControl header;
    char *prog, *stext, *text;
    ssizet *sslength;
    Uint size, i;
    SInherit *sinherits;
    Inherit *inherits;

    sslength = NULL;
    prog = stext = text = NULL;

    /*
     * Save a control block.
     */

    /* create header */
    header.flags = flags & CTRL_UNDEFINED;
    header.version = version;
    header.ninherits = ninherits;
    header.imapsz = imapsz;
    header.compiled = (Time) compiled << 16;
    header.progsize = progsize;
    header.nstrings = nstrings;
    header.strsize = strsize;
    header.nfuncdefs = nfuncdefs;
    header.nvardefs = nvardefs;
    header.nclassvars = nclassvars;
    header.nfuncalls = nfuncalls;
    header.nsymbols = nsymbols;
    header.nvariables = nvariables;
    header.vmapsize = vmapsize;

    /* create sector space */
    if (header.vmapsize != 0) {
	size = sizeof(SControl) +
	       header.vmapsize * (Uint) sizeof(unsigned short);
    } else {
	prog = this->prog;
	if (header.progsize >= CMPLIMIT) {
	    prog = ALLOC(char, header.progsize);
	    size = Swap::compress(prog, this->prog, header.progsize);
	    if (size != 0) {
		header.flags |= CMP_PRED;
		header.progsize = size;
	    } else {
		FREE(prog);
		prog = this->prog;
	    }
	}

	sslength = this->sslength;
	stext = this->stext;
	if (header.nstrings > 0 && sslength == (ssizet *) NULL) {
	    String **strs;
	    ssizet *l;
	    char *t;

	    sslength = ALLOC(ssizet, header.nstrings);
	    if (header.strsize > 0) {
		stext = ALLOC(char, header.strsize);
	    }

	    strs = strings;
	    l = sslength;
	    t = stext;
	    for (i = header.nstrings; i > 0; --i) {
		*l = (*strs)->len;
		memcpy(t, (*strs++)->text, *l);
		t += *l++;
	    }
	}

	text = stext;
	if (header.strsize >= CMPLIMIT) {
	    text = ALLOC(char, header.strsize);
	    size = Swap::compress(text, stext, header.strsize);
	    if (size != 0) {
		header.flags |= CMP_PRED << 2;
		header.strsize = size;
	    } else {
		FREE(text);
		text = stext;
	    }
	}

	size = sizeof(SControl) +
	       header.ninherits * sizeof(SInherit) +
	       header.imapsz +
	       header.progsize +
	       header.nstrings * (Uint) sizeof(ssizet) +
	       header.strsize +
	       UCHAR(header.nfuncdefs) * sizeof(FuncDef) +
	       UCHAR(header.nvardefs) * sizeof(VarDef) +
	       UCHAR(header.nclassvars) * (Uint) 3 +
	       header.nfuncalls * (Uint) 2 +
	       header.nsymbols * (Uint) sizeof(Symbol) +
	       header.nvariables - UCHAR(header.nvardefs);
    }
    nsectors = header.nsectors = Swap::alloc(size, nsectors, &sectors);
    OBJ(oindex)->cfirst = sectors[0];

    /*
     * Copy everything to the swap device.
     */

    /* save header */
    Swap::writev((char *) &header, sectors, (Uint) sizeof(SControl), (Uint) 0);
    size = sizeof(SControl);

    /* save sector map */
    Swap::writev((char *) sectors, sectors,
		 header.nsectors * (Uint) sizeof(Sector), size);
    size += header.nsectors * (Uint) sizeof(Sector);

    if (header.vmapsize != 0) {
	/*
	 * save only vmap
	 */
	Swap::writev((char *) vmap, sectors,
		  header.vmapsize * (Uint) sizeof(unsigned short), size);
    } else {
	/* save inherits */
	inherits = this->inherits;
	sinherits = ALLOCA(SInherit, i = header.ninherits);
	do {
	    sinherits->oindex = inherits->oindex;
	    sinherits->progoffset = inherits->progoffset;
	    sinherits->funcoffset = inherits->funcoffset;
	    sinherits->varoffset = inherits->varoffset;
	    sinherits->flags = inherits->priv;
	    inherits++;
	    sinherits++;
	} while (--i > 0);
	sinherits -= header.ninherits;
	Swap::writev((char *) sinherits, sectors,
		     header.ninherits * (Uint) sizeof(SInherit), size);
	size += header.ninherits * sizeof(SInherit);
	AFREE(sinherits);

	/* save iindices */
	Swap::writev(imap, sectors, imapsz, size);
	size += imapsz;

	/* save program */
	if (header.progsize > 0) {
	    Swap::writev(prog, sectors, (Uint) header.progsize, size);
	    size += header.progsize;
	    if (prog != this->prog) {
		FREE(prog);
	    }
	}

	/* save string constants */
	if (header.nstrings > 0) {
	    Swap::writev((char *) sslength, sectors,
			 header.nstrings * (Uint) sizeof(ssizet), size);
	    size += header.nstrings * (Uint) sizeof(ssizet);
	    if (header.strsize > 0) {
		Swap::writev(text, sectors, header.strsize, size);
		size += header.strsize;
		if (text != stext) {
		    FREE(text);
		}
		if (stext != this->stext) {
		    FREE(stext);
		}
	    }
	    if (sslength != this->sslength) {
		FREE(sslength);
	    }
	}

	/* save function definitions */
	if (UCHAR(header.nfuncdefs) > 0) {
	    Swap::writev((char *) funcdefs, sectors,
			 UCHAR(header.nfuncdefs) * (Uint) sizeof(FuncDef),
			 size);
	    size += UCHAR(header.nfuncdefs) * (Uint) sizeof(FuncDef);
	}

	/* save variable definitions */
	if (UCHAR(header.nvardefs) > 0) {
	    Swap::writev((char *) vardefs, sectors,
			 UCHAR(header.nvardefs) * (Uint) sizeof(VarDef), size);
	    size += UCHAR(header.nvardefs) * (Uint) sizeof(VarDef);
	    if (UCHAR(header.nclassvars) > 0) {
		Swap::writev(classvars, sectors,
			     UCHAR(header.nclassvars) * (Uint) 3, size);
		size += UCHAR(header.nclassvars) * (Uint) 3;
	    }
	}

	/* save function call table */
	if (header.nfuncalls > 0) {
	    Swap::writev((char *) funcalls, sectors,
			 header.nfuncalls * (Uint) 2, size);
	    size += header.nfuncalls * (Uint) 2;
	}

	/* save symbol table */
	if (header.nsymbols > 0) {
	    Swap::writev((char *) symbols, sectors,
			 header.nsymbols * (Uint) sizeof(Symbol), size);
	    size += header.nsymbols * sizeof(Symbol);
	}

	/* save variable types */
	if (header.nvariables > UCHAR(header.nvardefs)) {
	    Swap::writev(vtypes, sectors,
			 header.nvariables - UCHAR(header.nvardefs), size);
	}
    }
}

/*
 * restore a control block
 */
Control *Control::restore(Object *obj, Uint instance,
			  void (*readv) (char*, Sector*, Uint, Uint))
{
    Control *ctrl;

    ctrl = (Control *) NULL;
    if (obj->cfirst != SW_UNUSED) {
	if (!convDone) {
	    ctrl = conv(obj, instance, readv);
	} else {
	    ctrl = load(obj, instance, readv);
	    if (ctrl->vmapsize == 0) {
		ctrl->loadProgram(readv);
		ctrl->loadStrconsts(readv);
		ctrl->loadFuncdefs(readv);
		ctrl->loadVardefs(readv);
		ctrl->loadFuncalls(readv);
		ctrl->loadSymbols(readv);
		ctrl->loadVtypes(readv);
	    }
	}
	ctrl->save();
	obj->ctrl = ctrl;
    }

    return ctrl;
}

/*
 * return the entry in the symbol table for func, or NULL
 */
Symbol *Control::symb(const char *func, unsigned int len)
{
    Symbol *symb;
    Control *ctrl;
    FuncDef *f;
    unsigned int i, j;
    String *str;
    Symbol *symb1;

    if ((i=nsymbols) == 0) {
	return (Symbol *) NULL;
    }

    i = HM->hashstr(func, SYMBHASH) % i;
    symb1 = symb = &symbs()[i];
    ctrl = OBJR(inherits[UCHAR(symb->inherit)].oindex)->control();
    f = ctrl->funcs() + UCHAR(symb->index);
    str = ctrl->strconst(f->inherit, f->index);
    if (len == str->len && memcmp(func, str->text, len) == 0) {
	/* found it */
	return (f->sclass & C_UNDEFINED) ? (Symbol *) NULL : symb1;
    }
    while (symb->next != i && symb->next != (unsigned short) -1) {
	symb = &symbols[i = symb->next];
	ctrl = OBJR(inherits[UCHAR(symb->inherit)].oindex)->control();
	f = ctrl->funcs() + UCHAR(symb->index);
	str = ctrl->strconst(f->inherit, f->index);
	if (len == str->len && memcmp(func, str->text, len) == 0) {
	    /* found it: put symbol first in linked list */
	    i = symb1->inherit;
	    j = symb1->index;
	    symb1->inherit = symb->inherit;
	    symb1->index = symb->index;
	    symb->inherit = i;
	    symb->index = j;
	    return (f->sclass & C_UNDEFINED) ? (Symbol *) NULL : symb1;
	}
    }
    return (Symbol *) NULL;
}

/*
 * list the undefined functions in a program
 */
Array *Control::undefined(Dataspace *data)
{
    struct ulist {
	short count;		/* number of undefined functions */
	short index;		/* index in inherits list */
    } *u, *list;
    Control *ctrl;
    short i;
    Symbol *symb;
    FuncDef *f;
    Value *v;
    Object *obj;
    long size;
    Mapping *m;

    list = ALLOCA(ulist, ninherits);
    memset(list, '\0', ninherits * sizeof(ulist));
    size = 0;

    /*
     * count the number of undefined functions per program
     */
    for (i = nsymbols, symb = symbs(); i != 0; --i, symb++) {
	obj = OBJR(inherits[UCHAR(symb->inherit)].oindex);
	ctrl = (O_UPGRADING(obj)) ? OBJR(obj->prev)->ctrl : obj->control();
	if ((ctrl->funcs()[UCHAR(symb->index)].sclass & C_UNDEFINED) &&
	    list[UCHAR(symb->inherit)].count++ == 0) {
	    list[UCHAR(symb->inherit)].index = size;
	    size += 2;
	}
    }

    m = (Mapping *) NULL;
    try {
	EC->push();
	m = Mapping::create(data, size);
	memset(m->elts, '\0', size * sizeof(Value));
	for (i = nsymbols, symb = symbols; i != 0; --i, symb++) {
	    obj = OBJR(inherits[UCHAR(symb->inherit)].oindex);
	    ctrl = (O_UPGRADING(obj)) ? OBJR(obj->prev)->ctrl : obj->control();
	    f = ctrl->funcs() + UCHAR(symb->index);
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
		PUT_STRVAL(v, ctrl->strconst(f->inherit, f->index));
	    }
	}
	EC->pop();
    } catch (const char*) {
	if (m != (Mapping *) NULL) {
	    /* discard mapping */
	    m->ref();
	    m->del();
	}
	AFREE(list);
	EC->error((char *) NULL);	/* pass on error */
    }
    AFREE(list);

    m->sort();
    return m;
}

/*
 * initialize swapped data handling
 */
void Control::init()
{
    chead = ctail = (Control *) NULL;
    nctrl = 0;
    conv_14 = conv_15 = conv_16 = FALSE;
    convDone = FALSE;
}

/*
 * prepare for conversions
 */
void Control::initConv(bool c14, bool c15, bool c16)
{
    conv_14 = c14;
    conv_15 = c15;
    conv_16 = c16;
}

/*
 * snapshot conversion is complete
 */
void Control::converted()
{
    convDone = TRUE;
}

/*
 * Swap out a portion of the control and dataspace blocks in
 * memory.  Return the number of dataspace blocks swapped out.
 */
void Control::swapout(unsigned int frag)
{
    Sector n;
    Control *ctrl;

    /* swap out control blocks */
    ctrl = ctail;
    for (n = nctrl / frag; n > 0; --n) {
	Control *prev;

	prev = ctrl->prev;
	if (ctrl->ndata == 0) {
	    if (ctrl->sectors == (Sector *) NULL || (ctrl->flags & CTRL_VARMAP))
	    {
		ctrl->save();
	    }
	    OBJ(ctrl->oindex)->ctrl = (Control *) NULL;
	    delete ctrl;
	}
	ctrl = prev;
    }
}
