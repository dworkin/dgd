# define INCLUDE_CTYPE
# include "ed.h"
# include "edcmd.h"

/*
 * This file contains the command parsing functions.
 */

/*
 * These functions are in cmdsub.c
 */
extern Int  cb_search	P((cmdbuf*, Int, Int, int));
extern int  cb_print	P((cmdbuf*));
extern int  cb_list	P((cmdbuf*));
extern int  cb_number	P((cmdbuf*));
extern int  cb_page	P((cmdbuf*));
extern int  cb_assign	P((cmdbuf*));
extern int  cb_mark	P((cmdbuf*));
extern int  cb_append	P((cmdbuf*));
extern int  cb_insert	P((cmdbuf*));
extern int  cb_change	P((cmdbuf*));
extern int  cb_delete	P((cmdbuf*));
extern int  cb_copy	P((cmdbuf*));
extern int  cb_move	P((cmdbuf*));
extern int  cb_put	P((cmdbuf*));
extern int  cb_yank	P((cmdbuf*));
extern int  cb_lshift	P((cmdbuf*));
extern int  cb_rshift	P((cmdbuf*));
extern int  cb_indent	P((cmdbuf*));
extern int  cb_join	P((cmdbuf*));
extern int  cb_subst	P((cmdbuf*));
extern int  cb_file	P((cmdbuf*));
extern int  cb_read	P((cmdbuf*));
extern int  cb_edit	P((cmdbuf*));
extern int  cb_quit	P((cmdbuf*));
extern int  cb_write	P((cmdbuf*));
extern int  cb_wq	P((cmdbuf*));
extern int  cb_xit	P((cmdbuf*));
extern int  cb_set	P((cmdbuf*));


/*
 * NAME:	cmdbuf->new()
 * DESCRIPTION:	create and initialize a command edit buffer
 */
cmdbuf *cb_new(tmpfile)
char *tmpfile;
{
    register cmdbuf *cb;

    m_static();
    cb = ALLOC(cmdbuf, 1);
    memset(cb, '\0', sizeof(cmdbuf));
    cb->edbuf = eb_new(tmpfile);
    cb->regexp = rx_new();
    cb->vars = va_new();
    m_dynamic();

    cb->this = 0;
    cb->undo = (block) -1;	/* not 0! */
    return cb;
}

/*
 * NAME:	cmdbuf->del()
 * DESCRIPTION:	delete a command edit buffer
 */
void cb_del(cb)
register cmdbuf *cb;
{
    eb_del(cb->edbuf);
    rx_del(cb->regexp);
    va_del(cb->vars);
    FREE(cb);
}

/*
 * NAME:	skipst()
 * DESCRIPTION:	skip white space in a string. return a pointer to the first
 *		character after the white space (could be '\0')
 */
char *skipst(p)
register char *p;
{
    while (*p == ' ' || *p == HT) {
	p++;
    }
    return p;
}

/*
 * NAME:	pattern()
 * DESCRIPTION:	scan a pattern and copy it to a buffer.
 */
char *pattern(pat, delim, buffer)
char *pat, *buffer;
int delim;
{
    register char *p;
    register unsigned int size;

    p = pat;
    while (*p != '\0') {
	if (*p == delim) {
	    break;
	}
	switch (*p++) {
	case '\\':
	    if (*p != '\0') {
		p++;
	    }
	    break;

	case '[':
	    while (*p != '\0' && *p != ']') {
		if (*p++ == '\\') {
		    if (*p == '\0') {
			break;
		    }
		    p++;
		}
	    }
	    break;
	}
    }
    size = p - pat;
    if (size >= STRINGSZ) {
	error("Regular expression too large");
    }
    if (size > 0) {
	memcpy(buffer, pat, size);
    }
    buffer[size] = '\0';
    if (*p != '\0') {
	p++;
    }
    return p;
}

/*
 * NAME:	cmdbuf->pattern()
 * DESCRIPTION:	compile a regular expression, up to a delimeter
 */
