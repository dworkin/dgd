string creator(string file)

    Get the creator of a file.


string normalize_path(string file, string directory, string owner)

    Normalize a path.


int file_size(string file, int dirflag)	[System only]

    Check size of a file.  If dirflag is TRUE, recursively check size
    of directory.


void set_object_manager(object objectd)	[System only]

    Install an object manager, in which the following functions will be called
    afterwards:

    * void compiling(string path)

	The given object is about to be compiled.

    * void compile(string owner, object obj, string *source,
		   string inherited...)

	The given object has just been compiled.  If the source array is not
	empty, it was compiled from those strings.  Called just before the
	object is initialized with create(0).

    * void compile_lib(string owner, string path, string *source,
		       string inherited...)

	The given inheritable object has just been compiled.  If the source
	array is not empty, it was compiled from those strings.

    * void compile_failed(string owner, string path)

	An attempt to compile the given object has failed.

    * void clone(string owner, object obj)

	The given object has just been cloned.  Called just before the object
	is initialized with create(1).

    * void destruct(string owner, object obj)

	The given object is about to be destructed.

    * void destruct_lib(string owner, string path)

	The given inheritable object is about to be destructed.

    * void remove_program(string owner, string path, int timestamp, int index)

	The last reference to the given program has been removed.

    * mixed include_file(string compiled, string from, string path)

	The file `path' (which might not exist) is about to be included by
	`from' during the compilation of 'compiled'.  The returned value can be
	either a string for the translated path of the include file, or an
	array of strings, representing the included file itself.  Any other
	return value will prevent inclusion of the file 'path'.

    * int touch(object obj, string function)

	An object which has been marked by call_touch() is about to have the
	given function called in it.  A non-zero return value indicates that the
	object's "untouched" status should be preserved through the following
	call.

    * int forbid_call(string path)

	Return a non-zero value if `path' is not a legal first argument
	for call_other().

    * int forbid_inherit(string from, string path, int priv)

	Return a non-zero value if inheritance of `path' by `from' is not
	allowed.  The flag `priv' indicates that inheritance is private.


void set_error_manager(object errord)	[System only]

    Install an error manager, in which the following functions can be called
    afterwards:

    * void runtime_error(string error, int caught, mixed **trace)

	A runtime error has occurred.

    * void atomic_error(string error, int atom, mixed **trace)

	A runtime error has occurred in atomic code.

    * void compile_error(string file, int line, string error)

	A compile-time error has occurred.


void message(string str)	[System only]

    Show the given string with send_message().
