# include "comp.h"
# include "hash.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "kfun.h"
# include "fcontrol.h"
# include "control.h"

typedef struct _oh_ {		/* object hash table */
    hte chain;			/* hash table chain */
    object *obj;		/* object */
    short index;		/* -1: direct; 0: new; 1: indirect */
    uindex foffset;		/* function call offset */
    uindex voffset;		/* variable offset */
    struct _oh_ **next;		/* next in linked list */
} oh;

static hashtab *otab;		/* object hash table */
static oh **olist;		/* list of all object hash table entries */

/*
 * NAME:	oh->init()
 * DESCRIPTION:	initialize the object hash table
 */
static oh_init()
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
	*h = (oh *) NULL;
	h = f->next;
	FREE(f);
    }
    olist = (oh **) NULL;
}


# define VFH_CHUNK	64

typedef struct _vfh_ {		/* variable/function hash table */
    hte chain;			/* hash table chain */
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
static void vfh_new(name, ohash, ct, index, addr)
char *name;
oh *ohash;
unsigned short ct;
short index;
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
    h->chain.name = name;
    h->ohash = ohash;
    h->ct = ct;
    h->index = index;
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
    object *obj;		/* labeled object */
    short index;		/* index of object */
    struct _lab_ *next;		/* next label */
} lab;

static lab *labels;		/* list of labeled inherited objects */

/*
 * NAME:	lab->new()
 * DESCRIPTION:	declare a new inheritance label
 */
static void lab_new(str, obj)
string *str;
object *obj;
{
    register lab *l;

    l = ALLOC(lab, 1);
    str_ref(l->str = str);
    l->obj = obj;
    l->next = labels;
    labels = l;
}

/*
 * NAME:	lab->find()
 * DESCRIPTION:	find a labeled object in the list
 */
