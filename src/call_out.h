extern void	co_init		P((unsigned int));
extern uindex	co_new		P((object*, string*, Int, unsigned int, frame*,
				   int));
extern Int	co_del		P((object*, unsigned int));
extern array   *co_list		P((dataspace*, object*));
extern void	co_call		P((frame*));
extern void	co_info    	P((uindex*, uindex*));
extern Uint	co_delay	P((unsigned short*));
extern void	co_swapcount	P((unsigned int));
extern long	co_swaprate1 	P((void));
extern long	co_swaprate5 	P((void));
extern bool	co_dump		P((int));
extern void	co_restore	P((int, Uint));
