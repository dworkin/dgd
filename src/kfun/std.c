# ifndef FUNCDEF
# define INCLUDE_TIME
# include "kfun.h"
# include "fcontrol.h"
# include "path.h"
# include "comm.h"
# include "call_out.h"
# include "node.h"
# include "compile.h"
# endif


# ifdef FUNCDEF
FUNCDEF("call_other", kf_call_other, p_call_other)
# else
char p_call_other[] = { C_STATIC | C_VARARGS | C_LOCAL, T_MIXED, 32,
			T_MIXED, T_STRING,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED };

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
	error("Too few arguments to call_other()");
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
    val->type = T_NUMBER;
    val->u.number = 0;
    --val;
    if (val->type != T_STRING) {
	/* bad arg 2 */
	return 2;
    }
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
    value val;

    obj = i_this_object();
    if (obj->count != 0) {
	val.type = T_OBJECT;
	val.oindex = obj->index;
	val.u.objcnt = obj->count;
    } else {
	val.type = T_NUMBER;
	val.u.number = 0;
    }
    i_push_value(&val);		/* will make it zero if obj is destructed */
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_object", kf_previous_object, p_previous_object)
# else
char p_previous_object[] = { C_TYPECHECKED | C_STATIC | C_VARARGS | C_LOCAL,
			     T_OBJECT, 1, T_NUMBER };

/*
 * NAME:	kfun->previous_object()
 * DESCRIPTION:	return the previous object in the call_other chain
 */
