# include "dgd.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "fcontrol.h"
# include "table.h"

# define DESTRUCTED(k)	(o_object(&(k)) == (object *) NULL)
# define CHECKSP() if (sp <= ilvp + 2) error("Out of interpreter stack space")


typedef struct _frame_ {
    object *obj;		/* current object */
    object *prevobj;		/* previous object */
    unsigned short lock;	/* lock level */
    dataspace *data;		/* dataspace of current object */
    control *v_ctrl;		/* virtual control block */
    unsigned short v_voffset;	/* virtual variable offset */
    control *p_ctrl;		/* program control block */
    unsigned short p_foffset;	/* program function offset */
    unsigned short p_voffset;	/* program variable offset */
    dfuncdef *func;		/* current function */
    value *fp;			/* frame pointer (value stack) */
    unsigned short firstline;	/* first line number of function */
    unsigned short line;	/* current line number */
} frame;


static value *stack;		/* evaluator stack */
static value *stackend;		/* evaluator stack end */
value *sp;			/* evaluator stack pointer */
static value *ilvp;		/* indexed lvalue stack pointer */
static frame *iframe;		/* stack frames */
static frame *cframe;		/* current frame */
static frame *maxframe;		/* max frame */
static frame *maxmaxframe;	/* max locked frame */
static long ticksleft;		/* interpreter ticks left */
static long maxticks;		/* max ticks allowed */
static string *lvstr;		/* the last indexed string */

static value zero_value = { T_NUMBER, TRUE };

/*
 * NAME:	interpret->init()
 * DESCRIPTION:	initialize the interpreter
 */
void i_init(stacksize, maxdepth, maxmaxdepth, maxcost)
int stacksize, maxdepth, maxmaxdepth;
long maxcost;
{
    stack = ALLOC(value, stacksize);
    /*
     * The stack array is used both for the stack values (on one side) and
     * the indexed lvalues.
     */
    sp = stackend = stack + stacksize;
    ilvp = stack;
    iframe = ALLOC(frame, maxmaxdepth);
    cframe = iframe - 1;
    maxframe = iframe + maxdepth;
    maxmaxframe = iframe + maxmaxdepth;
    maxticks = maxcost;
}

/*
 * NAME:	interpret->clear()
 * DESCRIPTION:	clear the interpreter stack
 */
void i_clear()
{
    i_pop(stackend - sp);
    cframe = iframe - 1;
}

/*
 * NAME:	interpret->ref_value()
 * DESCRIPTION:	reference a value
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
 * DESCRIPTION:	delete a value (which can be an lvalue)
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
 * DESCRIPTION:	check if there is room on the stack for a new value
 */
void i_check_stack()
{
    CHECKSP();
}

/*
 * NAME:	interpret->push_value()
 * DESCRIPTION:	push a value on the stack
 */
