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
# include "regexp.h"

/*
 *   Regular expression matching. This is all fairly standard. Character classes
 * are implemented as bitmaps for a fast lookup. A special case is LSTAR, which
 * is like STAR except that only the longest possible match can be a match
 * (quite a speedup in some cases).
 */

# define EOM		0x00	/*	end of match */
# define EOL		0x01	/*	pattern$ */
# define ANY		0x02	/*	.	*/
# define SINGLE		0x03	/*	'a'	*/
# define CCLASS		0x04	/*	[a-zA-Z] */
# define STAR		0x05	/*	*	*/
# define LSTAR		0x06	/*	* with longest possible match */
# define SOW		0x07	/*	\<	*/
# define EOW		0x08	/*	\>	*/
# define LBRAC		0x09	/*	\(	*/
# define RBRAC		0x0a	/*	\)	*/

# define CCLSIZE		(256 / 8)
# define CCL(ccl, op, c)	((ccl)[UCHAR(c) / 8] op (1 << ((c) & 7)))
# define CCL_BUF(rx, c)		((rx)->buffer + RXBUFSZ - CCLSIZE * UCHAR(c))
# define CCL_CODE(rx, ccl)	(((rx)->buffer + RXBUFSZ - (ccl)) / CCLSIZE)

/*
 * Create a new regular expression buffer.
 */
RxBuf::RxBuf()
{
    valid = 0;
}

/*
 * Delete a regular expression buffer.
 */
RxBuf::~RxBuf()
{
}

/*
 * Compile a regular expression. Return 0 if succesful, or an
 * errorstring otherwise.
 * There are two gotos in this function. Reading the source
 * code is considered harmful.
 */
