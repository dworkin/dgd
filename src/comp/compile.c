# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "path.h"
# include "fcontrol.h"
# include "macro.h"
# include "token.h"
# include "ppcontrol.h"
# include "node.h"
# include "control.h"
# include "optimize.h"
# include "codegen.h"
# include "compile.h"

# define BLOCK_CHUNK	16

typedef struct _block_ {
    int vindex;			/* variable index */
    struct _block_ *prev;	/* surrounding block */
} block;

typedef struct _bchunk_ {
    block b[BLOCK_CHUNK];	/* chunk of blocks */
    struct _bchunk_ *next;	/* next in block chunk list */
} bchunk;

typedef struct {
    char *name;			/* variable name */
    short type;			/* variable type */
} var;

static bchunk *blist;			/* list of all block chunks */
static block *fblist;			/* list of free statement blocks */
static block *thisblock;		/* current statement block */
static int bchunksz = BLOCK_CHUNK;	/* size of current block chunk */
static int nvars;			/* number of local variables */
static int nparams;			/* number of parameters */
static var variables[MAX_LOCALS];	/* variables */

/*
 * NAME:	block->new()
 * DESCRIPTION:	start a new block
 */
static void block_new()
{
    register block *b;

    if (fblist != (block *) NULL) {
	b = fblist;
	fblist = b->prev;
    } else {
	if (bchunksz == BLOCK_CHUNK) {
	    register bchunk *l;

	    l = ALLOC(bchunk, 1);
	    l->next = blist;
	    blist = l;
	    bchunksz = 0;
	}
	b = &blist->b[bchunksz++];
    }
    b->vindex = (thisblock == (block *) NULL) ? 0 : nvars;
    b->prev = thisblock;
    thisblock = b;
}

/*
 * NAME:	block->del()
 * DESCRIPTION:	finish the current block
 */
static void block_del()
{
    register block *f;
    register int i;

    f = thisblock;
    for (i = f->vindex; i < nvars; i++) {
	/*
	 * Make sure that variables declared in the closing block can no
	 * longer be used.
	 */
	variables[i].name = "-";
    }
    thisblock = f->prev;
    f->prev = fblist;
    fblist = f;
}

/*
 * NAME:	block->var()
 * DESCRIPTION:	return the index of the local var if found, or MAX_LOCALS
 */
static int block_var(name)
char *name;
{
    register int i;

    for (i = nvars; i > 0; ) {
	if (strcmp(variables[--i].name, name) == 0) {
	    return i;
	}
    }
    return -1;
}

/*
 * NAME:	block->pdef()
 * DESCRIPTION:	declare a function parameter
 */
static void block_pdef(name, type)
char *name;
short type;
{
    if (block_var(name) >= 0) {
	c_error("redeclaration of parameter %s", name);
    } else {
	/* "too many parameters" is checked for elsewhere */
	variables[nparams].name = name;
	variables[nparams++].type = type;
	nvars++;
    }
}

/*
 * NAME:	block->vdef()
 * DESCRIPTION:	declare a local variable
 */
static void block_vdef(name, type)
char *name;
short type;
{
    if (block_var(name) >= thisblock->vindex) {
	c_error("redeclaration of local variable %s", name);
    } else if (nvars == MAX_LOCALS) {
	c_error("too many local variables");
    } else {
	variables[nvars].name = name;
	variables[nvars++].type = type;
    }
}

/*
 * NAME:	block->clear()
 * DESCRIPTION:	clean up blocks
 */
static void block_clear()
{
    register bchunk *l;

    for (l = blist; l != (bchunk *) NULL; ) {
	register bchunk *f;

	f = l;
	l = l->next;
	FREE(f);
    }
    blist = (bchunk *) NULL;
    bchunksz = BLOCK_CHUNK;
    fblist = (block *) NULL;
    thisblock = (block *) NULL;
    nvars = 0;
    nparams = 0;
}


# define LOOP_CHUNK	16

typedef struct _loop_ {
    char type;			/* case label type */
    bool brk;			/* seen any breaks? */
    bool cont;			/* seen any continues? */
    bool dflt;			/* seen any default labels? */
    short ncase;		/* number of case labels */
    node *case_list;		/* previous list of case nodes */
    struct _loop_ *prev;	/* previous loop or switch */
    struct _loop_ *env;		/* enclosing loop */
} loop;

typedef struct _lchunk_ {
    loop l[LOOP_CHUNK];		/* chunk of loops */
    struct _lchunk_ *next;	/* next in loop chunk list */
} lchunk;

static lchunk *llist;		/* list of all loop chunks */
static loop *fllist;		/* list of free loops */
static int lchunksz = LOOP_CHUNK; /* size of current loop chunk */

/*
 * NAME:	loop->new()
 * DESCRIPTION:	create a new loop
 */
static loop *loop_new(prev)
loop *prev;
{
    register loop *l;

    if (fllist != (loop *) NULL) {
	l = fllist;
	fllist = l->prev;
    } else {
	if (lchunksz == LOOP_CHUNK) {
	    register lchunk *l;

	    l = ALLOC(lchunk, 1);
	    l->next = llist;
	    llist = l;
	    lchunksz = 0;
	}
	l = &llist->l[lchunksz++];
    }
    l->brk = FALSE;
    l->cont = FALSE;
    l->prev = prev;
    return l;
}

/*
 * NAME:	loop->del()
 * DESCRIPTION:	delete a loop
 */
static loop *loop_del(l)
register loop *l;
{
    register loop *f;

    f = l;
    l = l->prev;
    f->prev = fllist;
    fllist = f;
    return l;
}

/*
 * NAME:	loop->clear()
 * DESCRIPTION:	delete all loops
 */
static void loop_clear()
{
    register lchunk *l;

    for (l = llist; l != (lchunk *) NULL; ) {
	register lchunk *f;

	f = l;
	l = l->next;
	FREE(f);
    }
    llist = (lchunk *) NULL;
    lchunksz = LOOP_CHUNK;
    fllist = (loop *) NULL;
}


typedef struct _context_ {
    char file[STRINGSZ];		/* file to compile */
    char inherit[STRINGSZ];		/* file to inherit */
    struct _context_ *prev;		/* previous context */
} context;

