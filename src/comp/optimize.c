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
# include "optimize.h"

/*
 * NAME:	max2()
 * DESCRIPTION:	return the maximum of two numbers
 */
static Uint max2(a, b)
Uint a, b;
{
    return (a > b) ? a : b;
}

/*
 * NAME:	max3()
 * DESCRIPTION:	return the maximum of three numbers
 */
static Uint max3(a, b, c)
register Uint a, b, c;
{
    return (a > b) ? ((a > c) ? a : c) : ((b > c) ? b : c);
}

static node **aside;
static Uint sidedepth;

/*
 * NAME:	side->start()
 * DESCRIPTION:	start a side expression
 */
static node **side_start(n, depth)
node **n;
Uint *depth;
{
    node **old;

    *n = (node *) NULL;
    old = aside;
    aside = n;
    *depth = sidedepth;
    sidedepth = 0;
    return old;
}

/*
 * NAME:	side->add()
 * DESCRIPTION:	deal with a side expression
 */
static void side_add(n, depth)
node **n;
Uint depth;
{
    node *m;

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
 * NAME:	side->end()
 * DESCRIPTION:	end a side expression
 */
static Uint side_end(n, side, oldside, olddepth)
node **n, *side, **oldside;
Uint olddepth;
{
    Uint depth;

    if (side != (node *) NULL) {
	if (*n == (node *) NULL) {
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


static Int kf_status, kf_call_trace;	/* kfun descriptors */

/*
 * NAME:	optimize->init()
 * DESCRIPTION:	initialize optimizer
 */
void opt_init()
{
    /* kfuns are already initialized at this point */
    kf_status = ((long) KFCALL << 24) | kf_func("status");
    kf_call_trace = ((long) KFCALL << 24) | kf_func("call_trace");
}

static Uint opt_expr P((node**, int));

/*
 * NAME:	optimize->lvalue()
 * DESCRIPTION:	optimize an lvalue
 */
static Uint opt_lvalue(n)
register node *n;
{
    register node *m;

    if (n->type == N_CAST) {
	n = n->l.left;
    }
    switch (n->type) {
    case N_GLOBAL:
	return 3;

    case N_INDEX:
	m = n->l.left;
	if (m->type == N_CAST) {
	    m = m->l.left;
	}
	switch (m->type) {
	case N_GLOBAL:
	    /* global_strval[x] = 'c'; */
	    return opt_expr(&n->r.right, FALSE) + 3;

	case N_INDEX:
	    /* strarray[x][y] = 'c'; */
	    return max3(opt_expr(&m->l.left, FALSE),
			opt_expr(&m->r.right, FALSE) + 1,
			opt_expr(&n->r.right, FALSE) + 4);

	default:
	    return max2(opt_expr(&n->l.left, FALSE),
			opt_expr(&n->r.right, FALSE) + 2);
	}

    default:
	return 2;
    }
}

/*
 * NAME:	optimize->binconst()
 * DESCRIPTION:	optimize a binary operator constant expression
 */
static Uint opt_binconst(m)
node **m;
{
    register node *n;
    xfloat f1, f2;
    bool flag;

    n = *m;
    if (n->l.left->type != n->r.right->type) {
	if (n->type == N_EQ) {
	    node_toint(n, (Int) FALSE);
	} else if (n->type == N_NE) {
	    node_toint(n, (Int) TRUE);
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
	    n->l.left->l.number <<= n->r.right->l.number;
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
	    n->l.left->l.number >>= n->r.right->l.number;
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
	    flt_add(&f1, &f2);
	    break;

	case N_DIV:
	    if (NFLT_ISZERO(n->r.right)) {
		return 2;	/* runtime error: division by 0.0 */
	    }
	    flt_div(&f1, &f2);
	    break;

	case N_EQ:
	    node_toint(n->l.left, (Int) (flt_cmp(&f1, &f2) == 0));
	    break;

	case N_GE:
	    node_toint(n->l.left, (Int) (flt_cmp(&f1, &f2) >= 0));
	    break;

	case N_GT:
	    node_toint(n->l.left, (Int) (flt_cmp(&f1, &f2) > 0));
	    break;

	case N_LE:
	    node_toint(n->l.left, (Int) (flt_cmp(&f1, &f2) <= 0));
	    break;

	case N_LT:
	    node_toint(n->l.left, (Int) (flt_cmp(&f1, &f2) < 0));
	    break;

	case N_MULT:
	    flt_mult(&f1, &f2);
	    break;

	case N_NE:
	    node_toint(n->l.left, (Int) (flt_cmp(&f1, &f2) != 0));
	    break;

	case N_SUB:
	    flt_sub(&f1, &f2);
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
	    node_tostr(n, str_add(n->l.left->l.string, n->r.right->l.string));
	    return 1;

	case N_EQ:
	    flag = (str_cmp(n->l.left->l.string, n->r.right->l.string) == 0);
	    break;

	case N_GE:
	    flag = (str_cmp(n->l.left->l.string, n->r.right->l.string) >= 0);
	    break;

	case N_GT:
	    flag = (str_cmp(n->l.left->l.string, n->r.right->l.string) > 0);
	    break;

	case N_LE:
	    flag = (str_cmp(n->l.left->l.string, n->r.right->l.string) <= 0);
	    break;

	case N_LT:
	    flag = (str_cmp(n->l.left->l.string, n->r.right->l.string) < 0);
	    break;

	case N_NE:
	    flag = (str_cmp(n->l.left->l.string, n->r.right->l.string) != 0);
	    break;

	default:
	    return 2;	/* runtime error expected */
	}

	node_toint(n, (Int) flag);
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

	node_toint(n, (Int) flag);
	return 1;
    }

    return 2;
}

/*
 * NAME:	optimize->tst()
 * DESCRIPTION:	optimize a tst operation
 */
static node *opt_tst(n)
register node *n;
{
    register node *m;

    switch (n->type) {
    case N_INT:
	n->l.number = (n->l.number != 0);
	return n;

    case N_FLOAT:
	node_toint(n, (Int) !NFLT_ISZERO(n));
	return n;

    case N_STR:
	node_toint(n, (Int) TRUE);
	return n;

    case N_NIL:
	node_toint(n, (Int) FALSE);
	return n;

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
	n->r.right = opt_tst(n->r.right);
	return n;

    default:
	m = node_new(n->line);
	m->type = N_TST;
	m->mod = T_INT;
	m->l.left = n;
	return m;
    }
}

/*
 * NAME:	optimize->not()
 * DESCRIPTION:	optimize a not operation
 */
static node *opt_not(n)
register node *n;
{
    register node *m;

    switch (n->type) {
    case N_INT:
	node_toint(n, (Int) (n->l.number == 0));
	return n;

    case N_FLOAT:
	node_toint(n, (Int) NFLT_ISZERO(n));
	return n;

    case N_STR:
	node_toint(n, (Int) FALSE);
	return n;

    case N_NIL:
	node_toint(n, (Int) TRUE);
	return n;

    case N_LAND:
	n->type = N_LOR;
	n->l.left = opt_not(n->l.left);
	n->r.right = opt_not(n->r.right);
	return n;

    case N_LOR:
	n->type = N_LAND;
	n->l.left = opt_not(n->l.left);
	n->r.right = opt_not(n->r.right);
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
	n->r.right = opt_not(n->r.right);
	return n;

    default:
	m = node_new(n->line);
	m->type = N_NOT;
	m->mod = T_INT;
	m->l.left = n;
	return m;
    }
}

/*
 * NAME:	optimize->binop()
 * DESCRIPTION:	optimize a binary operator expression
 */
static Uint opt_binop(m)
register node **m;
{
    register node *n, *t;
    Uint d1, d2, d;
    xfloat f1, f2;

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

    d1 = opt_expr(&n->l.left, FALSE);
    d2 = opt_expr(&n->r.right, FALSE);

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
	    node_tostr(n->r.right, str_add(n->l.left->r.right->l.string,
					   n->r.right->l.string));
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
		d1 += 2;			/* (-2) on both sides */
		if (n->l.left->l.left->type == N_RANGE) {
		    d1++;
		}
		n->type = N_SUM;
		if (n->r.right->type == N_RANGE) {
		    d2 = max2(d2, 3);		/* at least 3 */
		} else {
		    d2++;			/* add (-2) */
		}
		return d1 + d2;

	    default:
		if (n->r.right->type != N_RANGE && n->r.right->type != N_AGGR) {
		    break;
		}
		/* fall through */
	    case N_AGGR:
		d1++;				/* add (-2) */
		n->type = N_SUM;
		if (n->r.right->type == N_RANGE) {
		    d2 = max2(d2, 3);		/* at least 3 */
		} else {
		    d2++;			/* add (-2) */
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
		    d2++;			/* add (-2) */
		}
		return d1 + d2;
	    }
	}
    }

    if (n->l.left->flags & F_CONST) {
	if (n->r.right->flags & F_CONST) {
	    /* c1 . c2 */
	    return opt_binconst(m);
	}
	switch (n->type) {
	case N_ADD:
	    if (!T_ARITHMETIC(n->l.left->mod) || !T_ARITHMETIC(n->r.right->mod))
	    {
		break;
	    }
	    /* fall through */
	case N_ADD_INT:
	case N_AND:
	case N_AND_INT:
	case N_EQ:
	case N_EQ_INT:
	case N_MULT:
	case N_MULT_INT:
	case N_NE:
	case N_NE_INT:
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
	    *m = opt_not(n->l.left);
	    return d1;

	case N_NE:
	case N_NE_INT:
	    *m = opt_tst(n->l.left);
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
		case N_ADD:
		case N_SUB:
		    NFLT_GET(n->l.left->r.right, f1);
		    NFLT_GET(n->r.right, f2);
		    flt_add(&f1, &f2);
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

		case N_DIV:
		case N_MULT:
		    NFLT_GET(n->l.left->r.right, f1);
		    NFLT_GET(n->r.right, f2);
		    flt_mult(&f1, &f2);
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
		case N_ADD:
		    if (n->l.left->type == N_SUB) {
			if (n->l.left->l.left->type == N_FLOAT) {
			    /* (c1 - x) + c2 */
			    NFLT_GET(n->l.left->l.left, f1);
			    NFLT_GET(n->r.right, f2);
			    flt_add(&f1, &f2);
			    NFLT_PUT(n->l.left->l.left, f1);
			    *m = n->l.left;
			    return d1;
			}
			if (n->l.left->r.right->type == N_FLOAT) {
			    /* (x - c1) + c2 */
			    NFLT_GET(n->l.left->r.right, f1);
			    NFLT_GET(n->r.right, f2);
			    flt_sub(&f1, &f2);
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

		case N_DIV:
		    if (n->l.left->type == N_MULT &&
			n->l.left->r.right->type == N_FLOAT &&
			!NFLT_ISZERO(n->r.right)) {
			/* (x * c1) / c2 */
			NFLT_GET(n->l.left->r.right, f1);
			NFLT_GET(n->r.right, f2);
			flt_div(&f1, &f2);
			NFLT_PUT(n->l.left->r.right, f1);
			*m = n->l.left;
			d = d1;
		    }
		    break;

		case N_MULT:
		    if (n->l.left->type == N_DIV) {
			if (n->l.left->l.left->type == N_FLOAT) {
			    /* (c1 / x) * c2 */
			    NFLT_GET(n->l.left->l.left, f1);
			    NFLT_GET(n->r.right, f2);
			    flt_mult(&f1, &f2);
			    NFLT_PUT(n->l.left->l.left, f1);
			    *m = n->l.left;
			    return d1;
			}
			if (n->l.left->r.right->type == N_FLOAT &&
			    !NFLT_ISZERO(n->l.left->r.right)) {
			    /* (x / c1) * c2 */
			    NFLT_GET(n->r.right, f1);
			    NFLT_GET(n->l.left->r.right, f2);
			    flt_div(&f1, &f2);
			    NFLT_PUT(n->r.right, f1);
			    n->l.left = n->l.left->l.left;
			    d = d1;
			}
		    }
		    break;

		case N_SUB:
		    if (n->l.left->type == N_ADD &&
			n->l.left->r.right->type == N_FLOAT) {
			/* (x + c1) - c2 */
			NFLT_GET(n->l.left->r.right, f1);
			NFLT_GET(n->r.right, f2);
			flt_sub(&f1, &f2);
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
	    case N_SUB:
		if (n->r.right->type == N_SUB) {
		    if (n->r.right->l.left->type == N_FLOAT) {
			/* c1 - (c2 - x) */
			NFLT_GET(n->l.left, f1);
			NFLT_GET(n->r.right->l.left, f2);
			flt_sub(&f1, &f2);
			n->type = N_ADD;
			n->l.left = n->r.right->r.right;
			n->r.right = n->r.right->l.left;
			NFLT_PUT(n->r.right, f1);
			d = d2;
		    } else if (n->r.right->r.right->type == N_FLOAT) {
			/* c1 - (x - c2) */
			NFLT_GET(n->l.left, f1);
			NFLT_GET(n->r.right->r.right, f2);
			flt_add(&f1, &f2);
			NFLT_PUT(n->l.left, f1);
			n->r.right = n->r.right->l.left;
			return d2 + 1;
		    }
		} else if (n->r.right->type == N_ADD &&
			   n->r.right->r.right->type == N_FLOAT) {
		    /* c1 - (x + c2) */
		    NFLT_GET(n->l.left, f1);
		    NFLT_GET(n->r.right->r.right, f2);
		    flt_sub(&f1, &f2);
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

	    case N_DIV:
		if (n->r.right->type == N_DIV) {
		    if (n->r.right->l.left->type == N_FLOAT &&
			!NFLT_ISZERO(n->r.right->l.left)) {
			/* c1 / (c2 / x) */
			NFLT_GET(n->l.left, f1);
			NFLT_GET(n->r.right->l.left, f2);
			flt_div(&f1, &f2);
			n->type = N_MULT;
			n->l.left = n->r.right->r.right;
			n->r.right = n->r.right->l.left;
			NFLT_PUT(n->r.right, f1);
			d = d2;
		    } else if (n->r.right->r.right->type == N_FLOAT) {
			/* c1 / (x / c2) */
			NFLT_GET(n->l.left, f1);
			NFLT_GET(n->r.right->r.right, f2);
			flt_mult(&f1, &f2);
			NFLT_PUT(n->l.left, f1);
			n->r.right = n->r.right->l.left;
			return d2 + 1;
		    }
		} else if (n->r.right->type == N_MULT &&
			   n->r.right->r.right->type == N_FLOAT &&
			   !NFLT_ISZERO(n->r.right->r.right)) {
		    /* c1 / (x * c2) */
		    NFLT_GET(n->l.left, f1);
		    NFLT_GET(n->r.right->r.right, f2);
		    flt_div(&f1, &f2);
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
	    case N_SUB:
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
		    return opt_expr(m, FALSE);
		}
		if (n->r.right->l.number == -1) {
		    *m = n->l.left;
		    d = d1;
		}
		break;

	    case N_MULT:
		if (NFLT_ISZERO(n->r.right)) {
		    n->type = N_COMMA;
		    return opt_expr(m, FALSE);
		}
		/* fall through */
	    case N_DIV:
		if (NFLT_ISONE(n->r.right)) {
		    *m = n->l.left;
		    d = d1;
		}
		break;

	    case N_MULT_INT:
		if (n->r.right->l.number == 0) {
		    n->type = N_COMMA;
		    return opt_expr(m, FALSE);
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
		    return opt_expr(m, FALSE);
		}
		break;

	    case N_OR_INT:
		if (n->r.right->l.number == -1) {
		    n->type = N_COMMA;
		    return opt_expr(m, FALSE);
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
 * NAME:	optimize->asgnexp()
 * DESCRIPTION:	optimize an assignment expression
 */
static Uint opt_asgnexp(m, pop)
register node **m;
bool pop;
{
    register node *n;
    Uint d1, d2;

    n = *m;
    d2 = opt_expr(&n->r.right, FALSE);

    if ((n->r.right->type == N_INT || n->r.right->type == N_FLOAT) &&
	n->l.left->mod == n->r.right->mod) {
	switch (n->type) {
	case N_ADD_EQ:
	    if (NFLT_ISZERO(n->r.right)) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    if (NFLT_ISONE(n->r.right)) {
		n->type = N_ADD_EQ_1;
		return opt_lvalue(n->l.left) + 1;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		n->type = N_SUB_EQ_1;
		return opt_lvalue(n->l.left) + 1;
	    }
	    break;

	case N_ADD_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    if (n->r.right->l.number == 1) {
		n->type = N_ADD_EQ_1_INT;
		return opt_lvalue(n->l.left) + 1;
	    }
	    if (n->r.right->l.number == -1) {
		n->type = N_SUB_EQ_1_INT;
		return opt_lvalue(n->l.left) + 1;
	    }
	    break;

	case N_AND_EQ_INT:
	    if (n->r.right->l.number == 0) {
		n->type = N_ASSIGN;
		return opt_expr(m, pop);
	    }
	    if (n->r.right->l.number == -1) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    break;

	case N_MULT_EQ:
	    if (NFLT_ISZERO(n->r.right)) {
		n->type = N_ASSIGN;
		return opt_expr(m, pop);
	    }
	    /* fall through */
	case N_DIV_EQ:
	    if (NFLT_ISONE(n->r.right)) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    break;

	case N_MULT_EQ_INT:
	    if (n->r.right->l.number == 0) {
		n->type = N_ASSIGN;
		return opt_expr(m, pop);
	    }
	    /* fall through */
	case N_DIV_EQ_INT:
	    if (n->r.right->l.number == 1) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    break;

	case N_LSHIFT_EQ_INT:
	case N_RSHIFT_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    break;

	case N_MOD_EQ_INT:
	    if (n->r.right->l.number == 1) {
		n->type = N_ASSIGN;
		n->r.right->l.number = 0;
		return opt_expr(m, pop);
	    }
	    break;

	case N_OR_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    if (n->r.right->l.number == -1) {
		n->type = N_ASSIGN;
		return opt_expr(m, pop);
	    }
	    break;

	case N_SUB_EQ:
	    if (NFLT_ISZERO(n->r.right)) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    if (NFLT_ISONE(n->r.right)) {
		n->type = N_SUB_EQ_1;
		return opt_lvalue(n->l.left) + 1;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		n->type = N_ADD_EQ_1;
		return opt_lvalue(n->l.left) + 1;
	    }
	    break;

	case N_SUB_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    if (n->r.right->l.number == 1) {
		n->type = N_SUB_EQ_1_INT;
		return opt_lvalue(n->l.left) + 1;
	    }
	    if (n->r.right->l.number == -1) {
		n->type = N_ADD_EQ_1_INT;
		return opt_lvalue(n->l.left) + 1;
	    }
	    break;

	case N_XOR_EQ_INT:
	    if (n->r.right->l.number == 0) {
		*m = n->l.left;
		return opt_expr(m, pop);
	    }
	    break;
	}
    }

    d1 = opt_lvalue(n->l.left) + 1;

    if (n->type == N_SUM_EQ) {
	d1++;
	return max2(d1, ((d1 < 5) ? d1 : 5) + d2);
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
	    d2 += 2;				/* (-2) on both sides */
	    if (n->r.right->l.left->type == N_RANGE) {
		d1++;
	    }
	    n->type = N_SUM_EQ;
	    d1++;				/* add (-2) */
	    return max2(d1, ((d1 < 5) ? d1 : 5) + d2);

	case N_AGGR:
	    d2++;				/* add (-2) */
	    n->type = N_SUM_EQ;
	    d1++;				/* add (-2) */
	    return max2(d1, ((d1 < 5) ? d1 : 5) + d2);

	case N_RANGE:
	    d2 = max2(d2, 3);			/* at least 3 */
	    /* fall through */
	case N_SUM:
	    n->type = N_SUM_EQ;
	    d1++;				/* add (-2) */
	    return max2(d1, ((d1 < 5) ? d1 : 5) + d2);
	}
    }

    return max2(d1, ((d1 < 4) ? d1 : 4) + d2);
}

/*
 * NAME:	optimize->ctest()
 * DESCRIPTION:	test a constant expression
 */
static bool opt_ctest(n)
register node *n;
{
    if (n->type != N_INT) {
	node_toint(n, (Int) (n->type != N_NIL &&
			     (n->type != T_FLOAT || !NFLT_ISZERO(n))));
    }
    return (n->l.number != 0);
}

/*
 * NAME:	optimize->cond()
 * DESCRIPTION:	optimize a condition
 */
static Uint opt_cond(m, pop)
register node **m;
int pop;
{
    Uint d;

    d = opt_expr(m, pop);
    if (*m != (node *) NULL && (*m)->type == N_TST) {
	*m = (*m)->l.left;
    }
    return d;
}

/*
 * NAME:	optimize->expr()
 * DESCRIPTION:	optimize an expression
 */
static Uint opt_expr(m, pop)
register node **m;
int pop;
{
    register Uint d1, d2, i;
    register node *n;
    node **oldside, *side;
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
	return opt_expr(&n->l.left, FALSE);

    case N_CATCH:
	oldside = side_start(&side, &olddepth);
	d1 = opt_expr(&n->l.left, TRUE);
	d1 = max2(d1, side_end(&n->l.left, side, oldside, olddepth));
	if (d1 == 0) {
	    *m = node_nil();
	    (*m)->line = n->line;
	    return !pop;
	}
	return d1;

    case N_TOFLOAT:
	if (n->l.left->mod != T_INT) {
	    return opt_expr(&n->l.left, FALSE);
	}
	/* fall through */
    case N_NOT:
    case N_TST:
	if (pop) {
	    *m = n->l.left;
	    return opt_expr(m, TRUE);
	}
	return opt_expr(&n->l.left, FALSE);

    case N_TOSTRING:
	if (pop && (n->l.left->mod == T_INT || n->l.left->mod == T_FLOAT)) {
	    *m = n->l.left;
	    return opt_expr(m, TRUE);
	}
	return opt_expr(&n->l.left, FALSE);

    case N_LVALUE:
	return opt_lvalue(n->l.left);

    case N_ADD_EQ_1:
    case N_ADD_EQ_1_INT:
    case N_SUB_EQ_1:
    case N_SUB_EQ_1_INT:
	return opt_lvalue(n->l.left) + 1;

    case N_MIN_MIN:
	if (pop) {
	    n->type = N_SUB_EQ_1;
	}
	return opt_lvalue(n->l.left) + 1;

    case N_MIN_MIN_INT:
	if (pop) {
	    n->type = N_SUB_EQ_1_INT;
	}
	return opt_lvalue(n->l.left) + 1;

    case N_PLUS_PLUS:
	if (pop) {
	    n->type = N_ADD_EQ_1;
	}
	return opt_lvalue(n->l.left) + 1;

    case N_PLUS_PLUS_INT:
	if (pop) {
	    n->type = N_ADD_EQ_1_INT;
	}
	return opt_lvalue(n->l.left) + 1;

    case N_FUNC:
	m = &n->l.left->r.right;
	n = *m;
	if (n == (node *) NULL) {
	    return 1;
	}

	d1 = 0;
	for (i = 0; n->type == N_PAIR; ) {
	    oldside = side_start(&side, &olddepth);
	    d2 = opt_expr(&n->l.left, FALSE);
	    d1 = max3(d1, i + d2,
		      i + side_end(&n->l.left, side, oldside, olddepth));
	    m = &n->r.right;
	    n = n->l.left;
	    i += (n->type == N_LVALUE ||
		  (n->type == N_COMMA && n->r.right->type == N_LVALUE)) ? 4 : 1;
	    n = *m;
	}
	if (n->type == N_SPREAD) {
	    m = &n->l.left;
	}
	oldside = side_start(&side, &olddepth);
	d2 = opt_expr(m, FALSE);
	return max3(d1, i + d2, i + side_end(m, side, oldside, olddepth));

    case N_INSTANCEOF:
	return opt_expr(&n->l.left, FALSE) + 1;

    case N_GE:
    case N_GT:
    case N_LE:
    case N_LT:
	if (n->l.left->mod != n->r.right->mod) {
	    return max2(opt_expr(&n->l.left, FALSE),
			opt_expr(&n->r.right, FALSE) + 1);
	}
	/* fall through */
    case N_EQ:
    case N_NE:
	if (pop) {
	    d1 = opt_expr(&n->l.left, TRUE);
	    if (d1 == 0) {
		*m = n->r.right;
		return opt_expr(m, TRUE);
	    }
	    d2 = opt_expr(&n->r.right, TRUE);
	    if (d2 == 0) {
		*m = n->l.left;
		return d1;
	    }
	    n->type = N_COMMA;
	    side_add(m, d1);
	    return d2;
	}
	return opt_binop(m);

    case N_DIV_INT:
    case N_MOD_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == 0) {
	    d1 = opt_binop(m);
	    return (d1 == 1) ? !pop : d1;
	}
	/* fall through */
    case N_ADD_INT:
    case N_AND_INT:
    case N_EQ_INT:
    case N_GE_INT:
    case N_GT_INT:
    case N_LE_INT:
    case N_LSHIFT_INT:
    case N_LT_INT:
    case N_MULT_INT:
    case N_NE_INT:
    case N_OR_INT:
    case N_RSHIFT_INT:
    case N_SUB_INT:
    case N_XOR_INT:
	if (pop) {
	    d1 = opt_expr(&n->l.left, TRUE);
	    if (d1 == 0) {
		*m = n->r.right;
		return opt_expr(m, TRUE);
	    }
	    d2 = opt_expr(&n->r.right, TRUE);
	    if (d2 == 0) {
		*m = n->l.left;
		return d1;
	    }
	    n->type = N_COMMA;
	    side_add(m, d1);
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
	d1 = opt_binop(m);
	return (d1 == 1) ? !pop : d1;

    case N_INDEX:
	if (n->l.left->type == N_STR && n->r.right->type == N_INT) {
	    if (n->r.right->l.number < 0 ||
		n->r.right->l.number >= (long) n->l.left->l.string->len) {
		return 2;
	    }
	    node_toint(n, (Int) str_index(n->l.left->l.string,
					  (long) n->r.right->l.number));
	    return !pop;
	}
	if (n->l.left->type == N_FUNC && n->r.right->mod == T_INT) {
	    if (n->l.left->r.number == kf_status) {
		n->type = N_FUNC;
		if (n->l.left->l.left->r.right != (node *) NULL) {
		    /* status(obj)[i] */
		    n = n->l.left;
		    n->type = N_STR;
		    n->r.right = n->l.left;
		    n->l.string = n->l.left->l.string;
		    n = n->r.right;
		    n->type = N_PAIR;
		    n->l.left = n->r.right;
		    n->r.right = (*m)->r.right;
		    (*m)->r.number = ((long) KFCALL << 24) | KF_STATUSO_IDX;
		} else {
		    /* status()[i] */
		    n->l.left = n->l.left->l.left;
		    n->l.left->r.right = n->r.right;
		    n->r.number = ((long) KFCALL << 24) | KF_STATUS_IDX;
		}
		return opt_expr(m, pop);
	    }
	    if (n->l.left->r.number == kf_call_trace) {
		/* call_trace()[i] */
		n->type = N_FUNC;
		n->l.left = n->l.left->l.left;
		n->l.left->r.right = n->r.right;
		n->r.number = ((long) KFCALL << 24) | KF_CALLTR_IDX;
		return opt_expr(m, pop);
	    }
	}
	return max2(opt_expr(&n->l.left, FALSE),
		    opt_expr(&n->r.right, FALSE) + 1);

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
    case N_RSHIFT_EQ:
    case N_RSHIFT_EQ_INT:
    case N_SUB_EQ:
    case N_SUB_EQ_INT:
    case N_SUM_EQ:
    case N_XOR_EQ:
    case N_XOR_EQ_INT:
	return opt_asgnexp(m, pop);

    case N_ASSIGN:
	if (n->l.left->type == N_AGGR) {
	    d2 = 0;
	    for (n = n->l.left->l.left; n->type == N_PAIR; n = n->r.right) {
		d1 = opt_lvalue(n->l.left);
		d2 += (d1 < 4) ? d1 : 4;
	    }
	    d1 = opt_lvalue(n);
	    d2 += (d1 < 4) ? d1 : 4;
	    return d2 + max2(2, opt_expr(&(*m)->r.right, FALSE));
	} else {
	    d1 = opt_lvalue(n->l.left);
	    return max2(d1, ((d1 < 4) ? d1 : 4) + opt_expr(&n->r.right, FALSE));
	}

    case N_COMMA:
	side_add(m, opt_expr(&n->l.left, TRUE));
	return opt_expr(m, pop);

    case N_LAND:
	d1 = opt_cond(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (!opt_ctest(n->l.left)) {
		/* false && x */
		*m = n->l.left;
		return !pop;
	    }
	    /* true && x */
	    n->type = N_TST;
	    n->l.left = n->r.right;
	    return opt_expr(m, pop);
	}

	oldside = side_start(&side, &olddepth);
	d2 = opt_cond(&n->r.right, pop);
	d2 = max2(d2, side_end(&n->r.right, side, oldside, olddepth));
	if (n->r.right->flags & F_CONST) {
	    if (pop) {
		*m = n->l.left;
		return opt_expr(m, TRUE);
	    }
	    if (!opt_ctest(n->r.right)) {
		/* x && false */
		n->type = N_COMMA;
		return opt_expr(m, FALSE);
	    }
	    /* x && true */
	    n->type = N_TST;
	    return d1;
	}
	if (n->r.right->type == N_COMMA) {
	    n = n->r.right;
	    if ((n->r.right->flags & F_CONST) && !opt_ctest(n->r.right)) {
		/* x && (y, false) --> (x && y, false) */
		(*m)->r.right = n->l.left;
		n->l.left = *m;
		*m = n;
		return opt_expr(m, pop);
	    }
	}
	return max2(d1, d2);

    case N_LOR:
	d1 = opt_cond(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (opt_ctest(n->l.left)) {
		/* true || x */
		*m = n->l.left;
		return !pop;
	    }
	    /* false || x */
	    n->type = N_TST;
	    n->l.left = n->r.right;
	    return opt_expr(m, pop);
	}

	oldside = side_start(&side, &olddepth);
	d2 = opt_cond(&n->r.right, pop);
	d2 = max2(d2, side_end(&n->r.right, side, oldside, olddepth));
	if (n->r.right->flags & F_CONST) {
	    if (pop) {
		*m = n->l.left;
		return opt_expr(m, TRUE);
	    }
	    if (opt_ctest(n->r.right)) {
		/* x || true */
		n->type = N_COMMA;
		return opt_expr(m, FALSE);
	    }
	    /* x || false */
	    n->type = N_TST;
	    return d1;
	}
	if (n->r.right->type == N_COMMA) {
	    n = n->r.right;
	    if ((n->r.right->flags & F_CONST) && opt_ctest(n->r.right)) {
		/* x || (y, true) --> (x || y, true) */
		(*m)->r.right = n->l.left;
		n->l.left = *m;
		*m = n;
		return opt_expr(m, pop);
	    }
	}
	return max2(d1, d2);

    case N_QUEST:
	i = opt_cond(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (opt_ctest(n->l.left)) {
		*m = n->r.right->l.left;
	    } else {
		*m = n->r.right->r.right;
	    }
	    return opt_expr(m, pop);
	}
	if (n->l.left->type == N_COMMA && (n->l.left->r.right->flags & F_CONST))
	{
	    side_add(&n->l.left, i);
	    if (opt_ctest(n->l.left)) {
		*m = n->r.right->l.left;
	    } else {
		*m = n->r.right->r.right;
	    }
	    return opt_expr(m, pop);
	}

	n = n->r.right;
	oldside = side_start(&side, &olddepth);
	d1 = opt_expr(&n->l.left, pop);
	d1 = max2(d1, side_end(&n->l.left, side, oldside, olddepth));
	if (d1 == 0) {
	    n->l.left = (node *) NULL;
	}
	oldside = side_start(&side, &olddepth);
	d2 = opt_expr(&n->r.right, pop);
	d2 = max2(d2, side_end(&n->r.right, side, oldside, olddepth));
	if (d2 == 0) {
	    n->r.right = (node *) NULL;
	}
	return max3(i, d1, d2);

    case N_RANGE:
	d1 = opt_expr(&n->l.left, FALSE);
	d2 = 1;
	if (n->r.right->l.left != (node *) NULL) {
	    d2 = opt_expr(&n->r.right->l.left, FALSE);
	    if ((n->l.left->mod == T_STRING || (n->l.left->mod & T_REF) != 0) &&
		n->r.right->l.left->type == N_INT &&
		n->r.right->l.left->l.number == 0) {
		/*
		 * str[0 .. x] or arr[0 .. x]
		 */
		n->r.right->l.left = (node *) NULL;
		d2 = 1;
	    } else {
		d1 = max2(d1, d2 + 1);
		d2 = 2;
	    }
	}
	if (n->r.right->r.right != (node *) NULL) {
	    d1 = max2(d1, d2 + opt_expr(&n->r.right->r.right, FALSE));
	}
	if (n->l.left->type == N_STR) {
	    long from, to;

	    if (n->r.right->l.left == (node *) NULL) {
		from = 0;
	    } else {
		if (n->r.right->l.left->type != N_INT) {
		    return d1;
		}
		from = n->r.right->l.left->l.number;
	    }
	    if (n->r.right->r.right == (node *) NULL) {
		to = n->l.left->l.string->len - 1;
	    } else {
		if (n->r.right->r.right->type != N_INT) {
		    return d1;
		}
		to = n->r.right->r.right->l.number;
	    }
	    if (from >= 0 && from <= to + 1 &&
		to < (long) n->l.left->l.string->len) {
		node_tostr(n, str_range(n->l.left->l.string, from, to));
		return !pop;
	    }
	}
	return d1;

    case N_AGGR:
	if (n->mod == T_MAPPING) {
	    n = n->l.left;
	    if (n == (node *) NULL) {
		return 1;
	    }

	    d1 = 0;
	    for (i = 0; n->type == N_PAIR; i += 2) {
		oldside = side_start(&side, &olddepth);
		d2 = opt_expr(&n->l.left->l.left, FALSE);
		d1 = max3(d1, i + d2, i + side_end(&n->l.left->l.left,
						   side, oldside, olddepth));
		oldside = side_start(&side, &olddepth);
		d2 = opt_expr(&n->l.left->r.right, FALSE);
		d1 = max3(d1, i + 1 + d2,
			  i + 1 + side_end(&n->l.left->r.right, side, oldside,
					   olddepth));
		n = n->r.right;
	    }
	    oldside = side_start(&side, &olddepth);
	    d2 = opt_expr(&n->l.left, FALSE);
	    d1 = max3(d1, i + d2,
		      i + side_end(&n->l.left, side, oldside, olddepth));
	    oldside = side_start(&side, &olddepth);
	    d2 = opt_expr(&n->r.right, FALSE);
	    return max3(d1, i + 1 + d2,
			i + 1 + side_end(&n->r.right, side, oldside, olddepth));
	} else {
	    m = &n->l.left;
	    n = *m;
	    if (n == (node *) NULL) {
		return 1;
	    }

	    d1 = 0;
	    for (i = 0; n->type == N_PAIR; i++) {
		oldside = side_start(&side, &olddepth);
		d2 = opt_expr(&n->l.left, FALSE);
		d1 = max3(d1, i + d2,
			  i + side_end(&n->l.left, side, oldside, olddepth));
		m = &n->r.right;
		n = *m;
	    }
	    oldside = side_start(&side, &olddepth);
	    d2 = opt_expr(m, FALSE);
	    return max3(d1, i + d2, i + side_end(m, side, oldside, olddepth));
	}
    }

# ifdef DEBUG
    fatal("unknown expression type %d", n->type);
# endif
}

/*
 * NAME:	optimize->const()
 * DESCRIPTION:	check if a condition is a constant
 */
static int opt_const(n)
register node *n;
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
 * NAME:	optimize->skip()
 * DESCRIPTION:	skip a statement
 */
static node *opt_skip(n)
register node *n;
{
    register node *m;

    if (n == (node *) NULL || !(n->flags & F_REACH)) {
	return (node *) NULL;
    }

    for (;;) {
	if (n->type == N_PAIR) {
	    m = n->l.left;
	} else {
	    m = n;
	}

	switch (m->type) {
	case N_BLOCK:
	    m->l.left = opt_skip(m->l.left);
	    if (m->l.left != (node *) NULL) {
		return n;
	    }
	    break;

	case N_CASE:
	    return n;

	case N_COMPOUND:
	    m->l.left = opt_skip(m->l.left);
	    if (m->l.left != (node *) NULL) {
		return n;
	    }
	    break;

	case N_DO:
	case N_FOR:
	case N_FOREVER:
	    m->r.right = opt_skip(m->r.right);
	    if (m->r.right != (node *) NULL) {
		return n;
	    }
	    break;

	case N_IF:
	    m->r.right->l.left = opt_skip(m->r.right->l.left);
	    m->r.right->r.right = opt_skip(m->r.right->r.right);
	    if (m->r.right->l.left != (node *) NULL) {
		if (m->r.right->r.right != (node *) NULL) {
		    node_toint(m->l.left, (Int) TRUE);
		} else {
		    *m = *(m->r.right->l.left);
		}
		return n;
	    } else if (m->r.right->r.right != (node *) NULL) {
		*m = *(m->r.right->r.right);
		return n;
	    }
	    break;

	case N_CATCH:
	    m->r.right = opt_skip(m->r.right);
	    if (m->r.right != (node *) NULL) {
		*m = *(m->r.right);
		return n;
	    }
	    break;

	case N_PAIR:
	    if (n->flags & F_REACH) {
		if (m == n) {
		    return opt_skip(m);
		} else {
		    m = opt_skip(m);
		    if (m != (node *) NULL) {
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
	    return (node *) NULL;
	}
    }
}

/*
 * NAME:	optimize->stmt()
 * DESCRIPTION:	optimize a statement
 */
node *opt_stmt(first, depth)
node *first;
Uint *depth;
{
    register node *n, **m, **prev;
    register Uint d;
    Uint d1, d2;
    register int i;
    node *side;


    if (first == (node *) NULL) {
	*depth = 0;
	return (node *) NULL;
    }

    d = 0;
    prev = m = &first;

    for (;;) {
	n = ((*m)->type == N_PAIR) ? (*m)->l.left : *m;
	switch (n->type) {
	case N_BLOCK:
	    n->l.left = opt_stmt(n->l.left, &d1);
	    if (n->l.left == (node *) NULL) {
		n = (node *) NULL;
	    }
	    d = max2(d, d1);
	    break;

	case N_CASE:
	    n->l.left = opt_stmt(n->l.left, &d1);
	    d = max2(d, d1);
	    break;

	case N_COMPOUND:
	    n->l.left = opt_stmt(n->l.left, &d1);
	    if (n->l.left == (node *) NULL) {
		n = (node *) NULL;
	    } else if (n->r.right != (node *) NULL) {
		n->r.right = opt_stmt(n->r.right, &d2);
		d1 = max2(d1, d2);
	    }
	    d = max2(d, d1);
	    break;

	case N_DO:
	    n->r.right = opt_stmt(n->r.right, &d1);
	    side_start(&side, depth);
	    d2 = opt_cond(&n->l.left, FALSE);
	    d2 = max2(d2, side_end(&n->l.left, side, (node **) NULL, 0));
	    d = max3(d, d1, d2);
	    break;

	case N_FOR:
	    side_start(&side, depth);
	    d1 = opt_cond(&n->l.left, FALSE);
	    d1 = max2(d1, side_end(&n->l.left, side, (node **) NULL, 0));
	    i = opt_const(n->l.left);
	    if (i == 0) {
		/* never */
		n->r.right = opt_skip(n->r.right);
		if (n->r.right == (node *) NULL) {
		    n->type = N_POP;
		    n = opt_stmt(n, &d1);
		    d = max2(d, d1);
		    break;
		}
	    } else if (i > 0) {
		/* always */
		n->type = N_FOREVER;
		n = opt_stmt(n, &d1);
		d = max2(d, d1);
		break;
	    }

	    n->r.right = opt_stmt(n->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_FOREVER:
	    if (n->l.left != (node *) NULL) {
		side_start(&side, depth);
		d1 = opt_expr(&n->l.left, TRUE);
		d1 = max2(d1, side_end(&n->l.left, side, (node **) NULL, 0));
		if (d1 == 0) {
		    n->l.left = (node *) NULL;
		}
	    } else {
		d1 = 0;
	    }
	    n->r.right = opt_stmt(n->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_RLIMITS:
	    side_start(&side, depth);
	    d1 = opt_expr(&n->l.left->l.left, FALSE);
	    d1 = max2(d1, side_end(&n->l.left->l.left, side, (node **) NULL,
				   0));

	    side_start(&side, depth);
	    d2 = opt_expr(&n->l.left->r.right, FALSE);
	    d2 = max2(d2, side_end(&n->l.left->r.right, side, (node **) NULL,
				   0));

	    d1 = max2(d1, d2 + 1);
	    n->r.right = opt_stmt(n->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_CATCH:
	    n->l.left = opt_stmt(n->l.left, &d1);
	    if (n->l.left == (node *) NULL) {
		n = opt_stmt(opt_skip(n->r.right), &d1);
		d = max2(d, d1);
	    } else {
		n->r.right = opt_stmt(n->r.right, &d2);
		d = max3(d, d1, d2);
	    }
	    break;

	case N_IF:
	    side_start(&side, depth);
	    d1 = opt_cond(&n->l.left, FALSE);
	    d1 = max2(d1, side_end(&n->l.left, side, (node **) NULL, 0));

	    i = opt_const(n->l.left);
	    if (i == 0) {
		n->r.right->l.left = opt_skip(n->r.right->l.left);
	    } else if (i > 0) {
		n->r.right->r.right = opt_skip(n->r.right->r.right);
	    }
	    n->r.right->l.left = opt_stmt(n->r.right->l.left, &d2);
	    d1 = max2(d1, d2);
	    n->r.right->r.right = opt_stmt(n->r.right->r.right, &d2);

	    if (n->r.right->l.left == (node *) NULL) {
		if (n->r.right->r.right == (node *) NULL) {
		    n->type = N_POP;
		    n = opt_stmt(n, &d1);
		    d = max2(d, d1);
		    break;
		}
		n->l.left = opt_not(n->l.left);
		n->r.right->l.left = n->r.right->r.right;
		n->r.right->r.right = (node *) NULL;
	    }
	    d = max3(d, d1, d2);
	    break;

	case N_PAIR:
	    n = opt_stmt(n, &d1);
	    d = max2(d, d1);
	    break;

	case N_POP:
	    side_start(&side, depth);
	    d1 = opt_expr(&n->l.left, TRUE);
	    if (d1 == 0) {
		n->l.left = (node *) NULL;
	    }
	    d = max3(d, d1, side_end(&n->l.left, side, (node **) NULL, 0));
	    if (n->l.left == (node *) NULL) {
		n = (node *) NULL;
	    }
	    break;

	case N_RETURN:
	    side_start(&side, depth);
	    d1 = opt_expr(&n->l.left, FALSE);
	    d = max3(d, d1, side_end(&n->l.left, side, (node **) NULL, 0));
	    break;

	case N_SWITCH_INT:
	case N_SWITCH_RANGE:
	case N_SWITCH_STR:
	    n->r.right->r.right = opt_stmt(n->r.right->r.right, &d1);
	    if (n->r.right->r.right == (node *) NULL) {
		n = n->r.right;
		n->type = N_POP;
		n = opt_stmt(n, &d1);
		d = max2(d, d1);
	    } else {
		side_start(&side, depth);
		d2 = opt_expr(&n->r.right->l.left, FALSE);
		d2 = max2(d2, side_end(&n->r.right->l.left, side,
				       (node **) NULL, 0));
		d = max3(d, d1, d2);
	    }
	    break;
	}

	if ((*m)->type == N_PAIR) {
	    if (n == (node *) NULL) {
		*m = (*m)->r.right;
	    } else {
		(*m)->l.left = n;
		if (n->flags & F_END) {
		    n = opt_skip((*m)->r.right);
		    if (n == (node *) NULL) {
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
	    if (n == (node *) NULL && prev != m) {
		*prev = (*prev)->l.left;
	    }
	    break;
	}
    }

    *depth = d;
    return first;
}
