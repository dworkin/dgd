# ifdef DEBUG
# define MACROTL	, __FILE__, __LINE__
# define PROTOTL	, char*, int
# else
# define MACROTL	/* nothing */
# define PROTOTL	/* nothing */
# endif

# define ALLOC(pool, type, size)					      \
			((type *) (m_alloc((pool),			      \
					   sizeof(type) * (size_t) (size)     \
					   MACROTL)))
# define REALLOC(pool, mem, type, size1, size2)				      \
			((type *) (m_realloc((pool), (char *) (mem),	      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2)  \
					     MACROTL)))
# define FREE(pool, mem)						      \
			m_free((pool), (char *) (mem))
# define SALLOC(type, size)						      \
			ALLOC((struct _mempool_ *) NULL, type, size)
# define SFREE(mem)	FREE((struct _mempool_ *) NULL, mem)


extern void		 m_init		P((size_t, size_t));
extern struct _mempool_ *m_new_pool	P((void));
extern char		*m_alloc	P((struct _mempool_*, size_t PROTOTL));
extern char		*m_realloc	P((struct _mempool_*, char*, size_t,
					   size_t PROTOTL));
extern void		 m_free		P((struct _mempool_*, char*));
extern bool		 m_check	P((void));
extern void		 m_purge	P((void));
extern void		 m_info		P((Uint*, Uint*, Uint*, Uint*));
extern void		 m_finish	P((void));
