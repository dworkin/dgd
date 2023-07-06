/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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

typedef void (*ExtFunc)(Frame *, int, Value *);
struct ExtKFun {
    const char *name;	/* added kfun name */
    char *proto;	/* simplified prototype */
    ExtFunc func;	/* function address */
};

class KFun {
public:
    void argError(int n);
    void unary(Frame *f);
    void binary(Frame *f);
    void ternary(Frame *f);
    void compare(Frame *f);

    static void init();
    static void clear();
    static void add(const ExtKFun *kfadd, int n);
    static void jit();
    static int find(KFun *kf, unsigned int l, unsigned int h, const char *name);
    static int kfunc(const char *name);
    static void reclaim();
    static bool dump(int fd);
    static void restore(int fd);

    const char *name;					/* function name */
    char *proto;					/* prototype */
    int (*func)(Frame*, int, KFun*);			/* function address */
    ExtFunc ext;					/* extension */
    short version;					/* version number */
    bool lval;						/* has lvalue params */

private:
    static int callgate(Frame *f, int nargs, KFun *kf);
    static char *prototype(char *proto, bool *lval);
    static KFun *replace(KFun *table, int from, int to, int *size,
			 const char *name);
    static int cmp(cvoid *cv1, cvoid *cv2);
};

extern KFun kftab[], kfenc[], kfdec[], kfhsh[];		/* kfun tables */
extern kfindex kfind[];					/* indirection table */
extern int nkfun, ne, nd, nh;				/* # kfuns */

# define KFUN(kf)	(kftab[kfind[kf]])

extern int  kf_ckrangeft(Frame*, int, KFun*);
extern int  kf_ckrangef	(Frame*, int, KFun*);
extern int  kf_ckranget	(Frame*, int, KFun*);
extern int  kf_nil	(Frame*, int, KFun*);
extern int  kf_unused	(Frame*, int, KFun*);

# define KF_ADD			0
# define KF_ADD_INT		1
# define KF_ADD1		2
# define KF_ADD1_INT		3
# define KF_AND			4
# define KF_AND_INT		5
# define KF_DIV			6
# define KF_DIV_INT		7
# define KF_EQ			8
# define KF_EQ_INT		9
# define KF_GE			10
# define KF_GE_INT		11
# define KF_GT			12
# define KF_GT_INT		13
# define KF_LE			14
# define KF_LE_INT		15
# define KF_LSHIFT		16
# define KF_LSHIFT_INT		17
# define KF_LT			18
# define KF_LT_INT		19
# define KF_MOD			20
# define KF_MOD_INT		21
# define KF_MULT		22
# define KF_MULT_INT		23
# define KF_NE			24
# define KF_NE_INT		25
# define KF_NEG			26
# define KF_NEG_INT		27
# define KF_NOT			28
# define KF_NOT_INT		29
# define KF_OR			30
# define KF_OR_INT		31
# define KF_RANGEFT		32
# define KF_RANGEF		33
# define KF_RANGET		34
# define KF_RANGE		35
# define KF_RSHIFT		36
# define KF_RSHIFT_INT		37
# define KF_SUB			38
# define KF_SUB_INT		39
# define KF_SUB1		40
# define KF_SUB1_INT		41
# define KF_TOFLOAT		42
# define KF_TOINT		43
# define KF_TST			44
# define KF_TST_INT		45
# define KF_UMIN		46
# define KF_UMIN_INT		47
# define KF_XOR			48
# define KF_XOR_INT		49
# define KF_TOSTRING		50
# define KF_CKRANGEFT		51
# define KF_CKRANGEF		52
# define KF_CKRANGET		53
# define KF_CALL_OTHER		54
# define KF_STATUS_IDX		55
# define KF_STATUSO_IDX		56
# define KF_CALLTR_IDX		57
# define KF_NIL			58
# define KF_STATUS		59
# define KF_CALL_TRACE		60
# define KF_ADD_FLT		61
# define KF_ADD_FLT_STR		62
# define KF_ADD_INT_STR		63
# define KF_ADD_STR		64
# define KF_ADD_STR_FLT		65
# define KF_ADD_STR_INT		66
# define KF_ADD1_FLT		67
# define KF_DIV_FLT		68
# define KF_EQ_FLT		69
# define KF_EQ_STR		70
# define KF_GE_FLT		71
# define KF_GE_STR		72
# define KF_GT_FLT		73
# define KF_GT_STR		74
# define KF_LE_FLT		75
# define KF_LE_STR		76
# define KF_LT_FLT		77
# define KF_LT_STR		78
# define KF_MULT_FLT		79
# define KF_NE_FLT		80
# define KF_NE_STR		81
# define KF_NOT_FLT		82
# define KF_NOT_STR		83
# define KF_SUB_FLT		84
# define KF_SUB1_FLT		85
# define KF_TST_FLT		86
# define KF_TST_STR		87
# define KF_UMIN_FLT		88
# define KF_SUM			89
# define KF_FABS		90
# define KF_FLOOR		91
# define KF_CEIL		92
# define KF_FMOD		93
# define KF_FREXP		94
# define KF_LDEXP		95
# define KF_MODF		96
# define KF_EXP			97
# define KF_LOG			98
# define KF_LOG10		99
# define KF_POW			100
# define KF_SQRT		101
# define KF_COS			102
# define KF_SIN			103
# define KF_TAN			104
# define KF_ACOS		105
# define KF_ASIN		106
# define KF_ATAN		107
# define KF_ATAN2		108
# define KF_COSH		109
# define KF_SINH		110
# define KF_TANH		111
# define KF_CALLTR_IDX_IDX	112
# define KF_STRLEN		113
# define KF_RANGEFT_STRING	114
# define KF_RANGEF_STRING	115
# define KF_RANGET_STRING	116

# define KF_BUILTINS		117

# define SUM_SIMPLE		-2
# define SUM_ALLOCATE_NIL	-3
# define SUM_ALLOCATE_INT	-4
# define SUM_ALLOCATE_FLT	-5
# define SUM_AGGREGATE		-6

extern void hash_md5_start (Uint*);
extern void hash_md5_block (Uint*, char*);
extern void hash_md5_end   (char*, Uint*, char*, unsigned int, Uint);
