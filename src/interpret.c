# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "control.h"
# include "csupport.h"
# include "table.h"

# ifdef DEBUG
# undef EXTRA_STACK
# define EXTRA_STACK	0
# endif

typedef struct _inhash_ {
    Uint ocount;		/* object count */
    uindex iindex;		/* inherit index */
    uindex coindex;		/* class name program reference */
    Uint class;			/* class name string reference */
} inhash;

static value stack[MIN_STACK];	/* initial stack */
static frame topframe;		/* top frame */
static rlinfo rlim;		/* top rlimits info */
frame *cframe;			/* current frame */
static char *creator;		/* creator function name */
static unsigned int clen;	/* creator function name length */
static bool stricttc;		/* strict typechecking */
static inhash ihash[INHASHSZ];	/* instanceof hashtable */

int nil_type;			/* type of nil value */
value zero_int = { T_INT, TRUE };
value zero_float = { T_FLOAT, TRUE };
value nil_value = { T_NIL, TRUE };

/*
 * NAME:	interpret->init()
 * DESCRIPTION:	initialize the interpreter
 */
void i_init(create, flag)
char *create;
int flag;
{
    topframe.oindex = OBJ_NONE;
    topframe.fp = topframe.sp = stack + MIN_STACK;
    topframe.stack = topframe.lip = stack;
    rlim.nodepth = TRUE;
    rlim.noticks = TRUE;
    topframe.rlim = &rlim;
    topframe.level = 0;
    topframe.atomic = FALSE;
    cframe = &topframe;

    creator = create;
    clen = strlen(create);
    stricttc = flag;

    nil_value.type = nil_type = (stricttc) ? T_NIL : T_INT;
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

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr_ref(v->u.array);
	break;
    }
}

/*
 * NAME:	interpret->del_value()
 * DESCRIPTION:	dereference a value (not an lvalue)
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
    case T_LWOBJECT:
	arr_del(v->u.array);
	break;
    }
}

/*
 * NAME:	interpret->copy()
 * DESCRIPTION:	copy values from one place to another
 */
void i_copy(v, w, len)
register value *v, *w;
register unsigned int len;
{
    register value *o;

    for ( ; len != 0; --len) {
	switch (w->type) {
	case T_STRING:
	    str_ref(w->u.string);
	    break;

	case T_OBJECT:
	    if (DESTRUCTED(w)) {
		*v++ = nil_value;
		w++;
		continue;
	    }
	    break;

	case T_LWOBJECT:
	    o = d_get_elts(w->u.array);
	    if (DESTRUCTED(o)) {
		*v++ = nil_value;
		w++;
		continue;
	    }
	    /* fall through */
	case T_ARRAY:
	case T_MAPPING:
	    arr_ref(w->u.array);
	    break;
	}
	*v++ = *w++;
    }
}

/*
 * NAME:	interpret->grow_stack()
 * DESCRIPTION:	check if there is room on the stack for new values; if not,
 *		make space
 */
void i_grow_stack(f, size)
register frame *f;
int size;
{
    if (f->sp < f->lip + size + MIN_STACK) {
	register int spsize, lisize;
	register value *v, *stk;
	register long offset;

	/*
	 * extend the local stack
	 */
	spsize = f->fp - f->sp;
	lisize = f->lip - f->stack;
	size = ALGN(spsize + lisize + size + MIN_STACK, 8);
	stk = ALLOC(value, size);
	offset = (long) (stk + size) - (long) f->fp;

	/* move lvalue index stack values */
	if (lisize != 0) {
	    memcpy(stk, f->stack, lisize * sizeof(value));
	}
	f->lip = stk + lisize;

	/* move stack values */
	v = stk + size;
	if (spsize != 0) {
	    memcpy(v - spsize, f->sp, spsize * sizeof(value));
	    do {
		--v;
		if ((v->type == T_LVALUE || v->type == T_SLVALUE) &&
		    v->u.lval >= f->sp && v->u.lval < f->fp) {
		    v->u.lval = (value *) ((long) v->u.lval + offset);
		}
	    } while (--spsize > 0);
	}
	f->sp = v;

	/* replace old stack */
	if (f->sos) {
	    /* stack on stack: alloca'd */
	    AFREE(f->stack);
	    f->sos = FALSE;
	} else if (f->stack != stack) {
	    FREE(f->stack);
	}
	f->stack = stk;
	f->fp = stk + size;
    }
}

/*
 * NAME:	interpret->push_value()
 * DESCRIPTION:	push a value on the stack
 */
void i_push_value(f, v)
frame *f;
register value *v;
{
    register value *o;

    *--f->sp = *v;
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
	    *f->sp = nil_value;
	}
	break;

    case T_LWOBJECT:
	o = d_get_elts(v->u.array);
	if (DESTRUCTED(o)) {
	    /*
	     * can't wipe out the original, since it may be a value from a
	     * mapping
	     */
	    *f->sp = nil_value;
	    break;
	}
	/* fall through */
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
void i_pop(f, n)
register frame *f;
register int n;
{
    register value *v;

    for (v = f->sp; --n >= 0; v++) {
	switch (v->type) {
	case T_STRING:
	    str_del(v->u.string);
	    break;

	case T_LVALUE:
	    if (v->oindex == T_CLASS) {
		--f->lip;
	    }
	    break;

	case T_ALVALUE:
	    if (v->oindex == T_CLASS) {
		--f->lip;
	    }
	    --f->lip;
	case T_ARRAY:
	case T_MAPPING:
	case T_LWOBJECT:
	    arr_del(v->u.array);
	    break;

	case T_SLVALUE:
	    if (v->oindex == T_CLASS) {
		--f->lip;
	    }
	    str_del((--f->lip)->u.string);
	    break;

	case T_MLVALUE:
	    if (v->oindex == T_CLASS) {
		--f->lip;
	    }
	    i_del_value(--f->lip);
	    arr_del(v->u.array);
	    break;

	case T_SALVALUE:
	    if (v->oindex == T_CLASS) {
		--f->lip;
	    }
	    str_del((--f->lip)->u.string);
	    --f->lip;
	    arr_del(v->u.array);
	    break;

	case T_SMLVALUE:
	    if (v->oindex == T_CLASS) {
		--f->lip;
	    }
	    str_del((--f->lip)->u.string);
	    i_del_value(--f->lip);
	    arr_del(v->u.array);
	    break;
	}
    }
    f->sp = v;
}

/*
 * NAME:	interpret->reverse()
 * DESCRIPTION:	reverse the order of arguments on the stack
 */
void i_reverse(f, n)
frame *f;
register int n;
{
    value sp[MAX_LOCALS];
    value lip[3 * MAX_LOCALS];
    register value *v1, *v2, *w1, *w2;

    if (n > 1) {
	/*
	 * more than one argument
	 */
	v1 = f->sp;
	v2 = sp;
	w1 = lip;
	w2 = f->lip;
	memcpy(v2, v1, n * sizeof(value));
	v1 += n;

	do {
	    switch (v2->type) {
	    case T_LVALUE:
		if (v2->oindex == T_CLASS) {
		    *w1++ = *--w2;
		}
		break;

	    case T_SLVALUE:
	    case T_ALVALUE:
	    case T_MLVALUE:
		if (v2->oindex == T_CLASS) {
		    w2 -= 2;
		    *w1++ = w2[0];
		    *w1++ = w2[1];
		} else {
		    *w1++ = *--w2;
		}
		break;

	    case T_SALVALUE:
	    case T_SMLVALUE:
		if (v2->oindex == T_CLASS) {
		    w2 -= 3;
		    *w1++ = w2[0];
		    *w1++ = w2[1];
		    *w1++ = w2[2];
		} else {
		    w2 -= 2;
		    *w1++ = w2[0];
		    *w1++ = w2[1];
		}
		break;
	    }

	    *--v1 = *v2++;
	} while (--n != 0);

	/*
	 * copy back lvalue indices, if needed
	 */
	n = f->lip - w2;
	if (n > 1) {
	    memcpy(w2, lip, n * sizeof(value));
	}
    }
}

/*
 * NAME:	interpret->odest()
 * DESCRIPTION:	replace all occurrances of an object on the stack by nil
 */
