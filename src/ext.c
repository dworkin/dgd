/*
 * This file is part of DGD, http://www.dworkin.nl/dgd/
 * Copyright (C) 1993-2010 Dworkin B.V.
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
# include <math.h>

# define EXTENSION_MAJOR	0
# define EXTENSION_MINOR	5


/*
 * NAME:	ext->frame_object()
 * DESCRIPTION:	return the current object
 */
static object *ext_frame_object(f)
frame *f;
{
    return (f->lwobj == NULL) ? OBJW(f->oindex) : NULL;
}

/*
 * NAME:	ext->frame_dataspace()
 * DESCRIPTION:	return the current dataspace
 */
static dataspace *ext_frame_dataspace(f)
frame *f;
{
    return f->data;
}

/*
 * NAME:	ext->frame_arg()
 * DESCRIPTION:	return the given argument
 */
static value *ext_frame_arg(f, nargs, arg)
frame *f;
int nargs, arg;
{
    return f->sp + nargs - arg - 1;
}

/*
 * NAME:	ext->frame_atomic()
 * DESCRIPTION:	running atomically?
 */
static int ext_frame_atomic(f)
frame *f;
{
    return (f->level != 0);
}

/*
 * NAME:	ext->frame_push_int()
 * DESCRIPTION:	push a number on the stack
 */
Int ext_frame_push_int(f, i)
frame *f;
Int i;
{
    (--f->sp)->type = T_INT;
    return f->sp->u.number = i;
}

/*
 * NAME:	ext->frame_push_lvalue()
 * DESCRIPTION:	push an lvalue on the stack
 */
void ext_frame_push_lvalue(f, v, t)
register frame *f;
value *v;
int t;
{
    (--f->sp)->type = T_LVALUE;
    f->sp->oindex = t;
    f->sp->u.lval = v;
}

/*
 * NAME:	ext->frame_push_lvclass()
 * DESCRIPTION:	push an lvalue class on the stack
 */
void ext_frame_push_lvclass(f, t)
frame *f;
Int t;
{
    f->lip->type = T_INT;
    (f->lip++)->u.number = t;
}

/*
 * NAME:	ext->frame_pop_int()
 * DESCRIPTION:	pop a number from the stack
 */
Int ext_frame_pop_int(f)
frame *f;
{
    return (f->sp++)->u.number;
}

/*
 * NAME:	ext->frame_pop_truthval()
 * DESCRIPTION:	pop a truth value from the stack
 */
int ext_frame_pop_truthval(f)
register frame *f;
{
    switch (f->sp->type) {
    case T_NIL:
	f->sp++;
	return FALSE;

    case T_INT:
	return (f->sp++)->u.number != 0;

    case T_FLOAT:
	f->sp++;
	return !VFLT_ISZERO(f->sp - 1);

    case T_STRING:
	str_del(f->sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr_del(f->sp->u.array);
	break;
    }
    f->sp++;
    return TRUE;
}

/*
 * NAME:	ext->frame_store()
 * DESCRIPTION:	store a value
 */
void ext_frame_store(f)
register frame *f;
{
    i_store(f);
    f->sp[1] = f->sp[0];
    f->sp++;
}

/*
 * NAME:	ext->frame_store_int()
 * DESCRIPTION:	store an integer value
 */
Int ext_frame_store_int(f)
register frame *f;
{
    i_store(f);
    f->sp += 2;
    return f->sp[-2].u.number;
}

/*
 * NAME:	ext->frame_kfun()
 * DESCRIPTION:	call a kernel function
 */
void ext_frame_kfun(f, n)
frame *f;
register int n;
{
    register kfunc *kf;

    kf = &kftab[n];
    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
	i_typecheck(f, (frame *) NULL, kf->name, "kfun", kf->proto,
		    PROTO_NARGS(kf->proto), TRUE);
    }
    n = (*kf->func)(f, PROTO_NARGS(kf->proto), kf);
    if (n != 0) {
	error("Bad argument %d for kfun %s", n, kf->name);
    }
}

