# include <kernel/kernel.h>
# include <kernel/user.h>

inherit LIB_CONN;	/* basic connection object */


string buffer;		/* buffered input */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize
 */
static create(int clone)
{
    if (clone) {
	::create("binary");
	buffer = "";
    }
}

/*
 * NAME:	open()
 * DESCRIPTION:	open the connection
 */
static int open()
{
    ::open(allocate(TLS_SIZE));
# ifdef SYS_DATAGRAMS
    return TRUE;
# else
    return FALSE;
# endif
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
    int mode, len;
    string head;

    buffer += str;
    mode = query_mode();
    while (mode != MODE_BLOCK && mode != MODE_DISCONNECT) {
	if (mode != MODE_RAW) {
	    if (sscanf(buffer, "%s\r\n%s", str, buffer) != 0 ||
		sscanf(buffer, "%s\n%s", str, buffer) != 0) {
		do {
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
		} while (sscanf(str, "%*s\b") != 0);

		mode = ::receive_message(allocate(TLS_SIZE), str);
	    } else {
		break;
	    }
	} else {
	    if (strlen(buffer) != 0) {
		str = buffer;
		buffer = "";
		::receive_message(allocate(TLS_SIZE), str);
	    }
	    break;
	}
    }
}

# ifdef SYS_DATAGRAMS
/*
 * NAME:	receive_datagram()
 * DESCRIPTION:	receive a datagram
 */
static receive_datagram(string str)
{
    ::receive_datagram(allocate(TLS_SIZE), str);
}
# endif

/*
 * NAME:	set_mode()
 * DESCRIPTION:	set the connection mode
 */
set_mode(int mode)
{
    string str;

    if (SYSTEM()) {
	::set_mode(mode);
	if (mode == MODE_RAW && strlen(buffer) != 0) {
	    /* flush buffer */
	    str = buffer;
	    buffer = "";
	    ::receive_message(nil, str);
	}
    }
}

/*
 * NAME:	message()
 * DESCRIPTION:	send a message to the other side
 */
int message(string str)
{
    if (query_mode() < MODE_RAW) {
	str = implode(explode("\n" + str + "\n", "\n"), "\r\n");
    }
    return ::message(str);
}
