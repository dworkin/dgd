/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

# ifdef WIN32

typedef int Int;
typedef unsigned int Uint;

# include <limits.h>
# include <sys\types.h>
# include <malloc.h>

# ifdef INCLUDE_FILE_IO
# include <io.h>
# include <direct.h>
# include <fcntl.h>
# include <sys\stat.h>

# define F_OK	0
# define R_OK	4
# define W_OK	2
# endif

# ifdef INCLUDE_CTYPE
# include <ctype.h>
# endif

# ifdef INCLUDE_TELNET
# include "host\telnet.h"
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		2048

# define Uuint			unsigned __int64
# define bool			dgd_bool

# endif	/* WIN32 */


# ifdef SUNOS4

# define GENERIC_BSD

# include <alloca.h>
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# endif	/* SUNOS4 */


# ifdef SOLARIS

# if !defined( _FILE_OFFSET_BITS )
# define _FILE_OFFSET_BITS	64   /* 64 bit file offsets */
# endif

# define GENERIC_SYSV

# include <alloca.h>
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# include <sys/file.h>		/* for FNDELAY */

# endif	/* SOLARIS */


# ifdef DECALPHA

# define GENERIC_SYSV

# include <alloca.h>
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# endif /* DECALPHA */


# if defined(DARWIN) || defined(NETBSD) || defined(FREEBSD) || defined(OPENBSD)

# define GENERIC_BSD

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# endif /* DARWIN || NETBSD || FREEBSD || OPENBSD */


# ifdef LINUX

# if !defined( _FILE_OFFSET_BITS )
# define _FILE_OFFSET_BITS	64   /* 64 bit file offsets */
# endif

# define GENERIC_SYSV

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# endif /* LINUX */


# ifdef GENERIC_BSD

typedef int Int;
typedef unsigned int Uint;

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_CTYPE
# include <ctype.h>
# endif

# ifdef INCLUDE_TELNET
# include "host/telnet.h"
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# ifndef ALLOCA
# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)
# endif

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_BSD */


# ifdef GENERIC_SYSV

typedef int Int;
typedef unsigned int Uint;

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# ifndef FNDELAY
# define FNDELAY	O_NDELAY
# endif
# endif

# ifdef INCLUDE_CTYPE
# include <ctype.h>
# endif

# ifdef INCLUDE_TELNET
# include "host/telnet.h"
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# ifndef ALLOCA
# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)
# endif

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_SYSV */


typedef char bool;
# ifndef TRUE
# define TRUE		1
# define FALSE		0
# endif

/*
 * We assume this works on all compilers, but know it
 * doesn't on gcc before 2.x (does anyone use that anyway?)
 */
# ifndef Uuint
#  if !defined(__GNUC__) || __GNUC__ >= 2
#   define Uuint unsigned long long
#  else
#   error No long long support available?
#  endif
# endif

extern void  P_message	(char*);

# ifndef O_BINARY
# define O_BINARY	0
# endif

# ifdef INCLUDE_FILE_IO
# if defined(GENERIC_BSD) || defined(GENERIC_SYSV)
	/* no filename translation */
# define path_native(buf, path)	(path)

# define P_open		open
# define P_close	close
# define P_read		read
# define P_write	write
# define P_lseek	lseek
# define P_fstat	fstat
# define P_stat		stat
# define P_access	access
# define P_unlink	unlink
# define P_rename	rename
# define P_mkdir	mkdir
# define P_rmdir	rmdir
# define P_chdir	chdir
# else
	/* filename translation */
typedef long off_t;
extern char *path_native	(char*, char*);

extern int P_open	(char*, int, int);
extern int P_close	(int);
extern int P_read	(int, char*, int);
extern int P_write	(int, char*, int);
extern off_t P_lseek	(int, off_t, int);
extern int P_fstat	(int, struct stat*);
extern int P_stat	(char*, struct stat*);
extern int P_access	(char*, int);
extern int P_unlink	(char*);
extern int P_rename	(char*, char*);
extern int P_mkdir	(char*, int);
extern int P_rmdir	(char*);
extern int P_chdir	(char*);
# endif
# endif /* INCLUDE_FILE_IO */

extern bool  P_opendir	(char*);
extern char *P_readdir	(void);
extern void  P_closedir	(void);

extern void  P_srandom	(long);
extern long  P_random	(void);

extern Uint  P_time	(void);
extern Uint  P_mtime	(unsigned short*);
extern char *P_ctime	(char*, Uint);

/* these must be the same on all hosts */
# define BEL	'\007'
# define BS	'\010'
# define HT	'\011'
# define LF	'\012'
# define VT	'\013'
# define FF	'\014'
# define CR	'\015'

struct _struct_al_ { long l; short s; };
# define STRUCT_AL	(sizeof(struct _struct_al_) - sizeof(long))
# define ALGN(x, s)	(((x) + (s) - 1) & ~((s) - 1))


# if defined(CHAR_MAX) && CHAR_MAX == 255
# define UCHAR(c)	((char) (c))			/* unsigned character */
# define SCHAR(c)	((((char) (c)) - 128) ^ -128)	/* signed character */
# else
# define UCHAR(c)	((int) ((c) & 0xff))		/* unsigned character */
# define SCHAR(c)	((char) (c))			/* signed character */
# endif

# ifndef UNREFERENCED_PARAMETER
#  define UNREFERENCED_PARAMETER(P)	(void)(P)
# endif

typedef const void cvoid;
