typedef struct _srp_ srp;

extern srp *srp_new	P((char*));
extern void srp_del	P((srp*));
extern srp *srp_load	P((char*, string*, string*));
extern bool srp_save	P((srp*, string**, string**));
extern int  srp_check	P((srp*, int, int*, char**));
extern int  srp_shift	P((srp*, int, int));
extern int  srp_goto	P((srp*, int, int));
