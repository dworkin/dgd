# include "dgd.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "fcontrol.h"
# include "table.h"

# define CHECKSP(n) if (sp <= ilvp + (n) + 1) error("Stack overflow")


typedef struct _frame_ {
    object *obj;		/* current object */
    bool external;		/* TRUE if it's an external call */
    dataspace *data;		/* dataspace of current object */
    control *v_ctrl;		/* virtual control block */
    unsigned short voffset;	/* virtual variable offset */
    control *p_ctrl;		/* program control block */
    unsigned short p_index;	/* program index */
    unsigned short foffset;	/* program function offset */
    dfuncdef *func;		/* current function */
    char *pc;			/* program counter */
    value *fp;			/* frame pointer (value stack) */
} frame;


static value *stack;		/* interpreter stack */
static value *stackend;		/* interpreter stack end */
value *sp;			/* interpreter stack pointer */
static value *ilvp;		/* indexed lvalue stack pointer */
static frame *iframe;		/* stack frames */
static frame *cframe;		/* current frame */
static frame *maxframe;		/* max frame */
static frame *maxmaxframe;	/* max locked frame */
static long ticksleft;		/* interpreter ticks left */
static long maxticks;		/* max ticks allowed */
static unsigned short lock;	/* current lock level */
static string *lvstr;		/* the last indexed string */
static char *creator;		/* creator function name */
static unsigned short line;	/* current line number */
static char *oname, *pname;	/* object name, program name */

static value zero_value = { T_NUMBER, TRUE };

/*
 * NAME:	interpret->init()
 * DESCRIPTION:	initialize the interpreter
 */
void i_init(stacksize, maxdepth, reserved, maxcost, create)
int stacksize, maxdepth, reserved;
long maxcost;
char *create;
{
    stack = ALLOC(value, stacksize);
    /*
     * The stack array is used both for the stack values (on one side) and
     * the indexed lvalues.
     */
    sp = stackend = stack + stacksize;
    ilvp = stack;
    iframe = ALLOC(frame, maxdepth + reserved);
    cframe = iframe - 1;
    maxframe = iframe + maxdepth;
    maxmaxframe = iframe + maxdepth + reserved;
    maxticks = maxcost;
    creator = create;
}

/*
 * NAME:	interpret->ref_value()
 * DESCRIPTION:	reference a value on the stack
 */
