/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
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

# ifndef FUNCDEF
# include "kfun.h"
# include "path.h"
# include "comm.h"
# include "call_out.h"
# include "editor.h"
# include "node.h"
# include "control.h"
# include "compile.h"
# endif


# ifdef FUNCDEF
FUNCDEF("0.compile_object", kf_old_compile_object, pt_old_compile_object, 0)
# else
char pt_old_compile_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_OBJECT,
				 T_STRING };

/*
 * NAME:	kfun->old_compile_object()
 * DESCRIPTION:	compile an object
 */
int kf_old_compile_object(f)
register frame *f;
{
    char file[STRINGSZ];
    register object *obj;

    if (path_string(file, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 1;
    }
    obj = o_find(file, OACC_MODIFY);
    if (obj != (object *) NULL) {
	if (!(obj->flags & O_MASTER)) {
	    error("Cannot recompile cloned object");
	}
	if (O_UPGRADING(obj)) {
	    error("Object is already being upgraded");
	}
	if (O_INHERITED(obj)) {
	    error("Cannot recompile inherited object");
	}
    }
    obj = c_compile(f, file, obj, (string **) NULL, 0,
		    (OBJR(f->oindex)->flags & O_DRIVER) &&
		    strcmp(d_get_strconst(f->p_ctrl, f->func->inherit,
					  f->func->index)->text,
			   "inherit_program") == 0);
    str_del(f->sp->u.string);
    PUT_OBJVAL(f->sp, obj);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("compile_object", kf_compile_object, pt_compile_object, 1)
# else
char pt_compile_object[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 1, 1, 0, 8,
			     T_OBJECT, T_STRING, T_STRING };

/*
 * NAME:	kfun->compile_object()
 * DESCRIPTION:	compile an object
 */
int kf_compile_object(f, nargs)
register frame *f;
int nargs;
{
    char file[STRINGSZ];
    register value *v;
    register object *obj;
    register string **strs;
    register int i;

    v = &f->sp[nargs - 1];
    if (path_string(file, v->u.string->text, v->u.string->len) == (char *) NULL)
    {
	return 1;
    }
    obj = o_find(file, OACC_MODIFY);
    if (obj != (object *) NULL) {
	if (!(obj->flags & O_MASTER)) {
	    error("Cannot recompile cloned object");
	}
	if (O_UPGRADING(obj)) {
	    error("Object is already being upgraded");
	}
	if (O_INHERITED(obj)) {
	    error("Cannot recompile inherited object");
	}
    }
    if (--nargs != 0) {
	strs = ALLOCA(string*, nargs);
	for (i = nargs, v = f->sp; i > 0; --i) {
	    *strs++ = (v++)->u.string;
	}
	if (ec_push((ec_ftn) NULL)) {
	    AFREE(strs - nargs);
	    error((char *) NULL);
	}
    } else {
	strs = (string **) NULL;
    }
    obj = c_compile(f, file, obj, strs, nargs,
		    (OBJR(f->oindex)->flags & O_DRIVER) &&
		    strcmp(d_get_strconst(f->p_ctrl, f->func->inherit,
					  f->func->index)->text,
			   "inherit_program") == 0);
    if (nargs != 0) {
	ec_pop();
	AFREE(strs - nargs);
	i_pop(f, nargs);
    }
    str_del(f->sp->u.string);
    PUT_OBJVAL(f->sp, obj);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_other", kf_call_other, pt_call_other, 0)
# else
char pt_call_other[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 2, 1, 0, 9,
			 T_MIXED, T_MIXED, T_STRING, T_MIXED };

/*
 * NAME:	kfun->call_other()
 * DESCRIPTION:	call a function in another object
 */
int kf_call_other(f, nargs)
register frame *f;
int nargs;
{
    register value *val;
    object *obj;
    array *lwobj;

    obj = (object *) NULL;
    lwobj = (array *) NULL;
    val = &f->sp[nargs - 1];
    if (val->type == T_STRING) {
	*--f->sp = *val;
	*val = nil_value;	/* erase old copy */
	call_driver_object(f, "call_object", 1);
	*val = *f->sp++;
    }
    switch (val->type) {
    case T_OBJECT:
	obj = OBJR(val->oindex);
	break;

    case T_LWOBJECT:
	lwobj = val->u.array;
	break;

    default:
	/* bad arg 1 */
	return 1;
    }

    if (OBJR(f->oindex)->count == 0) {
	/*
	 * call from destructed object
	 */
	i_pop(f, nargs);
	*--f->sp = nil_value;
	return 0;
    }

    if (i_call(f, obj, lwobj, val[-1].u.string->text, val[-1].u.string->len,
	       FALSE, nargs - 2)) {
	val = f->sp++;		/* function exists */
    } else {
	val = &nil_value;	/* function doesn't exist */
    }
    str_del((f->sp++)->u.string);
    i_del_value(f->sp);
    *f->sp = *val;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_touch", kf_call_touch, pt_call_touch, 0)
# else
char pt_call_touch[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID,
			 T_OBJECT };

/*
 * NAME:	kfun->call_touch()
 * DESCRIPTION:	prepare to call a function when this object is next touched
 */
int kf_call_touch(register frame *f)
{
    object *obj;
    xfloat flt;
    value val, *elts;

    if (f->sp->type == T_LWOBJECT) {
	elts = d_get_elts(f->sp->u.array);
	GET_FLT(&elts[1], flt);
	flt.high = TRUE;
	PUT_FLTVAL(&val, flt);
	d_assign_elt(f->data, f->sp->u.array, &elts[1], &val);
	arr_del(f->sp->u.array);
    } else {
	obj = OBJW(f->sp->oindex);
	if (!O_HASDATA(obj)) {
	    error("call_touch() on uninitialized object");
	}
	obj->flags &= ~O_TOUCHED;
    }
    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_object", kf_this_object, pt_this_object, 0)
# else
char pt_this_object[] = { C_STATIC, 0, 0, 0, 6, T_OBJECT };

/*
 * NAME:	kfun->this_object()
 * DESCRIPTION:	return the current object
 */
int kf_this_object(f)
register frame *f;
{
    register object *obj;

    --f->sp;
    obj = OBJR(f->oindex);
    if (obj->count != 0) {
	if (f->lwobj == (array *) NULL) {
	    PUT_OBJVAL(f->sp, obj);
	} else {
	    PUT_LWOVAL(f->sp, f->lwobj);
	}
    } else {
	*f->sp = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_object", kf_previous_object, pt_previous_object, 0)
# else
char pt_previous_object[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7, T_OBJECT,
			      T_INT };

/*
 * NAME:	kfun->previous_object()
 * DESCRIPTION:	return the previous object in the call_other chain
 */
int kf_previous_object(f, nargs)
register frame *f;
int nargs;
{
    register frame *prev;
    register object *obj;

    if (nargs == 0) {
	*--f->sp = nil_value;
    } else if (f->sp->u.number < 0) {
	return 1;
    }

    prev = i_prev_object(f, (int) f->sp->u.number);
    if (prev != (frame *) NULL) {
	obj = OBJR(prev->oindex);
	if (obj->count != 0) {
	    if (prev->lwobj == (array *) NULL) {
		PUT_OBJVAL(f->sp, obj);
	    } else {
		PUT_LWOVAL(f->sp, prev->lwobj);
	    }
	    return 0;
	}
    }

    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_program", kf_previous_program, pt_previous_program, 0)
# else
char pt_previous_program[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7, T_STRING,
			       T_INT };

/*
 * NAME:	kfun->previous_program()
 * DESCRIPTION:	return the previous program in the function call chain
 */
int kf_previous_program(f, nargs)
register frame *f;
int nargs;
{
    char *prog;
    register string *str;

    if (nargs == 0) {
	*--f->sp = nil_value;
    } else if (f->sp->u.number < 0) {
	return 1;
    }

    prog = i_prev_program(f, (int) f->sp->u.number);
    if (prog != (char *) NULL) {
	PUT_STRVAL(f->sp, str = str_new((char *) NULL, strlen(prog) + 1L));
	str->text[0] = '/';
	strcpy(str->text + 1, prog);
    } else {
	*f->sp = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_trace", kf_call_trace, pt_call_trace, 0)
# else
char pt_call_trace[] = { C_STATIC, 0, 0, 0, 6, T_MIXED | (2 << REFSHIFT) };

/*
 * NAME:	kfun->call_trace()
 * DESCRIPTION:	return the entire call_other chain
 */
int kf_call_trace(f)
register frame *f;
{
    PUSH_ARRVAL(f, i_call_trace(f));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("clone_object", kf_clone_object, pt_clone_object, 0)
# else
char pt_clone_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_OBJECT,
			   T_OBJECT };

/*
 * NAME:	kfun->clone_object()
 * DESCRIPTION:	clone a new object
 */
int kf_clone_object(f)
register frame *f;
{
    register object *obj;

    if (f->sp->type == T_LWOBJECT) {
	error("Cloning from a non-persistent object");
    }
    obj = OBJW(f->sp->oindex);
    if (!(obj->flags & O_MASTER)) {
	error("Cloning from a clone");
    }
    obj = o_clone(obj);
    PUT_OBJ(f->sp, obj);
    if (i_call(f, obj, (array *) NULL, (char *) NULL, 0, TRUE, 0)) {
	i_del_value(f->sp++);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("destruct_object", kf_destruct_object, pt_destruct_object, 0)
# else
char pt_destruct_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID,
			      T_OBJECT };

/*
 * NAME:	kfun->destruct_object()
 * DESCRIPTION:	destruct an object
 */
int kf_destruct_object(f)
register frame *f;
{
    register object *obj;

    if (f->sp->type == T_LWOBJECT) {
	error("Destructing a non-persistent object");
    }
    obj = OBJW(f->sp->oindex);
    switch (obj->flags & O_SPECIAL) {
    case O_USER:
	comm_close(f, obj);
	break;

    case O_EDITOR:
	if (f->level != 0) {
	    error("Destructing editor object in atomic function");
	}
	ed_del(obj);
	break;
    }
    o_del(obj, f);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("new_object", kf_new_object, pt_new_object, 0)
# else
char pt_new_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_OBJECT,
			 T_OBJECT };

/*
 * NAME:	kfun->new_object()
 * DESCRIPTION:	create a new non-persistent object
 */
int kf_new_object(f)
register frame *f;
{
    object *obj;
    array *a;

    if (f->sp->type == T_OBJECT) {
	if (!((obj=OBJW(f->sp->oindex))->flags & O_MASTER)) {
	    error("Creating new instance from a non-master object");
	}

	PUT_LWOVAL(f->sp, lwo_new(f->data, obj));
	if (i_call(f, (object *) NULL, f->sp->u.array, (char *) NULL, 0, TRUE,
		   0)) {
	    i_del_value(f->sp++);
	}
    } else {
	a = lwo_copy(f->data, f->sp->u.array);
	arr_del(f->sp->u.array);
	PUT_LWOVAL(f->sp, a);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("object_name", kf_object_name, pt_object_name, 0)
# else
char pt_object_name[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_STRING,
			  T_OBJECT };

/*
 * NAME:	kfun->object_name()
 * DESCRIPTION:	return the name of an object
 */
int kf_object_name(f)
register frame *f;
{
    char buffer[STRINGSZ + 12], *name;
    register string *str;
    uindex n;

    if (f->sp->type == T_OBJECT) {
	name = o_name(buffer, OBJR(f->sp->oindex));
	PUT_STRVAL(f->sp, str = str_new((char *) NULL, strlen(name) + 1L));
	str->text[0] = '/';
	strcpy(str->text + 1, name);
    } else {
	n = f->sp->u.array->elts[0].oindex;
	arr_del(f->sp->u.array);
	name = o_name(buffer, OBJR(n));
	PUT_STRVAL(f->sp, str = str_new((char *) NULL, strlen(name) + 4L));
	str->text[0] = '/';
	strcpy(str->text + 1, name);
	strcpy(str->text + str->len - 3, "#-1");
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("find_object", kf_find_object, pt_find_object, 0)
# else
char pt_find_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_OBJECT,
			  T_STRING };

/*
 * NAME:	kfun->find_object()
 * DESCRIPTION:	find the loaded object for a given object name
 */
int kf_find_object(f)
register frame *f;
{
    char path[STRINGSZ];
    object *obj;

    if (path_string(path, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	str_del(f->sp->u.string);
	*f->sp = nil_value;
	return 0;
    }
    i_add_ticks(f, 2);
    obj = o_find(path, OACC_READ);
    str_del(f->sp->u.string);
    if (obj != (object *) NULL) {
	PUT_OBJVAL(f->sp, obj);
    } else {
	*f->sp = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("function_object", kf_function_object, pt_function_object, 0)
# else
char pt_function_object[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_STRING,
			      T_STRING, T_OBJECT };

/*
 * NAME:	kfun->function_object()
 * DESCRIPTION:	return the name of the program a function is in
 */
int kf_function_object(f)
register frame *f;
{
    object *obj;
    uindex n;
    dsymbol *symb;
    char *name;

    i_add_ticks(f, 2);
    if (f->sp->type == T_OBJECT) {
	obj = OBJR(f->sp->oindex);
    } else {
	n = f->sp->u.array->elts[0].oindex;
	arr_del(f->sp->u.array);
	obj = OBJR(n);
    }
    f->sp++;
    symb = ctrl_symb(o_control(obj), f->sp->u.string->text,
		     f->sp->u.string->len);
    str_del(f->sp->u.string);

    if (symb != (dsymbol *) NULL) {
	object *o;

	o = OBJR(obj->ctrl->inherits[UCHAR(symb->inherit)].oindex);
	if (!(d_get_funcdefs(o->ctrl)[UCHAR(symb->index)].class & C_STATIC) ||
	    obj->index == f->oindex) {
	    /*
	     * function exists and is callable
	     */
	    name = o->chain.name;
	    PUT_STR(f->sp, str_new((char *) NULL, strlen(name) + 1L));
	    f->sp->u.string->text[0] = '/';
	    strcpy(f->sp->u.string->text + 1, name);
	    return 0;
	}
    }
    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_user", kf_this_user, pt_this_user, 0)
# else
char pt_this_user[] = { C_STATIC, 0, 0, 0, 6, T_OBJECT };

/*
 * NAME:	kfun->this_user()
 * DESCRIPTION:	return the current user object (if any)
 */
int kf_this_user(f)
register frame *f;
{
    object *obj;

    obj = comm_user();
    if (obj != (object *) NULL) {
	PUSH_OBJVAL(f, obj);
    } else {
	*--f->sp = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("query_ip_number", kf_query_ip_number, pt_query_ip_number, 0)
# else
char pt_query_ip_number[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_STRING,
			      T_OBJECT };

/*
 * NAME:	kfun->query_ip_number()
 * DESCRIPTION:	return the ip number of a user
 */
int kf_query_ip_number(f)
register frame *f;
{
    object *obj;

    if (f->sp->type == T_OBJECT) {
	obj = OBJR(f->sp->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER) {
	    PUT_STRVAL(f->sp, comm_ip_number(obj));
	    return 0;
	}
    } else {
	arr_del(f->sp->u.array);
    }

    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("query_ip_name", kf_query_ip_name, pt_query_ip_name, 0)
# else
char pt_query_ip_name[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_STRING,
			    T_OBJECT };

/*
 * NAME:	kfun->query_ip_name()
 * DESCRIPTION:	return the ip name of a user
 */
int kf_query_ip_name(f)
register frame *f;
{
    object *obj;

    if (f->sp->type == T_OBJECT) {
	obj = OBJR(f->sp->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER) {
	    PUT_STRVAL(f->sp, comm_ip_name(obj));
	    return 0;
	}
    } else {
	arr_del(f->sp->u.array);
    }

    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("users", kf_users, pt_users, 0)
# else
char pt_users[] = { C_STATIC, 0, 0, 0, 6, T_OBJECT | (1 << REFSHIFT) };

/*
 * NAME:	kfun->users()
 * DESCRIPTION:	return the array of users
 */
int kf_users(f)
register frame *f;
{
    PUSH_ARRVAL(f, comm_users(f->data));
    i_add_ticks(f, f->sp->u.array->size);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("strlen", kf_strlen, pt_strlen, 0)
# else
char pt_strlen[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT, T_STRING };

/*
 * NAME:	kfun->strlen()
 * DESCRIPTION:	return the length of a string
 */
int kf_strlen(f)
register frame *f;
{
    ssizet len;

    len = f->sp->u.string->len;
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, len);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate", kf_allocate, pt_allocate, 0)
# else
char pt_allocate[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
		       T_MIXED | (1 << REFSHIFT), T_INT };

/*
 * NAME:	kfun->allocate()
 * DESCRIPTION:	allocate an array
 */
int kf_allocate(f)
register frame *f;
{
    register int i;
    register value *v;

    if (f->sp->u.number < 0) {
	return 1;
    }
    i_add_ticks(f, f->sp->u.number);
    PUT_ARRVAL(f->sp, arr_new(f->data, (long) f->sp->u.number));
    for (i = f->sp->u.array->size, v = f->sp->u.array->elts; i > 0; --i, v++) {
	*v = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate_int", kf_allocate_int, pt_allocate_int, 0)
# else
char pt_allocate_int[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
			   T_INT | (1 << REFSHIFT), T_INT };

/*
 * NAME:	kfun->allocate_int()
 * DESCRIPTION:	allocate an array of integers
 */
int kf_allocate_int(f)
register frame *f;
{
    register int i;
    register value *v;

    if (f->sp->u.number < 0) {
	return 1;
    }
    i_add_ticks(f, f->sp->u.number);
    PUT_ARRVAL(f->sp, arr_new(f->data, (long) f->sp->u.number));
    for (i = f->sp->u.array->size, v = f->sp->u.array->elts; i > 0; --i, v++) {
	*v = zero_int;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate_float", kf_allocate_float, pt_allocate_float, 0)
# else
char pt_allocate_float[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
			     T_FLOAT | (1 << REFSHIFT), T_INT };

/*
 * NAME:	kfun->allocate_float()
 * DESCRIPTION:	allocate an array
 */
int kf_allocate_float(f)
register frame *f;
{
    register int i;
    register value *v;

    if (f->sp->u.number < 0) {
	return 1;
    }
    i_add_ticks(f, f->sp->u.number);
    PUT_ARRVAL(f->sp, arr_new(f->data, (long) f->sp->u.number));
    for (i = f->sp->u.array->size, v = f->sp->u.array->elts; i > 0; --i, v++) {
	*v = zero_float;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sizeof", kf_sizeof, pt_sizeof, 0)
# else
char pt_sizeof[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT,
		     T_MIXED | (1 << REFSHIFT) };

/*
 * NAME:	kfun->sizeof()
 * DESCRIPTION:	return the size of an array
 */
int kf_sizeof(f)
register frame *f;
{
    unsigned short size;

    size = f->sp->u.array->size;
    arr_del(f->sp->u.array);
    PUT_INTVAL(f->sp, size);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_indices", kf_map_indices, pt_map_indices, 0)
# else
char pt_map_indices[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
			  T_MIXED | (1 << REFSHIFT), T_MAPPING };

/*
 * NAME:	kfun->map_indices()
 * DESCRIPTION:	return the array of mapping indices
 */
int kf_map_indices(f)
register frame *f;
{
    array *a;

    a = map_indices(f->data, f->sp->u.array);
    i_add_ticks(f, f->sp->u.array->size);
    arr_del(f->sp->u.array);
    PUT_ARRVAL(f->sp, a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_values", kf_map_values, pt_map_values, 0)
# else
char pt_map_values[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
			 T_MIXED | (1 << REFSHIFT), T_MAPPING };

/*
 * NAME:	kfun->map_values()
 * DESCRIPTION:	return the array of mapping values
 */
int kf_map_values(f)
register frame *f;
{
    array *a;

    a = map_values(f->data, f->sp->u.array);
    i_add_ticks(f, f->sp->u.array->size);
    arr_del(f->sp->u.array);
    PUT_ARRVAL(f->sp, a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_sizeof", kf_map_sizeof, pt_map_sizeof, 0)
# else
char pt_map_sizeof[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT,
			 T_MAPPING };

/*
 * NAME:	kfun->map_sizeof()
 * DESCRIPTION:	return the number of index/value pairs in a mapping
 */
int kf_map_sizeof(f)
register frame *f;
{
    unsigned short size;

    i_add_ticks(f, f->sp->u.array->size);
    size = map_size(f->data, f->sp->u.array);
    arr_del(f->sp->u.array);
    PUT_INTVAL(f->sp, size);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("typeof", kf_typeof, pt_typeof, 0)
# else
char pt_typeof[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * NAME:	kfun->typeof()
 * DESCRIPTION:	return the type of a value
 */
int kf_typeof(f)
register frame *f;
{
    i_del_value(f->sp);
    PUT_INTVAL(f->sp, (f->sp->type == T_LWOBJECT) ? T_OBJECT : f->sp->type);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("error", kf_error, pt_error, 0)
# else
char pt_error[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID, T_STRING };

/*
 * NAME:	kfun->error()
 * DESCRIPTION:	cause an error
 */
int kf_error(f)
frame *f;
{
    serror(f->sp->u.string);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("send_message", kf_send_message, pt_send_message, 0)
# else
char pt_send_message[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * NAME:	kfun->send_message()
 * DESCRIPTION:	send a message to a user
 */
int kf_send_message(f)
register frame *f;
{
    register object *obj;
    int num;

    if (f->sp->type != T_STRING && f->sp->type != T_INT) {
	return 1;
    }

    num = 0;
    if (f->lwobj == (array *) NULL) {
	obj = OBJR(f->oindex);
	if (obj->count != 0) {
	    if ((obj->flags & O_SPECIAL) == O_USER) {
		if (f->sp->type == T_INT) {
		    num = comm_echo(obj, f->sp->u.number != 0);
		} else {
		    num = comm_send(OBJW(obj->index), f->sp->u.string);
		}
	    } else if ((obj->flags & O_DRIVER) && f->sp->type == T_STRING) {
		P_message(f->sp->u.string->text);
		num = f->sp->u.string->len;
	    }
	}
    }
    if (f->sp->type == T_STRING) {
	str_del(f->sp->u.string);
    }
    PUT_INTVAL(f->sp, num);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("send_datagram", kf_send_datagram, pt_send_datagram, 0)
# else
char pt_send_datagram[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT,
			    T_STRING };

/*
 * NAME:	kfun->send_datagram()
 * DESCRIPTION:	send a datagram to a user
 */
int kf_send_datagram(f)
register frame *f;
{
    object *obj;
    int num;

    num = 0;
    if (f->lwobj == (array *) NULL) {
	obj = OBJW(f->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER && obj->count != 0) {
	    num = comm_udpsend(obj, f->sp->u.string);
	}
    }
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, num);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("datagram_challenge", kf_datagram_challenge, pt_datagram_challenge, 0)
# else
char pt_datagram_challenge[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID,
				 T_STRING };

/*
 * NAME:	kfun->datagram_challenge()
 * DESCRIPTION:	set the challenge for a datagram connection to attach
 */
int kf_datagram_challenge(f)
register frame *f;
{
    object *obj;

    if (f->lwobj == (array *) NULL) {
	obj = OBJW(f->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER && obj->count != 0) {
	    comm_challenge(obj, f->sp->u.string);
	}
    }
    str_del(f->sp->u.string);
    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("block_input", kf_block_input, pt_block_input, 0)
# else
char pt_block_input[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID, T_INT };

/*
 * NAME:	kfun->block_input()
 * DESCRIPTION:	block input for the current object
 */
int kf_block_input(f)
register frame *f;
{
    object *obj;

    if (f->lwobj == (array *) NULL) {
	obj = OBJR(f->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER && obj->count != 0) {
	    comm_block(obj, f->sp->u.number != 0);
	}
    }
    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("time", kf_time, pt_time, 0)
# else
char pt_time[] = { C_STATIC, 0, 0, 0, 6, T_INT };

/*
 * NAME:	kfun->time()
 * DESCRIPTION:	return the current time
 */
int kf_time(f)
frame *f;
{
    PUSH_INTVAL(f, P_time());
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("millitime", kf_millitime, pt_millitime, 0)
# else
char pt_millitime[] = { C_STATIC, 0, 0, 0, 6, T_MIXED | (1 << REFSHIFT) };

/*
 * NAME:	kfun->millitime()
 * DESCRIPTION:	return the current time in milliseconds
 */
int kf_millitime(f)
frame *f;
{
    array *a;
    unsigned short milli;
    xfloat flt;

    i_add_ticks(f, 2);
    a = arr_new(f->data, 2L);
    PUT_INTVAL(&a->elts[0], P_mtime(&milli));
    flt_itof((Int) milli, &flt);
    flt_mult(&flt, &thousandth);
    PUT_FLTVAL(&a->elts[1], flt);
    PUSH_ARRVAL(f, a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_out", kf_call_out, pt_call_out, 0)
# else
char pt_call_out[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 2, 1, 0, 9, T_INT,
		       T_STRING, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->call_out()
 * DESCRIPTION:	start a call_out
 */
int kf_call_out(f, nargs)
register frame *f;
int nargs;
{
    Int delay;
    unsigned short mdelay;
    xfloat flt1, flt2;
    uindex handle;

    if (f->sp[nargs - 2].type == T_INT) {
	delay = f->sp[nargs - 2].u.number;
	if (delay < 0) {
	    /* delay less than 0 */
	    return 2;
	}
	mdelay = 0xffff;
    } else if (f->sp[nargs - 2].type == T_FLOAT) {
	GET_FLT(&f->sp[nargs - 2], flt1);
	if (FLT_ISNEG(flt1.high, flt1.low) || flt_cmp(&flt1, &max_int) > 0) {
	    /* delay < 0.0 or delay > MAX_INT */
	    return 2;
	}
	flt_modf(&flt1, &flt2);
	delay = flt_ftoi(&flt2);
	flt_mult(&flt1, &thousand);
	mdelay = flt_ftoi(&flt1);
    } else {
	return 2;
    }
    if (f->lwobj != (array *) NULL) {
	error("call_out() in non-persistent object");
    }

    i_add_ticks(f, nargs);
    if (OBJR(f->oindex)->count != 0 &&
	(handle=d_new_call_out(f->data, f->sp[nargs - 1].u.string, delay,
			       mdelay, f, nargs - 2)) != 0) {
	/* pop duration */
	f->sp++;
    } else {
	/* no call_out was started: pop all arguments */
	i_pop(f, nargs - 1);
	handle = 0;
    }
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, handle);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("remove_call_out", kf_remove_call_out, pt_remove_call_out, 0)
# else
char pt_remove_call_out[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_MIXED,
			      T_INT };

/*
 * NAME:	kfun->remove_call_out()
 * DESCRIPTION:	remove a call_out
 */
int kf_remove_call_out(f)
register frame *f;
{
    Int delay;
    unsigned short mdelay;
    xfloat flt1, flt2;

    if (f->lwobj != (array *) NULL) {
	error("remove_call_out() in non-persistent object");
    }
    i_add_ticks(f, 10);
    delay = d_del_call_out(f->data, (Uint) f->sp->u.number, &mdelay);
    if (mdelay != 0xffff) {
	flt_itof(delay, &flt1);
	flt_itof(mdelay, &flt2);
	flt_mult(&flt2, &thousandth);
	flt_add(&flt1, &flt2);
	PUT_FLTVAL(f->sp, flt1);
    } else {
	PUT_INT(f->sp, delay);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("swapout", kf_swapout, pt_swapout, 0)
# else
char pt_swapout[] = { C_STATIC, 0, 0, 0, 6, T_VOID };

/*
 * NAME:	kfun->swapout()
 * DESCRIPTION:	swap out all objects
 */
int kf_swapout(f)
frame *f;
{
    swapout();

    *--f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.dump_state", kf_old_dump_state, pt_old_dump_state, 0)
# else
char pt_old_dump_state[] = { C_STATIC, 0, 0, 0, 6, T_VOID };

/*
 * NAME:	kfun->old_dump_state()
 * DESCRIPTION:	dump state
 */
int kf_old_dump_state(f)
frame *f;
{
    dump_state();

    *--f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("dump_state", kf_dump_state, pt_dump_state, 1)
# else
char pt_dump_state[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7, T_VOID, T_INT };

/*
 * NAME:	kfun->dump_state()
 * DESCRIPTION:	dump state
 */
int kf_dump_state(f, nargs)
frame *f;
int nargs;
{
    if (nargs != 0) {
	f->sp++;
    }
    dump_state();

    *--f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("shutdown", kf_shutdown, pt_shutdown, 0)
# else
char pt_shutdown[] = { C_STATIC, 0, 0, 0, 6, T_VOID };

/*
 * NAME:	kfun->shutdown()
 * DESCRIPTION:	shut down the mud
 */
int kf_shutdown(f)
frame *f;
{
    finish();

    *--f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_status, pt_status, 0)
# else
char pt_status[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7,
		     T_MIXED | (1 << REFSHIFT), T_OBJECT };

/*
 * NAME:	kfun->status()
 * DESCRIPTION:	return an array with status information about the gamedriver
 *		or an object
 */
int kf_status(f, nargs)
register frame *f;
int nargs;
{
    array *a;
    uindex n;

    i_add_ticks(f, 100);
    if (nargs == 0) {
	a = conf_status(f);
	--f->sp;
    } else {
	if (f->sp->type == T_OBJECT) {
	    n = f->sp->oindex;
	} else {
	    n = f->sp->u.array->elts[0].oindex;
	    arr_del(f->sp->u.array);
	}
	a = conf_object(f->data, OBJR(n));
    }
    PUT_ARRVAL(f->sp, a);
    return 0;
}
# endif
