# ifndef FUNCDEF
# include "kfun.h"
# endif


# ifdef FUNCDEF
FUNCDEF("fabs", kf_fabs, p_fabs)
# else
char p_fabs[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->fabs()
 * DESCRIPTION:	return the absolute value of a float
 */
int kf_fabs()
{
    VFLT_ABS(sp);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("floor", kf_floor, p_floor)
# else
char p_floor[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->floor()
 * DESCRIPTION:	round the argument downwards
 */
int kf_floor()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_floor(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ceil", kf_ceil, p_ceil)
# else
char p_ceil[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->ceil()
 * DESCRIPTION:	round the argument upwards
 */
int kf_ceil()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_ceil(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("fmod", kf_fmod, p_fmod)
# else
char p_fmod[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 2,
		  T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->fmod()
 * DESCRIPTION:	compute fmod
 */
int kf_fmod()
{
    xfloat f1, f2;

    VFLT_GET(sp, f2);
    sp++;
    VFLT_GET(sp, f1);
    flt_fmod(&f1, &f2);
    VFLT_PUT(sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("frexp", kf_frexp, p_frexp)
# else
char p_frexp[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
		   (1 << REFSHIFT) | T_MIXED, 1, T_FLOAT };

/*
 * NAME:	kfun->frexp()
 * DESCRIPTION:	split a float into a fraction and an exponent
 */
int kf_frexp()
{
    xfloat flt;
    Int num;
    array *a;

    VFLT_GET(sp, flt);
    num = flt_frexp(&flt);
    a = arr_new(2L);
    a->elts[0].type = T_FLOAT;
    VFLT_PUT(a->elts, flt);
    a->elts[1].type = T_INT;
    a->elts[1].u.number = num;
    sp->type = T_ARRAY;
    arr_ref(sp->u.array = a);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ldexp", kf_ldexp, p_ldexp)
# else
char p_ldexp[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 2,
		   T_FLOAT, T_INT };

/*
 * NAME:	kfun->ldexp()
 * DESCRIPTION:	make a float from a fraction and an exponent
 */
int kf_ldexp()
{
    xfloat flt;

    VFLT_GET(sp + 1, flt);
    flt_ldexp(&flt, sp->u.number);
    sp++;
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("modf", kf_modf, p_modf)
# else
char p_modf[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, (1 << REFSHIFT) | T_FLOAT,
		  1, T_FLOAT };

/*
 * NAME:	kfun->modf()
 * DESCRIPTION:	split float into fraction and integer part
 */
int kf_modf()
{
    xfloat f1, f2;
    array *a;

    VFLT_GET(sp, f1);
    flt_modf(&f1, &f2);
    a = arr_new(2L);
    a->elts[0].type = T_FLOAT;
    VFLT_PUT(a->elts, f1);
    a->elts[1].type = T_FLOAT;
    VFLT_PUT(a->elts + 1, f2);
    sp->type = T_ARRAY;
    arr_ref(sp->u.array = a);

    return 0;
}
# endif


# if 0
# ifdef FUNCDEF
FUNCDEF("exp", kf_exp, p_exp)
# else
char p_exp[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->exp()
 * DESCRIPTION:	compute exp
 */
int kf_exp()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_exp(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("log", kf_log, p_log)
# else
char p_log[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->log()
 * DESCRIPTION:	compute log
 */
int kf_log()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_log(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("log10", kf_log10, p_log10)
# else
char p_log10[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->log10()
 * DESCRIPTION:	compute log10
 */
int kf_log10()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_log10(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("pow", kf_pow, p_pow)
# else
char p_pow[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 2,
		 T_FLOAT, T_FLOAT };

/*
 * NAME:	kfun->pow()
 * DESCRIPTION:	compute pow
 */
int kf_pow()
{
    xfloat f1, f2;

    VFLT_GET(sp, f2);
    sp++;
    VFLT_GET(sp, f1);
    flt_pow(&f1, &f2);
    VFLT_PUT(sp, f1);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sqrt", kf_sqrt, p_sqrt)
# else
char p_sqrt[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->sqrt()
 * DESCRIPTION:	compute sqrt
 */
int kf_sqrt()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_sqrt(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("cos", kf_cos, p_cos)
# else
char p_cos[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->cos()
 * DESCRIPTION:	compute cos
 */
int kf_cos()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_cos(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sin", kf_sin, p_sin)
# else
char p_sin[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->sin()
 * DESCRIPTION:	compute sin
 */
int kf_sin()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_sin(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("tan", kf_tan, p_tan)
# else
char p_tan[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->tan()
 * DESCRIPTION:	compute tan
 */
int kf_tan()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_tan(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("acos", kf_acos, p_acos)
# else
char p_acos[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->acos()
 * DESCRIPTION:	compute acos
 */
int kf_acos()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_acos(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("asin", kf_asin, p_asin)
# else
char p_asin[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->asin()
 * DESCRIPTION:	compute asin
 */
int kf_asin()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_asin(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("atan", kf_atan, p_atan)
# else
char p_atan[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->atan()
 * DESCRIPTION:	compute atan
 */
int kf_atan()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_atan(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("atan2", kf_atan2, p_atan2)
# else
char p_atan2[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->atan2()
 * DESCRIPTION:	compute atan2
 */
int kf_atan2()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_atan2(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("cosh", kf_cosh, p_cosh)
# else
char p_cosh[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->cosh()
 * DESCRIPTION:	compute cosh
 */
int kf_cosh()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_cosh(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sinh", kf_sinh, p_sinh)
# else
char p_sinh[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->sinh()
 * DESCRIPTION:	compute sinh
 */
int kf_sinh()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_sinh(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("tanh", kf_tanh, p_tanh)
# else
char p_tanh[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_FLOAT, 1, T_FLOAT };

/*
 * NAME:	kfun->tanh()
 * DESCRIPTION:	compute tanh
 */
int kf_tanh()
{
    xfloat flt;

    VFLT_GET(sp, flt);
    flt_tanh(&flt);
    VFLT_PUT(sp, flt);
    return 0;
}
# endif
# endif
