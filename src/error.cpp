/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2015 DGD Authors (see the commit log for details)
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
# include "data.h"
# include "interpret.h"
# include "comm.h"
# include <stdarg.h>

struct context {
    Frame *f;				/* frame context */
    unsigned short offset;		/* sp offset */
    bool atomic;			/* atomic status */
    rlinfo *rlim;			/* rlimits info */
    ec_ftn handler;			/* error handler */
    context *next;			/* next in linked list */
};

static context firstcontext;		/* bottom context */
static context *econtext;		/* current error context */
static context *atomicec;		/* first context beyond atomic */
static String *errstr;			/* current error string */

/*
 * NAME:	errcontext->clear()
 * DESCRIPTION:	clear the error context string
 */
void ec_clear()
{
    if (errstr != (String *) NULL) {
	str_del(errstr);
	errstr = (String *) NULL;
    }
}

/*
 * NAME:	errcontext->push()
 * DESCRIPTION:	push a new errorcontext
 */
void ec_push(ec_ftn handler)
{
    context *e;

    if (econtext == (context *) NULL) {
	e = &firstcontext;
    } else {
	e = ALLOC(context, 1);
    }
    e->f = cframe;
    e->offset = cframe->fp - cframe->sp;
    e->atomic = cframe->atomic;
    e->rlim = cframe->rlim;

    e->handler = handler;
    e->next = econtext;
    econtext = e;
}

/*
 * NAME:	errcontext->pop()
 * DESCRIPTION:	pop the current errorcontext
 */
void ec_pop()
{
    context *e;

    e = econtext;
# ifdef DEBUG
    if (e == (context *) NULL) {
	fatal("pop empty error stack");
    }
# endif
    cframe->atomic = e->atomic;
    econtext = e->next;
    if (e != &firstcontext) {
	FREE(e);
    } else {
	ec_clear();
    }
}

/*
 * NAME:	errorcontext->handler()
 * DESCRIPTION:	dummy handler for previously handled error
 */
static void ec_handler(Frame *f, Int depth)
{
    UNREFERENCED_PARAMETER(f);
    UNREFERENCED_PARAMETER(depth);
}

/*
 * NAME:	set_errorstr()
 * DESCRIPTION:	set the current error string
 */
void set_errorstr(String *err)
{
    if (errstr != (String *) NULL) {
	str_del(errstr);
    }
    str_ref(errstr = err);
}

/*
 * NAME:	errorstr()
 * DESCRIPTION:	return the current error string
 */
String *errorstr()
{
    return errstr;
}

/*
 * NAME:	serror()
 * DESCRIPTION:	cause an error, with a string argument
 */
void serror(String *str)
{
    context *e;
    int offset;
    ec_ftn handler;

    if (str != (String *) NULL) {
	set_errorstr(str);
# ifdef DEBUG
    } else if (errstr == (String *) NULL) {
	fatal("no error string");
# endif
    }

    e = econtext;
    offset = e->offset;

    if (atomicec == (context *) NULL || atomicec == e) {
	do {
	    if (cframe->level != e->f->level) {
		if (atomicec == (context *) NULL) {
		    i_atomic_error(cframe, e->f->level);
		    if (e != econtext) {
			atomicec = e;
			break;	/* handle rollback later */
		    }
		}

		cframe = i_restore(cframe, e->f->level);
		atomicec = (context *) NULL;
	    }

	    if (e->handler != (ec_ftn) NULL) {
		handler = e->handler;
		e->handler = (ec_ftn) ec_handler;
		(*handler)(cframe, e->f->depth);
		break;
	    }
	    e = e->next;
	} while (e != (context *) NULL);
    }

    if (cframe->rlim != econtext->rlim) {
	i_set_rlimits(cframe, econtext->rlim);
    }
    cframe = i_set_sp(cframe, econtext->f->fp - offset);
    cframe->atomic = econtext->atomic;
    cframe->rlim = econtext->rlim;
    ec_pop();
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
	serror(str_new(ebuf, (long) strlen(ebuf)));
	va_end(args);
    } else {
	serror((String *) NULL);
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
    char ebuf1[STRINGSZ], ebuf2[STRINGSZ];

    if (count++ == 0) {
	va_start(args, format);
	vsprintf(ebuf1, format, args);
	va_end(args);

	sprintf(ebuf2, "Fatal error: %s\012", ebuf1);	/* LF */

	P_message(ebuf2);	/* show message */
    }
    abort();
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
	if (errstr == (String *) NULL) {
	    fatal("no error string");
	}
# endif
	if (errstr->len <= sizeof(ebuf) - 2) {
	    sprintf(ebuf, "%s\012", errstr->text);
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
