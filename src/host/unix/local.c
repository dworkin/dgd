# include "dgd.h"
# include <signal.h>

/*
 * NAME:	host->init()
 * DESCRIPTION:	host-specific initialisation
 */
void host_init()
{
    P_srandom(P_time());
    signal(SIGPIPE, SIG_IGN);
}

/*
 * NAME:	host->finish()
 * DESCRIPTION:	host specific tidying up
 */
void host_finish()
{
}

/*
 * NAME:	host->error()
 * DESCRIPTION:	pass on error message to host
 */
void host_error()
{
}
