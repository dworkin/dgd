extern void		 ctrl_init	P((char*, char*));
extern bool		 ctrl_inherit	P((char*, object*, string*));
extern void		 ctrl_create	P((char*));
extern long		 ctrl_dstring	P((string*));
extern void		 ctrl_dproto	P((string*, char*));
extern void		 ctrl_dfunc	P((string*, char*));
extern void		 ctrl_dprogram	P((char*, unsigned int));
extern void		 ctrl_dvar	P((string*, unsigned int,
					   unsigned int));
extern char		*ctrl_ifcall	P((string*, char*, long*));
extern char		*ctrl_fcall	P((string*, long*, int));
extern unsigned short	 ctrl_gencall	P((long));
extern unsigned short	 ctrl_var	P((string*, long*));
extern bool		 ctrl_chkfuncs	P((void));
extern dsymbol		*ctrl_symb	P((control*, char*, unsigned int));
extern control		*ctrl_construct	P((void));
extern void		 ctrl_clear	P((void));

# define PROTO_CLASS(prot)	((prot)[0])
# define PROTO_FTYPE(prot)	((prot)[1])
# define PROTO_NARGS(prot)	((prot)[2])
# define PROTO_ARGS(prot)	((prot) + 3)
# define PROTO_SIZE(prot)	(3 + PROTO_NARGS(prot))

# define T_IMPLICIT		(T_VOID | (1 << REFSHIFT))

# define KFCALL			0
# define KFCALL_LVAL		1
# define IKFCALL		2
# define IKFCALL_LVAL		3
# define DFCALL			4
# define IDFCALL		5
# define FCALL			6
