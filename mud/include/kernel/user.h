# define USERD			"/kernel/sys/userd"
# define LIB_CONN		("/kernel" + INHERITABLE_SUBDIR + "connection")
# define LIB_USER		("/kernel" + INHERITABLE_SUBDIR + "user")
# define LIB_WIZTOOL		("/kernel" + INHERITABLE_SUBDIR + "wiztool")
# define TELNET_CONN		("/kernel" + CLONABLE_SUBDIR + "telnet")
# define BINARY_CONN		("/kernel" + CLONABLE_SUBDIR + "binary")
# define API_USER		("/kernel" + INHERITABLE_SUBDIR + "api/user")

# define DEFAULT_USER		("/kernel" + CLONABLE_SUBDIR + "user")
# define DEFAULT_WIZTOOL	("/kernel" + CLONABLE_SUBDIR + "wiztool")
# define DEFAULT_USER_DIR	"/kernel/data"

# define MODE_DISCONNECT	0
# define MODE_NOECHO		1	/* telnet */
# define MODE_ECHO		2	/* telnet */
# define MODE_LINE		2	/* binary */
# define MODE_RAW		3	/* binary */
# define MODE_NOCHANGE		4	/* telnet + binary */
# define MODE_UNBLOCK		5	/* unblock to previous mode */
# define MODE_BLOCK		6	/* block input */

# define DEFAULT_TIMEOUT	120	/* two minutes */
