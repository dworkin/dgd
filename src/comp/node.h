typedef struct _node_ {
    unsigned short type;	/* type of node */
    unsigned short mod;		/* modifier */
    unsigned short line;	/* line number */
    union {
	long number;		/* numeric value */
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
extern node *node_fcall	P((int, char*, long));
extern node *node_mon	P((int, int, node*));
extern node *node_bin	P((int, int, node*, node*));
extern void  node_free	P((void));
extern void  node_clear	P((void));

# define N_ADD			 0
# define N_ADD_EQ		 1
# define N_AGGR			 2
# define N_AND			 3
# define N_AND_EQ		 4
# define N_ASSIGN		 5
# define N_BLOCK		 6
# define N_BREAK		 7
# define N_CASE			 8
# define N_CAST			 9
# define N_CATCH		10
# define N_COMMA		11
# define N_CONTINUE		12
# define N_DIV			13
# define N_DIV_EQ		14
# define N_DO			15
# define N_ELSE			16
# define N_EQ			17
# define N_FAKE			18
# define N_FOR			19
# define N_FOREVER		20
# define N_FUNC			21
# define N_GE			22
# define N_GLOBAL		23
# define N_GLOBAL_LVALUE	24
# define N_GT			25
# define N_IF			26
# define N_INDEX		27
# define N_INDEX_LVALUE		28
# define N_INT			29
# define N_LAND			30
# define N_LE			31
# define N_LOCAL		32
# define N_LOCAL_LVALUE		33
# define N_LOCK			34
# define N_LOR			35
# define N_LSHIFT		36
# define N_LSHIFT_EQ		37
# define N_LT			38
# define N_MIN			39
# define N_MIN_EQ		40
# define N_MOD			41
# define N_MOD_EQ		42
# define N_MULT			43
# define N_MULT_EQ		44
# define N_NE			45
# define N_NEG			46
# define N_NOT			47
# define N_OR			48
# define N_OR_EQ		49
# define N_PAIR			50
# define N_POP			51
# define N_QUEST		52
# define N_RANGE		53
# define N_RETURN		54
# define N_RSHIFT		55
# define N_RSHIFT_EQ		56
# define N_STR			57
# define N_SWITCH_INT		58
# define N_SWITCH_RANGE		59
# define N_SWITCH_STR		60
# define N_TST			61
# define N_UMIN			62
# define N_WHILE		63
# define N_XOR			64
# define N_XOR_EQ		65
# define N_MIN_MIN		66
# define N_PLUS_PLUS		67
