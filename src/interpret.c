# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "fcontrol.h"
# include "csupport.h"
# include "table.h"


typedef struct _frame_ {
    object *obj;		/* current object */
    control *ctrl;		/* object control block */
    dataspace *data;		/* dataspace of current object */
    bool external;		/* TRUE if it's an external call */
    control *p_ctrl;		/* program control block */
    unsigned short p_index;	/* program index */
    unsigned short foffset;	/* program function offset */
    dfuncdef *func;		/* current function */
    char *pc;			/* program counter */
    value *fp;			/* frame pointer (value stack) */
} frame;


static value *stack;		/* interpreter stack */
static value *stackend;		/* interpreter stack end */
static int reserved;		/* size of reserved stack */
value *sp;			/* interpreter stack pointer */
static value *ilvp;		/* indexed lvalue stack pointer */
static frame *iframe;		/* stack frames */
static frame *cframe;		/* current frame */
static frame *maxframe;		/* max frame */
static frame *maxmaxframe;	/* max locked frame */
static Int max_cost;		/* max exec cost allowed */
Int exec_cost;			/* interpreter ticks left */
static unsigned short lock;	/* current lock level */
static string *lvstr;		/* the last indexed string */
static char *creator;		/* creator function name */

static value zero_value = { T_INT, TRUE };

/*
 * NAME:	interpret->init()
 * DESCRIPTION:	initialize the interpreter
 */
