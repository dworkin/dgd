NAME
	event - broadcast event to subscribers

SYNOPSIS
	void event(string name, mixed args...)


DESCRIPTION
	Immediately after termination of the current thread, the function
	"evt_" + name is called in all objects subscribed to the named event,
	with the current object as first argument.  Each call is done using
	the tick and stack resources of the subscribed object.

SEE ALSO
	efun/add_event, efun/event_except, efun/query_events, efun/remove_event
