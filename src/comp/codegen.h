extern void  cg_init		P((bool));
extern bool  cg_compiled	P((void));
extern char *cg_function	P((string*, node*, int, int, unsigned short,
				   unsigned short*));
extern int   cg_nfuncs		P((void));
extern void  cg_clear		P((void));
