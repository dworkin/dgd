# define USR		"/usr"	/* default user directory */

# undef SYS_CONTINUOUS		/* off by default */
# undef SYS_DATAGRAMS		/* off by default */
# define TLS_SIZE	2	/* thread local storage */

# ifdef __ICHAT__
#  define SYS_NETWORKING	/* ichat server has networking capabilities */
# endif

# ifdef SYS_NETWORKING
#  define TELNET_PORT	6047	/* default telnet port */
#  define BINARY_PORT	6048	/* default binary port */
# endif
