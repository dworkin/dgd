# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "fcontrol.h"
# include "table.h"
# include "node.h"
# include "control.h"
# include "codegen.h"

# define PUSH		0
# define POP		1
# define INTVAL		2
# define TRUTHVAL	3

static void output();
static void cg_iexpr P((node*));
static void cg_expr P((node*, int));
static void cg_stmt P((node*));

static int vars[MAX_LOCALS];	/* local variable types */
static int nvars;		/* number of local variables */
static int tvc;			/* tmpval count */

/*
 * NAME:	comma()
 * DESCRIPTION:	output a comma
 */
static void comma()
{
    output(",\n");
}

/*
 * NAME:	kfun()
 * DESCRIPTION:	output a kfun call
 */
static void kfun(n)
int n;
{
    output("call_kfun(%d/*%s*/)", n, kftab[n].name);
}

/*
 * NAME:	kfun_arg()
 * DESCRIPTION:	output a kfun call with a specified number of arguments
 */
static void kfun_arg(n, nargs)
int n;
int nargs;
{
    output("call_kfun_arg(%d/*%s*/, %d)", n, kftab[n].name, nargs);
}

/*
 * NAME:	store()
 * DESCRIPTION:	output store code
 */
static void store()
{
    output(", store()");
}

/*
 * NAME:	codegen->lvalue()
 * DESCRIPTION:	generate code for an lvalue
 */
static void cg_lvalue(n)
node *n;
{
    switch (n->type) {
    case N_LOCAL:
	output("push_lvalue(fp + %d)", nvars - (int) n->r.number - 1);
	break;

    case N_GLOBAL:
	output("i_global_lvalue(%d, %d)", ((int) n->r.number >> 8) & 0xff, 
	       ((int) n->r.number) & 0xff);
	break;

    case N_INDEX:
	switch (n->l.left->type) {
	case N_LOCAL:
	    output("push_lvalue(fp + %d)",
		   nvars - (int) n->l.left->r.number - 1);
	    break;

	case N_GLOBAL:
	    output("i_global_lvalue(%d, %d)",
		   ((int) n->l.left->r.number >> 8) & 0xff,
		   ((int) n->l.left->r.number) & 0xff);
	    break;

	case N_INDEX:
	    cg_expr(n->l.left->l.left, PUSH);
	    comma();
	    cg_expr(n->l.left->r.right, PUSH);
	    output(", i_index_lvalue()");
	    break;

	default:
	    cg_expr(n->l.left, PUSH);
	    break;
	}
	comma();
	cg_expr(n->r.right, PUSH);
	output(", i_index_lvalue()");
	break;
    }
}

/*
 * NAME:	codegen->fetch()
 * DESCRIPTION:	generate code for a fetched lvalue
 */
static void cg_fetch(n)
node *n;
{
    if (n->type == N_CAST) {
	cg_lvalue(n->l.left);
	output(", i_fetch(), check_int(sp), ");
    } else {
	cg_lvalue(n);
	output(", i_fetch(), ");
    }
}

/*
 * NAME:	tmpval()
 * DESCRIPTION:	return a new temporary value index
 */
static int tmpval()
{
    if (tvc == NTMPVAL - 1) {
	yyerror("too many temporary values (%d)", tvc + 1);
    }
    return tvc++;
}

/*
 * NAME:	codegen->iasgnop()
 * DESCRIPTION:	handle an integer assignment operator
 */
static void cg_iasgnop(n, op)
register node *n;
char *op;
{
    if (n->l.left->type == N_LOCAL) {
	output("ivar%d %s ", vars[n->l.left->r.number], op);
	cg_iexpr(n->r.right);
    } else {
	cg_fetch(n->l.left);
	output("sp->u.number %s ", op);
	cg_iexpr(n->r.right);
	output(", store_int()");
    }
}

/*
 * NAME:	codegen->ifasgnop()
 * DESCRIPTION:	handle a function integer assignment operator
 */
static void cg_ifasgnop(n, op)
register node *n;
char *op;
{
    if (n->l.left->type == N_LOCAL) {
	output("ivar%d = %s(ivar%d, ", vars[n->l.left->r.number], op,
	       vars[n->l.left->r.number]);
	cg_iexpr(n->r.right);
	output(")");
    } else {
	cg_fetch(n->l.left);
	output("sp->u.number = %s(sp->u.number, ", op);
	cg_iexpr(n->r.right);
	output("), store_int()");
    }
}

