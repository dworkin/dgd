# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "edcmd.h"
# include "ed.h"

typedef struct _editor_ {
    cmdbuf *ed;			/* editor instance */
    struct _editor_ *next;	/* next in free list */
} editor;

# define EOUTBUFSZ	20480	/* 20 K */

static editor *editors;		/* editor table */
static editor *flist;		/* free list */
static int neditors;		/* # of editors */
static char *tmpedfile;		/* proto temporary file */
static char *outbuf;		/* output buffer */
static int outbufsz;		/* chars in output buffer */
static bool recursion;		/* recursion in editor command */
static bool internal;		/* flag editor internal error */

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
    editors = ALLOC(editor, neditors = num);
    f = (editor *) NULL;
    for (i = num, e = editors + i; i > 0; --i) {
	(--e)->ed = (cmdbuf *) NULL;
	e->next = f;
	f = e;
    }
    flist = f;
}

/*
 * NAME:	ed->finish()
 * DESCRIPTION:	terminate all editor sessions
 */
void ed_finish()
{
    register int i;
    register editor *e;

    for (i = neditors, e = editors; i > 0; --i, e++) {
	if (e->ed != (cmdbuf *) NULL) {
	    cb_del(e->ed);
	}
    }
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
    e->ed = (cmdbuf *) NULL;
    e->next = flist;
    flist = e;
    obj->flags &= ~O_EDITOR;
}

/*
 * NAME:	ed->command()
 * DESCRIPTION:	handle an editor command
 */
string *ed_command(obj, cmd)
object *obj;
char *cmd;
{
    register editor *e;
    char buffer[EOUTBUFSZ];
    extern void output();

    check_recursion();
    if (strchr(cmd, LF) != (char *) NULL) {
	error("Newline in editor command");
    }

    e = &editors[UCHAR(obj->eduser)];
    outbuf = buffer;
    outbufsz = 0;
    internal = FALSE;
    if (ec_push()) {
	e->ed->flags &= ~(CB_INSERT | CB_CHANGE);
	lb_inact(e->ed->edbuf->lb);
	recursion = FALSE;
	if (!internal) {
	    error((char *) NULL);	/* pass on error */
	}
	output("%s\012", errormesg());	/* LF */
    } else {
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

    if (outbufsz == 0) {
	return (string *) NULL;
    }
    return str_new(outbuf, (long) outbufsz);
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
    int len;

    sprintf(buf, f, a1, a2, a3);
    len = strlen(buf);
    if (outbufsz + len > EOUTBUFSZ) {
	error("Editor output string too long");
    }
    memcpy(outbuf + outbufsz, buf, len);
    outbufsz += len;
}

/*
 * NAME:	ed_error()
 * DESCRIPTION:	handle an editor internal error
 */
void ed_error(f, a1, a2, a3)
char *f, *a1, *a2, *a3;
{
    if (f != (char *) NULL) {
	internal = TRUE;
    }
    error(f, a1,a2, a3);
}
