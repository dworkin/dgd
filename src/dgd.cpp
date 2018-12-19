/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
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
# include "editor.h"
# include "call_out.h"
# include "comm.h"
# include "node.h"
# include "compile.h"
# include <stdarg.h>

static uindex dindex;		/* driver object index */
static Uint dcount;		/* driver object count */
static sector fragment;		/* swap fragment parameter */
static bool rebuild;		/* rebuild swapfile? */
bool intr;			/* received an interrupt? */

/*
 * NAME:	call_driver_object()
 * DESCRIPTION:	call a function in the driver object
 */
bool call_driver_object(Frame *f, const char *func, int narg)
{
    Object *driver;
    char *driver_name;

    if (dindex == UINDEX_MAX || dcount != (driver=OBJR(dindex))->count ||
	!(driver->flags & O_DRIVER)) {
	driver_name = conf_driver();
	driver = Object::find(driver_name, OACC_READ);
	if (driver == (Object *) NULL) {
	    driver = c_compile(f, driver_name, (Object *) NULL,
			       (String **) NULL, 0, FALSE);
	}
	dindex = driver->index;
	dcount = driver->count;
    }
    if (!i_call(f, driver, (Array *) NULL, func, strlen(func), TRUE, narg)) {
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
 * NAME:	endtask()
 * DESCRIPTION:	clean up after a task has terminated
 */
void endtask()
{
    comm_flush();
    d_export();
    Object::clean();
    i_clear();
    ed_clear();
    ErrorContext::clearException();

    co_swapcount(d_swapout(fragment));

    if (Object::stop) {
	comm_clear();
	ed_finish();
# ifdef DEBUG
	Object::swap = TRUE;
# endif
    }

    if (Object::swap || !Alloc::check()) {
	/*
	 * swap out everything and possibly extend the static memory area
	 */
	d_swapout(1);
	Array::freeall();
	String::clean();
	Alloc::purge();
	Object::swap = FALSE;
    }

    if (Object::dump) {
	/*
	 * create a snapshot
	 */
	conf_dump(Object::incr, Object::boot);
	Object::dump = FALSE;
	if (!Object::incr) {
	    rebuild = TRUE;
	    dindex = UINDEX_MAX;
	}
    }

    if (Object::stop) {
	sw_finish();
	conf_mod_finish();

	if (Object::boot) {
	    char **hotboot;

	    /*
	     * attempt to hotboot
	     */
	    hotboot = conf_hotboot();
	    P_execv(hotboot[0], hotboot);
	    message("Hotboot failed\012");	/* LF */
	}

	comm_finish();
	Array::freeall();
	String::clean();
	Alloc::finish();
	exit(Object::boot);
    }
}

/*
 * NAME:	errhandler()
 * DESCRIPTION:	default error handler
 */
void errhandler(Frame *f, Int depth)
{
    UNREFERENCED_PARAMETER(depth);
    i_runtime_error(f, (Int) 0);
}

/*
 * NAME:	dgd_main()
 * DESCRIPTION:	the main loop of DGD
 */
int dgd_main(int argc, char **argv)
{
    char *program, *module;
    Uint rtime, timeout;
    unsigned short rmtime, mtime;

    rmtime = 0;

    --argc;
    program = *argv++;
    module = (char *) NULL;
    if (argc > 1 && argv[0][0] == '-' && argv[0][1] == 'e') {
	if (argv[0][2] == '\0') {
	    --argc;
	    argv++;
	    module = argv[0];
	} else {
	    module = argv[0] + 2;
	}
	--argc;
	argv++;
    }
    if (argc < 1 || argc > 3) {
	message("Usage: %s [-e module] config_file [[partial_snapshot] snapshot]\012",     /* LF */
		program);
	return 2;
    }

    /* initialize */
    dindex = UINDEX_MAX;
    Object::swap = Object::dump = Object::incr = Object::stop = FALSE;
    rebuild = TRUE;
    rtime = 0;
    if (!conf_init(argv[0], (argc > 1) ? argv[1] : (char *) NULL,
		   (argc > 2) ? argv[2] : (char *) NULL, module,
		   &fragment)) {
	return 2;	/* initialization failed */
    }

    for (;;) {
	/* rebuild swapfile */
	if (rebuild) {
	    timeout = co_time(&mtime);
	    if (timeout > rtime || (timeout == rtime && mtime >= rmtime)) {
		rebuild = Object::copy(timeout);
		co_swapcount(d_swapout(fragment));
		if (rebuild) {
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
	    try {
		ErrorContext::push((ErrorContext::Handler) errhandler);
		call_driver_object(cframe, "interrupt", 0);
		i_del_value(cframe->sp++);
		ErrorContext::pop();
	    } catch (...) { }
	    endtask();
	}

	/* handle user input */
	timeout = co_delay(rtime, rmtime, &mtime);
	comm_receive(cframe, timeout, mtime);

	/* callouts */
	co_call(cframe);
    }
}
