# define I_INSTR_MASK		0x1f	/* instruction mask */

# define I_PUSH_ZERO		 0
# define I_PUSH_ONE		 1
# define I_PUSH_INT1		 2	/* 1 signed */
# define I_PUSH_INT4		 3	/* 4 signed */
# define I_PUSH_FLOAT		 4	/* 6 unsigned */
# define I_PUSH_STRING		 5	/* 1 unsigned */
# define I_PUSH_NEAR_STRING	 6	/* 1 unsigned, 1 unsigned */
# define I_PUSH_FAR_STRING	 7	/* 1 unsigned, 2 unsigned */
# define I_PUSH_LOCAL		 8	/* 1 signed */
# define I_PUSH_GLOBAL		 9	/* 1 unsigned, 1 unsigned */
# define I_PUSH_LOCAL_LVALUE	10	/* 1 signed */
# define I_PUSH_GLOBAL_LVALUE	11	/* 1 unsigned, 1 unsigned */
# define I_INDEX		12
# define I_INDEX_LVALUE		13
# define I_AGGREGATE		14	/* 2 unsigned */
# define I_MAP_AGGREGATE	15	/* 2 unsigned */
# define I_SPREAD		16	/* 1 signed */
# define I_CAST			17	/* 1 unsigned */
# define I_FETCH		18
# define I_STORE		19
# define I_JUMP			20	/* 2 unsigned */
# define I_JUMP_ZERO		21	/* 2 unsigned */
# define I_JUMP_NONZERO		22	/* 2 unsigned */
# define I_SWITCH		23	/* n */
# define I_CALL_KFUNC		24	/* 1 unsigned (+ 1 unsigned) */
# define I_CALL_IKFUNC		25	/* 1 unsigned (+ 1 unsigned) */
# define I_CALL_DFUNC		26	/* 1 unsigned, 1 unsigned, 1 unsigned */
# define I_CALL_IDFUNC		27	/* 1 unsigned, 1 unsigned, 1 unsigned */
# define I_CALL_FUNC		28	/* 2 unsigned, 1 unsigned */
# define I_CATCH		29	/* 2 signed */
# define I_RLIMITS		30
# define I_RETURN		31

# define I_LINE_MASK		0xc0	/* line add bits */
# define I_POP_BIT		0x20	/* pop 1 after instruction */
# define I_LINE_SHIFT		6


# define T_TYPE		0x0f	/* type mask */
# define T_INVALID	0x00
# define T_INT		0x01
# define T_FLOAT	0x02
# define T_STRING	0x03
# define T_OBJECT	0x04
# define T_ARRAY	0x05	/* value type only */
# define T_MAPPING	0x06
# define T_RESERVED	0x07	/* reserved for add-on packages */
# define T_MIXED	0x08	/* declaration type only */
# define T_VOID		0x09	/* function return type only */
# define T_LVALUE	0x0a	/* address of a value */
# define T_SLVALUE	0x0b	/* indexed string lvalue */
# define T_ALVALUE	0x0c	/* indexed array lvalue */
# define T_MLVALUE	0x0d	/* indexed mapping lvalue */
# define T_SALVALUE	0x0e	/* indexed string indexed array lvalue */
# define T_SMLVALUE	0x0f	/* indexed string indexed mapping lvalue */

# define T_ELLIPSIS	0x10	/* or'ed with declaration type */

# define T_REF		0xe0	/* reference count mask */
# define REFSHIFT	5

# define T_ARITHMETIC(t) ((t) <= T_FLOAT)
# define T_ARITHSTR(t)	((t) <= T_STRING)
# define T_INDEXED(t)	((t) >= T_ARRAY)	/* only T_ARRAY and T_MAPPING */

# define TYPENAMES	{ "invalid", "int", "float", "string", "object", \
			  "array", "mapping", "reserved", "mixed", "void" }

typedef struct _value_ {
    char type;			/* value type */
    bool modified;		/* dirty bit */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Uint objcnt;		/* object creation count */
	string *string;		/* string */
	array *array;		/* array or mapping */
	struct _value_ *lval;	/* lvalue: variable */
    } u;
} value;