void i_ref_value(v)
register value *v;
{
    switch (v->type) {
    case T_STRING:
	str_ref(v->u.string);
	break;

    case T_OBJECT:
	if (DESTRUCTED(v->u.object)) {
	    *v = zero_value;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_ref(v->u.array);
	break;
    }
}

/*
 * NAME:	interpret->del_value()
 * DESCRIPTION:	delete a value on the stack
 */
void i_del_value(v)
register value *v;
{
    switch (v->type) {
    case T_STRING:
	str_del(v->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_del(v->u.array);
	break;
    }
}

/*
 * NAME:	interpret->check_stack()
 * DESCRIPTION:	check if there is room on the stack for new values
 */
void i_check_stack(size)
int size;
{
    CHECKSP(size);
}

/*
 * NAME:	interpret->push_value()
 * DESCRIPTION:	push a value on the stack
 */
void i_push_value(v)
register value *v;
{
    CHECKSP(1);
    *--sp = *v;
    switch (v->type) {
    case T_STRING:
	str_ref(v->u.string);
	break;

    case T_OBJECT:
	if (DESTRUCTED(v->u.object)) {
	    /*
	     * can't wipe out the original, since it may be a value from a
	     * mapping
	     */
	    *sp = zero_value;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_ref(v->u.array);
	break;
    }
}

/*
 * NAME:	interpret->pop()
 * DESCRIPTION:	pop a number of values (can be lvalues) from the stack
 */
void i_pop(n)
register int n;
{
    register value *v;

    for (v = sp; --n >= 0; v++) {
	switch (v->type) {
	case T_STRING:
	    str_del(v->u.string);
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_del(v->u.array);
	    break;

	case T_SALVALUE:
	    --ilvp;
	case T_ALVALUE:
	    arr_del((--ilvp)->u.array);
	    break;

	case T_MLVALUE:
	case T_SMLVALUE:
	    i_del_value(--ilvp);
	    arr_del((--ilvp)->u.array);
	    break;
	}
    }
    sp = v;
}

/*
 * NAME:	interpret->odest()
 * DESCRIPTION:	replace all occurrances of an object on the stack by 0
 */
void i_odest(key)
objkey *key;
{
    register value *v;
    register long count;

    count = key->count;
    for (v = sp; v < stackend; v++) {
	if (v->type == T_OBJECT && v->u.object.count == count) {
	    /*
	     * wipe out destructed object on stack
	     */
	    *v = zero_value;
	}
    }
    for (v = stack; v < ilvp; v++) {
	if (v->type == T_OBJECT && v->u.object.count == count) {
	    /*
	     * wipe out destructed object on stack
	     */
	    *v = zero_value;
	}
    }
}

/*
 * NAME:	interpret->index()
 * DESCRIPTION:	index a value, REPLACING it by the indexed value
 */
void i_index(aval, ival)
register value *aval, *ival;
{
    register int i;
    register value *val;
    array *a;

    switch (aval->type) {
    case T_STRING:
	if (ival->type != T_NUMBER) {
	    error("Non-numeric string index");
	}
	i = UCHAR(aval->u.string->text[str_index(aval->u.string,
						 ival->u.number)]);
	str_del(aval->u.string);
	aval->type = T_NUMBER;
	aval->u.number = i;
	return;

    case T_ARRAY:
	if (ival->type != T_NUMBER) {
	    error("Non-numeric array index");
	}
	val = &d_get_elts(aval->u.array)[arr_index(aval->u.array,
						   ival->u.number)];
	break;

    case T_MAPPING:
	val = map_index(aval->u.array, ival, (value *) NULL);
	break;

    default:
	error("Index on bad type");
    }

    a = aval->u.array;
    switch (val->type) {
    case T_STRING:
	str_ref(val->u.string);
	break;

    case T_OBJECT:
	if (DESTRUCTED(val->u.object)) {
	    val = &zero_value;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_ref(val->u.array);
	break;
    }
    *aval = *val;
    arr_del(a);
}

/*
 * NAME:	interpret->index_lvalue()
 * DESCRIPTION:	Index a value, REPLACING it by an indexed lvalue.
 */
void i_index_lvalue(lval, ival)
register value *lval, *ival;
{
    register int i;
    register value *val;

    CHECKSP(1);	/* not actually required in all cases... */
    switch (lval->type) {
    case T_STRING:
	/* for instance, "foo"[1] = 'a'; */
	error("Bad lvalue");

    case T_ARRAY:
	if (ival->type != T_NUMBER) {
	    error("Non-numeric array index");
	}
	i = arr_index(lval->u.array, ival->u.number);
	ilvp->type = T_ARRAY;
	(ilvp++)->u.array = lval->u.array;
	lval->type = T_ALVALUE;
	lval->u.number = i;
	return;

    case T_MAPPING:
	ilvp->type = T_ARRAY;
	(ilvp++)->u.array = lval->u.array;
	*ilvp++ = *ival;
	i_ref_value(ival);
	lval->type = T_MLVALUE;
	return;

    case T_LVALUE:
	/*
	 * note: the lvalue is not yet referenced
	 */
	switch (lval->u.lval->type) {
	case T_STRING:
	    if (ival->type != T_NUMBER) {
		error("Non-numeric string index");
	    }
	    i = str_index(lvstr = lval->u.lval->u.string, ival->u.number);
	    ilvp->type = T_LVALUE;
	    (ilvp++)->u.lval = lval->u.lval;
	    /* indexed string lvalues are not referenced */
	    lval->type = T_SLVALUE;
	    lval->u.number = i;
	    return;

	case T_ARRAY:
	    if (ival->type != T_NUMBER) {
		error("Non-numeric array index");
	    }
	    i = arr_index(lval->u.lval->u.array, ival->u.number);
	    ilvp->type = T_ARRAY;
	    arr_ref((ilvp++)->u.array = lval->u.lval->u.array);
	    lval->type = T_ALVALUE;
	    lval->u.number = i;
	    return;

	case T_MAPPING:
	    ilvp->type = T_ARRAY;
	    arr_ref((ilvp++)->u.array = lval->u.lval->u.array);
	    *ilvp++ = *ival;
	    i_ref_value(ival);
	    lval->type = T_MLVALUE;
	    return;
	}
	break;

    case T_ALVALUE:
	val = &d_get_elts(ilvp[-1].u.array)[lval->u.number];
	switch (val->type) {
	case T_STRING:
	    if (ival->type != T_NUMBER) {
		error("Non-numeric string index");
	    }
	    i = str_index(lvstr = val->u.string, ival->u.number);
	    ilvp->type = T_NUMBER;
	    (ilvp++)->u.number = lval->u.number;
	    lval->type = T_SALVALUE;
	    lval->u.number = i;
	    return;

	case T_ARRAY:
	    if (ival->type != T_NUMBER) {
		error("Non-numeric array index");
	    }
	    i = arr_index(val->u.array, ival->u.number);
	    arr_ref(val->u.array);	/* has to be first */
	    arr_del(ilvp[-1].u.array);	/* has to be second */
	    ilvp[-1].u.array = val->u.array;
	    lval->u.number = i;
	    return;

	case T_MAPPING:
	    arr_ref(val->u.array);	/* has to be first */
	    arr_del(ilvp[-1].u.array);	/* has to be second */
	    ilvp[-1].u.array = val->u.array;
	    *ilvp++ = *ival;
	    i_ref_value(ival);
	    lval->type = T_MLVALUE;
	    lval->u.number = i;
	    return;
	}
	break;

    case T_MLVALUE:
	val = map_index(ilvp[-2].u.array, &ilvp[-1], (value *) NULL);
	switch (val->type) {
	case T_STRING:
	    if (ival->type != T_NUMBER) {
		error("Non-numeric string index");
	    }
	    i = str_index(lvstr = val->u.string, ival->u.number);
	    lval->type = T_SMLVALUE;
	    lval->u.number = i;
	    return;

	case T_ARRAY:
	    if (ival->type != T_NUMBER) {
		error("Non-numeric array index");
	    }
	    i = arr_index(val->u.array, ival->u.number);
	    i_del_value(--ilvp);
	    arr_ref(val->u.array);	/* has to be first */
	    arr_del(ilvp[-1].u.array);	/* has to be second */
	    ilvp[-1].u.array = val->u.array;
	    lval->type = T_ALVALUE;
	    lval->u.number = i;
	    return;

	case T_MAPPING:
	    arr_ref(val->u.array);	/* has to be first */
	    arr_del(ilvp[-2].u.array);	/* has to be second */
	    ilvp[-2].u.array = val->u.array;
	    i_del_value(&ilvp[-1]);
	    ilvp[-1] = *ival;
	    i_ref_value(ival);
	    lval->u.number = i;
	    return;
	}
	break;
    }
    error("Index on bad type");
}

/*
 * NAME:	istr()
 * DESCRIPTION:	create a copy of the argument string, with one char replaced
 */
static value *istr(str, i, val)
string *str;
unsigned short i;
register value *val;
{
    static value ret = { T_STRING };
    char c;

    if (val->type != T_NUMBER) {
	error("Non-numeric value in indexed string assignment");
    }

    ret.u.string = str_new(str->text, (long) str->len);
    ret.u.string->text[i] = val->u.number;
    return &ret;
}

/*
 * NAME:	interpret->store()
 * DESCRIPTION:	Perform an assignment. This invalidates the lvalue.
 */
void i_store(data, lval, val)
dataspace *data;
register value *lval, *val;
{
    register value *v;
    register unsigned short i;
    register array *a;

    switch (lval->type) {
    case T_LVALUE:
	d_assign_var(data, lval->u.lval, val);
	break;

    case T_SLVALUE:
	v = ilvp[-1].u.lval;
	i = lval->u.number;
	if (v->type != T_STRING || i >= v->u.string->len) {
	    /*
	     * The lvalue was changed.
	     */
	    error("Lvalue disappeared!");
	}
	--ilvp;
	d_assign_var(data, v, istr(v->u.string, i, val));
	break;

    case T_ALVALUE:
	a = (--ilvp)->u.array;
	d_assign_elt(a, &d_get_elts(a)[lval->u.number], val);
	arr_del(a);
	break;

    case T_MLVALUE:
	map_index(a = ilvp[-2].u.array, &ilvp[-1], val);
	i_del_value(--ilvp);
	--ilvp;
	arr_del(a);
	break;

    case T_SALVALUE:
	a = ilvp[-2].u.array;
	v = &a->elts[ilvp[-1].u.number];
	i = lval->u.number;
	if (v->type != T_STRING || i >= v->u.string->len) {
	    /*
	     * The lvalue was changed.
	     */
	    error("Lvalue disappeared!");
	}
	d_assign_elt(a, v, istr(v->u.string, i, val));
	ilvp -= 2;
	arr_del(a);
	break;

    case T_SMLVALUE:
	a = ilvp[-2].u.array;
	v = map_index(a, &ilvp[-1], (value *) NULL);
	if (v->type != T_STRING || lval->u.number >= v->u.string->len) {
	    /*
	     * The lvalue was changed.
	     */
	    error("Lvalue disappeared!");
	}
	d_assign_elt(a, v, istr(v->u.string,
		     (unsigned short) lval->u.number, val));
	i_del_value(--ilvp);
	--ilvp;
	arr_del(a);
	break;
    }
}

/*
 * NAME:	interpret->add_ticks()
 * DESCRIPTION:	add some execution cost, used by costly kfuns
 */
void i_add_ticks(extra)
int extra;
{
    ticksleft -= extra;
}

/*
 * NAME:	interpret->lock()
 * DESCRIPTION:	lock the current frame, allowing deeper recursion and longer
 *		evaluation
 */
void i_lock()
{
    lock++;
}

/*
 * NAME:	interpret->unlock()
 * DESCRIPTION:	unlock the current frame
 */
void i_unlock()
{
    --lock;
}

/*
 * NAME:	interpret->locklvl()
 * DESCRIPTION:	return the current lock level
 */
unsigned short i_locklvl()
{
    return lock;
}

/*
 * NAME:	interpret->this_object()
 * DESCRIPTION:	return the current object
 */
object *i_this_object()
{
    return cframe->obj;
}

/*
 * NAME:	interpret->prev_object()
 * DESCRIPTION:	return the nth previous object in the call_other chain
 */
object *i_prev_object(n)
register int n;
{
    register frame *f;

    for (f = cframe; n >= 0; --n) {
	/* back to last external call */
	while (!f->external) {
	    --f;
	}
	if (--f < iframe) {
	    return (object *) NULL;
	}
    }
    return (f->obj->key.count == 0) ? (object *) NULL : f->obj;
}

/*
 * NAME:	interpret->typecheck()
 * DESCRIPTION:	check the argument types given to a function
 */
static void i_typecheck(name, ftype, proto, nargs, strict)
char *name, *ftype;
register char *proto;
int nargs;
bool strict;
{
    register int n, i, ptype, atype;
    register char *args;

    i = nargs;
    for (n = PROTO_NARGS(proto), args = PROTO_ARGS(proto); n > 0 && i > 0; --n)
    {
	--i;
	ptype = UCHAR(*args++);
	if (ptype != T_MIXED) {
	    atype = sp[i].type;
	    if (ptype != atype &&
		(strict || atype != T_NUMBER || sp[i].u.number != 0) &&
		(atype != T_ARRAY || !(ptype & T_REF))) {
		error("Bad argument %d for %s %s", nargs - i, ftype, name);
	    }
	}
    }
}

# define FETCH1S(pc)	SCHAR(*(pc)++)
# define FETCH1U(pc)	UCHAR(*(pc)++)
# define FETCH2S(pc, v)	((short) (v = *(pc)++ << 8, v |= UCHAR(*(pc)++)))
# define FETCH2U(pc, v)	((unsigned short) (v = *(pc)++ << 8, \
					   v |= UCHAR(*(pc)++)))
# define FETCH3S(pc, v)	((Int) (v = *(pc)++ << 8, \
				v |= UCHAR(*(pc)++), v <<= 8, \
				v |= UCHAR(*(pc)++)))
