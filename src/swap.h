# ifndef H_SWAP
# define H_SWAP

typedef uindex sector;

# define SW_UNUSED	UINDEX_MAX

extern void	sw_init		P((char*, uindex, uindex, uindex));
extern void	sw_finish	P((void));
extern sector	sw_new		P((void));
extern void	sw_del		P((sector));
extern void	sw_readv	P((char*, sector*, long, long));
extern void	sw_writev	P((char*, sector*, long, long));
extern uindex	sw_mapsize	P((long));
extern uindex	sw_count	P((void));
extern void	sw_copy		P((void));
extern int	sw_dump		P((char*));
extern void	sw_restore	P((int, int));

# endif	/* H_SWAP */
