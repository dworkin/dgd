/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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


/*
 * scan a line for a pattern. If the pattern is found, longjump out.
 */
void CmdBuf::find(const char *text)
{
    if (ccb->regexp.exec(text, 0, ccb->ignorecase) > 0) {
	throw "found";
    }
    ccb->lineno++;
}

/*
 * search a range of lines for the occurance of a pattern. When
 * found, jump out immediately.
 */
Int CmdBuf::dosearch(Int first, Int last, bool reverse)
{
    try {
	lineno = 0;
	ignorecase = IGNORECASE(vars);
	edbuf.range(first, last, find, reverse);
	/* not found */
	return 0;
    } catch (const char*) {
	/* found */
	return (reverse) ? last - lineno : first + lineno;
    }
}


/*
 * output a line of text. The format is decided by flags.
 * Non-ascii characters (eight bit set) have no special processing.
 */
void CmdBuf::println(const char *text)
{
    char buffer[2 * MAX_LINE_SIZE + 14];	/* all ^x + number + list */
    CmdBuf *cb;
    char *p;

    cb = ccb;

    if (cb->flags & CB_NUMBER) {
	snprintf(buffer, sizeof(buffer), "%6ld  ", (long) cb->lineno++);
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
    EDC->message("%s\012", buffer);	/* LF */
}

/*
 * print a range of lines, according to the format specified in
 * the flags. Afterwards, the current line is set to the last line
 * printed.
 */
int CmdBuf::print()
{
    const char *p;

    /* handle flags right now */
    p = cmd;
    for (;;) {
	switch (*p++) {
	case '-':
	case '+':
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
	cmd = --p;
	break;
    }

    lineno = first;
    edbuf.range(first, last, println, FALSE);
    cthis = last;
    return 0;
}

/*
 * output a range of lines in a hopefully unambiguous format
 */
int CmdBuf::list()
{
    flags |= CB_LIST;
    return print();
}

/*
 * output a range of lines preceded by line numbers
 */
int CmdBuf::number()
{
    flags |= CB_NUMBER;
    return print();
}

/*
 * show a page of lines
 */
int CmdBuf::page()
{
    Int offset, window;

    if (edbuf.lines == 0) {
	EDC->error("No lines in buffer");
    }

    window = WINDOW(vars);
    switch (*cmd++) {
    default:	/* next line */
	cmd--;
	cthis++;
	/* fall through */
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
    if (first < 0) {
	first = cthis;
    }
    first += offset;
    if (first <= 0) {
	first = 1;
    } else if (first > edbuf.lines) {
	first = edbuf.lines;
    }

    /* set last */
    last = first + window - 1;
    if (last < first) {
	last = first;
    } else if (last > edbuf.lines) {
	last = edbuf.lines;
    }

    return print();
}

/*
 * show the specified line number
 */
int CmdBuf::assign()
{
    EDC->message("%ld\012", (long) (first < 0) ? edbuf.lines : first);	/* LF */
    return 0;
}


/*
 * set a mark in the range [a-z] to line number
 */
int CmdBuf::domark()
{
    if (!islower(cmd[0])) {
	EDC->error("Mark must specify a letter");
    }
    mark[*cmd++ - 'a'] = first;
    return 0;
}


/*
 * append a block of lines, read from user, to edit buffer
 */
int CmdBuf::append()
{
    not_in_global();
    dodo(first);

    startblock();
    flags |= CB_INSERT;
    return 0;
}

/*
 * insert a block of lines in the edit buffer
 */
int CmdBuf::insert()
{
    not_in_global();
    if (first > 0) {
	first--;
    }
    return append();
}

/*
 * change a subrange of lines in the edit buffer
 */
int CmdBuf::change()
{
    Int *m;

    not_in_global();
    dodo(first);

    /* erase marks of changed lines */
    for (m = mark; m < &mark[26]; m++) {
	if (*m >= first && *m <= last) {
	    *m = 0;
	}
    }

    startblock();
    flags |= CB_INSERT | CB_CHANGE;
    return 0;
}


/*
 * delete a subrange of lines in the edit buffer
 */
int CmdBuf::del()
{
    dodo(first);

    dobuf(dellines(first, last));

    edits++;

    return RET_FLAGS;
}

/*
 * copy a subrange of lines in the edit buffer
 */
int CmdBuf::copy()
{
    dodo(a_addr);
    add(a_addr, edbuf.yank(first, last), last - first + 1);

    edits++;

    return RET_FLAGS;
}

/*
 * move a subrange of lines in the edit buffer
 */
int CmdBuf::move()
{
    Int mark[26];
    Int offset, *m1, *m2;

    if (a_addr >= first - 1 && a_addr <= last) {
	EDC->error("Move to moved line");
    }

    dodo(first);
    memset(mark, '\0', sizeof(mark));
    if (a_addr < last) {
	offset = a_addr + 1 - first;
    } else {
	offset = a_addr - last;
	a_addr -= last - first + 1;
    }
    /* make a copy of the marks of the lines to move */
    for (m1 = mark, m2 = this->mark; m1 < &mark[26]; m1++, m2++) {
	if (*m2 >= first && *m2 <= last) {
	    *m1 = *m2;
	} else {
	    *m1 = 0;
	}
    }
    add(a_addr, dellines(first, last), last - first + 1);
    /* copy back adjusted marks of moved lines */
    for (m1 = mark, m2 = this->mark; m1 < &mark[26]; m1++, m2++) {
	if (*m1 != 0) {
	    *m2 = *m1 + offset;
	}
    }

    edits++;

    return RET_FLAGS;
}

/*
 * put a block in the edit buffer
 */
int CmdBuf::put()
{
    Block b;

    if (isalpha(a_buffer)) {
	/* 'a' and 'A' both refer to buffer 'a' */
	b = zbuf[tolower(a_buffer) - 'a'];
    } else {
	b = buf;
    }
    if (b == (Block) 0) {
	EDC->error("Nothing in buffer");
    }

    dodo(first);
    add(first, b, edbuf.size(b));

    edits++;

    return RET_FLAGS;
}

/*
 * yank a block of lines from the edit buffer
 */
int CmdBuf::yank()
{
    dobuf(edbuf.yank(first, last));
    return 0;
}


/*
 * shift a line left or right
 */
void CmdBuf::doshift(const char *text)
{
    CmdBuf *cb;
    int idx;

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
	cb->addblock(text);
	cb->lineno++;
    } else {
	idx += cb->shift;
	if (idx < MAX_LINE_SIZE) {
	    char buffer[MAX_LINE_SIZE];
	    char *p;

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
		cb->addblock(buffer);
		cb->lineno++;
		return;
	    }
	}

	/* Error: line too long. Finish block of lines already shifted. */
	cb->last = cb->lineno;
	cb->endblock();
	EDC->error("Result of shift would be too long");
    }
}

/*
 * shift a range of lines left or right
 */
int CmdBuf::doshift()
{
    dodo(first);
    startblock();
    lineno = first - 1;
    flags |= CB_CHANGE;
    edbuf.range(first, last, doshift, FALSE);
    endblock();

    return RET_FLAGS;
}

/*
 * shift a range of lines to the left
 */
int CmdBuf::lshift()
{
    shift = -SHIFTWIDTH(vars);
    return doshift();
}

/*
 * shift a range of lines to the right
 */
int CmdBuf::rshift()
{
    shift = SHIFTWIDTH(vars);
    return doshift();
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
 * add this line to the current block without shifting it
 */
void CmdBuf::noshift(const char *text)
{
    addblock(text);
    lineno++;
}

/*
 * Parse and indent a line of text. This isn't perfect, as
 * keywords could be defined as macros, comments are very hard to
 * handle properly, (, [ and ({ will match any of ), ] and }),
 * and last but not least everyone has his own taste of
 * indentation.
 */
void CmdBuf::indent(const char *text)
{
    static char f[] = { 7, 1, 7, 1, 2, 1, 6, 4, 2, 6, 7, 2, 0, };
    static char g[] = { 2, 2, 1, 7, 1, 5, 1, 3, 6, 2, 2, 2, 0, };
    char ident[MAX_LINE_SIZE];
    char line[MAX_LINE_SIZE];
    CmdBuf *cb;
    const char *p;
    char *sp;
    int *ip, idx;
    int top, token;
    const char *start;
    bool do_indent;

    cb = ccb;

    do_indent = FALSE;
    idx = 0;
    p = text = strcpy(line, text);

    /* process status vars */
    if (cb->quote != '\0') {
	cb->shift = 0;	/* in case a comment starts on this line */
	cb->noshift(p);
    } else if ((cb->flags & CB_PPCONTROL) || *p == '#') {
	cb->noshift(p);
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
	    cb->noshift(p);
	    return;
	} else if (cb->flags & CB_COMMENT) {
	    doshift(text);	/* use previous shift */
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
		    cb->endblock();
		    EDC->error("Unterminated string");
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
			doshift(text);
			do_indent = FALSE;
		    } else {
			const char *q;
			int idx2;

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
		    char *q;

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
	    top = UCHAR(*sp);
	    if (top == LOPERATOR && token == RHOOK) {
		/* ) after LOPERATOR is ROPERATOR */
		token = ROPERATOR;
	    }

	    if (f[top] <= g[token]) {	/* shift the token on the stack */
		int i;

		if (sp == cb->stackbot) {
		    /* out of stack. Finish already indented block. */
		    cb->last = cb->lineno;
		    cb->endblock();
		    EDC->error("Nesting too deep");
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
		    doshift(text);
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
		top = UCHAR(*sp++);
		ip++;
	    } while (f[UCHAR(*sp)] >= g[top]);
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
 * indent a range of lines
 */
int CmdBuf::indent()
{
    char s[STACKSZ];
    int i[STACKSZ];

    /* setup stacks */
    stackbot = s;
    stack = s + STACKSZ - 1;
    stack[0] = EOT;
    ind = i + STACKSZ - 1;
    ind[0] = 0;
    quote = '\0';

    dodo(first);
    startblock();
    lineno = first - 1;
    flags |= CB_CHANGE;
    flags &= ~(CB_PPCONTROL | CB_COMMENT | CB_JSKEYWORD);
    edbuf.range(first, last, indent, FALSE);
    endblock();

    return 0;
}


/*
 * join a string to the one already in the join buffer
 */
void CmdBuf::join(const char *text)
{
    CmdBuf *cb;
    char *p;

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
	EDC->error("Result of join would be too long");
    }
    strcpy(p, text);
}

/*
 * join a range of lines in the edit buffer
 */
int CmdBuf::join()
{
    char buf[MAX_LINE_SIZE + 1];
    Int *m;

    if (edbuf.lines == 0) {
	EDC->error("No lines in buffer");
    }
    if (first < 0) {
	first = cthis;
    }
    if (last < 0) {
	last = (first == edbuf.lines) ? first : first + 1;
    }

    dodo(first);

    cthis = othis = first;
    buf[0] = '\0';
    buffer = buf;
    buflen = 0;
    edbuf.range(first, last, join, FALSE);

    /* erase marks for joined lines */
    for (m = mark; m < &mark[26]; m++) {
	if (*m > first && *m <= last) {
	    *m = 0;
	}
    }

    flags |= CB_CHANGE;
    startblock();
    addblock(buf);
    endblock();

    return RET_FLAGS;
}


/*
 * add a string to the current substitute buffer
 */
void CmdBuf::sub(const char *text, unsigned int size)
{
    char *p;
    const char *q;
    unsigned int i;

    i = size;
    if (buflen + i >= MAX_LINE_SIZE) {
	if (flags & CB_CURRENTBLK) {
	    /* finish already processed block */
	    endblock();
	}
	cthis = othis = lineno;
	EDC->error("Line overflow in substitute");
    }

    p = buffer + buflen;
    q = text;
    if (flags & CB_TLOWER) {	/* lowercase one letter */
	*p++ = tolower(*q);
	q++;
	flags &= ~CB_TLOWER;
	--i;
    } else if (flags & CB_TUPPER) {	/* uppercase one letter */
	*p++ = toupper(*q);
	q++;
	flags &= ~CB_TUPPER;
	--i;
    }

    if (flags & CB_LOWER) {		/* lowercase string */
	while (i > 0) {
	    *p++ = tolower(*q);
	    q++;
	    --i;
	}
    } else if (flags & CB_UPPER) {		/* uppercase string */
	while (i > 0) {
	    *p++ = toupper(*q);
	    q++;
	    --i;
	}
    } else if (i > 0) {		/* don't change case */
	memcpy(p, q, i);
    }
    buflen += size;
}

/*
 * do substitutions in a line. If something is substituted on line
 * N, and the next substitution happens on line N + 2, line N + 1
 * is joined in the new block also.
 */
void CmdBuf::subst(const char *text)
{
    char line[MAX_LINE_SIZE];
    CmdBuf *cb;
    int idx, size;
    char *p;
    Int *k, *l;
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
    while (cb->regexp.exec(text, idx, IGNORECASE(cb->vars)) > 0) {
	if (cb->flags & CB_SKIPPED) {
	    /*
	     * add the previous line, in which nothing was substituted, to
	     * the block. Has to be done here, before the contents of the buffer
	     * are changed.
	     */
	    cb->addblock(cb->buffer);
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
	size = cb->regexp.start - text - idx;
	if (size > 0) {
	    /* copy first unchanged part of line to buffer */
	    cb->sub(text + idx, size);
	}
	p = cb->replace;
	while (*p != '\0') {
	    switch (*p) {
	    case '&':
		/* insert matching string */
		cb->sub(cb->regexp.start, cb->regexp.size);
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
		    if (cb->regexp.se[*p - '1'].start != (char*) NULL) {
			cb->sub(cb->regexp.se[*p - '1'].start,
				cb->regexp.se[*p - '1'].size);
			break;
		    }
		    /* if no subexpression, fall though */
		default:
		    cb->sub(p, 1);	/* ignore preceding backslash */
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
		cb->sub(p, 1);
		break;
	    }
	    p++;
	}

	idx = cb->regexp.start + cb->regexp.size - text;
	if (!(cb->flags & CB_GLOBSUBST) || text[idx] == '\0' ||
	    (cb->regexp.size == 0 && text[++idx] == '\0')) {
	    break;
	}
    }

    if (found) {
	if (text[idx] != '\0') {
	    /* concatenate unchanged part of line after found pattern */
	    cb->flags &= ~(CB_UPPER | CB_LOWER | CB_TUPPER | CB_TLOWER);
	    cb->sub(text + idx, strlen(text + idx));
	}
	if (!(cb->flags & CB_CURRENTBLK)) {
	    /* start a new block of lines with substitutions in them */
	    cb->flags |= CB_CHANGE;
	    cb->first = cb->lineno;
	    cb->startblock();
	    cb->flags |= CB_CURRENTBLK;
	}
	/* add this changed line to block */
	cb->buffer[cb->buflen] = '\0';
	if (newlines == 0) {
	    cb->addblock(cb->buffer);
	} else {
	    /*
	     * There were newlines in the substituted string. Add all
	     * lines to the current block, and save the marks in range.
	     */
	    p = cb->buffer;
	    do {
		cb->addblock(p);
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
	    cb->endblock();
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
 * do substitutions on a range of lines
 */
int CmdBuf::subst()
{
    char buf[MAX_LINE_SIZE], delim;
    Int m[26];
    Int edit;
    const char *p;
    Int *k, *l;

    delim = cmd[0];
    if (delim == '\0' || strchr("0123456789gpl#-+", delim) != (char*) NULL) {
	/* no search pattern & replace string specified */
	if (search[0] == '\0') {
	    EDC->error("No previous substitute to repeat");
	}
    } else if (!isalpha(delim)) {
	char *q;

	/* get search pattern */
	p = pattern(cmd + 1, delim, search);
	/* get replace string */
	q = replace;
	while (*p != '\0') {
	    if (*p == delim) {
		p++;
		break;
	    }
	    if (q == replace + STRINGSZ - 1) {
		search[0] = '\0';
		EDC->error("Replace string too large");
	    }
	    if ((*q++ = *p++) == '\\' && *p != '\0') {
		*q++ = *p++;
	    }
	}
	*q = '\0';
	cmd = p;
    } else {
	/* cause error */
	search[0] = '\0';
    }

    if (search[0] == '\0') {
	EDC->error("Missing regular expression for substitute");
    }

    /* compile regexp */
    p = regexp.comp(search);
    if (p != (char *) NULL) {
	EDC->error(p);
    }

    count();	/* get count */
    /* handle global flag */
    if (cmd[0] == 'g') {
	flags |= CB_GLOBSUBST;
	cmd++;
    } else {
	flags &= ~CB_GLOBSUBST;
    }

    /* make a blank mark table */
    moffset = m;
    for (l = m; l < &m[26]; ) {
	*l++ = 0;
    }
    offset = 0;

    /* do substitutions */
    dodo(first);
    lineno = first;
    edit = edits;
    buffer = buf;
    buflen = 0;
    flags &= ~(CB_CURRENTBLK | CB_SKIPPED);
    edbuf.range(first, last, subst, FALSE);
    if (flags & CB_CURRENTBLK) {
	/* finish current block, if needed */
	endblock();
    }

    othis = uthis;
    if (edit != edits) {
	/* some marks may have been messed up. fix them */
	for (l = m, k = mark; l < &m[26]; l++, k++) {
	    if (*l != 0) {
		*k = *l;
	    }
	}
    } else if (!(flags & CB_GLOBAL)) {
	EDC->error("Substitute pattern match failed");
    }

    return RET_FLAGS;
}


/*
 * copy a string to another buffer, unless it has length 0 or
 * is too long
 */
bool CmdBuf::getfname(char *buffer)
{
    const char *p, *q;

    /* find the end of the filename */
    p = strchr(cmd, ' ');
    q = strchr(cmd, HT);
    if (q != (char *) NULL && (p == (char *) NULL || p > q)) {
	p = q;
    }
    q = strchr(cmd, '|');
    if (q != (char *) NULL && (p == (char *) NULL || p > q)) {
	p = q;
    }
    if (p == (char *) NULL) {
	p = strchr(cmd, '\0');
    }

    /* checks */
    if (p == cmd) {
	return FALSE;
    }
    if (p - cmd >= STRINGSZ) {
	EDC->error("Filename too long");
    }

    /* copy */
    memcpy(buffer, cmd, p - cmd);
    buffer[p - cmd] = '\0';
    cmd = p;
    return TRUE;
}

/*
 * get/set the file name & current line, etc.
 */
int CmdBuf::file()
{
    not_in_global();

    if (getfname(fname)) {
	/* file name is changed: mark the file as "not edited" */
	flags |= CB_NOIMAGE;
    }

    /* give statistics */
    if (fname[0] == '\0') {
	EDC->message("No file");
    } else {
	EDC->message("\"%s\"", fname);
    }
    if (flags & CB_NOIMAGE) {
	EDC->message(" [Not edited]");
    }
    if (edits > 0) {
	EDC->message(" [Modified]");
    }
    EDC->message(" line %ld of %ld --%d%%--\012", /* LF */
		 (long) cthis, (long) edbuf.lines,
		 (edbuf.lines == 0) ? 0 : (int) ((100 * cthis) / edbuf.lines));

    return 0;
}

/*
 * insert a file in the current edit buffer
 */
int CmdBuf::read()
{
    char buffer[STRINGSZ];
    IO iob;

    not_in_global();

    if (!getfname(buffer)) {
	if (fname[0] == '\0') {
	    EDC->error("No current filename");
	}
	/* read current file, by default. I don't know why, but ex has it
	   that way. */
	strcpy(buffer, fname);
    }

    dodo(first);
    EDC->message("\"%s\" ", buffer);
    if (!iob.load(&edbuf, buffer, first)) {
	EDC->error("is unreadable");
    }
    iob.show();

    edits++;
    cthis = first + iob.lines;

    return 0;
}

/*
 * edit a new file
 */
int CmdBuf::edit()
{
    IO iob;

    not_in_global();

    if (edits > 0 && !(flags & CB_EXCL)) {
	EDC->error("No write since last change (edit! overrides)");
    }

    getfname(fname);
    if (fname[0] == '\0') {
	EDC->error("No current filename");
    }

    MM->staticMode();
    edbuf.clear();
    MM->dynamicMode();
    flags &= ~CB_NOIMAGE;
    edits = 0;
    first = cthis = 0;
    memset(mark, '\0', sizeof(mark));
    buf = 0;
    memset(zbuf, '\0', sizeof(zbuf));
    undo = (Block) -1;	/* not 0! */

    EDC->message("\"%s\" ", fname);
    if (!iob.load(&edbuf, fname, first)) {
	EDC->error("is unreadable");
    }
    iob.show();
    if (iob.zero > 0 || iob.split > 0 || iob.ill) {
	/* the editbuffer in memory is not a perfect image of the file read */
	flags |= CB_NOIMAGE;
    }

    cthis = iob.lines;

    return 0;
}

/*
 * quit editing
 */
int CmdBuf::quit()
{
    not_in_global();

    if (edits > 0 && !(flags & CB_EXCL)) {
	EDC->error("No write since last change (quit! overrides)");
    }

    return RET_QUIT;
}

/*
 * write a range of lines to a file
 */
int CmdBuf::write()
{
    char buffer[STRINGSZ];
    bool append;
    IO iob;

    not_in_global();

    if (strncmp(cmd, ">>", 2) == 0) {
	append = TRUE;
	cmd = skipst(cmd + 2);
    } else {
	append = FALSE;
    }

    /* check if write can be done */
    if (!getfname(buffer)) {
	if (fname[0] == '\0') {
	    EDC->error("No current filename");
	}
	strcpy(buffer, fname);
    }
    if (strcmp(buffer, fname) == 0) {
	if (first == 1 && last == edbuf.lines) {
	    if ((flags & (CB_NOIMAGE|CB_EXCL)) == CB_NOIMAGE) {
		EDC->error("File is changed (use w! to override)");
	    }
	} else if (!(flags & CB_EXCL)) {
	    EDC->error("Use w! to write partial buffer");
	}
    }

    EDC->message("\"%s\" ", buffer);
    if (!iob.save(&edbuf, buffer, first, last, append)) {
	EDC->error("write failed");
    }
    iob.show();

    if (first == 1 && last == edbuf.lines) {
	/* file is now perfect image of editbuffer in memory */
	flags &= ~CB_NOIMAGE;
	edits = 0;
    }

    return 0;
}

/*
 * write a range of lines to a file and quit
 */
int CmdBuf::wq()
{
    first = 1;
    last = edbuf.lines;
    write();
    return quit();
}

/*
 * write to the current file if modified, and quit
 */
int CmdBuf::xit()
{
    if (edits > 0) {
	flags |= CB_EXCL;
	return wq();
    } else {
	not_in_global();

	return RET_QUIT;
    }
}


/*
 * get/set variable(s)
 */
int CmdBuf::set()
{
    char buffer[STRINGSZ];
    const char *p;
    char *q;

    not_in_global();

    p = cmd;
    if (*p == '\0') {
	/* no arguments */
	vars.show();
    } else {
	do {
	    /* copy argument */
	    q = buffer;
	    while (*p != '\0' && *p != ' ' && *p != HT &&
		   q != buffer + STRINGSZ - 2) {
		*q++ = *p++;
	    }
	    *q = '\0';
	    /* let va_set() process it */
	    vars.set(buffer);
	    p = skipst(p);
	} while (*p != '\0');
	cmd = p;
    }
    return 0;
}
