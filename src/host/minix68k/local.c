# define INCLUDE_TIME
# include "dgd.h"
# include <sgtty.h>
# include <signal.h>

extern long seed	P((long));

static struct sgttyb tty;

/*
 * NAME:	intr()
 * DESCRIPTION:	exit gracefully
 */
static void intr(arg)
int arg;
{
    host_finish();
    exit(1);
}

/*
 * NAME:	host->init()
 * DESCRIPTION:	host-specific initialisation
 */
void host_init()
{
    time_t t;

    time(&t);
    seed((long) t);
    ioctl(0, TIOCGETP, &tty);
    tty.sg_flags |= CBREAK;
    ioctl(0, TIOCSETP, &tty);
    signal(SIGINT, intr);
    signal(SIGQUIT, intr);
}

/*
 * NAME:	host->finish()
 * DESCRIPTION:	host specific tidying up
 */
void host_finish()
{
    tty.sg_flags &= ~CBREAK;
    ioctl(0, TIOCSETP, &tty);
}

/*
 * NAME:	host->error()
 * DESCRIPTION:	pass on error message to host
 */
void host_error()
{
}