/*
 * NAME:	codegen->iexpr()
 * DESCRIPTION:	generate code for an integer expression
 */
static void cg_iexpr(n)
register node *n;
{
    register int i;

    output("(");
    switch (n->type) {
    case N_ADD:
    case N_ADD_EQ:
    case N_AGGR:
    case N_AND:
    case N_AND_EQ:
    case N_ASSIGN:
    case N_CATCH:
    case N_DIV:
    case N_DIV_EQ:
    case N_EQ:
    case N_FUNC:
    case N_GE:
    case N_GLOBAL:
    case N_GT:
    case N_INDEX:
    case N_LE:
    case N_LOCK:
    case N_LSHIFT:
    case N_LSHIFT_EQ:
    case N_LT:
    case N_LVALUE:
    case N_MOD:
    case N_MOD_EQ:
    case N_MULT:
    case N_MULT_EQ:
    case N_NE:
    case N_OR:
    case N_OR_EQ:
    case N_PAIR:
    case N_RANGE:
    case N_RSHIFT:
    case N_RSHIFT_EQ:
    case N_STR:
    case N_SUB:
    case N_SUB_EQ:
    case N_XOR:
    case N_XOR_EQ:
    case N_MIN_MIN:
    case N_PLUS_PLUS:
	i = tmpval();
	output("tv[%d] = (", i);
	cg_expr(n, INTVAL);
	output("), tv[%d]", i);
	break;

    case N_ADD_INT:
	cg_iexpr(n->l.left);
	if (n->r.right->type != N_INT || n->r.right->l.number != 0) {
	    output(" + ");
	    cg_iexpr(n->r.right);
	}
	break;

    case N_ADD_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 0) {
	    cg_iexpr(n->l.left);
	} else {
	    cg_iasgnop(n, "+=");
	}
	break;

    case N_AND_INT:
	cg_iexpr(n->l.left);
	if (n->r.right->type != N_INT || n->r.right->l.number != -1) {
	    output(" & ");
	    cg_iexpr(n->r.right);
	}
	break;

    case N_AND_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    cg_iexpr(n->l.left);
	} else {
	    cg_iasgnop(n, "&=");
	}
	break;

    case N_CAST:
	if (n->l.left->type == N_LOCAL) {
	    output("check_int(fp + %d), fp[%d].u.number", n->l.left->r.number,
		   n->l.left->r.number);
	} else {
	    cg_expr(n->l.left, PUSH);
	    i = tmpval();
	    output(", check_int(sp), tv[%d] = (sp++)->u.number, tv[%d]", i, i);
	}
	break;

    case N_COMMA:
	cg_expr(n->l.left, POP);
	comma();
	cg_iexpr(n->r.right);
	break;

    case N_DIV_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 1) {
	    cg_iexpr(n->l.left);
	} else {
	    output("xdiv(");
	    cg_iexpr(n->l.left);
	    comma();
	    cg_iexpr(n->r.right);
	    output(")");
	}
	break;

    case N_DIV_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 1) {
	    cg_iexpr(n->l.left);
	} else {
	    cg_ifasgnop(n, "xdiv");
	}
	break;

    case N_EQ_INT:
	cg_iexpr(n->l.left);
	output(" == ");
	cg_iexpr(n->r.right);
	break;

    case N_GE_INT:
	cg_iexpr(n->l.left);
	output(" >= ");
	cg_iexpr(n->r.right);
	break;

    case N_GT_INT:
	cg_iexpr(n->l.left);
	output(" > ");
	cg_iexpr(n->r.right);
	break;

    case N_INT:
	output("(Int) %ldL", (long) n->l.number);
	break;

    case N_LAND:
	output("(");
	cg_expr(n->l.left, TRUTHVAL);
	output(") && (");
	cg_expr(n->r.right, TRUTHVAL);
	output(")");
	break;

    case N_LE_INT:
	cg_iexpr(n->l.left);
	output(" <= ");
	cg_iexpr(n->r.right);
	break;

    case N_LOCAL:
	output("ivar%d", vars[n->r.number]);
	break;

    case N_LOR:
	output("(");
	cg_expr(n->l.left, TRUTHVAL);
	output(") || (");
	cg_expr(n->r.right, TRUTHVAL);
	output(")");
	break;

    case N_LSHIFT_INT:
	cg_iexpr(n->l.left);
	if (n->r.right->type != N_INT || n->r.right->l.number != 0) {
	    output(" << ");
	    cg_iexpr(n->r.right);
	}
	break;

    case N_LSHIFT_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 0) {
	    cg_iexpr(n->l.left);
	} else {
	    cg_iasgnop(n, "<<=");
	}
	break;

    case N_LT_INT:
	cg_iexpr(n->l.left);
	output(" < ");
	cg_iexpr(n->r.right);
	break;

    case N_MOD_INT:
	output("xmod(");
	cg_iexpr(n->l.left);
	output(", ");
	cg_iexpr(n->r.right);
	output(")");
	break;

    case N_MOD_EQ_INT:
	cg_ifasgnop(n, "xmod");
	break;

    case N_MULT_INT:
	cg_iexpr(n->l.left);
	if (n->r.right->type != N_INT || n->r.right->l.number != 1) {
	    output(" * ");
	    cg_iexpr(n->r.right);
	}
	break;

    case N_MULT_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 1) {
	    cg_iexpr(n->l.left);
	} else {
	    cg_iasgnop(n, "*=");
	}
	break;

    case N_NE_INT:
	cg_iexpr(n->l.left);
	output(" != ");
	cg_iexpr(n->r.right);
	break;

    case N_NOT:
	if (n->l.left->mod == T_NUMBER) {
	    output("!");
	    cg_iexpr(n->l.left);
	} else {
	    cg_expr(n, TRUTHVAL);
	}
	break;

    case N_OR_INT:
	cg_iexpr(n->l.left);
	if (n->r.right->type != N_INT || n->r.right->l.number != 0) {
	    output(" | ");
	    cg_iexpr(n->r.right);
	}
	break;

    case N_OR_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 0) {
	    cg_iexpr(n->l.left);
	} else {
	    cg_iasgnop(n, "|=");
	}
	break;

    case N_QUEST:
	output("(");
	cg_expr(n->l.left, TRUTHVAL);
	output(") ? ");
	cg_iexpr(n->r.right->l.left);
	output(" : ");
	cg_iexpr(n->r.right->r.right);
	break;

    case N_RSHIFT_INT:
	cg_iexpr(n->l.left);
	if (n->r.right->type != N_INT || n->r.right->l.number != 0) {
	    output(" >> ");
	    cg_iexpr(n->r.right);
	}
	break;

    case N_RSHIFT_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 0) {
	    cg_iexpr(n->l.left);
	} else {
	    cg_iasgnop(n, ">>=");
	}
	break;

    case N_SUB_INT:
	if (n->l.left->type == N_INT && n->l.left->l.number == 0) {
	    output("-");
	    cg_iexpr(n->r.right);
	} else {
	    cg_iexpr(n->l.left);
	    output(" - ");
	    cg_iexpr(n->r.right);
	}
	break;

    case N_SUB_EQ_INT:
	cg_iasgnop(n, "-=");
	break;

    case N_TST:
	if (n->l.left->mod == T_NUMBER) {
	    output("!!");
	    cg_iexpr(n->l.left);
	} else {
	    cg_expr(n->l.left, TRUTHVAL);
	}
	break;

    case N_XOR_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 0) {
	    cg_iexpr(n->l.left);
	} else if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    output("~");
	    cg_iexpr(n->l.left);
	} else {
	    cg_iexpr(n->l.left);
	    output(" ^ ");
	    cg_iexpr(n->r.right);
	}
	break;

    case N_XOR_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 0) {
	    cg_iexpr(n->l.left);
	} else {
	    cg_iasgnop(n, "^=");
	}
	break;

    case N_MIN_MIN_INT:
	if (n->l.left->type == N_LOCAL) {
	    output("ivar%d--", vars[n->l.left->r.number]);
	} else {
	    cg_fetch(n->l.left);
	    output("sp->u.number--, store_int() + 1");
	}
	break;

    case N_PLUS_PLUS_INT:
	if (n->l.left->type == N_LOCAL) {
	    output("ivar%d++", vars[n->l.left->r.number]);
	} else {
	    cg_fetch(n->l.left);
	    output("sp->u.number++, store_int() - 1");
	}
	break;
    }
    output(")");
}

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
	cg_expr(n->r.right, PUSH);
	comma();
	n = n->l.left;
    }
    cg_expr(n, PUSH);
    comma();
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
	cg_expr(n->r.right->r.right, PUSH);
	comma();
	cg_expr(n->r.right->l.left, PUSH);
	comma();
	n = n->l.left;
    }
    cg_expr(n->r.right, PUSH);
    comma();
    cg_expr(n->l.left, PUSH);
    comma();
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
	cg_expr(n->l.left, PUSH);
	comma();
	n = n->r.right;
    }
    cg_expr(n, PUSH);
    comma();
    return i;
}

