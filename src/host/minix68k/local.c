# include "dgd.h"
# include <sgtty.h>
# include <signal.h>

static struct sgttyb tty;

/*
 * NAME:	intr()
 * DESCRIPTION:	exit gracefully
 */
static void intr(arg)
int arg;
{
    tty.sg_flags &= ~CBREAK;
    ioctl(0, TIOCSETP, &tty);

    exit(1);
}

/*
 * NAME:	main()
 * DESCRIPTION:	main program
 */
int main(argc, argv)
int argc;
char *argv[];
{
    int ret;

    P_srandom(P_time());
    ioctl(0, TIOCGETP, &tty);
    tty.sg_flags |= CBREAK;
    ioctl(0, TIOCSETP, &tty);
    signal(SIGINT, intr);
    signal(SIGQUIT, intr);

    ret = dgd_main(argc, argv);

    tty.sg_flags &= ~CBREAK;
    ioctl(0, TIOCSETP, &tty);

    return ret;
}

/*
 * NAME:	P->getevent()
 * DESCRIPTION:	get an event (but there are none)
 */
void P_getevent()
{
}

/*
 * NAME:	P->message()
 * DESCRIPTION:	pass on message to host
 */
void P_message(mess)
char *mess;
{
    fputs(mess, stderr);
    fflush(stderr);
}
