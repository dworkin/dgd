# ifndef H_SWAP
# define H_SWAP

typedef uindex sector;

# define SW_UNUSED	UINDEX_MAX

extern void	sw_init		P((char*, unsigned int, unsigned int,
				   unsigned int));
extern void	sw_finish	P((void));
extern sector	sw_new		P((void));
extern void	sw_del		P((unsigned int));
extern void	sw_readv	P((char*, sector*, Uint, Uint));
extern void	sw_writev	P((char*, sector*, Uint, Uint));
extern uindex	sw_mapsize	P((Uint));
extern uindex	sw_count	P((void));
extern void	sw_copy		P((void));
extern int	sw_dump		P((char*));
extern void	sw_restore	P((int, int));

# endif	/* H_SWAP */
