extern void		 tk_init	P((void));
extern void		 tk_clear	P((void));
extern bool		 tk_include	P((char*, char*, unsigned int));
extern void		 tk_endinclude	P((void));
extern unsigned short	 tk_line	P((void));
extern char		*tk_filename	P((void));
extern void		 tk_setline	P((unsigned int));
extern void		 tk_setfilename	P((char*));
extern void		 tk_header	P((int));
extern void		 tk_setpp	P((int));
extern int		 tk_gettok	P((void));
extern void		 tk_skiptonl	P((int));
extern int		 tk_expand	P((macro*));

extern char *yytext;
extern int yyleng;
extern long yynumber;
extern xfloat yyfloat;
