# include <kernel/kernel.h>
# include <kernel/user.h>

private object userd;		/* user daemon */
private object user;		/* user object */
private string conntype;	/* connection type */
private int timeout;		/* timeout callout handle */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static create(string type)
{
    userd = find_object(USERD);
    conntype = type;
}

/*
 * NAME:	open()
 * DESCRIPTION:	open the connection
 */
static open()
{
    string banner;

    timeout = call_other(userd, "query_" + conntype + "_timeout",
			 query_ip_number(this_object()));
    if (timeout < 0) {
	/* disconnect immediately */
	destruct_object(this_object());
	return;
    }
    timeout = call_out("timeout", timeout);

    banner = call_other(userd, "query_" + conntype + "_banner");
    if (banner) {
	send_message(banner);
    }
}

/*
 * NAME:	close()
 * DESCRIPTION:	close the connection
 */
static close(int dest)
{
    rlimits (-1; -1) {
	if (user) {
	    call_other(userd, conntype + "_disconnect", user);
	    catch {
		user->logout(dest);
	    }
	}
	if (!dest) {
	    destruct_object(this_object());
	}
    }
}

/*
 * NAME:	reboot()
 * DESCRIPTION:	destruct connection object after a reboot
 */
reboot()
{
    if (previous_object() == userd) {
	close(0);
    }
}

/*
 * NAME:	set_name()
 * DESCRIPTION:	set the name of this connection
 */
set_name(string name)
{
    if (previous_object() == user) {
	userd->set_name(name, this_object());
    }
}

/*
 * NAME:	query_user()
 * DESCRIPTION:	return the associated user object
 */
object query_user()
{
    return user;
}

/*
 * NAME:	timeout()
 * DESCRIPTION:	if the connection timed out, disconnect
 */
static timeout()
{
    if (!user || user->query_conn() != this_object()) {
	destruct_object(this_object());
    }
}

/*
 * NAME:	receive_message()
 * DESCRIPTION:	forward a message to user object
 */
static int receive_message(string str)
{
    int result;

    if (!user) {
	user = call_other(userd, conntype + "_user", str);
	result = user->login(str);
    } else {
	result = user->receive_message(str);
    }
    if (result == MODE_DISCONNECT && this_object()) {
	destruct_object(this_object());
    }
    return result;
}

/*
 * NAME:	message()
 * DESCRIPTION:	send a message across the connection
 */
message(string str)
{
    if (previous_object() == user) {
	send_message(str);
    }
}
