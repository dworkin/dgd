/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2021 DGD Authors (see the commit log for details)
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

# define INCLUDE_CTYPE
# include "ed.h"
# include "edcmd.h"

/*
 * This file contains the command parsing functions.
 */

CmdBuf *ccb;		/* editor command buffer */

/*
 * create and initialize a command edit buffer
 */
CmdBuf::CmdBuf(char *tmpfile) :
    edbuf(tmpfile)
{
    flags = 0;
    cmd = (char *) NULL;
    reverse = false;
    ignorecase = false;
    edits = 0;
    cthis = 0;
    othis = 0;
    first = 0;
    last = 0;
    a_addr = 0;
    a_buffer = 0;
    lineno = 0;
    buffer = (char *) NULL;
    buflen = 0;
    glob_rx = (RxBuf *) NULL;
    glob_next = 0;
    glob_size = 0;
    stack = (char *) NULL;
    stackbot = (char *) NULL;
    ind = (int *) NULL;
    quote = 0;
    shift = 0;
    offset = 0;
    moffset = (Int *) NULL;
    memset(mark, '\0', sizeof(mark));
    buf = 0;
    memset(zbuf, '\0', sizeof(zbuf));
    memset(fname, '\0', sizeof(fname));
    undo = (Block) -1;				/* not 0! */
    uthis = 0;
    memset(umark, '\0', sizeof(umark));
    memset(search, '\0', sizeof(search));
    memset(replace, '\0', sizeof(replace));
}

/*
 * delete a command edit buffer
 */
CmdBuf::~CmdBuf()
{
}

/*
 * skip white space in a string. return a pointer to the first
 * character after the white space (could be '\0')
 */
const char *CmdBuf::skipst(const char *p)
{
    while (*p == ' ' || *p == HT) {
	p++;
    }
    return p;
}

/*
 * scan a pattern and copy it to a buffer.
 */
