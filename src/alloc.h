# ifdef DEBUG

# define ALLOC(type, size)						      \
			((type *) (m_alloc(sizeof(type) * (size_t) (size),    \
					   __FILE__, __LINE__)))
# define REALLOC(mem, type, size1, size2)				      \
			((type *) (m_realloc((char *) (mem),		      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2), \
					     __FILE__, __LINE__)))
extern char *m_alloc	P((size_t, char*, int));
extern char *m_realloc	P((char*, size_t, size_t, char*, int));

# else

# define ALLOC(type, size)						      \
			((type *) (m_alloc(sizeof(type) * (size_t) (size))))
# define REALLOC(mem, type, size1, size2)				      \
			((type *) (m_realloc((char *) (mem),		      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2))))
extern char *m_alloc	P((size_t));
extern char *m_realloc	P((char*, size_t, size_t));

# endif

# define FREE(mem)	m_free((char *) (mem))

extern void  m_init	P((size_t, size_t));
extern void  m_free	P((char*));
extern void  m_dynamic	P((void));
extern void  m_static	P((void));
extern bool  m_check	P((void));
extern void  m_purge	P((void));
extern void  m_finish	P((void));

typedef struct {
    Uint smemsize;	/* static memory size */
    Uint smemused;	/* static memory used */
    Uint dmemsize;	/* dynamic memory used */
    Uint dmemused;	/* dynamic memory used */
} allocinfo;

extern allocinfo *m_info P((void));
