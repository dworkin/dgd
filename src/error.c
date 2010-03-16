/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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

typedef struct _context_ {
    jmp_buf env;			/* error context */
    frame *f;				/* frame context */
    unsigned short offset;		/* sp offset */
    bool atomic;			/* atomic status */
    rlinfo *rlim;			/* rlimits info */
    ec_ftn handler;			/* error handler */
    struct _context_ *next;		/* next in linked list */
} context;

static context firstcontext;		/* bottom context */
static context *econtext;		/* current error context */
static context *atomicec;		/* first context beyond atomic */
static string *errstr;			/* current error string */

/*
 * NAME:	errcontext->clear()
 * DESCRIPTION:	clear the error context string
 */
void ec_clear()
{
    if (errstr != (string *) NULL) {
	str_del(errstr);
	errstr = (string *) NULL;
    }
}

/*
 * NAME:	errcontext->_push_()
 * DESCRIPTION:	push and return the current errorcontext
 */
jmp_buf *_ec_push_(ec_ftn handler)
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
    return &e->env;
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
static void ec_handler(frame *f, Int depth)
{
    UNREFERENCED_PARAMETER(f);
    UNREFERENCED_PARAMETER(depth);
}

/*
 * NAME:	errorstr()
 * DESCRIPTION:	return the current error string
 */
string *errorstr()
{
    return errstr;
}

/*
 * NAME:	serror()
 * DESCRIPTION:	cause an error, with a string argument
 */
void serror(string *str)
{
    jmp_buf env;
    context *e;
    int offset;
    ec_ftn handler;

    if (str != (string *) NULL) {
	if (errstr != (string *) NULL) {
	    str_del(errstr);
	}
	str_ref(errstr = str);
# ifdef DEBUG
    } else if (errstr == (string *) NULL) {
	fatal("no error string");
# endif
    }

    e = econtext;
    offset = e->offset;
    memcpy(&env, &e->env, sizeof(jmp_buf));

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
    longjmp(env, 1);
}

/*
 * NAME:	error()
 * DESCRIPTION:	cause an error
 */
void error(char *format, ...)
{
    va_list args;
    char ebuf[4 * STRINGSZ];

    if (format != (char *) NULL) {
	va_start(args, format);
	vsprintf(ebuf, format, args);
	serror(str_new(ebuf, (long) strlen(ebuf)));
    } else {
	serror((string *) NULL);
    }
}

/*
 * NAME:	fatal()
 * DESCRIPTION:	a fatal error has been encountered; terminate the program and
 *		dump a core if possible
 */
void fatal(char *format, ...)
{
    static short count;
    va_list args;
    char ebuf1[STRINGSZ], ebuf2[STRINGSZ];

    if (count++ == 0) {
	va_start(args, format);
	vsprintf(ebuf1, format, args);

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
void message(char *format, ...)
{
    va_list args;
    char ebuf[4 * STRINGSZ];

    if (format == (char *) NULL) {
# ifdef DEBUG
	if (errstr == (string *) NULL) {
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
    }
    P_message(ebuf);	/* show message */
}
