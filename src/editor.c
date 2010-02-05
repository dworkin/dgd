/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
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
	outbuf = ALLOC(char, USHRT_MAX + 1);
	editors = ALLOC(editor, num);
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
    if (EINDEX(newed) != EINDEX_MAX) {
	error("Too many simultaneous editors started");
    }
    e = flist;
    if (e == (editor *) NULL) {
	error("Too many editor instances");
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
void ed_del(obj)
object *obj;
{
    register editor *e;

    check_recursion();
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
string *ed_command(obj, cmd)
object *obj;
char *cmd;
{
    register editor *e;
    extern void output();

    check_recursion();
    if (strchr(cmd, LF) != (char *) NULL) {
	error("Newline in editor command");
    }

    e = &editors[EINDEX(obj->etabi)];
    outbufsz = 0;
    internal = FALSE;
    if (ec_push((ec_ftn) ed_handler)) {
	e->ed->flags &= ~(CB_INSERT | CB_CHANGE);
	lb_inact(e->ed->edbuf->lb);
	recursion = FALSE;
	if (!internal) {
	    error((char *) NULL);	/* pass on error */
	}
	output("%s\012", errorstr()->text);	/* LF */
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
