# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "node.h"
# include "compile.h"
# include "optimize.h"

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

static node **aside;
static unsigned short sidedepth;

/*
 * NAME:	side->start()
 * DESCRIPTION:	start a side expression
 */
static node **side_start(n, depth)
node **n;
unsigned short *depth;
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
unsigned short depth;
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
static unsigned short side_end(n, side, oldside, olddepth)
node **n, *side, **oldside;
unsigned short olddepth;
{
    unsigned short depth;

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


static unsigned short opt_expr P((node**, bool));

/*
 * NAME:	optimize->lvalue()
 * DESCRIPTION:	optimize an lvalue
 */
static unsigned short opt_lvalue(n)
register node *n;
{
    if (n->type == N_CAST) {
	n = n->l.left;
    }
    if (n->type == N_INDEX) {
	switch (n->l.left->type) {
	case N_LOCAL:
	case N_GLOBAL:
	    return 1;

	case N_INDEX:
	    return max3(max2(opt_expr(&n->l.left->l.left, FALSE),
			     opt_expr(&n->l.left->r.right, FALSE)),
			opt_expr(&n->r.right, FALSE), 3);

	default:
	    return max3(opt_expr(&n->l.left, FALSE),
			opt_expr(&n->r.right, FALSE), 2);
	}
    } else {
	return 1;
    }
}

/*
 * NAME:	optimize->binconst()
 * DESCRIPTION:	optimize a binary operator constant expression
 */
static unsigned short opt_binconst(m)
node **m;
{
    register node *n;
    xfloat f1, f2;

    n = *m;
    if (n->l.left->type != n->r.right->type) {
	if (n->type == N_EQ) {
	    *m = node_int((Int) FALSE);
	} else if (n->type == N_NE) {
	    *m = node_int((Int) TRUE);
	} else {
	    return 2;
	}
	(*m)->line = n->line;
	return 1;
    }

    if (n->l.left->type == N_STR) {
	bool flag;

	switch (n->type) {
	case N_ADD:
	    *m = node_str(str_add(n->l.left->l.string, n->r.right->l.string));
	    (*m)->line = n->line;
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
	    return 2;
	}

	*m = node_int((Int) flag);
	(*m)->line = n->line;
	return 1;
    }

    if (n->l.left->type == N_FLOAT) {
	NFLT_GET(n->l.left, f1);
	NFLT_GET(n->r.right, f2);
    }

    switch (n->type) {
    case N_ADD:
	flt_add(&f1, &f2);
	break;

    case N_ADD_INT:
	n->l.left->l.number += n->r.right->l.number;
	break;

    case N_AND_INT:
	n->l.left->l.number &= n->r.right->l.number;
	break;

    case N_DIV:
	if (NFLT_ISZERO(n->r.right)) {
	    return 2;
	}
	flt_div(&f1, &f2);
	break;

    case N_DIV_INT:
	if (n->r.right->l.number == 0) {
	    return 2;
	}
	n->l.left->l.number /= n->r.right->l.number;
	break;

    case N_EQ:
	n->l.left = node_int((Int) (flt_cmp(&f1, &f2) == 0));
	break;

    case N_EQ_INT:
	n->l.left->l.number = (n->l.left->l.number == n->r.right->l.number);
	break;

    case N_GE:
	n->l.left = node_int((Int) (flt_cmp(&f1, &f2) >= 0));
	break;

    case N_GE_INT:
	n->l.left->l.number = (n->l.left->l.number >= n->r.right->l.number);
	break;

    case N_GT:
	n->l.left = node_int((Int) (flt_cmp(&f1, &f2) > 0));
	break;

    case N_GT_INT:
	n->l.left->l.number = (n->l.left->l.number > n->r.right->l.number);
	break;

    case N_LE:
	n->l.left = node_int((Int) (flt_cmp(&f1, &f2) <= 0));
	break;

    case N_LE_INT:
	n->l.left->l.number = (n->l.left->l.number <= n->r.right->l.number);
	break;

    case N_LSHIFT_INT:
	n->l.left->l.number <<= n->r.right->l.number;
	break;

    case N_LT:
	n->l.left = node_int((Int) (flt_cmp(&f1, &f2) < 0));
	break;

    case N_LT_INT:
	n->l.left->l.number = (n->l.left->l.number < n->r.right->l.number);
	break;

    case N_MOD_INT:
	if (n->r.right->l.number == 0) {
	    return 2;
	}
	n->l.left->l.number %= n->r.right->l.number;
	break;

    case N_MULT:
	flt_mult(&f1, &f2);
	break;

    case N_MULT_INT:
	n->l.left->l.number *= n->r.right->l.number;
	break;

    case N_NE:
	n->l.left = node_int((Int) (flt_cmp(&f1, &f2) != 0));
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

    case N_SUB:
	flt_sub(&f1, &f2);
	break;

    case N_SUB_INT:
	n->l.left->l.number -= n->r.right->l.number;
	break;

    case N_XOR_INT:
	n->l.left->l.number ^= n->r.right->l.number;
	break;

    default:
	return 2;
    }

    if (n->l.left->type == N_FLOAT) {
	NFLT_PUT(n->l.left, f1);
    }
    *m = n->l.left;
    (*m)->line = n->line;
    return 1;
}

/*
 * NAME:	optimize->binop()
 * DESCRIPTION:	optimize a binary operator expression
 */
static unsigned short opt_binop(m)
register node **m;
{
    register node *n, *t;
    unsigned short d1, d2, d;
    xfloat f1, f2;

    n = *m;
    d1 = opt_expr(&n->l.left, FALSE);
    d2 = opt_expr(&n->r.right, FALSE);

    if (n->type == N_ADD) {
	if (n->l.left->type == N_STR && n->r.right->type == N_ADD &&
	    n->r.right->l.left->type == N_STR) {
	    /* s1 + (s2 + x) */
	    n->l.left = node_str(str_add(n->l.left->l.string,
					 n->r.right->l.left->l.string));
	    n->l.left->line = n->r.right->l.left->line;
	    n->r.right = n->r.right->r.right;
	    return d2;
	}
	if (n->r.right->type == N_STR && n->l.left->type == N_ADD &&
	    n->l.left->r.right->type == N_STR) {
	    /* (x + s1) + s2 */
	    n->r.right = node_str(str_add(n->l.left->r.right->l.string,
					  n->r.right->l.string));
	    n->r.right->line = n->l.left->r.right->line;
	    n->l.left = n->l.left->l.left;
	    return d1;
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
			return d2;
		    }
		} else if (n->r.right->type == N_ADD &&
			   n->r.right->r.right->type == N_FLOAT) {
		    /* c1 - (x + c2) */
		    NFLT_GET(n->l.left, f1);
		    NFLT_GET(n->r.right->r.right, f2);
		    flt_sub(&f1, &f2);
		    NFLT_PUT(n->l.left, f1);
		    n->r.right = n->r.right->l.left;
		    return d2;
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
			return d2;
		    }
		} else if (n->r.right->type == N_ADD_INT &&
			   n->r.right->r.right->type == N_INT) {
		    /* c1 - (x + c2) */
		    n->l.left->l.number -= n->r.right->r.right->l.number;
		    n->r.right = n->r.right->l.left;
		    return d2;
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
			return d2;
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
		    return d2;
		}
		break;
	    }
	}
	n = *m;

	if (T_ARITHMETIC(n->l.left->mod) && n->r.right->flags & F_CONST) {
	    switch (n->type) {
	    case N_ADD:
	    case N_SUB:
		if (NFLT_ISZERO(n->r.right)) {
		    *m = n->l.left;
		    --d;
		}
		break;

	    case N_ADD_INT:
	    case N_SUB_INT:
	    case N_LSHIFT_INT:
	    case N_RSHIFT_INT:
	    case N_XOR_INT:
		if (n->r.right->l.number == 0) {
		    *m = n->l.left;
		    --d;
		}
		break;

	    case N_AND_INT:
		if (n->r.right->l.number == 0) {
		    *m = n->r.right;
		    return 1;
		}
		if (n->r.right->l.number == -1) {
		    *m = n->l.left;
		    --d;
		}
		break;

	    case N_MULT:
		if (NFLT_ISZERO(n->r.right)) {
		    *m = n->r.right;
		    return 1;
		}
		/* fall through */
	    case N_DIV:
		if (NFLT_ISONE(n->r.right)) {
		    *m = n->l.left;
		    --d;
		}
		break;

	    case N_MULT_INT:
		if (n->r.right->l.number == 0) {
		    *m = n->r.right;
		    return 1;
		}
		/* fall through */
	    case N_DIV_INT:
		if (n->r.right->l.number == 1) {
		    *m = n->l.left;
		    --d;
		}
		break;

	    case N_MOD_INT:
		if (n->r.right->l.number == 1) {
		    n->r.right->l.number = 0;
		    *m = n->r.right;
		    return 1;
		}
		break;

	    case N_OR_INT:
		if (n->r.right->l.number == -1) {
		    *m = n->r.right;
		    return 1;
		}
		if (n->r.right->l.number == 0) {
		    *m = n->l.left;
		    --d;
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
static unsigned short opt_asgnexp(m, pop)
register node **m;
bool pop;
{
    register node *n;
    unsigned short d1, d2;

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
    return max2(d1, ((d1 < 4) ? d1 : 4) + d2);
}

/*
 * NAME:	optimize->test()
 * DESCRIPTION:	test a constant expression
 */
static bool opt_test(m)
register node **m;
{
    register node *n;

    n = *m;
    if (n->type != N_INT) {
	n = node_int((Int) (n->type != T_FLOAT || !NFLT_ISZERO(n)));
	n->line = (*m)->line;
	*m = n;
    }
    return (n->l.number != 0);
}

/*
 * NAME:	optimize->expr()
 * DESCRIPTION:	optimize an expression
 */
static unsigned short opt_expr(m, pop)
register node **m;
bool pop;
{
    register unsigned short d1, d2, i;
    register node *n;
    node **oldside, *side;
    unsigned short olddepth;

    n = *m;
    switch (n->type) {
    case N_FLOAT:
    case N_GLOBAL:
    case N_INT:
    case N_LOCAL:
    case N_STR:
	return !pop;

    case N_TOINT:
    case N_CAST:
	return opt_expr(&n->l.left, FALSE);

    case N_CATCH:
	d1 = opt_expr(&n->l.left, TRUE);
	if (d1 == 0) {
	    *m = node_int((Int) 0);
	}
	return d1;

    case N_LOCK:
	d1 = opt_expr(&n->l.left, pop);
	if (d1 == 0) {
	    *m = n->l.left;
	}
	return d1;

    case N_TOFLOAT:
	if (n->l.left->mod != T_INT) {
	    return opt_expr(&n->l.left, FALSE);
	}
	/* fall through */
    case N_NOT:
    case N_NOTF:
    case N_NOTI:
    case N_TST:
    case N_TSTF:
    case N_TSTI:
    case N_UPLUS:
	if (pop) {
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
	m = &n->l.left;
	n = n->l.left;
	if (n == (node *) NULL) {
	    return 1;
	}

	d1 = 0;
	for (i = 0; n->type == N_PAIR; i += (i == 0) ? 3 : 1) {
	    oldside = side_start(&side, &olddepth);
	    d2 = opt_expr(&n->l.left, FALSE);
	    d1 = max3(d1, i + d2,
		      i + side_end(&n->l.left, side, oldside, olddepth));
	    m = &n->r.right;
	    n = n->r.right;
	}
	if (n->type == N_SPREAD) {
	    m = &n->l.left;
	}
	oldside = side_start(&side, &olddepth);
	d2 = opt_expr(m, FALSE);
	return max3(d1, i + d2, i + side_end(m, side, oldside, olddepth));

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
	if (!T_ARITHSTR(n->l.left->mod) || !T_ARITHSTR(n->r.right->mod)) {
	    return max2(opt_expr(&n->l.left, FALSE),
			opt_expr(&n->r.right, FALSE) + 1);
	}
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
    case N_XOR:
	d1 = opt_binop(m);
	return (d1 == 1) ? !pop : d1;

    case N_INDEX:
	if (n->l.left->type == N_STR && n->r.right->type == N_INT) {
	    if (n->r.right->l.number < 0 ||
		n->r.right->l.number >= (long) n->l.left->l.string->len) {
		return 2;
	    }
	    *m = node_int((Int) str_index(n->l.left->l.string,
					  (long) n->r.right->l.number));
	    (*m)->line = n->line;
	    return !pop;
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
    case N_XOR_EQ:
    case N_XOR_EQ_INT:
	return opt_asgnexp(m, pop);

    case N_ASSIGN:
	d1 = opt_lvalue(n->l.left);
	return max2(d1, ((d1 < 3) ? d1 : 3) + opt_expr(&n->r.right, FALSE));

    case N_COMMA:
	side_add(m, opt_expr(&n->l.left, TRUE));
	return opt_expr(m, pop);

    case N_PAIR:
	d1 = opt_expr(&n->l.left, pop);
	oldside = side_start(&side, &olddepth);
	d2 = opt_expr(&n->r.right, FALSE);
	d1 = max3(d1, d2, side_end(&n->r.right, side, oldside, olddepth));
	n = n->r.right;
	if (n->type == N_COMMA) {
	    node *t;

	    t = n->l.left;
	    n->l.left = n->r.right;
	    n->r.right = t;
	}
	return d1;

    case N_LAND:
	if (n->l.left->type == N_TST || n->l.left->type == N_TSTF ||
	    n->l.left->type == N_TSTI) {
	    n->l.left = n->l.left->l.left;
	}
	if (n->r.right->type == N_TST || n->r.right->type == N_TSTF ||
	    n->r.right->type == N_TSTI) {
	    n->r.right = n->r.right->l.left;
	}

	d1 = opt_expr(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (!opt_test(&n->l.left)) {
		/* false && x */
		*m = n->l.left;
		return !pop;
	    }
	    /* true && x */
	    *m = n->r.right;
	    return opt_expr(m, pop);
	}

	oldside = side_start(&side, &olddepth);
	d2 = opt_expr(&n->r.right, pop);
	d2 = max2(d2, side_end(&n->r.right, side, oldside, olddepth));
	if (d2 == 0) {
	    *m = n->l.left;
	    if (pop) {
		return opt_expr(m, pop);
	    }
	    return d1;
	}
	if (n->r.right->flags & F_CONST) {
	    if (!opt_test(&n->r.right)) {
		/* x && false */
		n->type = N_COMMA;
		return opt_expr(m, pop);
	    }
	    /* x && true */
	    *m = n->l.left;
	    if (pop) {
		return opt_expr(m, pop);
	    }
	    return d1;
	}
	if (n->r.right->type == N_COMMA) {
	    n = n->r.right;
	    if (n->r.right->flags & F_CONST && !opt_test(&n->r.right)) {
		/* x && (y, false) --> (x && y, false) */
		(*m)->r.right = n->l.left;
		n->l.left = *m;
		*m = n;
		return opt_expr(m, pop);
	    }
	}
	return max2(d1, d2);

    case N_LOR:
	if (n->l.left->type == N_TST || n->l.left->type == N_TSTF ||
	    n->l.left->type == N_TSTI) {
	    n->l.left = n->l.left->l.left;
	}
	if (n->r.right->type == N_TST || n->r.right->type == N_TSTF ||
	    n->r.right->type == N_TSTI) {
	    n->r.right = n->r.right->l.left;
	}

	d1 = opt_expr(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (opt_test(&n->l.left)) {
		/* true || x */
		*m = n->l.left;
		return !pop;
	    }
	    /* false || x */
	    *m = n->r.right;
	    return opt_expr(m, pop);
	}

	oldside = side_start(&side, &olddepth);
	d2 = opt_expr(&n->r.right, pop);
	d2 = max2(d2, side_end(&n->r.right, side, oldside, olddepth));
	if (d2 == 0) {
	    *m = n->l.left;
	    if (pop) {
		return opt_expr(m, pop);
	    }
	    return d1;
	}
	if (n->r.right->flags & F_CONST) {
	    if (opt_test(&n->r.right)) {
		/* x || true */
		n->type = N_COMMA;
		return opt_expr(m, pop);
	    }
	    /* x || false */
	    *m = n->l.left;
	    if (pop) {
		return opt_expr(m, pop);
	    }
	    return d1;
	}
	if (n->r.right->type == N_COMMA) {
	    n = n->r.right;
	    if (n->r.right->flags & F_CONST && opt_test(&n->r.right)) {
		/* x || (y, true) --> (x || y, true) */
		(*m)->r.right = n->l.left;
		n->l.left = *m;
		*m = n;
		return opt_expr(m, pop);
	    }
	}
	return max2(d1, d2);

    case N_QUEST:
	i = opt_expr(&n->l.left, FALSE);
	if (n->l.left->flags & F_CONST) {
	    if (opt_test(&n->l.left)) {
		*m = n->r.right->l.left;
	    } else {
		*m = n->r.right->r.right;
	    }
	    return opt_expr(m, pop);
	}
	if (n->l.left->type == N_COMMA && n->l.left->r.right->flags & F_CONST) {
	    side_add(&n->l.left, i);
	    if (opt_test(&n->l.left)) {
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
	if (n->r.right->l.left != (node *) NULL) {
	    d1 = max2(d1, opt_expr(&n->r.right->l.left, FALSE));
	}
	if (n->r.right->r.right != (node *) NULL) {
	    d1 = max2(d1, opt_expr(&n->r.right->r.right, FALSE));
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
		*m = node_str(str_range(n->l.left->l.string, from, to));
		(*m)->line = n->line;
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
		d2 = opt_expr(&n->r.right->r.right, FALSE);
		d1 = max3(d1, i + d2, i + side_end(&n->r.right->r.right,
						   side, oldside, olddepth));
		oldside = side_start(&side, &olddepth);
		d2 = opt_expr(&n->r.right->l.left, FALSE);
		d1 = max3(d1, i + 1 + d2,
			  i + 1 + side_end(&n->r.right->l.left, side, oldside,
					   olddepth));
		n = n->l.left;
	    }
	    oldside = side_start(&side, &olddepth);
	    d2 = opt_expr(&n->r.right, FALSE);
	    d1 = max3(d1, i + d2,
		      i + side_end(&n->r.right, side, oldside, olddepth));
	    oldside = side_start(&side, &olddepth);
	    d2 = opt_expr(&n->l.left, FALSE);
	    return max3(d1, i + 1 + d2,
			i + 1 + side_end(&n->l.left, side, oldside, olddepth));
	} else {
	    m = &n->l.left;
	    n = n->l.left;
	    if (n == (node *) NULL) {
		return 1;
	    }

	    d1 = 0;
	    for (i = 0; n->type == N_PAIR; i++) {
		oldside = side_start(&side, &olddepth);
		d2 = opt_expr(&n->r.right, FALSE);
		d1 = max3(d1, i + d2,
			  i + side_end(&n->r.right, side, oldside, olddepth));
		m = &n->l.left;
		n = n->l.left;
	    }
	    oldside = side_start(&side, &olddepth);
	    d2 = opt_expr(m, FALSE);
	    return max3(d1, i + d2, i + side_end(m, side, oldside, olddepth));
	}
    }
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
    if (n->flags & F_CONST) {
	return (n->l.number != 0);
    }
    return -1;
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

	case N_DO:
	case N_FOR:
	case N_FOREVER:
	    m->r.right = opt_skip(m->r.right);
	    if (m->r.right != (node *) NULL) {
		return n;
	    }
	    break;

	case N_IF:
	    m->l.left->l.left = opt_skip(m->r.right->l.left);
	    m->r.right->r.right = opt_skip(m->r.right->r.right);
	    if (m->r.right->l.left != (node *) NULL ||
		m->r.right->r.right != (node *) NULL) {
		return n;
	    }
	    break;

	case N_PAIR:
	    if (m->flags & F_REACH) {
		if (m == n) {
		    return opt_skip(m);
		} else {
		    n->l.left = opt_skip(m);
		    return n;
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
void opt_stmt(n, depth)
register node *n;
unsigned short *depth;
{
    register node *m;
    register unsigned short d;
    register int i;
    node *side;
    unsigned short d1, d2;

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
	    opt_stmt(m->l.left, &d1);
	    d = max2(d, d1);
	    break;

	case N_DO:
	    side_start(&side, depth);
	    d1 = opt_expr(&m->l.left, FALSE);
	    d1 = max2(d1, side_end(&m->l.left, side, (node **) NULL, 0));
	    opt_stmt(m->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_FOR:
	    side_start(&side, depth);
	    d1 = opt_expr(&m->l.left, FALSE);
	    d1 = max2(d1, side_end(&m->l.left, side, (node **) NULL, 0));
	    i = opt_const(m->l.left);
	    if (i == 0) {
		m->r.right = opt_skip(m->r.right);
		if (m->r.right == (node *) NULL) {
		    m->type = N_POP;
		    opt_stmt(m, &d1);
		    d = max2(d, d1);
		    break;
		}
	    } else if (i > 0) {
		m->type = N_FOREVER;
		opt_stmt(m, &d1);
		d = max2(d, d1);
		break;
	    }

	    opt_stmt(m->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_FOREVER:
	    if (m->l.left != (node *) NULL) {
		side_start(&side, depth);
		d1 = opt_expr(&m->l.left, TRUE);
		if (d1 == 0) {
		    m->l.left = (node *) NULL;
		}
		d1 = max2(d1, side_end(&m->l.left, side, (node **) NULL, 0));
	    } else {
		d1 = 0;
	    }
	    opt_stmt(m->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;

	case N_IF:
	    side_start(&side, depth);
	    d1 = opt_expr(&m->l.left, FALSE);
	    d1 = max2(d1, side_end(&m->l.left, side, (node **) NULL, 0));
	    i = opt_const(m->l.left);
	    if (i == 0) {
		m->r.right->l.left = opt_skip(m->r.right->l.left);
	    } else if (i > 0) {
		m->r.right->r.right = opt_skip(m->r.right->r.right);
	    }

	    if (m->r.right->l.left == (node *) NULL) {
		if (m->r.right->r.right == (node *) NULL) {
		    m->type = N_POP;
		    opt_stmt(m, &d1);
		    d = max2(d, d1);
		    break;
		}
		m->l.left = c_not(m->l.left);
		m->r.right->l.left = m->r.right->r.right;
		m->r.right->r.right = (node *) NULL;
	    }
	    opt_stmt(m->r.right->l.left, &d2);
	    d = max3(d, d1, d2);
	    opt_stmt(m->r.right->r.right, &d2);
	    d = max2(d, d2);
	    break;

	case N_PAIR:
	    opt_stmt(m, &d1);
	    d = max2(d, d1);
	    break;

	case N_POP:
	    side_start(&side, depth);
	    d1 = opt_expr(&m->l.left, TRUE);
	    if (d1 == 0) {
		m->l.left = (node *) NULL;
	    }
	    d1 = max2(d1, side_end(&m->l.left, side, (node **) NULL, 0));
	    if (d1 == 0) {
		m->type = N_FAKE;
	    } else {
		d = max2(d, d1);
	    }
	    break;

	case N_RETURN:
	    side_start(&side, depth);
	    d1 = opt_expr(&m->l.left, FALSE);
	    d = max3(d, d1, side_end(&m->l.left, side, (node **) NULL, 0));
	    break;

	case N_SWITCH_INT:
	case N_SWITCH_RANGE:
	case N_SWITCH_STR:
	    side_start(&side, depth);
	    d1 = opt_expr(&m->r.right->l.left, FALSE);
	    d1 = max2(d1, side_end(&m->r.right->l.left, side, (node **) NULL,
				   0));
	    opt_stmt(m->r.right->r.right, &d2);
	    d = max3(d, d1, d2);
	    break;
	}
    }

    *depth = d;
}
