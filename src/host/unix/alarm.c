# include "dgd.h"
# include <signal.h>
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "interpret.h"
# include "call_out.h"

/*
 * NAME:	intr
 * DESCRIPTION:	catch an alarm interrupt
 */
static void intr(arg)
int arg;
{
    co_timeout();
}

/*
 * NAME:	P->alarm()
 * DESCRIPTION:	cause an alarm signal to occur after the given number of
 *		seconds
 */
void P_alarm(timeout)
unsigned int timeout;
{
    signal(SIGALRM, intr);
    alarm(timeout);
}
