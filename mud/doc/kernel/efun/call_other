NAME
	call_other - call a function in an object

SYNOPSIS
	mixed call_other(mixed obj, string function, mixed args...)


DESCRIPTION
	Call a function in an object.  The first argument must be either an
	object or a string.  If it is a string, call_object() will be called
	in the driver object to get the corresponding object.
	Only non-private functions can be called with call_other().  If the
	function is static, the object in which the function is called must
	be the same as the object from which the function is called, or the
	call will fail.
	Any additional arguments to call_other() will be passed on to the
	called function.
	In LPC, obj->func(arg1, arg2, argn) can be used as a shorthand for
	call_other(obj, "func", arg1, arg2, argn).

ERRORS
	An error will result if the first argument is not an object and not a
	string, or if the first argument is a string, but the specified object
	is either uncompiled or an object with "lib" as a path component.
	Calling a function that does not exist, or a function that cannot be
	called with call_other() because it is private or static, does not
	result in an error but returns the value nil.

SEE ALSO
	efun/call_limited, efun/clone_object, efun/destruct_object,
	kfun/function_object
