struct _lpcenv_ {
    struct _mempool_ *mp;	/* memory pool */
    struct _errenv_ *ee;	/* error environment */
    struct _arrenv_ *ae;	/* array environment */
    struct _objenv_ *oe;	/* object environment */
    struct _dataenv_ *de;	/* data environment */
    struct _intenv_ *ie;	/* interpreter environment */
    uindex this_user;		/* user current thread was started for */
};

extern void	sch_init	P((void));
extern lpcenv  *sch_env 	P((void));

# define IALLOC(env, type, size) ALLOC((env)->mp, type, size)
# define IREALLOC(env, mem, type, size1, size2) \
				REALLOC((env)->mp, mem, type, size1, size2)
# define IFREE(env, mem)	FREE((env)->mp, mem)
