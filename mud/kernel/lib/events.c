# include <kernel/rsrc.h>

private mapping events;
private object rsrcd;

/*
 * NAME:	create()
 * DESCRIPTION:	initialize events mapping
 */
static create()
{
    if (events == 0) {
	events = ([ ]);
	rsrcd = find_object(RSRCD);
    }
}

/*
 * NAME:	add_event()
 * DESCRIPTION:	add a new event type
 */
static nomask add_event(string name)
{
    if (events[name] == 0) {
	events[name] = ([ ]);
    }
}

/*
 * NAME:	remove_event()
 * DESCRIPTION:	remove an event type
 */
static nomask remove_event(string name, object obj)
{
    mapping event;

    event = events[name];
    if (event != 0) {
	rlimits (-1; -1) {
	    object *indices;
	    int i, sz;

	    indices = map_indices(event);
	    for (i = 0, sz = sizeof(indices); i < sz; i++) {
		rsrcd->rsrc_incr(indices[i]->query_owner(), "events",
				 indices[i], -1);
	    }
	    events[name] = 0;
	}
    }
}

/*
 * NAME:	register_event()
 * DESCRIPTION:	register an object as listening to an event
 */
nomask register_event(string name, string function)
{
    mixed **trace;
    object obj;

    if (name == 0) {
	error("Bad argument 1 to function register_event");
    }
    if (function == 0 || strlen(function) < 4 || function[0 .. 3] != "evt_") {
	error("Bad argument 2 to function register_event");
    }
    trace = call_trace();
    obj = find_object(trace[sizeof(trace) - 2][0]);
    if (obj == 0) {
	error("register_event in destructed object");
    }

    event = events[name];
    if (event == 0) {
	error("No such event");
    }
    rlimits (-1; -1) {
	if (event[obj] == 0 &&
	    rsrcd->incr_rsrc(obj->query_owner(), "events", obj, 1)) {
	    error("Too many events");
	}
	events[name][obj] = function;
    }
}

/*
 * NAME:	unregister_event()
 * DESCRIPTION:	unregister an object from an event
 */
nomask unregister_event(string name, object obj)
{
    mapping event;

    event = events[name];
    if (event != 0 && event[obj] != 0) {
	rlimits (-1; -1) {
	    rsrcd->incr_rsrc(obj->query_owner(), "events", obj, -1);
	    event[obj] = 0;
	}
    }
}

/*
 * NAME:	call_event()
 * DESCRIPTION:	cause an event
 */
nomask varargs int call_event(string name, mixed args...)
{
    mapping event;
    object *indices;
    string *values;
    int i, sz, recipients;

    event = events[name];
    if (event == 0) {
	error("No such event");
    }

    indices = map_indices(event);
    values = map_values(event);
    recipients = 0;
    for (i = 0, sz = sizeof(event); i < sz; i++) {
	if (indices[i] != 0) {
	    catch {
		call_limited(indices[i], values[i], name, args...);
	    }
	    recipients++;
	}
    }

    return recipients;
}
