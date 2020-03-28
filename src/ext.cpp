/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2020 DGD Authors (see the commit log for details)
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
# include "data.h"
# include "control.h"
# include "interpret.h"
# include "comm.h"
# include "table.h"
# include <float.h>
# include <math.h>

# define EXTENSION_MAJOR	1
# define EXTENSION_MINOR	2


/*
 * NAME:	ext->frame_object()
 * DESCRIPTION:	return the current object
 */
static Object *ext_frame_object(Frame *f)
{
    return (f->lwobj == NULL) ? OBJW(f->oindex) : NULL;
}

/*
 * NAME:	ext->frame_dataspace()
 * DESCRIPTION:	return the current dataspace
 */
static Dataspace *ext_frame_dataspace(Frame *f)
{
    return f->data;
}

/*
 * NAME:	ext->frame_arg()
 * DESCRIPTION:	return the given argument
 */
static Value *ext_frame_arg(Frame *f, int nargs, int arg)
{
    return f->sp + nargs - arg - 1;
}

/*
 * NAME:	ext->frame_atomic()
 * DESCRIPTION:	running atomically?
 */
static int ext_frame_atomic(Frame *f)
{
    return (f->level != 0);
}

/*
 * NAME:	ext->value_type()
 * DESCRIPTION:	return the type of a value
 */
static int ext_value_type(Value *val)
{
    return val->type;
}

/*
 * NAME:	ext->value_nil()
 * DESCRIPTION:	return nil
 */
static Value *ext_value_nil()
{
    return &Value::nil;
}

/*
 * NAME:	ext->value_temp()
 * DESCRIPTION:	return a scratch value
 */
Value *ext_value_temp(Dataspace *data)
{
    static Value temp;

    UNREFERENCED_PARAMETER(data);
    return &temp;
}

/*
 * NAME:	ext->value_temp2()
 * DESCRIPTION:	return another scratch value
 */
static Value *ext_value_temp2(Dataspace *data)
{
    static Value temp2;

    UNREFERENCED_PARAMETER(data);
    return &temp2;
}

/*
 * NAME:	ext->int_getval()
 * DESCRIPTION:	retrieve an int from a value
 */
static Int ext_int_getval(Value *val)
{
    return val->number;
}

/*
 * NAME:	ext->int_putval()
 * DESCRIPTION:	store an int in a value
 */
static void ext_int_putval(Value *val, Int i)
{
    PUT_INTVAL(val, i);
}

# ifndef NOFLOAT
/*
 * NAME:	ext->float_getval()
 * DESCRIPTION:	retrieve a float from a value
 */
static long double ext_float_getval(Value *val)
{
    Float flt;
    double d;

    GET_FLT(val, flt);
    if ((flt.high | flt.low) == 0) {
	return 0.0;
    } else {
	d = ldexp((double) (0x10 | (flt.high & 0xf)), 32);
	d = ldexp(d + flt.low, ((flt.high >> 4) & 0x7ff) - 1023 - 36);
	return (long double) ((flt.high >> 15) ? -d : d);
    }
}

/*
 * NAME:	ext->float_putval()
 * DESCRIPTION:	store a float in a value
 */
static int ext_float_putval(Value *val, long double ld)
{
    double d;
    Float flt;
    unsigned short sign;
    int e;
    Uuint m;

    d = (double) ld;
    if (d == 0.0) {
	flt.high = 0;
	flt.low = 0;
    } else {
	sign = (d < 0.0);
	d = frexp(fabs(d), &e);
# if (DBL_MANT_DIG > 37)
	d += (double) (1 << (DBL_MANT_DIG - 38));
	d -= (double) (1 << (DBL_MANT_DIG - 38));
	if (d >= 1.0) {
	    if (++e > 1023) {
		return FALSE;
	    }
	    d = ldexp(d, -1);
	}
# endif
	if (e <= -1023) {
	    flt.high = 0;
	    flt.low = 0;
	} else {
	    m = (Uuint) ldexp(d, 37);
	    flt.high = (sign << 15) | ((e - 1 + 1023) << 4) |
		       ((unsigned short) (m >> 32) & 0xf);
	    flt.low = (Uuint) m;
	}
    }
    PUT_FLTVAL(val, flt);
    return TRUE;
}
# endif

/*
 * NAME:	ext->string_getval()
 * DESCRIPTION:	retrieve a string from a value
 */
static String *ext_string_getval(Value *val)
{
    return val->string;
}

/*
 * NAME:	ext->string_putval()
 * DESCRIPTION:	store a string in a value
 */
static void ext_string_putval(Value *val, String *str)
{
    PUT_STRVAL_NOREF(val, str);
}

/*
 * NAME:	ext->string_new()
 * DESCRIPTION:	create a new string
 */
