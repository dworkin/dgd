# include "dgd.h"

# define ERRSTACKSZ		32	/* reasonable value */

static jmp_buf stack[ERRSTACKSZ];	/* error context stack */
static jmp_buf *esp = stack;		/* error context stack pointer */
static char errbuf[4 * STRINGSZ];	/* current error message */
static FILE *errlog;			/* secondary error log file */

/*
 * NAME:	errcontext->_push_()
 * DESCRIPTION:	pop and return the current errorcontext
 */
jmp_buf *_ec_push_()
{
    if (esp == stack + ERRSTACKSZ) {
	error("Too many nested error contexts");
    }
    return esp++;
}

/*
 * NAME:	errcontext->pop()
 * DESCRIPTION:	pop and return the current errorcontext
 */
jmp_buf *ec_pop()
{
    if (esp == stack) {
	fatal("pop empty error stack");
    }
    return --esp;
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
 * NAME:	errorlog()
 * DESCRIPTION:	specify a secondary error log file
 */
void errorlog(f)
FILE *f;
{
    errlog = f;
}

/*
 * NAME:	warning()
 * DESCRIPTION:	issue a warning message on stdout (and possibly errlog)
 */
void warning(format, arg1, arg2, arg3, arg4, arg5, arg6)
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    if (format != (char *) NULL) {
	fprintf(stderr, format, arg1, arg2, arg3, arg4, arg5, arg6);
	if (errlog != (FILE *) NULL) {
	    fprintf(errlog, format, arg1, arg2, arg3, arg4, arg5, arg6);
	}
    } else {
	fputs(errbuf, stderr);
	if (errlog != (FILE *) NULL) {
	    fputs(errbuf, errlog);
	}
    }
    putc('\n', stderr);
    if (errlog != (FILE *) NULL) {
	putc('\n', errlog);
    }
}

/*
 * NAME:	error()
 * DESCRIPTION:	cause an error
 */
void error(format, arg1, arg2, arg3, arg4, arg5, arg6)
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    if (format != (char *) NULL) {
	sprintf(errbuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
    }
    longjmp(*ec_pop(), 1);
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

    if (count++ == 0) {
	fputs("Fatal error: ", stderr);
	fprintf(stderr, format, arg1, arg2, arg3, arg4, arg5, arg6);
	putc('\n', stderr);
	fflush(stderr);
    }
    abort();
}
