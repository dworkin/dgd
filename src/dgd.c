# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "ed.h"
# include "call_out.h"
# include "comm.h"
# include "node.h"
# include "compile.h"

/*
 * NAME:	call_driver_object()
 * DESCRIPTION:	call a function in the driver object
 */
bool call_driver_object(func, narg)
char *func;
int narg;
{
    static object *driver;
    static Uint dcount;
    static char *driver_name;

    if (driver == (object *) NULL || dcount != driver->count) {
	if (driver_name == (char *) NULL) {
	    driver_name = conf_driver();
	}
	driver = o_find(driver_name);
	if (driver == (object *) NULL) {
	    driver = c_compile(driver_name);
	}
	dcount = driver->count;
    }
    if (!i_call(driver, func, TRUE, narg)) {
	fatal("missing function in driver object: %s", func);
    }
    return TRUE;
}

static bool swap;	/* are objects to be swapped out? */
static bool dump;	/* is the program to dump? */
static bool intr;	/* received an interrupt? */
static bool stop;	/* is the program to terminate? */

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
 * NAME:	errhandler()
 * DESCRIPTION:	handle an error
 */
static void errhandler()
{
    i_runtime_error(FALSE);
}

/*
 * NAME:	dgd_main()
 * DESCRIPTION:	the main loop of DGD
 */
int dgd_main(argc, argv)
int argc;
char **argv;
{
    char buf[INBUF_SIZE];
    int size;
    object *usr;

    if (argc < 2 || argc > 3) {
	P_message("Usage: dgd config_file [dump_file]\012");	/* LF */
	return 2;
    }

    if (ec_push((ec_ftn) NULL)) {
	message((char *) NULL);
	fatal("error during initialization");
    }
    if (argc == 2) {
	conf_init(argv[1], (char *) NULL);
    } else {
	conf_init(argv[1], argv[2]);
    }
    ec_pop();
    d_export();

    while (ec_push((ec_ftn) errhandler)) {
	i_clear();
	d_export();
	comm_flush(TRUE);
    }

    for (;;) {
	P_getevent();

	o_clean();

# ifdef DEBUG
	swap |= stop;
# endif
	if (!m_check()) {
	    /*
	     * Swap out everything and extend the static memory area.
	     */
	    d_swapout(1);
	    arr_freeall();
	    m_purge();
	} else if (swap) {
	    /*
	     * swap out everything
	     */
	    d_swapout(1);
	    arr_freeall();
	    m_purge();
	}
	swap = FALSE;

	if (dump) {
	    /*
	     * create a state dump
	     */
	    d_swapsync();
	    conf_dump();
	    dump = FALSE;
	}
	/* rebuild swapfile */
	sw_copy();

	if (stop) {
	    break;
	}

	/* interrupts */
	if (intr) {
	    intr = FALSE;
	    call_driver_object("interrupt", 0);
	    i_del_value(sp++);
	    d_export();
	}

	/* user input */
	usr = comm_receive(buf, &size);
	if (usr != (object *) NULL) {
	    (--sp)->type = T_STRING;
	    str_ref(sp->u.string = str_new(buf, (long) size));
	    if (i_call(usr, "receive_message", TRUE, 1)) {
		i_del_value(sp++);
		comm_flush(TRUE);
		d_export();
	    }
	}

	/* callouts */
	co_call();
	comm_flush(FALSE);
    }

    ec_pop();
    comm_finish();
    ed_finish();
    sw_finish();
    return 0;
}
