# define USR		"/usr"	/* default user directory */

# undef SYS_CONTINUOUS		/* off by default */

# ifdef __ICHAT__
#  define SYS_NETWORKING	/* ichat server has networking capabilities */
# endif

# ifdef SYS_NETWORKING
#  define TELNET_PORT	6027	/* default telnet port */
#  define BINARY_PORT	6028	/* default binary port */
# endif
