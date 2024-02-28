/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

/*
 * convert a LPCint to a string
 */
static char *kf_itoa(LPCint i, char *buffer)
{
    LPCuint u;
    char *p;

    u = (i >= 0) ? i : -i;

    p = buffer + LPCINT_BUFFER - 1;
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

char pt_unused[] = { C_STATIC, 0, 0, 0, 6, T_MIXED };

/*
 * unused kfun
 */
int kf_unused(Frame *f, int nargs, KFun *kf)
{
    UNREFERENCED_PARAMETER(f);
    UNREFERENCED_PARAMETER(nargs);
    UNREFERENCED_PARAMETER(kf);

    return 1;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add, pt_add, 0)
# else
char pt_add[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * value + value
 */
int kf_add(Frame *f, int n, KFun *kf)
{
    String *str;
    Array *a;
    char *num, buffer[FLOAT_BUFFER];
    Float f1, f2;
    long l;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	switch (f->sp->type) {
	case T_INT:
	    PUT_INT(&f->sp[1], f->sp[1].number + f->sp->number);
	    f->sp++;
	    return 0;

	case T_STRING:
	    f->addTicks(2);
	    num = kf_itoa(f->sp[1].number, buffer);
	    str = String::create((char *) NULL,
				 (l=(long) strlen(num)) + f->sp->string->len);
	    strcpy(str->text, num);
	    memcpy(str->text + l, f->sp->string->text, f->sp->string->len);
	    f->sp->string->del();
	    f->sp++;
	    PUT_STRVAL(f->sp, str);
	    return 0;
	}
	break;

    case T_FLOAT:
	f->addTicks(1);
	switch (f->sp->type) {
	case T_FLOAT:
	    GET_FLT(f->sp, f2);
	    f->sp++;
	    GET_FLT(f->sp, f1);
	    f1.add(f2);
	    PUT_FLT(f->sp, f1);
	    return 0;

	case T_STRING:
	    f->addTicks(2);
	    GET_FLT(&f->sp[1], f1);
	    f1.ftoa(buffer);
	    str = String::create((char *) NULL,
			    (l=(long) strlen(buffer)) + f->sp->string->len);
	    strcpy(str->text, buffer);
	    memcpy(str->text + l, f->sp->string->text, f->sp->string->len);
	    f->sp->string->del();
	    f->sp++;
	    PUT_STRVAL(f->sp, str);
	    return 0;
	}
	break;

    case T_STRING:
	f->addTicks(2);
	switch (f->sp->type) {
	case T_INT:
	    num = kf_itoa(f->sp->number, buffer);
	    f->sp++;
	    str = String::create((char *) NULL,
				 f->sp->string->len + (long) strlen(num));
	    memcpy(str->text, f->sp->string->text, f->sp->string->len);
	    strcpy(str->text + f->sp->string->len, num);
	    f->sp->string->del();
	    PUT_STR(f->sp, str);
	    return 0;

	case T_FLOAT:
	    f->addTicks(1);
	    GET_FLT(f->sp, f2);
	    f2.ftoa(buffer);
	    f->sp++;
	    str = String::create((char *) NULL,
				 f->sp->string->len + (long) strlen(buffer));
	    memcpy(str->text, f->sp->string->text, f->sp->string->len);
	    strcpy(str->text + f->sp->string->len, buffer);
	    f->sp->string->del();
	    PUT_STR(f->sp, str);
	    return 0;

	case T_STRING:
	    str = f->sp[1].string->add(f->sp->string);
	    f->sp->string->del();
	    f->sp++;
	    f->sp->string->del();
	    PUT_STR(f->sp, str);
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    f->addTicks((LPCint) f->sp[1].array->size + f->sp->array->size);
	    a = f->sp[1].array->add(f->data, f->sp->array);
	    f->sp->array->del();
	    f->sp++;
	    f->sp->array->del();
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    case T_MAPPING:
	if (f->sp->type == T_MAPPING) {
	    f->addTicks((LPCint) f->sp[1].array->size + f->sp->array->size);
	    a = f->sp[1].array->add(f->data, f->sp->array);
	    f->sp->array->del();
	    f->sp++;
	    f->sp->array->del();
	    PUT_MAP(f->sp, a);
	    return 0;
	}
	break;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->binary(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    kf->argError(2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_int, pt_add_int, 0)
# else
char pt_add_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int + int
 */
int kf_add_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], f->sp[1].number + f->sp->number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1, pt_add1, 0)
# else
char pt_add1[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * value++
 */
int kf_add1(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);

    if (f->sp->type == T_INT) {
	PUT_INT(f->sp, f->sp->number + 1);
    } else if (f->sp->type == T_FLOAT) {
	f->addTicks(1);
	GET_FLT(f->sp, f1);
	f2.initOne();
	f1.add(f2);
	PUT_FLT(f->sp, f1);
    } else if (f->sp->type == T_LWOBJECT &&
	       f->sp->array->elts[0].type == T_OBJECT) {
	kf->unary(f);
    } else {
	kf->argError(1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1_int, pt_add1_int, 0)
# else
char pt_add1_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * int++
 */
int kf_add1_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(f->sp, f->sp->number + 1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("&", kf_and, pt_and, 0)
# else
char pt_and[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * value & value
 */
int kf_and(Frame *f, int n, KFun *kf)
{
    Array *a;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type == T_INT) {
	    PUT_INT(&f->sp[1], f->sp[1].number & f->sp->number);
	    f->sp++;
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    f->addTicks((LPCint) f->sp[1].array->size + f->sp->array->size);
	    a = f->sp[1].array->intersect(f->data, f->sp->array);
	    f->sp->array->del();
	    f->sp++;
	    f->sp->array->del();
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    case T_MAPPING:
	if (f->sp->type == T_ARRAY) {
	    f->addTicks((LPCint) f->sp[1].array->size + f->sp->array->size);
	    a = f->sp[1].array->intersect(f->data, f->sp->array);
	    f->sp->array->del();
	    f->sp++;
	    PUT_MAP(f->sp, a);
	    return 0;
	}
	break;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->binary(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    kf->argError(2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("&", kf_and_int, pt_and_int, 0)
# else
char pt_and_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int & int
 */
int kf_and_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], f->sp[1].number & f->sp->number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div, pt_div, 0)
# else
char pt_div[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * mixed / mixed
 */
int kf_div(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	PUT_INT(&f->sp[1], Frame::div(f->sp[1].number, f->sp->number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf->argError(2);
	}
	f->addTicks(1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	f1.div(f2);
	PUT_FLT(f->sp, f1);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->binary(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div_int, pt_div_int, 0)
# else
char pt_div_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int / int
 */
int kf_div_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], Frame::div(f->sp[1].number, f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq, pt_eq, 0)
# else
char pt_eq[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * value == value
 */
int kf_eq(Frame *f, int n, KFun *kf)
{
    bool flag;
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp[1].type != f->sp->type) {
	f->pop(2);
	PUSH_INTVAL(f, FALSE);
	return 0;
    }

    switch (f->sp->type) {
    case T_NIL:
	f->sp++;
	PUT_INTVAL(f->sp, TRUE);
	break;

    case T_INT:
	PUT_INT(&f->sp[1], (f->sp[1].number == f->sp->number));
	f->sp++;
	break;

    case T_FLOAT:
	f->addTicks(1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (f1.cmp(f2) == 0));
	break;

    case T_STRING:
	f->addTicks(2);
	flag = (f->sp[1].string->cmp(f->sp->string) == 0);
	f->sp->string->del();
	f->sp++;
	f->sp->string->del();
	PUT_INTVAL(f->sp, flag);
	break;

    case T_OBJECT:
	PUT_INTVAL(&f->sp[1], (f->sp[1].oindex == f->sp->oindex));
	f->sp++;
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	flag = (f->sp[1].array == f->sp->array);
	f->sp->array->del();
	f->sp++;
	f->sp->array->del();
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
 * int == int
 */
int kf_eq_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], (f->sp[1].number == f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge, pt_ge, 0)
# else
char pt_ge[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * value >= value
 */
int kf_ge(Frame *f, int n, KFun *kf)
{
    Float f1, f2;
    bool flag;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	PUT_INT(&f->sp[1], (f->sp[1].number >= f->sp->number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf->argError(2);
	}
	f->addTicks(1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (f1.cmp(f2) >= 0));
	return 0;

    case T_STRING:
	if (f->sp->type != T_STRING) {
	    kf->argError(2);
	}
	f->addTicks(2);
	flag = (f->sp[1].string->cmp(f->sp->string) >= 0);
	f->sp->string->del();
	f->sp++;
	f->sp->string->del();
	PUT_INTVAL(f->sp, flag);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->compare(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_int, pt_ge_int, 0)
# else
char pt_ge_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int >= int
 */
int kf_ge_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], (f->sp[1].number >= f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt, pt_gt, 0)
# else
char pt_gt[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * value > value
 */
int kf_gt(Frame *f, int n, KFun *kf)
{
    Float f1, f2;
    bool flag;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	PUT_INT(&f->sp[1], (f->sp[1].number > f->sp->number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf->argError(2);
	}
	f->addTicks(1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (f1.cmp(f2) > 0));
	return 0;

    case T_STRING:
	if (f->sp->type != T_STRING) {
	    kf->argError(2);
	}
	f->addTicks(2);
	flag = (f->sp[1].string->cmp(f->sp->string) > 0);
	f->sp->string->del();
	f->sp++;
	f->sp->string->del();
	PUT_INTVAL(f->sp, flag);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->compare(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_int, pt_gt_int, 0)
# else
char pt_gt_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int > int
 */
int kf_gt_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], (f->sp[1].number > f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le, pt_le, 0)
# else
char pt_le[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * value <= value
 */
int kf_le(Frame *f, int n, KFun *kf)
{
    Float f1, f2;
    bool flag;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	PUT_INT(&f->sp[1], (f->sp[1].number <= f->sp->number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf->argError(2);
	}
	f->addTicks(1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (f1.cmp(f2) <= 0));
	return 0;

    case T_STRING:
	if (f->sp->type != T_STRING) {
	    kf->argError(2);
	}
	f->addTicks(2);
	flag = (f->sp[1].string->cmp(f->sp->string) <= 0);
	f->sp->string->del();
	f->sp++;
	f->sp->string->del();
	PUT_INTVAL(f->sp, flag);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->compare(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_int, pt_le_int, 0)
# else
char pt_le_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int <= int
 */
int kf_le_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], (f->sp[1].number <= f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<<", kf_lshift, pt_lshift, 0)
# else
char pt_lshift[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * int << int
 */
int kf_lshift(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);

    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].array->elts[0].type == T_OBJECT) {
	kf->binary(f);
	return 0;
    }
    if (f->sp[1].type != T_INT) {
	kf->argError(1);
    }
    if (f->sp->type != T_INT) {
	kf->argError(2);
    }
    PUT_INT(&f->sp[1], Frame::lshift(f->sp[1].number, f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<<", kf_lshift_int, pt_lshift_int, 0)
# else
char pt_lshift_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int << int
 */
int kf_lshift_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], Frame::lshift(f->sp[1].number, f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt, pt_lt, 0)
# else
char pt_lt[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * value < value
 */
int kf_lt(Frame *f, int n, KFun *kf)
{
    Float f1, f2;
    bool flag;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	PUT_INT(&f->sp[1], (f->sp[1].number < f->sp->number));
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf->argError(2);
	}
	f->addTicks(1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (f1.cmp(f2) < 0));
	return 0;

    case T_STRING:
	if (f->sp->type != T_STRING) {
	    kf->argError(2);
	}
	f->addTicks(2);
	flag = (f->sp[1].string->cmp(f->sp->string) < 0);
	f->sp->string->del();
	f->sp++;
	f->sp->string->del();
	PUT_INTVAL(f->sp, flag);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->compare(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_int, pt_lt_int, 0)
# else
char pt_lt_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int < int
 */
int kf_lt_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], (f->sp[1].number < f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("%", kf_mod, pt_mod, 0)
# else
char pt_mod[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * int % int
 */
int kf_mod(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);

    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].array->elts[0].type == T_OBJECT) {
	kf->binary(f);
	return 0;
    }
    if (f->sp[1].type != T_INT) {
	kf->argError(1);
    }
    if (f->sp->type != T_INT) {
	kf->argError(2);
    }
    PUT_INT(&f->sp[1], Frame::mod(f->sp[1].number, f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("%", kf_mod_int, pt_mod_int, 0)
# else
char pt_mod_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int % int
 */
int kf_mod_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], Frame::mod(f->sp[1].number, f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult, pt_mult, 0)
# else
char pt_mult[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * mixed * mixed
 */
int kf_mult(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	PUT_INT(&f->sp[1], f->sp[1].number * f->sp->number);
	f->sp++;
	return 0;

    case T_FLOAT:
	if (f->sp->type != T_FLOAT) {
	    kf->argError(2);
	}
	f->addTicks(1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	f1.mult(f2);
	PUT_FLT(f->sp, f1);
	return 0;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->binary(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult_int, pt_mult_int, 0)
# else
char pt_mult_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int * int
 */
int kf_mult_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], f->sp[1].number * f->sp->number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne, pt_ne, 0)
# else
char pt_ne[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_MIXED };

/*
 * value != value
 */
int kf_ne(Frame *f, int n, KFun *kf)
{
    bool flag;
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp[1].type != f->sp->type) {
	f->pop(2);
	PUSH_INTVAL(f, TRUE);
	return 0;
    }

    switch (f->sp->type) {
    case T_NIL:
	f->sp++;
	PUT_INTVAL(f->sp, FALSE);
	break;

    case T_INT:
	PUT_INT(&f->sp[1], (f->sp[1].number != f->sp->number));
	f->sp++;
	break;

    case T_FLOAT:
	f->addTicks(1);
	GET_FLT(f->sp, f2);
	f->sp++;
	GET_FLT(f->sp, f1);
	PUT_INTVAL(f->sp, (f1.cmp(f2) != 0));
	break;

    case T_STRING:
	f->addTicks(2);
	flag = (f->sp[1].string->cmp(f->sp->string) != 0);
	f->sp->string->del();
	f->sp++;
	f->sp->string->del();
	PUT_INTVAL(f->sp, flag);
	break;

    case T_OBJECT:
	PUT_INTVAL(&f->sp[1], (f->sp[1].oindex != f->sp->oindex));
	f->sp++;
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	flag = (f->sp[1].array != f->sp->array);
	f->sp->array->del();
	f->sp++;
	f->sp->array->del();
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
 * int != int
 */
int kf_ne_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], (f->sp[1].number != f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("~", kf_neg, pt_neg, 0)
# else
char pt_neg[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * ~ int
 */
int kf_neg(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);

    if (f->sp->type == T_LWOBJECT &&
	f->sp->array->elts[0].type == T_OBJECT) {
	kf->unary(f);
	return 0;
    }
    if (f->sp->type != T_INT) {
	kf->argError(1);
    }
    PUT_INT(f->sp, ~f->sp->number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("~", kf_neg_int, pt_neg_int, 0)
# else
char pt_neg_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * ~ int
 */
int kf_neg_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(f->sp, ~f->sp->number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!", kf_not, pt_not, 0)
# else
char pt_not[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * ! value
 */
int kf_not(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    switch (f->sp->type) {
    case T_NIL:
	PUT_INTVAL(f->sp, TRUE);
	return 0;

    case T_INT:
	PUT_INT(f->sp, !f->sp->number);
	return 0;

    case T_FLOAT:
	PUT_INTVAL(f->sp, VFLT_ISZERO(f->sp));
	return 0;

    case T_STRING:
	f->sp->string->del();
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	f->sp->array->del();
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
 * ! int
 */
int kf_not_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(f->sp, !f->sp->number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or, pt_or, 0)
# else
char pt_or[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * value | value
 */
int kf_or(Frame *f, int n, KFun *kf)
{
    Array *a;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type == T_INT) {
	    PUT_INT(&f->sp[1], f->sp[1].number | f->sp->number);
	    f->sp++;
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    f->addTicks((LPCint) f->sp[1].array->size + f->sp->array->size);
	    a = f->sp[1].array->setAdd(f->data, f->sp->array);
	    f->sp->array->del();
	    f->sp++;
	    f->sp->array->del();
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->binary(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    kf->argError(2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("|", kf_or_int, pt_or_int, 0)
# else
char pt_or_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int | int
 */
int kf_or_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], f->sp[1].number | f->sp->number);
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
 * value [ int .. int ]
 */
int kf_rangeft(Frame *f, int n, KFun *kf)
{
    String *str;
    Array *a;

    UNREFERENCED_PARAMETER(n);

    if (f->sp[2].type == T_MAPPING) {
	a = dynamic_cast<Mapping *> (f->sp[2].array)->range(f->data, &f->sp[1],
							    f->sp);
	(f->sp++)->del();
	(f->sp++)->del();
	f->addTicks(f->sp->array->size);
	f->sp->array->del();
	PUT_ARR(f->sp, a);

	return 0;
    }
    if (f->sp[2].type == T_LWOBJECT &&
	f->sp[2].array->elts[0].type == T_OBJECT) {
	if (VAL_NIL(f->sp + 1)) {
	    kf->argError(2);
	}
	if (VAL_NIL(f->sp)) {
	    kf->argError(3);
	}
	kf->ternary(f);

	return 0;
    }

    switch (f->sp[2].type) {
    case T_STRING:
	if (f->sp[1].type != T_INT) {
	    kf->argError(2);
	}
	if (f->sp->type != T_INT) {
	    kf->argError(3);
	}
	f->addTicks(2);
	str = f->sp[2].string->range(f->sp[1].number, f->sp->number);
	f->sp += 2;
	f->sp->string->del();
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	if (f->sp[1].type != T_INT) {
	    kf->argError(2);
	}
	if (f->sp->type != T_INT) {
	    kf->argError(3);
	}
	a = f->sp[2].array->range(f->data, f->sp[1].number, f->sp->number);
	f->addTicks(a->size);
	f->sp += 2;
	f->sp->array->del();
	PUT_ARR(f->sp, a);
	break;

    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_rangef, pt_rangef, 0)
# else
char pt_rangef[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * value [ int .. ]
 */
int kf_rangef(Frame *f, int n, KFun *kf)
{
    String *str;
    Array *a;

    UNREFERENCED_PARAMETER(n);

    if (f->sp[1].type == T_MAPPING) {
	a = dynamic_cast<Mapping *> (f->sp[1].array)->range(f->data, f->sp,
							    (Value *) NULL);
	(f->sp++)->del();
	f->addTicks(f->sp->array->size);
	f->sp->array->del();
	PUT_MAP(f->sp, a);

	return 0;
    }
    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].array->elts[0].type == T_OBJECT) {
	if (VAL_NIL(f->sp)) {
	    kf->argError(2);
	}
	*--f->sp = nil;
	kf->ternary(f);

	return 0;
    }

    switch (f->sp[1].type) {
    case T_STRING:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	f->addTicks(2);
	str = f->sp[1].string->range(f->sp->number, f->sp[1].string->len - 1L);
	f->sp++;
	f->sp->string->del();
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	a = f->sp[1].array->range(f->data, f->sp->number,
				  f->sp[1].array->size - 1);
	f->addTicks(a->size);
	f->sp++;
	f->sp->array->del();
	PUT_ARR(f->sp, a);
	break;

    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_ranget, pt_ranget, 0)
# else
char pt_ranget[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * value [ .. int ]
 */
int kf_ranget(Frame *f, int n, KFun *kf)
{
    String *str;
    Array *a;

    UNREFERENCED_PARAMETER(n);

    if (f->sp[1].type == T_MAPPING) {
	a = dynamic_cast<Mapping *> (f->sp[1].array)->range(f->data,
							    (Value *) NULL,
							    f->sp);
	(f->sp++)->del();
	f->addTicks(f->sp->array->size);
	f->sp->array->del();
	PUT_MAP(f->sp, a);

	return 0;
    }
    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].array->elts[0].type == T_OBJECT) {
	if (VAL_NIL(f->sp)) {
	    kf->argError(2);
	}
	--f->sp;
	f->sp[0] = f->sp[1];
	f->sp[1] = nil;
	kf->ternary(f);

	return 0;
    }

    switch (f->sp[1].type) {
    case T_STRING:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	f->addTicks(2);
	str = f->sp[1].string->range(0, f->sp->number);
	f->sp++;
	f->sp->string->del();
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	if (f->sp->type != T_INT) {
	    kf->argError(2);
	}
	a = f->sp[1].array->range(f->data, 0, f->sp->number);
	f->addTicks(a->size);
	f->sp++;
	f->sp->array->del();
	PUT_ARR(f->sp, a);
	break;

    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_range, pt_range, 0)
# else
char pt_range[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * value [ .. ]
 */
int kf_range(Frame *f, int n, KFun *kf)
{
    String *str;
    Array *a;

    UNREFERENCED_PARAMETER(n);

    if (f->sp->type == T_MAPPING) {
	a = dynamic_cast<Mapping *> (f->sp->array)->range(f->data,
							  (Value *) NULL,
							  (Value *) NULL);
	f->addTicks(f->sp->array->size);
	f->sp->array->del();
	PUT_MAP(f->sp, a);

	return 0;
    }
    if (f->sp->type == T_LWOBJECT &&
	f->sp->array->elts[0].type == T_OBJECT) {
	*--f->sp = nil;
	*--f->sp = nil;
	kf->ternary(f);

	return 0;
    }

    switch (f->sp->type) {
    case T_STRING:
	f->addTicks(2);
	str = f->sp->string->range(0, f->sp->string->len - 1);
	f->sp->string->del();
	PUT_STR(f->sp, str);
	break;

    case T_ARRAY:
	a = f->sp->array->range(f->data, 0, f->sp->array->size - 1);
	f->addTicks(a->size);
	f->sp->array->del();
	PUT_ARR(f->sp, a);
	break;

    default:
	kf->argError(1);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">>", kf_rshift, pt_rshift, 0)
# else
char pt_rshift[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * int >> int
 */
int kf_rshift(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);

    if (f->sp[1].type == T_LWOBJECT &&
	f->sp[1].array->elts[0].type == T_OBJECT) {
	kf->binary(f);
	return 0;
    }
    if (f->sp[1].type != T_INT) {
	kf->argError(1);
    }
    if (f->sp->type != T_INT) {
	kf->argError(2);
    }
    PUT_INT(&f->sp[1], Frame::rshift(f->sp[1].number, f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">>", kf_rshift_int, pt_rshift_int, 0)
# else
char pt_rshift_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int >> int
 */
int kf_rshift_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], Frame::rshift(f->sp[1].number, f->sp->number));
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub, pt_sub, 0)
# else
char pt_sub[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * value - value
 */
int kf_sub(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type == T_INT) {
	    PUT_INT(&f->sp[1], f->sp[1].number - f->sp->number);
	    f->sp++;
	    return 0;
	}
	break;

    case T_FLOAT:
	if (f->sp->type == T_FLOAT) {
	    f->addTicks(1);
	    GET_FLT(f->sp, f2);
	    f->sp++;
	    GET_FLT(f->sp, f1);
	    f1.sub(f2);
	    PUT_FLT(f->sp, f1);
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    Array *a;

	    f->addTicks((LPCint) f->sp[1].array->size + f->sp->array->size);
	    a = f->sp[1].array->sub(f->data, f->sp->array);
	    f->sp->array->del();
	    f->sp++;
	    f->sp->array->del();
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    case T_MAPPING:
	if (f->sp->type == T_ARRAY) {
	    Array *a;

	    f->addTicks((LPCint) f->sp[1].array->size + f->sp->array->size);
	    a = f->sp[1].array->sub(f->data, f->sp->array);
	    f->sp->array->del();
	    f->sp++;
	    f->sp->array->del();
	    PUT_MAP(f->sp, a);
	    return 0;
	}
	break;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->binary(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    kf->argError(2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub_int, pt_sub_int, 0)
# else
char pt_sub_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int - int
 */
int kf_sub_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], f->sp[1].number - f->sp->number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1, pt_sub1, 0)
# else
char pt_sub1[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * value--
 */
int kf_sub1(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);

    if (f->sp->type == T_INT) {
	PUT_INT(f->sp, f->sp->number - 1);
    } else if (f->sp->type == T_FLOAT) {
	f->addTicks(1);
	GET_FLT(f->sp, f1);
	f2.initOne();
	f1.sub(f2);
	PUT_FLT(f->sp, f1);
    } else if (f->sp->type == T_LWOBJECT &&
	       f->sp->array->elts[0].type == T_OBJECT) {
	kf->unary(f);
    } else {
	kf->argError(1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1_int, pt_sub1_int, 0)
# else
char pt_sub1_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * int--
 */
int kf_sub1_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(f->sp, f->sp->number - 1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(float)", kf_tofloat, pt_tofloat, 0)
# else
char pt_tofloat[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_MIXED };

/*
 * convert to float
 */
int kf_tofloat(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->toFloat(&flt);
    PUSH_FLTVAL(f, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(int)", kf_toint, pt_toint, 0)
# else
char pt_toint[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * convert to integer
 */
int kf_toint(Frame *f, int n, KFun *kf)
{
    LPCint num;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    num = f->toInt();
    PUSH_INTVAL(f, num);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst, pt_tst, 0)
# else
char pt_tst[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * !! value
 */
int kf_tst(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    switch (f->sp->type) {
    case T_NIL:
	PUT_INTVAL(f->sp, FALSE);
	return 0;

    case T_INT:
	PUT_INT(f->sp, (f->sp->number != 0));
	return 0;

    case T_FLOAT:
	PUT_INTVAL(f->sp, !VFLT_ISZERO(f->sp));
	return 0;

    case T_STRING:
	f->sp->string->del();
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	f->sp->array->del();
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
 * !! int
 */
int kf_tst_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(f->sp, (f->sp->number != 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_umin, pt_umin, 0)
# else
char pt_umin[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_MIXED };

/*
 * - mixed
 */
int kf_umin(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp->type) {
    case T_INT:
	PUT_INT(f->sp, -f->sp->number);
	return 0;

    case T_FLOAT:
	f->addTicks(1);
	if (!VFLT_ISZERO(f->sp)) {
	    GET_FLT(f->sp, flt);
	    flt.negate();
	    PUT_FLT(f->sp, flt);
	}
	return 0;

    case T_LWOBJECT:
	if (f->sp->array->elts[0].type == T_OBJECT) {
	    kf->unary(f);
	    return 0;
	}
	break;
    }

    kf->argError(1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_umin_int, pt_umin_int, 0)
# else
char pt_umin_int[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_INT };

/*
 * - int
 */
int kf_umin_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(f->sp, -f->sp->number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor, pt_xor, 0)
# else
char pt_xor[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_MIXED };

/*
 * value ^ value
 */
int kf_xor(Frame *f, int n, KFun *kf)
{
    Array *a;

    UNREFERENCED_PARAMETER(n);

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp->type == T_INT) {
	    PUT_INT(&f->sp[1], f->sp[1].number ^ f->sp->number);
	    f->sp++;
	    return 0;
	}
	break;

    case T_ARRAY:
	if (f->sp->type == T_ARRAY) {
	    f->addTicks((LPCint) f->sp[1].array->size + f->sp->array->size);
	    a = f->sp[1].array->setXAdd(f->data, f->sp->array);
	    f->sp->array->del();
	    f->sp++;
	    f->sp->array->del();
	    PUT_ARR(f->sp, a);
	    return 0;
	}
	break;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    kf->binary(f);
	    return 0;
	}
	/* fall through */
    default:
	kf->argError(1);
    }

    kf->argError(2);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("^", kf_xor_int, pt_xor_int, 0)
# else
char pt_xor_int[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_INT, T_INT };

/*
 * int ^ int
 */
int kf_xor_int(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INT(&f->sp[1], f->sp[1].number ^ f->sp->number);
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("(string)", kf_tostring, pt_tostring, 0)
# else
char pt_tostring[] = { C_STATIC, 1, 0, 0, 7, T_STRING, T_MIXED };

/*
 * cast an int or float to a string
 */
int kf_tostring(Frame *f, int n, KFun *kf)
{
    char *num, buffer[FLOAT_BUFFER];
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    num = NULL;

    f->addTicks(2);
    if (f->sp->type == T_INT) {
	/* from int */
	num = kf_itoa(f->sp->number, buffer);
    } else if (f->sp->type == T_FLOAT) {
	/* from float */
	f->addTicks(1);
	GET_FLT(f->sp, flt);
	flt.ftoa(num = buffer);
    } else if (f->sp->type == T_STRING) {
	return 0;
    } else {
	EC->error("Value is not a string");
    }

    PUT_STRVAL(f->sp, String::create(num, strlen(num)));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_ckrangeft, pt_ckrangeft, 0)
# else
char pt_ckrangeft[] = { C_STATIC, 3, 0, 0, 9, T_INT, T_MIXED, T_INT, T_INT };

/*
 * Check a [ from .. to ] subrange.
 *		This function doesn't pop its arguments and returns nothing.
 */
int kf_ckrangeft(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);

    if (f->sp[1].type != T_INT) {
	kf->argError(2);
    }
    if (f->sp->type != T_INT) {
	kf->argError(3);
    }
    if (f->sp[2].type == T_STRING) {
	f->sp[2].string->checkRange(f->sp[1].number, f->sp->number);
    } else if (f->sp[2].type == T_ARRAY) {
	f->sp[2].array->checkRange(f->sp[1].number, f->sp->number);
    } else {
	kf->argError(1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_ckrangef, pt_ckrangef, 0)
# else
char pt_ckrangef[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_INT };

/*
 * Check a [ from .. ] subrange, add missing index.
 * This function doesn't pop its arguments.
 */
int kf_ckrangef(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type != T_INT) {
	kf->argError(2);
    }
    if (f->sp[1].type == T_STRING) {
	(--f->sp)->type = T_INT;
	f->sp->number = (LPCint) f->sp[2].string->len - 1;
	f->sp[2].string->checkRange(f->sp[1].number, f->sp->number);
    } else if (f->sp[1].type == T_ARRAY) {
	(--f->sp)->type = T_INT;
	f->sp->number = (LPCint) f->sp[2].array->size - 1;
	f->sp[2].array->checkRange(f->sp[1].number, f->sp->number);
    } else {
	kf->argError(1);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_ckranget, pt_ckranget, 0)
# else
char pt_ckranget[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_MIXED, T_INT };

/*
 * Check a [ .. to ] subrange, add missing index.
 * This function doesn't pop its arguments.
 */
int kf_ckranget(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);

    if (f->sp->type != T_INT) {
	kf->argError(2);
    }
    if (f->sp[1].type == T_STRING) {
	f->sp[1].string->checkRange(0, f->sp->number);
    } else if (f->sp[1].type == T_ARRAY) {
	f->sp[1].array->checkRange(0, f->sp->number);
    } else {
	kf->argError(1);
    }

    --f->sp;
    f->sp[0] = f->sp[1];
    PUT_INT(&f->sp[1], 0);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_other", kf_call_other, pt_call_other, 0)
# else
char pt_call_other[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 2, 1, 0, 9,
			 T_MIXED, T_MIXED, T_STRING, T_MIXED };

/*
 * call a function in another object
 */
int kf_call_other(Frame *f, int nargs, KFun *kf)
{
    Value *val;
    Object *obj;
    LWO *lwobj;

    UNREFERENCED_PARAMETER(kf);

    obj = (Object *) NULL;
    lwobj = (LWO *) NULL;
    val = &f->sp[nargs - 1];
    if (val->type == T_STRING) {
	*--f->sp = *val;
	*val = nil;		/* erase old copy */
	DGD::callDriver(f, "call_object", 1);
	*val = *f->sp++;
    }
    switch (val->type) {
    case T_OBJECT:
	obj = OBJR(val->oindex);
	break;

    case T_LWOBJECT:
	lwobj = dynamic_cast<LWO *> (val->array);
	break;

    default:
	/* bad arg 1 */
	return 1;
    }

    if (OBJR(f->oindex)->count == 0) {
	/*
	 * call from destructed object
	 */
	f->pop(nargs);
	*--f->sp = nil;
	return 0;
    }

    if (f->call(obj, lwobj, val[-1].string->text, val[-1].string->len, FALSE,
		nargs - 2)) {
	val = f->sp++;		/* function exists */
    } else {
	val = &nil;		/* function doesn't exist */
    }
    (f->sp++)->string->del();
    f->sp->del();
    *f->sp = *val;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_status_idx, pt_status_idx, 0)
# else
char pt_status_idx[] = { C_STATIC, 1, 0, 0, 7, T_MIXED, T_INT };

/*
 * return status()[idx]
 */
int kf_status_idx(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type != T_INT) {
	EC->error("Non-numeric array index");
    }
    f->addTicks(2);
    if (!Config::statusi(f, f->sp->number, f->sp)) {
	EC->error("Array index out of range");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_statuso_idx, pt_statuso_idx, 0)
# else
char pt_statuso_idx[] = { C_STATIC, 2, 0, 0, 8, T_MIXED, T_MIXED, T_INT };

/*
 * return status(obj)[idx]
 */
int kf_statuso_idx(Frame *f, int nargs, KFun *kf)
{
    uindex n;

    UNREFERENCED_PARAMETER(nargs);

    n = 0;

    switch (f->sp[1].type) {
    case T_INT:
	if (f->sp[1].number != 0) {
	    EC->error("Array index on bad type");
	}
	f->addTicks(2);
	if (!Config::statusi(f, f->sp->number, &f->sp[1])) {
	    EC->error("Array index out of range");
	}
	f->sp++;
	return 0;

    case T_OBJECT:
	n = f->sp[1].oindex;
	break;

    case T_LWOBJECT:
	if (f->sp[1].array->elts[0].type == T_OBJECT) {
	    n = f->sp[1].array->elts[0].oindex;
	    f->sp[1].array->del();
	    f->sp[1] = nil;
	} else {
	    /* no user-visible parts within (right?) */
	    EC->error("Index on bad type");
	}
	break;

    default:
	kf->argError(1);
    }
    if (f->sp->type != T_INT) {
	EC->error("Non-numeric array index");
    }
    f->addTicks(2);
    if (!Config::objecti(f->data, OBJR(n), f->sp->number, &f->sp[1])) {
	EC->error("Array index out of range");
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
 * return call_trace()[idx]
 */
int kf_calltr_idx(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type != T_INT) {
	EC->error("Non-numeric array index");
    }
    if (!f->callTraceI(f->sp->number, f->sp)) {
	EC->error("Array index out of range");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("nil", kf_nil, pt_nil, 0)
# else
char pt_nil[] = { C_STATIC, 0, 0, 0, 6, T_NIL };

/*
 * return nil
 */
int kf_nil(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    *--f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_status, pt_status, 0)
# else
char pt_status[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7,
		     T_MIXED | (1 << REFSHIFT), T_MIXED };

/*
 * return an array with status information about the gamedriver
 *		or an object
 */
int kf_status(Frame *f, int nargs, KFun *kf)
{
    Array *a;
    uindex n;

    UNREFERENCED_PARAMETER(kf);

    f->addTicks(100);
    if (nargs == 0) {
	a = Config::status(f);
	--f->sp;
    } else {
	switch (f->sp->type) {
	case T_INT:
	    if (f->sp->number != 0) {
		*f->sp = nil;
		return 0;
	    }
	    a = Config::status(f);
	    break;

	case T_OBJECT:
	    n = f->sp->oindex;
	    a = Config::object(f->data, OBJR(n));
	    break;

	case T_LWOBJECT:
	    n = f->sp->array->elts[0].oindex;
	    f->sp->array->del();
	    a = Config::object(f->data, OBJR(n));
	    break;

	default:
	    return 1;
	}
    }
    PUT_ARRVAL(f->sp, a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_trace", kf_call_trace, pt_call_trace, 0)
# else
char pt_call_trace[] = { C_STATIC, 0, 0, 0, 6, T_MIXED | (2 << REFSHIFT) };

/*
 * return the entire call_other chain
 */
int kf_call_trace(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUSH_ARRVAL(f, f->callTrace());
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_float, pt_add_float, 0)
# else
char pt_add_float[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };

/*
 * float + float
 */
int kf_add_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    f1.add(f2);
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
 * float + string
 */
int kf_add_float_string(Frame *f, int n, KFun *kf)
{
    char buffer[FLOAT_BUFFER];
    Float flt;
    String *str;
    long l;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(3);
    GET_FLT(&f->sp[1], flt);
    flt.ftoa(buffer);
    str = String::create((char *) NULL,
			 (l=strlen(buffer)) + f->sp->string->len);
    strcpy(str->text, buffer);
    memcpy(str->text + l, f->sp->string->text, f->sp->string->len);
    f->sp->string->del();
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
 * int + string
 */
int kf_add_int_string(Frame *f, int n, KFun *kf)
{
    char buffer[LPCINT_BUFFER], *num;
    String *str;
    long l;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    num = kf_itoa(f->sp[1].number, buffer);
    str = String::create((char *) NULL, (l=strlen(num)) + f->sp->string->len);
    strcpy(str->text, num);
    memcpy(str->text + l, f->sp->string->text, f->sp->string->len);
    f->sp->string->del();
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
 * string + string
 */
int kf_add_string(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    str = f->sp[1].string->add(f->sp->string);
    f->sp->string->del();
    f->sp++;
    f->sp->string->del();
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
 * string + float
 */
int kf_add_string_float(Frame *f, int n, KFun *kf)
{
    char buffer[FLOAT_BUFFER];
    Float flt;
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(3);
    GET_FLT(f->sp, flt);
    flt.ftoa(buffer);
    f->sp++;
    str = String::create((char *) NULL,
			 f->sp->string->len + (long) strlen(buffer));
    memcpy(str->text, f->sp->string->text, f->sp->string->len);
    strcpy(str->text + f->sp->string->len, buffer);
    f->sp->string->del();
    PUT_STR(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("+", kf_add_string_int, pt_add_string_int, 0)
# else
char pt_add_string_int[] = { C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING, T_INT };

/*
 * string + int
 */
int kf_add_string_int(Frame *f, int n, KFun *kf)
{
    char buffer[LPCINT_BUFFER], *num;
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    num = kf_itoa(f->sp->number, buffer);
    f->sp++;
    str = String::create((char *) NULL,
			 f->sp->string->len + (long) strlen(num));
    memcpy(str->text, f->sp->string->text, f->sp->string->len);
    strcpy(str->text + f->sp->string->len, num);
    f->sp->string->del();
    PUT_STR(f->sp, str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("++", kf_add1_float, pt_add1_float, 0)
# else
char pt_add1_float[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * float++
 */
int kf_add1_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f1);
    f2.initOne();
    f1.add(f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("/", kf_div_float, pt_div_float, 0)
# else
char pt_div_float[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };

/*
 * float / float
 */
int kf_div_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    f1.div(f2);
    PUT_FLT(f->sp, f1);
    return 0;

}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq_float, pt_eq_float, 0)
# else
char pt_eq_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * float == float
 */
int kf_eq_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (f1.cmp(f2) == 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("==", kf_eq_string, pt_eq_string, 0)
# else
char pt_eq_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * string == string
 */
int kf_eq_string(Frame *f, int n, KFun *kf)
{
    bool flag;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    flag = (f->sp[1].string->cmp(f->sp->string) == 0);
    f->sp->string->del();
    f->sp++;
    f->sp->string->del();
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_float, pt_ge_float, 0)
# else
char pt_ge_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * float >= float
 */
int kf_ge_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (f1.cmp(f2) >= 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">=", kf_ge_string, pt_ge_string, 0)
# else
char pt_ge_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * string >= string
 */
int kf_ge_string(Frame *f, int n, KFun *kf)
{
    bool flag;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    flag = (f->sp[1].string->cmp(f->sp->string) >= 0);
    f->sp->string->del();
    f->sp++;
    f->sp->string->del();
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_float, pt_gt_float, 0)
# else
char pt_gt_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * float > float
 */
int kf_gt_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (f1.cmp(f2) > 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF(">", kf_gt_string, pt_gt_string, 0)
# else
char pt_gt_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * string > string
 */
int kf_gt_string(Frame *f, int n, KFun *kf)
{
    bool flag;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    flag = (f->sp[1].string->cmp(f->sp->string) > 0);
    f->sp->string->del();
    f->sp++;
    f->sp->string->del();
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_float, pt_le_float, 0)
# else
char pt_le_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * float <= float
 */
int kf_le_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (f1.cmp(f2) <= 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<=", kf_le_string, pt_le_string, 0)
# else
char pt_le_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * string <= string
 */
int kf_le_string(Frame *f, int n, KFun *kf)
{
    bool flag;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    flag = (f->sp[1].string->cmp(f->sp->string) <= 0);
    f->sp->string->del();
    f->sp++;
    f->sp->string->del();
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_float, pt_lt_float, 0)
# else
char pt_lt_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * float < float
 */
int kf_lt_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (f1.cmp(f2) < 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("<", kf_lt_string, pt_lt_string, 0)
# else
char pt_lt_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * string < string
 */
int kf_lt_string(Frame *f, int n, KFun *kf)
{
    bool flag;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    flag = (f->sp[1].string->cmp(f->sp->string) < 0);
    f->sp->string->del();
    f->sp++;
    f->sp->string->del();
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("*", kf_mult_float, pt_mult_float, 0)
# else
char pt_mult_float[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };

/*
 * float * float
 */
int kf_mult_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    f1.mult(f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne_float, pt_ne_float, 0)
# else
char pt_ne_float[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_FLOAT, T_FLOAT };

/*
 * float != float
 */
int kf_ne_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    PUT_INTVAL(f->sp, (f1.cmp(f2) != 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_ne_string, pt_ne_string, 0)
# else
char pt_ne_string[] = { C_STATIC, 2, 0, 0, 8, T_INT, T_STRING, T_STRING };

/*
 * string != string
 */
int kf_ne_string(Frame *f, int n, KFun *kf)
{
    bool flag;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    flag = (f->sp[1].string->cmp(f->sp->string) != 0);
    f->sp->string->del();
    f->sp++;
    f->sp->string->del();
    PUT_INTVAL(f->sp, flag);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_not_float, pt_not_float, 0)
# else
char pt_not_float[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_FLOAT };

/*
 * ! float
 */
int kf_not_float(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INTVAL(f->sp, VFLT_ISZERO(f->sp));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!=", kf_not_string, pt_not_string, 0)
# else
char pt_not_string[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_STRING };

/*
 * ! string
 */
int kf_not_string(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->sp->string->del();
    PUT_INTVAL(f->sp, FALSE);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_sub_float, pt_sub_float, 0)
# else
char pt_sub_float[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };

/*
 * float - float
 */
int kf_sub_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    f1.sub(f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("--", kf_sub1_float, pt_sub1_float, 0)
# else
char pt_sub1_float[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * float--
 */
int kf_sub1_float(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f1);
    f2.initOne();
    f1.sub(f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst_float, pt_tst_float, 0)
# else
char pt_tst_float[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_FLOAT };

/*
 * !! float
 */
int kf_tst_float(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUT_INTVAL(f->sp, !VFLT_ISZERO(f->sp));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("!!", kf_tst_string, pt_tst_string, 0)
# else
char pt_tst_string[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_STRING };

/*
 * !! string
 */
int kf_tst_string(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->sp->string->del();
    PUT_INTVAL(f->sp, TRUE);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("-", kf_umin_float, pt_umin_float, 0)
# else
char pt_umin_float[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * - float
 */
int kf_umin_float(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    if (!VFLT_ISZERO(f->sp)) {
	GET_FLT(f->sp, flt);
	flt.negate();
	PUT_FLT(f->sp, flt);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sum", kf_sum, pt_sum, 0)
# else
char pt_sum[] = { C_STATIC | C_ELLIPSIS, 0, 1, 0, 7, T_MIXED, T_MIXED };

/*
 * perform a summand operation
 */
int kf_sum(Frame *f, int nargs, KFun *kf)
{
    char buffer[FLOAT_BUFFER], *num;
    String *s;
    Array *a;
    Value *v, *e1, *e2;
    int i, type, vtype, nonint;
    long size;
    ssizet len;
    LPCint result;
    long isize;

    UNREFERENCED_PARAMETER(kf);

    /*
     * pass 1: check the types of everything and calculate the size
     */
    f->addTicks(nargs);
    type = T_NIL;
    isize = size = 0;
    nonint = nargs;
    result = 0;
    for (v = f->sp, i = nargs; --i >= 0; v++) {
	switch (v->number) {
	case SUM_SIMPLE:
	    /* simple term */
	    v++;
	    vtype = v->type;
	    if (vtype == T_STRING) {
		size += v->string->len;
	    } else if (vtype == T_ARRAY) {
		size += v->array->size;
	    } else {
		size += strlen(kf_itoa(v->number, buffer));
	    }
	    break;

	case SUM_ALLOCATE_NIL:
	    v++;
	    if (v->number < 0) {
		EC->error("Bad argument 1 for kfun allocate");
	    }
	    if (v->number > Config::arraySize()) {
		EC->error("Array too large");
	    }
	    size += v->number;
	    vtype = T_ARRAY;
	    break;

	case SUM_ALLOCATE_INT:
	    v++;
	    if (v->number < 0) {
		EC->error("Bad argument 1 for kfun allocate_int");
	    }
	    if (v->number > Config::arraySize()) {
		EC->error("Array too large");
	    }
	    size += v->number;
	    vtype = T_ARRAY;
	    break;

	case SUM_ALLOCATE_FLT:
	    v++;
	    if (v->number < 0) {
		EC->error("Bad argument 1 for kfun allocate_float");
	    }
	    if (v->number > Config::arraySize()) {
		EC->error("Array too large");
	    }
	    size += v->number;
	    vtype = T_ARRAY;
	    break;

	default:
	    if (v->number <= SUM_AGGREGATE) {
		/* aggregate */
		size += SUM_AGGREGATE - v->number;
		v += SUM_AGGREGATE - v->number;
		vtype = T_ARRAY;
	    } else {
		/* subrange term */
		size += v->number - v[1].number + 1;
		v += 2;
		vtype = v->type;
	    }
	    break;
	}

	if (vtype == T_STRING || vtype == T_ARRAY) {
	    nonint = i;
	    isize = size;
	    if (type == T_NIL && (vtype != T_ARRAY || i == nargs - 1)) {
		type = vtype;
	    } else if (type != vtype) {
		EC->error("Bad argument 2 for kfun +");
	    }
	} else if (vtype != T_INT || type == T_ARRAY) {
	    EC->error("Bad argument %d for kfun +", (i == 0) ? 1 : 2);
	} else {
	    result += v->number;
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
	s = String::create((char *) NULL, size);
	s->text[size] = '\0';
	for (v = f->sp, i = nargs; --i >= 0; v++) {
	    if (v->number == SUM_SIMPLE) {
		/* simple term */
		v++;
		if (v->type == T_STRING) {
		    size -= v->string->len;
		    memcpy(s->text + size, v->string->text, v->string->len);
		    v->string->del();
		    result = 0;
		} else if (nonint < i) {
		    num = kf_itoa(v->number, buffer);
		    len = strlen(num);
		    size -= len;
		    memcpy(s->text + size, num, len);
		    result = 0;
		} else {
		    result += v->number;
		}
	    } else {
		/* subrange */
		len = (v->number - v[1].number + 1);
		size -= len;
		memcpy(s->text + size, v[2].string->text + v[1].number, len);
		v += 2;
		v->string->del();
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
	a = Array::create(f->data, size);
	e1 = a->elts + size;
	for (v = f->sp, i = nargs; --i >= 0; v++) {
	    switch (v->number) {
	    case SUM_SIMPLE:
		/* simple term */
		v++;
		len = v->array->size;
		e2 = Dataspace::elts(v->array) + len;
		break;

	    case SUM_ALLOCATE_NIL:
		v++;
		for (len = v->number; len > 0; --len) {
		    *--e1 = nil;
		}
		continue;

	    case SUM_ALLOCATE_INT:
		v++;
		for (len = v->number; len > 0; --len) {
		    *--e1 = zeroInt;
		}
		continue;

	    case SUM_ALLOCATE_FLT:
		v++;
		for (len = v->number; len > 0; --len) {
		    *--e1 = zeroFloat;
		}
		continue;

	    default:
		if (v->number <= SUM_AGGREGATE) {
		    /* aggregate */
		    for (len = SUM_AGGREGATE - v->number; len > 0; --len) {
			*--e1 = *++v;
		    }
		    continue;
		} else {
		    /* subrange */
		    len = v->number - v[1].number + 1;
		    e2 = Dataspace::elts(v[2].array) + v->number + 1;
		    v += 2;
		    break;
		}
	    }

	    e1 -= len;
	    Value::copy(e1, e2 - len, len);
	    v->array->del();
	    size -= len;
	}

	f->sp = v - 1;
	Dataspace::refImports(a);
	PUT_ARRVAL(f->sp, a);
    } else {
	/* integers only */
	for (v = f->sp, i = nargs; --i > 0; v += 2) {
	    result += v[1].number;
	}

	f->sp = v + 1;
	f->sp->number += result;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("fabs", kf_fabs, alt_fabs, 0)
FUNCDEF("floor", kf_floor, alt_floor, 0)
FUNCDEF("ceil", kf_ceil, alt_ceil, 0)
FUNCDEF("fmod", kf_fmod, alt_fmod, 0)
FUNCDEF("frexp", kf_frexp, alt_frexp, 0)
FUNCDEF("ldexp", kf_ldexp, alt_ldexp, 0)
FUNCDEF("modf", kf_modf, alt_modf, 0)
FUNCDEF("exp", kf_exp, alt_exp, 0)
FUNCDEF("log", kf_log, alt_log, 0)
FUNCDEF("log10", kf_log10, alt_log10, 0)
FUNCDEF("pow", kf_pow, alt_pow, 0)
FUNCDEF("sqrt", kf_sqrt, alt_sqrt, 0)
FUNCDEF("cos", kf_cos, alt_cos, 0)
FUNCDEF("sin", kf_sin, alt_sin, 0)
FUNCDEF("tan", kf_tan, alt_tan, 0)
FUNCDEF("acos", kf_acos, alt_acos, 0)
FUNCDEF("asin", kf_asin, alt_asin, 0)
FUNCDEF("atan", kf_atan, alt_atan, 0)
FUNCDEF("atan2", kf_atan2, alt_atan2, 0)
FUNCDEF("cosh", kf_cosh, alt_cosh, 0)
FUNCDEF("sinh", kf_sinh, alt_sinh, 0)
FUNCDEF("tanh", kf_tanh, alt_tanh, 0)
# else
char alt_fabs[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_floor[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_ceil[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_fmod[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };
char alt_frexp[] = { C_STATIC, 1, 0, 0, 7, (1 << REFSHIFT) | T_MIXED, T_FLOAT };
char alt_ldexp[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_INT };
char alt_modf[] = { C_STATIC, 1, 0, 0, 7, (1 << REFSHIFT) | T_FLOAT, T_FLOAT };
char alt_exp[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_log[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_log10[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_pow[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };
char alt_sqrt[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_cos[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_sin[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_tan[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_acos[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_asin[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_atan[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_atan2[] = { C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT, T_FLOAT };
char alt_cosh[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_sinh[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
char alt_tanh[] = { C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };
# endif


# ifdef FUNCDEF
FUNCDEF("call_trace", kf_calltr_idx_idx, pt_calltr_idx_idx, 0)
# else
char pt_calltr_idx_idx[] = { C_STATIC, 2, 0, 0, 8, T_MIXED | (1 << REFSHIFT),
			     T_INT };

/*
 * return call_trace()[i][j]
 */
int kf_calltr_idx_idx(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp[0].type != T_INT || f->sp[1].type != T_INT) {
	EC->error("Non-numeric array index");
    }
    if (!f->callTraceII(f->sp[1].number, f->sp[0].number, f->sp + 1)) {
	EC->error("Array index out of range");
    }
    f->sp++;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("strlen", kf_strlen, pt_strlen, 0)
# else
char pt_strlen[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT, T_STRING };

/*
 * return the length of a string
 */
int kf_strlen(Frame *f, int n, KFun *kf)
{
    ssizet len;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    len = f->sp->string->len;
    f->sp->string->del();
    PUT_INTVAL(f->sp, len);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_rangeft_str, pt_rangeft_str, 0)
# else
char pt_rangeft_str[] = { C_STATIC, 3, 0, 0, 9, T_STRING, T_STRING, T_INT,
			  T_INT };
/*
 * value [ int .. int ]
 */
int kf_rangeft_str(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    str = f->sp[2].string->range(f->sp[1].number, f->sp->number);
    f->sp += 2;
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_rangef_str, pt_rangef_str, 0)
# else
char pt_rangef_str[] = { C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING, T_INT };

/*
 * value [ int .. ]
 */
int kf_rangef_str(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    str = f->sp[1].string->range(f->sp->number, f->sp[1].string->len - 1L);
    f->sp++;
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("[..]", kf_ranget_str, pt_ranget_str, 0)
# else
char pt_ranget_str[] = { C_STATIC, 2, 0, 0, 8, T_STRING, T_STRING, T_INT };

/*
 * value [ .. int ]
 */
int kf_ranget_str(Frame *f, int n, KFun *kf)
{
    String *str;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    str = f->sp[1].string->range(0, f->sp->number);
    f->sp++;
    f->sp->string->del();
    PUT_STR(f->sp, str);

    return 0;
}
# endif
