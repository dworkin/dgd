# include <kernel/kernel.h>
# include <kernel/user.h>

private object userd;		/* user daemon */
private object user;		/* user object */
private string conntype;	/* connection type */
private int mode;		/* connection mode */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static create(string type)
{
    userd = find_object(USERD);
    conntype = type;
    mode = MODE_ECHO;
}


# ifdef __ICHAT__
private int dedicated;		/* object created for execution */

/*
 * NAME:	execute_program()
 * DESCRIPTION:	execute a program on the host
 */
execute_program(string cmdline)
{
    if (previous_program() == AUTO) {
	::execute_program(cmdline);
	if (!user) {
	    user = previous_object();
	    dedicated = TRUE;
	}
    }
}

/*
 * NAME:	_program_terminated()
 * DESCRIPTION:	internal version of program_terminated()
 */
private _program_terminated(mixed *tls)
{
    user->program_terminated();
}

/*
 * NAME:	program_terminated()
 * DESCRIPTION:	called when the executing program has terminated
 */
static program_terminated()
{
    _program_terminated(allocate(TLS_SIZE));
    if (dedicated) {
	destruct_object(this_object());
    }
}
# endif	/* __ICHAT__ */


# ifdef SYS_NETWORKING
/*
 * NAME:	connect()
 * DESCRIPTION:	establish an outbount connection
 */
connect(string destination, int port)
{
    if (previous_program() == AUTO) {
	::connect(destination, port);
	user = previous_object();
    }
}
# endif


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
# ifdef SYS_NETWORKING
    else {
	mode = user->login(nil);
	if (mode == MODE_DISCONNECT) {
	    destruct_object(this_object());
	} else if (mode == MODE_BLOCK) {
	    ::block_input(TRUE);
	}
    }
# endif
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
		user->logout(FALSE);
	    }
	}
	destruct_object(this_object());
    }
}

/*
 * NAME:	set_user()
 * DESCRIPTION:	set or change the user object directly
 */
set_user(object obj, string str)
{
    if (previous_program() == LIB_USER) {
	user = obj;
	mode = obj->login(str);
	if (mode == MODE_DISCONNECT) {
	    destruct_object(this_object());
	} else if (mode == MODE_BLOCK) {
	    ::block_input(TRUE);
	}
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
    if (!user) {
	user = call_other(userd, conntype + "_user", str);
	mode = user->login(str);
    } else {
	mode = user->receive_message(str);
    }
    if (mode == MODE_DISCONNECT) {
	destruct_object(this_object());
    } else if (mode == MODE_BLOCK) {
	::block_input(TRUE);
    }
    return mode;
}

/*
 * NAME:	set_mode()
 * DESCRIPTION:	set the current connection mode
 */
static set_mode(int newmode)
{
    mode = newmode;
    ::block_input(newmode == MODE_BLOCK);
}

/*
 * NAME:	query_mode()
 * DESCRIPTION:	return the current connection mode
 */
int query_mode()
{
    return mode;
}

/*
 * NAME:	block_input()
 * DESCRIPTION:	block input for this connection
 */
block_input(int flag)
{
    if (SYSTEM()) {
	::block_input(flag || mode == MODE_BLOCK);
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
	mode = user->message_done();
	if (mode == MODE_DISCONNECT) {
	    destruct_object(this_object());
	} else if (mode == MODE_BLOCK) {
	    ::block_input(TRUE);
	}
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
 * DESCRIPTION:	send a datagram across the connection
 */
int datagram(string str)
{
    if (previous_object() == user) {
	return (send_datagram(str) == strlen(str));
    }
}
# endif