/*
 * NAME:	ext->frame_kfun_arg()
 * DESCRIPTION:	call a kernel function with variable # of arguments
 */
void ext_frame_kfun_arg(f, n, nargs)
frame *f;
register int n, nargs;
{
    register kfunc *kf;

    kf = &kftab[n];
    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
	i_typecheck(f, (frame *) NULL, kf->name, "kfun", kf->proto, nargs,
		    TRUE);
    }
    n = (*kf->func)(f, nargs, kf);
    if (n != 0) {
	error("Bad argument %d for kfun %s", n, kf->name);
    }
}

/*
 * NAME:	ext->value_type()
 * DESCRIPTION:	return the type of a value
 */
static int ext_value_type(val)
value *val;
{
    return val->type;
}

/*
 * NAME:	ext->value_nil()
 * DESCRIPTION:	return nil
 */
static value *ext_value_nil()
{
    return &nil_value;
}

/*
 * NAME:	ext->value_temp()
 * DESCRIPTION:	return a scratch value
 */
static value *ext_value_temp(data)
dataspace *data;
{
    static value temp;

    return &temp;
}

/*
 * NAME:	ext->int_getval()
 * DESCRIPTION:	retrieve an int from a value
 */
static Int ext_int_getval(val)
value *val;
{
    return val->u.number;
}

/*
 * NAME:	ext->int_putval()
 * DESCRIPTION:	store an int in a value
 */
static void ext_int_putval(val, i)
value *val;
Int i;
{
    PUT_INTVAL(val, i);
}

/*
 * NAME:	ext->int_div()
 * DESCRIPTION:	perform integer division
 */
Int ext_int_div(i, d)
register Int i, d;
{
    if (d == 0) {
	error("Division by zero");
    }
    if ((i | d) < 0) {
	Int r;

	r = ((Uint) ((i < 0) ? -i : i)) / ((Uint) ((d < 0) ? -d : d));
	return ((i ^ d) < 0) ? -r : r;
    }
    return ((Uint) i) / ((Uint) d);
}

/*
 * NAME:	ext->int_mod()
 * DESCRIPTION:	perform integer modulus
 */
Int ext_int_mod(i, d)
register Int i, d;
{
    if (d == 0) {
	error("Modulus by zero");
    }
    if (d < 0) {
	d = -d;
    }
    if (i < 0) {
	return - (Int) (((Uint) -i) % ((Uint) d));
    }
    return ((Uint) i) % ((Uint) d);
}

/*
 * NAME:	ext->int_lshift()
 * DESCRIPTION:	perform left shift
 */
Int ext_int_lshift(i, shift)
register Int i, shift;
{
    if ((shift & ~31) != 0) {
	if (shift < 0) {
	    error("Negative left shift");
	}
	return 0;
    }
    return (Uint) i << shift;
}

/*
 * NAME:	ext->int_rshift()
 * DESCRIPTION:	perform right shift
 */
Int ext_int_rshift(i, shift)
register Int i, shift;
{
    if ((shift & ~31) != 0) {
	if (shift < 0) {
	    error("Negative right shift");
	}
	return 0;
    }
    return (Uint) i >> shift;
}

/*
 * NAME:	ext->float_getval()
 * DESCRIPTION:	retrieve a float from a value
 */
static double ext_float_getval(val)
value *val;
{
    xfloat flt;
    register double d;

    GET_FLT(val, flt);
    if ((flt.high | flt.low) == 0) {
	return 0.0;
    } else {
	d = ldexp((double) (0x10 | (flt.high & 0xf)), 32);
	d = ldexp(d + flt.low, ((flt.high >> 4) & 0x7ff) - 1023 - 36);
	return (flt.high >> 15) ? -d : d;
    }
}

/*
 * NAME:	ext->float_putval()
 * DESCRIPTION:	store a float in a value
 */
