# include "comp.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "node.h"
# include "control.h"
# include "codegen.h"

static int tabs;

static void out(f, a)
char *f, *a;
{
    register int i;

    for (i = tabs; i > 0; --i) putchar(' ');
    printf(f, a);
    putchar('\n');
    fflush(stdout);
}

static void cg_expr P((node*));

/*
 * NAME:	codegen->aggr()
 * DESCRIPTION:	generate code for an aggregate
 */
static int cg_aggr(n)
register node *n;
{
    register int i;

    if (n == (node *) NULL) {
	return 0;
    }
    for (i = 1; n->type == N_COMMA; i++) {
	cg_expr(n->r.right);
	n = n->l.left;
    }
    cg_expr(n);
    return i;
}

/*
 * NAME:	codegen->map_aggr()
 * DESCRIPTION:	generate code for a mapping aggregate
 */
static int cg_map_aggr(n)
register node *n;
{
    register int i;

    if (n == (node *) NULL) {
	return 0;
    }
    for (i = 2; n->type == N_COMMA; i += 2) {
	cg_expr(n->r.right->r.right);
	cg_expr(n->r.right->l.left);
	n = n->l.left;
    }
    cg_expr(n->r.right);
    cg_expr(n->l.left);
    return i;
}

/*
 * NAME:	codegen->funargs()
 * DESCRIPTION:	generate code for function arguments
 */
static int cg_funargs(n)
register node *n;
{
    register int i;

    if (n == (node *) NULL) {
	return 0;
    }
    for (i = 1; n->type == N_COMMA; i++) {
	cg_expr(n->l.left);
	n = n->r.right;
    }
    cg_expr(n);
    return i;
}

/*
 * NAME:	codegen->expr()
 * DESCRIPTION:	generate code for an expression
 */
