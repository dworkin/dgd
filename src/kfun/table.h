typedef struct {
    char *name;			/* function name */
    int (*func)();		/* function address */
    char *proto;		/* prototype */
} kfunc;

kfunc kftab[];			/* kfun table */

extern void kf_init	P((void));
extern int  kf_func	P((char*));

# define KF_ADD		 0
# define KF_AND		 1
# define KF_DIV		 2
# define KF_EQ		 3
# define KF_GE		 4
# define KF_GT		 5
# define KF_LE		 6
# define KF_LSHIFT	 7
# define KF_LT		 8
# define KF_MOD		 9
# define KF_MULT	10
# define KF_NE		11
# define KF_NEG		12
# define KF_NOT		13
# define KF_OR		14
# define KF_RANGE	15
# define KF_RSHIFT	16
# define KF_SUB		17
# define KF_TST		18
# define KF_UMIN	19
# define KF_XOR		20

# define KF_BUILTINS	21
