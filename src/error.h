# define ec_push(cleanup)	setjmp(*_ec_push_(cleanup))

typedef void  (*ec_ftn)		P((frame*, Int));

extern void	ec_clear	P((void));
extern jmp_buf *_ec_push_	P((ec_ftn));
extern void	ec_pop		P((void));

extern void	serror		P((string*));
extern string  *errorstr	P((void));

extern void	message		();
extern void	error		();
extern void	fatal		();
