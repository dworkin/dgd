# ifdef DEBUG

# define ALLOC(type, size) \
		      ((type *) (m_alloc(sizeof(type) * (unsigned int) (size),\
					 __FILE__, __LINE__)))
extern char *m_alloc	P((unsigned int, char*, int));

# else

# define ALLOC(type, size) \
		      ((type *) (m_alloc(sizeof(type) * (unsigned int) (size))))
extern char *m_alloc	P((unsigned int));

# endif

# define FREE(memory)	m_free((char *) (memory))

extern void  m_init	P((unsigned int, unsigned int));
extern void  m_free	P((char*));
extern void  m_dynamic	P((void));
extern void  m_static	P((void));
extern bool  m_check	P((void));
extern void  m_purge	P((void));

typedef struct {
    Uint smemsize;	/* static memory size */
    Uint smemused;	/* static memory used */
    Uint dmemsize;	/* dynamic memory used */
    Uint dmemused;	/* dynamic memory used */
} allocinfo;

extern allocinfo *m_info P((void));
