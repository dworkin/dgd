/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2013 DGD Authors (see the commit log for details)
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

# ifndef FUNCDEF
# define INCLUDE_CTYPE
# include "kfun.h"
# include "table.h"

/*
 * NAME:	kfun->argerror()
 * DESCRIPTION:	handle an argument error in a builtin kfun
 */
static void kf_argerror(int kfun, int n)
{
    error("Bad argument %d for kfun %s", n, kftab[kfun].name);
}

/*
 * NAME:	kfun->op_unary()
 * DESCRIPTION:	handle unary operator
 */
static void kf_op_unary(register frame *f, int kfun)
{
    if (!i_call(f, (object *) NULL, f->sp->u.array, kftab[kfun].name,
		strlen(kftab[kfun].name), TRUE, 0)) {
	kf_argerror(kfun, 1);
    }
    if (f->sp->type != T_LWOBJECT || f->sp->u.array->elts[0].type != T_OBJECT) {
	error("operator %s did not return a light-weight object",
	      kftab[kfun].name);
    }

    arr_del(f->sp[1].u.array);
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * NAME:	kfun->op_binary()
 * DESCRIPTION:	handle binary operator
 */
static void kf_op_binary(register frame *f, int kfun)
{
    if (VAL_NIL(f->sp)) {
	kf_argerror(kfun, 2);
    }

    if (!i_call(f, (object *) NULL, f->sp[1].u.array, kftab[kfun].name,
		strlen(kftab[kfun].name), TRUE, 1)) {
	kf_argerror(kfun, 1);
    }
    if (f->sp->type != T_LWOBJECT || f->sp->u.array->elts[0].type != T_OBJECT) {
	error("operator %s did not return a light-weight object",
	      kftab[kfun].name);
    }

    arr_del(f->sp[1].u.array);
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * NAME:	kfun->op_compare()
 * DESCRIPTION:	handle compare operator
 */
static void kf_op_compare(register frame *f, int kfun)
{
    if (VAL_NIL(f->sp)) {
	kf_argerror(kfun, 2);
    }

    if (!i_call(f, (object *) NULL, f->sp[1].u.array, kftab[kfun].name,
		strlen(kftab[kfun].name), TRUE, 1)) {
	kf_argerror(kfun, 1);
    }
    if (f->sp->type != T_INT || (f->sp->u.number & ~1)) {
	error("operator %s did not return a truth value",
	      kftab[kfun].name);
    }

    arr_del(f->sp[1].u.array);
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * NAME:	kfun->op_ternary()
 * DESCRIPTION:	handle ternary operator
 */
static void kf_op_ternary(register frame *f, int kfun)
{
    if (!i_call(f, (object *) NULL, f->sp[2].u.array, kftab[kfun].name,
		strlen(kftab[kfun].name), TRUE, 2)) {
	kf_argerror(kfun, 1);
    }
    if (f->sp->type != T_LWOBJECT || f->sp->u.array->elts[0].type != T_OBJECT) {
	error("operator %s did not return a light-weight object",
	      kftab[kfun].name);
    }

    arr_del(f->sp[1].u.array);
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * NAME:	kfun->itoa()
 * DESCRIPTION:	convert an Int to a string
 */
static char *kf_itoa(Int i, char *buffer)
{
    Uint u;
    char *p;

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
FUNCDEF("+", kf_add, pt_add, 0)
# else
char pt_add[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->add()
 * DESCRIPTION:	value + value
 */
int kf_add(frame *f)
{
    string *str;
    array *a;
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

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_binary(f, KF_ADD);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_ADD, 1);
    }

    kf_argerror(KF_ADD, 2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_int, pt_add_int, 0)
# else
char pt_add_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->add_int()
 * DESCRIPTION:	int + int
 */
int kf_add_int(frame *f)
{
    PUT_INT(&f->sp[1], f->sp[1].u.number + f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1, pt_add1, 0)
# else
char pt_add1[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->add1()
 * DESCRIPTION:	value++
 */
int kf_add1(frame *f)
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
    } else if (f->sp->type == T_LWOBJECT &&
	       f->sp->u.array->elts[0].type == T_OBJECT) {
	kf_op_unary(f, KF_ADD1);
    } else {
	kf_argerror(KF_ADD1, 1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1_int, pt_add1_int, 0)
# else
char pt_add1_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->add1_int()
 * DESCRIPTION:	int++
 */
int kf_add1_int(frame *f)
{
    PUT_INT(f->sp, f->sp->u.number + 1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("&", kf_and, pt_and, 0)
# else
char pt_and[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->and()
 * DESCRIPTION:	value & value
 */
int kf_and(frame *f)
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

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_binary(f, KF_AND);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_AND, 1);
    }

    kf_argerror(KF_AND, 2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("&", kf_and_int, pt_and_int, 0)
# else
char pt_and_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->and_int()
 * DESCRIPTION:	int & int
 */
int kf_and_int(frame *f)
{
    PUT_INT(&f->sp[1], f->sp[1].u.number & f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div, pt_div, 0)
# else
char pt_div[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->div()
 * DESCRIPTION:	mixed / mixed
 */
int kf_div(frame *f)
{
    Int i, d;
    xfloat f1, f2;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_DIV, 2);
	}
	i = f->sp[1].u.number;
	d = f->sp->u.number;
	if (d == 0) {
	    error("Division by zero");
	}
	PUT_INT(&f->sp[1], i / d);
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf_argerror(KF_DIV, 2);
	}
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	flt_div(&f1, &f2);
	PUT_FLT(f->sp, f1);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_binary(f, KF_DIV);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_DIV, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div_int, pt_div_int, 0)
# else
char pt_div_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->div()
 * DESCRIPTION:	int / int
 */
int kf_div_int(frame *f)
{
    Int i, d;

    i = f->sp[1].u.number;
    d = f->sp->u.number;
    if (d == 0) {
	error("Division by zero");
    }
    PUT_INT(&f->sp[1], i / d);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq, pt_eq, 0)
# else
char pt_eq[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->eq()
 * DESCRIPTION:	value == value
 */
int kf_eq(frame *f)
{
    bool flag;
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
FUNCDEF("==", kf_eq_int, pt_eq_int, 0)
# else
char pt_eq_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->eq_int()
 * DESCRIPTION:	int == int
 */
int kf_eq_int(frame *f)
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number == f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge, pt_ge, 0)
# else
char pt_ge[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ge()
 * DESCRIPTION:	value >= value
 */
int kf_ge(frame *f)
{
    xfloat f1, f2;
    bool flag;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_GE, 2);
	}
	PUT_INT(&f->sp[1], (f->sp[1].u.number >= f->sp->u.number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf_argerror(KF_GE, 2);
	}
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) >= 0));
	return 0;

    case T_STRING:
	if (f->sp->type != T_STRING) {
	    kf_argerror(KF_GE, 2);
	}
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) >= 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_compare(f, KF_GE);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_GE, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_int, pt_ge_int, 0)
# else
char pt_ge_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->ge_int()
 * DESCRIPTION:	int >= int
 */
int kf_ge_int(frame *f)
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number >= f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt, pt_gt, 0)
# else
char pt_gt[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->gt()
 * DESCRIPTION:	value > value
 */
int kf_gt(frame *f)
{
    xfloat f1, f2;
    bool flag;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_GT, 2);
	}
	PUT_INT(&f->sp[1], (f->sp[1].u.number > f->sp->u.number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf_argerror(KF_GT, 2);
	}
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) > 0));
	return 0;

    case T_STRING:
	if (f->sp->type != T_STRING) {
	    kf_argerror(KF_GT, 2);
	}
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) > 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_compare(f, KF_GT);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_GT, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_int, pt_gt_int, 0)
# else
char pt_gt_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->gt_int()
 * DESCRIPTION:	int > int
 */
