NAME
	call_trace - return the function call trace

SYNOPSIS
	mixed **call_trace()


DESCRIPTION
	Return the function call trace as an array.  The elements are of
	the following format:

	    ({ objname, progname, function, line, extern, arg1, ..., argn })

	The line number is 0 if the function is in a compiled object.
	Extern is 1 if the function was called with call_other(), and 0
	otherwise.
	The offsets in the array are named in the include file <trace.h>.
	The last element of the returned array is the trace of the
	current function.

ACCESS
	If the owner of the current object is not the same as the creator
	of the program containing a function, the arguments are omitted.

SEE ALSO
	kfun/previous_object, kfun/previous_program
