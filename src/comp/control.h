extern void	 ctrl_init	P((char*));
extern void	 ctrl_inherit	P((object*, string*));
extern void	 ctrl_create	P((char*));
extern long	 ctrl_dstring	P((string*));
extern void	 ctrl_dproto	P((string*, char*));
extern void	 ctrl_dfunc	P((string*, char*));
extern void	 ctrl_dprogram	P((char*, unsigned short));
extern void	 ctrl_dvar	P((string*, unsigned short, unsigned short));
extern char	*ctrl_lfcall	P((string*, char*, long*));
extern char	*ctrl_ifcall	P((string*, long*));
extern char	*ctrl_fcall	P((string*, long*, bool));
extern short	 ctrl_var	P((string*, long*));
extern control	*ctrl_construct	P((void));
extern void	 ctrl_clear	P((void));

# define T_IMPLICIT	(T_VOID | (1 << REFSHIFT))

# define KFCALL		0
# define LFCALL		1
# define DFCALL		2
# define FCALL		3
