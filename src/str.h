typedef struct _string_ {
    unsigned short len;	/* string length */
    uindex ref;		/* number of references + const bit */
    union {
	struct _string_ **strconst;	/* indirect reference to myself */
	struct _strref_ *primary;	/* primary reference */
    } u;
    char text[1];	/* actual characters following this struct */
} string;

# define STR_REF	((uindex) UINDEX_MAX / 2)
# define STR_CONST	((uindex) (UINDEX_MAX - STR_REF))

extern void		str_init	P((void));
extern string	       *str_new		P((char*, long));
# define str_ref(s)	((s)->ref++)
extern void		str_del		P((string*));

extern long		str_put		P((string*, long));
extern void		str_clear	P((void));

extern int		str_cmp		P((string*, string*));
extern string	       *str_add		P((string*, string*));
extern unsigned short	str_index	P((string*, long));
extern string	       *str_range	P((string*, long, long));
