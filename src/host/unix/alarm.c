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
 * NAME:	_alarm()
 * DESCRIPTION:	cause an alarm signal to occur after the given number of
 *		seconds
 */
void _alarm(timeout)
unsigned int timeout;
{
    struct sigvec vec;

    signal(SIGALRM, intr);
    alarm(timeout);
    sigvec(SIGALRM, (struct sigvec *) NULL, &vec);
    vec.sv_flags |= SV_INTERRUPT;
    sigvec(SIGALRM, &vec, (struct sigvec *) NULL);
}
