# ifndef FUNCDEF
# define INCLUDE_CTYPE
# include "kfun.h"
# include "table.h"

/*
 * NAME:	kfun->argerror()
 * DESCRIPTION:	handle an argument error in a builtin kfun
 */
static void kf_argerror(kfun, n)
int kfun, n;
{
    error("Bad argument %d for kfun %s", n, kftab[kfun].name);
}

/*
 * NAME:	kfun->itoa()
 * DESCRIPTION:	convert an Int to a string
 */
static char *kf_itoa(i, buffer)
Int i;
char *buffer;
{
    register Uint u;
    register char *p;

    u = (i >= 0) ? i : -i;

    p = buffer + 11;
    *p = '\0';
    do {
	*--p = '0' + u % 10;
	u /= 10;
    } while (u != 0);
    if (i < 0) {
	*--p = '-';
    }

    return p;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add, pt_add)
# else
char pt_add[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->add()
 * DESCRIPTION:	value + value
 */
int kf_add(f)
register frame *f;
{
    register string *str;
    register array *a;
    char *num, buffer[18];
    xfloat f1, f2;
    long l;

    switch (f->sp[1].type) {
    case T_INT:
	switch (f->sp->type) {
	case T_INT:
	    PUT_INT(&f->sp[1], f->sp[1].u.number + f->sp->u.number);
	    f->sp++;
	    return 0;

	case T_STRING:
	    i_add_ticks(f, 2);
	    num = kf_itoa(f->sp[1].u.number, buffer);
	    str = str_new((char *) NULL,
			  (l=(long) strlen(num)) + f->sp->u.string->len);
	    strcpy(str->text, num);
	    memcpy(str->text + l, f->sp->u.string->text, f->sp->u.string->len);
	    str_del(f->sp->u.string);
	    f->sp++;
	    PUT_STRVAL(f->sp, str);
	    return 0;
	}
	break;

    case T_FLOAT:
	i_add_ticks(f, 1);
	switch (f->sp->type) {
	case T_FLOAT:
	    GET_FLT(f->sp, f2);
	    f->sp++;
	    GET_FLT(f->sp, f1);
	    flt_add(&f1, &f2);
	    PUT_FLT(f->sp, f1);
	    return 0;

	case T_STRING:
	    i_add_ticks(f, 2);
	    GET_FLT(&f->sp[1], f1);
	    flt_ftoa(&f1, buffer);
	    str = str_new((char *) NULL,
			  (l=(long) strlen(buffer)) + f->sp->u.string->len);
	    strcpy(str->text, buffer);
	    memcpy(str->text + l, f->sp->u.string->text, f->sp->u.string->len);
	    str_del(f->sp->u.string);
	    f->sp++;
	    PUT_STRVAL(f->sp, str);
	    return 0;
	}
	break;

    case T_STRING:
	i_add_ticks(f, 2);
	switch (f->sp->type) {
	case T_INT:
	    num = kf_itoa(f->sp->u.number, buffer);
	    f->sp++;
	    str = str_new((char *) NULL,
			  f->sp->u.string->len + (long) strlen(num));
	    memcpy(str->text, f->sp->u.string->text, f->sp->u.string->len);
	    strcpy(str->text + f->sp->u.string->len, num);
	    str_del(f->sp->u.string);
	    PUT_STR(f->sp, str);
	    return 0;

	case T_FLOAT:
	    i_add_ticks(f, 1);
	    GET_FLT(f->sp, f2);
	    flt_ftoa(&f2, buffer);
	    f->sp++;
	    str = str_new((char *) NULL,
			  f->sp->u.string->len + (long) strlen(buffer));
	    memcpy(str->text, f->sp->u.string->text, f->sp->u.string->len);
	    strcpy(str->text + f->sp->u.string->len, buffer);
	    str_del(f->sp->u.string);
	    PUT_STR(f->sp, str);
	    return 0;

	case T_STRING:
	    str = str_add(f->sp[1].u.string, f->sp->u.string);
	    str_del(f->sp->u.string);
	    f->sp++;
	    str_del(f->sp->u.string);
	    PUT_STR(f->sp, str);
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    i_add_ticks(f, (Int) f->sp[1].u.array->size + f->sp->u.array->size);
	    a = arr_add(f->data, f->sp[1].u.array, f->sp->u.array);
	    arr_del(f->sp->u.array);
	    f->sp++;
	    arr_del(f->sp->u.array);
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    case T_MAPPING:
	if (f->sp->type == T_MAPPING) {
	    i_add_ticks(f, (Int) f->sp[1].u.array->size + f->sp->u.array->size);
	    a = map_add(f->data, f->sp[1].u.array, f->sp->u.array);
	    arr_del(f->sp->u.array);
	    f->sp++;
	    arr_del(f->sp->u.array);
	    PUT_MAP(f->sp, a);
	    return 0;
	}
	break;

    default:
	kf_argerror(KF_ADD, 1);
    }

    kf_argerror(KF_ADD, 2);
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_int, pt_add_int)
# else
char pt_add_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->add_int()
 * DESCRIPTION:	int + int
 */
int kf_add_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], f->sp[1].u.number + f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1, pt_add1)
# else
char pt_add1[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->add1()
 * DESCRIPTION:	value++
 */
int kf_add1(f)
register frame *f;
{
    xfloat f1, f2;

    if (f->sp->type == T_INT) {
	PUT_INT(f->sp, f->sp->u.number + 1);
    } else if (f->sp->type == T_FLOAT) {
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f1);
	FLT_ONE(f2.high, f2.low);
	flt_add(&f1, &f2);
	PUT_FLT(f->sp, f1);
    } else {
	kf_argerror(KF_ADD1, 1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1_int, pt_add1_int)
# else
char pt_add1_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->add1_int()
 * DESCRIPTION:	int++
 */
int kf_add1_int(f)
frame *f;
{
    PUT_INT(f->sp, f->sp->u.number + 1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("&", kf_and, pt_and)
# else
char pt_and[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->and()
 * DESCRIPTION:	value & value
 */
int kf_and(f)
register frame *f;
{
    array *a;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type == T_INT) {
	    PUT_INT(&f->sp[1], f->sp[1].u.number & f->sp->u.number);
	    f->sp++;
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    i_add_ticks(f, (Int) f->sp[1].u.array->size + f->sp->u.array->size);
	    a = arr_intersect(f->data, f->sp[1].u.array, f->sp->u.array);
	    arr_del(f->sp->u.array);
	    f->sp++;
	    arr_del(f->sp->u.array);
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    case T_MAPPING:
	if (f->sp->type == T_ARRAY) {
	    i_add_ticks(f, (Int) f->sp[1].u.array->size + f->sp->u.array->size);
	    a = map_intersect(f->data, f->sp[1].u.array, f->sp->u.array);
	    arr_del(f->sp->u.array);
	    f->sp++;
	    PUT_MAP(f->sp, a);
	    return 0;
	}
	break;

    default:
	kf_argerror(KF_AND, 1);
    }

    kf_argerror(KF_AND, 2);
}
# endif


# ifdef FUNCDEF
FUNCDEF("&", kf_and_int, pt_and_int)
# else
char pt_and_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->and_int()
 * DESCRIPTION:	int & int
 */
int kf_and_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], f->sp[1].u.number & f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div, pt_div)
# else
char pt_div[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->div()
 * DESCRIPTION:	mixed / mixed
 */
int kf_div(f)
register frame *f;
{
    register Int i, d;
    xfloat f1, f2;

    if (f->sp[1].type != f->sp->type) {
	kf_argerror(KF_DIV, 2);
    }
    switch (f->sp->type) {
    case T_INT:
	i = f->sp[1].u.number;
	d = f->sp->u.number;
	if (d == 0) {
	    error("Division by zero");
	}
	if ((i | d) < 0) {
	    Int r;

	    r = ((Uint) ((i < 0) ? -i : i)) / ((Uint) ((d < 0) ? -d : d));
	    PUT_INT(&f->sp[1], ((i ^ d) < 0) ? -r : r);
	} else {
	    PUT_INT(&f->sp[1], ((Uint) i) / ((Uint) d));
	}
	f->sp++;
	return 0;

    case T_FLOAT:
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	flt_div(&f1, &f2);
	PUT_FLT(f->sp, f1);
	return 0;

    default:
	kf_argerror(KF_DIV, 1);
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div_int, pt_div_int)
# else
char pt_div_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->div()
 * DESCRIPTION:	int / int
 */
int kf_div_int(f)
register frame *f;
{
    register Int i, d;

    i = f->sp[1].u.number;
    d = f->sp->u.number;
    if (d == 0) {
	error("Division by zero");
    }
    if ((i | d) < 0) {
	Int r;

	r = ((Uint) ((i < 0) ? -i : i)) / ((Uint) ((d < 0) ? -d : d));
	PUT_INT(&f->sp[1], ((i ^ d) < 0) ? -r : r);
    } else {
	PUT_INT(&f->sp[1], ((Uint) i) / ((Uint) d));
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq, pt_eq)
# else
char pt_eq[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->eq()
 * DESCRIPTION:	value == value
 */
int kf_eq(f)
register frame *f;
{
    register bool flag;
    xfloat f1, f2;

    if (f->sp[1].type != f->sp->type) {
	i_pop(f, 2);
	PUSH_INTVAL(f, FALSE);
	return 0;
    }

    switch (f->sp->type) {
    case T_NIL:
	f->sp++;
	PUT_INTVAL(f->sp, TRUE);
	break;

    case T_INT:
	PUT_INT(&f->sp[1], (f->sp[1].u.number == f->sp->u.number));
	f->sp++;
	break;

    case T_FLOAT:
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) == 0));
	break;

    case T_STRING:
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) == 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	break;

    case T_OBJECT:
	PUT_INTVAL(&f->sp[1], (f->sp[1].oindex == f->sp->oindex));
	f->sp++;
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	flag = (f->sp[1].u.array == f->sp->u.array);
	arr_del(f->sp->u.array);
	f->sp++;
	arr_del(f->sp->u.array);
	PUT_INTVAL(f->sp, flag);
	break;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq_int, pt_eq_int)
# else
char pt_eq_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->eq_int()
 * DESCRIPTION:	int == int
 */
int kf_eq_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number == f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge, pt_ge)
# else
char pt_ge[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ge()
 * DESCRIPTION:	value >= value
 */
int kf_ge(f)
register frame *f;
{
    xfloat f1, f2;
    bool flag;

    if (f->sp[1].type != f->sp->type) {
	kf_argerror(KF_GE, 2);
    }
    switch (f->sp->type) {
    case T_INT:
	PUT_INT(&f->sp[1], (f->sp[1].u.number >= f->sp->u.number));
	f->sp++;
	return 0;

    case T_FLOAT:
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) >= 0));
	return 0;

    case T_STRING:
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) >= 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	return 0;

    default:
	kf_argerror(KF_GE, 1);
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_int, pt_ge_int)
# else
char pt_ge_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->ge_int()
 * DESCRIPTION:	int >= int
 */