# define FETCH4S(pc, v)	((Int) (v = *(pc)++ << 8, \
				v |= UCHAR(*(pc)++), v <<= 8, \
				v |= UCHAR(*(pc)++), v <<= 8, \
				v |= UCHAR(*(pc)++)))

static void i_interpret P((char*));

/*
 * NAME:	interpret->funcall()
 * DESCRIPTION:	Call a function in an object. The arguments must be on the
 *		stack already.
 */
static void i_funcall(obj, v_ctrli, p_ctrli, funci, nargs)
register object *obj;
register int v_ctrli, p_ctrli, nargs;
int funci;
{
    register frame *f, *pf;
    register char *pc;
    register unsigned short n;
    value val;

    f = pf = cframe;
    f++;
    if (f == maxmaxframe || (f == maxframe && lock == 0)) {
	error("Function call stack overflow");
    }

    if (f == iframe) {
	/*
	 * top level call
	 */
	ticksleft = maxticks;

	f->obj = obj;
	f->external = TRUE;
	f->data = o_dataspace(obj);
	f->v_ctrl = obj->ctrl;
	f->voffset = 0;
    } else if (obj != (object *) NULL) {
	/*
	 * call_other
	 */
	f->obj = obj;
	f->external = TRUE;
	f->data = o_dataspace(obj);
	f->v_ctrl = obj->ctrl;
	f->voffset = 0;
    } else {
	/*
	 * local function call or labeled function call
	 */
	f->obj = pf->obj;
	f->external = FALSE;
	f->data = pf->data;
	f->voffset = pf->voffset;
	if (v_ctrli == 0) {
	    /* local or virtually inherited function call */
	    f->v_ctrl = pf->v_ctrl;
	} else {
	    /* labeled inherited function call */
	    f->v_ctrl = o_control(pf->p_ctrl->inherits[v_ctrli].obj);
	    f->voffset += pf->v_ctrl->inherits[pf->p_ctrl->nvirtuals +
					       pf->p_index - 1].varoffset +
			  pf->p_ctrl->inherits[v_ctrli].varoffset -
			  pf->p_ctrl->inherits[1].varoffset;
	}
    }

    /* set the program control block */
    f->p_ctrl = o_control(f->v_ctrl->inherits[p_ctrli].obj);
    f->foffset = f->v_ctrl->inherits[p_ctrli].funcoffset;
    f->p_index = p_ctrli + 1 - f->p_ctrl->nvirtuals;

    /* get the function */
    f->func = &d_get_funcdefs(f->p_ctrl)[funci];
    if (f->func->class & C_UNDEFINED) {
	error("Undefined function %s",
	      d_get_strconst(f->p_ctrl, f->func->inherit,
			     f->func->index)->text);
    }

    pc = d_get_prog(f->p_ctrl) + f->func->offset;
    if (nargs > 0 && (PROTO_CLASS(pc) & C_TYPECHECKED)) {
	/* typecheck arguments */
	i_typecheck(d_get_strconst(f->p_ctrl, f->func->inherit,
				   f->func->index)->text,
		    "function", pc, nargs, FALSE);
    }

    /* handle arguments */
    if (nargs > PROTO_NARGS(pc)) {
	/* pop superfluous arguments */
	i_pop(nargs - PROTO_NARGS(pc));
	nargs = PROTO_NARGS(pc);
    } else if (nargs < PROTO_NARGS(pc)) {
	/* add missing arguments */
	CHECKSP(PROTO_NARGS(pc) - nargs);
	do {
	    *--sp = zero_value;
	} while (++nargs < PROTO_NARGS(pc));
    }
    pc += PROTO_SIZE(pc);

    /* initialize local variables */
    n = FETCH1U(pc);
    if (n > 0) {
	CHECKSP(n);
	nargs += n;
	do {
	    *--sp = zero_value;
	} while (--n > 0);
    }
    f->fp = sp;
    pc += 2;

    /* interpret function code */
    ticksleft -= 5;
    cframe++;
    i_interpret(pc);
    --cframe;

    /* clean up stack, move return value upwards */
    val = *sp++;
    i_pop(nargs);
    *--sp = val;
}

