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

# define IALLOC(env, type, sz)	ALLOC((env)->mp, type, sz)
# define IALLOCA(env, type, sz)	ALLOCA((env)->mp, type, sz)
# define IREALLOC(env, mem, type, sz1, sz2) \
				REALLOC((env)->mp, mem, type, sz1, sz2)
# define IFREE(env, mem)	FREE((env)->mp, mem)
# define IFREEA(env, mem)	FREEA((env)->mp, mem)