static void ext_float_putval(val, d)
value *val;
double d;
{
    xfloat flt;
    bool sign;
    int e;

    if (d == 0.0) {
	flt.high = 0;
	flt.low = 0;
    } else {
	sign = (d < 0.0);
	d = ldexp(frexp(fabs(d), &e), 5);
	flt.high = ((unsigned short) sign << 15) | ((e - 1 + 1023) << 4) |
		   ((unsigned short) d & 0xf);
	flt.low = (Uint) (ldexp(d - (int) d, 32) + 0.5);
    }
    PUT_FLTVAL(val, flt);
}

/*
 * NAME:	ext->string_getval()
 * DESCRIPTION:	retrieve a string from a value
 */
static string *ext_string_getval(val)
value *val;
{
    return val->u.string;
}

/*
 * NAME:	ext->string_putval()
 * DESCRIPTION:	store a string in a value
 */
static void ext_string_putval(val, str)
value *val;
string *str;
{
    PUT_STRVAL_NOREF(val, str);
}

/*
 * NAME:	ext->string_new()
 * DESCRIPTION:	create a new string
 */
static string *ext_string_new(data, text, len)
dataspace *data;
char *text;
int len;
{
    return str_new(text, len);
}

/*
 * NAME:	ext->string_text()
 * DESCRIPTION:	return string text
 */
static char *ext_string_text(str)
string *str;
{
    return str->text;
}

/*
 * NAME:	ext->string_length()
 * DESCRIPTION:	return string length
 */
static int ext_string_length(str)
string *str;
{
    return str->len;
}

/*
 * NAME:	ext->object_putval()
 * DESCRIPTION:	store an object in a value
 */
static void ext_object_putval(val, obj)
value *val;
object *obj;
{
    PUT_OBJVAL(val, obj);
}

/*
 * NAME:	ext->object_name()
 * DESCRIPTION:	store the name of an object
 */
static char *ext_object_name(f, obj, buf)
frame *f;
object *obj;
char *buf;
{
    return o_name(buf, obj);
}

/*
 * NAME:	ext->object_isspecial()
 * DESCRIPTION:	return TRUE if the given object is special, FALSE otherwise
 */
static int ext_object_isspecial(obj)
object *obj;
{
    return ((obj->flags & O_SPECIAL) != 0);
}

/*
 * NAME:	ext->object_ismarked()
 * DESCRIPTION:	return TRUE if the given object is marked, FALSE otherwise
 */
static int ext_object_ismarked(obj)
object *obj;
{
    return ((obj->flags & O_SPECIAL) == O_SPECIAL);
}

/*
 * NAME:	ext->object_mark()
 * DESCRIPTION:	mark the given object
 */
static void ext_object_mark(obj)
object *obj;
{
    obj->flags |= O_SPECIAL;
}

/*
 * NAME:	ext->object_unmark()
 * DESCRIPTION:	unmark the given object
 */
static void ext_object_unmark(obj)
object *obj;
{
    obj->flags &= O_SPECIAL;
}

/*
 * NAME:	ext->array_getval()
 * DESCRIPTION:	retrieve an array from a value
 */
static array *ext_array_getval(val)
value *val;
{
    return val->u.array;
}

/*
 * NAME:	ext->array_putval()
 * DESCRIPTION:	store an array in a value
 */
static void ext_array_putval(val, a)
value *val;
array *a;
{
    PUT_ARRVAL_NOREF(val, a);
}

/*
 * NAME:	ext->array_new()
 * DESCRIPTION:	create a new array
 */
static array *ext_array_new(data, size)
dataspace *data;
int size;
{
    return arr_ext_new(data, size);
}

/*
 * NAME:	ext->array_index()
 * DESCRIPTION:	return an array element
 */
static value *ext_array_index(a, i)
array *a;
int i;
{
    return &d_get_elts(a)[i];
}