void i_init(vstack, vreserved, cstack, creserved, create)
int vstack, vreserved, cstack, creserved;
char *create;
{
    stack = ALLOC(value, vstack + vreserved);
    /*
     * The stack array is used both for the stack values (on one side) and
     * the indexed lvalues.
     */
    sp = stackend = stack + vstack + vreserved;
    reserved = vreserved;
    ilvp = stack;
    iframe = ALLOC(frame, cstack + creserved);
    cframe = iframe - 1;
    maxframe = iframe + cstack;
    maxmaxframe = iframe + cstack + creserved;
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
	if (DESTRUCTED(v)) {
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
register int size;
{
    if (sp <= ilvp + ++size + reserved) {
	if (lock == 0 || sp <= ilvp + size) {
	    error("Stack overflow");
	}
    }
}

/*
 * NAME:	interpret->push_value()
 * DESCRIPTION:	push a value on the stack
 */
void i_push_value(v)
register value *v;
{
    *--sp = *v;
    switch (v->type) {
    case T_STRING:
	str_ref(v->u.string);
	break;

    case T_OBJECT:
	if (DESTRUCTED(v)) {
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
void i_odest(obj)
object *obj;
{
    register value *v;
    register Uint count;

    count = obj->count;
    for (v = sp; v < stackend; v++) {
	if (v->type == T_OBJECT && v->u.objcnt == count) {
	    /*
	     * wipe out destructed object on stack
	     */
	    *v = zero_value;
	}
    }
    for (v = stack; v < ilvp; v++) {
	if (v->type == T_OBJECT && v->u.objcnt == count) {
	    /*
	     * wipe out destructed object on stack
	     */
	    *v = zero_value;
	}
    }
}

/*
 * NAME:	interpret->string()
 * DESCRIPTION:	push a string constant on the stack
 */
void i_string(inherit, index)
char inherit;
unsigned short index;
{
    (--sp)->type = T_STRING;
    str_ref(sp->u.string = d_get_strconst(cframe->p_ctrl, inherit, index));
}

/*
 * NAME:	interpret->global()
 * DESCRIPTION:	push a global value on the stack
 */
void i_global(inherit, index)
register int inherit, index;
{
    if (inherit != 0) {
	inherit = cframe->ctrl->inherits[cframe->p_index + inherit].varoffset;
    }
    i_push_value(d_get_variable(cframe->data, inherit + index));
    exec_cost -= 3;
}

/*
 * NAME:	interpret->global_lvalue()
 * DESCRIPTION:	push a global lvalue on the stack
 */
void i_global_lvalue(inherit, index)
register int inherit, index;
{
    if (inherit != 0) {
	inherit = cframe->ctrl->inherits[cframe->p_index + inherit].varoffset;
    }
    (--sp)->type = T_LVALUE;
    sp->u.lval = d_get_variable(cframe->data, inherit + index);
    exec_cost -= 3;
}

/*
 * NAME:	interpret->aggregate()
 * DESCRIPTION:	create an array on the stack
 */
void i_aggregate(size)
register unsigned short size;
{
    register array *a;

    if (size == 0) {
	a = arr_new(0L);
    } else {
	a = arr_new((long) size);
	memcpy(a->elts, sp, size * sizeof(value));
	sp += size;
    }
    (--sp)->type = T_ARRAY;
    arr_ref(sp->u.array = a);
}

/*
 * NAME:	interpret->map_aggregate()
 * DESCRIPTION:	create a mapping on the stack
 */
void i_map_aggregate(size)
register unsigned short size;
{
    register array *a;

    if (size == 0) {
	a = map_new(0L);
    } else {
	a = map_new((long) size);
	memcpy(a->elts, sp, size * sizeof(value));
	sp += size;
	if (ec_push()) {
	    /* error in sorting, delete mapping and pass on error */
	    arr_ref(a);
	    arr_del(a);
	    error((char *) NULL);
	}
	map_sort(a);
	ec_pop();
    }
    (--sp)->type = T_MAPPING;
    arr_ref(sp->u.array = a);
}

/*
 * NAME:	interpret->spread()
 * DESCRIPTION:	push the values in an array on the stack, return the size
 *		of the array - 1
 */
int i_spread(n)
register int n;
{
    register array *a;
    register int i;
    register value *v;

    if (sp->type != T_ARRAY) {
	error("Spread of non-array");
    }
    a = sp->u.array;
    if (n < 0 || n > a->size) {
	/* no lvalues */
	n = a->size;
    }
    if (a->size > 0) {
	i_check_stack((a->size << 1) - n - 1);
	a->ref += a->size - n;
    }
    sp++;

    /* values */
    for (i = 0, v = d_get_elts(a); i < n; i++, v++) {
	i_push_value(v);
    }
    /* lvalues */
    for (n = a->size; i < n; i++) {
	ilvp->type = T_ARRAY;
	(ilvp++)->u.array = a;
	(--sp)->type = T_ALVALUE;
	sp->u.number = i;
    }

    arr_del(a);
    return n - 1;
}

/*
 * NAME:	interpret->index()
 * DESCRIPTION:	index a value, REPLACING it by the indexed value
 */
void i_index()
{
    register int i;
    register value *aval, *ival, *val;
    array *a;

    ival = sp++;
    aval = sp;
    switch (aval->type) {
    case T_STRING:
	if (ival->type != T_INT) {
	    i_del_value(ival);
	    error("Non-numeric string index");
	}
	i = UCHAR(aval->u.string->text[str_index(aval->u.string,
						 (long) ival->u.number)]);
	str_del(aval->u.string);
	aval->type = T_INT;
	aval->u.number = i;
	return;

    case T_ARRAY:
	if (ival->type != T_INT) {
	    i_del_value(ival);
	    error("Non-numeric array index");
	}
	val = &d_get_elts(aval->u.array)[arr_index(aval->u.array,
						   (long) ival->u.number)];
	break;

    case T_MAPPING:
	val = map_index(aval->u.array, ival, (value *) NULL);
	i_del_value(ival);
	break;

    default:
	i_del_value(ival);
	error("Index on bad type");
    }

    a = aval->u.array;
    switch (val->type) {
    case T_STRING:
	str_ref(val->u.string);
	break;

    case T_OBJECT:
	if (DESTRUCTED(val)) {
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
void i_index_lvalue()
{
    register int i;
    register value *lval, *ival, *val;

    ival = sp++;
    lval = sp;
    switch (lval->type) {
    case T_STRING:
	/* for instance, "foo"[1] = 'a'; */
	i_del_value(ival);
	error("Bad lvalue");

    case T_ARRAY:
	if (ival->type != T_INT) {
	    i_del_value(ival);
	    error("Non-numeric array index");
	}
	i = arr_index(lval->u.array, (long) ival->u.number);
	ilvp->type = T_ARRAY;
	(ilvp++)->u.array = lval->u.array;
	lval->type = T_ALVALUE;
	lval->u.number = i;
	return;

    case T_MAPPING:
	ilvp->type = T_ARRAY;
	(ilvp++)->u.array = lval->u.array;
	*ilvp++ = *ival;
	lval->type = T_MLVALUE;
	return;

    case T_LVALUE:
	/*
	 * note: the lvalue is not yet referenced
	 */
	switch (lval->u.lval->type) {
	case T_STRING:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric string index");
	    }
	    i = str_index(lvstr = lval->u.lval->u.string,
			  (long) ival->u.number);
	    ilvp->type = T_LVALUE;
	    (ilvp++)->u.lval = lval->u.lval;
	    /* indexed string lvalues are not referenced */
	    lval->type = T_SLVALUE;
	    lval->u.number = i;
	    return;

	case T_ARRAY:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric array index");
	    }
	    i = arr_index(lval->u.lval->u.array, (long) ival->u.number);
	    ilvp->type = T_ARRAY;
	    arr_ref((ilvp++)->u.array = lval->u.lval->u.array);
	    lval->type = T_ALVALUE;
	    lval->u.number = i;
	    return;

	case T_MAPPING:
	    ilvp->type = T_ARRAY;
	    arr_ref((ilvp++)->u.array = lval->u.lval->u.array);
	    *ilvp++ = *ival;
	    lval->type = T_MLVALUE;
	    return;
	}
	break;

    case T_ALVALUE:
	val = &d_get_elts(ilvp[-1].u.array)[lval->u.number];
	switch (val->type) {
	case T_STRING:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric string index");
	    }
	    i = str_index(lvstr = val->u.string, (long) ival->u.number);
	    ilvp->type = T_INT;
	    (ilvp++)->u.number = lval->u.number;
	    lval->type = T_SALVALUE;
	    lval->u.number = i;
	    return;

	case T_ARRAY:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric array index");
	    }
	    i = arr_index(val->u.array, (long) ival->u.number);
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
	    lval->type = T_MLVALUE;
	    return;
	}
	break;

    case T_MLVALUE:
	val = map_index(ilvp[-2].u.array, &ilvp[-1], (value *) NULL);
	switch (val->type) {
	case T_STRING:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric string index");
	    }
	    i = str_index(lvstr = val->u.string, (long) ival->u.number);
	    lval->type = T_SMLVALUE;
	    lval->u.number = i;
	    return;

	case T_ARRAY:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric array index");
	    }
	    i = arr_index(val->u.array, (long) ival->u.number);
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
	    return;
	}
	break;
    }
    i_del_value(ival);
    error("Index on bad type");
}

/*
 * NAME:	interpret->typename()
 * DESCRIPTION:	return the name of the argument type
 */
char *i_typename(type)
register unsigned short type;
{
    static bool flag;
    static char buf1[8 + 8 + 1], buf2[8 + 8 + 1], *name[] = TYPENAMES;
    register char *buf;

    if (flag) {
	buf = buf1;
	flag = FALSE;
    } else {
	buf = buf2;
	flag = TRUE;
    }
    strcpy(buf, name[type & T_TYPE]);
    type &= T_REF;
    type >>= REFSHIFT;
    if (type > 0) {
	register char *p;

	p = buf + strlen(buf);
	*p++ = ' ';
	do {
	    *p++ = '*';
	} while (--type > 0);
	*p = '\0';
    }
    return buf;
}

/*
 * NAME:	interpret->cast()
 * DESCRIPTION:	cast a value to a type
 */
void i_cast(val, type)
register value *val;
register unsigned short type;
{
    char *tname;

    if (val->type != type &&
	(val->type != T_INT || val->u.number != 0 || type == T_FLOAT)) {
	tname = i_typename(type);
	if (strchr("aeiuoy", tname[0]) != (char *) NULL) {
	    error("Value is not an %s", tname);
	} else {
	    error("Value is not a %s", tname);
	}
    }
}

/*
 * NAME:	interpret->fetch()
 * DESCRIPTION:	fetch the value of an lvalue
 */
void i_fetch()
{
    switch (sp->type) {
    case T_LVALUE:
	i_push_value(sp->u.lval);
	break;

    case T_ALVALUE:
	i_push_value(d_get_elts(ilvp[-1].u.array) + sp->u.number);
	break;

    case T_MLVALUE:
	i_push_value(map_index(ilvp[-2].u.array, &ilvp[-1], (value *) NULL));
	break;

    default:
	/*
	 * Indexed string.
	 * The fetch is always done directly after an lvalue
	 * constructor, so lvstr is valid.
	 */
	(--sp)->type = T_INT;
	sp->u.number = UCHAR(lvstr->text[sp[1].u.number]);
	break;
    }
}

/*
 * NAME:	istr()
 * DESCRIPTION:	create a copy of the argument string, with one char replaced
 */
static value *istr(str, i, val)
register string *str;
unsigned short i;
register value *val;
{
    static value ret = { T_STRING };

    if (val->type != T_INT) {
	error("Non-numeric value in indexed string assignment");
    }

    ret.u.string = (str->ref == 1) ? str : str_new(str->text, (long) str->len);
    ret.u.string->text[i] = val->u.number;
    return &ret;
}

/*
 * NAME:	interpret->store()
 * DESCRIPTION:	Perform an assignment. This invalidates the lvalue.
 */
void i_store(lval, val)
register value *lval, *val;
{
    register value *v;
    register unsigned short i;
    register array *a;

    switch (lval->type) {
    case T_LVALUE:
	d_assign_var(cframe->data, lval->u.lval, val);
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
	d_assign_var(cframe->data, v, istr(v->u.string, i, val));
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
	d_assign_elt(a, v, istr(v->u.string, (unsigned short) lval->u.number,
				val));
	i_del_value(--ilvp);
	--ilvp;
	arr_del(a);
	break;
    }
}

/*
 * NAME:	interpret->set_cost()
 * DESCRIPTION:	set the maximum allowed execution cost
 */
void i_set_cost(cost)
Int cost;
{
    max_cost = cost;
}

/*
 * NAME:	interpret->reset_cost()
 * DESCRIPTION:	reset the execution cost, and return the previous value
 */
Int i_reset_cost()
{
    Int cost;

    cost = exec_cost;
    exec_cost = max_cost;
    return cost;
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
 * NAME:	interpret->set_lock()
 * DESCRIPTION:	set the current lock level
 */
void i_set_lock(l)
unsigned short l;
{
    lock = l;
}

/*
 * NAME:	interpret->query_lock()
 * DESCRIPTION:	return the current lock level
 */
unsigned short i_query_lock()
{
    return lock;
}

/*
 * NAME:	interpret->set_frame()
 * DESCRIPTION:	set the current stack frame level
 */
void i_set_frame(f)
int f;
{
    cframe = iframe + f;
}

/*
 * NAME:	interpret->query_frame()
 * DESCRIPTION:	return the current stack frame level
 */
int i_query_frame()
{
    return cframe - iframe;
}

/*
 * NAME:	interpret->this_program()
 * DESCRIPTION:	return the current program
 */
control *i_this_program()
{
    return cframe->p_ctrl;
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
    return (f->obj->count == 0) ? (object *) NULL : f->obj;
}

/*
 * NAME:	interpret->foffset()
 * DESCRIPTION:	return a pointer to function call offset
 */
char *i_foffset(index)
unsigned short index;
{
    return &cframe->ctrl->funcalls[2L * (cframe->foffset + index)];
}

/*
 * NAME:	interpret->pindex()
 * DESCRIPTION:	return the current program index
 */
int i_pindex()
{
    return cframe->p_index;
}

/*
 * NAME:	interpret->typecheck()
 * DESCRIPTION:	check the argument types given to a function
 */
void i_typecheck(name, ftype, proto, nargs, strict)
char *name, *ftype;
register char *proto;
int nargs;
bool strict;
{
    register int i, n, atype, ptype;
    register char *args;

    i = nargs;
    n = PROTO_NARGS(proto);
    args = PROTO_ARGS(proto);
    while (n > 0 && i > 0) {
	--i;
	ptype = UCHAR(*args);
	if (ptype & T_ELLIPSIS) {
	    ptype &= ~T_ELLIPSIS;
	    if (ptype == T_MIXED || ptype == T_LVALUE) {
		return;
	    }
	} else {
	    args++;
	    --n;
	}

	if (ptype != T_MIXED) {
	    atype = sp[i].type;
	    if (ptype != atype &&
		(strict || atype != T_INT || sp[i].u.number != 0 ||
		 ptype == T_FLOAT) &&
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
# define FETCH4U(pc, v)	((Uint) (v = *(pc)++ << 8, \
				 v |= UCHAR(*(pc)++), v <<= 8, \
				 v |= UCHAR(*(pc)++), v <<= 8, \
				 v |= UCHAR(*(pc)++)))

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
    if (sp->type != T_INT) {
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
    if (sp->type != T_INT) {
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
	if (sp->type == T_INT && sp->u.number == 0) {
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
    register unsigned short instr, u, u2;
    register Uint l;
    register frame *f;
    register char *p;
    register kfunc *kf;
    xfloat flt;
    value *v;
    int size;

    f = cframe;
    size = 0;

    for (;;) {
	if (--exec_cost <= 0 && lock == 0) {
	    error("Maximum execution cost exceeded (%ld)", (long) max_cost);
	}
	instr = FETCH1U(pc);
	f->pc = pc;

	switch (instr & I_INSTR_MASK) {
	case I_PUSH_ZERO:
	    *--sp = zero_value;
	    break;

	case I_PUSH_ONE:
	    (--sp)->type = T_INT;
	    sp->u.number = 1;
	    break;

	case I_PUSH_INT1:
	    (--sp)->type = T_INT;
	    sp->u.number = FETCH1S(pc);
	    break;

	case I_PUSH_INT4:
	    (--sp)->type = T_INT;
	    sp->u.number = FETCH4S(pc, l);
	    break;

	case I_PUSH_FLOAT2:
	    (--sp)->type = T_FLOAT;
	    flt.high = FETCH2U(pc, u);
	    flt.low = 0L;
	    VFLT_PUT(sp, flt);
	    break;

	case I_PUSH_FLOAT6:
	    (--sp)->type = T_FLOAT;
	    flt.high = FETCH2U(pc, u);
	    flt.low = FETCH4U(pc, l);
	    VFLT_PUT(sp, flt);
	    break;

	case I_PUSH_STRING:
	    (--sp)->type = T_STRING;
	    str_ref(sp->u.string = d_get_strconst(f->p_ctrl,
						  f->p_ctrl->ninherits - 1,
						  FETCH1U(pc)));
	    break;

	case I_PUSH_NEAR_STRING:
	    (--sp)->type = T_STRING;
	    u = FETCH1U(pc);
	    str_ref(sp->u.string = d_get_strconst(f->p_ctrl, u, FETCH1U(pc)));
	    break;

	case I_PUSH_FAR_STRING:
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
		u = f->ctrl->inherits[f->p_index + u].varoffset;
	    }
	    i_push_value(d_get_variable(f->data, u + FETCH1U(pc)));
	    exec_cost -= 3;
	    break;

	case I_PUSH_LOCAL_LVALUE:
	    (--sp)->type = T_LVALUE;
	    sp->u.lval = f->fp + FETCH1U(pc);
	    break;

	case I_PUSH_GLOBAL_LVALUE:
	    u = FETCH1U(pc);
	    if (u != 0) {
		u = f->ctrl->inherits[f->p_index + u].varoffset;
	    }
	    (--sp)->type = T_LVALUE;
	    sp->u.lval = d_get_variable(f->data, u + FETCH1U(pc));
	    exec_cost -= 3;
	    break;

	case I_INDEX:
	    i_index();
	    break;

	case I_INDEX_LVALUE:
	    i_index_lvalue();
	    break;

	case I_AGGREGATE:
	    i_aggregate(FETCH2U(pc, u));
	    break;

	case I_MAP_AGGREGATE:
	    i_map_aggregate(FETCH2U(pc, u));
	    break;

	case I_SPREAD:
	    size = i_spread(FETCH1S(pc));
	    break;

	case I_CAST:
	    i_cast(sp, FETCH1U(pc));
	    break;

	case I_FETCH:
	    i_fetch();
	    break;

	case I_STORE:
	    i_store(sp + 1, sp);
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
	    if ((sp->type == T_INT && sp->u.number == 0) ||
		(sp->type == T_FLOAT && VFLT_ISZERO(sp))) {
		pc = p;
	    }
	    break;

	case I_JUMP_NONZERO:
	    p = pc;
	    p += FETCH2S(pc, u);
	    if ((sp->type != T_INT || sp->u.number != 0) &&
		(sp->type != T_FLOAT || !VFLT_ISZERO(sp))) {
		pc = p;
	    }
	    break;

	case I_SWITCH:
	    switch (FETCH1U(pc)) {
	    case 0:
		pc = i_switch_int(pc);
		break;

	    case 1:
		pc = i_switch_range(pc);
		break;

	    case 2:
		pc = i_switch_str(pc);
		break;
	    }
	    break;

	case I_CALL_KFUNC:
	    kf = &KFUN(FETCH1U(pc));
	    if (PROTO_CLASS(kf->proto) & C_VARARGS) {
		/* variable # of arguments */
		u = FETCH1U(pc) + size;
		size = 0;
	    } else {
		/* fixed # of arguments */
		u = PROTO_NARGS(kf->proto);
	    }
	    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
		i_typecheck(kf->name, "kfun", kf->proto, u, TRUE);
	    }
	    u = (*kf->func)(u);
	    if (u != 0) {
		if ((short) u < 0) {
		    error("Too few arguments for kfun %s", kf->name);
		} else if (u <= PROTO_NARGS(kf->proto)) {
		    error("Bad argument %d for kfun %s", u, kf->name);
		} else {
		    error("Too many arguments for kfun %s", kf->name);
		}
	    }
	    break;

	case I_CALL_AFUNC:
	    u = FETCH1U(pc);
	    i_funcall((object *) NULL, 0, u, FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CALL_DFUNC:
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    i_funcall((object *) NULL, f->p_index + u, u2, FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CALL_FUNC:
	    p = &f->ctrl->funcalls[2L * (f->foffset + FETCH2U(pc, u))];
	    i_funcall((object *) NULL, UCHAR(p[0]), UCHAR(p[1]),
		      FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CATCH:
	    p = pc;
	    p += FETCH2S(pc, u);
	    u = lock;
	    v = sp;
	    if (f->obj->flags & O_DRIVER) {
		/* reset execution cost */
		l = exec_cost;
		exec_cost = max_cost;
	    }
	    if (!ec_push()) {
		i_interpret(pc);
		ec_pop();
		pc = f->pc;
	    } else {
		/* error */
		i_log_error(TRUE);
		lock = u;
		cframe = f;
		f->pc = pc = p;
		i_pop(v - sp);
		p = errormesg();
		(--sp)->type = T_STRING;
		str_ref(sp->u.string = str_new(p, (long) strlen(p)));
	    }
	    if (f->obj->flags & O_DRIVER) {
		/* restore execution cost */
		exec_cost = l;
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
 * NAME:	interpret->funcall()
 * DESCRIPTION:	Call a function in an object. The arguments must be on the
 *		stack already.
 */
void i_funcall(obj, p_ctrli, funci, nargs)
register object *obj;
register int p_ctrli, nargs;
int funci;
{
    register frame *f, *pf;
    register char *pc;
    register unsigned short n;
    value val;
# ifdef DEBUG
    value *keep;
# endif

    f = pf = cframe;
    f++;
    if (f == maxmaxframe || (f == maxframe && lock == 0)) {
	error("Function call stack overflow");
    }

    if (f == iframe) {
	/*
	 * top level call
	 */
	exec_cost = max_cost;

	f->obj = obj;
	f->ctrl = obj->ctrl;
	f->data = o_dataspace(obj);
	f->external = TRUE;
    } else if (obj != (object *) NULL) {
	/*
	 * call_other
	 */
	f->obj = obj;
	f->ctrl = obj->ctrl;
	f->data = o_dataspace(obj);
	f->external = TRUE;
    } else {
	/*
	 * local function call
	 */
	f->obj = pf->obj;
	f->ctrl = pf->ctrl;
	f->data = pf->data;
	f->external = FALSE;
    }
    if (exec_cost < 100 && lock == 0) {
	error("Maximum execution cost exceeded (%ld)", (long) max_cost);
    }

    /* set the program control block */
    f->foffset = f->ctrl->inherits[p_ctrli].funcoffset;
    f->p_ctrl = o_control(f->ctrl->inherits[p_ctrli].obj);
    f->p_index = p_ctrli + 1 - f->p_ctrl->ninherits;

    /* get the function */
    f->func = &d_get_funcdefs(f->p_ctrl)[funci];
    if (f->func->class & C_UNDEFINED) {
	error("Undefined function %s",
	      d_get_strconst(f->p_ctrl, f->func->inherit,
			     f->func->index)->text);
    }

    pc = d_get_prog(f->p_ctrl) + f->func->offset;
    if (PROTO_CLASS(pc) & C_TYPECHECKED) {
	/* typecheck arguments */
	i_typecheck(d_get_strconst(f->p_ctrl, f->func->inherit,
				   f->func->index)->text,
		    "function", pc, nargs, FALSE);
    }

    /* handle arguments */
    n = PROTO_NARGS(pc);
    if (n > 0 && (PROTO_ARGS(pc)[n - 1] & T_ELLIPSIS)) {
	register value *v;
	array *a;

	if (nargs >= n) {
	    /* put additional arguments in array */
	    nargs -= n - 1;
	    a = arr_new((long) nargs);
	    v = a->elts + nargs;
	    do {
		*--v = *sp++;
	    } while (--nargs > 0);
	    nargs = n;
	} else {
	    /* make empty arguments array, and optionally push zeroes */
	    i_check_stack(n - nargs);
	    while (++nargs < n) {
		*--sp = zero_value;
	    }
	    a = arr_new(0L);
	}
	(--sp)->type = T_ARRAY;
	arr_ref(sp->u.array = a);
    } else if (nargs > n) {
	/* pop superfluous arguments */
	i_pop(nargs - n);
	nargs = n;
    } else if (nargs < n) {
	/* add missing arguments */
	i_check_stack(n - nargs);
	do {
	    *--sp = zero_value;
	} while (++nargs < n);
    }
    pc += PROTO_SIZE(pc);

    /* check stack depth */
    i_check_stack(FETCH2U(pc, n));

    /* initialize local variables */
    n = FETCH1U(pc);
    if (n > 0) {
	nargs += n;
	do {
	    *--sp = zero_value;
	} while (--n > 0);
    }
    f->fp = sp;

    d_get_funcalls(f->ctrl);	/* make sure they are available */
    exec_cost -= 5;
    cframe++;
# ifdef DEBUG
    keep = sp;
# endif
    if (f->func->class & C_COMPILED) {
	/* compiled function */
	(*pcfunctions[FETCH2U(pc, n)])();
    } else {
	/* interpreted function */
	pc += 2;
	i_interpret(pc);
    }
    --cframe;

    /* clean up stack, move return value upwards */
    val = *sp++;
# ifdef DEBUG
    if (sp != keep) {
	fatal("bad stack pointer after function call");
    }
# endif
    i_pop(nargs);
    *--sp = val;
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

    ctrl = o_control(obj);
    if (!(obj->flags & O_CREATED)) {
	/*
	 * initialize the object
	 */
	obj->flags |= O_CREATED;
	if (i_call(obj, creator, TRUE, 0)) {
	    i_del_value(sp++);
	}
    }

    /* find the function in the symbol table */
    symb = ctrl_symb(ctrl, func);
    if (symb == (dsymbol *) NULL) {
	/* function doesn't exist in symbol table */
	i_pop(nargs);
	return FALSE;
    }

    ctrl = ctrl->inherits[UCHAR(symb->inherit)].obj->ctrl;
    f = &d_get_funcdefs(ctrl)[UCHAR(symb->index)];

    /* check if the function can be called */
    if (!call_static && (f->class & C_STATIC) &&
	(cframe < iframe || cframe->obj != obj)) {
	i_pop(nargs);
	return FALSE;
    }

    /* call the function */
    i_funcall(obj, UCHAR(symb->inherit), UCHAR(symb->index), nargs);

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
    pc += PROTO_SIZE(pc) + 3;
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
	case I_FETCH:
	case I_STORE:
	case I_LOCK:
	case I_RETURN:
	    break;

	case I_PUSH_INT1:
	case I_PUSH_STRING:
	case I_PUSH_LOCAL:
	case I_PUSH_LOCAL_LVALUE:
	case I_SPREAD:
	case I_CAST:
	    pc++;
	    break;

	case I_PUSH_FLOAT2:
	case I_PUSH_NEAR_STRING:
	case I_PUSH_GLOBAL:
	case I_PUSH_GLOBAL_LVALUE:
	case I_AGGREGATE:
	case I_MAP_AGGREGATE:
	case I_JUMP:
	case I_JUMP_ZERO:
	case I_JUMP_NONZERO:
	case I_CALL_AFUNC:
	case I_CATCH:
	    pc += 2;
	    break;

	case I_PUSH_FAR_STRING:
	case I_CALL_DFUNC:
	case I_CALL_FUNC:
	    pc += 3;
	    break;

	case I_PUSH_INT4:
	    pc += 4;
	    break;

	case I_PUSH_FLOAT6:
	    pc += 6;
	    break;

	case I_SWITCH:
	    switch (FETCH1U(pc)) {
	    case 0:
		FETCH2U(pc, u);
		sz = FETCH1U(pc);
		pc += 2 + (u - 1) * (sz + 2);
		break;

	    case 1:
		FETCH2U(pc, u);
		sz = FETCH1U(pc);
		pc += 2 + (u - 1) * (2 * sz + 2);
		break;

	    case 2:
		FETCH2U(pc, u);
		pc += 2;
		if (FETCH1U(pc) == 0) {
		    pc += 2;
		    --u;
		}
		pc += (u - 1) * 5;
		break;
	    }
	    break;

	case I_CALL_KFUNC:
	    if (PROTO_CLASS(KFUN(FETCH1U(pc)).proto) & C_VARARGS) {
		pc++;
	    }
	    break;
	}
    }

    return line;
}

/*
 * NAME:	interpret->call_trace()
 * DESCRIPTION:	return the function call trace
 */
array *i_call_trace()
{
    register frame *f;
    register value *v;
    register string *str;
    register char *name;
    array *a;
    value *elts;

    a = arr_new(cframe - iframe + 1L);
    elts = a->elts;
    for (f = iframe; f <= cframe; f++) {
	elts->type = T_ARRAY;
	arr_ref(elts->u.array = arr_new(5L));
	v = elts->u.array->elts;
	name = o_name(f->obj);
	v[0].type = T_STRING;
	str = str_new((char *) NULL, strlen(name) + 1L);
	str->text[0] = '/';
	strcpy(str->text + 1, name);
	str_ref(v[0].u.string = str);
	name = f->p_ctrl->inherits[f->p_ctrl->ninherits - 1].obj->chain.name;
	v[1].type = T_STRING;
	str = str_new((char *) NULL, strlen(name) + 1L);
	str->text[0] = '/';
	strcpy(str->text + 1, name);
	str_ref(v[1].u.string = str);
	v[2].type = T_STRING;
	str_ref(v[2].u.string = d_get_strconst(f->p_ctrl, f->func->inherit,
					       f->func->index));
	v[3].type = T_INT;
	if (f->func->class & C_COMPILED) {
	    v[3].u.number = 0;
	} else {
	    v[3].u.number = i_line(f);
	}
	v[4].type = T_INT;
	v[4].u.number = f->external;
	elts++;
    }

    return a;
}

/*
 * NAME:	interpret->log_error()
 * DESCRIPTION:	log an error
 */
void i_log_error(flag)
bool flag;
{
    char *err;

    if (ec_push()) {
	message("Error within log_error:\012");	/* LF */
	warning((char *) NULL);
    } else {
	i_lock();
	i_check_stack(2);
	err = errormesg();
	(--sp)->type = T_STRING;
	str_ref(sp->u.string = str_new(err, (long) strlen(err)));
	(--sp)->type = T_INT;
	sp->u.number = flag;
	call_driver_object("log_error", 2);
	i_del_value(sp++);
	i_unlock();
	ec_pop();
    }
}

/*
 * NAME:	interpret->clear()
 * DESCRIPTION:	clear the interpreter stack
 */
void i_clear()
{
    cframe = iframe - 1;
    i_pop(stackend - sp);
    lock = 0;
}