const char *RxBuf::comp(const char *pattern)
{
    const char *p;
    char *m, *prevcode, *prevpat, *cclass, *eoln;
    char letter, c, dummy, braclist[NSUBEXP];
    int brac, depth;

    /* initialize */
    valid = 0;
    anchor = 0;
    firstc = '\0';
    cclass = buffer + RXBUFSZ;
    eoln = (char *) NULL;
    dummy = 0;
    prevpat = &dummy;
    prevcode = &dummy;
    brac = depth = 0;

    p = pattern;
    m = buffer;

    /* process the regular expression */
    while (*p) {
	switch (*p) {
	case '^':
	    if (*prevpat == 0) {	/* is this the first pattern char? */
		anchor = 1;
	    } else {
		goto single;
	    }
	    break;

	case '.':
	    prevcode = prevpat = m;
	    *m++ = ANY;
	    break;

	case '*':
	    if (*prevpat == 0) {
		return "* must follow pattern";
	    }
	    switch (*prevcode) {
	    case STAR:
	    case LSTAR:
		/* ignore */
		break;

	    case ANY:
		--m;
		prevcode = prevpat = m;
		*m++ = STAR;
		*m++ = ANY;
		break;

	    case SINGLE:
	    case CCLASS:
		letter = *--m;
		c = *--m;
		prevcode = prevpat = m;
		*m++ = STAR;
		*m++ = c;
		*m++ = letter;
		break;

	    case SOW:
		return "* follows \\<";

	    case EOW:
		/* not really an error, but too troublesome */
		return "* follows \\>";

	    case LBRAC:
		return "* follows \\(";

	    case RBRAC:
		return "* follows \\)";
	    }
	    break;

	case '[':
	    cclass -= CCLSIZE;
	    memset(cclass, '\0', CCLSIZE);
	    letter = *++p;
	    if (letter == '^') {	/* remember this for later */
		p++;
	    }
	    if (*p == ']') {
		return "Empty character class";
	    }
	    while (*p != ']') {
		if (*p == '\\') {
		    p++;
		}
		if (*p == '\0') {
		    return "Unmatched [";
		}
		CCL(cclass, |=, *p);
		if (p[1] == '-' && p[2] != ']') {	/* subrange */
		    c = *p;
		    p += 2;
		    if (*p == '\0') {
			return "Unmatched [";
		    }
		    if (UCHAR(c) > UCHAR(*p)) {
			return "Invalid character class";
		    }
		    while (UCHAR(c) <= UCHAR(*p)) {
			/* this could be done more efficiently. */
			CCL(cclass, |=, c);
			c++;
		    }
		}
		p++;
	    }
	    if (letter == '^') {
		int i;

		/* invert the whole cclass */
		i = CCLSIZE;
		do {
		    cclass[--i] ^= 0xff;
		} while (i > 0);
		*cclass &= ~1;	/* never matches 0 */
	    }

	    if (*prevpat == STAR) {
		/*
		 * if the starred pattern is followed by something that could
		 * not possibly match the starred pattern, the STAR can be
		 * changed into an LSTAR.
		 */
		if (prevpat[1] == SINGLE) {
		    if (CCL(cclass, &, prevpat[2]) == 0) {
			*prevpat = LSTAR;
		    }
		} else if (prevpat[1] == CCLASS) {
		    char *ccl2;
		    int i;

		    i = CCLSIZE;
		    ccl2 = CCL_BUF(this, i);
		    do {
			if (cclass[i] & ccl2[i]) break;
		    } while (--i > 0);
		    if (i == 0) {
			*prevpat = LSTAR;
		    }
		}
	    }
	    prevcode = prevpat = m;
	    *m++ = CCLASS;
	    *m++ = CCL_CODE(this, cclass);
	    break;

	case '\\':
	    switch (*++p) {
	    case '<':
		prevcode = m;
		*m++ = SOW;
		break;

	    case '>':
		if (*prevpat == 0) {
		    return "\\> must follow pattern";
		}
		prevcode = m;
		*m++ = EOW;
		break;

	    case '(':
		if (brac == NSUBEXP) {
		    return "Too many \\( \\) pairs";
		}
		prevcode = m;
		*m++ = LBRAC;
		*m++ = braclist[depth++] = brac++;
		break;

	    case ')':
		if (depth == 0) {
		    return "Unmatched \\)";
		}
		prevcode = m;
		*m++ = RBRAC;
		*m++ = braclist[--depth];
		break;

	    case '\0':
		return "Premature end of pattern";

	    default:
		goto single;
	    }
	    break;

	case '$':
	    eoln = m;
	    /* fall through, if it really is a EOL it will be changed later. */

	default:
	single:
	    if (*prevpat == STAR) {
		/* find out if the STAR could be made LSTAR */
		if (prevpat[1] == SINGLE) {
		    if (prevpat[2] != *p) {
			*prevpat = LSTAR;
		    }
		} else if (prevpat[1] == CCLASS) {
		    if (CCL(CCL_BUF(this, prevpat[2]), &, *p) == 0) {
			*prevpat = LSTAR;
		    }
		}
	    }
	    prevcode = prevpat = m;
	    *m++ = SINGLE;
	    *m++ = *p;
	    break;
	}

	if (m >= cclass - 2) {
	    return "Regular expression too complex";
	}
	p++;
    }

    /* the pattern has been compiled correctly, make some final checks */
    if (depth > 0) {
	return "Unmatched \\(";
    }
    start = (char *) NULL;
    while (brac < NSUBEXP) {
	se[brac++].start = (char *) NULL;	/* unused */
    }
    if (*prevpat == SINGLE && prevpat == eoln) {
	*prevpat++ = EOL;
	*prevpat = EOL;	/* won't hurt */
    }
    *m = EOM;
    if (buffer[0] == SINGLE) {
	firstc = buffer[1];	/* first char for quick search */
    }
    valid = 1;	/* buffer contains valid NFA */
    return (char *) NULL;
}

/*
 * match the text (t) against the pattern (m). Return 1 if
 * success.
 */
