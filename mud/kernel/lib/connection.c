# include <kernel/kernel.h>
# include <kernel/user.h>

private object userd;		/* user daemon */
private object user;		/* user object */
private string conntype;	/* connection type */

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
    int timeout;
    string banner;

    timeout = call_other(userd, "query_" + conntype + "_timeout");
    if (timeout < 0) {
	/* disconnect immediately */
	destruct_object(this_object());
	return;
    }
    if (timeout != 0) {
	call_out("timeout", timeout);
    }

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
int message(string str)
{
    if (previous_object() == user) {
	return (send_message(str) == strlen(str));
    }
}

/*
 * NAME:	message_done()
 * DESCRIPTION:	called when output is completed
 */
static void message_done()
{
    if (user) {
	user->message_done();
    }
}
