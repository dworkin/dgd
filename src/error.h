# define ec_push(env, cleanup)		setjmp(*_ec_push_(env, cleanup))

typedef void		(*ec_ftn)	P((frame*, Int));

extern struct _errenv_ *ec_new_env	P((void));
extern void		ec_clear	P((lpcenv*));
extern jmp_buf	       *_ec_push_	P((lpcenv*, ec_ftn));
extern void		ec_pop		P((lpcenv*));

extern void		serror		P((lpcenv*, string*));
extern string	       *errorstr	P((lpcenv*));

extern void		message		();
void			error		();
extern void		fatal		();
