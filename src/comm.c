# include "dgd.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "comm.h"

/*
 * NAME:	comm->send()
 * DESCRIPTION:	send a message to a user
 */
void comm_send(mess, user)
string *mess;
object *user;
{
    write(1, mess->text, mess->len);
}
