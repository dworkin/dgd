struct _string_ {
    struct _strref_ *primary;	/* primary reference */
    Uint ref;			/* number of references + const bit */
    ssizet len;			/* string length */
    char text[1];		/* actual characters following this struct */
};

extern string	        *str_alloc	P((lpcenv*, char*, long));
extern string	        *str_new	P((lpcenv*, char*, long));
# define str_ref(s)	 ((s)->ref++)
extern void		 str_del	P((lpcenv*, string*));

extern struct _strmerge_*str_merge	P((lpcenv*));
extern Uint		 str_put	P((lpcenv*, struct _strmerge_*, string*,
					   Uint));
extern void		 str_clear	P((lpcenv*, struct _strmerge_*));

extern int		 str_cmp	P((string*, string*));
extern string	        *str_add	P((lpcenv*, string*, string*));
extern ssizet		 str_index	P((lpcenv*, string*, long));
extern void		 str_ckrange	P((lpcenv*, string*, long, long));
extern string	        *str_range	P((lpcenv*, string*, long, long));
