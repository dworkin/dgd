# include <kernel/kernel.h>
# include <kernel/user.h>

inherit LIB_USER;

# define STATE_NORMAL		0
# define STATE_LOGIN		1
# define STATE_OLDPASSWD	2
# define STATE_NEWPASSWD1	3
# define STATE_NEWPASSWD2	4

private string name;		/* user name */
private int state;		/* user object state */
private string newpasswd;	/* new password */
string password;		/* user password */
private object wiztool;		/* command handler */


static create(int clone)
{
    if (clone) {
	add_event("input");
    }
}

int login(string str)
{
    if (KERNEL()) {
	name = str;
	restore_object(DEFAULT_USER_DIR + "/" + str + ".dat");
	if (password) {
	    previous_object()->message("Password:");
	    state = STATE_LOGIN;
	} else {
	    connect(previous_object());
	    previous_object()->set_name(name);
	    wiztool = clone_object(DEFAULT_WIZTOOL, name);
	    message("Pick a new password:");
	    state = STATE_NEWPASSWD1;
	}
	return MODE_NOECHO;
    }
}

int receive_message(string str)
{
    if (KERNEL()) {
	switch (state) {
	case STATE_NORMAL:
	    if (!query_editor(wiztool)) {
		/* check standard commands */
		switch (str) {
		case "password":
		    message("Old password:\n");
		    state = STATE_OLDPASSWD;
		    return MODE_NOECHO;

		case "quit":
		    disconnect();
		    destruct_object(this_object());
		    return 0;
		}
	    }

	    /* try wiztool */
	    event("input", str);
	    break;

	case STATE_LOGIN:
	    if (crypt(str, password) != password) {
		message("\nBad password.\n");
		disconnect();
		destruct_object(this_object());
		return 0;
	    } else {
		connect(previous_object());
		previous_object()->set_name(name);
		wiztool = clone_object(DEFAULT_WIZTOOL, name);
	    }
	    break;

	case STATE_OLDPASSWD:
	    if (crypt(str, password) != password) {
		message("\nBad password.\n");
		return MODE_ECHO;
	    }
	    message("\nNew password:");
	    state = STATE_NEWPASSWD1;
	    return MODE_NOECHO;

	case STATE_NEWPASSWD1:
	    newpasswd = str;
	    message("\nRetype new password:");
	    state = STATE_NEWPASSWD2;
	    return MODE_NOECHO;

	case STATE_NEWPASSWD2:
	    if (newpasswd == str) {
		password = crypt(str);
		message("\nPassword changed.\n");
	    } else {
		message("\nMismatch; password not changed.\n");
	    }
	    newpasswd = 0;
	    break;
	}

	state = STATE_NORMAL;
	str = query_editor(wiztool);
	if (str) {
	    message((str == "insert") ? "*\b" : ":");
	} else {
	    message((name == "admin") ? "# " : "> ");
	}
	return MODE_ECHO;
    }
}

string query_name() { return name; }

int allow_subscribe_event(object obj, string event)
{
    return (event == "input" && obj->query_owner() == name);
}
