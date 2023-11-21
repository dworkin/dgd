/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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

# include <stdint.h>
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

# include <cstdlib>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>
# include <stdarg.h>

# if _MSC_VER < 1900		/* Visual Studio 2015 */
# define snprintf		_snprintf_s
# define vsnprintf		_vsnprintf_s
# endif

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		2048

typedef int (__stdcall _voidf_)();
# define voidf			_voidf_
# define isfinite(f)		_finite(f)

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

# ifdef DEBUG
# define MEMDEBUG
# endif

typedef int Int;
typedef unsigned int Uint;

# include <stdint.h>
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

# include <cstdlib>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>
# include <stdarg.h>

# ifndef ALLOCA
# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)
# endif

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_BSD */


# ifdef GENERIC_SYSV

# ifdef DEBUG
# define MEMDEBUG
# endif

typedef int Int;
typedef unsigned int Uint;

# include <stdint.h>
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

# include <cstdlib>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>
# include <stdarg.h>

# ifndef ALLOCA
# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)
# endif

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_SYSV */


# ifndef TRUE
# define TRUE		1
# define FALSE		0
# endif

extern void  P_message	(const char*);

# ifndef O_BINARY
# define O_BINARY	0
# endif

# ifdef INCLUDE_FILE_IO
# if defined(GENERIC_BSD) || defined(GENERIC_SYSV)
	/* no filename translation */
# define path_native(buf, path)	(path)

# define P_open		::open
# define P_close	::close
# define P_read		::read
# define P_write	::write
# define P_lseek	::lseek
# define P_fstat	::fstat
# define P_stat		::stat
# define P_access	::access
# define P_unlink	::unlink
# define P_rename	::rename
# define P_mkdir	::mkdir
# define P_rmdir	::rmdir
# define P_chdir	::chdir
# define P_execv	::execv
# else
	/* filename translation */
typedef long off_t;
extern char *path_native	(char*, const char*);

extern int P_open	(const char*, int, int);
extern int P_close	(int);
extern int P_read	(int, char*, int);
extern int P_write	(int, const char*, int);
extern off_t P_lseek	(int, off_t, int);
extern int P_fstat	(int, struct stat*);
extern int P_stat	(const char*, struct stat*);
extern int P_access	(const char*, int);
extern int P_unlink	(const char*);
extern int P_rename	(const char*, const char*);
extern int P_mkdir	(const char*, int);
extern int P_rmdir	(const char*);
extern int P_chdir	(const char*);
extern int P_execv	(const char*, char**);
# endif
# endif /* INCLUDE_FILE_IO */

extern bool  P_opendir	(const char*);
extern char *P_readdir	();
extern void  P_closedir	();

# ifndef voidf
# define voidf		void
# endif

extern voidf *P_dload	(char*, const char*);

extern void  P_srandom	(long);
extern long  P_random	();

extern Uint  P_time	();
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

struct _struct_al_ { jmp_buf buf; short s; };
# define STRUCT_AL	(sizeof(struct _struct_al_) - sizeof(jmp_buf))
# define ALGN(x, s)	(((x) + (s) - 1) & ~((s) - 1))


# define UCHAR(c)	((unsigned char) (c))
# define SCHAR(c)	((signed char) (c))

# ifndef UNREFERENCED_PARAMETER
#  define UNREFERENCED_PARAMETER(P)	(void)(P)
# endif

typedef const void cvoid;
