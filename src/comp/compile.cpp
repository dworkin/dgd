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
# include "path.h"
# include "ppcontrol.h"
# include "node.h"
# include "optimize.h"
# include "codegen.h"
# include "compile.h"

# define COND_CHUNK	16
# define COND_BMAP	BMAP(MAX_LOCALS)

class Cond : public ChunkAllocated {
public:
    void fill();
    void save(Cond *c2);
    void match(Cond *c2);

    static void create(Cond *c2);
    static void del();
    static void clear();

    Cond *prev;			/* surrounding conditional */
    Uint init[COND_BMAP];	/* initialize variable bitmap */
};

static Chunk<Cond, COND_CHUNK> cchunk;
static Cond *thiscond;		/* current condition */

/*
 * create a new condition
 */
void Cond::create(Cond *c2)
{
    Cond *c;

    c = chunknew (cchunk) Cond;
    c->prev = thiscond;
    if (c2 != (Cond *) NULL) {
	c->save(c2);
    } else {
	memset(c->init, '\0', COND_BMAP * sizeof(Uint));
    }
    thiscond = c;
}

/*
 * delete the current condition
 */
void Cond::del()
{
    Cond *c;

    c = thiscond;
    thiscond = c->prev;
    delete c;
}

/*
 * set all conditions
 */
void Cond::fill()
{
    memset(init, '\xff', COND_BMAP * sizeof(Uint));
}

/*
 * save a condition
 */
void Cond::save(Cond *c2)
{
    memcpy(init, c2->init, COND_BMAP * sizeof(Uint));
}

/*
 * match two conditions
 */
void Cond::match(Cond *c2)
{
    Uint *p, *q;
    int i;

    p = init;
    q = c2->init;
    for (i = COND_BMAP; i > 0; --i) {
	*p++ &= *q++;
    }
}

/*
 * clean up conditions
 */
void Cond::clear()
{
    cchunk.clean();
    thiscond = (Cond *) NULL;
}


# define CODEBLOCK_CHUNK	16

class CodeBlock : public ChunkAllocated {
public:
    static void create();
    static void del(bool keep);
    static int var(char *name);
    static void pdef(char *name, short type, String *cvstr);
    static void vdef(char *name, short type, String *cvstr);
    static int edef(char *name);
    static void clear();

    int vindex;			/* variable index */
    int nvars;			/* # variables in this block */
    CodeBlock *prev;		/* surrounding block */
    Node *gotos;		/* gotos in this block */
    Node *labels;		/* labels in this block */

private:
    static void resolve(Node *g);
};

static Chunk<CodeBlock, CODEBLOCK_CHUNK> bchunk;
static CodeBlock *thisblock;	/* current statement block */
static int vindex;		/* variable index */
static int nvars;		/* number of local variables */
static int nparams;		/* number of parameters */
static struct {
    const char *name;		/* variable name */
    short type;			/* variable type */
    bool rdonly;		/* read-only */
    short unset;		/* used before set? */
    String *cvstr;		/* class name */
} variables[MAX_LOCALS];	/* variables */

/*
 * start a new block
 */
void CodeBlock::create()
{
    CodeBlock *b;

    b = chunknew (bchunk) CodeBlock;
    if (thisblock == (CodeBlock *) NULL) {
	Cond::create((Cond *) NULL);
	b->vindex = 0;
	b->nvars = nparams;
    } else {
	b->vindex = ::vindex;
	b->nvars = 0;
    }
    b->prev = thisblock;
    b->gotos = (Node *) NULL;
    b->labels = (Node *) NULL;
    thisblock = b;
}

/*
 * resolve gotos in this block
 */
void CodeBlock::resolve(Node *g)
{
    CodeBlock *b;
    Node *l;

    for (b = thisblock; b != (CodeBlock *) NULL; b = b->prev) {
	for (l = b->labels; l != (Node *) NULL; l = l->r.right) {
	    if (l->l.string->cmp(g->l.string) == 0) {
		g->mod -= l->mod;
		g->r.right = l;
		return;
	    }
	}
    }

    Compile::error("unknown label: %s", g->l.string->text);
}

/*
 * finish the current block
 */
void CodeBlock::del(bool keep)
{
    Node *g, *r;
    CodeBlock *f;
    int i;

    for (g = thisblock->gotos; g != (Node *) NULL; g = r) {
	r = g->r.right;
	resolve(g);
    }

    f = thisblock;
    if (keep) {
	for (i = f->vindex; i < f->vindex + f->nvars; i++) {
	    /*
	     * Make sure that variables declared in the closing block can no
	     * longer be used.
	     */
	    variables[i].name = "-";
	}
    } else {
	::vindex = f->vindex;
    }
    thisblock = f->prev;
    if (thisblock == (CodeBlock *) NULL) {
	Cond::del();
    }
    delete f;
}

/*
 * return the index of the local var if found, or -1
 */
int CodeBlock::var(char *name)
{
    int i;

    for (i = ::vindex; i > 0; ) {
	if (strcmp(variables[--i].name, name) == 0) {
	    return i;
	}
    }
    return -1;
}

/*
 * declare a function parameter
 */
void CodeBlock::pdef(char *name, short type, String *cvstr)
{
    if (var(name) >= 0) {
	Compile::error("redeclaration of parameter %s", name);
    } else {
	/* "too many parameters" is checked for elsewhere */
	variables[nparams].name = name;
	variables[nparams].type = type;
	variables[nparams].rdonly = FALSE;
	variables[nparams].unset = 0;
	variables[nparams++].cvstr = cvstr;
	::vindex++;
	::nvars++;
    }
}

/*
 * declare a local variable
 */
void CodeBlock::vdef(char *name, short type, String *cvstr)
{
    if (var(name) >= thisblock->vindex) {
	Compile::error("redeclaration of local variable %s", name);
    } else if (::vindex == MAX_LOCALS) {
	Compile::error("too many local variables");
    } else {
	BCLR(thiscond->init, ::vindex);
	thisblock->nvars++;
	variables[::vindex].name = name;
	variables[::vindex].type = type;
	variables[::vindex].rdonly = FALSE;
	variables[::vindex].unset = 0;
	variables[::vindex++].cvstr = cvstr;
	if (::vindex > ::nvars) {
	    ::nvars++;
	}
    }
}

/*
 * declare an exception
 */
int CodeBlock::edef(char *name)
{
    if (::vindex == MAX_LOCALS) {
	Compile::error("too many local variables");
	return 0;
    } else {
	BCLR(thiscond->init, ::vindex);
	thisblock->nvars++;
	variables[::vindex].name = name;
	variables[::vindex].type = T_STRING;
	variables[::vindex].rdonly = TRUE;
	variables[::vindex].unset = 0;
	variables[::vindex].cvstr = (String *) NULL;
	if (::vindex >= ::nvars) {
	    ::nvars++;
	}

	return ::vindex++;
    }
}

/*
 * clean up blocks
 */
void CodeBlock::clear()
{
    bchunk.clean();
    thisblock = (CodeBlock *) NULL;
    ::vindex = 0;
    ::nvars = 0;
    ::nparams = 0;
}


# define LOOP_CHUNK	16

class Loop : public ChunkAllocated {
public:
    Loop();

    static void create();
    static void del();
    static void clear();

    bool brk;			/* seen any breaks? */
    bool cont;			/* seen any continues? */
    unsigned short nesting;	/* rlimits/catch nesting level */
    Node *vlist;		/* variable list */
    Loop *prev;			/* previous loop or switch */
};

static Chunk<Loop, LOOP_CHUNK> lchunk;
static unsigned short nesting;	/* current rlimits/catch nesting level */
static Loop *thisloop;		/* current loop */

Loop::Loop()
{
    brk = FALSE;
    cont = FALSE;
    nesting = ::nesting;
    vlist = (Node *) NULL;
}

/*
 * create a new loop
 */
void Loop::create()
{
    Loop *l;

    l = chunknew (lchunk) Loop;
    l->prev = thisloop;
    thisloop = l;
}

/*
 * delete a loop
 */
void Loop::del()
{
    Loop *f;

    f = thisloop;
    thisloop = thisloop->prev;
    delete f;
}

/*
 * delete all loops
 */
void Loop::clear()
{
    lchunk.clean();
}


class Switch : public Loop {
public:
    Switch();

    static void create();
    static void del();
    static void clear();

    char type;			/* case label type */
    bool dflt;			/* seen any default labels? */
    Uint ncase;			/* number of case labels */
    Node *case_list;		/* previous list of case nodes */
    Loop *env;			/* enclosing loop */
};

static Chunk<Switch, LOOP_CHUNK> wchunk;
static Node *case_list;		/* list of case labels */
static Switch *switch_list;	/* list of nested switches */

Switch::Switch() : Loop()
{
    type = T_MIXED;
    dflt = FALSE;
    ncase = 0;
    env = thisloop;
}

/*
 * create a swith
 */
