# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "interpret.h"
# include "comm.h"

typedef struct _context_ {
    jmp_buf jump;			/* error context */
    frame *f;				/* frame context */
    unsigned short offset;		/* sp offset */
    rlinfo *rlim;			/* rlimits info */
    ec_ftn handler;			/* error handler */
    struct _context_ *next;		/* next in linked list */
} context;

typedef struct _errenv_ {
    context firstcontext;		/* bottom context */
    context *econtext;			/* current error context */
    string *errstr;			/* current error string */
} errenv;


/*
 * NAME:	errcontext->new_env()
 * DESCRIPTION:	create a new error environment
 */
errenv *ec_new_env()
{
    register errenv *ee;

    ee = SALLOC(errenv, 1);
    ee->econtext = (context *) NULL;
    ee->errstr = (string *) NULL;

    return ee;
}

/*
 * NAME:	errcontext->clear()
 * DESCRIPTION:	clear the error context string
 */
void ec_clear(env)
register lpcenv *env;
{
    if (env->ee->errstr != (string *) NULL) {
	str_del(env, env->ee->errstr);
	env->ee->errstr = (string *) NULL;
    }
}

/*
 * NAME:	errcontext->_push_()
 * DESCRIPTION:	push and return the current errorcontext
 */
jmp_buf *_ec_push_(env, handler)
lpcenv *env;
ec_ftn handler;
{
    register errenv *ee;
    register context *ec;
    register frame *f;

    ee = env->ee;
    if (ee->econtext == (context *) NULL) {
	ec = &ee->firstcontext;
    } else {
	ec = IALLOC(env, context, 1);
    }
    ec->f = f = env->ie->cframe;
    ec->offset = f->fp - f->sp;
    ec->rlim = f->rlim;

    ec->handler = handler;
    ec->next = ee->econtext;
    ee->econtext = ec;
    return &ec->jump;
}

/*
 * NAME:	errcontext->pop()
 * DESCRIPTION:	pop the current errorcontext
 */
void ec_pop(env)
register lpcenv *env;
{
    register context *ec;

    ec = env->ee->econtext;
# ifdef DEBUG
    if (ec == (context *) NULL) {
	fatal("pop empty error stack");
    }
# endif
    env->ee->econtext = ec->next;
    if (ec != &env->ee->firstcontext) {
	IFREE(env, ec);
    } else {
	ec_clear(env);
    }
}

/*
 * NAME:	errorcontext->handler()
 * DESCRIPTION:	dummy handler for previously handled error
 */
static void ec_handler(f, depth)
frame *f;
Int depth;
{
}

/*
 * NAME:	errorstr()
 * DESCRIPTION:	return the current error string
 */
string *errorstr(env)
lpcenv *env;
{
    return env->ee->errstr;
}

/*
 * NAME:	serror()
 * DESCRIPTION:	cause an error, with a string argument
 */
void serror(env, str)
register lpcenv *env;
string *str;
{
    jmp_buf jump;
    register errenv *ee;
    register context *ec;
    register frame *f;
    int offset;
    ec_ftn handler;

    ee = env->ee;
    if (str != (string *) NULL) {
	if (ee->errstr != (string *) NULL) {
	    str_del(env, ee->errstr);
	}
	str_ref(ee->errstr = str);
# ifdef DEBUG
    } else if (ee->errstr == (string *) NULL) {
	fatal("no error string");
# endif
    }
    ec = ee->econtext;
    offset = ec->offset;
    memcpy(&jump, &ec->jump, sizeof(jmp_buf));

    /* restore to atomic entry point */
    env->ie->cframe = f = i_restore(env->ie->cframe, ec->f->level);

    /* handle error */
    for (;;) {
	if (ec->handler != (ec_ftn) NULL) {
	    handler = ec->handler;
	    ec->handler = (ec_ftn) ec_handler;
	    (*handler)(f, ec->f->depth);
	    break;
	}
	ec = ec->next;
	if (ec == (context *) NULL) {
	    /*
	     * default error handler: print message on stdout
	     */
	    P_message(ee->errstr->text);
	    P_message("\012");			/* LF */
	    break;
	}
    }

    /* restore to catch */
    ec = ee->econtext;
    if (f->rlim != ec->rlim) {
	i_set_rlimits(f, ec->rlim);
    }
    env->ie->cframe = f = i_set_sp(f, ec->f->fp - offset);
    f->rlim = ec->rlim;
    ec_pop(env);
    longjmp(jump, 1);
}

/*
 * NAME:	error()
 * DESCRIPTION:	cause an error
 */
void error(env, format, arg1, arg2, arg3, arg4, arg5, arg6)
register lpcenv *env;
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    char ebuf[4 * STRINGSZ];
	    
    if (format != (char *) NULL) {
	sprintf(ebuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
	serror(env, str_new(env, ebuf, (long) strlen(ebuf)));
    } else {
	serror(env, (string *) NULL);
    }
}

/*
 * NAME:	fatal()
 * DESCRIPTION:	a fatal error has been encountered; terminate the program and
 *		dump a core if possible
 */
void fatal(format, arg1, arg2, arg3, arg4, arg5, arg6)
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    static short count;
    char ebuf1[STRINGSZ], ebuf2[STRINGSZ];

    if (count++ == 0) {
	sprintf(ebuf1, format, arg1, arg2, arg3, arg4, arg5, arg6);
	sprintf(ebuf2, "Fatal error: %s\012", ebuf1);	/* LF */

	P_message(ebuf2);	/* show message */

	comm_finish();
    }
    abort();
}

/*
 * NAME:	message()
 * DESCRIPTION:	issue a message on stderr
 */
void message(format, arg1, arg2, arg3, arg4, arg5, arg6)
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    char ebuf[4 * STRINGSZ];

    sprintf(ebuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
    P_message(ebuf);	/* show message */
}
