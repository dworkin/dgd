# ifndef FUNCDEF
# include "kfun.h"
# include "fcontrol.h"
# include "path.h"
# include "comm.h"
# include "call_out.h"
# include "ed.h"
# include "node.h"
# include "compile.h"
# endif


# ifdef FUNCDEF
FUNCDEF("call_other", kf_call_other, p_call_other)
# else
char p_call_other[] = { C_TYPECHECKED | C_STATIC | C_VARARGS | C_LOCAL, T_MIXED,
			3, T_MIXED, T_STRING, T_MIXED | T_ELLIPSIS };

/*
 * NAME:	kfun->call_other()
 * DESCRIPTION:	call a function in another object
 */
int kf_call_other(nargs)
int nargs;
{
    register object *obj;
    register value *val;
    char *file;

    if (nargs < 2) {
	return -1;
    }

    val = &sp[nargs - 1];
    switch (val->type) {
    case T_STRING:
	file = path_object(val->u.string->text);
	if (file == (char *) NULL) {
	    return 1;
	}
	obj = o_find(file);
	if (obj == (object *) NULL) {
	    /* object isn't loaded: compile it */
	    obj = c_compile(file);
	}
	str_del(val->u.string);
	break;

    case T_OBJECT:
	obj = o_object(val->oindex, val->u.objcnt);
	break;

    default:
	/* bad arg 1 */
	return 1;
    }

    /* default return value */
    val->type = T_INT;
    val->u.number = 0;
    --val;
    if (i_this_object()->count == 0) {
	/*
	 * cannot call_other from destructed object
	 */
	i_pop(nargs - 1);
	return 0;
    }

    if (i_call(obj, val->u.string->text, FALSE, nargs - 2)) {
	/* function exists */
	val = sp++;
	str_del((sp++)->u.string);
	*sp = *val;
    } else {
	/* function doesn't exist */
	str_del((sp++)->u.string);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_object", kf_this_object, p_this_object)
# else
char p_this_object[] = { C_STATIC | C_LOCAL, T_OBJECT, 0 };

/*
 * NAME:	kfun->this_object()
 * DESCRIPTION:	return the current object
 */
int kf_this_object()
{
    register object *obj;

    --sp;
    obj = i_this_object();
    if (obj->count != 0) {
	sp->type = T_OBJECT;
	sp->oindex = obj->index;
	sp->u.objcnt = obj->count;
    } else {
	sp->type = T_INT;
	sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_object", kf_previous_object, p_previous_object)
# else
char p_previous_object[] = { C_TYPECHECKED | C_STATIC | C_VARARGS | C_LOCAL,
			     T_OBJECT, 1, T_INT };

/*
 * NAME:	kfun->previous_object()
 * DESCRIPTION:	return the previous object in the call_other chain
 */
int kf_previous_object(nargs)
int nargs;
{
    register object *obj;

    if (nargs == 0) {
	(--sp)->type = T_INT;
	sp->u.number = 0;
    }

    obj = i_prev_object((int) sp->u.number);
    if (obj != (object *) NULL) {
	sp->type = T_OBJECT;
	sp->oindex = obj->index;
	sp->u.objcnt = obj->count;
    } else {
	sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_trace", kf_call_trace, p_call_trace)
# else
char p_call_trace[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
			T_MIXED | (2 << REFSHIFT), 0 };

/*
 * NAME:	kfun->call_trace()
 * DESCRIPTION:	return the entire call_other chain
 */
int kf_call_trace()
{
    (--sp)->type = T_ARRAY;
    arr_ref(sp->u.array = i_call_trace());
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("clone_object", kf_clone_object, p_clone_object)
# else
char p_clone_object[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_OBJECT, 1,
			  T_STRING };

/*
 * NAME:	kfun->clone_object()
 * DESCRIPTION:	clone a new object
 */
int kf_clone_object()
{
    register object *obj;
    char *file;

    file = path_object(sp->u.string->text);
    if (file == (char *) NULL) {
	return 1;
    }

    obj = o_find(file);
    if (obj == (object *) NULL) {
	obj = c_compile(file);
    }
    if (!(obj->flags & O_MASTER)) {
	error("Cloning from a clone");
    }
    str_del(sp->u.string);
    sp->type = T_OBJECT;
    obj = o_new((char *) NULL, obj, (control *) NULL);
    sp->oindex = obj->index;
    sp->u.objcnt = obj->count;
    i_call(obj, "", FALSE, 0);	/* cause creator to be called */
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("destruct_object", kf_destruct_object, p_destruct_object)
# else
char p_destruct_object[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_VOID, 1,
			     T_OBJECT };

/*
 * NAME:	kfun->destruct_object()
 * DESCRIPTION:	destruct an object
 */
int kf_destruct_object()
{
    register object *obj;

    obj = o_object(sp->oindex, sp->u.objcnt);
    if (obj->flags & O_USER) {
	comm_close(obj);
    }
    if (obj->flags & O_EDITOR) {
	ed_del(obj);
    }
    i_odest(obj);	/* wipe out occurrances on the stack */
    o_del(obj);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("object_name", kf_object_name, p_object_name)
# else
char p_object_name[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_STRING, 1,
			 T_OBJECT };

/*
 * NAME:	kfun->object_name()
 * DESCRIPTION:	return the name of an object
 */
int kf_object_name()
{
    char *name;

    name = o_name(o_object(sp->oindex, sp->u.objcnt));
    sp->type = T_STRING;
    str_ref(sp->u.string = str_new((char *) NULL, strlen(name) + 1L));
    sp->u.string->text[0] = '/';
    strcpy(sp->u.string->text + 1, name);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("find_object", kf_find_object, p_find_object)
# else
char p_find_object[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_OBJECT, 1,
			 T_STRING };

/*
 * NAME:	kfun->find_object()
 * DESCRIPTION:	find the loaded object for a given object name
 */
int kf_find_object()
{
    char *name;
    object *obj;

    name = path_object(sp->u.string->text);
    if (name == (char *) NULL) {
	return 1;
    }

    str_del(sp->u.string);
    obj = o_find(name);
    if (obj != (object *) NULL) {
	sp->type = T_OBJECT;
	sp->oindex = obj->index;
	sp->u.objcnt = obj->count;
    } else {
	sp->type = T_INT;
	sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("function_object", kf_function_object, p_function_object)
# else
char p_function_object[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_STRING, 2,
			     T_STRING, T_OBJECT };

/*
 * NAME:	kfun->function_object()
 * DESCRIPTION:	return the name of the program a function is in
 */
int kf_function_object()
{
    object *obj;
    dsymbol *symb;
    char *name;

    obj = o_object(sp->oindex, sp->u.objcnt);
    sp++;
    symb = ctrl_symb(o_control(obj), sp->u.string->text);
    str_del(sp->u.string);
    if (symb != (dsymbol *) NULL) {
	name = o_name(obj->ctrl->inherits[UCHAR(symb->inherit)].obj);
	str_ref(sp->u.string = str_new((char *) NULL, strlen(name) + 1L));
	sp->u.string->text[0] = '/';
	strcpy(sp->u.string->text + 1, name);
    } else {
	sp->type = T_INT;
	sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_user", kf_this_user, p_this_user)
# else
char p_this_user[] = { C_STATIC | C_LOCAL, T_OBJECT, 0 };

/*
 * NAME:	kfun->this_user()
 * DESCRIPTION:	return the current user object (if any)
 */
int kf_this_user()
{
    object *obj;

    obj = comm_user();
    if (obj != (object *) NULL) {
	(--sp)->type = T_OBJECT;
	sp->oindex = obj->index;
	sp->u.objcnt = obj->count;
    } else {
	(--sp)->type = T_INT;
	sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("query_ip_number", kf_query_ip_number, p_query_ip_number)
# else
char p_query_ip_number[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_STRING, 1,
			     T_OBJECT };

/*
 * NAME:	kfun->query_ip_number()
 * DESCRIPTION:	return the ip number of a user
 */
int kf_query_ip_number()
{
    register object *obj;

    obj = o_object(sp->oindex, sp->u.objcnt);
    if (obj->flags & O_USER) {
	sp->type = T_STRING;
	str_ref(sp->u.string = comm_ip_number(obj));
    } else {
	sp->type = T_INT;
	sp->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("users", kf_users, p_users)
# else
char p_users[] = { C_STATIC | C_LOCAL, T_OBJECT | (1 << REFSHIFT), 0 };

/*
 * NAME:	kfun->users()
 * DESCRIPTION:	return the array of users
 */
int kf_users()
{
    (--sp)->type = T_ARRAY;
    arr_ref(sp->u.array = comm_users());
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("strlen", kf_strlen, p_strlen)
# else
char p_strlen[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 1, T_STRING };

/*
 * NAME:	kfun->strlen()
 * DESCRIPTION:	return the length of a string
 */
int kf_strlen()
{
    unsigned short len;

    len = sp->u.string->len;
    str_del(sp->u.string);
    sp->type = T_INT;
    sp->u.number = len;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate", kf_allocate, p_allocate)
# else
char p_allocate[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
		      T_MIXED | (1 << REFSHIFT), 1, T_INT };

/*
 * NAME:	kfun->allocate()
 * DESCRIPTION:	allocate an array
 */
int kf_allocate()
{
    register int i;
    register value *v;

    arr_ref(sp->u.array = arr_new((long) sp->u.number));
    sp->type = T_ARRAY;
    for (i = sp->u.array->size, v = sp->u.array->elts; i > 0; --i, v++) {
	v->type = T_INT;
	v->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sizeof", kf_sizeof, p_sizeof)
# else
char p_sizeof[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 1,
		    T_MIXED | (1 << REFSHIFT) };

/*
 * NAME:	kfun->sizeof()
 * DESCRIPTION:	return the size of an array
 */
int kf_sizeof()
{
    unsigned short size;

    size = sp->u.array->size;
    arr_del(sp->u.array);
    sp->type = T_INT;
    sp->u.number = size;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_indices", kf_map_indices, p_map_indices)
# else
char p_map_indices[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
			 T_MIXED | (1 << REFSHIFT), 1, T_MAPPING };

/*
 * NAME:	kfun->map_indices()
 * DESCRIPTION:	return the array of mapping indices
 */
int kf_map_indices()
{
    array *a;

    a = map_indices(sp->u.array);
    arr_del(sp->u.array);
    sp->type = T_ARRAY;
    arr_ref(sp->u.array = a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_values", kf_map_values, p_map_values)
# else
char p_map_values[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
			T_MIXED | (1 << REFSHIFT), 1, T_MAPPING };

/*
 * NAME:	kfun->map_values()
 * DESCRIPTION:	return the array of mapping values
 */
int kf_map_values()
{
    array *a;

    a = map_values(sp->u.array);
    arr_del(sp->u.array);
    sp->type = T_ARRAY;
    arr_ref(sp->u.array = a);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("map_sizeof", kf_map_sizeof, p_map_sizeof)
# else
char p_map_sizeof[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 1,
			T_MAPPING };

/*
 * NAME:	kfun->map_sizeof()
 * DESCRIPTION:	return the number of index/value pairs in a mapping
 */
int kf_map_sizeof()
{
    unsigned short size;

    size = map_size(sp->u.array);
    arr_del(sp->u.array);
    sp->type = T_INT;
    sp->u.number = size;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("typeof", kf_typeof, p_typeof)
# else
char p_typeof[] = { C_STATIC | C_LOCAL, T_INT, 1, T_MIXED };

/*
 * NAME:	kfun->typeof()
 * DESCRIPTION:	return the type of a value
 */
int kf_typeof()
{
    unsigned short type;

    type = sp->type;
    i_del_value(sp);
    sp->type = T_INT;
    sp->u.number = type;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("error", kf_error, p_error)
# else
char p_error[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_VOID, 1, T_STRING };

/*
 * NAME:	kfun->error()
 * DESCRIPTION:	cause an error
 */
int kf_error()
{
    if (strchr(sp->u.string->text, LF) != (char *) NULL) {
	error("'\\n' in error string");
    }
    error("%s", sp->u.string->text);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("send_message", kf_send_message, p_send_message)
# else
char p_send_message[] = { C_STATIC | C_LOCAL, T_VOID, 1, T_MIXED };

/*
 * NAME:	kfun->send_message()
 * DESCRIPTION:	send a message to a user
 */
int kf_send_message()
{
    object *obj;

    if (sp->type != T_INT && sp->type != T_STRING) {
	return 1;
    }

    obj = i_this_object();
    if (obj->count != 0) {
	if (obj->flags & O_USER) {
	    if (sp->type == T_INT) {
		comm_echo(obj, sp->u.number != 0);
	    } else {
		comm_send(obj, sp->u.string);
	    }
	} else if ((obj->flags & O_DRIVER) && sp->type == T_STRING) {
	    P_message(sp->u.string->text);
	}
    }
    if (sp->type == T_STRING) {
	str_del(sp->u.string);
    }
    sp->type = T_INT;
    sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("time", kf_time, p_time)
# else
char p_time[] = { C_STATIC | C_LOCAL, T_INT, 0 };

/*
 * NAME:	kfun->time()
 * DESCRIPTION:	return the current time
 */
int kf_time()
{
    (--sp)->type = T_INT;
    sp->u.number = P_time();
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("get_exec_cost", kf_get_exec_cost, p_get_exec_cost)
# else
char p_get_exec_cost[] = { C_STATIC | C_LOCAL, T_INT, 0 };

/*
 * NAME:	kfun->get_exec_cost()
 * DESCRIPTION:	return the allowed execution cost
 */
int kf_get_exec_cost()
{
    (--sp)->type = T_INT;
    sp->u.number = exec_cost;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_out", kf_call_out, p_call_out)
# else
char p_call_out[] = { C_TYPECHECKED | C_STATIC | C_VARARGS | C_LOCAL, T_INT, 3,
		      T_STRING, T_INT, T_MIXED | T_ELLIPSIS };

/*
 * NAME:	kfun->call_out()
 * DESCRIPTION:	start a call_out
 */
int kf_call_out(nargs)
int nargs;
{
    object *obj;
    uindex handle;

    if (nargs < 2) {
	return -1;
    }

    obj = i_this_object();
    if (obj->count != 0 &&
	(handle=co_new(obj, sp[nargs - 1].u.string,
		       (long) sp[nargs - 2].u.number, nargs - 2)) != 0) {
	/* pop duration */
	sp++;
    } else {
	/* no call_out was started: pop all arguments */
	i_pop(nargs - 1);
    }
    str_del(sp->u.string);
    sp->type = T_INT;
    sp->u.number = handle;

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("remove_call_out", kf_remove_call_out, p_remove_call_out)
# else
char p_remove_call_out[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 1,
			     T_INT };

/*
 * NAME:	kfun->remove_call_out()
 * DESCRIPTION:	remove a call_out
 */
int kf_remove_call_out()
{
    sp->u.number = co_del(i_this_object(), (uindex) sp->u.number);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("swapout", kf_swapout, p_swapout)
# else
char p_swapout[] = { C_STATIC | C_LOCAL, T_VOID, 0 };

/*
 * NAME:	kfun->swapout()
 * DESCRIPTION:	swap out all objects
 */
int kf_swapout()
{
    swapout();

    (--sp)->type = T_INT;
    sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("dump_state", kf_dump_state, p_dump_state)
# else
char p_dump_state[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_VOID, 0 };

/*
 * NAME:	kfun->dump_state()
 * DESCRIPTION:	dump state
 */
int kf_dump_state()
{
    dump_state();
    (--sp)->type = T_INT;
    sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("shutdown", kf_shutdown, p_shutdown)
# else
char p_shutdown[] = { C_STATIC | C_LOCAL, T_VOID, 0 };

/*
 * NAME:	kfun->shutdown()
 * DESCRIPTION:	shut down the mud
 */
int kf_shutdown()
{
    finish();

    (--sp)->type = T_INT;
    sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_status, p_status)
# else
char p_status[] = { C_TYPECHECKED | C_STATIC | C_VARARGS | C_LOCAL,
		    T_MIXED | (1 << REFSHIFT), 1, T_OBJECT };

/*
 * NAME:	kfun->status()
 * DESCRIPTION:	return an array with status information about the gamedriver
 *		or an object
 */
int kf_status(nargs)
int nargs;
{
    if (nargs == 0) {
	(--sp)->u.array = conf_status();
    } else {
	sp->u.array = conf_object(o_object(sp->oindex, sp->u.objcnt));
    }
    sp->type = T_ARRAY;
    arr_ref(sp->u.array);
    return 0;
}
# endif
