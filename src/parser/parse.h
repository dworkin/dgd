struct _parser_;

extern void	ps_del		P((struct _parser_*, lpcenv*));
extern array   *ps_parse_string	P((frame*, string*, string*, Int));
extern void	ps_save		P((struct _parser_*, lpcenv*));
