inherit "/kernel/lib/wiztool";

private object user;		/* associated user object */

/*
 * NAME:	init()
 * DESCRIPTION:	initialize object
 */
init()
{
    if (!user) {
	subscribe_event(user = previous_object(), "input");
    }
}

object query_user() { return user; }

/*
 * NAME:	evt_input()
 * DESCRIPTION:	handle an input event
 */
static evt_input(string str)
{
    string arg;

    if (str == "") {
	return;	/* fix later */
    }

    sscanf(str, "%s %s", str, arg);

    switch (str) {
    case "code":
    case "clear":
    case "compile":
    case "clone":
    case "destruct":

    case "cd":
    case "pwd":
    case "ls":
    case "cp":
    case "mv":
    case "rm":
    case "mkdir":
    case "rmdir":
    case "ed":
	call_other(this_object(), "cmd_" + str, user, str, arg);
	break;

    default:
	user->message("No command: " + str + "\n");
	break;
    }
}