static void cb_pattern(cb, delim)
register cmdbuf *cb;
char delim;
{
    char buffer[STRINGSZ];

    cb->cmd = pattern(cb->cmd, delim, buffer);
    if (buffer[0] == '\0') {
	if (!cb->regexp->valid) {
	    error("No previous regular expression");
	}
    } else {
	char *err;

	err = rx_comp(cb->regexp, buffer);
	if (err != (char *) NULL) {
	    error(err);
	}
    }
}

/*
 * NAME:	cmdbuf->address()
 * DESCRIPTION:	parse an address. First is the first line to search from if the
 *		address is a search pattern.
 */
static Int cb_address(cb, first)
register cmdbuf *cb;
Int first;
{
    register Int l;
    register char *p;

    l = 0;

    switch (*(p = cb->cmd)) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	do {
	    l *= 10;
	    l += *p++ - '0';
	} while (isdigit(*p));
	cb->cmd = p;
	break;

    case '.':
	cb->cmd++;
    case '+':
    case '-':
	l = cb->this;
	break;

    case '$':
	l = cb->edbuf->lines;
	cb->cmd++;
	break;

    case '\'':
	if (islower(*++p)) {
	    l = cb->mark[*p - 'a'];
	} else {
	    error("Marks are a-z");
	}
	if (l == 0) {
	    error("Undefined mark referenced");
	}
	cb->cmd += 2;
	break;

    case '/':
	cb->cmd++;
	cb_pattern(cb, *p);
	l = cb->edbuf->lines;
	if (l == 0) {
	    return 1;	/* force out-of-range error */
	}
	if (first < l) {
	    l = cb_search(cb, first + 1, l, FALSE);
	    if (l != 0) {
		break;
	    }
	}
	l = cb_search(cb, (Int) 1, first, FALSE);
	if (l == 0) {
	    error("Pattern not found");
	}
	break;

    case '?':
	cb->cmd++;
	cb_pattern(cb, *p);
	if (first > 1) {
	    l = cb_search(cb, (Int) 1, first - 1, TRUE);
	}
	if (l == 0) {
	    l = cb->edbuf->lines;
	    if (l == 0) {
		return 1;	/* force out-of-range error */
	    }
	    l = cb_search(cb, first, l, TRUE);
	    if (l == 0) {
		error("Pattern not found");
	    }
	}
	break;

    default:
	return -1;
    }

    cb->cmd = skipst(cb->cmd);
    while (cb->cmd[0] == '+' || cb->cmd[0] == '-') {
	register Int r;

	p = skipst(cb->cmd + 1);
	if (!isdigit(*p)) {
	    r = 1;
	} else {
	    r = 0;
	    do {
		r *= 10;
		r += *p++ - '0';
	    } while (isdigit(*p));
	}
	if (cb->cmd[0] == '+') {
	    l += r;
	} else {
	    l -= r;
	}
	cb->cmd = skipst(p);
    }
    if (l < 0) {
	error("Negative address");
    }
    return l;
}

/*
 * NAME:	cmdbuf->range()
 * DESCRIPTION:	parse line range from the command buffer. Valid range for an
 *		address is 0-$
 */
static void cb_range(cb)
register cmdbuf *cb;
{
    cb->first = -1;
    cb->last = -1;
    cb->cmd = skipst(cb->cmd);
    if (cb->cmd[0] == '\0') {
	return;
    }
    if (cb->cmd[0] == '%') {
	cb->first = 1;
	cb->last = cb->edbuf->lines;
	cb->cmd = skipst(cb->cmd + 1);
    } else {
	cb->first = cb_address(cb, cb->this);
	if (cb->cmd[0] == ',' || cb->cmd[0] == ';') {
	    if (cb->first < 0) {
		cb->first = cb->this;
	    } else if (cb->cmd[0] == ';') {
		cb->this = cb->first;
	    }
	    cb->cmd = skipst(cb->cmd + 1);
	    cb->last = cb_address(cb, cb->this);
	    if (cb->last < 0) {
		cb->last = cb->this;
	    }
	}
    }
}

/*
 * NAME:	cmdbuf->count()
 * DESCRIPTION:	set the line range according to the (optional) count in the
 *		command buffer
 */
