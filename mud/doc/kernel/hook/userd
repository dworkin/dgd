void set_telnet_manager(int port, object telnetd) [System only]
void set_binary_manager(int port, object binaryd) [System only]

    Install a manager on a specific port for a telnet/binary connection, in
    which the following functions will be called:

    * object select(string str)

	Return a user object, selected by the argument string, which is the
	first line of input on that connection.

    * int query_timeout(object connection)

	Return a timeout, after which the given connection is closed if no
	user object has been associated with it yet.  If the timeout is -1,
	the connection is closed immediately.

    * string query_banner(object connection)

	Return a login banner for the given connection.  Nil can be returned to
	indicate that no banner should be shown at all.
