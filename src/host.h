# ifdef HOST_WITH_UNSIGNED_CHAR
# define UCHAR(c)	((char) (c))			/* unsigned character */
# define SCHAR(c)	((((char) (c)) - 128) ^ -128)	/* signed character */
# endif

# ifdef BEOS

# define UCHAR(c)	((int) ((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

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

# include <alloca.h>

# define STRUCT_AL		4	/* define this if align(struct) > 2 */
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		8192

# define bool			dgd_bool
# define exit			dgd_exit
# define abort			dgd_abort

# endif /* BEOS */


# ifdef WIN32

# define UCHAR(c)	((int)((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

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

# define open			_open
# define close			_close
# define read			_read
# define write			_write
# define lseek			_lseek
# define unlink			_unlink
# define chdir			P_chdir
# define mkdir(dir, mode)	_mkdir(dir)
# define rmdir			_rmdir
# define access			_access
# define stat			_stat

extern int P_chdir(char*);

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

# define STRUCT_AL		4	/* define this if align(struct) > 2 */
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		2048

# define bool			dgd_bool
# define exit			dgd_exit

extern void dgd_exit(int);

# endif	/* WIN32 */


# ifdef MACOS

# define UCHAR(c)	((int)((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# include <limits.h>
# include "macdgd.h"

# ifdef INCLUDE_TELNET
# include "telnet.h"
# endif

# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)

# define FS_BLOCK_SIZE		2048

# endif	/* MACOS */


# ifdef SUNOS4

# define GENERIC_BSD

# include <alloca.h>
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# endif	/* SUNOS4 */


# ifdef SOLARIS

# define GENERIC_SYSV

# include <alloca.h>
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# include <sys/file.h>		/* for FNDELAY */

# endif	/* SOLARIS */


# ifdef DECALPHA

# define GENERIC_SYSV

# define STRUCT_AL		8	/* define this if align(struct) > 2 */

# include <alloca.h>
# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# endif /* DECALPHA */


# if defined(NETBSD) || defined(BSD386)

# define GENERIC_BSD

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# endif /* NETBSD || BSD386 */


# ifdef LINUX

# define GENERIC_SYSV

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
							     (size_t) (size)))
# define AFREE(ptr)		/* on function return */

# endif /* LINUX */


# ifdef GENERIC_BSD

# define UCHAR(c)	((int) ((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

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
# include <arpa/telnet.h>
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# ifndef STRUCT_AL
# define STRUCT_AL		4	/* define this if align(struct) > 2 */
# endif
# ifndef ALLOCA
# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)
# endif

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_BSD */


# ifdef GENERIC_SYSV

# define UCHAR(c)	((int) ((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

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
# include <arpa/telnet.h>
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# ifndef STRUCT_AL
# define STRUCT_AL		4	/* define this if align(struct) > 2 */
# endif
# ifndef ALLOCA
# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)
# endif

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_SYSV */


typedef char bool;
# define TRUE		1
# define FALSE		0


extern void  P_message	P((char*));

# ifndef O_BINARY
# define O_BINARY	0
# endif

extern bool  P_opendir	P((char*));
extern char *P_readdir	P((void));
extern void  P_closedir	P((void));

extern void  P_srandom	P((long));
extern long  P_random	P((void));

extern Uint  P_time	P((void));
extern char *P_ctime	P((Uint));

extern void  P_alarm	P((unsigned int));
extern bool  P_timeout	P((void));

extern char *P_crypt	P((char*, char*));

/* these must be the same on all hosts */
# define BEL	'\007'
# define BS	'\010'
# define HT	'\011'
# define LF	'\012'
# define VT	'\013'
# define FF	'\014'
# define CR	'\015'

# define ALGN(x, s)	(((x) + (s) - 1) & ~((s) - 1))