void cb_count(cb)
register cmdbuf *cb;
{
    register char *p;

    p = cb->cmd;
    if (isdigit(*p)) {
	register Int count;

	count = 0;
	do {
	    count *= 10;
	    count += *p++ - '0';
	} while (isdigit(*p));
	cb->cmd = skipst(p);

	if (cb->first < 0) {
	    cb->first = cb->this;
	}
	cb->last = cb->first + count - 1;
	if (cb->last < cb->first || cb->last > cb->edbuf->lines) {
	    error("Not that many lines in buffer");
	}
    }
}

/*
 * NAME:	not_in_global()
 * DESCRIPTION:	error if currently executing a global command
 */
void not_in_global(cb)
cmdbuf *cb;
{
    if (cb->flags & CB_GLOBAL) {
	error("Command not allowed in global");
    }
}

/*
 * NAME:	cmdbuf->do()
 * DESCRIPTION:	copy the present command buffer status in the undo buffer
 */
void cb_do(cb, this)
register cmdbuf *cb;
Int this;
{
    cb->undo = cb->edbuf->buffer;
    cb->uthis = this;
    memcpy(cb->umark, cb->mark, sizeof(cb->mark));
}

/*
 * NAME:	cmdbuf->undo()
 * DESCRIPTION:	undo the effects of a previous command by exchanging the
 *		command buffer status with the undo buffer
 */
int cb_undo(cb)
register cmdbuf *cb;
{
    block b;
    Int this, mark[26];

    not_in_global(cb);
    if (cb->undo == (block) -1) {
	error("Nothing to undo");
    }

    b = cb->undo;
    cb->undo = cb->edbuf->buffer;
    cb->edbuf->lines = (b == (block) 0) ? 0 : bk_size(cb->edbuf->lb, b);
    cb->edbuf->buffer = b;

    this = cb->uthis;
    if (this == 0 && b != (block) 0) {
	this = 1;
    }
    cb->uthis = cb->othis;
    cb->this = cb->othis = this;

    memcpy(mark, cb->umark, sizeof(mark));
    memcpy(cb->umark, cb->mark, sizeof(mark));
    memcpy(cb->mark, mark, sizeof(mark));

    cb->edit++;
    return RET_FLAGS;
}

/*
 * NAME:	cmdbuf->buf()
 * DESCRIPTION:	put a block in the appropriate buffers
 */
void cb_buf(cb, b)
register cmdbuf *cb;
register block b;
{
    if (isupper(cb->a_buffer)) {
	register block *zbuf;

	/*
	 * copy or append to named buffer
	 */
	zbuf = &cb->zbuf[cb->a_buffer - 'A'];
	if (*zbuf != (block) 0) {
	    *zbuf = bk_cat(cb->edbuf->lb, *zbuf, b);
	} else {
	    *zbuf = b;
	}
    } else if (islower(cb->a_buffer)) {
	cb->zbuf[cb->a_buffer - 'a'] = b;
    }
    /*
     * always put it in the default yank buffer too
     */
    cb->buf = b;
}


/*
 * Commands are allowed to affect the first or the last or all of the next lines
 * to be examined by a global command. The lines must stay in a contiguous
 * block.
 */

/*
 * NAME:	add()
 * DESCRIPTION:	add a block of lines to the edit buffer
 */
void add(cb, ln, b, size)
register cmdbuf *cb;
register Int ln, size;
block b;
{
    register Int *m;

    /* global checks */
    if (cb->flags & CB_GLOBAL) {
	if (ln < cb->glob_next) {
	    cb->glob_next += size;
	} else if (ln < cb->glob_next + cb->glob_size - 1) {
	    error("Illegal add in global");
	}
    }

    eb_put(cb->edbuf, ln, b);

    /* adjust marks of lines after new block */
    for (m = cb->mark; m < &cb->mark[26]; m++) {
	if (*m > ln) {
	    *m += size;
	}
    }

    cb->this = cb->othis = ln + size;
}

/*
 * NAME:	delete()
 * DESCRIPTION:	delete a block of lines from the edit buffer
 */
