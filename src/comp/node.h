/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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
    char type;			/* type of node */
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
# define F_SIDEFX	0x02	/* expression has side effect */
# define F_ENTRY	0x04	/* (first) statement has case/default entry */
# define F_REACH	0x08	/* statement block has case/default entry */
# define F_BREAK	0x10	/* break */
# define F_CONTINUE	0x20	/* continue */
# define F_RETURN	0x40	/* return */
# define F_END		(F_BREAK | F_CONTINUE | F_RETURN)
# define F_FLOW		(F_ENTRY | F_REACH | F_END)
# define F_VARARGS	0x04	/* varargs in parameter list */
# define F_ELLIPSIS	0x08	/* ellipsis in parameter list */

extern void  node_init	P((int));
extern node *node_new	P((unsigned int));
extern node *node_int	P((Int));
extern node *node_float	P((xfloat*));
extern node *node_nil	P((void));
extern node *node_str	P((string*));
extern node *node_var	P((unsigned int, int));
extern node *node_type	P((int, string*));
extern node *node_fcall	P((int, string*, char*, Int));
extern node *node_mon	P((int, int, node*));
extern node *node_bin	P((int, int, node*, node*));
extern void  node_toint	P((node*, Int));
extern void  node_tostr	P((node*, string*));
extern void  node_free	P((void));
extern void  node_clear	P((void));

# define N_ADD			  1
# define N_ADD_INT		  2
# define N_ADD_EQ		  3
# define N_ADD_EQ_INT		  4
# define N_ADD_EQ_1		  5
# define N_ADD_EQ_1_INT		  6
# define N_AGGR			  7
# define N_AND			  8
# define N_AND_INT		  9
# define N_AND_EQ		 10
# define N_AND_EQ_INT		 11
# define N_ASSIGN		 12
# define N_BLOCK		 13
# define N_BREAK		 14
# define N_CASE			 15
# define N_CAST			 16
# define N_CATCH		 17
# define N_COMMA		 18
# define N_COMPOUND		 19
# define N_CONTINUE		 20
# define N_DIV			 21
# define N_DIV_INT		 22
# define N_DIV_EQ		 23
# define N_DIV_EQ_INT		 24
# define N_DO			 25
# define N_ELSE			 26
# define N_EQ			 27
# define N_EQ_INT		 28
# define N_FAKE			 29
# define N_FLOAT		 30
# define N_FOR			 31
# define N_FOREVER		 32
# define N_FUNC			 33
# define N_GE			 34
# define N_GE_INT		 35
# define N_GLOBAL		 36
# define N_GT			 37
# define N_GT_INT		 38
# define N_IF			 39
# define N_INDEX		 40
# define N_INSTANCEOF		 41
# define N_INT			 42
# define N_LAND			 43
# define N_LE			 44
# define N_LE_INT		 45
# define N_LOCAL		 46
# define N_LOR			 47
# define N_LSHIFT		 48
# define N_LSHIFT_INT		 49
# define N_LSHIFT_EQ		 50
# define N_LSHIFT_EQ_INT	 51
# define N_LT			 52
# define N_LT_INT		 53
# define N_LVALUE		 54
# define N_MOD			 55
# define N_MOD_INT		 56
# define N_MOD_EQ		 57
# define N_MOD_EQ_INT		 58
# define N_MULT			 59
# define N_MULT_INT		 60
# define N_MULT_EQ		 61
# define N_MULT_EQ_INT		 62
# define N_NE			 63
# define N_NE_INT		 64
# define N_NIL			 65
# define N_NOT			 66
# define N_OR			 67
# define N_OR_INT		 68
# define N_OR_EQ		 69
# define N_OR_EQ_INT		 70
# define N_PAIR			 71
# define N_POP			 72
# define N_QUEST		 73
# define N_RANGE		 74
# define N_RETURN		 75
# define N_RLIMITS		 76
# define N_RSHIFT		 77
# define N_RSHIFT_INT		 78
# define N_RSHIFT_EQ		 79
# define N_RSHIFT_EQ_INT	 80
# define N_SPREAD		 81
# define N_STR			 82
# define N_SUB			 83
# define N_SUB_INT		 84
# define N_SUB_EQ		 85
# define N_SUB_EQ_INT		 86
# define N_SUB_EQ_1		 87
# define N_SUB_EQ_1_INT		 88
# define N_SUM			 89
# define N_SUM_EQ		 90
# define N_SWITCH_INT		 91
# define N_SWITCH_RANGE		 92
# define N_SWITCH_STR		 93
# define N_TOFLOAT		 94
# define N_TOINT		 95
# define N_TOSTRING		 96
# define N_TST			 97
# define N_TYPE			 98
# define N_VAR			 99
# define N_XOR			100
# define N_XOR_INT		101
# define N_XOR_EQ		102
# define N_XOR_EQ_INT		103
# define N_MIN_MIN		104
# define N_MIN_MIN_INT		105
# define N_PLUS_PLUS		106
# define N_PLUS_PLUS_INT	107

extern int nil_node;
