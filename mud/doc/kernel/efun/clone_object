NAME
	clone_object - clone an object

SYNOPSIS
	object clone_object(string master, varargs string owner)


DESCRIPTION
	Create a clone of the specified object with an unique name of the form
	"object_name#1234".  The cloned object must not itself be a clone.  The
	new object is returned.  The create() function will be called in the
	cloned object immediately.
	If the optional second argument is specified and non-zero, and the
	owner of the current object is "System", the new object will have the
	specified owner.  Otherwise, it will have the same owner as the current
	object.

ACCESS
	The current object must have read access to the file of the object to
	be cloned.

ERRORS
	If the number of existing objects is equal to the value of the
	ST_OTABSIZE field of the array returned by status(), where ST_OTABSIZE
	is defined in the include file <status.h>, attempting to clone a new
	object will crash the system.

SEE ALSO
	efun/call_other, efun/destruct_object, efun/new_object