void Switch::create()
{
    Switch *w;

    w = chunknew (wchunk) Switch;
    w->case_list = ::case_list;
    ::case_list = (Node *) NULL;
    w->prev = switch_list;
    switch_list = w;
}

/*
 * delete a switch
 */
void Switch::del()
{
    Switch *f;

    f = switch_list;
    ::case_list = switch_list->case_list;
    switch_list = (Switch *) switch_list->prev;
    delete f;
}

/*
 * delete all switches
 */
void Switch::clear()
{
    wchunk.clean();
}


struct Context {
    char *file;				/* file to compile */
    Frame *frame;			/* current interpreter stack frame */
    Context *prev;			/* previous context */
};

static Context *current;		/* current context */
static char *auto_object;		/* auto object */
static char *driver_object;		/* driver object */
static char *include;			/* standard include file */
static char **paths;			/* include paths */
static bool stricttc;			/* strict typechecking */
static bool typechecking;		/* is current function typechecked? */
static bool seen_decls;			/* seen any declarations yet? */
static short ftype;			/* current function type & class */
static String *fclass;			/* function class string */
extern int nerrors;			/* # of errors during parsing */

/*
 * initialize the compiler
 */
void Compile::init(char *a, char *d, char *i, char **p, int tc)
{
    stricttc = (tc == 2);
    Node::init(stricttc);
    Optimize::init();
    auto_object = a;
    driver_object = d;
    include = i;
    paths = p;
    ::typechecking = tc;
}

/*
 * clean up the compiler
 */
void Compile::clear()
{
    Codegen::clear();
    Loop::clear();
    thisloop = (Loop *) NULL;
    Switch::clear();
    switch_list = (Switch *) NULL;
    CodeBlock::clear();
    Cond::clear();
    Node::clear();
    seen_decls = FALSE;
    nesting = 0;
}

/*
 * return the global typechecking flag
 */
bool Compile::typechecking()
{
    return ::typechecking;
}

static long ncompiled;		/* # objects compiled */

/*
 * Inherit an object in the object currently being compiled.
 * Return TRUE if compilation can continue, or FALSE otherwise.
 */
bool Compile::inherit(char *file, Node *label, int priv)
{
    char buf[STRINGSZ];
    Object *obj;
    Frame *f;
    long ncomp;

    obj = NULL;

    if (strcmp(current->file, auto_object) == 0) {
	error("cannot inherit from auto object");
	return FALSE;
    }

    f = current->frame;
    if (strcmp(current->file, driver_object) == 0) {
	/*
	 * the driver object can only inherit the auto object
	 */
	file = PM->resolve(buf, file);
	if (strcmp(file, auto_object) != 0) {
	    error("illegal inherit from driver object");
	    return FALSE;
	}
	obj = Object::find(file, OACC_READ);
	if (obj == (Object *) NULL) {
	    compile(f, file, (Object *) NULL, 0, TRUE);
	    return FALSE;
	}
    } else {
	ncomp = ncompiled;

	/* get associated object */
	PUSH_STRVAL(f, String::create(NULL, strlen(current->file) + 1L));
	f->sp->string->text[0] = '/';
	strcpy(f->sp->string->text + 1, current->file);
	PUSH_STRVAL(f, String::create(file, strlen(file)));
	PUSH_INTVAL(f, priv);

	strncpy(buf, file, STRINGSZ - 1);
	buf[STRINGSZ - 1] = '\0';
	if (DGD::callDriver(f, "inherit_program", 3)) {
	    if (f->sp->type == T_OBJECT) {
		obj = OBJR(f->sp->oindex);
		f->sp++;
	    } else {
		/* returned value not an object */
		EC->error("Cannot inherit \"%s\"", buf);
	    }

	    if (ncomp != ncompiled) {
		return FALSE;	/* objects compiled inside inherit_program() */
	    }
	} else {
	    /* precompiling */
	    f->sp++;
	    file = PM->from(buf, current->file, file);
	    obj = Object::find(file, OACC_READ);
	    if (obj == (Object *) NULL) {
		compile(f, file, (Object *) NULL, 0, TRUE);
		return FALSE;
	    }
	}
    }

    if (obj->flags & O_DRIVER) {
	/* would mess up too many things */
	error("illegal to inherit driver object");
	return FALSE;
    }

    return Control::inherit(current->frame, current->file, obj,
			    (label == (Node *) NULL) ?
			     (String *) NULL : label->l.string,
			    priv);
}

extern int yyparse ();

/*
 * compile an LPC file
 */
Object *Compile::compile(Frame *f, char *file, Object *obj, int nstr, int iflag)
{
    Context c;
    char file_c[STRINGSZ + 2];
    Control *ctrl;

    if (iflag) {
	Context *cc;
	int n;

	for (cc = current, n = 0; cc != (Context *) NULL; cc = cc->prev, n++) {
	    if (strcmp(file, cc->file) == 0) {
		EC->error("Cycle in inheritance from \"/%s.c\"", current->file);
	    }
	}
	if (n >= 255) {
	    EC->error("Compilation nesting too deep");
	}

	PP->clear();
	Control::clear();
	clear();
    } else if (current != (Context *) NULL) {
	EC->error("Compilation within compilation");
    }

    c.file = file;
    if (strncmp(file, BIPREFIX, BIPREFIXLEN) == 0 ||
	strchr(file, '#') != (char *) NULL) {
	EC->error("Illegal object name \"/%s\"", file);
    }
    strcpy(file_c, file);
    if (nstr == 0) {
	strcat(file_c, ".c");
    }
    c.frame = f;
    c.prev = current;
    current = &c;
    ncompiled++;

    try {
	EC->push();
	for (;;) {
	    if (autodriver() != 0) {
		Control::prepare();
	    } else {
		Object *aobj;

		if (Object::find(driver_object, OACC_READ) == (Object *) NULL) {
		    /*
		     * compile the driver object to do pathname translation
		     */
		    current = (Context *) NULL;
		    compile(f, driver_object, (Object *) NULL, 0, FALSE);
		    current = &c;
		}

		aobj = Object::find(auto_object, OACC_READ);
		if (aobj == (Object *) NULL) {
		    /*
		     * compile auto object
		     */
		    aobj = compile(f, auto_object, (Object *) NULL, 0, TRUE);
		}
		/* inherit auto object */
		if (O_UPGRADING(aobj)) {
		    EC->error("Upgraded auto object while compiling \"/%s\"",
			      file_c);
		}
		Control::prepare();
		Control::inherit(c.frame, file, aobj, (String *) NULL, FALSE);
	    }

	    if (nstr != 0) {
		int i;
		Value *v;

		v = f->sp;
		PP->init(file_c, paths, v->string->text, v->string->len, 1);
		for (i = 1; i < nstr; i++) {
		    v++;
		    PP->push(v->string->text, v->string->len);
		}
	    } else if (!PP->init(file_c, paths, (char *) NULL, 0, 1)) {
		EC->error("Could not compile \"/%s\"", file_c);
	    }
	    if (!PP->include(include, (char *) NULL, 0)) {
		EC->error("Could not include \"/%s\"", include);
	    }

	    Codegen::init(c.prev != (Context *) NULL);
	    if (yyparse() == 0 && Control::checkFuncs()) {
		if (obj != (Object *) NULL) {
		    if (obj->count == 0) {
			EC->error("Object destructed during recompilation");
		    }
		    if (O_UPGRADING(obj)) {
			EC->error("Object recompiled during recompilation");
		    }
		    if (O_INHERITED(obj)) {
			/* inherited */
			EC->error("Object inherited during recompilation");
		    }
		}
		if (!Object::space()) {
		    EC->error("Too many objects");
		}

		/*
		 * successfully compiled
		 */
		break;

	    } else if (nerrors == 0) {
		/* another try */
		PP->clear();
		Control::clear();
		clear();
	    } else {
		/* compilation failed */
		EC->error("Failed to compile \"/%s\"", file_c);
	    }
	}
	EC->pop();
    } catch (const char*) {
	PP->clear();
	Control::clear();
	clear();
	current = c.prev;
	EC->error((char *) NULL);
    }

    PP->clear();
    if (!seen_decls) {
	/*
	 * object with inherit statements only (or nothing at all)
	 */
	Control::create();
    }
    ctrl = Control::construct();
    Control::clear();
    clear();
    current = c.prev;

    if (obj == (Object *) NULL) {
	/* new object */
	obj = Object::create(file, ctrl);
	if (strcmp(file, driver_object) == 0) {
	    obj->flags |= O_DRIVER;
	} else if (strcmp(file, auto_object) == 0) {
	    obj->flags |= O_AUTO;
	}
    } else {
	unsigned short *vmap;

	/* recompiled object */
	obj->upgrade(ctrl, f);
	vmap = ctrl->varmap(obj->ctrl);
	if (vmap != (unsigned short *) NULL) {
	    ctrl->setVarmap(vmap);
	}
    }
    return obj;
}

