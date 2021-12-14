/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2021 DGD Authors (see the commit log for details)
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
# include "kfun.h"
# endif


# ifdef FUNCDEF
FUNCDEF("fabs", kf_fabs, pt_fabs, 0)
# else
char pt_fabs[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * return the absolute value of a float
 */
int kf_fabs(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, flt);
    flt.abs();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("floor", kf_floor, pt_floor, 0)
# else
char pt_floor[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * round the argument downwards
 */
int kf_floor(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, flt);
    flt.floor();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ceil", kf_ceil, pt_ceil, 0)
# else
char pt_ceil[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * round the argument upwards
 */
int kf_ceil(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, flt);
    flt.ceil();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("fmod", kf_fmod, pt_fmod, 0)
# else
char pt_fmod[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT,
		   T_FLOAT };

/*
 * compute fmod
 */
int kf_fmod(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    f1.fmod(f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("frexp", kf_frexp, pt_frexp, 0)
# else
char pt_frexp[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
		    (1 << REFSHIFT) | T_MIXED, T_FLOAT };

/*
 * split a float into a fraction and an exponent
 */
int kf_frexp(Frame *f, int n, KFun *kf)
{
    Float flt;
    LPCint num;
    Array *a;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    GET_FLT(f->sp, flt);
    num = flt.frexp();
    a = Array::create(f->data, 2);
    PUT_FLTVAL(&a->elts[0], flt);
    PUT_INTVAL(&a->elts[1], num);
    PUT_ARRVAL(f->sp, a);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ldexp", kf_ldexp, pt_ldexp, 0)
# else
char pt_ldexp[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT,
		    T_INT };

/*
 * make a float from a fraction and an exponent
 */
int kf_ldexp(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(1);
    GET_FLT(f->sp + 1, flt);
    flt.ldexp(f->sp->number);
    f->sp++;
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("modf", kf_modf, pt_modf, 0)
# else
char pt_modf[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
		   (1 << REFSHIFT) | T_FLOAT, T_FLOAT };

/*
 * split float into fraction and integer part
 */
int kf_modf(Frame *f, int n, KFun *kf)
{
    Float f1, f2;
    Array *a;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    GET_FLT(f->sp, f1);
    f1.modf(&f2);
    a = Array::create(f->data, 2);
    PUT_FLTVAL(&a->elts[0], f1);
    PUT_FLTVAL(&a->elts[1], f2);
    PUT_ARRVAL(f->sp, a);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("exp", kf_exp, pt_exp, 0)
# else
char pt_exp[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute exp
 */
int kf_exp(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(21);
    GET_FLT(f->sp, flt);
    flt.exp();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("log", kf_log, pt_log, 0)
# else
char pt_log[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute log
 */
int kf_log(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(35);
    GET_FLT(f->sp, flt);
    flt.log();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("log10", kf_log10, pt_log10, 0)
# else
char pt_log10[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute log10
 */
int kf_log10(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(41);
    GET_FLT(f->sp, flt);
    flt.log10();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("pow", kf_pow, pt_pow, 0)
# else
char pt_pow[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT,
		  T_FLOAT };

/*
 * compute pow
 */
int kf_pow(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(48);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    f1.pow(f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sqrt", kf_sqrt, pt_sqrt, 0)
# else
char pt_sqrt[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute sqrt
 */
int kf_sqrt(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(11);
    GET_FLT(f->sp, flt);
    flt.sqrt();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("cos", kf_cos, pt_cos, 0)
# else
char pt_cos[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute cos
 */
int kf_cos(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(25);
    GET_FLT(f->sp, flt);
    flt.cos();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sin", kf_sin, pt_sin, 0)
# else
char pt_sin[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute sin
 */
int kf_sin(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(25);
    GET_FLT(f->sp, flt);
    flt.sin();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("tan", kf_tan, pt_tan, 0)
# else
char pt_tan[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute tan
 */
int kf_tan(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(31);
    GET_FLT(f->sp, flt);
    flt.tan();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("acos", kf_acos, pt_acos, 0)
# else
char pt_acos[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute acos
 */
int kf_acos(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(24);
    GET_FLT(f->sp, flt);
    flt.acos();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asin", kf_asin, pt_asin, 0)
# else
char pt_asin[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute asin
 */
int kf_asin(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(24);
    GET_FLT(f->sp, flt);
    flt.asin();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("atan", kf_atan, pt_atan, 0)
# else
char pt_atan[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute atan
 */
int kf_atan(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(24);
    GET_FLT(f->sp, flt);
    flt.atan();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("atan2", kf_atan2, pt_atan2, 0)
# else
char pt_atan2[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_FLOAT, T_FLOAT,
		    T_FLOAT };

/*
 * compute atan2
 */
int kf_atan2(Frame *f, int n, KFun *kf)
{
    Float f1, f2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(27);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    f1.atan2(f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("cosh", kf_cosh, pt_cosh, 0)
# else
char pt_cosh[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute cosh
 */
int kf_cosh(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(24);
    GET_FLT(f->sp, flt);
    flt.cosh();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sinh", kf_sinh, pt_sinh, 0)
# else
char pt_sinh[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute sinh
 */
int kf_sinh(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(24);
    GET_FLT(f->sp, flt);
    flt.sinh();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("tanh", kf_tanh, pt_tanh, 0)
# else
char pt_tanh[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * compute tanh
 */
int kf_tanh(Frame *f, int n, KFun *kf)
{
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(24);
    GET_FLT(f->sp, flt);
    flt.tanh();
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif
