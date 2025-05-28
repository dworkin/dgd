/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2025 DGD Authors (see the commit log for details)
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
# include "compile.h"
# endif


# ifdef FUNCDEF
FUNCDEF("0.compile_object", kf_unused, pt_unused, 0)
FUNCDEF("compile_object", kf_compile_object, pt_compile_object, 1)
# else
char pt_compile_object[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 1, 1, 0, 8,
			     T_OBJECT, T_STRING, T_STRING };

/*
 * compile an object
 */
int kf_compile_object(Frame *f, int nargs, KFun *kf)
{
    char file[STRINGSZ];
    Value *v;
    Object *obj;
    bool iflag;

    UNREFERENCED_PARAMETER(kf);

    v = &f->sp[nargs - 1];
    if (PM->string(file, v->string->text, v->string->len) == (char *) NULL) {
	return 1;
    }
    obj = Object::find(file, OACC_MODIFY);
    if (obj != (Object *) NULL) {
	if (!(obj->flags & O_MASTER)) {
	    EC->error("Cannot recompile cloned object");
	}
	if (O_UPGRADING(obj)) {
	    EC->error("Object is already being upgraded");
	}
	if (O_INHERITED(obj)) {
	    EC->error("Cannot recompile inherited object");
	}
    }
    try {
	EC->push();
	if (OBJR(f->oindex)->flags & O_DRIVER) {
	    Frame *xf;

	    for (xf = f; !xf->external; xf = xf->prev) ;
	    iflag = (strcmp(xf->p_ctrl->strconst(xf->func->inherit,
						 xf->func->index)->text,
			    "inherit_program") == 0);
	} else {
	    iflag = FALSE;
	}
	obj = Compile::compile(f, file, obj, --nargs, iflag);
	EC->pop();
    } catch (const char*) {
	EC->error((char *) NULL);
    }
    f->pop(nargs);
    f->sp->string->del();
    PUT_OBJVAL(f->sp, obj);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_touch", kf_call_touch, pt_call_touch, 0)
# else
char pt_call_touch[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT,
			 T_OBJECT };

/*
 * prepare to call a function when this object is next touched
 */
int kf_call_touch(Frame *f, int n, KFun *kf)
{
    Object *obj;
    Float flt;
    Value val, *elts;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type == T_LWOBJECT) {
	elts = Dataspace::elts(f->sp->array);
	GET_FLT(&elts[1], flt);
	flt.high = TRUE;
	PUT_FLTVAL(&val, flt);
	f->data->assignElt(f->sp->array, &elts[1], &val);
	f->sp->array->del();
	PUT_INTVAL(f->sp, TRUE);
    } else {
	obj = OBJW(f->sp->oindex);
	obj->flags &= ~O_TOUCHED;
	PUT_INTVAL(f->sp, O_HASDATA(obj));
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_object", kf_this_object, pt_this_object, 0)
# else
char pt_this_object[] = { C_STATIC, 0, 0, 0, 6, T_OBJECT };

/*
 * return the current object
 */
int kf_this_object(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    --f->sp;
    obj = OBJR(f->oindex);
    if (obj->count != 0) {
	if (f->lwobj == (LWO *) NULL) {
	    PUT_OBJVAL(f->sp, obj);
	} else {
	    PUT_LWOVAL(f->sp, f->lwobj);
	}
    } else {
	*f->sp = nil;
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
 * return the previous object in the call_other chain
 */
int kf_previous_object(Frame *f, int nargs, KFun *kf)
{
    Frame *prev;
    Object *obj;

    UNREFERENCED_PARAMETER(kf);

    if (nargs == 0) {
	*--f->sp = nil;
    } else if (f->sp->number < 0) {
	return 1;
    }

    prev = f->prevObject((int) f->sp->number);
    if (prev != (Frame *) NULL) {
	obj = OBJR(prev->oindex);
	if (obj->count != 0) {
	    if (prev->lwobj == (LWO *) NULL) {
		PUT_OBJVAL(f->sp, obj);
	    } else {
		PUT_LWOVAL(f->sp, prev->lwobj);
	    }
	    return 0;
	}
    }

    *f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_program", kf_previous_program, pt_previous_program, 0)
# else
char pt_previous_program[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7, T_STRING,
			       T_INT };

/*
 * return the previous program in the function call chain
 */
int kf_previous_program(Frame *f, int nargs, KFun *kf)
{
    const char *prog;
    String *str;

    UNREFERENCED_PARAMETER(kf);

    if (nargs == 0) {
	*--f->sp = nil;
    } else if (f->sp->number < 0) {
	return 1;
    }

    prog = f->prevProgram((int) f->sp->number);
    if (prog != (char *) NULL) {
	PUT_STRVAL(f->sp,
		   str = String::create((char *) NULL, strlen(prog) + 1L));
	str->text[0] = '/';
	strcpy(str->text + 1, prog);
    } else {
	*f->sp = nil;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("clone_object", kf_clone_object, pt_clone_object, 0)
# else
char pt_clone_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_OBJECT,
			   T_OBJECT };

/*
 * clone a new object
 */
int kf_clone_object(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type == T_LWOBJECT) {
	EC->error("Cloning from a non-persistent object");
    }
    obj = OBJW(f->sp->oindex);
    if (!(obj->flags & O_MASTER)) {
	EC->error("Cloning from a clone");
    }
    obj = obj->clone();
    PUT_OBJ(f->sp, obj);
    if (f->call(obj, (LWO *) NULL, (char *) NULL, 0, TRUE, 0)) {
	(f->sp++)->del();
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
 * destruct an object
 */
int kf_destruct_object(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type == T_LWOBJECT) {
	EC->error("Destructing a non-persistent object");
    }
    obj = OBJW(f->sp->oindex);
    switch (obj->flags & O_SPECIAL) {
    case O_USER:
	Comm::close(f, obj);
	break;

    case O_EDITOR:
	if (f->level != 0) {
	    EC->error("Destructing editor object in atomic function");
	}
	Editor::del(obj);
	break;
    }
    obj->del(f);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("new_object", kf_new_object, pt_new_object, 0)
# else
char pt_new_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_OBJECT,
			 T_OBJECT };

/*
 * create a new non-persistent object
 */
int kf_new_object(Frame *f, int n, KFun *kf)
{
    Object *obj;
    LWO *a;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type == T_OBJECT) {
	if (!((obj=OBJW(f->sp->oindex))->flags & O_MASTER)) {
	    EC->error("Creating new instance from a non-master object");
	}

	PUT_LWOVAL(f->sp, LWO::create(f->data, obj));
	if (f->call((Object *) NULL, dynamic_cast<LWO *> (f->sp->array),
		    (char *) NULL, 0, TRUE, 0)) {
	    (f->sp++)->del();
	}
    } else {
	a = dynamic_cast<LWO *> (f->sp->array);
	a = a->copy(f->data);
	f->sp->array->del();
	PUT_LWOVAL(f->sp, a);
    }
    return 0;
}
# endif


 # ifdef FUNCDEF
FUNCDEF("instanceof", kf_instanceof, pt_instanceof, 0)
# else
char pt_instanceof[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_INT, T_OBJECT,
			 T_STRING };

/*
 * check whether an object is an instance of a type
 */
int kf_instanceof(Frame *f, int nargs, KFun *kf)
{
    char buffer[STRINGSZ + 12];
    uindex oindex;
    const char *builtin, *name;
    String *str;
    int instance;

    UNREFERENCED_PARAMETER(nargs);
    UNREFERENCED_PARAMETER(kf);

    builtin = (char *) NULL;
    if (f->sp[1].type == T_OBJECT) {
	oindex = f->sp[1].oindex;
    } else if (f->sp[1].array->elts[0].type != T_OBJECT) {
	builtin = Object::builtinName(f->sp[1].array->elts[0].number);
    } else {
	oindex = f->sp[1].array->elts[0].oindex;
	f->sp[1].array->del();
    }
    if (f->lwobj == (LWO *) NULL) {
	name = OBJR(f->oindex)->objName(buffer);
	PUT_STRVAL(f->sp + 1,
		   str = String::create((char *) NULL, strlen(name) + 1L));
	str->text[0] = '/';
	strcpy(str->text + 1, name);
    } else {
	name = OBJR(f->lwobj->elts[0].oindex)->objName(buffer);
	PUT_STRVAL(f->sp + 1,
		   str = String::create((char *) NULL, strlen(name) + 4L));
	strcpy(str->text + 1, name);
	strcpy(str->text + str->len - 3, "#-1");
    }
    DGD::callDriver(f, "object_type", 2);
    if (f->sp->type != T_STRING) {
	EC->error("Invalid object type");
    }
    PM->resolve(buffer, f->sp->string->text);
    if (builtin != (char *) NULL) {
	instance = (strcmp(builtin, buffer) == 0);
    } else {
	instance = Frame::instanceOf(oindex, buffer);
    }
    f->sp->string->del();
    PUT_INTVAL(f->sp, instance);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("object_name", kf_object_name, pt_object_name, 0)
# else
char pt_object_name[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_STRING,
			  T_OBJECT };

/*
 * return the name of an object
 */
int kf_object_name(Frame *f, int nargs, KFun *kf)
{
    char buffer[STRINGSZ + 12];
    const char *name;
    String *str;
    uindex n;

    UNREFERENCED_PARAMETER(nargs);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type == T_OBJECT) {
	name = OBJR(f->sp->oindex)->objName(buffer);
	PUT_STRVAL(f->sp,
		   str = String::create((char *) NULL, strlen(name) + 1L));
	str->text[0] = '/';
	strcpy(str->text + 1, name);
    } else {
	if (f->sp->array->elts[0].type == T_OBJECT) {
	    /* ordinary light-weight object */
	    n = f->sp->array->elts[0].oindex;
	    f->sp->array->del();
	    name = OBJR(n)->objName(buffer);
	} else {
	    /* builtin type */
	    name = Object::builtinName(f->sp->array->elts[0].number);
	}
	PUT_STRVAL(f->sp,
		   str = String::create((char *) NULL, strlen(name) + 4L));
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
 * find the loaded object for a given object name
 */
int kf_find_object(Frame *f, int n, KFun *kf)
{
    char path[STRINGSZ];
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (PM->string(path, f->sp->string->text,
		   f->sp->string->len) == (char *) NULL) {
	f->sp->string->del();
	*f->sp = nil;
	return 0;
    }
    f->addTicks(2);
    obj = Object::find(path, OACC_READ);
    f->sp->string->del();
    if (obj != (Object *) NULL) {
	PUT_OBJVAL(f->sp, obj);
    } else {
	*f->sp = nil;
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
 * return the name of the program a function is in
 */
int kf_function_object(Frame *f, int nargs, KFun *kf)
{
    Object *obj;
    bool callable;
    uindex n;
    Symbol *symb;
    const char *name;

    UNREFERENCED_PARAMETER(nargs);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    if (f->sp->type == T_OBJECT) {
	obj = OBJR(f->sp->oindex);
	callable = (f->oindex == obj->index && f->lwobj == (LWO *) NULL);
    } else if (f->sp->array->elts[0].type == T_OBJECT) {
	n = f->sp->array->elts[0].oindex;
	callable = (f->lwobj == f->sp->array);
	f->sp->array->del();
	obj = OBJR(n);
    } else {
	/* no user-probeable functions within (right?) */
	(f->sp++)->array->del();
	f->sp->string->del();
	*f->sp = nil;
	return 0;
    }
    f->sp++;
    symb = obj->control()->symb(f->sp->string->text, f->sp->string->len);
    f->sp->string->del();

    if (symb != (Symbol *) NULL) {
	Object *o;

	o = OBJR(obj->ctrl->inherits[UCHAR(symb->inherit)].oindex);
	if (!(o->ctrl->funcs()[UCHAR(symb->index)].sclass & C_STATIC) ||
	    callable) {
	    /*
	     * function exists and is callable
	     */
	    name = o->name;
	    PUT_STR(f->sp,
		    String::create((char *) NULL, strlen(name) + 1L));
	    f->sp->string->text[0] = '/';
	    strcpy(f->sp->string->text + 1, name);
	    return 0;
	}
    }
    *f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_user", kf_this_user, pt_this_user, 0)
# else
char pt_this_user[] = { C_STATIC, 0, 0, 0, 6, T_OBJECT };

/*
 * return the current user object (if any)
 */
int kf_this_user(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    obj = Comm::user();
    if (obj != (Object *) NULL) {
	PUSH_OBJVAL(f, obj);
    } else {
	*--f->sp = nil;
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
 * return the ip number of a user
 */
int kf_query_ip_number(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type == T_OBJECT) {
	obj = OBJR(f->sp->oindex);
	if (Comm::isConnection(obj)) {
	    PUT_STRVAL(f->sp, Comm::ipNumber(obj));
	    return 0;
	}
    } else {
	f->sp->array->del();
    }

    *f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("query_ip_name", kf_query_ip_name, pt_query_ip_name, 0)
# else
char pt_query_ip_name[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_STRING,
			    T_OBJECT };

/*
 * return the ip name of a user
 */
int kf_query_ip_name(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type == T_OBJECT) {
	obj = OBJR(f->sp->oindex);
	if (Comm::isConnection(obj)) {
	    PUT_STRVAL(f->sp, Comm::ipName(obj));
	    return 0;
	}
    } else {
	f->sp->array->del();
    }

    *f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("users", kf_users, pt_users, 0)
# else
char pt_users[] = { C_STATIC, 0, 0, 0, 6, T_OBJECT | (1 << REFSHIFT) };

/*
 * return the array of users
 */
int kf_users(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUSH_ARRVAL(f, Comm::listUsers(f->data));
    f->addTicks(f->sp->array->size);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate", kf_allocate, pt_allocate, 0)
# else
char pt_allocate[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
		       T_MIXED | (1 << REFSHIFT), T_INT };

/*
 * allocate an array
 */
int kf_allocate(Frame *f, int n, KFun *kf)
{
    int i;
    Value *v;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->number < 0) {
	return 1;
    }
    f->addTicks(f->sp->number);
    PUT_ARRVAL(f->sp, Array::create(f->data, f->sp->number));
    for (i = f->sp->array->size, v = f->sp->array->elts; i > 0; --i, v++) {
	*v = nil;
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
 * allocate an array of integers
 */
int kf_allocate_int(Frame *f, int n, KFun *kf)
{
    int i;
    Value *v;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->number < 0) {
	return 1;
    }
    f->addTicks(f->sp->number);
    PUT_ARRVAL(f->sp, Array::create(f->data, f->sp->number));
    for (i = f->sp->array->size, v = f->sp->array->elts; i > 0; --i, v++) {
	*v = zeroInt;
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
 * allocate an array
 */
int kf_allocate_float(Frame *f, int n, KFun *kf)
{
    int i;
    Value *v;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->number < 0) {
	return 1;
    }
    f->addTicks(f->sp->number);
    PUT_ARRVAL(f->sp, Array::create(f->data, f->sp->number));
    for (i = f->sp->array->size, v = f->sp->array->elts; i > 0; --i, v++) {
	*v = zeroFloat;
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
 * return the size of an array
 */
int kf_sizeof(Frame *f, int n, KFun *kf)
{
    unsigned short size;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    size = f->sp->array->size;
    f->sp->array->del();
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
 * return the array of mapping indices
 */
int kf_map_indices(Frame *f, int n, KFun *kf)
{
    Array *a;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    a = dynamic_cast<Mapping *> (f->sp->array)->indices(f->data);
    f->addTicks(f->sp->array->size);
    f->sp->array->del();
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
 * return the array of mapping values
 */
int kf_map_values(Frame *f, int n, KFun *kf)
{
    Array *a;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    a = dynamic_cast<Mapping *> (f->sp->array)->values(f->data);
    f->addTicks(f->sp->array->size);
    f->sp->array->del();
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
 * return the number of index/value pairs in a mapping
 */
int kf_map_sizeof(Frame *f, int n, KFun *kf)
{
    unsigned short size;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(f->sp->array->size);
    size = dynamic_cast<Mapping *> (f->sp->array)->msize(f->data);
    f->sp->array->del();
    PUT_INTVAL(f->sp, size);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("typeof", kf_typeof, pt_typeof, 0)
# else
char pt_typeof[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * return the type of a value
 */
int kf_typeof(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->sp->del();
    PUT_INTVAL(f->sp, (f->sp->type == T_LWOBJECT) ? T_OBJECT : f->sp->type);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("error", kf_error, pt_error, 0)
# else
char pt_error[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID, T_STRING };

/*
 * cause an error
 */
int kf_error(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    EC->error(f->sp->string);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("send_message", kf_send_message, pt_send_message, 0)
# else
char pt_send_message[] = { C_STATIC, 1, 0, 0, 7, T_INT, T_MIXED };

/*
 * send a message to a user
 */
int kf_send_message(Frame *f, int n, KFun *kf)
{
    Object *obj;
    int num;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->sp->type != T_STRING && f->sp->type != T_INT) {
	return 1;
    }

    num = 0;
    if (f->lwobj == (LWO *) NULL) {
	obj = OBJR(f->oindex);
	if (obj->count != 0) {
	    if ((obj->flags & O_SPECIAL) == O_USER) {
		if (f->sp->type == T_INT) {
		    num = Comm::echo(obj, f->sp->number != 0);
		} else {
		    num = Comm::send(OBJW(obj->index), f->sp->string);
		}
	    } else if ((obj->flags & O_DRIVER) && f->sp->type == T_STRING) {
		P_message(f->sp->string->text);
		num = f->sp->string->len;
	    }
	}
    }
    if (f->sp->type == T_STRING) {
	f->sp->string->del();
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
 * send a datagram to a user (non networkpackage function)
 */
int kf_send_datagram(Frame *f, int n, KFun *kf)
{
    Object *obj;
    int num;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    num = 0;
    if (f->lwobj == (LWO *) NULL) {
	obj = OBJW(f->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER && obj->count != 0) {
	    num = Comm::udpsend(obj, f->sp->string);
	}
    }
    f->sp->string->del();
    PUT_INTVAL(f->sp, num);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("send_close", kf_send_close, pt_send_close, 0)
# else
char pt_send_close[] = { C_STATIC, 0, 0, 0, 6, T_VOID };

/*
 * close output stream
 */
int kf_send_close(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->lwobj == (LWO *) NULL) {
	obj = OBJW(f->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER && obj->count != 0) {
	    Comm::stop(obj);
	}
    }
    *--f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("datagram_challenge", kf_datagram_challenge, pt_datagram_challenge, 0)
# else
char pt_datagram_challenge[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID,
				 T_STRING };

/*
 * set the challenge for a datagram connection to attach
 */
int kf_datagram_challenge(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->lwobj == (LWO *) NULL) {
	obj = OBJW(f->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER && obj->count != 0) {
	    Comm::challenge(obj, f->sp->string);
	}
    }
    f->sp->string->del();
    *f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("block_input", kf_block_input, pt_block_input, 0)
# else
char pt_block_input[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID, T_INT };

/*
 * block input for the current object
 */
int kf_block_input(Frame *f, int n, KFun *kf)
{
    Object *obj;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->lwobj == (LWO *) NULL) {
	obj = OBJR(f->oindex);
	if ((obj->flags & O_SPECIAL) == O_USER && obj->count != 0) {
	    Comm::block(obj, f->sp->number != 0);
	}
    }
    *f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("time", kf_time, pt_time, 0)
# else
char pt_time[] = { C_STATIC, 0, 0, 0, 6, T_INT };

/*
 * return the current time
 */
int kf_time(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    PUSH_INTVAL(f, P_time());
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("millitime", kf_millitime, pt_millitime, 0)
# else
char pt_millitime[] = { C_STATIC, 0, 0, 0, 6, T_MIXED | (1 << REFSHIFT) };

/*
 * return the current time in milliseconds
 */
int kf_millitime(Frame *f, int n, KFun *kf)
{
    Array *a;
    unsigned short milli;
    Float flt;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    f->addTicks(2);
    a = Array::create(f->data, 2);
    PUT_INTVAL(&a->elts[0], P_mtime(&milli));
    Float::itof(milli, &flt);
    flt.div(thousand);
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
 * start a call_out
 */
int kf_call_out(Frame *f, int nargs, KFun *kf)
{
    LPCint delay;
    Uint mdelay;
    Float flt1, flt2;
    uindex handle;

    UNREFERENCED_PARAMETER(kf);

    if (f->sp[nargs - 2].type == T_INT) {
	delay = f->sp[nargs - 2].number;
	if (delay < 0) {
	    /* delay less than 0 */
	    return 2;
	}
	mdelay = TIME_INT;
    } else if (f->sp[nargs - 2].type == T_FLOAT) {
	GET_FLT(&f->sp[nargs - 2], flt1);
	if (flt1.negative() || flt1.cmp(max_int) > 0) {
	    /* delay < 0.0 or delay > MAX_INT */
	    return 2;
	}
	flt1.modf(&flt2);
	delay = flt2.ftoi();
	flt1.mult(thousand);
	mdelay = flt1.ftoi();
    } else {
	return 2;
    }
    if (f->lwobj != (LWO *) NULL) {
	EC->error("call_out() in non-persistent object");
    }

    f->addTicks(nargs);
    if (OBJR(f->oindex)->count != 0 &&
	(handle=f->data->newCallOut(f->sp[nargs - 1].string, delay,
				    mdelay, f, nargs - 2, FALSE)) != 0) {
	/* pop duration */
	f->sp++;
    } else {
	/* no call_out was started: pop all arguments */
	f->pop(nargs - 1);
	handle = 0;
    }
    f->sp->string->del();
    PUT_INTVAL(f->sp, handle);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_out_summand", kf_call_out_summand, pt_call_out_summand, 0)
# else
char pt_call_out_summand[] = { C_TYPECHECKED | C_STATIC | C_ELLIPSIS, 3, 1, 0,
			       10, T_INT, T_STRING, T_MIXED, T_FLOAT, T_MIXED };

/*
 * start a summand call_out
 */
int kf_call_out_summand(Frame *f, int nargs, KFun *kf)
{
    LPCint delay;
    Uint mdelay;
    Float flt1, flt2;
    bool result;

    UNREFERENCED_PARAMETER(kf);

    if (f->sp[nargs - 2].type == T_INT) {
	delay = f->sp[nargs - 2].number;
	if (delay < 0) {
	    /* delay less than 0 */
	    return 2;
	}
	mdelay = TIME_INT;
    } else if (f->sp[nargs - 2].type == T_FLOAT) {
	GET_FLT(&f->sp[nargs - 2], flt1);
	if (flt1.negative() || flt1.cmp(max_int) > 0) {
	    /* delay < 0.0 or delay > MAX_INT */
	    return 2;
	}
	flt1.modf(&flt2);
	delay = flt2.ftoi();
	flt1.mult(thousand);
	mdelay = flt1.ftoi();
    } else {
	return 2;
    }
    if (f->lwobj != (LWO *) NULL) {
	EC->error("call_out_summand() in non-persistent object");
    }

    f->addTicks(nargs);
    if (OBJR(f->oindex)->count != 0 &&
	f->data->newCallOut(f->sp[nargs - 1].string, delay, mdelay, f,
			    nargs - 2, TRUE) != 0) {
	/* pop duration */
	f->sp++;
	result = TRUE;
    } else {
	/* no call_out was started: pop all arguments */
	f->pop(nargs - 1);
	result = FALSE;
    }
    f->sp->string->del();
    PUT_INTVAL(f->sp, result);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("remove_call_out", kf_remove_call_out, pt_remove_call_out, 0)
# else
char pt_remove_call_out[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_MIXED,
			      T_INT };

/*
 * remove a call_out
 */
int kf_remove_call_out(Frame *f, int n, KFun *kf)
{
    LPCint delay;
    unsigned short mdelay;
    Float flt1, flt2;

    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    if (f->lwobj != (LWO *) NULL) {
	EC->error("remove_call_out() in non-persistent object");
    }
    f->addTicks(10);
    delay = f->data->delCallOut((Uint) f->sp->number, &mdelay);
    if (mdelay != TIME_INT) {
	Float::itof(delay, &flt1);
	Float::itof(mdelay, &flt2);
	flt2.div(thousand);
	flt1.add(flt2);
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
 * swap out all objects
 */
int kf_swapout(Frame *f, int n, KFun *kf)
{
    UNREFERENCED_PARAMETER(n);
    UNREFERENCED_PARAMETER(kf);

    Object::swapout();

    *--f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.dump_state", kf_unused, pt_unused, 0)
FUNCDEF("dump_state", kf_dump_state, pt_dump_state, 1)
# else
char pt_dump_state[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7, T_VOID, T_INT };

/*
 * dump state
 */
int kf_dump_state(Frame *f, int nargs, KFun *kf)
{
    bool incr;

    UNREFERENCED_PARAMETER(kf);

    if (nargs == 0) {
	incr = FALSE;
	--f->sp;
    } else {
	incr = (f->sp->number != 0);
    }
    *f->sp = nil;

    Object::dumpState(incr);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("connect", kf_connect, pt_connect, 1)
# else
char pt_connect[] = { C_TYPECHECKED | C_STATIC , 2, 1, 0, 9,
		      T_VOID, T_STRING, T_INT, T_STRING };

/*
 * connect to a server
 */
int kf_connect(Frame *f, int nargs, KFun *kf)
{
    unsigned short port;
    Object *obj;

    UNREFERENCED_PARAMETER(nargs);
    UNREFERENCED_PARAMETER(kf);

    if (nargs > 2) {
	f->sp->string->del();
	f->sp++;
    }
    if (f->lwobj != (LWO *) NULL) {
	EC->error("connect() in non-persistent object");
    }
    obj = OBJW(f->oindex);

    if (obj->count == 0) {
	EC->error("connect() in destructed object");
    }

    if (obj->flags & O_SPECIAL) {
	EC->error("connect() in special purpose object");
    }

    if (f->sp->number < 1 || f->sp->number > 65535) {
	EC->error("Port number out of range");
    }
    port = (f->sp++)->number;

    Comm::connect(f, obj, f->sp->string->text, port);
    f->sp->string->del();
    *f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.connect", kf_old_connect, pt_old_connect, 0)
# else
char pt_old_connect[] = { C_TYPECHECKED | C_STATIC , 2, 0, 0, 8,
			  T_VOID, T_STRING, T_INT };

/*
 * connect to a server
 */
int kf_old_connect(Frame *f, int nargs, KFun *kf)
{
    return kf_connect(f, nargs, kf);
}
# endif


# ifdef FUNCDEF
FUNCDEF("connect_datagram", kf_connect_datagram, pt_connect_datagram, 0)
# else
char pt_connect_datagram[] = { C_TYPECHECKED | C_STATIC , 3, 1, 0, 10,
			       T_VOID, T_INT, T_STRING, T_INT, T_STRING };

/*
 * connect by datagrams to a server
 */
int kf_connect_datagram(Frame *f, int nargs, KFun *kf)
{
    unsigned short port;
    Object *obj;

    UNREFERENCED_PARAMETER(nargs);
    UNREFERENCED_PARAMETER(kf);

    if (nargs > 3) {
	(f->sp++)->string->del();
    }
    if (f->lwobj != (LWO *) NULL) {
	EC->error("connect_datagram() in non-persistent object");
    }
    obj = OBJW(f->oindex);

    if (obj->count == 0) {
	EC->error("connect_datagram() in destructed object");
    }

    if (obj->flags & O_SPECIAL) {
	EC->error("connect_datagram() in special purpose object");
    }

    if (f->sp->number < 1 || f->sp->number > 65535) {
	EC->error("Port number out of range");
    }
    port = (f->sp++)->number;

    Comm::connectDgram(f, obj, f->sp[1].number, f->sp->string->text, port);
    (f->sp++)->string->del();
    *f->sp = nil;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("0.shutdown", kf_unused, pt_unused, 0)
FUNCDEF("shutdown", kf_shutdown, pt_shutdown, 1)
# else
char pt_shutdown[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7, T_VOID, T_INT };

/*
 * shut down the mud
 */
int kf_shutdown(Frame *f, int nargs, KFun *kf)
{
    bool boot;

    UNREFERENCED_PARAMETER(kf);

    if (nargs != 0) {
	boot = (f->sp->number != 0);
	if (boot && Config::hotbootExec() == (char **) NULL) {
	    EC->error("Hotbooting is disabled");
	}
	f->sp++;
    } else {
	boot = FALSE;
    }
    Object::finish(boot);

    *--f->sp = nil;
    return 0;
}
# endif


# ifdef CLOSURES
# ifdef FUNCDEF
FUNCDEF("new.function", kf_new_function, pt_new_function, 0)
# else
char pt_new_function[] = { C_STATIC | C_ELLIPSIS, 1, 1, 0, 8, T_OBJECT,
			   T_STRING, T_MIXED };

/*
 * create a new function
 */
int kf_new_function(Frame *f, int nargs, KFun *kf)
{
    Array *a;
    Float flt;
    Value *v, *elts;
    Object *obj;

    UNREFERENCED_PARAMETER(kf);

    a = Array::create(f->data, 4 + nargs);
    elts = a->elts;

    /* these two fields are required for builtin types */
    PUT_INTVAL(&elts[0], BUILTIN_FUNCTION);
    flt.high = 0;
    flt.low = 0;
    PUT_FLTVAL(&elts[1], flt);

    /* version number: recommended for builtin types */
    PUT_INTVAL(&elts[2], 0);

    /* object, function name, arg1, ..., argn */
    obj = OBJR(f->oindex);
    if (obj->count != 0) {
	if (f->lwobj == (LWO *) NULL) {
	    PUT_OBJVAL(&elts[3], obj);
	} else {
	    PUT_LWOVAL(&elts[3], f->lwobj);
	}
    } else {
	elts[3] = nil;	/* postpone error until function is called */
    }
    v = f->sp;
    elts += a->size;
    do {
	*--elts = *v++;
    } while (--nargs != 0);
    Dataspace::refImports(a);
    f->sp = v;

    PUSH_LWOVAL(f, a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("extend.function", kf_extend_function, pt_extend_function, 0)
# else
char pt_extend_function[] = { C_STATIC | C_ELLIPSIS, 1, 1, 0, 8, T_OBJECT,
			      T_OBJECT, T_MIXED };

/*
 * extend a function
 */
int kf_extend_function(Frame *f, int nargs, KFun *kf)
{
    Array *a;
    Value *v, *elts;
    int n;

    UNREFERENCED_PARAMETER(kf);

    --nargs;
    if (f->sp[nargs].type != T_LWOBJECT ||
	(elts=Dataspace::elts(a=f->sp[nargs].array))[0].type != T_INT ||
	elts[0].number != BUILTIN_FUNCTION) {
	EC->error("Bad argument 1 for kfun *");
    }

    if (nargs != 0) {
	/*
	 * add arguments to function
	 */
	n = a->size;
	a = Array::create(f->data, n + nargs);
	Value::copy(a->elts, elts, n);

	v = f->sp;
	elts = a->elts + a->size;
	do {
	    *--elts = *v++;
	} while (--nargs != 0);
	v->array->del();
	v->array = a;
	v->array->ref();
	f->sp = v;
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call.function", kf_call_function, pt_call_function, 0)
# else
char pt_call_function[] = { C_STATIC | C_ELLIPSIS, 1, 1, 0, 8, T_MIXED,
			    T_OBJECT, T_MIXED };

/*
 * call a function
 */
int kf_call_function(Frame *f, int nargs, KFun *kf)
{
    Array *a;
    Value *elts, *v, *w;
    Object *obj;
    LWO *lwobj;
    int n;

    UNREFERENCED_PARAMETER(kf);

    --nargs;
    if (f->sp[nargs].type != T_LWOBJECT ||
	(elts=Dataspace::elts(a=f->sp[nargs].array))[0].type != T_INT ||
	elts[0].number != BUILTIN_FUNCTION) {
	EC->error("Bad argument 1 for kfun *");
    }

    switch (elts[3].type) {
    case T_OBJECT:
	if (DESTRUCTED(&elts[3])) {
	    EC->error("Function in destructed object");
	}
	obj = OBJR(elts[3].oindex);
	lwobj = NULL;
	break;

    case T_LWOBJECT:
	obj = NULL;
	lwobj = dynamic_cast<LWO *> (elts[3].array);
	v = Dataspace::elts(lwobj);
	if (v->type == T_OBJECT && DESTRUCTED(v)) {
	    EC->error("Function in destructed object");
	}
	break;

    default:
	EC->error("Function in destructed object");
	break;
    }

    /* insert bound arguments on the stack */
    n = a->size - 5;
    if (n != 0) {
	f->growStack(n);
	memmove(f->sp - n, f->sp, nargs * sizeof(Value));
	f->sp -= n;

	v = f->sp + nargs;
	nargs += n;
	w = elts + a->size;
	do {
	    (--w)->ref();
	    *v++ = *w;
	} while (--n != 0);
    }

    if (!f->call(obj, lwobj, elts[4].string->text, elts[4].string->len, TRUE,
		 nargs)) {
	EC->error("Function not found");
    }

    a->del();
    f->sp[1] = f->sp[0];
    f->sp++;
    return 0;
}
# endif
# endif


/*
 * turned into builtin
 */
# ifdef FUNCDEF
FUNCDEF("0.call_other", kf_call_other, pt_call_other, 0)
FUNCDEF("0.call_trace", kf_call_trace, pt_call_trace, 0)
FUNCDEF("0.status", kf_status, pt_status, 0)
FUNCDEF("0.strlen", kf_strlen, pt_strlen, 0)
# endif
