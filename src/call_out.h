typedef struct _cbuf_ cbuf;

extern void	co_init		P((unsigned int));
extern Uint	co_check	P((unsigned int, Int, unsigned int,
				   Uint*, unsigned short*, cbuf**));
extern void	co_new		P((unsigned int, object*, Uint, unsigned int,
				   cbuf*));
extern Int	co_remaining	P((Uint));
extern void	co_del		P((object*, unsigned int, Uint));
extern void	co_list		P((array*));
extern void	co_call		P((frame*));
extern void	co_info    	P((uindex*, uindex*));
extern Uint	co_delay	P((unsigned short*));
extern void	co_swapcount	P((unsigned int));
extern long	co_swaprate1 	P((void));
extern long	co_swaprate5 	P((void));
extern bool	co_dump		P((int));
extern void	co_restore	P((int, Uint));