static context *current;		/* current context */
static char *auto_object;		/* auto object */
static char *driver_object;		/* driver object */
static char *include;			/* standard include file */
static char **paths;			/* include paths */
static bool typechecking;		/* is current function typechecked? */
static bool seen_decls;			/* seen any declarations yet? */
static short ftype;			/* current function type & class */
static loop *thisloop;			/* current loop */
static loop *switch_list;		/* list of nested switches */
static node *case_list;			/* list of case labels */

/*
 * NAME:	compile->init()
 * DESCRIPTION:	initialize the compiler
 */
void c_init(a, d, i, p, flag)
char *a, *d, *i, **p;
bool flag;
{
    auto_object = a;
    driver_object = d;
    include = i;
    paths = p;
    typechecking = flag | cg_compiled();
}

/*
 * NAME:	compile->clear()
 * DESCRIPTION:	clean up the compiler
 */
static void c_clear()
{
    cg_clear();
    loop_clear();
    thisloop = (loop *) NULL;
    switch_list = (loop *) NULL;
    block_clear();
    node_clear();
    seen_decls = FALSE;
}

/*
 * NAME:	compile->inherit()
 * DESCRIPTION:	Inherit an object in the object currently being compiled.
 *		Return TRUE if the object is loaded or if an error occurred,
 *		FALSE otherwise.
 */
bool c_inherit(file, label)
register char *file;
node *label;
{
    register context *c;
    register object *o;

    if (strcmp(current->file, auto_object) == 0) {
	c_error("cannot inherit from auto object");
	return TRUE;
    }
    if (file != auto_object) {
	file = path_inherit(current->file, file);
	if (file == (char *) NULL || file[0] == '\0') {
	    c_error("illegal inherit path");
	    return TRUE;
	}
	if (strcmp(file, driver_object) == 0) {
	    /* would mess up too many things */
	    c_error("illegal to inherit driver object");
	    return TRUE;
	}
	if (strcmp(current->file, driver_object) == 0 &&
	    strcmp(file, auto_object) != 0) {
	    /* driver object can only inherit the auto object */
	    c_error("illegal inherit from driver object");
	}
	for (c = current; c != (context *) NULL; c = c->prev) {
	    if (strcmp(file, c->file) == 0) {
		c_error("cycle in inheritance");
		return TRUE;
	    }
	}
    }

    o = o_find(file);
    if (o == (object *) NULL || !(o->flags & O_MASTER) ||
	!ctrl_inherit(o, (label == (node *) NULL) ?
			  (string *) NULL : label->l.string)) {
	/* object is unloaded */
	strncpy(current->inherit, file, STRINGSZ);
	current->inherit[STRINGSZ - 1] = '\0';
	return FALSE;
    }

    return TRUE;
}

/*
 * NAME:	compile()
 * DESCRIPTION:	compile an LPC file
 */
static object *compile(file)
register char *file;
{
    static bool recursion;
    context c;
    char file_c[STRINGSZ + 2];
    char errlog[STRINGSZ];
    extern int yyparse P((void));

    if (recursion) {
	error("Compilation within compilation");
    }

    strcpy(c.file, file);
    c.prev = current;
    current = &c;

    if (strchr(c.file, '#') != (char *) NULL ||
	(file=path_file(c.file)) == (char *) NULL) {
	error("Illegal file name \"/%s\"", c.file);
    }
    strcpy(file_c, file);
    strcat(file_c, ".c");

    for (;;) {
	if (c_autodriver() != 0) {
	    ctrl_init(auto_object, driver_object);
	} else {
	    if (!cg_compiled() && o_find(driver_object) == (object *) NULL) {
		/*
		 * (re)compile the driver object to do pathname translation
		 */
		current = (context *) NULL;
		compile(driver_object);
		current = &c;
	    }
	    ctrl_init(auto_object, driver_object);
	    if (!c_inherit(auto_object, (node *) NULL)) {
		/*
		 * (re)compile auto object and inherit it
		 */
		ctrl_clear();
		compile(auto_object);
		ctrl_init(auto_object, driver_object);
		c_inherit(auto_object, (node *) NULL);
	    }
	}

	if (!pp_init(file_c, paths, 1)) {
	    ctrl_clear();
	    if (c_autodriver() == 0) {
		/*
		 * Object can't be loaded.  Ask the driver object for
		 * a replacement.
		 */
		i_check_stack(1);
		(--sp)->type = T_STRING;
		str_ref(sp->u.string = str_new((char *) NULL,
					       strlen(c.file) + 1L));
		sp->u.string->text[0] = '/';
		strcpy(sp->u.string->text + 1, c.file);
		call_driver_object("compile_object", 1);
		if (sp->type == T_OBJECT) {
		    register object *o;

		    o = o_object(sp->oindex, sp->u.objcnt);
		    sp++;
		    if ((o->flags & O_AUTO) ||
			strcmp(c.file, auto_object) == 0) {
			error("Illegal rename of auto object");
		    }
		    if ((o->flags & O_DRIVER) ||
			strcmp(c.file, driver_object) == 0) {
			error("Illegal rename of driver object");
		    }
		    /*
		     * Driver object supplied alternative.  Rename it and
		     * return it.
		     */
		    o_rename(o, c.file);
		    return o;
		}
		/*
		 * not an alternative
		 */
		i_del_value(sp++);
	    }
	    error("Could not compile \"/%s.c\"", c.file);
	}
	if (!tk_include(include)) {
	    pp_clear();
	    ctrl_clear();
	    error("Could not include \"/%s\"", include);
	}

	cg_init(c.prev != (context *) NULL);
	if (ec_push()) {
	    c_error("error while compiling:");
	    recursion = FALSE;
	    errorlog((char *) NULL);
	    pp_clear();
	    ctrl_clear();
	    c_clear();
	    error((char *) NULL);
	}
	if (c_autodriver() == 0) {
	    /*
	     * compile time error logging
	     */
	    i_check_stack(1);
	    (--sp)->type = T_STRING;
	    str_ref(sp->u.string = str_new((char *) NULL, strlen(c.file) + 1L));
	    sp->u.string->text[0] = '/';
	    strcpy(sp->u.string->text + 1, c.file);
	    call_driver_object("compile_log", 1);
	    if (sp->type == T_STRING) {
		file = path_file(path_resolve(sp->u.string->text));
		if (file != (char *) NULL) {
		    /* start logging errors */
		    errorlog(strcpy(errlog, file));
		}
	    }
	    i_del_value(sp++);
	}

	c.inherit[0] = '\0';
	recursion = TRUE;
	if (yyparse() == 0 && ctrl_chkfuncs(c.file)) {
	    control *ctrl;

	    recursion = FALSE;
	    errorlog((char *) NULL);
	    ec_pop();
	    pp_clear();

	    if (!seen_decls) {
		/*
		 * object with inherit statements only (or nothing at all)
		 */
		ctrl_create(c.file);
	    }
	    ctrl = ctrl_construct();
	    ctrl_clear();
	    c_clear();
	    current = c.prev;
	    return ctrl->inherits[ctrl->ninherits - 1].obj;
	} else {
	    recursion = FALSE;
	    errorlog((char *) NULL);
	    ec_pop();
	    pp_clear();
	    ctrl_clear();
	    c_clear();

	    if (c.inherit[0] == '\0') {
		error("Failed to compile \"/%s.c\"", c.file);
	    }
	    compile(c.inherit);
	}
    }
}

