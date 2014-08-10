/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2013 DGD Authors (see the commit log for details)
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

# define I_INSTR_MASK		0x1f	/* instruction mask */

# define II_PUSH_ZERO		  0
# define II_PUSH_ONE		  1
# define II_PUSH_INT1		  2	/* 1 signed */
# define II_PUSH_INT4		  3	/* 4 signed */
# define II_PUSH_FLOAT		  4	/* 6 unsigned */
# define II_PUSH_STRING		  5	/* 1 unsigned */
# define II_PUSH_NEAR_STRING	  6	/* 1 unsigned, 1 unsigned */
# define II_PUSH_FAR_STRING	  7	/* 1 unsigned, 2 unsigned */
# define II_PUSH_LOCAL		  8	/* 1 signed */
# define II_PUSH_GLOBAL		  9	/* 1 unsigned */
# define II_PUSH_FAR_GLOBAL	 10	/* 1 unsigned, 1 unsigned */
# define II_PUSH_LOCAL_LVAL	 11	/* 1 signed */
# define II_PUSH_GLOBAL_LVAL	 12	/* 1 unsigned */
# define II_PUSH_FAR_GLOBAL_LVAL 13	/* 1 unsigned, 1 unsigned */
# define II_INDEX		 14
# define II_INDEX_LVAL		 15
# define II_AGGREGATE		 16	/* 1 unsigned, 2 unsigned */
# define II_SPREAD		 17	/* 1 signed */
# define II_CAST		 18	/* 1 unsigned */
# define II_DUP			 19
# define II_STORE		 20
# define II_JUMP		 21	/* 2 unsigned */
# define II_JUMP_ZERO		 22	/* 2 unsigned */
# define II_JUMP_NONZERO	 23	/* 2 unsigned */
# define II_SWITCH		 24	/* n */
# define II_CALL_KFUNC		 25	/* 1 unsigned (+ 1 unsigned) */
# define II_CALL_AFUNC		 26	/* 1 unsigned, 1 unsigned */
# define II_CALL_DFUNC		 27	/* 1 unsigned, 1 unsigned, 1 unsigned */
# define II_CALL_FUNC		 28	/* 2 unsigned, 1 unsigned */
# define II_CATCH		 29	/* 2 unsigned */
# define II_RLIMITS		 30
# define II_RETURN		 31

# define I_EINSTR_MASK		0x3f	/* extended instruction mask */

# define I_PUSH_INT1		0x00	/* 1 signed */
# define I_PUSH_INT2		0x20	/* 2 signed */
# define I_PUSH_INT4		0x01	/* 4 signed */
# define I_PUSH_INT8		0x21	/* reserved */
# define I_PUSH_FLOAT6		0x03	/* 6 unsigned */
# define I_PUSH_FLOAT12		0x23	/* reserved */
# define I_PUSH_STRING		0x04	/* 1 unsigned */
# define I_PUSH_NEAR_STRING	0x24	/* 1 unsigned, 1 unsigned */
# define I_PUSH_FAR_STRING	0x05	/* 1 unsigned, 2 unsigned */
# define I_PUSH_LOCAL		0x25	/* 1 signed */
# define I_PUSH_GLOBAL		0x06	/* 1 unsigned */
# define I_PUSH_FAR_GLOBAL	0x26	/* 1 unsigned, 1 unsigned */
# define I_INDEX		0x07
# define I_INDEX2		0x08
# define I_SPREAD		0x28	/* 1 signed (+ 1+3 unsigned) */
# define I_AGGREGATE		0x09	/* 1 unsigned, 2 unsigned */
# define I_CAST			0x0a	/* 1+3 unsigned */
# define I_CALL_EFUNC		0x0e	/* 2 unsigned (+ 1 unsigned) */
# define I_CALL_CEFUNC		0x0f	/* 2 unsigned, 1 unsigned */
# define I_CALL_CKFUNC		0x10	/* 1 unsigned, 1 unsigned */
# define I_STORE_LOCAL		0x11	/* 1 signed */
# define I_STORE_GLOBAL		0x12	/* 1 unsigned */
# define I_STORE_FAR_GLOBAL	0x13	/* 1 unsigned, 1 unsigned */
# define I_STORE_INDEX		0x14
# define I_STORE_LOCAL_INDEX	0x15	/* 1 signed */
# define I_STORE_GLOBAL_INDEX	0x16	/* 1 unsigned, 1 unsigned */
# define I_STORE_INDEX_INDEX	0x17
# define I_JUMP_ZERO		0x18	/* 2 unsigned */
# define I_JUMP_NONZERO		0x38	/* 2 unsigned */
# define I_JUMP			0x19	/* 2 unsigned */
# define I_SWITCH		0x39	/* n */
# define I_CALL_KFUNC		0x1a	/* 1 unsigned (+ 1 unsigned) */
# define I_CALL_AFUNC		0x1b	/* 1 unsigned, 1 unsigned */
# define I_CALL_DFUNC		0x1c	/* 1 unsigned, 1 unsigned, 1 unsigned */
# define I_CALL_FUNC		0x1d	/* 2 unsigned, 1 unsigned */
# define I_CATCH		0x1e	/* 2 unsigned */
# define I_RLIMITS		0x1f
# define I_RETURN		0x3f

