/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2022 DGD Authors (see the commit log for details)
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

class Node : public ChunkAllocated {
public:
    void toint(LPCint i);
    void tostr(String *str);
    Node *revert();

    static void init(int);
    static void clear();

    static Node *create(unsigned short line);
    static Node *createInt(LPCint num);
    static Node *createFloat(Float *flt);
    static Node *createNil();
    static Node *createStr(String *str);
    static Node *createVar(unsigned int type, int idx);
    static Node *createType(int, String*);
    static Node *createFcall(int mod, String *tclass, char *func, LPCint call);
    static Node *createOp(const char *op);
    static Node *createMon(int type, int mod, Node *left);
    static Node *createBin(int type, int mod, Node *left, Node *right);
    static Node *revert(Node *n);

    unsigned char type;		/* type of node */
    char flags;			/* bitflags */
    unsigned short mod;		/* modifier */
    unsigned short line;	/* line number */
    String *sclass;		/* object class */
    union {
	LPCint number;		/* numeric value */
	FloatHigh fhigh;	/* high word of float */
	String *string;		/* string value */
	char *ptr;		/* character pointer */
	Node *left;		/* left child */
    } l;
    union {
	LPCint number;		/* numeric value */
	FloatLow flow;		/* low longword of float */
	Node *right;		/* right child */
    } r;

private:
    Node(unsigned short line);
};

# define NFLT_GET(n, f)	((f).high = (n)->l.fhigh, (f).low = (n)->r.flow)
# define NFLT_PUT(n, f)	((n)->l.fhigh = (f).high, (n)->r.flow = (f).low)
# define NFLT_ISZERO(n)	FLOAT_ISZERO((n)->l.fhigh, (n)->r.flow)
# define NFLT_ISONE(n)	FLOAT_ISONE((n)->l.fhigh, (n)->r.flow)
# define NFLT_ISMONE(n)	FLOAT_ISMONE((n)->l.fhigh, (n)->r.flow)

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
# define N_EXCEPTION		 35
# define N_FAKE			 36
# define N_FLOAT		 37
# define N_FOR			 38
# define N_FOREVER		 39
# define N_FUNC			 40
# define N_GE			 41
# define N_GE_INT		 42
# define N_GE_FLOAT		 43
# define N_GLOBAL		 44
# define N_GT			 45
# define N_GT_INT		 46
# define N_GT_FLOAT		 47
# define N_IF			 48
# define N_INDEX		 49
# define N_INSTANCEOF		 50
# define N_INT			 51
# define N_LAND			 52
# define N_LE			 53
# define N_LE_INT		 54
# define N_LE_FLOAT		 55
# define N_LOCAL		 56
# define N_LOR			 57
# define N_LSHIFT		 58
# define N_LSHIFT_INT		 59
# define N_LSHIFT_EQ		 60
# define N_LSHIFT_EQ_INT	 61
# define N_LT			 62
# define N_LT_INT		 63
# define N_LT_FLOAT		 64
# define N_LVALUE		 65
# define N_MOD			 66
# define N_MOD_INT		 67
# define N_MOD_EQ		 68
# define N_MOD_EQ_INT		 69
# define N_MULT			 70
# define N_MULT_INT		 71
# define N_MULT_FLOAT		 72
# define N_MULT_EQ		 73
# define N_MULT_EQ_INT		 74
# define N_MULT_EQ_FLOAT	 75
# define N_NE			 76
# define N_NE_INT		 77
# define N_NE_FLOAT		 78
# define N_NIL			 79
# define N_NOT			 80
# define N_OR			 81
# define N_OR_INT		 82
# define N_OR_EQ		 83
# define N_OR_EQ_INT		 84
# define N_PAIR			 85
# define N_POP			 86
# define N_QUEST		 87
# define N_RANGE		 88
# define N_RETURN		 89
# define N_RLIMITS		 90
# define N_RSHIFT		 91
# define N_RSHIFT_INT		 92
# define N_RSHIFT_EQ		 93
# define N_RSHIFT_EQ_INT	 94
# define N_SPREAD		 95
# define N_STR			 96
# define N_SUB			 97
# define N_SUB_INT		 98
# define N_SUB_FLOAT		 99
# define N_SUB_EQ		100
# define N_SUB_EQ_INT		101
# define N_SUB_EQ_FLOAT		102
# define N_SUB_EQ_1		103
# define N_SUB_EQ_1_INT		104
# define N_SUB_EQ_1_FLOAT	105
# define N_SUM			106
# define N_SUM_EQ		107
# define N_SWITCH_INT		108
# define N_SWITCH_RANGE		109
# define N_SWITCH_STR		110
# define N_TOFLOAT		111
# define N_TOINT		112
# define N_TOSTRING		113
# define N_TST			114
# define N_TYPE			115
# define N_VAR			116
# define N_XOR			117
# define N_XOR_INT		118
# define N_XOR_EQ		119
# define N_XOR_EQ_INT		120
# define N_MIN_MIN		121
# define N_MIN_MIN_INT		122
# define N_MIN_MIN_FLOAT	123
# define N_PLUS_PLUS		124
# define N_PLUS_PLUS_INT	125
# define N_PLUS_PLUS_FLOAT	126
# define N_NEG			127
# define N_UMIN			128
# define N_GOTO			129
# define N_LABEL		130

extern int nil_node;
