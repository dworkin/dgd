typedef struct _node_ {
    unsigned short type;	/* type of node */
    unsigned short mod;		/* modifier */
    unsigned short line;	/* line number */
    union {
	Int number;		/* numeric value */
	string *string;		/* string value */
	char *ptr;		/* character pointer */
	struct _node_ *left;	/* left child */
    } l;
    union {
	long number;		/* numeric value */
	struct _node_ *right;	/* right child */
    } r;
} node;

extern node *node_int	P((Int));
extern node *node_str	P((string*));
extern node *node_fcall	P((int, char*, Int));
extern node *node_mon	P((int, int, node*));
extern node *node_bin	P((int, int, node*, node*));
extern void  node_free	P((void));
extern void  node_clear	P((void));

# define N_ADD			 1
# define N_ADD_INT		 2
# define N_ADD_EQ		 3
# define N_ADD_EQ_INT		 4
# define N_AGGR			 5
# define N_AND			 6
# define N_AND_INT		 7
# define N_AND_EQ		 8
# define N_AND_EQ_INT		 9
# define N_ASSIGN		10
# define N_BLOCK		11
# define N_BREAK		12
# define N_CASE			13
# define N_CAST			14
# define N_CATCH		15
# define N_COMMA		16
# define N_CONTINUE		17
# define N_DIV			18
# define N_DIV_INT		19
# define N_DIV_EQ		20
# define N_DIV_EQ_INT		21
# define N_DO			22
# define N_ELSE			23
# define N_EQ			24
# define N_EQ_INT		25
# define N_FAKE			26
# define N_FOR			27
# define N_FOREVER		28
# define N_FUNC			29
# define N_GE			30
# define N_GE_INT		31
# define N_GLOBAL		32
# define N_GT			33
# define N_GT_INT		34
# define N_IF			35
# define N_INDEX		36
# define N_INT			37
# define N_LAND			38
# define N_LE			39
# define N_LE_INT		40
# define N_LOCAL		41
# define N_LOCK			42
# define N_LOR			43
# define N_LSHIFT		44
# define N_LSHIFT_INT		45
# define N_LSHIFT_EQ		46
# define N_LSHIFT_EQ_INT	47
# define N_LT			48
# define N_LT_INT		49
# define N_LVALUE		50
# define N_MOD			51
# define N_MOD_INT		52
# define N_MOD_EQ		53
# define N_MOD_EQ_INT		54
# define N_MULT			55
# define N_MULT_INT		56
# define N_MULT_EQ		57
# define N_MULT_EQ_INT		58
# define N_NE			59
# define N_NE_INT		60
# define N_NOT			61
# define N_NOT_INT		62
# define N_OR			63
# define N_OR_INT		64
# define N_OR_EQ		65
# define N_OR_EQ_INT		66
# define N_PAIR			67
# define N_POP			68
# define N_QUEST		69
# define N_RANGE		70
# define N_RETURN		71
# define N_RSHIFT		72
# define N_RSHIFT_INT		73
# define N_RSHIFT_EQ		74
# define N_RSHIFT_EQ_INT	75
# define N_STR			76
# define N_SUB			77
# define N_SUB_INT		78
# define N_SUB_EQ		79
# define N_SUB_EQ_INT		80
# define N_SWITCH_INT		81
# define N_SWITCH_RANGE		82
# define N_SWITCH_STR		83
# define N_TST			84
# define N_WHILE		85
# define N_XOR			86
# define N_XOR_INT		87
# define N_XOR_EQ		88
# define N_XOR_EQ_INT		89
# define N_MIN_MIN		90
# define N_MIN_MIN_INT		91
# define N_PLUS_PLUS		92
# define N_PLUS_PLUS_INT	93
