# include "dgd.h"
# include "interpret.h"
# include "str.h"
# include "object.h"
# include "call_out.h"

/*
 * NAME:	call_out->init()
 * DESCRIPTION:	initialize call_out handling
 */
void co_init(interval, max)
long interval;
int max;
{
}

/*
 * NAME:	call_out->new()
 * DESCRIPTION:	create a new call_out
 */
void co_new(obj, str, delay, args, nargs)
object *obj;
string *str;
long delay;
value *args;
int nargs;
{
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	remove a call_out
 */
long co_del(obj, str)
object *obj;
string *str;
{
}

/*
 * NAME:	call_out->call()
 * DESCRIPTION:	call expired call_outs
 */
void co_call()
{
}
