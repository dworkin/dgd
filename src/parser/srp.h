extern struct _srp_    *srp_new		P((lpcenv*, char*));
extern void		srp_del		P((struct _srp_*));
extern struct _srp_    *srp_load	P((lpcenv*, char*, char*, Uint));
extern bool		srp_save	P((struct _srp_*, char**, Uint*));
extern short		srp_check	P((struct _srp_*, unsigned short,
					   unsigned short*, char**));
extern short		srp_shift	P((struct _srp_*, unsigned short,
					   unsigned short));
extern short		srp_goto	P((struct _srp_*, unsigned short,
					   unsigned short));
