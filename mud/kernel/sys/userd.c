# include <kernel/kernel.h>
# include <kernel/user.h>

static create()
{
    compile_object(CONNECTION);
    compile_object(TELNET_USER);
}

object *query_users() {}

object new_telnet_user()
{
    if (KERNEL()) {
	object conn, user;

	conn = clone_object(CONNECTION);
	user = clone_object(TELNET_USER);
	conn->set_user(user);
	user->set_conn(conn);
	return conn;
    }
}

object new_binary_user()
{
    if (KERNEL()) {
	object conn;

	conn = clone_object("/kernel/obj/connection");
	conn->set_user(clone_object("/kernel/obj/binary"));
	return conn;
    }
}

logout(object connection)
{
    if (KERNEL()) {
    }
}
