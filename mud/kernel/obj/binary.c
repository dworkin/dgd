# include <kernel/kernel.h>
# include <kernel/user.h>

inherit LIB_CONN;	/* basic connection object */


int linemode;		/* process input and output line by line? */
string buffer;		/* buffered input */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static create(int clone)
{
    if (clone) {
	::create("binary");
	linemode = TRUE;
	buffer = "";
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
    int len;
    string head;

    buffer += str;
    while (sscanf(buffer, "%*s\n") != 0 && this_object()) {
	if (linemode) {
	    if (sscanf(buffer, "%s\r\n%s", str, buffer) != 0 ||
		sscanf(buffer, "%s\n%s", str, buffer) != 0) {
		/*
		 * might not work properly if both delete and backspace
		 * are used
		 */
		while (sscanf(str, "%s\b%s", head, str) != 0) {
		    len = strlen(head);
		    if (len != 0) {
			str = head[0 .. len - 2] + str;
		    }
		}
		while (sscanf(str, "%s\x7f%s", head, str) != 0) {
		    len = strlen(head);
		    if (len != 0) {
			str = head[0 .. len - 2] + str;
		    }
		}

		linemode = (::receive_message(allocate(TLS_SIZE),
					      str) != MODE_RAW);
	    }
	} else {
	    linemode = (::receive_message(allocate(TLS_SIZE),
					  buffer) != MODE_RAW);
	    buffer = "";
	    break;
	}
    }
}

/*
 * NAME:	message()
 * DESCRIPTION:	send a message to the other side
 */
int message(string str)
{
    if (linemode) {
	str = implode(explode("\n" + str + "\n", "\n"), "\r\n");
    }
    return ::message(str);
}
