# include "dgd.h"

/*
 * NAME:	main()
 * DESCRIPTION:	main program
 */
int main(argc, argv)
int argc;
char *argv[];
{
    P_srandom(P_time());
    if (!isatty(fileno(stderr))) {
	P_fbinio(stderr);
    }
    return dgd_main(argc, argv);
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

/*
 * NAME:	P->getevent()
 * DESCRIPTION:	get an event (though there are none)
 */
void P_getevent()
{
}