void i_odest(prev, obj)
register frame *prev;
object *obj;
{
    register frame *f;
    register Uint count;
    register value *v;
    register unsigned short n;

    count = obj->count;

    /* wipe out objects in stack frames */
    for (;;) {
	f = prev;
	for (v = f->sp; v < f->fp; v++) {
	    switch (v->type) {
	    case T_OBJECT:
		if (v->u.objcnt == count) {
		    *v = nil_value;
		}
		break;

	    case T_LWOBJECT:
		if (v->u.array->elts[0].u.objcnt == count) {
		    arr_del(v->u.array);
		    *v = nil_value;
		}
		break;
	    }
	}
	for (v = f->lip; --v >= f->stack; ) {
	    switch (v->type) {
	    case T_OBJECT:
		if (v->u.objcnt == count) {
		    *v = nil_value;
		}
		break;

	    case T_LWOBJECT:
		if (v->u.array->elts[0].u.objcnt == count) {
		    arr_del(v->u.array);
		    *v = nil_value;
		}
		break;
	    }
	}

	prev = f->prev;
	if (prev == (frame *) NULL) {
	    break;
	}
	if ((f->func->class & C_ATOMIC) && !prev->atomic) {
	    /*
	     * wipe out objects in arguments to atomic function call
	     */
	    for (n = f->nargs, v = prev->sp; n != 0; --n, v++) {
		switch (v->type) {
		case T_OBJECT:
		    if (v->u.objcnt == count) {
			*v = nil_value;
		    }
		    break;

		case T_LWOBJECT:
		    if (v->u.array->elts[0].u.objcnt == count) {
			arr_del(v->u.array);
			*v = nil_value;
		    }
		    break;
		}
	    }
	    break;
	}
    }
}

/*
 * NAME:	interpret->string()
 * DESCRIPTION:	push a string constant on the stack
 */
void i_string(f, inherit, index)
frame *f;
int inherit;
unsigned int index;
{
    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, inherit, index));
}

/*
 * NAME:	interpret->aggregate()
 * DESCRIPTION:	create an array on the stack
 */
void i_aggregate(f, size)
register frame *f;
register unsigned int size;
{
    register array *a;

    if (size == 0) {
	a = arr_new(f->data, 0L);
    } else {
	register value *v, *elts;

	i_add_ticks(f, size);
	a = arr_new(f->data, (long) size);
	elts = a->elts + size;
	v = f->sp;
	do {
	    *--elts = *v++;
	} while (--size != 0);
	d_ref_imports(a);
	f->sp = v;
    }
    PUSH_ARRVAL(f, a);
}

/*
 * NAME:	interpret->map_aggregate()
 * DESCRIPTION:	create a mapping on the stack
 */
void i_map_aggregate(f, size)
register frame *f;
register unsigned int size;
{
    register array *a;

    if (size == 0) {
	a = map_new(f->data, 0L);
    } else {
	register value *v, *elts;

	i_add_ticks(f, size);
	a = map_new(f->data, (long) size);
	elts = a->elts + size;
	v = f->sp;
	do {
	    *--elts = *v++;
	} while (--size != 0);
	f->sp = v;
	if (ec_push((ec_ftn) NULL)) {
	    /* error in sorting, delete mapping and pass on error */
	    arr_ref(a);
	    arr_del(a);
	    error((char *) NULL);
	}
	map_sort(a);
	ec_pop();
	d_ref_imports(a);
    }
    PUSH_MAPVAL(f, a);
}

/*
 * NAME:	interpret->spread()
 * DESCRIPTION:	push the values in an array on the stack, return the size
 *		of the array - 1
 */
int i_spread(f, n, vtype, class)
register frame *f;
register int n, vtype;
Uint class;
{
    register array *a;
    register int i;
    register value *v;

    if (f->sp->type != T_ARRAY) {
	error("Spread of non-array");
    }
    a = f->sp->u.array;
    if (n < 0 || n > a->size) {
	/* no lvalues */
	n = a->size;
    }
    if (a->size > 0) {
	i_add_ticks(f, a->size);
	i_grow_stack(f, (a->size << 1) - n - 1);
	a->ref += a->size - n;
    }
    f->sp++;

    /* values */
    for (i = 0, v = d_get_elts(a); i < n; i++, v++) {
	i_push_value(f, v);
    }
    /* lvalues */
    for (n = a->size; i < n; i++) {
	(--f->sp)->type = T_ALVALUE;
	f->sp->oindex = vtype;
	f->sp->u.array = a;
	f->lip->type = T_INT;
	(f->lip++)->u.number = i;
	if (vtype == T_CLASS) {
	    f->lip->type = T_INT;
	    (f->lip++)->u.number = class;
	}
    }

    arr_del(a);
    return n - 1;
}

/*
 * NAME:	interpret->global()
 * DESCRIPTION:	push a global value on the stack
 */
void i_global(f, inherit, index)
register frame *f;
register int inherit, index;
{
    i_add_ticks(f, 4);
    if (inherit != 0) {
	inherit = f->ctrl->inherits[f->p_index - inherit].varoffset;
    }
    if (f->lwobj == (array *) NULL) {
	i_push_value(f, d_get_variable(f->data, inherit + index));
    } else {
	i_push_value(f, &f->lwobj->elts[2 + inherit + index]);
    }
}

/*
 * NAME:	interpret->global_lvalue()
 * DESCRIPTION:	push a global lvalue on the stack
 */
void i_global_lvalue(f, inherit, index, vtype, class)
register frame *f;
register int inherit;
int index, vtype;
Uint class;
{
    i_add_ticks(f, 4);
    if (inherit != 0) {
	inherit = f->ctrl->inherits[f->p_index - inherit].varoffset;
    }
    if (f->lwobj == (array *) NULL) {
	(--f->sp)->type = T_LVALUE;
	f->sp->oindex = vtype;
	f->sp->u.lval = d_get_variable(f->data, inherit + index);
    } else {
	(--f->sp)->type = T_ALVALUE;
	f->sp->oindex = vtype;
	arr_ref(f->sp->u.array = f->lwobj);
	f->lip->type = T_INT;
	(f->lip++)->u.number = 2 + inherit + index;
    }

    if (vtype == T_CLASS) {
	f->lip->type = T_INT;
	(f->lip++)->u.number = class;
    }
}

/*
 * NAME:	interpret->index()
 * DESCRIPTION:	index a value, REPLACING it with the indexed value
 */