/*
 * indicate if the auto object or driver object is being compiled
 */
int Compile::autodriver()
{
    if (strcmp(current->file, auto_object) == 0) {
	return O_AUTO;
    }
    if (strcmp(current->file, driver_object) == 0) {
	return O_DRIVER;
    }
    return 0;
}


/*
 * handle an object type
 */
String *Compile::objecttype(Node *n)
{
    char path[STRINGSZ];

    if (autodriver() == 0) {
	char *p;
	Frame *f;

	f = current->frame;
	p = PP->filename();
	PUSH_STRVAL(f, String::create(p, strlen(p)));
	PUSH_STRVAL(f, n->l.string);
	DGD::callDriver(f, "object_type", 2);
	if (f->sp->type != T_STRING) {
	    error("invalid object type");
	    p = n->l.string->text;
	} else {
	    p = f->sp->string->text;
	}
	PM->resolve(path, p);
	(f->sp++)->del();
    } else {
	PM->resolve(path, n->l.string->text);
    }

    return String::create(path, strlen(path));
}

/*
 * ACTION:	declare a function
 */
void Compile::declFunc(unsigned short sclass, Node *type, String *str,
		       Node *formals, bool function)
{
    char proto[5 + (MAX_LOCALS + 1) * 4];
    char tnbuf[TNBUFSIZE];
    char *p, t;
    int nargs, vargs;
    long l;
    bool typechecked, varargs;

    varargs = FALSE;

    /* check for some errors */
    if ((sclass & (C_PRIVATE | C_NOMASK)) == (C_PRIVATE | C_NOMASK)) {
	error("private contradicts nomask");
    }
    if (sclass & C_VARARGS) {
	if (stricttc) {
	    error("varargs must be in parameter list");
	}
	sclass &= ~C_VARARGS;
	varargs = TRUE;
    }
    t = type->mod;
    if ((t & T_TYPE) == T_NIL) {
	/* don't typecheck this function */
	typechecked = FALSE;
	t = T_MIXED;
    } else {
	typechecked = TRUE;
	if (t != T_VOID && (t & T_TYPE) == T_VOID) {
	    error("invalid type for function %s (%s)", str->text,
		  Value::typeName(tnbuf, t));
	    t = T_MIXED;
	}
    }

    /* handle function arguments */
    ftype = t;
    fclass = type->sclass;
    p = &PROTO_FTYPE(proto);
    nargs = vargs = 0;

    if (formals != (Node *) NULL && (formals->flags & F_ELLIPSIS)) {
	sclass |= C_ELLIPSIS;
    }
    formals = Node::revert(formals);
    for (;;) {
	*p++ = t;
	if ((t & T_TYPE) == T_CLASS) {
	    l = Control::defString(type->sclass);
	    *p++ = l >> 16;
	    *p++ = l >> 8;
	    *p++ = l;
	}
	if (formals == (Node *) NULL) {
	    break;
	}
	if (nargs == MAX_LOCALS) {
	    error("too many parameters in function %s", str->text);
	    break;
	}

	if (formals->type == N_PAIR) {
	    type = formals->l.left;
	    formals = formals->r.right;
	} else {
	    type = formals;
	    formals = (Node *) NULL;
	}
	t = type->mod;
	if ((t & T_TYPE) == T_NIL) {
	    if (typechecked) {
		error("missing type for parameter %s", type->l.string->text);
	    }
	    t = T_MIXED;
	} else if ((t & T_TYPE) == T_VOID) {
	    error("invalid type for parameter %s (%s)", type->l.string->text,
		  Value::typeName(tnbuf, t));
	    t = T_MIXED;
	} else if (typechecked && t != T_MIXED) {
	    /* only bother to typecheck functions with non-mixed arguments */
	    sclass |= C_TYPECHECKED;
	}
	if (type->flags & F_VARARGS) {
	    if (varargs) {
		error("extra varargs for parameter %s", type->l.string->text);
	    }
	    varargs = TRUE;
	}
	if (formals == (Node *) NULL && (sclass & C_ELLIPSIS)) {
	    /* ... */
	    varargs = TRUE;
	    if (((t + (1 << REFSHIFT)) & T_REF) == 0) {
		error("too deep indirection for parameter %s",
		      type->l.string->text);
	    }
	    if (function) {
		CodeBlock::pdef(type->l.string->text, t + (1 << REFSHIFT),
			       type->sclass);
	    }
	} else if (function) {
	    CodeBlock::pdef(type->l.string->text, t, type->sclass);
	}

	if (!varargs) {
	    nargs++;
	} else {
	    vargs++;
	}
    }

    PROTO_CLASS(proto) = sclass;
    PROTO_NARGS(proto) = nargs;
    PROTO_VARGS(proto) = vargs;
    nargs = p - proto;
    PROTO_HSIZE(proto) = nargs >> 8;
    PROTO_LSIZE(proto) = nargs;

    /* define prototype */
    if (function) {
	Control::defFunc(str, proto, fclass);
    } else {
	PROTO_CLASS(proto) |= C_UNDEFINED;
	Control::defProto(str, proto, fclass);
    }
}

/*
 * declare a variable
 */
void Compile::declVar(unsigned short sclass, Node *type, String *str,
		      bool global)
{
    char tnbuf[TNBUFSIZE];

    if ((type->mod & T_TYPE) == T_VOID) {
	error("invalid type for variable %s (%s)", str->text,
	      Value::typeName(tnbuf, type->mod));
	type->mod = T_MIXED;
    }
    if (global) {
	if (sclass & (C_ATOMIC | C_NOMASK | C_VARARGS)) {
	    error("invalid class for variable %s", str->text);
	}
	Control::defVar(str, sclass, type->mod, type->sclass);
    } else {
	if (sclass != 0) {
	    error("invalid class for variable %s", str->text);
	}
	CodeBlock::vdef(str->text, type->mod, type->sclass);
    }
}

/*
 * handle a list of declarations
 */
void Compile::declList(unsigned short sclass, Node *type, Node *list,
		       bool global)
{
    Node *n;

    list = Node::revert(list);	/* for proper order of err mesgs */
    while (list != (Node *) NULL) {
	if (list->type == N_PAIR) {
	    n = list->l.left;
	    list = list->r.right;
	} else {
	    n = list;
	    list = (Node *) NULL;
	}
	type->mod = (type->mod & T_TYPE) | n->mod;
	if (n->type == N_FUNC) {
	    declFunc(sclass, type, n->l.left->l.string, n->r.right, FALSE);
	} else {
	    declVar(sclass, type, n->l.string, global);
	}
    }
}

/*
 * handle a global declaration
 */
void Compile::global(unsigned int sclass, Node *type, Node *n)
{
    if (!seen_decls) {
	Control::create();
	seen_decls = TRUE;
    }
    declList(sclass, type, n, TRUE);
}

static String *fname;		/* name of current function */
static unsigned short fline;	/* first line of function */

/*
 * create a function
 */
void Compile::function(unsigned int sclass, Node *type, Node *n)
{
    if (!seen_decls) {
	Control::create();
	seen_decls = TRUE;
    }
    type->mod |= n->mod;
    declFunc(sclass, type, fname = n->l.left->l.string, n->r.right, TRUE);
}

/*
 * create a function body
 */
void Compile::funcbody(Node *n)
{
    char *prog;
    Uint depth;
    unsigned short size;
    Float flt;

    flt.initZero();
    switch (ftype) {
    case T_INT:
	n = concat(n, Node::createMon(N_RETURN, 0, Node::createInt(0)));
	break;

    case T_FLOAT:
	n = concat(n, Node::createMon(N_RETURN, 0, Node::createFloat(&flt)));
	break;

    default:
	n = concat(n, Node::createMon(N_RETURN, 0, Node::createNil()));
	break;
    }

    n = Optimize::stmt(n, &depth);
    if (depth > 0x7fff) {
	error("function uses too much stack space");
    } else {
	prog = Codegen::function(fname, n, nvars, nparams,
				 (unsigned short) depth, &size);
	Control::defProgram(prog, size);
    }
    Node::clear();
    vindex = 0;
    nvars = 0;
    nparams = 0;
}

/*
 * handle local declarations
 */
void Compile::local(unsigned int sclass, Node *type, Node *n)
{
    declList(sclass, type, n, FALSE);
}


/*
 * start a condition
 */
void Compile::startCond()
{
    Cond::create(thiscond);
}

/*
 * start a second condition
 */
void Compile::startCond2()
{
    Cond::create(thiscond->prev);
}

/*
 * end a condition
 */
void Compile::endCond()
{
    Cond::del();
}

/*
 * save condition
 */
void Compile::saveCond()
{
    thiscond->prev->save(thiscond);
    Cond::del();
}

/*
 * match conditions
 */
void Compile::matchCond()
{
    thiscond->prev->match(thiscond);
    Cond::del();
}

/*
 * check if an expression has the value nil
 */
