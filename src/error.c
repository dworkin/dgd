# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "comm.h"

# define ERR_LOG_BUF_SZ		1024	/* extra error log buffer size */

static jmp_buf stack[ERRSTACKSZ];	/* error context stack */
static jmp_buf *esp = stack;		/* error context stack pointer */
static char errbuf[4 * STRINGSZ];	/* current error message */
static char *errlog;			/* secondary error log */
static int fd = -1;			/* file descriptor */
static char *buffer;			/* buffer */
static int bufsz;			/* # chars in buffer */

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
char *f;
{
    if (fd >= 0) {
	/*
	 * close previous error stream
	 */
	if (bufsz > 0) {
	    write(fd, buffer, bufsz);
	}
	close(fd);
	FREE(buffer);
	fd = -1;
    }
    errlog = f;
}

/*
 * NAME:	warning()
 * DESCRIPTION:	issue a warning message on stderr (and possibly errlog)
 */
void warning(format, arg1, arg2, arg3, arg4, arg5, arg6)
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    if (format != (char *) NULL) {
	sprintf(errbuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
    }
    message("%s\012", errbuf);	/* LF */
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
 * DESCRIPTION:	issue a message on stderr (and possibly errlog)
 */
void message(format, arg1, arg2, arg3, arg4, arg5, arg6)
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    char ebuf[4 * STRINGSZ];

    sprintf(ebuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
    P_message(ebuf);	/* show message */

    /* secondary error log */
    if (errlog != (char *) NULL) {
	/*
	 * open log
	 */
	fd = open(errlog, O_CREAT | O_APPEND | O_WRONLY | O_BINARY, 0664);
	if (fd >= 0) {
	    buffer = ALLOC(char, ERR_LOG_BUF_SZ);
	    bufsz = 0;
	}
	errlog = (char *) NULL;
    }
    if (fd >= 0) {
	register int len, chunk;
	register char *buf;

	len = strlen(buf = ebuf);
	while (bufsz + len >= ERR_LOG_BUF_SZ) {
	    chunk = ERR_LOG_BUF_SZ - bufsz;
	    memcpy(buffer + bufsz, buf, chunk);
	    write(fd, buffer, ERR_LOG_BUF_SZ);
	    buf += chunk;
	    len -= chunk;
	    bufsz = 0;
	}
	if (len > 0) {
	    memcpy(buffer + bufsz, buf, len);
	    bufsz += len;
	}
    }
}
