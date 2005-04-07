# include <kernel/kernel.h>
# include <kernel/user.h>
# include <kernel/access.h>

inherit LIB_USER;
inherit user API_USER;
inherit access API_ACCESS;


# define STATE_NORMAL		0
# define STATE_LOGIN		1
# define STATE_OLDPASSWD	2
# define STATE_NEWPASSWD1	3
# define STATE_NEWPASSWD2	4

static string name;		/* user name */
static string Name;		/* capitalized user name */
static mapping state;		/* state for a connection object */
string password;		/* user password */
static string newpasswd;	/* new password */
static object wiztool;		/* command handler */
static int nconn;		/* # of connections */
static int accinit;		/* access interface initialized */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize user object
 */
static void create(int clone)
{
    if (clone) {
	user::create();
	access::create();
	accinit = TRUE;
	state = ([ ]);
    }
}

/*
 * NAME:	tell_audience()
 * DESCRIPTION:	send message to listening users
 */
private void tell_audience(string str)
{
    object *users, user;
    int i;

    users = users();
    for (i = sizeof(users); --i >= 0; ) {
	user = users[i];
	if (user != this_object() &&
	    sscanf(object_name(user), DEFAULT_USER + "#%*d") != 0) {
	    user->message(str);
	}
    }
}

/*
 * NAME:	login()
 * DESCRIPTION:	login a new user
 */
int login(string str)
{
    if (previous_program() == LIB_CONN) {
	if (nconn == 0) {
	    ::login(str);
	}
	nconn++;
	if (strlen(str) == 0 || sscanf(str, "%*s ") != 0 ||
	    sscanf(str, "%*s/") != 0) {
	    return MODE_DISCONNECT;
	}
	Name = name = str;
	if (Name[0] >= 'a' && Name[0] <= 'z') {
	    Name[0] -= 'a' - 'A';
	}
	restore_object(DEFAULT_USER_DIR + "/" + str + ".pwd");

	if (password) {
	    /* check password */
	    previous_object()->message("Password:");
	    state[previous_object()] = STATE_LOGIN;
	} else {
	    /* no password; login immediately */
	    connection(previous_object());
	    tell_audience(Name + " logs in.\n");
	    if (!accinit) {
		access::create();
		accinit = TRUE;
	    }
	    if (str != "admin" && sizeof(query_users() & ({ str })) == 0) {
		message("> ");
		state[previous_object()] = STATE_NORMAL;
		return MODE_ECHO;
	    }
	    if (!wiztool) {
		wiztool = clone_object(DEFAULT_WIZTOOL, str);
	    }
	    message("Pick a new password:");
	    state[previous_object()] = STATE_NEWPASSWD1;
	}
	return MODE_NOECHO;
    }
}

/*
 * NAME:	logout()
 * DESCRIPTION:	logout user
 */
void logout(int quit)
{
    if (previous_program() == LIB_CONN && --nconn == 0) {
	if (query_conn()) {
	    if (quit) {
		tell_audience(Name + " logs out.\n");
	    } else {
		tell_audience(Name + " disconnected.\n");
	    }
	}
	::logout();
	if (wiztool) {
	    destruct_object(wiztool);
	}
	destruct_object(this_object());
    }
}

/*
 * NAME:	receive_message()
 * DESCRIPTION:	process a message from the user
 */
int receive_message(string str)
{
    if (previous_program() == LIB_CONN) {
	string cmd;
	object user, *users;
	int i, sz;

	switch (state[previous_object()]) {
	case STATE_NORMAL:
	    cmd = str;
	    if (strlen(str) != 0 && str[0] == '!') {
		cmd = cmd[1 ..];
	    }

	    if (!wiztool || !query_editor(wiztool) || cmd != str) {
		/* check standard commands */
		if (strlen(cmd) != 0) {
		    switch (cmd[0]) {
		    case '\'':
			if (strlen(cmd) > 1) {
			    cmd[0] = ' ';
			    str = cmd;
			}
			cmd = "say";
			break;

		    case ':':
			if (strlen(cmd) > 1) {
			    cmd[0] = ' ';
			    str = cmd;
			}
			cmd = "emote";
			break;

		    default:
			sscanf(cmd, "%s ", cmd);
			break;
		    }
		}

		switch (cmd) {
		case "say":
		    if (sscanf(str, "%*s %s", str) == 0) {
			message("Usage: say <text>\n");
		    } else {
			tell_audience(Name + " says: " + str + "\n");
		    }
		    str = nil;
		    break;

		case "emote":
		    if (sscanf(str, "%*s %s", str) == 0) {
			message("Usage: emote <text>\n");
		    } else {
			tell_audience(Name + " " + str + "\n");
		    }
		    str = nil;
		    break;

		case "tell":
		    if (sscanf(str, "%*s %s %s", cmd, str) != 3 ||
			!(user=find_user(cmd))) {
			message("Usage: tell <user> <text>\n");
		    } else {
			user->message(Name + " tells you: " + str + "\n");
		    }
		    str = nil;
		    break;

		case "users":
		    users = users();
		    str = "Logged on:";
		    for (i = 0, sz = sizeof(users); i < sz; i++) {
			cmd = users[i]->query_name();
			if (cmd) {
			    str += " " + cmd;
			}
		    }
		    message(str + "\n");
		    str = nil;
		    break;

		case "password":
		    if (password) {
			message("Old password:");
			state[previous_object()] = STATE_OLDPASSWD;
		    } else {
			message("New password:");
			state[previous_object()] = STATE_NEWPASSWD1;
		    }
		    return MODE_NOECHO;

		case "quit":
		    return MODE_DISCONNECT;
		}
	    }

	    if (str) {
		if (wiztool) {
		    wiztool->input(str);
		} else if (strlen(str) != 0) {
		    message("No command: " + str + "\n");
		}
	    }
	    break;

	case STATE_LOGIN:
	    if (hash_string("crypt", str, password) != password) {
		previous_object()->message("\nBad password.\n");
		return MODE_DISCONNECT;
	    }
	    connection(previous_object());
	    message("\n");
	    tell_audience(Name + " logs in.\n");
	    if (!accinit) {
		access::create();
		accinit = TRUE;
	    }
	    if (!wiztool &&
		(name == "admin" || sizeof(query_users() & ({ name })) != 0)) {
		wiztool = clone_object(DEFAULT_WIZTOOL, name);
	    }
	    break;

	case STATE_OLDPASSWD:
	    if (hash_string("crypt", str, password) != password) {
		message("\nBad password.\n");
		break;
	    }
	    message("\nNew password:");
	    state[previous_object()] = STATE_NEWPASSWD1;
	    return MODE_NOECHO;

	case STATE_NEWPASSWD1:
	    newpasswd = str;
	    message("\nRetype new password:");
	    state[previous_object()] = STATE_NEWPASSWD2;
	    return MODE_NOECHO;

	case STATE_NEWPASSWD2:
	    if (newpasswd == str) {
		password = hash_string("crypt", str);
		if (wiztool) {
		    /* save wizards only */
		    save_object(DEFAULT_USER_DIR + "/" + name + ".pwd");
		}
		message("\nPassword changed.\n");
	    } else {
		message("\nMismatch; password not changed.\n");
	    }
	    newpasswd = nil;
	    break;
	}

	str = (wiztool) ? query_editor(wiztool) : nil;
	if (str) {
	    message((str == "insert") ? "*\b" : ":");
	} else {
	    message((name == "admin") ? "# " : "> ");
	}
	state[previous_object()] = STATE_NORMAL;
	return MODE_ECHO;
    }
}
