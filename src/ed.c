# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "edcmd.h"
# include "ed.h"

typedef union _editor_ {
    cmdbuf *ed;			/* editor instance */
    union _editor_ *next;	/* next in free list */
} editor;

static editor *editors;		/* editor table */
static editor *flist;		/* free list */
static char *tmpedfile;		/* proto temporary file */
static bool recursion;		/* recursion in editor command */

/*
 * NAME:	ed->init()
 * DESCRIPTION:	initialize editor handling
 */
void ed_init(tmp, num)
char *tmp;
int num;
{
    register int i;
    register editor *e, *f;

    tmpedfile = tmp;
    editors = ALLOC(editor, num);
    f = (editor *) NULL;
    for (i = num, e = editors + i; i > 0; --i) {
	(--e)->next = f;
	f = e;
    }
    flist = f;
}

/*
 * NAME:	check_recursion()
 * DESCRIPTION:	check for recursion in editor commands
 */
static void check_recursion()
{
    if (recursion) {
	error("Recursion in editor command");
    }
}

/*
 * NAME:	ed->new()
 * DESCRIPTION:	start a new editor
 */
void ed_new(obj)
object *obj;
{
    char tmp[STRINGSZ + 3];
    register editor *e;

    check_recursion();
    e = flist;
    if (e == (editor *) NULL) {
	error("Too many editor instances");
    }
    flist = e->next;
    obj->eduser = e - editors;
    obj->flags |= O_EDITOR;

    sprintf(tmp, "%s%03d", tmpedfile, UCHAR(obj->eduser));
    e->ed = cb_new(tmp);
}

/*
 * NAME:	ed->del()
 * DESCRIPTION:	delete an editor instance
 */
void ed_del(obj)
object *obj;
{
    register editor *e;

    check_recursion();
    e = &editors[UCHAR(obj->eduser)];
    cb_del(e->ed);
    e->next = flist;
    flist = e;
    obj->flags &= ~O_EDITOR;
}

/*
 * NAME:	ed->command()
 * DESCRIPTION:	handle an editor command
 */
void ed_command(obj, cmd)
object *obj;
char *cmd;
{
    register editor *e;

    check_recursion();
    if (strchr(cmd, '\n') != (char *) NULL) {
	error("Newline in editor command");
    }
    e = &editors[UCHAR(obj->eduser)];
    if (ec_push()) {
	e->ed->flags &= ~(CB_INSERT | CB_CHANGE);
	lb_inact(e->ed->edbuf->lb);
	recursion = FALSE;
	output("%s\n", errormesg());
	return;
    }
    recursion = TRUE;
    if (cb_command(e->ed, cmd)) {
	lb_inact(e->ed->edbuf->lb);
	recursion = FALSE;
    } else {
	recursion = FALSE;
	ed_del(obj);
    }
    ec_pop();
}

/*
 * NAME:	ed->status()
 * DESCRIPTION:	return the editor status of an object
 */
char *ed_status(obj)
object *obj;
{
    return (editors[UCHAR(obj->eduser)].ed->flags & CB_INSERT) ?
	    "insert" : "command";
}

/*
 * NAME:	output()
 * DESCRIPTION:	handle output from the editor
 */
void output(f, a1, a2, a3)
char *f, *a1, *a2, *a3;
{
    char buf[2 * MAX_LINE_SIZE + 15];

    i_check_stack(1);
    sprintf(buf, f, a1, a2, a3);
    (--sp)->type = T_STRING;
    str_ref(sp->u.string = str_new(buf, (long) strlen(buf)));
    if (i_call(i_this_object(), "receive_message", TRUE, 1)) {
	i_del_value(sp++);
    }
}
