# include <kernel/kernel.h>
# include <kernel/user.h>

inherit LIB_CONN;	/* basic connection object */


int mode;		/* input mode */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static create(int clone)
{
    if (clone) {
	::create("telnet");
	mode = MODE_ECHO;
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
    int result;

    result = ::receive_message(allocate(TLS_SIZE), str);
    if (result != mode && (result == MODE_NOECHO || result == MODE_ECHO)) {
	send_message((mode = result) - MODE_NOECHO);
    }
}
