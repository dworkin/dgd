# define ALLOC(type, size) \
		((type *) (alloc(sizeof(type) * (unsigned int) (size))))
# ifdef DEBUG
# define FREE(memory)		xfree((char *) (memory))
# else
# define FREE(memory)		free((char *) (memory))
# endif

extern char *alloc  P((unsigned int));
# ifdef DEBUG
extern void xfree   P((char *));
# endif
