# include "comp.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "path.h"
# include "lex.h"
# include "fcontrol.h"
# include "control.h"
# include "node.h"
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

    for (i = 0; i < nvars; i++) {
	if (strcmp(variables[i].name, name) == 0) {
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
	yyerror("redeclaration of function parameter %s", name);
    } else {
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
    typechecking = flag;
    ctrl_init(a);
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
	yyerror("may not inherit from auto object");
	return TRUE;
    }
    file = path_inherit(current->file, file);
    if (file == (char *) NULL) {
	yyerror("illegal inherit path");
	return TRUE;
    }
    for (c = current; c != (context *) NULL; c = c->prev) {
	if (strcmp(file, c->file) == 0) {
	    yyerror("cycle in inheritance");
	    return TRUE;
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

static int recursion;		/* recursive compilation level */

/*
 * NAME:	compile()
 * DESCRIPTION:	compile an LPC file
 */
static control *compile(file)
char *file;
{
    context c;
    char file_c[STRINGSZ + 2];

    strncpy(c.file, file, STRINGSZ);
    c.file[STRINGSZ - 1] = '\0';
    c.prev = current;
    current = &c;

    strcpy(file_c, c.file);
    strcat(file_c, ".c");

    for (;;) {
	if (strcmp(c.file, auto_object) != 0 &&
	    strcmp(c.file, driver_object) != 0 &&
	    !c_inherit(auto_object, (node *) NULL)) {
	    /*
	     * (re)compile auto object and inherit it
	     */
	    compile(auto_object);
	    c_inherit(auto_object, (node *) NULL);
	}
	if (strchr(c.file, '#') != (char *) NULL || !pp_init(file_c, paths)) {
	    error("Could not compile \"%s\"", file_c);
	}
	if (!tk_include(include)) {
	    pp_clear();
	    error("Could not include \"%s\"", include);
	}
	c.inherit[0] = '\0';

	if (ec_push()) {
	    recursion = 0;
	    pp_clear();
	    ctrl_clear();
	    c_clear();
	    error((char *) NULL);
	}
	recursion++;
	if (yyparse()) {
	    recursion = 0;
	    ec_pop();
	    pp_clear();
	    ctrl_clear();
	    c_clear();
	    error("Failed to compile \"%s\"", file_c);
	}
	--recursion;
	ec_pop();
	pp_clear();

	if (c.inherit[0] == '\0') {
	    control *ctrl;

	    if (!seen_decls) {
		/*
		 * object with inherit statements only
		 */
		ctrl_create();
	    }
	    ctrl = ctrl_construct(c.file);
	    ctrl_clear();
	    c_clear();
	    current = c.prev;
	    return ctrl;
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
    control *ctrl;

    if (recursion > 0) {
	error("Compilation within compilation");
    }
    ctrl = compile(path_resolve(file));
    return ctrl->inherits[ctrl->nvirtuals - 1].obj;
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
static void c_decl_func(class, type, str, formals, function, typechecking)
unsigned short class, type;
string *str;
register node *formals;
bool function;
bool typechecking;
{
    char proto[3 + MAX_LOCALS];
    register char *args;
    register int nargs;

    /* check for some errors */
    if (strcmp(str->text, "catch") == 0 || strcmp(str->text, "lock") == 0) {
	yyerror("cannot redefine %s()\n", str->text);
    } else if (type == T_ERROR) {
	if (typechecking) {
	    yyerror("no type specified for function %s", str->text);
	}
	type = T_MIXED;
    }
    if ((class & (C_PRIVATE | C_NOMASK)) == (C_PRIVATE | C_NOMASK)) {
	yyerror("class private contradicts class nomask");
    }

    /* handle function class and return type */
    if (typechecking) {
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
	if (arg->mod == T_ERROR) {
	    if (typechecking) {
		yyerror("no type specified for parameter %s",
			arg->l.string->text);
	    }
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
    if (global) {
	if (class & C_NOMASK) {
	    yyerror("invalid class nomask for variable %s", str->text);
	}
	ctrl_dvar(str, class, type);
    } else {
	if (class != 0) {
	    yyerror("invalid class specifier for variable %s", str->text);
	}
	block_vdef(str->text, type);
    }
}

/*
 * NAME:	compile->decl_list()
 * DESCRIPTION:	handle a list of declarations
 */
static void c_decl_list(class, type, list, typechecking, global)
register unsigned short class, type;
register node *list;
bool typechecking, global;
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
	    c_decl_func(class, type | (n->mod & T_REF), n->l.left->l.string,
			n->r.right, FALSE, typechecking);
	} else {
	    c_decl_var(class, type | (n->mod & T_REF), n->l.string, global);
	}
    }
}

/*
 * NAME:	compile->global()
 * DESCRIPTION:	handle a global declaration
 */
void c_global(class, type, n, typechecking)
unsigned short class, type;
node *n;
bool typechecking;
{
    if (!seen_decls) {
	ctrl_create();
	seen_decls = TRUE;
    }
    c_decl_list(class, type, n, typechecking, TRUE);
}

/*
 * NAME:	compile->function()
 * DESCRIPTION:	create a function
 */
void c_function(class, type, n, typechecking)
unsigned short class, type;
register node *n;
bool typechecking;
{
    if (!seen_decls) {
	ctrl_create();
	seen_decls = TRUE;
    }
    c_decl_func(class, type | (n->mod & T_REF), n->l.left->l.string, n->r.right,
		TRUE, typechecking);
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

    prog = cg_function(n, nvars, nparams, &size);
    ctrl_dprogram(prog, size);
    node_free();
    nvars = 0;
    nparams = 0;
}

/*
 * NAME:	compile->typechecking()
 * DESCRIPTION:	return TRUE if strict typing is turned on
 */
bool c_typechecking()
{
    return typechecking;
}

/*
 * NAME:	compile->local()
 * DESCRIPTION:	handle local declarations
 */
void c_local(class, type, n, typechecking)
short class, type;
node *n;
bool typechecking;
{
    c_decl_list(class, type, n, typechecking, FALSE);
}


/*
 * NAME:	compile->list_exp()
 * DESCRIPTION:	purge a list of expressions
 */
void c_list_exp(n)
register node *n;
{
    register node *m;

    if (n->type == N_COMMA) {
	while ((m=n->l.left)->type == N_COMMA) {
	    if (m->r.right->type == N_INT || m->r.right->type == N_STR ||
		m->r.right->type == N_LOCAL || m->r.right->type == N_GLOBAL) {
		/*
		 * ((a, b), c) -> (a, c)
		 */
		n->l.left = m->l.left;
	    } else {
		n = m;
	    }
	}
	if (n->l.left->type == N_INT || n->l.left->type == N_STR ||
	    n->l.left->type == N_LOCAL || n->l.left->type == N_GLOBAL) {
	    /*
	     * (a, b) -> b
	     */
	    n = n->r.right;
	}
    }
}

/*
 * NAME:	compile->minimize_exp_stmt()
 * DESCRIPTION:	minimize an expression as a statement
 */
static node *c_minimize_exp_stmt(n)
register node *n;
{
    if (n == (node *) NULL) {
	return (node *) NULL;
    }

    switch (n->type) {
    case N_INT:
    case N_STR:
    case N_LOCAL:
    case N_GLOBAL:
	return (node *) NULL;

    case N_TST:
    case N_NOT:
	return c_minimize_exp_stmt(n->l.left);

    case N_LOR:
    case N_LAND:
	n->l.left = c_minimize_exp_stmt(n->l.left);
	n->r.right = c_minimize_exp_stmt(n->r.right);
	if (n->r.right == (node *) NULL) {
	    return n->l.left;
	}
	break;

    case N_COMMA:
	n->l.left = c_minimize_exp_stmt(n->l.left);
	n->r.right = c_minimize_exp_stmt(n->r.right);
	if (n->l.left == (node *) NULL) {
	    return n->r.right;
	}
	if (n->r.right == (node *) NULL) {
	    return n->l.left;
	}
	break;
    }

    return n;
}

/*
 * NAME:	compile->exp_stmt()
 * DESCRIPTION:	reduce an expression as a statement
 */
node *c_exp_stmt(n)
node *n;
{
    n = c_minimize_exp_stmt(n);
    if (n != (node *) NULL) {
	n = node_mon(N_POP, 0, n);
    }
}

/*
 * NAME:	compile->cond()
 * DESCRIPTION:	handle a condition
 */
static node *c_cond(n1, n2, n3, head)
register node *n1, *n2;
node *n3;
bool head;
{
    if (n1 == (node *) NULL) {
	/* always */
	return n2;
    }

    switch (n1->type) {
    case N_INT:
	/* always/never */
	return (n1->l.number != 0) ? n2 : n3;

    case N_STR:
	/* always */
	return n2;

    case N_COMMA:
	if (n1->r.right->type == N_STR) {
	    /* always */
	    break;
	} else if (n1->r.right->type == N_INT) {
	    /* always ... */
	    if (n1->r.right->l.number == 0) {
		/* ... no, never ... */
		n2 = n3;
	    }
	    /* ... with side effect */
	    break;
	}
    default:
	return n1;
    }

    /*
     * add "condition" as statement (preserving side effects)
     */
    n1 = c_exp_stmt(n1->l.left);
    if (n1 == (node *) NULL) {
	/* condition reduced to nothing */
	return n2;
    }
    if (n2 != (node *) NULL) {
	if (head) {
	    /* add before */
	    n1 = node_bin(N_PAIR, 0, n1, n2);
	} else {
	    /* add after */
	    n1 = node_bin(N_PAIR, 0, n2, n1);
	}
    }
    return n1;
}

/*
 * NAME:	compile->if()
 * DESCRIPTION:	handle an if statement
 */
node *c_if(n1, n2, n3)
node *n1, *n2, *n3;
{
    register node *n;

    if (n2 == (node *) NULL && n3 == (node *) NULL) {
	return c_exp_stmt(n1);
    } else {
	n = c_cond(n1, n2, n3, TRUE);
	if (n != n1) {
	    /* fixed condition */
	    return n;
	}
	return node_bin(N_IF, 0, n, node_bin(N_ELSE, 0, n2, n3));
    }
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
 * DESCRIPTION:	reloop a loop
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
    register node *n;
    node dummy;

    n2 = c_reloop(n2);
    n = c_cond(n1, n2, &dummy, FALSE);
    if (n == n1) {
	/* regular do loop */
	n = node_bin(N_DO, 0, n, n2);
    } else if (n != &dummy) {
	/* always true */
	n = node_mon(N_FOREVER, 0, n);
    } else if (n2 != (node *) NULL && (thisloop->brk || thisloop->cont)) {
	/* never true, but there is a break or continue statement */
	n2 = node_bin(N_PAIR, 0, n2, node_mon(N_BREAK, 0, (node *) NULL));
	thisloop->brk = TRUE;
	n = node_mon(N_FOREVER, 0, n2);	/* but done at most once */
    } else {
	/* never true: just once */
	n = n2;
    }
    return c_endloop(n);
}

/*
 * NAME:	compile->while()
 * DESCRIPTION:	end a while loop
 */
node *c_while(n1, n2)
register node *n1, *n2;
{
    register node *n;
    node dummy;

    n2 = c_reloop(n2);
    n = c_cond(n1, n2, &dummy, TRUE);
    if (n == n1) {
	/* regular while loop */
	n = node_bin(N_WHILE, 0, n, n2);
    } else if (n != &dummy) {
	/* always true */
	n = node_mon(N_FOREVER, 0, n);
    } else {
	/* never true */
	n = (node *) NULL;
    }
    return c_endloop(n);
}

/*
 * NAME:	compile->for()
 * DESCRIPTION:	end a for loop
 */
node *c_for(n1, n2, n3, n4)
register node *n1, *n2, *n3, *n4;
{
    node dummy;

    if (n1 != (node *) NULL) {
	c_list_exp(n1);
    }
    if (n2 != (node *) NULL) {
	c_list_exp(n2);
    }
    n4 = c_reloop(n4);
    if (n3 != (node *) NULL) {
	c_list_exp(n3);
	n3 = c_exp_stmt(n3);
	if (n3 != (node *) NULL) {
	    if (n4 == (node *) NULL) {
		/* empty */
		n4 = n3;
	    } else {
		/* append */
		n4 = node_bin(N_PAIR, 0, n4, n3);
	    }
	}
    }

    n3 = c_cond(n2, n4, &dummy, TRUE);
    if (n3 == n2) {
	/* regular for loop */
	n3 = node_bin(N_FOR, 0, n2, n4);
    } else if (n3 != &dummy) {
	/* always true */
	n3 = node_mon(N_FOREVER, 0, n3);
    } else {
	/* never true */
	n3 = (node *) NULL;
    }

    n3 = c_endloop(n3);
    if (n1 != (node *) NULL && (n1=c_exp_stmt(n1)) != (node *) NULL) {
	/* prefix */
	n3 = node_bin(N_PAIR, 0, n1, n3);
    }
    return n3;
}

/*
 * NAME:	compile->startswitch()
 * DESCRIPTION:	start a switch statement
 */
void c_startswitch(n)
register node *n;
{
    if (n->type == N_INT || n->type == N_STR) {
	yyerror("switch expression is constant");
    } else if (n->mod != T_NUMBER && n->mod != T_STRING && n->mod != T_MIXED) {
	yyerror("bad switch expression type (%s)", c_typename(n->mod));
    }
    switch_list = loop_new(switch_list);
    switch_list->type = (typechecking) ? n->mod : T_MIXED;
    switch_list->dflt = FALSE;
    switch_list->ncase = 0;
    switch_list->case_list = case_list;
    case_list = (node *) NULL;
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
    short type;

    n = (node *) NULL;
    if (switch_list->type != (char) T_ERROR) {
	if (stmt == (node *) NULL) {
	    yyerror("no statements in switch");
	} else if (stmt->type != N_CASE &&
		   (stmt->type != N_PAIR || stmt->l.left->type != N_CASE)) {
	    yyerror("unlabeled statements in switch");
	} else if ((size=switch_list->ncase - switch_list->dflt) == 0) {
	    yyerror("only default label in switch");
	} else {
	    /*
	     * get the labels in an array, and sort them
	     */
	    v = ALLOC(node*, size);
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
	    } else {
		register long l, h;

		type = N_SWITCH_INT;
		/*
		 * check for duplicate cases
		 */
		i = size;
		l = LONG_MAX;
		h = LONG_MIN;
		w = v;
		for (;;) {
		    if (l > w[0]->l.left->l.number) {
			l = w[0]->l.left->l.number;
		    }
		    if (h < w[0]->l.left->r.number) {
			h = w[0]->l.left->r.number;
		    }
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

		h -= l - 1;
		if (i == 0 && h > size) {
		    if (3L * h > 5L * size) {
			/*
			 * range
			 */
			type = N_SWITCH_RANGE;
		    } else {
			/*
			 * convert range label switch to int label switch
			 * by adding new labels
			 */
			w = ALLOC(node*, h);
			for (i = size; i > 0; --i) {
			    *w++ = *v;
			    for (l = v[0]->l.left->l.number;
				 l < v[0]->l.left->r.number; l++) {
				/* insert N_CASE in statement */
				n = node_mon(N_CASE, 0, v[0]->r.right->l.left);
				v[0]->r.right->l.left = n;
				*w++ = node_bin(N_PAIR, 0, node_int((Int)l), n);
			    }
			    (*v++)->l.left->l.number = l;
			}
			FREE(v - size);
			size = h;
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
	    FREE(v);
	    if (switch_list->dflt) {
		/* add default case */
		n = node_bin(N_PAIR, 0, (node *) NULL, n);
		size++;
	    }

	    if (switch_list->brk) {
		stmt = node_mon(N_BLOCK, N_BREAK, stmt);
	    }
	    n = node_bin(type, size, n, node_bin(N_PAIR, 0, expr, stmt));
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
    if (switch_list->type == (char) T_ERROR) {
	return (node *) NULL;
    }

    if (n1->type == N_INT) {
	/* int */
	if (n2 != (node *) NULL) {
	    /* range */
	    if (n2->type != N_INT) {
		yyerror("non-integer constant case range in switch");
		switch_list->type = T_ERROR;
		return (node *) NULL;
	    }
	    if (n2->l.number < n1->l.number) {
		yyerror("inverted case range in switch");
		switch_list->type = T_ERROR;
		return (node *) NULL;
	    } else if (n2->l.number != n1->l.number) {
		n1->type = N_RANGE;
		n1->r.number = n2->l.number;
	    }
	}
	/* compare type with other cases */
	if (n1->l.number != 0 || n2 != (node *) NULL) {
	    if (switch_list->type == T_MIXED) {
		switch_list->type = T_NUMBER;
	    } else if (switch_list->type != T_NUMBER) {
		yyerror("multiple case types in switch");
		switch_list->type = T_ERROR;
		return (node *) NULL;
	    }
	}
    } else {
	/* string */
	if (n2 != (node *) NULL) {
	    yyerror("non-integer constant case in switch");
	    switch_list->type = T_ERROR;
	    return (node *) NULL;
	}
	if (n1->type != N_STR) {
	    yyerror("non-constant case in switch");
	    switch_list->type = T_ERROR;
	    return (node *) NULL;
	}
	/* compare type with other cases */
	if (switch_list->type == T_MIXED) {
	    switch_list->type = T_STRING;
	} else if (switch_list->type != T_STRING) {
	    yyerror("multiple case types in switch");
	    switch_list->type = T_ERROR;
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
	switch_list->type = T_ERROR;
    } else if (switch_list->dflt) {
	yyerror("duplicate default label in switch");
	switch_list->type = T_ERROR;
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

    l = thisloop;
    if (l == (loop *) NULL) {
	l = switch_list;
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
 * DESCRIPTION:	handle a return statement
 */
short c_ftype()
{
    return ftype;
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
	return node_mon(N_CATCH, 0, n);
    } else if (strcmp(n->l.string->text, "lock") == 0) {
	if (strcmp(current->file, auto_object) != 0) {
	    yyerror("lock() can only be used in auto object");
	}
	return node_mon(N_LOCK, 0, n);
    } else {
	char *proto;
	long call;

	proto = ctrl_fcall(n->l.string, &call, typechecking);
	n->r.right = (proto == (char *) NULL) ? (node *) NULL :
		      node_fcall(PROTO_FTYPE(proto), proto, call);
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
		  node_fcall(PROTO_FTYPE(proto), proto, call);
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
 * NAME:	funcall()
 * DESCRIPTION:	handle a function call
 */
static node *funcall(func, args)
register node *func, *args;
{
    register int n, nargs, typechecked;
    register node *arg;
    char *argp, *proto, *fname;

    /* get info, prepare return value */
    fname = func->l.string->text;
    func = func->r.right;
    if (func == (node *) NULL) {
	/* error during function lookup */
	return node_mon(N_FAKE, T_MIXED, (node *) NULL);
    }
    proto = func->l.ptr;
    func->mod = PROTO_FTYPE(proto);
    func->l.left = args;

    /*
     * check function arguments
     */
    nargs = PROTO_NARGS(proto);
    typechecked = PROTO_CLASS(proto) & C_TYPECHECKED;
    for (n = 1, argp = PROTO_ARGS(proto); n <= nargs; n++, argp++) {
	if (args == (node *) NULL) {
	    if (typechecked && !(PROTO_CLASS(proto) & C_VARARGS)) {
		yyerror("too few arguments for function %s", fname);
	    }
	    return func;
	}
	if (args->type == N_COMMA) {
	    arg = args->l.left;
	    args = args->r.right;
	} else {
	    arg = args;
	    args = (node *) NULL;
	}
	if (UCHAR(*argp) == T_LVALUE) {
	    switch (arg->type) {
	    case N_GLOBAL:
		arg->type = N_GLOBAL_LVALUE;
		break;

	    case N_LOCAL:
		arg->type = N_LOCAL_LVALUE;
		break;

	    case N_INDEX:
		switch (arg->l.left->type) {
		case N_LOCAL:
		    arg->l.left->type = N_LOCAL_LVALUE;
		    break;

		case N_GLOBAL:
		    arg->l.left->type = N_GLOBAL_LVALUE;
		    break;

		case N_INDEX:
		    arg->l.left->type = N_INDEX_LVALUE;
		    break;
		}
		arg->type = N_INDEX_LVALUE;
		break;

	    default:
		yyerror("bad argument %d for function %s (needs lvalue)",
			n, fname);
		break;
	    }
	} else if (typechecked && c_tmatch(arg->mod, UCHAR(*argp)) == T_ERROR &&
		   (arg->type != N_INT || arg->l.number != 0)) {
	    yyerror("bad argument %d for function %s (needs %s)", n, fname,
		    c_typename(UCHAR(*argp)));
	}
    }
    if (typechecked && args != (node *) NULL &&
	!(PROTO_CLASS(proto) & C_VARARGS)) {
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
	    func->mod = args->mod;
	    c_list_exp(args);
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
    return funcall(c_flookup(node_str(str_new("call_other", 10L))),
		   node_bin(N_COMMA, 0, other,
			    node_bin(N_COMMA, 0, func,
				     revert_list(args, N_COMMA))));
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
    case N_NE:
    case N_GT:
    case N_GE:
    case N_LT:
    case N_LE:
	return n;

    case N_UMIN:
	if (n->l.left->mod == T_NUMBER) {
	    n = n->l.left;
	}
	break;
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

    case N_TST:
	n->type = N_NOT;
	return n;

    case N_NOT:
	n->type = N_TST;
	return n;

    case N_EQ:
	n->type = N_NE;
	return n;

    case N_NE:
	n->type = N_EQ;
	return n;

    case N_GT:
	n->type = N_LE;
	return n;

    case N_GE:
	n->type = N_LT;
	return n;

    case N_LT:
	n->type = N_GE;
	return n;

    case N_LE:
	n->type = N_GT;
	return n;

    case N_UMIN:
	if (n->l.left->mod == T_NUMBER) {
	    n = n->l.left;
	}
	break;
    }

    return node_mon(N_NOT, T_NUMBER, n);
}

/*
 * NAME:	compile->lvalue()
 * DESCRIPTION:	handle an lvalue
 */
node *c_lvalue(n, oper)
register node *n;
char *oper;
{
    switch (n->type) {
    case N_GLOBAL:
	n->type = N_GLOBAL_LVALUE;
	break;

    case N_LOCAL:
	n->type = N_LOCAL_LVALUE;
	break;

    case N_INDEX:
	switch (n->l.left->type) {
	case N_LOCAL:
	    n->l.left->type = N_LOCAL_LVALUE;
	    break;

	case N_GLOBAL:
	    n->l.left->type = N_GLOBAL_LVALUE;
	    break;

	case N_INDEX:
	    n->l.left->type = N_INDEX_LVALUE;
	    break;
	}
	n->type = N_INDEX_LVALUE;
	break;

    case N_FAKE:
	break;

    default:
	yyerror("bad lvalue for %s", oper);
	break;
    }
    return n;
}

/*
 * NAME:	compile->match()
 * DESCRIPTION:	See if the two supplied types are compatible. If so, return the
 *		combined type. If not, return T_ERROR.
 */
short c_tmatch(type1, type2)
register unsigned short type1, type2;
{
    if (type1 == type2) {
	/* identical types */
	return type1;
    }
    if ((type1 & T_TYPE) == T_MIXED && (type1 & T_REF) <= (type2 & T_REF)) {
	/* mixed <-> int,  mixed * <-> int *,  mixed * <-> int ** */
	if (type1 == T_MIXED) {
	    if (type2 & T_REF) {
		return T_MIXED | (1 << REFSHIFT);	/* mixed <-> int * */
	    } else {
		return type2;		/* mixed <-> int */
	    }
	}
	return type1;
    }
    if ((type2 & T_TYPE) == T_MIXED && (type1 & T_REF) >= (type2 & T_REF)) {
	/* int <-> midex,  int * <-> mixed *,  int ** <-> mixed * */
	if (type2 == T_MIXED && (type1 & T_REF)) {
	    if (type1 & T_REF) {
		return T_MIXED | (1 << REFSHIFT);	/* int * <-> mixed */
	    } else {
		return type1;		/* int <-> mixed */
	    }
	}
	return type2;
    }
    return T_ERROR;
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
