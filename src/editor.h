extern void	ed_init		P((char*, int));
extern void	ed_finish	P((void));
extern void	ed_clear	P((void));
extern void	ed_new		P((object*, lpcenv*));
extern void	ed_del		P((object*, lpcenv*));
extern string  *ed_command	P((object*, lpcenv*, char*));
extern char    *ed_status	P((object*));
