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
			       (string *) NULL, FALSE);
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
    if (ext_cleanup != (void (*) P((void))) NULL) {
	(*ext_cleanup)();
    }
    d_export();
    o_clean();
    i_clear();
    ed_clear();
    ec_clear();

    co_swapcount(d_swapout(fragment));

    if (stop) {
	if (ext_finish != (void (*) P((void))) NULL) {
	    (*ext_finish)();
	}
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
	d_swapsync();
	conf_dump();
	dump = FALSE;
	rebuild = TRUE;
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
    swap = dump = rebuild = intr = stop = FALSE;
    rtime = 0;
    if (!conf_init(argv[1], (argc == 3) ? argv[2] : (char *) NULL, &fragment)) {
	return 2;	/* initialization failed */
    }

    for (;;) {
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

	/* rebuild swapfile */
	if (rebuild) {
	    timeout = co_time(&mtime);
	    if (timeout > rtime || (timeout == rtime && mtime >= rmtime)) {
		rebuild = sw_copy(timeout);
		if (rebuild) {
		    rtime = timeout + 1;
		    rmtime = mtime;
		} else {
		    rtime = 0;
		}
	    }
	}

	/* handle user input */
	timeout = co_delay(rtime, rmtime, &mtime);
	comm_receive(cframe, timeout, mtime);

	/* callouts */
	co_call(cframe);
    }
}
