# include <kernel/kernel.h>
# include <kernel/user.h>

inherit LIB_CONN;	/* basic connection object */


/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static create(int clone)
{
    if (clone) {
	::create("telnet");
    }
}

/*
 * NAME:	open()
 * DESCRIPTION:	open the connection
 */
static open()
{
    ::open(allocate(TLS_SIZE));
}

/*
 * NAME:	close()
 * DESCRIPTION:	close the connection
 */
static close(int dest)
{
    ::close(allocate(TLS_SIZE), dest);
}

/*
 * NAME:	receive_message()
 * DESCRIPTION:	forward a message to listeners
 */
static receive_message(string str)
{
    int mode, newmode;

    mode = query_mode();
    newmode = ::receive_message(allocate(TLS_SIZE), str);
    if (newmode != mode && (newmode == MODE_NOECHO || newmode == MODE_ECHO)) {
	send_message(newmode - MODE_NOECHO);
    }
}

/*
 * NAME:	set_mode()
 * DESCRIPTION:	set the connection mode
 */
set_mode(int newmode)
{
    int mode;

    if (SYSTEM()) {
	mode = query_mode();
	::set_mode(newmode);
	if (newmode != mode && (newmode == MODE_NOECHO || newmode == MODE_ECHO))
	{
	    send_message(newmode - MODE_NOECHO);
	}
    }
}

/*
 * NAME:	message_done()
 * DESCRIPTION:	called when output is completed
 */
static message_done()
{
    ::message_done(allocate(TLS_SIZE));
}
