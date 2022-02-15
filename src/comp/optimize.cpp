/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2022 DGD Authors (see the commit log for details)
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
# include "optimize.h"

/*
 * return the maximum of two numbers
 */
Uint Optimize::max2(Uint a, Uint b)
{
    return (a > b) ? a : b;
}

/*
 * return the maximum of three numbers
 */
Uint Optimize::max3(Uint a, Uint b, Uint c)
{
    return (a > b) ? ((a > c) ? a : c) : ((b > c) ? b : c);
}

static Node **aside;
static Uint sidedepth;

/*
 * start a side expression
 */
Node **Optimize::sideStart(Node **n, Uint *depth)
{
    Node **old;

    *n = (Node *) NULL;
    old = aside;
    aside = n;
    *depth = sidedepth;
    sidedepth = 0;
    return old;
}

/*
 * deal with a side expression
 */
void Optimize::sideAdd(Node **n, Uint depth)
{
    Node *m;

    if (depth != 0) {
	m = *aside;
	*aside = *n;
	*n = (*n)->r.right;
	(*aside)->r.right = (*aside)->l.left;
	(*aside)->l.left = m;
	aside = &(*aside)->r.right;
	sidedepth = max2(sidedepth, depth);
    } else {
	*n = (*n)->r.right;
    }
}

/*
 * end a side expression
 */
Uint Optimize::sideEnd(Node **n, Node *side, Node **oldside, Uint olddepth)
{
    Uint depth;

    if (side != (Node *) NULL) {
	if (*n == (Node *) NULL) {
	    *n = side->r.right;
	} else {
	    side->l.left = side->r.right;
	    side->r.right = *n;
	    *n = side;
	}
    }
    aside = oldside;
    depth = sidedepth;
    sidedepth = olddepth;
    return depth;
}


static LPCint kd_status, kd_call_trace, kd_allocate,	/* kfun descriptors */
	      kd_allocate_int, kd_allocate_float;

/*
 * initialize optimizer
 */
void Optimize::init()
{
    /* kfuns are already initialized at this point */
    kd_status = ((LPCint) KFCALL << 24) | KF_STATUS;
    kd_call_trace = ((LPCint) KFCALL << 24) | KF_CALL_TRACE;
    kd_allocate = ((LPCint) KFCALL << 24) | KFun::kfunc("allocate");
    kd_allocate_int = ((LPCint) KFCALL << 24) | KFun::kfunc("allocate_int");
    kd_allocate_float = ((LPCint) KFCALL << 24) | KFun::kfunc("allocate_float");
}

/*
 * optimize an lvalue
 */
Uint Optimize::lvalue(Node *n)
{
    Node *m;

    if (n->type == N_CAST) {
	n = n->l.left;
    }
    switch (n->type) {
    case N_INDEX:
	m = n;
	if (m->l.left->type == N_CAST) {
	    m = m->l.left;
	}
	switch (m->l.left->type) {
	case N_INDEX:
	    /* strarray[x][y] = 'c'; */
	    return max2(expr(&m->l.left, FALSE),
			expr(&n->r.right, FALSE) + 3);

	default:
	    return max2(expr(&n->l.left, FALSE),
			expr(&n->r.right, FALSE) + 1);
	}

    default:
	return 0;
    }
}

/*
 * optimize a binary operator constant expression
 */
Uint Optimize::binconst(Node **m)
{
    Node *n;
    Float f1, f2;
    bool flag;

    n = *m;
    if (n->l.left->type != n->r.right->type) {
	if (n->type == N_EQ) {
	    n->toint(FALSE);
	} else if (n->type == N_NE) {
	    n->toint(TRUE);
	} else {
	    return 2;	/* runtime error expected */
	}
	return 1;
    }

    switch (n->l.left->type) {
    case N_INT:
	switch (n->type) {
	case N_ADD_INT:
	    n->l.left->l.number += n->r.right->l.number;
	    break;

	case N_AND_INT:
	    n->l.left->l.number &= n->r.right->l.number;
	    break;

	case N_DIV_INT:
	    if (n->r.right->l.number == 0) {
		return 2;	/* runtime error: division by 0 */
	    }
	    n->l.left->l.number /= n->r.right->l.number;
	    break;

	case N_EQ_INT:
	    n->l.left->l.number = (n->l.left->l.number == n->r.right->l.number);
	    break;

	case N_GE_INT:
	    n->l.left->l.number = (n->l.left->l.number >= n->r.right->l.number);
	    break;

	case N_GT_INT:
	    n->l.left->l.number = (n->l.left->l.number > n->r.right->l.number);
	    break;

	case N_LE_INT:
	    n->l.left->l.number = (n->l.left->l.number <= n->r.right->l.number);
	    break;

	case N_LSHIFT_INT:
	    if (n->r.right->l.number & ~(LPCINT_BITS - 1)) {
		if (n->r.right->l.number < 0) {
		    return 2;
		} else {
		    n->l.left->l.number = 0;
		}
	    } else {
		n->l.left->l.number = (LPCuint) n->l.left->l.number <<
				      n->r.right->l.number;
	    }
	    break;

	case N_LT_INT:
	    n->l.left->l.number = (n->l.left->l.number < n->r.right->l.number);
	    break;

	case N_MOD_INT:
	    if (n->r.right->l.number == 0) {
		return 2;	/* runtime error: % 0 */
	    }
	    n->l.left->l.number %= n->r.right->l.number;
	    break;

	case N_MULT_INT:
	    n->l.left->l.number *= n->r.right->l.number;
	    break;

	case N_NE_INT:
	    n->l.left->l.number = (n->l.left->l.number != n->r.right->l.number);
	    break;

	case N_OR_INT:
	    n->l.left->l.number |= n->r.right->l.number;
	    break;

	case N_RSHIFT_INT:
	    if (n->r.right->l.number & ~(LPCINT_BITS - 1)) {
		if (n->r.right->l.number < 0) {
		    return 2;
		}
		n->l.left->l.number = 0;
	    } else {
		n->l.left->l.number = (LPCuint) n->l.left->l.number >>
				      n->r.right->l.number;
	    }
	    break;

	case N_SUB_INT:
	    n->l.left->l.number -= n->r.right->l.number;
	    break;

	case N_XOR_INT:
	    n->l.left->l.number ^= n->r.right->l.number;
	    break;

	default:
	    return 2;	/* runtime error expected */
	}

	*m = n->l.left;
	(*m)->line = n->line;
	return 1;

    case N_FLOAT:
	NFLT_GET(n->l.left, f1);
	NFLT_GET(n->r.right, f2);

	switch (n->type) {
	case N_ADD:
	case N_ADD_FLOAT:
	    f1.add(f2);
	    break;

	case N_DIV:
	case N_DIV_FLOAT:
	    if (NFLT_ISZERO(n->r.right)) {
		return 2;	/* runtime error: division by 0.0 */
	    }
	    f1.div(f2);
	    break;

	case N_EQ:
	case N_EQ_FLOAT:
	    n->l.left->toint((f1.cmp(f2) == 0));
	    break;

	case N_GE:
	case N_GE_FLOAT:
	    n->l.left->toint((f1.cmp(f2) >= 0));
	    break;

	case N_GT:
	case N_GT_FLOAT:
	    n->l.left->toint((f1.cmp(f2) > 0));
	    break;

	case N_LE:
	case N_LE_FLOAT:
	    n->l.left->toint((f1.cmp(f2) <= 0));
	    break;

	case N_LT:
	case N_LT_FLOAT:
	    n->l.left->toint((f1.cmp(f2) < 0));
	    break;

	case N_MULT:
	case N_MULT_FLOAT:
	    f1.mult(f2);
	    break;

	case N_NE:
	case N_NE_FLOAT:
	    n->l.left->toint((f1.cmp(f2) != 0));
	    break;

	case N_SUB:
	case N_SUB_FLOAT:
	    f1.sub(f2);
	    break;

	default:
	    return 2;	/* runtime error expected */
	}

	NFLT_PUT(n->l.left, f1);
	*m = n->l.left;
	(*m)->line = n->line;
	return 1;

    case N_STR:
	switch (n->type) {
	case N_ADD:
	    n->tostr(n->l.left->l.string->add(n->r.right->l.string));
	    return 1;

	case N_EQ:
	    flag = (n->l.left->l.string->cmp(n->r.right->l.string) == 0);
	    break;

	case N_GE:
	    flag = (n->l.left->l.string->cmp(n->r.right->l.string) >= 0);
	    break;

	case N_GT:
	    flag = (n->l.left->l.string->cmp(n->r.right->l.string) > 0);
	    break;

	case N_LE:
	    flag = (n->l.left->l.string->cmp(n->r.right->l.string) <= 0);
	    break;

	case N_LT:
	    flag = (n->l.left->l.string->cmp(n->r.right->l.string) < 0);
	    break;

	case N_NE:
	    flag = (n->l.left->l.string->cmp(n->r.right->l.string) != 0);
	    break;

	default:
	    return 2;	/* runtime error expected */
	}

	n->toint(flag);
	return 1;

    case N_NIL:
	switch (n->type) {
	case N_EQ:
	    flag = TRUE;
	    break;

	case N_NE:
	    flag = FALSE;
	    break;

	default:
	    return 2;	/* runtime error expected */
	}

	n->toint(flag);
	return 1;
    }

    return 2;
}