void i_index(f)
register frame *f;
{
    register int i;
    register value *aval, *ival, *val;
    array *a;

    i_add_ticks(f, 2);
    ival = f->sp++;
    aval = f->sp;
    switch (aval->type) {
    case T_STRING:
	if (ival->type != T_INT) {
	    i_del_value(ival);
	    error("Non-numeric string index");
	}
	i = UCHAR(aval->u.string->text[str_index(aval->u.string,
						 (long) ival->u.number)]);
	str_del(aval->u.string);
	PUT_INTVAL(aval, i);
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
	val = map_index(f->data, aval->u.array, ival, (value *) NULL);
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
	    val = &nil_value;
	}
	break;

    case T_LWOBJECT:
	ival = d_get_elts(val->u.array);
	if (DESTRUCTED(ival)) {
	    val = &nil_value;
	    break;
	}
	/* fall through */
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
void i_index_lvalue(f, vtype, class)
register frame *f;
int vtype;
Uint class;
{
    register int i;
    register value *lval, *ival, *val;

    i_add_ticks(f, 2);
    ival = f->sp++;
    lval = f->sp;
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
	lval->type = T_ALVALUE;
	lval->oindex = vtype;
	f->lip->type = T_INT;
	(f->lip++)->u.number = i;
	break;

    case T_MAPPING:
	lval->type = T_MLVALUE;
	lval->oindex = vtype;
	*f->lip++ = *ival;
	break;

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
	    i = str_index(lval->u.lval->u.string, (long) ival->u.number);
	    lval->type = T_SLVALUE;
	    lval->oindex = vtype;
	    f->lip->type = T_STRING;
	    f->lip->oindex = i;
	    str_ref((f->lip++)->u.string = lval->u.lval->u.string);
	    break;

	case T_ARRAY:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric array index");
	    }
	    i = arr_index(lval->u.lval->u.array, (long) ival->u.number);
	    lval->type = T_ALVALUE;
	    lval->oindex = vtype;
	    arr_ref(lval->u.array = lval->u.lval->u.array);
	    f->lip->type = T_INT;
	    (f->lip++)->u.number = i;
	    break;

	case T_MAPPING:
	    lval->type = T_MLVALUE;
	    lval->oindex = vtype;
	    arr_ref(lval->u.array = lval->u.lval->u.array);
	    *f->lip++ = *ival;
	    break;

	default:
	    i_del_value(ival);
	    error("Index on bad type");
	}
	break;

    case T_ALVALUE:
	val = &d_get_elts(lval->u.array)[f->lip[-1].u.number];
	switch (val->type) {
	case T_STRING:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric string index");
	    }
	    i = str_index(val->u.string, (long) ival->u.number);
	    lval->type = T_SALVALUE;
	    lval->oindex = vtype;
	    f->lip->type = T_STRING;
	    f->lip->oindex = i;
	    str_ref((f->lip++)->u.string = val->u.string);
	    break;

	case T_ARRAY:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric array index");
	    }
	    i = arr_index(val->u.array, (long) ival->u.number);
	    arr_ref(val->u.array);	/* has to be first */
	    arr_del(lval->u.array);	/* has to be second */
	    lval->oindex = vtype;
	    lval->u.array = val->u.array;
	    f->lip[-1].u.number = i;
	    break;

	case T_MAPPING:
	    arr_ref(val->u.array);	/* has to be first */
	    arr_del(lval->u.array);	/* has to be second */
	    lval->type = T_MLVALUE;
	    lval->oindex = vtype;
	    lval->u.array = val->u.array;
	    f->lip[-1] = *ival;
	    break;

	default:
	    i_del_value(ival);
	    error("Index on bad type");
	}
	break;

    case T_MLVALUE:
	val = map_index(f->data, lval->u.array, &f->lip[-1], (value *) NULL);
	switch (val->type) {
	case T_STRING:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric string index");
	    }
	    i = str_index(val->u.string, (long) ival->u.number);
	    lval->type = T_SMLVALUE;
	    lval->oindex = vtype;
	    f->lip->type = T_STRING;
	    f->lip->oindex = i;
	    str_ref((f->lip++)->u.string = val->u.string);
	    break;

	case T_ARRAY:
	    if (ival->type != T_INT) {
		i_del_value(ival);
		error("Non-numeric array index");
	    }
	    i = arr_index(val->u.array, (long) ival->u.number);
	    arr_ref(val->u.array);	/* has to be first */
	    arr_del(lval->u.array);	/* has to be second */
	    lval->type = T_ALVALUE;
	    lval->oindex = vtype;
	    lval->u.array = val->u.array;
	    i_del_value(&f->lip[-1]);
	    f->lip[-1].type = T_INT;
	    f->lip[-1].u.number = i;
	    break;

	case T_MAPPING:
	    arr_ref(val->u.array);	/* has to be first */
	    arr_del(lval->u.array);	/* has to be second */
	    lval->oindex = vtype;
	    lval->u.array = val->u.array;
	    i_del_value(&f->lip[-1]);
	    f->lip[-1] = *ival;
	    break;

	default:
	    i_del_value(ival);
	    error("Index on bad type");
	}
	break;
    }

    if (vtype == T_CLASS) {
	f->lip->type = T_INT;
	(f->lip++)->u.number = class;
    }
}

/*
 * NAME:	interpret->typename()
 * DESCRIPTION:	return the name of the argument type
 */