/*
 * NAME:	ext->array_assign()
 * DESCRIPTION:	assign a value to an array element
 */
static void ext_array_assign(data, a, i, val)
dataspace *data;
array *a;
int i;
value *val;
{
    d_assign_elt(data, a, &d_get_elts(a)[i], val);
}

/*
 * NAME:	ext->array_size()
 * DESCRIPTION:	return the size of an array
 */
static int ext_array_size(a)
array *a;
{
    return a->size;
}

/*
 * NAME:	ext->mapping_putval()
 * DESCRIPTION:	store a mapping in a value
 */
static void ext_mapping_putval(val, m)
value *val;
array *m;
{
    PUT_MAPVAL_NOREF(val, m);
}

/*
 * NAME:	ext->mapping_new()
 * DESCRIPTION:	create a new mapping
 */
static array *ext_mapping_new(data)
dataspace *data;
{
    return map_new(data, 0);
}

/*
 * NAME:	ext->mapping_index()
 * DESCRIPTION:	return a value from a mapping
 */
static value *ext_mapping_index(m, idx)
array *m;
value *idx;
{
    return map_index(m->primary->data, m, idx, NULL);
}

/*
 * NAME:	ext->mapping_assign()
 * DESCRIPTION:	assign to a mapping value
 */
static void ext_mapping_assign(data, m, idx, val)
dataspace *data;
array *m;
value *idx, *val;
{
    map_index(data, m, idx, val);
}

/*
 * NAME:	ext->mapping_enum()
 * DESCRIPTION:	return the nth enumerated index
 */
static value *ext_mapping_enum(m, i)
array *m;
int i;
{
    map_compact(m->primary->data, m);
    return &d_get_elts(m)[i];
}

/*
 * NAME:	ext->mapping_size()
 * DESCRIPTION:	return the size of a mapping
 */
static int ext_mapping_size(m)
array *m;
{
    return map_size(m->primary->data, m);
}

/*
 * NAME:	ext->runtime_error()
 * DESCRIPTION:	handle an error at runtime
 */
static void ext_runtime_error(f, mesg)
frame *f;
char *mesg;
{
    error(mesg);
}

/*
 * NAME:	ext->runtime_rlimits()   
 * DESCRIPTION:	handle rlimits
 */
void ext_runtime_rlimits(f)
register frame *f;
{
    if (f->sp[1].type != T_INT) {
	error("Bad rlimits depth type");
    }
    if (f->sp->type != T_INT) {
	error("Bad rlimits ticks type");
    }
    f->sp += 2;

    i_new_rlimits(f, f->sp[-1].u.number, f->sp[-2].u.number);
}

/*
 * NAME:	ext->runtime_rswitch()
 * DESCRIPTION:	handle a range switch
 */
int ext_runtime_rswitch(i, tab, h)
register Int i, *tab;
register int h;
{
    register int l, m;
    register Int *t;

    l = 0;
    do {
	m = (l + h) >> 1;
	t = tab + (m << 1);
	if (i < *t++) {
	    h = m;	/* search in lower half */
	} else if (i > *t) {
	    l = m + 1;	/* search in upper half */
	} else {
	    return m + 1;	/* found */
	}
    } while (l < h);

    return 0;		/* not found */
}

/*
 * NAME:	ext->runtime_sswitch()
 * DESCRIPTION:	handle a str switch
 */
int ext_runtime_sswitch(f, tab, h)
register frame *f;
register char *tab;
register int h;
{
    register int l, m, c;
    register char *t;
    register string *s;
    value *v;

    v = f->sp++;
    if (VAL_NIL(v)) {
	return (tab[0] == 0);
    } else if (v->type != T_STRING) {
	i_del_value(v);
	return 0;
    }

    s = v->u.string;
    if (*tab++ == 0) {
	tab -= 3;
	l = 1;
    } else {
	l = 0;
    }

    do {
	m = (l + h) >> 1;
	t = tab + 3 * m;
	c = str_cmp(s, d_get_strconst(f->p_ctrl, t[0],
				      (UCHAR(t[1]) << 8) + UCHAR(t[2])));
	if (c == 0) {
	    str_del(s);
	    return m + 1;	/* found */
	} else if (c < 0) {
	    h = m;	/* search in lower half */
	} else {
	    l = m + 1;	/* search in upper half */
	}
    } while (l < h);

    str_del(s);
    return 0;		/* not found */
}