/*
 * optimize a tst operation
 */
Node *Optimize::tst(Node *n)
{
    Node *m;

    switch (n->type) {
    case N_INT:
	n->l.number = (n->l.number != 0);
	return n;

    case N_FLOAT:
	n->toint(!NFLT_ISZERO(n));
	return n;

    case N_STR:
	n->toint(TRUE);
	return n;

    case N_NIL:
	n->toint(FALSE);
	return n;

    case N_TST:
    case N_NOT:
    case N_LAND:
    case N_EQ:
    case N_EQ_INT:
    case N_EQ_FLOAT:
    case N_NE:
    case N_NE_INT:
    case N_NE_FLOAT:
    case N_GT:
    case N_GT_INT:
    case N_GT_FLOAT:
    case N_GE:
    case N_GE_INT:
    case N_GE_FLOAT:
    case N_LT:
    case N_LT_INT:
    case N_LT_FLOAT:
    case N_LE:
    case N_LE_INT:
    case N_LE_FLOAT:
	return n;

    case N_COMMA:
	n->mod = T_INT;
	n->r.right = tst(n->r.right);
	return n;

    default:
	m = Node::create(n->line);
	m->type = N_TST;
	m->mod = T_INT;
	m->l.left = n;
	return m;
    }
}

/*
 * optimize a not operation
 */
Node *Optimize::_not(Node *n)
{
    Node *m;

    switch (n->type) {
    case N_INT:
	n->toint((LPCint) (n->l.number == 0));
	return n;

    case N_FLOAT:
	n->toint((LPCint) NFLT_ISZERO(n));
	return n;

    case N_STR:
	n->toint((LPCint) FALSE);
	return n;

    case N_NIL:
	n->toint((LPCint) TRUE);
	return n;

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

    case N_EQ_FLOAT:
	n->type = N_NE_FLOAT;
	return n;

    case N_NE:
	n->type = N_EQ;
	return n;

    case N_NE_INT:
	n->type = N_EQ_INT;
	return n;

    case N_NE_FLOAT:
	n->type = N_EQ_FLOAT;
	return n;

    case N_GT:
	n->type = N_LE;
	return n;

    case N_GT_INT:
	n->type = N_LE_INT;
	return n;

    case N_GT_FLOAT:
	n->type = N_LE_FLOAT;
	return n;

    case N_GE:
	n->type = N_LT;
	return n;

    case N_GE_INT:
	n->type = N_LT_INT;
	return n;

    case N_GE_FLOAT:
	n->type = N_LT_FLOAT;
	return n;

    case N_LT:
	n->type = N_GE;
	return n;

    case N_LT_INT:
	n->type = N_GE_INT;
	return n;

    case N_LT_FLOAT:
	n->type = N_GE_FLOAT;
	return n;

    case N_LE:
	n->type = N_GT;
	return n;

    case N_LE_INT:
	n->type = N_GT_INT;
	return n;

    case N_LE_FLOAT:
	n->type = N_GT_FLOAT;
	return n;

    case N_COMMA:
	n->mod = T_INT;
	n->r.right = _not(n->r.right);
	return n;

    default:
	m = Node::create(n->line);
	m->type = N_NOT;
	m->mod = T_INT;
	m->l.left = n;
	return m;
    }
}

/*
 * optimize a binary operator expression
 */
