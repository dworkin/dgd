# ifdef DEBUG

# define ALLOC(type, size) \
			((type *) (alloc(sizeof(type) * (unsigned int) (size),\
					 __FILE__, __LINE__)))
extern char *alloc	P((unsigned int, char*, int));

# else

# define ALLOC(type, size) \
			((type *) (alloc(sizeof(type) * (unsigned int) (size))))
extern char *alloc	P((unsigned int));

# endif

# define FREE(memory)	mfree((char *) (memory))

extern void  minit	P((unsigned int, unsigned int));
extern void  mfree	P((char*));
extern void  mdynamic	P((void));
extern void  mstatic	P((void));
extern bool  mcheck	P((void));
extern void  mpurge	P((void));
extern void  mexpand	P((void));

typedef struct {
    Uint smemsize;	/* static memory size */
    Uint smemused;	/* static memory used */
    Uint dmemsize;	/* dynamic memory used */
    Uint dmemused;	/* dynamic memory used */
} allocinfo;

extern allocinfo *minfo	P((void));
