# include "dgd.h"
# include <signal.h>

/*
 * NAME:	term()
 * DESCRIPTION:	catch SIGTERM
 */
static void term()
{
    signal(SIGTERM, term);
    interrupt();
}

/*
 * NAME:	main()
 * DESCRIPTION:	main program
 */
int main(argc, argv)
int argc;
char *argv[];
{
    P_srandom((long) P_time());
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, term);
    return dgd_main(argc, argv);
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