Uint Optimize::binop(Node **m)
{
    Node *n, *t;
    Uint d1, d2, d;
    Float f1, f2;

    n = *m;
    if (n->type == N_ADD && n->r.right->type == N_ADD &&
	n->l.left->mod == n->r.right->mod &&
	(n->mod == T_STRING || (n->mod & T_REF) != 0)) {
	/*
	 * a + (b + c) --> (a + b) + c
	 * the order in which these are added won't affect the final result
	 */
	t = n->l.left;
	n->l.left = n->r.right;
	n->r.right = n->l.left->r.right;
	n->l.left->r.right = n->l.left->l.left;
	n->l.left->l.left = t;
    }

    d1 = expr(&n->l.left, FALSE);
    d2 = expr(&n->r.right, FALSE);

    if (n->type == N_SUM) {
	if (n->l.left->type == N_RANGE) {
	    d1 = max2(d1, 3);
	} else if (n->l.left->type != N_SUM) {
	    d1++;
	}
	if (n->r.right->type == N_RANGE) {
	    d2 = max2(d2, 3);
	} else {
	    d2++;
	}
	return d1 + d2;
    }
    if (n->type == N_ADD) {
	if (n->r.right->type == N_STR &&
	    (n->l.left->type == N_ADD || n->l.left->type == N_SUM) &&
	    n->l.left->r.right->type == N_STR) {
	    /* (x + s1) + s2 */
	    n->r.right->tostr(
		       n->l.left->r.right->l.string->add(n->r.right->l.string));
	    n->l.left = n->l.left->l.left;
	    return d1;
	}

	if (n->l.left->mod == T_STRING || (n->l.left->mod & T_REF) != 0) {
	    /*
	     * see if the summand operator can be used
	     */
	    switch (n->l.left->type) {
	    case N_ADD:
		n->l.left->type = N_SUM;
		d1 += 2;			/* SUM_SIMPLE on both sides */
		if (n->l.left->l.left->type == N_RANGE) {
		    d1++;
		}
		n->type = N_SUM;
		if (n->r.right->type == N_RANGE) {
		    d2 = max2(d2, 3);		/* at least 3 */
		} else {
		    d2++;			/* add SUM_SIMPLE */
		}
		return d1 + d2;

	    case N_FUNC:
		if (n->l.left->r.number == kd_allocate ||
		    n->l.left->r.number == kd_allocate_int ||
		    n->l.left->r.number == kd_allocate_float) {
		    t = n->l.left->l.left->r.right;
		    if (t != (Node *) NULL && t->type != N_PAIR &&
			t->type != N_SPREAD && t->mod == T_INT) {
			d1++;			/* add SUM_ALLOCATE */
			n->type = N_SUM;
			if (n->r.right->type == N_RANGE) {
			    d2 = max2(d2, 3);	/* at least 3 */
			} else {
			    d2++;		/* add SUM_SIMPLE */
			}
			return d1 + d2;
		    }
		}
		/* fall through */
	    default:
		if (n->r.right->type != N_RANGE && n->r.right->type != N_AGGR) {
		    if (n->r.right->type != N_FUNC ||
			(n->r.right->r.number != kd_allocate &&
			 n->r.right->r.number != kd_allocate_int &&
			 n->r.right->r.number != kd_allocate_float)) {
			break;
		    }
		    t = n->r.right->l.left->r.right;
		    if (t == (Node *) NULL || t->type == N_PAIR ||
			t->type == N_SPREAD || t->mod != T_INT) {
			break;
		    }
		}
		/* fall through */
	    case N_AGGR:
		d1++;				/* add SUM_SIMPLE */
		n->type = N_SUM;
		if (n->r.right->type == N_RANGE) {
		    d2 = max2(d2, 3);		/* at least 3 */
		} else {
		    d2++;			/* add SUM_SIMPLE */
		}
		return d1 + d2;

	    case N_RANGE:
		d1 = max2(d1, 3);		/* at least 3 */
		/* fall through */
	    case N_SUM:
		n->type = N_SUM;
		if (n->r.right->type == N_RANGE) {
		    d2 = max2(d2, 3);		/* at least 3 */
		} else {
		    d2++;			/* add SUM_SIMPLE */
		}
		return d1 + d2;
	    }
	}
    }

    if (n->l.left->flags & F_CONST) {
	if (n->r.right->flags & F_CONST) {
	    /* c1 . c2 */
	    return binconst(m);
	}
	switch (n->type) {
	case N_ADD:
	    if (!T_ARITHMETIC(n->l.left->mod) || !T_ARITHMETIC(n->r.right->mod))
	    {
		break;
	    }
	    /* fall through */
	case N_ADD_INT:
	case N_ADD_FLOAT:
	case N_AND:
	case N_AND_INT:
	case N_EQ:
	case N_EQ_INT:
	case N_EQ_FLOAT:
	case N_MULT:
	case N_MULT_INT:
	case N_MULT_FLOAT:
	case N_NE:
	case N_NE_INT:
	case N_NE_FLOAT:
	case N_OR:
	case N_OR_INT:
	case N_XOR:
	case N_XOR_INT:
	    /* swap constant to the right */
	    t = n->l.left;
	    n->l.left = n->r.right;
	    n->r.right = t;
	    d = d1;
	    d1 = d2;
	    d2 = d;
	    break;
	}
    }
    d = max2(d1, d2 + 1);

    if ((n->r.right->type == N_INT && n->r.right->l.number == 0 &&
	 n->l.left->mod == T_INT) ||
	(n->r.right->type == N_FLOAT && NFLT_ISZERO(n->r.right) &&
	 n->l.left->mod == T_FLOAT) ||
	(n->r.right->type == nil_node && n->r.right->l.number == 0 &&
	 n->l.left->mod != T_MIXED && T_POINTER(n->l.left->mod))) {
	/*
	 * int == 0, float == 0.0, ptr == nil
	 */
	switch (n->type) {
	case N_EQ:
	case N_EQ_INT:
	case N_EQ_FLOAT:
	    *m = _not(n->l.left);
	    return d1;

	case N_NE:
	case N_NE_INT:
	case N_NE_FLOAT:
	    *m = tst(n->l.left);
	    return d1;
	}
    }

    if (T_ARITHMETIC(n->mod) && n->mod == n->l.left->mod &&
	n->mod == n->r.right->mod) {
	if (n->r.right->flags & F_CONST) {
	    /* x . c */
	    if ((n->type == n->l.left->type ||
		 (n->mod == T_INT && n->l.left->mod == T_INT &&
		  n->type == n->l.left->type + 1)) &&
		(n->l.left->r.right->flags & F_CONST)) {
		/* (x . c1) . c2 */
		switch (n->type) {
		case N_ADD_FLOAT:
		case N_SUB_FLOAT:
		    NFLT_GET(n->l.left->r.right, f1);
		    NFLT_GET(n->r.right, f2);
		    f1.add(f2);
		    NFLT_PUT(n->l.left->r.right, f1);
		    *m = n->l.left;
		    d = d1;
		    break;

		case N_ADD_INT:
		case N_SUB_INT:
		case N_LSHIFT_INT:
		case N_RSHIFT_INT:
		    n->l.left->r.right->l.number += n->r.right->l.number;
		    *m = n->l.left;
		    d = d1;
		    break;

		case N_AND_INT:
		    n->l.left->r.right->l.number &= n->r.right->l.number;
		    *m = n->l.left;
		    d = d1;
		    break;

		case N_DIV_FLOAT:
		case N_MULT_FLOAT:
		    NFLT_GET(n->l.left->r.right, f1);
		    NFLT_GET(n->r.right, f2);
		    f1.mult(f2);
		    NFLT_PUT(n->l.left->r.right, f1);
		    *m = n->l.left;
		    d = d1;
		    break;

		case N_DIV_INT:
		case N_MULT_INT:
		    n->l.left->r.right->l.number *= n->r.right->l.number;
		    *m = n->l.left;
		    d = d1;
		    break;

		case N_OR_INT:
		    n->l.left->r.right->l.number |= n->r.right->l.number;
		    *m = n->l.left;
		    d = d1;
		    break;

		case N_XOR_INT:
		    n->l.left->r.right->l.number ^= n->r.right->l.number;
		    *m = n->l.left;
		    d = d1;
		    break;
		}
	    } else {
		switch (n->type) {
		case N_ADD_FLOAT:
		    if (n->l.left->type == N_SUB_FLOAT) {
			if (n->l.left->l.left->type == N_FLOAT) {
			    /* (c1 - x) + c2 */
			    NFLT_GET(n->l.left->l.left, f1);
			    NFLT_GET(n->r.right, f2);
			    f1.add(f2);
			    NFLT_PUT(n->l.left->l.left, f1);
			    *m = n->l.left;
			    return d1;
			}
			if (n->l.left->r.right->type == N_FLOAT) {
			    /* (x - c1) + c2 */
			    NFLT_GET(n->l.left->r.right, f1);
			    NFLT_GET(n->r.right, f2);
			    f1.sub(f2);
			    NFLT_PUT(n->l.left->r.right, f1);
			    *m = n->l.left;
			    d = d1;
			}
		    }
		    break;

		case N_ADD_INT:
		    if (n->l.left->type == N_SUB ||
			n->l.left->type == N_SUB_INT) {
			if (n->l.left->l.left->type == N_INT) {
			    /* (c1 - x) + c2 */
			    n->l.left->l.left->l.number += n->r.right->l.number;
			    *m = n->l.left;
			    return d1;
			}
			if (n->l.left->r.right->type == N_INT) {
			    /* (x - c1) + c2 */
			    n->l.left->r.right->l.number -=
							n->r.right->l.number;
			    *m = n->l.left;
			    d = d1;
			}
		    }
		    break;

		case N_DIV_FLOAT:
		    if (n->l.left->type == N_MULT_FLOAT &&
			n->l.left->r.right->type == N_FLOAT &&
			!NFLT_ISZERO(n->r.right)) {
			/* (x * c1) / c2 */
			NFLT_GET(n->l.left->r.right, f1);
			NFLT_GET(n->r.right, f2);
			f1.div(f2);
			NFLT_PUT(n->l.left->r.right, f1);
			*m = n->l.left;
			d = d1;
		    }
		    break;

		case N_MULT_FLOAT:
		    if (n->l.left->type == N_DIV_FLOAT) {
			if (n->l.left->l.left->type == N_FLOAT) {
			    /* (c1 / x) * c2 */
			    NFLT_GET(n->l.left->l.left, f1);
			    NFLT_GET(n->r.right, f2);
			    f1.mult(f2);
			    NFLT_PUT(n->l.left->l.left, f1);
			    *m = n->l.left;
			    return d1;
			}
			if (n->l.left->r.right->type == N_FLOAT &&
			    !NFLT_ISZERO(n->l.left->r.right)) {
			    /* (x / c1) * c2 */
			    NFLT_GET(n->r.right, f1);
			    NFLT_GET(n->l.left->r.right, f2);
			    f1.div(f2);
			    NFLT_PUT(n->r.right, f1);
			    n->l.left = n->l.left->l.left;
			    d = d1;
			}
		    }
		    break;

		case N_SUB_FLOAT:
		    if (n->l.left->type == N_ADD_FLOAT &&
			n->l.left->r.right->type == N_FLOAT) {
			/* (x + c1) - c2 */
			NFLT_GET(n->l.left->r.right, f1);
			NFLT_GET(n->r.right, f2);
			f1.sub(f2);
			NFLT_PUT(n->l.left->r.right, f1);
			*m = n->l.left;
			d = d1;
		    }
		    break;

		case N_SUB_INT:
		    if (n->l.left->type == N_ADD_INT &&
			n->l.left->r.right->type == N_INT) {
			/* (x + c1) - c2 */
			n->l.left->r.right->l.number -= n->r.right->l.number;
			*m = n->l.left;
			d = d1;
		    }
		    break;
		}
	    }
	} else if (n->l.left->flags & F_CONST) {
	    /* c . x */
	    switch (n->type) {
	    case N_SUB_FLOAT:
		if (n->r.right->type == N_SUB_FLOAT) {
		    if (n->r.right->l.left->type == N_FLOAT) {
			/* c1 - (c2 - x) */
			NFLT_GET(n->l.left, f1);
			NFLT_GET(n->r.right->l.left, f2);
			f1.sub(f2);
			n->type = N_ADD;
			n->l.left = n->r.right->r.right;
			n->r.right = n->r.right->l.left;
			NFLT_PUT(n->r.right, f1);
			d = d2;
		    } else if (n->r.right->r.right->type == N_FLOAT) {
			/* c1 - (x - c2) */
			NFLT_GET(n->l.left, f1);
			NFLT_GET(n->r.right->r.right, f2);
			f1.add(f2);
			NFLT_PUT(n->l.left, f1);
			n->r.right = n->r.right->l.left;
			return d2 + 1;
		    }
		} else if (n->r.right->type == N_ADD_FLOAT &&
			   n->r.right->r.right->type == N_FLOAT) {
		    /* c1 - (x + c2) */
		    NFLT_GET(n->l.left, f1);
		    NFLT_GET(n->r.right->r.right, f2);
		    f1.sub(f2);
		    NFLT_PUT(n->l.left, f1);
		    n->r.right = n->r.right->l.left;
		    return d2 + 1;
		}
		break;

	    case N_SUB_INT:
		if ((n->r.right->type == N_SUB ||
		     n->r.right->type == N_SUB_INT)) {
		    if (n->r.right->l.left->type == N_INT) {
			/* c1 - (c2 - x) */
			n->r.right->l.left->l.number -= n->l.left->l.number;
			n->type = n->r.right->type;
			n->l.left = n->r.right->r.right;
			n->r.right = n->r.right->l.left;
			d = d2;
		    } else if (n->r.right->r.right->type == N_INT) {
			/* c1 - (x - c2) */
			n->l.left->l.number += n->r.right->r.right->l.number;
			n->r.right->r.right = n->r.right->l.left;
			n->r.right->l.left = n->l.left;
			*m = n->r.right;
			return d2 + 1;
		    }
		} else if (n->r.right->type == N_ADD_INT &&
			   n->r.right->r.right->type == N_INT) {
		    /* c1 - (x + c2) */
		    n->l.left->l.number -= n->r.right->r.right->l.number;
		    n->r.right = n->r.right->l.left;
		    return d2 + 1;
		}
		break;

	    case N_DIV_FLOAT:
		if (n->r.right->type == N_DIV_FLOAT) {
		    if (n->r.right->l.left->type == N_FLOAT &&
			!NFLT_ISZERO(n->r.right->l.left)) {
			/* c1 / (c2 / x) */
			NFLT_GET(n->l.left, f1);
			NFLT_GET(n->r.right->l.left, f2);
			f1.div(f2);
			n->type = N_MULT;
			n->l.left = n->r.right->r.right;
			n->r.right = n->r.right->l.left;
			NFLT_PUT(n->r.right, f1);
			d = d2;
		    } else if (n->r.right->r.right->type == N_FLOAT) {
			/* c1 / (x / c2) */
			NFLT_GET(n->l.left, f1);
			NFLT_GET(n->r.right->r.right, f2);
			f1.mult(f2);
			NFLT_PUT(n->l.left, f1);
			n->r.right = n->r.right->l.left;
			return d2 + 1;
		    }
		} else if (n->r.right->type == N_MULT_FLOAT &&
			   n->r.right->r.right->type == N_FLOAT &&
			   !NFLT_ISZERO(n->r.right->r.right)) {
		    /* c1 / (x * c2) */
		    NFLT_GET(n->l.left, f1);
		    NFLT_GET(n->r.right->r.right, f2);
		    f1.div(f2);
		    NFLT_PUT(n->l.left, f1);
		    n->r.right = n->r.right->l.left;
		    return d2 + 1;
		}
		break;
	    }
	}
	n = *m;

	if (T_ARITHMETIC(n->l.left->mod) && (n->r.right->flags & F_CONST)) {
	    switch (n->type) {
	    case N_ADD:
	    case N_ADD_FLOAT:
	    case N_SUB:
	    case N_SUB_FLOAT:
		if (NFLT_ISZERO(n->r.right)) {
		    *m = n->l.left;
		    d = d1;
		}
		break;

	    case N_ADD_INT:
	    case N_SUB_INT:
	    case N_LSHIFT_INT:
	    case N_RSHIFT_INT:
	    case N_XOR_INT:
		if (n->r.right->l.number == 0) {
		    *m = n->l.left;
		    d = d1;
		}
		break;

	    case N_AND_INT:
		if (n->r.right->l.number == 0) {
		    n->type = N_COMMA;
		    return expr(m, FALSE);
		}
		if (n->r.right->l.number == -1) {
		    *m = n->l.left;
		    d = d1;
		}
		break;

	    case N_MULT:
	    case N_MULT_FLOAT:
		if (NFLT_ISZERO(n->r.right)) {
		    n->type = N_COMMA;
		    return expr(m, FALSE);
		}
		/* fall through */
	    case N_DIV:
	    case N_DIV_FLOAT:
		if (NFLT_ISONE(n->r.right)) {
		    *m = n->l.left;
		    d = d1;
		}
		break;

	    case N_MULT_INT:
		if (n->r.right->l.number == 0) {
		    n->type = N_COMMA;
		    return expr(m, FALSE);
		}
		/* fall through */
	    case N_DIV_INT:
		if (n->r.right->l.number == 1) {
		    *m = n->l.left;
		    d = d1;
		}
		break;

	    case N_MOD_INT:
		if (n->r.right->l.number == 1) {
		    n->r.right->l.number = 0;
		    n->type = N_COMMA;
		    return expr(m, FALSE);
		}
		break;

	    case N_OR_INT:
		if (n->r.right->l.number == -1) {
		    n->type = N_COMMA;
		    return expr(m, FALSE);
		}
		if (n->r.right->l.number == 0) {
		    *m = n->l.left;
		    d = d1;
		}
		break;
	    }
	}
    }

    return d;
}

