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
# include "node.h"
# include "compile.h"
# include "table.h"

static uindex dindex;		/* driver object index */
static Uint dcount;		/* driver object count */
static sector fragment;		/* swap fragment parameter */
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

    if (dindex == UINDEX_MAX || dcount != (driver=OBJR(f->env, dindex))->count)
    {
	driver_name = conf_driver();
	driver = o_find(sch_env(), driver_name, OACC_READ);
	if (driver == (object *) NULL) {
	    driver = c_compile(f, driver_name, (object *) NULL,
			       (string *) NULL);
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
    lpcenv *env;

    env = sch_env();
    comm_flush();
    if (ext_cleanup != (void (*) P((void))) NULL) {
	(*ext_cleanup)();
    }
    d_export(env);
    o_clean();
    i_clear(env);
    ed_clear();
    ec_clear(env);

    co_swapcount(d_swapout(sch_env(), fragment));

    if (stop) {
	if (ext_finish != (void (*) P((void))) NULL) {
	    (*ext_finish)();
	}
	comm_finish();
	ed_finish();
	kf_finish();
# ifdef DEBUG
	swap = 1;
# endif
    }

    if (swap || !m_check()) {
	/*
	 * swap out everything and possibly extend the static memory area
	 */
	d_swapout(sch_env(), 1);
	arr_freeall(env);
	m_purge();
	swap = FALSE;
    }

    if (dump) {
	/*
	 * create a state dump
	 */
	d_swapsync(sch_env());
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
    bool swrebuild;
    Uint timeout;
    unsigned short mtime;
    lpcenv *env;

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

    env = sch_env();
    for (;;) {
	/* interrupts */
	if (intr) {
	    intr = FALSE;
	    if (ec_push(env, (ec_ftn) errhandler)) {
		endthread();
	    } else {
		call_driver_object(env->ie->cframe, "interrupt", 0);
		i_del_value(env, env->ie->cframe->sp++);
		ec_pop(env);
		endthread();
	    }
	}

	/* rebuild swapfile */
	swrebuild = sw_copy();

	/* handle user input */
	timeout = co_delay(&mtime);
	if (swrebuild &&
	    (mtime == 0xffff || timeout > 1 || (timeout == 1 && mtime != 0))) {
	    /*
	     * wait no longer than one second if the swapfile has to be
	     * rebuilt
	     */
	    timeout = 1;
	    mtime = 0;
	}
	comm_receive(env->ie->cframe, timeout, mtime);

	/* callouts */
	co_call(env->ie->cframe);
    }
}
