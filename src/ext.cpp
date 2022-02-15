/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2022 DGD Authors (see the commit log for details)
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
# include "interpret.h"
# include "control.h"
# include "table.h"
# include <float.h>
# include <math.h>

# define EXTENSION_MAJOR	0
# define EXTENSION_MINOR	9


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
    return &nil_value;
}

/*
 * NAME:	ext->value_temp()
 * DESCRIPTION:	return a scratch value
 */
static Value *ext_value_temp(Dataspace *data)
{
    static Value temp;

    UNREFERENCED_PARAMETER(data);
    return &temp;
}

/*
 * NAME:	ext->int_getval()
 * DESCRIPTION:	retrieve an int from a value
 */
static Int ext_int_getval(Value *val)
{
    return val->u.number;
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
    return val->u.string;
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
    return str_new(text, len);
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
    return o_name(buf, obj);
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
    return val->u.array;
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
    return arr_ext_new(data, size);
}

/*
 * NAME:	ext->array_index()
 * DESCRIPTION:	return an array element
 */
static Value *ext_array_index(Array *a, int i)
{
    return &d_get_elts(a)[i];
}

/*
 * NAME:	ext->array_assign()
 * DESCRIPTION:	assign a value to an array element
 */
static void ext_array_assign(Dataspace *data, Array *a, int i, Value *val)
{
    d_assign_elt(data, a, &d_get_elts(a)[i], val);
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
    return map_new(data, 0);
}

/*
 * NAME:	ext->mapping_index()
 * DESCRIPTION:	return a value from a mapping
 */
static Value *ext_mapping_index(Array *m, Value *idx)
{
    return map_index(m->primary->data, m, idx, (Value *) NULL, (Value *) NULL);
}

/*
 * NAME:	ext->mapping_assign()
 * DESCRIPTION:	assign to a mapping value
 */
static void ext_mapping_assign(Dataspace *data, Array *m, Value *idx,
			       Value *val)
{
    map_index(data, m, idx, val, (Value *) NULL);
}

/*
 * NAME:	ext->mapping_enum()
 * DESCRIPTION:	return the nth enumerated index
 */
static Value *ext_mapping_enum(Array *m, int i)
{
    map_compact(m->primary->data, m);
    return &d_get_elts(m)[i];
}

/*
 * NAME:	ext->mapping_size()
 * DESCRIPTION:	return the size of a mapping
 */
static int ext_mapping_size(Array *m)
{
    return map_size(m->primary->data, m);
}

/*
 * NAME:	ext->runtime_error()
 * DESCRIPTION:	handle an error at runtime
 */
static void ext_runtime_error(Frame *f, char *mesg)
{
    UNREFERENCED_PARAMETER(f);
    error(mesg);
}

/*
 * NAME:        ext->jit()
 * DESCRIPTION: don't initialize JIT extension
 */
static void ext_jit(int (*jit_init)(int, int, size_t, size_t, uint16_t*, int,
                                    uint8_t*, int),
                    void (*jit_compile)(uint64_t, uint64_t, int, uint8_t*, int,
                                        uint8_t*, uint8_t*))
{
    UNREFERENCED_PARAMETER(jit_init);
    UNREFERENCED_PARAMETER(jit_compile);
}

/*
 * NAME:        ext->kfuns()
 * DESCRIPTION: don't pass kernel function prototypes to the JIT extension
 */
void ext_kfuns(kfindex *map, char *protos, int nkfun)
{
    UNREFERENCED_PARAMETER(map);
    UNREFERENCED_PARAMETER(protos);
    UNREFERENCED_PARAMETER(nkfun);
}

/*
 * NAME:	ext->dgd()
 * DESCRIPTION:	initialize extension interface
 */
bool ext_dgd(char *module, char *config)
{
    voidf *ext_ext[4];
    voidf *ext_frame[4];
    voidf *ext_data[2];
    voidf *ext_value[3];
    voidf *ext_int[2];
    voidf *ext_float[2];
    voidf *ext_string[5];
    voidf *ext_object[6];
    voidf *ext_array[6];
    voidf *ext_mapping[7];
    voidf *ext_runtime[1];
    voidf **ftabs[11];
    int sizes[11];
    int (*init) (int, int, voidf**[], int[], const char*);

    init = (int (*) (int, int, voidf**[], int[], const char*))
						    P_dload(module, "ext_init");
    if (init == NULL) {
	return FALSE;
    }

    ext_ext[0] = (voidf *) &kf_ext_kfun;
    ext_ext[1] = (voidf *) NULL;
    ext_ext[2] = (voidf *) &ext_jit;
    ext_ext[3] = (voidf *) NULL;
    ext_frame[0] = (voidf *) &ext_frame_object;
    ext_frame[1] = (voidf *) &ext_frame_dataspace;
    ext_frame[2] = (voidf *) &ext_frame_arg;
    ext_frame[3] = (voidf *) &ext_frame_atomic;
    ext_data[0] = (voidf *) &d_get_extravar;
    ext_data[1] = (voidf *) &d_set_extravar;
    ext_value[0] = (voidf *) &ext_value_type;
    ext_value[1] = (voidf *) &ext_value_nil;
    ext_value[2] = (voidf *) &ext_value_temp;
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

    ftabs[ 0] = ext_ext;	sizes[ 0] = 4;
    ftabs[ 1] = ext_frame;	sizes[ 1] = 4;
    ftabs[ 2] = ext_data;	sizes[ 2] = 2;
    ftabs[ 3] = ext_value;	sizes[ 3] = 3;
    ftabs[ 4] = ext_int;	sizes[ 4] = 2;
    ftabs[ 5] = ext_float;	sizes[ 5] = 2;
    ftabs[ 6] = ext_string;	sizes[ 6] = 5;
    ftabs[ 7] = ext_object;	sizes[ 7] = 6;
    ftabs[ 8] = ext_array;	sizes[ 8] = 6;
    ftabs[ 9] = ext_mapping;	sizes[ 9] = 7;
    ftabs[10] = ext_runtime;	sizes[10] = 1;

    if (!init(EXTENSION_MAJOR, EXTENSION_MINOR, ftabs, sizes, config)) {
	fatal("incompatible runtime extension");
    }
    return TRUE;
}
