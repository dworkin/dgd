NAME
	new_object - create a new light_weight object

SYNOPSIS
	object new_object(string master, varargs string owner)
	object new_object(object master)


DESCRIPTION
	Create a new light-weight instance of the specified object with a name
	of the form "object_name#-1".  If the master object is itself a light-
	weight object, it will be copied.  Light-weight objects cannot be
	destructed and are automatically deallocated once the last reference
	to them is removed.  The new object is returned.  The create() function
	will be called in the new object immediately.
	If the optional second argument is specified and non-zero and the owner
	of the current object is "System", the new object will have the
	specified owner.  Otherwise, it will have the same owner as the current
	object.

ACCESS
	Unless the master object is itself a light-weight object to be copied,
	the current object must have read access to the file of the object to
	be created.

SEE ALSO
	efun/clone_object
