# include <kernel/user.h>
# include <status.h>

inherit LIB_USER;

static evt_login()
{
    message("\nDGD " + status()[ST_VERSION] + " (telnet)\n\nlogin: ");
}

static evt_logout()
{
    message("Goodbye.\n");
}

static evt_message(string str)
{
    message("You typed: " + str + "\n> ");
    if (str == "@quit") {
	message("Disconnecting...\n");
	disconnect();
    }
}