# define I_LINE_MASK		0xc0	/* line add bits */
# define I_POP_BIT		0x20	/* pop 1 after instruction */
# define I_TYPE_BIT		I_POP_BIT /* lvalue typechecks assignment */
# define I_LINE_SHIFT		6

# define LVAL_LOCAL		0
# define LVAL_GLOBAL		1
# define LVAL_INDEX		2
# define LVAL_LOCAL_INDEX	3
# define LVAL_GLOBAL_INDEX	4
# define LVAL_INDEX_INDEX	5


# define FETCH1S(pc)	SCHAR(*(pc)++)
# define FETCH1U(pc)	UCHAR(*(pc)++)
# define FETCH2S(pc, v)	((short) (v = *(pc)++ << 8, v |= UCHAR(*(pc)++)))
# define FETCH2U(pc, v)	((unsigned short) (v = *(pc)++ << 8, \
					   v |= UCHAR(*(pc)++)))
# define FETCH3S(pc, v)	((Int) (v = *(pc)++ << 8, \
				v |= UCHAR(*(pc)++), v <<= 8, \
				v |= UCHAR(*(pc)++)))
# define FETCH3U(pc, v)	((Uint) (v = UCHAR(*(pc)++) << 8, \
				 v |= UCHAR(*(pc)++), v <<= 8, \
				 v |= UCHAR(*(pc)++)))
# define FETCH4S(pc, v)	((Int) (v = *(pc)++ << 8, \
				v |= UCHAR(*(pc)++), v <<= 8, \
				v |= UCHAR(*(pc)++), v <<= 8, \
				v |= UCHAR(*(pc)++)))
# define FETCH4U(pc, v)	((Uint) (v = *(pc)++ << 8, \
				 v |= UCHAR(*(pc)++), v <<= 8, \
				 v |= UCHAR(*(pc)++), v <<= 8, \
				 v |= UCHAR(*(pc)++)))


# define T_TYPE		0x0f	/* type mask */
# define T_NIL		0x00
# define T_INT		0x01
# define T_FLOAT	0x02
# define T_STRING	0x03
# define T_OBJECT	0x04
# define T_ARRAY	0x05	/* value type only */
# define T_MAPPING	0x06

# ifdef TYPEOFDETAIL        /* Mask for Detailed Typeof */

# ifdef CLOSURES
# define T_FUNCTION 0x07    /* Closure builtin mask */
# endif

# endif

# define T_LWOBJECT	0x07	/* runtime only */
# define T_CLASS	0x07	/* typechecking only */
# define T_MIXED	0x08	/* declaration type only */
# define T_VOID		0x09	/* function return type only */
# define T_LVALUE	0x0a	/* address of a value */
# define T_SLVALUE	0x0b	/* indexed string lvalue */
# define T_ALVALUE	0x0c	/* indexed array lvalue */
# define T_MLVALUE	0x0d	/* indexed mapping lvalue */
# define T_SALVALUE	0x0e	/* indexed string indexed array lvalue */
# define T_SMLVALUE	0x0f	/* indexed string indexed mapping lvalue */

# define T_VARARGS	0x10	/* or'ed with declaration type */
# define T_ELLIPSIS	0x10	/* or'ed with declaration type */

# define T_REF		0xf0	/* reference count mask */
# define REFSHIFT	4

# define T_ARITHMETIC(t) ((t) <= T_FLOAT)
# define T_ARITHSTR(t)	((t) <= T_STRING)
# define T_POINTER(t)	((t) >= T_STRING)
# define T_INDEXED(t)	((t) >= T_ARRAY)   /* T_ARRAY, T_MAPPING, T_LWOBJECT */