bool Compile::nil(Node *n)
{
    if (n->type == N_COMMA) {
	/* the parser always generates comma expressions as (a, b), c */
	n = n->r.right;
    }
    return (n->type == nil_node && n->l.number == 0);
}

/*
 * concatenate two statements
 */
Node *Compile::concat(Node *n1, Node *n2)
{
    Node *n;

    if (n1 == (Node *) NULL) {
	return n2;
    } else if (n2 == (Node *) NULL ||
	       ((n1->flags & F_END) && !(n2->flags & F_REACH))) {
	return n1;
    }

    n = Node::createBin(N_PAIR, 0, n1, n2);
    n->flags |= (n1->flags & (F_ENTRY | F_REACH)) |
		(n2->flags & (F_REACH | F_END));
    return n;
}

/*
 * reduce an expression to a statement
 */
Node *Compile::exprStmt(Node *n)
{
    if (n != (Node *) NULL) {
	return Node::createMon(N_POP, 0, n);
    }
    return n;
}

/*
 * handle an if statement
 */
Node *Compile::ifStmt(Node *n1, Node *n2)
{
    return Node::createBin(N_IF, 0, n1, Node::createMon(N_ELSE, 0, n2));
}

/*
 * end an if statement
 */
Node *Compile::endIfStmt(Node *n1, Node *n3)
{
    Node *n2;
    int flags1, flags2;

    n2 = n1->r.right->l.left;
    n1->r.right->r.right = n3;
    if (n2 != (Node *) NULL) {
	flags1 = n2->flags & F_END;
	n1->flags |= n2->flags & F_REACH;
    } else {
	flags1 = 0;
    }
    if (n3 != (Node *) NULL) {
	flags2 = n3->flags & F_END;
	n1->flags |= n3->flags & F_REACH;
    } else {
	flags2 = 0;
    }

    if (flags1 != 0 && flags2 != 0) {
	n1->flags |= flags1 | flags2;
    }
    return n1;
}

/*
 * create a scope block for break or continue
 */
Node *Compile::block(Node *n, int type, int flags)
{
    n = Node::createMon(N_BLOCK, type, n);
    n->flags |= n->l.left->flags & F_FLOW & ~F_EXIT & ~flags;
    return n;
}

/*
 * start a loop
 */
void Compile::loop()
{
    Loop::create();
}

/*
 * loop back a loop
 */
Node *Compile::reloop(Node *n)
{
    return (thisloop->cont) ? block(n, N_CONTINUE, F_END) : n;
}

/*
 * end a loop
 */
Node *Compile::endloop(Node *n)
{
    if (thisloop->brk) {
	n = block(n, N_BREAK, F_BREAK);
    }
    Loop::del();
    return n;
}

/*
 * end a do-while loop
 */
Node *Compile::doStmt(Node *n1, Node *n2)
{
    n1 = Node::createBin(N_DO, 0, n1, n2 = reloop(n2));
    if (n2 != (Node *) NULL) {
	n1->flags |= n2->flags & F_FLOW;
    }
    return endloop(n1);
}

/*
 * end a while loop
 */
Node *Compile::whileStmt(Node *n1, Node *n2)
{
    n1 = Node::createBin(N_FOR, 0, n1, n2 = reloop(n2));
    if (n2 != (Node *) NULL) {
	n1->flags |= n2->flags & F_FLOW & ~(F_ENTRY | F_EXIT);
    }
    return endloop(n1);
}

/*
 * end a for loop
 */
Node *Compile::forStmt(Node *n1, Node *n2, Node *n3, Node *n4)
{
    n4 = reloop(n4);
    n2 = Node::createBin((n2 == (Node *) NULL) ? N_FOREVER : N_FOR,
			 0, n2, concat(n4, n3));
    if (n4 != (Node *) NULL) {
	n2->flags = n4->flags & F_FLOW & ~(F_ENTRY | F_EXIT);
    }

    return concat(n1, endloop(n2));
}

/*
 * begin rlimit handling
 */
void Compile::startRlimits()
{
    nesting++;
}

/*
 * handle statements with resource limitations
 */
Node *Compile::endRlimits(Node *n1, Node *n2, Node *n3)
{
    --nesting;

    if (strcmp(current->file, driver_object) == 0 ||
	strcmp(current->file, auto_object) == 0) {
	n1 = Node::createBin(N_RLIMITS, 1,
			     Node::createBin(N_PAIR, 0, n1, n2), n3);
    } else {
	Frame *f;

	f = current->frame;
	PUSH_STRVAL(f, String::create((char *) NULL,
				      strlen(current->file) + 1));
	f->sp->string->text[0] = '/';
	strcpy(f->sp->string->text + 1, current->file);
	DGD::callDriver(f, "compile_rlimits", 1);
	n1 = Node::createBin(N_RLIMITS, VAL_TRUE(f->sp),
			     Node::createBin(N_PAIR, 0, n1, n2),
			     n3);
	(f->sp++)->del();
    }

    if (n3 != (Node *) NULL) {
	n1->flags |= n3->flags & F_END;
    }
    return n1;
}

/*
 * preserve an exception
 */
Node *Compile::exception(Node *n)
{
    int exception;

    exception = CodeBlock::edef(n->l.string->text);
    BSET(thiscond->init, exception);
    n = Node::createMon(N_LOCAL, T_STRING, n);
    n->r.number = exception;
    return Node::createMon(N_EXCEPTION, T_STRING, n);
}

/*
 * begin catch handling
 */
void Compile::startCatch()
{
    nesting++;
}

/*
 * end catch handling
 */
void Compile::endCatch()
{
    --nesting;
}

/*
 * handle statements within catch
 */
Node *Compile::doneCatch(Node *n1, Node *n2, bool pop)
{
    Node *n;
    int flags1, flags2;

    n = Node::createBin(N_CATCH, pop, n1, n2);
    if (n1 != (Node *) NULL) {
	flags1 = n1->flags & F_END;
    } else {
	flags1 = 0;
    }
    if (n2 != (Node *) NULL) {
	n->flags |= n2->flags & F_REACH;
	flags2 = n2->flags & F_END;
    } else {
	flags2 = 0;
    }

    if (flags1 != 0 && flags2 != 0) {
	n->flags |= flags1 | flags2;
    }
    return n;
}

/*
 * start a switch statement
 */
void Compile::startSwitch(Node *n, int typechecked)
{
    char tnbuf[TNBUFSIZE];

    Switch::create();
    if (typechecked &&
	n->mod != T_INT && n->mod != T_STRING && n->mod != T_MIXED) {
	error("bad switch expression type (%s)",
	      Value::typeName(tnbuf, n->mod));
	switch_list->type = T_NIL;
    }

    Cond::create((Cond *) NULL);
    thiscond->fill();
    Cond::create(thiscond->prev);
}

static int cmp(cvoid*, cvoid*);

/*
 * compare two case label nodes
 */
static int cmp(cvoid *cv1, cvoid *cv2)
{
    Node **n1, **n2;

    n1 = (Node **) cv1;
    n2 = (Node **) cv2;
    if (n1[0]->l.left->type == N_STR) {
	if (n2[0]->l.left->type == N_STR) {
	    return n1[0]->l.left->l.string->cmp(n2[0]->l.left->l.string);
	} else {
	    return 1;	/* str > nil */
	}
    } else if (n2[0]->l.left->type == N_STR) {
	return -1;	/* nil < str */
    } else {
	return (n1[0]->l.left->l.number <= n2[0]->l.left->l.number) ? -1 : 1;
    }
}

/*
 * end a switch
 */