/*
 * NAME:	codegen->locals()
 * DESCRIPTION:	generate code for assignments to local integer variables
 */
static void cg_locals(n)
register node *n;
{
    register node *m;

    while (n != (node *) NULL) {
	if (n->type == N_COMMA) {
	    m = n->l.left->l.left;
	    n = n->r.right;
	} else {
	    m = n->l.left;
	    n = (node *) NULL;
	}
	output(", check_int(fp + %d), ivar%d = fp[%d].u.number",
	       nvars - (int) m->r.number - 1, vars[m->r.number],
	       nvars - (int) m->r.number - 1);
    }
}

/*
 * NAME:	codegen->expr()
 * DESCRIPTION:	generate code for an expression
 */
static void cg_expr(n, state)
register node *n;
register int state;
{
    register int i;
    long l;

    switch (n->type) {
    case N_ADD_INT:
    case N_ADD_EQ_INT:
    case N_AND_INT:
    case N_AND_EQ_INT:
    case N_DIV_INT:
    case N_DIV_EQ_INT:
    case N_EQ_INT:
    case N_GE_INT:
    case N_GT_INT:
    case N_INT:
    case N_LAND:
    case N_LE_INT:
    case N_LOR:
    case N_LSHIFT_INT:
    case N_LSHIFT_EQ_INT:
    case N_LT_INT:
    case N_MOD_INT:
    case N_MOD_EQ_INT:
    case N_MULT_INT:
    case N_MULT_EQ_INT:
    case N_NE_INT:
    case N_OR_INT:
    case N_OR_EQ_INT:
    case N_RSHIFT_INT:
    case N_RSHIFT_EQ_INT:
    case N_SUB_INT:
    case N_SUB_EQ_INT:
    case N_XOR_INT:
    case N_XOR_EQ_INT:
    case N_MIN_MIN_INT:
    case N_PLUS_PLUS_INT:
	if (state == PUSH) {
	    output("PUSH_NUMBER ");
	}
	cg_iexpr(n);
	return;

    case N_ADD:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_ADD);
	break;

    case N_ADD_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_ADD);
	store();
	break;

    case N_AGGR:
	if (n->mod == T_MAPPING) {
	    output("i_map_aggregate(%u)", cg_map_aggr(n->l.left));
	} else {
	    output("i_aggregate(%u)", cg_aggr(n->l.left));
	}
	break;

    case N_AND:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_AND);
	break;

    case N_AND_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_AND);
	store();
	break;

    case N_ASSIGN:
	if (n->l.left->type == N_LOCAL && vars[n->l.left->r.number] != 0) {
	    if (state == PUSH) {
		output("PUSH_NUMBER ");
	    }
	    output("ivar%d = ", vars[n->l.left->r.number]);
	    cg_iexpr(n->r.right);
	    return;
	}
	if (n->l.left->type == N_CAST) {
	    /* ignore cast */
	    cg_lvalue(n->l.left->l.left);
	} else {
	    cg_lvalue(n->l.left);
	}
	comma();
	cg_expr(n->r.right, PUSH);
	store();
	break;

    case N_CAST:
	cg_expr(n->l.left, PUSH);
	output(", check_int(sp)");	/* always cast to int */
	break;

    case N_CATCH:
	if (n->l.left == (node *) NULL) {
	    /* nothing to catch */
	    if (state == PUSH) {
		output("PUSH_NUMBER 0");
	    } else {
		output("0");
	    }
	} else {
	    output("(!ec_push() ? (");
	    cg_expr(n->l.left, POP);
	    if (state == PUSH) {
		output(", PUSH_NUMBER 0) : ");
		output("(p = errormesg(), (--sp)->type = T_STRING, ");
		output("str_ref(sp->u.string = str_new(p, (long)strlen(p)))))");
	    } else {
		output(", 0) : 0)");
	    }
	}
	return;

    case N_COMMA:
	cg_expr(n->l.left, POP);
	comma();
	cg_expr(n->r.right, state);
	return;

    case N_DIV:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_DIV);
	break;

    case N_DIV_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_DIV);
	store();
	break;

    case N_EQ:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_EQ);
	break;

    case N_FUNC:
	i = cg_funargs(n->l.left);
	switch (n->r.number >> 24) {
	case KFCALL:
	    if (PROTO_CLASS(kftab[(short) n->r.number].proto) & C_VARARGS) {
		kfun_arg((short) n->r.number, i);
	    } else {
		kfun((short) n->r.number);
	    }
	    break;

	case LFCALL:
	    output("i_funcall((object *) NULL, %d, %d, %d, %d)",
		   ((int) n->r.number >> 16) & 0xff,
		   ((int) n->r.number >> 8) & 0xff,
		   ((int) n->r.number) & 0xff, i);
	    break;

	case DFCALL:
	    output("i_funcall((object *) NULL, 0, %d, %d, %d)",
		   ((int) n->r.number >> 8) & 0xff,
		   ((int) n->r.number) & 0xff, i);
	    break;

	case FCALL:
	    output("p = i_foffset(%u), ", (unsigned short) n->r.number);
	    output("i_funcall((object *) NULL, 0, UCHAR(p[0]), UCHAR(p[1]),");
	    output(" %d)", i);
	    break;
	}
	break;

    case N_GE:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_GE);
	break;

    case N_GLOBAL:
	if (state == POP) {
	    output("0");
	    return;
	}
	output("i_global(%d, %d)", ((int) n->r.number >> 8) & 0xff,
	       ((int) n->r.number) & 0xff);
	break;

    case N_GT:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_GT);
	break;

    case N_INDEX:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	output(", i_index()");
	break;

    case N_LE:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_LE);
	break;

    case N_LOCAL:
	if (vars[n->r.number] != 0) {
	    if (state == PUSH) {
		output("PUSH_NUMBER ");
	    }
	    output("ivar%d", vars[n->r.number]);
	    return;
	}
	if (state == TRUTHVAL) {
	    output("truthval(fp + %d)", nvars - (int) n->r.number - 1);
	    return;
	}
	output("i_push_value(fp + %d)", nvars - (int) n->r.number - 1);
	break;

    case N_LOCK:
	if (state == POP) {
	    output("i_lock(), ");
	    cg_expr(n->l.left, POP);
	    output(", i_unlock()");
	    return;
	}
	output("i_lock(), ");
	cg_expr(n->l.left, PUSH);
	output(", i_unlock()");
	break;

    case N_LSHIFT:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_LSHIFT);
	break;

    case N_LSHIFT_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_LSHIFT);
	store();
	break;

    case N_LT:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_LT);
	break;

    case N_LVALUE:
	cg_lvalue(n->l.left);
	break;

    case N_MOD:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_MOD);
	break;

    case N_MOD_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_MOD);
	store();
	break;

    case N_MULT:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_MULT);
	break;

    case N_MULT_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_MULT);
	store();
	break;

    case N_NE:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_NE);
	break;

    case N_NOT:
	if (state == PUSH) {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    kfun(KF_NOT);
	} else {
	    output("!(");
	    cg_expr(n->l.left, TRUTHVAL);
	    output(")");
	}
	return;

    case N_OR:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_OR);
	break;

    case N_OR_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_OR);
	store();
	break;

    case N_PAIR:
	cg_expr(n->l.left, PUSH);
	cg_locals(n->r.right);
	break;

    case N_QUEST:
	output("(");
	cg_expr(n->l.left, TRUTHVAL);
	output(") ? (");
	cg_expr(n->r.right->l.left, state);
	output(", 0) : (");
	cg_expr(n->r.right->r.right, state);
	output(", 0)");
	return;

    case N_RANGE:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right->l.left, PUSH);
	comma();
	cg_expr(n->r.right->r.right, PUSH);
	comma();
	kfun(KF_RANGE);
	break;

    case N_RSHIFT:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_RSHIFT);
	break;

    case N_RSHIFT_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_RSHIFT);
	store();
	break;

    case N_STR:
	if (state == PUSH) {
	    l = ctrl_dstring(n->l.string);
	    output("i_string(%d, %u)", ((int) (l >> 16)) & 0xff,
		   (unsigned short) l);
	} else {
	    output("1");
	}
	return;

    case N_SUB:
	if (n->l.left->type == N_INT && n->l.left->l.number == 0) {
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun(KF_UMIN);
	} else {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun(KF_SUB);
	}
	break;

    case N_SUB_EQ:
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(KF_SUB);
	store();
	break;

    case N_TST:
	if (state == PUSH) {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    kfun(KF_TST);
	} else {
	    cg_expr(n->l.left, TRUTHVAL);
	}
	return;

    case N_XOR:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    kfun(KF_NEG);
	} else {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun(KF_XOR);
	}
	break;

    case N_XOR_EQ:
	cg_fetch(n->l.left);
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    kfun(KF_NEG, 0);
	} else {
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun(KF_XOR);
	}
	store();
	break;

    case N_MIN_MIN:
	cg_fetch(n->l.left);
	output("PUSH_NUMBER 1, ");
	kfun(KF_SUB, 0);
	store();
	if (state != POP) {
	    output(", sp->u.number++");
	}
	break;

    case N_PLUS_PLUS:
	cg_fetch(n->l.left);
	output("PUSH_NUMBER 1, ");
	kfun(KF_ADD, 0);
	store();
	if (state != POP) {
	    output(", sp->u.number--");
	}
	break;
    }

    switch (state) {
    case POP:
	if (n->mod == T_NUMBER || n->mod == T_VOID) {
	    output(", sp++");
	} else {
	    output(", i_del_value(sp++)");
	}
	break;

    case INTVAL:
	output(", (sp++)->u.number");
	break;

    case TRUTHVAL:
	i = tmpval();
	if (n->mod == T_NUMBER) {
	    output(", tv[%d] = (sp++)->u.number, tv[%d]", i, i);
	} else {
	    output(", tv[%d] = poptruthval(), tv[%d]", i, i);
	}
	break;
    }
}

