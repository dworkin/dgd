typedef struct {
    char *name;			/* function name */
    int (*func)();		/* function address */
    char *proto;		/* prototype */
} kfunc;

extern kfunc kftab[];		/* kfun table */

extern void kf_init	P((void));
extern int  kf_func	P((char*));

# define KF_ADD		 0
# define KF_ADD_INT	 1
# define KF_AND		 2
# define KF_AND_INT	 3
# define KF_DIV		 4
# define KF_DIV_INT	 5
# define KF_EQ		 6
# define KF_EQ_INT	 7
# define KF_GE		 8
# define KF_GE_INT	 9
# define KF_GT		10
# define KF_GT_INT	11
# define KF_LE		12
# define KF_LE_INT	13
# define KF_LSHIFT	14
# define KF_LSHIFT_INT	15
# define KF_LT		16
# define KF_LT_INT	17
# define KF_MOD		18
# define KF_MOD_INT	19
# define KF_MULT	20
# define KF_MULT_INT	21
# define KF_NE		22
# define KF_NE_INT	23
# define KF_NEG		24
# define KF_NEG_INT	25
# define KF_NOT		26
# define KF_OR		27
# define KF_OR_INT	28
# define KF_RANGE	29
# define KF_RSHIFT	30
# define KF_RSHIFT_INT	31
# define KF_SUB		32
# define KF_SUB_INT	33
# define KF_TST		34
# define KF_UMIN	35
# define KF_UMIN_INT	36
# define KF_XOR		37
# define KF_XOR_INT	38

# define KF_BUILTINS	39
