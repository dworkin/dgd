typedef struct _srp_ srp;

extern srp *srp_new	P((char*, unsigned int));
extern void srp_del	P((srp*));
extern int  srp_check	P((srp*, int, int, int*, char**));
extern int  srp_shift	P((srp*, int, int));
extern int  srp_goto	P((srp*, int, int));
