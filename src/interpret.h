# define I_INSTR_MASK		0x1f	/* instruction mask */

# define I_PUSH_ZERO		 0
# define I_PUSH_ONE		 1
# define I_PUSH_INT1		 2	/* 1 signed */
# define I_PUSH_INT2		 3	/* 2 signed */
# define I_PUSH_INT4		 4	/* 4 signed */
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
# define I_CHECK_INT		16
# define I_FETCH		17
# define I_STORE		18
# define I_JUMP			19	/* 2 signed */
# define I_JUMP_ZERO		20	/* 2 signed */
# define I_JUMP_NONZERO		21	/* 2 signed */
# define I_SWITCH_INT		22	/* n */
# define I_SWITCH_RANGE		23	/* n */
# define I_SWITCH_STR		24	/* n */
# define I_CALL_KFUNC		25	/* 1 unsigned (+ 1 unsigned) */
# define I_CALL_LFUNC		26	/* 1 unsigned, 1 unsigned, 1 unsigned,
					   1 unsigned */
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
# define T_NUMBER	0x01
# define T_OBJECT	0x02
# define T_STRING	0x03
# define T_ARRAY	0x04	/* value type only */
# define T_MAPPING	0x05
# define T_MIXED	0x06	/* declaration type only */
# define T_VOID		0x07	/* function return type only */
# define T_LVALUE	0x08	/* address of a value */
# define T_SLVALUE	0x09	/* indexed string lvalue */
# define T_ALVALUE	0x0a	/* indexed array lvalue */
# define T_MLVALUE	0x0b	/* indexed mapping lvalue */
# define T_SALVALUE	0x0c	/* indexed string indexed array lvalue */
# define T_SMLVALUE	0x0d	/* indexed string indexed mapping lvalue */

# define T_REF		0xf0	/* reference count mask */
# define REFSHIFT	4

# define TYPENAMES	\
{ "invalid", "int", "object", "string", "array", "mapping", "mixed", "void" }

typedef struct _objkey_ {
    uindex index;		/* index in object table */
    long count;			/* object creation count */
} objkey;

typedef struct _value_ {
    char type;			/* value type */
    bool modified;		/* dirty bit */
    union {
	objkey object;		/* object */
	Int number;		/* number */
	struct _string_ *string;/* string */
	struct _array_ *array;	/* array or mapping */
	struct _value_ *lval;	/* lvalue: variable */
    } u;
} value;


# define C_PRIVATE	0x01
# define C_STATIC	0x02
# define C_LOCAL	0x04
# define C_NOMASK	0x08
# define C_VARARGS	0x10
# define C_TYPECHECKED	0x20
# define C_COMPILED	0x40
# define C_UNDEFINED	0x80


typedef struct _dataspace_ dataspace;

extern void		i_init		P((int, int, int, long, char*));
extern void		i_ref_value	P((value*));
extern void		i_del_value	P((value*));
extern void		i_check_stack	P((int));
extern void		i_push_value	P((value*));
extern void		i_pop		P((int));
extern void		i_odest		P((objkey*));
extern void		i_index		P((value*, value*));
extern void		i_index_lvalue	P((value*, value*));
extern void		i_store		P((dataspace*, value*, value*));
extern void		i_add_ticks	P((int));
extern void		i_lock		P((void));
extern void		i_unlock	P((void));
extern unsigned short	i_locklvl	P((void));
extern struct _object_ *i_this_object	P((void));
extern struct _object_ *i_prev_object	P((int));
extern bool		i_call		P((struct _object_*, char*, bool, int));
extern void		i_dump_trace	P((FILE*));
extern void		i_log_error	P((void));
extern void		i_clear		P((void));

extern value *sp;
