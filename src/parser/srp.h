typedef struct _srp_ srp;

extern srp     *srp_new		P((char*));
extern void	srp_del		P((srp*));
extern srp     *srp_load	P((char*, char*, Uint));
extern bool	srp_save	P((srp*, char**, Uint*));
extern short	srp_check	P((srp*, unsigned int, unsigned short*,
				   char**));
extern short	srp_shift	P((srp*, unsigned int, unsigned int));
extern short	srp_goto	P((srp*, unsigned int, unsigned int));
