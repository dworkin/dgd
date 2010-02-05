/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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
# include "control.h"
# include "compile.h"
# include "codegen.h"

# define PUSH		0
# define POP		1
# define INTVAL		2
# define TRUTHVAL	3
# define TOPTRUTHVAL	4

static void output();
static void cg_iexpr P((node*, int));
static void cg_expr P((node*, int));
static void cg_stmt P((node*));

static bool skip;		/* no output for current code? */
static int vars[MAX_LOCALS];	/* local variable types */
static int nvars;		/* number of local variables */
static int nparam;		/* how many of those are arguments */
static int tvc;			/* tmpval count */
static int catch_level;		/* level of nested catches */
static Int kf_call_trace, kf_call_other, kf_clone_object, kf_new_object,
	   kf_editor;

/*
 * NAME:	tmpval()
 * DESCRIPTION:	return a new temporary value index
 */
static int tmpval()
{
    if (tvc == NTMPVAL - 1) {
	c_error("out of temporary values (%d)", NTMPVAL);
    }
    return tvc++;
}

/*
 * NAME:	local()
 * DESCRIPTION:	output a local or argument
 */
static char *local(n)
register int n;
{
    static char buffer[16];

    n = nparam - n - 1;
    if (n < 0) {
	sprintf(buffer, "(f->fp - %d)", -n);
    } else {
	sprintf(buffer, "(f->argp + %d)", n);
    }
    return buffer;
}

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
static void kfun(func)
char *func;
{
    output("kf_%s(f)", func);
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
 * NAME:	codegen->cast()
 * DESCRIPTION:	generate code for a cast
 */
static void cg_cast(what, type, class)
char *what;
unsigned short type;
string *class;
{
    long l;

    if ((type & T_REF) != 0) {
	type = T_ARRAY;
    }
    l = 0;
    if (type == T_CLASS) {
	l = ctrl_dstring(class) & 0xffffffL;
    }
    output("i_cast(f, %s, %u, %ld)", what, type, l);
}

/*
 * NAME:	codegen->lvalue()
 * DESCRIPTION:	generate code for an lvalue
 */
static void cg_lvalue(n, type)
register node *n, *type;
{
    register node *m;
    register unsigned short t;
    register long l;

    t = 0;
    l = 0;
    if (type != (node *) NULL && type->mod != T_MIXED) {
	t = type->mod;
	if (t & T_REF) {
	    t = T_ARRAY;
	}
	if (t == T_CLASS) {
	    l = ctrl_dstring(type->class) & 0xffffffL;
	}
    }

    if (n->type == N_CAST) {
	n = n->l.left;
    }
    switch (n->type) {
    case N_LOCAL:
	output("push_lvalue(%s, %d)", local((int) n->r.number), t);
	if (t == T_CLASS) {
	    output(", push_lvclass(%ld)", l);
	}
	break;

    case N_GLOBAL:
	output("i_global_lvalue(f, %d, %d/*%s*/, %d, %ld)",
	       ((int) n->r.number >> 8) & 0xff, 
	       ((int) n->r.number) & 0xff, n->l.left->l.string->text, t, l);
	break;

    case N_INDEX:
	m = n->l.left;
	if (m->type == N_CAST) {
	    m = m->l.left;
	}
	switch (m->type) {
	case N_LOCAL:
	    output("push_lvalue(%s, 0)", local((int) m->r.number));
	    break;

	case N_GLOBAL:
	    output("i_global_lvalue(f, %d, %d/*%s*/, 0, 0L)",
		   ((int) m->r.number >> 8) & 0xff, ((int) m->r.number) & 0xff,
		   m->l.left->l.string->text);
	    break;

	case N_INDEX:
	    cg_expr(m->l.left, PUSH);
	    comma();
	    cg_expr(m->r.right, PUSH);
	    output(", i_index_lvalue(f, 0, 0L)");
	    break;

	default:
	    cg_expr(m, PUSH);
	    break;
	}
	comma();
	cg_expr(n->r.right, PUSH);
	output(", i_index_lvalue(f, %d, %ld)", t, l);
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
    cg_lvalue(n, (node *) NULL);
    output(", i_dup(f), ");
    if (n->type == N_CAST) {
	cg_cast("f->sp", n->mod, n->class);
	comma();
    }
}

/*
 * NAME:	codegen->iasgn()
 * DESCRIPTION:	handle general integer assignment (operator) case
 */
static void cg_iasgn(n, op, i, direct)
register node *n;
char *op;
register int i;
bool direct;
{
    if (i < 0) {
	/* assignment on stack */
	if (n->type == N_INT) {
	    output("f->sp->u.number %s ", op);
	    cg_iexpr(n, TRUE);
	} else {
	    i = tmpval();
	    output("tv[%d] = ", i);
	    cg_iexpr(n, TRUE);
	    output(", f->sp->u.number %s tv[%d]", op, i);
	}
    } else {
	/* assignment to register var */
	if (catch_level != 0) {
	    output("%s->u.number = ", local(i));
	}
	output("ivar%d %s ", vars[i], op);
	cg_iexpr(n, direct);
    }
}

/*
 * NAME:	codegen->iasgnop()
 * DESCRIPTION:	handle an integer assignment operator
 */
static void cg_iasgnop(n, op, direct)
register node *n;
char *op;
bool direct;
{
    if (n->l.left->type == N_LOCAL) {
	cg_iasgn(n->r.right, op, (int) n->l.left->r.number, direct);
    } else {
	cg_fetch(n->l.left);
	cg_iasgn(n->r.right, op, -1, TRUE);
	output(", store_int()");
    }
}

/*
 * NAME:	codegen->ifasgnop()
 * DESCRIPTION:	handle a function integer assignment operator
 */
static void cg_ifasgnop(n, op, direct)
register node *n;
char *op;
bool direct;
{
    register int i;

    if (n->l.left->type == N_LOCAL) {
	i = n->l.left->r.number;
	if(catch_level != 0) {
	    output("%s->u.number = ", local(i));
	}
	output("ivar%d = %s(ivar%d, ", vars[i], op, vars[i]);
	cg_iexpr(n->r.right, direct);
	output(")");
    } else {
	cg_fetch(n->l.left);
	n = n->r.right;
	if (n->type == N_INT) {
	    output("f->sp->u.number = %s(f->sp->u.number, ", op);
	    cg_iexpr(n, TRUE);
	    output("), store_int()");
	} else {
	    i = tmpval();
	    output("tv[%d] = %s(f->sp->u.number, ", i, op);
	    cg_iexpr(n, TRUE);
	    output("), f->sp->u.number = tv[%d], store_int()", op, i);
	}
    }
}

/*
 * NAME:	codegen->ibinop()
 * DESCRIPTION:	generate code for an integer binary operator
 */
static void cg_ibinop(n, op, direct)
register node *n;
char *op;
bool direct;
{
    if (!((n->l.left->flags | n->r.right->flags) & F_CONST) &&
	n->l.left->type != N_LOCAL && n->r.right->type != N_LOCAL) {
	/* neither is a constant or a local intvar */
	direct = FALSE;
    }
    cg_iexpr(n->l.left, direct);
    output(" %s ", op);
    cg_iexpr(n->r.right, direct);
}

/*
 * NAME:	codegen->iexpr()
 * DESCRIPTION:	generate code for an integer expression
 */
static void cg_iexpr(n, direct)
register node *n;
int direct;
{
    register int i;

    output("(");
    switch (n->type) {
    case N_ADD:
    case N_ADD_EQ:
    case N_ADD_EQ_1:
    case N_AGGR:
    case N_AND:
    case N_AND_EQ:
    case N_ASSIGN:
    case N_CATCH:
    case N_DIV:
    case N_DIV_EQ:
    case N_EQ:
    case N_FLOAT:
    case N_FUNC:
    case N_GE:
    case N_GLOBAL:
    case N_GT:
    case N_INDEX:
    case N_INSTANCEOF:
    case N_LE:
    case N_LSHIFT:
    case N_LSHIFT_EQ:
    case N_LT:
    case N_LVALUE:
    case N_MOD:
    case N_MOD_EQ:
    case N_MULT:
    case N_MULT_EQ:
    case N_NE:
    case N_NIL:
    case N_OR:
    case N_OR_EQ:
    case N_RANGE:
    case N_RSHIFT:
    case N_RSHIFT_EQ:
    case N_STR:
    case N_SUB:
    case N_SUB_EQ:
    case N_SUB_EQ_1:
    case N_SUM:
    case N_SUM_EQ:
    case N_TOFLOAT:
    case N_TOINT:
    case N_TOSTRING:
    case N_XOR:
    case N_XOR_EQ:
    case N_MIN_MIN:
    case N_PLUS_PLUS:
	if (direct) {
	    cg_expr(n, INTVAL);
	} else {
	    i = tmpval();
	    output("tv[%d] = (", i);
	    cg_expr(n, INTVAL);
	    output("), tv[%d]", i);
	}
	break;

    case N_ADD_INT:
	cg_ibinop(n, "+", direct);
	break;

    case N_ADD_EQ_INT:
	cg_iasgnop(n, "+=", direct);
	break;

    case N_ADD_EQ_1_INT:
	if (n->l.left->type == N_LOCAL) {
	    if (catch_level != 0) {
		output("%s->u.number = ", local((int) n->l.left->r.number));
	    }
	    output("++ivar%d", vars[n->l.left->r.number]);
	} else {
	    cg_fetch(n->l.left);
	    output("++f->sp->u.number, store_int()");
	}
	break;

    case N_AND_INT:
	cg_ibinop(n, "&", direct);
	break;

    case N_AND_EQ_INT:
	cg_iasgnop(n, "&=", direct);
	break;

    case N_CAST:
	if (n->l.left->type == N_LOCAL) {
	    cg_cast(local((int) n->l.left->r.number), T_INT, (string *) NULL);
	    output(", %s->u.number", local((int) n->l.left->r.number));
	} else {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    cg_cast("f->sp", T_INT, (string *) NULL);
	    if (direct) {
		output(", (f->sp++)->u.number");
	    } else {
		i = tmpval();
		output(", tv[%d] = (f->sp++)->u.number, tv[%d]", i, i);
	    }
	}
	break;

    case N_COMMA:
	cg_expr(n->l.left, POP);
	comma();
	cg_iexpr(n->r.right, direct);
	break;

    case N_DIV_INT:
	output("xdiv(");
	cg_ibinop(n, ",", direct);
	output(")");
	break;

    case N_DIV_EQ_INT:
	cg_ifasgnop(n, "xdiv", direct);
	break;

    case N_EQ_INT:
	cg_ibinop(n, "==", direct);
	break;

    case N_GE_INT:
	cg_ibinop(n, ">=", direct);
	break;

    case N_GT_INT:
	cg_ibinop(n, ">", direct);
	break;

    case N_INT:
	output("(Int) 0x%lxL", (long) n->l.number);
	break;

    case N_LAND:
	output("(");
	cg_expr(n->l.left, TOPTRUTHVAL);
	output(") && (");
	cg_expr(n->r.right, (direct) ? TOPTRUTHVAL : TRUTHVAL);
	output(")");
	break;

    case N_LE_INT:
	cg_ibinop(n, "<=", direct);
	break;

    case N_LOCAL:
	output("ivar%d", vars[n->r.number]);
	break;

    case N_LOR:
	output("(");
	cg_expr(n->l.left, TOPTRUTHVAL);
	output(") || (");
	cg_expr(n->r.right, (direct) ? TOPTRUTHVAL : TRUTHVAL);
	output(")");
	break;

    case N_LSHIFT_INT:
	output("xlshift(");
	cg_ibinop(n, ",", direct);
	output(")");
	break;

    case N_LSHIFT_EQ_INT:
	cg_ifasgnop(n, "xlshift", direct);
	break;

    case N_LT_INT:
	cg_ibinop(n, "<", direct);
	break;

    case N_MOD_INT:
	output("xmod(");
	cg_ibinop(n, ",", direct);
	output(")");
	break;

    case N_MOD_EQ_INT:
	cg_ifasgnop(n, "xmod", direct);
	break;

    case N_MULT_INT:
	cg_ibinop(n, "*", direct);
	break;

    case N_MULT_EQ_INT:
	cg_iasgnop(n, "*=", direct);
	break;

    case N_NE_INT:
	cg_ibinop(n, "!=", direct);
	break;

    case N_NOT:
	output("!");
	if (n->l.left->mod == T_INT) {
	    cg_iexpr(n->l.left, direct);
	} else {
	    output("(");
	    cg_expr(n->l.left, (direct) ? TOPTRUTHVAL : TRUTHVAL);
	    output(")");
	}
	break;

    case N_OR_INT:
	cg_ibinop(n, "|", direct);
	break;

    case N_OR_EQ_INT:
	cg_iasgnop(n, "|=", direct);
	break;

    case N_QUEST:
	output("(");
	cg_expr(n->l.left, TOPTRUTHVAL);
	output(") ? ");
	if (n->r.right->l.left != (node *) NULL) {
	    cg_iexpr(n->r.right->l.left, direct);
	} else {
	    output("0");
	}
	output(" : ");
	if (n->r.right->r.right != (node *) NULL) {
	    cg_iexpr(n->r.right->r.right, direct);
	} else {
	    output("0");
	}
	break;

    case N_RSHIFT_INT:
	output("xrshift(");
	cg_ibinop(n, ",", direct);
	output(")");
	break;

    case N_RSHIFT_EQ_INT:
	cg_ifasgnop(n, "xrshift", direct);
	break;

    case N_SUB_INT:
	if (n->l.left->type == N_INT && n->l.left->l.number == 0) {
	    output("-");
	    cg_iexpr(n->r.right, direct);
	} else {
	    cg_ibinop(n, "-", direct);
	}
	break;

    case N_SUB_EQ_INT:
	cg_iasgnop(n, "-=", direct);
	break;

    case N_SUB_EQ_1_INT:
	if (n->l.left->type == N_LOCAL) {
	    if (catch_level != 0) {
		output("%s->u.number = ", local((int) n->l.left->r.number));
	    }
	    output("--ivar%d", vars[n->l.left->r.number]);
	} else {
	    cg_fetch(n->l.left);
	    output("--f->sp->u.number, store_int()");
	}
	break;

    case N_TST:
	if (n->l.left->mod == T_INT) {
	    output("!!");
	    cg_iexpr(n->l.left, direct);
	} else {
	    cg_expr(n->l.left, (direct) ? TOPTRUTHVAL : TRUTHVAL);
	}
	break;

    case N_XOR_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    output("~");
	    cg_iexpr(n->l.left, direct);
	} else {
	    cg_ibinop(n, "^", direct);
	}
	break;

    case N_XOR_EQ_INT:
	cg_iasgnop(n, "^=", direct);
	break;

    case N_MIN_MIN_INT:
	if (n->l.left->type == N_LOCAL) {
	    if (catch_level != 0) {
		output("%s->u.number--, ", local((int) n->l.left->r.number));
	    }
	    output("ivar%d--", vars[n->l.left->r.number]);
	} else {
	    cg_fetch(n->l.left);
	    output("f->sp->u.number--, store_int() + 1");
	}
	break;

    case N_PLUS_PLUS_INT:
	if (n->l.left->type == N_LOCAL) {
	    if (catch_level != 0) {
		output("%s->u.number++, ", local((int) n->l.left->r.number));
	    }
	    output("ivar%d++", vars[n->l.left->r.number]);
	} else {
	    cg_fetch(n->l.left);
	    output("f->sp->u.number++, store_int() - 1");
	}
	break;
    }
    output(")");
}

/*
 * NAME:	codegen->asgnop()
 * DESCRIPTION:	generate code for an assignment operator
 */
static void cg_asgnop(n, op)
register node *n;
char *op;
{
    if (n->l.left->type == N_LOCAL && vars[n->l.left->r.number] != 0) {
	output("PUSH_NUMBER ivar%d, ", vars[n->l.left->r.number]);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(op);
	comma();
	cg_cast("f->sp", T_INT, (string *) NULL);
	comma();
	if (catch_level != 0) {
	    output("%s->u.number = ", local((int) n->l.left->r.number));
	}
	output("ivar%d = f->sp->u.number", vars[n->l.left->r.number]);
    } else {
	cg_fetch(n->l.left);
	cg_expr(n->r.right, PUSH);
	comma();
	kfun(op);
	store();
    }
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
    for (i = 1; n->type == N_PAIR; i++) {
	cg_expr(n->l.left, PUSH);
	comma();
	n = n->r.right;
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
    for (i = 2; n->type == N_PAIR; i += 2) {
	cg_expr(n->l.left->l.left, PUSH);
	comma();
	cg_expr(n->l.left->r.right, PUSH);
	comma();
	n = n->r.right;
    }
    cg_expr(n->l.left, PUSH);
    comma();
    cg_expr(n->r.right, PUSH);
    comma();
    return i;
}

/*
 * NAME:	codegen->lval_aggr()
 * DESCRIPTION:	generate code for an lvalue aggregate
 */
static int cg_lval_aggr(n, type)
register node *n, *type;
{
    register int i;

    for (i = 1; n->type == N_PAIR; i++) {
	cg_lvalue(n->l.left, (type != (node *) NULL) ? type : n->l.left);
	comma();
	n = n->r.right;
    }
    cg_lvalue(n, (type != (node *) NULL) ? type : n);
    comma();
    return i;
}

/*
 * NAME:	codegen->lval_locals()
 * DESCRIPTION:	copy changed values to local integer vars
 */
static void cg_lval_locals(n)
register node *n;
{
    register int i;

    while (n->type == N_PAIR) {
	if (n->l.left->type == N_LOCAL && vars[i = n->l.left->r.number] != 0) {
	    output(", ivar%d = %s->u.number", vars[i], local(i));
	}
	n = n->r.right;
    }
    if (n->type == N_LOCAL && vars[i = n->r.number] != 0) {
	output(", ivar%d = %s->u.number", vars[i], local(i));
    }
}

/*
 * NAME:	codegen->sumargs()
 * DESCRIPTION:	generate code for summand arguments
 */
static int cg_sumargs(n)
register node *n;
{
    int i;

    if (n->type == N_SUM) {
	i = cg_sumargs(n->l.left);
	n = n->r.right;
    } else {
	i = 0;
    }

    if (n->type == N_AGGR) {
	output("PUSH_NUMBER %d", -3 - cg_aggr(n->l.left));
    } else if (n->type == N_RANGE) {
	cg_expr(n->l.left, PUSH);
	comma();
	n = n->r.right;
	if (n->l.left != (node *) NULL) {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    if (n->r.right != (node *) NULL) {
		cg_expr(n->r.right, PUSH);
		comma();
		kfun("ckrangeft");
	    } else {
		kfun("ckrangef");
	    }
	} else if (n->r.right != (node *) NULL) {
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun("ckranget");
	} else {
	    kfun("range");
	    output(", PUSH_NUMBER -2");
	}
    } else {
	cg_expr(n, PUSH);
	output(", PUSH_NUMBER -2");
    }
    comma();

    return i + 1;
}

/*
 * NAME:	codegen->funargs()
 * DESCRIPTION:	generate code for function arguments
 */
static char *cg_funargs(n, lv)
register node *n;
bool lv;
{
    static char buffer[32];
    register int i;

    if (n == (node *) NULL) {
	return "0";
    }
    for (i = 1; n->type == N_PAIR; i++) {
	cg_expr(n->l.left, PUSH);
	comma();
	n = n->r.right;
    }
    if (n->type == N_SPREAD) {
	int type;
	long l;

	if (!lv || (type=n->l.left->mod & ~(1 << REFSHIFT)) == T_MIXED) {
	    type = 0;
	}
	if (type == T_CLASS) {
	    l = ctrl_dstring(n->l.left->class) & 0xffffffL;
	} else {
	    l = 0;
	}
	cg_expr(n->l.left, PUSH);
	comma();
	sprintf(buffer, "%d + i_spread(f, %d, %d, %ld)", i, (short) n->mod,
		type, l);
    } else {
	cg_expr(n, PUSH);
	comma();
	sprintf(buffer, "%d", i);
    }
    return buffer;
}

/*
 * NAME:        codegen->locals()
 * DESCRIPTION: propagate values between local variables and ivars
 */
static void cg_locals(n, vtoi)
register node *n;
bool vtoi;
{
    if (n != (node *) NULL) {
	register node *m;
	register int i;

	/* skip non-lvalue arguments */
	while (n->type == N_PAIR && n->l.left->type != N_LVALUE) {
	    n = n->r.right;
	}

	while (n->type == N_PAIR) {
	    m = n->l.left;
	    if (m->l.left->type == N_LOCAL &&
		vars[i = m->l.left->r.number] != 0) {
		if (vtoi) {
		    output("%s->u.number = ivar%d, ", local(i), vars[i]);
		} else {
		    output(", ivar%d = %s->u.number", vars[i], local(i));
		}
	    }
	    n = n->r.right;
	}

	/* last one can be lvalue or spread array */
	m = n->l.left;
	if (n->type == N_LVALUE && m->type == N_LOCAL &&
	    vars[i = m->r.number] != 0) {
	    if (vtoi) {
		output("%s->u.number = ivar%d, ", local(i), vars[i]);
	    } else {
		output(", ivar%d = %s->u.number", vars[i], local(i));
	    }
	}
    }
}

/*
 * NAME:	codegen->binop()
 * DESCRIPTION:	generate code for a binary operator
 */
static void cg_binop(n)
node *n;
{
    cg_expr(n->l.left, PUSH);
    comma();
    cg_expr(n->r.right, PUSH);
    comma();
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
    char *p;

    switch (n->type) {
    case N_ADD_INT:
    case N_ADD_EQ_INT:
    case N_ADD_EQ_1_INT:
    case N_AND_INT:
    case N_AND_EQ_INT:
    case N_DIV_INT:
    case N_DIV_EQ_INT:
    case N_EQ_INT:
    case N_GE_INT:
    case N_GT_INT:
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
    case N_SUB_EQ_1_INT:
    case N_XOR_INT:
    case N_XOR_EQ_INT:
    case N_MIN_MIN_INT:
    case N_PLUS_PLUS_INT:
	if (state == PUSH) {
	    i = tmpval();
	    output("tv[%d] = ", i);
	    cg_iexpr(n, TRUE);
	    output(", PUSH_NUMBER tv[%d]", i);
	} else {
	    cg_iexpr(n, (state != TRUTHVAL));
	}
	return;

    case N_ADD:
	cg_expr(n->l.left, PUSH);
	comma();
	if (n->r.right->type == N_FLOAT) {
	    if (NFLT_ISONE(n->r.right)) {
		kfun("add1");
		break;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		kfun("sub1");
		break;
	    }
	}
	cg_expr(n->r.right, PUSH);
	comma();
	kfun("add");
	break;

    case N_ADD_EQ:
	cg_asgnop(n, "add");
	break;

    case N_ADD_EQ_1:
	cg_fetch(n->l.left);
	kfun("add1");
	store();
	break;
	
    case N_AGGR:
	if (n->mod == T_MAPPING) {
	    output("i_map_aggregate(f, %u)", cg_map_aggr(n->l.left));
	} else {
	    output("i_aggregate(f, %u)", cg_aggr(n->l.left));
	}
	break;

    case N_AND:
	cg_binop(n);
	kfun("and");
	break;

    case N_AND_EQ:
	cg_asgnop(n, "and");
	break;

    case N_ASSIGN:
	if (n->l.left->type == N_AGGR) {
	    i = cg_lval_aggr(n->l.left->l.left, n->l.left->r.right);
	    cg_expr(n->r.right, PUSH);
	    output(", PUSH_NUMBER %d, ", i);
	    kfun("store_aggr");
	    if (catch_level == 0) {
		cg_lval_locals(n->l.left->l.left);
	    }
	} else {
	    if (n->l.left->type == N_LOCAL && vars[n->l.left->r.number] != 0) {
		cg_iasgn(n->r.right, "=", (int) n->l.left->r.number,
			 (state != PUSH && state != TRUTHVAL));
		if (state == PUSH) {
		    output(", PUSH_NUMBER ivar%d", vars[n->l.left->r.number]);
		}
		return;
	    }
	    if (n->r.right->type == N_CAST) {
		cg_lvalue(n->l.left, n->r.right);
		n->r.right = n->r.right->l.left;
	    } else {
		cg_lvalue(n->l.left, (node *) NULL);
	    }
	    comma();
	    cg_expr(n->r.right, PUSH);
	    store();
	}
	break;

    case N_CAST:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_cast("f->sp", n->mod, n->class);
	break;

    case N_CATCH:
	output("(");
	if (catch_level == 0) {
	    for (i = nvars; i > 0; ) {
		if (vars[--i] != 0) {
		    output("%s->u.number = ivar%d, ", local(i), vars[i]);
		}
	    }
	}
	output("!ec_push((ec_ftn) i_catcherr) ? (f->atomic = FALSE, ");
	catch_level++;
	cg_expr(n->l.left, POP);
	--catch_level;
	output(", ec_pop(), ");
	if (state == PUSH) {
	    output("*--f->sp = nil_value, 0) : (");
	    if (catch_level == 0) {
		for (i = nvars; i > 0; ) {
		    if (vars[--i] != 0) {
			output("ivar%d = %s->u.number, ", vars[i], local(i));
		    }
		}
	    }
	    output("PUSH_STRVAL(f, errorstr()), 0))");
	} else {
	    output("FALSE) : (");
	    if (catch_level == 0) {
		for (i = nvars; i > 0; ) {
		    if (vars[--i] != 0) {
			output("ivar%d = %s->u.number, ", vars[i], local(i));
		    }
		}
	    }
	    output("TRUE))");
	}
	return;

    case N_COMMA:
	cg_expr(n->l.left, POP);
	comma();
	cg_expr(n->r.right, state);
	return;

    case N_DIV:
	cg_binop(n);
	kfun("div");
	break;

    case N_DIV_EQ:
	cg_asgnop(n, "div");
	break;

    case N_EQ:
	cg_binop(n);
	kfun("eq");
	break;

    case N_FLOAT:
	output("PUSH_FLTCONST(f, 0x%04x, 0x%08xL)", n->l.fhigh,
	       (long) n->r.flow);
	break;

    case N_FUNC:
	p = cg_funargs(n->l.left->r.right, (n->r.number >> 24) & KFCALL_LVAL);
	switch (n->r.number >> 24) {
	case KFCALL:
	    if (catch_level == 0 &&
		(n->r.number == kf_call_trace ||
		 n->r.number == kf_call_other ||
		 n->r.number == kf_clone_object ||
		 n->r.number == kf_new_object || n->r.number == kf_editor)) {
		for (i = nparam; i > 0; ) {
		    if (vars[--i] != 0) {
			output("%s->u.number = ivar%d, ", local(i), vars[i]);
		    }
		}
	    }
	    /* fall through */
	case KFCALL_LVAL:
	    if (PROTO_NARGS(KFUN((short) n->r.number).proto) +
		PROTO_VARGS(KFUN((short) n->r.number).proto) == 0) {
		/* kfun without arguments won't do argument checking */
		kfun(KFUN((short) n->r.number).name);
	    } else {
		if (catch_level == 0 && ((n->r.number >> 24) & KFCALL_LVAL)) {
		    cg_locals(n->l.left->r.right, TRUE);
		}
		if (PROTO_VARGS(KFUN((short) n->r.number).proto) != 0) {
		    output("call_kfun_arg(f, %d/*%s*/, %s)",
			   &KFUN((short) n->r.number) - kftab,
			   n->l.left->l.string->text, p);
		} else {
		    output("call_kfun(f, %d/*%s*/)",
			   &KFUN((short) n->r.number) - kftab,
			   n->l.left->l.string->text);
		}
		if ((n->r.number >> 24) & KFCALL_LVAL) {
		    cg_locals(n->l.left->r.right, FALSE);
		}
	    }
	    break;

	case DFCALL:
	    if (catch_level == 0) {
		for (i = nparam; i > 0; ) {
		    if (vars[--i] != 0) {
			output("%s->u.number = ivar%d, ", local(i), vars[i]);
		    }
		}
	    }
	    if (((n->r.number >> 8) & 0xff) == 0) {
		output("i_funcall(f, (object *) NULL, (array *) NULL, 0, ");
		output("%d/*%s*/, %s)", ((int) n->r.number) & 0xff,
		       n->l.left->l.string->text, p);
	    } else {
		output("i_funcall(f, (object *) NULL, (array *) NULL, ");
		output("UCHAR(f->ctrl->imap[f->p_index + %d]), %d/*%s*/, %s)",
		       ((int) n->r.number >> 8) & 0xff,
		       ((int) n->r.number) & 0xff,
		       n->l.left->l.string->text, p);
	    }
	    break;

	case FCALL:
	    if (catch_level == 0) {
		for (i = nparam; i > 0; ) {
		    if (vars[--i] != 0) {
			output("%s->u.number = ivar%d, ", local(i), vars[i]);
		    }
		}
	    }
	    output("p = i_foffset(%u), i_funcall(f, (object *) NULL, ",
		   ctrl_gencall((long) n->r.number));
	    output("(array *) NULL, UCHAR(p[0]), UCHAR(p[1])/*%s*/, %s)",
		   n->l.left->l.string->text, p);
	    break;
	}
	break;

    case N_GE:
	cg_binop(n);
	kfun("ge");
	break;

    case N_GLOBAL:
	output("i_global(f, %d, %d/*%s*/)", ((int) n->r.number >> 8) & 0xff,
	       ((int) n->r.number) & 0xff, n->l.left->l.string->text);
	break;

    case N_GT:
	cg_binop(n);
	kfun("gt");
	break;

    case N_INDEX:
	cg_binop(n);
	output("i_index(f)");
	break;

    case N_INSTANCEOF:
	cg_expr(n->l.left, PUSH);
	output(", PUSH_NUMBER %ld, ",
	       ctrl_dstring(n->r.right->l.string) & 0xffffffL);
	kfun("instanceof");
	break;

    case N_INT:
	if (state == PUSH) {
	    output("PUSH_NUMBER ");
	}
	cg_iexpr(n, TRUE);
	return;

    case N_LE:
	cg_binop(n);
	kfun("le");
	break;

    case N_LOCAL:
	if (vars[n->r.number] != 0) {
	    if (state == PUSH) {
		output("PUSH_NUMBER ");
	    }
	    output("ivar%d", vars[n->r.number]);
	    return;
	}
	if (state == TRUTHVAL || state == TOPTRUTHVAL) {
	    p = local((int) n->r.number);
	    switch (n->mod) {
	    case T_FLOAT:
		output("!VFLT_ISZERO(%s)", p);
		break;

	    case T_STRING:
		output("%s->type == T_STRING", p);
		break;

	    case T_OBJECT:
		output("%s->type == T_OBJECT || %s->type == T_LWOBJECT", p, p);
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		output("T_INDEXED(%s->type)", p);
		break;

	    default:
		output("VAL_TRUE(%s)", p);
		break;
	    }
	    return;
	}
	output((n->mod == T_FLOAT) ?  "*--f->sp = *%s" : "i_push_value(f, %s)",
	       local((int) n->r.number));
	break;

    case N_LSHIFT:
	cg_binop(n);
	kfun("lshift");
	break;

    case N_LSHIFT_EQ:
	cg_asgnop(n, "lshift");
	break;

    case N_LT:
	cg_binop(n);
	kfun("lt");
	break;

    case N_LVALUE:
	cg_lvalue(n->l.left, n->l.left);
	break;

    case N_MOD:
	cg_binop(n);
	kfun("mod");
	break;

    case N_MOD_EQ:
	cg_asgnop(n, "mod");
	break;

    case N_MULT:
	cg_binop(n);
	kfun("mult");
	break;

    case N_MULT_EQ:
	cg_asgnop(n, "mult");
	break;

    case N_NE:
	cg_binop(n);
	kfun("ne");
	break;

    case N_NIL:
	output("(--f->sp)->type = nil_type, f->sp->u.number = 0");
	break;

    case N_NOT:
	if (state == PUSH) {
	    i = tmpval();
	    output("tv[%d] = ", i);
	    cg_iexpr(n, TRUE);
	    output(", PUSH_NUMBER tv[%d]", i);
	} else {
	    output("!");
	    n = n->l.left;
	    if (n->mod == T_INT) {
		cg_iexpr(n, (state != TRUTHVAL));
	    } else {
		output("(");
		cg_expr(n, (state != TRUTHVAL) ? TOPTRUTHVAL : TRUTHVAL);
		output(")");
	    }
	}
	return;

    case N_OR:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun("or");
	break;

    case N_OR_EQ:
	cg_asgnop(n, "or");
	break;

    case N_QUEST:
	output("(");
	cg_expr(n->l.left, TOPTRUTHVAL);
	output(") ? (");
	if (n->r.right->l.left != (node *) NULL) {
	    cg_expr(n->r.right->l.left, state);
	    if (state == PUSH || state == POP) {
		output(", 0");
	    }
	} else {
	    output("0");
	}
	output(") : (");
	if (n->r.right->r.right != (node *) NULL) {
	    cg_expr(n->r.right->r.right, state);
	    if (state == PUSH || state == POP) {
		output(", 0");
	    }
	} else {
	    output("0");
	}
	output(")");
	return;

    case N_RANGE:
	cg_expr(n->l.left, PUSH);
	comma();
	n = n->r.right;
	if (n->l.left != (node *) NULL) {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    if (n->r.right != (node *) NULL) {
		cg_expr(n->r.right, PUSH);
		comma();
		kfun("rangeft");
	    } else {
		kfun("rangef");
	    }
	} else if (n->r.right != (node *) NULL) {
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun("ranget");
	} else {
	    kfun("range");
	}
	break;

    case N_RSHIFT:
	cg_expr(n->l.left, PUSH);
	comma();
	cg_expr(n->r.right, PUSH);
	comma();
	kfun("rshift");
	break;

    case N_RSHIFT_EQ:
	cg_asgnop(n, "rshift");
	break;

    case N_STR:
	l = ctrl_dstring(n->l.string);
	output("i_string(f, %d, %u)", ((int) (l >> 16)) & 0xff,
	       (unsigned short) l);
	break;

    case N_SUB:
	if ((n->l.left->type == N_INT && n->l.left->l.number == 0) ||
	    (n->l.left->type == N_FLOAT && NFLT_ISZERO(n->l.left))) {
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun("umin");
	} else {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    if (n->r.right->type == N_FLOAT) {
		if (NFLT_ISONE(n->r.right)) {
		    kfun("sub1");
		    break;
		}
		if (NFLT_ISMONE(n->r.right)) {
		    kfun("add1");
		    break;
		}
	    }
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun("sub");
	}
	break;

    case N_SUB_EQ:
	cg_asgnop(n, "sub");
	break;

    case N_SUB_EQ_1:
	cg_fetch(n->l.left);
	kfun("sub1");
	store();
	break;

    case N_SUM:
	output("kf_sum(f, %d)", cg_sumargs(n));
	break;

    case N_SUM_EQ:
	cg_fetch(n->l.left);
	output("PUSH_NUMBER -2,\n");
	output("kf_sum(f, %d), store()", cg_sumargs(n->r.right) + 1);
	break;

    case N_TOFLOAT:
	cg_expr(n->l.left, PUSH);
	comma();
	kfun("tofloat");
	break;

    case N_TST:
	if (state == PUSH) {
	    i = tmpval();
	    output("tv[%d] = ", i);
	    cg_iexpr(n, TRUE);
	    output(", PUSH_NUMBER tv[%d]", i);
	} else {
	    output("!!");
	    n = n->l.left;
	    if (n->mod == T_INT) {
		cg_iexpr(n, (state != TRUTHVAL));
	    } else {
		output("(");
		cg_expr(n, (state != TRUTHVAL) ? TOPTRUTHVAL : TRUTHVAL);
		output(")");
	    }
	}
	return;

    case N_TOINT:
	cg_expr(n->l.left, PUSH);
	comma();
	kfun("toint");
	break;

    case N_TOSTRING:
	cg_expr(n->l.left, PUSH);
	comma();
	kfun("tostring");
	break;

    case N_XOR:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    kfun("neg");
	} else {
	    cg_expr(n->l.left, PUSH);
	    comma();
	    cg_expr(n->r.right, PUSH);
	    comma();
	    kfun("xor");
	}
	break;

    case N_XOR_EQ:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    cg_fetch(n->l.left);
	    kfun("neg");
	    store();
	} else {
	    cg_asgnop(n, "xor");
	}
	break;

    case N_MIN_MIN:
	cg_fetch(n->l.left);
	kfun("sub1");
	store();
	comma();
	if (n->mod == T_INT) {
	    output("f->sp->u.number++");
	} else {
	    kfun("add1");
	}
	break;

    case N_PLUS_PLUS:
	cg_fetch(n->l.left);
	kfun("add1");
	store();
	comma();
	if (n->mod == T_INT) {
	    output("f->sp->u.number--");
	} else {
	    kfun("sub1");
	}
	break;

# ifdef DEBUG
    default:
	fatal("unknown expression type %d", n->type);
# endif
    }

    switch (state) {
    case POP:
	if ((n->type != N_FUNC || n->r.number >> 24 != FCALL) &&
	    (n->mod == T_INT || n->mod == T_FLOAT || n->mod == T_VOID)) {
	    output(", f->sp++");
	} else {
	    output(", i_del_value(f->sp++)");
	}
	break;

    case INTVAL:
	output(", (f->sp++)->u.number");
	break;

    case TRUTHVAL:
	if (n->mod == T_INT) {
	    i = tmpval();
	    output(", tv[%d] = (f->sp++)->u.number, tv[%d]", i, i);
	} else {
	    output(", poptruthval(f)");
	}
	break;

    case TOPTRUTHVAL:
	switch (n->mod) {
	case T_INT:
	    output(", (f->sp++)->u.number");
	    break;

	case T_FLOAT:
	    output(", f->sp++, !VFLT_ISZERO(f->sp - 1)");
	    break;

	default:
	    output(", poptruthval(f)");
	    break;
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
    output("%d, ", c);
    outcount++;
}

/*
 * NAME:	codegen->startvars()
 * DESCRIPTION:	handle integer variable declarations
 */
static void cg_startvars(n)
register node *n;
{
    if (n != (node *) NULL) {
	while (n->type == N_PAIR) {
	    cg_startvars(n->l.left);
	    n = n->r.right;
	}
	if (n->type == N_VAR) {
	    if (n->mod == T_INT) {
		output("register Int ivar%d = (i_del_value(%s), ",
		       n->l.number + 1, local(n->l.number));
		output("%s->type = T_INT, 0);\n", local(n->l.number));
		vars[n->l.number] = n->l.number + 1;
	    } else {
		vars[n->l.number] = 0;
	    }
	}
    }
}

/*
 * NAME:	codegen->endvars()
 * DESCRIPTION:	remove integer variables
 */
static void cg_endvars(n)
register node *n;
{
    if (n != (node *) NULL) {
	while (n->type == N_PAIR) {
	    cg_endvars(n->l.left);
	    n = n->r.right;
	}
	if (n->type == N_VAR && n->mod == T_INT) {
	    vars[n->l.number] = 0;
	}
    }
}

/*
 * NAME:      codegen->switch_init()
 * DESCRIPTION:       handle initializers for a switch
 */
static node *cg_switch_init(n)
node *n;
{
    register node *m;

    /*
     * initializers
     */
    m = n->r.right->r.right;
    if (m->type == N_BLOCK) {
	m = m->l.left;
    }
# ifdef DEBUG
    if (m->type != N_COMPOUND) {
	fatal("N_COMPOUND expected");
    }
# endif
    n = m->r.right;
    cg_startvars(n);
    cg_stmt(n);
    m->r.right = NULL;

    return n;
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

    output("{");
    m = cg_switch_init(n);

    /*
     * switch expression
     */
    if (n->r.right->l.left->mod == T_INT) {
	output("switch (");
	cg_expr(n->r.right->l.left, INTVAL);
	output(") {\n");
	switch_table[0] = 0;
    } else {
	cg_expr(n->r.right->l.left, PUSH);
	output(";\nif (f->sp->type != T_INT) { i_del_value(f->sp++);");
	output(" goto sw%d; }", ++swcount);
	output("\nswitch ((f->sp++)->u.number) {\n");
	switch_table[0] = swcount;
    }

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);
    cg_endvars(m);

    output("}\n}\n");
    if (switch_table[0] > 0) {
	output("sw%d: ;\n", (int) switch_table[0]);
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

    m = cg_switch_init(n);

    /*
     * switch expression
     */
    if (n->r.right->l.left->mod == T_INT) {
	output("switch (switch_range((");
	cg_expr(n->r.right->l.left, INTVAL);
	output("), swtab, %d)) {\n", size - 1);
	switch_table[0] = 0;
    } else {
	cg_expr(n->r.right->l.left, PUSH);
	output(";\nif (f->sp->type != T_INT) { i_del_value(f->sp++);");
	output(" goto sw%d; }", ++swcount);
	output("\nswitch (switch_range((f->sp++)->u.number, swtab, %d)) {\n",
	       size - 1);
	switch_table[0] = swcount;
    }

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);
    cg_endvars(m);

    output("}\n}\n");
    if (switch_table[0] > 0) {
	output("sw%d: ;\n", (int) switch_table[0]);
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
    if (m->l.left->type == nil_node) {
	/*
	 * nil
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
	outchar((char) (l >> 16));
	outchar((char) (l >> 8));
	outchar((char) l);
	m = m->r.right;
    } while (++i < size);
    switch_table[0] = 0;
    output("\n};\n");

    m = cg_switch_init(n);
    cg_expr(n->r.right->l.left, PUSH);
    output(";\nswitch (switch_str(f->sp++, f->p_ctrl, swtab, %d)) {\n",
	   size - 1);

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);
    cg_endvars(m);

    output("}\n}\n");
    AFREE(switch_table);
    switch_table = table;
}

typedef struct _rclink_ {
    int type;			/* rlimits or catch */
    struct _rclink_ *next;	/* next in linked list */
} rclink;

static rclink *rclist;

/*
 * NAME:	breakout()
 * DESCRIPTION:	break out of a catch or rlimits
 */
static void breakout(n)
register int n;
{
    register int c, r;
    register rclink *link;

    c = r = 0;
    link = rclist;

    do {
	if (link->type == N_CATCH) {
	    c++;
	} else {
	    r++;
	}
	link = link->next;
    } while (--n != 0);

    while (c != 0) {
	output("ec_pop();\n");
	--c;
    }
    if (r != 0) {
	output("i_set_rlimits(f, f->rlim->next");
	while (--r != 0) {
	    output("->next");
	}
	output(");\n");
    }
}

/*
 * NAME:	codegen->stmt()
 * DESCRIPTION:	generate code for a statement
 */
static void cg_stmt(n)
register node *n;
{
    rclink rcstart;
    register node *m;
    register int i;

    while (n != (node *) NULL) {
	if (n->type == N_PAIR) {
	    m = n->l.left;
	    n = n->r.right;
	} else {
	    m = n;
	    n = (node *) NULL;
	}
	if (catch_level == 0) {
	    tvc = 0;
	}
	switch (m->type) {
	case N_BLOCK:
	    cg_stmt(m->l.left);
	    break;

	case N_BREAK:
	    if (m->mod != 0) {
		breakout(m->mod);
	    }
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

	case N_COMPOUND:
	    output("{ ");
	    if (m->r.right != (node *) NULL) {
		cg_startvars(m->r.right);
		cg_stmt(m->r.right);
	    }
	    cg_stmt(m->l.left);
	    output("} ");
	    if (m->r.right != (node *) NULL) {
		cg_endvars(m->r.right);
	    }
	    break;

	case N_CONTINUE:
	    if (m->mod != 0) {
		breakout(m->mod);
	    }
	    output("continue;\n");
	    break;

	case N_DO:
	    output("do {\ni_add_ticks(f, 1);\n");
	    cg_stmt(m->r.right);
	    output("} while (");
	    tvc = 0;
	    cg_expr(m->l.left, TOPTRUTHVAL);
	    output(");\n");
	    break;

	case N_FOR:
	    /* hack: process in the same order as in codegeni.c */
	    if (m->r.right != (node *) NULL) {
		i = skip;
		skip = TRUE;
		cg_stmt(m->r.right);
		skip = i;
	    }

	    output("for (;");
	    cg_expr(m->l.left, TOPTRUTHVAL);
	    output(";");
	    m = m->r.right;
	    if (m != (node *) NULL) {
		if (m->type == N_PAIR && m->l.left->type == N_BLOCK &&
		    m->l.left->mod == N_CONTINUE) {
		    cg_expr(m->r.right->l.left, POP);
		    output(") {\ni_add_ticks(f, 1);\n");
		    cg_stmt(m->l.left->l.left);
		} else {
		    output(") {\ni_add_ticks(f, 1);\n");
		    cg_stmt(m);
		}
	    } else {
		output(") {\ni_add_ticks(f, 1);\n");
	    }
	    output("}\n");
	    break;

	case N_FOREVER:
	    output("for (");
	    if (m->l.left != (node *) NULL) {
		cg_expr(m->l.left, POP);
	    }
	    output(";;) {\ni_add_ticks(f, 1);\n");
	    cg_stmt(m->r.right);
	    output("}\n");
	    break;

	case N_RLIMITS:
	    cg_expr(m->l.left->l.left, PUSH);
	    comma();
	    cg_expr(m->l.left->r.right, PUSH);
	    rcstart.type = N_RLIMITS;
	    rcstart.next = rclist;
	    rclist = &rcstart;
	    output(";\nnew_rlimits(f);\n");
	    cg_stmt(m->r.right);
	    if (!(m->r.right->flags & F_END)) {
		output("i_set_rlimits(f, f->rlim->next);\n");
	    }
	    rclist = rcstart.next;
	    break;

	case N_CATCH:
	    rcstart.type = N_CATCH;
	    rcstart.next = rclist;
	    rclist = &rcstart;
	    if (catch_level == 0) {
		for (i = nvars; i > 0; ) {
		    if (vars[--i] != 0) {
			output("%s->u.number = ivar%d;\n", local(i), vars[i]);
		    }
		}
	    }
	    output("if (!ec_push((ec_ftn) i_catcherr)) {f->atomic = FALSE;\n");
	    catch_level++;
	    cg_stmt(m->l.left);
	    --catch_level;
	    if (!(m->l.left->flags & F_END)) {
		output("ec_pop();");
	    }
	    output("} else {\n");
	    if (catch_level == 0) {
		for (i = nvars; i > 0; ) {
		    if (vars[--i] != 0) {
			output("ivar%d = %s->u.number;\n", vars[i], local(i));
		    }
		}
	    }
	    rclist = rcstart.next;
	    if (m->r.right != (node *) NULL) {
		cg_stmt(m->r.right);
	    }
	    output("}\n");
	    break;

	case N_IF:
	    output("if (");
	    cg_expr(m->l.left, TOPTRUTHVAL);
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
	    output(";\n");
	    if (m->mod != 0) {
		breakout(m->mod);
	    }
	    output("return;\n");
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


static unsigned short nfuncs;
static string *funcnames[255];

/*
 * NAME:	codegen->init()
 * DESCRIPTION:	initialize the code generator
 */
void cg_init(inherited)
int inherited;
{
    skip = inherited;
    nfuncs = 0;
    kf_call_trace = ((long) KFCALL << 24) | kf_func("call_trace");
    kf_call_other = ((long) KFCALL << 24) | kf_func("call_other");
    kf_clone_object = ((long) KFCALL << 24) | kf_func("clone_object");
    kf_new_object = ((long) KFCALL << 24) | kf_func("new_object");
    kf_editor = ((long) KFCALL << 24) | kf_func("editor");
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
char *cg_function(fname, n, nvar, npar, depth, size)
string *fname;
node *n;
int nvar, npar;
unsigned int depth;
unsigned short *size;
{
    register int i;
    char *prog;

    depth += nvar;
    prog = ALLOC(char, *size = 6);
    prog[0] = depth >> 8;
    prog[1] = depth;
    prog[2] = nvar - npar;

    nvars = nvar;
    nparam = npar;
    if (!skip) {
	str_ref(funcnames[nfuncs] = fname);
	output("\nstatic void LPC_%s(f)\nregister frame *f;\n{\n", fname->text);
	output("char *p; Int tv[%d];\n", NTMPVAL);
    }
    for (i = 0; i < nparam; i++) {
	if (c_vtype(i) == T_INT) {
	    output("register Int ivar%d = %s->u.number;\n", vars[i] = i + 1,
		   local(i));
	} else {
	    vars[i] = 0;
	}
    }
    while (i < nvars) {
	vars[i++] = 0;
    }

    output("\n");

    swcount = 0;
    cg_stmt(n);

    output("}\n");
    prog[3] = nfuncs >> 16;
    prog[4] = nfuncs >> 8;
    prog[5] = nfuncs++;

    return prog;
}

/*
 * NAME:	codegen->nfuncs()
 * DESCRIPTION:	return the number of functions generated
 */
int cg_nfuncs()
{
    return nfuncs;
}

/*
 * NAME:	codegen->clear()
 * DESCRIPTION:	clean up code generator
 */
void cg_clear()
{
    if (!skip && nfuncs != 0) {
	register int i;
	register string **f;

	output("\nstatic pcfunc functions[] = {\n");
	for (i = nfuncs, f = funcnames; i != 0; --i, f++) {
	    output("LPC_%s,\n", (*f)->text);
	    str_del(*f);
	}
	output("};\n");
    }
}

/*
 * NAME:	output()
 * DESCRIPTION:	output a formatted string
 */
static void output(format, arg1, arg2, arg3, arg4)
char *format, *arg1, *arg2, *arg3, *arg4;
{
    if (!skip) {
	printf(format, arg1, arg2, arg3, arg4);
    }
}
