# define USR		"/usr"	/* default user directory */

# undef SYS_PERSISTENT		/* off by default */
# undef SYS_DATAGRAMS		/* off by default */

# ifdef __SKOTOS__
#  define SYS_NETWORKING	/* Skotos server has networking capabilities */
# endif

# ifdef SYS_NETWORKING
#  define TELNET_PORT	6047	/* default telnet port */
#  define BINARY_PORT	6048	/* default binary port */
# endif
