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
static uindex fragment;		/* swap fragment parameter */
static bool swap;		/* are objects to be swapped out? */
static bool dump;		/* is the program to dump? */
bool intr;			/* received an interrupt? */
static bool stop;		/* is the program to terminate? */

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

    if (dindex == UINDEX_MAX || dcount != (driver=OBJ(dindex))->count) {
	driver_name = conf_driver();
	driver = o_find(driver_name);
	if (driver == (object *) NULL) {
	    driver = c_compile(f, driver_name, (object *) NULL);
	}
	dindex = driver->index;
	dcount = driver->count;
    }
    if (!i_call(f, driver, func, strlen(func), TRUE, narg)) {
	fatal("missing function in driver object: %s", func);
    }
    return TRUE;
}

/*
 * NAME:	swapout()
 * DESCRIPTION:	indicate that objects are to be swapped out
 */
void swapout()
{
    swap = TRUE;
}

/*
 * NAME:	dump_state()
 * DESCRIPTION:	indicate that the state must be dumped
 */
void dump_state()
{
    dump = TRUE;
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
 * NAME:	finish()
 * DESCRIPTION:	indicate that the program must finish
 */
void finish()
{
    stop = TRUE;
}

/*
 * NAME:	endthread()
 * DESCRIPTION:	clean up after a thread has terminated
 */
void endthread()
{
    d_export();
    o_clean();
    i_clear();

    if (fragment != 0) {
	co_swapcount(d_swapout(fragment));
    }

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
	d_swapsync();
	conf_dump();
	dump = FALSE;
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
    Uint timeout;
    unsigned short mtime;

    if (argc < 2 || argc > 3) {
	P_message("Usage: dgd config_file [dump_file]\012");	/* LF */
	return 2;
    }

    /* initialize */
    dindex = UINDEX_MAX;
    swap = dump = intr = stop = FALSE;
    if (!conf_init(argv[1], (argc == 3) ? argv[2] : (char *) NULL, &fragment)) {
	return 2;	/* initialization failed */
    }

    for (;;) {
	/* interrupts */
	if (intr) {
	    intr = FALSE;
	    if (ec_push((ec_ftn) errhandler)) {
		endthread();
		comm_flush(FALSE);
	    } else {
		call_driver_object(cframe, "interrupt", 0);
		i_del_value(cframe->sp++);
		ec_pop();
		endthread();
		comm_flush(FALSE);
	    }
	}

	/* rebuild swapfile */
	sw_copy();

	/* handle user input */
	timeout = co_delay(&mtime);
	comm_receive(cframe, timeout, mtime);

	/* callouts */
	co_call(cframe);
	comm_flush(FALSE);
    }
}
