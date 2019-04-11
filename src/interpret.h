/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2019 DGD Authors (see the commit log for details)
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

# define I_INSTR_MASK		0x3f	/* instruction mask */

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
# define I_INSTANCEOF		0x0b	/* 1 unsigned, 2 unsigned */
# define I_STORES		0x0c	/* 1 unsigned */
# define I_STORE_GLOBAL_INDEX	0x0d	/* 1 unsigned */
# define I_CALL_EFUNC		0x0e	/* 2 unsigned (+ 1 unsigned) */
# define I_CALL_CEFUNC		0x0f	/* 2 unsigned, 1 unsigned */
# define I_CALL_CKFUNC		0x10	/* 1 unsigned, 1 unsigned */
# define I_STORE_LOCAL		0x11	/* 1 signed */
# define I_STORE_GLOBAL		0x12	/* 1 unsigned */
# define I_STORE_FAR_GLOBAL	0x13	/* 1 unsigned, 1 unsigned */
# define I_STORE_INDEX		0x14
# define I_STORE_LOCAL_INDEX	0x15	/* 1 signed */
# define I_STORE_FAR_GLOBAL_INDEX 0x16	/* 1 unsigned, 1 unsigned */
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
# define I_LINE_SHIFT		6

# define VERSION_VM_MAJOR	2
# define VERSION_VM_MINOR	1


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

# define PUSH_INTVAL(f, i)	((--(f)->sp)->number = (i),		\
				 (f)->sp->type = T_INT)
# define PUT_INTVAL(v, i)	((v)->number = (i), (v)->type = T_INT)
# define PUT_INT(v, i)		((v)->number = (i))
# define PUSH_FLTVAL(f, fl)	((--(f)->sp)->oindex = (fl).high,	\
				 (f)->sp->objcnt = (fl).low,		\
				 (f)->sp->type = T_FLOAT)
# define PUSH_FLTCONST(f, h, l)	((--(f)->sp)->oindex = (h),		\
				 (f)->sp->objcnt = (l),			\
				 (f)->sp->type = T_FLOAT)
# define PUT_FLTVAL(v, fl)	((v)->oindex = (fl).high,		\
				 (v)->objcnt = (fl).low,		\
				 (v)->type = T_FLOAT)
# define PUT_FLT(v, fl)		((v)->oindex = (fl).high,		\
				 (v)->objcnt = (fl).low)
# define GET_FLT(v, fl)		((fl).high = (v)->oindex,		\
				 (fl).low = (v)->objcnt)
# define PUSH_STRVAL(f, s)	(((--(f)->sp)->string = (s))->ref(),	\
				 (f)->sp->type = T_STRING)
# define PUT_STRVAL(v, s)	(((v)->string = (s))->ref(),		\
				 (v)->type = T_STRING)
# define PUT_STRVAL_NOREF(v, s)	((v)->string = (s), (v)->type = T_STRING)
# define PUT_STR(v, s)		(((v)->string = (s))->ref())
# define PUSH_OBJVAL(f, o)	((--(f)->sp)->oindex = (o)->index,	\
				 (f)->sp->objcnt = (o)->count,		\
				 (f)->sp->type = T_OBJECT)
# define PUT_OBJVAL(v, o)	((v)->oindex = (o)->index,		\
				 (v)->objcnt = (o)->count,		\
				 (v)->type = T_OBJECT)
# define PUT_OBJ(v, o)		((v)->oindex = (o)->index,		\
				 (v)->objcnt = (o)->count)
# define PUSH_ARRVAL(f, a)	(((--(f)->sp)->array = (a))->ref(),	\
				 (f)->sp->type = T_ARRAY)
# define PUT_ARRVAL(v, a)	(((v)->array = (a))->ref(),		\
				 (v)->type = T_ARRAY)
# define PUT_ARRVAL_NOREF(v, a)	((v)->array = (a), (v)->type = T_ARRAY)
# define PUT_ARR(v, a)		(((v)->array = (a))->ref())
# define PUSH_MAPVAL(f, m)	(((--(f)->sp)->array = (m))->ref(),	\
				 (f)->sp->type = T_MAPPING)
# define PUT_MAPVAL(v, m)	(((v)->array = (m))->ref(),		\
				 (v)->type = T_MAPPING)
# define PUT_MAPVAL_NOREF(v, m)	((v)->array = (m), (v)->type = T_MAPPING)
# define PUT_MAP(v, m)		(((v)->array = (m))->ref())
# define PUSH_LWOVAL(f, o)	(((--(f)->sp)->array = (o))->ref(),	\
				 (f)->sp->type = T_LWOBJECT)
# define PUT_LWOVAL(v, o)	(((v)->array = (o))->ref(),		\
				 (v)->type = T_LWOBJECT)