int kf_previous_object(nargs)
int nargs;
{
    register object *obj;

    if (nargs == 0) {
	(--sp)->type = T_NUMBER;
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
FUNCDEF("clone_object", kf_clone_object, p_clone_object)
# else
char p_clone_object[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_OBJECT, 1,
			  T_STRING };

/*
 * NAME:	kfun->clone_object()
 * DESCRIPTION:	clone a new object
 */
kf_clone_object()
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
kf_destruct_object()
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
kf_find_object()
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
	sp->type = T_NUMBER;
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
kf_function_object()
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
	sp->type = T_NUMBER;
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
	(--sp)->type = T_NUMBER;
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
	sp->type = T_NUMBER;
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
char p_strlen[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 1, T_STRING };

/*
 * NAME:	kfun->strlen()
 * DESCRIPTION:	return the length of a string
 */
int kf_strlen()
{
    unsigned short len;

    len = sp->u.string->len;
    str_del(sp->u.string);
    sp->type = T_NUMBER;
    sp->u.number = len;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate", kf_allocate, p_allocate)
# else
char p_allocate[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
		      T_MIXED | (1 << REFSHIFT), 1, T_NUMBER };

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
	v->type = T_NUMBER;
	v->u.number = 0;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sizeof", kf_sizeof, p_sizeof)
# else
char p_sizeof[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 1,
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
    sp->type = T_NUMBER;
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
char p_map_sizeof[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 1,
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
    sp->type = T_NUMBER;
    sp->u.number = size;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("intp", kf_intp, p_intp)
# else
char p_intp[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_MIXED };

/*
 * NAME:	kfun->intp()
 * DESCRIPTION:	check if a value is an integer
 */
int kf_intp()
{
    if (sp->type == T_NUMBER) {
	sp->u.number = TRUE;
    } else {
	i_del_value(sp);
	sp->type = T_NUMBER;
	sp->u.number = FALSE;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("stringp", kf_stringp, p_stringp)
# else
char p_stringp[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_MIXED };

/*
 * NAME:	kfun->stringp()
 * DESCRIPTION:	check if a value is a string
 */
int kf_stringp()
{
    if (sp->type == T_STRING) {
	str_del(sp->u.string);
	sp->u.number = TRUE;
    } else {
	i_del_value(sp);
	sp->u.number = FALSE;
    }
    sp->type = T_NUMBER;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("objectp", kf_objectp, p_objectp)
# else
char p_objectp[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_MIXED };

/*
 * NAME:	kfun->objectp()
 * DESCRIPTION:	check if a value is an object
 */
int kf_objectp()
{
    if (sp->type == T_OBJECT) {
	sp->u.number = TRUE;
    } else {
	i_del_value(sp);
	sp->u.number = FALSE;
    }
    sp->type = T_NUMBER;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("arrayp", kf_arrayp, p_arrayp)
# else
char p_arrayp[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_MIXED };

/*
 * NAME:	kfun->arrayp()
 * DESCRIPTION:	check if a value is an array
 */
int kf_arrayp()
{
    if (sp->type == T_ARRAY) {
	arr_del(sp->u.array);
	sp->u.number = TRUE;
    } else {
	i_del_value(sp);
	sp->u.number = FALSE;
    }
    sp->type = T_NUMBER;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("mappingp", kf_mappingp, p_mappingp)
# else
char p_mappingp[] = { C_STATIC | C_LOCAL, T_NUMBER, 1, T_MIXED };

/*
 * NAME:	kfun->mappingp()
 * DESCRIPTION:	check if a value is a mapping
 */
int kf_mappingp()
{
    if (sp->type == T_MAPPING) {
	arr_del(sp->u.array);
	sp->u.number = TRUE;
    } else {
	i_del_value(sp);
	sp->u.number = FALSE;
    }
    sp->type = T_NUMBER;
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
    if (strchr(sp->u.string->text, '\n') != (char *) NULL) {
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

    if (sp->type != T_NUMBER && sp->type != T_STRING) {
	return 1;
    }

    obj = i_this_object();
    if (obj->count != 0) {
	if (obj->flags & O_USER) {
	    if (sp->type == T_NUMBER) {
		comm_echo(obj, sp->u.number != 0);
	    } else {
		comm_send(obj, sp->u.string);
	    }
	} else if ((obj->flags & O_DRIVER) && sp->type == T_STRING) {
	    message(sp->u.string->text);
	}
    }
    if (sp->type == T_STRING) {
	str_del(sp->u.string);
    }
    sp->type = T_NUMBER;
    sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("time", kf_time, p_time)
# else
char p_time[] = { C_STATIC | C_LOCAL, T_NUMBER, 0 };

/*
 * NAME:	kfun->time()
 * DESCRIPTION:	return the current time
 */
int kf_time()
{
    (--sp)->type = T_NUMBER;
    sp->u.number = _time();
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("get_exec_cost", kf_get_exec_cost, p_get_exec_cost)
# else
char p_get_exec_cost[] = { C_STATIC | C_LOCAL, T_NUMBER, 0 };

/*
 * NAME:	kfun->get_exec_cost()
 * DESCRIPTION:	return the allowed execution cost
 */
int kf_get_exec_cost()
{
    (--sp)->type = T_NUMBER;
    sp->u.number = exec_cost;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_out", kf_call_out, p_call_out)
# else
char p_call_out[] = { C_STATIC | C_VARARGS | C_LOCAL, T_VOID, 32,
		      T_STRING, T_NUMBER,
		      T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
		      T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
		      T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
		      T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
		      T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->call_out()
 * DESCRIPTION:	start a call_out
 */
int kf_call_out(nargs)
int nargs;
{
    object *obj;

    if (nargs < 2) {
	error("Too few arguments to function call_out()");
    }
    if (sp[nargs - 1].type != T_STRING) {
	return 1;
    }
    if (sp[nargs - 2].type != T_NUMBER) {
	return 2;
    }

    obj = i_this_object();
    if (obj->count != 0 &&
	co_new(obj, sp[nargs - 1].u.string, (long) sp[nargs - 2].u.number,
	       nargs - 2)) {
	sp++;			/* pop duration */
    } else {
	i_pop(nargs - 1);	/* pop arguments manually */
    }
    str_del(sp->u.string);
    sp->type = T_NUMBER;
    sp->u.number = 0;

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("remove_call_out", kf_remove_call_out, p_remove_call_out)
# else
char p_remove_call_out[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 1,
			     T_STRING };

/*
 * NAME:	kfun->remove_call_out()
 * DESCRIPTION:	remove a call_out
 */
int kf_remove_call_out()
{
    long timeleft;

    timeleft = co_del(i_this_object(), sp->u.string);
    str_del(sp->u.string);
    sp->type = T_NUMBER;
    sp->u.number = timeleft;

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
    comm_flush(TRUE);
    host_finish();
    warning("Shutdown.");
    exit(0);
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
    (--sp)->type = T_NUMBER;
    sp->u.number = 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_status, p_status)
# else
char p_status[] = { C_TYPECHECKED | C_STATIC | C_VARARGS | C_LOCAL,
		    T_NUMBER | (1 << REFSHIFT), 1, T_OBJECT };

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
