typedef struct _dfa_ dfa;

# define DFA_EOS	-1
# define DFA_REJECT	-2
# define DFA_TOOBIG	-3

extern dfa     *dfa_new		P((char*));
extern void	dfa_del		P((dfa*));
extern dfa     *dfa_load	P((char*, char*, Uint));
extern bool	dfa_save	P((dfa*, char**, Uint*));
extern short	dfa_scan	P((dfa*, string*, ssizet*, char**, ssizet*));
