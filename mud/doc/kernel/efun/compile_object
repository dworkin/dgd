NAME
	compile_object - compile an object

SYNOPSIS
	object compile_object(string file, varargs string source)


DESCRIPTION
        Compile an object from a LPC file, specified by the first argument with
        ".c" appended.  If the optional source argument is supplied, the object
        is compiled from that string, instead.  The returned object will have
        the file string as name.
	If the object to be compiled already exists and is not inherited by
	any other object, it and all of its clones will be upgraded to the
	new version.  Variables will be preserved only if they also exist in
	the new version and have the same type; new variables will be
	initialized to nil.  The actual upgrading is done immediately upon
	completion of the current thread.
	If the new object has "lib" as a path component, it can only be
	inherited and nil is returned.  Otherwise, if the object has "obj" as a
	path component, it can be cloned.

ACCESS
	The current object must have write access to the file to be compiled.

SEE ALSO
	kfun/object_name
