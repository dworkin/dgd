extern dsymbol *ctrl_symb	P((control*, char*));

# define PROTO_CLASS(prot)	((prot)[0])
# define PROTO_FTYPE(prot)	((prot)[1])
# define PROTO_NARGS(prot)	((prot)[2])
# define PROTO_ARGS(prot)	((prot) + 3)
# define PROTO_SIZE(prot)	(3 + PROTO_NARGS(prot))
