# include "dgd.h"
# include <signal.h>

/*
 * NAME:	main()
 * DESCRIPTION:	main program
 */
int main(argc, argv)
int argc;
char *argv[];
{
    P_srandom(P_time());
    signal(SIGPIPE, SIG_IGN);
    return dgd_main(argc, argv);
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
 * DESCRIPTION:	show message
 */
void P_message(mess)
char *mess;
{
    fputs(mess, stderr);
    fflush(stderr);
}
