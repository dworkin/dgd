NAME
	get_dir - get information about files in a directory

SYNOPSIS
	mixed **get_dir(string file)


DESCRIPTION
	Get information about a file or files in a directory.  The return
	value is of the form

	    ({ ({ file names }), ({ file sizes }), ({ file mod times }),
	       ({ objects }) })

	If a file is a directory, the file size will be given as -2.
	If the last path component of the specified file can be interpreted
	as a regular expression, all files which match this regular expression
	are collected.  Otherwise, only the file itself is taken.  If no files
	match, or if the file is not present, the return value of get_dir()
	will be ({ ({ }), ({ }), ({ }), ({ }) }).
	Objects that have "lib" as a path component are replaced with 1 in the
	object array.
	The following characters have a special meaning in a regular expression:

	    ?	    any single character
	    *	    any (possibly empty) string
	    [a-z]   any character in the range a-z
	    [^a-z]  any character not in range a-z
	    \c	    the character c, not interpreted as having a special
		    meaning

	The files will be sorted by file name.
	Only as many files as specified by status()[ST_ARRAYSIZE], with
	ST_ARRAYSIZE defined in the include file <status.h>, will be collected.

SEE ALSO
	efun/file_info, kfun/make_dir, kfun/remove_dir
