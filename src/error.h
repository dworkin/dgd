# define ec_push(cleanup)	setjmp(*_ec_push_(cleanup))

typedef void  (*ec_ftn)		P((frame*, Int));

extern jmp_buf *_ec_push_	P((ec_ftn));
extern void	ec_pop		P((void));

extern char	*errormesg	P((void));

extern void	message		();
extern void	error		();
extern void	fatal		();
