# include <kernel/kernel.h>
# include <kernel/user.h>

inherit LIB_CONN;	/* basic connection object */

int mode;		/* input mode */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static create()
{
    ::create("telnet");
    mode = MODE_ECHO;
}

/*
 * NAME:	receive_message()
 * DESCRIPTION:	forward a message to listeners
 */
static receive_message(string str)
{
    int result;

    result = ::receive_message(str);
    if (result != mode && result != 0 && result != MODE_RAW) {
	send_message((mode = result) - 1);
    }
}