/*
 * NAME:	compile->compile()
 * DESCRIPTION:	compile an LPC file
 */
object *c_compile(file)
char *file;
{
    current = (context *) NULL;	/* initial context */
    return compile(file);
}

/*
 * NAME:	compile->autodriver()
 * DESCRIPTION:	indicate if the auto object or driver object is being
 *		compiled
 */
int c_autodriver()
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
 * NAME:	revert_list()
 * DESCRIPTION:	revert a "linked list" of nodes
 */
static node *revert_list(n)
register node *n;
{
    register node *m;

    if (n != (node *) NULL && n->type == N_PAIR) {
	while ((m=n->l.left)->type == N_PAIR) {
	    /*
	     * ((a, b), c) -> (a, (b, c))
	     */
	    n->l.left = m->r.right;
	    m->r.right = n;
	    n = m;
	}
    }
    return n;
}

/*
 * NAME:	compile->decl_func()
 * ACTION:	declare a function
 */
static void c_decl_func(class, type, str, formals, function)
unsigned short class, type;
string *str;
register node *formals;
bool function;
{
    char proto[3 + MAX_LOCALS];
    bool typechecked;
    register char *args;
    register int nargs;

    /* check for some errors */
    if (strcmp(str->text, "catch") == 0 || strcmp(str->text, "lock") == 0) {
	c_error("cannot redeclare %s()", str->text);
    }
    if ((class & (C_PRIVATE | C_NOMASK)) == (C_PRIVATE | C_NOMASK)) {
	c_error("private contradicts nomask");
    }
    if ((type & T_TYPE) == T_INVALID) {
	typechecked = FALSE;
	type = T_MIXED;
	if (typechecking) {
	    c_error("missing type for function %s", str->text);
	}
    } else {
	typechecked = TRUE;
	if (type != T_VOID && (type & T_TYPE) == T_VOID) {
	    c_error("invalid type for function %s (%s)", str->text,
		    i_typename(type));
	    type = T_MIXED;
	}
    }

    /* handle function class and return type */
    if (class & C_PRIVATE) {
	class |= C_LOCAL;	/* private implies local */
    }
    if (typechecked) {
	class |= C_TYPECHECKED;
    }
    PROTO_CLASS(proto) = class;
    PROTO_FTYPE(proto) = type;

    /* handle function arguments */
    args = PROTO_ARGS(proto);
    nargs = 0;
    formals = revert_list(formals);
    while (formals != (node *) NULL) {
	register node *arg;
	register unsigned short t;

	if (nargs == MAX_LOCALS) {
	    c_error("too many parameters in function %s", str->text);
	    break;
	}
	if (formals->type == N_PAIR) {
	    arg = formals->l.left;
	    formals = formals->r.right;
	} else {
	    arg = formals;
	    formals = (node *) NULL;
	}
	t = arg->mod;
	if ((t & T_TYPE) == T_INVALID) {
	    if (typechecked) {
		c_error("missing type for parameter %s", arg->l.string->text);
	    }
	    t = T_MIXED | (t & T_ELLIPSIS);
	} else if ((t & T_TYPE) == T_VOID) {
	    c_error("invalid type for parameter %s (%s)", arg->l.string->text,
		    i_typename(t & ~T_ELLIPSIS));
	    t = T_MIXED | (t & T_ELLIPSIS);
	}
	*args++ = t;
	nargs++;
	if (t & T_ELLIPSIS) {
	    if (!(class & C_VARARGS)) {
		c_error("ellipsis without varargs");
	    }
	    t = (t & ~T_ELLIPSIS) + (1 << REFSHIFT);
	    if ((t & T_REF) == 0) {
		t |= T_REF;
	    }
	}

	if (function) {
	    block_pdef(arg->l.string->text, t);
	}
    }
    PROTO_NARGS(proto) = nargs;

    /* define prototype */
    if (function) {
	if (cg_compiled()) {
	    /* LPC compiled to C */
	    PROTO_CLASS(proto) |= C_COMPILED;
	}
	ftype = type;
	ctrl_dfunc(str, proto);
    } else {
	PROTO_CLASS(proto) |= C_UNDEFINED;
	ctrl_dproto(str, proto);
    }
}

/*
 * NAME:	compile->decl_var()
 * DESCRIPTION:	declare a variable
 */
static void c_decl_var(class, type, str, global)
unsigned short class, type;
string *str;
bool global;
{
    if ((type & T_TYPE) == T_VOID) {
	c_error("invalid type for variable %s (%s)", str->text,
		i_typename(type));
	type = T_MIXED;
    }
    if (global) {
	if (class & C_NOMASK) {
	    c_error("invalid class for variable %s", str->text);
	}
	ctrl_dvar(str, class, type);
    } else {
	if (class != 0) {
	    c_error("invalid class for variable %s", str->text);
	}
	block_vdef(str->text, type);
    }
}