# ifdef LPC_EXTENSION
/*
 * NAME:	ext->dgd()
 * DESCRIPTION:	initialize extension interface
 */
bool ext_dgd(module)
char *module;
{
    char *ext_ext[1];
    char *ext_frame[4];
    char *ext_data[2];
    char *ext_value[3];
    char *ext_int[2];
    char *ext_float[2];
    char *ext_string[5];
    char *ext_object[6];
    char *ext_array[6];
    char *ext_mapping[7];
    char *ext_runtime[1];
    char **ftabs[11];
    int sizes[11];
    int (*init) P((int, int, char**[], int[]));

    init = (int (*) P((int, int, char**[], int[]))) P_dload(module, "ext_init");
    if (init == NULL) {
	return FALSE;
    }

    ext_ext[0] = (char *) &kf_ext_kfun;
    ext_frame[0] = (char *) &ext_frame_object;
    ext_frame[1] = (char *) &ext_frame_dataspace;
    ext_frame[2] = (char *) &ext_frame_arg;
    ext_frame[3] = (char *) &ext_frame_atomic;
    ext_data[0] = (char *) &d_get_extravar;
    ext_data[1] = (char *) &d_set_extravar;
    ext_value[0] = (char *) &ext_value_type;
    ext_value[1] = (char *) &ext_value_nil;
    ext_value[2] = (char *) &ext_value_temp;
    ext_int[0] = (char *) &ext_int_getval;
    ext_int[1] = (char *) &ext_int_putval;
    ext_float[0] = (char *) &ext_float_getval;
    ext_float[1] = (char *) &ext_float_putval;
    ext_string[0] = (char *) &ext_string_getval;
    ext_string[1] = (char *) &ext_string_putval;
    ext_string[2] = (char *) &ext_string_new;
    ext_string[3] = (char *) &ext_string_text;
    ext_string[4] = (char *) &ext_string_length;
    ext_object[0] = (char *) &ext_object_putval;
    ext_object[1] = (char *) &ext_object_name;
    ext_object[2] = (char *) &ext_object_isspecial;
    ext_object[3] = (char *) &ext_object_ismarked;
    ext_object[4] = (char *) &ext_object_mark;
    ext_object[5] = (char *) &ext_object_unmark;
    ext_array[0] = (char *) &ext_array_getval;
    ext_array[1] = (char *) &ext_array_putval;
    ext_array[2] = (char *) &ext_array_new;
    ext_array[3] = (char *) &ext_array_index;
    ext_array[4] = (char *) &ext_array_assign;
    ext_array[5] = (char *) &ext_array_size;
    ext_mapping[0] = (char *) &ext_array_getval;
    ext_mapping[1] = (char *) &ext_mapping_putval;
    ext_mapping[2] = (char *) &ext_mapping_new;
    ext_mapping[3] = (char *) &ext_mapping_index;
    ext_mapping[4] = (char *) &ext_mapping_assign;
    ext_mapping[5] = (char *) &ext_mapping_enum;
    ext_mapping[6] = (char *) &ext_mapping_size;
    ext_runtime[0] = (char *) &ext_runtime_error;

    ftabs[ 0] = ext_ext;	sizes[ 0] = 1;
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

    if (!init(EXTENSION_MAJOR, EXTENSION_MINOR, ftabs, sizes)) {
	fatal("incompatible runtime extension");
    }
    return TRUE;
}
# endif	/* LPC_EXTENSION */