static Int *switch_table;	/* label table for current switch */
static int swcount;		/* current switch number */
static int outcount;		/* switch table element count */

/*
 * NAME:	outint()
 * DESCRIPTION:	output an integer
 */
static void outint(i)
Int i;
{
    if (outcount == 8) {
	output("\n");
	outcount = 0;
    }
    output("%ld, ", (long) i);
    outcount++;
}

/*
 * NAME:	outchar()
 * DESCRIPTION:	output a character
 */
static void outchar(c)
char c;
{
    if (outcount == 16) {
	output("\n");
	outcount = 0;
    }
    output("%d, ", UCHAR(c));
    outcount++;
}

/*
 * NAME:	codegen->switch_int()
 * DESCRIPTION:	generate single label code for a switch statement
 */
static void cg_switch_int(n)
register node *n;
{
    register node *m;
    register int i, size;
    Int *table;

    /*
     * switch table
     */
    m = n->l.left;
    size = n->mod;
    if (m->l.left == (node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    table = switch_table;
    switch_table = ALLOCA(Int, size);
    i = 1;
    do {
	switch_table[i++] = m->l.left->l.number;
	m = m->r.right;
    } while (i < size);

    /*
     * switch expression
     */
    if (n->r.right->l.left->mod == T_NUMBER) {
	output("switch (");
	cg_expr(n->r.right->l.left, INTVAL);
	output(") {\n");
	switch_table[0] = 0;
    } else {
	cg_expr(n->r.right->l.left, PUSH);
	output(";\nif (sp->type != T_NUMBER) { i_del_value(sp++); goto sw%d; }",
	       ++swcount);
	output("\nswitch ((sp++)->u.number) {\n");
	switch_table[0] = swcount;
    }

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);

    output("}\n");
    if (switch_table[0] > 0) {
	output("sw%d:\n", (int) switch_table[0]);
    }
    AFREE(switch_table);
    switch_table = table;
}

/*
 * NAME:	codegen->switch_range()
 * DESCRIPTION:	generate range label code for a switch statement
 */
static void cg_switch_range(n)
register node *n;
{
    register node *m;
    register int i, size;
    Int *table;

    /*
     * switch table
     */
    m = n->l.left;
    size = n->mod;
    if (m->l.left == (node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    table = switch_table;
    switch_table = ALLOCA(Int, size);
    output("{\nstatic Int swtab[] = {\n");
    outcount = 0;
    i = 1;
    do {
	switch_table[i] = i;
	outint(m->l.left->l.number);
	outint(m->l.left->r.number);
	m = m->r.right;
    } while (++i < size);
    output("\n};\n");

    /*
     * switch expression
     */
    if (n->r.right->l.left->mod == T_NUMBER) {
	output("switch (switch_range((");
	cg_expr(n->r.right->l.left, INTVAL);
	output("), swtab, %d)) {\n", size - 1);
	switch_table[0] = 0;
    } else {
	cg_expr(n->r.right->l.left, PUSH);
	output(";\nif (sp->type != T_NUMBER) { i_del_value(sp++); goto sw%d; }",
	       ++swcount);
	output("\nswitch (switch_range((sp++)->u.number, swtab, %d)) {\n",
	       size - 1);
	switch_table[0] = swcount;
    }

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);

    output("}\n}\n");
    if (switch_table[0] > 0) {
	output("sw%d:\n", (int) switch_table[0]);
    }
    AFREE(switch_table);
    switch_table = table;
}

/*
 * NAME:	codegen->switch_str()
 * DESCRIPTION:	generate code for a string switch statement
 */
static void cg_switch_str(n)
register node *n;
{
    register node *m;
    register int i, size;
    Int *table;

    /*
     * switch table
     */
    m = n->l.left;
    size = n->mod;
    if (m->l.left == (node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    table = switch_table;
    switch_table = ALLOCA(Int, size);
    output("{\nstatic char swtab[] = {\n");
    outcount = 0;
    i = 1;
    if (m->l.left->type == N_INT) {
	/*
	 * 0
	 */
	outchar(0);
	switch_table[i++] = 1;
	m = m->r.right;
    } else {
	/* no 0 case */
	outchar(1);
    }
    do {
	register long l;

	switch_table[i] = i;
	l = ctrl_dstring(m->l.left->l.string);
	outchar((int) (l >> 16));
	outchar((int) (l >> 8));
	outchar((int) l);
	m = m->r.right;
    } while (++i < size);
    output("\n};\n");

    /*
     * switch expression
     */
    cg_expr(n->r.right->l.left, PUSH);
    output(";\nswitch (switch_str(sp++, swtab, %d)) {\n", size - 1);
    switch_table[0] = 0;

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);

    output("}\n}\n");
    AFREE(switch_table);
    switch_table = table;
}

/*
 * NAME:	codegen->stmt()
 * DESCRIPTION:	generate code for a statement
 */
static void cg_stmt(n)
register node *n;
{
    register node *m;

    while (n != (node *) NULL) {
	if (n->type == N_PAIR) {
	    m = n->l.left;
	    n = n->r.right;
	} else {
	    m = n;
	    n = (node *) NULL;
	}
	tvc = 0;
	if (m != (node *) NULL) {
	    switch (m->type) {
	    case N_BLOCK:
		cg_stmt(m->l.left);
		break;

	    case N_BREAK:
		output("break;\n");
		break;

	    case N_CASE:
		if (m->mod == 0) {
		    if (switch_table[0] > 0) {
			output("sw%d:\n", (int) switch_table[0]);
			switch_table[0] = 0;
		    }
		    output("default:\n");
		} else {
		    output("case %ld:\n", (long) switch_table[m->mod]);
		}
		cg_stmt(m->l.left);
		break;

	    case N_CONTINUE:
		output("continue;\n");
		break;

	    case N_DO:
		output("do {\ni_add_cost(1);\n");
		cg_stmt(m->r.right);
		output("} while (");
		tvc = 0;
		cg_expr(m->l.left, TRUTHVAL);
		output(");\n");
		break;

	    case N_FOR:
		output("for (;");
		cg_expr(m->l.left, TRUTHVAL);
		output(";");
		m = m->r.right;
		if (m != (node *) NULL) {
		    if (m->type == N_PAIR && m->l.left->type == N_BLOCK &&
			m->l.left->mod == N_CONTINUE) {
			cg_expr(m->r.right->l.left, POP);
			output(") {\ni_add_cost(1);\n");
			cg_stmt(m->l.left->l.left);
		    } else {
			output(") {\ni_add_cost(1);\n");
			cg_stmt(m);
		    }
		} else {
		    output(") {\ni_add_cost(1);\n");
		}
		output("}\n");
		break;

	    case N_WHILE:
		output("while (");
		cg_expr(m->l.left, TRUTHVAL);
		output(") {\ni_add_cost(1);\n");
		cg_stmt(m->r.right);
		output("}\n");
		break;

	    case N_FOREVER:
		output("for (;;) {\ni_add_cost(1);\n");
		cg_stmt(m->l.left);
		output("}\n");
		break;

	    case N_IF:
		output("if (");
		cg_expr(m->l.left, TRUTHVAL);
		output(") {\n");
		cg_stmt(m->r.right->l.left);
		if (m->r.right->r.right != (node *) NULL) {
		    output("} else {\n");
		    cg_stmt(m->r.right->r.right);
		}
		output("}\n");
		break;

	    case N_PAIR:
		cg_stmt(m);
		break;

	    case N_POP:
		cg_expr(m->l.left, POP);
		output(";\n");
		break;

	    case N_RETURN:
		cg_expr(m->l.left, PUSH);
		output(";\nreturn;\n");
		break;

	    case N_SWITCH_INT:
		cg_switch_int(m);
		break;

	    case N_SWITCH_RANGE:
		cg_switch_range(m);
		break;

	    case N_SWITCH_STR:
		cg_switch_str(m);
		break;
	    }
	}
    }
}


static bool inherited;
static unsigned short nfuncs;

/*
 * NAME:	codegen->init()
 * DESCRIPTION:	initialize the code generator
 */
void cg_init(flag)
bool flag;
{
    inherited = flag;
    nfuncs = 1;
}

/*
 * NAME:	codegen->compiled()
 * DESCRIPTION:	return TRUE to signal that the code is compiled, and not
 *		interpreted
 */
bool cg_compiled()
{
    return TRUE;
}

/*
 * NAME:	codegen->function()
 * DESCRIPTION:	generate code for a function
 */
char *cg_function(n, nvar, npar, depth, size)
node *n;
int nvar, npar;
unsigned short depth, *size;
{
    register int i, j;
    char *prog;

    depth += nvar;
    prog = ALLOC(char, *size = 5);
    prog[0] = depth >> 8;
    prog[1] = depth;
    prog[2] = nvar - npar;
    prog[3] = 0;
    prog[4] = 0;

    output("\nstatic void func%u()\n{\nvalue *fp = sp;\nchar *p;\n",
	   nfuncs, nvar);
    j = 0;
    for (i = 0; i < nvar; i++) {
	if (c_vtype(i) == T_NUMBER) {
	    vars[i] = ++j;
	    output("register Int ivar%d = ", j);
	    if (i < npar) {
		output("fp[%d].u.number;\n", nvar - i - 1);
	    } else {
		output("0;\n");
	    }
	} else {
	    vars[i] = 0;
	}
    }
    output("\n");

    nvars = nvar;
    swcount = 0;
    cg_stmt(n);

    if (!inherited) {
	output("}\n");
	prog[3] = nfuncs >> 8;
	prog[4] = nfuncs++;
    }

    return prog;
}

/*
 * NAME:	codegen->nfuncs()
 * DESCRIPTION:	return the number of functions generated
 */
int cg_nfuncs()
{
    return nfuncs - 1;
}

/*
 * NAME:	codegen->clear()
 * DESCRIPTION:	clean up code generator
 */
void cg_clear()
{
}

/*
 * NAME:	output()
 * DESCRIPTION:	output a formatted string
 */
static void output(format, arg1, arg2, arg3)
char *format, *arg1, *arg2, *arg3;
{
    if (!inherited) {
	printf(format, arg1, arg2, arg3);
    }
}
