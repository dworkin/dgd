extern void		 ctrl_init	P((char*, char*));
extern bool		 ctrl_inherit	P((object*, string*));
extern void		 ctrl_create	P((char*));
extern long		 ctrl_dstring	P((string*));
extern void		 ctrl_dproto	P((string*, char*));
extern void		 ctrl_dfunc	P((string*, char*));
extern void		 ctrl_dprogram	P((char*, unsigned short));
extern void		 ctrl_dvar	P((string*, unsigned short,
					   unsigned short));
extern char		*ctrl_ifcall	P((string*, char*, long*));
extern char		*ctrl_fcall	P((string*, long*, bool));
extern unsigned short	 ctrl_var	P((string*, long*));
extern bool		 ctrl_chkfuncs	P((char*));
extern control		*ctrl_construct	P((void));
extern void		 ctrl_clear	P((void));

# define T_IMPLICIT	(T_VOID | (1 << REFSHIFT))

# define KFCALL		0
# define DFCALL		1
# define FCALL		2