/*
 * NAME:	compile->decl_list()
 * DESCRIPTION:	handle a list of declarations
 */
static void c_decl_list(class, type, list, global)
register unsigned short class, type;
register node *list;
bool global;
{
    register node *n;

    list = revert_list(list);	/* for proper order of err mesgs */
    while (list != (node *) NULL) {
	if (list->type == N_PAIR) {
	    n = list->l.left;
	    list = list->r.right;
	} else {
	    n = list;
	    list = (node *) NULL;
	}
	if (n->type == N_FUNC) {
	    c_decl_func(class, type | n->mod, n->l.left->l.string, n->r.right,
			FALSE);
	} else {
	    c_decl_var(class, type | n->mod, n->l.string, global);
	}
    }
}

/*
 * NAME:	compile->global()
 * DESCRIPTION:	handle a global declaration
 */
void c_global(class, type, n)
unsigned short class, type;
node *n;
{
    if (!seen_decls) {
	ctrl_create(current->file);
	seen_decls = TRUE;
    }
    c_decl_list(class, type, n, TRUE);
}

static string *fname;		/* name of current function */
static unsigned short fline;	/* first line of function */

/*
 * NAME:	compile->function()
 * DESCRIPTION:	create a function
 */
void c_function(class, type, n)
unsigned short class, type;
register node *n;
{
    if (!seen_decls) {
	ctrl_create(current->file);
	seen_decls = TRUE;
    }
    if (class & C_NOMASK) {
	class |= C_LOCAL;	/* nomask implies local */
    }
    c_decl_func(class, type | n->mod, fname = n->l.left->l.string, n->r.right,
		TRUE);
}

/*
 * NAME:	compile->funcbody()
 * DESCRIPTION:	create a function body
 */
void c_funcbody(n)
register node *n;
{
    register unsigned short i;
    register node *v, *zero;
    char *prog;
    unsigned short size;

    if (n == (node *) NULL || !(n->flags & F_RETURN)) {
	n = c_concat(n, node_mon(N_RETURN, 0, node_int((Int) 0)));
    }

    /*
     * initialize local floats to 0.0
     */
    zero = (node *) NULL;
    for (i = nvars; i > nparams; ) {
	if (variables[--i].type == T_FLOAT) {
	    v = node_mon(N_LOCAL, T_FLOAT, (node *) NULL);
	    v->line = fline;
	    v->r.number = i;
	    if (zero == (node *) NULL) {
		xfloat flt;

		FLT_ZERO(flt.high, flt.low);
		zero = node_float(&flt);
		zero->line = fline;
	    }
	    zero = node_bin(N_ASSIGN, T_FLOAT, v, zero);
	    zero->line = fline;
	}
    }
    if (zero != (node *) NULL) {
	n = c_concat(c_exp_stmt(zero), n);
    }

    opt_stmt(n, &size);
    prog = cg_function(fname, n, nvars, nparams, size, &size);
    ctrl_dprogram(prog, size);
    node_free();
    nvars = 0;
    nparams = 0;
}

/*
 * NAME:	compile->local()
 * DESCRIPTION:	handle local declarations
 */
void c_local(class, type, n)
unsigned short class, type;
node *n;
{
    c_decl_list(class, type, n, FALSE);
}


/*
 * NAME:	compile->zero()
 * DESCRIPTION:	check if an expression has the value integer 0
 */
bool c_zero(n)
register node *n;
{
    if (n->type == N_COMMA) {
	n = n->r.right;
    }
    return (n->type == N_INT && n->l.number == 0);
}

/*
 * NAME:	compile->concat()
 * DESCRIPTION:	concatenate two statements
 */
node *c_concat(n1, n2)
register node *n1, *n2;
{
    node *n;

    if (n1 == (node *) NULL) {
	return n2;
    } else if (n2 == (node *) NULL ||
	       ((n1->flags & (F_BREAK | F_CONT | F_RETURN)) &&
	        !(n2->flags & F_ENTRY))) {
	return n1;
    }

    n = node_bin(N_PAIR, 0, n1, n2);
    n->flags |= (n1->flags | n2->flags) & F_REACH;
    return n;
}

/*
 * NAME:	compile->exp_stmt()
 * DESCRIPTION:	reduce an expression to a statement
 */
node *c_exp_stmt(n)
node *n;
{
    if (n != (node *) NULL) {
	return node_mon(N_POP, 0, n);
    }
    return n;
}

/*
 * NAME:	compile->if()
 * DESCRIPTION:	handle an if statement
 */
node *c_if(n1, n2, n3)
register node *n1, *n2, *n3;
{
    register int flags1, flags2;

    n1 = node_bin(N_IF, 0, n1, node_bin(N_ELSE, 0, n2, n3));
    if (n2 != (node *) NULL) {
	flags1 = n2->flags & (F_BREAK | F_CONT | F_RETURN);
	n1->flags |= n2->flags & F_REACH;
    } else {
	flags1 = 0;
    }
    if (n3 != (node *) NULL) {
	flags2 = n3->flags & (F_BREAK | F_CONT | F_RETURN);
	n1->flags |= n3->flags & F_REACH;
    } else {
	flags2 = 0;
    }

    n1->flags |= (flags1 < flags2) ? flags1 : flags2;
    return n1;
}

/*
 * NAME:	compile->loop()
 * DESCRIPTION:	start a loop
 */
void c_loop()
{
    thisloop = loop_new(thisloop);
}

/*
 * NAME:	compile->reloop()
 * DESCRIPTION:	loop back a loop
 */
static node *c_reloop(n)
node *n;
{
    if (thisloop->cont) {
	n = node_mon(N_BLOCK, N_CONTINUE, n);
    }
    return n;
}

/*
 * NAME:	compile->endloop()
 * DESCRIPTION:	end a loop
 */
static node *c_endloop(n)
node *n;
{
    if (thisloop->brk) {
	n = node_mon(N_BLOCK, N_BREAK, n);
    }
    thisloop = loop_del(thisloop);
    return n;
}

/*
 * NAME:	compile->do()
 * DESCRIPTION:	end a do-while loop
 */
