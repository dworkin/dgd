# define DFA_EOS	-1
# define DFA_REJECT	-2
# define DFA_TOOBIG	-3

extern struct _dfa_    *dfa_new		P((lpcenv*, char*));
extern void		dfa_del		P((struct _dfa_*));
extern struct _dfa_    *dfa_load	P((lpcenv*, char*, char*, Uint));
extern bool		dfa_save	P((struct _dfa_*, char**, Uint*));
extern short		dfa_scan	P((struct _dfa_*, string*, ssizet*,
					   char**, ssizet*));
