# include <kernel/user.h>
# include <status.h>

inherit LIB_USER;

static evt_login()
{
    message("\r\nDGD " + status()[ST_VERSION] + " (binary)\r\n\r\nlogin: ");
}

static evt_logout()
{
    message("Goodbye.\r\n");
}

static evt_message(string str)
{
    message("You typed: " + str + "\r\n> ");
    if (str == "@quit") {
	disconnect();
    }
}
