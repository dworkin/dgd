# ifndef FUNCDEF
# include "kfun.h"
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add, p_add)
# else
char p_add[] = { C_STATIC | C_LOCAL, T_MIXED, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->add()
 * DESCRIPTION:	value + value
 */
int kf_add()
{
    char buffer[12];
    register string *str;
    register array *a;

    switch (sp[1].type) {
    case T_NUMBER:
	switch (sp->type) {
	case T_NUMBER:
	    sp[1].u.number += sp->u.number;
	    sp++;
	    return 0;

	case T_STRING:
	    sprintf(buffer, "%ld", (long) sp[1].u.number);
	    str = str_new((char *) NULL,
			  (Int) strlen(buffer) + sp->u.string->len);
	    strcpy(str->text, buffer);
	    memcpy(str->text + strlen(buffer), sp->u.string->text,
		   sp->u.string->len);
	    str_del(sp->u.string);
	    sp++;
	    sp->type = T_STRING;
	    str_ref(sp->u.string = str);
	    return 0;
	}
	break;

    case T_STRING:
	switch (sp->type) {
	case T_NUMBER:
	    sprintf(buffer, "%ld", (long) sp->u.number);
	    sp++;
	    str = str_new((char *) NULL,
			  sp->u.string->len + (Int) strlen(buffer));
	    memcpy(str->text, sp->u.string->text, sp->u.string->len);
	    strcpy(str->text + sp->u.string->len, buffer);
	    str_del(sp->u.string);
	    str_ref(sp->u.string = str);
	    return 0;

	case T_STRING:
	    str = str_add(sp[1].u.string, sp->u.string);
	    str_del(sp->u.string);
	    sp++;
	    str_del(sp->u.string);
	    str_ref(sp->u.string = str);
	    return 0;
	}
	break;

    case T_ARRAY:
	if (sp->type == T_ARRAY) {
	    a = arr_add(sp[1].u.array, sp->u.array);
	    arr_del(sp->u.array);
	    sp++;
	    arr_del(sp->u.array);
	    arr_ref(sp->u.array = a);
	    return 0;
	}
	break;

    case T_MAPPING:
	if (sp->type == T_MAPPING) {
	    a = map_add(sp[1].u.array, sp->u.array);
	    arr_del(sp->u.array);
	    sp++;
	    arr_del(sp->u.array);
	    arr_ref(sp->u.array = a);
	    return 0;
	}
	break;

    default:
	return 1;
    }

    return 2;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_int, p_add_int)
# else
char p_add_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->add_int()
 * DESCRIPTION:	int + int
 */
int kf_add_int()
{
    sp[1].u.number += sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("&", kf_and, p_and)
# else
char p_and[] = { C_STATIC | C_LOCAL, T_MIXED, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->and()
 * DESCRIPTION:	value & value
 */
int kf_and()
{
    array *a;

    switch (sp[1].type) {
    case T_NUMBER:
	if (sp->type == T_NUMBER) {
	    sp[1].u.number &= sp->u.number;
	    sp++;
	    return 0;
	}
	break;

    case T_ARRAY:
	if (sp->type == T_ARRAY) {
	    a = arr_intersect(sp[1].u.array, sp->u.array);
	    arr_del(sp->u.array);
	    sp++;
	    arr_del(sp->u.array);
	    arr_ref(sp->u.array = a);
	    return 0;
	}
	break;

    default:
	return 1;
    }

    return 2;
}
# endif


# ifdef FUNCDEF
FUNCDEF("&", kf_and_int, p_and_int)
# else
char p_and_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->and_int()
 * DESCRIPTION:	int & int
 */
int kf_and_int()
{
    sp[1].u.number &= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div, p_div)
# else
char p_div[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 2,
		 T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->div()
 * DESCRIPTION:	int / int
 */
int kf_div()
{
    if (sp->u.number == 0) {
	error("Division by zero");
    }
    sp[1].u.number /= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div, p_div_int)
# else
char p_div_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq, p_eq)
# else
char p_eq[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->eq()
 * DESCRIPTION:	value == value
 */
int kf_eq()
{
    register bool flag;

    if (sp[1].type != sp->type) {
	i_pop(2);
	(--sp)->type = T_NUMBER;
	sp->u.number = FALSE;
	return 0;
    }
    switch (sp[1].type) {
    case T_NUMBER:
	sp[1].u.number = (sp[1].u.number == sp->u.number);
	sp++;
	break;

    case T_OBJECT:
	sp[1].type = T_NUMBER;
	sp[1].u.number = (sp[1].u.object.count == sp->u.object.count);
	sp++;
	break;

    case T_STRING:
	flag = (str_cmp(sp[1].u.string, sp->u.string) == 0);
	str_del(sp->u.string);
	sp++;
	str_del(sp->u.string);
	sp->type = T_NUMBER;
	sp->u.number = flag;
	break;

    case T_ARRAY:
    case T_MAPPING:
	flag = (sp[1].u.array == sp->u.array);
	arr_del(sp->u.array);
	sp++;
	arr_del(sp->u.array);
	sp->type = T_NUMBER;
	sp->u.number = flag;
	break;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq_int, p_eq_int)
# else
char p_eq_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->eq_int()
 * DESCRIPTION:	int == int
 */
int kf_eq_int()
{
    sp[1].u.number == (sp[1].u.number == sp->u.number);
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge, p_ge)
# else
char p_ge[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ge()
 * DESCRIPTION:	value >= value
 */
int kf_ge()
{
    switch (sp[1].type) {
    case T_NUMBER:
	if (sp->type == T_NUMBER) {
	    sp[1].u.number = (sp[1].u.number >= sp->u.number);
	    sp++;
	    return 0;
	}
	break;

    case T_STRING:
	if (sp->type == T_STRING) {
	    bool flag;
    
	    flag = (str_cmp(sp[1].u.string, sp->u.string) >= 0);
	    str_del(sp->u.string);
	    sp++;
	    str_del(sp->u.string);
	    sp->u.number = flag;
	    return 0;
	}
	break;

    default:
	return 1;
    }

    return 2;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_int, p_ge_int)
# else
char p_ge_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->ge_int()
 * DESCRIPTION:	int >= int
 */
int kf_ge_int()
{
    sp[1].u.number = (sp[1].u.number >= sp->u.number);
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt, p_gt)
# else
char p_gt[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->gt()
 * DESCRIPTION:	value > value
 */
int kf_gt()
{
    switch (sp[1].type) {
    case T_NUMBER:
	if (sp->type == T_NUMBER) {
	    sp[1].u.number = (sp[1].u.number > sp->u.number);
	    sp++;
	    return 0;
	}
	break;

    case T_STRING:
	if (sp->type == T_STRING) {
	    bool flag;
    
	    flag = (str_cmp(sp[1].u.string, sp->u.string) > 0);
	    str_del(sp->u.string);
	    sp++;
	    str_del(sp->u.string);
	    sp->u.number = flag;
	    return 0;
	}
	break;

    default:
	return 1;
    }

    return 2;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_int, p_gt_int)
# else
char p_gt_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->gt_int()
 * DESCRIPTION:	int > int
 */
int kf_gt_int()
{
    sp[1].u.number = (sp[1].u.number > sp->u.number);
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le, p_le)
# else
char p_le[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->le()
 * DESCRIPTION:	value <= value
 */
int kf_le()
{
    switch (sp[1].type) {
    case T_NUMBER:
	if (sp->type == T_NUMBER) {
	    sp[1].u.number = (sp[1].u.number <= sp->u.number);
	    sp++;
	    return 0;
	}
	break;

    case T_STRING:
	if (sp->type == T_STRING) {
	    bool flag;
    
	    flag = (str_cmp(sp[1].u.string, sp->u.string) <= 0);
	    str_del(sp->u.string);
	    sp++;
	    str_del(sp->u.string);
	    sp->u.number = flag;
	    return 0;
	}
	break;

    default:
	return 1;
    }

    return 2;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_int, p_le_int)
# else
char p_le_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->gt_int()
 * DESCRIPTION:	int <= int
 */
int kf_le_int()
{
    sp[1].u.number = (sp[1].u.number <= sp->u.number);
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<<", kf_lshift, p_lshift)
# else
char p_lshift[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 2,
		    T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->lshift()
 * DESCRIPTION:	int << int
 */
int kf_lshift()
{
    sp[1].u.number <<= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<<", kf_lshift, p_lshift_int)
# else
char p_lshift_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt, p_lt)
# else
char p_lt[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->lt()
 * DESCRIPTION:	value < value
 */
int kf_lt()
{
    switch (sp[1].type) {
    case T_NUMBER:
	if (sp->type == T_NUMBER) {
	    sp[1].u.number = (sp[1].u.number < sp->u.number);
	    sp++;
	    return 0;
	}
	break;

    case T_STRING:
	if (sp->type == T_STRING) {
	    bool flag;
    
	    flag = (str_cmp(sp[1].u.string, sp->u.string) < 0);
	    str_del(sp->u.string);
	    sp++;
	    str_del(sp->u.string);
	    sp->u.number = flag;
	    return 0;
	}
	break;

    default:
	return 1;
    }

    return 2;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_int, p_lt_int)
# else
char p_lt_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->lt_int()
 * DESCRIPTION:	int < int
 */
int kf_lt_int()
{
    sp[1].u.number = (sp[1].u.number < sp->u.number);
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("%", kf_mod, p_mod)
# else
char p_mod[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 2,
		 T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->mod()
 * DESCRIPTION:	int % int
 */
int kf_mod()
{
    if (sp->u.number == 0) {
	error("Modulus by zero");
    }
    sp[1].u.number %= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("%", kf_mod, p_mod_int)
# else
char p_mod_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult, p_mult)
# else
char p_mult[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 2,
		  T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->mult()
 * DESCRIPTION:	int * int
 */
int kf_mult()
{
    sp[1].u.number *= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult, p_mult_int)
# else
char p_mult_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne, p_ne)
# else
char p_ne[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ne()
 * DESCRIPTION:	value != value
 */
int kf_ne()
{
    register bool flag;

    if (sp[1].type != sp->type) {
	i_pop(2);
	(--sp)->type = T_NUMBER;
	sp->u.number = TRUE;
	return 0;
    }
    switch (sp[1].type) {
    case T_NUMBER:
	sp[1].u.number = (sp[1].u.number != sp->u.number);
	sp++;
	break;

    case T_OBJECT:
	sp[1].type = T_NUMBER;
	sp[1].u.number = (sp[1].u.object.count != sp->u.object.count);
	sp++;
	break;

    case T_STRING:
	flag = (str_cmp(sp[1].u.string, sp->u.string) != 0);
	str_del(sp->u.string);
	sp++;
	str_del(sp->u.string);
	sp->type = T_NUMBER;
	sp->u.number = flag;
	break;

    case T_ARRAY:
    case T_MAPPING:
	flag = (sp[1].u.array != sp->u.array);
	arr_del(sp->u.array);
	sp++;
	arr_del(sp->u.array);
	sp->type = T_NUMBER;
	sp->u.number = flag;
	break;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne_int, p_ne_int)
# else
char p_ne_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->ne_int()
 * DESCRIPTION:	int != int
 */
int kf_ne_int()
{
    sp[1].u.number = (sp[1].u.number != sp->u.number);
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("~", kf_neg, p_neg)
# else
char p_neg[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 1, T_NUMBER };

/*
 * NAME:	kfun->neg()
 * DESCRIPTION:	~ int
 */
int kf_neg()
{
    sp->u.number = ~sp->u.number;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("~", kf_neg, p_neg_int)
# else
char p_neg_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_NUMBER };
# endif


# ifdef FUNCDEF
FUNCDEF("!", kf_not, p_not)
# else
char p_not[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_MIXED };

/*
 * NAME:	kfun->not()
 * DESCRIPTION:	! value
 */
int kf_not()
{
    switch (sp->type) {
    case T_NUMBER:
	sp->u.number = !sp->u.number;
	return 0;

    case T_STRING:
	str_del(sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_del(sp->u.array);
	break;
    }

    sp->type = T_NUMBER;
    sp->u.number = FALSE;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or, p_or)
# else
char p_or[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 2,
		T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->or()
 * DESCRIPTION:	int | int
 */
int kf_or()
{
    sp[1].u.number |= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or, p_or_int)
# else
char p_or_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_range, p_range)
# else
char p_range[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_MIXED, 3,
		   T_MIXED, T_NUMBER, T_NUMBER };
/*
 * NAME:	kfun->range()
 * DESCRIPTION:	value [ int .. int ]
 */
int kf_range()
{
    string *str;
    array *a;

    switch (sp[2].type) {
    case T_STRING:
	str = str_range(sp[2].u.string, sp[1].u.number, sp->u.number);
	sp += 2;
	str_del(sp->u.string);
	str_ref(sp->u.string = str);
	break;

    case T_ARRAY:
	a = arr_range(sp[2].u.array, sp[1].u.number, sp->u.number);
	sp += 2;
	arr_del(sp->u.array);
	arr_ref(sp->u.array = a);
	break;

    default:
	return 1;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">>", kf_rshift, p_rshift)
# else
char p_rshift[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 2,
		    T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->rshift()
 * DESCRIPTION:	int >> int
 */
int kf_rshift()
{
    sp[1].u.number >>= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">>", kf_rshift, p_rshift_int)
# else
char p_rshift_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub, p_sub)
# else
char p_sub[] = { C_STATIC | C_LOCAL, T_MIXED, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->sub()
 * DESCRIPTION:	value - value
 */
int kf_sub()
{
    switch (sp[1].type) {
    case T_NUMBER:
	if (sp->type == T_NUMBER) {
	    sp[1].u.number -= sp->u.number;
	    sp++;
	    return 0;
	}
	break;

    case T_ARRAY:
	if (sp->type == T_ARRAY) {
	    array *a;

	    a = arr_sub(sp[1].u.array, sp->u.array);
	    arr_del(sp->u.array);
	    sp++;
	    arr_del(sp->u.array);
	    arr_ref(sp->u.array = a);
	    return 0;
	}
	break;

    default:
	return 1;
    }

    return 2;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub_int, p_sub_int)
# else
char p_sub_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->sub_int()
 * DESCRIPTION:	int - int
 */
int kf_sub_int()
{
    sp[1].u.number -= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst, p_tst)
# else
char p_tst[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_MIXED };

/*
 * NAME:	kfun->tst()
 * DESCRIPTION:	!! value
 */
int kf_tst()
{
    register bool flag;

    switch (sp->type) {
    case T_NUMBER:
	sp->u.number = (sp->u.number != 0);
	return 0;

    case T_STRING:
	str_del(sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_del(sp->u.array);
	break;
    }

    sp->type = T_NUMBER;
    sp->u.number = TRUE;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("unary -", kf_umin, p_umin)
# else
char p_umin[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 1, T_NUMBER };

/*
 * NAME:	kfun->umin()
 * DESCRIPTION:	- int
 */
int kf_umin()
{
    sp->u.number = -sp->u.number;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("unary -", kf_umin, p_umin_int)
# else
char p_umin_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_NUMBER };
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor, p_xor)
# else
char p_xor[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 2,
		 T_NUMBER, T_NUMBER };

/*
 * NAME:	kfun->xor()
 * DESCRIPTION:	int ^ int
 */
int kf_xor()
{
    sp[1].u.number ^= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor, p_xor_int)
# else
char p_xor_int[] = { C_STATIC | C_LOCAL, T_NUMBER, 2, T_NUMBER, T_NUMBER };
# endif
