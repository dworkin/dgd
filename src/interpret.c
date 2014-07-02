/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2014 DGD Authors (see the commit log for details)
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
# define EXTRA_STACK  0
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
void i_init(char *create, int flag)
{
    topframe.oindex = OBJ_NONE;
    topframe.fp = topframe.sp = stack + MIN_STACK;
    topframe.stack = topframe.lip = stack;
    rlim.maxdepth = 0;
    rlim.ticks = 0;
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
void i_ref_value(value *v)
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
void i_del_value(value *v)
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
void i_copy(value *v, value *w, unsigned int len)
{
    value *o;

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
	    if (o->type == T_OBJECT && DESTRUCTED(o)) {
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
void i_grow_stack(frame *f, int size)
{
    if (f->sp < f->lip + size + MIN_STACK) {
	int spsize, lisize;
	value *v, *stk;
	intptr_t offset;

	/*
	 * extend the local stack
	 */
	spsize = f->fp - f->sp;
	lisize = f->lip - f->stack;
	size = ALGN(spsize + lisize + size + MIN_STACK, 8);
	stk = ALLOC(value, size);
	offset = (intptr_t) (stk + size) - (intptr_t) f->fp;

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
		    v->u.lval = (value *) ((intptr_t) v->u.lval + offset);
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
void i_push_value(frame *f, value *v)
{
    value *o;

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
	if (o->type == T_OBJECT && DESTRUCTED(o)) {
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
void i_pop(frame *f, int n)
{
    value *v;

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
value *i_reverse(frame *f, int n)
{
    if (f->p_ctrl->flags & CTRL_OLDVM) {
	value sp[MAX_LOCALS];
	value lip[3 * MAX_LOCALS];
	value *v1, *v2, *w1, *w2;
	value *top;

	top = f->sp + n;
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
	return top;
    } else {
	value stack[MAX_LOCALS * 6];
	value *v, *w;
	int size;

	size = 0;
	v = f->sp;
	if (n == 1) {
	    switch (v->u.number >> 28) {
	    case LVAL_LOCAL:
	    case LVAL_GLOBAL:
		size = 1;
		break;

	    case LVAL_INDEX:
	    case LVAL_LOCAL_INDEX:
	    case LVAL_GLOBAL_INDEX:
		size = 3;
		break;

	    case LVAL_INDEX_INDEX:
		size = 5;
		break;
	    }
	    size += (((v->u.number >> 24) & 0xf) == T_CLASS);
	    v += size;
	} else if (n > 1) {
	    w = stack + sizeof(stack) / sizeof(value);
	    do {
		switch (v->u.number >> 28) {
		case LVAL_LOCAL:
		case LVAL_GLOBAL:
		    size = 1;
		    break;

		case LVAL_INDEX:
		case LVAL_LOCAL_INDEX:
		case LVAL_GLOBAL_INDEX:
		    size = 3;
		    break;

		case LVAL_INDEX_INDEX:
		    size = 5;
		    break;
		}
		size += (((v->u.number >> 24) & 0xf) == T_CLASS);

		w -= size;
		memcpy(w, v, size * sizeof(value));
		v += size;
	    } while (--n != 0);

	    memcpy(f->sp, w, (v - f->sp) * sizeof(value));
	}

	return v;
    }
}

/*
 * NAME:	interpret->odest()
 * DESCRIPTION:	replace all occurrences of an object on the stack by nil
 */
void i_odest(frame *prev, object *obj)
{
    frame *f;
    Uint count;
    value *v;
    unsigned short n;

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
		if (v->u.array->elts[0].type == T_OBJECT &&
		    v->u.array->elts[0].u.objcnt == count) {
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
		if (v->u.array->elts[0].type == T_OBJECT &&
		    v->u.array->elts[0].u.objcnt == count) {
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
		    if (v->u.array->elts[0].type == T_OBJECT &&
			v->u.array->elts[0].u.objcnt == count) {
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
void i_string(frame *f, int inherit, unsigned int index)
{
    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, inherit, index));
}

/*
 * NAME:	interpret->aggregate()
 * DESCRIPTION:	create an array on the stack
 */
void i_aggregate(frame *f, unsigned int size)
{
    array *a;

    if (size == 0) {
	a = arr_new(f->data, 0L);
    } else {
	value *v, *elts;

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
void i_map_aggregate(frame *f, unsigned int size)
{
    array *a;

    if (size == 0) {
	a = map_new(f->data, 0L);
    } else {
	value *v, *elts;

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
int i_spread(frame *f, int n, int vtype, Uint class)
{
    array *a;
    int i;
    value *v;

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
	i = a->size - n;
	a->ref += i;
	i_grow_stack(f, n + i * (3 + (vtype == T_CLASS)));
    }
    f->sp++;

    /* values */
    for (i = 0, v = d_get_elts(a); i < n; i++, v++) {
	i_push_value(f, v);
    }
    /* lvalues */
    for (n = a->size; i < n; i++) {
	if (f->p_ctrl->flags & CTRL_OLDVM) {
	    (--f->sp)->type = T_ALVALUE;
	    f->sp->oindex = vtype;
	    f->sp->u.array = a;
	    f->lip->type = T_INT;
	    (f->lip++)->u.number = i;
	    if (vtype == T_CLASS) {
		f->lip->type = T_INT;
		(f->lip++)->u.number = class;
	    }
	} else {
	    --f->sp;
	    PUT_ARRVAL_NOREF(f->sp, a);
	    PUSH_INTVAL(f, i);
	    if (vtype == T_CLASS) {
		PUSH_INTVAL(f, class);
	    }
	    PUSH_INTVAL(f, (LVAL_INDEX << 28) | (vtype << 24));
	}
    }

    arr_del(a);
    return n - 1;
}

/*
 * NAME:	interpret->global()
 * DESCRIPTION:	push a global value on the stack
 */
void i_global(frame *f, int inherit, int index)
{
    i_add_ticks(f, 4);
    inherit = UCHAR(f->ctrl->imap[f->p_index + inherit]);
    inherit = f->ctrl->inherits[inherit].varoffset;
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
void i_global_lvalue(frame *f, int inherit, int index, int vtype, Uint class)
{
    i_add_ticks(f, 4);
    inherit = UCHAR(f->ctrl->imap[f->p_index + inherit]);
    inherit = f->ctrl->inherits[inherit].varoffset;
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
 * NAME:	interpret->operator()
 * DESCRIPTION:	index or indexed assignment
 */
static void i_operator(frame *f, array *lwobj, char *op, int nargs, value *var,
		       value *idx, value *val)
{
    i_push_value(f, idx);
    if (nargs > 1) {
	i_push_value(f, val);
    }
    if (!i_call(f, (object *) NULL, lwobj, op, strlen(op), TRUE, nargs)) {
	error("Index on bad type");
    }

    *var = *f->sp++;
}

/*
 * NAME:	interpret->index()
 * DESCRIPTION:	index a value, REPLACING it with the indexed value
 */
void i_index(frame *f)
{
    int i;
    value *aval, *ival, *val;
    array *a;

    val = NULL;
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
	val = map_index(f->data, aval->u.array, ival, (value *) NULL,
			(value *) NULL);
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
	if (ival->type == T_OBJECT && DESTRUCTED(ival)) {
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
 * NAME:	interpret->index2()
 * DESCRIPTION:	index a value
 */
void i_index2(frame *f, value *aval, value *ival, value *val, bool keep)
{
    int i;

    i_add_ticks(f, 2);
    switch (aval->type) {
    case T_STRING:
	if (ival->type != T_INT) {
	    i_del_value(ival);
	    error("Non-numeric string index");
	}
	i = UCHAR(aval->u.string->text[str_index(aval->u.string,
						 ival->u.number)]);
	if (!keep) {
	    str_del(aval->u.string);
	}
	PUT_INTVAL(val, i);
	return;

    case T_ARRAY:
	if (ival->type != T_INT) {
	    i_del_value(ival);
	    error("Non-numeric array index");
	}
	*val = d_get_elts(aval->u.array)[arr_index(aval->u.array,
						   ival->u.number)];
	break;

    case T_MAPPING:
	*val = *map_index(f->data, aval->u.array, ival, NULL, NULL);
	if (!keep) {
	    i_del_value(ival);
	}
	break;

    case T_LWOBJECT:
	i_operator(f, aval->u.array, "[]", 1, val, ival, (value *) NULL);
	if (!keep) {
	    i_del_value(ival);
	}
	break;

    default:
	error("Index on bad type");
    }

    switch (val->type) {
    case T_STRING:
	str_ref(val->u.string);
	break;

    case T_OBJECT:
	if (DESTRUCTED(val)) {
	    *val = nil_value;
	}
	break;

    case T_LWOBJECT:
	ival = d_get_elts(val->u.array);
	if (ival->type == T_OBJECT && DESTRUCTED(ival)) {
	    *val = nil_value;
	    break;
	}
	/* fall through */
    case T_ARRAY:
    case T_MAPPING:
	arr_ref(val->u.array);
	break;
    }

    if (!keep) {
	arr_del(aval->u.array);
    }
}

/*
 * NAME:	interpret->index_lvalue()
 * DESCRIPTION:	Index a value, REPLACING it by an indexed lvalue.
 */
void i_index_lvalue(frame *f, int vtype, Uint class)
{
    int i;
    value *lval, *ival, *val;

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
	val = map_index(f->data, lval->u.array, &f->lip[-1], (value *) NULL,
			(value *) NULL);
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
char *i_typename(char *buf, unsigned int type)
{
    static char *name[] = TYPENAMES;

    if ((type & T_TYPE) == T_CLASS) {
	type = (type & T_REF) | T_OBJECT;
    }
    strcpy(buf, name[type & T_TYPE]);
    type &= T_REF;
    type >>= REFSHIFT;
    if (type > 0) {
	char *p;

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
 * NAME:	interpret->classname()
 * DESCRIPTION:	return the name of a class
 */
char *i_classname(frame *f, Uint class)
{
    return d_get_strconst(f->p_ctrl, class >> 16, class & 0xffff)->text;
}

/*
 * NAME:	interpret->instanceof()
 * DESCRIPTION:	is an object an instance of the named program?
 */
int i_instanceof(frame *f, unsigned int oindex, Uint class)
{
    inhash *h;
    char *prog;
    unsigned short i;
    dinherit *inh;
    object *obj;
    control *ctrl;

    /* first try hash table */
    obj = OBJR(oindex);
    ctrl = o_control(obj);
    prog = i_classname(f, class);
    h = &ihash[(obj->count ^ (oindex << 2) ^ (f->p_ctrl->oindex << 4) ^ class) %
								    INHASHSZ];
    if (h->ocount == obj->count && h->coindex == f->p_ctrl->oindex &&
	h->class == class && h->iindex < ctrl->ninherits) {
	oindex = ctrl->inherits[h->iindex].oindex;
	if (strcmp(OBJR(oindex)->chain.name, prog) == 0) {
	    return (ctrl->inherits[h->iindex].priv) ? -1 : 1;	/* found it */
	}
    }

    /* next, search for it the hard way */
    for (i = ctrl->ninherits, inh = ctrl->inherits + i; i != 0; ) {
	--i;
	--inh;
	if (strcmp(prog, OBJR(inh->oindex)->chain.name) == 0) {
	    /* found it; update hashtable */
	    h->ocount = obj->count;
	    h->coindex = f->p_ctrl->oindex;
	    h->class = class;
	    h->iindex = i;
	    return (ctrl->inherits[i].priv) ? -1 : 1;
	}
    }
    return FALSE;
}

/*
 * NAME:	interpret->cast()
 * DESCRIPTION:	cast a value to a type
 */
void i_cast(frame *f, value *val, unsigned int type, Uint class)
{
    char tnbuf[TNBUFSIZE];
    value *elts;

    if (type == T_CLASS) {
	if (val->type == T_OBJECT) {
	    if (!i_instanceof(f, val->oindex, class)) {
		error("Value is not of object type /%s", i_classname(f, class));
	    }
	    return;
	} else if (val->type == T_LWOBJECT) {
	    elts = d_get_elts(val->u.array);
	    if (elts->type == T_OBJECT) {
		if (!i_instanceof(f, elts->oindex, class)) {
		    error("Value is not of object type /%s",
			  i_classname(f, class));
		}
	    } else if (strcmp(o_builtin_name(elts->u.number),
			      i_classname(f, class)) != 0) {
		/*
		 * builtin types can only be cast to their own type
		 */
		error("Value is not of object type /%s", i_classname(f, class));
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
 * NAME:	interpret->dup()
 * DESCRIPTION:	duplicate a value on the stack
 */
void i_dup(frame *f)
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
				  (value *) NULL, (value *) NULL));
	break;

    case T_SLVALUE:
    case T_SALVALUE:
    case T_SMLVALUE:
	/*
	 * Indexed string.
	 */
	PUSH_INTVAL(f, UCHAR(f->lip[-1].u.string->text[f->lip[-1].oindex]));
	break;

    default:
	i_push_value(f, f->sp);
	break;
    }
}

/*
 * NAME:	istr()
 * DESCRIPTION:	create a copy of the argument string, with one char replaced
 */
static value *istr(value *val, string *str, ssizet i, value *v)
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
 * NAME:	interpret->store_local()
 * DESCRIPTION:	assign a value to a local variable
 */
static void i_store_local(frame *f, int local, value *val, value *verify)
{
    value *var;

    i_add_ticks(f, 1);
    var = (local < 0) ? f->fp + local : f->argp + local;
    if (verify == NULL ||
	(var->type == T_STRING && var->u.string == verify->u.string)) {
	d_assign_var(f->data, var, val);
    }
}

/*
 * NAME:	interpret->store_global()
 * DESCRIPTION:	assign a value to a global variable
 */
void i_store_global(frame *f, int inherit, int index, value *val, value *verify)
{
    unsigned short offset;
    value *var;

    i_add_ticks(f, 5);
    inherit = f->ctrl->imap[f->p_index + inherit];
    offset = f->ctrl->inherits[inherit].varoffset + index;
    if (f->lwobj == NULL) {
	var = d_get_variable(f->data, offset);
	if (verify == NULL ||
	    (var->type == T_STRING && var->u.string == verify->u.string)) {
	    d_assign_var(f->data, var, val);
	}
    } else {
	var = &f->lwobj->elts[2 + offset];
	if (verify == NULL ||
	    (var->type == T_STRING && var->u.string == verify->u.string)) {
	    d_assign_elt(f->data, f->lwobj, var, val);
	}
    }
}

/*
 * NAME:	interpret->store_index()
 * DESCRIPTION:	perform an indexed assignment
 */
bool i_store_index(frame *f, value *var, value *aval, value *ival, value *val)
{
    string *str;
    array *arr;

    i_add_ticks(f, 3);
    switch (aval->type) {
    case T_STRING:
	if (ival->type != T_INT) {
	    error("Non-numeric string index");
	}
	if (val->type != T_INT) {
	    error("Non-numeric value in indexed string assignment");
	}
	str = str_new(aval->u.string->text, aval->u.string->len);
	str->text[str_index(str, ival->u.number)] = val->u.number;
	PUT_STRVAL(var, str);
	return TRUE;

    case T_ARRAY:
	if (ival->type != T_INT) {
	    error("Non-numeric array index");
	}
	arr = aval->u.array;
	aval = &d_get_elts(arr)[arr_index(arr, ival->u.number)];
	if (var->type != T_STRING ||
	    (aval->type == T_STRING && var->u.string == aval->u.string)) {
	    d_assign_elt(f->data, arr, aval, val);
	}
	arr_del(arr);
	break;

    case T_MAPPING:
	arr = aval->u.array;
	if (var->type != T_STRING) {
	    var = NULL;
	}
	map_index(f->data, arr, ival, val, var);
	i_del_value(ival);
	arr_del(arr);
	break;

    case T_LWOBJECT:
	arr = aval->u.array;
	i_operator(f, arr, "[]=", 2, var, ival, val);
	i_del_value(var);
	i_del_value(ival);
	arr_del(arr);
	break;

    default:
	error("Index on bad type");
    }

    return FALSE;
}

/*
 * NAME:	interpret->store()
 * DESCRIPTION:	Perform an assignment.
 */
void i_store(frame *f)
{
    value *val;
    Uint class;

    if (f->p_ctrl->flags & CTRL_OLDVM) {
	value *lval;
	array *a;
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
			 istr(&ival, f->lip[-1].u.string, f->lip[-1].oindex,
			      val));
	    str_del((--f->lip)->u.string);
	    break;

	case T_ALVALUE:
	    a = lval->u.array;
	    d_assign_elt(f->data, a, &d_get_elts(a)[(--f->lip)->u.number], val);
	    arr_del(a);
	    break;

	case T_MLVALUE:
	    map_index(f->data, a = lval->u.array, &f->lip[-1], val,
		      (value *) NULL);
	    i_del_value(--f->lip);
	    arr_del(a);
	    break;

	case T_SALVALUE:
	    a = lval->u.array;
	    d_assign_elt(f->data, a, &a->elts[f->lip[-2].u.number],
			 istr(&ival, f->lip[-1].u.string, f->lip[-1].oindex,
			      val));
	    str_del((--f->lip)->u.string);
	    --f->lip;
	    arr_del(a);
	    break;

	case T_SMLVALUE:
	    map_index(f->data, a = lval->u.array, &f->lip[-2],
		      istr(&ival, f->lip[-1].u.string, f->lip[-1].oindex, val),
		      (value *) NULL);
	    str_del((--f->lip)->u.string);
	    i_del_value(--f->lip);
	    arr_del(a);
	    break;
	}
	f->sp += 2;
    } else {
	Uint lval;
	int type;
	value var, *val, *tmp;

	val = f->sp + 1;
	lval = (val++)->u.number;
	type = (lval >> 24) & 0xf;
	if (type == T_CLASS) {
	    class = (val++)->u.number;
	} else {
	    class = 0;
	}
	if (type != 0) {
	    i_cast(f, f->sp, type, class);
	}
	tmp = f->sp;
	f->sp = val;
	val = tmp;

	switch (lval >> 28) {
	case LVAL_LOCAL:
	    i_store_local(f, SCHAR(lval), val, NULL);
	    break;

	case LVAL_GLOBAL:
	    i_store_global(f, (lval >> 8) & 0xffff, UCHAR(lval), val, NULL);
	    break;

	case LVAL_INDEX:
	    var = nil_value;
	    if (i_store_index(f, &var, f->sp + 1, f->sp, val)) {
		str_del(f->sp[1].u.string);
		str_del(var.u.string);
	    }
	    f->sp += 2;
	    break;

	case LVAL_LOCAL_INDEX:
	    var = nil_value;
	    if (i_store_index(f, &var, f->sp + 1, f->sp, val)) {
		i_store_local(f, SCHAR(lval), &var, f->sp + 1);
		str_del(f->sp[1].u.string);
		str_del(var.u.string);
	    }
	    f->sp += 2;
	    break;

	case LVAL_GLOBAL_INDEX:
	    var = nil_value;
	    if (i_store_index(f, &var, f->sp + 1, f->sp, val)) {
		i_store_global(f, (lval >> 8) & 0xffff, UCHAR(lval), val,
			       f->sp + 1);
		str_del(f->sp[1].u.string);
		str_del(var.u.string);
	    }
	    f->sp += 2;
	    break;

	case LVAL_INDEX_INDEX:
	    var = nil_value;
	    if (i_store_index(f, &var, f->sp + 1, f->sp, val)) {
		i_store_index(f, f->sp + 1, f->sp + 3, f->sp + 2, &var);
		str_del(f->sp[1].u.string);
		str_del(var.u.string);
	    }
	    f->sp += 4;
	    break;
	}
    }
}

/*
 * NAME:	interpret->get_depth()
 * DESCRIPTION:	get the remaining stack depth (-1: infinite)
 */
Int i_get_depth(frame *f)
{
    rlinfo *rlim;

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
Int i_get_ticks(frame *f)
{
    rlinfo *rlim;

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
static void i_check_rlimits(frame *f)
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
void i_new_rlimits(frame *f, Int depth, Int t)
{
    rlinfo *rlim;

    rlim = ALLOC(rlinfo, 1);
    memset(rlim, '\0', sizeof(rlinfo));
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
void i_set_rlimits(frame *f, rlinfo *rlim)
{
    rlinfo *r, *next;

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
frame *i_set_sp(frame *ftop, value *sp)
{
    value *v, *w;
    frame *f;

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
}

/*
 * NAME:	interpret->prev_object()
 * DESCRIPTION:	return the nth previous object in the call_other chain
 */
frame *i_prev_object(frame *f, int n)
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
char *i_prev_program(frame *f, int n)
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
void i_typecheck(frame *f, frame *prog_f, char *name, char *ftype, char *proto, int nargs, int strict)
{
    char tnbuf[TNBUFSIZE];
    int i, n, atype, ptype;
    char *args;
    bool ellipsis;
    Uint class;
    value *elts;

    class = 0;
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
		if (f->sp[i].type == T_OBJECT) {
		    if (!i_instanceof(prog_f, f->sp[i].oindex, class)) {
			error("Bad object argument %d for function %s",
			      nargs - i, name);
		    }
		} else {
		    elts = d_get_elts(f->sp[i].u.array);
		    if (elts->type == T_OBJECT) {
			if (!i_instanceof(prog_f, elts->oindex, class)) {
			    error("Bad object argument %d for function %s",
				  nargs - i, name);
			}
		    } else if (strcmp(o_builtin_name(elts->u.number),
				      i_classname(prog_f, class)) != 0) {
			error("Bad object argument %d for function %s",
			      nargs - i, name);
		    }
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
static unsigned short i_switch_int(frame *f, char *pc)
{
    unsigned short h, l, m, sz, dflt;
    Int num;
    char *p;

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
static unsigned short i_switch_range(frame *f, char *pc)
{
    unsigned short h, l, m, sz, dflt;
    Int num;
    char *p;

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
static unsigned short i_switch_str(frame *f, char *pc)
{
    unsigned short h, l, m, u, u2, dflt;
    int cmp;
    char *p;
    control *ctrl;

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
void i_catcherr(frame *f, Int depth)
{
    i_runtime_error(f, depth);
}

/*
 * NAME:	interpret->interpret0()
 * DESCRIPTION:	Old interpreter function. Interpret stack machine code.
 */
static void i_interpret0(frame *f, char *pc)
{
    unsigned short instr, u, u2;
    Uint l;
    char *p;
    kfunc *kf;
    int size;
    bool atomic;
    Int newdepth, newticks;

    size = 0;
    l = 0;

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
	case II_PUSH_ZERO:
	    PUSH_INTVAL(f, 0);
	    break;

	case II_PUSH_ONE:
	    PUSH_INTVAL(f, 1);
	    break;

	case II_PUSH_INT1:
	    PUSH_INTVAL(f, FETCH1S(pc));
	    break;

	case II_PUSH_INT4:
	    PUSH_INTVAL(f, FETCH4S(pc, l));
	    break;

	case II_PUSH_FLOAT:
	    FETCH2U(pc, u);
	    PUSH_FLTCONST(f, u, FETCH4U(pc, l));
	    break;

	case II_PUSH_STRING:
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, f->p_ctrl->ninherits - 1,
					  FETCH1U(pc)));
	    break;

	case II_PUSH_NEAR_STRING:
	    u = FETCH1U(pc);
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, u, FETCH1U(pc)));
	    break;

	case II_PUSH_FAR_STRING:
	    u = FETCH1U(pc);
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, u, FETCH2U(pc, u2)));
	    break;

	case II_PUSH_LOCAL:
	    u = FETCH1S(pc);
	    i_push_value(f, ((short) u < 0) ? f->fp + (short) u : f->argp + u);
	    break;

	case II_PUSH_GLOBAL:
	    i_global(f, f->p_ctrl->progindex, FETCH1U(pc));
	    break;

	case II_PUSH_FAR_GLOBAL:
	    u = FETCH1U(pc);
	    i_global(f, u, FETCH1U(pc));
	    break;

	case II_PUSH_LOCAL_LVAL:
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

	case II_PUSH_GLOBAL_LVAL:
	    u = FETCH1U(pc);
	    if (instr & I_TYPE_BIT) {
		instr = FETCH1U(pc);
		if (instr == T_CLASS) {
		    FETCH3U(pc, l);
		}
	    } else {
		instr = 0;
	    }
	    i_global_lvalue(f, f->p_ctrl->progindex, u, instr, l);
	    continue;

	case II_PUSH_FAR_GLOBAL_LVAL:
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

	case II_INDEX:
	    i_index(f);
	    break;

	case II_INDEX_LVAL:
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

	case II_AGGREGATE:
	    if (FETCH1U(pc) == 0) {
		i_aggregate(f, FETCH2U(pc, u));
	    } else {
		i_map_aggregate(f, FETCH2U(pc, u));
	    }
	    break;

	case II_SPREAD:
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

	case II_CAST:
	    u = FETCH1U(pc);
	    if (u == T_CLASS) {
		FETCH3U(pc, l);
	    }
	    i_cast(f, f->sp, u, l);
	    break;

	case II_DUP:
	    i_dup(f);
	    break;

	case II_STORE:
	    i_store(f);
	    --f->sp;
	    f->sp[0] = f->sp[-1];
	    break;

	case II_JUMP:
	    p = f->prog + FETCH2U(pc, u);
	    pc = p;
	    break;

	case II_JUMP_ZERO:
	    p = f->prog + FETCH2U(pc, u);
	    if (!VAL_TRUE(f->sp)) {
		pc = p;
	    }
	    break;

	case II_JUMP_NONZERO:
	    p = f->prog + FETCH2U(pc, u);
	    if (VAL_TRUE(f->sp)) {
		pc = p;
	    }
	    break;

	case II_SWITCH:
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

	case II_CALL_KFUNC:
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
		} else if (u <= PROTO_NARGS(kf->proto) + PROTO_VARGS(kf->proto))
		{
		    error("Bad argument %d for kfun %s", u, kf->name);
		} else {
		    error("Too many arguments for kfun %s", kf->name);
		}
	    }
	    break;

	case II_CALL_AFUNC:
	    u = FETCH1U(pc);
	    i_funcall(f, (object *) NULL, (array *) NULL, 0, u,
		      FETCH1U(pc) + size);
	    size = 0;
	    break;

	case II_CALL_DFUNC:
	    u = UCHAR(f->ctrl->imap[f->p_index + FETCH1U(pc)]);
	    u2 = FETCH1U(pc);
	    i_funcall(f, (object *) NULL, (array *) NULL, u, u2,
		      FETCH1U(pc) + size);
	    size = 0;
	    break;

	case II_CALL_FUNC:
	    p = &f->ctrl->funcalls[2L * (f->foffset + FETCH2U(pc, u))];
	    i_funcall(f, (object *) NULL, (array *) NULL, UCHAR(p[0]),
		      UCHAR(p[1]), FETCH1U(pc) + size);
	    size = 0;
	    break;

	case II_CATCH:
	    atomic = f->atomic;
	    p = f->prog + FETCH2U(pc, u);
	    if (!ec_push((ec_ftn) i_catcherr)) {
		f->atomic = FALSE;
		i_interpret0(f, pc);
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

	case II_RLIMITS:
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
	    i_interpret0(f, pc);
	    pc = f->pc;
	    i_set_rlimits(f, f->rlim->next);
	    break;

	case II_RETURN:
	    return;
	}

	if (instr & I_POP_BIT) {
	    /* pop the result of the last operation (never an lvalue) */
	    i_del_value(f->sp++);
	}
    }
}

/*
 * NAME:	interpret->interpret1()
 * DESCRIPTION:	Main interpreter function v1. Interpret stack machine code.
 */
static void i_interpret1(frame *f, char *pc)
{
    unsigned short instr, u, u2;
    Uint l;
    char *p;
    kfunc *kf;
    int size;
    bool atomic;
    Int newdepth, newticks;
    value val;

    size = 0;
    l = 0;

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

	switch (instr & I_EINSTR_MASK) {
	case I_PUSH_INT1:
	    PUSH_INTVAL(f, FETCH1S(pc));
	    continue;

	case I_PUSH_INT2:
	    PUSH_INTVAL(f, FETCH2S(pc, u));
	    continue;

	case I_PUSH_INT4:
	    PUSH_INTVAL(f, FETCH4S(pc, l));
	    continue;

	case I_PUSH_FLOAT6:
	    FETCH2U(pc, u);
	    PUSH_FLTCONST(f, u, FETCH4U(pc, l));
	    continue;

	case I_PUSH_STRING:
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, f->p_ctrl->ninherits - 1,
					  FETCH1U(pc)));
	    continue;

	case I_PUSH_NEAR_STRING:
	    u = FETCH1U(pc);
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, u, FETCH1U(pc)));
	    continue;

	case I_PUSH_FAR_STRING:
	    u = FETCH1U(pc);
	    PUSH_STRVAL(f, d_get_strconst(f->p_ctrl, u, FETCH2U(pc, u2)));
	    continue;

	case I_PUSH_LOCAL:
	    u = FETCH1S(pc);
	    i_push_value(f, ((short) u < 0) ? f->fp + (short) u : f->argp + u);
	    continue;

	case I_PUSH_GLOBAL:
	    i_global(f, f->p_ctrl->ninherits - 1, FETCH1U(pc));
	    continue;

	case I_PUSH_FAR_GLOBAL:
	    u = FETCH1U(pc);
	    i_global(f, u, FETCH1U(pc));
	    continue;

	case I_INDEX:
	case I_INDEX | I_POP_BIT:
	    i_index2(f, f->sp + 1, f->sp, &val, FALSE);
	    *++f->sp = val;
	    break;

	case I_INDEX2:
	    --f->sp;
	    i_index2(f, f->sp + 2, f->sp + 1, f->sp, TRUE);
	    continue;

	case I_AGGREGATE:
	case I_AGGREGATE | I_POP_BIT:
	    if (FETCH1U(pc) == 0) {
		i_aggregate(f, FETCH2U(pc, u));
	    } else {
		i_map_aggregate(f, FETCH2U(pc, u));
	    }
	    break;

	case I_SPREAD:
	    u = FETCH1S(pc);
	    if ((short) u >= 0) {
		u2 = FETCH1U(pc);
		if (u2 == T_CLASS) {
		    FETCH3U(pc, l);
		}
	    } else {
		u2 = 0;
	    }
	    size = i_spread(f, (short) u, u2, l);
	    continue;

	case I_CAST:
	case I_CAST | I_POP_BIT:
	    u = FETCH1U(pc);
	    if (u == T_CLASS) {
		FETCH3U(pc, l);
	    }
	    i_cast(f, f->sp, u, l);
	    break;

	case I_STORE_LOCAL:
	case I_STORE_LOCAL | I_POP_BIT:
	    i_store_local(f, FETCH1S(pc), f->sp, NULL);
	    break;

	case I_STORE_GLOBAL:
	case I_STORE_GLOBAL | I_POP_BIT:
	    i_store_global(f, f->p_ctrl->ninherits - 1, FETCH1U(pc), f->sp,
			   NULL);
	    break;

	case I_STORE_FAR_GLOBAL:
	case I_STORE_FAR_GLOBAL | I_POP_BIT:
	    u = FETCH1U(pc);
	    i_store_global(f, u, FETCH1U(pc), f->sp, NULL);
	    break;

	case I_STORE_INDEX:
	case I_STORE_INDEX | I_POP_BIT:
	    val = nil_value;
	    if (i_store_index(f, &val, f->sp + 2, f->sp + 1, f->sp)) {
		str_del(f->sp[2].u.string);
		str_del(val.u.string);
	    }
	    f->sp[2] = f->sp[0];
	    f->sp += 2;
	    break;

	case I_STORE_LOCAL_INDEX:
	case I_STORE_LOCAL_INDEX | I_POP_BIT:
	    u = FETCH1S(pc);
	    val = nil_value;
	    if (i_store_index(f, &val, f->sp + 2, f->sp + 1, f->sp)) {
		i_store_local(f, (short) u, &val, f->sp + 2);
		str_del(f->sp[2].u.string);
		str_del(val.u.string);
	    }
	    f->sp[2] = f->sp[0];
	    f->sp += 2;
	    break;

	case I_STORE_GLOBAL_INDEX:
	case I_STORE_GLOBAL_INDEX | I_POP_BIT:
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    val = nil_value;
	    if (i_store_index(f, &val, f->sp + 2, f->sp + 1, f->sp)) {
		i_store_global(f, u, u2, &val, f->sp + 2);
		str_del(f->sp[2].u.string);
		str_del(val.u.string);
	    }
	    f->sp[2] = f->sp[0];
	    f->sp += 2;
	    break;

	case I_STORE_INDEX_INDEX:
	case I_STORE_INDEX_INDEX | I_POP_BIT:
	    val = nil_value;
	    if (i_store_index(f, &val, f->sp + 2, f->sp + 1, f->sp)) {
		i_store_index(f, f->sp + 2, f->sp + 4, f->sp + 3, &val);
		str_del(f->sp[2].u.string);
		str_del(val.u.string);
	    } else {
		i_del_value(f->sp + 3);
		i_del_value(f->sp + 4);
	    }
	    f->sp[4] = f->sp[0];
	    f->sp += 4;
	    break;

	case I_JUMP_ZERO:
	    p = f->prog + FETCH2U(pc, u);
	    if (!VAL_TRUE(f->sp)) {
		pc = p;
	    }
	    i_del_value(f->sp++);
	    continue;

	case I_JUMP_NONZERO:
	    p = f->prog + FETCH2U(pc, u);
	    if (VAL_TRUE(f->sp)) {
		pc = p;
	    }
	    i_del_value(f->sp++);
	    continue;

	case I_JUMP:
	    p = f->prog + FETCH2U(pc, u);
	    pc = p;
	    continue;

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
	    i_del_value(f->sp++);
	    continue;

	case I_CALL_KFUNC:
	case I_CALL_KFUNC | I_POP_BIT:
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
		} else if (u <= PROTO_NARGS(kf->proto) + PROTO_VARGS(kf->proto))
		{
		    error("Bad argument %d for kfun %s", u, kf->name);
		} else {
		    error("Too many arguments for kfun %s", kf->name);
		}
	    }
	    break;

	case I_CALL_EFUNC:
	case I_CALL_EFUNC | I_POP_BIT:
	    kf = &KFUN(FETCH2U(pc, u));
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
		} else if (u <= PROTO_NARGS(kf->proto) + PROTO_VARGS(kf->proto))
		{
		    error("Bad argument %d for kfun %s", u, kf->name);
		} else {
		    error("Too many arguments for kfun %s", kf->name);
		}
	    }
	    break;

	case I_CALL_CKFUNC:
	case I_CALL_CKFUNC | I_POP_BIT:
	    kf = &KFUN(FETCH1U(pc));
	    u = FETCH1U(pc) + size;
	    size = 0;
	    if (u != PROTO_NARGS(kf->proto)) {
		if (u < PROTO_NARGS(kf->proto)) {
		    error("Too few arguments for kfun %s", kf->name);
		} else {
		    error("Too many arguments for kfun %s", kf->name);
		}
	    }
	    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
		i_typecheck(f, (frame *) NULL, kf->name, "kfun", kf->proto, u,
			    TRUE);
	    }
	    u = (*kf->func)(f, u, kf);
	    if (u != 0) {
		error("Bad argument %d for kfun %s", u, kf->name);
	    }
	    break;

	case I_CALL_CEFUNC:
	case I_CALL_CEFUNC | I_POP_BIT:
	    kf = &KFUN(FETCH2U(pc, u));
	    u = FETCH1U(pc) + size;
	    size = 0;
	    if (u != PROTO_NARGS(kf->proto)) {
		if (u < PROTO_NARGS(kf->proto)) {
		    error("Too few arguments for kfun %s", kf->name);
		} else {
		    error("Too many arguments for kfun %s", kf->name);
		}
	    }
	    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
		i_typecheck(f, (frame *) NULL, kf->name, "kfun", kf->proto, u,
			    TRUE);
	    }
	    u = (*kf->func)(f, u, kf);
	    if (u != 0) {
		error("Bad argument %d for kfun %s", u, kf->name);
	    }
	    break;

	case I_CALL_AFUNC:
	case I_CALL_AFUNC | I_POP_BIT:
	    u = FETCH1U(pc);
	    i_funcall(f, (object *) NULL, (array *) NULL, 0, u,
		      FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CALL_DFUNC:
	case I_CALL_DFUNC | I_POP_BIT:
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    i_funcall(f, (object *) NULL, (array *) NULL,
		      UCHAR(f->ctrl->imap[f->p_index + u]), u2,
		      FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CALL_FUNC:
	case I_CALL_FUNC | I_POP_BIT:
	    p = &f->ctrl->funcalls[2L * (f->foffset + FETCH2U(pc, u))];
	    i_funcall(f, (object *) NULL, (array *) NULL, UCHAR(p[0]),
		      UCHAR(p[1]), FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CATCH:
	case I_CATCH | I_POP_BIT:
	    atomic = f->atomic;
	    p = f->prog + FETCH2U(pc, u);
	    if (!ec_push((ec_ftn) i_catcherr)) {
		f->atomic = FALSE;
		i_interpret1(f, pc);
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
	    i_interpret1(f, pc);
	    pc = f->pc;
	    i_set_rlimits(f, f->rlim->next);
	    continue;

	case I_RETURN:
	    return;

# ifdef DEBUG
	default:
	    fatal("illegal instruction");
# endif
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
void i_funcall(frame *prev_f, object *obj, array *lwobj, int p_ctrli, int funci, int nargs)
{
    char *pc;
    unsigned short n;
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
    f.p_index = f.ctrl->inherits[p_ctrli].progoffset;

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
	int i;

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
	value *v;
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
    f.prog = pc += 2;
    if (f.p_ctrl->flags & CTRL_OLDVM) {
	i_interpret0(&f, pc);
    } else {
	i_interpret1(&f, pc);
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
bool i_call(frame *f, object *obj, array *lwobj, char *func, unsigned int len,
	int call_static, int nargs)
{
    dsymbol *symb;
    dfuncdef *fdef;
    control *ctrl;

    if (lwobj != (array *) NULL) {
	uindex oindex;
	xfloat flt;
	value val;

	GET_FLT(&lwobj->elts[1], flt);
	if (lwobj->elts[0].type == T_OBJECT) {
	    /*
	     * ordinary light-weight object: upgrade first if needed
	     */
	    oindex = lwobj->elts[0].oindex;
	    obj = OBJR(oindex);
	    if (obj->update != flt.low) {
		d_upgrade_lwobj(lwobj, obj);
	    }
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
	if (lwobj->elts[0].type == T_INT) {
	    /* no user-callable functions within (right?) */
	    i_pop(f, nargs);
	    return FALSE;
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
 * NAME:	interpret->line0()
 * DESCRIPTION:	return the line number the program counter of the specified
 *		frame is at
 */
static unsigned short i_line0(frame *f)
{
    char *pc, *numbers;
    int instr;
    short offset;
    unsigned short line, u, sz;

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
	case II_INDEX_LVAL:
	    if ((instr & I_TYPE_BIT) && FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    /* fall through */
	case II_PUSH_ZERO:
	case II_PUSH_ONE:
	case II_INDEX:
	case II_DUP:
	case II_STORE:
	case II_RETURN:
	    break;

	case II_PUSH_INT1:
	case II_PUSH_STRING:
	case II_PUSH_LOCAL:
	case II_PUSH_GLOBAL:
	case II_RLIMITS:
	    pc++;
	    break;

	case II_CAST:
	    if (FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    break;

	case II_PUSH_LOCAL_LVAL:
	case II_PUSH_GLOBAL_LVAL:
	case II_SPREAD:
	    pc++;
	    if ((instr & I_TYPE_BIT) && FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    break;

	case II_PUSH_NEAR_STRING:
	case II_PUSH_FAR_GLOBAL:
	case II_JUMP:
	case II_JUMP_ZERO:
	case II_JUMP_NONZERO:
	case II_CALL_AFUNC:
	case II_CATCH:
	    pc += 2;
	    break;

	case II_PUSH_FAR_GLOBAL_LVAL:
	    pc += 2;
	    if ((instr & I_TYPE_BIT) && FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    break;

	case II_PUSH_FAR_STRING:
	case II_AGGREGATE:
	case II_CALL_DFUNC:
	case II_CALL_FUNC:
	    pc += 3;
	    break;

	case II_PUSH_INT4:
	    pc += 4;
	    break;

	case II_PUSH_FLOAT:
	    pc += 6;
	    break;

	case II_SWITCH:
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

	case II_CALL_KFUNC:
	    if (PROTO_VARGS(KFUN(FETCH1U(pc)).proto) != 0) {
		pc++;
	    }
	    break;
	}
    }

    return line;
}

/*
 * NAME:	interpret->line1()
 * DESCRIPTION:	return the line number the program counter of the specified
 *		frame is at
 */
static unsigned short i_line1(frame *f)
{
    char *pc, *numbers;
    int instr;
    short offset;
    unsigned short line, u, sz;

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

	switch (instr & I_EINSTR_MASK) {
	case I_INDEX:
	case I_INDEX | I_POP_BIT:
	case I_INDEX2:
	case I_STORE_INDEX:
	case I_STORE_INDEX | I_POP_BIT:
	case I_STORE_INDEX_INDEX:
	case I_STORE_INDEX_INDEX | I_POP_BIT:
	case I_RETURN:
	    break;

	case I_CALL_KFUNC:
	case I_CALL_KFUNC | I_POP_BIT:
	    if (PROTO_VARGS(KFUN(FETCH1U(pc)).proto) != 0) {
		pc++;
	    }
	    break;

	case I_PUSH_INT1:
	case I_PUSH_STRING:
	case I_PUSH_LOCAL:
	case I_PUSH_GLOBAL:
	case I_STORE_LOCAL:
	case I_STORE_LOCAL | I_POP_BIT:
	case I_STORE_GLOBAL:
	case I_STORE_GLOBAL | I_POP_BIT:
	case I_STORE_LOCAL_INDEX:
	case I_STORE_LOCAL_INDEX | I_POP_BIT:
	case I_RLIMITS:
	    pc++;
	    break;

	case I_SPREAD:
	    if (FETCH1S(pc) < 0) {
		break;
	    }
	    /* fall through */
	case I_CAST:
	case I_CAST | I_POP_BIT:
	    if (FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    break;

	case I_CALL_EFUNC:
	case I_CALL_EFUNC | I_POP_BIT:
	    if (PROTO_VARGS(KFUN(FETCH2U(pc, u)).proto) != 0) {
		pc++;
	    }
	    break;

	case I_PUSH_INT2:
	case I_PUSH_NEAR_STRING:
	case I_PUSH_FAR_GLOBAL:
	case I_STORE_FAR_GLOBAL:
	case I_STORE_FAR_GLOBAL | I_POP_BIT:
	case I_STORE_GLOBAL_INDEX:
	case I_STORE_GLOBAL_INDEX | I_POP_BIT:
	case I_JUMP_ZERO:
	case I_JUMP_NONZERO:
	case I_JUMP:
	case I_CALL_AFUNC:
	case I_CALL_AFUNC | I_POP_BIT:
	case I_CALL_CKFUNC:
	case I_CALL_CKFUNC | I_POP_BIT:
	case I_CATCH:
	case I_CATCH | I_POP_BIT:
	    pc += 2;
	    break;

	case I_PUSH_FAR_STRING:
	case I_AGGREGATE:
	case I_AGGREGATE | I_POP_BIT:
	case I_CALL_DFUNC:
	case I_CALL_DFUNC | I_POP_BIT:
	case I_CALL_FUNC:
	case I_CALL_FUNC | I_POP_BIT:
	case I_CALL_CEFUNC:
	case I_CALL_CEFUNC | I_POP_BIT:
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
	}
    }

    return line;
}

/*
 * NAME:	interpret->func_trace()
 * DESCRIPTION:	return the trace of a single function
 */
static array *i_func_trace(frame *f, dataspace *data)
{
    char buffer[STRINGSZ + 12];
    value *v;
    string *str;
    char *name;
    unsigned short n;
    value *args;
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
    PUT_INTVAL(v, (f->func->class & C_COMPILED) ? 0 :
		   (f->p_ctrl->flags & CTRL_OLDVM) ? i_line0(f) : i_line1(f));
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
bool i_call_tracei(frame *ftop, Int idx, value *v)
{
    frame *f;
    unsigned short n;

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
array *i_call_trace(frame *ftop)
{
    frame *f;
    value *v;
    unsigned short n;
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
static void emptyhandler(frame *f, Int depth)
{
    UNREFERENCED_PARAMETER(f);
    UNREFERENCED_PARAMETER(depth);
}

/*
 * NAME:	interpret->call_critical()
 * DESCRIPTION:	Call a function in the driver object at a critical moment.
 *		The function is called with rlimits (-1; -1) and errors
 *		caught.
 */
bool i_call_critical(frame *f, char *func, int narg, int flag)
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
void i_runtime_error(frame *f, Int depth)
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
void i_atomic_error(frame *ftop, Int level)
{
    frame *f;

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
frame *i_restore(frame *ftop, Int level)
{
    frame *f;

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
    frame *f;

    f = cframe;
    if (f->stack != stack) {
	FREE(f->stack);
	f->fp = f->sp = stack + MIN_STACK;
	f->stack = f->lip = stack;
    }

    f->rlim = &rlim;
}
