# include "dgd.h"
# include <signal.h>

static bool timeout;		/* alarm timed out */

/*
 * NAME:	intr
 * DESCRIPTION:	catch an alarm interrupt
 */
static void intr(arg)
int arg;
{
    timeout = TRUE;
}

/*
 * NAME:	P->alarm()
 * DESCRIPTION:	cause an alarm signal to occur after the given number of
 *		seconds
 */
void P_alarm(delay)
unsigned int delay;
{
    timeout = FALSE;
    signal(SIGALRM, intr);
    alarm(delay);
}

/*
 * NAME:	P->timeout()
 * DESCRIPTION:	return the value of the timeout flag
 */
bool P_timeout()
{
    return timeout;
}
