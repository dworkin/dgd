/*
 * Definitions for the TELNET protocol.
 */

# define IAC		255	/* interpret as command */
# define DONT		254	/* don't */
# define DO		253	/* do */
# define WONT		252	/* won't */
# define WILL		251	/* will */
# define GA		249	/* go ahead */

/* options */
# define TELOPT_ECHO	1	/* echo */