int kf_gt_int(frame *f)
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number > f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le, pt_le, 0)
# else
char pt_le[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->le()
 * DESCRIPTION:	value <= value
 */
int kf_le(frame *f)
{
    xfloat f1, f2;
    bool flag;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_LE, 2);
	}
	PUT_INT(&f->sp[1], (f->sp[1].u.number <= f->sp->u.number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf_argerror(KF_LE, 2);
	}
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) <= 0));
	return 0;

    case T_STRING:
	if (f->sp->type != T_STRING) {
	    kf_argerror(KF_LE, 2);
	}
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) <= 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_compare(f, KF_LE);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_LE, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_int, pt_le_int, 0)
# else
char pt_le_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->le_int()
 * DESCRIPTION:	int <= int
 */
int kf_le_int(frame *f)
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number <= f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<<", kf_lshift, pt_lshift, 0)
# else
char pt_lshift[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->lshift()
 * DESCRIPTION:	int << int
 */
int kf_lshift(frame *f)
{
    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].u.array->elts[0].type == T_OBJECT) {
	kf_op_binary(f, KF_LSHIFT);
	return 0;
    }
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
FUNCDEF("<<", kf_lshift_int, pt_lshift_int, 0)
# else
char pt_lshift_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->lshift_int()
 * DESCRIPTION:	int << int
 */
