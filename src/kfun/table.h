typedef struct {
    char *name;		/* function name */
    int (*func)();	/* function address */
    char *proto;	/* prototype */
} kfunc;

extern kfunc kftab[];	/* kfun table */
extern char  kfind[];	/* kfun indirection table */

# define KFUN(kf)	(kftab[UCHAR(kfind[kf])])

extern void kf_init	P((void));
extern int  kf_func	P((char*));
extern bool kf_dump	P((int));
extern void kf_restore	P((int));

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
# define KF_NOTF	29
# define KF_NOTI	30
# define KF_OR		31
# define KF_OR_INT	32
# define KF_RANGEFT	33
# define KF_RANGEF	34
# define KF_RANGET	35
# define KF_RANGE	36
# define KF_RSHIFT	37
# define KF_RSHIFT_INT	38
# define KF_SUB		39
# define KF_SUB_INT	40
# define KF_SUB1	41
# define KF_SUB1_INT	42
# define KF_TOFLOAT	43
# define KF_TOINT	44
# define KF_TST		45
# define KF_TSTF	46
# define KF_TSTI	47
# define KF_UMIN	48
# define KF_UMIN_INT	49
# define KF_XOR		50
# define KF_XOR_INT	51

# define KF_BUILTINS	52
