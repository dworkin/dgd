typedef struct _string_ {
    union {
	struct _string_ **strconst;	/* indirect reference to myself */
	struct _strref_ *primary;	/* primary reference */
    } u;
    Uint ref;		/* number of references + const bit */
    unsigned short len;	/* string length */
    char text[1];	/* actual characters following this struct */
} string;

# define STR_REF	0x7fffffffL
# define STR_CONST	0x80000000L

extern void		str_init	P((void));
extern string	       *str_new		P((char*, long));
# define str_ref(s)	((s)->ref++)
extern void		str_del		P((string*));

extern long		str_put		P((string*, long));
extern void		str_clear	P((void));

extern int		str_cmp		P((string*, string*));
extern string	       *str_add		P((string*, string*));
extern unsigned short	str_index	P((string*, long));
extern void		str_ckrange	P((string*, long, long));
extern string	       *str_range	P((string*, long, long));