char *i_typename(buf, type)
register char *buf;
register unsigned int type;
{
    static char *name[] = TYPENAMES;

    if ((type & T_TYPE) == T_CLASS) {
	type = (type & T_REF) | T_OBJECT;
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
 * NAME:	interpret->instanceof()
 * DESCRIPTION:	is an object an instance of the named program?
 */
bool i_instanceof(f, oindex, class)
register frame *f;
unsigned int oindex;
Uint class;
{
    register inhash *h;
    register char *prog;
    register unsigned short i;
    register dinherit *inh;
    object *obj;
    control *ctrl;

    /* first try hash table */
    obj = OBJR(oindex);
    ctrl = o_control(obj);
    prog = d_get_strconst(f->p_ctrl, class >> 16, class & 0xffff)->text;
    h = &ihash[(obj->count ^ (oindex << 2) ^ (f->p_ctrl->oindex << 4) ^ class) %
								    INHASHSZ];
    if (h->ocount == obj->count && h->coindex == f->p_ctrl->oindex &&
	h->class == class && h->iindex < ctrl->ninherits) {
	oindex = ctrl->inherits[h->iindex].oindex;
	if (strcmp(OBJR(oindex)->chain.name, prog) == 0) {
	    return TRUE;	/* found it */
	}
    }

    /* next, search for it the hard way */
    for (i = ctrl->ninherits, inh = ctrl->inherits + i; i != 0; ) {
	--i;
	--inh;
	i_add_ticks(f, 2);
	if (strcmp(prog, OBJR(inh->oindex)->chain.name) == 0) {
	    /* found it; update hashtable */
	    h->ocount = obj->count;
	    h->coindex = f->p_ctrl->oindex;
	    h->class = class;
	    h->iindex = i;
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 * NAME:	interpret->cast()
 * DESCRIPTION:	cast a value to a type
 */
void i_cast(f, val, type, class)
frame *f;
register value *val;
register unsigned int type;
Uint class;
{
    char tnbuf[8];

    if (type == T_CLASS) {
	if (val->type == T_OBJECT || val->type == T_LWOBJECT) {
	    if (!i_instanceof(f,
			      (val->type == T_OBJECT) ?
			       val->oindex : d_get_elts(val->u.array)->oindex,
			       class)) {
		error("Value is not of object type /%s",
		      d_get_strconst(f->p_ctrl, class >> 16,
				     class & 0xffff)->text);
	    }
	    return;
	}
	type = T_OBJECT;
    }
    if (val->type != type && (val->type != T_LWOBJECT || type != T_OBJECT) &&
	(!VAL_NIL(val) || !T_POINTER(type))) {
	i_typename(tnbuf, type);
	if (strchr("aeiuoy", tnbuf[0]) != (char *) NULL) {
	    error("Value is not an %s", tnbuf);
	} else {
	    error("Value is not a %s", tnbuf);
	}
    }
}

/*
 * NAME:	interpret->fetch()
 * DESCRIPTION:	fetch the value of an lvalue
 */
void i_fetch(f)
register frame *f;
{
    switch (f->sp->type) {
    case T_LVALUE:
	i_push_value(f, f->sp->u.lval);
	break;

    case T_ALVALUE:
	i_push_value(f, d_get_elts(f->sp->u.array) + f->lip[-1].u.number);
	break;

    case T_MLVALUE:
	i_push_value(f, map_index(f->data, f->sp->u.array, &f->lip[-1],
				  (value *) NULL));
	break;

    default:
	/*
         * Indexed string.
         */
	PUSH_INTVAL(f, UCHAR(f->lip[-1].u.string->text[f->lip[-1].oindex]));
	break;
    }
}

/*
 * NAME:	istr()
 * DESCRIPTION:	create a copy of the argument string, with one char replaced
 */
static value *istr(val, str, i, v)
register value *val, *v;
register string *str;
ssizet i;
{
    if (v->type != T_INT) {
	error("Non-numeric value in indexed string assignment");
    }

    PUT_STRVAL_NOREF(val, (str->primary == (strref *) NULL && str->ref == 1) ?
			   str : str_new(str->text, (long) str->len));
    val->u.string->text[i] = v->u.number;
    return val;
}

/*
 * NAME:	interpret->store()
 * DESCRIPTION:	Perform an assignment. This invalidates the lvalue.
 */
void i_store(f)
register frame *f;
{
    register value *lval, *val;
    register array *a;
    Uint class;
    value ival;

    lval = f->sp + 1;
    val = f->sp;
    if (lval->oindex != 0) {
	if (lval->oindex == T_CLASS) {
	    --f->lip;
	    class = f->lip->u.number;
	} else {
	    class = 0;
	}
	i_cast(f, val, lval->oindex, class);
    }

    i_add_ticks(f, 1);
    switch (lval->type) {
    case T_LVALUE:
	d_assign_var(f->data, lval->u.lval, val);
	break;

    case T_SLVALUE:
	d_assign_var(f->data, lval->u.lval,
		     istr(&ival, f->lip[-1].u.string, f->lip[-1].oindex, val));
	str_del((--f->lip)->u.string);
	break;

    case T_ALVALUE:
	a = lval->u.array;
	d_assign_elt(f->data, a, &d_get_elts(a)[(--f->lip)->u.number], val);
	arr_del(a);
	break;

    case T_MLVALUE:
	map_index(f->data, a = lval->u.array, &f->lip[-1], val);
	i_del_value(--f->lip);
	arr_del(a);
	break;

    case T_SALVALUE:
	a = lval->u.array;
	d_assign_elt(f->data, a, &a->elts[f->lip[-2].u.number],
		     istr(&ival, f->lip[-1].u.string, f->lip[-1].oindex, val));
	str_del((--f->lip)->u.string);
	--f->lip;
  	arr_del(a);
	break;

    case T_SMLVALUE:
	map_index(f->data, a = lval->u.array, &f->lip[-2],
		  istr(&ival, f->lip[-1].u.string, f->lip[-1].oindex, val));
	str_del((--f->lip)->u.string);
	i_del_value(--f->lip);
	arr_del(a);
	break;
    }
}

/*
 * NAME:	interpret->get_depth()
 * DESCRIPTION:	get the remaining stack depth (-1: infinite)
 */
Int i_get_depth(f)
frame *f;
{
    register rlinfo *rlim;

    rlim = f->rlim;
    if (rlim->nodepth) {
	return -1;
    }
    return rlim->maxdepth - f->depth;
}

/*
 * NAME:	interpret->get_ticks()
 * DESCRIPTION:	get the remaining ticks (-1: infinite)
 */
Int i_get_ticks(f)
frame *f;
{
    register rlinfo *rlim;

    rlim = f->rlim;
    if (rlim->noticks) {
	return -1;
    } else {
	return (rlim->ticks < 0) ? 0 : rlim->ticks << f->level;
    }
}

/*
 * NAME:	interpret->check_rlimits()
 * DESCRIPTION:	check if this rlimits call is valid
 */
static void i_check_rlimits(f)
register frame *f;
{
    object *obj;

    obj = OBJR(f->oindex);
    if (obj->count == 0) {
	error("Illegal use of rlimits");
    }
    --f->sp;
    f->sp[0] = f->sp[1];
    f->sp[1] = f->sp[2];
    if (f->lwobj == (array *) NULL) {
	PUT_OBJVAL(&f->sp[2], obj);
    } else {
	PUT_LWOVAL(&f->sp[2], f->lwobj);
    }

    /* obj, stack, ticks */
    call_driver_object(f, "runtime_rlimits", 3);

    if (!VAL_TRUE(f->sp)) {
	error("Illegal use of rlimits");
    }
    i_del_value(f->sp++);
}

/*
 * NAME:	interpret->new_rlimits()
 * DESCRIPTION:	create new rlimits scope
 */
void i_new_rlimits(f, depth, t)
register frame *f;
Int depth, t;
{
    register rlinfo *rlim;

    rlim = ALLOC(rlinfo, 1);
    if (depth != 0) {
	if (depth < 0) {
	    rlim->nodepth = TRUE;
	} else {
	    rlim->maxdepth = f->depth + depth;
	    rlim->nodepth = FALSE;
	}
    } else {
	rlim->maxdepth = f->rlim->maxdepth;
	rlim->nodepth = f->rlim->nodepth;
    }
    if (t != 0) {
	if (t < 0) {
	    rlim->noticks = TRUE;
	} else {
	    t >>= f->level;
	    f->rlim->ticks -= t;
	    rlim->ticks = t;
	    rlim->noticks = FALSE;
	}
    } else {
	f->rlim->ticks = 0;
	rlim->ticks = f->rlim->ticks;
	rlim->noticks = f->rlim->noticks;
    }

    rlim->next = f->rlim;
    f->rlim = rlim;
}

/*
 * NAME:	interpret->set_rlimits()
 * DESCRIPTION:	restore rlimits to an earlier state
 */
void i_set_rlimits(f, rlim)
frame *f;
register rlinfo *rlim;
{
    register rlinfo *r, *next;

    r = f->rlim;
    if (r->ticks < 0) {
	r->ticks = 0;
    }
    while (r != rlim) {
	next = r->next;
	if (!r->noticks) {
	    next->ticks += r->ticks;
	}
	FREE(r);
	r = next;
    }
    f->rlim = rlim;
}

/*
 * NAME:	interpret->set_sp()
 * DESCRIPTION:	set the current stack pointer
 */
frame *i_set_sp(ftop, sp)
frame *ftop;
register value *sp;
{
    register value *v, *w;
    register frame *f;

    for (f = ftop; ; f = f->prev) {
	v = f->sp;
	w = f->lip;
	for (;;) {
	    if (v == sp) {
		f->sp = v;
		f->lip = w;
		return f;
	    }
	    if (v == f->fp) {
		break;
	    }
	    switch (v->type) {
	    case T_STRING:
		str_del(v->u.string);
		break;

	    case T_LVALUE:
		if (v->oindex == T_CLASS) {
		    --w;
		}
		break;

	    case T_SLVALUE:
		if (v->oindex == T_CLASS) {
		    --w;
		}
		str_del((--w)->u.string);
		break;

	    case T_ALVALUE:
		if (v->oindex == T_CLASS) {
		    --w;
		}
		--w;
	    case T_ARRAY:
	    case T_MAPPING:
	    case T_LWOBJECT:
		arr_del(v->u.array);
		break;

	    case T_MLVALUE:
		if (v->oindex == T_CLASS) {
		    --w;
		}
		i_del_value(--w);
		arr_del(v->u.array);
		break;

	    case T_SALVALUE:
		if (v->oindex == T_CLASS) {
		    --w;
		}
		str_del((--w)->u.string);
		--w;
		arr_del(v->u.array);
		break;

	    case T_SMLVALUE:
		if (v->oindex == T_CLASS) {
		    --w;
		}
		str_del((--w)->u.string);
		i_del_value(--w);
		arr_del(v->u.array);
		break;
	    }
	    v++;
	}

	if (f->lwobj != (array *) NULL) {
	    arr_del(f->lwobj);
	}
	if (f->sos) {
	    /* stack on stack */
	    AFREE(f->stack);
	} else if (f->oindex != OBJ_NONE) {
	    FREE(f->stack);
	}
    }

    f->sp = v;
    f->lip = w;
    return f;
}

/*
 * NAME:	interpret->prev_object()
 * DESCRIPTION:	return the nth previous object in the call_other chain
 */
frame *i_prev_object(f, n)
register frame *f;
register int n;
{
    while (n >= 0) {
	/* back to last external call */
	while (!f->external) {
	    f = f->prev;
	}
	f = f->prev;
	if (f->oindex == OBJ_NONE) {
	    return (frame *) NULL;
	}
	--n;
    }
    return f;
}

/*
 * NAME:	interpret->prev_program()
 * DESCRIPTION:	return the nth previous program in the function call chain
 */
char *i_prev_program(f, n)
register frame *f;
register int n;
{
    while (n >= 0) {
	f = f->prev;
	if (f->oindex == OBJ_NONE) {
	    return (char *) NULL;
	}
	--n;
    }

    return OBJR(f->p_ctrl->oindex)->chain.name;
}

/*
 * NAME:	interpret->typecheck()
 * DESCRIPTION:	check the argument types given to a function
 */
void i_typecheck(f, prog_f, name, ftype, proto, nargs, strict)
register frame *f;
frame *prog_f;
char *name, *ftype;
register char *proto;
int nargs;
int strict;
{
    char tnbuf[8];
    register int i, n, atype, ptype;
    register char *args;
    bool ellipsis;
    Uint class;

    i = nargs;
    n = PROTO_NARGS(proto) + PROTO_VARGS(proto);
    ellipsis = (PROTO_CLASS(proto) & C_ELLIPSIS);
    args = PROTO_ARGS(proto);
    while (n > 0 && i > 0) {
	--i;
	ptype = *args++;
	if ((ptype & T_TYPE) == T_CLASS) {
	    FETCH3U(args, class);
	}
	if (n == 1 && ellipsis) {
	    if (ptype == T_MIXED || ptype == T_LVALUE) {
		return;
	    }
	    if ((ptype & T_TYPE) == T_CLASS) {
		args -= 4;
	    } else {
		--args;
	    }
	} else {
	    --n;
	}

	if (ptype != T_MIXED) {
	    atype = f->sp[i].type;
	    if (atype == T_LWOBJECT) {
		atype = T_OBJECT;
	    }
	    if ((ptype & T_TYPE) == T_CLASS && ptype == T_CLASS &&
		atype == T_OBJECT) {
		if (!i_instanceof(prog_f,
				  (f->sp[i].type == T_OBJECT) ?
				   f->sp[i].oindex :
				   d_get_elts(f->sp[i].u.array)->oindex,
				  class)) {
		    error("Bad object argument %d for function %s",
			  nargs - i, name);
		}
		continue;
	    }
	    if (ptype != atype && (atype != T_ARRAY || !(ptype & T_REF))) {
		if (!VAL_NIL(f->sp + i) || !T_POINTER(ptype)) {
		    /* wrong type */
		    error("Bad argument %d (%s) for %s %s", nargs - i,
			  i_typename(tnbuf, atype), ftype, name);
		} else if (strict) {
		    /* nil argument */
		    error("Bad argument %d for %s %s", nargs - i, ftype, name);
		}
	    }
	}
    }
}

/*
 * NAME:	interpret->switch_int()
 * DESCRIPTION:	handle an int switch
 */
static unsigned short i_switch_int(f, pc)
register frame *f;
register char *pc;
{
    register unsigned short h, l, m, sz, dflt;
    register Int num;
    register char *p;

    FETCH2U(pc, h);
    sz = FETCH1U(pc);
    FETCH2U(pc, dflt);
    if (f->sp->type != T_INT) {
	return dflt;
    }

    l = 0;
    --h;
    switch (sz) {
    case 1:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 3 * m;
	    num = FETCH1S(p);
	    if (f->sp->u.number == num) {
		return FETCH2U(p, l);
	    } else if (f->sp->u.number < num) {
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
	    if (f->sp->u.number == num) {
		return FETCH2U(p, l);
	    } else if (f->sp->u.number < num) {
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
	    if (f->sp->u.number == num) {
		return FETCH2U(p, l);
	    } else if (f->sp->u.number < num) {
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
	    if (f->sp->u.number == num) {
		return FETCH2U(p, l);
	    } else if (f->sp->u.number < num) {
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
static unsigned short i_switch_range(f, pc)
register frame *f;
register char *pc;
{
    register unsigned short h, l, m, sz, dflt;
    register Int num;
    register char *p;

    FETCH2U(pc, h);
    sz = FETCH1U(pc);
    FETCH2U(pc, dflt);
    if (f->sp->type != T_INT) {
	return dflt;
    }

    l = 0;
    --h;
    switch (sz) {
    case 1:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 4 * m;
	    num = FETCH1S(p);
	    if (f->sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		num = FETCH1S(p);
		if (f->sp->u.number <= num) {
		    return FETCH2U(p, l);
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
	    if (f->sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH2S(p, num);
		if (f->sp->u.number <= num) {
		    return FETCH2U(p, l);
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
	    if (f->sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH3S(p, num);
		if (f->sp->u.number <= num) {
		    return FETCH2U(p, l);
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
	    if (f->sp->u.number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH4S(p, num);
		if (f->sp->u.number <= num) {
		    return FETCH2U(p, l);
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
static unsigned short i_switch_str(f, pc)
register frame *f;
register char *pc;
{
    register unsigned short h, l, m, u, u2, dflt;
    register int cmp;
    register char *p;
    register control *ctrl;

    FETCH2U(pc, h);
    FETCH2U(pc, dflt);
    if (FETCH1U(pc) == 0) {
	FETCH2U(pc, l);
	if (VAL_NIL(f->sp)) {
	    return l;
	}
	--h;
    }
    if (f->sp->type != T_STRING) {
	return dflt;
    }

    ctrl = f->p_ctrl;
    l = 0;
    --h;
    while (l < h) {
	m = (l + h) >> 1;
	p = pc + 5 * m;
	u = FETCH1U(p);
	cmp = str_cmp(f->sp->u.string, d_get_strconst(ctrl, u, FETCH2U(p, u2)));
	if (cmp == 0) {
	    return FETCH2U(p, l);
	} else if (cmp < 0) {
	    h = m;	/* search in lower half */
	} else {
	    l = m + 1;	/* search in upper half */
	}
    }
    return dflt;
}

/*
 * NAME:	interpret->catcherr()
 * DESCRIPTION:	handle caught error
 */
void i_catcherr(f, depth)
frame *f;
Int depth;
{
    i_runtime_error(f, depth);
}

/*
 * NAME:	interpret->interpret()
 * DESCRIPTION:	Main interpreter function. Interpret stack machine code.
 */
static void i_interpret(f, pc)
register frame *f;
register char *pc;
{
    register unsigned short instr, u, u2;
    register Uint l;
    register char *p;
    register kfunc *kf;
    int size;
    Int newdepth, newticks;
    bool atomic;

    size = 0;

    for (;;) {
# ifdef DEBUG
	if (f->sp < f->lip + MIN_STACK) {
	    fatal("out of value stack");
	}
# endif
	if (--f->rlim->ticks <= 0) {
	    if (f->rlim->noticks) {
		f->rlim->ticks = 0x7fffffff;
	    } else {
		error("Out of ticks");
	    }
	}
	instr = FETCH1U(pc);
	f->pc = pc;

	switch (instr & I_INSTR_MASK) {
	case I_PUSH_ZERO:
	    PUSH_INTVAL(f, 0);
	    break;

	case I_PUSH_ONE:
	    PUSH_INTVAL(f, 1);
	    break;

	case I_PUSH_INT1:
	    PUSH_INTVAL(f, FETCH1S(pc));
	    break;

	case I_PUSH_INT4:
	    PUSH_INTVAL(f, FETCH4S(pc, l));
	    break;

	case I_PUSH_FLOAT:
	    FETCH2U(pc, u);
	    PUSH_FLTCONST(f, u, FETCH4U(pc, l));
	    break;

	case I_PUSH_STRING:
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, f->p_ctrl->ninherits - 1,
					  FETCH1U(pc)));
	    break;

	case I_PUSH_NEAR_STRING:
	    u = FETCH1U(pc);
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, u, FETCH1U(pc)));
	    break;

	case I_PUSH_FAR_STRING:
	    u = FETCH1U(pc);
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, u, FETCH2U(pc, u2)));
	    break;

	case I_PUSH_LOCAL:
	    u = FETCH1S(pc);
	    i_push_value(f, ((short) u < 0) ? f->fp + (short) u : f->argp + u);
	    break;

	case I_PUSH_GLOBAL:
	    i_global(f, 1, FETCH1U(pc));
	    break;

	case I_PUSH_FAR_GLOBAL:
	    u = FETCH1U(pc);
	    i_global(f, u, FETCH1U(pc));
	    break;

	case I_PUSH_LOCAL_LVAL:
	    u = FETCH1S(pc);
	    if (instr & I_TYPE_BIT) {
		instr = FETCH1U(pc);
		if (instr == T_CLASS) {
		    FETCH3U(pc, l);
		    f->lip->type = T_INT;
		    (f->lip++)->u.number = l;
		}
	    } else {
		instr = 0;
	    }
	    (--f->sp)->type = T_LVALUE;
	    f->sp->oindex = instr;
	    f->sp->u.lval = ((short) u < 0) ? f->fp + (short) u : f->argp + u;
	    continue;

	case I_PUSH_GLOBAL_LVAL:
	    u = FETCH1U(pc);
	    if (instr & I_TYPE_BIT) {
		instr = FETCH1U(pc);
		if (instr == T_CLASS) {
		    FETCH3U(pc, l);
		}
	    } else {
		instr = 0;
	    }
	    i_global_lvalue(f, 1, u, instr, l);
	    continue;

	case I_PUSH_FAR_GLOBAL_LVAL:
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    if (instr & I_TYPE_BIT) {
		instr = FETCH1U(pc);
		if (instr == T_CLASS) {
		    FETCH3U(pc, l);
		}
	    } else {
		instr = 0;
	    }
	    i_global_lvalue(f, u, u2, instr, l);
	    continue;

	case I_INDEX:
	    i_index(f);
	    break;

	case I_INDEX_LVAL:
	    if (instr & I_TYPE_BIT) {
		instr = FETCH1U(pc);
		if (instr == T_CLASS) {
		    FETCH3U(pc, l);
		}
	    } else {
		instr = 0;
	    }
	    i_index_lvalue(f, instr, l);
	    continue;

	case I_AGGREGATE:
	    if (FETCH1U(pc) == 0) {
		i_aggregate(f, FETCH2U(pc, u));
	    } else {
		i_map_aggregate(f, FETCH2U(pc, u));
	    }
	    break;

	case I_SPREAD:
	    u = FETCH1S(pc);
	    if (instr & I_TYPE_BIT) {
		instr = FETCH1U(pc);
		if (instr == T_CLASS) {
		    FETCH3U(pc, l);
		}
	    } else {
		instr = 0;
	    }
	    size = i_spread(f, (short) u, instr, l);
	    continue;

	case I_CAST:
	    u = FETCH1U(pc);
	    if (u == T_CLASS) {
		FETCH3U(pc, l);
	    }
	    i_cast(f, f->sp, u, l);
	    break;

	case I_FETCH:
	    i_fetch(f);
	    break;

	case I_STORE:
	    i_store(f);
	    f->sp[1] = f->sp[0];
	    f->sp++;
	    break;

	case I_JUMP:
	    p = f->prog + FETCH2U(pc, u);
	    pc = p;
	    break;

	case I_JUMP_ZERO:
	    p = f->prog + FETCH2U(pc, u);
	    if (!VAL_TRUE(f->sp)) {
		pc = p;
	    }
	    break;

	case I_JUMP_NONZERO:
	    p = f->prog + FETCH2U(pc, u);
	    if (VAL_TRUE(f->sp)) {
		pc = p;
	    }
	    break;

	case I_SWITCH:
	    switch (FETCH1U(pc)) {
	    case SWITCH_INT:
		pc = f->prog + i_switch_int(f, pc);
		break;

	    case SWITCH_RANGE:
		pc = f->prog + i_switch_range(f, pc);
		break;

	    case SWITCH_STRING:
		pc = f->prog + i_switch_str(f, pc);
		break;
	    }
	    break;

	case I_CALL_KFUNC:
	    kf = &KFUN(FETCH1U(pc));
	    if (PROTO_VARGS(kf->proto) != 0) {
		/* variable # of arguments */
		u = FETCH1U(pc) + size;
		size = 0;
	    } else {
		/* fixed # of arguments */
		u = PROTO_NARGS(kf->proto);
	    }
	    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
		i_typecheck(f, (frame *) NULL, kf->name, "kfun", kf->proto, u,
			    TRUE);
	    }
	    u = (*kf->func)(f, u, kf);
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
	    i_funcall(f, (object *) NULL, (array *) NULL, 0, u,
		      FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CALL_DFUNC:
	    u = f->p_index - FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    i_funcall(f, (object *) NULL, (array *) NULL, u, u2,
		      FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CALL_FUNC:
	    p = &f->ctrl->funcalls[2L * (f->foffset + FETCH2U(pc, u))];
	    i_funcall(f, (object *) NULL, (array *) NULL, UCHAR(p[0]),
		      UCHAR(p[1]), FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CATCH:
	    p = f->prog + FETCH2U(pc, u);
	    atomic = f->atomic;
	    f->atomic = FALSE;
	    if (!ec_push((ec_ftn) i_catcherr)) {
		i_interpret(f, pc);
		ec_pop();
		pc = f->pc;
		*--f->sp = nil_value;
	    } else {
		/* error */
		f->pc = pc = p;
		PUSH_STRVAL(f, errorstr());
	    }
	    f->atomic = atomic;
	    break;

	case I_RLIMITS:
	    if (f->sp[1].type != T_INT) {
		error("Bad rlimits depth type");
	    }
	    if (f->sp->type != T_INT) {
		error("Bad rlimits ticks type");
	    }
	    newdepth = f->sp[1].u.number;
	    newticks = f->sp->u.number;
	    if (!FETCH1U(pc)) {
		/* runtime check */
		i_check_rlimits(f);
	    } else {
		/* pop limits */
		f->sp += 2;
	    }

	    i_new_rlimits(f, newdepth, newticks);
	    i_interpret(f, pc);
	    pc = f->pc;
	    i_set_rlimits(f, f->rlim->next);
	    break;

	case I_RETURN:
	    return;
	}

	if (instr & I_POP_BIT) {
	    /* pop the result of the last operation (never an lvalue) */
	    i_del_value(f->sp++);
	}
    }
}

/*
 * NAME:	interpret->funcall()
 * DESCRIPTION:	Call a function in an object. The arguments must be on the
 *		stack already.
 */
void i_funcall(prev_f, obj, lwobj, p_ctrli, funci, nargs)
register frame *prev_f;
register object *obj;
array *lwobj;
register int p_ctrli, nargs;
int funci;
{
    register char *pc;
    register unsigned short n;
    frame f;
    bool ellipsis;
    value val;

    f.prev = prev_f;
    if (prev_f->oindex == OBJ_NONE) {
	/*
	 * top level call
	 */
	f.oindex = obj->index;
	f.lwobj = (array *) NULL;
	f.ctrl = obj->ctrl;
	f.data = o_dataspace(obj);
	f.external = TRUE;
    } else if (lwobj != (array *) NULL) {
	/*
	 * call_other to lightweight object
	 */
	f.oindex = obj->index;
	f.lwobj = lwobj;
	f.ctrl = obj->ctrl;
	f.data = lwobj->primary->data;
	f.external = TRUE;
    } else if (obj != (object *) NULL) {
	/*
	 * call_other to persistent object
	 */
	f.oindex = obj->index;
	f.lwobj = (array *) NULL;
	f.ctrl = obj->ctrl;
	f.data = o_dataspace(obj);
	f.external = TRUE;
    } else {
	/*
	 * local function call
	 */
	f.oindex = prev_f->oindex;
	f.lwobj = prev_f->lwobj;
	f.ctrl = prev_f->ctrl;
	f.data = prev_f->data;
	f.external = FALSE;
    }
    f.depth = prev_f->depth + 1;
    f.rlim = prev_f->rlim;
    if (f.depth >= f.rlim->maxdepth && !f.rlim->nodepth) {
	error("Stack overflow");
    }
    if (f.rlim->ticks < 100) {
	if (f.rlim->noticks) {
	    f.rlim->ticks = 0x7fffffff;
	} else {
	    error("Out of ticks");
	}
    }

    /* set the program control block */
    obj = OBJR(f.ctrl->inherits[p_ctrli].oindex);
    f.foffset = f.ctrl->inherits[p_ctrli].funcoffset;
    f.p_ctrl = o_control(obj);
    f.p_index = p_ctrli + 1;

    /* get the function */
    f.func = &d_get_funcdefs(f.p_ctrl)[funci];
    if (f.func->class & C_UNDEFINED) {
	error("Undefined function %s",
	      d_get_strconst(f.p_ctrl, f.func->inherit, f.func->index)->text);
    }

    pc = d_get_prog(f.p_ctrl) + f.func->offset;
    if (f.func->class & C_TYPECHECKED) {
	/* typecheck arguments */
	i_typecheck(prev_f, &f,
		    d_get_strconst(f.p_ctrl, f.func->inherit,
				   f.func->index)->text,
		    "function", pc, nargs, FALSE);
    }

    /* handle arguments */
    ellipsis = (PROTO_CLASS(pc) & C_ELLIPSIS);
    n = PROTO_NARGS(pc) + PROTO_VARGS(pc);
    if (nargs < n) {
	register int i;

	/* if fewer actual than formal parameters, check for varargs */
	if (nargs < PROTO_NARGS(pc) && stricttc) {
	    error("Insufficient arguments for function %s",
		  d_get_strconst(f.p_ctrl, f.func->inherit,
				 f.func->index)->text);
	}

	/* add missing arguments */
	i_grow_stack(prev_f, n - nargs);
	if (ellipsis) {
	    --n;
	}

	pc = &PROTO_FTYPE(pc);
	i = nargs;
	do {
	    if ((FETCH1U(pc) & T_TYPE) == T_CLASS) {
		pc += 3;
	    }
	} while (--i >= 0);
	while (nargs < n) {
	    switch (i=FETCH1U(pc)) {
	    case T_INT:
		*--prev_f->sp = zero_int;
		break;

	    case T_FLOAT:
		*--prev_f->sp = zero_float;
		    break;

	    default:
		if ((i & T_TYPE) == T_CLASS) {
		    pc += 3;
		}
		*--prev_f->sp = nil_value;
		break;
	    }
	    nargs++;
	}
	if (ellipsis) {
	    PUSH_ARRVAL(prev_f, arr_new(f.data, 0));
	    nargs++;
	    if ((FETCH1U(pc) & T_TYPE) == T_CLASS) {
		pc += 3;
	    }
	}
    } else if (ellipsis) {
	register value *v;
	array *a;

	/* put additional arguments in array */
	nargs -= n - 1;
	a = arr_new(f.data, nargs);
	v = a->elts + nargs;
	do {
	    *--v = *prev_f->sp++;
	} while (--nargs > 0);
	d_ref_imports(a);
	PUSH_ARRVAL(prev_f, a);
	nargs = n;
	pc += PROTO_SIZE(pc);
    } else if (nargs > n) {
	if (stricttc) {
	    error("Too many arguments for function %s",
		  d_get_strconst(f.p_ctrl, f.func->inherit,
				 f.func->index)->text);
	}

	/* pop superfluous arguments */
	i_pop(prev_f, nargs - n);
	nargs = n;
	pc += PROTO_SIZE(pc);
    } else {
	pc += PROTO_SIZE(pc);
    }
    f.sp = prev_f->sp;
    f.nargs = nargs;
    cframe = &f;
    if (f.lwobj != (array *) NULL) {
	arr_ref(f.lwobj);
    }

    /* deal with atomic functions */
    f.level = prev_f->level;
    if ((f.func->class & C_ATOMIC) && !prev_f->atomic) {
	o_new_plane();
	d_new_plane(f.data, ++f.level);
	f.atomic = TRUE;
	if (!f.rlim->noticks) {
	    f.rlim->ticks >>= 1;
	}
    } else {
	if (f.level != f.data->plane->level) {
	    d_new_plane(f.data, f.level);
	}
	f.atomic = prev_f->atomic;
    }

    i_add_ticks(&f, 10);

    if ((obj->flags & O_SPECIAL) != O_SPECIAL ||
	ext_funcall == (bool (*) P((frame*, int, value*, char*))) NULL ||
	!(*ext_funcall)(&f, nargs, &val,
		        d_get_strconst(f.p_ctrl, f.func->inherit,
				       f.func->index)->text)) {
	/*
	 * ordinary function call
	 */

	/* create new local stack */
	f.argp = f.sp;
	FETCH2U(pc, n);
	f.stack = f.lip = ALLOCA(value, n + MIN_STACK + EXTRA_STACK);
	f.fp = f.sp = f.stack + n + MIN_STACK + EXTRA_STACK;
	f.sos = TRUE;

	/* initialize local variables */
	n = FETCH1U(pc);
# ifdef DEBUG
	nargs = n;
# endif
	if (n > 0) {
	    do {
		*--f.sp = nil_value;
	    } while (--n > 0);
	}

	/* execute code */
	d_get_funcalls(f.ctrl);	/* make sure they are available */
	if (f.func->class & C_COMPILED) {
	    Uint l;

	    /* compiled function */
	    (*pcfunctions[FETCH3U(pc, l)])(&f);
	} else {
	    /* interpreted function */
	    f.prog = pc += 2;
	    i_interpret(&f, pc);
	}

	/* clean up stack, move return value to outer stackframe */
	val = *f.sp++;
# ifdef DEBUG
	if (f.sp != f.fp - nargs || f.lip != f.stack) {
	    fatal("bad stack pointer after function call");
	}
# endif
	i_pop(&f, f.fp - f.sp);
	if (f.sos) {
		/* still alloca'd */
	    AFREE(f.stack);
	} else {
	    /* extended and malloced */
	    FREE(f.stack);
	}
    }

    if (f.lwobj != (array *) NULL) {
	arr_del(f.lwobj);
    }
    cframe = prev_f;
    i_pop(prev_f, f.nargs);
    *--prev_f->sp = val;

    if ((f.func->class & C_ATOMIC) && !prev_f->atomic) {
	d_commit_plane(f.level, &val);
	o_commit_plane();
	if (!f.rlim->noticks) {
	    f.rlim->ticks *= 2;
	}
    }
}

/*
 * NAME:	interpret->call()
 * DESCRIPTION:	Attempt to call a function in an object. Return TRUE if
 *		the call succeeded.
 */
bool i_call(f, obj, lwobj, func, len, call_static, nargs)
frame *f;
object *obj;
array *lwobj;
char *func;
unsigned int len;
int call_static;
int nargs;
{
    register dsymbol *symb;
    register dfuncdef *fdef;
    register control *ctrl;

    if (lwobj != (array *) NULL) {
	uindex oindex;
	xfloat flt;
	value val;

	oindex = lwobj->elts[0].oindex;
	obj = OBJR(oindex);
	GET_FLT(&lwobj->elts[1], flt);
	if (obj->update != flt.low) {
	    d_upgrade_lwobj(lwobj, obj);
	}
	if (flt.high != FALSE) {
	    /*
	     * touch the light-weight object
	     */
	    flt.high = FALSE;
	    PUT_FLTVAL(&val, flt);
	    d_assign_elt(f->data, lwobj, &lwobj->elts[1], &val);
	    PUSH_LWOVAL(f, lwobj);
	    PUSH_STRVAL(f, str_new(func, len));
	    call_driver_object(f, "touch", 2);
	    if (VAL_TRUE(f->sp)) {
		/* preserve through call */
		flt.high = TRUE;
		PUT_FLT(&lwobj->elts[1], flt);
	    }
	    i_del_value(f->sp++);
	}
    } else if (!(obj->flags & O_TOUCHED)) {
	/*
	 * initialize/touch the object
	 */
	obj = OBJW(obj->index);
	obj->flags |= O_TOUCHED;
	if (O_HASDATA(obj)) {
	    PUSH_OBJVAL(f, obj);
	    PUSH_STRVAL(f, str_new(func, len));
	    call_driver_object(f, "touch", 2);
	    if (VAL_TRUE(f->sp)) {
		obj->flags &= ~O_TOUCHED;	/* preserve though call */
	    }
	    i_del_value(f->sp++);
	} else {
	    obj->data = d_new_dataspace(obj);
	    if (func != (char *) NULL &&
		i_call(f, obj, (array *) NULL, creator, clen, TRUE, 0)) {
		i_del_value(f->sp++);
	    }
	}
    }
    if (func == (char *) NULL) {
	func = creator;
	len = clen;
    }

    /* find the function in the symbol table */
    ctrl = o_control(obj);
    symb = ctrl_symb(ctrl, func, len);
    if (symb == (dsymbol *) NULL) {
	/* function doesn't exist in symbol table */
	i_pop(f, nargs);
	return FALSE;
    }

    ctrl = OBJR(ctrl->inherits[UCHAR(symb->inherit)].oindex)->ctrl;
    fdef = &d_get_funcdefs(ctrl)[UCHAR(symb->index)];

    /* check if the function can be called */
    if (!call_static && (fdef->class & C_STATIC) &&
	((lwobj != (array *) NULL) ?
	 lwobj != f->lwobj : f->oindex != obj->index)) {
	i_pop(f, nargs);
	return FALSE;
    }

    /* call the function */
    i_funcall(f, obj, lwobj, UCHAR(symb->inherit), UCHAR(symb->index), nargs);

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
	case I_INDEX_LVAL:
	    if ((instr & I_TYPE_BIT) && FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    /* fall through */
	case I_PUSH_ZERO:
	case I_PUSH_ONE:
	case I_INDEX:
	case I_FETCH:
	case I_STORE:
	case I_RETURN:
	    break;

	case I_PUSH_INT1:
	case I_PUSH_STRING:
	case I_PUSH_LOCAL:
	case I_PUSH_GLOBAL:
	case I_RLIMITS:
	    pc++;
	    break;

	case I_CAST:
	    if (FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    break;

	case I_PUSH_LOCAL_LVAL:
	case I_PUSH_GLOBAL_LVAL:
	case I_SPREAD:
	    pc++;
	    if ((instr & I_TYPE_BIT) && FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    break;

	case I_PUSH_NEAR_STRING:
	case I_PUSH_FAR_GLOBAL:
	case I_JUMP:
	case I_JUMP_ZERO:
	case I_JUMP_NONZERO:
	case I_CALL_AFUNC:
	case I_CATCH:
	    pc += 2;
	    break;

	case I_PUSH_FAR_GLOBAL_LVAL:
	    pc += 2;
	    if ((instr & I_TYPE_BIT) && FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    break;

	case I_PUSH_FAR_STRING:
	case I_AGGREGATE:
	case I_CALL_DFUNC:
	case I_CALL_FUNC:
	    pc += 3;
	    break;

	case I_PUSH_INT4:
	    pc += 4;
	    break;

	case I_PUSH_FLOAT:
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
	    if (PROTO_VARGS(KFUN(FETCH1U(pc)).proto) != 0) {
		pc++;
	    }
	    break;
	}
    }

    return line;
}

/*
 * NAME:	interpret->func_trace()
 * DESCRIPTION:	return the trace of a single function
 */
static array *i_func_trace(f, data)
register frame *f;
dataspace *data;
{
    char buffer[STRINGSZ + 12];
    register value *v;
    register string *str;
    register char *name;
    register unsigned short n;
    register value *args;
    array *a;
    unsigned short max_args;

    max_args = conf_array_size() - 5;

    n = f->nargs;
    args = f->argp + n;
    if (n > max_args) {
	/* unlikely, but possible */
	n = max_args;
    }
    a = arr_new(data, n + 5L);
    v = a->elts;

    /* object name */
    name = o_name(buffer, OBJR(f->oindex));
    if (f->lwobj == (array *) NULL) {
	PUT_STRVAL(v, str = str_new((char *) NULL, strlen(name) + 1L));
	v++;
	str->text[0] = '/';
	strcpy(str->text + 1, name);
    } else {
	PUT_STRVAL(v, str = str_new((char *) NULL, strlen(name) + 4L));
	v++;
	str->text[0] = '/';
	strcpy(str->text + 1, name);
	strcpy(str->text + str->len - 3, "#-1");
    }

    /* program name */
    name = OBJR(f->p_ctrl->oindex)->chain.name;
    PUT_STRVAL(v, str = str_new((char *) NULL, strlen(name) + 1L));
    v++;
    str->text[0] = '/';
    strcpy(str->text + 1, name);

    /* function name */
    PUT_STRVAL(v, d_get_strconst(f->p_ctrl, f->func->inherit, f->func->index));
    v++;

    /* line number */
    PUT_INTVAL(v, (f->func->class & C_COMPILED) ? 0 : i_line(f));
    v++;

    /* external flag */
    PUT_INTVAL(v, f->external);
    v++;

    /* arguments */
    while (n > 0) {
	*v++ = *--args;
	i_ref_value(args);
	--n;
    }
    d_ref_imports(a);

    return a;
}

/*
 * NAME:	interpret->call_tracei()
 * DESCRIPTION:	get the trace of a single function
 */
bool i_call_tracei(ftop, idx, v)
frame *ftop;
Int idx;
value *v;
{
    register frame *f;
    register unsigned short n;

    for (f = ftop, n = 0; f->oindex != OBJ_NONE; f = f->prev, n++) ;
    if (idx < 0 || idx >= n) {
	return FALSE;
    }

    for (f = ftop, n -= idx + 1; n != 0; f = f->prev, --n) ;
    PUT_ARRVAL(v, i_func_trace(f, ftop->data));
    return TRUE;
}

/*
 * NAME:	interpret->call_trace()
 * DESCRIPTION:	return the function call trace
 */
array *i_call_trace(ftop)
register frame *ftop;
{
    register frame *f;
    register value *v;
    register unsigned short n;
    array *a;

    for (f = ftop, n = 0; f->oindex != OBJ_NONE; f = f->prev, n++) ;
    a = arr_new(ftop->data, (long) n);
    i_add_ticks(ftop, 10 * n);
    for (f = ftop, v = a->elts + n; f->oindex != OBJ_NONE; f = f->prev) {
	--v;
	PUT_ARRVAL(v, i_func_trace(f, ftop->data));
    }

    return a;
}

/*
 * NAME:	emptyhandler()
 * DESCRIPTION:	fake error handler
 */
static void emptyhandler(f, depth)
frame *f;
Int depth;
{
}

/*
 * NAME:	interpret->call_critical()
 * DESCRIPTION:	Call a function in the driver object at a critical moment.
 *		The function is called with rlimits (-1; -1) and errors
 *		caught.
 */
bool i_call_critical(f, func, narg, flag)
register frame *f;
char *func;
int narg, flag;
{
    bool ok;

    i_new_rlimits(f, -1, -1);
    f->sp += narg;		/* so the error context knows what to pop */
    if (ec_push((flag) ? (ec_ftn) NULL : (ec_ftn) emptyhandler)) {
	ok = FALSE;
    } else {
	f->sp -= narg;	/* recover arguments */
	call_driver_object(f, func, narg);
	ec_pop();
	ok = TRUE;
    }
    i_set_rlimits(f, f->rlim->next);

    return ok;
}

/*
 * NAME:	interpret->runtime_error()
 * DESCRIPTION:	handle a runtime error
 */
void i_runtime_error(f, depth)
register frame *f;
Int depth;
{
    PUSH_STRVAL(f, errorstr());
    PUSH_INTVAL(f, depth);
    PUSH_INTVAL(f, i_get_ticks(f));
    if (!i_call_critical(f, "runtime_error", 3, FALSE)) {
	message("Error within runtime_error:\012");	/* LF */
	message((char *) NULL);
    } else {
	i_del_value(f->sp++);
    }
}

/*
 * NAME:	interpret->atomic_error()
 * DESCRIPTION:	handle error in atomic code
 */
void i_atomic_error(ftop, level)
register frame *ftop;
Int level;
{
    register frame *f;

    for (f = ftop; f->level != level; f = f->prev) ;

    PUSH_STRVAL(ftop, errorstr());
    PUSH_INTVAL(ftop, f->depth);
    PUSH_INTVAL(ftop, i_get_ticks(ftop));
    if (!i_call_critical(ftop, "atomic_error", 3, FALSE)) {
	message("Error within atomic_error:\012");	/* LF */
	message((char *) NULL);
    } else {
	i_del_value(ftop->sp++);
    }
}

/*
 * NAME:	interpret->restore()
 * DESCRIPTION:	restore state to given level
 */
frame *i_restore(ftop, level)
register frame *ftop;
Int level;
{
    register frame *f;

    for (f = ftop; f->level != level; f = f->prev) ;

    if (f->rlim != ftop->rlim) {
	i_set_rlimits(ftop, f->rlim);
    }
    if (!f->rlim->noticks) {
	f->rlim->ticks *= 2;
    }
    i_set_sp(ftop, f->sp);
    d_discard_plane(ftop->level);
    o_discard_plane();

    return f;
}

/*
 * NAME:	interpret->clear()
 * DESCRIPTION:	clean up the interpreter state
 */
void i_clear()
{
    register frame *f;

    f = cframe;
    if (f->stack != stack) {
	FREE(f->stack);
	f->fp = f->sp = stack + MIN_STACK;
	f->stack = f->lip = stack;
    }

    f->rlim = &rlim;
}
