NAME
	event_except - broadcast event to subscribers, excluding some

SYNOPSIS
	void event_except(string name, object *exclude, mixed args...)


DESCRIPTION
	Immediately after termination of the current thread, the function
	"evt_" + name is called in all objects subscribed to the named event,
	with the current object as first argument.  Each call is done using
	the tick and stack resources of the subscribed object.  Objects in the
	exclude list will be skipped.

SEE ALSO
	efun/add_event, efun/event, efun/query_events, efun/remove_event