int kf_lshift_int(frame *f)
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
FUNCDEF("<", kf_lt, pt_lt, 0)
# else
char pt_lt[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->lt()
 * DESCRIPTION:	value < value
 */
int kf_lt(frame *f)
{
    xfloat f1, f2;
    bool flag;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_LT, 2);
	}
	PUT_INT(&f->sp[1], (f->sp[1].u.number < f->sp->u.number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf_argerror(KF_LT, 2);
	}
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) < 0));
	return 0;

    case T_STRING:
	if (f->sp->type != T_STRING) {
	    kf_argerror(KF_LT, 2);
	}
	i_add_ticks(f, 2);
	flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) < 0);
	str_del(f->sp->u.string);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_INTVAL(f->sp, flag);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_compare(f, KF_LT);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_LT, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_int, pt_lt_int, 0)
# else
char pt_lt_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->lt_int()
 * DESCRIPTION:	int < int
 */
int kf_lt_int(frame *f)
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number < f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("%", kf_mod, pt_mod, 0)
# else
char pt_mod[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->mod()
 * DESCRIPTION:	int % int
 */
int kf_mod(frame *f)
{
    Int i, d;

    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].u.array->elts[0].type == T_OBJECT) {
	kf_op_binary(f, KF_MOD);
	return 0;
    }
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
    PUT_INT(&f->sp[1], i % d);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("%", kf_mod_int, pt_mod_int, 0)
# else
char pt_mod_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->mod_int()
 * DESCRIPTION:	int % int
 */
int kf_mod_int(frame *f)
{
    Int i, d;

    i = f->sp[1].u.number;
    d = f->sp->u.number;
    if (d == 0) {
	error("Modulus by zero");
    }
    PUT_INT(&f->sp[1], i % d);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult, pt_mult, 0)
# else
char pt_mult[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->mult()
 * DESCRIPTION:	mixed * mixed
 */
int kf_mult(frame *f)
{
    xfloat f1, f2;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_MULT, 2);
	}
	PUT_INT(&f->sp[1], f->sp[1].u.number * f->sp->u.number);
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf_argerror(KF_MULT, 2);
	}
	i_add_ticks(f, 1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	flt_mult(&f1, &f2);
	PUT_FLT(f->sp, f1);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_binary(f, KF_MULT);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_MULT, 1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult_int, pt_mult_int, 0)
# else
char pt_mult_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->mult_int()
 * DESCRIPTION:	int * int
 */
