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
static open(mixed *tls)
{
    int timeout;
    string banner;

    timeout = call_other(userd, "query_" + conntype + "_timeout");
    if (timeout < 0) {
	/* disconnect immediately */
	destruct_object(this_object());
	return;
    }

    if (!user) {
	if (timeout != 0) {
	    call_out("timeout", timeout);
	}

	banner = call_other(userd, "query_" + conntype + "_banner");
	if (banner) {
	    send_message(banner);
	}
    }
}

/*
 * NAME:	close()
 * DESCRIPTION:	close the connection
 */
static close(mixed *tls, int dest)
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
 * NAME:	disconnect()
 * DESCRIPTION:	break connection
 */
disconnect()
{
    if (previous_program() == LIB_USER) {
	destruct_object(this_object());
    }
}

/*
 * NAME:	reboot()
 * DESCRIPTION:	destruct connection object after a reboot
 */
reboot()
{
    if (previous_object() == userd) {
	if (user) {
	    catch {
		user->logout(0);
	    }
	}
	destruct_object(this_object());
    }
}

/*
 * NAME:	set_user()
 * DESCRIPTION:	set or change the user object directly
 */
int set_user(object obj, string str)
{
    if (KERNEL()) {
	user = obj;
	return obj->login(str);
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
static int receive_message(mixed *tls, string str)
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
 * NAME:	block_input()
 * DESCRIPTION:	block input for this connection
 */
block_input(int flag)
{
    if (SYSTEM()) {
	::block_input(flag);
    }
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
static message_done(mixed *tls)
{
    if (user) {
	user->message_done();
    }
}

# ifdef SYS_DATAGRAMS
/*
 * NAME:	receive_datagram()
 * DESCRIPTION:	forward a datagram to the user
 */
static receive_datagram(mixed *tls, string str)
{
    if (user) {
	user->receive_datagram(str);
    }
}

/*
 * NAME:	datagram()
 * DESCRIPTION:	forward a datagram to the user
 */
int datagram(string str)
{
    if (previous_object() == user) {
	return (send_datagram(str) == strlen(str));
    }
}
# endif