Node *Compile::endSwitch(Node *expr, Node *stmt)
{
    char tnbuf[TNBUFSIZE];
    Node **v, **w, *n;
    unsigned short i, size;
    LPCint l;
    LPCuint cnt;
    short type, sz;

    n = stmt;
    if (n != (Node *) NULL) {
	n->r.right = switch_list->vlist;
	if (switch_list->prev != (Loop *) NULL) {
	    switch_list->prev->vlist = concat(n->r.right,
					      switch_list->prev->vlist);
	}

	if (switch_list->dflt) {
	    if (!(n->flags & F_BREAK)) {
		thiscond->prev->match(thiscond);
	    }
	    thiscond->prev->prev->save(thiscond->prev);
	}
    }

    thiscond->del();
    thiscond->del();

    if (switch_list->type != T_NIL) {
	if (stmt == (Node *) NULL) {
	    /* empty switch statement */
	    n = exprStmt(expr);
	} else if (!(stmt->flags & F_ENTRY)) {
	    error("unreachable code in switch");
	} else if (switch_list->ncase > 0x7fff) {
	    error("too many cases in switch");
	} else if ((size=switch_list->ncase - switch_list->dflt) == 0) {
	    if (switch_list->ncase == 0) {
		/* can happen when recovering from syntax error */
		n = exprStmt(expr);
	    } else {
		/* only a default label: erase N_CASE */
		n = case_list->r.right->r.right->l.left;
		*(case_list->r.right->r.right) = *n;
		n->type = N_FAKE;
		if (switch_list->brk) {
		    /*
		     * enclose the break statement with a proper block
		     */
		    stmt = concat(stmt,
				  Node::createMon(N_BREAK, 0, (Node *) NULL));
		    stmt = Node::createBin(N_FOREVER, 0, (Node *) NULL, stmt);
		    stmt->flags |= stmt->r.right->flags & F_FLOW;
		    stmt = block(stmt, N_BREAK, F_BREAK);
		}
		n = concat(exprStmt(expr), stmt);
	    }
	} else if (expr->mod != T_MIXED && expr->mod != switch_list->type &&
		   switch_list->type != T_MIXED) {
	    error("wrong switch expression type (%s)",
		  Value::typeName(tnbuf, expr->mod));
	} else {
	    /*
	     * get the labels in an array, and sort them
	     */
	    v = ALLOCA(Node*, size);
	    for (i = size, n = case_list; i > 0; n = n->l.left) {
		if (n->r.right->l.left != (Node *) NULL) {
		    *v++ = n->r.right;
		    --i;
		}
	    }
	    std::qsort(v -= size, size, sizeof(Node *), cmp);

	    if (switch_list->type == T_STRING) {
		type = N_SWITCH_STR;
		if (size >= 2) {
		    /*
		     * check for duplicate cases
		     */
		    if (v[1]->l.left->type == nil_node) {
			error("duplicate case labels in switch");
		    } else {
			i = (v[0]->l.left->type == nil_node);
			for (w = v + i, i = size - i - 1; i > 0; w++, --i) {
			    if (w[0]->l.left->l.string->cmp(w[1]->l.left->l.string) == 0) {
				error("duplicate case labels in switch");
				break;
			    }
			}
		    }
		}
		sz = 0;
	    } else {
		type = N_SWITCH_INT;
		/*
		 * check for duplicate cases
		 */
		i = size;
		cnt = LPCUINT_MAX;
		w = v;
		for (;;) {
		    cnt += w[0]->l.left->r.number - w[0]->l.left->l.number + 1;
		    if (--i == 0) {
			break;
		    }
		    if (w[0]->l.left->r.number >= w[1]->l.left->l.number) {
			if (w[0]->l.left->l.number == w[1]->l.left->r.number) {
			    error("duplicate case labels in switch");
			} else {
			    error("overlapping case label ranges in switch");
			}
			break;
		    }
		    w++;
		}

		/* determine the number of bytes per case */
		l = v[0]->l.left->l.number;
		if (l < 0) {
		    l = -1 - l;
		}
		if (l < w[0]->l.left->r.number) {
		    l = w[0]->l.left->r.number;
		}
		if (l <= 127) {
		    sz = 1;
		} else if (l <= 32767) {
		    sz = 2;
		} else if (l <= 8388607L) {
		    sz = 3;
# ifdef LARGENUM
		} else if (l <= 2147483647LL) {
		    sz = 4;
		} else if (l <= 549755813887LL) {
		    sz = 5;
		} else if (l <= 140737488355327LL) {
		    sz = 6;
		} else if (l <= 36028797018963967LL) {
		    sz = 7;
# endif
		} else {
		    sz = sizeof(LPCint);
		}

		if (i == 0 && cnt >= size) {
		    if (cnt >= LPCUINT_MAX / 6 ||
			(sz + 2L) * cnt >= (2 * sz + 2L) * size) {
			/*
			 * no point in changing the type of switch
			 */
			type = N_SWITCH_RANGE;
		    } else {
			/*
			 * convert range label switch to int label switch
			 * by adding new labels
			 */
			w = ALLOCA(Node*, ++cnt);
			for (i = size; i > 0; --i) {
			    *w++ = *v;
			    for (l = v[0]->l.left->l.number;
				 l < v[0]->l.left->r.number; ) {
				/* insert N_CASE in statement */
				n = Node::createMon(N_CASE, 0,
						    v[0]->r.right->l.left);
				v[0]->r.right->l.left = n;
				l++;
				*w++ = Node::createBin(N_PAIR, 0,
						       Node::createInt(l), n);
			    }
			    v++;
			}
			AFREE(v - size);
			size = cnt;
			v = w - size;
		    }
		}
	    }

	    /*
	     * turn array into linked list
	     */
	    v += size;
	    n = (Node *) NULL;
	    i = size;
	    do {
		(*--v)->r.right->mod = i;
		n = Node::createBin(N_PAIR, 0, v[0]->l.left, n);
	    } while (--i > 0);
	    AFREE(v);
	    if (switch_list->dflt) {
		/* add default case */
		n = Node::createBin(N_PAIR, 0, (Node *) NULL, n);
		size++;
	    }

	    if (switch_list->brk) {
		stmt = block(stmt, N_BREAK, F_BREAK);
	    }
	    n = Node::createBin(type, size, n,
				Node::createBin(N_PAIR, sz, expr, stmt));
	}
    }

    Switch::del();
    if (switch_list == (Switch *) NULL) {
	vindex = thisblock->vindex + thisblock->nvars;
    }

    return n;
}

/*
 * handle a case label
 */
Node *Compile::caseLabel(Node *n1, Node *n2)
{
    if (switch_list == (Switch *) NULL) {
	error("case label not inside switch");
	return (Node *) NULL;
    }
    if (switch_list->nesting != nesting) {
	error("illegal jump into rlimits or catch");
	return (Node *) NULL;
    }
    if (switch_list->type == T_NIL) {
	return (Node *) NULL;
    }

    if (n1->type == N_STR || n1->type == N_NIL) {
	/* string */
	if (n2 != (Node *) NULL) {
	    error("bad case range");
	    switch_list->type = T_NIL;
	    return (Node *) NULL;
	}
	/* compare type with other cases */
	if (switch_list->type == T_MIXED) {
	    switch_list->type = T_STRING;
	} else if (switch_list->type != T_STRING) {
	    error("multiple case types in switch");
	    switch_list->type = T_NIL;
	    return (Node *) NULL;
	}
    } else {
	/* int */
	if (n1->type != N_INT) {
	    error("bad case expression");
	    switch_list->type = T_NIL;
	    return (Node *) NULL;
	}
	if (n2 == (Node *) NULL) {
	    n1->r.number = n1->l.number;
	} else {
	    /* range */
	    if (n2->type != N_INT) {
		error("bad case range");
		switch_list->type = T_NIL;
		return (Node *) NULL;
	    }
	    if (n2->l.number < n1->l.number) {
		/* inverted range */
		n1->r.number = n1->l.number;
		n1->l.number = n2->l.number;
		n1->type = N_RANGE;
	    } else {
		n1->r.number = n2->l.number;
		if (n1->l.number != n1->r.number) {
		    n1->type = N_RANGE;
		}
	    }
	}
	/* compare type with other cases */
	if (n1->l.number != 0 || n2 != (Node *) NULL || ::nil.type != T_INT) {
	    if (switch_list->type == T_MIXED) {
		switch_list->type = T_INT;
	    } else if (switch_list->type != T_INT) {
		error("multiple case types in switch");
		switch_list->type = T_NIL;
		return (Node *) NULL;
	    }
	}
    }

    thiscond->save(thiscond->prev->prev);

    switch_list->ncase++;
    n2 = Node::createMon(N_CASE, 0, (Node *) NULL);
    n2->flags |= F_ENTRY | F_CASE;
    case_list = Node::createBin(N_PAIR, 0, case_list,
				Node::createBin(N_PAIR, 0, n1, n2));
    return n2;
}

/*
 * handle a default label
 */
Node *Compile::defaultLabel()
{
    Node *n;

    n = (Node *) NULL;
    if (switch_list == (Switch *) NULL) {
	error("default label not inside switch");
    } else if (switch_list->dflt) {
	error("duplicate default label in switch");
	switch_list->type = T_NIL;
    } else if (switch_list->nesting != nesting) {
	error("illegal jump into rlimits or catch");
    } else {
	thiscond->save(thiscond->prev->prev);

	switch_list->ncase++;
	switch_list->dflt = TRUE;
	n = Node::createMon(N_CASE, 0, (Node *) NULL);
	n->flags |= F_ENTRY | F_CASE;
	case_list = Node::createBin(N_PAIR, 0, case_list,
				    Node::createBin(N_PAIR, 0, (Node *) NULL,
				    n));
    }

    return n;
}

/*
 * add a label
 */
Node *Compile::label(Node *n)
{
    CodeBlock *b;
    Node *l;

    for (b = thisblock; b != (CodeBlock *) NULL; b = b->prev) {
	for (l = b->labels; l != (Node *) NULL; l = l->r.right) {
	    if (n->l.string->cmp(l->l.string) == 0) {
		error("redeclaration of label: %s", n->l.string->text);
		return NULL;
	    }
	}
    }

    n->r.right = thisblock->labels;
    thisblock->labels = n;
    n->type = N_LABEL;
    n->mod = nesting;
    n->flags = F_ENTRY | F_LABEL;
    return n;
}

