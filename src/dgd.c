# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
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
    static Int dcount;
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
static bool stop;	/* is the program to terminate? */
static bool dump;	/* is the program to dump? */

/*
 * NAME:	swapout()
 * DESCRIPTION:	indicate that objects are to be swapped out
 */
void swapout(flag)
bool flag;
{
    swap = TRUE;
    dump = flag;
}

/*
 * NAME:	finish()
 * DESCRIPTION:	indicate that the program must finish
 */
void finish(flag)
bool flag;
{
    stop = TRUE;
    swap = dump = flag;
}

/*
 * NAME:	main()
 * DESCRIPTION:	the main loop of DGD
 */
int main(argc, argv)
int argc;
char *argv[];
{
    char buf[INBUF_SIZE];
    Int max_cost;
    int size;
    object *usr;

    host_init();

    if (argc < 2 || argc > 3) {
	fprintf(stderr, "Usage: %s config_file [dump_file]\n", argv[0]);
	host_finish();
	return 2;
    }

    if (ec_push()) {
	warning((char *) NULL);
	fatal("error during initialization");
    }
    if (argc == 2) {
	conf_init(argv[1], (char *) NULL);
    } else {
	conf_init(argv[1], argv[2]);
    }
    ec_pop();
    max_cost = conf_exec_cost();

    while (ec_push()) {
	i_log_error(FALSE);
	i_clear();
    }

    do {
	i_set_cost(max_cost >> 1);
	co_call();
	comm_flush(FALSE);

	if (!stop) {
	    i_set_cost(max_cost);
	    usr = comm_receive(buf, &size);
	    if (usr != (object *) NULL) {
		(--sp)->type = T_STRING;
		str_ref(sp->u.string = str_new(buf, (long) size));
		if (i_call(usr, "receive_message", TRUE, 1)) {
		    i_del_value(sp++);
		}
		comm_flush(TRUE);
	    }
	}

	o_clean();

	if (!mcheck()) {
	    d_swapout(1);
	    arr_freeall();
	    mpurge();
	    mexpand();
	} else if (swap) {
	    d_swapout(1);
	    arr_freeall();
	    mpurge();
	}
	swap = FALSE;

	if (dump) {
	    conf_dump();
	    dump = FALSE;
	}
    } while (!stop);

    ec_pop();
    comm_finish();
    ed_finish();
    sw_finish();
    host_finish();
    return 0;
}