void i_push_value(v)
register value *v;
{
    CHECKSP();
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
    *--sp = *v;
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
register objkey *key;
{
    register value *v;

    for (v = sp; v < stackend; v++) {
	if (v->type == T_OBJECT &&
	    key->index == v->u.object.index && key->count == v->u.object.count)
	{
	    /*
	     * wipe out destructed object on stack
	     */
	    *v = zero_value;
	}
    }
    for (v = stack; v < ilvp; v++) {
	if (v->type == T_OBJECT &&
	    key->index == v->u.object.index && key->count == v->u.object.count)
	{
	    /*
	     * wipe out destructed object on stack
	     */
	    *v = zero_value;
	}
    }
}

/*
 * NAME:	interpret->index()
 * DESCRIPTION:	index a value, REPLACING it by the indexed va
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
	i = map_index(aval->u.array, ival, FALSE);
	if (i < 0) {
	    val = &zero_value;
	} else {
	    val = &aval->u.array->elts[i];
	}
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

    CHECKSP();	/* not required in all cases... */
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
	i_ref_value(ival);
	*ilvp++ = *ival;
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
	    (ilvp++)->u.lval = lval->u.lval;
	    /* indexed string lvalues are never referenced */
	    lval->type = T_SLVALUE;
	    lval->u.number = i;
	    return;

	case T_ARRAY:
	    if (ival->type != T_NUMBER) {
		error("Non-numeric array index");
	    }
	    i = arr_index(lval->u.lval->u.array, ival->u.number);
	    ilvp->type = T_ARRAY;
	    (ilvp++)->u.array = lval->u.lval->u.array;
	    lval->type = T_ALVALUE;
	    lval->u.number = i;
	    return;

	case T_MAPPING:
	    ilvp->type = T_ARRAY;
	    (ilvp++)->u.array = lval->u.lval->u.array;
	    i_ref_value(ival);
	    *ilvp++ = *ival;
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
	    i_ref_value(ival);
	    *ilvp++ = *ival;
	    lval->type = T_MLVALUE;
	    lval->u.number = i;
	    return;
	}
	break;

    case T_MLVALUE:
	i = map_index(ilvp[-2].u.array, &ilvp[-1], FALSE);
	if (i >= 0) {
	    val = &ilvp[-2].u.array->elts[i];
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
		arr_ref(val->u.array);		/* has to be first */
		arr_del(ilvp[-1].u.array);	/* has to be second */
		ilvp[-1].u.array = val->u.array;
		lval->type = T_ALVALUE;
		lval->u.number = i;
		return;

	    case T_MAPPING:
		arr_ref(val->u.array);		/* has to be first */
		arr_del(ilvp[-2].u.array);	/* has to be second */
		ilvp[-2].u.array = val->u.array;
		i_del_value(&ilvp[-1]);
		i_ref_value(ival);
		ilvp[-1] = *ival;
		lval->u.number = i;
		return;
	    }
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
	error("Non-numeric string index");
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
	d_assign_elt(a, (unsigned short) lval->u.number, val);
	arr_del(a);
	break;

    case T_MLVALUE:
	i = map_index(a = ilvp[-2].u.array, &ilvp[-1], TRUE);
	i_del_value(--ilvp);
	--ilvp;
	d_assign_elt(a, (unsigned short) lval->u.number, val);
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
	--ilvp;
	d_assign_elt(a, (unsigned short) ilvp->u.number,
		     istr(v->u.string, i, val));
	arr_del(a);
	break;

    case T_SMLVALUE:
	a = ilvp[-2].u.array;
	i = map_index(a, &ilvp[-1], TRUE);
	v = &a->elts[i];
	if (v->type != T_STRING || lval->u.number >= v->u.string->len) {
	    /*
	     * The lvalue was changed.
	     */
	    error("Lvalue disappeared!");
	}
	i_del_value(--ilvp);
	--ilvp;
	d_assign_elt(a, i, istr(v->u.string,
		     (unsigned short) lval->u.number, val));
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
    cframe->lock++;
}

/*
 * NAME:	interpret->unlock()
 * DESCRIPTION:	unlock the current frame
 */
void i_unlock()
{
    cframe->lock--;
}

/*
 * NAME:	interpret->locklvl()
 * DESCRIPTION:	return the current lock level
 */
