typedef struct _dfa_ dfa;

dfa    *dfa_new		P((char*));
void	dfa_del		P((dfa*));
dfa    *dfa_load	P((char*, string*, string*));
bool	dfa_save	P((dfa*, string**, string**));
int	dfa_lazyscan	P((dfa*, string*, unsigned int*));
