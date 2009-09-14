# ifndef FUNCDEF
# include "kfun.h"
# endif


# ifdef FUNCDEF
FUNCDEF("fabs", kf_fabs, pt_fabs, 0)
# else
char pt_fabs[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->fabs()
 * DESCRIPTION:	return the absolute value of a float
 */
int kf_fabs(f)
frame *f;
{
    xfloat flt;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, flt);
    FLT_ABS(flt.high, flt.low);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("floor", kf_floor, pt_floor, 0)
# else
char pt_floor[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->floor()
 * DESCRIPTION:	round the argument downwards
 */
int kf_floor(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, flt);
    flt_floor(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ceil", kf_ceil, pt_ceil, 0)
# else
char pt_ceil[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->ceil()
 * DESCRIPTION:	round the argument upwards
 */
int kf_ceil(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, flt);
    flt_ceil(&flt);
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
 * NAME:	kfun->fmod()
 * DESCRIPTION:	compute fmod
 */
int kf_fmod(f)
register frame *f;
{
    xfloat f1, f2;

    i_add_ticks(f, 1);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    flt_fmod(&f1, &f2);
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
 * NAME:	kfun->frexp()
 * DESCRIPTION:	split a float into a fraction and an exponent
 */
int kf_frexp(f)
register frame *f;
{
    xfloat flt;
    Int num;
    array *a;

    i_add_ticks(f, 2);
    GET_FLT(f->sp, flt);
    num = flt_frexp(&flt);
    a = arr_new(f->data, 2L);
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
 * NAME:	kfun->ldexp()
 * DESCRIPTION:	make a float from a fraction and an exponent
 */
int kf_ldexp(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 1);
    GET_FLT(f->sp + 1, flt);
    flt_ldexp(&flt, f->sp->u.number);
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
 * NAME:	kfun->modf()
 * DESCRIPTION:	split float into fraction and integer part
 */
int kf_modf(f)
register frame *f;
{
    xfloat f1, f2;
    array *a;

    i_add_ticks(f, 2);
    GET_FLT(f->sp, f1);
    flt_modf(&f1, &f2);
    a = arr_new(f->data, 2L);
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
 * NAME:	kfun->exp()
 * DESCRIPTION:	compute exp
 */
int kf_exp(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 21);
    GET_FLT(f->sp, flt);
    flt_exp(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("log", kf_log, pt_log, 0)
# else
char pt_log[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->log()
 * DESCRIPTION:	compute log
 */
int kf_log(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 35);
    GET_FLT(f->sp, flt);
    flt_log(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("log10", kf_log10, pt_log10, 0)
# else
char pt_log10[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->log10()
 * DESCRIPTION:	compute log10
 */
int kf_log10(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 41);
    GET_FLT(f->sp, flt);
    flt_log10(&flt);
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
 * NAME:	kfun->pow()
 * DESCRIPTION:	compute pow
 */
int kf_pow(f)
register frame *f;
{
    xfloat f1, f2;

    i_add_ticks(f, 48);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    flt_pow(&f1, &f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sqrt", kf_sqrt, pt_sqrt, 0)
# else
char pt_sqrt[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->sqrt()
 * DESCRIPTION:	compute sqrt
 */
int kf_sqrt(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 11);
    GET_FLT(f->sp, flt);
    flt_sqrt(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("cos", kf_cos, pt_cos, 0)
# else
char pt_cos[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->cos()
 * DESCRIPTION:	compute cos
 */
int kf_cos(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 25);
    GET_FLT(f->sp, flt);
    flt_cos(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sin", kf_sin, pt_sin, 0)
# else
char pt_sin[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->sin()
 * DESCRIPTION:	compute sin
 */
int kf_sin(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 25);
    GET_FLT(f->sp, flt);
    flt_sin(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("tan", kf_tan, pt_tan, 0)
# else
char pt_tan[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->tan()
 * DESCRIPTION:	compute tan
 */
int kf_tan(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 31);
    GET_FLT(f->sp, flt);
    flt_tan(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("acos", kf_acos, pt_acos, 0)
# else
char pt_acos[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->acos()
 * DESCRIPTION:	compute acos
 */
int kf_acos(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 24);
    GET_FLT(f->sp, flt);
    flt_acos(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asin", kf_asin, pt_asin, 0)
# else
char pt_asin[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->asin()
 * DESCRIPTION:	compute asin
 */
int kf_asin(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 24);
    GET_FLT(f->sp, flt);
    flt_asin(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("atan", kf_atan, pt_atan, 0)
# else
char pt_atan[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->atan()
 * DESCRIPTION:	compute atan
 */
int kf_atan(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 24);
    GET_FLT(f->sp, flt);
    flt_atan(&flt);
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
 * NAME:	kfun->atan2()
 * DESCRIPTION:	compute atan2
 */
int kf_atan2(f)
register frame *f;
{
    xfloat f1, f2;

    i_add_ticks(f, 27);
    GET_FLT(f->sp, f2);
    f->sp++;
    GET_FLT(f->sp, f1);
    flt_atan2(&f1, &f2);
    PUT_FLT(f->sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("cosh", kf_cosh, pt_cosh, 0)
# else
char pt_cosh[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->cosh()
 * DESCRIPTION:	compute cosh
 */
int kf_cosh(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 24);
    GET_FLT(f->sp, flt);
    flt_cosh(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sinh", kf_sinh, pt_sinh, 0)
# else
char pt_sinh[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->sinh()
 * DESCRIPTION:	compute sinh
 */
int kf_sinh(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 24);
    GET_FLT(f->sp, flt);
    flt_sinh(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("tanh", kf_tanh, pt_tanh, 0)
# else
char pt_tanh[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->tanh()
 * DESCRIPTION:	compute tanh
 */
int kf_tanh(f)
register frame *f;
{
    xfloat flt;

    i_add_ticks(f, 24);
    GET_FLT(f->sp, flt);
    flt_tanh(&flt);
    PUT_FLT(f->sp, flt);
    return 0;
}
# endif
