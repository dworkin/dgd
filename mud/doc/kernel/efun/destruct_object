NAME
	destruct_object - destruct an object

SYNOPSIS
	int destruct_object(mixed obj)


DESCRIPTION
	Destruct the object given as the argument, which can be an object or
	the name of an object.  Any value holding the object will immediately
	change into nil, and the object will cease to exist.
	If an object destructs itself, it will cease to exist as soon as
	execution leaves it.  If the last reference to a master object is
	removed (including cloned objects and inheriting objects), the
	function remove_program(objname) will be called in the driver object.
	Return 1 if the object existed and was destructed, 0 otherwise.

ACCESS
	Unless the creator of the current object is "System", an object can
	only be destructed if it has the same owner as the current object.

ERRORS
	Objects destructing themselves may not do certain things between the
	time of destruction and the time the object will cease to exist.  Most
	notably, call_other() may not be used from destructed objects.

SEE ALSO
	efun/call_other, efun/clone_object