/*
 * handle goto
 */
Node *Compile::gotoStmt(Node *n)
{
    n->r.right = thisblock->gotos;
    thisblock->gotos = n;
    n->type = N_GOTO;
    n->mod = nesting;
    n->flags = F_EXIT;
    return n;
}

/*
 * handle a break statement
 */
Node *Compile::breakStmt()
{
    Loop *l;
    Node *n;

    l = switch_list;
    if (l == (Loop *) NULL || switch_list->env != thisloop) {
	/* no switch, or loop inside switch */
	l = thisloop;
    } else {
	thiscond->prev->match(thiscond);
    }
    if (l == (Loop *) NULL) {
	error("break statement not inside loop or switch");
	return (Node *) NULL;
    }
    l->brk = TRUE;

    n = Node::createMon(N_BREAK, nesting - l->nesting, (Node *) NULL);
    n->flags |= F_BREAK;
    return n;
}

/*
 * handle a continue statement
 */
Node *Compile::continueStmt()
{
    Node *n;

    if (thisloop == (Loop *) NULL) {
	error("continue statement not inside loop");
	return (Node *) NULL;
    }
    thisloop->cont = TRUE;

    n = Node::createMon(N_CONTINUE, nesting - thisloop->nesting, (Node *) NULL);
    n->flags |= F_CONTINUE;
    return n;
}

/*
 * handle a return statement
 */
Node *Compile::returnStmt(Node *n, int typechecked)
{
    char tnbuf1[TNBUFSIZE], tnbuf2[TNBUFSIZE];

    if (n == (Node *) NULL) {
	if (typechecked && ftype != T_VOID) {
	    error("function must return value");
	}
	n = Node::createNil();
    } else if (typechecked) {
	if (ftype == T_VOID) {
	    /*
	     * can't return anything from a void function
	     */
	    error("value returned from void function");
	} else if ((!nil(n) || !T_POINTER(ftype)) &&
		   matchType(n->mod, ftype) == T_NIL) {
	    /*
	     * type error
	     */
	    error("returned value doesn't match %s (%s)",
		  Value::typeName(tnbuf1, ftype),
		  Value::typeName(tnbuf2, n->mod));
	} else if ((ftype != T_MIXED && n->mod == T_MIXED) ||
		   (ftype == T_CLASS &&
		    (n->mod != T_CLASS || fclass->cmp(n->sclass) != 0))) {
	    /*
	     * typecheck at runtime
	     */
	    n = Node::createMon(N_CAST, ftype, n);
	    n->sclass = fclass;
	    if (fclass != (String *) NULL) {
		fclass->ref();
	    }
	}
    }

    n = Node::createMon(N_RETURN, nesting, n);
    n->flags |= F_EXIT;
    return n;
}

/*
 * start a compound statement
 */
void Compile::startCompound()
{
    if (thisblock == (CodeBlock *) NULL) {
	fline = PP->line();
    }
    CodeBlock::create();
}

/*
 * end a compound statement
 */
Node *Compile::endCompound(Node *n)
{
    int flags;

    if (n != (Node *) NULL) {
      flags = n->flags & (F_REACH | F_END);
      if (n->type == N_PAIR) {
	  n = Node::revert(n);
	  n->flags = (n->flags & ~F_END) | flags;
      }
      n = Node::createMon(N_COMPOUND, 0, n);
      n->flags = n->l.left->flags & ~F_LABEL;

      if (thisblock->nvars != 0) {
	  Node *v, *l, *z, *f, *p;
	  int i;

	  /*
	   * create variable type definitions and implicit initializers
	   */
	  l = z = f = p = (Node *) NULL;
	  i = thisblock->vindex;
	  if (i < nparams) {
	      i = nparams;
	  }
	  while (i < thisblock->vindex + thisblock->nvars) {
	      l = concat(Node::createVar(variables[i].type, i), l);

	      if (switch_list != (Switch *) NULL || (flags & F_LABEL) ||
		  variables[i].unset) {
		  switch (variables[i].type) {
		  case T_INT:
		      v = Node::createMon(N_LOCAL, T_INT, (Node *) NULL);
		      v->line = 0;
		      v->r.number = i;
		      if (z == (Node *) NULL) {
			  z = Node::createInt(0);
			  z->line = 0;
		      }
		      z = Node::createBin(N_ASSIGN, T_INT, v, z);
		      z->line = 0;
		      break;

		  case T_FLOAT:
		      v = Node::createMon(N_LOCAL, T_FLOAT, (Node *) NULL);
		      v->line = 0;
		      v->r.number = i;
		      if (f == (Node *) NULL) {
			  Float flt;

			  flt.initZero();
			  f = Node::createFloat(&flt);
			  f->line = 0;
		      }
		      f = Node::createBin(N_ASSIGN, T_FLOAT, v, f);
		      f->line = 0;
		      break;

		  default:
		      v = Node::createMon(N_LOCAL, T_MIXED, (Node *) NULL);
		      v->line = 0;
		      v->r.number = i;
		      if (p == (Node *) NULL) {
			  p = Node::createNil();
			  p->line = 0;
		      }
		      p = Node::createBin(N_ASSIGN, T_MIXED, v, p);
		      p->line = 0;
		      break;
		  }
	      }
	      i++;
	  }

	  /* add vartypes and initializers to compound statement */
	  if (z != (Node *) NULL) {
	      l = concat(exprStmt(z), l);
	  }
	  if (f != (Node *) NULL) {
	      l = concat(exprStmt(f), l);
	  }
	  if (p != (Node *) NULL) {
	      l = concat(exprStmt(p), l);
	  }
	  n->r.right = l;
	  if (switch_list != (Switch *) NULL) {
	      switch_list->vlist = concat(l, switch_list->vlist);
	  }
      }
    }

    CodeBlock::del(switch_list != (Switch *) NULL);
    return n;
}

/*
 * look up a local function, inherited function or kfun
 */
Node *Compile::flookup(Node *n, int typechecked)
{
    char *proto;
    String *sclass;
    long call;

    proto = Control::funCall(n->l.string, &sclass, &call, typechecked);
    n->r.right = (proto == (char *) NULL) ?
		  (Node *) NULL :
		  Node::createFcall(PROTO_FTYPE(proto), sclass, proto, call);
    return n;
}

/*
 * look up an inherited function
 */
Node *Compile::iflookup(Node *n, Node *label)
{
    char *proto;
    String *sclass;
    long call;

    proto = Control::iFunCall(n->l.string, (label != (Node *) NULL) ?
				     label->l.string->text : (char *) NULL,
			      &sclass, &call);
    n->r.right = (proto == (char *) NULL) ? (Node *) NULL :
		  Node::createFcall(PROTO_FTYPE(proto), sclass, proto, call);
    return n;
}

/*
 * determine combined type of an array aggregate
 */
unsigned int Compile::aggrType(unsigned int t1, unsigned int t2)
{
    if ((t2 & T_TYPE) == T_CLASS) {
	/* from class to object */
	t2 = (t2 & T_REF) | T_OBJECT;
    }

    switch (t1) {
    case T_ARRAY:
	return t2;		/* first element */

    case T_NIL:
	if (t2 == T_NIL || T_POINTER(t2)) {
	    return t2;		/* nil matches pointer/nil */
	}
	/* fall through */
    case T_MIXED:
	break;

    default:
	if (t2 == T_NIL) {
	    if (T_POINTER(t1)) {
		return t1;	/* nil matches pointer */
	    }
	    break;
	}

	if ((t1 & T_REF) != (t2 & T_REF)) {
	    /* unequal references */
	    return (t1 > t2) ? (t2 & T_REF) | T_MIXED : (t1 & T_REF) | T_MIXED;
	}
	return (t1 == t2) ? t1 : (t1 & T_REF) | T_MIXED;
    }

    return T_MIXED;
}

/*
 * create an aggregate
 */
Node *Compile::aggregate(Node *n, unsigned int type)
{
    if (type == T_ARRAY) {
	if (!stricttc || n == NULL) {
	    type = T_MIXED;
	} else {
	    Node *m;

	    for (m = n; m->type == N_PAIR; m = m->l.left) {
		type = aggrType(type, m->r.right->mod);
	    }
	    type = aggrType(type, m->mod);
	    if (type == T_NIL) {
		type = T_MIXED;
	    }
	}
	type += (1 << REFSHIFT);
    }
    return Node::createMon(N_AGGR, type, Node::revert(n));
}

/*
 * create a reference to a local variable
 */
