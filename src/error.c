# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "comm.h"

typedef struct _context_ {
    jmp_buf env;			/* error context */
    frame *f;				/* frame context */
    unsigned short offset;		/* sp offset */
    rlinfo *rlim;			/* rlimits info */
    ec_ftn handler;			/* error handler */
    struct _context_ *next;		/* next in linked list */
} context;

static context firstcontext;		/* bottom context */
static context *econtext;		/* current error context */
static char errbuf[4 * STRINGSZ];	/* current error message */

/*
 * NAME:	errcontext->_push_()
 * DESCRIPTION:	push and return the current errorcontext
 */
jmp_buf *_ec_push_(handler)
ec_ftn handler;
{
    register context *e;

    if (econtext == (context *) NULL) {
	e = &firstcontext;
    } else {
	e = ALLOC(context, 1);
    }
    e->f = cframe;
    e->offset = cframe->fp - cframe->sp;
    e->rlim = cframe->rlim;

    e->handler = handler;
    e->next = econtext;
    econtext = e;
    return &e->env;
}

/*
 * NAME:	errcontext->pop()
 * DESCRIPTION:	pop the current errorcontext
 */
void ec_pop()
{
    register context *e;

    e = econtext;
# ifdef DEBUG
    if (e == (context *) NULL) {
	fatal("pop empty error stack");
    }
# endif
    econtext = e->next;
    if (e != &firstcontext) {
	FREE(e);
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
 * NAME:	errormesg()
 * DESCRIPTION:	return the current error message
 */
char *errormesg()
{
    return errbuf;
}

/*
 * NAME:	error()
 * DESCRIPTION:	cause an error
 */
void error(format, arg1, arg2, arg3, arg4, arg5, arg6)
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    jmp_buf env;
    register context *e;
    frame *f;
    int offset;
    ec_ftn handler;

    if (format != (char *) NULL) {
	sprintf(errbuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    e = econtext;
    f = e->f;
    offset = e->offset;
    memcpy(&env, &e->env, sizeof(jmp_buf));

    do {
	if (e->handler != (ec_ftn) NULL) {
	    handler = e->handler;
	    e->handler = (ec_ftn) ec_handler;
	    (*handler)(cframe, e->f->depth);
	    break;
	}
	e = e->next;
    } while (e != (context *) NULL);

    if (cframe->rlim != econtext->rlim) {
	i_set_rlimits(cframe, econtext->rlim);
    }
    cframe = i_set_sp(cframe, f->fp - offset);
    cframe->rlim = econtext->rlim;
    ec_pop();
    longjmp(env, 1);
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

    if (format == (char *) NULL) {
	sprintf(ebuf, "%s\012", errbuf);
    } else {
	sprintf(ebuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
    }
    P_message(ebuf);	/* show message */
}