/*
 * optimize an assignment expression
 */
Uint Optimize::assignExpr(Node **m, bool pop)
{
    Node *n, *t;
    Uint d1, d2;

    n = *m;
    d2 = expr(&n->r.right, FALSE);

    if ((n->r.right->type == N_INT || n->r.right->type == N_FLOAT) &&
	n->l.left->mod == n->r.right->mod) {
	switch (n->type) {
	case N_ADD_EQ:
	case N_ADD_EQ_FLOAT:
	    if (NFLT_ISZERO(n->r.right)) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    if (NFLT_ISONE(n->r.right)) {
		n->type = N_ADD_EQ_1_FLOAT;
		return lvalue(n->l.left) + 1;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		n->type = N_SUB_EQ_1_FLOAT;
		return lvalue(n->l.left) + 1;
	    }
	    break;

	case N_ADD_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    if (n->r.right->l.number == 1) {
		n->type = N_ADD_EQ_1_INT;
		return lvalue(n->l.left) + 1;
	    }
	    if (n->r.right->l.number == -1) {
		n->type = N_SUB_EQ_1_INT;
		return lvalue(n->l.left) + 1;
	    }
	    break;

	case N_AND_EQ_INT:
	    if (n->r.right->l.number == 0) {
		n->type = N_ASSIGN;
		return expr(m, pop);
	    }
	    if (n->r.right->l.number == -1) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    break;

	case N_MULT_EQ:
	case N_MULT_EQ_FLOAT:
	    if (NFLT_ISZERO(n->r.right)) {
		n->type = N_ASSIGN;
		return expr(m, pop);
	    }
	    /* fall through */
	case N_DIV_EQ:
	case N_DIV_EQ_FLOAT:
	    if (NFLT_ISONE(n->r.right)) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    break;

	case N_MULT_EQ_INT:
	    if (n->r.right->l.number == 0) {
		n->type = N_ASSIGN;
		return expr(m, pop);
	    }
	    /* fall through */
	case N_DIV_EQ_INT:
	    if (n->r.right->l.number == 1) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    break;

	case N_LSHIFT_EQ_INT:
	case N_RSHIFT_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    break;

	case N_MOD_EQ_INT:
	    if (n->r.right->l.number == 1) {
		n->type = N_ASSIGN;
		n->r.right->l.number = 0;
		return expr(m, pop);
	    }
	    break;

	case N_OR_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    if (n->r.right->l.number == -1) {
		n->type = N_ASSIGN;
		return expr(m, pop);
	    }
	    break;

	case N_SUB_EQ:
	case N_SUB_EQ_FLOAT:
	    if (NFLT_ISZERO(n->r.right)) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    if (NFLT_ISONE(n->r.right)) {
		n->type = N_SUB_EQ_1_FLOAT;
		return lvalue(n->l.left) + 1;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		n->type = N_ADD_EQ_1_FLOAT;
		return lvalue(n->l.left) + 1;
	    }
	    break;

	case N_SUB_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    if (n->r.right->l.number == 1) {
		n->type = N_SUB_EQ_1_INT;
		return lvalue(n->l.left) + 1;
	    }
	    if (n->r.right->l.number == -1) {
		n->type = N_ADD_EQ_1_INT;
		return lvalue(n->l.left) + 1;
	    }
	    break;

	case N_XOR_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return expr(m, pop);
	    }
	    break;
	}
    }

    d1 = lvalue(n->l.left) + 1;

    if (n->type == N_SUM_EQ) {
	d1++;
	return max2(d1, ((d1 < 6) ? d1 : 6) + d2);
    }
    if (n->type == N_ADD_EQ &&
	(n->mod == T_STRING || (n->mod & T_REF) != 0) &&
	(n->r.right->mod == T_STRING || (n->r.right->mod & T_REF) != 0 ||
	 n->r.right->type == N_RANGE)) {
	/*
	 * see if the summand operator can be used
	 */
	switch (n->r.right->type) {
	case N_ADD:
	    n->r.right->type = N_SUM;
	    d2 += 2;				/* SUM_SIMPLE on both sides */
	    if (n->r.right->l.left->type == N_RANGE) {
		d1++;
	    }
	    n->type = N_SUM_EQ;
	    d1++;				/* add SUM_SIMPLE */
	    return max2(d1, ((d1 < 6) ? d1 : 6) + d2);

	case N_AGGR:
	    d2++;				/* add SUM_SIMPLE */
	    n->type = N_SUM_EQ;
	    d1++;				/* add SUM_SIMPLE */
	    return max2(d1, ((d1 < 6) ? d1 : 6) + d2);

	case N_RANGE:
	    d2 = max2(d2, 3);			/* at least 3 */
	    /* fall through */
	case N_SUM:
	    n->type = N_SUM_EQ;
	    d1++;				/* add SUM_SIMPLE */
	    return max2(d1, ((d1 < 6) ? d1 : 6) + d2);

	case N_FUNC:
	    if (n->r.right->r.number == kd_allocate ||
		n->r.right->r.number == kd_allocate_int ||
		n->r.right->r.number == kd_allocate_float) {
		t = n->r.right->l.left->r.right;
		if (t != (Node *) NULL && t->type != N_PAIR &&
		    t->type != N_SPREAD && t->mod == T_INT) {
		    d2++;			/* add SUM_ALLOCATE */
		    n->type = N_SUM_EQ;
		    d1++;			/* add SUM_SIMPLE */
		    return max2(d1, ((d1 < 6) ? d1 : 6) + d2);
		}
	    }
	    break;
	}
    }

    return max2(d1, ((d1 < 5) ? d1 : 5) + d2);
}

