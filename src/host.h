# ifdef MINIX_68K

# include <limits.h>
# include <sys/types.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <unistd.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TIME
# include <time.h>
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

# define ALLOCA(type, size)	ALLOC(type, size)
# define AFREE(ptr)		FREE(ptr)

# define FS_BLOCK_SIZE		1024
# define UNIX_PATH_RESOLVE

/* simulated syscalls */
extern int  rename	P((char*, char*));

# endif	/* MINIX_68K */


# ifdef SUN

# include <limits.h>
# include <sys/types.h>

# ifdef INCLUDE_FILE_IO
# include <fcntl.h>
# include <unistd.h>
# include <sys/stat.h>
# endif

# ifdef INCLUDE_TIME
# include <time.h>
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
# define UCHAR(c)	(int)((c)&0xFF)	/* unsigned character */
# define SCHAR(c)	(c)		/* signed character */

typedef int Int;

# define ALLOCA(type, size)	alloca(sizeof(type) * (unsigned int) (size))
# define AFREE(ptr)		/* on function return */

# define FS_BLOCK_SIZE		8192
# define UNIX_PATH_RESOLVE

# define crypt		_crypt

# endif	/* SUN */


# define ALIGN(x, s)	(((x) + (s) - 1) & ~((s) - 1))

# ifdef INCLUDE_TIME
extern time_t _time		P((void));
extern char  *_ctime		P((time_t));
# endif

extern void  host_init		P((void));
extern void  host_finish	P((void));
extern void  host_message	P((char*));
extern void  host_error		P((void));

extern bool  _opendir		P((char*));
extern char *_readdir		P((void));
extern void  _closedir		P((void));

extern void  _alarm		P((unsigned int));

extern long  random		P((void));
extern char *crypt		P((char*, char*));
