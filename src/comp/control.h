extern void		 ctrl_init	P((int));
extern bool		 ctrl_inherit	P((frame*, char*, object*, string*,
					   int));
extern void		 ctrl_create	P((void));
extern long		 ctrl_dstring	P((string*));
extern void		 ctrl_dproto	P((string*, char*, string*));
extern void		 ctrl_dfunc	P((string*, char*, string*));
extern void		 ctrl_dprogram	P((char*, unsigned int));
extern void		 ctrl_dvar	P((string*, unsigned int,
					   unsigned int, string*));
extern char		*ctrl_ifcall	P((string*, char*, string**, long*));
extern char		*ctrl_fcall	P((string*, string**, long*, int));
extern unsigned short	 ctrl_gencall	P((long));
extern unsigned short	 ctrl_var	P((string*, long*, string**));
extern bool		 ctrl_chkfuncs	P((void));
extern dsymbol		*ctrl_symb	P((control*, char*, unsigned int));
extern control		*ctrl_construct	P((void));
extern void		 ctrl_clear	P((void));
extern unsigned short	*ctrl_varmap	P((control*, control*));
extern array		*ctrl_undefined	P((dataspace*, control*));

# define PROTO_CLASS(prot)	((prot)[0])
# define PROTO_NARGS(prot)	((prot)[1])
# define PROTO_VARGS(prot)	((prot)[2])
# define PROTO_HSIZE(prot)	((prot)[3])
# define PROTO_LSIZE(prot)	((prot)[4])
# define PROTO_SIZE(prot)	((PROTO_HSIZE(prot) << 8) | PROTO_LSIZE(prot))
# define PROTO_FTYPE(prot)	((prot)[5])
# define PROTO_ARGS(prot)	((prot) +				      \
				 (((PROTO_FTYPE(prot) & T_TYPE) == T_CLASS) ? \
				   9 : 6))

# define T_IMPLICIT		(T_VOID | (1 << REFSHIFT))

# define KFCALL			0
# define KFCALL_LVAL		1
# define DFCALL			2
# define FCALL			3
