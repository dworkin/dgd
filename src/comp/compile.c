# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "path.h"
# include "fcontrol.h"
# include "macro.h"
# include "token.h"
# include "ppcontrol.h"
# include "node.h"
# include "control.h"
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
    b->vindex = nvars;
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
	yyerror("redeclaration of parameter %s", name);
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
	yyerror("redeclaration of local variable %s", name);
    } else if (nvars == MAX_LOCALS) {
	yyerror("too many local variables");
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
    ctrl_init(a, d);
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
	yyerror("cannot inherit from auto object");
	return TRUE;
    }
    if (file != auto_object) {
	file = path_inherit(current->file, file);
	if (file == (char *) NULL) {
	    yyerror("illegal inherit path");
	    return TRUE;
	}
	if (strcmp(file, driver_object) == 0) {
	    /* would mess up too many things */
	    yyerror("illegal to inherit driver object");
	    return TRUE;
	}
	for (c = current; c != (context *) NULL; c = c->prev) {
	    if (strcmp(file, c->file) == 0) {
		yyerror("cycle in inheritance");
		return TRUE;
	    }
	}
    }

    o = o_find(file);
    if (o == (object *) NULL || !(o->flags & O_MASTER)) {
	/* object is unloaded */
	strncpy(current->inherit, file, STRINGSZ);
	current->inherit[STRINGSZ - 1] = '\0';
	return FALSE;
    }

    ctrl_inherit(o, (label == (node *) NULL) ?
		     (string *) NULL : label->l.string);
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

    if (recursion) {
	error("Compilation within compilation");
    }

    strcpy(c.file, file);
    c.prev = current;
    current = &c;

    file = path_file(c.file);
    if (file == (char *) NULL) {
	error("Illegal file name \"/%s\"", c.file);
    }
    strcpy(file_c, file);
    strcat(file_c, ".c");

    for (;;) {
	if (!c_autodriver()) {
	    if (!cg_compiled() && o_find(driver_object) == (object *) NULL) {
		/*
		 * (re)compile the driver object to do pathname translation
		 */
		current = (context *) NULL;
		compile(driver_object);
		current = &c;
	    }
	    if (!c_inherit(auto_object, (node *) NULL)) {
		/*
		 * (re)compile auto object and inherit it
		 */
		compile(auto_object);
		c_inherit(auto_object, (node *) NULL);
	    }
	}
	if (strchr(c.file, '#') != (char *) NULL) {
	    error("Illegal file name \"/%s\"", c.file);
	}
	if (!pp_init(file_c, paths, 1)) {
	    if (!c_autodriver()) {
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
		    object *o;
		    char *name;

		    o = o_object(sp->oindex, sp->u.objcnt);
		    sp++;
		    name = o_name(o);
		    if (strcmp(name, auto_object) == 0) {
			error("Illegal rename of auto object");
		    }
		    if (strcmp(name, driver_object) == 0) {
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
	    error("Could not include \"/%s\"", include);
	}
	c.inherit[0] = '\0';

	cg_init(c.prev != (context *) NULL);
	if (ec_push()) {
	    yyerror("error while compiling:");
	    recursion = FALSE;
	    errorlog((char *) NULL);
	    pp_clear();
	    ctrl_clear();
	    c_clear();
	    error((char *) NULL);
	}
	if (!c_autodriver()) {
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
	recursion = TRUE;
	if (yyparse() != 0) {
	    recursion = FALSE;
	    errorlog((char *) NULL);
	    ec_pop();
	    pp_clear();
	    ctrl_clear();
	    c_clear();
	    error("Failed to compile \"/%s.c\"", c.file);
	}
	recursion = FALSE;
	errorlog((char *) NULL);
	ec_pop();
	pp_clear();

	if (c.inherit[0] == '\0') {
	    control *ctrl;

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
	    return ctrl->inherits[ctrl->nvirtuals - 1].obj;
	}
	ctrl_clear();
	c_clear();
	compile(c.inherit);
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
 * DESCRIPTION:	return TRUE if the auto object or driver object is being
 *		compiled
 */
bool c_autodriver()
{
    return strcmp(current->file, auto_object) == 0 ||
	   strcmp(current->file, driver_object) == 0;
}


/*
 * NAME:	revert_list()
 * DESCRIPTION:	revert a "linked list" of nodes
 */
static node *revert_list(n, sep)
register node *n;
register short sep;
{
    register node *m;

    if (n != (node *) NULL && n->type == sep) {
	while ((m=n->l.left)->type == sep) {
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
	yyerror("cannot redeclare %s()\n", str->text);
	return;
    }
    if ((class & (C_PRIVATE | C_NOMASK)) == (C_PRIVATE | C_NOMASK)) {
	yyerror("private contradicts nomask");
    }
    if ((type & T_TYPE) == T_INVALID) {
	typechecked = FALSE;
	type = T_MIXED;
	if (typechecking) {
	    yyerror("missing type for function %s", str->text);
	}
    } else {
	typechecked = TRUE;
	if (type != T_VOID && (type & T_TYPE) == T_VOID) {
	    yyerror("invalid type for function %s (%s)", str->text,
		    c_typename(type));
	    type = T_MIXED;
	}
    }

    /* handle function class and return type */
    if (typechecked) {
	class |= C_TYPECHECKED;
    }
    PROTO_CLASS(proto) = class;
    PROTO_FTYPE(proto) = type;

    /* handle function arguments */
    args = PROTO_ARGS(proto);
    nargs = 0;
    formals = revert_list(formals, N_PAIR);
    while (formals != (node *) NULL) {
	register node *arg;

	if (nargs == MAX_LOCALS) {
	    yyerror("too many parameters in function %s", str->text);
	    break;
	}
	if (formals->type == N_PAIR) {
	    arg = formals->l.left;
	    formals = formals->r.right;
	} else {
	    arg = formals;
	    formals = (node *) NULL;
	}
	if (arg->mod == T_INVALID) {
	    if (typechecked) {
		yyerror("missing type for parameter %s", arg->l.string->text);
	    }
	    arg->mod = T_MIXED;
	} else if ((arg->mod & T_TYPE) == T_VOID) {
	    yyerror("invalid type for parameter %s (%s)", arg->l.string->text,
		    c_typename(arg->mod));
	    arg->mod = T_MIXED;
	}
	*args++ = arg->mod;
	nargs++;
	if (function) {
	    block_pdef(arg->l.string->text, arg->mod);
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
	yyerror("invalid type for variable %s (%s)", str->text,
		c_typename(type));
	type = T_MIXED;
    }
    if (global) {
	if (class & C_NOMASK) {
	    yyerror("invalid class for variable %s", str->text);
	}
	ctrl_dvar(str, class, type);
    } else {
	if (class != 0) {
	    yyerror("invalid class for variable %s", str->text);
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

    list = revert_list(list, N_PAIR);	/* for proper order of err mesgs */
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
    c_decl_func(class, type | n->mod, n->l.left->l.string, n->r.right, TRUE);
}

/*
 * NAME:	max2()
 * DESCRIPTION:	return the maximum of two numbers
 */
static unsigned short max2(a, b)
unsigned short a, b;
{
    return (a > b) ? a : b;
}

/*
 * NAME:	max3()
 * DESCRIPTION:	return the maximum of three numbers
 */
static unsigned short max3(a, b, c)
register unsigned short a, b, c;
{
    return (a > b) ? ((a > c) ? a : c) : ((b > c) ? b : c);
}

static unsigned short d_expr P((node*));

/*
 * NAME:	depth->lvalue()
 * DESCRIPTION:	return the stack depth of an lvalue
 */
static unsigned short d_lvalue(n)
register node *n;
{
    switch (n->type) {
    case N_LOCAL:
    case N_GLOBAL:
	return 1;

    case N_INDEX:
	switch (n->l.left->type) {
	case N_LOCAL:
	case N_GLOBAL:
	    return 1 + d_expr(n->r.right);

	case N_INDEX:
	    return max3(d_expr(n->l.left->l.left),
			1 + d_expr(n->l.left->r.right),
			2 + d_expr(n->r.right));

	default:
	    return max2(d_expr(n->l.left), 1 + d_expr(n->r.right));
	}
    }
}

/*
 * NAME:	depth->expr()
 * DESCRIPTION:	return the stack depth of an expression
 */
static unsigned short d_expr(n)
register node *n;
{
    register unsigned short d, i;

    switch (n->type) {
    case N_ADD:
    case N_ADD_INT:
    case N_AND:
    case N_AND_INT:
    case N_DIV:
    case N_DIV_INT:
    case N_EQ:
    case N_EQ_INT:
    case N_GE:
    case N_GE_INT:
    case N_GT:
    case N_GT_INT:
    case N_INDEX:
    case N_LE:
    case N_LE_INT:
    case N_LSHIFT:
    case N_LSHIFT_INT:
    case N_LT:
    case N_LT_INT:
    case N_MOD:
    case N_MOD_INT:
    case N_MULT:
    case N_MULT_INT:
    case N_NE:
    case N_NE_INT:
    case N_OR:
    case N_OR_INT:
    case N_PAIR:
    case N_RSHIFT:
    case N_RSHIFT_INT:
    case N_SUB:
    case N_SUB_INT:
    case N_XOR:
    case N_XOR_INT:
	return max2(d_expr(n->l.left), 1 + d_expr(n->r.right));

    case N_ADD_EQ:
    case N_ADD_EQ_INT:
    case N_AND_EQ:
    case N_AND_EQ_INT:
    case N_DIV_EQ:
    case N_DIV_EQ_INT:
    case N_LSHIFT_EQ:
    case N_LSHIFT_EQ_INT:
    case N_MOD_EQ:
    case N_MOD_EQ_INT:
    case N_MULT_EQ:
    case N_MULT_EQ_INT:
    case N_OR_EQ:
    case N_OR_EQ_INT:
    case N_SUB_EQ:
    case N_SUB_EQ_INT:
    case N_XOR_EQ:
    case N_XOR_EQ_INT:
    case N_RSHIFT_EQ:
    case N_RSHIFT_EQ_INT:
	d = 4 + d_expr(n->r.right);
	n = n->l.left;
	if (n->type == N_CAST) {
	    n = n->l.left;
	}
	return max2(d, 1 + d_lvalue(n));

    case N_AGGR:
	if (n->mod == T_MAPPING) {
	    n = n->l.left;
	    if (n == (node *) NULL) {
		return 1;
	    }

	    d = 0;
	    for (i = 0; n->type == N_COMMA; i += 2) {
		d = max3(d, i + d_expr(n->r.right->r.right),
			 i + 1 + d_expr(n->r.right->l.left));
		n = n->l.left;
	    }
	    return max3(d, i + d_expr(n->r.right), i + 1 + d_expr(n->l.left));
	} else {
	    n = n->l.left;
	    if (n == (node *) NULL) {
		return 1;
	    }

	    d = 0;
	    for (i = 0; n->type == N_COMMA; i++) {
		d = max2(d, i + d_expr(n->r.right));
		n = n->l.left;
	    }
	    return max2(d, i + d_expr(n));
	}

    case N_ASSIGN:
	d = 3 + d_expr(n->r.right);
	if (n->l.left->type == N_CAST) {
	    n = n->l.left;
	}
	return max2(d, d_lvalue(n->l.left));

    case N_CATCH:
	if (n->l.left == (node *) NULL) {
	    return 1;
	}
    case N_CAST:
    case N_LOCK:
    case N_NOT:
    case N_TST:
	return d_expr(n->l.left);

    case N_COMMA:
    case N_LAND:
    case N_LOR:
	return max2(d_expr(n->l.left), d_expr(n->r.right));

    case N_FUNC:
	n = n->l.left;
	if (n == (node *) NULL) {
	    return 1;
	}

	d = 0;
	for (i = 0; n->type == N_COMMA; i++) {
	    d = max2(d, i + d_expr(n->l.left));
	    n = n->r.right;
	}
	return max2(d, i + 1 + d_expr(n));

    case N_GLOBAL:
    case N_INT:
    case N_LOCAL:
    case N_STR:
	return 1;

    case N_LVALUE:
	return d_lvalue(n->l.left);

    case N_QUEST:
	return max3(d_expr(n->l.left),
		    d_expr(n->r.right->l.left),
		    d_expr(n->r.right->r.right));

    case N_RANGE:
	return max3(d_expr(n->l.left),
		    1 + d_expr(n->r.right->l.left),
		    2 + d_expr(n->r.right->r.right));

    case N_MIN_MIN:
    case N_MIN_MIN_INT:
    case N_PLUS_PLUS:
    case N_PLUS_PLUS_INT:
	n = n->l.left;
	if (n->type == N_CAST) {
	    n = n->l.left;
	}
	return 2 + d_lvalue(n);
    }
}

/*
 * NAME:	depth->stmt()
 * DESCRIPTION:	return the stack depth of a statement
 */
static unsigned short d_stmt(n)
register node *n;
{
    register unsigned short d;
    register node *m;

    d = 0;
    while (n != (node *) NULL) {
	if (n->type == N_PAIR) {
	    m = n->l.left;
	    n = n->r.right;
	} else {
	    m = n;
	    n = (node *) NULL;
	}
	switch (m->type) {
	case N_BLOCK:
	case N_CASE:
	case N_FOREVER:
	    d = max2(d, d_stmt(m->l.left));
	    break;

	case N_DO:
	case N_FOR:
	case N_WHILE:
	    d = max3(d, d_expr(m->l.left), d_stmt(m->r.right));
	    break;

	case N_IF:
	    d = max3(d, d_expr(m->l.left),
		     max2(d_stmt(m->r.right->l.left),
			  d_stmt(m->r.right->r.right)));
	    break;

	case N_PAIR:
	    d = max2(d, d_stmt(m));
	    break;

	case N_POP:
	case N_RETURN:
	    d = max2(d, d_expr(m->l.left));
	    break;

	case N_SWITCH_INT:
	case N_SWITCH_RANGE:
	case N_SWITCH_STR:
	    d = max2(d, d_expr(m->r.right->l.left));
	    break;
	}
    }
    return d;
}

/*
 * NAME:	compile->funcbody()
 * DESCRIPTION:	create a function body
 */
void c_funcbody(n)
node *n;
{
    char *prog;
    unsigned short size;

    n = c_concat(n, node_mon(N_RETURN, 0, node_int((Int) 0)));
    prog = cg_function(n, nvars, nparams, d_stmt(n), &size);
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
 * NAME:	compile->concat()
 * DESCRIPTION:	concatenate two statements
 */
node *c_concat(n1, n2)
register node *n1, *n2;
{
    if (n1 == (node *) NULL) {
	return n2;
    } else if (n2 == (node *) NULL ||
	       (n2->type != N_CASE &&
		(n1->type == N_BREAK ||
		 n1->type == N_CONTINUE ||
		 n1->type == N_RETURN ||
		 (n1->type == N_PAIR &&
		  (n1->r.right->type == N_BREAK ||
		   n1->r.right->type == N_CONTINUE ||
		   n1->r.right->type == N_RETURN))))) {
	/*
	 * doesn't handle { foo(); bar(); return; } gnu(); properly
	 */
	return n1;
    }
    return node_bin(N_PAIR, 0, n1, n2);
}

/*
 * NAME:	compile->minimize_exp()
 * DESCRIPTION:	Minimize an expression.  The 2nd argument is a flag that
 *		indicates if the final result is to be kept.
 */
static node *c_minimize_exp(n, keep)
register node *n;
register bool keep;
{
    if (n == (node *) NULL) {
	return (node *) NULL;
    }

    switch (n->type) {
    case N_GLOBAL:
    case N_INT:
    case N_LOCAL:
    case N_STR:
	if (!keep) {
	    return (node *) NULL;
	}
	break;

    case N_NOT:
    case N_TST:
	n->l.left = c_minimize_exp(n->l.left, keep);
	if (n->l.left == (node *) NULL) {
	    return (node *) NULL;
	}
	break;

    case N_CATCH:
	n->l.left = c_minimize_exp(n->l.left, FALSE);
	break;

    case N_LOCK:
	n->l.left = c_minimize_exp(n->l.left, keep);
	if (n->l.left == (node *) NULL) {
	    return (node *) NULL;
	}
	break;

    case N_LVALUE:
	n = c_minimize_exp(n->l.left, keep);
	break;

    case N_CAST:
    case N_MIN_MIN:
    case N_MIN_MIN_INT:
    case N_PLUS_PLUS:
    case N_PLUS_PLUS_INT:
	n->l.left = c_minimize_exp(n->l.left, TRUE);
	break;

    case N_COMMA:
	n->l.left = c_minimize_exp(n->l.left, FALSE);
	n->r.right = c_minimize_exp(n->r.right, keep);
	if (n->l.left == (node *) NULL) {
	    return n->r.right;
	}
	if (n->r.right == (node *) NULL) {
	    return n->l.left;
	}
	break;

    case N_PAIR:
	n->l.left = c_minimize_exp(n->l.left, keep);
	n->r.right = c_minimize_exp(n->r.right, FALSE);
	if (n->l.left == (node *) NULL) {
	    return n->r.right;
	}
	if (n->r.right == (node *) NULL) {
	    return n->l.left;
	}
	break;

    case N_ADD_INT:
    case N_AND_INT:
    case N_DIV_INT:
    case N_EQ:
    case N_EQ_INT:
    case N_GE_INT:
    case N_GT_INT:
    case N_LE_INT:
    case N_LSHIFT_INT:
    case N_LT_INT:
    case N_MOD_INT:
    case N_MULT_INT:
    case N_NE:
    case N_NE_INT:
    case N_OR_INT:
    case N_RSHIFT_INT:
    case N_SUB_INT:
    case N_XOR_INT:
	n->l.left = c_minimize_exp(n->l.left, keep);
	n->r.right = c_minimize_exp(n->r.right, keep);
	if (n->l.left == (node *) NULL) {
	    return n->r.right;
	}
	if (n->r.right == (node *) NULL) {
	    return n->l.left;
	}
	break;

    case N_ADD:
    case N_ADD_EQ:
    case N_ADD_EQ_INT:
    case N_AND:
    case N_AND_EQ:
    case N_AND_EQ_INT:
    case N_ASSIGN:
    case N_DIV:
    case N_DIV_EQ:
    case N_DIV_EQ_INT:
    case N_GE:
    case N_GT:
    case N_INDEX:
    case N_LAND:
    case N_LE:
    case N_LOR:
    case N_LSHIFT:
    case N_LSHIFT_EQ:
    case N_LSHIFT_EQ_INT:
    case N_LT:
    case N_MOD:
    case N_MOD_EQ:
    case N_MOD_EQ_INT:
    case N_MULT:
    case N_MULT_EQ:
    case N_MULT_EQ_INT:
    case N_OR:
    case N_OR_EQ:
    case N_OR_EQ_INT:
    case N_RSHIFT:
    case N_RSHIFT_EQ:
    case N_RSHIFT_EQ_INT:
    case N_SUB:
    case N_SUB_EQ:
    case N_SUB_EQ_INT:
    case N_XOR:
    case N_XOR_EQ:
    case N_XOR_EQ_INT:
	n->l.left = c_minimize_exp(n->l.left, TRUE);
	n->r.right = c_minimize_exp(n->r.right, TRUE);
	break;

    case N_QUEST:
    case N_RANGE:
	n->l.left = c_minimize_exp(n->l.left, TRUE);
	n->r.right->l.left = c_minimize_exp(n->r.right->l.left, TRUE);
	n->r.right->r.right = c_minimize_exp(n->r.right->r.right, TRUE);
	break;
    }

    return n;
}

/*
 * NAME:	compile->list_exp()
 * DESCRIPTION:	minimize a list of expressions
 */
node *c_list_exp(n)
node *n;
{
    return c_minimize_exp(n, TRUE);
}

/*
 * NAME:	compile->exp_stmt()
 * DESCRIPTION:	reduce an expression to a statement
 */
node *c_exp_stmt(n)
node *n;
{
    n = c_minimize_exp(n, FALSE);
    if (n != (node *) NULL) {
	if (n->type == N_QUEST) {
	    /* special case for ? : */
	    return c_if(n->l.left, c_exp_stmt(n->r.right->l.left),
			c_exp_stmt(n->r.right->r.right));
	} else {
	    return node_mon(N_POP, 0, n);
	}
    }
    return n;
}

# define COND_QUEST	0
# define COND_FIRST	1
# define COND_LAST	2

/*
 * NAME:	compile->cond()
 * DESCRIPTION:	Handle a condition.  Return 0 if it is a normal condition;
 *		return 1 or 2 if it is fixed on a certain alternative, and
 *		condition and alternative have been combined.
 */
static int c_cond(n1, n2, n3, n4, op)
register node *n1, *n2;
node *n3, **n4;
int op;
{
    int alt;

    if (n1 == (node *) NULL) {
	/* always */
	*n4 = n2;
	return 1;	/* 1st alternative */
    }

    switch (n1->type) {
    case N_INT:
	if (n1->l.number != 0) {
	    /* always */
	    *n4 = n2;
	    return 1;	/* 1st alternative */
	} else {
	    /* never */
	    *n4 = n3;
	    return 2;	/* 2nd alternative */
	}

    case N_STR:
	/* always */
	*n4 = n2;
	return 1;	/* 1st alternative */

    case N_COMMA:
	if (n1->r.right->type == N_STR) {
	    /* always */
	    alt = 1;
	    break;
	} else if (n1->r.right->type == N_INT) {
	    /* always ... */
	    alt = 1;
	    if (n1->r.right->l.number == 0) {
		/* ... no, never ... */
		n2 = n3;
		alt = 2;
	    }
	    /* ... with side effect */
	    break;
	}
    default:
	return 0;	/* normal case */
    }

    /*
     * add "condition" as statement or expression (preserving side effects)
     */
    switch (op) {
    case COND_QUEST:
	*n4 = node_bin(N_COMMA, n2->mod, n1, n2);
	break;

    case COND_FIRST:
	*n4 = c_concat(c_exp_stmt(n1->l.left), n2);
	break;
	
    case COND_LAST:
	*n4 = c_concat(n2, c_exp_stmt(n1->l.left));
	break;
    }
    return alt;
}

/*
 * NAME:	compile->quest()
 * DESCRIPTION:	handle ? :
 */
node *c_quest(n1, n2, n3)
node *n1, *n2, *n3;
{
    if (c_cond(n1, n2, n3, &n1, COND_QUEST) != 0) {
	/* fixed condition */
	return n1;
    }
    return node_bin(N_QUEST, 0, n1, node_bin(N_PAIR, 0, n2, n3));
}

/*
 * NAME:	compile->if()
 * DESCRIPTION:	handle an if statement
 */
node *c_if(n1, n2, n3)
register node *n1, *n2;
node *n3;
{
    if (n2 == (node *) NULL) {
	if (n3 == (node *) NULL) {
	    /* no statements at all */
	    return c_exp_stmt(n1);
	}
	/* reverse condition */
	n1 = c_not(n1);
	n2 = n3;
	n3 = (node *) NULL;
    }
    if (c_cond(n1, n2, n3, &n3, COND_FIRST) != 0) {
	/* fixed condition */
	return n3;
    }
    return node_bin(N_IF, 0, n1, node_bin(N_ELSE, 0, n2, n3));
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
register node *n;
{
    if (thisloop->cont && n != (node *) NULL) {
	n = node_mon(N_BLOCK, N_CONTINUE, n);
    }
    return n;
}

/*
 * NAME:	compile->endloop()
 * DESCRIPTION:	end a loop
 */
static node *c_endloop(n)
register node *n;
{
    if (thisloop->brk && n != (node *) NULL) {
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
    node *n;

    n2 = c_reloop(n2);
    switch (c_cond(n1, n2, n2, &n, COND_LAST)) {
    case 0:
	/* regular do loop */
	n1 = node_bin(N_DO, 0, n1, n2);
	break;

    case 1:
	/* always true */
	n1 = node_mon(N_FOREVER, 0, n);
	break;

    case 2:
	if (thisloop->brk || thisloop->cont) {
	    /* never true, but there is a break or continue statement */
	    n1 = node_mon(N_FOREVER, 0,
			  c_concat(n, node_mon(N_BREAK, 0, (node *) NULL)));
	    thisloop->brk = TRUE;
	} else {
	    /* never true: just once */
	    n1 = n;
	}
	break;
    }
    return c_endloop(n1);
}

/*
 * NAME:	compile->while()
 * DESCRIPTION:	end a while loop
 */
node *c_while(n1, n2)
register node *n1, *n2;
{
    node *n;

    n2 = c_reloop(n2);
    switch (c_cond(n1, n2, (node *) NULL, &n, COND_FIRST)) {
    case 0:
	/* regular while loop */
	n1 = node_bin(N_WHILE, 0, n1, n2);
	break;

    case 1:
	/* always true */
	n1 = node_mon(N_FOREVER, 0, n);
	break;

    case 2:
	/* never true */
	n1 = n;
	break;
    }
    return c_endloop(n1);
}

/*
 * NAME:	compile->for()
 * DESCRIPTION:	end a for loop
 */
node *c_for(n1, n2, n3, n4)
register node *n1, *n2, *n3, *n4;
{
    node *n;

    n4 = c_reloop(n4);
    if (n3 != (node *) NULL) {
	n4 = c_concat(n4, c_exp_stmt(n3));
    }

    switch (c_cond(n2, n4, (node *) NULL, &n, COND_FIRST)) {
    case 0:
	/* regular for loop */
	n3 = node_bin(N_FOR, 0, n2, n4);
	break;

    case 1:
	/* always true */
	n3 = node_mon(N_FOREVER, 0, n);
	break;

    case 2:
	/* never true */
	n3 = n;
	break;
    }

    return c_concat(c_exp_stmt(n1), c_endloop(n3));
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
    if (n->type == N_INT || n->type == N_STR) {
	yyerror("switch expression is constant");
	switch_list->type = T_INVALID;
    } else if (typechecking && n->mod != T_NUMBER && n->mod != T_STRING &&
	       n->mod != T_MIXED) {
	yyerror("bad switch expression type (%s)", c_typename(n->mod));
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
    register short i, size;
    register long l, cnt;
    short type, sz;

    n = (node *) NULL;
    if (switch_list->type != T_INVALID) {
	if (stmt == (node *) NULL) {
	    yyerror("no statements in switch");
	} else if (stmt->type != N_CASE &&
		   (stmt->type != N_PAIR || stmt->l.left->type != N_CASE)) {
	    yyerror("unlabeled statements in switch");
	} else if ((size=switch_list->ncase - switch_list->dflt) == 0) {
	    yyerror("only default label in switch");
	} else if (expr->mod != T_MIXED && expr->mod != switch_list->type &&
		   switch_list->type != T_MIXED) {
	    yyerror("wrong switch expression type (%s)", c_typename(expr->mod));
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
		    yyerror("duplicate case labels in switch");
		} else {
		    i = (v[0]->l.left->type == N_INT);
		    for (w = v + i, i = size - i - 1; i > 0; w++, --i) {
			if (strcmp(w[0]->l.left->l.string->text,
				   w[1]->l.left->l.string->text) == 0) {
			    yyerror("duplicate case labels in switch");
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
			    yyerror("duplicate case labels in switch");
			} else {
			    yyerror("overlapping case label ranges in switch");
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
		    if ((sz + 2L) * cnt > (2 * sz + 2L) * size) {
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
	yyerror("case label not inside switch");
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
		yyerror("bad case range in switch");
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
		switch_list->type = T_NUMBER;
	    } else if (switch_list->type != T_NUMBER) {
		yyerror("multiple case types in switch");
		switch_list->type = T_INVALID;
		return (node *) NULL;
	    }
	}
    } else {
	/* string */
	if (n2 != (node *) NULL) {
	    yyerror("bad case range in switch");
	    switch_list->type = T_INVALID;
	    return (node *) NULL;
	}
	if (n1->type != N_STR) {
	    yyerror("non-constant case in switch");
	    switch_list->type = T_INVALID;
	    return (node *) NULL;
	}
	/* compare type with other cases */
	if (switch_list->type == T_MIXED) {
	    switch_list->type = T_STRING;
	} else if (switch_list->type != T_STRING) {
	    yyerror("multiple case types in switch");
	    switch_list->type = T_INVALID;
	    return (node *) NULL;
	}
    }

    switch_list->ncase++;
    n2 = node_mon(N_CASE, 0, (node *) NULL);
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
	yyerror("default label not inside switch");
    } else if (switch_list->dflt) {
	yyerror("duplicate default label in switch");
	switch_list->type = T_INVALID;
    } else {
	switch_list->ncase++;
	switch_list->dflt = TRUE;
	n = node_mon(N_CASE, 0, (node *) NULL);
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

    l = switch_list;
    if (l == (loop *) NULL || switch_list->env != thisloop) {
	/* no switch, or loop inside switch */
	l = thisloop;
    }
    if (l == (loop *) NULL) {
	yyerror("break statement not inside loop or switch");
	return (node *) NULL;
    }
    l->brk = TRUE;
    return node_mon(N_BREAK, 0, (node *) NULL);
}

/*
 * NAME:	compile->continue()
 * DESCRIPTION:	handle a continue statement
 */
node *c_continue()
{
    if (thisloop == (loop *) NULL) {
	yyerror("continue statement not inside loop");
	return (node *) NULL;
    }
    thisloop->cont = TRUE;
    return node_mon(N_CONTINUE, 0, (node *) NULL);
}

/*
 * NAME:	compile->ftype()
 * DESCRIPTION:	return the type of the current function
 */
short c_ftype()
{
    return ftype;
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
 * NAME:	compile->startcompound()
 * DESCRIPTION:	start a compound statement
 */
void c_startcompound()
{
    block_new();
}

/*
 * NAME:	compile->endcompound()
 * DESCRIPTION:	end a compound statement
 */
node *c_endcompound(n)
node *n;
{
    block_del();
    return revert_list(n, N_PAIR);
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
	    yyerror("only auto object can use lock()");
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

    proto = (label != (node *) NULL) ?
	    ctrl_lfcall(n->l.string, label->l.string->text, &call) :
	    ctrl_ifcall(n->l.string, &call);
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
 * NAME:	lvalue()
 * DESCRIPTION:	Convert a value into an lvalue.  Return the converted argument
 *		or NULL.
 */
static node *lvalue(n)
register node *n;
{
    switch (n->type) {
    case N_LOCAL:
    case N_GLOBAL:
    case N_INDEX:
    case N_FAKE:
	break;

    case N_CAST:
	switch (n->l.left->type) {
	case N_LOCAL:
	case N_GLOBAL:
	case N_INDEX:
	case N_FAKE:
	    return node_mon(N_LVALUE, T_NUMBER, n);
	}
    default:
	return (node *) NULL;
    }

    return node_mon(N_LVALUE, n->mod, n);
}

/*
 * NAME:	funcall()
 * DESCRIPTION:	handle a function call
 */
static node *funcall(func, args)
register node *func;
node *args;
{
    register int n, nargs, typechecked;
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
    nargs = PROTO_NARGS(proto);
    typechecked = PROTO_CLASS(proto) & C_TYPECHECKED;
    for (n = 1, argp = PROTO_ARGS(proto); n <= nargs; n++, argp++) {
	if (args == (node *) NULL) {
	    if (!(PROTO_CLASS(proto) & C_VARARGS)) {
		yyerror("too few arguments for function %s", fname);
	    }
	    return func;
	}
	if ((*argv)->type == N_COMMA) {
	    arg = &(*argv)->l.left;
	    argv = &(*argv)->r.right;
	} else {
	    arg = argv;
	    args = (node *) NULL;
	}
	if (UCHAR(*argp) == T_LVALUE) {
	    *arg = lvalue(*arg);
	    if (*arg == (node *) NULL) {
		yyerror("bad argument %d for function %s (needs lvalue)",
			n, fname);
	    }
	    /* only kfuns can have lvalue parameters */
	    func->r.number |= 1L << 16;
	} else if ((typechecked || (*arg)->mod == T_VOID) &&
		   ((*arg)->type != N_INT || (*arg)->l.number != 0) &&
		   c_tmatch((*arg)->mod, UCHAR(*argp)) == T_INVALID) {
	    yyerror("bad argument %d for function %s (needs %s)", n, fname,
		    c_typename(UCHAR(*argp)));
	}
    }
    if (args != (node *) NULL && !(PROTO_CLASS(proto) & C_UNDEFINED)) {
	yyerror("too many arguments for function %s", fname);
    }

    return func;
}

/*
 * NAME:	compile->funcall()
 * DESCRIPTION:	handle a function call
 */
node *c_funcall(func, args)
register node *func, *args;
{
    if (func->type != N_STR) {
	/*
	 * catch() or lock()
	 */
	if (args == (node *) NULL) {
	    yyerror("%s() needs expression argument",
		    func->l.left->l.string->text);
	    func->mod = T_MIXED;
	} else {
	    if (func->type == N_LOCK) {
		func->mod = args->mod;
	    }
	    func->l.left = args;
	}
	return func;
    } else {
	return funcall(func, revert_list(args, N_COMMA));
    }
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
	args = node_bin(N_COMMA, 0, func, revert_list(args, N_COMMA));
    }
    return funcall(c_flookup(node_str(str_new("call_other", 10L)), FALSE),
		   node_bin(N_COMMA, 0, other, args));
}

/*
 * NAME:	compile->checklval()
 * DESCRIPTION:	check assignments to local variables in a function call
 */
node *c_checklval(n)
node *n;
{
    register node *t, *a, *l;

    if (n->r.number >> 16 == ((KFCALL << 8) | 1)) {
	/*
	 * the function has lvalue parameters
	 */
	l = (node *) NULL;
	a = n->l.left;
	while (a != (node *) NULL) {
	    if (a->type == N_COMMA) {
		t = a->l.left;
		a = a->r.right;
	    } else {
		t = a;
		a = (node *) NULL;
	    }
	    if (t->type == N_LVALUE && (t=t->l.left)->type == N_LOCAL &&
		t->mod == T_NUMBER) {
		/*
		 * assignment to local integer variable
		 */
		t = node_mon(N_CAST, T_NUMBER, t);
		l = (l == (node *) NULL) ? t : node_bin(N_COMMA, 0, t, l);
	    }
	}

	if (l != (node *) NULL) {
	    /*
	     * append checks for integer var assignments to function call
	     */
	    return node_bin(N_PAIR, n->mod, n, l);
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

    case N_STR:
	return node_int((Int) TRUE);

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
	n->type = T_NUMBER;
	n->r.right = c_tst(n->r.right);
	return n;
    }

    return node_mon(N_TST, T_NUMBER, n);
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
	n->l.number = !n->l.number;
	return n;

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
	n->type = T_NUMBER;
	n->r.right = c_not(n->r.right);
	return n;
    }

    return node_mon(N_NOT, T_NUMBER, n);
}

/*
 * NAME:	compile->lvalue()
 * DESCRIPTION:	handle an lvalue
 */
node *c_lvalue(n, oper)
node *n;
char *oper;
{
    n = lvalue(n);
    if (n == (node *) NULL) {
	yyerror("bad lvalue for %s", oper);
	return node_mon(N_FAKE, T_MIXED, (node *) NULL);
    }
    return n;
}

/*
 * NAME:	compile->typename()
 * DESCRIPTION:	return the name of the argument type
 */
char *c_typename(type)
register unsigned short type;
{
    static bool flag;
    static char buf1[8 + 16 + 1], buf2[8 + 16 + 1], *name[] = TYPENAMES;
    register char *buf;

    if (flag) {
	buf = buf1;
	flag = FALSE;
    } else {
	buf = buf2;
	flag = TRUE;
    }
    strcpy(buf, name[type & T_TYPE]);
    type &= T_REF;
    type >>= REFSHIFT;
    if (type > 0) {
	register char *p;

	p = buf + strlen(buf);
	*p++ = ' ';
	do {
	    *p++ = '*';
	} while (--type > 0);
	*p = '\0';
    }
    return buf;
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
	if (type1 == T_MIXED && (type2 & T_REF)) {
	    type1 |= 1 << REFSHIFT;	/* mixed <-> int * */
	}
	return type1;
    }
    if ((type2 & T_TYPE) == T_MIXED && (type2 & T_REF) <= (type1 & T_REF)) {
	/* int <-> mixed,  int * <-> mixed *,  int ** <-> mixed * */
	if (type2 == T_MIXED && (type1 & T_REF)) {
	    type2 |= 1 << REFSHIFT;	/* int * <-> mixed */
	}
	return type2;
    }
    return T_INVALID;
}
