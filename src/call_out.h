struct _cbuf_;

extern bool	co_init		P((unsigned int));
extern Uint	co_check	P((lpcenv*, unsigned int, Int, unsigned int,
				   Uint*, unsigned short*, struct _cbuf_**));
extern void	co_new		P((unsigned int, unsigned int, Uint,
				   unsigned int, struct _cbuf_*));
extern Int	co_remaining	P((Uint));
extern void	co_del		P((unsigned int, unsigned int, Uint));
extern void	co_list		P((lpcenv*, array*));
extern void	co_call		P((frame*));
extern void	co_info    	P((uindex*, uindex*));
extern Uint	co_delay	P((unsigned short*));
extern void	co_swapcount	P((unsigned int));
extern long	co_swaprate1 	P((void));
extern long	co_swaprate5 	P((void));
extern bool	co_dump		P((lpcenv*, int));
extern void	co_restore	P((lpcenv*, int, Uint));
