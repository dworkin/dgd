/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2014 DGD Authors (see the commit log for details)
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

typedef struct _node_ {
    unsigned char type;		/* type of node */
    char flags;			/* bitflags */
    unsigned short mod;		/* modifier */
    unsigned short line;	/* line number */
    string *class;		/* object class */
    union {
	Int number;		/* numeric value */
	unsigned short fhigh;	/* high word of float */
	string *string;		/* string value */
	char *ptr;		/* character pointer */
	struct _node_ *left;	/* left child */
    } l;
    union {
	Int number;		/* numeric value */
	Uint flow;		/* low longword of float */
	struct _node_ *right;	/* right child */
    } r;
} node;

# define NFLT_GET(n, f)	((f).high = (n)->l.fhigh, (f).low = (n)->r.flow)
# define NFLT_PUT(n, f)	((n)->l.fhigh = (f).high, (n)->r.flow = (f).low)
# define NFLT_ISZERO(n)	FLT_ISZERO((n)->l.fhigh, (n)->r.flow)
# define NFLT_ISONE(n)	FLT_ISONE((n)->l.fhigh, (n)->r.flow)
# define NFLT_ISMONE(n)	FLT_ISMONE((n)->l.fhigh, (n)->r.flow)
# define NFLT_ONE(n)	FLT_ONE((n)->l.fhigh, (n)->r.flow)
# define NFLT_ABS(n)	FLT_ABS((n)->l.fhigh, (n)->r.flow)
# define NFLT_NEG(n)	FLT_NEG((n)->l.fhigh, (n)->r.flow)

# define F_CONST	0x01	/* constant expression */
# define F_ENTRY	0x02	/* (first) statement has case/default entry */
# define F_CASE		0x04	/* statement block has case/default entry */
# define F_LABEL	0x08	/* statement block has label entry */
# define F_REACH	(F_CASE | F_LABEL)
# define F_BREAK	0x10	/* break */
# define F_CONTINUE	0x20	/* continue */
# define F_EXIT		0x40	/* return/goto */
# define F_END		(F_BREAK | F_CONTINUE | F_EXIT)
# define F_FLOW		(F_ENTRY | F_CASE | F_END)
# define F_VARARGS	0x04	/* varargs in parameter list */
# define F_ELLIPSIS	0x08	/* ellipsis in parameter list */

extern void  node_init	(int);
extern node *node_new	(unsigned int);
extern node *node_int	(Int);
extern node *node_float	(xfloat*);
extern node *node_nil	(void);
extern node *node_str	(string*);
extern node *node_var	(unsigned int, int);
extern node *node_type	(int, string*);
extern node *node_fcall	(int, string*, char*, Int);
extern node *node_op	(char*);
extern node *node_mon	(int, int, node*);
extern node *node_bin	(int, int, node*, node*);
extern void  node_toint	(node*, Int);
extern void  node_tostr	(node*, string*);
extern void  node_free	(void);
extern void  node_clear	(void);

# define N_ADD			  1
# define N_ADD_INT		  2
# define N_ADD_FLOAT		  3
# define N_ADD_EQ		  4
# define N_ADD_EQ_INT		  5
# define N_ADD_EQ_FLOAT		  6
# define N_ADD_EQ_1		  7
# define N_ADD_EQ_1_INT		  8
# define N_ADD_EQ_1_FLOAT	  9
# define N_AGGR			 10
# define N_AND			 11
# define N_AND_INT		 12
# define N_AND_EQ		 13
# define N_AND_EQ_INT		 14
# define N_ASSIGN		 15
# define N_BLOCK		 16
# define N_BREAK		 17
# define N_CASE			 18
# define N_CAST			 19
# define N_CATCH		 20
# define N_COMMA		 21
# define N_COMPOUND		 22
# define N_CONTINUE		 23
# define N_DIV			 24
# define N_DIV_INT		 25
# define N_DIV_FLOAT		 26
# define N_DIV_EQ		 27
# define N_DIV_EQ_INT		 28
# define N_DIV_EQ_FLOAT		 29
# define N_DO			 30
# define N_ELSE			 31
# define N_EQ			 32
# define N_EQ_INT		 33
# define N_EQ_FLOAT		 34
# define N_FAKE			 35
# define N_FLOAT		 36
# define N_FOR			 37
# define N_FOREVER		 38
# define N_FUNC			 39
# define N_GE			 40
# define N_GE_INT		 41
# define N_GE_FLOAT		 42
# define N_GLOBAL		 43
# define N_GT			 44
# define N_GT_INT		 45
# define N_GT_FLOAT		 46
# define N_IF			 47
# define N_INDEX		 48
# define N_INSTANCEOF		 49
# define N_INT			 50
# define N_LAND			 51
# define N_LE			 52
# define N_LE_INT		 53
# define N_LE_FLOAT		 54
# define N_LOCAL		 55
# define N_LOR			 56
# define N_LSHIFT		 57
# define N_LSHIFT_INT		 58
# define N_LSHIFT_EQ		 59
# define N_LSHIFT_EQ_INT	 60
# define N_LT			 61
# define N_LT_INT		 62
# define N_LT_FLOAT		 63
# define N_LVALUE		 64
# define N_MOD			 65
# define N_MOD_INT		 66
# define N_MOD_EQ		 67
# define N_MOD_EQ_INT		 68
# define N_MULT			 69
# define N_MULT_INT		 70
# define N_MULT_FLOAT		 71
# define N_MULT_EQ		 72
# define N_MULT_EQ_INT		 73
# define N_MULT_EQ_FLOAT	 74
# define N_NE			 75
# define N_NE_INT		 76
# define N_NE_FLOAT		 77
# define N_NIL			 78
# define N_NOT			 79
# define N_OR			 80
# define N_OR_INT		 81
# define N_OR_EQ		 82
# define N_OR_EQ_INT		 83
# define N_PAIR			 84
# define N_POP			 85
# define N_QUEST		 86
# define N_RANGE		 87
# define N_RETURN		 88
# define N_RLIMITS		 89
# define N_RSHIFT		 90
# define N_RSHIFT_INT		 91
# define N_RSHIFT_EQ		 92
# define N_RSHIFT_EQ_INT	 93
# define N_SPREAD		 94
# define N_STR			 95
# define N_SUB			 96
# define N_SUB_INT		 97
# define N_SUB_FLOAT		 98
# define N_SUB_EQ		 99
# define N_SUB_EQ_INT		100
# define N_SUB_EQ_FLOAT		101
# define N_SUB_EQ_1		102
# define N_SUB_EQ_1_INT		103
# define N_SUB_EQ_1_FLOAT	104
# define N_SUM			105
# define N_SUM_EQ		106
# define N_SWITCH_INT		107
# define N_SWITCH_RANGE		108
# define N_SWITCH_STR		109
# define N_TOFLOAT		110
# define N_TOINT		111
# define N_TOSTRING		112
# define N_TST			113
# define N_TYPE			114
# define N_VAR			115
# define N_XOR			116
# define N_XOR_INT		117
# define N_XOR_EQ		118
# define N_XOR_EQ_INT		119
# define N_MIN_MIN		120
# define N_MIN_MIN_INT		121
# define N_MIN_MIN_FLOAT	122
# define N_PLUS_PLUS		123
# define N_PLUS_PLUS_INT	124
# define N_PLUS_PLUS_FLOAT	125
# define N_NEG			126
# define N_UMIN			127
# define N_GOTO			128
# define N_LABEL		129

extern int nil_node;