node *c_do(n1, n2)
register node *n1, *n2;
{
    n1 = c_endloop(node_bin(N_DO, 0, n1, c_reloop(n2)));
    if (n2 != (node *) NULL) {
	n1->flags |= n2->flags & (F_ENTRY | F_REACH);
    }
    return n1;
}

/*
 * NAME:	compile->while()
 * DESCRIPTION:	end a while loop
 */
node *c_while(n1, n2)
register node *n1, *n2;
{
    n1 = c_endloop(node_bin(N_FOR, 0, n1, c_reloop(n2)));
    if (n2 != (node *) NULL) {
	n1->flags |= n2->flags & F_REACH;
    }
    return n1;
}

/*
 * NAME:	compile->for()
 * DESCRIPTION:	end a for loop
 */
node *c_for(n1, n2, n3, n4)
register node *n2, *n4;
node *n1, *n3;
{
    if (n4 != (node *) NULL) {
	n4 = c_reloop(n4);
	if (n4->type == N_BLOCK) {
	    n4->flags |= n4->l.left->flags & (F_REACH | F_BREAK | F_RETURN);
	}
    }
    n2 = c_concat(n1,
		  c_endloop(node_bin((n2 == (node *) NULL) ? N_FOREVER : N_FOR,
			    0, n2, c_concat(n4, n3))));
    if (n4 != (node *) NULL) {
	n2->flags = n4->flags & F_REACH;
    }
    return n2;
}

/*
 * NAME:	compile->startswitch()
 * DESCRIPTION:	start a switch statement
 */
void c_startswitch(n, typechecking)
register node *n;
bool typechecking;
{
    switch_list = loop_new(switch_list);
    switch_list->type = T_MIXED;
    if (typechecking &&
	n->mod != T_INT && n->mod != T_STRING && n->mod != T_MIXED) {
	c_error("bad switch expression type (%s)", i_typename(n->mod));
	switch_list->type = T_INVALID;
    }
    switch_list->dflt = FALSE;
    switch_list->ncase = 0;
    switch_list->case_list = case_list;
    case_list = (node *) NULL;
    switch_list->env = thisloop;
}

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two case label nodes
 */
static int cmp(n1, n2)
register node **n1, **n2;
{
    if (n1[0]->l.left->type == N_STR) {
	if (n2[0]->l.left->type == N_STR) {
	    return strcmp(n1[0]->l.left->l.string->text,
			  n2[0]->l.left->l.string->text);
	} else {
	    return 1;	/* str > 0 */
	}
    } else if (n2[0]->l.left->type == N_STR) {
	return -1;	/* 0 < str */
    } else {
	return (n1[0]->l.left->l.number <= n2[0]->l.left->l.number) ? -1 : 1;
    }
}

/*
 * NAME:	compile->endswitch()
 * DESCRIPTION:	end a switch
 */