/*
 * NAME:	interpret->switch_int()
 * DESCRIPTION:	handle an int switch
 */
static char *i_switch_int(pc)
register char *pc;
{
    register unsigned short h, l, m, sz;
    register Int num;
    register char *p, *dflt;

    FETCH2U(pc, h);
    sz = FETCH1U(pc);
    p = pc;
    dflt = p + FETCH2S(pc, l);
    --h;
    if (sp->type != T_NUMBER) {
	return dflt;
    }

    l = 0;
    switch (sz) {
    case 1:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 3 * m;
	    num = FETCH1S(p);
	    if (sp->u.number == num) {
		pc = p;
		return pc + FETCH2S(p, l);
	    } else if (sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 2:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 4 * m;
	    FETCH2S(p, num);
	    if (sp->u.number == num) {
		pc = p;
		return pc + FETCH2S(p, l);
	    } else if (sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 3:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 5 * m;
	    FETCH3S(p, num);
	    if (sp->u.number == num) {
		pc = p;
		return pc + FETCH2S(p, l);
	    } else if (sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 4:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 6 * m;
	    FETCH4S(p, num);
	    if (sp->u.number == num) {
		pc = p;
		return pc + FETCH2S(p, l);
	    } else if (sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;
    }

    return dflt;
}

/*
 * NAME:	interpret->switch_range()
 * DESCRIPTION:	handle a range switch
 */
static char *i_switch_range(pc)
register char *pc;
{
    register unsigned short h, l, m, sz;
    register Int num;
    register char *p, *dflt;

    FETCH2U(pc, h);
    sz = FETCH1U(pc);
    p = pc;
    dflt = p + FETCH2S(pc, l);
    --h;
    if (sp->type != T_NUMBER) {
	return dflt;
    }

    l = 0;
    switch (sz) {
    case 1:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 4 * m;
	    num = FETCH1S(p);
	    if (sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		num = FETCH1S(p);
		if (sp->u.number <= num) {
		    pc = p;
		    return pc + FETCH2S(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 2:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 6 * m;
	    FETCH2S(p, num);
	    if (sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH2S(p, num);
		if (sp->u.number <= num) {
		    pc = p;
		    return pc + FETCH2S(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 3:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 8 * m;
	    FETCH3S(p, num);
	    if (sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH3S(p, num);
		if (sp->u.number <= num) {
		    pc = p;
		    return pc + FETCH2S(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 4:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 10 * m;
	    FETCH4S(p, num);
	    if (sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH4S(p, num);
		if (sp->u.number <= num) {
		    pc = p;
		    return pc + FETCH2S(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;
    }
    return dflt;
}

/*
 * NAME:	interpret->switch_str()
 * DESCRIPTION:	handle a string switch
 */
static char *i_switch_str(pc)
register char *pc;
{
    register unsigned short h, l, m, u, u2;
    register int cmp;
    register char *p, *dflt;
    register control *ctrl;

    FETCH2U(pc, h);
    p = pc;
    dflt = p + FETCH2S(pc, l);
    --h;
    if (FETCH1U(pc) == 0) {
	p = pc;
	p += FETCH2S(pc, l);
	if (sp->type == T_NUMBER && sp->u.number == 0) {
	    return p;
	}
	--h;
    }
    if (sp->type != T_STRING) {
	return dflt;
    }

    ctrl = cframe->p_ctrl;
    l = 0;
    while (l < h) {
	m = (l + h) >> 1;
	p = pc + 5 * m;
	u = FETCH1U(p);
	cmp = str_cmp(sp->u.string, d_get_strconst(ctrl, u, FETCH2U(p, u2)));
	if (cmp == 0) {
	    pc = p;
	    return pc + FETCH2S(p, l);
	} else if (cmp < 0) {
	    h = m;	/* search in lower half */
	} else {
	    l = m + 1;	/* search in upper half */
	}
    }
    return dflt;
}

/*
 * NAME:	interpret->interpret()
 * DESCRIPTION:	Main interpreter function. Interpret stack machine code.
 */
static void i_interpret(pc)
register char *pc;
{
    register unsigned short instr, u, u2, u3;
    register long l;
    register frame *f;
    register char *p;
    register kfunc *kf;
    value *v;
    array *a;

    f = cframe;

    for (;;) {
	if (--ticksleft <= 0 && lock == 0) {
	    error("Maximum execution cost exceeded (%ld)",
		  maxticks - ticksleft);
	}
	instr = FETCH1U(pc);
	f->pc = pc;

	switch (instr & I_INSTR_MASK) {
	case I_PUSH_ZERO:
	    CHECKSP(1);
	    *--sp = zero_value;
	    break;

	case I_PUSH_ONE:
	    CHECKSP(1);
	    (--sp)->type = T_NUMBER;
	    sp->u.number = 1;
	    break;

	case I_PUSH_INT1:
	    CHECKSP(1);
	    (--sp)->type = T_NUMBER;
	    sp->u.number = FETCH1S(pc);
	    break;

	case I_PUSH_INT2:
	    CHECKSP(1);
	    (--sp)->type = T_NUMBER;
	    sp->u.number = FETCH2S(pc, u);
	    break;

	case I_PUSH_INT4:
	    CHECKSP(1);
	    (--sp)->type = T_NUMBER;
	    sp->u.number = FETCH4S(pc, l);
	    break;

	case I_PUSH_STRING:
	    CHECKSP(1);
	    (--sp)->type = T_STRING;
	    str_ref(sp->u.string = d_get_strconst(f->p_ctrl,
						  f->p_ctrl->nvirtuals - 1,
						  FETCH1U(pc)));
	    break;

	case I_PUSH_NEAR_STRING:
	    CHECKSP(1);
	    (--sp)->type = T_STRING;
	    u = FETCH1U(pc);
	    str_ref(sp->u.string = d_get_strconst(f->p_ctrl, u, FETCH1U(pc)));
	    break;

	case I_PUSH_FAR_STRING:
	    CHECKSP(1);
	    (--sp)->type = T_STRING;
	    u = FETCH1U(pc);
	    str_ref(sp->u.string = d_get_strconst(f->p_ctrl, u,
						  FETCH2U(pc, u2)));
	    break;

	case I_PUSH_LOCAL:
	    i_push_value(f->fp + FETCH1U(pc));
	    break;

	case I_PUSH_GLOBAL:
	    u = FETCH1U(pc);
	    if (u != 0) {
		u = f->voffset + f->v_ctrl->inherits[f->p_index + u].varoffset;
	    }
	    i_push_value(d_get_variable(f->data, u + FETCH1U(pc)));
	    ticksleft -= 3;
	    break;

	case I_PUSH_LOCAL_LVALUE:
	    CHECKSP(1);
	    (--sp)->type = T_LVALUE;
	    sp->u.lval = f->fp + FETCH1U(pc);
	    break;

	case I_PUSH_GLOBAL_LVALUE:
	    u = FETCH1U(pc);
	    if (u != 0) {
		u = f->voffset + f->v_ctrl->inherits[f->p_index + u].varoffset;
	    }
	    CHECKSP(1);
	    (--sp)->type = T_LVALUE;
	    sp->u.lval = d_get_variable(f->data, u + FETCH1U(pc));
	    ticksleft -= 3;
	    break;

	case I_INDEX:
	    i_index(sp + 1, sp);
	    i_del_value(sp++);
	    break;

	case I_INDEX_LVALUE:
	    i_index_lvalue(sp + 1, sp);
	    i_del_value(sp++);
	    break;

	case I_AGGREGATE:
	    FETCH2U(pc, u);
	    if (u == 0) {
		CHECKSP(1);
		a = arr_new(0L);
	    } else {
		a = arr_new((long) u);
		memcpy(a->elts, sp, u * sizeof(value));
		sp += u;
	    }
	    (--sp)->type = T_ARRAY;
	    arr_ref(sp->u.array = a);
	    break;

	case I_MAP_AGGREGATE:
	    FETCH2U(pc, u);
	    if (u == 0) {
		CHECKSP(1);
		a = map_new(0L);
	    } else {
		a = map_new((long) u);
		memcpy(a->elts, sp, u * sizeof(value));
		sp += u;
		map_sort(a);
	    }
	    (--sp)->type = T_MAPPING;
	    arr_ref(sp->u.array = a);
	    break;

	case I_CHECK_INT:
	    if (sp->type != T_NUMBER) {
		error("Argument is not a number");
	    }
	    break;

	case I_FETCH:
	    switch (sp->type) {
	    case T_LVALUE:
		i_push_value(sp->u.lval);
		break;

	    case T_ALVALUE:
		i_push_value(d_get_elts(ilvp[-1].u.array) + sp->u.number);
		break;

	    case T_MLVALUE:
		i_push_value(map_index(ilvp[-2].u.array, &ilvp[-1],
				       (value *) NULL));
		break;

	    default:
		/*
		 * Indexed string.
		 * The fetch is always done directly after an lvalue
		 * constructor, so lvstr is valid.
		 */
		CHECKSP(1);
		(--sp)->type = T_NUMBER;
		sp->u.number = UCHAR(lvstr->text[sp[1].u.number]);
		break;
	    }
	    break;

	case I_STORE:
	    i_store(f->data, sp + 1, sp);
	    /* overwrite lvalue with value */
	    sp[1] = sp[0];
	    sp++;
	    break;

	case I_JUMP:
	    p = pc;
	    p += FETCH2S(pc, u);
	    pc = p;
	    break;

	case I_JUMP_ZERO:
	    p = pc;
	    p += FETCH2S(pc, u);
	    if (sp->type == T_NUMBER && sp->u.number == 0) {
		pc = p;
	    }
	    break;

	case I_JUMP_NONZERO:
	    p = pc;
	    p += FETCH2S(pc, u);
	    if (sp->type != T_NUMBER || sp->u.number != 0) {
		pc = p;
	    }
	    break;

	case I_SWITCH_INT:
	    pc = i_switch_int(pc);
	    break;

	case I_SWITCH_RANGE:
	    pc = i_switch_range(pc);
	    break;

	case I_SWITCH_STR:
	    pc = i_switch_str(pc);
	    break;

	case I_CALL_KFUNC:
	    kf = &kftab[FETCH1U(pc)];
	    if (PROTO_CLASS(kf->proto) & C_VARARGS) {
		/* variable # of arguments */
		u = FETCH1U(pc);
	    } else {
		/* fixed # of arguments */
		u = PROTO_NARGS(kf->proto);
	    }
	    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
		i_typecheck(kf->name, "kfun", kf->proto, u, TRUE);
	    }
	    u = (*kf->func)(u);
	    if (u != 0) {
		error("Bad argument %d for kfun %s", u, kf->name);
	    }
	    break;

	case I_CALL_LFUNC:
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    u3 = FETCH1U(pc);
	    i_funcall((object *) NULL, u, u2, u3, FETCH1U(pc));
	    break;

	case I_CALL_DFUNC:
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    i_funcall((object *) NULL, 0, u, u2, FETCH1U(pc));
	    break;

	case I_CALL_FUNC:
	    p = &f->v_ctrl->funcalls[2L * (f->foffset + FETCH2U(pc, u))];
	    i_funcall((object *) NULL, 0, UCHAR(p[0]), UCHAR(p[1]),
		      FETCH1U(pc));
	    break;

	case I_CATCH:
	    if (!ec_push()) {
		p = pc;
		p += FETCH2S(pc, u);
		u = lock;
		v = sp;
		i_interpret(pc);
		pc = f->pc;
		ec_pop();
	    } else {
		/* error */
		cframe = f;
		f->pc = pc = p;
		lock = u;
		i_pop(v - sp);
		CHECKSP(1);
		p = errormesg();
		(--sp)->type = T_STRING;
		str_ref(sp->u.string = str_new(p, (long) strlen(p)));
	    }
	    break;

	case I_LOCK:
	    lock++;
	    i_interpret(pc);
	    pc = f->pc;
	    --lock;
	    break;

	case I_RETURN:
	    return;
	}

	if (instr & I_POP_BIT) {
	    /* pop the result of the last operation (never an lvalue) */
	    i_del_value(sp++);
	}
    }
}

/*
 * NAME:	interpret->call()
 * DESCRIPTION:	Attempt to call a function in an object. Return TRUE if
 *		the call succeeded.
 */
bool i_call(obj, func, call_static, nargs)
object *obj;
char *func;
bool call_static;
int nargs;
{
    register dsymbol *symb;
    register dfuncdef *f;
    register control *ctrl;

    if (!(obj->flags & O_CREATED)) {
	/*
	 * call the creator function in the object, if this hasn't happened
	 * before
	 */
	obj->flags |= O_CREATED;
	if (i_call(obj, creator, TRUE, 0)) {
	    i_del_value(sp++);
	}
    }

    /* find the function in the symbol table */
    symb = ctrl_symb(o_control(obj), func);
    if (symb == (dsymbol *) NULL) {
	/* function doesn't exist in symbol table */
	i_pop(nargs);
	return FALSE;
    }

    ctrl = obj->ctrl->inherits[UCHAR(symb->inherit)].obj->ctrl;
    f = &d_get_funcdefs(ctrl)[UCHAR(symb->index)];

    /* check if the function can be called */
    if (!call_static && (f->class & C_STATIC) &&
	(cframe < iframe || cframe->obj != obj)) {
	i_pop(nargs);
	return FALSE;
    }

    /* call the function */
    i_funcall(obj, 0, UCHAR(symb->inherit), UCHAR(symb->index), nargs);

    return TRUE;
}

/*
 * NAME:	interpret->line()
 * DESCRIPTION:	return the line number the program counter of the specified
 *		frame is at
 */
static unsigned short i_line(f)
register frame *f;
{
    register char *pc, *numbers;
    register int instr;
    register short offset;
    register unsigned short line, u, sz;

    line = 0;
    pc = f->p_ctrl->prog + f->func->offset;
    pc += PROTO_SIZE(pc) + 1;
    FETCH2U(pc, u);
    numbers = pc + u;

    while (pc < f->pc) {
	instr = FETCH1U(pc);

	offset = instr >> I_LINE_SHIFT;
	if (offset <= 2) {
	    /* simple offset */
	    line += offset;
	} else {
	    offset = FETCH1U(numbers);
	    if (offset >= 128) {
		/* one byte offset */
		line += offset - 128 - 64;
	    } else {
		/* two byte offset */
		line += ((offset << 8) | FETCH1U(numbers)) - 16384;
	    }
	}

	switch (instr & I_INSTR_MASK) {
	case I_PUSH_ZERO:
	case I_PUSH_ONE:
	case I_INDEX:
	case I_INDEX_LVALUE:
	case I_CHECK_INT:
	case I_FETCH:
	case I_STORE:
	case I_LOCK:
	case I_RETURN:
	    break;

	case I_PUSH_INT1:
	case I_PUSH_STRING:
	case I_PUSH_LOCAL:
	case I_PUSH_LOCAL_LVALUE:
	    pc++;
	    break;

	case I_PUSH_INT2:
	case I_PUSH_NEAR_STRING:
	case I_PUSH_GLOBAL:
	case I_PUSH_GLOBAL_LVALUE:
	case I_AGGREGATE:
	case I_MAP_AGGREGATE:
	case I_JUMP:
	case I_JUMP_ZERO:
	case I_JUMP_NONZERO:
	case I_CATCH:
	    pc += 2;
	    break;

	case I_PUSH_FAR_STRING:
	case I_CALL_DFUNC:
	case I_CALL_FUNC:
	    pc += 3;
	    break;

	case I_PUSH_INT4:
	case I_CALL_LFUNC:
	    pc += 4;
	    break;

	case I_SWITCH_INT:
	    FETCH2U(pc, u);
	    sz = FETCH1U(pc);
	    pc += 2 + (u - 1) * (sz + 2);
	    break;

	case I_SWITCH_RANGE:
	    FETCH2U(pc, u);
	    sz = FETCH1U(pc);
	    pc += 2 + (u - 1) * (2 * sz + 2);
	    break;

	case I_SWITCH_STR:
	    FETCH2U(pc, u);
	    pc += 2;
	    if (FETCH1U(pc) == 0) {
		pc += 2;
		--u;
	    }
	    pc += (u - 1) * 5;
	    break;

	case I_CALL_KFUNC:
	    if (PROTO_CLASS(kftab[FETCH1U(pc)].proto) & C_VARARGS) {
		pc++;
	    }
	    break;
	}
    }

    return line;
}

/*
 * NAME:	interpret->dump_trace()
 * DESCRIPTION:	dump the function call trace on stderr
 */
void i_dump_trace(fp)
FILE *fp;
{
    register frame *f;
    register object *prog;
    register int len;
    register char *p;

    for (f = iframe; f <= cframe; f++) {
	prog = f->p_ctrl->inherits[f->p_ctrl->nvirtuals - 1].obj;
	fprintf(fp, "%4u %-17s /%s", i_line(f),
		d_get_strconst(f->p_ctrl, f->func->inherit,
			       f->func->index)->text,
		prog->chain.name);
	if (f->obj->ctrl != f->p_ctrl) {
	    /*
	     * Program and object are not the same; the object name must
	     * be printed as well.
	     */
	    len = strlen(prog->chain.name);
	    p = o_name(f->obj);
	    if (strncmp(prog->chain.name, p, len) == 0 && p[len] == '#') {
		/* object is a clone */
		p += len;
	    }
	    fprintf(fp, " (/%s)", p);
	}
	fputc('\n', fp);
    }
    fflush(fp);
}

/*
 * NAME:	interpret->log_error()
 * DESCRIPTION:	log the current error
 */
void i_log_error()
{
    if (line != 0) {
	char *err;

	err = errormesg();
	(--sp)->type = T_STRING;
	str_ref(sp->u.string = str_new(err, (long) strlen(err)));
	(--sp)->type = T_STRING;
	str_ref(sp->u.string = str_new((char *) NULL, strlen(oname) + 1L));
	sp->u.string->text[0] = '/';
	strcpy(sp->u.string->text + 1, oname);
	(--sp)->type = T_STRING;
	str_ref(sp->u.string = str_new((char *) NULL, strlen(pname) + 1L));
	sp->u.string->text[0] = '/';
	strcpy(sp->u.string->text + 1, pname);
	(--sp)->type = T_NUMBER;
	sp->u.number = line;
	call_driver_object("log_error", 4);
	i_del_value(sp++);
    }
}

/*
 * NAME:	interpret->clear()
 * DESCRIPTION:	clear the interpreter stack
 */
void i_clear()
{
    if (cframe < iframe) {
	line = 0;
    } else {
	control *p_ctrl;

	/* keep error context */
	line = i_line(cframe);
	oname = o_name(cframe->obj);
	p_ctrl = cframe->p_ctrl;
	pname = o_name(p_ctrl->inherits[p_ctrl->nvirtuals - 1].obj);

	cframe = iframe - 1;
    }
    i_pop(stackend - sp);
    lock = 0;
}
