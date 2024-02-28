/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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
# include "data.h"
# include "interpret.h"
# include "editor.h"
# include "call_out.h"
# include "comm.h"
# include "ext.h"
# include "node.h"
# include "compile.h"

static uindex dindex;		/* driver object index */
static Uint dcount;		/* driver object count */
static Sector fragment;		/* swap fragment parameter */
static bool rebuild;		/* rebuild swapfile? */
bool intr;			/* received an interrupt? */

/*
 * call a function in the driver object
 */
bool DGD::callDriver(Frame *f, const char *func, int narg)
{
    Object *driver;
    char *driver_name;

    if (dindex == UINDEX_MAX || dcount != (driver=OBJR(dindex))->count ||
	!(driver->flags & O_DRIVER)) {
	driver_name = Config::driver();
	driver = Object::find(driver_name, OACC_READ);
	if (driver == (Object *) NULL) {
	    driver = Compile::compile(f, driver_name, (Object *) NULL, 0,
				      FALSE);
	}
	dindex = driver->index;
	dcount = driver->count;
    }
    if (!f->call(driver, (LWO *) NULL, func, strlen(func), TRUE, narg)) {
	EC->fatal("missing function in driver object: %s", func);
    }
    return TRUE;
}

/*
 * register an interrupt
 */
void DGD::interrupt()
{
    intr = TRUE;
}

/*
 * clean up after a task has terminated
 */
void DGD::endTask()
{
    Comm::flush();
    Dataspace::xport();
    Object::clean();
    Frame::clear();
    Editor::clear();
    EC->clearException();

    CallOut::swapcount(Dataspace::swapout(fragment));

    if (stop) {
	Comm::clear();
	Editor::finish();
# ifdef DEBUG
	swap = TRUE;
# endif
    }

    if (swap || !MM->check()) {
	/*
	 * swap out everything and possibly extend the static memory area
	 */
	Dataspace::swapout(1);
	Array::freeall();
	String::clean();
	MM->purge();
	swap = FALSE;
    }

    if (dump) {
	/*
	 * create a snapshot
	 */
	Config::dump(incr, boot);
	dump = FALSE;
	if (!incr) {
	    rebuild = TRUE;
	    dindex = UINDEX_MAX;
	}
    }

    if (stop) {
	Swap::finish();
	Config::modFinish(TRUE);
	Ext::finish();

	if (boot) {
	    char **hotboot;

	    /*
	     * attempt to hotboot
	     */
	    hotboot = Config::hotbootExec();
	    P_execv(hotboot[0], hotboot);
	    EC->message("Hotboot failed\012");	/* LF */
	}

	Comm::finish();
	Array::freeall();
	String::clean();
	MM->finish();
	std::exit(boot);
    }
}

/*
 * default error handler
 */
void DGD::errHandler(Frame *f, LPCint depth)
{
    UNREFERENCED_PARAMETER(depth);
    Frame::runtimeError(f, 0);
}

/*
 * the main loop of DGD
 */
int DGD::main(int argc, char **argv)
{
    char *program;
    Uint rtime, timeout;
    unsigned short rmtime, mtime;

    rmtime = 0;

    --argc;
    program = *argv++;
    if (argc < 1 || argc > 3) {
	EC->message("Usage: %s config_file [[partial_snapshot] snapshot]\012",     /* LF */
		    program);
	return 2;
    }

    /* initialize */
    dindex = UINDEX_MAX;
    swap = dump = incr = stop = FALSE;
    rebuild = TRUE;
    rtime = 0;
    if (!Config::init(argv[0], (argc > 1) ? argv[1] : (char *) NULL,
		      (argc > 2) ? argv[2] : (char *) NULL, &fragment)) {
	return 2;	/* initialization failed */
    }

    for (;;) {
	/* rebuild swapfile */
	if (rebuild) {
	    timeout = CallOut::cotime(&mtime);
	    if (timeout > rtime || (timeout == rtime && mtime >= rmtime)) {
		rebuild = Object::copy(timeout);
		CallOut::swapcount(Dataspace::swapout(fragment));
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
		EC->push((ErrorContext::Handler) errHandler);
		callDriver(cframe, "interrupt", 0);
		(cframe->sp++)->del();
		EC->pop();
	    } catch (const char*) { }
	    endTask();
	}

	/* handle user input */
	timeout = CallOut::delay(rtime, rmtime, &mtime);
	Comm::receive(cframe, timeout, mtime);

	/* callouts */
	CallOut::call(cframe);
    }
}