node *c_endswitch(expr, stmt)
node *expr, *stmt;
{
    register node **v, **w, *n;
    register unsigned short i, size;
    register long l;
    register unsigned long cnt;
    short type, sz;

    n = (node *) NULL;
    if (switch_list->type != T_INVALID) {
	if (stmt == (node *) NULL) {
	    /* empty switch statement */
	    n = c_exp_stmt(expr);
	} else if (!(stmt->flags & F_ENTRY)) {
	    c_error("unreachable statement(s) in switch");
	} else if ((size=switch_list->ncase - switch_list->dflt) == 0) {
	    /* only a default label */
	    n = c_concat(c_exp_stmt(expr), stmt->l.left);
	} else if (expr->mod != T_MIXED && expr->mod != switch_list->type &&
		   switch_list->type != T_MIXED) {
	    c_error("wrong switch expression type (%s)", i_typename(expr->mod));
	} else {
	    /*
	     * get the labels in an array, and sort them
	     */
	    v = ALLOCA(node*, size);
	    for (i = size, n = case_list; i > 0; n = n->l.left) {
		if (n->r.right->l.left != (node *) NULL) {
		    *v++ = n->r.right;
		    --i;
		}
	    }
	    qsort(v -= size, size, sizeof(node *), cmp);

	    if (switch_list->type == T_STRING) {
		type = N_SWITCH_STR;
		/*
		 * check for duplicate cases
		 */
		if (size >= 2 && v[1]->l.left->type == N_INT) {
		    c_error("duplicate case labels in switch");
		} else {
		    i = (v[0]->l.left->type == N_INT);
		    for (w = v + i, i = size - i - 1; i > 0; w++, --i) {
			if (strcmp(w[0]->l.left->l.string->text,
				   w[1]->l.left->l.string->text) == 0) {
			    c_error("duplicate case labels in switch");
			    break;
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
		cnt = 0;
		w = v;
		for (;;) {
		    cnt += w[0]->l.left->r.number - w[0]->l.left->l.number + 1;
		    if (--i == 0) {
			break;
		    }
		    if (w[0]->l.left->r.number >= w[1]->l.left->l.number) {
			if (w[0]->l.left->l.number == w[1]->l.left->r.number) {
			    c_error("duplicate case labels in switch");
			} else {
			    c_error("overlapping case label ranges in switch");
			}
			break;
		    }
		    w++;
		}

		/* determine the number of bytes per case */
		l = v[0]->l.left->l.number;
		if (l < 0) {
		    l = 1 - l;
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
		} else {
		    sz = 4;
		}

		if (i == 0 && cnt > size) {
		    if (cnt > ULONG_MAX / 6L ||
			(sz + 2L) * cnt > (2 * sz + 2L) * size) {
			/*
			 * no point in changing the type of switch
			 */
			type = N_SWITCH_RANGE;
		    } else {
			/*
			 * convert range label switch to int label switch
			 * by adding new labels
			 */
			w = ALLOCA(node*, cnt);
			for (i = size; i > 0; --i) {
			    *w++ = *v;
			    for (l = v[0]->l.left->l.number;
				 l < v[0]->l.left->r.number; ) {
				/* insert N_CASE in statement */
				n = node_mon(N_CASE, 0, v[0]->r.right->l.left);
				v[0]->r.right->l.left = n;
				l++;
				*w++ = node_bin(N_PAIR, 0, node_int((Int)l), n);
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
	    n = (node *) NULL;
	    i = size;
	    do {
		(*--v)->r.right->mod = i;
		n = node_bin(N_PAIR, 0, v[0]->l.left, n);
	    } while (--i > 0);
	    AFREE(v);
	    if (switch_list->dflt) {
		/* add default case */
		n = node_bin(N_PAIR, 0, (node *) NULL, n);
		size++;
	    }

	    if (switch_list->brk) {
		stmt = node_mon(N_BLOCK, N_BREAK, stmt);
	    }
	    n = node_bin(type, size, n, node_bin(N_PAIR, sz, expr, stmt));
	}
    }

    case_list = switch_list->case_list;
    switch_list = loop_del(switch_list);

    return n;
}

/*
 * NAME:	compile->case()
 * DESCRIPTION:	handle a case label
 */
node *c_case(n1, n2)
register node *n1, *n2;
{
    if (switch_list == (loop *) NULL) {
	c_error("case label not inside switch");
	return (node *) NULL;
    }
    if (switch_list->type == T_INVALID) {
	return (node *) NULL;
    }

    if (n1->type == N_INT) {
	/* int */
	if (n2 == (node *) NULL) {
	    n1->r.number = n1->l.number;
	} else {
	    /* range */
	    if (n2->type != N_INT) {
		c_error("bad case range");
		switch_list->type = T_INVALID;
		return (node *) NULL;
	    }
	    if (n2->l.number < n1->l.number) {
		/* inverted range */
		n1->r.number = n1->l.number;
		n1->l.number = n2->l.number;
		n1->type = N_RANGE;
	    } else if (n2->l.number != n1->l.number) {
		n1->r.number = n2->l.number;
		n1->type = N_RANGE;
	    }
	}
	/* compare type with other cases */
	if (n1->l.number != 0 || n2 != (node *) NULL) {
	    if (switch_list->type == T_MIXED) {
		switch_list->type = T_INT;
	    } else if (switch_list->type != T_INT) {
		c_error("multiple case types in switch");
		switch_list->type = T_INVALID;
		return (node *) NULL;
	    }
	}
    } else {
	/* string */
	if (n2 != (node *) NULL) {
	    c_error("bad case range");
	    switch_list->type = T_INVALID;
	    return (node *) NULL;
	}
	if (n1->type != N_STR) {
	    c_error("bad case expression");
	    switch_list->type = T_INVALID;
	    return (node *) NULL;
	}
	/* compare type with other cases */
	if (switch_list->type == T_MIXED) {
	    switch_list->type = T_STRING;
	} else if (switch_list->type != T_STRING) {
	    c_error("multiple case types in switch");
	    switch_list->type = T_INVALID;
	    return (node *) NULL;
	}
    }

    switch_list->ncase++;
    n2 = node_mon(N_CASE, 0, (node *) NULL);
    n2->flags |= F_ENTRY | F_REACH;
    case_list = node_bin(N_PAIR, 0, case_list, node_bin(N_PAIR, 0, n1, n2));
    return n2;
}

/*
 * NAME:	compile->default()
 * DESCRIPTION:	handle a default label
 */
node *c_default()
{
    register node *n;

    n = (node *) NULL;
    if (switch_list == (loop *) NULL) {
	c_error("default label not inside switch");
    } else if (switch_list->dflt) {
	c_error("duplicate default label in switch");
	switch_list->type = T_INVALID;
    } else {
	switch_list->ncase++;
	switch_list->dflt = TRUE;
	n = node_mon(N_CASE, 0, (node *) NULL);
	n->flags |= F_ENTRY | F_REACH;
	case_list = node_bin(N_PAIR, 0, case_list,
			     node_bin(N_PAIR, 0, (node *) NULL, n));
    }

    return n;
}

/*
 * NAME:	compile->break()
 * DESCRIPTION:	handle a break statement
 */
node *c_break()
{
    register loop *l;
    node *n;

    l = switch_list;
    if (l == (loop *) NULL || switch_list->env != thisloop) {
	/* no switch, or loop inside switch */
	l = thisloop;
    }
    if (l == (loop *) NULL) {
	c_error("break statement not inside loop or switch");
	return (node *) NULL;
    }
    l->brk = TRUE;

    n = node_mon(N_BREAK, 0, (node *) NULL);
    n->flags |= F_BREAK;
    return n;
}

/*
 * NAME:	compile->continue()
 * DESCRIPTION:	handle a continue statement
 */
node *c_continue()
{
    node *n;

    if (thisloop == (loop *) NULL) {
	c_error("continue statement not inside loop");
	return (node *) NULL;
    }
    thisloop->cont = TRUE;

    n = node_mon(N_CONTINUE, 0, (node *) NULL);
    n->flags |= F_CONT;
    return n;
}

/*
 * NAME:	compile->return()
 * DESCRIPTION:	handle a return statement
 */
node *c_return(n, typechecking)
register node *n;
bool typechecking;
{
    if (n == (node *) NULL) {
	if (typechecking && ftype != T_VOID) {
	    c_error("function must return value");
	}
	n = node_int((Int) 0);
    } else if (typechecking) {
	if (ftype == T_VOID) {
	    /*
	     * can't return anything from a void function
	     */
	    c_error("value returned from void function");
	} else if ((!c_zero(n) || ftype == T_FLOAT) &&
		   c_tmatch(n->mod, ftype) == T_INVALID) {
	    /*
	     * type error
	     */
	    c_error("returned value doesn't match %s (%s)",
		    i_typename(ftype), i_typename(n->mod));
	}
    }

    n = node_mon(N_RETURN, 0, n);
    n->flags |= F_RETURN;
    return n;
}

/*
 * NAME:	compile->startcompound()
 * DESCRIPTION:	start a compound statement
 */
void c_startcompound()
{
    if (thisblock == (block *) NULL) {
	fline = tk_line();
    }
    block_new();
}

/*
 * NAME:	compile->endcompound()
 * DESCRIPTION:	end a compound statement
 */
node *c_endcompound(n)
register node *n;
{
    register int flags;

    block_del();
    if (n != (node *) NULL && n->type == N_PAIR) {
	flags = n->flags & (F_REACH | F_BREAK | F_CONT | F_RETURN);
	n = revert_list(n);
	n->flags |= flags | (n->l.left->flags & F_ENTRY);
    }

    return n;
}

/*
 * NAME:	compile->flookup()
 * DESCRIPTION:	look up a local function, inherited function or kfun
 */
node *c_flookup(n, typechecking)
register node *n;
bool typechecking;
{
    if (strcmp(n->l.string->text, "catch") == 0) {
	return node_mon(N_CATCH, T_STRING, n);
    } else if (strcmp(n->l.string->text, "lock") == 0) {
	if (strcmp(current->file, auto_object) != 0) {
	    c_error("only auto object can use lock()");
	}
	return node_mon(N_LOCK, 0, n);
    } else {
	char *proto;
	long call;

	proto = ctrl_fcall(n->l.string, &call, typechecking);
	n->r.right = (proto == (char *) NULL) ? (node *) NULL :
		      node_fcall(PROTO_FTYPE(proto), proto, (Int) call);
	return n;
    }
}

/*
 * NAME:	compile->iflookup()
 * DESCRIPTION:	look up an inherited function
 */
node *c_iflookup(n, label)
node *n, *label;
{
    char *proto;
    long call;

    proto = ctrl_ifcall(n->l.string, (label != (node *) NULL) ?
				     label->l.string->text : (char *) NULL,
			&call);
    n->r.right = (proto == (char *) NULL) ? (node *) NULL :
		  node_fcall(PROTO_FTYPE(proto), proto, (Int) call);
    return n;
}

/*
 * NAME:	compile->variable()
 * DESCRIPTION:	create a reference to a variable
 */
node *c_variable(n)
register node *n;
{
    register int i;

    i = block_var(n->l.string->text);
    if (i >= 0) {
	/* local var */
	n = node_mon(N_LOCAL, variables[i].type, n);
	n->r.number = i;
    } else {
	long ref;

	/*
	 * try a global variable
	 */
	n = node_mon(N_GLOBAL, ctrl_var(n->l.string, &ref), n);
	n->r.number = ref;
    }
    return n;
}

/*
 * NAME:	compile->vtype()
 * DESCRIPTION:	return the type of a variable
 */
short c_vtype(i)
int i;
{
    return variables[i].type;
}

/*
 * NAME:	lvalue()
 * DESCRIPTION:	check if a value can be an lvalue
 */
static bool lvalue(n)
register node *n;
{
    switch (n->type) {
    case N_LOCAL:
    case N_GLOBAL:
    case N_INDEX:
    case N_FAKE:
	break;

    case N_CAST:
	if (n->mod == n->l.left->mod) {
	    /* only an implicit cast is allowed */
	    break;
	}
	/* fall through */
    default:
	return FALSE;
    }

    return TRUE;
}

/*
 * NAME:	compile->lock()
 * DESCRIPTION:	handle lock
 */
node *c_lock(n)
node *n;
{
    if (strcmp(current->file, auto_object) != 0) {
	c_error("only auto object can use lock()");
    }
    return node_mon(N_LOCK, n->mod, n);
}

/*
 * NAME:	funcall()
 * DESCRIPTION:	handle a function call
 */
static node *funcall(func, args)
register node *func;
node *args;
{
    register int n, nargs, typechecked, t;
    register node **argv, **arg;
    char *argp, *proto, *fname;

    /* get info, prepare return value */
    fname = func->l.string->text;
    func = func->r.right;
    if (func == (node *) NULL) {
	/* error during function lookup */
	return node_mon(N_FAKE, T_MIXED, (node *) NULL);
    }
    proto = func->l.ptr;
    func->mod = (PROTO_FTYPE(proto) == T_IMPLICIT) ?
		 T_MIXED : PROTO_FTYPE(proto);
    func->l.left = args;
    argv = &func->l.left;

    /*
     * check function arguments
     */
    typechecked = PROTO_CLASS(proto) & C_TYPECHECKED;
    nargs = PROTO_NARGS(proto);
    argp = PROTO_ARGS(proto);
    for (n = 1; n <= nargs; n++) {
	if (args == (node *) NULL) {
	    if (!(PROTO_CLASS(proto) & C_VARARGS)) {
		c_error("too few arguments for function %s", fname);
	    }
	    break;
	}
	if ((*argv)->type == N_PAIR) {
	    arg = &(*argv)->l.left;
	    argv = &(*argv)->r.right;
	} else {
	    arg = argv;
	    args = (node *) NULL;
	}
	t = UCHAR(*argp) & ~T_ELLIPSIS;

	if ((*arg)->type == N_SPREAD) {
	    if (argp[nargs - n] == (T_LVALUE | T_ELLIPSIS)) {
		(*arg)->mod = nargs-- - n;
		/* only kfuns can have lvalue parameters */
		func->r.number |= 1L << 16;
	    } else {
		(*arg)->mod = (unsigned short) -1;
	    }
	    t = (*arg)->l.left->mod;
	    if (t != T_MIXED) {
		if ((t & T_REF) == 0) {
		    c_error("ellipsis requires array");
		    t = T_MIXED;
		} else {
		    t -= (1 << REFSHIFT);
		}
	    }
	    if (!(PROTO_CLASS(proto) & C_VARARGS) &&
		PROTO_FTYPE(proto) != T_IMPLICIT) {
		c_error("ellipsis in call to non-varargs function");
	    }

	    while (n <= nargs) {
		if (typechecked &&
		    c_tmatch(t, UCHAR(*argp) & ~T_ELLIPSIS) == T_INVALID) {
		    c_error("bad argument %d for function %s (needs %s)", n,
			    fname, i_typename(UCHAR(*argp) & ~T_ELLIPSIS));
		}
		n++;
		argp++;
	    }
	    break;
	} else if (t == T_LVALUE) {
	    if (!lvalue(*arg)) {
		c_error("bad argument %d for function %s (needs lvalue)",
			n, fname);
	    }
	    *arg = node_mon(N_LVALUE, (*arg)->mod, *arg);
	    /* only kfuns can have lvalue parameters */
	    func->r.number |= 1L << 16;
	} else if ((typechecked || (*arg)->mod == T_VOID) &&
		   (!c_zero(*arg) || t == T_FLOAT) &&
		   c_tmatch((*arg)->mod, t) == T_INVALID) {
	    c_error("bad argument %d for function %s (needs %s)", n, fname,
		    i_typename(t));
	}

	if (UCHAR(*argp) & T_ELLIPSIS) {
	    nargs++;
	} else {
	    argp++;
	}
    }
    if (args != (node *) NULL && PROTO_FTYPE(proto) != T_IMPLICIT) {
	c_error("too many arguments for function %s", fname);
    }

    return func;
}

/*
 * NAME:	compile->funcall()
 * DESCRIPTION:	handle a function call
 */
node *c_funcall(func, args)
node *func, *args;
{
    return funcall(func, revert_list(args));
}

/*
 * NAME:	compile->arrow()
 * DESCRIPTION:	handle ->
 */
node *c_arrow(other, func, args)
node *other, *func, *args;
{
    if (args == (node *) NULL) {
	args = func;
    } else {
	args = node_bin(N_PAIR, 0, func, revert_list(args));
    }
    return funcall(c_flookup(node_str(str_new("call_other", 10L)), FALSE),
		   node_bin(N_PAIR, 0, other, args));
}

/*
 * NAME:	compile->checkcall()
 * DESCRIPTION:	check assignments in a function call, as well as the
 *		returned value
 */
node *c_checkcall(n)
node *n;
{
    register node *t, *a, *l;

    if (n->type == N_FUNC) {
	if (n->r.number >> 16 == ((KFCALL << 8) | 1)) {
	    /*
	     * the function has lvalue parameters
	     */
	    l = (node *) NULL;
	    a = n->l.left;
	    while (a != (node *) NULL) {
		if (a->type == N_PAIR) {
		    t = a->l.left;
		    a = a->r.right;
		} else {
		    t = a;
		    a = (node *) NULL;
		}
		if (t->type == N_LVALUE && t->l.left->type == N_LOCAL &&
		    t->mod != T_MIXED) {
		    /*
		     * check the assignment
		     */
		    t = node_mon(N_CAST, t->mod, t->l.left);
		    l = (l == (node *) NULL) ? t : node_bin(N_COMMA, 0, t, l);
		}
	    }

	    if (l != (node *) NULL) {
		/*
		 * append checks for assignments to function call
		 */
		return node_bin(N_PAIR, n->mod, n, l);
	    }
	}

	if (n->r.number >> 24 != KFCALL && n->r.number >> 24 != DFCALL &&
	    n->mod != T_MIXED && n->mod != T_VOID) {
	    /*
	     * make sure the return value is as it should be
	     */
	    n = node_mon(N_CAST, n->mod, n);
	}
    }

    return n;
}

/*
 * NAME:	compile->tst()
 * DESCRIPTION:	handle a condition
 */
node *c_tst(n)
register node *n;
{
    switch (n->type) {
    case N_INT:
	n->l.number = (n->l.number != 0);
	return n;

    case N_FLOAT:
	return node_int((Int) !NFLT_ISZERO(n));

    case N_STR:
	return node_int((Int) TRUE);

    case N_TST:
    case N_TSTF:
    case N_TSTI:
    case N_NOT:
    case N_NOTF:
    case N_NOTI:
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
	n->r.right = c_tst(n->r.right);
	return n;
    }

    return node_mon(N_TST, T_INT, n);
}

/*
 * NAME:	compile->not()
 * DESCRIPTION:	handle a !condition
 */
node *c_not(n)
register node *n;
{
    switch (n->type) {
    case N_INT:
	n->l.number = (n->l.number == 0);
	return n;

    case N_FLOAT:
	return node_int((Int) NFLT_ISZERO(n));

    case N_STR:
	return node_int((Int) FALSE);

    case N_LAND:
	n->type = N_LOR;
	n->l.left = c_not(n->l.left);
	n->r.right = c_not(n->r.right);
	return n;

    case N_LOR:
	n->type = N_LAND;
	n->l.left = c_not(n->l.left);
	n->r.right = c_not(n->r.right);
	return n;

    case N_TST:
	n->type = N_NOT;
	return n;

    case N_TSTF:
	n->type = N_NOTF;
	return n;

    case N_TSTI:
	n->type = N_NOTI;
	return n;

    case N_NOT:
	n->type = N_TST;
	return n;

    case N_NOTF:
	n->type = N_TSTF;
	return n;

    case N_NOTI:
	n->type = N_TSTI;
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
	n->r.right = c_not(n->r.right);
	return n;
    }

    return node_mon(N_NOT, T_INT, n);
}

/*
 * NAME:	compile->lvalue()
 * DESCRIPTION:	handle an lvalue
 */
node *c_lvalue(n, oper)
node *n;
char *oper;
{
    if (!lvalue(n)) {
	c_error("bad lvalue for %s", oper);
	return node_mon(N_FAKE, T_MIXED, n);
    }
    return n;
}

/*
 * NAME:	compile->tmatch()
 * DESCRIPTION:	See if the two supplied types are compatible. If so, return the
 *		combined type. If not, return T_INVALID.
 */
unsigned short c_tmatch(type1, type2)
register unsigned short type1, type2;
{
    if (type1 == type2) {
	/* identical types */
	return type1;
    }
    if (type1 == T_VOID || type2 == T_VOID) {
	/* void doesn't match with anything else, not even with mixed */
	return T_INVALID;
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
    return T_INVALID;
}

/*
 * NAME:	compile->error()
 * DESCRIPTION:	Produce a warning with the supplied error message.
 */
void c_error(f, a1, a2, a3)
char *f, *a1, *a2, *a3;
{
    char buf[4 * STRINGSZ];	/* file name + 2 * string + overhead */
    extern int nerrors;

    sprintf(buf, "/%s, %u: ", tk_filename(), tk_line());
    sprintf(buf + strlen(buf), f, a1, a2, a3);
    message("%s\012", buf);	/* LF */
    nerrors++;
}