# define VFLT_GET(v, f)	((f).high = (v)->oindex, (f).low = (v)->u.objcnt)
# define VFLT_PUT(v, f)	((v)->oindex = (f).high, (v)->u.objcnt = (f).low)
# define VFLT_ISZERO(v)	FLT_ISZERO((v)->oindex, (v)->u.objcnt)
# define VFLT_ISONE(v)	FLT_ISONE((v)->oindex, (v)->u.objcnt)
# define VFLT_ONE(v)	FLT_ONE((v)->oindex, (v)->u.objcnt)
# define VFLT_ABS(v)	FLT_ABS((v)->oindex, (v)->u.objcnt)
# define VFLT_NEG(v)	FLT_NEG((v)->oindex, (v)->u.objcnt)
# define VFLT_HASH(v)	((v)->oindex ^ (v)->u.objcnt)

# define DESTRUCTED(v)	(o_object((v)->oindex, (v)->u.objcnt) == (object*) NULL)


# define C_PRIVATE	0x01
# define C_STATIC	0x02
# define C_NOMASK	0x04
# define C_VARARGS	0x08
# define C_RESERVED	0x10		/* reserved for add-on packages */
# define C_TYPECHECKED	0x20
# define C_COMPILED	0x40
# define C_UNDEFINED	0x80


typedef struct _frame_ {
    struct _frame_ *prev;	/* previous stack frame */
    object *obj;		/* current object */
    struct _control_ *ctrl;	/* object control block */
    struct _dataspace_ *data;	/* dataspace of current object */
    struct _control_ *p_ctrl;	/* program control block */
    unsigned short p_index;	/* program index */
    Int depth;			/* stack depth */
    bool external;		/* TRUE if it's an external call */
    bool sos;			/* stack on stack */
    uindex foffset;		/* program function offset */
    struct _dfuncdef_ *func;	/* current function */
    char *prog;			/* start of program */
    char *pc;			/* program counter */
    unsigned short nargs;	/* # arguments */
    value *stack;		/* local value stack */
    value *ilvp;		/* old indexed lvalue pointer */
    value *argp;		/* argument pointer (old sp) */
    value *fp;			/* frame pointer (at end of local stack) */
} frame;


extern void		i_init		P((char*));
extern void		i_ref_value	P((value*));
extern void		i_del_value	P((value*));
extern void		i_grow_stack	P((int));
extern void		i_push_value	P((value*));
extern void		i_pop		P((int));
extern void		i_odest		P((object*));
extern void		i_string	P((int, unsigned int));
extern void		i_aggregate	P((unsigned int));
extern void		i_map_aggregate	P((unsigned int));
extern int		i_spread	P((int));
extern void		i_global	P((int, int));
extern void		i_global_lvalue	P((int, int));
extern void		i_index		P((void));
extern void		i_index_lvalue	P((void));
extern char	       *i_typename	P((unsigned int));
extern void		i_cast		P((value*, unsigned int));
extern void		i_fetch		P((void));
extern void		i_store		P((value*, value*));
extern Int		i_get_depth	P((void));
extern Int		i_get_ticks	P((void));
extern void		i_check_rlimits	P((void));
extern int		i_set_rlimits	P((Int, Int));
extern int		i_get_rllevel	P((void));
extern void		i_set_rllevel	P((int));
extern void		i_set_sp	P((value*));
extern object	       *i_prev_object	P((int));
extern void		i_typecheck	P((char*, char*, char*, int, int));
extern void		i_cleanup	P((void));
extern void		i_funcall	P((object*, int, int, int));
extern bool		i_call		P((object*, char*, int, int));
extern array	       *i_call_trace	P((void));
extern bool		i_call_critical	P((char*, int, int));
extern void		i_runtime_error	P((int));
extern void		i_clear		P((void));

extern frame *cframe;
extern value *sp;
extern Int ticks;

# define i_add_ticks(e)	(ticks -= (e))
