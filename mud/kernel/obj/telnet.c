# include <kernel/kernel.h>
# include <kernel/user.h>

inherit LIB_CONN;	/* basic connection object */


object driver;		/* driver object */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static void create(int clone)
{
    if (clone) {
	::create("telnet");
	driver = find_object(DRIVER);
    }
}

/*
 * NAME:	open()
 * DESCRIPTION:	open the connection
 */
static int open()
{
    ::open(allocate(driver->query_tls_size()));
    return FALSE;
}

/*
 * NAME:	close()
 * DESCRIPTION:	close the connection
 */
static void close(int dest)
{
    ::close(allocate(driver->query_tls_size()), dest);
}

/*
 * NAME:	receive_message()
 * DESCRIPTION:	forward a message to listeners
 */
static void receive_message(string str)
{
    ::receive_message(allocate(driver->query_tls_size()), str);
}

/*
 * NAME:	set_mode()
 * DESCRIPTION:	set the connection mode
 */
void set_mode(int newmode)
{
    int mode;

    if (previous_program() == LIB_CONN || SYSTEM()) {
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
static void message_done()
{
    ::message_done(allocate(driver->query_tls_size()));
}