/*
 * test a constant expression
 */
bool Optimize::ctest(Node *n)
{
    if (n->type != N_INT) {
	n->toint((n->type != N_NIL && (n->type != T_FLOAT || !NFLT_ISZERO(n))));
    }
    return (n->l.number != 0);
}

/*
 * optimize a condition
 */
Uint Optimize::cond(Node **m, bool pop)
{
    Uint d;

    d = expr(m, pop);
    if (*m != (Node *) NULL && (*m)->type == N_TST) {
	*m = (*m)->l.left;
    }
    return d;
}

/*
 * optimize an expression
 */
Uint Optimize::expr(Node **m, bool pop)
{
    Uint d1, d2, i;
    Node *n;
    Node **oldside, *side;
    Uint olddepth;

    n = *m;
    switch (n->type) {
    case N_FLOAT:
    case N_GLOBAL:
    case N_INT:
    case N_LOCAL:
    case N_STR:
    case N_NIL:
	return !pop;

    case N_TOINT:
    case N_CAST:
	return expr(&n->l.left, FALSE);

    case N_NEG:
    case N_UMIN:
	return max2(expr(&n->l.left, FALSE), 2);

    case N_CATCH:
	oldside = sideStart(&side, &olddepth);
	d1 = expr(&n->l.left, TRUE);
	if (d1 == 0) {
	    n->l.left = (Node *) NULL;
	}
	d1 = max2(d1, sideEnd(&n->l.left, side, oldside, olddepth));
	if (d1 == 0) {
	    *m = Node::createNil();
	    (*m)->line = n->line;
	    return !pop;
	}
	return d1;

    case N_TOFLOAT:
	if (n->l.left->mod != T_INT) {
	    return expr(&n->l.left, FALSE);
	}
	/* fall through */
    case N_NOT:
    case N_TST:
	if (pop) {
	    *m = n->l.left;
	    return expr(m, TRUE);
	}
	return expr(&n->l.left, FALSE);

    case N_TOSTRING:
	if (pop && (n->l.left->mod == T_INT || n->l.left->mod == T_FLOAT)) {
	    *m = n->l.left;
	    return expr(m, TRUE);
	}
	return expr(&n->l.left, FALSE);

    case N_LVALUE:
	return lvalue(n->l.left);

    case N_ADD_EQ_1:
    case N_ADD_EQ_1_INT:
    case N_ADD_EQ_1_FLOAT:
    case N_SUB_EQ_1:
    case N_SUB_EQ_1_INT:
    case N_SUB_EQ_1_FLOAT:
	return lvalue(n->l.left) + 1;

    case N_MIN_MIN:
	if (pop) {
	    n->type = N_SUB_EQ_1;
	}
	return lvalue(n->l.left) + 1;

    case N_MIN_MIN_INT:
	if (pop) {
	    n->type = N_SUB_EQ_1_INT;
	}
	return lvalue(n->l.left) + 1;

    case N_MIN_MIN_FLOAT:
	if (pop) {
	    n->type = N_SUB_EQ_1_FLOAT;
	}
	return lvalue(n->l.left) + 1;

    case N_PLUS_PLUS:
	if (pop) {
	    n->type = N_ADD_EQ_1;
	}
	return lvalue(n->l.left) + 1;

    case N_PLUS_PLUS_INT:
	if (pop) {
	    n->type = N_ADD_EQ_1_INT;
	}
	return lvalue(n->l.left) + 1;

    case N_PLUS_PLUS_FLOAT:
	if (pop) {
	    n->type = N_ADD_EQ_1_FLOAT;
	}
	return lvalue(n->l.left) + 1;

    case N_FUNC:
	m = &n->l.left->r.right;
	n = *m;
	if (n == (Node *) NULL) {
	    return 1;
	}

	d1 = 0;
	for (i = 0; n->type == N_PAIR; ) {
	    oldside = sideStart(&side, &olddepth);
	    d2 = expr(&n->l.left, FALSE);
	    d1 = max3(d1, i + d2,
		      i + sideEnd(&n->l.left, side, oldside, olddepth));
	    m = &n->r.right;
	    n = n->l.left;
	    i += (n->type == N_LVALUE ||
		  (n->type == N_COMMA && n->r.right->type == N_LVALUE)) ? 6 : 1;
	    n = *m;
	}
	if (n->type == N_SPREAD) {
	    m = &n->l.left;
	}
	oldside = sideStart(&side, &olddepth);
	d2 = expr(m, FALSE);
	d1 = max3(d1, i + d2, i + sideEnd(m, side, oldside, olddepth));
	n = *m;
	if (n->type == N_LVALUE ||
	    (n->type == N_COMMA && n->r.right->type == N_LVALUE)) {
	    d1 += 2;
	}
	return d1;

    case N_INSTANCEOF:
	return expr(&n->l.left, FALSE) + 1;

    case N_GE:
    case N_GT:
    case N_LE:
    case N_LT:
	if (n->l.left->mod != n->r.right->mod) {
	    return max2(expr(&n->l.left, FALSE),
			expr(&n->r.right, FALSE) + 1);
	}
	/* fall through */
    case N_EQ:
    case N_NE:
	if (pop) {
	    d1 = expr(&n->l.left, TRUE);
	    if (d1 == 0) {
		*m = n->r.right;
		return expr(m, TRUE);
	    }
	    d2 = expr(&n->r.right, TRUE);
	    if (d2 == 0) {
		*m = n->l.left;
		return d1;
	    }
	    n->type = N_COMMA;
	    sideAdd(m, d1);
	    return d2;
	}
	return binop(m);

    case N_DIV_INT:
    case N_MOD_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 0) {
	    d1 = binop(m);
	    return (d1 == 1) ? !pop : d1;
	}
	/* fall through */
    case N_ADD_INT:
    case N_ADD_FLOAT:
    case N_AND_INT:
    case N_DIV_FLOAT:
    case N_EQ_INT:
    case N_EQ_FLOAT:
    case N_GE_INT:
    case N_GE_FLOAT:
    case N_GT_INT:
    case N_GT_FLOAT:
    case N_LE_INT:
    case N_LE_FLOAT:
    case N_LSHIFT_INT:
    case N_LT_INT:
    case N_LT_FLOAT:
    case N_MULT_INT:
    case N_MULT_FLOAT:
    case N_NE_INT:
    case N_NE_FLOAT:
    case N_OR_INT:
    case N_RSHIFT_INT:
    case N_SUB_INT:
    case N_SUB_FLOAT:
    case N_XOR_INT:
	if (pop) {
	    d1 = expr(&n->l.left, TRUE);
	    if (d1 == 0) {
		*m = n->r.right;
		return expr(m, TRUE);
	    }
	    d2 = expr(&n->r.right, TRUE);
	    if (d2 == 0) {
		*m = n->l.left;
		return d1;
	    }
	    n->type = N_COMMA;
	    sideAdd(m, d1);
	    return d2;
	}
	/* fall through */
    case N_ADD:
    case N_AND:
    case N_DIV:
    case N_LSHIFT:
    case N_MOD:
    case N_MULT:
    case N_OR:
    case N_RSHIFT:
    case N_SUB:
    case N_SUM:
    case N_XOR:
	d1 = binop(m);
	return (d1 == 1) ? !pop : d1;

    case N_INDEX:
	if (n->l.left->type == N_STR && n->r.right->type == N_INT) {
	    if (n->r.right->l.number < 0 ||
		n->r.right->l.number >= (long) n->l.left->l.string->len) {
		return 2;
	    }
	    n->toint(n->l.left->l.string->index(n->r.right->l.number));
	    return !pop;
	}
	if (n->l.left->type == N_FUNC && n->r.right->mod == T_INT) {
	    if (n->l.left->r.number == kd_status) {
		n->type = N_FUNC;
		if (n->l.left->l.left->r.right != (Node *) NULL) {
		    /* status(obj)[i] */
		    n = n->l.left;
		    n->type = N_STR;
		    n->r.right = n->l.left;
		    n->l.string = n->l.left->l.string;
		    n = n->r.right;
		    n->type = N_PAIR;
		    n->l.left = n->r.right;
		    n->r.right = (*m)->r.right;
		    (*m)->r.number = ((LPCint) KFCALL << 24) | KF_STATUSO_IDX;
		} else {
		    /* status()[i] */
		    n->l.left = n->l.left->l.left;
		    n->l.left->r.right = n->r.right;
		    n->r.number = ((LPCint) KFCALL << 24) | KF_STATUS_IDX;
		}
		return expr(m, pop);
	    }
	    if (n->l.left->r.number == kd_call_trace) {
		/* call_trace()[i] */
		n->type = N_FUNC;
		n->l.left = n->l.left->l.left;
		n->l.left->r.right = n->r.right;
		n->r.number = ((LPCint) KFCALL << 24) | KF_CALLTR_IDX;
		return expr(m, pop);
	    }
	}
	if (n->l.left->type == N_INDEX && n->l.left->l.left->type == N_FUNC &&
	    n->l.left->l.left->r.number == kd_call_trace &&
	    n->r.right->mod == T_INT && n->l.left->r.right->mod == T_INT) {
	    /* call_trace()[i][j] */
	    n->type = N_FUNC;
	    n->l.left->type = N_PAIR;
	    side = n->l.left->l.left->l.left;
	    side->r.right = n->l.left;
	    n->l.left->l.left = n->l.left->r.right;
	    n->l.left->r.right = n->r.right;
	    n->l.left = side;
	    n->r.number = ((LPCint) KFCALL << 24) | KF_CALLTR_IDX_IDX;
	    return expr(m, pop);
	}
	return max3(expr(&n->l.left, FALSE),
		    expr(&n->r.right, FALSE) + 1, 3);

    case N_ADD_EQ:
    case N_ADD_EQ_INT:
    case N_ADD_EQ_FLOAT:
    case N_AND_EQ:
    case N_AND_EQ_INT:
    case N_DIV_EQ:
    case N_DIV_EQ_INT:
    case N_DIV_EQ_FLOAT:
    case N_LSHIFT_EQ:
    case N_LSHIFT_EQ_INT:
    case N_MOD_EQ:
    case N_MOD_EQ_INT:
    case N_MULT_EQ:
    case N_MULT_EQ_INT:
    case N_MULT_EQ_FLOAT:
    case N_OR_EQ:
    case N_OR_EQ_INT:
    case N_RSHIFT_EQ:
    case N_RSHIFT_EQ_INT:
    case N_SUB_EQ:
    case N_SUB_EQ_INT:
    case N_SUB_EQ_FLOAT:
    case N_SUM_EQ:
    case N_XOR_EQ:
    case N_XOR_EQ_INT:
	return assignExpr(m, pop);

    case N_ASSIGN:
	if (n->l.left->type == N_AGGR) {
	    d2 = 0;
	    for (n = n->l.left->l.left; n->type == N_PAIR; n = n->r.right) {
		d1 = lvalue(n->l.left);
		d2 += 2 + ((d1 < 4) ? d1 : 4);
	    }
	    d1 = lvalue(n);
	    d2 += 2 + ((d1 < 4) ? d1 : 4);
	    return d2 + max2(2, expr(&(*m)->r.right, FALSE));
	} else {
	    d1 = lvalue(n->l.left);
	    return max2(d1, ((d1 < 4) ? d1 : 4) + expr(&n->r.right, FALSE));
	}

    case N_EXCEPTION:
	return 1;

    case N_COMMA:
	sideAdd(m, expr(&n->l.left, TRUE));
	return expr(m, pop);

    case N_LAND:
	d1 = cond(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (!ctest(n->l.left)) {
		/* false && x */
		*m = n->l.left;
		return !pop;
	    }
	    /* true && x */
	    n->type = N_TST;
	    n->l.left = n->r.right;
	    return expr(m, pop);
	}

	oldside = sideStart(&side, &olddepth);
	d2 = cond(&n->r.right, pop);
	if (d2 == 0) {
	    n->r.right = (Node *) NULL;
	}
	d2 = max2(d2, sideEnd(&n->r.right, side, oldside, olddepth));
	if (d2 == 0) {
	    *m = n->l.left;
	    return expr(m, TRUE);
	}
	if (n->r.right->flags & F_CONST) {
	    if (pop) {
		*m = n->l.left;
		return expr(m, TRUE);
	    }
	    if (!ctest(n->r.right)) {
		/* x && false */
		n->type = N_COMMA;
		return expr(m, FALSE);
	    }
	    /* x && true */
	    n->type = N_TST;
	    return d1;
	}
	if (n->r.right->type == N_COMMA) {
	    n = n->r.right;
	    if ((n->r.right->flags & F_CONST) && !ctest(n->r.right)) {
		/* x && (y, false) --> (x && y, false) */
		(*m)->r.right = n->l.left;
		n->l.left = *m;
		*m = n;
		return expr(m, pop);
	    }
	}
	return max2(d1, d2);

    case N_LOR:
	d1 = cond(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (ctest(n->l.left)) {
		/* true || x */
		*m = n->l.left;
		return !pop;
	    }
	    /* false || x */
	    n->type = N_TST;
	    n->l.left = n->r.right;
	    return expr(m, pop);
	}

	oldside = sideStart(&side, &olddepth);
	d2 = cond(&n->r.right, pop);
	if (d2 == 0) {
	    n->r.right = (Node *) NULL;
	}
	d2 = max2(d2, sideEnd(&n->r.right, side, oldside, olddepth));
	if (d2 == 0) {
	    *m = n->l.left;
	    return expr(m, TRUE);
	}
	if (n->r.right->flags & F_CONST) {
	    if (pop) {
		*m = n->l.left;
		return expr(m, TRUE);
	    }
	    if (ctest(n->r.right)) {
		/* x || true */
		n->type = N_COMMA;
		return expr(m, FALSE);
	    }
	    /* x || false */
	    n->type = N_TST;
	    return d1;
	}
	if (n->r.right->type == N_COMMA) {
	    n = n->r.right;
	    if ((n->r.right->flags & F_CONST) && ctest(n->r.right)) {
		/* x || (y, true) --> (x || y, true) */
		(*m)->r.right = n->l.left;
		n->l.left = *m;
		*m = n;
		return expr(m, pop);
	    }
	}
	return max2(d1, d2);

    case N_QUEST:
	i = cond(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (ctest(n->l.left)) {
		*m = n->r.right->l.left;
	    } else {
		*m = n->r.right->r.right;
	    }
	    return expr(m, pop);
	}
	if (n->l.left->type == N_COMMA && (n->l.left->r.right->flags & F_CONST))
	{
	    sideAdd(&n->l.left, i);
	    if (ctest(n->l.left)) {
		*m = n->r.right->l.left;
	    } else {
		*m = n->r.right->r.right;
	    }
	    return expr(m, pop);
	}

	n = n->r.right;
	oldside = sideStart(&side, &olddepth);
	d1 = expr(&n->l.left, pop);
	if (d1 == 0) {
	    n->l.left = (Node *) NULL;
	}
	d1 = max2(d1, sideEnd(&n->l.left, side, oldside, olddepth));
	if (d1 == 0) {
	    n->l.left = (Node *) NULL;
	}
	oldside = sideStart(&side, &olddepth);
	d2 = expr(&n->r.right, pop);
	if (d2 == 0) {
	    n->r.right = (Node *) NULL;
	}
	d2 = max2(d2, sideEnd(&n->r.right, side, oldside, olddepth));
	if (d2 == 0) {
	    n->r.right = (Node *) NULL;
	}
	return max3(i, d1, d2);

    case N_RANGE:
	d1 = expr(&n->l.left, FALSE);
	d2 = 1;
	if (n->r.right->l.left != (Node *) NULL) {
	    d2 = expr(&n->r.right->l.left, FALSE);
	    if ((n->l.left->mod == T_STRING || (n->l.left->mod & T_REF) != 0) &&
		n->r.right->l.left->type == N_INT &&
		n->r.right->l.left->l.number == 0) {
		/*
		 * str[0 .. x] or arr[0 .. x]
		 */
		n->r.right->l.left = (Node *) NULL;
		d2 = 1;
	    } else {
		d1 = max2(d1, d2 + 1);
		d2 = 2;
	    }
	}
	if (n->r.right->r.right != (Node *) NULL) {
	    d1 = max2(d1, d2 + expr(&n->r.right->r.right, FALSE));
	}
	if (n->l.left->type == N_STR) {
	    long from, to;

	    if (n->r.right->l.left == (Node *) NULL) {
		from = 0;
	    } else {
		if (n->r.right->l.left->type != N_INT) {
		    return d1;
		}
		from = n->r.right->l.left->l.number;
	    }
	    if (n->r.right->r.right == (Node *) NULL) {
		to = n->l.left->l.string->len - 1;
	    } else {
		if (n->r.right->r.right->type != N_INT) {
		    return d1;
		}
		to = n->r.right->r.right->l.number;
	    }
	    if (from >= 0 && from <= to + 1 &&
		to < (long) n->l.left->l.string->len) {
		n->tostr(n->l.left->l.string->range(from, to));
		return !pop;
	    }
	    return d1;
	}
	return max2(d1, 3);

    case N_AGGR:
	if (n->mod == T_MAPPING) {
	    n = n->l.left;
	    if (n == (Node *) NULL) {
		return 1;
	    }

	    d1 = 0;
	    for (i = 0; n->type == N_PAIR; i += 2) {
		oldside = sideStart(&side, &olddepth);
		d2 = expr(&n->l.left->l.left, FALSE);
		d1 = max3(d1, i + d2, i + sideEnd(&n->l.left->l.left,
						  side, oldside, olddepth));
		oldside = sideStart(&side, &olddepth);
		d2 = expr(&n->l.left->r.right, FALSE);
		d1 = max3(d1, i + 1 + d2,
			  i + 1 + sideEnd(&n->l.left->r.right, side, oldside,
					  olddepth));
		n = n->r.right;
	    }
	    oldside = sideStart(&side, &olddepth);
	    d2 = expr(&n->l.left, FALSE);
	    d1 = max3(d1, i + d2,
		      i + sideEnd(&n->l.left, side, oldside, olddepth));
	    oldside = sideStart(&side, &olddepth);
	    d2 = expr(&n->r.right, FALSE);
	    return max3(d1, i + 1 + d2,
			i + 1 + sideEnd(&n->r.right, side, oldside, olddepth));
	} else {
	    m = &n->l.left;
	    n = *m;
	    if (n == (Node *) NULL) {
		return 1;
	    }

	    d1 = 0;
	    for (i = 0; n->type == N_PAIR; i++) {
		oldside = sideStart(&side, &olddepth);
		d2 = expr(&n->l.left, FALSE);
		d1 = max3(d1, i + d2,
			  i + sideEnd(&n->l.left, side, oldside, olddepth));
		m = &n->r.right;
		n = *m;
	    }
	    oldside = sideStart(&side, &olddepth);
	    d2 = expr(m, FALSE);
	    return max3(d1, i + d2, i + sideEnd(m, side, oldside, olddepth));
	}
    }

