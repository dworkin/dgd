# define I_INSTR_MASK		0x1f	/* instruction mask */

# define I_PUSH_ZERO		 0
# define I_PUSH_ONE		 1
# define I_PUSH_INT1		 2	/* 1 signed */
# define I_PUSH_INT4		 3	/* 4 signed */
# define I_PUSH_FLOAT2		 4	/* 2 unsigned */
# define I_PUSH_FLOAT6		 5	/* 6 unsigned */
# define I_PUSH_STRING		 6	/* 1 unsigned */
# define I_PUSH_NEAR_STRING	 7	/* 1 unsigned, 1 unsigned */
# define I_PUSH_FAR_STRING	 8	/* 1 unsigned, 2 unsigned */
# define I_PUSH_LOCAL		 9	/* 1 signed */
# define I_PUSH_GLOBAL		10	/* 1 unsigned, 1 unsigned */
# define I_PUSH_LOCAL_LVALUE	11	/* 1 signed */
# define I_PUSH_GLOBAL_LVALUE	12	/* 1 unsigned, 1 unsigned */
# define I_INDEX		13
# define I_INDEX_LVALUE		14
# define I_AGGREGATE		15	/* 2 unsigned */
# define I_MAP_AGGREGATE	16	/* 2 unsigned */
# define I_SPREAD		17	/* 1 signed */
# define I_CAST			18	/* 1 unsigned */
# define I_FETCH		19
# define I_STORE		20
# define I_JUMP			21	/* 2 signed */
# define I_JUMP_ZERO		22	/* 2 signed */
# define I_JUMP_NONZERO		23	/* 2 signed */
# define I_SWITCH		24	/* n */
# define I_CALL_KFUNC		25	/* 1 unsigned (+ 1 unsigned) */
# define I_CALL_AFUNC		26	/* 1 unsigned, 1 unsigned */
# define I_CALL_DFUNC		27	/* 1 unsigned, 1 unsigned, 1 unsigned */
# define I_CALL_FUNC		28	/* 2 unsigned, 1 unsigned */
# define I_CATCH		29	/* 2 signed */
# define I_LOCK			30
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
# define T_MIXED	0x07	/* declaration type only */
# define T_VOID		0x08	/* function return type only */
# define T_LVALUE	0x09	/* address of a value */
# define T_SLVALUE	0x0a	/* indexed string lvalue */
# define T_ALVALUE	0x0b	/* indexed array lvalue */
# define T_MLVALUE	0x0c	/* indexed mapping lvalue */
# define T_SALVALUE	0x0d	/* indexed string indexed array lvalue */
# define T_SMLVALUE	0x0e	/* indexed string indexed mapping lvalue */

# define T_ELLIPSIS	0x10	/* or'ed with declaration type */

# define T_REF		0xe0	/* reference count mask */
# define REFSHIFT	5

# define T_ARITHMETIC(t) ((t) <= T_FLOAT)
# define T_ARITHSTR(t)	((t) <= T_STRING)
# define T_INDEXED(t)	((t) >= T_ARRAY)	/* only T_ARRAY and T_MAPPING */

# define TYPENAMES	{ "invalid", "int", "float", "string", "object", \
			  "array", "mapping", "mixed", "void" }

typedef struct _value_ {
    char type;			/* value type */
    bool modified;		/* dirty bit */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Int objcnt;		/* object creation count */
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
# define C_LOCAL	0x04
# define C_NOMASK	0x08
# define C_VARARGS	0x10
# define C_TYPECHECKED	0x20
# define C_COMPILED	0x40
# define C_UNDEFINED	0x80


extern void		i_init		P((int, int, int, int, char*));
extern void		i_ref_value	P((value*));
extern void		i_del_value	P((value*));
extern void		i_check_stack	P((int));
extern void		i_push_value	P((value*));
extern void		i_pop		P((int));
extern void		i_odest		P((object*));
extern void		i_string	P((char, unsigned short));
extern void		i_aggregate	P((unsigned short));
extern void		i_map_aggregate	P((unsigned short));
extern int		i_spread	P((int));
extern void		i_global	P((int, int));
extern void		i_global_lvalue	P((int, int));
extern void		i_index		P((void));
extern void		i_index_lvalue	P((void));
extern char	       *i_typename	P((unsigned short));
extern void		i_cast		P((value*, unsigned short));
extern void		i_fetch		P((void));
extern void		i_store		P((value*, value*));
extern void		i_set_cost	P((Int));
extern Int		i_reset_cost	P((void));
extern void		i_lock		P((void));
extern void		i_unlock	P((void));
extern void		i_set_lock	P((unsigned short));
extern unsigned short	i_query_lock	P((void));
extern void		i_set_frame	P((int));
extern int		i_query_frame	P((void));
extern struct _control_*i_this_program	P((void));
extern object	       *i_this_object	P((void));
extern object	       *i_prev_object	P((int));
extern char	       *i_foffset	P((unsigned short));
extern int		i_pindex	P((void));
extern void		i_typecheck	P((char*, char*, char*, int, bool));
extern void		i_funcall	P((object*, int, int, int));
extern bool		i_call		P((object*, char*, bool, int));
extern array	       *i_call_trace	P((void));
extern void		i_log_error	P((bool));
extern void		i_clear		P((void));

extern value *sp;
extern Int exec_cost;

# define i_add_cost(e)	(exec_cost -= (e))