block delete(cb, first, last)
register cmdbuf *cb;
register Int first, last;
{
    register Int size, *m;

    size = last - first + 1;

    /* global checks */
    if (cb->flags & CB_GLOBAL) {
	if (last < cb->glob_next) {
	    cb->glob_next -= size;
	} else if (first <= cb->glob_next) {
	    cb->glob_size -= last - cb->glob_next + 1;
	    cb->glob_next = first;
	} else if (last >= cb->glob_next + cb->glob_size) {
	    cb->glob_size = first - cb->glob_next;
	} else {
	    error("Illegal delete in global");
	}
    }

    /* adjust & erase marks */
    for (m = cb->mark; m < &cb->mark[26]; m++) {
	if (*m >= first) {
	    if (*m > last) {
		*m -= size;
	    } else {
		*m = 0;
	    }
	}
    }

    cb->othis = first;
    if (last == cb->edbuf->lines) {
	cb->othis--;
    }
    cb->this = cb->othis;

    return eb_delete(cb->edbuf, first, last);
}

/*
 * NAME:	change()
 * DESCRIPTION:	replace a subrange of lines by a block
 */
void change(cb, first, last, b)
register cmdbuf *cb;
register Int first, last;
block b;
{
    register Int offset, *m;

    offset = last - first + 1;
    if (b != (block) 0) {
	offset -= bk_size(cb->edbuf->lb, b);
    }

    /* global checks */
    if (cb->flags & CB_GLOBAL) {
	if (last < cb->glob_next) {
	    cb->glob_next -= offset;
	} else if (first <= cb->glob_next) {
	    cb->glob_size -= last - cb->glob_next + 1;
	    cb->glob_next = last - offset + 1;
	} else if (last >= cb->glob_next + cb->glob_size) {
	    cb->glob_size = first - cb->glob_next;
	} else {
	    error("Illegal change in global");
	}
    }

    /* adjust marks. If the marks of the changed lines have to be erased,
       the calling routine must handle it. */
    for (m = cb->mark; m < &cb->mark[26]; m++) {
	if (*m > last) {
	    *m -= offset;
	}
    }

    cb->othis = first;
    cb->this = last - offset;
    if (cb->this == 0 && last != cb->edbuf->lines) {
	cb->this = 1;
    }

    eb_change(cb->edbuf, first, last, b);
}


/*
 * NAME:	startblock()
 * DESCRIPTION:	start a block of lines
 */
void startblock(cb)
cmdbuf *cb;
{
    eb_startblock(cb->edbuf);
}

/*
 * NAME:	addblock()
 * DESCRIPTION:	add a line to the current block of lines
 */
void addblock(cb, text)
cmdbuf *cb;
char *text;
{
    eb_addblock(cb->edbuf, text);
}

/*
 * NAME:	endblock()
 * DESCRIPTION:	finish the current block
 */
void endblock(cb)
register cmdbuf *cb;
{
    eb_endblock(cb->edbuf);

    if (cb->flags & CB_CHANGE) {
	if (cb->first <= cb->last) {
	    change(cb, cb->first, cb->last, cb->edbuf->flines);
	} else if (cb->first == 0 && cb->edbuf->lines != 0) {
	    cb->this = cb->othis = 1;
	} else {
	    cb->this = cb->othis = cb->first;
	}
    } else {
	if (cb->edbuf->flines != (block) 0) {
	    add(cb, cb->first, cb->edbuf->flines,
		bk_size(cb->edbuf->lb, cb->edbuf->flines));
	} else if (cb->first == 0 && cb->edbuf->lines != 0) {
	    cb->this = cb->othis = 1;
	} else {
	    cb->this = cb->othis = cb->first;
	}
    }

    cb->flags &= ~(CB_INSERT | CB_CHANGE);
    cb->edit++;
}


/*
 * NAME:	find()
 * DESCRIPTION:	match a pattern in a global command
 */
static void find(ptr, text)
char *ptr, *text;
{
    register cmdbuf *cb;

    cb = (cmdbuf *) ptr;
    cb->glob_next++;
    cb->glob_size--;
    if (rx_exec(cb->glob_rx, text, 0, cb->ignorecase) != cb->reverse) {
	longjmp(cb->env, TRUE);
    }
}

/*
 * NAME:	cmdbuf->global()
 * DESCRIPTION:	do a global command
 */
