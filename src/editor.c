# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "interpret.h"
# include "edcmd.h"
# include "editor.h"

typedef struct _editor_ {
    cmdbuf *ed;			/* editor instance */
    struct _editor_ *next;	/* next in free list */
} editor;

static editor *editors;		/* editor table */
static editor *flist;		/* free list */
static int neditors;		/* # of editors */
static char *tmpedfile;		/* proto temporary file */
static char *outbuf;		/* output buffer */
static Uint outbufsz;		/* chars in output buffer */
static eindex newed;		/* new editor in current thread */
static bool recursion;		/* recursion in editor command */
static bool internal;		/* flag editor internal error */
static lpcenv *edenv;		/* editor environment */

/*
 * NAME:	ed->init()
 * DESCRIPTION:	initialize editor handling
 */
void ed_init(tmp, num)
char *tmp;
register int num;
{
    register editor *e, *f;

    tmpedfile = tmp;
    f = (editor *) NULL;
    neditors = num;
    if (num != 0) {
	outbuf = SALLOC(char, USHRT_MAX + 1);
	editors = SALLOC(editor, num);
	for (e = editors + num; num != 0; --num) {
	    (--e)->ed = (cmdbuf *) NULL;
	    e->next = f;
	    f = e;
	}
    }
    flist = f;
    newed = EINDEX_MAX;
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
 * NAME:	ed->clear()
 * DESCRIPTION:	allow new editor to be created
 */
void ed_clear()
{
    newed = EINDEX_MAX;
}

/*
 * NAME:	check_recursion()
 * DESCRIPTION:	check for recursion in editor commands
 */
static void check_recursion(env)
lpcenv *env;
{
    if (recursion) {
	error(env, "Recursion in editor command");
    }
}

/*
 * NAME:	ed->new()
 * DESCRIPTION:	start a new editor
 */
void ed_new(obj, env)
object *obj;
lpcenv *env;
{
    char tmp[STRINGSZ + 3];
    register editor *e;

    check_recursion(env);
    if (EINDEX(newed) != EINDEX_MAX) {
	error(env, "Too many simultaneous editors started");
    }
    e = flist;
    if (e == (editor *) NULL) {
	error(env, "Too many editor instances");
    }
    flist = e->next;
    obj->etabi = newed = e - editors;
    obj->flags |= O_EDITOR;

    sprintf(tmp, "%s%05u", tmpedfile, EINDEX(obj->etabi));
    e->ed = cb_new(tmp);
}

/*
 * NAME:	ed->del()
 * DESCRIPTION:	delete an editor instance
 */
void ed_del(obj, env)
object *obj;
lpcenv *env;
{
    register editor *e;

    check_recursion(env);
    e = &editors[EINDEX(obj->etabi)];
    cb_del(e->ed);
    if (obj->etabi == newed) {
	newed = EINDEX_MAX;
    }
    e->ed = (cmdbuf *) NULL;
    e->next = flist;
    flist = e;
    obj->flags &= ~O_EDITOR;
}

/*
 * NAME:	ed->handler()
 * DESCRIPTION:	fake error handler
 */
static void ed_handler(f, depth)
frame *f;
Int depth;
{
    /*
     * This function just exists to prevent the higher level error handler
     * from being called.
     */
}

/*
 * NAME:	ed->command()
 * DESCRIPTION:	handle an editor command
 */
string *ed_command(obj, env, cmd)
object *obj;
lpcenv *env;
char *cmd;
{
    register editor *e;
    extern void output();

    check_recursion(env);
    if (strchr(cmd, LF) != (char *) NULL) {
	error(env, "Newline in editor command");
    }

    e = &editors[EINDEX(obj->etabi)];
    outbufsz = 0;
    internal = FALSE;
    if (ec_push(env, (ec_ftn) ed_handler)) {
	e->ed->flags &= ~(CB_INSERT | CB_CHANGE);
	lb_inact(e->ed->edbuf->lb);
	recursion = FALSE;
	if (!internal) {
	    error(env, (char *) NULL);	/* pass on error */
	}
	output("%s\012", errorstr(env)->text);	/* LF */
    } else {
	edenv = env;
	recursion = TRUE;
	if (cb_command(e->ed, env, cmd)) {
	    lb_inact(e->ed->edbuf->lb);
	    recursion = FALSE;
	} else {
	    recursion = FALSE;
	    ed_del(obj, env);
	}
	ec_pop(env);
    }

    if (outbufsz == 0) {
	return (string *) NULL;
    }
    return str_new(env, outbuf, (long) outbufsz);
}

/*
 * NAME:	ed->status()
 * DESCRIPTION:	return the editor status of an object
 */
char *ed_status(obj)
object *obj;
{
    return (editors[EINDEX(obj->etabi)].ed->flags & CB_INSERT) ?
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
    Uint len;

    sprintf(buf, f, a1, a2, a3);
    len = strlen(buf);
    if (outbufsz + len > USHRT_MAX) {
	error(edenv, "Editor output string too long");
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
    error(edenv, f, a1,a2, a3);
}
