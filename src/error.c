# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "comm.h"

typedef struct {
    jmp_buf env;			/* error context */
    frame *f;				/* frame context */
    int offset;				/* sp offset */
    ec_ftn cleanup;			/* cleanup function */
} context;

static context stack[ERRSTACKSZ];	/* error context stack */
static context *esp = stack;		/* error context stack pointer */
static char errbuf[4 * STRINGSZ];	/* current error message */

/*
 * NAME:	errcontext->_push_()
 * DESCRIPTION:	push and return the current errorcontext
 */
jmp_buf *_ec_push_(ftn)
ec_ftn ftn;
{
    if (esp == stack + ERRSTACKSZ) {
	error("Too many nested error contexts");
    }
    esp->f = cframe;
    if (esp->f != (frame *) NULL) {
	esp->offset = cframe->fp - sp;
    }
    esp->cleanup = ftn;
    return &(esp++)->env;
}

/*
 * NAME:	errcontext->pop()
 * DESCRIPTION:	pop and return the current errorcontext
 */
void ec_pop()
{
    if (--esp < stack) {
	fatal("pop empty error stack");
    }
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
    value *oldsp;

    if (format != (char *) NULL) {
	sprintf(errbuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    ec_pop();
    memcpy(&env, &esp->env, sizeof(jmp_buf));
    oldsp = (esp->f != (frame *) NULL) ? esp->f->fp - esp->offset :
					 (value *) NULL;
    if (esp->cleanup != (ec_ftn) NULL) {
	(*(esp->cleanup))();
    }
    i_set_sp(oldsp);
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
