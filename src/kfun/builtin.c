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
    register string *str;
    register array *a;
    char buffer[18];
    xfloat f1, f2;
    long l;

    switch (sp[1].type) {
    case T_INT:
	switch (sp->type) {
	case T_INT:
	    sp[1].u.number += sp->u.number;
	    sp++;
	    return 0;

	case T_STRING:
	    sprintf(buffer, "%ld", (long) sp[1].u.number);
	    str = str_new((char *) NULL,
			  (l=(long) strlen(buffer)) + sp->u.string->len);
	    strcpy(str->text, buffer);
	    memcpy(str->text + l, sp->u.string->text, sp->u.string->len);
	    str_del(sp->u.string);
	    sp++;
	    sp->type = T_STRING;
	    str_ref(sp->u.string = str);
	    return 0;
	}
	break;

    case T_FLOAT:
	switch (sp->type) {
	case T_FLOAT:
	    VFLT_GET(sp, f2);
	    sp++;
	    VFLT_GET(sp, f1);
	    flt_add(&f1, &f2);
	    VFLT_PUT(sp, f1);
	    return 0;

	case T_STRING:
	    VFLT_GET(sp + 1, f1);
	    flt_ftoa(&f1, buffer);
	    str = str_new((char *) NULL,
			  (l=(long) strlen(buffer)) + sp->u.string->len);
	    strcpy(str->text, buffer);
	    memcpy(str->text + l, sp->u.string->text, sp->u.string->len);
	    str_del(sp->u.string);
	    sp++;
	    sp->type = T_STRING;
	    str_ref(sp->u.string = str);
	    return 0;
	}
	break;

    case T_STRING:
	switch (sp->type) {
	case T_INT:
	    sprintf(buffer, "%ld", (long) sp->u.number);
	    sp++;
	    str = str_new((char *) NULL,
			  sp->u.string->len + (long) strlen(buffer));
	    memcpy(str->text, sp->u.string->text, sp->u.string->len);
	    strcpy(str->text + sp->u.string->len, buffer);
	    str_del(sp->u.string);
	    str_ref(sp->u.string = str);
	    return 0;

	case T_FLOAT:
	    VFLT_GET(sp, f2);
	    flt_ftoa(&f2, buffer);
	    sp++;
	    str = str_new((char *) NULL,
			  sp->u.string->len + (long) strlen(buffer));
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
char p_add_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
FUNCDEF("++", kf_add1, p_add1)
# else
char p_add1[] = { C_STATIC | C_LOCAL, T_MIXED, 1, T_MIXED };

/*
 * NAME:	kfun->add1()
 * DESCRIPTION:	value++
 */
int kf_add1()
{
    xfloat f1, f2;

    if (sp->type == T_INT) {
	sp->u.number++;
    } else if (sp->type == T_FLOAT) {
	VFLT_GET(sp, f1);
	FLT_ONE(f2.high, f2.low);
	flt_add(&f1, &f2);
	VFLT_PUT(sp, f1);
    } else {
	return 1;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1_int, p_add1_int)
# else
char p_add1_int[] = { C_STATIC | C_LOCAL, T_INT, 1, T_INT };

/*
 * NAME:	kfun->add1_int()
 * DESCRIPTION:	int++
 */
int kf_add1_int()
{
    sp->u.number++;
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
    case T_INT:
	if (sp->type == T_INT) {
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

    case T_MAPPING:
	if (sp->type == T_ARRAY) {
	    a = map_intersect(sp[1].u.array, sp->u.array);
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
char p_and_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
char p_div[] = { C_STATIC | C_LOCAL, T_MIXED, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->div()
 * DESCRIPTION:	mixed / mixed
 */
int kf_div()
{
    xfloat f1, f2;

    if (sp[1].type != sp->type) {
	return 2;
    }
    switch (sp->type) {
    case T_INT:
	if (sp->u.number == 0) {
	    error("Division by zero");
	}
	sp[1].u.number /= sp->u.number;
	sp++;
	return 0;

    case T_FLOAT:
	VFLT_GET(sp, f2);
	sp++;
	VFLT_GET(sp, f1);
	flt_div(&f1, &f2);
	VFLT_PUT(sp, f1);
	return 0;

    default:
	return 1;
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div_int, p_div_int)
# else
char p_div_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

/*
 * NAME:	kfun->div()
 * DESCRIPTION:	int / int
 */
int kf_div_int()
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
FUNCDEF("==", kf_eq, p_eq)
# else
char p_eq[] = { C_STATIC | C_LOCAL, T_INT, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->eq()
 * DESCRIPTION:	value == value
 */
int kf_eq()
{
    register bool flag;
    xfloat f1, f2;

    if (sp[1].type != sp->type) {
	i_pop(2);
	(--sp)->type = T_INT;
	sp->u.number = FALSE;
	return 0;
    }

    switch (sp->type) {
    case T_INT:
	sp[1].u.number = (sp[1].u.number == sp->u.number);
	sp++;
	break;

    case T_FLOAT:
	VFLT_GET(sp, f2);
	sp++;
	VFLT_GET(sp, f1);
	sp->type = T_INT;
	sp->u.number = (flt_cmp(&f1, &f2) == 0);
	break;

    case T_STRING:
	flag = (str_cmp(sp[1].u.string, sp->u.string) == 0);
	str_del(sp->u.string);
	sp++;
	str_del(sp->u.string);
	sp->type = T_INT;
	sp->u.number = flag;
	break;

    case T_OBJECT:
	sp[1].type = T_INT;
	sp[1].u.number = (sp[1].oindex == sp->oindex);
	sp++;
	break;

    case T_ARRAY:
    case T_MAPPING:
	flag = (sp[1].u.array == sp->u.array);
	arr_del(sp->u.array);
	sp++;
	arr_del(sp->u.array);
	sp->type = T_INT;
	sp->u.number = flag;
	break;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq_int, p_eq_int)
# else
char p_eq_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

/*
 * NAME:	kfun->eq_int()
 * DESCRIPTION:	int == int
 */
int kf_eq_int()
{
    sp[1].u.number = (sp[1].u.number == sp->u.number);
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge, p_ge)
# else
char p_ge[] = { C_STATIC | C_LOCAL, T_INT, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ge()
 * DESCRIPTION:	value >= value
 */
int kf_ge()
{
    xfloat f1, f2;
    bool flag;

    if (sp[1].type != sp->type) {
	return 2;
    }
    switch (sp->type) {
    case T_INT:
	sp[1].u.number = (sp[1].u.number >= sp->u.number);
	sp++;
	return 0;

    case T_FLOAT:
	VFLT_GET(sp, f2);
	sp++;
	VFLT_GET(sp, f1);
	sp->type = T_INT;
	sp->u.number = (flt_cmp(&f1, &f2) >= 0);
	return 0;

    case T_STRING:
	flag = (str_cmp(sp[1].u.string, sp->u.string) >= 0);
	str_del(sp->u.string);
	sp++;
	str_del(sp->u.string);
	sp->type = T_INT;
	sp->u.number = flag;
	return 0;

    default:
	return 1;
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_int, p_ge_int)
# else
char p_ge_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
char p_gt[] = { C_STATIC | C_LOCAL, T_INT, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->gt()
 * DESCRIPTION:	value > value
 */
int kf_gt()
{
    xfloat f1, f2;
    bool flag;

    if (sp[1].type != sp->type) {
	return 2;
    }
    switch (sp->type) {
    case T_INT:
	sp[1].u.number = (sp[1].u.number > sp->u.number);
	sp++;
	return 0;

    case T_FLOAT:
	VFLT_GET(sp, f2);
	sp++;
	VFLT_GET(sp, f1);
	sp->type = T_INT;
	sp->u.number = (flt_cmp(&f1, &f2) > 0);
	return 0;

    case T_STRING:
	flag = (str_cmp(sp[1].u.string, sp->u.string) > 0);
	str_del(sp->u.string);
	sp++;
	str_del(sp->u.string);
	sp->type = T_INT;
	sp->u.number = flag;
	return 0;

    default:
	return 1;
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_int, p_gt_int)
# else
char p_gt_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
char p_le[] = { C_STATIC | C_LOCAL, T_INT, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->le()
 * DESCRIPTION:	value <= value
 */
int kf_le()
{
    xfloat f1, f2;
    bool flag;

    if (sp[1].type != sp->type) {
	return 2;
    }
    switch (sp->type) {
    case T_INT:
	sp[1].u.number = (sp[1].u.number <= sp->u.number);
	sp++;
	return 0;

    case T_FLOAT:
	VFLT_GET(sp, f2);
	sp++;
	VFLT_GET(sp, f1);
	sp->type = T_INT;
	sp->u.number = (flt_cmp(&f1, &f2) <= 0);
	return 0;

    case T_STRING:
	flag = (str_cmp(sp[1].u.string, sp->u.string) <= 0);
	str_del(sp->u.string);
	sp++;
	str_del(sp->u.string);
	sp->type = T_INT;
	sp->u.number = flag;
	return 0;

    default:
	return 1;
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_int, p_le_int)
# else
char p_le_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

/*
 * NAME:	kfun->le_int()
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
char p_lshift[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 2,
		    T_INT, T_INT };

/*
 * NAME:	kfun->lshift()
 * DESCRIPTION:	int << int
 */
int kf_lshift()
{
    sp[1].u.number  = (Uint) sp[1].u.number << sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<<", kf_lshift, p_lshift_int)
# else
char p_lshift_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt, p_lt)
# else
char p_lt[] = { C_STATIC | C_LOCAL, T_INT, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->lt()
 * DESCRIPTION:	value < value
 */
int kf_lt()
{
    xfloat f1, f2;
    bool flag;

    if (sp[1].type != sp->type) {
	return 2;
    }
    switch (sp->type) {
    case T_INT:
	sp[1].u.number = (sp[1].u.number < sp->u.number);
	sp++;
	return 0;

    case T_FLOAT:
	VFLT_GET(sp, f2);
	sp++;
	VFLT_GET(sp, f1);
	sp->type = T_INT;
	sp->u.number = (flt_cmp(&f1, &f2) < 0);
	return 0;

    case T_STRING:
	flag = (str_cmp(sp[1].u.string, sp->u.string) < 0);
	str_del(sp->u.string);
	sp++;
	str_del(sp->u.string);
	sp->type = T_INT;
	sp->u.number = flag;
	return 0;

    default:
	return 1;
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_int, p_lt_int)
# else
char p_lt_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
char p_mod[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
char p_mod_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult, p_mult)
# else
char p_mult[] = { C_STATIC | C_LOCAL, T_MIXED, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->mult()
 * DESCRIPTION:	mixed * mixed
 */
int kf_mult()
{
    xfloat f1, f2;

    if (sp[1].type != sp->type) {
	return 2;
    }
    switch (sp->type) {
    case T_INT:
	sp[1].u.number *= sp->u.number;
	sp++;
	return 0;

    case T_FLOAT:
	VFLT_GET(sp, f2);
	sp++;
	VFLT_GET(sp, f1);
	flt_mult(&f1, &f2);
	VFLT_PUT(sp, f1);
	return 0;

    default:
	return 1;
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult_int, p_mult_int)
# else
char p_mult_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

/*
 * NAME:	kfun->mult_int()
 * DESCRIPTION:	int * int
 */
int kf_mult_int()
{
    sp[1].u.number *= sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne, p_ne)
# else
char p_ne[] = { C_STATIC | C_LOCAL, T_INT, 2, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ne()
 * DESCRIPTION:	value != value
 */
int kf_ne()
{
    register bool flag;
    xfloat f1, f2;

    if (sp[1].type != sp->type) {
	i_pop(2);
	(--sp)->type = T_INT;
	sp->u.number = TRUE;
	return 0;
    }

    switch (sp->type) {
    case T_INT:
	sp[1].u.number = (sp[1].u.number != sp->u.number);
	sp++;
	break;

    case T_FLOAT:
	VFLT_GET(sp, f2);
	sp++;
	VFLT_GET(sp, f1);
	sp->type = T_INT;
	sp->u.number = (flt_cmp(&f1, &f2) != 0);
	break;

    case T_STRING:
	flag = (str_cmp(sp[1].u.string, sp->u.string) != 0);
	str_del(sp->u.string);
	sp++;
	str_del(sp->u.string);
	sp->type = T_INT;
	sp->u.number = flag;
	break;

    case T_OBJECT:
	sp[1].type = T_INT;
	sp[1].u.number = (sp[1].oindex != sp->oindex);
	sp++;
	break;

    case T_ARRAY:
    case T_MAPPING:
	flag = (sp[1].u.array != sp->u.array);
	arr_del(sp->u.array);
	sp++;
	arr_del(sp->u.array);
	sp->type = T_INT;
	sp->u.number = flag;
	break;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne_int, p_ne_int)
# else
char p_ne_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
char p_neg[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 1, T_INT };

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
char p_neg_int[] = { C_STATIC | C_LOCAL, T_INT, 1, T_INT };
# endif


# ifdef FUNCDEF
FUNCDEF("!", kf_not, p_not)
# else
char p_not[] = { C_STATIC | C_LOCAL, T_INT, 1, T_MIXED };

/*
 * NAME:	kfun->not()
 * DESCRIPTION:	! value
 */
int kf_not()
{
    switch (sp->type) {
    case T_INT:
	sp->u.number = !sp->u.number;
	return 0;

    case T_FLOAT:
	sp->type = T_INT;
	sp->u.number = VFLT_ISZERO(sp);
	return 0;

    case T_STRING:
	str_del(sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_del(sp->u.array);
	break;
    }

    sp->type = T_INT;
    sp->u.number = FALSE;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!", kf_notf, p_not)
# else
/*
 * NAME:	kfun->notf()
 * DESCRIPTION:	! fvalue
 */
int kf_notf()
{
    switch (sp->type) {
    case T_FLOAT:
	sp->type = T_INT;
	sp->u.number = VFLT_ISZERO(sp);
	return 0;

    case T_STRING:
	str_del(sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_del(sp->u.array);
	break;
    }

    sp->type = T_INT;
    sp->u.number = FALSE;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!", kf_noti, p_not)
# else
/*
 * NAME:	kfun->noti()
 * DESCRIPTION:	! ivalue
 */
int kf_noti()
{
    switch (sp->type) {
    case T_INT:
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

    sp->type = T_INT;
    sp->u.number = FALSE;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or, p_or)
# else
char p_or[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
char p_or_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_rangeft, p_rangeft)
# else
char p_rangeft[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_MIXED, 3,
		     T_MIXED, T_INT, T_INT };
/*
 * NAME:	kfun->rangeft()
 * DESCRIPTION:	value [ int .. int ]
 */
int kf_rangeft()
{
    string *str;
    array *a;

    switch (sp[2].type) {
    case T_STRING:
	str = str_range(sp[2].u.string, (long) sp[1].u.number,
			(long) sp->u.number);
	sp += 2;
	str_del(sp->u.string);
	str_ref(sp->u.string = str);
	break;

    case T_ARRAY:
	a = arr_range(sp[2].u.array, (long) sp[1].u.number,
		      (long) sp->u.number);
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
FUNCDEF("[]", kf_rangef, p_rangef)
# else
char p_rangef[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_MIXED, 2,
		    T_MIXED, T_INT };

/*
 * NAME:	kfun->rangef()
 * DESCRIPTION:	value [ int .. ]
 */
int kf_rangef()
{
    string *str;
    array *a;

    switch (sp[1].type) {
    case T_STRING:
	str = str_range(sp[1].u.string, (long) sp->u.number,
			sp[1].u.string->len - 1L);
	sp++;
	str_del(sp->u.string);
	str_ref(sp->u.string = str);
	break;

    case T_ARRAY:
	a = arr_range(sp[1].u.array, (long) sp->u.number,
		      sp[1].u.array->size - 1L);
	sp++;
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
FUNCDEF("[]", kf_ranget, p_ranget)
# else
char p_ranget[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_MIXED, 2,
		    T_MIXED, T_INT };

/*
 * NAME:	kfun->ranget()
 * DESCRIPTION:	value [ .. int ]
 */
int kf_ranget()
{
    string *str;
    array *a;

    switch (sp[1].type) {
    case T_STRING:
	str = str_range(sp[1].u.string, 0L, (long) sp->u.number);
	sp++;
	str_del(sp->u.string);
	str_ref(sp->u.string = str);
	break;

    case T_ARRAY:
	a = arr_range(sp[1].u.array, 0L, (long) sp->u.number);
	sp++;
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
FUNCDEF("[]", kf_range, p_range)
# else
char p_range[] = { C_STATIC | C_LOCAL, T_MIXED, 1, T_MIXED };

/*
 * NAME:	kfun->range()
 * DESCRIPTION:	value [ .. ]
 */
int kf_range()
{
    string *str;
    array *a;

    switch (sp->type) {
    case T_STRING:
	str = str_range(sp->u.string, 0L, sp->u.string->len - 1L);
	str_del(sp->u.string);
	str_ref(sp->u.string = str);
	break;

    case T_ARRAY:
	a = arr_range(sp->u.array, 0L, sp->u.array->size - 1L);
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
char p_rshift[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 2,
		    T_INT, T_INT };

/*
 * NAME:	kfun->rshift()
 * DESCRIPTION:	int >> int
 */
int kf_rshift()
{
    sp[1].u.number = (Uint) sp[1].u.number >> sp->u.number;
    sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">>", kf_rshift, p_rshift_int)
# else
char p_rshift_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };
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
    xfloat f1, f2;

    switch (sp[1].type) {
    case T_INT:
	if (sp->type == T_INT) {
	    sp[1].u.number -= sp->u.number;
	    sp++;
	    return 0;
	}
	break;

    case T_FLOAT:
	if (sp->type == T_FLOAT) {
	    VFLT_GET(sp, f2);
	    sp++;
	    VFLT_GET(sp, f1);
	    flt_sub(&f1, &f2);
	    VFLT_PUT(sp, f1);
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

    case T_MAPPING:
	if (sp->type == T_ARRAY) {
	    array *a;

	    a = map_sub(sp[1].u.array, sp->u.array);
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
char p_sub_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
FUNCDEF("--", kf_sub1, p_sub1)
# else
char p_sub1[] = { C_STATIC | C_LOCAL, T_MIXED, 1, T_MIXED };

/*
 * NAME:	kfun->sub1()
 * DESCRIPTION:	value--
 */
int kf_sub1()
{
    xfloat f1, f2;

    if (sp->type == T_INT) {
	sp->u.number--;
    } else if (sp->type == T_FLOAT) {
	VFLT_GET(sp, f1);
	FLT_ONE(f2.high, f2.low);
	flt_sub(&f1, &f2);
	VFLT_PUT(sp, f1);
    } else {
	return 1;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1_int, p_sub1_int)
# else
char p_sub1_int[] = { C_STATIC | C_LOCAL, T_INT, 1, T_INT };

/*
 * NAME:	kfun->sub1_int()
 * DESCRIPTION:	int--
 */
int kf_sub1_int()
{
    sp->u.number--;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(float)", kf_tofloat, p_tofloat)
# else
char p_tofloat[] = { C_STATIC | C_LOCAL, T_FLOAT, 1, T_MIXED };

/*
 * NAME:	kfun->tofloat()
 * DESCRIPTION:	convert to float
 */
int kf_tofloat()
{
    xfloat flt;

    if (sp->type == T_INT) {
	flt_itof(sp->u.number, &flt);
	sp->type = T_FLOAT;
	VFLT_PUT(sp, flt);
    } else if (sp->type != T_FLOAT) {
	error("Value is not a float");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(int)", kf_toint, p_toint)
# else
char p_toint[] = { C_STATIC | C_LOCAL, T_INT, 1, T_MIXED };

/*
 * NAME:	kfun->toint()
 * DESCRIPTION:	convert to integer
 */
int kf_toint()
{
    xfloat flt;

    if (sp->type == T_FLOAT) {
	VFLT_GET(sp, flt);
	sp->u.number = flt_ftoi(&flt);
	sp->type = T_INT;
    } else if (sp->type != T_INT) {
	error("Value is not an int");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst, p_tst)
# else
char p_tst[] = { C_STATIC | C_LOCAL, T_INT, 1, T_MIXED };

/*
 * NAME:	kfun->tst()
 * DESCRIPTION:	!! value
 */
int kf_tst()
{
    switch (sp->type) {
    case T_INT:
	sp->u.number = (sp->u.number != 0);
	return 0;

    case T_FLOAT:
	sp->type = T_INT;
	sp->u.number = !VFLT_ISZERO(sp);
	return 0;

    case T_STRING:
	str_del(sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_del(sp->u.array);
	break;
    }

    sp->type = T_INT;
    sp->u.number = TRUE;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tstf, p_tst)
# else
/*
 * NAME:	kfun->tstf()
 * DESCRIPTION:	!! fvalue
 */
int kf_tstf()
{
    switch (sp->type) {
    case T_FLOAT:
	sp->type = T_INT;
	sp->u.number = !VFLT_ISZERO(sp);
	return 0;

    case T_STRING:
	str_del(sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_del(sp->u.array);
	break;
    }

    sp->type = T_INT;
    sp->u.number = TRUE;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tsti, p_tst)
# else
/*
 * NAME:	kfun->tsti()
 * DESCRIPTION:	!! ivalue
 */
int kf_tsti()
{
    switch (sp->type) {
    case T_INT:
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

    sp->type = T_INT;
    sp->u.number = TRUE;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("unary -", kf_umin, p_umin)
# else
char p_umin[] = { C_STATIC | C_LOCAL, T_MIXED, 1, T_MIXED };

/*
 * NAME:	kfun->umin()
 * DESCRIPTION:	- mixed
 */
int kf_umin()
{
    switch (sp->type) {
    case T_INT:
	sp->u.number = -sp->u.number;
	return 0;

    case T_FLOAT:
	if (!VFLT_ISZERO(sp)) {
	    VFLT_NEG(sp);
	}
	return 0;
    }

    return 1;
}
# endif


# ifdef FUNCDEF
FUNCDEF("unary -", kf_umin_int, p_umin_int)
# else
char p_umin_int[] = { C_STATIC | C_LOCAL, T_INT, 1, T_INT };

/*
 * NAME:	kfun->umin_int()
 * DESCRIPTION:	- int
 */
int kf_umin_int()
{
    sp->u.number = -sp->u.number;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor, p_xor)
# else
char p_xor[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };

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
char p_xor_int[] = { C_STATIC | C_LOCAL, T_INT, 2, T_INT, T_INT };
# endif