# ifdef DEBUG
    EC->fatal("unknown expression type %d", n->type);
# endif
    return 0;
}

/*
 * check if a condition is a constant
 */
int Optimize::constant(Node *n)
{
    if (n->type == N_COMMA) {
	n = n->r.right;
    }
    switch (n->type) {
    case N_INT:
	return (n->l.number != 0);

    case N_FLOAT:
	return (!NFLT_ISZERO(n));

    case N_STR:
	return TRUE;

    case N_NIL:
	return FALSE;

    default:
	return -1;
    }
}

/*
 * skip a statement
 */
Node *Optimize::skip(Node *n)
{
    Node *m;

    if (n == (Node *) NULL || !(n->flags & F_REACH)) {
	return (Node *) NULL;
    }

    for (;;) {
	if (n->type == N_PAIR) {
	    m = n->l.left;
	} else {
	    m = n;
	}

	switch (m->type) {
	case N_BLOCK:
	    m->l.left = skip(m->l.left);
	    if (m->l.left != (Node *) NULL) {
		return n;
	    }
	    break;

	case N_CASE:
	case N_LABEL:
	    return n;

	case N_COMPOUND:
	    m->l.left = skip(m->l.left);
	    if (m->l.left != (Node *) NULL) {
		return n;
	    }
	    break;

	case N_DO:
	case N_FOR:
	case N_FOREVER:
	    m->r.right = skip(m->r.right);
	    if (m->r.right != (Node *) NULL) {
		return n;
	    }
	    break;

	case N_IF:
	    m->r.right->l.left = skip(m->r.right->l.left);
	    m->r.right->r.right = skip(m->r.right->r.right);
	    if (m->r.right->l.left != (Node *) NULL) {
		if (m->r.right->r.right != (Node *) NULL) {
		    m->l.left->toint(TRUE);
		} else {
		    *m = *(m->r.right->l.left);
		}
		return n;
	    } else if (m->r.right->r.right != (Node *) NULL) {
		*m = *(m->r.right->r.right);
		return n;
	    }
	    break;

	case N_CATCH:
	    m->r.right = skip(m->r.right);
	    if (m->r.right != (Node *) NULL) {
		*m = *(m->r.right);
		return n;
	    }
	    break;

	case N_PAIR:
	    if (n->flags & F_REACH) {
		if (m == n) {
		    return skip(m);
		} else {
		    m = skip(m);
		    if (m != (Node *) NULL) {
			n->l.left = m;
			return n;
		    }
		}
	    }
	    break;
	}

	if (n->type == N_PAIR) {
	    n = n->r.right;
	} else {
	    return (Node *) NULL;
	}
    }
}

