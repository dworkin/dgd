/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2020 DGD Authors (see the commit log for details)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "comm.h"
# include <stdarg.h>


static ErrorContext *econtext;		/* current error context */
static ErrorContext *atomicec;		/* first context beyond atomic */
jmp_buf *ErrorContext::env;		/* current error env */
String *ErrorContext::err;		/* current error string */

ErrorContext::ErrorContext(Frame *frame, Handler handler)
{
    f = frame;
    offset = frame->fp - frame->sp;
    atomic = frame->atomic;
    rlim = frame->rlim;

    this->handler = handler;
    next = (ErrorContext *) NULL;
}

/*
 * push a new errorcontext
 */
jmp_buf *ErrorContext::push(Handler handler)
{
    ErrorContext *e;
    jmp_buf *jump;

    if (econtext == (ErrorContext *) NULL) {
	Alloc::staticMode();
	e = new ErrorContext(cframe, handler);
	Alloc::dynamicMode();
	jump = (jmp_buf *) NULL;
    } else {
	e = new ErrorContext(cframe, handler);
	jump = &econtext->extEnv;
    }
    e->next = econtext;
    econtext = e;
    env = &e->extEnv;
    return jump;
}

/*
 * pop the current errorcontext
 */
void ErrorContext::pop()
{
    ErrorContext *e;

    e = econtext;
# ifdef DEBUG
    if (e == (ErrorContext *) NULL) {
	fatal("pop empty error stack");
    }
# endif
    cframe->atomic = e->atomic;
    econtext = e->next;
    if (econtext == (ErrorContext *) NULL) {
	Alloc::staticMode();
	delete e;
	Alloc::dynamicMode();
	clearException();
	env = (jmp_buf *) NULL;
    } else {
	delete e;
	env = &econtext->extEnv;
    }
}

/*
 * NAME:	dummyHandler()
 * DESCRIPTION:	dummy handler for previously handled error
 */
static void dummyHandler(Frame *f, Int depth)
{
    UNREFERENCED_PARAMETER(f);
    UNREFERENCED_PARAMETER(depth);
}

/*
 * set the current error string
 */
void ErrorContext::setException(String *err)
{
    if (ErrorContext::err != (String *) NULL) {
	ErrorContext::err->del();
    }
    ErrorContext::err = err;
    err->ref();
}

/*
 * return the current error string
 */
String *ErrorContext::exception()
{
    return err;
}

/*
 * clear the error context string
 */
void ErrorContext::clearException()
{
    if (err != (String *) NULL) {
	err->del();
	err = (String *) NULL;
    }
}

/*
 * NAME:	error()
 * DESCRIPTION:	cause an error, with a string argument
 */
void error(String *str)
{
    ErrorContext *e;
    int offset;
    ErrorContext::Handler handler;

    if (str != (String *) NULL) {
	ErrorContext::setException(str);
# ifdef DEBUG
    } else if (ErrorContext::exception() == (String *) NULL) {
	fatal("no error string");
# endif
    }

    e = econtext;
    offset = e->offset;

    if (atomicec == (ErrorContext *) NULL || atomicec == e) {
	do {
	    if (cframe->level != e->f->level) {
		if (atomicec == (ErrorContext *) NULL) {
		    cframe->atomicError(e->f->level);
		    if (e != econtext) {
			atomicec = e;
			break;	/* handle rollback later */
		    }
		}

		cframe = cframe->restore(e->f->level);
		atomicec = (ErrorContext *) NULL;
	    }

	    if (e->handler != (ErrorContext::Handler) NULL) {
		handler = e->handler;
		e->handler = (ErrorContext::Handler) dummyHandler;
		(*handler)(cframe, e->f->depth);
		break;
	    }
	    e = e->next;
	} while (e != (ErrorContext *) NULL);
    }

    if (cframe->rlim != econtext->rlim) {
	cframe->setRlimits(econtext->rlim);
    }
    cframe = cframe->setSp(econtext->f->fp - offset);
    cframe->atomic = econtext->atomic;
    cframe->rlim = econtext->rlim;
    ErrorContext::pop();
    throw "LPC error";
}

/*
 * NAME:	error()
 * DESCRIPTION:	cause an error
 */
void error(const char *format, ...)
{
    va_list args;
    char ebuf[4 * STRINGSZ];

    if (format != (char *) NULL) {
	va_start(args, format);
	vsprintf(ebuf, format, args);
	error(String::create(ebuf, strlen(ebuf)));
	va_end(args);
    } else {
	error((String *) NULL);
    }
}

/*
 * NAME:	fatal()
 * DESCRIPTION:	a fatal error has been encountered; terminate the program and
 *		dump a core if possible
 */
void fatal(const char *format, ...)
{
    static short count;
    va_list args;
    char ebuf1[STRINGSZ], ebuf2[STRINGSZ + 14];

    if (count++ == 0) {
	va_start(args, format);
	vsprintf(ebuf1, format, args);
	va_end(args);

	sprintf(ebuf2, "Fatal error: %s\012", ebuf1);	/* LF */

	P_message(ebuf2);	/* show message */
    }
    std::abort();
}

/*
 * NAME:	message()
 * DESCRIPTION:	issue a message on stderr
 */
void message(const char *format, ...)
{
    va_list args;
    char ebuf[4 * STRINGSZ];

    if (format == (char *) NULL) {
# ifdef DEBUG
	if (ErrorContext::exception() == (String *) NULL) {
	    fatal("no error string");
	}
# endif
	if (ErrorContext::exception()->len <= sizeof(ebuf) - 2) {
	    sprintf(ebuf, "%s\012", ErrorContext::exception()->text);
	} else {
	    strcpy(ebuf, "[too long error string]\012");
	}
    } else {
	va_start(args, format);
	vsprintf(ebuf, format, args);
	va_end(args);
    }
    P_message(ebuf);	/* show message */
}