# define TYPENAMES	{ "nil", "int", "float", "string", "object", \
			  "array", "mapping", "object", "mixed", "void" }
# define TNBUFSIZE	24

# define VAL_NIL(v)	((v)->type == nil_type && (v)->u.number == 0)
# define VAL_TRUE(v)	((v)->u.number != 0 || (v)->type > T_FLOAT ||	\
			 ((v)->type == T_FLOAT && (v)->oindex != 0))

# define PUSH_INTVAL(f, i)	((--(f)->sp)->u.number = (i),		\
				 (f)->sp->type = T_INT)
# define PUT_INTVAL(v, i)	((v)->u.number = (i), (v)->type = T_INT)
# define PUT_INT(v, i)		((v)->u.number = (i))
# define PUSH_FLTVAL(f, fl)	((--(f)->sp)->oindex = (fl).high,	\
				 (f)->sp->u.objcnt = (fl).low,		\
				 (f)->sp->type = T_FLOAT)
# define PUSH_FLTCONST(f, h, l)	((--(f)->sp)->oindex = (h),		\
				 (f)->sp->u.objcnt = (l),		\
				 (f)->sp->type = T_FLOAT)
# define PUT_FLTVAL(v, fl)	((v)->oindex = (fl).high,		\
				 (v)->u.objcnt = (fl).low,		\
				 (v)->type = T_FLOAT)
# define PUT_FLT(v, fl)		((v)->oindex = (fl).high,		\
				 (v)->u.objcnt = (fl).low)
# define GET_FLT(v, fl)		((fl).high = (v)->oindex,		\
				 (fl).low = (v)->u.objcnt)
# define PUSH_STRVAL(f, s)	(str_ref((--(f)->sp)->u.string = (s)),	\
				 (f)->sp->type = T_STRING)
# define PUT_STRVAL(v, s)	(str_ref((v)->u.string = (s)),		\
				 (v)->type = T_STRING)
# define PUT_STRVAL_NOREF(v, s)	((v)->u.string = (s), (v)->type = T_STRING)
# define PUT_STR(v, s)		(str_ref((v)->u.string = (s)))
# define PUSH_OBJVAL(f, o)	((--(f)->sp)->oindex = (o)->index,	\
				 (f)->sp->u.objcnt = (o)->count,	\
				 (f)->sp->type = T_OBJECT)
# define PUT_OBJVAL(v, o)	((v)->oindex = (o)->index,		\
				 (v)->u.objcnt = (o)->count,		\
				 (v)->type = T_OBJECT)
# define PUT_OBJ(v, o)		((v)->oindex = (o)->index,		\
				 (v)->u.objcnt = (o)->count)
# define PUSH_ARRVAL(f, a)	(arr_ref((--(f)->sp)->u.array = (a)),	\
				 (f)->sp->type = T_ARRAY)
# define PUT_ARRVAL(v, a)	(arr_ref((v)->u.array = (a)),		\
				 (v)->type = T_ARRAY)
# define PUT_ARRVAL_NOREF(v, a)	((v)->u.array = (a), (v)->type = T_ARRAY)
# define PUT_ARR(v, a)		(arr_ref((v)->u.array = (a)))
# define PUSH_MAPVAL(f, m)	(arr_ref((--(f)->sp)->u.array = (m)),	\
				 (f)->sp->type = T_MAPPING)
# define PUT_MAPVAL(v, m)	(arr_ref((v)->u.array = (m)),		\
				 (v)->type = T_MAPPING)
# define PUT_MAPVAL_NOREF(v, m)	((v)->u.array = (m), (v)->type = T_MAPPING)
# define PUT_MAP(v, m)		(arr_ref((v)->u.array = (m)))
# define PUSH_LWOVAL(f, o)	(arr_ref((--(f)->sp)->u.array = (o)),	\
				 (f)->sp->type = T_LWOBJECT)
# define PUT_LWOVAL(v, o)	(arr_ref((v)->u.array = (o)),		\
				 (v)->type = T_LWOBJECT)
# define PUT_LWOVAL_NOREF(v, o)	((v)->u.array = (o), (v)->type = T_LWOBJECT)
# define PUT_LWO(v, o)		(arr_ref((v)->u.array = (o)))

# define VFLT_ISZERO(v)	FLT_ISZERO((v)->oindex, (v)->u.objcnt)
# define VFLT_ISONE(v)	FLT_ISONE((v)->oindex, (v)->u.objcnt)
# define VFLT_HASH(v)	((v)->oindex ^ (v)->u.objcnt)

