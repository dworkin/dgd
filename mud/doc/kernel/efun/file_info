NAME
	file_info - get information about a single file

SYNOPSIS
	mixed *file_info(string file)


DESCRIPTION
	Get information about a file.  The return value is of the form

	    ({ file size, file modification time, object })

	If a file is a directory, the file size will be given as -2.
	The object value is set to 1 if the object exists and has "lib" as a
	path component.
	If the file doesn't exist, nil is returned.

SEE ALSO
	efun/get_dir