int kf_mult_int(frame *f)
{
    PUT_INT(&f->sp[1], f->sp[1].u.number * f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne, pt_ne, 0)
# else
char pt_ne[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ne()
 * DESCRIPTION:	value != value
 */
int kf_ne(frame *f)
{
    bool flag;
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
FUNCDEF("!=", kf_ne_int, pt_ne_int, 0)
# else
char pt_ne_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->ne_int()
 * DESCRIPTION:	int != int
 */
int kf_ne_int(frame *f)
{
    PUT_INT(&f->sp[1], (f->sp[1].u.number != f->sp->u.number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("~", kf_neg, pt_neg, 0)
# else
char pt_neg[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->neg()
 * DESCRIPTION:	~ int
 */
int kf_neg(frame *f)
{
    if (f->sp->type == T_LWOBJECT &&
	f->sp->u.array->elts[0].type == T_OBJECT) {
	kf_op_unary(f, KF_NEG);
	return 0;
    }
    if (f->sp->type != T_INT) {
	kf_argerror(KF_NEG, 1);
    }
    PUT_INT(f->sp, ~f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("~", kf_neg_int, pt_neg_int, 0)
# else
char pt_neg_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->neg_int()
 * DESCRIPTION:	~ int
 */
int kf_neg_int(frame *f)
{
    PUT_INT(f->sp, ~f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!", kf_not, pt_not, 0)
# else
char pt_not[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * NAME:	kfun->not()
 * DESCRIPTION:	! value
 */
int kf_not(frame *f)
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
FUNCDEF("!", kf_not_int, pt_not, 0)
# else
/*
 * NAME:	kfun->not_int()
 * DESCRIPTION:	! int
 */
int kf_not_int(frame *f)
{
    PUT_INT(f->sp, !f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or, pt_or, 0)
# else
char pt_or[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->or()
 * DESCRIPTION:	value | value
 */
int kf_or(frame *f)
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

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_binary(f, KF_OR);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_OR, 1);
    }

    kf_argerror(KF_OR, 2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or_int, pt_or_int, 0)
# else
char pt_or_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->or_int()
 * DESCRIPTION:	int | int
 */
int kf_or_int(frame *f)
{
    PUT_INT(&f->sp[1], f->sp[1].u.number | f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_rangeft, pt_rangeft, 0)
# else
char pt_rangeft[] = { C_STATIC, 3, 0, 0, 9, T_MIXED, T_MIXED, T_MIXED,
		      T_MIXED };
/*
 * NAME:	kfun->rangeft()
 * DESCRIPTION:	value [ int .. int ]
 */
int kf_rangeft(frame *f)
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
    if (f->sp[2].type == T_LWOBJECT &&
	f->sp[2].u.array->elts[0].type == T_OBJECT) {
	if (VAL_NIL(f->sp + 1)) {
	    kf_argerror(KF_RANGEFT, 2);
	}
	if (VAL_NIL(f->sp)) {
	    kf_argerror(KF_RANGEFT, 3);
	}
	kf_op_ternary(f, KF_RANGEFT);

	return 0;
    }

    switch (f->sp[2].type) {
    case T_STRING:
	if (f->sp[1].type != T_INT) {
	    kf_argerror(KF_RANGEFT, 2);
	}
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_RANGEFT, 3);
	}
	i_add_ticks(f, 2);
	str = str_range(f->sp[2].u.string, (long) f->sp[1].u.number,
			(long) f->sp->u.number);
	f->sp += 2;
	str_del(f->sp->u.string);
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	if (f->sp[1].type != T_INT) {
	    kf_argerror(KF_RANGEFT, 2);
	}
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_RANGEFT, 3);
	}
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
FUNCDEF("[..]", kf_rangef, pt_rangef, 0)
# else
char pt_rangef[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->rangef()
 * DESCRIPTION:	value [ int .. ]
 */
int kf_rangef(frame *f)
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
    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].u.array->elts[0].type == T_OBJECT) {
	if (VAL_NIL(f->sp)) {
	    kf_argerror(KF_RANGEF, 2);
	}
	*--f->sp = nil_value;
	kf_op_ternary(f, KF_RANGEF);

	return 0;
    }

    switch (f->sp[1].type) {
    case T_STRING:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_RANGEF, 2);
	}
	i_add_ticks(f, 2);
	str = str_range(f->sp[1].u.string, (long) f->sp->u.number,
			f->sp[1].u.string->len - 1L);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_RANGEF, 2);
	}
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
FUNCDEF("[..]", kf_ranget, pt_ranget, 0)
# else
char pt_ranget[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->ranget()
 * DESCRIPTION:	value [ .. int ]
 */
int kf_ranget(frame *f)
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
    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].u.array->elts[0].type == T_OBJECT) {
	if (VAL_NIL(f->sp)) {
	    kf_argerror(KF_RANGET, 2);
	}
	--f->sp;
	f->sp[0] = f->sp[1];
	f->sp[1] = nil_value;
	kf_op_ternary(f, KF_RANGET);

	return 0;
    }

    switch (f->sp[1].type) {
    case T_STRING:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_RANGET, 2);
	}
	i_add_ticks(f, 2);
	str = str_range(f->sp[1].u.string, 0L, (long) f->sp->u.number);
	f->sp++;
	str_del(f->sp->u.string);
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	if (f->sp->type != T_INT) {
	    kf_argerror(KF_RANGET, 2);
	}
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
FUNCDEF("[..]", kf_range, pt_range, 0)
# else
char pt_range[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->range()
 * DESCRIPTION:	value [ .. ]
 */
int kf_range(frame *f)
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
    if (f->sp->type == T_LWOBJECT &&
	f->sp->u.array->elts[0].type == T_OBJECT) {
	*--f->sp = nil_value;
	*--f->sp = nil_value;
	kf_op_ternary(f, KF_RANGE);

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
FUNCDEF(">>", kf_rshift, pt_rshift, 0)
# else
char pt_rshift[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->rshift()
 * DESCRIPTION:	int >> int
 */
int kf_rshift(frame *f)
{
    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].u.array->elts[0].type == T_OBJECT) {
	kf_op_binary(f, KF_RSHIFT);
	return 0;
    }
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
FUNCDEF(">>", kf_rshift_int, pt_rshift_int, 0)
# else
char pt_rshift_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->rshift_int()
 * DESCRIPTION:	int >> int
 */
int kf_rshift_int(frame *f)
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
FUNCDEF("-", kf_sub, pt_sub, 0)
# else
char pt_sub[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->sub()
 * DESCRIPTION:	value - value
 */
int kf_sub(frame *f)
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

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_binary(f, KF_SUB);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_SUB, 1);
    }

    kf_argerror(KF_SUB, 2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub_int, pt_sub_int, 0)
# else
char pt_sub_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->sub_int()
 * DESCRIPTION:	int - int
 */
int kf_sub_int(frame *f)
{
    PUT_INT(&f->sp[1], f->sp[1].u.number - f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1, pt_sub1, 0)
# else
char pt_sub1[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->sub1()
 * DESCRIPTION:	value--
 */
int kf_sub1(frame *f)
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
    } else if (f->sp->type == T_LWOBJECT &&
	       f->sp->u.array->elts[0].type == T_OBJECT) {
	kf_op_unary(f, KF_SUB1);
    } else {
	kf_argerror(KF_SUB1, 1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1_int, pt_sub1_int, 0)
# else
char pt_sub1_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->sub1_int()
 * DESCRIPTION:	int--
 */
int kf_sub1_int(frame *f)
{
    PUT_INT(f->sp, f->sp->u.number - 1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(float)", kf_tofloat, pt_tofloat, 0)
# else
char pt_tofloat[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_MIXED };

/*
 * NAME:	kfun->tofloat()
 * DESCRIPTION:	convert to float
 */
int kf_tofloat(frame *f)
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
FUNCDEF("(int)", kf_toint, pt_toint, 0)
# else
char pt_toint[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * NAME:	kfun->toint()
 * DESCRIPTION:	convert to integer
 */
int kf_toint(frame *f)
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
FUNCDEF("!!", kf_tst, pt_tst, 0)
# else
char pt_tst[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * NAME:	kfun->tst()
 * DESCRIPTION:	!! value
 */
int kf_tst(frame *f)
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
FUNCDEF("!!", kf_tst_int, pt_tst, 0)
# else
/*
 * NAME:	kfun->tst_int()
 * DESCRIPTION:	!! int
 */
int kf_tst_int(frame *f)
{
    PUT_INT(f->sp, (f->sp->u.number != 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_umin, pt_umin, 0)
# else
char pt_umin[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->umin()
 * DESCRIPTION:	- mixed
 */
int kf_umin(frame *f)
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

    case T_LWOBJECT:
	if (f->sp->u.array->elts[0].type == T_OBJECT) {
	    kf_op_unary(f, KF_UMIN);
	    return 0;
	}
	break;
    }

    kf_argerror(KF_UMIN, 1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_umin_int, pt_umin_int, 0)
# else
char pt_umin_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * NAME:	kfun->umin_int()
 * DESCRIPTION:	- int
 */
int kf_umin_int(frame *f)
{
    PUT_INT(f->sp, -f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor, pt_xor, 0)
# else
char pt_xor[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->xor()
 * DESCRIPTION:	value ^ value
 */
int kf_xor(frame *f)
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

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    kf_op_binary(f, KF_XOR);
	    return 0;
	}
	/* fall through */
    default:
	kf_argerror(KF_XOR, 1);
    }

    kf_argerror(KF_XOR, 2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor_int, pt_xor_int, 0)
# else
char pt_xor_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * NAME:	kfun->xor_int()
 * DESCRIPTION:	int ^ int
 */
int kf_xor_int(frame *f)
{
    PUT_INT(&f->sp[1], f->sp[1].u.number ^ f->sp->u.number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(string)", kf_tostring, pt_tostring, 0)
# else
char pt_tostring[] = { C_STATIC, 1, 0, 0, 7, T_STRING, T_MIXED };

/*
 * NAME:	kfun->tostring()
 * DESCRIPTION:	cast an int or float to a string
 */
int kf_tostring(frame *f)
{
    char *num, buffer[18];
    xfloat flt;

    num = NULL;

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
FUNCDEF("[..]", kf_ckrangeft, pt_ckrangeft, 0)
# else
char pt_ckrangeft[] = { C_STATIC, 3, 0, 0, 9, T_INT, T_MIXED, T_INT, T_INT };

/*
 * NAME:	kfun->ckrangeft()
 * DESCRIPTION:	Check a [ from .. to ] subrange.
 *		This function doesn't pop its arguments and returns nothing.
 */
int kf_ckrangeft(frame *f)
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
FUNCDEF("[..]", kf_ckrangef, pt_ckrangef, 0)
# else
char pt_ckrangef[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_INT };

/*
 * NAME:	kfun->ckrangef()
 * DESCRIPTION:	Check a [ from .. ] subrange, add missing index.
 *		This function doesn't pop its arguments.
 */
int kf_ckrangef(frame *f)
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
FUNCDEF("[..]", kf_ckranget, pt_ckranget, 0)
# else
char pt_ckranget[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_INT };

/*
 * NAME:	kfun->ckranget()
 * DESCRIPTION:	Check a [ .. to ] subrange, add missing index.
 *		This function doesn't pop its arguments.
 */
int kf_ckranget(frame *f)
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
FUNCDEF("sum", kf_sum, pt_sum, 0)
# else
char pt_sum[] = { C_STATIC | C_ELLIPSIS, 0, 1, 0, 7, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->sum()
 * DESCRIPTION:	perform a summand operation
 */
int kf_sum(frame *f, int nargs)
{
    char buffer[12], *num;
    string *s;
    array *a;
    value *v, *e1, *e2;
    int i, type, vtype, nonint;
    long size;
    ssizet len;
    Int result;
    long isize;

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
		len = (v->u.number - v[1].u.number + 1);
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
FUNCDEF("status", kf_status_idx, pt_status_idx, 0)
# else
char pt_status_idx[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_INT };

/*
 * NAME:	kfun->status_idx()
 * DESCRIPTION:	return status()[idx]
 */
int kf_status_idx(frame *f)
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
FUNCDEF("status", kf_statuso_idx, pt_statuso_idx, 0)
# else
char pt_statuso_idx[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_INT };

/*
 * NAME:	kfun->statuso_idx()
 * DESCRIPTION:	return status(obj)[idx]
 */
int kf_statuso_idx(frame *f)
{
    uindex n;

    n = 0;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp[1].u.number != 0) {
	    error("Index on bad type");
	}
	i_add_ticks(f, 6);
	if (!conf_statusi(f, f->sp->u.number, &f->sp[1])) {
	    error("Index out of range");
	}
	f->sp++;
	return 0;

    case T_OBJECT:
	n = f->sp[1].oindex;
	break;

    case T_LWOBJECT:
	if (f->sp[1].u.array->elts[0].type == T_OBJECT) {
	    n = f->sp[1].u.array->elts[0].oindex;
	    arr_del(f->sp[1].u.array);
	    f->sp[1] = nil_value;
	} else {
	    /* no user-visible parts within (right?) */
	    error("Index on bad type");
	}
	break;

    default:
	kf_argerror(KF_STATUSO_IDX, 1);
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
FUNCDEF("call_trace", kf_calltr_idx, pt_calltr_idx, 0)
# else
char pt_calltr_idx[] = { C_STATIC, 1, 0, 0, 7, T_MIXED | (1 << REFSHIFT),
			 T_INT };

/*
 * NAME:	kfun->calltr_idx()
 * DESCRIPTION:	return call_trace()[idx]
 */
int kf_calltr_idx(frame *f)
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
FUNCDEF("nil", kf_nil, pt_nil, 0)
# else
char pt_nil[] = { C_STATIC, 0, 0, 0, 6, T_NIL };

/*
 * NAME:	kfun->nil()
 * DESCRIPTION:	return nil
 */
int kf_nil(frame *f)
{
    *--f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<-", kf_instanceof, pt_instanceof, 0)
# else
char pt_instanceof[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_OBJECT, T_INT };

/*
 * NAME:	kfun->instanceof()
 * DESCRIPTION:	instanceof
 */
int kf_instanceof(frame *f)
{
    uindex oindex;
    value *elts;
    int instance;

    oindex = 0;

    switch (f->sp[1].type) {
    case T_OBJECT:
	oindex = f->sp[1].oindex;
	break;

    case T_LWOBJECT:
	elts = d_get_elts(f->sp[1].u.array);
	if (elts->type != T_OBJECT) {
	    /*
	     * builtin types can only be an instance of their own type
	     */
	    instance = (strcmp(o_builtin_name(elts->u.number),
			       i_classname(f, f->sp->u.number)) == 0);
	    f->sp++;
	    arr_del(f->sp->u.array);
	    PUT_INTVAL(f->sp, instance);
	    return 0;
	}
	oindex = elts->oindex;
	arr_del(f->sp[1].u.array);
	break;

    default:
	kf_argerror(KF_INSTANCEOF, 1);
    }
    instance = i_instanceof(f, oindex, f->sp->u.number);
    f->sp++;
    PUT_INTVAL(f->sp, instance);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("=", kf_store_aggr, pt_store_aggr, 0)
# else
char pt_store_aggr[] = { C_STATIC, 2, 0, 0, 8, T_MIXED,
			 T_MIXED | (1 << REFSHIFT), T_INT };

/*
 * NAME:	kfun->store_aggr()
 * DESCRIPTION:	store array elements in lvalues on the stack, which will also
 *		be popped
 */
int kf_store_aggr(frame *f)
{
    int n;
    value *v;
    value val;

    n = (f->sp++)->u.number;
    if (f->sp[0].type != T_ARRAY || f->sp[0].u.array->size != n) {
	kf_argerror(KF_STORE_AGGR, 2);
    }

    if (ec_push(NULL)) {
	i_del_value(&val);
	error(NULL);
    }
    val = *f->sp++;
    for (v = d_get_elts(val.u.array) + n; n > 0; --n) {
	i_push_value(f, --v);
	i_store(f);
	i_del_value(v);
    }
    *--f->sp = val;
    ec_pop();

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_float, pt_add_float, 0)
# else
char pt_add_float[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->add_float()
 * DESCRIPTION:	float + float
 */
int kf_add_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    flt_add(&f1, &f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_float_string, pt_add_float_string, 0)
# else
char pt_add_float_string[] = { C_STATIC, 2, 0, 0, 8, T_STRING, T_FLOAT,
			       T_STRING };

/*
 * NAME:	kfun->add_float_string()
 * DESCRIPTION:	float + string
 */
int kf_add_float_string(frame *f)
{
    char buffer[18];
    xfloat flt;
    string *str;
    long l;

    i_add_ticks(f, 3);
    GET_FLT(&f->sp[1], flt);
    flt_ftoa(&flt, buffer);
    str = str_new((char *) NULL, (l=strlen(buffer)) + f->sp->u.string->len);
    strcpy(str->text, buffer);
    memcpy(str->text + l, f->sp->u.string->text, f->sp->u.string->len);
    str_del(f->sp->u.string);
    f->sp++;
    PUT_STRVAL(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_int_string, pt_add_int_string, 0)
# else
char pt_add_int_string[] = { C_STATIC, 2, 0, 0, 8, T_STRING, T_INT, T_STRING };

/*
 * NAME:	kfun->add_int_string()
 * DESCRIPTION:	int + string
 */
int kf_add_int_string(frame *f)
{
    char buffer[12], *num;
    string *str;
    long l;

    i_add_ticks(f, 2);
    num = kf_itoa(f->sp[1].u.number, buffer);
    str = str_new((char *) NULL, (l=strlen(num)) + f->sp->u.string->len);
    strcpy(str->text, num);
    memcpy(str->text + l, f->sp->u.string->text, f->sp->u.string->len);
    str_del(f->sp->u.string);
    f->sp++;
    PUT_STRVAL(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_string, pt_add_string, 0)
# else
char pt_add_string[] = { C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING, T_STRING };

/*
 * NAME:	kfun->add_string()
 * DESCRIPTION:	string + string
 */
int kf_add_string(frame *f)
{
    string *str;

    i_add_ticks(f, 2);
    str = str_add(f->sp[1].u.string, f->sp->u.string);
    str_del(f->sp->u.string);
    f->sp++;
    str_del(f->sp->u.string);
    PUT_STR(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_string_float, pt_add_string_float, 0)
# else
char pt_add_string_float[] = { C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING,
			       T_FLOAT };

/*
 * NAME:	kfun->add_string_float()
 * DESCRIPTION:	string + float
 */
int kf_add_string_float(frame *f)
{
    char buffer[18];
    xfloat flt;
    string *str;

    i_add_ticks(f, 3);
    GET_FLT(f->sp, flt);
    flt_ftoa(&flt, buffer);
    f->sp++;
    str = str_new((char *) NULL, f->sp->u.string->len + (long) strlen(buffer));
    memcpy(str->text, f->sp->u.string->text, f->sp->u.string->len);
    strcpy(str->text + f->sp->u.string->len, buffer);
    str_del(f->sp->u.string);
    PUT_STR(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_string_int, pt_add_string_int, 0)
# else
char pt_add_string_int[] = { C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING, T_INT };

/*
 * NAME:	kfun->add_string_int()
 * DESCRIPTION:	string + int
 */
int kf_add_string_int(frame *f)
{
    char buffer[12], *num;
    string *str;

    i_add_ticks(f, 2);
    num = kf_itoa(f->sp->u.number, buffer);
    f->sp++;
    str = str_new((char *) NULL, f->sp->u.string->len + (long) strlen(num));
    memcpy(str->text, f->sp->u.string->text, f->sp->u.string->len);
    strcpy(str->text + f->sp->u.string->len, num);
    str_del(f->sp->u.string);
    PUT_STR(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1_float, pt_add1_float, 0)
# else
char pt_add1_float[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->add1_float()
 * DESCRIPTION:	float++
 */
int kf_add1_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f1);
    FLT_ONE(f2.high, f2.low);
    flt_add(&f1, &f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div_float, pt_div_float, 0)
# else
char pt_div_float[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->div_float()
 * DESCRIPTION:	float / float
 */
int kf_div_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    flt_div(&f1, &f2);
    PUT_FLT(f->sp, f1);
    return 0;

}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq_float, pt_eq_float, 0)
# else
char pt_eq_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->eq_float()
 * DESCRIPTION:	float == float
 */
int kf_eq_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) == 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq_string, pt_eq_string, 0)
# else
char pt_eq_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * NAME:	kfun->eq_string()
 * DESCRIPTION:	string == string
 */
int kf_eq_string(frame *f)
{
    bool flag;

    i_add_ticks(f, 2);
    flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) == 0);
    str_del(f->sp->u.string);
    f->sp++;
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_float, pt_ge_float, 0)
# else
char pt_ge_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->ge_float()
 * DESCRIPTION:	float >= float
 */
int kf_ge_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) >= 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_string, pt_ge_string, 0)
# else
char pt_ge_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * NAME:	kfun->ge_string()
 * DESCRIPTION:	string >= string
 */
int kf_ge_string(frame *f)
{
    bool flag;

    i_add_ticks(f, 2);
    flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) >= 0);
    str_del(f->sp->u.string);
    f->sp++;
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_float, pt_gt_float, 0)
# else
char pt_gt_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->add_float()
 * DESCRIPTION:	float > float
 */
int kf_gt_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) > 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_string, pt_gt_string, 0)
# else
char pt_gt_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * NAME:	kfun->add_string()
 * DESCRIPTION:	string > string
 */
int kf_gt_string(frame *f)
{
    bool flag;

    i_add_ticks(f, 2);
    flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) > 0);
    str_del(f->sp->u.string);
    f->sp++;
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_float, pt_le_float, 0)
# else
char pt_le_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->le_float()
 * DESCRIPTION:	float <= float
 */
int kf_le_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) <= 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_string, pt_le_string, 0)
# else
char pt_le_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * NAME:	kfun->le_float()
 * DESCRIPTION:	string <= string
 */
int kf_le_string(frame *f)
{
    bool flag;

    i_add_ticks(f, 2);
    flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) <= 0);
    str_del(f->sp->u.string);
    f->sp++;
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_float, pt_lt_float, 0)
# else
char pt_lt_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->lt_float()
 * DESCRIPTION:	float < float
 */
int kf_lt_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) < 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_string, pt_lt_string, 0)
# else
char pt_lt_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * NAME:	kfun->lt_string()
 * DESCRIPTION:	string < string
 */
int kf_lt_string(frame *f)
{
    bool flag;

    i_add_ticks(f, 2);
    flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) < 0);
    str_del(f->sp->u.string);
    f->sp++;
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult_float, pt_mult_float, 0)
# else
char pt_mult_float[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->mult_float()
 * DESCRIPTION:	float * float
 */
int kf_mult_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    flt_mult(&f1, &f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne_float, pt_ne_float, 0)
# else
char pt_ne_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->ne_float()
 * DESCRIPTION:	float != float
 */
int kf_ne_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (flt_cmp(&f1, &f2) != 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne_string, pt_ne_string, 0)
# else
char pt_ne_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * NAME:	kfun->ne_string()
 * DESCRIPTION:	string != string
 */
int kf_ne_string(frame *f)
{
    bool flag;

    i_add_ticks(f, 2);
    flag = (str_cmp(f->sp[1].u.string, f->sp->u.string) != 0);
    str_del(f->sp->u.string);
    f->sp++;
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_not_float, pt_not_float, 0)
# else
char pt_not_float[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_FLOAT };

/*
 * NAME:	kfun->not_float()
 * DESCRIPTION:	! float
 */
int kf_not_float(frame *f)
{
    PUT_INTVAL(f->sp, VFLT_ISZERO(f->sp));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_not_string, pt_not_string, 0)
# else
char pt_not_string[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_STRING };

/*
 * NAME:	kfun->not_string()
 * DESCRIPTION:	! string
 */
int kf_not_string(frame *f)
{
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, FALSE);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub_float, pt_sub_float, 0)
# else
char pt_sub_float[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->sub_float()
 * DESCRIPTION:	float - float
 */
int kf_sub_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    flt_sub(&f1, &f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1_float, pt_sub1_float, 0)
# else
char pt_sub1_float[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->sub1_float()
 * DESCRIPTION:	float--
 */
int kf_sub1_float(frame *f)
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f1);
    FLT_ONE(f2.high, f2.low);
    flt_sub(&f1, &f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst_float, pt_tst_float, 0)
# else
char pt_tst_float[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_FLOAT };

/*
 * NAME:	kfun->tst_float()
 * DESCRIPTION:	!! float
 */
int kf_tst_float(frame *f)
{
    PUT_INTVAL(f->sp, !VFLT_ISZERO(f->sp));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst_string, pt_tst_string, 0)
# else
char pt_tst_string[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_STRING };

/*
 * NAME:	kfun->tst_string()
 * DESCRIPTION:	!! string
 */
int kf_tst_string(frame *f)
{
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, TRUE);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_umin_float, pt_umin_float, 0)
# else
char pt_umin_float[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->umin_float()
 * DESCRIPTION:	- float
 */
int kf_umin_float(frame *f)
{
    xfloat flt;

    i_add_ticks(f, 1);
    if (!VFLT_ISZERO(f->sp)) {
	GET_FLT(f->sp, flt);
	FLT_NEG(flt.high, flt.low);
	PUT_FLT(f->sp, flt);
    }
    return 0;
}
# endif
