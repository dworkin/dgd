# include <kernel/kernel.h>
# include <kernel/user.h>

object user;		/* associated user object */
object userd;		/* user daemon */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static create()
{
    add_event("login");
    add_event("logout");
    add_event("message");
    userd = find_object(USERD);
}

/*
 * NAME:	set_user()
 * DESCRIPTION:	set the associated user object for this connection
 */
set_user(object usr)
{
    if (KERNEL()) {
	user = usr;
    }
}

/*
 * NAME:	query_user()
 * DESCRIPTION:	query the associated user object
 */
object query_user()
{
    if (KERNEL()) {
	return user;
    }
}

/*
 * NAME:	open()
 * DESCRIPTION:	open the connection
 */
static open()
{
    event("login");
}

/*
 * NAME:	close()
 * DESCRIPTION:	close the connection
 */
static close(int dest)
{
    event("logout");
    if (!dest) {
	destruct_object(this_object());
    }
}

/*
 * NAME:	allow_subscribe_event
 * DESCRIPTION:	specify which objects can listen to messages
 */
int allow_subscribe_event(object obj)
{
    return (obj == user || obj == userd);
}

/*
 * NAME:	receive_message()
 * DESCRIPTION:	forward a message to listeners
 */
static receive_message(string str)
{
    event("message", str);
}

/*
 * NAME:	message()
 * DESCRIPTION:	send a message to the other side
 */
message(mixed arg)
{
    if (previous_object() == user || previous_object() == userd) {
	send_message(arg);
    }
}