int cb_global(cb)
register cmdbuf *cb;
{
    register char *p;
    char buffer[STRINGSZ], delim;
    block undo;
    Int uthis, umark[26];
    bool aborted;

    not_in_global(cb);	/* no recursion please */

    /* get the regular expression */
    delim = cb->cmd[0];
    if (delim != '\0' && !isalnum(delim)) {
	cb->cmd = pattern(cb->cmd + 1, delim, buffer);
    } else {
	buffer[0] = '\0';
    }
    if (buffer[0] == '\0') {
	error("Missing regular expression for global");
    }

    /* keep global undo status */
    undo = cb->edbuf->buffer;
    uthis = cb->first;
    memcpy(umark, cb->mark, sizeof(cb->mark));

    /*
     * A local error context is created, so the regular expression buffer
     * can be deallocated in case of an error.
     */
    cb->glob_rx = rx_new();
    if (!ec_push((ec_ftn) NULL)) {
	/* compile regexp */
	p = rx_comp(cb->glob_rx, buffer);
	if (p != (char *) NULL) {
	    error(p);
	}

	/* get the command to be done in global */
	p = skipst(cb->cmd);
	cb->cmd = p + strlen(p);
	if (*p == '\0') {
	    p = "p";	/* default: print lines */
	}
	cb->flags |= CB_GLOBAL;
	cb->reverse = (cb->flags & CB_EXCL) != 0;
	cb->ignorecase = IGNORECASE(cb->vars);
	cb->glob_next = cb->first;
	cb->glob_size = cb->last - cb->first + 1;

	do {
	    if (setjmp(cb->env)) {
		/* found: do the commands */
		cb->this = cb->glob_next - 1;
		cb_command(cb, p);
	    } else {
		/* search */
		eb_range(cb->edbuf, cb->glob_next,
			 cb->glob_next + cb->glob_size - 1, find, (char *) cb,
			 FALSE);
	    }
	} while (cb->glob_size > 0);

	/* pop error context */
	ec_pop();
	aborted = FALSE;
    } else {
	aborted = TRUE;
    }
    /* come here if global is finished or in case of an error */

    /* clean up regular expression */
    rx_del(cb->glob_rx);

    /* set undo status */
    cb->undo = undo;
    cb->uthis = uthis;
    memcpy(cb->umark, umark, sizeof(umark));

    /* no longer in global */
    cb->flags &= ~CB_GLOBAL;

    if (aborted) {
	error((char *) NULL);
    }
    return 0;
}

/*
 * NAME:	cmdbuf->vglobal()
 * DESCRIPTION:	v == g!
 */
int cb_vglobal(cb)
cmdbuf *cb;
{
    cb->flags |= CB_EXCL;
    return cb_global(cb);
}


typedef struct {
    char flags;		/* type of command */
    char chr;		/* first char of command */
    char *cmd;		/* full command string */
    int (*ftn)();	/* command function */
} cmd;

# define CM_LNMASK	0x03
# define CM_LNNONE	0x00	/* range checking in function */
# define CM_LN0		0x01	/* (.)0  */
# define CM_LNDOT	0x02	/* (.,.) */
# define CM_LNRNG	0x03	/* (1,$) */

# define CM_EXCL	0x04

# define CM_BUFFER	0x10	/* buffer argument */
# define CM_ADDR	0x20	/* address argument */
# define CM_COUNT	0x40	/* count argument */