bool RxBuf::match(const char *start, const char *text, bool ic, char *m,
		  const char *t)
{
    const char *p;
    char *cclass, code, c;

    for (;;) {
	switch (code = *m++) {
	case EOM:
	    /* found a match */
	    this->start = t;
	    return TRUE;

	case EOL:
	    if (*t != '\0') {
		return FALSE;
	    }
	    /* cannot return at this point as a \) might still follow. */
	    continue;

	case ANY:
	    if (*t == '\0') {	/* any but '\0' */
		return FALSE;
	    }
	    break;

	case SINGLE:
	    /* match single character */
	    if (*t != *m && (!ic || tolower(*t) != tolower(*m))) {
		return FALSE;
	    }
	    m++;
	    break;

	case CCLASS:
	    /* match character class */
	    cclass = CCL_BUF(this, *m++);
	    if (CCL(cclass, &, *t) == 0) {
		if (ic) {
		    c = tolower(*t);
		    if (CCL(cclass, &, c)) {
			break;
		    }
		}
		return FALSE;
	    }
	    break;

	case STAR:
	case LSTAR:
	    p = t;
	    /* match as much characters as possible */
	    switch (*m++) {
	    case ANY:
		while (*p != '\0') {
		    p++;
		}
		break;

	    case SINGLE:
		while (*p == *m || (ic && tolower(*p) == tolower(*m))) {
		    p++;
		}
		m++;
		break;

	    case CCLASS:
		cclass = CCL_BUF(this, *m++);
		if (!ic) {
		    while (CCL(cclass, &, *p)) {
			p++;
		    }
		} else {
		    c = tolower(*p);
		    while (CCL(cclass, &, c)) {
			p++;
			c = tolower(*p);
		    }
		}
		break;
	    }
	    if (code == LSTAR) {
		/* only the maximum match is a match */
		t = p;
	    } else {
		/* try all possible lengths of the starred pattern */
		while (p > t) {
		    if (match(start, text, ic, m, p)) {
			return TRUE;
		    }
		    --p;
		}
	    }
	    continue;

	case SOW:
	    /* start of word */
	    if ((t != start && (isalnum(t[-1]) || t[-1] == '_'))
	      || (!isalpha(*t) && *t != '_')) {
		return FALSE;
	    }
	    continue;

	case EOW:
	    /* end of word */
	    if (t == start || (!isalnum(t[-1]) && t[-1] != '_')
	      || (isalnum(*t) || *t == '_')) {
		return FALSE;
	    }
	    continue;

	case LBRAC:
	    /* start of subexpression */
	    se[UCHAR(*m++)].start = t;
	    continue;

	case RBRAC:
	    /* end of subexpression */
	    se[UCHAR(*m)].size = t - se[UCHAR(*m)].start;
	    m++;
	    continue;
	}
	t++;
    }
}

/*
 * try to match a string, possibly indexed, possibly with no
 * difference between lowercase and uppercase. Return -1 if the
 * pattern is invalid, 0 if no match was found, or 1 if a match
 * was found.
 */
int RxBuf::exec(const char *text, int idx, bool ic)
{
    start = (char *) NULL;
    if (!valid) {
	return -1;
    }

    if (anchor) {
	/* the easy case */
	if (idx || !match(text, text, ic, buffer, text)) {
	    return 0;
	}
    } else {
	for (;;) {
	    if (firstc != '\0') {
		const char *p;

		/* find the first character of the pattern in the string */
		p = strchr(text + idx, firstc);
		if (ic) {
		    const char *q;

		    q = strchr(text + idx, toupper(firstc));
		    if (q != (char*) NULL && (p == (char *) NULL || p > q)) {
			p = q;
		    }
		}
		if (p != (char *) NULL) {
		    idx = p - text;
		} else {
		    return 0;
		}
	    }
	    if (match(text, text + idx, ic, buffer, text + idx)) {
		break;
	    }
	    /* if no match, try the next character in the string */
	    if (text[idx++] == '\0') {
		return 0;
	    }
	}
    }

    /* a match was found, record its starting place and length */
    size = start - text - idx;
    start -= size;
    return 1;
}
