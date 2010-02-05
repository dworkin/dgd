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

# define INCLUDE_CTYPE
# include "ed.h"
# include "edcmd.h"
# include "fileio.h"

/*
 * This file defines the command subroutines for edcmd.c
 */

extern char *skipst		P((char*));
extern char *pattern		P((char*, int, char*));
extern void  cb_count		P((cmdbuf*));
extern void  not_in_global	P((cmdbuf*));
extern void  cb_do		P((cmdbuf*, Int));
extern void  cb_buf		P((cmdbuf*, block));
extern void  add		P((cmdbuf*, Int, block, Int));
extern block delete		P((cmdbuf*, Int, Int));
extern void  change		P((cmdbuf*, Int, Int, block));
extern void  startblock		P((cmdbuf*));
extern void  addblock		P((cmdbuf*, char*));
extern void  endblock		P((cmdbuf*));


/*
 * NAME:	find()
 * DESCRIPTION:	scan a line for a pattern. If the pattern is found, longjump
 *		out.
 */
static void find(text)
char *text;
{
    if (rx_exec(ccb->regexp, text, 0, ccb->ignorecase) > 0) {
	longjmp(ccb->env, TRUE);
    }
    ccb->lineno++;
}

/*
 * NAME:	cmdbuf->search()
 * DESCRIPTION:	search a range of lines for the occurance of a pattern. When
 *		found, jump out immediately.
 */
Int cb_search(cb, first, last, reverse)
cmdbuf *cb;
Int first, last;
int reverse;
{
    if (setjmp(cb->env)) {
	/* found */
	return (reverse) ? last - cb->lineno : first + cb->lineno;
    }

    cb->lineno = 0;
    cb->ignorecase = IGNORECASE(cb->vars);
    eb_range(cb->edbuf, first, last, find, reverse);
    /* not found */
    return 0;
}


/*
 * NAME:	println()
 * DESCRIPTION:	output a line of text. The format is decided by flags.
 *		Non-ascii characters (eight bit set) have no special processing.
 */
static void println(text)
register char *text;
{
    char buffer[2 * MAX_LINE_SIZE + 14];	/* all ^x + number + list */
    register cmdbuf *cb;
    register char *p;

    cb = ccb;

    if (cb->flags & CB_NUMBER) {
	sprintf(buffer, "%6ld  ", (long) cb->lineno++);
	p = buffer + 8;
    } else {
	p = buffer;
    }

    while (*text != '\0') {
	if (UCHAR(*text) < ' ') {
	    /* control character */
	    if (*text == HT && !(cb->flags & CB_LIST)) {
		*p++ = HT;
	    } else {
		*p++ = '^'; *p++ = (*text & 0x1f) + '@';
	    }
	} else if (*text == 0x7f) {
	    /* DEL */
	    *p++ = '^'; *p++ = '?';
	} else {
	    /* normal character */
	    *p++ = *text;
	}
	text++;
    }
    if (cb->flags & CB_LIST) {
	*p++ = '$';
    }
    *p = '\0';
    output("%s\012", buffer);	/* LF */
}

/*
 * NAME:	cmdbuf->print()
 * DESCRIPTION:	print a range of lines, according to the format specified in
 *		the flags. Afterwards, the current line is set to the last line
 *		printed.
 */