int kf_ge_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number >= f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt, pt_gt)
# else
char pt_gt[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->gt()
 * DESCRIPTION:	value > value
 */
int kf_gt(f)
register frame *f;
{
    xfloat f1, f2;
    bool flag;

    if (f->sp[1].type != f->sp->type) {
	kf_argerror(KF_GT, 2);
    }
    switch (f->sp->type) {
    case T_INT:
	PUT_INT(&f->sp[1], (f->sp[1].u.number > f->sp->u.number));
	f->sp++;
	return 0;

    case T_FLOAT:
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) > 0));
	return 0;

    case T_STRING:
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) > 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	return 0;

    default:
	kf_argerror(KF_GT, 1);
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_int, pt_gt_int)
# else
char pt_gt_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->gt_int()
 * DESCRIPTION:	int > int
 */
int kf_gt_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number > f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le, pt_le)
# else
char pt_le[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->le()
 * DESCRIPTION:	value <= value
 */
int kf_le(f)
register frame *f;
{
    xfloat f1, f2;
    bool flag;

    if (f->sp[1].type != f->sp->type) {
	kf_argerror(KF_LE, 2);
    }
    switch (f->sp->type) {
    case T_INT:
	PUT_INT(&f->sp[1], (f->sp[1].u.number <= f->sp->u.number));
	f->sp++;
	return 0;

    case T_FLOAT:
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) <= 0));
	return 0;

    case T_STRING:
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) <= 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	return 0;

    default:
	kf_argerror(KF_LE, 1);
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_int, pt_le_int)
# else
char pt_le_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->le_int()
 * DESCRIPTION:	int <= int
 */
