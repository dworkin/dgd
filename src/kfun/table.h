/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2011 DGD Authors (see the file Changelog for details)
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

typedef void (*extfunc)(frame *, int, value *);
typedef struct {
    char *name;		/* function name */
    char *proto;	/* prototype */
    int (*func)();	/* function address */
    extfunc ext;	/* extension */
    short version;	/* version number */
} kfunc;

extern kfunc kftab[], kfenc[], kfdec[], kfhsh[];	/* kfun tables */
extern char  kfind[];					/* indirection table */
extern int   nkfun, ne, nd, nh;				/* # kfuns */

# define KFUN(kf)	(kftab[UCHAR(kfind[kf])])

typedef struct {
    char *name;		/* added kfun name */
    char *proto;	/* simplified prototype */
    extfunc func;	/* function address */
} extkfunc;

extern void kf_clear	(void);
extern void kf_ext_kfun	(extkfunc*, int);
extern void kf_init	(void);
extern int  kf_func	(char*);
extern void kf_reclaim	(void);
extern bool kf_dump	(int);
extern void kf_restore	(int, int);

# define KF_ADD		 0
# define KF_ADD_INT	 1
# define KF_ADD1	 2
# define KF_ADD1_INT	 3
# define KF_AND		 4
# define KF_AND_INT	 5
# define KF_DIV		 6
# define KF_DIV_INT	 7
# define KF_EQ		 8
# define KF_EQ_INT	 9
# define KF_GE		10
# define KF_GE_INT	11
# define KF_GT		12
# define KF_GT_INT	13
# define KF_LE		14
# define KF_LE_INT	15
# define KF_LSHIFT	16
# define KF_LSHIFT_INT	17
# define KF_LT		18
# define KF_LT_INT	19
# define KF_MOD		20
# define KF_MOD_INT	21
# define KF_MULT	22
# define KF_MULT_INT	23
# define KF_NE		24
# define KF_NE_INT	25
# define KF_NEG		26
# define KF_NEG_INT	27
# define KF_NOT		28
# define KF_NOT_INT	29
# define KF_OR		30
# define KF_OR_INT	31
# define KF_RANGEFT	32
# define KF_RANGEF	33
# define KF_RANGET	34
# define KF_RANGE	35
# define KF_RSHIFT	36
# define KF_RSHIFT_INT	37
# define KF_SUB		38
# define KF_SUB_INT	39
# define KF_SUB1	40
# define KF_SUB1_INT	41
# define KF_TOFLOAT	42
# define KF_TOINT	43
# define KF_TST		44
# define KF_TST_INT	45
# define KF_UMIN	46
# define KF_UMIN_INT	47
# define KF_XOR		48
# define KF_XOR_INT	49
# define KF_TOSTRING	50
# define KF_CKRANGEFT	51
# define KF_CKRANGEF	52
# define KF_CKRANGET	53
# define KF_SUM		54
# define KF_STATUS_IDX	55
# define KF_STATUSO_IDX	56
# define KF_CALLTR_IDX	57
# define KF_NIL		58
# define KF_INSTANCEOF	59
# define KF_STORE_AGGR	60
# define KF_ADD_FLT	61
# define KF_ADD_FLT_STR	62
# define KF_ADD_INT_STR	63
# define KF_ADD_STR	64
# define KF_ADD_STR_FLT	65
# define KF_ADD_STR_INT	66
# define KF_ADD1_FLT	67
# define KF_DIV_FLT	68
# define KF_EQ_FLT	69
# define KF_EQ_STR	70
# define KF_GE_FLT	71
# define KF_GE_STR	72
# define KF_GT_FLT	73
# define KF_GT_STR	74
# define KF_LE_FLT	75
# define KF_LE_STR	76
# define KF_LT_FLT	77
# define KF_LT_STR	78
# define KF_MULT_FLT	79
# define KF_NE_FLT	80
# define KF_NE_STR	81
# define KF_NOT_FLT	82
# define KF_NOT_STR	83
# define KF_SUB_FLT	84
# define KF_SUB1_FLT	85
# define KF_TST_FLT	86
# define KF_TST_STR	87
# define KF_UMIN_FLT	88

# define KF_BUILTINS	89
