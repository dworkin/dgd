extern void	sw_init		P((char*, unsigned int, unsigned int,
				   unsigned int, Uint));
extern void	sw_finish	P((void));
extern void	sw_newv		P((sector*, unsigned int));
extern void	sw_wipev	P((sector*, unsigned int));
extern void	sw_delv		P((sector*, unsigned int));
extern void	sw_readv	P((char*, sector*, Uint, Uint));
extern void	sw_writev	P((char*, sector*, Uint, Uint));
extern void	sw_dreadv	P((char*, sector*, Uint, Uint));
extern sector	sw_mapsize	P((unsigned int));
extern sector	sw_count	P((void));
extern bool	sw_copy		P((Uint));
extern int	sw_dump		P((char*));
extern void	sw_restore	P((int, unsigned int));