# define PUT_LWOVAL_NOREF(v, o)	((v)->array = (o), (v)->type = T_LWOBJECT)
# define PUT_LWO(v, o)		(((v)->array = (o))->ref())

# define VFLT_ISZERO(v)	FLOAT_ISZERO((v)->oindex, (v)->objcnt)
# define VFLT_ISONE(v)	FLOAT_ISONE((v)->oindex, (v)->objcnt)
# define VFLT_HASH(v)	((v)->oindex ^ (v)->objcnt)

# define DESTRUCTED(v)	(OBJR((v)->oindex)->count != (v)->objcnt)


# define C_PRIVATE	0x01
# define C_STATIC	0x02
# define C_NOMASK	0x04
# define C_ELLIPSIS	0x08
# define C_VARARGS	0x08
# define C_ATOMIC	0x10
# define C_TYPECHECKED	0x20
# define C_UNDEFINED	0x80


# define SWITCH_INT	0
# define SWITCH_RANGE	1
# define SWITCH_STRING	2


struct RLInfo {
    Int maxdepth;		/* max stack depth */
    Int ticks;			/* ticks left */
    bool nodepth;		/* no stack depth checking */
    bool noticks;		/* no ticks checking */
    RLInfo *next;		/* next in linked list */
};

struct Frame {
    Frame *prev;		/* previous stack frame */
    uindex oindex;		/* current object index */
    Array *lwobj;		/* lightweight object */
    Control *ctrl;		/* object control block */
    Dataspace *data;		/* dataspace of current object */
    Control *p_ctrl;		/* program control block */
    unsigned short p_index;	/* program index */
    unsigned short nargs;	/* # arguments */
    bool external;		/* TRUE if it's an external call */
    bool sos;			/* stack on stack */
    uindex foffset;		/* program function offset */
    struct FuncDef *func;	/* current function */
    char *prog;			/* start of program */
    char *pc;			/* program counter */
    Value *stack;		/* local value stack */
    Value *sp;			/* stack pointer */
    Value *argp;		/* argument pointer (previous sp) */
    Value *fp;			/* frame pointer (at end of local stack) */
    Int depth;			/* stack depth */
    RLInfo *rlim;		/* rlimits info */
    Int level;			/* plane level */
    bool atomic;		/* within uncaught atomic code */
    bool kflv;			/* kfun with lvalue parameters */
};

extern void	i_init		(char*, bool);
extern void	i_grow_stack	(Frame*, int);
extern void	i_push_value	(Frame*, Value*);
extern void	i_pop		(Frame*, int);
extern void	i_odest		(Frame*, Object*);
extern void	i_string	(Frame*, int, unsigned int);
extern void	i_aggregate	(Frame*, unsigned int);
extern void	i_map_aggregate	(Frame*, unsigned int);
extern int	i_spread	(Frame*, int);
extern void	i_global	(Frame*, int, int);
extern void	i_index		(Frame*, Value*, Value*, Value*, bool);
extern char    *i_typename	(char*, unsigned int);
extern char    *i_classname	(Frame*, Uint);
extern int	i_instanceof	(Frame*, unsigned int, Uint);
extern int	i_instancestr	(unsigned int, char*);
extern void	i_cast		(Frame*, Value*, unsigned int, Uint);
extern void	i_store_global	(Frame*, int, int, Value*, Value*);
extern bool	i_store_index	(Frame*, Value*, Value*, Value*, Value*);
extern void	i_lvalues	(Frame*, int);
extern Int	i_get_depth	(Frame*);
extern Int	i_get_ticks	(Frame*);
extern void	i_new_rlimits	(Frame*, Int, Int);
extern void	i_set_rlimits	(Frame*, RLInfo*);
extern Frame   *i_set_sp	(Frame*, Value*);
extern Frame   *i_prev_object	(Frame*, int);
extern const char    *i_prev_program	(Frame*, int);
extern void	i_typecheck	(Frame*, Frame*, const char*, const char*,
				 char*, int, bool);
extern void	i_funcall	(Frame*, Object*, Array*, int, int, int);
extern bool	i_call		(Frame*, Object*, Array*, const char*,
				 unsigned int, int, int);
extern bool	i_call_tracei	(Frame*, Int, Value*);
extern Array   *i_call_trace	(Frame*);
extern bool	i_call_critical	(Frame*, const char*, int, int);
extern void	i_runtime_error	(Frame*, Int);
extern void	i_atomic_error	(Frame*, Int);
extern Frame   *i_restore	(Frame*, Int);
extern void	i_clear		();

extern Frame *cframe;

# define i_add_ticks(f, t)	((f)->rlim->ticks -= (t))
