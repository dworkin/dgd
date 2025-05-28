/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2025 DGD Authors (see the commit log for details)
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
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "comm.h"


static ErrorContextImpl ECI;		/* global error context */
ErrorContext *EC = &ECI;

ErrorContextImpl::ErrorFrame::ErrorFrame(Frame *frame, Handler handler)
{
    f = frame;
    offset = frame->fp - frame->sp;
    atomic = frame->atomic;
    rlim = frame->rlim;

    this->handler = handler;
    next = (ErrorFrame *) NULL;
}

/*
 * push a new errorcontext
 */
jmp_buf *ErrorContextImpl::push(Handler handler)
{
    ErrorFrame *e;
    jmp_buf *jump;

    if (eFrame == (ErrorFrame *) NULL) {
	MM->staticMode();
	e = new ErrorFrame(cframe, handler);
	MM->dynamicMode();
	jump = (jmp_buf *) NULL;
    } else {
	e = new ErrorFrame(cframe, handler);
	jump = &eFrame->env;
    }
    e->next = eFrame;
    eFrame = e;
    EC->env = &e->env;
    return jump;
}

/*
 * pop the current errorcontext
 */
void ErrorContextImpl::pop()
{
    ErrorFrame *e;

    e = eFrame;
# ifdef DEBUG
    if (e == (ErrorFrame *) NULL) {
	fatal("pop empty error stack");
    }
# endif
    cframe->atomic = e->atomic;
    eFrame = e->next;
    if (eFrame == (ErrorFrame *) NULL) {
	MM->staticMode();
	delete e;
	MM->dynamicMode();
	clearException();
	EC->env = (jmp_buf *) NULL;
    } else {
	delete e;
	EC->env = &eFrame->env;
    }
}

/*
 * dummy handler for previously handled error
 */
static void dummyHandler(Frame *f, LPCint depth)
{
    UNREFERENCED_PARAMETER(f);
    UNREFERENCED_PARAMETER(depth);
}

/*
 * set the current error string
 */
void ErrorContextImpl::setException(String *err)
{
    if (this->err != (String *) NULL) {
	this->err->del();
    }
    this->err = err;
    err->ref();
}

/*
 * return the current error string
 */
String *ErrorContextImpl::exception()
{
    return err;
}

/*
 * clear the error context string
 */
void ErrorContextImpl::clearException()
{
    if (err != (String *) NULL) {
	err->del();
	err = (String *) NULL;
    }
}

/*
 * handle error
 */
void ErrorContextImpl::error(String *str)
{
    ErrorFrame *e;
    Handler handler;

    if (str != (String *) NULL) {
	setException(str);
# ifdef DEBUG
    } else if (exception() == (String *) NULL) {
	fatal("no error string");
# endif
    }

    e = eFrame;

    if (atomicFrame == (ErrorFrame *) NULL || atomicFrame == e) {
	do {
	    if (cframe->level != e->f->level) {
		if (atomicFrame == (ErrorFrame *) NULL) {
		    cframe->atomicError(e->f->level);
		    if (e != eFrame) {
			atomicFrame = e;
			break;	/* handle rollback later */
		    }
		}

		cframe = cframe->restore(e->f->level);
		atomicFrame = (ErrorFrame *) NULL;
	    }

	    if (e->handler != (Handler) NULL) {
		handler = e->handler;
		e->handler = (Handler) dummyHandler;
		(*handler)(cframe, e->f->depth);
		break;
	    }
	    e = e->next;
	} while (e != (ErrorFrame *) NULL);
    }

    if (cframe->rlim != eFrame->rlim) {
	cframe->setRlimits(eFrame->rlim);
    }
    cframe = cframe->setSp(eFrame->f->fp - eFrame->offset);
    cframe->atomic = eFrame->atomic;
    cframe->rlim = eFrame->rlim;
    pop();
    throw "LPC error";
}

/*
 * cause an error
 */
void ErrorContextImpl::error(const char *format, ...)
{
    va_list args;
    char ebuf[4 * STRINGSZ];

    if (format != (char *) NULL) {
	va_start(args, format);
	vsnprintf(ebuf, sizeof(ebuf), format, args);
	error(String::create(ebuf, strlen(ebuf)));
	va_end(args);
    } else {
	error((String *) NULL);
    }
}

/*
 * a fatal error has been encountered; terminate the program and
 * dump a core if possible
 */
void ErrorContextImpl::fatal(const char *format, ...)
{
    static short count;
    va_list args;
    char ebuf1[STRINGSZ], ebuf2[STRINGSZ + 14];

    if (count++ == 0) {
	va_start(args, format);
	vsnprintf(ebuf1, sizeof(ebuf1), format, args);
	va_end(args);

	snprintf(ebuf2, sizeof(ebuf2), "Fatal error: %s\012", ebuf1);	/* LF */

	P_message(ebuf2);	/* show message */
    }
    std::abort();
}

/*
 * issue a message on stderr
 */
void ErrorContextImpl::message(const char *format, ...)
{
    va_list args;
    char ebuf[4 * STRINGSZ];

    if (format == (char *) NULL) {
# ifdef DEBUG
	if (exception() == (String *) NULL) {
	    fatal("no error string");
	}
# endif
	if (exception()->len <= sizeof(ebuf) - 2) {
	    snprintf(ebuf, sizeof(ebuf), "%s\012", exception()->text);
	} else {
	    strcpy(ebuf, "[too long error string]\012");
	}
    } else {
	va_start(args, format);
	vsnprintf(ebuf, sizeof(ebuf), format, args);
	va_end(args);
    }
    P_message(ebuf);	/* show message */
}