static cmd ed_commands[] = {
    { CM_LN0,				'a', "append",	cb_append },
# define CM_ASSIGN 1
    { CM_LNNONE,			'=', (char *) NULL,
							cb_assign },
    { CM_LNDOT | CM_COUNT,		'c', "change",	cb_change },
    { CM_LNDOT | CM_BUFFER | CM_COUNT, 	'd', "delete",	cb_delete },
    { CM_LNNONE | CM_EXCL,		'e', "edit",	cb_edit },
    { CM_LNNONE,			'f', "file",	cb_file },
    { CM_LNRNG | CM_EXCL,		'g', "global",	cb_global },
    { CM_LN0 | CM_BUFFER,		 0,  "put",	cb_put },
    { CM_LN0,				'i', "insert",	cb_insert },
    { CM_LNNONE | CM_EXCL | CM_COUNT, 	'j', "join",	cb_join },
    { CM_LNDOT,				'k', "mark",	cb_mark },
    { CM_LNDOT | CM_COUNT,		'l', "list",	cb_list },
    { CM_LNDOT | CM_ADDR,		'm', "move",	cb_move },
# define CM_NUMBER 13
    { CM_LNDOT | CM_COUNT,		'#', "number",	cb_number },
    { CM_LNRNG | CM_EXCL,		 0,  "wq",	cb_wq },
    { CM_LNDOT | CM_COUNT,		'p', "print",	cb_print },
    { CM_LNNONE | CM_EXCL,		'q', "quit",	cb_quit },
    { CM_LN0,				'r', "read",	cb_read },
    { CM_LNDOT,				's', "substitute",
							cb_subst },
    { CM_LNDOT | CM_ADDR,		't', "copy",	cb_copy },
    { CM_LNNONE,			'u', "undo",	cb_undo },
    { CM_LNRNG,				'v', (char *) NULL,
							cb_vglobal },
    { CM_LNRNG | CM_EXCL,		'w', "write",	cb_write },
    { CM_LNRNG,				'x', "xit",	cb_xit },
    { CM_LNDOT | CM_BUFFER | CM_COUNT,	'y', "yank",	cb_yank },
    { CM_LNNONE,			'z', (char *) NULL,
							cb_page },
    { CM_LNNONE,			 0,  "set",	cb_set },
# define CM_LSHIFT	27
    { CM_LNDOT | CM_COUNT,		'<', (char *) NULL,
							cb_lshift },
# define CM_RSHIFT	28
    { CM_LNDOT | CM_COUNT,		'>', (char *) NULL,
							cb_rshift },
# define CM_INDENT	29
    { CM_LNRNG,				'I', (char *) NULL,
							cb_indent },
};

# define NR_CMD		27	/* not including <, > and I */


/*
 * NAME:	cmdbuf->command()
 * DESCRIPTION:	Parse and execute an editor command. Return TRUE if this command
 *		did not terminate the editor. Multiple commands may be
 *		specified, separated by |
 */