unsigned short i_locklvl()
{
    return cframe->lock;
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
 * DESCRIPTION:	return the previous object in the call_other chain
 */
object *i_prev_object()
{
    return cframe->prevobj;
}

# define FETCH1S(pc)	SCHAR(*(pc)++)
# define FETCH1U(pc)	UCHAR(*(pc)++)
# define FETCH2S(pc, v)	((short) (v = *(pc)++ << 8, v |= UCHAR(*(pc)++)))
# define FETCH2U(pc, v)	((unsigned short) (v = *(pc)++ << 8, \
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
void i_funcall(obj, v_ctrli, p_ctrli, funci, nargs)
register object *obj;
register int v_ctrli, p_ctrli, funci, nargs;
{
    register frame *f, *pf;
    register char *pc;
    register unsigned short n;
    value val;

    f = pf = cframe;
    f++;
    if (f == maxmaxframe || (f == maxframe && pf->lock == 0)) {
	error("Function call stack size exceeded");
    }

    if (f == iframe) {
	/*
	 * top level call
	 */
	ticksleft = maxticks;

	f->obj = obj;
	f->prevobj = (object *) NULL;
	f->lock = 0;
	f->data = o_dataspace(obj);
	f->v_ctrl = obj->ctrl;
	f->v_voffset = 0;
    } else if (obj != (object *) NULL) {
	/*
	 * call_other
	 */
	f->obj = obj;
	f->prevobj = pf->obj;
	f->lock = pf->lock;
	f->data = o_dataspace(obj);
	f->v_ctrl = obj->ctrl;
	f->v_voffset = 0;
    } else {
	/*
	 * local function call or labeled function call
	 */
	f->obj = pf->obj;
	f->prevobj = pf->prevobj;
	f->lock = pf->lock;
	f->data = pf->data;
	f->v_voffset = pf->v_voffset;
	if (v_ctrli == 0) {
	    /* local or virtually inherited function call */
	    f->v_ctrl = pf->v_ctrl;
	} else {
	    /* labeled inherited function call */
	    f->v_ctrl = pf->v_ctrl->inherits[v_ctrli].obj->ctrl;
	    f->v_voffset += pf->v_ctrl->inherits[v_ctrli].varoffset;
	}
    }

    /* set the program control block */
    f->p_ctrl = o_control(f->v_ctrl->inherits[p_ctrli].obj);
    if (p_ctrli == 0) {
	/* it's the auto object */
	f->p_foffset = 0;
	f->p_voffset = 0;
    } else {
	f->p_foffset = f->v_ctrl->inherits[p_ctrli].funcoffset;
	f->p_voffset = f->v_voffset + f->v_ctrl->inherits[p_ctrli].varoffset;
    }

    /* get the function */
    f->func = &d_get_funcdefs(f->p_ctrl)[funci];
    if (f->func->class & C_UNDEFINED) {
	error("Undefined function %s",
	      d_get_strconst(f->p_ctrl, f->func->inherit,
			     f->func->index)->text);
    }
    pc = d_get_prog(f->p_ctrl) + f->func->offset;

    /* handle arguments */
    if (nargs > PROTO_NARGS(pc)) {
	/* pop superfluous arguments */
	i_pop(nargs - PROTO_NARGS(pc));
	nargs = PROTO_NARGS(pc);
    } else {
	/* add missing arguments */
	while (nargs < PROTO_NARGS(pc)) {
	    CHECKSP();
	    *--sp = zero_value;
	    nargs++;
	}
    }
    pc += PROTO_SIZE(pc);

    /* initialize local variables */
    for (n = FETCH1U(pc); n > 0; --n) {
	CHECKSP();
	*--sp = zero_value;
    }
    f->fp = sp;
    f->line = f->firstline = FETCH2U(pc, n);

    /* interpret function code */
    ticksleft -= 5;
    cframe++;
    i_interpret(pc);
    --cframe;

    /* clean up stack, move return value upwards */
    val = *sp++;
    i_pop((f->fp - sp) + nargs);
    *--sp = val;
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
    for (n = PROTO_NARGS(proto), args = PROTO_ARGS(proto); n > 0; --n) {
	if (i == 0) {
	    if (!(PROTO_CLASS(proto) & C_VARARGS) && strict) {
		error("Too few arguments for %s %s", ftype, name);
	    }
	    break;
	}
	--i;
	ptype = UCHAR(*args++);
	if (ptype != T_MIXED) {
	    atype = sp[i].type;
	    if (ptype != atype && (atype != T_ARRAY || !(ptype & T_REF))) {
		error("Bad argument %d for %s %s", nargs - i, ftype, name);
	    }
	}
    }
    if (i != 0 && !(PROTO_CLASS(proto) & C_VARARGS) && strict) {
	error("Too many arguments for %s %s", ftype, name);
    }
}

/*
 * NAME:	interpret->switch_int()
 * DESCRIPTION:	handle an int switch
 */
static char *i_switch_int(pc)
register char *pc;
{
    register unsigned short h, l, m;
    register Int num;
    register char *p, *dflt;

    FETCH2U(pc, h);
    p = pc;
    dflt = p + FETCH2S(pc, l);
    --h;
    if (sp->type != T_NUMBER) {
	return dflt;
    }

    l = 0;
    while (l < h) {
	m = (l + h) >> 1;
	p = pc + 3 * m;
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
    return dflt;
}

/*
 * NAME:	interpret->switch_range()
 * DESCRIPTION:	handle a range switch
 */
static char *i_switch_range(pc)
register char *pc;
{
    register unsigned short h, l, m;
    register Int num;
    register char *p, *dflt;

    FETCH2U(pc, h);
    p = pc;
    dflt = p + FETCH2S(pc, l);
    --h;
    if (sp->type != T_NUMBER) {
	return dflt;
    }

    l = 0;
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
    value *oldsp;
    array *a;

    f = cframe;
    for (;;) {
	if (--ticksleft <= 0 && f->lock == 0) {
	    error("Maximum execution cost exceeded %ld", maxticks - ticksleft);
	}
	instr = FETCH1U(pc);
	f->line += (instr >> I_LINE_SHIFT) - 1;

	switch (instr & I_INSTR_MASK) {
	case I_PUSH_ZERO:
	    CHECKSP();
	    *--sp = zero_value;
	    break;

	case I_PUSH_ONE:
	    CHECKSP();
	    (--sp)->type = T_NUMBER;
	    sp->u.number = 1;
	    break;

	case I_PUSH_INT1:
	    CHECKSP();
	    (--sp)->type = T_NUMBER;
	    sp->u.number = FETCH1S(pc);
	    break;

	case I_PUSH_INT2:
	    CHECKSP();
	    (--sp)->type = T_NUMBER;
	    sp->u.number = FETCH2S(pc, u);
	    break;

	case I_PUSH_INT4:
	    CHECKSP();
	    (--sp)->type = T_NUMBER;
	    sp->u.number = FETCH4S(pc, l);
	    break;

	case I_PUSH_STRING:
	    CHECKSP();
	    (--sp)->type = T_STRING;
	    str_ref(sp->u.string = d_get_strconst(f->p_ctrl,
						  f->p_ctrl->nvirtuals - 1,
						  FETCH1U(pc)));
	    break;

	case I_PUSH_FAR_STRING:
	    CHECKSP();
	    (--sp)->type = T_STRING;
	    u = FETCH1U(pc);
	    str_ref(sp->u.string = d_get_strconst(f->p_ctrl, u, FETCH1U(pc)));
	    break;

	case I_PUSH_LOCAL:
	    i_push_value(f->fp + FETCH1U(pc));
	    break;

	case I_PUSH_GLOBAL:
	    u = FETCH1U(pc);
	    if (u != 0) {
		u = f->p_voffset + f->p_ctrl->inherits[u].varoffset;
	    }
	    i_push_value(d_get_variable(f->data, u + FETCH1U(pc)));
	    ticksleft -= 3;
	    break;

	case I_PUSH_LOCAL_LVALUE:
	    CHECKSP();
	    (--sp)->type = T_LVALUE;
	    sp->u.lval = f->fp + FETCH1U(pc);
	    break;

	case I_PUSH_GLOBAL_LVALUE:
	    u = FETCH1U(pc);
	    if (u != 0) {
		u = f->p_voffset + f->p_ctrl->inherits[u].varoffset;
	    }
	    CHECKSP();
	    (--sp)->type = T_LVALUE;
	    sp->u.lval = d_get_variable(f->data, u + FETCH1U(pc));
	    ticksleft -= 3;
	    break;

	case I_INDEX:
	    i_index(sp + 1, sp);
	    i_del_value(sp++);
	    i_ref_value(sp);
	    break;

	case I_INDEX_LVALUE:
	    i_index_lvalue(sp + 1, sp);
	    i_del_value(sp++);
	    break;

	case I_AGGREGATE:
	    FETCH2U(pc, u);
	    if (u == 0) {
		CHECKSP();
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
		CHECKSP();
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

	case I_FETCH:
	    switch (sp->type) {
	    case T_LVALUE:
		i_push_value(sp->u.lval);
		break;

	    case T_ALVALUE:
		i_push_value(d_get_elts(ilvp[-1].u.array) + sp->u.number);
		break;

	    case T_MLVALUE:
		u = map_index(ilvp[-2].u.array, &ilvp[-1], TRUE);
		i_push_value(ilvp[-2].u.array->elts + u);
		break;

	    default:
		/*
		 * Indexed string.
		 * The fetch is always done directly after an lvalue
		 * constructor, so lvstr is valid.
		 */
		CHECKSP();
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
	    pc = p + FETCH2S(pc, u);
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
	    p = &f->v_ctrl->funcalls[2L * (f->p_foffset + FETCH2U(pc, u))];
	    i_funcall((object *) NULL, 0, UCHAR(p[0]), UCHAR(p[1]),
		      FETCH1U(pc));
	    break;

	case I_CATCH:
	    u = pc + FETCH2S(pc, u) - f->p_ctrl->prog;
	    if (ec_push()) {
		/* error */
		pc = f->p_ctrl->prog + u;
		f->lock = u2;
		i_pop(oldsp - sp);
		CHECKSP();
		p = errormesg();
		(--sp)->type = T_STRING;
		str_ref(sp->u.string = str_new(p, (long) strlen(p)));
	    } else {
		u2 = f->lock;
		oldsp = sp;
		i_interpret(pc);
		ec_pop();
		/* sp is back at oldsp */
		CHECKSP();
		(--sp)->type = T_NUMBER;
		sp->u.number = 0;
	    }
	    break;

	case I_LOCK:
	    f->lock++;
	    i_interpret(pc);
	    f->lock--;
	    break;

	case I_RETURN:
	    return;

	case I_LINE:
	    f->line = f->firstline + FETCH1U(pc);
	    break;

	case I_LINE2:
	    f->line = FETCH2U(pc, u);
	    break;
	}

	if (instr & I_POP_BIT) {
	    /* pop the result of the last operation */
	    i_del_value(sp++);
	}
    }
}

/*
 * NAME:	interpret->apply()
 * DESCRIPTION:	Attempt to call a function in an object. Return TRUE if
 *		the call succeeded.
 */
bool i_apply(obj, func, call_static, nargs)
object *obj;
char *func;
bool call_static;
int nargs;
{
    register dsymbol *symb;
    register dfuncdef *f;
    register control *ctrl;

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

    /* check argument types, if needed */
    if (f->class & C_TYPECHECKED) {
	i_typecheck(func, "function", d_get_prog(ctrl) + f->offset, nargs,
		    FALSE);
    }

    /* call the function */
    i_funcall(obj, 0, UCHAR(symb->inherit), UCHAR(symb->index), nargs);

    return TRUE;
}

/*
 * NAME:	dump_trace()
 * DESCRIPTION:	dump the function call trace on stderr
 */
void i_dump_trace(fp)
register FILE *fp;
{
    register frame *f;

    for (f = iframe; f <= cframe; f++) {
	register object *prog;

	prog = f->p_ctrl->inherits[f->p_ctrl->nvirtuals - 1].obj;
	fprintf(fp, "%4u %-17s /%s", f->line,
		d_get_strconst(f->p_ctrl, f->func->inherit,
			       f->func->index)->text,
		prog->chain.name);
	if (f->obj->ctrl != f->p_ctrl) {
	    register int len;
	    char *p;

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
}
