typedef struct _node_ {
    char type;			/* type of node */
    char flags;			/* bitflags */
    unsigned short mod;		/* modifier */
    unsigned short line;	/* line number */
    union {
	Int number;		/* numeric value */
	unsigned short fhigh;	/* high word of float */
	struct _string_ *string;/* string value */
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
# define F_ENTRY	0x04	/* statement has case/default entry */
# define F_REACH	0x08	/* statement block has case/default entry */
# define F_BREAK	0x10	/* break */
# define F_CONT		0x20	/* continue */
# define F_RETURN	0x40	/* return */

extern node *node_int	P((Int));
extern node *node_float	P((xfloat*));
extern node *node_str	P((string*));
extern node *node_fcall	P((int, char*, Int));
extern node *node_mon	P((int, int, node*));
extern node *node_bin	P((int, int, node*, node*));
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
# define N_CONTINUE		 19
# define N_DIV			 20
# define N_DIV_INT		 21
# define N_DIV_EQ		 22
# define N_DIV_EQ_INT		 23
# define N_DO			 24
# define N_ELSE			 25
# define N_EQ			 26
# define N_EQ_INT		 27
# define N_FAKE			 28
# define N_FLOAT		 29
# define N_FOR			 30
# define N_FOREVER		 31
# define N_FUNC			 32
# define N_GE			 33
# define N_GE_INT		 34
# define N_GLOBAL		 35
# define N_GT			 36
# define N_GT_INT		 37
# define N_IF			 38
# define N_INDEX		 39
# define N_INT			 40
# define N_LAND			 41
# define N_LE			 42
# define N_LE_INT		 43
# define N_LOCAL		 44
# define N_LOCK			 45
# define N_LOR			 46
# define N_LSHIFT		 47
# define N_LSHIFT_INT		 48
# define N_LSHIFT_EQ		 49
# define N_LSHIFT_EQ_INT	 50
# define N_LT			 51
# define N_LT_INT		 52
# define N_LVALUE		 53
# define N_MOD			 54
# define N_MOD_INT		 55
# define N_MOD_EQ		 56
# define N_MOD_EQ_INT		 57
# define N_MULT			 58
# define N_MULT_INT		 59
# define N_MULT_EQ		 60
# define N_MULT_EQ_INT		 61
# define N_NE			 62
# define N_NE_INT		 63
# define N_NOT			 64
# define N_NOTF			 65
# define N_NOTI			 66
# define N_OR			 67
# define N_OR_INT		 68
# define N_OR_EQ		 69
# define N_OR_EQ_INT		 70
# define N_PAIR			 71
# define N_POP			 72
# define N_QUEST		 73
# define N_RANGE		 74
# define N_RETURN		 75
# define N_RSHIFT		 76
# define N_RSHIFT_INT		 77
# define N_RSHIFT_EQ		 78
# define N_RSHIFT_EQ_INT	 79
# define N_SPREAD		 80
# define N_STR			 81
# define N_SUB			 82
# define N_SUB_INT		 83
# define N_SUB_EQ		 84
# define N_SUB_EQ_INT		 85
# define N_SUB_EQ_1		 86
# define N_SUB_EQ_1_INT		 87
# define N_SWITCH_INT		 88
# define N_SWITCH_RANGE		 89
# define N_SWITCH_STR		 90
# define N_TOFLOAT		 91
# define N_TOINT		 92
# define N_TST			 93
# define N_TSTF			 94
# define N_TSTI			 95
# define N_UPLUS		 96
# define N_XOR			 97
# define N_XOR_INT		 98
# define N_XOR_EQ		 99
# define N_XOR_EQ_INT		100
# define N_MIN_MIN		101
# define N_MIN_MIN_INT		102
# define N_PLUS_PLUS		103
# define N_PLUS_PLUS_INT	104
