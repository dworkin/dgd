# include "hash.h"

typedef struct _macro_ {
    hte chain;			/* hash table entry chain */
    char *replace;		/* replace text */
    int narg;			/* number of arguments */
} macro;

# define MA_NARG	0x1f
# define MA_NOEXPAND	0x20
# define MA_STRING	0x40
# define MA_TAG		0x80

# define MAX_NARG	31

# define MAX_REPL_SIZE	(4 * MAX_LINE_SIZE)

extern void   mc_init	P((void));
extern void   mc_clear  P((void));
extern void   mc_define P((char*, char*, int));
extern void   mc_undef  P((char*));
extern macro *mc_lookup P((char*));