Node *Compile::localVar(Node *n)
{
    int i;

    i = CodeBlock::var(n->l.string->text);
    if (i < 0) {
	return (Node *) NULL;
    }

    if (!BTST(thiscond->init, i)) {
	variables[i].unset++;
    }
    n = Node::createMon(N_LOCAL, variables[i].type, n);
    n->sclass = variables[i].cvstr;
    if (n->sclass != (String *) NULL) {
	n->sclass->ref();
    }
    n->r.number = i;
    return n;
}

/*
 * create a reference to a global variable
 */
Node *Compile::globalVar(Node *n)
{
    String *sclass;
    long ref;

    n = Node::createMon(N_GLOBAL, Control::var(n->l.string, &ref, &sclass), n);
    n->sclass = sclass;
    if (sclass != (String *) NULL) {
	sclass->ref();
    }
    n->r.number = ref;

    return n;
}

/*
 * return the type of a variable
 */
short Compile::vtype(int i)
{
    return variables[i].type;
}

/*
 * check if a value can be an lvalue
 */
bool Compile::lvalue(Node *n)
{
    if (n->type == N_CAST && n->mod == n->l.left->mod) {
	/* only an implicit cast is allowed */
	n = n->l.left;
    }
    switch (n->type) {
    case N_LOCAL:
	if (variables[n->r.number].rdonly) {
	    return FALSE;
	}
	/* fall through */
    case N_GLOBAL:
    case N_INDEX:
    case N_FAKE:
	return TRUE;

    default:
	return FALSE;
    }
}

/*
 * handle a function call
 */
Node *Compile::funcall(Node *call, Node *args, int funcptr)
{
    char tnbuf[TNBUFSIZE];
    int n, nargs, t;
    Node *func, **argv, **arg;
    char *argp, *proto, *fname;
    bool typechecked, ellipsis;
    int spread;

    /* get info, prepare return value */
    fname = call->l.string->text;
    func = call->r.right;
    if (func == (Node *) NULL) {
	/* error during function lookup */
	return Node::createMon(N_FAKE, T_MIXED, (Node *) NULL);
    }
    proto = func->l.ptr;
    if (func->mod == T_IMPLICIT) {
	func->mod = T_MIXED;
    }
    func->l.left = call;
    call->r.right = args;
    argv = &call->r.right;

# ifdef CLOSURES
    if (funcptr) {
	if (func->r.number >> 24 == KFCALL) {
	    error("cannot create pointer to kfun");
	}
	if (PROTO_CLASS(proto) & C_PRIVATE) {
	    error("cannot create pointer to private function");
	}
    }
# endif

    /*
     * check function arguments
     */
    typechecked = ((PROTO_CLASS(proto) & C_TYPECHECKED) != 0);
    ellipsis = ((PROTO_CLASS(proto) & C_ELLIPSIS) != 0);
    nargs = PROTO_NARGS(proto) + PROTO_VARGS(proto);
    argp = PROTO_ARGS(proto);
    for (n = 1; n <= nargs; n++) {
	if (args == (Node *) NULL) {
	    if (n <= PROTO_NARGS(proto) && !funcptr) {
		error("too few arguments for function %s", fname);
	    }
	    break;
	}
	if ((*argv)->type == N_PAIR) {
	    arg = &(*argv)->l.left;
	    argv = &(*argv)->r.right;
	} else {
	    arg = argv;
	    args = (Node *) NULL;
	}
	t = UCHAR(*argp);

	if ((*arg)->type == N_SPREAD) {
	    t = (*arg)->l.left->mod;
	    if (t != T_MIXED) {
		if ((t & T_REF) == 0) {
		    error("ellipsis requires array");
		    t = T_MIXED;
		} else {
		    t -= (1 << REFSHIFT);
		}
	    }

	    spread = n;
	    while (n <= nargs) {
		if (*argp == T_LVALUE) {
		    (*arg)->mod = n - spread;
		    break;
		}
		if (typechecked && matchType(t, *argp) == T_NIL) {
		    error("bad argument %d for function %s (needs %s)", n,
			  fname, Value::typeName(tnbuf, *argp));
		}
		n++;
		argp += ((*argp & T_TYPE) == T_CLASS) ? 4 : 1;
	    }
	    break;
	} else if (t == T_LVALUE) {
	    if (!lvalue(*arg)) {
		error("bad argument %d for function %s (needs lvalue)", n,
		      fname);
	    }
	    *arg = Node::createMon(N_LVALUE, (*arg)->mod, *arg);
	} else if ((typechecked || (*arg)->mod == T_VOID) &&
		   matchType((*arg)->mod, t) == T_NIL &&
		   (!nil(*arg) || !T_POINTER(t))) {
	    error("bad argument %d for function %s (needs %s)", n, fname,
		  Value::typeName(tnbuf, t));
	}

	if (n == nargs && ellipsis) {
	    nargs++;
	} else {
	    argp += ((*argp & T_TYPE) == T_CLASS) ? 4 : 1;
	}
    }
    if (args != (Node *) NULL && PROTO_FTYPE(proto) != T_IMPLICIT) {
	if (args->type == N_SPREAD) {
	    t = args->l.left->mod;
	    if (t != T_MIXED && (t & T_REF) == 0) {
		error("ellipsis requires array");
	    }
	} else {
	    error("too many arguments for function %s", fname);
	}
    }

    if ((func->r.number >> 24) == KFCALL &&
	proto[PROTO_SIZE(proto) - 1] == T_LVALUE) {
	/* kfuns can have lvalue parameters */
	func->r.number |= (long) KFCALL_LVAL << 24;
    }

    return func;
}

/*
 * handle a function call
 */
Node *Compile::funcall(Node *func, Node *args)
{
    return funcall(func, Node::revert(args), FALSE);
}

/*
 * handle ->
 */
Node *Compile::arrow(Node *other, Node *func, Node *args)
{
    if (args == (Node *) NULL) {
	args = func;
    } else {
	args = Node::createBin(N_PAIR, 0, func, Node::revert(args));
    }
    return funcall(flookup(Node::createStr(String::create("call_other", 10)),
			   FALSE),
		   Node::createBin(N_PAIR, 0, other, args), FALSE);
}

/*
 * handle &func()
 */
Node *Compile::address(Node *func, Node *args, int typechecked)
{
# ifdef CLOSURES
    args = Node::revert(args);
    funcall(flookup(func, typechecked), args, TRUE);	/* check only */

    if (args == (Node *) NULL) {
	args = func;
    } else {
	args = Node::createBin(N_PAIR, 0, func, args);
    }
    func = funcall(flookup(Node::createStr(String::create("new.function", 12)),
			   FALSE),
		   args, FALSE);
    func->mod = T_CLASS;
    func->sclass = String::create(BIPREFIX "function", BIPREFIXLEN + 8);
    func->sclass->ref();
    return func;
# else
    UNREFERENCED_PARAMETER(func);
    UNREFERENCED_PARAMETER(args);
    UNREFERENCED_PARAMETER(typechecked);
    error("syntax error");
    return Node::createMon(N_FAKE, T_MIXED, (Node *) NULL);
# endif
}

/*
 * handle &(*func)()
 */
Node *Compile::extend(Node *func, Node *args, int typechecked)
{
# ifdef CLOSURES
    if (typechecked && func->mod != T_MIXED) {
	if (func->mod != T_OBJECT &&
	    (func->mod != T_CLASS ||
	     strcmp(func->sclass->text, BIPREFIX "function") != 0)) {
	    error("bad argument 1 for function * (needs function)");
	}
    }
    if (args == (Node *) NULL) {
	args = func;
    } else {
	args = Node::createBin(N_PAIR, 0, func, Node::revert(args));
    }
    func = funcall(flookup(Node::createStr(String::create("extend.function",
							  15)),
			   FALSE),
		   args, FALSE);
    func->mod = T_CLASS;
    func->sclass = String::create(BIPREFIX "function", BIPREFIXLEN + 8);
    func->sclass->ref();
    return func;
# else
    UNREFERENCED_PARAMETER(func);
    UNREFERENCED_PARAMETER(args);
    UNREFERENCED_PARAMETER(typechecked);
    error("syntax error");
    return Node::createMon(N_FAKE, T_MIXED, (Node *) NULL);
# endif
}

/*
 * handle (*func)()
 */
Node *Compile::call(Node *func, Node *args, int typechecked)
{
# ifdef CLOSURES
    if (typechecked && func->mod != T_MIXED) {
	if (func->mod != T_OBJECT &&
	    (func->mod != T_CLASS ||
	     strcmp(func->sclass->text, BIPREFIX "function") != 0)) {
	    error("bad argument 1 for function * (needs function)");
	}
    }
    if (args == (Node *) NULL) {
	args = func;
    } else {
	args = Node::createBin(N_PAIR, 0, func, Node::revert(args));
    }
    return funcall(flookup(Node::createStr(String::create("call.function", 13)),
			   FALSE),
		   args, FALSE);
# else
    UNREFERENCED_PARAMETER(func);
    UNREFERENCED_PARAMETER(args);
    UNREFERENCED_PARAMETER(typechecked);
    error("syntax error");
    return Node::createMon(N_FAKE, T_MIXED, (Node *) NULL);
# endif
}