# define DESTRUCTED(v)	(OBJR((v)->oindex)->count != (v)->u.objcnt)


# define C_PRIVATE	0x01
# define C_STATIC	0x02
# define C_NOMASK	0x04
# define C_ELLIPSIS	0x08
# define C_VARARGS	0x08
# define C_ATOMIC	0x10
# define C_TYPECHECKED	0x20
# define C_COMPILED	0x40
# define C_UNDEFINED	0x80


# define SWITCH_INT	0
# define SWITCH_RANGE	1
# define SWITCH_STRING	2


typedef struct _rlinfo_ {
    Int maxdepth;		/* max stack depth */
    Int ticks;			/* ticks left */
    bool nodepth;		/* no stack depth checking */
    bool noticks;		/* no ticks checking */
    struct _rlinfo_ *next;	/* next in linked list */
} rlinfo;

struct _frame_ {
    frame *prev;		/* previous stack frame */
    uindex oindex;		/* current object index */
    array *lwobj;		/* lightweight object */
    control *ctrl;		/* object control block */
    dataspace *data;		/* dataspace of current object */
    control *p_ctrl;		/* program control block */
    unsigned short p_index;	/* program index */
    unsigned short nargs;	/* # arguments */
    bool external;		/* TRUE if it's an external call */
    bool sos;			/* stack on stack */
    uindex foffset;		/* program function offset */
    struct _dfuncdef_ *func;	/* current function */
    char *prog;			/* start of program */
    char *pc;			/* program counter */
    value *stack;		/* local value stack */
    value *sp;			/* stack pointer */
    value *lip;			/* lvalue index pointer */
    value *argp;		/* argument pointer (previous sp) */
    value *fp;			/* frame pointer (at end of local stack) */
    Int depth;			/* stack depth */
    rlinfo *rlim;		/* rlimits info */
    Int level;			/* plane level */
    bool atomic;		/* within uncaught atomic code */
};

extern void	i_init		(char*, int);
extern void	i_ref_value	(value*);
extern void	i_del_value	(value*);
extern void	i_copy		(value*, value*, unsigned int);
extern void	i_grow_stack	(frame*, int);
extern void	i_push_value	(frame*, value*);
extern void	i_pop		(frame*, int);
extern value   *i_reverse	(frame*, int);
extern void	i_odest		(frame*, object*);
extern void	i_string	(frame*, int, unsigned int);
extern void	i_aggregate	(frame*, unsigned int);
extern void	i_map_aggregate	(frame*, unsigned int);
extern int	i_spread	(frame*, int, int, Uint);
extern void	i_global	(frame*, int, int);
extern void	i_global_lvalue	(frame*, int, int, int, Uint);
extern void	i_index		(frame*);
extern void	i_index2	(frame*, value*, value*, value*, bool);
extern void	i_index_lvalue	(frame*, int, Uint);
extern char    *i_typename	(char*, unsigned int);
extern char    *i_classname	(frame*, Uint);
extern int	i_instanceof	(frame*, unsigned int, Uint);
extern void	i_cast		(frame*, value*, unsigned int, Uint);
extern void	i_dup		(frame*);
extern void	i_store_global	(frame*, int, int, value*, value*);
extern bool	i_store_index	(frame*, value*, value*, value*, value*);
extern void	i_store		(frame*);
extern Int	i_get_depth	(frame*);
extern Int	i_get_ticks	(frame*);
extern void	i_new_rlimits	(frame*, Int, Int);
extern void	i_set_rlimits	(frame*, rlinfo*);
extern frame   *i_set_sp	(frame*, value*);
extern frame   *i_prev_object	(frame*, int);
extern char    *i_prev_program	(frame*, int);
extern void	i_typecheck	(frame*, frame*, char*, char*, char*, int,
				   int);
extern void	i_catcherr	(frame*, Int);
extern void	i_funcall	(frame*, object*, array*, int, int, int);
extern bool	i_call		(frame*, object*, array*, char*, unsigned int,
				   int, int);
extern bool	i_call_tracei	(frame*, Int, value*);
extern array   *i_call_trace	(frame*);
extern bool	i_call_critical	(frame*, char*, int, int);
extern void	i_runtime_error	(frame*, Int);
extern void	i_atomic_error	(frame*, Int);
extern frame   *i_restore	(frame*, Int);
extern void	i_clear		(void);

extern frame *cframe;
extern int nil_type;
extern value zero_int, zero_float, nil_value;

# define i_add_ticks(f, t)	((f)->rlim->ticks -= (t))