int cb_print(cb)
register cmdbuf *cb;
{
    register char *p;

    /* handle flags right now */
    p = cb->cmd;
    for (;;) {
	switch (*p++) {
	case '-':
	case '+':
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
	cb->cmd = --p;
	break;
    }

    cb->lineno = cb->first;
    eb_range(cb->edbuf, cb->first, cb->last, println, FALSE);
    cb->this = cb->last;
    return 0;
}

/*
 * NAME:	cmdbuf->list()
 * DESCRIPTION:	output a range of lines in a hopefully unambiguous format
 */
int cb_list(cb)
cmdbuf *cb;
{
    cb->flags |= CB_LIST;
    return cb_print(cb);
}

/*
 * NAME:	cmdbuf->number()
 * DESCRIPTION:	output a range of lines preceded by line numbers
 */
int cb_number(cb)
cmdbuf *cb;
{
    cb->flags |= CB_NUMBER;
    return cb_print(cb);
}

/*
 * NAME:	cmdbuf->page()
 * DESCRIPTION:	show a page of lines
 */
int cb_page(cb)
register cmdbuf *cb;
{
    register Int offset, window;

    if (cb->edbuf->lines == 0) {
	error("No lines in buffer");
    }

    window = WINDOW(cb->vars);
    switch (*(cb->cmd)++) {
    default:	/* next line */
	cb->cmd--;
	cb->this++;
    case '+':	/* top */
	offset = 0;
	break;

    case '-':	/* bottom */
	offset = 1 - window;
	break;

    case '.':	/* middle */
	offset = 1 - (window + 1) / 2;
	break;
    }

    /* set first */
    if (cb->first < 0) {
	cb->first = cb->this;
    }
    cb->first += offset;
    if (cb->first <= 0) {
	cb->first = 1;
    } else if (cb->first > cb->edbuf->lines) {
	cb->first = cb->edbuf->lines;
    }

    /* set last */
    cb->last = cb->first + window - 1;
    if (cb->last < cb->first) {
	cb->last = cb->first;
    } else if (cb->last > cb->edbuf->lines) {
	cb->last = cb->edbuf->lines;
    }

    return cb_print(cb);
}

/*
 * NAME:	cmdbuf->assign()
 * DESCRIPTION:	show the specified line number
 */
int cb_assign(cb)
register cmdbuf *cb;
{
    output("%ld\012",
	   (long) (cb->first < 0) ? cb->edbuf->lines : cb->first);	/* LF */
    return 0;
}


/*
 * NAME:	cmdbuf->mark()
 * DESCRIPTION:	set a mark in the range [a-z] to line number
 */
int cb_mark(cb)
register cmdbuf *cb;
{
    if (!islower(cb->cmd[0])) {
	error("Mark must specify a letter");
    }
    cb->mark[*(cb->cmd)++ - 'a'] = cb->first;
    return 0;
}


/*
 * NAME:	cmdbuf->append()
 * DESCRIPTION:	append a block of lines, read from user, to edit buffer
 */
int cb_append(cb)
register cmdbuf *cb;
{
    not_in_global(cb);
    cb_do(cb, cb->first);

    startblock(cb);
    cb->flags |= CB_INSERT;
    return 0;
}

/*
 * NAME:	cmdbuf->insert()
 * DESCRIPTION:	insert a block of lines in the edit buffer
 */
int cb_insert(cb)
cmdbuf *cb;
{
    not_in_global(cb);
    if (cb->first > 0) {
	cb->first--;
    }
    return cb_append(cb);
}

/*
 * NAME:	cmdbuf->change()
 * DESCRIPTION:	change a subrange of lines in the edit buffer
 */
int cb_change(cb)
cmdbuf *cb;
{
    register Int *m;

    not_in_global(cb);
    cb_do(cb, cb->first);

    /* erase marks of changed lines */
    for (m = cb->mark; m < &cb->mark[26]; m++) {
	if (*m >= cb->first && *m <= cb->last) {
	    *m = 0;
	}
    }

    startblock(cb);
    cb->flags |= CB_INSERT | CB_CHANGE;
    return 0;
}


/*
 * NAME:	cmdbuf->delete()
 * DESCRIPTION:	delete a subrange of lines in the edit buffer
 */
int cb_delete(cb)
register cmdbuf *cb;
{
    cb_do(cb, cb->first);

    cb_buf(cb, delete(cb, cb->first, cb->last));

    cb->edit++;

    return RET_FLAGS;
}

/*
 * NAME:	cmdbuf->copy()
 * DESCRIPTION:	copy a subrange of lines in the edit buffer
 */
int cb_copy(cb)
register cmdbuf *cb;
{
    cb_do(cb, cb->a_addr);
    add(cb, cb->a_addr, eb_yank(cb->edbuf, cb->first, cb->last),
      cb->last - cb->first + 1);

    cb->edit++;

    return RET_FLAGS;
}

/*
 * NAME:	cmdbuf->move()
 * DESCRIPTION:	move a subrange of lines in the edit buffer
 */
int cb_move(cb)
register cmdbuf *cb;
{
    Int mark[26];
    register Int offset, *m1, *m2;

    if (cb->a_addr >= cb->first - 1 && cb->a_addr <= cb->last) {
	error("Move to moved line");
    }

    cb_do(cb, cb->first);
    memset(mark, '\0', sizeof(mark));
    if (cb->a_addr < cb->last) {
	offset = cb->a_addr + 1 - cb->first;
    } else {
	offset = cb->a_addr - cb->last;
	cb->a_addr -= cb->last - cb->first + 1;
    }
    /* make a copy of the marks of the lines to move */
    for (m1 = mark, m2 = cb->mark; m1 < &mark[26]; m1++, m2++) {
	if (*m2 >= cb->first && *m2 <= cb->last) {
	    *m1 = *m2;
	} else {
	    *m1 = 0;
	}
    }
    add(cb, cb->a_addr, delete(cb, cb->first, cb->last),
      cb->last - cb->first + 1);
    /* copy back adjusted marks of moved lines */
    for (m1 = mark, m2 = cb->mark; m1 < &mark[26]; m1++, m2++) {
	if (*m1 != 0) {
	    *m2 = *m1 + offset;
	}
    }

    cb->edit++;

    return RET_FLAGS;
}

/*
 * NAME:	cmdbuf->put()
 * DESCRIPTION:	put a block in the edit buffer
 */
int cb_put(cb)
register cmdbuf *cb;
{
    register block b;

    if (isalpha(cb->a_buffer)) {
	/* 'a' and 'A' both refer to buffer 'a' */
	b = cb->zbuf[tolower(cb->a_buffer) - 'a'];
    } else {
	b = cb->buf;
    }
    if (b == (block) 0) {
	error("Nothing in buffer");
    }

    cb_do(cb, cb->first);
    add(cb, cb->first, b, bk_size(cb->edbuf->lb, b));

    cb->edit++;

    return RET_FLAGS;
}

/*
 * NAME:	cmdbuf->yank()
 * DESCRIPTION:	yank a block of lines from the edit buffer
 */
int cb_yank(cb)
register cmdbuf *cb;
{
    cb_buf(cb, eb_yank(cb->edbuf, cb->first, cb->last));
    return 0;
}


/*
 * NAME:	shift()
 * DESCRIPTION:	shift a line left or right
 */
static void shift(text)
register char *text;
{
    register cmdbuf *cb;
    register int idx;

    cb = ccb;

    /* first determine the number of leading spaces */
    idx = 0;
    while (*text == ' ' || *text == HT) {
	if (*text++ == ' ') {
	    idx++;
	} else {
	    idx = (idx + 8) & ~7;
	}
    }

    if (*text == '\0') {
	/* don't shift lines with ws only */
	addblock(cb, text);
	cb->lineno++;
    } else {
	idx += cb->shift;
	if (idx < MAX_LINE_SIZE) {
	    char buffer[MAX_LINE_SIZE];
	    register char *p;

	    p = buffer;
	    /* fill with leading ws */
	    while (idx >= 8) {
		*p++ = HT;
		idx -= 8;
	    }
	    while (idx > 0) {
		*p++ = ' ';
		--idx;
	    }
	    if (p - buffer + strlen(text) < MAX_LINE_SIZE) {
		strcpy(p, text);
		addblock(cb, buffer);
		cb->lineno++;
		return;
	    }
	}

	/* Error: line too long. Finish block of lines already shifted. */
	cb->last = cb->lineno;
	endblock(cb);
	error("Result of shift would be too long");
    }
}

/*
 * NAME:	cmdbuf->shift()
 * DESCRIPTION:	shift a range of lines left or right
 */
static int cb_shift(cb)
register cmdbuf *cb;
{
    cb_do(cb, cb->first);
    startblock(cb);
    cb->lineno = cb->first - 1;
    cb->flags |= CB_CHANGE;
    eb_range(cb->edbuf, cb->first, cb->last, shift, FALSE);
    endblock(cb);

    return RET_FLAGS;
}

/*
 * NAME:	cmdbuf->lshift()
 * DESCRIPTION:	shift a range of lines to the left
 */
int cb_lshift(cb)
register cmdbuf *cb;
{
    cb->shift = -SHIFTWIDTH(cb->vars);
    return cb_shift(cb);
}

/*
 * NAME:	cmdbuf->rshift()
 * DESCRIPTION:	shift a range of lines to the right
 */
int cb_rshift(cb)
register cmdbuf *cb;
{
    cb->shift = SHIFTWIDTH(cb->vars);
    return cb_shift(cb);
}


# define STACKSZ	1024	/* size of indent stack */

/* token definitions in indent */
# define SEMICOLON	0
# define LBRACKET	1
# define RBRACKET	2
# define LOPERATOR	3
# define ROPERATOR	4
# define LHOOK		5
# define RHOOK		6
# define TOKEN		7
# define ELSE		8
# define IF		9
# define FOR		10	/* WHILE, RLIMIT */
# define DO		11
# define EOT		12

/*
 * NAME:	noshift()
 * DESCRIPTION:	add this line to the current block without shifting it
 */
static void noshift(cb, text)
cmdbuf *cb;
char *text;
{
    addblock(cb, text);
    cb->lineno++;
}

/*
 * NAME:	indent()
 * DESCRIPTION:	Parse and indent a line of text. This isn't perfect, as
 *		keywords could be defined as macros, comments are very hard to
 *		handle properly, (, [ and ({ will match any of ), ] and }),
 *		and last but not least everyone has his own taste of
 *		indentation.
 */
static void indent(text)
char *text;
{
    static char f[] = { 7, 1, 7, 1, 2, 1, 6, 4, 2, 6, 7, 2, 0, };
    static char g[] = { 2, 2, 1, 7, 1, 5, 1, 3, 6, 2, 2, 2, 0, };
    char ident[MAX_LINE_SIZE];
    char line[MAX_LINE_SIZE];
    register cmdbuf *cb;
    register char *p, *sp;
    register int *ip, idx;
    register int top, token;
    char *start;
    bool do_indent;

    cb = ccb;

    do_indent = FALSE;
    idx = 0;
    p = text = strcpy(line, text);

    /* process status vars */
    if (cb->quote != '\0') {
	cb->shift = 0;	/* in case a comment starts on this line */
	noshift(cb, p);
    } else if ((cb->flags & CB_PPCONTROL) || *p == '#') {
	noshift(cb, p);
	while (*p != '\0') {
	    if (*p == '\\' && *++p == '\0') {
		cb->flags |= CB_PPCONTROL;
		return;
	    }
	    p++;
	}
	cb->flags &= ~CB_PPCONTROL;
	return;
    } else {
	/* count leading ws */
	while (*p == ' ' || *p == HT) {
	    if (*p++ == ' ') {
		idx++;
	    } else {
		idx = (idx + 8) & ~7;
	    }
	}
	if (*p == '\0') {
	    noshift(cb, p);
	    return;
	} else if (cb->flags & CB_COMMENT) {
	    shift(text);	/* use previous shift */
	} else {
	    do_indent = TRUE;
	}
    }

    /* process this line */
    start = p;
    while (*p != '\0') {

	/* lexical scanning: find the next token */
	ident[0] = '\0';
	if (cb->flags & CB_COMMENT) {
	    /* comment */
	    while (*p != '*') {
		if (*p == '\0') {
		    return;
		}
		p++;
	    }
	    while (*p == '*') {
		p++;
	    }
	    if (*p == '/') {
		cb->flags &= ~CB_COMMENT;
		p++;
	    }
	    continue;

	} else if (cb->quote != '\0') {
	    /* string or character constant */
	    for (;;) {
		if (*p == cb->quote) {
		    cb->quote = '\0';
		    p++;
		    break;
		} else if (*p == '\0') {
		    cb->last = cb->lineno;
		    endblock(cb);
		    error("Unterminated string");
		} else if (*p == '\\' && *++p == '\0') {
		    break;
		}
		p++;
	    }
	    token = TOKEN;

	} else {
	    switch (*p++) {
	    case ' ':	/* white space */
	    case HT:
		continue;

	    case '\'':	/* start of string */
	    case '"':
		cb->quote = p[-1];
		continue;

	    case '/':
		if (*p == '*') {	/* start of comment */
		    cb->flags |= CB_COMMENT;
		    if (do_indent) {
			/* this line hasn't been indented yet */
			cb->shift = cb->ind[0] - idx;
			shift(text);
			do_indent = FALSE;
		    } else {
			register char *q;
			register int idx2;

			/*
			 * find how much the comment has shifted, so the same
			 * shift can be used if the comment continues on the
			 * next line
			 */
			idx2 = cb->ind[0];
			for (q = start; q < p - 1;) {
			    if (*q++ == HT) {
				idx = (idx + 8) & ~7;
				idx2 = (idx2 + 8) & ~7;
			    } else {
				idx++;
				idx2++;
			    }
			}
			cb->shift = idx2 - idx;
		    }
		    p++;
		    continue;
		}
		token = TOKEN;
		break;

	    case '{':
		token = LBRACKET;
		break;

	    case '(':
		if (cb->flags & CB_JSKEYWORD) {
		    /*
		     * LOPERATOR & ROPERATOR are a kludge. The operator
		     * precedence parser that is used could not work if
		     * parenthesis after keywords was not treated specially.
		     */
		    token = LOPERATOR;
		    break;
		}
		if (*p == '{') {
		    p++;	/* ({ is one token */
		}
	    case '[':
		token = LHOOK;
		break;

	    case '}':
		if (*p != ')') {
		    token = RBRACKET;
		    break;
		}
		p++;
		/* }) is one token; fall through */
	    case ')':
	    case ']':
		token = RHOOK;
		break;

	    case ';':
		token = SEMICOLON;
		break;

	    default:
		if (isalpha(*--p) || *p == '_') {
		    register char *q;

		    /* Identifier. See if it's a keyword. */
		    q = ident;
		    do {
			*q++ = *p++;
		    } while (isalnum(*p) || *p == '_');
		    *q = '\0';

		    if      (strcmp(ident, "if") == 0)		token = IF;
		    else if (strcmp(ident, "else") == 0)	token = ELSE;
		    else if (strcmp(ident, "for") == 0 ||
			     strcmp(ident, "while") == 0 ||
			     strcmp(ident, "rlimits") == 0)	token = FOR;
		    else if (strcmp(ident, "do") == 0)		token = DO;
		    else    /* not a keyword */			token = TOKEN;
		} else {
		    /* anything else is a "token" */
		    p++;
		    token = TOKEN;
		}
		break;
	    }
	}

	/* parse */
	sp = cb->stack;
	ip = cb->ind;
	for (;;) {
	    top = *sp;
	    if (top == LOPERATOR && token == RHOOK) {
		/* ) after LOPERATOR is ROPERATOR */
		token = ROPERATOR;
	    }

	    if (f[top] <= g[token]) {	/* shift the token on the stack */
		register int i;

		if (sp == cb->stackbot) {
		    /* out of stack. Finish already indented block. */
		    cb->last = cb->lineno;
		    endblock(cb);
		    error("Nesting too deep");
		}

		/* handle indentation */
		i = *ip;
		/* if needed, reduce indentation prior to shift */
		if ((token == LBRACKET && 
		  (*sp == ROPERATOR || *sp == ELSE || *sp == DO)) ||
		  token == RBRACKET ||
		  (token == IF && *sp == ELSE)) {
		    /* back up */
		    i -= SHIFTWIDTH(cb->vars);
		}
		/* shift the current line, if appropriate */
		if (do_indent) {
		    cb->shift = i - idx;
		    if (i > 0 && token != RHOOK &&
		      (*sp == LOPERATOR || *sp == LHOOK)) {
			/* half indent after ( [ ({ (HACK!) */
			cb->shift += SHIFTWIDTH(cb->vars) / 2;
		    } else if (token == TOKEN && *sp == LBRACKET &&
		      (strcmp(ident, "case") == 0 ||
		      strcmp(ident, "default") == 0)) {
			/* back up if this is a switch label */
			cb->shift -= SHIFTWIDTH(cb->vars);
		    }
		    shift(text);
		    do_indent = FALSE;
		}
		/* change indentation after current token */
		if (token == LBRACKET || token == ROPERATOR || token == ELSE ||
		  token == DO) {
		    /* add indentation */
		    i += SHIFTWIDTH(cb->vars);
		} else if (token == SEMICOLON &&
		  (*sp == ROPERATOR || *sp == ELSE)) {
		    /* in case it is followed by a comment */
		    i -= SHIFTWIDTH(cb->vars);
		}

		*--sp = token;
		*--ip = i;
		break;
	    }

	    /* reduce handle */
	    do {
		top = *sp++;
		ip++;
	    } while (f[*sp] >= g[top]);
	}
	cb->stack = sp;
	cb->ind = ip;
	if (token >= IF) {	/* but not ELSE */
	    cb->flags |= CB_JSKEYWORD;
	} else {
	    cb->flags &= ~CB_JSKEYWORD;
	}
    }
}

/*
 * NAME:	cmdbuf->indent()
 * DESCRIPTION:	indent a range of lines
 */
int cb_indent(cb)
register cmdbuf *cb;
{
    char s[STACKSZ];
    int i[STACKSZ];

    /* setup stacks */
    cb->stackbot = s;
    cb->stack = s + STACKSZ - 1;
    cb->stack[0] = EOT;
    cb->ind = i + STACKSZ - 1;
    cb->ind[0] = 0;
    cb->quote = '\0';

    cb_do(cb, cb->first);
    startblock(cb);
    cb->lineno = cb->first - 1;
    cb->flags |= CB_CHANGE;
    cb->flags &= ~(CB_PPCONTROL | CB_COMMENT | CB_JSKEYWORD);
    eb_range(cb->edbuf, cb->first, cb->last, indent, FALSE);
    endblock(cb);

    return 0;
}


/*
 * NAME:	join()
 * DESCRIPTION:	join a string to the one already in the join buffer
 */
static void join(text)
register char *text;
{
    register cmdbuf *cb;
    register char *p;

    cb = ccb;

    p = cb->buffer + cb->buflen;
    if (cb->buflen != 0 && !(cb->flags & CB_EXCL)) {
	/* do special processing */
	text = skipst(text);
	if (*text != '\0' && *text != ')' && p[-1] != ' ' && p[-1] != HT) {
	    if (p[-1] == '.') {
		*p++ = ' ';
	    }
	    *p++ = ' ';
	}
	cb->buflen = p - cb->buffer;
    }
    cb->buflen += strlen(text);
    if (cb->buflen >= MAX_LINE_SIZE) {
	error("Result of join would be too long");
    }
    strcpy(p, text);
}

/*
 * NAME:	cmdbuf->join()
 * DESCRIPTION:	join a range of lines in the edit buffer
 */
int cb_join(cb)
register cmdbuf *cb;
{
    char buf[MAX_LINE_SIZE + 1];
    register Int *m;

    if (cb->edbuf->lines == 0) {
	error("No lines in buffer");
    }
    if (cb->first < 0) {
	cb->first = cb->this;
    }
    if (cb->last < 0) {
	cb->last = (cb->first == cb->edbuf->lines) ? cb->first : cb->first + 1;
    }

    cb_do(cb, cb->first);

    cb->this = cb->othis = cb->first;
    buf[0] = '\0';
    cb->buffer = buf;
    cb->buflen = 0;
    eb_range(cb->edbuf, cb->first, cb->last, join, FALSE);

    /* erase marks for joined lines */
    for (m = cb->mark; m < &cb->mark[26]; m++) {
	if (*m > cb->first && *m <= cb->last) {
	    *m = 0;
	}
    }

    cb->flags |= CB_CHANGE;
    startblock(cb);
    addblock(cb, buf);
    endblock(cb);

    return RET_FLAGS;
}


/*
 * NAME:	sub()
 * DESCRIPTION:	add a string to the current substitute buffer
 */
static void sub(cb, text, size)
register cmdbuf *cb;
char *text;
unsigned int size;
{
    register char *p, *q;
    register unsigned int i;

    i = size;
    if (cb->buflen + i >= MAX_LINE_SIZE) {
	if (cb->flags & CB_CURRENTBLK) {
	    /* finish already processed block */
	    endblock(cb);
	}
	cb->this = cb->othis = cb->lineno;
	error("Line overflow in substitute");
    }

    p = cb->buffer + cb->buflen;
    q = text;
    if (cb->flags & CB_TLOWER) {	/* lowercase one letter */
	*p++ = tolower(*q);
	q++;
	cb->flags &= ~CB_TLOWER;
	--i;
    } else if (cb->flags & CB_TUPPER) {	/* uppercase one letter */
	*p++ = toupper(*q);
	q++;
	cb->flags &= ~CB_TUPPER;
	--i;
    }

    if (cb->flags & CB_LOWER) {		/* lowercase string */
	while (i > 0) {
	    *p++ = tolower(*q);
	    q++;
	    --i;
	}
    } else if (cb->flags & CB_UPPER) {		/* uppercase string */
	while (i > 0) {
	    *p++ = toupper(*q);
	    q++;
	    --i;
	}
    } else if (i > 0) {		/* don't change case */
	memcpy(p, q, i);
    }
    cb->buflen += size;
}

/*
 * NAME:	subst()
 * DESCRIPTION:	do substitutions in a line. If something is substituted on line
 *		N, and the next substitution happens on line N + 2, line N + 1
 *		is joined in the new block also.
 */
static void subst(text)
register char *text;
{
    char line[MAX_LINE_SIZE];
    register cmdbuf *cb;
    register int idx, size;
    register char *p;
    register Int *k, *l;
    Int newlines;
    bool found;

    cb = ccb;

    found = FALSE;
    newlines = 0;
    idx = 0;

    /*
     * Because the write buffer might be flushed, and the text would
     * not remain in memory, use a local copy.
     */
    text = strcpy(line, text);
    while (rx_exec(cb->regexp, text, idx, IGNORECASE(cb->vars)) > 0) {
	if (cb->flags & CB_SKIPPED) {
	    /*
	     * add the previous line, in which nothing was substituted, to
	     * the block. Has to be done here, before the contents of the buffer
	     * are changed.
	     */
	    addblock(cb, cb->buffer);
	    cb->flags &= ~CB_SKIPPED;
	    /*
	     * check if there were newlines in the last substitution. If there
	     * are, marks on the previous line (without substitutions) will
	     * also have to be changed.
	     */
	    if (cb->offset > 0) {
		for (k = cb->mark, l = cb->moffset; k < &cb->mark[26]; k++, l++)
		{
		    if (*k == cb->lineno - 1 && *l == 0) {
			*l = *k + cb->offset;
		    }
		}
	    }
	}
	found = TRUE;
	cb->flags &= ~(CB_UPPER | CB_LOWER | CB_TUPPER | CB_TLOWER);
	size = cb->regexp->start - text - idx;
	if (size > 0) {
	    /* copy first unchanged part of line to buffer */
	    sub(cb, text + idx, size);
	}
	p = cb->replace;
	while (*p != '\0') {
	    switch (*p) {
	    case '&':
		/* insert matching string */
		sub(cb, cb->regexp->start, cb->regexp->size);
		break;

	    case '\\':		/* special substitute characters */
		switch (*++p) {
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		    /* insert subexpression between \( \) */
		    if (cb->regexp->se[*p - '1'].start != (char*) NULL) {
			sub(cb, cb->regexp->se[*p - '1'].start,
			    cb->regexp->se[*p - '1'].size);
			break;
		    }
		    /* if no subexpression, fall though */
		default:
		    sub(cb, p, 1);	/* ignore preceding backslash */
		    break;

		case 'n':
		    cb->buffer[cb->buflen++] = '\0';
		    newlines++;		/* insert newline */
		    break;

		case 'U':
		    /* convert string to uppercase */
		    cb->flags |= CB_UPPER;
		    cb->flags &= ~(CB_LOWER | CB_TUPPER | CB_TLOWER);
		    break;

		case 'L':
		    /* convert string to lowercase */
		    cb->flags |= CB_LOWER;
		    cb->flags &= ~(CB_UPPER | CB_TUPPER | CB_TLOWER);
		    break;

		case 'e':
		case 'E':
		    /* end case conversion */
		    cb->flags &= ~(CB_UPPER | CB_LOWER | CB_TUPPER | CB_TLOWER);
		    break;

		case 'u':
		    /* convert char to uppercase */
		    cb->flags |= CB_TUPPER;
		    cb->flags &= ~CB_TLOWER;
		    break;

		case 'l':
		    /* convert char to lowercase */
		    cb->flags &= ~CB_TUPPER;
		    cb->flags |= CB_TLOWER;
		    break;

		case '\0':	/* sigh */
		    continue;
		}
		break;

	    default:		/* normal char */
		sub(cb, p, 1);
		break;
	    }
	    p++;
	}

	idx = cb->regexp->start + cb->regexp->size - text;
	if (!(cb->flags & CB_GLOBSUBST) || text[idx] == '\0' ||
	    (cb->regexp->size == 0 && text[++idx] == '\0')) {
	    break;
	}
    }

    if (found) {
	if (text[idx] != '\0') {
	    /* concatenate unchanged part of line after found pattern */
	    cb->flags &= ~(CB_UPPER | CB_LOWER | CB_TUPPER | CB_TLOWER);
	    sub(cb, text + idx, strlen(text + idx));
	}
	if (!(cb->flags & CB_CURRENTBLK)) {
	    /* start a new block of lines with substitutions in them */
	    cb->flags |= CB_CHANGE;
	    cb->first = cb->lineno;
	    startblock(cb);
	    cb->flags |= CB_CURRENTBLK;
	}
	/* add this changed line to block */
	cb->buffer[cb->buflen] = '\0';
	if (newlines == 0) {
	    addblock(cb, cb->buffer);
	} else {
	    /*
	     * There were newlines in the substituted string. Add all
	     * lines to the current block, and save the marks in range.
	     */
	    p = cb->buffer;
	    do {
		addblock(cb, p);
		p += strlen(p) + 1;
	    } while (p <= cb->buffer + cb->buflen);

	    for (k = cb->mark, l = cb->moffset; k < &cb->mark[26]; k++, l++) {
		if (*k == cb->lineno && *l == 0) {
		    *l = *k + cb->offset;
		}
	    }
	    cb->offset += newlines;
	}
	cb->buflen = 0;
	cb->last = cb->lineno;
    } else {
	if (cb->flags & CB_SKIPPED) {
	    /* two lines without substitutions now. Finish previous block. */
	    endblock(cb);
	    cb->lineno += cb->offset;
	    cb->offset = 0;
	    cb->flags &= ~(CB_CURRENTBLK | CB_SKIPPED);
	} else if (cb->flags & CB_CURRENTBLK) {
	    /*
	     * no substitution on this line, but there was one on the previous
	     * line. mark this line as skipped, so it can still be added to
	     * the block of changed lines if the next line has substitutions.
	     */
	    strcpy(cb->buffer, text);
	    cb->flags |= CB_SKIPPED;
	}
    }
    cb->lineno++;
}

/*
 * NAME:	cmdbuf->substitute()
 * DESCRIPTION:	do substitutions on a range of lines
 */
int cb_subst(cb)
register cmdbuf *cb;
{
    char buf[MAX_LINE_SIZE], delim;
    Int m[26];
    Int edit;
    register char *p;
    register Int *k, *l;

    delim = cb->cmd[0];
    if (delim == '\0' || strchr("0123456789gpl#-+", delim) != (char*) NULL) {
	/* no search pattern & replace string specified */
	if (cb->search[0] == '\0') {
	    error("No previous substitute to repeat");
	}
    } else if (!isalpha(delim)) {
	register char *q;

	/* get search pattern */
	p = pattern(cb->cmd + 1, delim, cb->search);
	/* get replace string */
	q = cb->replace;
	while (*p != '\0') {
	    if (*p == delim) {
		p++;
		break;
	    }
	    if (q == cb->replace + STRINGSZ - 1) {
		cb->search[0] = '\0';
		error("Replace string too large");
	    }
	    if ((*q++ = *p++) == '\\' && *p != '\0') {
		*q++ = *p++;
	    }
	}
	*q = '\0';
	cb->cmd = p;
    } else {
	/* cause error */
	cb->search[0] = '\0';
    }

    if (cb->search[0] == '\0') {
	error("Missing regular expression for substitute");
    }

    /* compile regexp */
    p = rx_comp(cb->regexp, cb->search);
    if (p != (char *) NULL) {
	error(p);
    }

    cb_count(cb);	/* get count */
    /* handle global flag */
    if (cb->cmd[0] == 'g') {
	cb->flags |= CB_GLOBSUBST;
	cb->cmd++;
    } else {
	cb->flags &= ~CB_GLOBSUBST;
    }

    /* make a blank mark table */
    cb->moffset = m;
    for (l = m; l < &m[26]; ) {
	*l++ = 0;
    }
    cb->offset = 0;

    /* do substitutions */
    cb_do(cb, cb->first);
    cb->lineno = cb->first;
    edit = cb->edit;
    cb->buffer = buf;
    cb->buflen = 0;
    cb->flags &= ~(CB_CURRENTBLK | CB_SKIPPED);
    eb_range(cb->edbuf, cb->first, cb->last, subst, FALSE);
    if (cb->flags & CB_CURRENTBLK) {
	/* finish current block, if needed */
	endblock(cb);
    }

    cb->othis = cb->uthis;
    if (edit == cb->edit) {
	error("Substitute pattern match failed");
    }

    /* some marks may have been messed up. fix them */
    for (l = m, k = cb->mark; l < &m[26]; l++, k++) {
	if (*l != 0) {
	    *k = *l;
	}
    }

    return RET_FLAGS;
}


/*
 * NAME:	getfname()
 * DESCRIPTION:	copy a string to another buffer, unless it has length 0 or
 *		is too long
 */
static bool getfname(cb, buffer)
register cmdbuf *cb;
char *buffer;
{
    register char *p, *q;

    /* find the end of the filename */
    p = strchr(cb->cmd, ' ');
    q = strchr(cb->cmd, HT);
    if (q != (char *) NULL && (p == (char *) NULL || p > q)) {
	p = q;
    }
    q = strchr(cb->cmd, '|');
    if (q != (char *) NULL && (p == (char *) NULL || p > q)) {
	p = q;
    }
    if (p == (char *) NULL) {
	p = strchr(cb->cmd, '\0');
    }

    /* checks */
    if (p == cb->cmd) {
	return FALSE;
    }
    if (p - cb->cmd >= STRINGSZ) {
	error("Filename too long");
    }

    /* copy */
    memcpy(buffer, cb->cmd, p - cb->cmd);
    buffer[p - cb->cmd] = '\0';
    cb->cmd = p;
    return TRUE;
}

/*
 * NAME:	cmdbuf->file()
 * DESCRIPTION:	get/set the file name & current line, etc.
 */
int cb_file(cb)
register cmdbuf *cb;
{
    not_in_global(cb);

    if (getfname(cb, cb->fname)) {
	/* file name is changed: mark the file as "not edited" */
	cb->flags |= CB_NOIMAGE;
    }

    /* give statistics */
    if (cb->fname[0] == '\0') {
	output("No file");
    } else {
	output("\"%s\"", cb->fname);
    }
    if (cb->flags & CB_NOIMAGE) {
	output(" [Not edited]");
    }
    if (cb->edit > 0) {
	output(" [Modified]");
    }
    output(" line %ld of %ld --%d%%--\012", /* LF */
	   (long) cb->this, (long) cb->edbuf->lines,
	   (cb->edbuf->lines == 0) ? 0 :
				(int) ((100 * cb->this) / cb->edbuf->lines));

    return 0;
}

/*
 * NAME:	io->show()
 * DESCRIPTION:	show statistics on the file just read/written
 */
static void io_show(iob)
register io *iob;
{
    output("%ld lines, %ld characters", (long) iob->lines,
	   (long) (iob->chars + iob->zero - iob->split - iob->ill));
    if (iob->zero > 0) {
	output(" [%ld zero]", (long) iob->zero);
    }
    if (iob->split > 0) {
	output(" [%ld split]", (long) iob->split);
    }
    if (iob->ill) {
	output(" [incomplete last line]");
    }
    output("\012");	/* LF */
}

/*
 * NAME:	cmdbuf->read()
 * DESCRIPTION:	insert a file in the current edit buffer
 */
int cb_read(cb)
register cmdbuf *cb;
{
    char buffer[STRINGSZ];
    io iob;

    not_in_global(cb);

    if (!getfname(cb, buffer)) {
	if (cb->fname[0] == '\0') {
	    error("No current filename");
	}
	/* read current file, by default. I don't know why, but ex has it
	   that way. */
	strcpy(buffer, cb->fname);
    }

    cb_do(cb, cb->first);
    output("\"%s\" ", buffer);
    if (!io_load(cb->edbuf, buffer, cb->first, &iob)) {
	error("is unreadable");
    }
    io_show(&iob);

    cb->edit++;
    cb->this = cb->first + iob.lines;

    return 0;
}

/*
 * NAME:	cmdbuf->edit()
 * DESCRIPTION:	edit a new file
 */
int cb_edit(cb)
register cmdbuf *cb;
{
    io iob;

    not_in_global(cb);

    if (cb->edit > 0 && !(cb->flags & CB_EXCL)) {
	error("No write since last change (edit! overrides)");
    }

    getfname(cb, cb->fname);
    if (cb->fname[0] == '\0') {
	error("No current filename");
    }

    m_static();
    eb_clear(cb->edbuf);
    m_dynamic();
    cb->flags &= ~CB_NOIMAGE;
    cb->edit = 0;
    cb->first = cb->this = 0;
    memset(cb->mark, '\0', sizeof(cb->mark));
    cb->buf = 0;
    memset(cb->zbuf, '\0', sizeof(cb->zbuf));
    cb->undo = (block) -1;	/* not 0! */

    output("\"%s\" ", cb->fname);
    if (!io_load(cb->edbuf, cb->fname, cb->first, &iob)) {
	error("is unreadable");
    }
    io_show(&iob);
    if (iob.zero > 0 || iob.split > 0 || iob.ill) {
	/* the editbuffer in memory is not a perfect image of the file read */
	cb->flags |= CB_NOIMAGE;
    }

    cb->this = iob.lines;

    return 0;
}

/*
 * NAME:	cmdbuf->quit()
 * DESCRIPTION:	quit editing
 */
int cb_quit(cb)
cmdbuf *cb;
{
    not_in_global(cb);

    if (cb->edit > 0 && !(cb->flags & CB_EXCL)) {
	error("No write since last change (quit! overrides)");
    }

    return RET_QUIT;
}

/*
 * NAME:	cmdbuf->write()
 * DESCRIPTION:	write a range of lines to a file
 */
int cb_write(cb)
register cmdbuf *cb;
{
    char buffer[STRINGSZ];
    bool append;
    io iob;

    not_in_global(cb);

    if (strncmp(cb->cmd, ">>", 2) == 0) {
	append = TRUE;
	cb->cmd = skipst(cb->cmd + 2);
    } else {
	append = FALSE;
    }

    /* check if write can be done */
    if (!getfname(cb, buffer)) {
	if (cb->fname[0] == '\0') {
	    error("No current filename");
	}
	strcpy(buffer, cb->fname);
    }
    if (strcmp(buffer, cb->fname) == 0) {
	if (cb->first == 1 && cb->last == cb->edbuf->lines) {
	    if ((cb->flags & (CB_NOIMAGE|CB_EXCL)) == CB_NOIMAGE) {
		error("File is changed (use w! to override)");
	    }
	} else if (!(cb->flags & CB_EXCL)) {
	    error("Use w! to write partial buffer");
	}
    }

    output("\"%s\" ", buffer);
    if (!io_save(cb->edbuf, buffer, cb->first, cb->last, append, &iob)) {
	error("write failed");
    }
    io_show(&iob);

    if (cb->first == 1 && cb->last == cb->edbuf->lines) {
	/* file is now perfect image of editbuffer in memory */
	cb->flags &= ~CB_NOIMAGE;
	cb->edit = 0;
    }

    return 0;
}

/*
 * NAME:	cmdbuf->wq()
 * DESCRIPTION:	write a range of lines to a file and quit
 */
int cb_wq(cb)
cmdbuf *cb;
{
    cb->first = 1;
    cb->last = cb->edbuf->lines;
    cb_write(cb);
    return cb_quit(cb);
}

/*
 * NAME:	cmdbuf->xit()
 * DESCRIPTION:	write to the current file if modified, and quit
 */
int cb_xit(cb)
register cmdbuf *cb;
{
    if (cb->edit > 0) {
	cb->flags |= CB_EXCL;
	return cb_wq(cb);
    } else {
	not_in_global(cb);

	return RET_QUIT;
    }
}


/*
 * NAME:	cmdbuf->set()
 * DESCRIPTION:	get/set variable(s)
 */
int cb_set(cb)
register cmdbuf *cb;
{
    char buffer[STRINGSZ];
    register char *p, *q;

    not_in_global(cb);

    p = cb->cmd;
    if (strlen(p) >= STRINGSZ) {
	p[STRINGSZ - 1] = '\0';	/* must fit in the buffer */
    }
    if (*p == '\0') {
	/* no arguments */
	va_show(cb->vars);
    } else {
	do {
	    /* copy argument */
	    q = buffer;
	    while (*p != '\0' && *p != ' ' && *p != HT) {
		*q++ = *p++;
	    }
	    *q = '\0';
	    /* let va_set() process it */
	    va_set(cb->vars, buffer);
	    p = skipst(p);
	} while (*p != '\0');
	cb->cmd = p;
    }
    return 0;
}
