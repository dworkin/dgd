/*
 * Global defines and typedefs.
 */

# ifdef __STDC__
#  define P(proto)	proto
# else
#  define P(proto)	()
# endif

typedef char bool;
# define TRUE		1
# define FALSE		0

/* these may be changed, but sizeof(uindex) <= sizeof(int) */
typedef unsigned short uindex;
# define UINDEX_MAX	USHRT_MAX


/*
 * Host dependent stuff.
 */

# include "host.h"


/*
 * Gamedriver configuration.  Hash table sizes should be primes.
 */

/* general */
# define BUF_SIZE	FS_BLOCK_SIZE	/* I/O buffer size */
# define MAX_LINE_SIZE	1024	/* max. line size in ed and lex (power of 2) */
# define STRINGSZ	256	/* general (internal) string size */
# define STRMAPHASHSZ	20	/* # characters to hash of map string indices */
# define STRMERGETABSZ	1031	/* general string merge table size */
# define STRMERGEHASHSZ	20	/* # characters in merge strings to hash */
# define ARRMERGETABSZ	1031	/* general array merge table size */
# define OBJTABSZ	1031	/* object name table size */
# define OBJHASHSZ	100	/* # characters in object names to hash */

/* editor */
# define NR_EDBUFS	3	/* # buffers in editor cache (>= 3) */
/*# define TMPFILE_SIZE	2097152	/* max. editor tmpfile size */

/* lexical scanner */
# define MACTABSZ	131	/* macro hash table size */
# define MACHASHSZ	10	/* # characters in macros to hash */

/* compiler */
# define YYDEPTH	200	/* parser stack size */
# define MAX_ERRORS	5	/* max. number of errors during compilation */
# define MAX_LOCALS	32	/* max. number of parameters + local vars */
# define OMERGETABSZ	131	/* inherit object merge table size */
# define VFMERGETABSZ	257	/* variable/function merge table sizes */
# define VFMERGEHASHSZ	10	/* # characters in function/variables to hash */
# define NTMPVAL	64	/* # of temporary values for LPC->C code */


extern void  conf_init		P((char*));
extern char *conf_base_dir	P((void));
extern char *conf_driver	P((void));
extern long  conf_exec_cost	P((void));
