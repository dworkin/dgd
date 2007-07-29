struct _string_ {
    struct _strref_ *primary;	/* primary reference */
    Uint ref;			/* number of references + const bit */
    ssizet len;			/* string length */
    char text[1];		/* actual characters following this struct */
};

extern string	       *str_alloc	P((char*, long));
extern string	       *str_new		P((char*, long));
# define str_ref(s)	((s)->ref++)
extern void		str_del		P((string*));

extern void		str_merge	P((void));
extern Uint		str_put		P((string*, Uint));
extern void		str_clear	P((void));

extern int		str_cmp		P((string*, string*));
extern string	       *str_add		P((string*, string*));
extern ssizet		str_index	P((string*, long));
extern void		str_ckrange	P((string*, long, long));
extern string	       *str_range	P((string*, long, long));