bool cb_command(cb, command)
register cmdbuf *cb;
char *command;
{
    cb->cmd = command;

    for (;;) {
	if (cb->flags & CB_INSERT) {
	    /* insert mode */
	    if (strlen(command) >= MAX_LINE_SIZE) {
		endblock(cb);
		error("Line too long");
	    }
	    if (strcmp(command, ".") == 0) {
		/* finish block */
		endblock(cb);
	    } else {
		/* add the "command" to the current block */
		addblock(cb, command);
	    }
	} else {
	    register cmd *cm;
	    register char *p;
	    int ltype, ret;

	    cb->flags &= ~(CB_EXCL | CB_NUMBER | CB_LIST);

	    cb->a_addr = -1;
	    cb->a_buffer = 0;

	    /*
	     * parse the command line: [range] [command] [arguments]
	     */

	    cb->cmd = skipst(cb->cmd);
	    if (cb->cmd[0] == '\0') {
		/* no command: print next line */
		if (cb->this == cb->edbuf->lines) {
		    error("End-of-file");
		}
		cm = &ed_commands['p' - 'a'];
		cb->first = cb->last = cb->this + 1;
	    } else {
		/* parse [range] */
		cb_range(cb);
		p = cb->cmd = skipst(cb->cmd);
		cm = (cmd *) NULL;

		/* parse [command] */
		if (*p == 'k') {
		    p++;
		} else {
		    while (isalpha(*p)) {
			p++;
		    }
		}
		if (p == cb->cmd) {
		    /* length == 0 */
		    switch (*p++) {
		    case '=':
			cm = &ed_commands[CM_ASSIGN];
			break;

		    case '#':
			cm = &ed_commands[CM_NUMBER];
			break;

		    case '<':
			cm = &ed_commands[CM_LSHIFT];
			break;

		    case '>':
			cm = &ed_commands[CM_RSHIFT];
			break;

		    case '\0':
		    case '|':
			cm = &ed_commands['p' - 'a'];
		    default:
			--p;
			break;
		    }
		} else if (p - cb->cmd == 1) {
		    /* length == 1 */
		    if (ed_commands[p[-1] - 'a'].chr == p[-1]) {
			cm = &ed_commands[p[-1] - 'a'];
		    } else if (p[-1] == 'I') {
			cm = &ed_commands[CM_INDENT];
		    }
		} else {
		    /* length > 1 */
		    cm = ed_commands;
		    for (;;) {
			if (cm->cmd &&
			  strncmp(cb->cmd, cm->cmd, p - cb->cmd) == 0) {
			    break;
			}
			if (++cm == &ed_commands[NR_CMD]) {
			    cm = (cmd *) NULL;
			    break;
			}
		    }
		}

		if (cm == (cmd *) NULL) {
		    error("No such command");
		}

		/* CM_EXCL */
		if ((cm->flags & CM_EXCL) && *p == '!') {
		    cb->flags |= CB_EXCL;
		    p++;
		}

		p = skipst(p);

		/* CM_BUFFER */
		if ((cm->flags & CM_BUFFER) && isalpha(*p)) {
		    cb->a_buffer = *p;
		    p = skipst(p + 1);
		}

		cb->cmd = p;

		/* CM_COUNT */
		if (cm->flags & CM_COUNT) {
		    cb_count(cb);
		}

		/* CM_ADDR */
		if (cm->flags & CM_ADDR) {
		    cb->a_addr = cb_address(cb, cb->this);
		    if (cb->a_addr < 0) {
			error("Command requires a trailing address");
		    }
		    cb->cmd = skipst(cb->cmd);
		}
	    }

	    /*
	     * check/adjust line range
	     */
	    ltype = cm->flags & CM_LNMASK;
	    if (ltype != CM_LN0) {
		if ((ltype == CM_LNDOT || ltype == CM_LNRNG) &&
		  cb->edbuf->lines == 0) {
		    error("No lines in buffer");
		}
		if (cb->first == 0) {
		    error("Nonzero address required on this command");
		}
	    }
	    switch (ltype) {
	    case CM_LNDOT:
	    case CM_LN0:
		if (cb->first < 0) {
		    cb->first = cb->this;
		}
		if (cb->last < 0) {
		    cb->last = cb->first;
		}
		break;

	    case CM_LNRNG:
		if (cb->first < 0) {
		    cb->first = 1;
		    cb->last = cb->edbuf->lines;
		} else if ( cb->last < 0) {
		    cb->last = cb->first;
		}
		break;
	    }
	    if (cb->first > cb->edbuf->lines || cb->last > cb->edbuf->lines ||
		cb->a_addr > cb->edbuf->lines) {
		error("Not that many lines in buffer");
	    }
	    if (cb->last >= 0 && cb->last < cb->first) {
		error("Inverted address range");
	    }

	    ret = (*cm->ftn)(cb);

	    p = skipst(cb->cmd);

	    if (ret == RET_FLAGS) {
		for (;;) {
		    switch (*p++) {
		    case '-':
			--cb->this;
			continue;

		    case '+':
			cb->this++;
			continue;

		    case 'p':
			/* ignore */
			continue;

		    case 'l':
			cb->flags |= CB_LIST;
			continue;

		    case '#':
			cb->flags |= CB_NUMBER;
			continue;
		    }
		    --p;
		    break;
		}

		if (cb->this <= 0) {
		    cb->this = 1;
		}
		if (cb->this > cb->edbuf->lines) {
		    cb->this = cb->edbuf->lines;
		}
		if (cb->this != 0 && !(cb->flags & CB_GLOBAL)) {
		    /* no autoprint in global */
		    cb->first = cb->last = cb->this;
		    cb_print(cb);
		}
		p = skipst(cb->cmd);
	    }

	    /* another command? */
	    if (*p == '|' && (cb->flags & CB_GLOBAL) && ret != RET_QUIT) {
		cb->cmd = p + 1;
		continue;
	    }
	    /* it has to be finished now */
	    if (*p != '\0') {
		error("Illegal characters after command");
	    }
	    if (ret == RET_QUIT) {
		return FALSE;
	    }
	}

	return TRUE;
    }
}