const char *CmdBuf::pattern(const char *pat, int delim, char *buffer)
{
    const char *p;
    unsigned int size;

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
	EDC->error("Regular expression too large");
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
 * compile a regular expression, up to a delimeter
 */
void CmdBuf::pattern(char delim)
{
    char buffer[STRINGSZ];

    cmd = pattern(cmd, delim, buffer);
    if (buffer[0] == '\0') {
	if (!regexp.valid) {
	    EDC->error("No previous regular expression");
	}
    } else {
	const char *err;

	err = regexp.comp(buffer);
	if (err != (char *) NULL) {
	    EDC->error(err);
	}
    }
}

/*
 * parse an address. First is the first line to search from if the
 * address is a search pattern.
 */
Int CmdBuf::address(Int first)
{
    Int l;
    const char *p;

    l = 0;

    switch (*(p = cmd)) {
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
	cmd = p;
	break;

    case '.':
	cmd++;
	/* fall through */
    case '+':
    case '-':
	l = cthis;
	break;

    case '$':
	l = edbuf.lines;
	cmd++;
	break;

    case '\'':
	if (islower(*++p)) {
	    l = mark[*p - 'a'];
	} else {
	    EDC->error("Marks are a-z");
	}
	if (l == 0) {
	    EDC->error("Undefined mark referenced");
	}
	cmd += 2;
	break;

    case '/':
	cmd++;
	pattern(*p);
	l = edbuf.lines;
	if (l == 0) {
	    return 1;	/* force out-of-range error */
	}
	if (first < l) {
	    l = dosearch(first + 1, l, FALSE);
	    if (l != 0) {
		break;
	    }
	}
	l = dosearch((Int) 1, first, FALSE);
	if (l == 0) {
	    EDC->error("Pattern not found");
	}
	break;

    case '?':
	cmd++;
	pattern(*p);
	if (first > 1) {
	    l = dosearch((Int) 1, first - 1, TRUE);
	}
	if (l == 0) {
	    l = edbuf.lines;
	    if (l == 0) {
		return 1;	/* force out-of-range error */
	    }
	    l = dosearch(first, l, TRUE);
	    if (l == 0) {
		EDC->error("Pattern not found");
	    }
	}
	break;

    default:
	return -1;
    }

    cmd = skipst(cmd);
    while (cmd[0] == '+' || cmd[0] == '-') {
	Int r;

	p = skipst(cmd + 1);
	if (!isdigit(*p)) {
	    r = 1;
	} else {
	    r = 0;
	    do {
		r *= 10;
		r += *p++ - '0';
	    } while (isdigit(*p));
	}
	if (cmd[0] == '+') {
	    l += r;
	} else {
	    l -= r;
	}
	cmd = skipst(p);
    }
    if (l < 0) {
	EDC->error("Negative address");
    }
    return l;
}

/*
 * parse line range from the command buffer. Valid range for an
 * address is 0-$
 */
void CmdBuf::range()
{
    first = -1;
    last = -1;
    cmd = skipst(cmd);
    if (cmd[0] == '\0') {
	return;
    }
    if (cmd[0] == '%') {
	first = 1;
	last = edbuf.lines;
	cmd = skipst(cmd + 1);
    } else {
	first = address(cthis);
	if (cmd[0] == ',' || cmd[0] == ';') {
	    if (first < 0) {
		first = cthis;
	    } else if (cmd[0] == ';') {
		cthis = first;
	    }
	    cmd = skipst(cmd + 1);
	    last = address(cthis);
	    if (last < 0) {
		last = cthis;
	    }
	}
    }
}

/*
 * set the line range according to the (optional) count in the
 * command buffer
 */
void CmdBuf::count()
{
    const char *p;

    p = cmd;
    if (isdigit(*p)) {
	Int count;

	count = 0;
	do {
	    count *= 10;
	    count += *p++ - '0';
	} while (isdigit(*p));
	cmd = skipst(p);

	if (first < 0) {
	    first = cthis;
	}
	last = first + count - 1;
	if (last < first || last > edbuf.lines) {
	    EDC->error("Not that many lines in buffer");
	}
    }
}

/*
 * error if currently executing a global command
 */
void CmdBuf::not_in_global()
{
    if (flags & CB_GLOBAL) {
	EDC->error("Command not allowed in global");
    }
}

/*
 * copy the present command buffer status in the undo buffer
 */
void CmdBuf::dodo(Int cthis)
{
    undo = edbuf.buffer;
    uthis = cthis;
    memcpy(umark, mark, sizeof(mark));
}

/*
 * undo the effects of a previous command by exchanging the
 * command buffer status with the undo buffer
 */
int CmdBuf::doundo()
{
    Block b;
    Int cthis, mark[26];

    not_in_global();
    if (undo == (Block) -1) {
	EDC->error("Nothing to undo");
    }

    b = undo;
    this->undo = edbuf.buffer;
    edbuf.lines = (b == (Block) 0) ? 0 : edbuf.size(b);
    edbuf.buffer = b;

    cthis = this->uthis;
    if (cthis == 0 && b != (Block) 0) {
	cthis = 1;
    }
    uthis = othis;
    this->cthis = othis = cthis;

    memcpy(mark, umark, sizeof(mark));
    memcpy(umark, this->mark, sizeof(mark));
    memcpy(this->mark, mark, sizeof(mark));

    edits++;
    return RET_FLAGS;
}

/*
 * put a block in the appropriate buffers
 */
void CmdBuf::dobuf(Block b)
{
    if (isupper(a_buffer)) {
	Block *zbuf;

	/*
	 * copy or append to named buffer
	 */
	zbuf = &this->zbuf[a_buffer - 'A'];
	if (*zbuf != (Block) 0) {
	    *zbuf = edbuf.cat(*zbuf, b);
	} else {
	    *zbuf = b;
	}
    } else if (islower(a_buffer)) {
	this->zbuf[a_buffer - 'a'] = b;
    }
    /*
     * always put it in the default yank buffer too
     */
    buf = b;
}


/*
 * Commands are allowed to affect the first or the last or all of the next lines
 * to be examined by a global command. The lines must stay in a contiguous
 * block.
 */

/*
 * add a block of lines to the edit buffer
 */
void CmdBuf::add(Int ln, Block b, Int size)
{
    Int *m;

    /* global checks */
    if (flags & CB_GLOBAL) {
	if (ln < glob_next) {
	    glob_next += size;
	} else if (ln < glob_next + glob_size - 1) {
	    EDC->error("Illegal add in global");
	}
    }

    edbuf.put(ln, b);

    /* adjust marks of lines after new block */
    for (m = mark; m < &mark[26]; m++) {
	if (*m > ln) {
	    *m += size;
	}
    }

    cthis = othis = ln + size;
}

/*
 * delete a block of lines from the edit buffer
 */
Block CmdBuf::dellines(Int first, Int last)
{
    Int size, *m;

    size = last - first + 1;

    /* global checks */
    if (flags & CB_GLOBAL) {
	if (last < glob_next) {
	    glob_next -= size;
	} else if (first <= glob_next) {
	    glob_size -= last - glob_next + 1;
	    glob_next = first;
	} else if (last >= glob_next + glob_size) {
	    glob_size = first - glob_next;
	} else {
	    EDC->error("Illegal delete in global");
	}
    }

    /* adjust & erase marks */
    for (m = mark; m < &mark[26]; m++) {
	if (*m >= first) {
	    if (*m > last) {
		*m -= size;
	    } else {
		*m = 0;
	    }
	}
    }

    othis = first;
    if (last == edbuf.lines) {
	othis--;
    }
    cthis = othis;

    return edbuf.del(first, last);
}

/*
 * replace a subrange of lines by a block
 */
void CmdBuf::change(Int first, Int last, Block b)
{
    Int offset, *m;

    offset = last - first + 1;
    if (b != (Block) 0) {
	offset -= edbuf.size(b);
    }

    /* global checks */
    if (flags & CB_GLOBAL) {
	if (last < glob_next) {
	    glob_next -= offset;
	} else if (first <= glob_next) {
	    glob_size -= last - glob_next + 1;
	    glob_next = last - offset + 1;
	} else if (last >= glob_next + glob_size) {
	    glob_size = first - glob_next;
	} else {
	    EDC->error("Illegal change in global");
	}
    }

    /* adjust marks. If the marks of the changed lines have to be erased,
       the calling routine must handle it. */
    for (m = mark; m < &mark[26]; m++) {
	if (*m > last) {
	    *m -= offset;
	}
    }

    othis = first;
    cthis = last - offset;
    if (cthis == 0 && last != edbuf.lines) {
	cthis = 1;
    }

    edbuf.change(first, last, b);
}


/*
 * start a block of lines
 */
void CmdBuf::startblock()
{
    edbuf.startblock();
}

/*
 * add a line to the current block of lines
 */
void CmdBuf::addblock(const char *text)
{
    edbuf.addblock(text);
}

/*
 * finish the current block
 */
void CmdBuf::endblock()
{
    edbuf.endblock();

    if (flags & CB_CHANGE) {
	if (first <= last) {
	    change(first, last, edbuf.flines);
	} else if (first == 0 && edbuf.lines != 0) {
	    cthis = othis = 1;
	} else {
	    cthis = othis = first;
	}
    } else {
	if (edbuf.flines != (Block) 0) {
	    add(first, edbuf.flines, edbuf.size(edbuf.flines));
	} else if (first == 0 && edbuf.lines != 0) {
	    cthis = othis = 1;
	} else {
	    cthis = othis = first;
	}
    }

    flags &= ~(CB_INSERT | CB_CHANGE);
    edits++;
}


/*
 * match a pattern in a global command
 */
void CmdBuf::globfind(const char *text)
{
    CmdBuf *cb;

    cb = ccb;
    cb->glob_next++;
    cb->glob_size--;
    if (cb->glob_rx->exec(text, 0, cb->ignorecase) != (int) cb->reverse) {
	throw "found";
    }
}

/*
 * do a global command
 */
int CmdBuf::global()
{
    const char *p;
    char buffer[STRINGSZ], delim;
    Block undo;
    Int uthis, umark[26];
    bool aborted;

    not_in_global();	/* no recursion please */

    /* get the regular expression */
    delim = cmd[0];
    if (delim != '\0' && !isalnum(delim)) {
	cmd = pattern(cmd + 1, delim, buffer);
    } else {
	buffer[0] = '\0';
    }
    if (buffer[0] == '\0') {
	EDC->error("Missing regular expression for global");
    }

    /* keep global undo status */
    undo = edbuf.buffer;
    uthis = first;
    memcpy(umark, mark, sizeof(mark));

    /*
     * A local error context is created, so the regular expression buffer
     * can be deallocated in case of an error.
     */
    glob_rx = new RxBuf();
    try {
	EDC->push();
	/* compile regexp */
	p = glob_rx->comp(buffer);
	if (p != (char *) NULL) {
	    EDC->error(p);
	}

	/* get the command to be done in global */
	p = skipst(cmd);
	cmd = p + strlen(p);
	if (*p == '\0') {
	    p = "p";	/* default: print lines */
	}
	flags |= CB_GLOBAL;
	reverse = (flags & CB_EXCL) != 0;
	ignorecase = IGNORECASE(vars);
	glob_next = first;
	glob_size = last - first + 1;

	do {
	    try {
		/* search */
		edbuf.range(glob_next, glob_next + glob_size - 1, globfind,
			    FALSE);
	    } catch (const char*) {
		/* found: do the commands */
		cthis = glob_next - 1;
		command(p);
	    }
	} while (glob_size > 0);

	/* pop error context */
	aborted = FALSE;
	EDC->pop();
    } catch (const char*) {
	aborted = TRUE;
    }
    /* come here if global is finished or in case of an error */

    /* clean up regular expression */
    delete glob_rx;

    /* set undo status */
    this->undo = undo;
    this->uthis = uthis;
    memcpy(this->umark, umark, sizeof(umark));

    /* no longer in global */
    flags &= ~CB_GLOBAL;

    if (aborted) {
	EDC->error((char *) NULL);
    }
    return 0;
}

/*
 * v == g!
 */
int CmdBuf::vglobal()
{
    flags |= CB_EXCL;
    return global();
}


struct Cmd {
    char flags;			/* type of command */
    char chr;			/* first char of command */
    const char *cmd;		/* full command string */
    int (*ftn)(CmdBuf*);	/* command function */
};

# define CM_LNMASK	0x03
# define CM_LNNONE	0x00	/* range checking in function */
# define CM_LN0		0x01	/* (.)0  */
# define CM_LNDOT	0x02	/* (.,.) */
# define CM_LNRNG	0x03	/* (1,$) */

# define CM_EXCL	0x04

# define CM_BUFFER	0x10	/* buffer argument */
# define CM_ADDR	0x20	/* address argument */
# define CM_COUNT	0x40	/* count argument */

static int cb_append(CmdBuf *cb)	{ return cb->append(); }
static int cb_assign(CmdBuf *cb)	{ return cb->assign(); }
static int cb_change(CmdBuf *cb)	{ return cb->change(); }
static int cb_delete(CmdBuf *cb)	{ return cb->del(); }
static int cb_edit(CmdBuf *cb)		{ return cb->edit(); }
static int cb_file(CmdBuf *cb)		{ return cb->file(); }
static int cb_global(CmdBuf *cb)	{ return cb->global(); }
static int cb_put(CmdBuf *cb)		{ return cb->put(); }
static int cb_insert(CmdBuf *cb)	{ return cb->insert(); }
static int cb_join(CmdBuf *cb)		{ return cb->join(); }
static int cb_mark(CmdBuf *cb)		{ return cb->domark(); }
static int cb_list(CmdBuf *cb)		{ return cb->list(); }
static int cb_move(CmdBuf *cb)		{ return cb->move(); }
static int cb_number(CmdBuf *cb)	{ return cb->number(); }
static int cb_wq(CmdBuf *cb)		{ return cb->wq(); }
static int cb_print(CmdBuf *cb)		{ return cb->print(); }
static int cb_quit(CmdBuf *cb)		{ return cb->quit(); }
static int cb_read(CmdBuf *cb)		{ return cb->read(); }
static int cb_subst(CmdBuf *cb)		{ return cb->subst(); }
static int cb_copy(CmdBuf *cb)		{ return cb->copy(); }
static int cb_undo(CmdBuf *cb)		{ return cb->doundo(); }
static int cb_vglobal(CmdBuf *cb)	{ return cb->vglobal(); }
static int cb_write(CmdBuf *cb)		{ return cb->write(); }
static int cb_xit(CmdBuf *cb)		{ return cb->xit(); }
static int cb_yank(CmdBuf *cb)		{ return cb->yank(); }
static int cb_page(CmdBuf *cb)		{ return cb->page(); }
static int cb_set(CmdBuf *cb)		{ return cb->set(); }
static int cb_lshift(CmdBuf *cb)	{ return cb->lshift(); }
static int cb_rshift(CmdBuf *cb)	{ return cb->rshift(); }
static int cb_indent(CmdBuf *cb)	{ return cb->indent(); }

static Cmd ed_commands[] = {
    { CM_LN0,				'a', "append",	cb_append },
# define CM_ASSIGN 1
    { CM_LNNONE,			'=', (char *) NULL,
							cb_assign },
    { CM_LNDOT | CM_COUNT,		'c', "change",	cb_change },
    { CM_LNDOT | CM_BUFFER | CM_COUNT,	'd', "delete",	cb_delete },
    { CM_LNNONE | CM_EXCL,		'e', "edit",	cb_edit },
    { CM_LNNONE,			'f', "file",	cb_file },
    { CM_LNRNG | CM_EXCL,		'g', "global",	cb_global },
    { CM_LN0 | CM_BUFFER,		 0,  "put",	cb_put },
    { CM_LN0,				'i', "insert",	cb_insert },
    { CM_LNNONE | CM_EXCL | CM_COUNT,	'j', "join",	cb_join },
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
 * Parse and execute an editor command. Return TRUE if this command
 * did not terminate the editor. Multiple commands may be
 * specified, separated by |
 */
bool CmdBuf::command(const char *command)
{
    cmd = command;
    ccb = this;

    for (;;) {
	if (flags & CB_INSERT) {
	    /* insert mode */
	    if (strlen(command) >= MAX_LINE_SIZE) {
		endblock();
		EDC->error("Line too long");
	    }
	    if (strcmp(command, ".") == 0) {
		/* finish block */
		endblock();
	    } else {
		/* add the "command" to the current block */
		addblock(command);
	    }
	} else {
	    Cmd *cm;
	    const char *p;
	    int ltype, ret;

	    flags &= ~(CB_EXCL | CB_NUMBER | CB_LIST);

	    a_addr = -1;
	    a_buffer = 0;

	    /*
	     * parse the command line: [range] [command] [arguments]
	     */

	    cmd = skipst(cmd);
	    if (cmd[0] == '\0') {
		/* no command: print next line */
		if (cthis == edbuf.lines) {
		    EDC->error("End-of-file");
		}
		cm = &ed_commands['p' - 'a'];
		first = last = cthis + 1;
	    } else {
		/* parse [range] */
		range();
		p = cmd = skipst(cmd);
		cm = (Cmd *) NULL;

		/* parse [command] */
		if (*p == 'k') {
		    p++;
		} else {
		    while (isalpha(*p)) {
			p++;
		    }
		}
		if (p == cmd) {
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
			/* fall through */
		    default:
			--p;
			break;
		    }
		} else if (p - cmd == 1) {
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
			  strncmp(cmd, cm->cmd, p - cmd) == 0) {
			    break;
			}
			if (++cm == &ed_commands[NR_CMD]) {
			    cm = (Cmd *) NULL;
			    break;
			}
		    }
		}

		if (cm == (Cmd *) NULL) {
		    EDC->error("No such command");
		}

		/* CM_EXCL */
		if ((cm->flags & CM_EXCL) && *p == '!') {
		    flags |= CB_EXCL;
		    p++;
		}

		p = skipst(p);

		/* CM_BUFFER */
		if ((cm->flags & CM_BUFFER) && isalpha(*p)) {
		    a_buffer = *p;
		    p = skipst(p + 1);
		}

		cmd = p;

		/* CM_COUNT */
		if (cm->flags & CM_COUNT) {
		    count();
		}

		/* CM_ADDR */
		if (cm->flags & CM_ADDR) {
		    a_addr = address(cthis);
		    if (a_addr < 0) {
			EDC->error("Command requires a trailing address");
		    }
		    cmd = skipst(cmd);
		}
	    }

	    /*
	     * check/adjust line range
	     */
	    ltype = cm->flags & CM_LNMASK;
	    if (ltype != CM_LN0) {
		if ((ltype == CM_LNDOT || ltype == CM_LNRNG) &&
		  edbuf.lines == 0) {
		    EDC->error("No lines in buffer");
		}
		if (first == 0) {
		    EDC->error("Nonzero address required on this command");
		}
	    }
	    switch (ltype) {
	    case CM_LNDOT:
	    case CM_LN0:
		if (first < 0) {
		    first = cthis;
		}
		if (last < 0) {
		    last = first;
		}
		break;

	    case CM_LNRNG:
		if (first < 0) {
		    first = 1;
		    last = edbuf.lines;
		} else if ( last < 0) {
		    last = first;
		}
		break;
	    }
	    if (first > edbuf.lines || last > edbuf.lines ||
		a_addr > edbuf.lines) {
		EDC->error("Not that many lines in buffer");
	    }
	    if (last >= 0 && last < first) {
		EDC->error("Inverted address range");
	    }

	    ret = (*cm->ftn)(this);

	    p = skipst(cmd);

	    if (ret == RET_FLAGS) {
		for (;;) {
		    switch (*p++) {
		    case '-':
			--cthis;
			continue;

		    case '+':
			cthis++;
			continue;

		    case 'p':
			/* ignore */
			continue;

		    case 'l':
			flags |= CB_LIST;
			continue;

		    case '#':
			flags |= CB_NUMBER;
			continue;
		    }
		    --p;
		    break;
		}

		if (cthis <= 0) {
		    cthis = 1;
		}
		if (cthis > edbuf.lines) {
		    cthis = edbuf.lines;
		}
		if (cthis != 0 && !(flags & CB_GLOBAL)) {
		    /* no autoprint in global */
		    first = last = cthis;
		    print();
		}
		p = skipst(cmd);
	    }

	    /* another command? */
	    if (*p == '|' && (flags & CB_GLOBAL) && ret != RET_QUIT) {
		cmd = p + 1;
		continue;
	    }
	    /* it has to be finished now */
	    if (*p != '\0') {
		EDC->error("Illegal characters after command");
	    }
	    if (ret == RET_QUIT) {
		return FALSE;
	    }
	}

	return TRUE;
    }
}