static void cg_expr(n)
register node *n;
{
    tabs++;
    switch (n->type) {
    case N_ADD:
	out("N_ADD");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_ADD_EQ:
	out("N_ADD_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_AGGR:
	if (n->mod == T_MAPPING) {
	    out("N_AGGR T_MAPPING");
	    cg_map_aggr(n->l.left);
	} else {
	    out("N_AGGR T_ARRAY");
	    cg_aggr(n->l.left);
	}
	break;

    case N_AND:
	out("N_AND");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_AND_EQ:
	out("N_AND_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_ASSIGN:
	out("N_ASSIGN");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_CAST:
	out("N_CAST");
	cg_expr(n->l.left);

    case N_CATCH:
	out("N_CATCH");
	cg_expr(n->l.left);
	break;

    case N_COMMA:
	out("N_COMMA");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_DIV:
	out("N_DIV");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_DIV_EQ:
	out("N_DIV_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_EQ:
	out("N_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_FUNC:
	switch (n->r.number >> 24) {
	case KFCALL:
	    out("N_FUNC kfun2 %d", UCHAR(n->r.number));
	    break;

	case LFCALL:
	    out("N_FUNC lfun %06X", n->r.number);
	    break;

	case DFCALL:
	    out("N_FUNC dfun %04x", (int) n->r.number);
	    break;

	case FCALL:
	    out("N_FUNC fun %u", (unsigned short) n->r.number);
	    break;
	}
	cg_funargs(n->l.left);
	break;

    case N_GE:
	out("N_GE");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_GLOBAL:
	out("N_GLOBAL %04x", (int) n->r.number);
	break;

    case N_GLOBAL_LVALUE:
	out("N_GLOBAL_LVALUE %04x", (int) n->r.number);
	break;

    case N_GT:
	out("N_GT");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_INDEX_INT:
	out("N_INDEX_INT");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_INDEX_INT_LVALUE:
	out("N_INDEX_INT_LVALUE");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_INDEX_VALUE:
	out("N_INDEX_VALUE");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_INDEX_VALUE_LVALUE:
	out("N_INDEX_VALUE_LVALUE");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_INT:
	out("N_INT %ld", n->l.number);
	break;

    case N_LAND:
	out("N_AND");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_LE:
	out("N_LE");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_LOCAL:
	out("N_LOCAL %d", (int) n->r.number);
	break;

    case N_LOCAL_LVALUE:
	out("N_LOCAL_LVALUE %d", (int) n->r.number);
	break;

    case N_LOCK:
	out("N_LOCK");
	cg_expr(n->l.left);
	break;

    case N_LOR:
	out("N_LOR");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_LSHIFT:
	out("N_LSHIFT");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_LSHIFT_EQ:
	out("N_LSHIFT_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_LT:
	out("N_LT");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_MIN:
	out("N_MIN");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_MIN_EQ:
	out("N_MIN_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_MOD:
	out("N_MOD");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_MOD_EQ:
	out("N_MOD_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_MULT:
	out("N_MULT");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_MULT_EQ:
	out("N_MULT_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_NE:
	out("N_NE");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_NEG:
	out("N_NEG");
	cg_expr(n->l.left);
	break;

    case N_NOT:
	out("N_NOT");
	cg_expr(n->l.left);
	break;

    case N_OR:
	out("N_OR");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_OR_EQ:
	out("N_OR_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_QUEST:
	out("N_QUEST");
	cg_expr(n->l.left);
	cg_expr(n->r.right->l.left);
	cg_expr(n->r.right->r.right);
	break;

    case N_RANGE:
	out("N_RANGE");
	cg_expr(n->l.left);
	cg_expr(n->r.right->l.left);
	cg_expr(n->r.right->r.right);
	break;

    case N_RSHIFT:
	out("N_RSHIFT");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_RSHIFT_EQ:
	out("N_RSHIFT");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_STR:
	out("N_STR %06X", ctrl_dstring(n->l.string));
	break;

    case N_TST:
	out("N_TST");
	cg_expr(n->l.left);
	break;

    case N_UMIN:
	out("N_UMIN");
	cg_expr(n->l.left);
	break;

    case N_XOR:
	out("N_XOR");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_XOR_EQ:
	out("N_XOR_EQ");
	cg_expr(n->l.left);
	cg_expr(n->r.right);
	break;

    case N_MIN_MIN:
	out("N_MIN_MIN");
	cg_expr(n->l.left);
	break;

    case N_PLUS_PLUS:
	out("N_PLUS_PLUS");
	cg_expr(n->l.left);
	break;
    }
    --tabs;
}

/*
 * NAME:	codegen->stmt()
 * DESCRIPTION:	generate code for a statement
 */
static void cg_stmt(n)
register node *n;
{
    register node *m;

    tabs++;
    while (n != (node *) NULL) {
	if (n->type == N_PAIR) {
	    m = n->l.left;
	    n = n->r.right;
	} else {
	    m = n;
	    n = (node *) NULL;
	}
	if (m != (node *) NULL) {
	    switch (m->type) {
	    case N_BLOCK:
		if (m->mod == N_BREAK) {
		    out("N_BLOCK N_BREAK");
		} else {
		    out("N_BLOCK N_CONTINUE");
		}
		cg_stmt(m->l.left);
		break;

	    case N_BREAK:
		out("N_BREAK");
		break;

	    case N_CASE:
		out("N_CASE %d", m->mod);
		cg_stmt(m->l.left);
		break;

	    case N_CONTINUE:
		out("N_CONTINUE");
		break;

	    case N_DO:
		out("N_DO");
		cg_stmt(m->r.right);
		cg_expr(m->l.left);
		break;

	    case N_FOR:
		out("N_FOR");
		cg_expr(m->l.left);
		cg_stmt(m->r.right);
		break;

	    case N_WHILE:
		out("N_WHILE");
		cg_expr(m->l.left);
		cg_stmt(m->r.right);
		break;

	    case N_FOREVER:
		out("N_FOREVER");
		cg_stmt(m->l.left);
		break;

	    case N_IF:
		out("N_IF");
		cg_expr(m->l.left);
		cg_stmt(m->r.right->l.left);
		if (m->r.right->r.right != (node *) NULL) {
		    cg_stmt(m->r.right->r.right);
		}
		break;

	    case N_PAIR:
		out("N_PAIR");
		cg_stmt(m);
		break;

	    case N_POP:
		out("N_POP");
		cg_expr(m->l.left);
		break;

	    case N_RETURN:
		out("N_RETURN");
		cg_expr(m->l.left);
		break;

	    case N_SWITCH_INT:
		out("N_SWITCH_INT");
		break;

	    case N_SWITCH_RANGE:
		out("N_SWITCH_RANGE");
		break;

	    case N_SWITCH_STR:
		out("N_SWITCH_STR");
		break;
	    }
	}
    }
    --tabs;
}

/*
 * NAME:	codegen->function()
 * DESCRIPTION:	generate code for a function
 */
char *cg_function(n, nvar, npar, size)
node *n;
int nvar, npar;
unsigned short *size;
{
    static char foo[] = "function";

    tabs = -1;
    cg_stmt(n);
    *size = 8;
    return (char *) memcpy(ALLOC(char, 8), foo, 8);
}

void cg_clear()
{
}