int kf_le_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number <= f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<<", kf_lshift, pt_lshift)
# else
char pt_lshift[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->lshift()
 * DESCRIPTION:	int << int
 */
int kf_lshift(f)
register frame *f;
{
    if (f->sp[1].type != T_INT) {
	kf_argerror(KF_LSHIFT, 1);
    }
    if (f->sp->type != T_INT) {
	kf_argerror(KF_LSHIFT, 2);
    }
    if ((f->sp->u.number & ~31) != 0) {
	if (f->sp->u.number < 0) {
	    error("Negative left shift");
	}
	PUT_INT(&f->sp[1], 0);
    } else {
	PUT_INT(&f->sp[1], (Uint) f->sp[1].u.number << f->sp->u.number);
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<<", kf_lshift_int, pt_lshift_int)
# else
char pt_lshift_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->lshift_int()
 * DESCRIPTION:	int << int
 */
int kf_lshift_int(f)
register frame *f;
{
    if ((f->sp->u.number & ~31) != 0) {
	if (f->sp->u.number < 0) {
	    error("Negative left shift");
	}
	PUT_INT(&f->sp[1], 0);
    } else {
	PUT_INT(&f->sp[1], (Uint) f->sp[1].u.number << f->sp->u.number);
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt, pt_lt)
# else
char pt_lt[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->lt()
 * DESCRIPTION:	value < value
 */
int kf_lt(f)
register frame *f;
{
    xfloat f1, f2;
    bool flag;

    if (f->sp[1].type != f->sp->type) {
	kf_argerror(KF_LT, 2);
    }
    switch (f->sp->type) {
    case T_INT:
	PUT_INT(&f->sp[1], (f->sp[1].u.number < f->sp->u.number));
	f->sp++;
	return 0;

    case T_FLOAT:
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) < 0));
	return 0;

    case T_STRING:
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) < 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	return 0;

    default:
	kf_argerror(KF_LT, 1);
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_int, pt_lt_int)
# else
char pt_lt_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->lt_int()
 * DESCRIPTION:	int < int
 */
int kf_lt_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number < f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("%", kf_mod, pt_mod)
# else
char pt_mod[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->mod()
 * DESCRIPTION:	int % int
 */
int kf_mod(f)
register frame *f;
{
    register Int i, d;

    if (f->sp[1].type != T_INT) {
	kf_argerror(KF_MOD, 1);
    }
    if (f->sp->type != T_INT) {
	kf_argerror(KF_MOD, 2);
    }
    i = f->sp[1].u.number;
    d = f->sp->u.number;
    if (d == 0) {
	error("Modulus by zero");
    }
    if (d < 0) {
	d = -d;
    }
    if (i < 0) {
	PUT_INT(&f->sp[1], - (Int) (((Uint) -i) % ((Uint) d)));
    } else {
	PUT_INT(&f->sp[1], ((Uint) i) % ((Uint) d));
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("%", kf_mod_int, pt_mod_int)
# else
char pt_mod_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->mod_int()
 * DESCRIPTION:	int % int
 */
int kf_mod_int(f)
register frame *f;
{
    register Int i, d;

    i = f->sp[1].u.number;
    d = f->sp->u.number;
    if (d == 0) {
	error("Modulus by zero");
    }
    if (d < 0) {
	d = -d;
    }
    if (i < 0) {
	PUT_INT(&f->sp[1], - (Int) (((Uint) -i) % ((Uint) d)));
    } else {
	PUT_INT(&f->sp[1], ((Uint) i) % ((Uint) d));
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult, pt_mult)
# else
char pt_mult[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->mult()
 * DESCRIPTION:	mixed * mixed
 */
int kf_mult(f)
register frame *f;
{
    xfloat f1, f2;

    if (f->sp[1].type != f->sp->type) {
	kf_argerror(KF_MULT, 2);
    }
    switch (f->sp->type) {
    case T_INT:
	PUT_INT(&f->sp[1], f->sp[1].u.number * f->sp->u.number);
	f->sp++;
	return 0;

    case T_FLOAT:
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	flt_mult(&f1, &f2);
	PUT_FLT(f->sp, f1);
	return 0;

    default:
	kf_argerror(KF_MULT, 1);
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult_int, pt_mult_int)
# else
char pt_mult_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->mult_int()
 * DESCRIPTION:	int * int
 */
int kf_mult_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], f->sp[1].u.number * f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne, pt_ne)
# else
char pt_ne[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ne()
 * DESCRIPTION:	value != value
 */
int kf_ne(f)
register frame *f;
{
    register bool flag;
    xfloat f1, f2;

    if (f->sp[1].type != f->sp->type) {
	i_pop(f, 2);
	PUSH_INTVAL(f, TRUE);
	return 0;
    }

    switch (f->sp->type) {
    case T_NIL:
	f->sp++;
	PUT_INTVAL(f->sp, FALSE);
	break;

    case T_INT:
	PUT_INT(&f->sp[1], (f->sp[1].u.number != f->sp->u.number));
	f->sp++;
	break;

    case T_FLOAT:
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) != 0));
	break;

    case T_STRING:
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) != 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	break;

    case T_OBJECT:
	PUT_INTVAL(&f->sp[1], (f->sp[1].oindex != f->sp->oindex));
	f->sp++;
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	flag = (f->sp[1].u.array != f->sp->u.array);
	arr_del(f->sp->u.array);
	f->sp++;
	arr_del(f->sp->u.array);
	PUT_INTVAL(f->sp, flag);
	break;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne_int, pt_ne_int)
# else
char pt_ne_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->ne_int()
 * DESCRIPTION:	int != int
 */
int kf_ne_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number != f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("~", kf_neg, pt_neg)
# else
char pt_neg[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->neg()
 * DESCRIPTION:	~ int
 */
int kf_neg(f)
register frame *f;
{
    if (f->sp->type != T_INT) {
	kf_argerror(KF_NEG, 1);
    }
    PUT_INT(f->sp, ~f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("~", kf_neg_int, pt_neg_int)
# else
char pt_neg_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->neg_int()
 * DESCRIPTION:	~ int
 */
int kf_neg_int(f)
frame *f;
{
    PUT_INT(f->sp, ~f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!", kf_not, pt_not)
# else
char pt_not[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * NAME:	kfun->not()
 * DESCRIPTION:	! value
 */
int kf_not(f)
register frame *f;
{
    switch (f->sp->type) {
    case T_NIL:
	PUT_INTVAL(f->sp, TRUE);
	return 0;

    case T_INT:
	PUT_INT(f->sp, !f->sp->u.number);
	return 0;

    case T_FLOAT:
	PUT_INTVAL(f->sp, VFLT_ISZERO(f->sp));
	return 0;

    case T_STRING:
	str_del(f->sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr_del(f->sp->u.array);
	break;
    }

    PUT_INTVAL(f->sp, FALSE);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!", kf_not_int, pt_not)
# else
/*
 * NAME:	kfun->not_int()
 * DESCRIPTION:	! int
 */
int kf_not_int(f)
frame *f;
{
    PUT_INT(f->sp, !f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or, pt_or)
# else
char pt_or[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->or()
 * DESCRIPTION:	value | value
 */
int kf_or(f)
register frame *f;
{
    array *a;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type == T_INT) {
	    PUT_INT(&f->sp[1], f->sp[1].u.number | f->sp->u.number);
	    f->sp++;
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    i_add_ticks(f, (Int) f->sp[1].u.array->size + f->sp->u.array->size);
	    a = arr_setadd(f->data, f->sp[1].u.array, f->sp->u.array);
	    arr_del(f->sp->u.array);
	    f->sp++;
	    arr_del(f->sp->u.array);
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    default:
	kf_argerror(KF_OR, 1);
    }

    kf_argerror(KF_OR, 2);
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or_int, pt_or_int)
# else
char pt_or_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->or_int()
 * DESCRIPTION:	int | int
 */
int kf_or_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], f->sp[1].u.number | f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_rangeft, pt_rangeft)
# else
char pt_rangeft[] = { C_STATIC, 3, 0, 0, 9, T_MIXED, T_MIXED, T_MIXED,
		      T_MIXED };
/*
 * NAME:	kfun->rangeft()
 * DESCRIPTION:	value [ int .. int ]
 */
int kf_rangeft(f)
register frame *f;
{
    string *str;
    array *a;

    if (f->sp[2].type == T_MAPPING) {
	a = map_range(f->data, f->sp[2].u.array, &f->sp[1], f->sp);
	i_del_value(f->sp++);
	i_del_value(f->sp++);
	i_add_ticks(f, f->sp->u.array->size);
	arr_del(f->sp->u.array);
	PUT_ARR(f->sp, a);

	return 0;
    }

    if (f->sp[1].type != T_INT) {
	kf_argerror(KF_RANGEFT, 2);
    }
    if (f->sp->type != T_INT) {
	kf_argerror(KF_RANGEFT, 3);
    }
    switch (f->sp[2].type) {
    case T_STRING:
	i_add_ticks(f, 2);
	str = str_range(f->sp[2].u.string, (long) f->sp[1].u.number,
			(long) f->sp->u.number);
	f->sp += 2;
	str_del(f->sp->u.string);
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	a = arr_range(f->data, f->sp[2].u.array, (long) f->sp[1].u.number,
		      (long) f->sp->u.number);
	i_add_ticks(f, a->size);
	f->sp += 2;
	arr_del(f->sp->u.array);
	PUT_ARR(f->sp, a);
	break;

    default:
	kf_argerror(KF_RANGEFT, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_rangef, pt_rangef)
# else
char pt_rangef[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->rangef()
 * DESCRIPTION:	value [ int .. ]
 */
int kf_rangef(f)
register frame *f;
{
    string *str;
    array *a;

    if (f->sp[1].type == T_MAPPING) {
	a = map_range(f->data, f->sp[1].u.array, f->sp, (value *) NULL);
	i_del_value(f->sp++);
	i_add_ticks(f, f->sp->u.array->size);
	arr_del(f->sp->u.array);
	PUT_MAP(f->sp, a);

	return 0;
    }

    if (f->sp->type != T_INT) {
	kf_argerror(KF_RANGEF, 2);
    }
    switch (f->sp[1].type) {
    case T_STRING:
	i_add_ticks(f, 2);
	str = str_range(f->sp[1].u.string, (long) f->sp->u.number,
			f->sp[1].u.string->len - 1L);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	a = arr_range(f->data, f->sp[1].u.array, (long) f->sp->u.number,
		      f->sp[1].u.array->size - 1L);
	i_add_ticks(f, a->size);
	f->sp++;
	arr_del(f->sp->u.array);
	PUT_ARR(f->sp, a);
	break;

    default:
	kf_argerror(KF_RANGEF, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_ranget, pt_ranget)
# else
char pt_ranget[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ranget()
 * DESCRIPTION:	value [ .. int ]
 */
int kf_ranget(f)
register frame *f;
{
    string *str;
    array *a;

    if (f->sp[1].type == T_MAPPING) {
	a = map_range(f->data, f->sp[1].u.array, (value *) NULL, f->sp);
	i_del_value(f->sp++);
	i_add_ticks(f, f->sp->u.array->size);
	arr_del(f->sp->u.array);
	PUT_MAP(f->sp, a);

	return 0;
    }

    if (f->sp->type != T_INT) {
	kf_argerror(KF_RANGET, 2);
    }
    switch (f->sp[1].type) {
    case T_STRING:
	i_add_ticks(f, 2);
	str = str_range(f->sp[1].u.string, 0L, (long) f->sp->u.number);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	a = arr_range(f->data, f->sp[1].u.array, 0L, (long) f->sp->u.number);
	i_add_ticks(f, a->size);
	f->sp++;
	arr_del(f->sp->u.array);
	PUT_ARR(f->sp, a);
	break;

    default:
	kf_argerror(KF_RANGET, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_range, pt_range)
# else
char pt_range[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->range()
 * DESCRIPTION:	value [ .. ]
 */
int kf_range(f)
register frame *f;
{
    string *str;
    array *a;

    if (f->sp->type == T_MAPPING) {
	a = map_range(f->data, f->sp->u.array, (value *) NULL, (value *) NULL);
	i_add_ticks(f, f->sp->u.array->size);
	arr_del(f->sp->u.array);
	PUT_MAP(f->sp, a);

	return 0;
    }

    switch (f->sp->type) {
    case T_STRING:
	i_add_ticks(f, 2);
	str = str_range(f->sp->u.string, 0L, f->sp->u.string->len - 1L);
	str_del(f->sp->u.string);
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	a = arr_range(f->data, f->sp->u.array, 0L, f->sp->u.array->size - 1L);
	i_add_ticks(f, a->size);
	arr_del(f->sp->u.array);
	PUT_ARR(f->sp, a);
	break;

    default:
	kf_argerror(KF_RANGE, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">>", kf_rshift, pt_rshift)
# else
char pt_rshift[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->rshift()
 * DESCRIPTION:	int >> int
 */
int kf_rshift(f)
register frame *f;
{
    if (f->sp[1].type != T_INT) {
	kf_argerror(KF_RSHIFT, 1);
    }
    if (f->sp->type != T_INT) {
	kf_argerror(KF_RSHIFT, 2);
    }
    if ((f->sp->u.number & ~31) != 0) {
	if (f->sp->u.number < 0) {
	    error("Negative right shift");
	}
	PUT_INT(&f->sp[1], 0);
    } else {
	PUT_INT(&f->sp[1], (Uint) f->sp[1].u.number >> f->sp->u.number);
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">>", kf_rshift_int, pt_rshift_int)
# else
char pt_rshift_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->rshift_int()
 * DESCRIPTION:	int >> int
 */
int kf_rshift_int(f)
register frame *f;
{
    if ((f->sp->u.number & ~31) != 0) {
	if (f->sp->u.number < 0) {
	    error("Negative right shift");
	}
	PUT_INT(&f->sp[1], 0);
    } else {
	PUT_INT(&f->sp[1], (Uint) f->sp[1].u.number >> f->sp->u.number);
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub, pt_sub)
# else
char pt_sub[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->sub()
 * DESCRIPTION:	value - value
 */
int kf_sub(f)
register frame *f;
{
    xfloat f1, f2;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type == T_INT) {
	    PUT_INT(&f->sp[1], f->sp[1].u.number - f->sp->u.number);
	    f->sp++;
	    return 0;
	}
	break;

    case T_FLOAT:
	if (f->sp->type == T_FLOAT) {
	    i_add_ticks(f, 1);
	    GET_FLT(f->sp, f2);
	    f->sp++;
	    GET_FLT(f->sp, f1);
	    flt_sub(&f1, &f2);
	    PUT_FLT(f->sp, f1);
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    array *a;

	    i_add_ticks(f, (Int) f->sp[1].u.array->size + f->sp->u.array->size);
	    a = arr_sub(f->data, f->sp[1].u.array, f->sp->u.array);
	    arr_del(f->sp->u.array);
	    f->sp++;
	    arr_del(f->sp->u.array);
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    case T_MAPPING:
	if (f->sp->type == T_ARRAY) {
	    array *a;

	    i_add_ticks(f, (Int) f->sp[1].u.array->size + f->sp->u.array->size);
	    a = map_sub(f->data, f->sp[1].u.array, f->sp->u.array);
	    arr_del(f->sp->u.array);
	    f->sp++;
	    arr_del(f->sp->u.array);
	    PUT_MAP(f->sp, a);
	    return 0;
	}
	break;

    default:
	kf_argerror(KF_SUB, 1);
    }

    kf_argerror(KF_SUB, 2);
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub_int, pt_sub_int)
# else
char pt_sub_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->sub_int()
 * DESCRIPTION:	int - int
 */
int kf_sub_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], f->sp[1].u.number - f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1, pt_sub1)
# else
char pt_sub1[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->sub1()
 * DESCRIPTION:	value--
 */
int kf_sub1(f)
register frame *f;
{
    xfloat f1, f2;

    if (f->sp->type == T_INT) {
	PUT_INT(f->sp, f->sp->u.number - 1);
    } else if (f->sp->type == T_FLOAT) {
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f1);
	FLT_ONE(f2.high, f2.low);
	flt_sub(&f1, &f2);
	PUT_FLT(f->sp, f1);
    } else {
	kf_argerror(KF_SUB1, 1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1_int, pt_sub1_int)
# else
char pt_sub1_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->sub1_int()
 * DESCRIPTION:	int--
 */
int kf_sub1_int(f)
frame *f;
{
    PUT_INT(f->sp, f->sp->u.number - 1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(float)", kf_tofloat, pt_tofloat)
# else
char pt_tofloat[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_MIXED };

/*
 * NAME:	kfun->tofloat()
 * DESCRIPTION:	convert to float
 */
int kf_tofloat(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 1);
    if (f->sp->type == T_INT) {
	/* from int */
	flt_itof(f->sp->u.number, &flt);
	PUT_FLTVAL(f->sp, flt);
	return 0;
    } else if (f->sp->type == T_STRING) {
	char *p;

	/* from string */
	p = f->sp->u.string->text;
	if (!flt_atof(&p, &flt) ||
	    p != f->sp->u.string->text + f->sp->u.string->len) {
	    error("String cannot be converted to float");
	}
	str_del(f->sp->u.string);
	PUT_FLTVAL(f->sp, flt);
	return 0;
    }

    if (f->sp->type != T_FLOAT) {
	error("Value is not a float");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(int)", kf_toint, pt_toint)
# else
char pt_toint[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * NAME:	kfun->toint()
 * DESCRIPTION:	convert to integer
 */
int kf_toint(f)
register frame *f;
{
    xfloat flt;

    if (f->sp->type == T_FLOAT) {
	/* from float */
	i_add_ticks(f, 1);
	GET_FLT(f->sp, flt);
	PUT_INTVAL(f->sp, flt_ftoi(&flt));
	return 0;
    } else if (f->sp->type == T_STRING) {
	char *p;
	Int i;

	/* from string */
	p = f->sp->u.string->text;
	i = strtoint(&p);
	if (p != f->sp->u.string->text + f->sp->u.string->len) {
	    error("String cannot be converted to int");
	}
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, i);
	return 0;
    }

    if (f->sp->type != T_INT) {
	error("Value is not an int");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst, pt_tst)
# else
char pt_tst[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * NAME:	kfun->tst()
 * DESCRIPTION:	!! value
 */
int kf_tst(f)
register frame *f;
{
    switch (f->sp->type) {
    case T_NIL:
	PUT_INTVAL(f->sp, FALSE);
	return 0;

    case T_INT:
	PUT_INT(f->sp, (f->sp->u.number != 0));
	return 0;

    case T_FLOAT:
	PUT_INTVAL(f->sp, !VFLT_ISZERO(f->sp));
	return 0;

    case T_STRING:
	str_del(f->sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr_del(f->sp->u.array);
	break;
    }

    PUT_INTVAL(f->sp, TRUE);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst_int, pt_tst)
# else
/*
 * NAME:	kfun->tst_int()
 * DESCRIPTION:	!! int
 */
int kf_tst_int(f)
frame *f;
{
    PUT_INT(f->sp, (f->sp->u.number != 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("unary -", kf_umin, pt_umin)
# else
char pt_umin[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->umin()
 * DESCRIPTION:	- mixed
 */
int kf_umin(f)
register frame *f;
{
    xfloat flt;

    switch (f->sp->type) {
    case T_INT:
	PUT_INT(f->sp, -f->sp->u.number);
	return 0;

    case T_FLOAT:
	i_add_ticks(f, 1);
	if (!VFLT_ISZERO(f->sp)) {
	    GET_FLT(f->sp, flt);
	    FLT_NEG(flt.high, flt.low);
	    PUT_FLT(f->sp, flt);
	}
	return 0;
    }

    kf_argerror(KF_UMIN, 1);
}
# endif


# ifdef FUNCDEF
FUNCDEF("unary -", kf_umin_int, pt_umin_int)
# else
char pt_umin_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->umin_int()
 * DESCRIPTION:	- int
 */
int kf_umin_int(f)
frame *f;
{
    PUT_INT(f->sp, -f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor, pt_xor)
# else
char pt_xor[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->xor()
 * DESCRIPTION:	value ^ value
 */
int kf_xor(f)
register frame *f;
{
    array *a;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type == T_INT) {
	    PUT_INT(&f->sp[1], f->sp[1].u.number ^ f->sp->u.number);
	    f->sp++;
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    i_add_ticks(f, (Int) f->sp[1].u.array->size + f->sp->u.array->size);
	    a = arr_setxadd(f->data, f->sp[1].u.array, f->sp->u.array);
	    arr_del(f->sp->u.array);
	    f->sp++;
	    arr_del(f->sp->u.array);
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    default:
	kf_argerror(KF_XOR, 1);
    }

    kf_argerror(KF_XOR, 2);
}
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor_int, pt_xor_int)
# else
char pt_xor_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->xor_int()
 * DESCRIPTION:	int ^ int
 */
int kf_xor_int(f)
register frame *f;
{
    PUT_INT(&f->sp[1], f->sp[1].u.number ^ f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(string)", kf_tostring, pt_tostring)
# else
char pt_tostring[] = { C_STATIC, 1, 0, 0, 7, T_STRING, T_MIXED };

/*
 * NAME:	kfun->tostring()
 * DESCRIPTION:	cast an int or float to a string
 */
int kf_tostring(f)
register frame *f;
{
    char *num, buffer[18];
    xfloat flt;

    i_add_ticks(f, 2);
    if (f->sp->type == T_INT) {
	/* from int */
	num = kf_itoa(f->sp->u.number, buffer);
    } else if (f->sp->type == T_FLOAT) {
	/* from float */
	i_add_ticks(f, 1);
	GET_FLT(f->sp, flt);
	flt_ftoa(&flt, num = buffer);
    } else if (f->sp->type == T_STRING) {
	return 0;
    } else {
	error("Value is not a string");
    }

    PUT_STRVAL(f->sp, str_new(num, (long) strlen(num)));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_ckrangeft, pt_ckrangeft)
# else
char pt_ckrangeft[] = { C_STATIC, 3, 0, 0, 9, T_INT, T_MIXED, T_INT, T_INT };

/*
 * NAME:	kfun->ckrangeft()
 * DESCRIPTION:	Check a [ from .. to ] subrange.
 *		This function doesn't pop its arguments and returns nothing.
 */
int kf_ckrangeft(f)
register frame *f;
{
    if (f->sp[1].type != T_INT) {
	kf_argerror(KF_CKRANGEFT, 2);
    }
    if (f->sp->type != T_INT) {
	kf_argerror(KF_CKRANGEFT, 3);
    }
    if (f->sp[2].type == T_STRING) {
	str_ckrange(f->sp[2].u.string, (long) f->sp[1].u.number,
		    (long) f->sp->u.number);
    } else if (f->sp[2].type == T_ARRAY) {
	arr_ckrange(f->sp[2].u.array, (long) f->sp[1].u.number,
		    (long) f->sp->u.number);
    } else {
	kf_argerror(KF_CKRANGEFT, 1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_ckrangef, pt_ckrangef)
# else
char pt_ckrangef[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_INT };

/*
 * NAME:	kfun->ckrangef()
 * DESCRIPTION:	Check a [ from .. ] subrange, add missing index.
 *		This function doesn't pop its arguments.
 */
int kf_ckrangef(f)
register frame *f;
{
    if (f->sp->type != T_INT) {
	kf_argerror(KF_CKRANGEF, 2);
    }
    if (f->sp[1].type == T_STRING) {
	(--f->sp)->type = T_INT;
	f->sp->u.number = (Int) f->sp[2].u.string->len - 1;
	str_ckrange(f->sp[2].u.string, (long) f->sp[1].u.number,
		    (long) f->sp->u.number);
    } else if (f->sp[1].type == T_ARRAY) {
	(--f->sp)->type = T_INT;
	f->sp->u.number = (Int) f->sp[2].u.array->size - 1;
	arr_ckrange(f->sp[2].u.array, (long) f->sp[1].u.number,
		    (long) f->sp->u.number);
    } else {
	kf_argerror(KF_CKRANGEF, 1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[]", kf_ckranget, pt_ckranget)
# else
char pt_ckranget[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_INT };

/*
 * NAME:	kfun->ckranget()
 * DESCRIPTION:	Check a [ .. to ] subrange, add missing index.
 *		This function doesn't pop its arguments.
 */
int kf_ckranget(f)
register frame *f;
{
    if (f->sp->type != T_INT) {
	kf_argerror(KF_CKRANGET, 2);
    }
    if (f->sp[1].type == T_STRING) {
	str_ckrange(f->sp[1].u.string, 0L, (long) f->sp->u.number);
    } else if (f->sp[1].type == T_ARRAY) {
	arr_ckrange(f->sp[1].u.array, 0L, (long) f->sp->u.number);
    } else {
	kf_argerror(KF_CKRANGET, 1);
    }

    --f->sp;
    f->sp[0] = f->sp[1];
    PUT_INT(&f->sp[1], 0);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sum", kf_sum, pt_sum)
# else
char pt_sum[] = { C_STATIC | C_ELLIPSIS, 0, 1, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->sum()
 * DESCRIPTION:	perform a summand operation
 */
int kf_sum(f, nargs)
register frame *f;
int nargs;
{
    char buffer[12], *num;
    string *s;
    array *a;
    register value *v, *e1, *e2;
    register int i, type, vtype, nonint;
    register long size;
    register ssizet len;
    register Int result;
    register long isize;

    /*
     * pass 1: check the types of everything and calculate the size
     */
    i_add_ticks(f, nargs);
    type = T_NIL;
    isize = size = 0;
    nonint = nargs;
    result = 0;
    for (v = f->sp, i = nargs; --i >= 0; v++) {
	if (v->u.number == -2) {
	    /* simple term */
	    v++;
	    vtype = v->type;
	    if (vtype == T_STRING) {
		size += v->u.string->len;
	    } else if (vtype == T_ARRAY) {
		size += v->u.array->size;
	    } else {
		size += strlen(kf_itoa(v->u.number, buffer));
	    }
	} else if (v->u.number < -2) {
	    /* aggregate */
	    size += -3 - v->u.number;
	    v += -3 - v->u.number;
	    vtype = T_ARRAY;
	} else {
	    /* subrange term */
	    size += v->u.number - v[1].u.number + 1;
	    v += 2;
	    vtype = v->type;
	}

	if (vtype == T_STRING || vtype == T_ARRAY) {
	    nonint = i;
	    isize = size;
	    if (type == T_NIL && (vtype != T_ARRAY || i == nargs - 1)) {
		type = vtype;
	    } else if (type != vtype) {
		error("Bad argument 2 for kfun +");
	    }
	} else if (vtype != T_INT || type == T_ARRAY) {
	    error("Bad argument %d for kfun +", (i == 0) ? 1 : 2);
	} else {
	    result += v->u.number;
	}
    }
    if (nonint > 1) {
	size = isize + strlen(kf_itoa(result, buffer));
    }

    /*
     * pass 2: build the string or array
     */
    result = 0;
    if (type == T_STRING) {
	s = str_new((char *) NULL, size);
	s->text[size] = '\0';
	for (v = f->sp, i = nargs; --i >= 0; v++) {
	    if (v->u.number == -2) {
		/* simple term */
		v++;
		if (v->type == T_STRING) {
		    size -= v->u.string->len;
		    memcpy(s->text + size, v->u.string->text, v->u.string->len);
		    str_del(v->u.string);
		    result = 0;
		} else if (nonint < i) {
		    num = kf_itoa(v->u.number, buffer);
		    len = strlen(num);
		    size -= len;
		    memcpy(s->text + size, num, len);
		    result = 0;
		} else {
		    result += v->u.number;
		}
	    } else {
		/* subrange */
		len = v->u.number - v[1].u.number + 1;
		size -= len;
		memcpy(s->text + size, v[2].u.string->text + v[1].u.number,
		       len);
		v += 2;
		str_del(v->u.string);
		result = 0;
	    }
	}
	if (nonint > 0) {
	    num = kf_itoa(result, buffer);
	    memcpy(s->text, num, strlen(num));
	}

	f->sp = v - 1;
	PUT_STRVAL(f->sp, s);
    } else if (type == T_ARRAY) {
	a = arr_new(f->data, size);
	e1 = a->elts + size;
	for (v = f->sp, i = nargs; --i >= 0; v++) {
	    if (v->u.number == -2) {
		/* simple term */
		v++;
		len = v->u.array->size;
		e2 = d_get_elts(v->u.array) + len;
	    } else if (v->u.number < -2) {
		/* aggregate */
		for (len = -3 - v->u.number; len > 0; --len) {
		    *--e1 = *++v;
		}
		continue;
	    } else {
		/* subrange */
		len = v->u.number - v[1].u.number + 1;
		e2 = d_get_elts(v[2].u.array) + v->u.number + 1;
		v += 2;
	    }

	    e1 -= len;
	    i_copy(e1, e2 - len, len);
	    arr_del(v->u.array);
	    size -= len;
	}

	f->sp = v - 1;
	d_ref_imports(a);
	PUT_ARRVAL(f->sp, a);
    } else {
	/* integers only */
	for (v = f->sp, i = nargs; --i > 0; v += 2) {
	    result += v[1].u.number;
	}

	f->sp = v + 1;
	f->sp->u.number += result;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_status_idx, pt_status_idx)
# else
char pt_status_idx[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_INT };

/*
 * NAME:	kfun->status_idx()
 * DESCRIPTION:	return status()[idx]
 */
int kf_status_idx(f)
register frame *f;
{
    if (f->sp->type != T_INT) {
	error("Non-numeric array index");
    }
    i_add_ticks(f, 6);
    if (!conf_statusi(f, f->sp->u.number, f->sp)) {
	error("Index out of range");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_statuso_idx, pt_statuso_idx)
# else
char pt_statuso_idx[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_OBJECT, T_INT };

/*
 * NAME:	kfun->statuso_idx()
 * DESCRIPTION:	return status(obj)[idx]
 */
int kf_statuso_idx(f)
register frame *f;
{
    uindex n;

    switch (f->sp[1].type) {
    case T_OBJECT:
	n = f->sp[1].oindex;
	break;

    case T_LWOBJECT:
	n = f->sp[1].u.array->elts[0].oindex;
	arr_del(f->sp[1].u.array);
	f->sp[1] = nil_value;
	break;

    default:
	return 1;
    }
    if (f->sp->type != T_INT) {
	error("Non-numeric array index");
    }
    i_add_ticks(f, 6);
    if (!conf_objecti(f->data, OBJR(n), f->sp->u.number, &f->sp[1])) {
	error("Index out of range");
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_trace", kf_calltr_idx, pt_calltr_idx)
# else
char pt_calltr_idx[] = { C_STATIC, 1, 0, 0, 7, T_MIXED | (1 << REFSHIFT),
			 T_INT };

/*
 * NAME:	kfun->calltr_idx()
 * DESCRIPTION:	return call_trace()[idx]
 */
int kf_calltr_idx(f)
register frame *f;
{
    if (f->sp->type != T_INT) {
	error("Non-numeric array index");
    }
    i_add_ticks(f, 10);
    if (!i_call_tracei(f, f->sp->u.number, f->sp)) {
	error("Index out of range");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("nil", kf_nil, pt_nil)
# else
char pt_nil[] = { C_STATIC, 0, 0, 0, 6, T_NIL };

/*
 * NAME:	kfun->nil()
 * DESCRIPTION:	return nil
 */
int kf_nil(f)
register frame *f;
{
    *--f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<-", kf_instanceof, pt_instanceof)
# else
char pt_instanceof[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_OBJECT, T_INT };

/*
 * NAME:	kfun->instanceof()
 * DESCRIPTION:	instanceof
 */
int kf_instanceof(f)
register frame *f;
{
    uindex oindex;
    bool flag;

    switch (f->sp[1].type) {
    case T_OBJECT:
	oindex = f->sp[1].oindex;
	break;

    case T_LWOBJECT:
	oindex = d_get_elts(f->sp[1].u.array)->oindex;
	arr_del(f->sp[1].u.array);
	break;

    default:
	kf_argerror(KF_INSTANCEOF, 1);
    }
    flag = i_instanceof(f, oindex, f->sp->u.number);
    f->sp++;
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif
