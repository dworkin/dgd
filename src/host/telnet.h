/*
 * Definitions for the TELNET protocol.
 */

# define IAC		255	/* interpret as command */
# define DONT		254	/* don't */
# define DO		253	/* do */
# define WONT		252	/* won't */
# define WILL		251	/* will */
# define SB		250	/* begin subnegotiation */
# define GA		249	/* go ahead */
# define AYT		246	/* are you there */
# define IP		244	/* interrupt process */
# define BREAK		243	/* break */
# define SE		240	/* end subnegotiation */

/* options */
# define TELOPT_ECHO	1	/* echo */
# define TELOPT_SGA	3	/* suppress go ahead */
# define TELOPT_TM	6	/* timing mark */