/*
 * optimize a statement
 */
Node *Optimize::stmt(Node *first, Uint *depth)
{
    Node *n, **m, **prev;
    Uint d;
    Uint d1, d2;
    int i;
    Node *side;


    if (first == (Node *) NULL) {
	*depth = 0;
	return (Node *) NULL;
    }

    d = 0;
    prev = m = &first;

    for (;;) {
	n = ((*m)->type == N_PAIR) ? (*m)->l.left : *m;
	switch (n->type) {
	case N_BLOCK:
	    n->l.left = stmt(n->l.left, &d1);
	    if (n->l.left == (Node *) NULL) {
		n = (Node *) NULL;
	    }
	    d = max2(d, d1);
	    break;

	case N_CASE:
	    n->l.left = stmt(n->l.left, &d1);
	    d = max2(d, d1);
	    break;

	case N_COMPOUND:
	    n->l.left = stmt(n->l.left, &d1);
	    if (n->l.left == (Node *) NULL) {
		n = (Node *) NULL;
	    } else if (n->r.right != (Node *) NULL) {
		n->r.right = stmt(n->r.right, &d2);
		d1 = max2(d1, d2);
	    }
	    d = max2(d, d1);
	    break;

	case N_DO:
	    n->r.right = stmt(n->r.right, &d1);
	    sideStart(&side, depth);
	    d2 = cond(&n->l.left, FALSE);
	    d2 = max2(d2, sideEnd(&n->l.left, side, (Node **) NULL, 0));
	    d = max3(d, d1, d2);
	    break;

	case N_FOR:
	    sideStart(&side, depth);
	    d1 = cond(&n->l.left, FALSE);
	    d1 = max2(d1, sideEnd(&n->l.left, side, (Node **) NULL, 0));
	    i = constant(n->l.left);
	    if (i == 0) {
		/* never */
		n->r.right = skip(n->r.right);
		if (n->r.right == (Node *) NULL) {
		    n->type = N_POP;
		    n = stmt(n, &d1);
		    d = max2(d, d1);
		    break;
		}
	    } else if (i > 0) {
		/* always */
		n->type = N_FOREVER;
		n = stmt(n, &d1);
		d = max2(d, d1);
		break;
	    }

	    n->r.right = stmt(n->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_FOREVER:
	    if (n->l.left != (Node *) NULL) {
		sideStart(&side, depth);
		d1 = expr(&n->l.left, TRUE);
		if (d1 == 0) {
		    n->l.left = (Node *) NULL;
		}
		d1 = max2(d1, sideEnd(&n->l.left, side, (Node **) NULL, 0));
		if (d1 == 0) {
		    n->l.left = (Node *) NULL;
		}
	    } else {
		d1 = 0;
	    }
	    n->r.right = stmt(n->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_RLIMITS:
	    sideStart(&side, depth);
	    d1 = expr(&n->l.left->l.left, FALSE);
	    d1 = max2(d1, sideEnd(&n->l.left->l.left, side, (Node **) NULL,
				   0));

	    sideStart(&side, depth);
	    d2 = expr(&n->l.left->r.right, FALSE);
	    d2 = max2(d2, sideEnd(&n->l.left->r.right, side, (Node **) NULL,
				   0));

	    d1 = max2(d1, d2 + 1);
	    n->r.right = stmt(n->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_CATCH:
	    n->l.left = stmt(n->l.left, &d1);
	    if (n->l.left == (Node *) NULL) {
		n = stmt(skip(n->r.right), &d1);
		d = max2(d, d1);
	    } else {
		n->r.right = stmt(n->r.right, &d2);
		d = max3(d, d1, d2);
	    }
	    break;

	case N_IF:
	    sideStart(&side, depth);
	    d1 = cond(&n->l.left, FALSE);
	    d1 = max2(d1, sideEnd(&n->l.left, side, (Node **) NULL, 0));

	    i = constant(n->l.left);
	    if (i == 0) {
		n->r.right->l.left = skip(n->r.right->l.left);
	    } else if (i > 0) {
		n->r.right->r.right = skip(n->r.right->r.right);
	    }
	    n->r.right->l.left = stmt(n->r.right->l.left, &d2);
	    d1 = max2(d1, d2);
	    n->r.right->r.right = stmt(n->r.right->r.right, &d2);

	    if (n->r.right->l.left == (Node *) NULL) {
		if (n->r.right->r.right == (Node *) NULL) {
		    n->type = N_POP;
		    n = stmt(n, &d1);
		    d = max2(d, d1);
		    break;
		}
		n->l.left = _not(n->l.left);
		n->r.right->l.left = n->r.right->r.right;
		n->r.right->r.right = (Node *) NULL;
	    }
	    d = max3(d, d1, d2);
	    break;

	case N_PAIR:
	    n = stmt(n, &d1);
	    d = max2(d, d1);
	    break;

	case N_POP:
	    sideStart(&side, depth);
	    d1 = expr(&n->l.left, TRUE);
	    if (d1 == 0) {
		n->l.left = (Node *) NULL;
	    }
	    d = max3(d, d1, sideEnd(&n->l.left, side, (Node **) NULL, 0));
	    if (n->l.left == (Node *) NULL) {
		n = (Node *) NULL;
	    }
	    break;

	case N_RETURN:
	    sideStart(&side, depth);
	    d1 = expr(&n->l.left, FALSE);
	    d = max3(d, d1, sideEnd(&n->l.left, side, (Node **) NULL, 0));
	    break;

	case N_SWITCH_INT:
	case N_SWITCH_RANGE:
	case N_SWITCH_STR:
	    n->r.right->r.right = stmt(n->r.right->r.right, &d1);
	    if (n->r.right->r.right == (Node *) NULL) {
		n = n->r.right;
		n->type = N_POP;
		n = stmt(n, &d1);
		d = max2(d, d1);
	    } else {
		sideStart(&side, depth);
		d2 = expr(&n->r.right->l.left, FALSE);
		d2 = max2(d2, sideEnd(&n->r.right->l.left, side,
				       (Node **) NULL, 0));
		d = max3(d, d1, d2);
	    }
	    break;
	}

	if ((*m)->type == N_PAIR) {
	    if (n == (Node *) NULL) {
		*m = (*m)->r.right;
	    } else {
		(*m)->l.left = n;
		if (n->flags & F_END) {
		    n = skip((*m)->r.right);
		    if (n == (Node *) NULL) {
			*m = (*m)->l.left;
			break;
		    }
		    (*m)->r.right = n;
		}
		prev = m;
		m = &(*m)->r.right;
	    }
	} else {
	    *m = n;
	    if (n == (Node *) NULL && prev != m) {
		*prev = (*prev)->l.left;
	    }
	    break;
	}
    }

    *depth = d;
    return first;
}
