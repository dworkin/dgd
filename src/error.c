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
register lpcenv *env;
ec_ftn handler;
{
    register context *e;

    if (env->ee->econtext == (context *) NULL) {
	e = &env->ee->firstcontext;
    } else {
	e = IALLOC(env, context, 1);
    }
    e->f = env->ie->cframe;
    e->offset = env->ie->cframe->fp - env->ie->cframe->sp;
    e->rlim = env->ie->cframe->rlim;

    e->handler = handler;
    e->next = env->ee->econtext;
    env->ee->econtext = e;
    return &e->jump;
}

/*
 * NAME:	errcontext->pop()
 * DESCRIPTION:	pop the current errorcontext
 */
void ec_pop(env)
register lpcenv *env;
{
    register context *e;

    e = env->ee->econtext;
# ifdef DEBUG
    if (e == (context *) NULL) {
	fatal("pop empty error stack");
    }
# endif
    env->ee->econtext = e->next;
    if (e != &env->ee->firstcontext) {
	IFREE(env, e);
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
    register context *e;
    frame *f;
    int offset;
    ec_ftn handler;

    if (str != (string *) NULL) {
	if (env->ee->errstr != (string *) NULL) {
	    str_del(env, env->ee->errstr);
	}
	str_ref(env->ee->errstr = str);
# ifdef DEBUG
    } else if (env->ee->errstr == (string *) NULL) {
	fatal("no error string");
# endif
    }

    e = env->ee->econtext;
    f = e->f;
    offset = e->offset;
    memcpy(&jump, &e->jump, sizeof(jmp_buf));

    env->ie->cframe = i_restore(env->ie->cframe, f->level);
    for (;;) {
	if (e->handler != (ec_ftn) NULL) {
	    handler = e->handler;
	    e->handler = (ec_ftn) ec_handler;
	    (*handler)(env->ie->cframe, e->f->depth);
	    break;
	}
	e = e->next;
	if (e == (context *) NULL) {
	    /*
	     * default error handler: print message on stdout
	     */
	    P_message(env->ee->errstr->text);
	    P_message("\012");			/* LF */
	    break;
	}
    }

    if (env->ie->cframe->rlim != env->ee->econtext->rlim) {
	i_set_rlimits(env->ie->cframe, env->ee->econtext->rlim);
    }
    env->ie->cframe = i_set_sp(env->ie->cframe, f->fp - offset);
    env->ie->cframe->rlim = env->ee->econtext->rlim;
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