/*
 * handle new
 */
Node *Compile::newObject(Node *o, Node *args)
{
    if (args != (Node *) NULL) {
	args = Node::createBin(N_PAIR, 0, o, Node::revert(args));
    } else {
	args = o;
    }
    return funcall(flookup(Node::createStr(String::create("new_object", 10)),
			   FALSE),
		args, FALSE);
}

/*
 * handle <-
 */
Node *Compile::instanceOf(Node *n, Node *prog)
{
    String *str;

    if (n->mod != T_MIXED && n->mod != T_OBJECT && n->mod != T_CLASS) {
	error("bad argument 1 for function <- (needs object)");
    }
    str = objecttype(prog);
    prog->l.string->del();
    prog->l.string = str;
    prog->l.string->ref();
    return Node::createBin(N_INSTANCEOF, T_INT, n, prog);
}

/*
 * check return value of a system call
 */
Node *Compile::checkcall(Node *n, int typechecked)
{
    if (n->type == N_FUNC && (n->r.number >> 24) == FCALL) {
	if (typechecked) {
	    if (n->mod != T_MIXED && n->mod != T_VOID) {
		/*
		 * make sure the return value is as it should be
		 */
		n = Node::createMon(N_CAST, n->mod, n);
		n->sclass = n->l.left->sclass;
		if (n->sclass != (String *) NULL) {
		    n->sclass->ref();
		}
	    }
	} else {
	    /* could be anything */
	    n->mod = T_MIXED;
	}
    } else if (n->mod == T_VOID && !typechecked) {
	/* no void expressions */
	n->mod = T_INT;
    }

    return n;
}

/*
 * handle a condition
 */
Node *Compile::tst(Node *n)
{
    switch (n->type) {
    case N_INT:
	n->l.number = (n->l.number != 0);
	return n;

    case N_FLOAT:
	return Node::createInt(!NFLT_ISZERO(n));

    case N_STR:
	return Node::createInt(TRUE);

    case N_NIL:
	return Node::createInt(FALSE);

    case N_TST:
    case N_NOT:
    case N_LAND:
    case N_EQ:
    case N_EQ_INT:
    case N_NE:
    case N_NE_INT:
    case N_GT:
    case N_GT_INT:
    case N_GE:
    case N_GE_INT:
    case N_LT:
    case N_LT_INT:
    case N_LE:
    case N_LE_INT:
	return n;

    case N_COMMA:
	n->mod = T_INT;
	n->r.right = tst(n->r.right);
	return n;
    }

    return Node::createMon(N_TST, T_INT, n);
}

/*
 * handle a !condition
 */
Node *Compile::_not(Node *n)
{
    switch (n->type) {
    case N_INT:
	n->l.number = (n->l.number == 0);
	return n;

    case N_FLOAT:
	return Node::createInt(NFLT_ISZERO(n));

    case N_STR:
	return Node::createInt(FALSE);

    case N_NIL:
	return Node::createInt(TRUE);

    case N_LAND:
	n->type = N_LOR;
	n->l.left = _not(n->l.left);
	n->r.right = _not(n->r.right);
	return n;

    case N_LOR:
	n->type = N_LAND;
	n->l.left = _not(n->l.left);
	n->r.right = _not(n->r.right);
	return n;

    case N_TST:
	n->type = N_NOT;
	return n;

    case N_NOT:
	n->type = N_TST;
	return n;

    case N_EQ:
	n->type = N_NE;
	return n;

    case N_EQ_INT:
	n->type = N_NE_INT;
	return n;

    case N_NE:
	n->type = N_EQ;
	return n;

    case N_NE_INT:
	n->type = N_EQ_INT;
	return n;

    case N_GT:
	n->type = N_LE;
	return n;

    case N_GT_INT:
	n->type = N_LE_INT;
	return n;

    case N_GE:
	n->type = N_LT;
	return n;

    case N_GE_INT:
	n->type = N_LT_INT;
	return n;

    case N_LT:
	n->type = N_GE;
	return n;

    case N_LT_INT:
	n->type = N_GE_INT;
	return n;

    case N_LE:
	n->type = N_GT;
	return n;

    case N_LE_INT:
	n->type = N_GT_INT;
	return n;

    case N_COMMA:
	n->mod = T_INT;
	n->r.right = _not(n->r.right);
	return n;
    }

    return Node::createMon(N_NOT, T_INT, n);
}

/*
 * handle an lvalue
 */
Node *Compile::lvalue(Node *n, const char *oper)
{
    if (!lvalue(n)) {
	error("bad lvalue for %s", oper);
	return Node::createMon(N_FAKE, T_MIXED, n);
    }
    return n;
}

/*
 * check an aggregate of lvalues
 */
void Compile::lvalAggr(Node **n)
{
    Node **m;

    if (*n == (Node *) NULL) {
      error("no lvalues in aggregate");
    } else {
      while (n != (Node **) NULL) {
	  if ((*n)->type == N_PAIR) {
	      m = &(*n)->l.left;
	      n = &(*n)->r.right;
	  } else {
	      m = n;
	      n = (Node **) NULL;
	  }
	  if (!lvalue(*m)) {
	      error("bad lvalue in aggregate");
	      *m = Node::createMon(N_FAKE, T_MIXED, *m);
	  }
	  if ((*m)->type == N_LOCAL && !BTST(thiscond->init, (*m)->r.number)) {
	      BSET(thiscond->init, (*m)->r.number);
	      --variables[(*m)->r.number].unset;
	  }
      }
    }
}

/*
 * handle an assignment
 */
Node *Compile::assign(Node *n)
{
    if (n->type == N_AGGR) {
	lvalAggr(&n->l.left);
    } else {
	n = lvalue(n, "assignment");
	if (n->type == N_LOCAL && !BTST(thiscond->init, n->r.number)) {
	    BSET(thiscond->init, n->r.number);
	    --variables[n->r.number].unset;
	}
    }
    return n;
}

/*
 * See if the two supplied types are compatible. If so, return the
 * combined type. If not, return T_NIL.
 */
unsigned short Compile::matchType(unsigned int type1, unsigned int type2)
{
    if (type1 == T_NIL || type2 == T_NIL) {
	/* nil doesn't match with anything else */
	return T_NIL;
    }
    if (type1 == type2) {
	return type1;	/* identical types (including T_CLASS) */
    }
    if ((type1 & T_TYPE) == T_CLASS) {
	type1 = (type1 & T_REF) | T_OBJECT;
    }
    if ((type2 & T_TYPE) == T_CLASS) {
	type2 = (type2 & T_REF) | T_OBJECT;
    }
    if (type1 == type2) {
	return type1;	/* identical types (excluding T_CLASS) */
    }
    if (type1 == T_VOID || type2 == T_VOID) {
	/* void doesn't match with anything else, not even with mixed */
	return T_NIL;
    }
    if ((type1 & T_TYPE) == T_MIXED && (type1 & T_REF) <= (type2 & T_REF)) {
	/* mixed <-> int,  mixed * <-> int *,  mixed * <-> int ** */
	if (type1 == T_MIXED && (type2 & T_REF) != 0) {
	    type1 |= 1 << REFSHIFT;	/* mixed <-> int * */
	}
	return type1;
    }
    if ((type2 & T_TYPE) == T_MIXED && (type2 & T_REF) <= (type1 & T_REF)) {
	/* int <-> mixed,  int * <-> mixed *,  int ** <-> mixed * */
	if (type2 == T_MIXED && (type1 & T_REF) != 0) {
	    type2 |= 1 << REFSHIFT;	/* int * <-> mixed */
	}
	return type2;
    }
    return T_NIL;
}

/*
 * Forward the error to the preprocessor error handler.
 */
void Compile::error(const char *format, ...)
{
    va_list args;
    char buf[3 * STRINGSZ];	/* 2 * string + overhead */

    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    PP->error("%s", buf);
}


class PreprocImpl : public Preproc {
public:
    /*
     * Call the driver object with the supplied error message.
     */
    virtual void error(const char *format, ...) {
	va_list args;
	char buf[3 * STRINGSZ];		/* 2 * string + overhead */

	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if (driver_object != (char *) NULL &&
	    Object::find(driver_object, OACC_READ) != (Object *) NULL) {
	    Frame *f;
	    char *fname;

	    f = current->frame;
	    fname = PP->filename();
	    PUSH_STRVAL(f, String::create(fname, strlen(fname)));
	    PUSH_INTVAL(f, PP->line());
	    PUSH_STRVAL(f, String::create(buf, strlen(buf)));

	    DGD::callDriver(f, "compile_error", 3);
	    (f->sp++)->del();
	} else {
	    /* there is no driver object to call; fall back to the default */
	    Preproc::error("%s", buf);
	}

	nerrors++;
    }
};

static PreprocImpl PPI;
Preproc *PP = &PPI;
