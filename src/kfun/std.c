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
FUNCDEF("compile_object", kf_compile_object, pt_compile_object)
# else
char pt_compile_object[] = { C_TYPECHECKED | C_STATIC, T_OBJECT, 1, T_STRING };

/*
 * NAME:	kfun->compile_object()
 * DESCRIPTION:	compile an object
 */
int kf_compile_object(f)
register frame *f;
{
    char *file;
    register object *obj;

    file = path_string(f->sp->u.string->text, f->sp->u.string->len);
    if (file == (char *) NULL) {
	return 1;
    }
    obj = o_find(file);
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
    obj = c_compile(f, file, obj);
    str_del(f->sp->u.string);
    f->sp->type = T_OBJECT;
    f->sp->oindex = obj->index;
    f->sp->u.objcnt = obj->count;

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_other", kf_call_other, pt_call_other)
# else
char pt_call_other[] = { C_TYPECHECKED | C_STATIC | C_VARARGS, T_MIXED, 3,
			 T_MIXED, T_STRING, T_MIXED | T_ELLIPSIS };

/*
 * NAME:	kfun->call_other()
 * DESCRIPTION:	call a function in another object
 */
int kf_call_other(f, nargs)
register frame *f;
int nargs;
{
    register object *obj;
    register value *val;

    if (nargs < 2) {
	return -1;
    }

    val = &f->sp[nargs - 1];
    switch (val->type) {
    case T_STRING:
	*--f->sp = *val;
	val->type = T_INT;	/* erase old copy */
	call_driver_object(f, "call_object", 1);
	if (f->sp->type != T_OBJECT) {
	    i_del_value(f->sp++);
	    return 1;
	}
	obj = &otable[f->sp->oindex];
	f->sp++;
	break;

    case T_OBJECT:
	obj = &otable[val->oindex];
	break;

    default:
	/* bad arg 1 */
	return 1;
    }

    /* default return value */
    val->type = T_INT;
    val->u.number = 0;
    --val;

    if (f->obj->count == 0) {
	/*
	 * call from destructed object
	 */
	i_pop(f, nargs - 1);
	return 0;
    }

    if (i_call(f, obj, val->u.string->text, val->u.string->len, FALSE,
	       nargs - 2)) {
	/* function exists */
	val = f->sp++;
	str_del((f->sp++)->u.string);
	*f->sp = *val;
    } else {
	/* function doesn't exist */
	str_del((f->sp++)->u.string);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_object", kf_this_object, pt_this_object)
# else
char pt_this_object[] = { C_STATIC, T_OBJECT, 0 };

/*
 * NAME:	kfun->this_object()
 * DESCRIPTION:	return the current object
 */
int kf_this_object(f)
register frame *f;
{
    register object *obj;

    --f->sp;
    obj = f->obj;
    if (obj->count != 0) {
	f->sp->type = T_OBJECT;
	f->sp->oindex = obj->index;
	f->sp->u.objcnt = obj->count;
    } else {
	f->sp->type = T_INT;
	f->sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_object", kf_previous_object, pt_previous_object)
# else
char pt_previous_object[] = { C_TYPECHECKED | C_STATIC | C_VARARGS, T_OBJECT, 1,
			      T_INT };

/*
 * NAME:	kfun->previous_object()
 * DESCRIPTION:	return the previous object in the call_other chain
 */
int kf_previous_object(f, nargs)
register frame *f;
int nargs;
{
    register object *obj;

    if (nargs == 0) {
	(--f->sp)->type = T_INT;
	f->sp->u.number = 0;
    } else if (f->sp->u.number < 0) {
	return 1;
    }

    obj = i_prev_object(f, (int) f->sp->u.number);
    if (obj != (object *) NULL) {
	f->sp->type = T_OBJECT;
	f->sp->oindex = obj->index;
	f->sp->u.objcnt = obj->count;
    } else {
	f->sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_program", kf_previous_program, pt_previous_program)
# else
char pt_previous_program[] = { C_TYPECHECKED | C_STATIC | C_VARARGS, T_STRING,
			       1, T_INT };

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
	(--f->sp)->type = T_INT;
	f->sp->u.number = 0;
    } else if (f->sp->u.number < 0) {
	return 1;
    }

    prog = i_prev_program(f, (int) f->sp->u.number);
    if (prog != (char *) NULL) {
	f->sp->type = T_STRING;
	str = str_new((char *) NULL, strlen(prog) + 1L);
	str->text[0] = '/';
	strcpy(str->text + 1, prog);
	str_ref(f->sp->u.string = str);
    } else {
	f->sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_trace", kf_call_trace, pt_call_trace)
# else
char pt_call_trace[] = { C_STATIC, T_MIXED | (2 << REFSHIFT), 0 };

/*
 * NAME:	kfun->call_trace()
 * DESCRIPTION:	return the entire call_other chain
 */
int kf_call_trace(f)
register frame *f;
{
    array *a;

    a = i_call_trace(f);
    (--f->sp)->type = T_ARRAY;
    arr_ref(f->sp->u.array = a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("clone_object", kf_clone_object, pt_clone_object)
# else
char pt_clone_object[] = { C_TYPECHECKED | C_STATIC, T_OBJECT, 1, T_OBJECT };

/*
 * NAME:	kfun->clone_object()
 * DESCRIPTION:	clone a new object
 */
int kf_clone_object(f)
register frame *f;
{
    register object *obj;

    obj = &otable[f->sp->oindex];
    if (!(obj->flags & O_MASTER)) {
	error("Cloning from a clone");
    }
    obj = o_clone(obj);
    f->sp->oindex = obj->index;
    f->sp->u.objcnt = obj->count;
    i_call(f, obj, "", 0, FALSE, 0);	/* cause creator to be called */
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("destruct_object", kf_destruct_object, pt_destruct_object)
# else
char pt_destruct_object[] = { C_TYPECHECKED | C_STATIC, T_VOID, 1, T_OBJECT };

/*
 * NAME:	kfun->destruct_object()
 * DESCRIPTION:	destruct an object
 */
int kf_destruct_object(f)
register frame *f;
{
    register object *obj;

    obj = &otable[f->sp->oindex];
    if (obj->flags & O_USER) {
	comm_close(f, obj);
    }
    if (obj->flags & O_EDITOR) {
	ed_del(obj);
    }
    i_odest(f, obj);	/* wipe out occurrances on the stack */
    o_del(obj, f);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("object_name", kf_object_name, pt_object_name)
# else
char pt_object_name[] = { C_TYPECHECKED | C_STATIC, T_STRING, 1, T_OBJECT };

/*
 * NAME:	kfun->object_name()
 * DESCRIPTION:	return the name of an object
 */
int kf_object_name(f)
register frame *f;
{
    char *name;

    name = o_name(&otable[f->sp->oindex]);
    f->sp->type = T_STRING;
    str_ref(f->sp->u.string = str_new((char *) NULL, strlen(name) + 1L));
    f->sp->u.string->text[0] = '/';
    strcpy(f->sp->u.string->text + 1, name);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("find_object", kf_find_object, pt_find_object)
# else
char pt_find_object[] = { C_TYPECHECKED | C_STATIC, T_OBJECT, 1, T_STRING };

/*
 * NAME:	kfun->find_object()
 * DESCRIPTION:	find the loaded object for a given object name
 */
int kf_find_object(f)
register frame *f;
{
    char *path;
    object *obj;

    path = path_string(f->sp->u.string->text, f->sp->u.string->len);
    if (path == (char *) NULL) {
	return 1;
    }
    i_add_ticks(f, 2);
    obj = o_find(path);
    str_del(f->sp->u.string);
    if (obj != (object *) NULL) {
	f->sp->type = T_OBJECT;
	f->sp->oindex = obj->index;
	f->sp->u.objcnt = obj->count;
    } else {
	f->sp->type = T_INT;
	f->sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("function_object", kf_function_object, pt_function_object)
# else
char pt_function_object[] = { C_TYPECHECKED | C_STATIC, T_STRING, 2,
			      T_STRING, T_OBJECT };

/*
 * NAME:	kfun->function_object()
 * DESCRIPTION:	return the name of the program a function is in
 */
int kf_function_object(f)
register frame *f;
{
    object *obj;
    dsymbol *symb;
    char *name;

    i_add_ticks(f, 2);
    obj = &otable[f->sp->oindex];
    f->sp++;
    symb = ctrl_symb(o_control(obj), f->sp->u.string->text,
		     f->sp->u.string->len);
    str_del(f->sp->u.string);

    if (symb != (dsymbol *) NULL) {
	object *o;

	o = obj->ctrl->inherits[UCHAR(symb->inherit)].obj;
	if (!(d_get_funcdefs(o->ctrl)[UCHAR(symb->index)].class & C_STATIC) ||
	    obj == f->obj) {
	    /*
	     * function exists and is callable
	     */
	    name = o->chain.name;
	    str_ref(f->sp->u.string = str_new((char *) NULL,
					      strlen(name) + 1L));
	    f->sp->u.string->text[0] = '/';
	    strcpy(f->sp->u.string->text + 1, name);
	    return 0;
	}
    }
    f->sp->type = T_INT;
    f->sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_user", kf_this_user, pt_this_user)
# else
char pt_this_user[] = { C_STATIC, T_OBJECT, 0 };

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
	(--f->sp)->type = T_OBJECT;
	f->sp->oindex = obj->index;
	f->sp->u.objcnt = obj->count;
    } else {
	(--f->sp)->type = T_INT;
	f->sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("query_ip_number", kf_query_ip_number, pt_query_ip_number)
# else
char pt_query_ip_number[] = { C_TYPECHECKED | C_STATIC, T_STRING, 1, T_OBJECT };

/*
 * NAME:	kfun->query_ip_number()
 * DESCRIPTION:	return the ip number of a user
 */
int kf_query_ip_number(f)
register frame *f;
{
    register object *obj;

    obj = &otable[f->sp->oindex];
    if (obj->flags & O_USER) {
	f->sp->type = T_STRING;
	str_ref(f->sp->u.string = comm_ip_number(obj));
    } else {
	f->sp->type = T_INT;
	f->sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("users", kf_users, pt_users)
# else
char pt_users[] = { C_STATIC, T_OBJECT | (1 << REFSHIFT), 0 };

/*
 * NAME:	kfun->users()
 * DESCRIPTION:	return the array of users
 */
int kf_users(f)
register frame *f;
{
    (--f->sp)->type = T_ARRAY;
    arr_ref(f->sp->u.array = comm_users(f->data));
    i_add_ticks(f, f->sp->u.array->size);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("strlen", kf_strlen, pt_strlen)
# else
char pt_strlen[] = { C_TYPECHECKED | C_STATIC, T_INT, 1, T_STRING };

/*
 * NAME:	kfun->strlen()
 * DESCRIPTION:	return the length of a string
 */
int kf_strlen(f)
register frame *f;
{
    unsigned short len;

    len = f->sp->u.string->len;
    str_del(f->sp->u.string);
    f->sp->type = T_INT;
    f->sp->u.number = len;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate", kf_allocate, pt_allocate)
# else
char pt_allocate[] = { C_TYPECHECKED | C_STATIC, T_MIXED | (1 << REFSHIFT), 1,
		       T_INT };

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
    arr_ref(f->sp->u.array = arr_new(f->data, (long) f->sp->u.number));
    f->sp->type = T_ARRAY;
    for (i = f->sp->u.array->size, v = f->sp->u.array->elts; i > 0; --i, v++) {
	v->type = T_INT;
	v->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sizeof", kf_sizeof, pt_sizeof)
# else
char pt_sizeof[] = { C_TYPECHECKED | C_STATIC, T_INT, 1,
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
    f->sp->type = T_INT;
    f->sp->u.number = size;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_indices", kf_map_indices, pt_map_indices)
# else
char pt_map_indices[] = { C_TYPECHECKED | C_STATIC, T_MIXED | (1 << REFSHIFT),
			  1, T_MAPPING };

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
    f->sp->type = T_ARRAY;
    arr_ref(f->sp->u.array = a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_values", kf_map_values, pt_map_values)
# else
char pt_map_values[] = { C_TYPECHECKED | C_STATIC, T_MIXED | (1 << REFSHIFT), 1,
			 T_MAPPING };

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
    f->sp->type = T_ARRAY;
    arr_ref(f->sp->u.array = a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_sizeof", kf_map_sizeof, pt_map_sizeof)
# else
char pt_map_sizeof[] = { C_TYPECHECKED | C_STATIC, T_INT, 1, T_MAPPING };

/*
 * NAME:	kfun->map_sizeof()
 * DESCRIPTION:	return the number of index/value pairs in a mapping
 */
int kf_map_sizeof(f)
register frame *f;
{
    unsigned short size;

    i_add_ticks(f, f->sp->u.array->size);
    size = map_size(f->sp->u.array);
    arr_del(f->sp->u.array);
    f->sp->type = T_INT;
    f->sp->u.number = size;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("typeof", kf_typeof, pt_typeof)
# else
char pt_typeof[] = { C_STATIC, T_INT, 1, T_MIXED };

/*
 * NAME:	kfun->typeof()
 * DESCRIPTION:	return the type of a value
 */
int kf_typeof(f)
register frame *f;
{
    unsigned short type;

    type = f->sp->type;
    i_del_value(f->sp);
    f->sp->type = T_INT;
    f->sp->u.number = type;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("error", kf_error, pt_error)
# else
char pt_error[] = { C_TYPECHECKED | C_STATIC, T_VOID, 1, T_STRING };

/*
 * NAME:	kfun->error()
 * DESCRIPTION:	cause an error
 */
int kf_error(f)
register frame *f;
{
    if (strchr(f->sp->u.string->text, LF) != (char *) NULL) {
	error("'\\n' in error string");
    }
    if (f->sp->u.string->len >= 4 * STRINGSZ) {
	error("Error string too long");
    }
    error("%s", f->sp->u.string->text);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("send_message", kf_send_message, pt_send_message)
# else
char pt_send_message[] = { C_STATIC, T_INT, 1, T_MIXED };

/*
 * NAME:	kfun->send_message()
 * DESCRIPTION:	send a message to a user
 */
int kf_send_message(f)
register frame *f;
{
    object *obj;
    int num;

    if (f->sp->type == T_STRING) {
	num = f->sp->u.string->len;
    } else if (f->sp->type == T_INT) {
	num = 1;
    } else {
	return 1;
    }

    obj = f->obj;
    if (obj->count != 0) {
	if (obj->flags & O_USER) {
	    if (f->sp->type == T_INT) {
		comm_echo(obj, f->sp->u.number != 0);
	    } else {
		num = comm_send(obj, f->sp->u.string);
	    }
	} else if ((obj->flags & O_DRIVER) && f->sp->type == T_STRING) {
	    P_message(f->sp->u.string->text);
	}
    }
    if (f->sp->type == T_STRING) {
	str_del(f->sp->u.string);
    }
    f->sp->type = T_INT;
    f->sp->u.number = num;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("time", kf_time, pt_time)
# else
char pt_time[] = { C_STATIC, T_INT, 0 };

/*
 * NAME:	kfun->time()
 * DESCRIPTION:	return the current time
 */
int kf_time(f)
frame *f;
{
    (--f->sp)->type = T_INT;
    f->sp->u.number = P_time();
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_out", kf_call_out, pt_call_out)
# else
char pt_call_out[] = { C_TYPECHECKED | C_STATIC | C_VARARGS, T_INT, 3,
		       T_STRING, T_INT, T_MIXED | T_ELLIPSIS };

/*
 * NAME:	kfun->call_out()
 * DESCRIPTION:	start a call_out
 */
int kf_call_out(f, nargs)
register frame *f;
int nargs;
{
    object *obj;
    uindex handle;

    if (nargs < 2) {
	return -1;
    }

    i_add_ticks(f, nargs);
    obj = f->obj;
    if (obj->count != 0 &&
	(handle=co_new(obj, f->sp[nargs - 1].u.string,
		       f->sp[nargs - 2].u.number, f, nargs - 2)) != 0) {
	/* pop duration */
	f->sp++;
    } else {
	/* no call_out was started: pop all arguments */
	i_pop(f, nargs - 1);
	handle = 0;
    }
    str_del(f->sp->u.string);
    f->sp->type = T_INT;
    f->sp->u.number = handle;

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("remove_call_out", kf_remove_call_out, pt_remove_call_out)
# else
char pt_remove_call_out[] = { C_TYPECHECKED | C_STATIC, T_INT, 1, T_INT };

/*
 * NAME:	kfun->remove_call_out()
 * DESCRIPTION:	remove a call_out
 */
int kf_remove_call_out(f)
register frame *f;
{
    i_add_ticks(f, 10);
    f->sp->u.number = co_del(f->obj, (uindex) f->sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("swapout", kf_swapout, pt_swapout)
# else
char pt_swapout[] = { C_STATIC, T_VOID, 0 };

/*
 * NAME:	kfun->swapout()
 * DESCRIPTION:	swap out all objects
 */
int kf_swapout(f)
frame *f;
{
    swapout();

    (--f->sp)->type = T_INT;
    f->sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("dump_state", kf_dump_state, pt_dump_state)
# else
char pt_dump_state[] = { C_STATIC, T_VOID, 0 };

/*
 * NAME:	kfun->dump_state()
 * DESCRIPTION:	dump state
 */
int kf_dump_state(f)
frame *f;
{
    dump_state();
    (--f->sp)->type = T_INT;
    f->sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("shutdown", kf_shutdown, pt_shutdown)
# else
char pt_shutdown[] = { C_STATIC, T_VOID, 0 };

/*
 * NAME:	kfun->shutdown()
 * DESCRIPTION:	shut down the mud
 */
int kf_shutdown(f)
frame *f;
{
    finish();

    (--f->sp)->type = T_INT;
    f->sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_status, pt_status)
# else
char pt_status[] = { C_TYPECHECKED | C_STATIC | C_VARARGS,
		     T_MIXED | (1 << REFSHIFT), 1, T_OBJECT };

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

    i_add_ticks(f, 100);
    if (nargs == 0) {
	a = conf_status(f);
	--f->sp;
    } else {
	a = conf_object(f->data, &otable[f->sp->oindex]);
    }
    f->sp->type = T_ARRAY;
    arr_ref(f->sp->u.array = a);
    return 0;
}
# endif
