typedef struct _parser_ parser;

extern void	ps_del		P((parser*));
extern array   *ps_parse_string	P((frame*, string*, string*, Int));
extern void	ps_save		P((parser*));
