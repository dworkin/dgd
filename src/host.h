# ifdef MINIX_68K

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TELNET
# include "host/telnet.h"
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

/* # define UCHAR(c)	((char) (c))			/* unsigned character */
# define UCHAR(c)	((int) ((c) & 0xff))		/* unsigned character */
# define SCHAR(c)	((char) (c))			/* signed character */
/* # define SCHAR(c)	((((char) (c)) - 128) ^ -128)	/* signed character */

typedef long Int;
typedef unsigned long Uint;

# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)

# define FS_BLOCK_SIZE		1024

extern int   rename		P((char*, char*));		/* simulated */
extern char *crypt		P((const char*, const char*));

# endif	/* MINIX_68K */


# ifdef ATARI_ST

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TELNET
# include "host/telnet.h"
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define UCHAR(c)	((int)((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		512

# define P_fbinio(fp)		((fp)->_flag |= _IOBIN)

extern char *crypt		P((char*, char*));

# endif	/* ATARI_ST */


# ifdef SUNOS4

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TELNET
# include <arpa/telnet.h>
# endif

# include <alloca.h>
# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define STRUCT_AL	4		/* define this if align(struct) > 2 */
# define UCHAR(c)	((int) ((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		8192

# define crypt			_crypt
extern char *crypt		P((const char*, const char*));

# endif	/* SUNOS4 */


# ifdef BSD386
# define GENERIC_BSD
# endif
# ifdef LINUX
# define GENERIC_SYSV
# endif


# ifdef GENERIC_BSD

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TELNET
# include <arpa/telnet.h>
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define STRUCT_AL	4		/* define this if align(struct) > 2 */
# define UCHAR(c)	((int) ((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_BSD */


# ifdef GENERIC_SYSV

# include <limits.h>
# include <sys/types.h>
# include <unistd.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <sys/stat.h>
# define FNDELAY	O_NDELAY
# endif

# ifdef INCLUDE_TELNET
# include <arpa/telnet.h>
# endif

# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <stdio.h>

# define STRUCT_AL	4		/* define this if align(struct) > 2 */
# define UCHAR(c)	((int) ((c) & 0xff))	/* unsigned character */
# define SCHAR(c)	((char) (c))		/* signed character */

typedef int Int;
typedef unsigned int Uint;

# define ALLOCA(type, size)	((type *) alloca(sizeof(type) * \
						 (unsigned int) (size)))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		8192

# endif	/* GENERIC_SYSV */


extern void  P_getevent	P((void));
extern void  P_message	P((char*));

# ifndef P_fbinio
# define P_fbinio(x)	/* nothing */
# endif
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


/* these must be the same on all hosts */
# define BEL	'\007'
# define BS	'\010'
# define HT	'\011'
# define LF	'\012'
# define VT	'\013'
# define FF	'\014'
# define CR	'\015'

# define ALIGN(x, s)	(((x) + (s) - 1) & ~((s) - 1))
