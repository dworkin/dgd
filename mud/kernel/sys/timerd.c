# include <kernel/kernel.h>
# include <kernel/rsrc.h>

object rsrcd;
object driver;
mapping suspended;

static create()
{
# if 0
    rsrcd = find_object(RSRCD);
    driver = find_object(DRIVER);
# endif
}

int add_timer(object obj, string owner,
	      string function, int start, int repeat)
{
# if 0
    if (KERNEL()) {
	catch {
	    rlimits (-1; -1) {
		handle = call_out("timer", start - time(), function, obj, owner,
				  start, repeat);
		if (!rsrcd->rsrc_incr(driver->creator(obj), "timers", obj, 1)) {
		    remove_call_out(handle);
		    error("Too many timers");
		}
	    }
	} : error(DRIVER->query_error());
    }
# endif
}

int remove_timer(object obj, string creator, int handle)
{
# if 0
    if (KERNEL()) {
	mixed **callouts;

	callouts = status(this_object())[O_CALLOUTS];
	for (i = sizeof(callouts); --i >= 0; ) {
	    if (callouts[i][CO_HANDLE] == handle) {
		if (callouts[i][CO_FIRSTXARG + 1] == obj) {
		    rlimits (-1; -1) {
			rsrcd->rsrc_incr(creator, "times", obj, -1);
			return remove_call_out(handle);
		    }
		}
		break;
	    }
	}
	return -1;
    }
# endif
}

remove_timers(object obj)
{
# if 0
    if (previous_program() == AUTO) {
	callouts = status(this_object())[O_CALLOUTS];
	for (i = sizeof(callouts); --i >= 0; ) {
	    if (callouts[i][CO_FIRSTXARG + 1] == obj) {
		remove_call_out(callouts[i][CO_HANDLE]);
	    }
	}
    }
# endif
}

static timer(string function, object obj, string owner, int start, int repeat)
{
# if 0
    if (suspended) {
	suspended += ({ ({ function, obj, owner, start, repeat }) });
    } else {
	if (repeat < 0) {
	    rsrcd->rsrc_incr(owner, "timers", obj, -1);
	} else {
	    start += repeat;
	    call_out("timer", start - time(), function, obj, owner, start,
		     repeat);
	}
	obj->_F_call_limited(owner, function, ({ }));
    }
# endif
}

suspend_calls()
{
# if 0
    if (SYSTEM()) {
	if (suspended) {
	    error("Callouts and timers already suspended");
	}
	suspender = previous_object();
	suspended = ({ });
    }
# endif
}

wakeup_calls()
{
# if 0
    if (SYSTEM() && suspended) {
	for (i = 0; i
    }
# endif
}

int suspended(string function, mixed *args)
{
# if 0
    if (suspended && previous_object() != suspender) {
	if (previous_program() == AUTO) {
	    suspended += ({ ({ function, args }) });
	}
	return 1;
    }
# endif
}