static lab *lab_find(name)
char *name;
{
    register lab *l;

    for (l = labels; l != (lab *) NULL; l = l->next) {
	if (strcmp(l->str->text, name) == 0) {
	    return l;
	}
    }
    return (lab *) NULL;
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
 *		Strict checking requires the number of arguments to be
 *		the same; otherwise the number of arguments in prot2 is
 *		allowed to be larger.
 */
static bool cmp_proto(prot1, prot2, strict)
register char *prot1, *prot2;
bool strict;
{
    register int i;

    /* check if either prototype is implicit */
    if ((!(PROTO_NARGS(prot1) & C_TYPECHECKED) &&
	 (PROTO_FTYPE(prot1) & (1 << REFSHIFT))) ||
	(!(PROTO_NARGS(prot2) & C_TYPECHECKED) &&
	 (PROTO_FTYPE(prot2) & (1 << REFSHIFT)))) {
	return TRUE;
    }

    /* check if classes are equal */
    i = UCHAR(~(C_TYPECHECKED | C_COMPILED | C_LOCAL | C_UNDEFINED));
    if (!strict) {
	i &= ~C_VARARGS;
    }
    if ((PROTO_CLASS(prot1) ^ PROTO_CLASS(prot2)) & i) {
	return FALSE;
    }

    /* check if the number of arguments is equal */
    if ((i=PROTO_NARGS(prot2)) != PROTO_NARGS(prot1)) {
	if (strict && i > PROTO_NARGS(prot1)) {
	    return FALSE;
	}
    }

    /* check if functions are both typechecked */
    if (!(PROTO_CLASS(prot1) & PROTO_CLASS(prot2) & C_TYPECHECKED)) {
	return TRUE;
    }

    /* compare return type */
    if (PROTO_FTYPE(prot1) != PROTO_FTYPE(prot2)) {
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
static int nvirtuals;			/* # virtually inh. objects */
static char *auto_file;			/* name of auto object */
static hashtab *vtab;			/* variable merge table */
static hashtab *ftab;			/* function merge table */
static unsigned short nvars;		/* # variables */
static unsigned short nsymbs;		/* # symbols */
static uindex nfcalls;			/* # function calls */

/*
 * NAME:	control->init()
 * DESCRIPTION:	initialize control block construction
 */
void ctrl_init(auto_obj)
char *auto_obj;
{
    auto_file = auto_obj;
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
    register char *name;
    register vfh **h;

    v = d_get_vardefs(ctrl);
    for (n = 0; n < ctrl->nvardefs; n++) {
	/*
	 * Add only non-private variables, and check if a variable with the
	 * same name hasn't been inherited already.
	 */
	if (!(v->class & C_PRIVATE)) {
	    name = d_get_strconst(ctrl, v->inherit, v->index)->text;
	    h = (vfh **) ht_lookup(vtab, name);
	    if (*h == (vfh *) NULL) {
		/* new variable */
		vfh_new(name, ohash, v->type, n, h);
	    } else {
		/* duplicate variable */
		yyerror("multiple inheritance of variable %s", name);
	    }
	}
	v++;
    }
    nvars += ctrl->nvariables - ctrl->inherits[ctrl->nvirtuals - 1].varoffset;
}

/*
 * NAME:	control->funcdef()
 * DESCRIPTION:	put a function definition from an inherited object into
 *		the function merge table
 */
static void ctrl_funcdef(ctrl, index, ohash)
register control *ctrl;
register int index;
oh *ohash;
{
    register vfh **h;
    register dfuncdef *f;
    object *o;
    char *name;

    f = &ctrl->funcdefs[index];
    name = d_get_strconst(ctrl, f->inherit, f->index)->text;
    h = (vfh **) ht_lookup(ftab, name);
    if (*h == (vfh *) NULL) {
	/* new function (-1: no calls to it yet) */
	vfh_new(name, ohash, -1, index, h);
	if (!(f->class & C_STATIC) || ctrl->nvirtuals != 1) {
	    /* don't count static functions from the auto object */
	    nsymbs++;
	}
    } else if ((*h)->ohash != ohash) {
	/*
	 * Duplicate function.
	 * First check if the function in the hash table is in
	 * an object inherited by the current object.
	 */
	register dinherit *inh;
	register int n;

	o = (*h)->ohash->obj;
	inh = ctrl->inherits;
	n = ctrl->nvirtuals;
	ctrl = o->ctrl;
	for (;;) {
	    if (--n == 0) {
		char *prot1, *prot2;

		/*
		 * Functions defined in different objects.
		 * At least one function must be undefined, and
		 * the prototypes must be equal.
		 */
		prot1 = ctrl->prog + f->offset;
		prot2 = ctrl->prog + ctrl->funcdefs[(*h)->index].offset;
		if (!((f->class | PROTO_CLASS(prot2)) & C_UNDEFINED) ||
		    !cmp_proto(prot1, prot2, TRUE)) {
		    /*
		     * prototype conflict
		     */
		    yyerror("multiple inheritance of function %s", name);
		} else if (!(f->class & C_UNDEFINED)) {
		    /*
		     * replace undefined function
		     */
		    (*h)->ohash = ohash;
		    (*h)->index = index;
		}
		break;
	    }
	    if (o == inh->obj) {
		/*
		 * redefined inherited function
		 */
		if ((ctrl->funcdefs[(*h)->index].class & C_STATIC) &&
		    ctrl->nvirtuals == 1) {
		    /*
		     * redefine static function in auto object
		     */
		    nsymbs++;
		}
		(*h)->ohash = ohash;
		(*h)->index = index;
		break;
	    }
	    inh++;
	}
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

    if (ctrl->nvirtuals == 1) {
	register dfuncdef *f;
	register oh *ohash;

	/*
	 * For the auto object, add non-private functions from the function
	 * definition table.
	 */
	f = ctrl->funcdefs;
	ohash = oh_new(ctrl->inherits[ctrl->nvirtuals - 1].obj->chain.name);
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
	symb = d_get_symbols(ctrl);
	for (n = ctrl->nsymbols; n > 0; --n) {
	    o = ctrl->inherits[UCHAR(symb->inherit)].obj;
	    ctrl_funcdef(o->ctrl, UCHAR(symb->index), oh_new(o->chain.name));
	    symb++;
	}
    }
}

/*
 * NAME:	control->inherit()
 * DESCRIPTION:	inherit an object
 */
void ctrl_inherit(o, label)
register object *o;
string *label;
{
    register control *ctrl;

    if (label == (string *) NULL) {
	register oh *ohash;

	/*
	 * virtual inheritance
	 */
	ohash = oh_new(o->chain.name);
	if (ohash->index == 0) {
	    register int i, n;
	    dinherit *inh;

	    /*
	     * new inherited object
	     */
	    ctrl = o_control(o);
	    for (i = ctrl->nvirtuals, inh = ctrl->inherits; i > 0; --i) {
		/*
		 * check all the objects inherited by the object now inherited
		 */
		o = (inh++)->obj;

		ohash = oh_new(o->chain.name);
		if (ohash->index == 0) {
		    /*
		     * inherit a new object
		     */
		    ohash->obj = o;
		    ohash->index = 1;
		    ohash->foffset = nfcalls;
		    ohash->voffset = nvars;
		    ctrl = o_control(o);
		    nfcalls += ctrl->nfuncalls -
			       ctrl->inherits[ctrl->nvirtuals - 1].funcoffset;

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
		    yyerror("inherited different instances of \"/%s\"",
			    o->chain.name);
		    return;
		} else if (ohash->index < 0 && ohash->obj->ctrl->nvirtuals > 1)
		{
		    /*
		     * Inherit an object which previously was inherited
		     * directly (but is not the auto object). Mark it as
		     * indirect now.
		     */
		    ohash->index = 1;
		    n = ohash->obj->ctrl->nvirtuals - 1;
		    ninherits -= n;
		    nvirtuals -= n;
		}
	    }

	    n = ctrl->nvirtuals;
	    if (n > 1) {
		/*
		 * Don't count the auto object, unless it is the auto object
		 * only.
		 */
		--n;
	    }
	    ninherits += n;
	    nvirtuals += n;
	    directs[ndirects++] = ohash;
	    ohash->index = -1;	/* direct */

	    /*
	     * put functions in function merge table
	     */
	    ctrl_funcdefs(ctrl);

	} else if (ohash->obj != o) {
	    /*
	     * inherited two objects with same name
	     */
	    yyerror("inherited different instances of \"/%s\"", o->chain.name);
	}
    } else {
	/*
	 * labeled inheritance
	 */
	if (lab_find(label->text) != (lab *) NULL) {
	    yyerror("redeclaration of label %s", label->text);
	}
	ctrl = o_control(o);
	if (ctrl->inherits[0].obj != oh_new(auto_file)->obj) {
	    yyerror("inherited different instances of \"/%s\"", auto_file);
	}
	ninherits++;
	lab_new(label, o);
    }

    if (ninherits >= MAX_INHERITS) {
	yyerror("too many objects inherited");
    }
}


# define STRING_CHUNK	64

typedef struct _strchunk_ {
    string *s[STRING_CHUNK];		/* chunk of strings */
    struct _strchunk_ *next;		/* next in string chunk list */
} strchunk;

# define FCALL_CHUNK	32

typedef struct _fcchunk_ {
    struct {
	char inherit;			/* program index */
	char index;			/* function index */
    } f[FCALL_CHUNK];
    struct _fcchunk_ *next;		/* next in fcall chunk list */
} fcchunk;

typedef struct _cfunc_ {
    dfuncdef func;			/* function name/type */
    char *proto;			/* function prototype */
    char *prog;				/* fuction program */
    unsigned short progsize;		/* fuction program size */
} cfunc;

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
void ctrl_create()
{
    register dinherit *new;
    register control *ctrl;
    register unsigned short n;
    register int i, count;
    lab *l;

    /*
     * create a new control block
     */
    newctrl = d_new_control();
    newctrl->nvirtuals = nvirtuals + 1;
    newctrl->ninherits = ++ninherits;
    new = newctrl->inherits = ALLOC(dinherit, ninherits);
    count = nvirtuals;
    new += count;

    if (nvirtuals > 0) {
	/*
	 * initialize the virtually inherited objects
	 */
	for (n = ndirects; n > 0; ) {
	    register oh *ohash;
	    register dinherit *old;

	    ohash = directs[--n];
	    if (ohash->index < 0) {		/* directly inherited */
		ctrl = ohash->obj->ctrl;
		i = ctrl->nvirtuals - 1;
		old = ctrl->inherits + i;
		/*
		 * do this ctrl->nvirtuals - 1 times, but at least once
		 */
		do {
		    ohash = oh_new(old->obj->chain.name);
		    --old;
		    (--new)->obj = ohash->obj;
		    new->funcoffset = ohash->foffset;
		    new->varoffset = ohash->voffset;
		    ohash->index = --count;	/* may happen more than once */
		} while (--i > 0);
	    }
	}

	/*
	 * Collect all string constants from inherited objects and put them in
	 * the string merge table.
	 */
	for (count = 0; count < nvirtuals; count++) {
	    if (oh_new(new->obj->chain.name)->index == count) {
		ctrl = new->obj->ctrl;
		i = ctrl->nvirtuals - 1;
		for (n = ctrl->nstrings; n > 0; ) {
		    --n;
		    str_put(d_get_strconst(ctrl, i, n)->text,
			    (count << 16L) | n);
		}
	    }
	    new++;
	}
    }

    /*
     * stats for new object
     */
    new->obj = (object *) NULL;
    new->funcoffset = nfcalls;
    new->varoffset = nvars;

    /*
     * initialize the labeled inherited objects
     */
    n = newctrl->inherits[1].varoffset;	/* # variables in auto */
    i = 0;
    for (l = labels; l != (lab *) NULL; l = l->next) {
	(++new)->obj = l->obj;
	new->funcoffset = 0;	/* not used */
	new->varoffset = i;
	i += new->obj->ctrl->nvariables - n;
	l->index = ++count;
    }

    newctrl->nvariables = nvars + i;

    /*
     * prepare for construction of a new control block
     */
    newohash = oh_new("/");	/* legal object couldn't have this name */
    newohash->index = nvirtuals;
    functions = ALLOC(cfunc, 256);
    variables = ALLOC(dvardef, 256);
    progsize = 0;
    nstrs = 0;
    nfdefs = 0;
    nfcalls = 0;
    nvars = 0;
}

/*
 * NAME:	control->dstring()
 * DESCRIPTION:	define a new (?) string constant
 */
long ctrl_dstring(str)
string *str;
{
    register long desc, new;

    desc = str_put(str->text, new = (nvirtuals << 16L) | nstrs);
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
    if (desc >> 16 == nvirtuals) {
	desc |= 0x01000000L;	/* mark it as new */
    }
    return desc;
}

/*
 * NAME:	control->dproto()
 * DESCRIPTION:	define a new function prototype
 */
void ctrl_dproto(str, proto)
string *str;
char *proto;
{
    register vfh **h;
    register int i;
    register dfuncdef *func;
    long s;

    /* first check if prototype exists already */
    h = (vfh **) ht_lookup(ftab, str->text);
    if (*h != (vfh *) NULL) {
	char *proto2;
	control *ctrl;

	/*
	 * redefinition
	 */
	if ((*h)->ohash == newohash) {
	    /*
	     * redefinition of new function
	     */
	    proto2 = functions[(*h)->index].proto;
	    if (!((PROTO_CLASS(proto) | PROTO_CLASS(proto2)) & C_UNDEFINED) ||
		!cmp_proto(proto2, proto, TRUE)) {
		/*
		 * either both prototypes are from functions, or they are not
		 * equal
		 */
		yyerror("multiple declaration of function %s", str->text);
	    } else if (!(PROTO_CLASS(proto) & C_UNDEFINED)) {
		/*
		 * replace empty prototype by function (assume this function is
		 * called from ctrl_dfunc)
		 */
		FREE(proto2);
		i = PROTO_SIZE(proto);
		functions[fdef = (*h)->index].proto =
				    (char *) memcpy(ALLOC(char, i), proto, i);
		functions[fdef].func.class = PROTO_CLASS(proto);
	    }
	    return;
	}

	/*
	 * redefinition of inherited function
	 */
	ctrl = (*h)->ohash->obj->ctrl;
	proto2 = ctrl->prog + ctrl->funcdefs[(*h)->index].offset;
	if (PROTO_CLASS(proto2) & C_NOMASK) {
	    /*
	     * attempt to redefine nomask function
	     */
	    yyerror("redeclaration of nomask function %s", str->text);
	} else if (!(PROTO_CLASS(proto2) & C_LOCAL) &&
		   !(PROTO_CLASS(proto) & C_PRIVATE) &&
		   !cmp_proto(proto2, proto, FALSE)) {
	    /*
	     * Calls from the inherited object to the old function will
	     * be redirected to the new one; it is allowed to add extra
	     * parameters to the function, but the other ones must match.
	     */
	    yyerror("invalid redeclaration of function %s", str->text);
	}

	if (!(PROTO_CLASS(proto) & C_PRIVATE) &&
	    (PROTO_CLASS(proto2) & C_STATIC) && ctrl->nvirtuals == 1) {
	    /*
	     * replace static function in auto object by non-private function
	     */
	    nsymbs++;
	}
	/*
	 * insert the definition before the old one in the hash table
	 */
    } else if (!(PROTO_CLASS(proto) & C_PRIVATE) &&
	       (!(PROTO_CLASS(proto) & C_STATIC) || newctrl->nvirtuals != 1)) {
	/*
	 * add new prototype to symbol table
	 */
	nsymbs++;
    }

    if (nfdefs == 256) {
	yyerror("too many functions declared");
	return;
    }

    /*
     * Handle actual definition.
     */
    vfh_new(strcpy(ALLOC(char, strlen(str->text) + 1), str->text),
	    newohash, -1, nfdefs, h);
    if (newctrl->nvirtuals == 1 && (PROTO_CLASS(proto) & C_STATIC)) {
	/* static function in auto object */
	PROTO_CLASS(proto) |= C_LOCAL;
    }
    i = PROTO_SIZE(proto);
    functions[nfdefs].proto = (char *) memcpy(ALLOC(char, i), proto, i);
    functions[nfdefs].progsize = 0;
    progsize += i;
    func = &functions[nfdefs++].func;
    func->class = PROTO_CLASS(proto);
    s = ctrl_dstring(str);
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
    if (*h != (vfh *) NULL && !(class & C_PRIVATE)) {
	/*
	 * non-private redeclaration of a variable
	 */
	yyerror("redeclaration of variable %s", str->text);
	return;
    }
    if (nvars == 256) {
	yyerror("too many variables declared");
	return;
    }

    /* actually define the variable */
    vfh_new(str->text, newohash, type, nvars, h);
    var = &variables[nvars++];
    var->class = class;
    var->type = type;
    s = ctrl_dstring(str);
    var->inherit = s >> 16;
    var->index = s;
}

/*
 * NAME:	control->lfcall()
 * DESCRIPTION:	call a labelled function
 */
char *ctrl_lfcall(str, label, call)
string *str;
char *label;
long *call;
{
    register lab *l;
    register dsymbol *symb;
    register control *ctrl;

    /* first check if the function exists */
    l = lab_find(label);
    if (l == (lab *) NULL) {
	yyerror("undefined label %s", label);
	return (char *) NULL;
    }
    symb = ctrl_symb(ctrl = l->obj->ctrl, str->text);
    if (symb == (dsymbol *) NULL) {
	register short kf;

	/*
	 * It may seem strange to allow label::kfun, but remember that they
	 * are supposed to be inherited in the auto object.
	 */
	kf = kf_func(str->text);
	if (kf >= 0) {
	    /* kfun call */
	    *call = (KFCALL << 24L) | kf;
	    return kftab[kf].proto;
	}
	yyerror("undefined function %s::%s", label, str->text);
	return (char *) NULL;
    }

    *call = (LFCALL << 24L) | (l->index << 16L) | (UCHAR(symb->inherit) << 8L) |
	    UCHAR(symb->index);
    ctrl = ctrl->inherits[UCHAR(symb->inherit)].obj->ctrl;
    return d_get_prog(ctrl) + ctrl->funcdefs[UCHAR(symb->index)].offset;
}

/*
 * NAME:	control->ifcall()
 * DESCRIPTION:	call an inherited function
 */
char *ctrl_ifcall(str, call)
string *str;
long *call;
{
    register vfh *h;
    register control *ctrl;

    /* check if the function exists */
    h = *(vfh **) ht_lookup(ftab, str->text);
    if (h == (vfh *) NULL || (h->ohash == newohash &&
	((h=(vfh *) h->chain.next) == (vfh *) NULL ||
	 strcmp(h->chain.name, str->text) != 0))) {
	register short kf;

	kf = kf_func(str->text);
	if (kf >= 0) {
	    /* kfun call */
	    *call = (KFCALL << 24L) | kf;
	    return kftab[kf].proto;
	}
	yyerror("undefined function ::%s", str->text);
	return (char *) NULL;
    }

    ctrl = h->ohash->obj->ctrl;
    if (ctrl->funcdefs[h->index].class & C_UNDEFINED) {
	yyerror("undefined function ::%s", str->text);
	return (char *) NULL;
    }
    *call = (DFCALL << 24L) | (h->ohash->index << 8L) | h->index;
    return ctrl->prog + ctrl->funcdefs[h->index].offset;
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
	static char uproto[] = { C_UNDEFINED, T_MIXED | (1 << REFSHIFT), 0 };
	register short kf;

	/*
	 * undefined function
	 */
	kf = kf_func(str->text);
	if (kf >= 0) {
	    /* kfun call */
	    *call = (KFCALL << 24L) | kf;
	    return kftab[kf].proto;
	}
	if (typechecking) {
	    /* there should have been a prototype */
	    yyerror("undefined function %s", str->text);
	    return (char *) NULL;
	}

	/* created an undefined prototype for the function */
	if (nfcalls == 256) {
	    yyerror("too many undefined functions");
	    return (char *) NULL;
	}
	memcpy(proto = ALLOC(char, sizeof(uproto)), uproto, sizeof(uproto));
	ctrl_dproto(str, proto);
	h = *(vfh **) ht_lookup(ftab, str->text);
    } else if (h->ohash == newohash) {
	/*
	 * call to new function
	 */
	proto = functions[h->index].proto;
    } else {
	register control *ctrl;

	/*
	 * call to inherited function
	 */
	ctrl = h->ohash->obj->ctrl;
	proto = ctrl->prog + ctrl->funcdefs[h->index].offset;
    }

    if (PROTO_CLASS(proto) & C_LOCAL) {
	/* direct call */
	*call = (DFCALL << 24L) | (h->ohash->index << 8L) | h->index;
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
	    fclist->f[fcchunksz].inherit = h->ohash->index;
	    fclist->f[fcchunksz++].index = h->index;
	    h->ct = nfcalls++;
	}
	*call = (FCALL << 24L) | h->ct;
    }
    return proto;
}

/*
 * NAME:	control->var()
 * DESCRIPTION:	handle a variable reference
 */
short ctrl_var(str, ref)
string *str;
long *ref;
{
    register vfh *h;

    /* check if the variable exists */
    h = *(vfh **) ht_lookup(vtab, str->text);
    if (h == (vfh *) NULL) {
	yyerror("undefined variable %s", str->text);
	if (nvars < 256) {
	    /* don't repeat this error */
	    ctrl_dvar(str, 0, T_MIXED);
	}
	return T_MIXED;
    }

    *ref = (h->ohash->index << 8L) | h->index;
    return h->ct;	/* the variable type */
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

    if ((newctrl->nstrings = nstrs) != 0) {
	newctrl->strings = ALLOC(string*, newctrl->nstrings);
	s = newctrl->strings + nstrs;
	i = strchunksz;
	for (l = strlist; l != (strchunk *) NULL; ) {
	    while (i > 0) {
		*--s = l->s[--i];	/* already referenced */
	    }
	    i = STRING_CHUNK;
	    f = l;
	    l = l->next;
	    FREE(f);
	}
	strlist = (strchunk *) NULL;
	strchunksz = i;
    }
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
    register fcchunk *l;

    newctrl->nfuncalls = newctrl->inherits[newctrl->nvirtuals - 1].funcoffset +
			 nfcalls;
    if (newctrl->nfuncalls == 0) {
	return;
    }
    fc = newctrl->funcalls = ALLOC(char, 2 * newctrl->nfuncalls);
    for (i = 0; i < newctrl->nvirtuals - 1; i++) {
	/*
	 * Go down the inherited objects, starting with the ai object, and
	 * fill in the function call table segment for each object once.
	 */
	if (oh_new(newctrl->inherits[i].obj->chain.name)->index == i) {
	    register char *ofc;
	    register vfh *h;
	    register dfuncdef *f;
	    register control *ctrl, *ctrl2;
	    register uindex n;

	    /*
	     * build the function call segment, based on the function call
	     * table of the inherited object
	     */
	    ctrl = newctrl->inherits[i].obj->ctrl;
	    n = ctrl->nvirtuals - 1;
	    ofc = d_get_funcalls(ctrl) + 2L * ctrl->inherits[n].funcoffset;
	    for (n = ctrl->nfuncalls - ctrl->inherits[n].funcoffset; n > 0; --n)
	    {
		ctrl2 = ctrl->inherits[UCHAR(ofc[0])].obj->ctrl;
		f = &ctrl2->funcdefs[UCHAR(ofc[1])];
		h = *(vfh **) ht_lookup(ftab, d_get_strconst(ctrl2, f->inherit,
							     f->index)->text);
		if (f->class & C_LOCAL) {
		    /*
		     * keep old call
		     */
		    *fc++ = (ofc[0] == 0) ? 0 : ofc[0] + i;
		    *fc++ = ofc[1];
		} else {
		    if (functions[h->index].func.class & C_PRIVATE) {
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
	    *--fc = l->f[--i].index;
	    *--fc = l->f[i].inherit;
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
    for (i = 0; i < newctrl->nvirtuals; i++) {
	if (oh_new(newctrl->inherits[i].obj->chain.name)->index == i) {
	    register dfuncdef *f;
	    register control *ctrl;

	    ctrl = newctrl->inherits[i].obj->ctrl;
	    for (f = ctrl->funcdefs, n = 0; n < ctrl->nfuncdefs; f++, n++) {
		register vfh *h;
		register char *name;

		name = d_get_strconst(ctrl, f->inherit, f->index)->text;
		if ((f->class & C_PRIVATE) ||
		    ((f->class & C_STATIC) && ctrl->nvirtuals == 1)) {
		    continue;
		}
		h = *(vfh **) ht_lookup(ftab, name);
		if (h->ohash->index == nvirtuals &&
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
		    x = hashstr(name, VFMERGEHASHSZ, nsymbs);
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
    register unsigned int i;
    dinherit *inherits;
    dsymbol *symtab;

    if ((i = ctrl->nsymbols) == 0) {
	return (dsymbol *) NULL;
    }

    inherits = ctrl->inherits;
    symtab = d_get_symbols(ctrl);
    i = hashstr(func, VFMERGEHASHSZ, i);
    for (;;) {
	symb = &symtab[i];
	ctrl = o_control(inherits[UCHAR(symb->inherit)].obj);
	f = d_get_funcdefs(ctrl) + UCHAR(symb->index);
	if (strcmp(func, d_get_strconst(ctrl, f->inherit, f->index)->text) == 0)
	{
	    /* found it */
	    return (f->class & C_UNDEFINED) ? (dsymbol *) NULL : symb;
	}
	if (i == symb->next) {
	    /* points to itself: end of linked list */
	    return (dsymbol *) NULL;
	}
	i = symb->next;
    }
}

/*
 * NAME:	control->construct()
 * DESCRIPTION:	construct and return a control block for the object just
 *		compiled
 */
control *ctrl_construct(name)
char *name;
{
    register control *ctrl;
    register int i;

    ctrl = newctrl;
    for (i = ctrl->nvirtuals; i < ctrl->ninherits; i++) {
	/* adjust varoffset of labeled inherits */
	ctrl->inherits[i].varoffset += nvars;
    }
    ctrl->nvariables += nvars;

    o_new(name, (object *) NULL, ctrl);
    oh_new(name)->index = nvirtuals;
    ctrl_mkstrings();
    ctrl_mkfuncs();
    ctrl_mkvars();
    ctrl_mkfcalls();
    ctrl_mksymbs();

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
    lab_clear();

    ndirects = 0;
    ninherits = 0;
    nvirtuals = 0;
    nvars = 0;
    nfcalls = 0;
    nsymbs = 0;

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
