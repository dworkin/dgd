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
 * NAME:	rxbuf->new()
 * DESCRIPTION:	Create a new regular expression buffer.
 */
rxbuf *rx_new()
{
    rxbuf *rx;

    rx = ALLOC(rxbuf, 1);
    rx->valid = 0;
    return rx;
}

/*
 * NAME:	rxbuf->del()
 * DESCRIPTION:	Delete a regular expression buffer.
 */
void rx_del(rx)
rxbuf *rx;
{
    FREE(rx);
}

/*
 * NAME:	rxbuf->comp()
 * DESCRIPTION:	Compile a regular expression. Return 0 if succesful, or an
 *		errorstring otherwise.
 *		There are two gotos in this function. Reading the source
 *		code is considered harmful.
 */
char *rx_comp(rx, pattern)
rxbuf *rx;
char *pattern;
{
    register char *p, *m, *prevcode, *prevpat, *cclass, *eoln;
    char letter, c, dummy, braclist[NSUBEXP];
    int brac, depth;

    /* initialize */
    rx->valid = 0;
    rx->anchor = 0;
    rx->firstc = '\0';
    cclass = rx->buffer + RXBUFSZ;
    eoln = (char *) NULL;
    dummy = 0;
    prevpat = &dummy;
    prevcode = &dummy;
    brac = depth = 0;

    p = pattern;
    m = rx->buffer;

    /* process the regular expression */
    while (*p) {
	switch (*p) {
	case '^':
	    if (*prevpat == 0) {	/* is this the first pattern char? */
		rx->anchor = 1;
	    } else {
		goto single;
	    }
	    break;

	case '.':
	    prevcode = prevpat = m;
	    *m++ = ANY;
	    break;

	case '*':
	    if (*prevcode == RBRAC) {
		return "Regular expression contains \\)*";
	    }
	    if (*prevcode == EOW) {
		/* not really an error, but too troublesome */
		return "Regular expression contains \\>*";
	    }
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
		register int i;

		/* invert the whole cclass */
		i = CCLSIZE;
		do {
		    cclass[i] ^= 0xff;
		} while (--i > 0);
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
		    register char *ccl2;
		    register int i;

		    i = CCLSIZE;
		    ccl2 = CCL_BUF(rx, i);
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
	    *m++ = CCL_CODE(rx, cclass);
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
		    if (CCL(CCL_BUF(rx, prevpat[2]), &, *p) == 0) {
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
    rx->start = (char *) NULL;
    while (brac < NSUBEXP) {
	rx->se[brac++].start = (char *) NULL;	/* unused */
    }
    if (*prevpat == SINGLE && prevpat == eoln) {
	*prevpat++ = EOL;
	*prevpat = EOL;	/* won't hurt */
    }
    *m = EOM;
    if (rx->buffer[0] == SINGLE) {
	rx->firstc = rx->buffer[1];	/* first char for quick search */
    }
    rx->valid = 1;	/* buffer contains valid NFA */
    return (char *) NULL;
}

/*
 * NAME:	match()
 * DESCRIPTION:	match the text (t) against the pattern (m). Return 1 if
 *		success.
 */
static bool match(rx, text, ic, m, t)
rxbuf *rx;
char *text;
bool ic;
register char *m, *t;
{
    register char *p, *cclass, code, c;

    for (;;) {
	switch (code = *m++) {
	case EOM:
	    /* found a match */
	    rx->start = t;
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
	    cclass = CCL_BUF(rx, *m++);
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
		cclass = CCL_BUF(rx, *m++);
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
		    if (match(rx, text, ic, m, p)) {
			return TRUE;
		    }
		    --p;
		}
	    }
	    continue;

	case SOW:
	    /* start of word */
	    if ((t != text && (isalnum(t[-1]) || t[-1] == '_'))
	      || (!isalpha(*t) && *t != '_')) {
		return FALSE;
	    }
	    continue;

	case EOW:
	    /* end of word */
	    if ((!isalnum(t[-1]) && t[-1] != '_')
	      || (isalpha(*t) || *t == '_')) {
		return FALSE;
	    }
	    continue;

	case LBRAC:
	    /* start of subexpression */
	    rx->se[*m++].start = t;
	    continue;

	case RBRAC:
	    /* end of subexpression */
	    rx->se[*m].size = t - rx->se[*m].start;
	    m++;
	    continue;
	}
	t++;
    }
}

/*
 * NAME:	rxbuf->exec()
 * DESCRIPTION:	try to match a string, possibly indexed, possibly with no
 *		difference between lowercase and uppercase. Return -1 if the
 *		pattern is invalid, 0 if no match was found, or 1 if a match
 *		was found.
 */
int rx_exec(rx, text, idx, ic)
register rxbuf *rx;
register char *text;
register int idx;
int ic;
{
    rx->start = (char *) NULL;
    if (!rx->valid) {
	return -1;
    }

    if (rx->anchor) {
	/* the easy case */
	if (idx || !match(rx, text, ic, rx->buffer, text)) {
	    return 0;
	}
    } else {
	for (;;) {
	    if (rx->firstc != '\0') {
		register char *p;

		/* find the first character of the pattern in the string */
		p = strchr(text + idx, rx->firstc);
		if (ic) {
		    register char *q;

		    q = strchr(text + idx, toupper(rx->firstc));
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
	    if (match(rx, text + idx, ic, rx->buffer, text + idx)) {
		break;
	    }
	    /* if no match, try the next character in the string */
	    if (text[idx++] == '\0') {
		return 0;
	    }
	}
    }

    /* a match was found, record its starting place and length */
    rx->size = rx->start - text - idx;
    rx->start -= rx->size;
    return 1;
}
