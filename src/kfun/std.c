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
    obj = c_compile(f, file, obj);
    str_del(f->sp->u.string);
    PUT_OBJVAL(f->sp, obj);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("call_other", kf_call_other, pt_call_other)
# else
char pt_call_other[] = { C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS, T_MIXED, 3,
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

    val = &f->sp[nargs - 1];
    switch (val->type) {
    case T_STRING:
	*--f->sp = *val;
	*val = nil_value;	/* erase old copy */
	call_driver_object(f, "call_object", 1);
	if (f->sp->type != T_OBJECT) {
	    i_del_value(f->sp++);
	    return 1;
	}
	obj = OBJR(f->sp->oindex);
	f->sp++;
	break;

    case T_OBJECT:
	obj = OBJR(val->oindex);
	break;

    default:
	/* bad arg 1 */
	return 1;
    }

    /* default return value */
    *val = nil_value;
    --val;

    if (OBJR(f->oindex)->count == 0) {
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
    obj = OBJR(f->oindex);
    if (obj->count != 0) {
	PUT_OBJVAL(f->sp, obj);
    } else {
	*f->sp = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_object", kf_previous_object, pt_previous_object)
# else
char pt_previous_object[] =
	{ C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS | C_VARARGS, T_OBJECT, 1,
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
	*--f->sp = nil_value;
    } else if (f->sp->u.number < 0) {
	return 1;
    }

    obj = i_prev_object(f, (int) f->sp->u.number);
    if (obj != (object *) NULL) {
	PUT_OBJVAL(f->sp, obj);
    } else {
	*f->sp = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_program", kf_previous_program, pt_previous_program)
# else
char pt_previous_program[] =
	{ C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS | C_VARARGS, T_STRING,
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
    PUSH_ARRVAL(f, i_call_trace(f));
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

    obj = OBJF(f->sp->oindex);
    if (!(obj->flags & O_MASTER)) {
	error("Cloning from a clone");
    }
    obj = o_clone(obj);
    PUT_OBJ(f->sp, obj);
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

    case O_SPECIAL:
	if (ext_destruct != (void (*) P((object*))) NULL) {
	    (*ext_destruct)(obj);
	}
	break;
    }
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
    char buffer[STRINGSZ + 12], *name;

    name = o_name(buffer, OBJR(f->sp->oindex));
    PUT_STRVAL(f->sp, str_new((char *) NULL, strlen(name) + 1L));
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
    char path[STRINGSZ];
    object *obj;

    if (path_string(path, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 1;
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
    obj = OBJR(f->sp->oindex);
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
	PUSH_OBJVAL(f, obj);
    } else {
	*--f->sp = nil_value;
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
    object *obj;

    obj = OBJR(f->sp->oindex);
    if ((obj->flags & O_SPECIAL) == O_USER) {
	PUT_STRVAL(f->sp, comm_ip_number(obj));
    } else {
	*f->sp = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("query_ip_name", kf_query_ip_name, pt_query_ip_name)
# else
char pt_query_ip_name[] = { C_TYPECHECKED | C_STATIC, T_STRING, 1, T_OBJECT };

/*
 * NAME:	kfun->query_ip_name()
 * DESCRIPTION:	return the ip name of a user
 */
int kf_query_ip_name(f)
register frame *f;
{
    object *obj;

    obj = OBJR(f->sp->oindex);
    if ((obj->flags & O_SPECIAL) == O_USER) {
	PUT_STRVAL(f->sp, comm_ip_name(obj));
    } else {
	*f->sp = nil_value;
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
    PUSH_ARRVAL(f, comm_users(f->data));
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
    ssizet len;

    len = f->sp->u.string->len;
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, len);
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
    PUT_ARRVAL(f->sp, arr_new(f->data, (long) f->sp->u.number));
    for (i = f->sp->u.array->size, v = f->sp->u.array->elts; i > 0; --i, v++) {
	*v = nil_value;
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate_int", kf_allocate_int, pt_allocate_int)
# else
char pt_allocate_int[] = { C_TYPECHECKED | C_STATIC, T_INT | (1 << REFSHIFT), 1,
			   T_INT };

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
FUNCDEF("allocate_float", kf_allocate_float, pt_allocate_float)
# else
char pt_allocate_float[] = { C_TYPECHECKED | C_STATIC,
			     T_FLOAT | (1 << REFSHIFT), 1, T_INT };

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
    PUT_INTVAL(f->sp, size);
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
    PUT_ARRVAL(f->sp, a);
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
    PUT_ARRVAL(f->sp, a);
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
    size = map_size(f->data, f->sp->u.array);
    arr_del(f->sp->u.array);
    PUT_INTVAL(f->sp, size);
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
    i_del_value(f->sp);
    PUT_INTVAL(f->sp, f->sp->type);
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
frame *f;
{
    serror(f->sp->u.string);
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
    register object *obj;
    int num;

    if (f->sp->type != T_STRING && f->sp->type != T_INT) {
	return 1;
    }

    num = 0;
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
    if (f->sp->type == T_STRING) {
	str_del(f->sp->u.string);
    }
    PUT_INTVAL(f->sp, num);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("send_datagram", kf_send_datagram, pt_send_datagram)
# else
char pt_send_datagram[] = { C_TYPECHECKED | C_STATIC, T_INT, 1, T_STRING };

/*
 * NAME:	kfun->send_datagram()
 * DESCRIPTION:	send a datagram to a user
 */
int kf_send_datagram(f)
register frame *f;
{
    object *obj;
    int num;

    obj = OBJW(f->oindex);
    if ((obj->flags & O_SPECIAL) == O_USER && obj->count != 0) {
	num = comm_udpsend(obj, f->sp->u.string);
    } else {
	num = 0;
    }
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, num);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("block_input", kf_block_input, pt_block_input)
# else
char pt_block_input[] = { C_TYPECHECKED | C_STATIC, T_VOID, 1, T_INT };

/*
 * NAME:	kfun->block_input()
 * DESCRIPTION:	block input for the current object
 */
int kf_block_input(f)
register frame *f;
{
    object *obj;

    obj = OBJR(f->oindex);
    if ((obj->flags & O_SPECIAL) == O_USER) {
	comm_block(obj, f->sp->u.number != 0);
    }
    *f->sp = nil_value;
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
    PUSH_INTVAL(f, P_time());
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("millitime", kf_millitime, pt_millitime)
# else
char pt_millitime[] = { C_STATIC, T_MIXED | (1 << REFSHIFT), 0 };

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
FUNCDEF("call_out", kf_call_out, pt_call_out)
# else
char pt_call_out[] = { C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS, T_INT, 3,
		       T_STRING, T_MIXED, T_MIXED | T_ELLIPSIS };

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
	if (FLT_ISNEG(flt1.high, flt1.low) || flt_cmp(&flt1, &sixty) > 0) {
	    /* delay < 0.0 or delay > 60.0 */
	    return 2;
	}
	flt_modf(&flt1, &flt2);
	delay = flt_ftoi(&flt2);
	flt_mult(&flt1, &thousand);
	mdelay = flt_ftoi(&flt1);
    } else {
	return 2;
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
FUNCDEF("remove_call_out", kf_remove_call_out, pt_remove_call_out)
# else
char pt_remove_call_out[] = { C_TYPECHECKED | C_STATIC, T_MIXED, 1, T_INT };

/*
 * NAME:	kfun->remove_call_out()
 * DESCRIPTION:	remove a call_out
 */
int kf_remove_call_out(f)
register frame *f;
{
    Int delay;
    xfloat flt;

    i_add_ticks(f, 10);
    delay = d_del_call_out(f->data, (Uint) f->sp->u.number);
    if (delay < -1) {
	flt_itof(-2 - delay, &flt);
	flt_mult(&flt, &thousandth);
	PUT_FLTVAL(f->sp, flt);
    } else {
	PUT_INT(f->sp, delay);
    }
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

    *--f->sp = nil_value;
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

    *--f->sp = nil_value;
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

    *--f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("status", kf_status, pt_status)
# else
char pt_status[] = { C_TYPECHECKED | C_STATIC | C_KFUN_VARARGS | C_VARARGS,
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
	a = conf_object(f->data, OBJR(f->sp->oindex));
    }
    PUT_ARRVAL(f->sp, a);
    return 0;
}
# endif
