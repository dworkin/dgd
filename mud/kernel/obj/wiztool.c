# include <kernel/kernel.h>
# include <kernel/user.h>

inherit LIB_WIZTOOL;


private object user;		/* associated user object */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize object
 */
static create(int clone)
{
    if (clone) {
	::create(200);
	subscribe_event(user = this_user(), "input");
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

    if (query_editor(this_object())) {
	str = editor(str);
	if (str) {
	    user->message(str);
	}
	return;
    }

    if (str == "") {
	return;	/* fix later */
    }

    sscanf(str, "%s %s", str, arg);

    switch (str) {
    case "code":
    case "history":
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

    case "access":
    case "grant":
	call_other(this_object(), "cmd_" + str, user, str, arg);
	break;

    default:
	user->message("No command: " + str + "\n");
	break;
    }
}
