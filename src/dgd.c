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

# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "editor.h"
# include "call_out.h"
# include "comm.h"
# include "node.h"
# include "compile.h"

static uindex dindex;		/* driver object index */
static Uint dcount;		/* driver object count */
static sector fragment;		/* swap fragment parameter */
static bool rebuild;		/* rebuild swapfile? */
bool intr;			/* received an interrupt? */

/*
 * NAME:	call_driver_object()
 * DESCRIPTION:	call a function in the driver object
 */
bool call_driver_object(f, func, narg)
frame *f;
char *func;
int narg;
{
    object *driver;
    char *driver_name;

    if (dindex == UINDEX_MAX || dcount != (driver=OBJR(dindex))->count ||
	!(driver->flags & O_DRIVER)) {
	driver_name = conf_driver();
	driver = o_find(driver_name, OACC_READ);
	if (driver == (object *) NULL) {
	    driver = c_compile(f, driver_name, (object *) NULL,
			       (string **) NULL, 0, FALSE);
	}
	dindex = driver->index;
	dcount = driver->count;
    }
    if (!i_call(f, driver, (array *) NULL, func, strlen(func), TRUE, narg)) {
	fatal("missing function in driver object: %s", func);
    }
    return TRUE;
}

/*
 * NAME:	interrupt()
 * DESCRIPTION:	register an interrupt
 */
void interrupt()
{
    intr = TRUE;
}

/*
 * NAME:	endthread()
 * DESCRIPTION:	clean up after a thread has terminated
 */
void endthread()
{
    comm_flush();
    d_export();
    o_clean();
    i_clear();
    ed_clear();
    ec_clear();

    co_swapcount(d_swapout(fragment));

    if (stop) {
	comm_finish();
	ed_finish();
# ifdef DEBUG
	swap = 1;
# endif
    }

    if (swap || !m_check()) {
	/*
	 * swap out everything and possibly extend the static memory area
	 */
	d_swapout(1);
	arr_freeall();
	m_purge();
	swap = FALSE;
    }

    if (dump) {
	/*
	 * create a state dump
	 */
	conf_dump();
	dump = FALSE;
	rebuild = TRUE;
	dindex = UINDEX_MAX;
    }

    if (stop) {
	sw_finish();
	m_finish();
	exit(0);
    }
}

/*
 * NAME:	errhandler()
 * DESCRIPTION:	default error handler
 */
void errhandler(f, depth)
frame *f;
Int depth;
{
    i_runtime_error(f, (Int) 0);
}

# ifdef DGD_EXTENSION
/*
 * NAME:	dgd_error()
 * DESCRIPTION:	error handler for the extension interface
 */
void dgd_error(f, format, arg1, arg2, arg3, arg4, arg5, arg6)
frame *f;
char *format, *arg1, *arg2, *arg3, *arg4, *arg5, *arg6;
{
    char ebuf[4 * STRINGSZ];

    if (format != (char *) NULL) {
	sprintf(ebuf, format, arg1, arg2, arg3, arg4, arg5, arg6);
	serror(str_new(ebuf, (long) strlen(ebuf)));
    } else {
	serror((string *) NULL);
    }
}
# endif

/*
 * NAME:	dgd_main()
 * DESCRIPTION:	the main loop of DGD
 */
int dgd_main(argc, argv)
int argc;
char **argv;
{
    Uint rtime, timeout;
    unsigned short rmtime, mtime;

    if (argc < 2 || argc > 3) {
	P_message("Usage: dgd config_file [dump_file]\012");	/* LF */
	return 2;
    }

    /* initialize */
    dindex = UINDEX_MAX;
    swap = dump = intr = stop = FALSE;
    rebuild = TRUE;
    rtime = 0;
    if (!conf_init(argv[1], (argc == 3) ? argv[2] : (char *) NULL, &fragment)) {
	return 2;	/* initialization failed */
    }

    for (;;) {
	/* rebuild swapfile */
	if (rebuild) {
	    timeout = co_time(&mtime);
	    if (timeout > rtime || (timeout == rtime && mtime >= rmtime)) {
		rebuild = o_copy(timeout);
		if (rebuild) {
		    co_swapcount(d_swapout(fragment));
		    rtime = timeout + 1;
		    rmtime = mtime;
		} else {
		    rtime = 0;
		}
	    }
	}

	/* interrupts */
	if (intr) {
	    intr = FALSE;
	    if (!ec_push((ec_ftn) errhandler)) {
		call_driver_object(cframe, "interrupt", 0);
		i_del_value(cframe->sp++);
		ec_pop();
	    }
	    endthread();
	}

	/* handle user input */
	timeout = co_delay(rtime, rmtime, &mtime);
	comm_receive(cframe, timeout, mtime);

	/* callouts */
	co_call(cframe);
    }
}