static String *ext_string_new(Dataspace *data, char *text, int len)
{
    UNREFERENCED_PARAMETER(data);

    try {
	return String::create(text, len);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * NAME:	ext->string_text()
 * DESCRIPTION:	return string text
 */
static char *ext_string_text(String *str)
{
    return str->text;
}

/*
 * NAME:	ext->string_length()
 * DESCRIPTION:	return string length
 */
static int ext_string_length(String *str)
{
    return str->len;
}

/*
 * NAME:	ext->object_putval()
 * DESCRIPTION:	store an object in a value
 */
static void ext_object_putval(Value *val, Object *obj)
{
    PUT_OBJVAL(val, obj);
}

/*
 * NAME:	ext->object_name()
 * DESCRIPTION:	store the name of an object
 */
static const char *ext_object_name(Frame *f, Object *obj, char *buf)
{
    UNREFERENCED_PARAMETER(f);
    return obj->objName(buf);
}

/*
 * NAME:	ext->object_isspecial()
 * DESCRIPTION:	return TRUE if the given object is special, FALSE otherwise
 */
static int ext_object_isspecial(Object *obj)
{
    return ((obj->flags & O_SPECIAL) != 0);
}

/*
 * NAME:	ext->object_ismarked()
 * DESCRIPTION:	return TRUE if the given object is marked, FALSE otherwise
 */
static int ext_object_ismarked(Object *obj)
{
    return ((obj->flags & O_SPECIAL) == O_SPECIAL);
}

/*
 * NAME:	ext->object_mark()
 * DESCRIPTION:	mark the given object
 */
static void ext_object_mark(Object *obj)
{
    obj->flags |= O_SPECIAL;
}

/*
 * NAME:	ext->object_unmark()
 * DESCRIPTION:	unmark the given object
 */
static void ext_object_unmark(Object *obj)
{
    obj->flags &= ~O_SPECIAL;
}

/*
 * NAME:	ext->array_getval()
 * DESCRIPTION:	retrieve an array from a value
 */
static Array *ext_array_getval(Value *val)
{
    return val->array;
}

/*
 * NAME:	ext->array_putval()
 * DESCRIPTION:	store an array in a value
 */
static void ext_array_putval(Value *val, Array *a)
{
    PUT_ARRVAL_NOREF(val, a);
}

/*
 * NAME:	ext->array_new()
 * DESCRIPTION:	create a new array
 */
static Array *ext_array_new(Dataspace *data, int size)
{
    try {
	return Array::createNil(data, size);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * NAME:	ext->array_index()
 * DESCRIPTION:	return an array element
 */
static Value *ext_array_index(Array *a, int i)
{
    return &Dataspace::elts(a)[i];
}

/*
 * NAME:	ext->array_assign()
 * DESCRIPTION:	assign a value to an array element
 */
static void ext_array_assign(Dataspace *data, Array *a, int i, Value *val)
{
    data->assignElt(a, &Dataspace::elts(a)[i], val);
}

/*
 * NAME:	ext->array_size()
 * DESCRIPTION:	return the size of an array
 */
static int ext_array_size(Array *a)
{
    return a->size;
}

/*
 * NAME:	ext->mapping_putval()
 * DESCRIPTION:	store a mapping in a value
 */
static void ext_mapping_putval(Value *val, Array *m)
{
    PUT_MAPVAL_NOREF(val, m);
}

/*
 * NAME:	ext->mapping_new()
 * DESCRIPTION:	create a new mapping
 */
static Array *ext_mapping_new(Dataspace *data)
{
    return Array::mapCreate(data, 0);
}

/*
 * NAME:	ext->mapping_index()
 * DESCRIPTION:	return a value from a mapping
 */
static Value *ext_mapping_index(Array *m, Value *idx)
{
    try {
	return m->mapIndex(m->primary->data, idx, (Value *) NULL,
			   (Value *) NULL);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * NAME:	ext->mapping_assign()
 * DESCRIPTION:	assign to a mapping value
 */
static void ext_mapping_assign(Dataspace *data, Array *m, Value *idx,
			       Value *val)
{
    try {
	m->mapIndex(data, idx, val, (Value *) NULL);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * NAME:	ext->mapping_enum()
 * DESCRIPTION:	return the nth enumerated index
 */
static Value *ext_mapping_enum(Array *m, int i)
{
    m->mapCompact(m->primary->data);
    return &Dataspace::elts(m)[i];
}

/*
 * NAME:	ext->mapping_size()
 * DESCRIPTION:	return the size of a mapping
 */
static int ext_mapping_size(Array *m)
{
    return m->mapSize(m->primary->data);
}

/*
 * NAME:	ext->runtime_error()
 * DESCRIPTION:	handle an error at runtime
 */
void ext_runtime_error(Frame *f, const char *mesg)
{
    UNREFERENCED_PARAMETER(f);

    try {
	error(mesg);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * NAME:	ext->runtime_ticks()
 * DESCRIPTION:	spend ticks
 */
void ext_runtime_ticks(Frame *f, int ticks)
{
    i_add_ticks(f, ticks);
}

/*
 * NAME:	ext->runtime_check()
 * DESCRIPTION:	check ticks
 */
void ext_runtime_check(Frame *f, int ticks)
{
    i_add_ticks(f, ticks);
    if (!f->rlim->noticks && f->rlim->ticks <= 0) {
	f->rlim->ticks = 0;
	error("Out of ticks");
    }
}


/*
 * push int on the stack
 */
static void ext_vm_int(Frame *f, Int n)
{
    PUSH_INTVAL(f, n);
}

/*
 * push float on the stack
 */
static void ext_vm_float(Frame *f, double flt)
{
    ext_float_putval(--f->sp, flt);
}

/*
 * push string on the stack
 */
static void ext_vm_string(Frame *f, uint16_t inherit, uint16_t index)
{
    PUSH_STRVAL(f, f->p_ctrl->strconst(inherit, index));
}

/*
 * push parameter on the stack
 */
static void ext_vm_param(Frame *f, uint8_t param)
{
    f->pushValue(f->argp + param);
}

/*
 * get int parameter
 */
static Int ext_vm_param_int(Frame *f, uint8_t param)
{
    return f->argp[param].number;
}

/*
 * get float parameter
 */
static double ext_vm_param_float(Frame *f, uint8_t param)
{
    return ext_float_getval(f->argp + param);
}

/*
 * push local variable on the stack
 */
static void ext_vm_local(Frame *f, uint8_t local)
{
    f->pushValue(f->fp - local);
}

/*
 * get int local variable
 */
static Int ext_vm_local_int(Frame *f, uint8_t local)
{
    return (f->fp - local)->number;
}

/*
 * get float local variable
 */
static double ext_vm_local_float(Frame *f, uint8_t local)
{
    return ext_float_getval(f->fp - local);
}

/*
 * get global variable
 */
static void ext_vm_global(Frame *f, uint16_t inherit, uint8_t index)
{
    f->pushValue(f->global(inherit, index));
}

/*
 * get integer global variable
 */
static Int ext_vm_global_int(Frame *f, uint16_t inherit, uint8_t index)
{
    return f->global(inherit, index)->number;
}

/*
 * get float global variable
 */
static double ext_vm_global_float(Frame *f, uint16_t inherit, uint8_t index)
{
    return ext_float_getval(f->global(inherit, index));
}

/*
 * index value on the stack
 */
static void ext_vm_index(Frame *f)
{
    try {
	Value val;

	f->index(f->sp + 1, f->sp, &val, FALSE);
	*++f->sp = val;
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * index string
 */
static Int ext_vm_index_int(Frame *f)
{
    try {
	Value val;

	f->index(f->sp + 1, f->sp, &val, FALSE);
	f->sp += 2;
	return val.number;
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * index and keep
 */
static void ext_vm_index2(Frame *f)
{
    try {
	Value val;

	f->index(f->sp + 1, f->sp, &val, TRUE);
	*--f->sp = val;
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * index string and keep, and return int
 */
static Int ext_vm_index2_int(Frame *f)
{
    try {
	Value val;

	f->index(f->sp + 1, f->sp, &val, TRUE);
	return val.number;
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * array aggregate
 */
static void ext_vm_aggregate(Frame *f, uint16_t size)
{
    try {
	f->aggregate(size);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * mapping aggregate
 */
static void ext_vm_map_aggregate(Frame *f, uint16_t size)
{
    try {
	f->mapAggregate(size);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * cast value to a type
 */
static void ext_vm_cast(Frame *f, uint8_t type, uint16_t inherit,
			uint16_t index)
{
    try {
	f->cast(f->sp, type, ((Uint) inherit << 16) + index);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * cast value to an int
 */
static Int ext_vm_cast_int(Frame *f)
{
    if (f->sp->type != T_INT) {
	ext_runtime_error(f, "Value is not an int");
    }
    return (f->sp++)->number;
}

/*
 * cast value to a float
 */
static double ext_vm_cast_float(Frame *f)
{
    if (f->sp->type != T_FLOAT) {
	ext_runtime_error(f, "Value is not a float");
    }
    return ext_float_getval(f->sp++);
}

/*
 * obj <= "/path/to/thing"
 */
static Int ext_vm_instanceof(Frame *f, uint16_t inherit, uint16_t index)
{
    Int instance;

    instance = f->instanceOf(((Uint) inherit << 16) + index);
    f->sp++;
    return instance;
}

/*
 * check range
 */
static void ext_vm_range(Frame *f)
{
    try {
	kf_ckrangeft(f, 0, NULL);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * check range from
 */
static void ext_vm_range_from(Frame *f)
{
    try {
	kf_ckrangef(f, 0, NULL);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * check range to
 */
static void ext_vm_range_to(Frame *f)
{
    try {
	kf_ckranget(f, 0, NULL);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * store parameter
 */
static void ext_vm_store_param(Frame *f, uint8_t param)
{
    f->storeParam(param, f->sp);
}

/*
 * store int in parameter
 */
static void ext_vm_store_param_int(Frame *f, uint8_t param, Int number)
{
    Value val;

    PUT_INTVAL(&val, number);
    f->storeParam(param, &val);
}

/*
 * store float in parameter
 */
static void ext_vm_store_param_float(Frame *f, uint8_t param, double flt)
{
    Value val;

    ext_float_putval(&val, flt);
    f->storeParam(param, &val);
}

/*
 * store local variable
 */
static void ext_vm_store_local(Frame *f, uint8_t local)
{
    f->storeLocal(local, f->sp);
}

/*
 * store int in local variable
 */
static void ext_vm_store_local_int(Frame *f, uint8_t local, Int number)
{
    Value val;

    PUT_INTVAL(&val, number);
    f->storeLocal(local, &val);
}

/*
 * store float in local variable
 */
static void ext_vm_store_local_float(Frame *f, uint8_t local, double flt)
{
    Value val;

    ext_float_putval(&val, flt);
    f->storeLocal(local, &val);
}

/*
 * store global variable
 */
static void ext_vm_store_global(Frame *f, uint16_t inherit, uint8_t index)
{
    f->storeGlobal(inherit, index, f->sp);
}

/*
 * store int in global variable
 */
static void ext_vm_store_global_int(Frame *f, uint16_t inherit,
				    uint8_t index, Int number)
{
    Value val;

    PUT_INTVAL(&val, number);
    f->storeGlobal(inherit, index, &val);
}

/*
 * store float in global variable
 */
static void ext_vm_store_global_float(Frame *f, uint16_t inherit,
				      uint8_t index, double flt)
{
    Value val;

    ext_float_putval(&val, flt);
    f->storeGlobal(inherit, index, &val);
}

/*
 * indexed store
 */
static void ext_vm_store_index(Frame *f)
{
    try {
	f->storeIndex(f->sp);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * indexed store in parameter
 */
static void ext_vm_store_param_index(Frame *f, uint8_t param)
{
    try {
	f->storeParamIndex(param, f->sp);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * indexed store in local variable
 */
static void ext_vm_store_local_index(Frame *f, uint8_t local)
{
    try {
	f->storeLocalIndex(local, f->sp);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * indexed store in global variable
 */
static void ext_vm_store_global_index(Frame *f, uint16_t inherit, uint8_t index)
{
    try {
	f->storeGlobalIndex(inherit, index, f->sp);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * indexed indexed store
 */
static void ext_vm_store_index_index(Frame *f)
{
    try {
	f->storeIndexIndex(f->sp);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * prepare for a number of stores
 */
static void ext_vm_stores(Frame *f, uint8_t n)
{
    if (f->sp->type != T_ARRAY) {
	ext_runtime_error(f, "Value is not an array");
    }
    if (n > f->sp->array->size) {
	ext_runtime_error(f, "Wrong number of lvalues");
    }
    Dataspace::elts(f->sp->array);
    f->nStores = n;
}

/*
 * prepare for a number of stores
 */
static void ext_vm_stores_lval(Frame *f, uint8_t n)
{
    if (n < f->sp->array->size) {
	ext_runtime_error(f, "Missing lvalue");
    }
    f->nStores = n;
}

/*
 * prepare for a number of stores
 */
static void ext_vm_stores_spread(Frame *f, uint8_t n, uint8_t offset,
				 uint8_t type, uint16_t inherit, uint16_t index)
{
    --n;
    if (n < f->storesSpread(n, offset, type, ((Uint) inherit << 16) + index)) {
	ext_runtime_error(f, "Missing lvalue");
    }
    f->nStores = n;
}

/*
 * cast value to a type
 */
static void ext_vm_stores_cast(Frame *f, uint8_t type,
			       uint16_t inherit, uint16_t index)
{
    try {
	if (f->nStores <= f->sp->array->size) {
	    f->cast(&f->sp->array->elts[f->nStores - 1], type,
		    ((Uint) inherit << 16) + index);
	}
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * store value in parameter
 */
static void ext_vm_stores_param(Frame *f, uint8_t param)
{
    if (--(f->nStores) < f->sp->array->size) {
	f->storeParam(param, &f->sp->array->elts[f->nStores]);
    }
}

/*
 * store int in parameter
 */
static Int ext_vm_stores_param_int(Frame *f, uint8_t param)
{
    if (--(f->nStores) < f->sp->array->size) {
	f->storeParam(param, &f->sp->array->elts[f->nStores]);
    }
    return f->argp[param].number;
}

/*
 * store float in parameter
 */
static double ext_vm_stores_param_float(Frame *f, uint8_t param)
{
    if (--(f->nStores) < f->sp->array->size) {
	f->storeParam(param, &f->sp->array->elts[f->nStores]);
    }
    return ext_float_getval(f->argp + param);
}

/*
 * store value in local variable
 */
static void ext_vm_stores_local(Frame *f, uint8_t local)
{
    if (--(f->nStores) < f->sp->array->size) {
	f->storeLocal(local, &f->sp->array->elts[f->nStores]);
    }
}

/*
 * store int in local variable
 */
static Int ext_vm_stores_local_int(Frame *f, uint8_t local, Int n)
{
    if (--(f->nStores) < f->sp->array->size) {
	f->storeLocal(local, &f->sp->array->elts[f->nStores]);
	return (f->fp - local)->number;
    }
    return n;
}

/*
 * store float in local variable
 */
static double ext_vm_stores_local_float(Frame *f, uint8_t local, double flt)
{
    if (--(f->nStores) < f->sp->array->size) {
	f->storeLocal(local, &f->sp->array->elts[f->nStores]);
	return ext_float_getval(f->fp - local);
    }
    return flt;
}

/*
 * store value in global variable
 */
static void ext_vm_stores_global(Frame *f, uint16_t inherit, uint8_t index)
{
    if (--(f->nStores) < f->sp->array->size) {
	f->storeGlobal(inherit, index, &f->sp->array->elts[f->nStores]);
    }
}

/*
 * indexed store
 */
static void ext_vm_stores_index(Frame *f)
{
    try {
	if (--(f->nStores) < f->sp->array->size) {
	    f->storeIndex(&f->sp->array->elts[f->nStores]);
	} else {
	    f->storeSkip();
	}
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * indexed store in parameter
 */
static void ext_vm_stores_param_index(Frame *f, uint8_t param)
{
    try {
	if (--(f->nStores) < f->sp->array->size) {
	    f->storeParamIndex(param, &f->sp->array->elts[f->nStores]);
	} else {
	    f->storeSkip();
	}
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * indexed store in local variable
 */
static void ext_vm_stores_local_index(Frame *f, uint8_t local)
{
    try {
	if (--(f->nStores) < f->sp->array->size) {
	    f->storeLocalIndex(local, &f->sp->array->elts[f->nStores]);
	} else {
	    f->storeSkip();
	}
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * indexed store in global variable
 */
static void ext_vm_stores_global_index(Frame *f, uint16_t inherit,
				       uint8_t index)
{
    try {
	if (--(f->nStores) < f->sp->array->size) {
	    f->storeGlobalIndex(inherit, index,
				&f->sp->array->elts[f->nStores]);
	} else {
	    f->storeSkip();
	}
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * indexed indexed store
 */
static void ext_vm_stores_index_index(Frame *f)
{
    try {
	if (--(f->nStores) < f->sp->array->size) {
	    f->storeIndexIndex(&f->sp->array->elts[f->nStores]);
	} else {
	    f->storeSkipSkip();
	}
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * integer division
 */
static Int ext_vm_div_int(Frame *f, Int num, Int denom)
{
    UNREFERENCED_PARAMETER(f);

    try {
	return Frame::div(num, denom);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * integer left shift
 */
static Int ext_vm_lshift_int(Frame *f, Int num, Int shift)
{
    UNREFERENCED_PARAMETER(f);

    try {
	return Frame::lshift(num, shift);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * integer modulus
 */
static Int ext_vm_mod_int(Frame *f, Int num, Int denom)
{
    UNREFERENCED_PARAMETER(f);

    try {
	return Frame::mod(num, denom);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * integer right shift
 */
static Int ext_vm_rshift_int(Frame *f, Int num, Int shift)
{
    UNREFERENCED_PARAMETER(f);

    try {
	return Frame::rshift(num, shift);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * convert to float
 */
static double ext_vm_tofloat(Frame *f)
{
    try {
	Float flt;
	Value val;

	f->toFloat(&flt);
	PUT_FLT(&val, flt);
	return ext_float_getval(&val);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * convert to int
 */
static Int ext_vm_toint(Frame *f)
{
    try {
	return f->toInt();
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * convert float to int
 */
static Int ext_vm_toint_float(Frame *f, double iflt)
{
    UNREFERENCED_PARAMETER(f);

    try {
	Value val;
	Float flt;

	ext_float_putval(&val, iflt);
	GET_FLT(&val, flt);
	return flt.ftoi();
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * nil
 */
static void ext_vm_nil(Frame *f)
{
    *--f->sp = Value::nil;
}

/*
 * float addition
 */
static double ext_vm_add_float(Frame *f, double flt1, double flt2)
{
    try {
	Value val;
	Float f1, f2;

	i_add_ticks(f, 1);
	ext_float_putval(&val, flt1);
	GET_FLT(&val, f1);
	ext_float_putval(&val, flt2);
	GET_FLT(&val, f2);
	f1.add(f2);
	PUT_FLTVAL(&val, f1);
	return ext_float_getval(&val);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * float division
 */
static double ext_vm_div_float(Frame *f, double flt1, double flt2)
{
    try {
	Value val;
	Float f1, f2;

	i_add_ticks(f, 1);
	ext_float_putval(&val, flt1);
	GET_FLT(&val, f1);
	ext_float_putval(&val, flt2);
	GET_FLT(&val, f2);
	f1.div(f2);
	PUT_FLTVAL(&val, f1);
	return ext_float_getval(&val);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * float multiplication
 */
static double ext_vm_mult_float(Frame *f, double flt1, double flt2)
{
    try {
	Value val;
	Float f1, f2;

	i_add_ticks(f, 1);
	ext_float_putval(&val, flt1);
	GET_FLT(&val, f1);
	ext_float_putval(&val, flt2);
	GET_FLT(&val, f2);
	f1.mult(f2);
	PUT_FLTVAL(&val, f1);
	return ext_float_getval(&val);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * float subtraction
 */
static double ext_vm_sub_float(Frame *f, double flt1, double flt2)
{
    try {
	Value val;
	Float f1, f2;

	i_add_ticks(f, 1);
	ext_float_putval(&val, flt1);
	GET_FLT(&val, f1);
	ext_float_putval(&val, flt2);
	GET_FLT(&val, f2);
	f1.sub(f2);
	PUT_FLTVAL(&val, f1);
	return ext_float_getval(&val);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call kernel function
 */
static void ext_vm_kfunc(Frame *f, uint16_t n, int nargs)
{
    try {
	f->kfunc(n, nargs);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call kernel function with int result
 */
static Int ext_vm_kfunc_int(Frame *f, uint16_t n, int nargs)
{
    try {
	f->kfunc(n, nargs);
	return (f->sp++)->number;
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call kfun with float result
 */
static double ext_vm_kfunc_float(Frame *f, uint16_t n, int nargs)
{
    try {
	f->kfunc(n, nargs);
	return ext_float_getval(f->sp++);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call kfun with spread
 */
static void ext_vm_kfunc_spread(Frame *f, uint16_t n, int nargs)
{
    try {
	f->kfunc(n, nargs + f->spread(-1));
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call kfun with spread and int result
 */
static Int ext_vm_kfunc_spread_int(Frame *f, uint16_t n, int nargs)
{
    try {
	f->kfunc(n, nargs + f->spread(-1));
	return (f->sp++)->number;
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call kfun with spread and float result
 */
static double ext_vm_kfunc_spread_float(Frame *f, uint16_t n, int nargs)
{
    try {
	f->kfunc(n, nargs + f->spread(-1));
	return ext_float_getval(f->sp++);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call kfun with lvalue spread
 */
static void ext_vm_kfunc_spread_lval(Frame *f, uint16_t lval, uint16_t n,
				     int nargs)
{
    try {
	f->kfunc(n, nargs + f->spread(lval));
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call direct function
 */
static void ext_vm_dfunc(Frame *f, uint16_t inherit, uint8_t n, int nargs)
{
    try {
	f->funcall(NULL, NULL, f->ctrl->imap[f->p_index + inherit], n, nargs);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call direct function with int result
 */
static Int ext_vm_dfunc_int(Frame *f, uint16_t inherit, uint8_t n, int nargs)
{
    try {
	f->funcall(NULL, NULL, f->ctrl->imap[f->p_index + inherit], n, nargs);
	return (f->sp++)->number;
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call direct function with float result
 */
static double ext_vm_dfunc_float(Frame *f, uint16_t inherit, uint8_t n,
				 int nargs)
{
    try {
	f->funcall(NULL, NULL, f->ctrl->imap[f->p_index + inherit], n, nargs);
	return ext_float_getval(f->sp++);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call direct function with spread
 */
static void ext_vm_dfunc_spread(Frame *f, uint16_t inherit, uint8_t n,
				int nargs)
{
    try {
	f->funcall(NULL, NULL, f->ctrl->imap[f->p_index + inherit], n,
		   nargs + f->spread(-1));
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call direct function with spread and int result
 */
static Int ext_vm_dfunc_spread_int(Frame *f, uint16_t inherit, uint8_t n,
				   int nargs)
{
    try {
	f->funcall(NULL, NULL, f->ctrl->imap[f->p_index + inherit], n,
		   nargs + f->spread(-1));
	return (f->sp++)->number;
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call direct function wit spread and float result
 */
static double ext_vm_dfunc_spread_float(Frame *f, uint16_t inherit,
					uint8_t n, int nargs)
{
    try {
	f->funcall(NULL, NULL, f->ctrl->imap[f->p_index + inherit], n,
		   nargs + f->spread(-1));
	return ext_float_getval(f->sp++);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call virtual function
 */
static void ext_vm_func(Frame *f, uint16_t index, int nargs)
{
    try {
	f->vfunc(index, nargs);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * call virtual function with spread
 */
static void ext_vm_func_spread(Frame *f, uint16_t index, int nargs)
{
    try {
	f->vfunc(index, nargs + f->spread(-1));
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * pop value from stack
 */
static void ext_vm_pop(Frame *f)
{
    (f->sp++)->del();
}

/*
 * pop value and return boolean result
 */
static bool ext_vm_pop_bool(Frame *f)
{
    bool flag;

    flag = VAL_TRUE(f->sp);
    (f->sp++)->del();

    return flag;
}

/*
 * pop value and return int result
 */
static Int ext_vm_pop_int(Frame *f)
{
    return (f->sp++)->number;
}

/*
 * pop value and return float result
 */
static double ext_vm_pop_float(Frame *f)
{
    return ext_float_getval(f->sp++);
}

/*
 * is there an int on the stack?
 */
static bool ext_vm_switch_int(Frame *f)
{
    return (f->sp->type == T_INT);
}

/*
 * perform a range switch
 */
static uint32_t ext_vm_switch_range(Int *table, uint32_t size, Int number)
{
    uint32_t mid, low, high;

    low = 0;
    high = size;
    while (low < high) {
	mid = (low + high) & ~0x01;
	if (number >= table[mid]) {
	    if (number <= table[mid + 1]) {
		return mid >> 1;
	    }
	    low = (mid >> 1) + 1;
	} else {
	    high = mid >> 1;
	}
    }

    return size;
}

/*
 * perform a string switch
 */
static uint32_t ext_vm_switch_string(Frame *f, uint16_t *table, uint32_t size)
{
    String *str;
    Control *ctrl;
    uint32_t mid, low, high;
    int cmp;

    if (f->sp->type == T_STRING) {
	str = f->sp->string;
	ctrl = f->p_ctrl;

	low = (table[0] == 0 && table[1] == 0xffff);
	high = size;
	while (low < high) {
	    mid = (low + high) & ~0x01;

	    cmp = str->cmp(ctrl->strconst(table[mid], table[mid + 1]));
	    if (cmp == 0) {
		size = mid >> 1;
		break;
	    }
	    if (cmp < 0) {
		high = mid >> 1;
	    } else {
		low = (mid >> 1) + 1;
	    }
	}
    } else if (f->sp->type == T_NIL && table[0] == 0 && table[1] == 0xffff) {
	size = 0;	/* case nil */
    }

    (f->sp++)->del();
    return size;
}

/*
 * start rlimits
 */
static void ext_vm_rlimits(Frame *f, bool privileged)
{
    try {
	f->rlimits(privileged);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}

/*
 * end rlimits
 */
static void ext_vm_rlimits_end(Frame *f)
{
    f->setRlimits(f->rlim->next);
}

/*
 * catch an error
 */
static jmp_buf *ext_vm_catch(Frame *f)
{
    jmp_buf *env;

    env = ErrorContext::push(Frame::runtimeError);
    f->atomic = FALSE;
    return env;
}

/*
 * caught an error
 */
static void ext_vm_caught(Frame *f, bool push)
{
    if (push) {
	PUSH_STRVAL(f, ErrorContext::exception());
    }
}

/*
 * end catch
 */
static void ext_vm_catch_end(Frame *f, bool push)
{
    ErrorContext::pop();
    if (push) {
	*--f->sp = Value::nil;
    }
}

/*
 * set current line
 */
static void ext_vm_line(Frame *f, uint16_t line)
{
    f->source = line;
}

/*
 * add ticks at end of loop
 */
static void ext_vm_loop_ticks(Frame *f)
{
    try {
	loop_ticks(f);
    } catch (...) {
	longjmp(*ErrorContext::env, 1);
    }
}


static void (*mod_fdlist)(int*, int);
static void (*mod_finish)();

/*
 * NAME:	ext->spawn()
 * DESCRIPTION:	supply function to pass open descriptors to, after a
 *		subprocess has been spawned
 */
static void ext_spawn(void (*fdlist)(int*, int), void (*finish)())
{
    /* close channels with other modules */
    Config::modFinish();

    mod_fdlist = fdlist;
    mod_finish = finish;
}

static int (*jit_init)(int, int, size_t, size_t, int, int, int, uint8_t*,
		       size_t, void**);
static void (*jit_finish)();
static void (*jit_compile)(uint64_t, uint64_t, int, uint8_t*, size_t, int,
			   uint8_t*, size_t, uint8_t*, size_t);
static int (*jit_execute)(uint64_t, uint64_t, int, int, void*);
static void (*jit_release)(uint64_t, uint64_t);

/*
 * NAME:        ext->jit()
 * DESCRIPTION: initialize JIT extension
 */
static void ext_jit(int (*init)(int, int, size_t, size_t, int, int, int,
				uint8_t*, size_t, void**),
		    void (*finish)(),
		    void (*compile)(uint64_t, uint64_t, int, uint8_t*, size_t,
				    int, uint8_t*, size_t, uint8_t*, size_t),
		    int (*execute)(uint64_t, uint64_t, int, int, void*),
		    void (*release)(uint64_t, uint64_t),
		    int (*functions)(uint64_t, uint64_t, int, void*))
{
    UNREFERENCED_PARAMETER(functions);

    jit_init = init;
    jit_finish = finish;
    jit_compile = compile;
    jit_execute = execute;
    jit_release = release;
}

/*
 * NAME:        ext->kfuns()
 * DESCRIPTION: pass kernel function prototypes to the JIT extension
 */
void ext_kfuns(char *protos, int size, int nkfun)
{
    if (jit_compile != NULL) {
	static voidf *vmtab[96];

	vmtab[ 0] = (voidf *) &ext_vm_int;
	vmtab[ 1] = (voidf *) &ext_vm_float;
	vmtab[ 2] = (voidf *) &ext_vm_string;
	vmtab[ 3] = (voidf *) &ext_vm_param;
	vmtab[ 4] = (voidf *) &ext_vm_param_int;
	vmtab[ 5] = (voidf *) &ext_vm_param_float;
	vmtab[ 6] = (voidf *) &ext_vm_local;
	vmtab[ 7] = (voidf *) &ext_vm_local_int;
	vmtab[ 8] = (voidf *) &ext_vm_local_float;
	vmtab[ 9] = (voidf *) &ext_vm_global;
	vmtab[10] = (voidf *) &ext_vm_global_int;
	vmtab[11] = (voidf *) &ext_vm_global_float;
	vmtab[12] = (voidf *) &ext_vm_index;
	vmtab[13] = (voidf *) &ext_vm_index_int;
	vmtab[14] = (voidf *) &ext_vm_index2;
	vmtab[15] = (voidf *) &ext_vm_index2_int;
	vmtab[16] = (voidf *) &ext_vm_aggregate;
	vmtab[17] = (voidf *) &ext_vm_map_aggregate;
	vmtab[18] = (voidf *) &ext_vm_cast;
	vmtab[19] = (voidf *) &ext_vm_cast_int;
	vmtab[20] = (voidf *) &ext_vm_cast_float;
	vmtab[21] = (voidf *) &ext_vm_instanceof;
	vmtab[22] = (voidf *) &ext_vm_range;
	vmtab[23] = (voidf *) &ext_vm_range_from;
	vmtab[24] = (voidf *) &ext_vm_range_to;
	vmtab[25] = (voidf *) &ext_vm_store_param;
	vmtab[26] = (voidf *) &ext_vm_store_param_int;
	vmtab[27] = (voidf *) &ext_vm_store_param_float;
	vmtab[28] = (voidf *) &ext_vm_store_local;
	vmtab[29] = (voidf *) &ext_vm_store_local_int;
	vmtab[30] = (voidf *) &ext_vm_store_local_float;
	vmtab[31] = (voidf *) &ext_vm_store_global;
	vmtab[32] = (voidf *) &ext_vm_store_global_int;
	vmtab[33] = (voidf *) &ext_vm_store_global_float;
	vmtab[34] = (voidf *) &ext_vm_store_index;
	vmtab[35] = (voidf *) &ext_vm_store_param_index;
	vmtab[36] = (voidf *) &ext_vm_store_local_index;
	vmtab[37] = (voidf *) &ext_vm_store_global_index;
	vmtab[38] = (voidf *) &ext_vm_store_index_index;
	vmtab[39] = (voidf *) &ext_vm_stores;
	vmtab[40] = (voidf *) &ext_vm_stores_lval;
	vmtab[41] = (voidf *) &ext_vm_stores_spread;
	vmtab[42] = (voidf *) &ext_vm_stores_cast;
	vmtab[43] = (voidf *) &ext_vm_stores_param;
	vmtab[44] = (voidf *) &ext_vm_stores_param_int;
	vmtab[45] = (voidf *) &ext_vm_stores_param_float;
	vmtab[46] = (voidf *) &ext_vm_stores_local;
	vmtab[47] = (voidf *) &ext_vm_stores_local_int;
	vmtab[48] = (voidf *) &ext_vm_stores_local_float;
	vmtab[49] = (voidf *) &ext_vm_stores_global;
	vmtab[50] = (voidf *) &ext_vm_stores_index;
	vmtab[51] = (voidf *) &ext_vm_stores_param_index;
	vmtab[52] = (voidf *) &ext_vm_stores_local_index;
	vmtab[53] = (voidf *) &ext_vm_stores_global_index;
	vmtab[54] = (voidf *) &ext_vm_stores_index_index;
	vmtab[55] = (voidf *) &ext_vm_div_int;
	vmtab[56] = (voidf *) &ext_vm_lshift_int;
	vmtab[57] = (voidf *) &ext_vm_mod_int;
	vmtab[58] = (voidf *) &ext_vm_rshift_int;
	vmtab[59] = (voidf *) &ext_vm_tofloat;
	vmtab[60] = (voidf *) &ext_vm_toint;
	vmtab[61] = (voidf *) &ext_vm_toint_float;
	vmtab[62] = (voidf *) &ext_vm_nil;
	vmtab[63] = (voidf *) &ext_vm_add_float;
	vmtab[64] = (voidf *) &ext_vm_div_float;
	vmtab[65] = (voidf *) &ext_vm_mult_float;
	vmtab[66] = (voidf *) &ext_vm_sub_float;
	vmtab[67] = (voidf *) &ext_vm_kfunc;
	vmtab[68] = (voidf *) &ext_vm_kfunc_int;
	vmtab[69] = (voidf *) &ext_vm_kfunc_float;
	vmtab[70] = (voidf *) &ext_vm_kfunc_spread;
	vmtab[71] = (voidf *) &ext_vm_kfunc_spread_int;
	vmtab[72] = (voidf *) &ext_vm_kfunc_spread_float;
	vmtab[73] = (voidf *) &ext_vm_kfunc_spread_lval;
	vmtab[74] = (voidf *) &ext_vm_dfunc;
	vmtab[75] = (voidf *) &ext_vm_dfunc_int;
	vmtab[76] = (voidf *) &ext_vm_dfunc_float;
	vmtab[77] = (voidf *) &ext_vm_dfunc_spread;
	vmtab[78] = (voidf *) &ext_vm_dfunc_spread_int;
	vmtab[79] = (voidf *) &ext_vm_dfunc_spread_float;
	vmtab[80] = (voidf *) &ext_vm_func;
	vmtab[81] = (voidf *) &ext_vm_func_spread;
	vmtab[82] = (voidf *) &ext_vm_pop;
	vmtab[83] = (voidf *) &ext_vm_pop_bool;
	vmtab[84] = (voidf *) &ext_vm_pop_int;
	vmtab[85] = (voidf *) &ext_vm_pop_float;
	vmtab[86] = (voidf *) &ext_vm_switch_int;
	vmtab[87] = (voidf *) &ext_vm_switch_range;
	vmtab[88] = (voidf *) &ext_vm_switch_string;
	vmtab[89] = (voidf *) &ext_vm_rlimits;
	vmtab[90] = (voidf *) &ext_vm_rlimits_end;
	vmtab[91] = (voidf *) &ext_vm_catch;
	vmtab[92] = (voidf *) &ext_vm_caught;
	vmtab[93] = (voidf *) &ext_vm_catch_end;
	vmtab[94] = (voidf *) &ext_vm_line;
	vmtab[95] = (voidf *) &ext_vm_loop_ticks;

	if (!(*jit_init)(VERSION_VM_MAJOR, VERSION_VM_MINOR, sizeof(Int), 1,
			 Config::typechecking(), KF_BUILTINS, nkfun,
			 (uint8_t *) protos, size, (void **) vmtab)) {
	    jit_compile = NULL;
	}
    }
}

/*
 * JIT compile program
 */
static void ext_compile(const Frame *f, Control *ctrl)
{
    Object *obj;
    int i, j, nftypes;
    const Inherit *inh;
    char *ft, *vt;
    const FuncDef *fdef;
    const VarDef *vdef;

    /*
     * compile new program
     */
    obj = OBJ(ctrl->oindex);
    if (obj->flags & O_COMPILED) {
	return;
    }
    obj->flags |= O_COMPILED;

    /* count function types & variable types */
    nftypes = 0;
    for (inh = ctrl->inherits, i = ctrl->ninherits; i > 0; inh++, --i) {
	nftypes += OBJR(inh->oindex)->control()->nfuncdefs;
    }

    char *ftypes = ALLOCA(char, ctrl->ninherits + nftypes);
    char *vtypes = ALLOCA(char, ctrl->ninherits + ctrl->nvariables);

    /* collect function types & variable types */
    ft = ftypes;
    vt = vtypes;
    for (inh = ctrl->inherits, i = ctrl->ninherits; i > 0; inh++, --i) {
	ctrl = OBJR(inh->oindex)->ctrl;
	ctrl->program();
	*ft++ = ctrl->nfuncdefs;
	for (fdef = ctrl->funcs(), j = ctrl->nfuncdefs; j > 0; fdef++, --j)
	{
	    *ft++ = PROTO_FTYPE(ctrl->prog + fdef->offset);
	}
	*vt++ = ctrl->nvardefs;
	for (vdef = ctrl->vars(), j = ctrl->nvardefs; j > 0; vdef++, --j) {
	    *vt++ = vdef->type;
	}
    }

    /* start JIT compiler */
    ctrl = f->p_ctrl;
    (*jit_compile)(ctrl->oindex, ctrl->instance, ctrl->ninherits,
		   (uint8_t *) ctrl->prog, ctrl->progsize, ctrl->nfuncdefs,
		   (uint8_t *) ftypes, ctrl->ninherits + nftypes,
		   (uint8_t *) vtypes, ctrl->ninherits + ctrl->nvariables);

    AFREE(ftypes);
    AFREE(vtypes);
}

/*
 * NAME:	ext->execute()
 * DESCRIPTION:	JIT-compile and execute a function
 */
bool ext_execute(const Frame *f, int func)
{
    Control *ctrl;
    int result;

    if (jit_compile == NULL) {
	return FALSE;
    }
    ctrl = f->p_ctrl;
    if (ctrl->instance == 0) {
	return FALSE;
    }

    if (!setjmp(*ErrorContext::push())) {
	result = (*jit_execute)(ctrl->oindex, ctrl->instance, ctrl->version,
				func, (void *) f);
	ErrorContext::pop();
    } else {
	error((char *) NULL);
    }

    if (result < 0) {
	ext_compile(f, ctrl);

	return FALSE;
    } else {
	return (bool) result;
    }
}

/*
 * remove JIT-compiled object
 */
void ext_release(uint64_t index, uint64_t instance)
{
    if (jit_compile != NULL) {
	(*jit_release)(index, instance);
    }
}

/*
 * NAME:	ext->dgd()
 * DESCRIPTION:	initialize extension interface
 */
bool ext_dgd(char *module, char *config, void (**fdlist)(int*, int),
	     void (**finish)())
{
    voidf *ext_ext[5];
    voidf *ext_frame[4];
    voidf *ext_data[2];
    voidf *ext_value[4];
    voidf *ext_int[2];
    voidf *ext_float[2];
    voidf *ext_string[5];
    voidf *ext_object[6];
    voidf *ext_array[6];
    voidf *ext_mapping[7];
    voidf *ext_runtime[6];
    voidf **ftabs[11];
    int sizes[11];
    int (*init) (int, int, voidf**[], int[], const char*);

    init = (int (*) (int, int, voidf**[], int[], const char*))
						    P_dload(module, "ext_init");
    if (init == NULL) {
	return FALSE;
    }

    mod_fdlist = NULL;
    mod_finish = NULL;

    ext_ext[0] = (voidf *) &KFun::add;
    ext_ext[1] = (voidf *) NULL;
    ext_ext[2] = (voidf *) &ext_spawn;
    ext_ext[3] = (voidf *) &Connection::fdclose;
    ext_ext[4] = (voidf *) &ext_jit;
    ext_frame[0] = (voidf *) &ext_frame_object;
    ext_frame[1] = (voidf *) &ext_frame_dataspace;
    ext_frame[2] = (voidf *) &ext_frame_arg;
    ext_frame[3] = (voidf *) &ext_frame_atomic;
    ext_data[0] = (voidf *) &Dataspace::extra;
    ext_data[1] = (voidf *) &Dataspace::setExtra;
    ext_value[0] = (voidf *) &ext_value_type;
    ext_value[1] = (voidf *) &ext_value_nil;
    ext_value[2] = (voidf *) &ext_value_temp;
    ext_value[3] = (voidf *) &ext_value_temp2;
    ext_int[0] = (voidf *) &ext_int_getval;
    ext_int[1] = (voidf *) &ext_int_putval;
# ifndef NOFLOAT
    ext_float[0] = (voidf *) &ext_float_getval;
    ext_float[1] = (voidf *) &ext_float_putval;
# else
    ext_float[0] = (voidf *) NULL;
    ext_float[1] = (voidf *) NULL;
# endif
    ext_string[0] = (voidf *) &ext_string_getval;
    ext_string[1] = (voidf *) &ext_string_putval;
    ext_string[2] = (voidf *) &ext_string_new;
    ext_string[3] = (voidf *) &ext_string_text;
    ext_string[4] = (voidf *) &ext_string_length;
    ext_object[0] = (voidf *) &ext_object_putval;
    ext_object[1] = (voidf *) &ext_object_name;
    ext_object[2] = (voidf *) &ext_object_isspecial;
    ext_object[3] = (voidf *) &ext_object_ismarked;
    ext_object[4] = (voidf *) &ext_object_mark;
    ext_object[5] = (voidf *) &ext_object_unmark;
    ext_array[0] = (voidf *) &ext_array_getval;
    ext_array[1] = (voidf *) &ext_array_putval;
    ext_array[2] = (voidf *) &ext_array_new;
    ext_array[3] = (voidf *) &ext_array_index;
    ext_array[4] = (voidf *) &ext_array_assign;
    ext_array[5] = (voidf *) &ext_array_size;
    ext_mapping[0] = (voidf *) &ext_array_getval;
    ext_mapping[1] = (voidf *) &ext_mapping_putval;
    ext_mapping[2] = (voidf *) &ext_mapping_new;
    ext_mapping[3] = (voidf *) &ext_mapping_index;
    ext_mapping[4] = (voidf *) &ext_mapping_assign;
    ext_mapping[5] = (voidf *) &ext_mapping_enum;
    ext_mapping[6] = (voidf *) &ext_mapping_size;
    ext_runtime[0] = (voidf *) &ext_runtime_error;
    ext_runtime[1] = (voidf *) &hash_md5_start;
    ext_runtime[2] = (voidf *) &hash_md5_block;
    ext_runtime[3] = (voidf *) &hash_md5_end;
    ext_runtime[4] = (voidf *) &ext_runtime_ticks;
    ext_runtime[5] = (voidf *) &ext_runtime_check;

    ftabs[ 0] = ext_ext;	sizes[ 0] = 5;
    ftabs[ 1] = ext_frame;	sizes[ 1] = 4;
    ftabs[ 2] = ext_data;	sizes[ 2] = 2;
    ftabs[ 3] = ext_value;	sizes[ 3] = 4;
    ftabs[ 4] = ext_int;	sizes[ 4] = 2;
    ftabs[ 5] = ext_float;	sizes[ 5] = 2;
    ftabs[ 6] = ext_string;	sizes[ 6] = 5;
    ftabs[ 7] = ext_object;	sizes[ 7] = 6;
    ftabs[ 8] = ext_array;	sizes[ 8] = 6;
    ftabs[ 9] = ext_mapping;	sizes[ 9] = 7;
    ftabs[10] = ext_runtime;	sizes[10] = 6;

    if (!init(EXTENSION_MAJOR, EXTENSION_MINOR, ftabs, sizes, config)) {
	fatal("incompatible runtime extension");
    }
    *fdlist = mod_fdlist;
    *finish = mod_finish;
    return TRUE;
}

/*
 * NAME:	ext->finish()
 * DESCRIPTION:	finish JIT compiler interface
 */
void ext_finish()
{
    if (jit_compile != NULL) {
	(*jit_finish)();
    }
}
