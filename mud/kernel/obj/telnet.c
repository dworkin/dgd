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
 * NAME:	receive_message()
 * DESCRIPTION:	forward a message to listeners
 */
static receive_message(string str)
{
    int result;

    result = ::receive_message(str);
    if (result != mode && (result == MODE_NOECHO || result == MODE_ECHO)) {
	send_message((mode = result) - MODE_NOECHO);
    }
}
