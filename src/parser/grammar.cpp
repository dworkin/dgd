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
# include "dgd.h"
# include "hash.h"
# include "str.h"
# include "grammar.h"

# define STORE2(p, n)	((p)[0] = (n) >> 8, (p)[1] = (n))
# define STORE3(p, n)	((p)[0] = (n) >> 16, (p)[1] = (n) >> 8, (p)[2] = (n))

# define TOK_NULL	0	/* nothing */
# define TOK_REGEXP	1	/* regular expression */
# define TOK_STRING	2	/* string */
# define TOK_ESTRING	3	/* string */
# define TOK_PRODSYM	4	/* left hand of production rule */
# define TOK_TOKSYM	5	/* left hand of token rule */
# define TOK_SYMBOL	6	/* symbol in rhs of production rule */
# define TOK_QUEST	7	/* question mark */
# define TOK_ERROR	8	/* bad token */
# define TOK_BADREGEXP  9	/* malformed regular expression */
# define TOK_TOOBIGRGX 10	/* too big regular expression */
# define TOK_BADSTRING 11	/* malformed string constant */
# define TOK_TOOBIGSTR 12	/* string constant too long */
# define TOK_TOOBIGSYM 13	/* symbol too long */

class RgxNode {
public:
    static int create(char *buffer, int len, char *str, RgxNode *node,
		      int thisnode, int *lastp);
    static int token(String *str, ssizet *strlen, char *buffer,
		     unsigned int *buflen);

private:
    unsigned short type;	/* node type */
    unsigned short left;	/* left child node or other info */
    unsigned short right;	/* right child node or other info */
    char offset;		/* high byte of offset */
    char len;			/* length */
};

# define RGX_CHAR	0	/* single char */
# define RGX_CONCAT	1	/* concatenation */
# define RGX_STAR	2	/* rgx*, rgx+, rgx? */
# define RGX_ALT	3	/* rgx|rgx */
# define RGX_PAREN	4	/* (rgx) */

/*
 * construct pre-parsed regular expression
 */
int RgxNode::create(char *buffer, int len, char *str, RgxNode *node,
		    int thisnode, int *lastp)
{
    int last, n;

    last = *lastp;

    while (thisnode >= 0) {
	/* connect from previous */
	while (last >= 0) {
	    buffer[UCHAR(node[last].len)] = len;
	    last = (short) node[last].left;
	}

	switch (node[thisnode].type) {
	case RGX_CHAR:
	    /*
	     * x_
	     *  ->
	     */
	    memcpy(buffer + len, str + node[thisnode].left +
				 (UCHAR(node[thisnode].offset) << 16),
		   UCHAR(node[thisnode].len));
	    len += UCHAR(node[thisnode].len);
	    node[thisnode].len = len++;
	    node[thisnode].left = last;
	    last = thisnode;
	    thisnode = (short) node[thisnode].right;
	    break;

	case RGX_CONCAT:
	    /* concatenated nodes */
	    len = create(buffer, len, str, node, node[thisnode].left, &last);
	    thisnode = (short) node[thisnode].right;
	    break;

	case RGX_STAR:
	    /*
	     *    *        +         ?
	     * <-----	<-----         ->
	     * |_XXX_	+_XXX_    |_XXX_
	     *  ----->	 ----->    ----->
	     */
	    buffer[n = len] = (node[thisnode].right == '+') ? '+' : '|';
	    len += 2;
	    len = create(buffer, len, str, node, node[thisnode].left, &last);
	    if (node[thisnode].right != '?') {
		while (last >= 0) {
		    buffer[UCHAR(node[last].len)] = n;
		    last = (short) node[last].left;
		}
	    }
	    node[thisnode].len = n + 1;
	    node[thisnode].left = last;
	    *lastp = thisnode;
	    return len;

	case RGX_ALT:
	    /*
	     *      ----->
	     * |_XXX_YYY_
	     *  ----->  ->
	     */
	    buffer[len++] = '|';
	    n = len++;
	    len = create(buffer, len, str, node, node[thisnode].left, &last);
	    buffer[n] = len;
	    n = -1;
	    len = create(buffer, len, str, node, node[thisnode].right, &n);
	    while (n >= 0) {
		thisnode = (short) node[n].left;
		node[n].left = last;
		last = n;
		n = thisnode;
	    }
	    *lastp = last;
	    return len;

	case RGX_PAREN:
	    /* (X) */
	    thisnode = (short) node[thisnode].left;
	    break;
	}
    }

    *lastp = last;
    return len;
}

/*
 * get a token from the grammar string
 */
int RgxNode::token(String *str, ssizet *strlen, char *buffer,
		   unsigned int *buflen)
{
    RgxNode node[2 * STRINGSZ];
    short nstack[STRINGSZ];
    int paren, thisnode, topnode, lastnode, strtok;
    ssizet offset;
    char *p;
    char *q;
    ssizet size;
    unsigned int len;

    size = *strlen;
    p = str->text + str->len - size;
    while (size != 0) {
	--size;
	switch (*p) {
	case ' ':
	case HT:
	case LF:
	    /* whitespace */
	    break;

	case '?':
	    *strlen = size;
	    return TOK_QUEST;

	case '/':
	    /* regular expression */
	    topnode = lastnode = thisnode = -1;
	    paren = 0;
	    p++;
	    len = 0;
	    while (*p != '/') {
		if (size == 0) {
		    return TOK_BADREGEXP;
		}
		--size;
		switch (*p) {
		case '*':
		case '+':
		case '?':
		    /* repeat a number of times */
		    if (thisnode < 0 ||
			(node[thisnode].type != RGX_CHAR &&
			 node[thisnode].type != RGX_PAREN)) {
			return TOK_BADREGEXP;
		    }
		    len += 2;
		    if (len >= STRINGSZ || lastnode == 2 * STRINGSZ - 1) {
			return TOK_TOOBIGRGX;
		    }
		    node[++lastnode] = node[thisnode];
		    node[thisnode].type = RGX_STAR;
		    node[thisnode].left = lastnode;
		    node[thisnode].right = *p;
		    break;

		case '|':
		    /* alternative */
		    if (topnode < 0) {
			return TOK_BADREGEXP;
		    }
		    len += 2;
		    if (len >= STRINGSZ || lastnode == 2 * STRINGSZ - 1) {
			return TOK_TOOBIGRGX;
		    }
		    node[thisnode = ++lastnode].type = RGX_ALT;
		    node[thisnode].left = topnode;
		    topnode = thisnode;
		    break;

		case '(':
		    /* opening parenthesis */
		    if (paren == STRINGSZ || lastnode >= 2 * STRINGSZ - 2) {
			return TOK_TOOBIGRGX;
		    }
		    if (thisnode < 0) {
			/* no previous node */
			topnode = thisnode = ++lastnode;
		    } else if (node[thisnode].type == RGX_CHAR ||
			       node[thisnode].type == RGX_ALT) {
			/* auto-link from previous node */
			node[thisnode].right = ++lastnode;
			thisnode = lastnode;
		    } else {
			/* concatenate with previous node */
			node[++lastnode] = node[thisnode];
			node[thisnode].type = RGX_CONCAT;
			node[thisnode].left = lastnode;
			node[thisnode].right = ++lastnode;
			thisnode = lastnode;
		    }
		    node[thisnode].type = RGX_PAREN;

		    nstack[paren++] = topnode;
		    nstack[paren++] = thisnode;
		    topnode = thisnode = -1;
		    break;

		case ')':
		    /* closing parenthesis */
		    if (paren == 0 || topnode < 0 ||
			node[thisnode].type == RGX_ALT) {
			return TOK_BADREGEXP;
		    }
		    thisnode = nstack[--paren];
		    node[thisnode].left = topnode;
		    topnode = nstack[--paren];
		    break;

		default:
		    if (lastnode >= 2 * STRINGSZ - 2) {
			return TOK_TOOBIGRGX;
		    }
		    if (thisnode < 0) {
			/* no previous node */
			topnode = thisnode = ++lastnode;
		    } else if (node[thisnode].type == RGX_CHAR ||
			       node[thisnode].type == RGX_ALT) {
			/* auto-link from previous node */
			node[thisnode].right = ++lastnode;
			thisnode = lastnode;
		    } else {
			/* concatenate with previous node */
			node[++lastnode] = node[thisnode];
			node[thisnode].type = RGX_CONCAT;
			node[thisnode].left = lastnode;
			node[thisnode].right = ++lastnode;
			thisnode = lastnode;
		    }

		    q = p;
		    if (*p == '[') {
			/*
			 * character class
			 */
			p++;
			if (*p == '^') {
			    --size;
			    p++;
			}
			if (*p == ']') {
			    return TOK_BADREGEXP; /* empty character class */
			}
			do {
			    if (*p == '\\') {
				--size;
				p++;
			    }
			    if (size == 0) {
				return TOK_BADREGEXP;
			    }
			    --size;
			    if (p[1] == '-' && p[2] != ']') {
				/* a-b */
				if (size < 2 || UCHAR(*p) > UCHAR(p[2])) {
				    return TOK_BADREGEXP; /* malformed regexp */
				}
				size -= 2;
				p += 2;
			    }
			} while (*++p != ']');
			--size;
		    } else if (*p == '\\') {
			/*
			 * escaped character, copy both \ and char
			 */
			if (size == 0) {
			    return TOK_BADREGEXP;
			}
			--size;
			p++;
			if (*p != '+' && *p != '|' && *p != '[' && *p != '.' &&
			    *p != '\\') {
			    q++;
			}
		    }

		    node[thisnode].type = RGX_CHAR;
		    offset = q - str->text;
		    node[thisnode].left = offset;
#if SSIZET_MAX > USHRT_MAX
		    node[thisnode].offset = offset >> 16;
#else
		    node[thisnode].offset = 0;
#endif
		    node[thisnode].right = -1;
		    node[thisnode].len = p - q + 1;
		    len += p - q + 2;
		    if (len >= STRINGSZ) {
			return TOK_TOOBIGRGX;
		    }
		    break;
		}
		p++;
	    }

	    if (thisnode < 0 || node[thisnode].type == RGX_ALT || paren != 0) {
		return TOK_BADREGEXP;
	    }
	    thisnode = -1;
	    len = create(buffer, 0, str->text, node, topnode, &thisnode);
	    while (thisnode >= 0) {
		buffer[UCHAR(node[thisnode].len)] = len - 1;
		thisnode = (short) node[thisnode].left;
	    }
	    buffer[len] = '\0';
	    *buflen = len;
	    *strlen = size - 1;
	    return TOK_REGEXP;

	case '\'':
	    /* string */
	    p++;
	    strtok = TOK_STRING;
	    len = 0;
	    while (*p != '\'') {
		if (size == 0) {
		    return TOK_BADSTRING;
		}
		--size;
		if (*p == '\\') {
		    /* escaped character */
		    if (size == 0) {
			return TOK_BADSTRING;
		    }
		    if (len != 0) {
			strtok = TOK_ESTRING;
		    }
		    --size;
		    p++;
		}

		if (len == STRINGSZ - 1) {
		    return TOK_TOOBIGSTR;
		}
		*buffer++ = *p++;
		len++;
	    }
	    if (len == 0) {
		return TOK_BADSTRING;
	    }
	    *buffer = '\0';
	    *buflen = len;
	    *strlen = size - 1;
	    return strtok;

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
	case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
	case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
	case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '_':
	    /* symbol */
	    *buffer++ = *p++;
	    len = 1;
	    while (isalnum(*p) || *p == '_') {
		if (len == STRINGSZ - 1) {
		    return TOK_TOOBIGSYM;
		}
		*buffer++ = *p++;
		len++;
		--size;
	    }
	    *buffer = '\0';
	    *buflen = len;

	    /* see if it's followed by = or : */
	    while (*p == ' ' || *p == HT || *p == LF) {
		p++;
		--size;
	    }
	    if (*p == '=') {
		/* start of token rule */
		*strlen = size - 1;
		return TOK_TOKSYM;
	    } else if (*p == ':') {
		/* start of production rule */
		*strlen = size - 1;
		return TOK_PRODSYM;
	    }
	    *strlen = size;
	    return TOK_SYMBOL;

	default:
	    /* bad token */
	    return TOK_ERROR;
	}
	p++;
    }

    /* nothing at all */
    return TOK_NULL;
}


class Rule : public Hash::Entry, public ChunkAllocated {
public:
    static Rule *create(class RlChunk **c, int type);

    String *symb;		/* rule symbol */
    short type;			/* unknown, token or production rule */
    unsigned short num;		/* number of alternatives, or symbol number */
    Uint len;			/* length of rule, or offset in grammar */
    union {
	String *rgx;		/* regular expression */
	class RuleSym *syms;	/* linked list of rule elements */
    };
    String *func;		/* optional LPC function */
    Rule *alt, **last;		/* first and last in alternatives list */
    Rule *list;			/* next in linked list */
};

# define RULE_UNKNOWN	0	/* unknown rule symbol */
# define RULE_REGEXP	1	/* regular expression rule */
# define RULE_STRING	2	/* string rule */
# define RULE_PROD	3	/* production rule */

# define RLCHUNKSZ	32

class RlChunk : public Chunk<Rule, RLCHUNKSZ> {
public:
    /*
     * iterate through items from destructor
     */
    virtual ~RlChunk() {
	items();
    }

    /*
     * dereference strings when iterating through items
     */
    virtual bool item(Rule *rl) {
	if (rl->symb != (String *) NULL) {
	    rl->symb->del();
	}
	if (rl->type == RULE_REGEXP && rl->rgx != (String *) NULL) {
	    rl->rgx->del();
	}
	if (rl->func != (String *) NULL) {
	    rl->func->del();
	}
	return TRUE;
    }
};

/*
 * allocate a new rule
 */
Rule *Rule::create(RlChunk **c, int type)
{
    Rule *rl;

    if (*c == (RlChunk *) NULL) {
	*c = new RlChunk;
    }
    rl = chunknew (**c) Rule;
    rl->symb = (String *) NULL;
    rl->type = type;
    rl->num = 0;
    rl->len = 0;
    rl->syms = (RuleSym *) NULL;
    rl->func = (String *) NULL;
    rl->alt = rl->list = (Rule *) NULL;
    rl->last = &rl->alt;

    return rl;
}

class RuleSym : public ChunkAllocated {
public:
    static RuleSym *create(class RsChunk **c, Rule *rl);

    Rule *rule;			/* symbol */
    RuleSym *next;		/* next in rule */
};

# define RSCHUNKSZ	64

class RsChunk : public Chunk<RuleSym, RSCHUNKSZ> {
};

/*
 * allocate a new rulesym
 */
RuleSym *RuleSym::create(RsChunk **c, Rule *rl)
{
    RuleSym *rs;

    if (*c == (RsChunk *) NULL) {
	*c = new RsChunk;
    }

    rs = chunknew (**c) RuleSym;
    rs->rule = rl;
    rs->next = (RuleSym *) NULL;
    return rs;
}

/*
 * Internal grammar string description:
 *
 * header	[2]	version number
 *		[x][y]	whitespace rule or -1
 *		[x][y]	nomatch rule or -1
 *		[x][y]	# regexp rules
 *		[x][y]	# total regexp rules (+ alternatives)
 *		[x][y]	# string rules
 *		[x][y]	# escaped string rules
 *		[x][y]	# production rules (first is starting rule)
 *		[x][y]	# total production rules (+ alternatives)
 *
 * rgx offset	[x][y]	regexp rule offsets
 *		...
 *
 * str offset	[...]	str:
 *			[x][y][z] offset in source
 *			[x]	  length of string
 *		...
 *
 * estr offset	[x][y]	string rule offsets
 *		...
 *
 * prod offset	[x][y]	production rule offsets
 *		...
 *
 * regexp rule	[x][y]	number of alternatives
 *		[x]	length of regexp		} ...
 *		[...]	regexp				}
 *
 * string rule	[x]	length of string		} ...
 *		[...]	string				}
 *
 * prod rule	[x][y]	number of alternatives
 *		[x]	number of symbols in rule	}
 *		[x]	length of rule			}
 *		[...]	rule:				} ...
 *			[x][y]	token or rule ...	}
 *			[...]	optional: function name	}
 */

/*
 * create a pre-processed grammar string
 */
String *Grammar::create(Rule *rgxlist, Rule *strlist, Rule *estrlist,
			Rule *prodlist, int nrgx, int nstr, int nestr,
			int nprod, long size)
{
    int start, prod1;
    String *gram;
    char *p, *q;
    Rule *rl, *r;
    RuleSym *rs;
    int n;

    gram = String::create((char *) NULL, size);

    /* header */
    p = gram->text;
    *p++ = GRAM_VERSION;	/* version number */
    STORE2(p, -1); p += 2;	/* whitespace rule */
    STORE2(p, -1); p += 2;	/* nomatch rule */
    STORE2(p, nrgx); p += 4;	/* # regular expression rules */
    STORE2(p, nstr); p += 2;	/* # string rules */
    STORE2(p, nestr); p += 2;	/* # escaped string rules */
    nprod++;			/* +1 for start rule */
    STORE2(p, nprod);		/* # production rules */
    n = nrgx + nstr + nestr + nprod;
    prod1 = nrgx + nstr + nestr + 1;
    q = p + 4 + ((n + nstr) << 1);
    p = gram->text + size;

    /* determine production rule offsets */
    for (rl = prodlist; rl != (Rule *) NULL; rl = rl->list) {
	size -= (rl->num << 1) + rl->len + 2;
	p -= (rl->num << 1) + rl->len + 2;
	q -= 2; STORE2(q, size);
	STORE2(p, rl->num);
	rl->num = --n;
	rl->len = size;
    }

    /* start rule offset */
    size -= 6;
    p -= 6;
    q -= 2; STORE2(q, size);
    --n;
    start = size;

    /* deal with strings */
    for (rl = estrlist; rl != (Rule *) NULL; rl = rl->list) {
	size -= rl->symb->len + 1;
	p -= rl->symb->len + 1;
	q -= 2; STORE2(q, size);
	rl->num = --n;
	*p = rl->symb->len;
	memcpy(p + 1, rl->symb->text, rl->symb->len);
    }
    for (rl = strlist; rl != (Rule *) NULL; rl = rl->list) {
	*--q = rl->symb->len;
	q -= 3; STORE3(q, rl->len);
	rl->num = --n;
    }

    /* deal with regexps */
    nrgx = 0;
    for (rl = rgxlist; rl != (Rule *) NULL; rl = rl->list) {
	size -= rl->num + rl->len + 2;
	q -= 2; STORE2(q, size);
	p = gram->text + size;
	STORE2(p, rl->num);
	rl->num = --n;
	p += 2;
	for (r = rl; r != (Rule *) NULL; r = r->alt) {
	    if (r->rgx != (String *) NULL) {
		*p++ = r->rgx->len;
		memcpy(p, r->rgx->text, r->rgx->len);
		p += r->rgx->len;
		nrgx++;
	    } else {
		/* nomatch */
		STORE2(gram->text + 3, n);
	    }
	}
	if (rl->symb->len == 10 && strcmp(rl->symb->text, "whitespace") == 0) {
	    p = gram->text + 1;
	    STORE2(p, n);
	}
    }
    p = gram->text + 7;
    STORE2(p, nrgx);		/* total regular expressions */

    /* fill in production rules */
    nprod = 1;
    for (rl = prodlist; rl != (Rule *) NULL; rl = rl->list) {
	q = gram->text + rl->len + 2;
	for (r = rl; r != (Rule *) NULL; r = r->alt) {
	    p = q + 2;
	    n = 0;
	    for (rs = r->syms; rs != (RuleSym *) NULL; rs = rs->next) {
		STORE2(p, rs->rule->num); p += 2;
		n++;
	    }
	    if (r->func != (String *) NULL) {
		memcpy(p, r->func->text, r->func->len + 1);
		p += r->func->len + 1;
	    }
	    *q++ = n;
	    *q = p - q - 1;
	    q = p;
	    nprod++;
	}
    }

    /* start rule */
    p = gram->text + start;
    *p++ = 0;
    *p++ = 1;
    *p++ = 1;
    *p++ = 2;
    *p++ = prod1 >> 8;
    *p   = prod1;

    p = gram->text + 15;
    STORE2(p, nprod);

    return gram;
}

/*
 * check the grammar, return a pre-processed version
 */
String *Grammar::parse(String *gram)
{
    char buffer[STRINGSZ];
    Hash::Hashtab *ruletab, *strtab;
    RsChunk *rschunks;
    RlChunk *rlchunks;
    Rule *rgxlist, *strlist, *estrlist, *prodlist, *tmplist, *rr, *rrl;
    int token, ruleno, nrgx, nstr, nestr, nprod;
    ssizet glen;
    unsigned int buflen;
    bool nomatch;
    RuleSym **rs;
    Rule *rl, **r;
    long size;
    unsigned int len;

# if MAX_STRLEN > 0xffffffL
    if (gram->len > 0xffffffL) {
	EC->error("Grammar string too large");
    }
# endif

    /* initialize */
    ruletab = HM->create(PARSERULTABSZ, PARSERULHASHSZ, FALSE);
    strtab = HM->create(PARSERULTABSZ, PARSERULHASHSZ, FALSE);
    rschunks = (RsChunk *) NULL;
    rlchunks = (RlChunk *) NULL;
    rgxlist = strlist = estrlist = prodlist = tmplist = (Rule *) NULL;
    nrgx = nstr = nestr = nprod = 0;
    size = 17 + 8;	/* size of header + start rule */
    glen = gram->len;
    nomatch = FALSE;

    token = RgxNode::token(gram, &glen, buffer, &buflen);
    for (ruleno = 1; ; ruleno++) {
	switch (token) {
	case TOK_TOKSYM:
	    /*
	     * token rule definition
	     */
	    r = (Rule **) ruletab->lookup(buffer, TRUE);
	    if (*r != (Rule *) NULL) {
		if ((*r)->type == RULE_UNKNOWN) {
		    /* replace unknown rule */
		    rl = *r;
		    rl->type = RULE_REGEXP;
		    size += 4;
		    nrgx++;

		    if (rl->alt != (Rule *) NULL) {
			rl->alt->list = rl->list;
		    } else {
			tmplist = rl->list;
		    }
		    if (rl->list != (Rule *) NULL) {
			rl->list->alt = rl->alt;
		    }
		    rl->alt = (Rule *) NULL;
		    rl->list = rgxlist;
		    rgxlist = rl;
		} else if ((*r)->type == RULE_REGEXP) {
		    /* new alternative regexp */
		    rl = Rule::create(&rlchunks, RULE_REGEXP);

		    *((*r)->last) = rl;
		    (*r)->last = &rl->alt;
		} else {
		    snprintf(buffer, sizeof(buffer),
			     "Rule %d previously defined as production rule",
			     ruleno);
		    goto err;
		}
	    } else {
		/* new rule */
		rl = Rule::create(&rlchunks, RULE_REGEXP);
		rl->symb = String::create(buffer, buflen);
		rl->symb->ref();
		rl->name = rl->symb->text;
		rl->next = *r;
		*r = rl;
		size += 4;
		nrgx++;

		rl->list = rgxlist;
		rgxlist = rl;
	    }

	    switch (RgxNode::token(gram, &glen, buffer, &buflen)) {
	    case TOK_REGEXP:
		rl->rgx = String::create(buffer, buflen);
		rl->rgx->ref();
		(*r)->num++;
		(*r)->len += buflen;
		size += buflen + 1;
		break;

	    case TOK_BADREGEXP:
		snprintf(buffer, sizeof(buffer),
			 "Rule %d: malformed regular expression", ruleno);
		goto err;

	    case TOK_TOOBIGRGX:
		snprintf(buffer, sizeof(buffer),
			 "Rule %d: regular expression too large", ruleno);
		goto err;

	    case TOK_SYMBOL:
		if (buflen == 7 && strcmp(buffer, "nomatch") == 0) {
		    if (nomatch) {
			snprintf(buffer, sizeof(buffer),
				 "Rule %d: extra nomatch rule", ruleno);
			goto err;
		    }
		    nomatch = TRUE;
		    rl->rgx = (String *) NULL;
		    break;
		}
		/* fall through */
	    default:
		snprintf(buffer, sizeof(buffer),
			 "Rule %d: regular expression expected", ruleno);
		goto err;
	    }

	    /* next token */
	    token = RgxNode::token(gram, &glen, buffer, &buflen);
	    break;

	case TOK_PRODSYM:
	    /*
	     * production rule definition
	     */
	    r = (Rule **) ruletab->lookup(buffer, TRUE);
	    if (*r != (Rule *) NULL) {
		if ((*r)->type == RULE_UNKNOWN) {
		    /* replace unknown rule */
		    rl = *r;
		    rl->type = RULE_PROD;
		    size += 4;
		    nprod++;

		    if (rl->alt != (Rule *) NULL) {
			rl->alt->list = rl->list;
		    } else {
			tmplist = rl->list;
		    }
		    if (rl->list != (Rule *) NULL) {
			rl->list->alt = rl->alt;
		    }
		    rl->alt = (Rule *) NULL;
		    rl->list = prodlist;
		    prodlist = rl;
		} else if ((*r)->type == RULE_PROD) {
		    /* new alternative production */
		    rl = Rule::create(&rlchunks, RULE_PROD);

		    *((*r)->last) = rl;
		    (*r)->last = &rl->alt;
		} else {
		    snprintf(buffer, sizeof(buffer),
			     "Rule %d previously defined as token rule",
			     ruleno);
		    goto err;
		}
	    } else {
		/* new rule */
		rl = Rule::create(&rlchunks, RULE_PROD);
		rl->symb = String::create(buffer, buflen);
		rl->symb->ref();
		rl->name = rl->symb->text;
		rl->next = *r;
		*r = rl;
		size += 4;
		nprod++;

		rl->list = prodlist;
		prodlist = rl;
	    }

	    rr = *r;
	    rrl = rl;
	    rs = &rl->syms;
	    len = 0;
	    for (;;) {
		switch (token = RgxNode::token(gram, &glen, buffer, &buflen)) {
		case TOK_SYMBOL:
		    /*
		     * symbol
		     */
		    r = (Rule **) ruletab->lookup(buffer, TRUE);
		    if (*r == (Rule *) NULL) {
			/* new unknown rule */
			rl = Rule::create(&rlchunks, RULE_UNKNOWN);
			rl->symb = String::create(buffer, buflen);
			rl->symb->ref();
			rl->name = rl->symb->text;
			rl->next = *r;
			*r = rl;

			rl->list = tmplist;
			if (tmplist != (Rule *) NULL) {
			    tmplist->alt = rl;
			}
			tmplist = rl;
		    } else {
			/* previously known rule */
			rl = *r;
		    }
		    *rs = RuleSym::create(&rschunks, rl);
		    rs = &(*rs)->next;
		    len += 2;
		    continue;

		case TOK_STRING:
		case TOK_ESTRING:
		    /*
		     * string
		     */
		    r = (Rule **) strtab->lookup(buffer, FALSE);
		    while (*r != (Rule *) NULL) {
			if ((*r)->symb->len == buflen &&
			    memcmp((*r)->symb->text, buffer, buflen) == 0) {
			    break;
			}
			r = (Rule **) &(*r)->list;
		    }
		    if (*r == (Rule *) NULL) {
			/* new string rule */
			rl = Rule::create(&rlchunks, RULE_STRING);
			rl->symb = String::create(buffer, buflen);
			rl->symb->ref();
			rl->name = rl->symb->text;
			rl->next = *r;
			*r = rl;

			if (token == TOK_STRING) {
			    size += 4;
			    nstr++;
			    rl->len = gram->len - glen - buflen - 1;
			    rl->list = strlist;
			    strlist = rl;
			} else {
			    size += 3 + buflen;
			    nestr++;
			    rl->list = estrlist;
			    estrlist = rl;
			}
		    } else {
			/* existing string rule */
			rl = *r;
		    }
		    *rs = RuleSym::create(&rschunks, rl);
		    rs = &(*rs)->next;
		    len += 2;
		    continue;

		case TOK_QUEST:
		    /*
		     * ? function
		     */
		    if (RgxNode::token(gram, &glen, buffer, &buflen) !=
								TOK_SYMBOL) {
			snprintf(buffer, sizeof(buffer),
				 "Rule %d: function name expected", ruleno);
			goto err;
		    }
		    rrl->func = String::create(buffer, buflen);
		    rrl->func->ref();
		    len += buflen + 1;

		    token = RgxNode::token(gram, &glen, buffer, &buflen);
		    /* fall through */
		default:
		    break;
		}
		break;
	    }

	    if (len > 255) {
		snprintf(buffer, sizeof(buffer), "Rule %d is too long", ruleno);
		goto err;
	    }
	    rr->num++;
	    rr->len += len;
	    size += len + 2;
	    break;

	case TOK_NULL:
	    /*
	     * end of grammar
	     */
	    if (tmplist != (Rule *) NULL) {
		snprintf(buffer, sizeof(buffer), "Undefined symbol %s",
			 tmplist->symb->text);
		goto err;
	    }
	    if (rgxlist == (Rule *) NULL) {
		strcpy(buffer, "No tokens");
		goto err;
	    }
	    if (prodlist == (Rule *) NULL) {
		strcpy(buffer, "No starting rule");
		goto err;
	    }
	    if (size > (long) USHRT_MAX) {
		strcpy(buffer, "Grammar too large");
		goto err;
	    }
	    gram = create(rgxlist, strlist, estrlist, prodlist, nrgx, nstr,
			  nestr, nprod, size);
	    delete rschunks;
	    delete rlchunks;
	    delete strtab;
	    delete ruletab;
	    return gram;

	case TOK_ERROR:
	    snprintf(buffer, sizeof(buffer), "Rule %d: bad token", ruleno);
	    goto err;

	case TOK_BADREGEXP:
	    snprintf(buffer, sizeof(buffer),
		     "Rule %d: malformed regular expression", ruleno);
	    goto err;

	case TOK_TOOBIGRGX:
	    snprintf(buffer, sizeof(buffer),
		     "Rule %d: regular expression too large", ruleno);
	    goto err;

	case TOK_BADSTRING:
	    snprintf(buffer, sizeof(buffer),
		     "Rule %d: malformed string constant", ruleno);
	    goto err;

	case TOK_TOOBIGSTR:
	    snprintf(buffer, sizeof(buffer), "Rule %d: string too long",
		     ruleno);
	    goto err;

	case TOK_TOOBIGSYM:
	    snprintf(buffer, sizeof(buffer), "Rule %d: symbol too long",
		     ruleno);
	    goto err;

	default:
	    snprintf(buffer, sizeof(buffer), "Rule %d: unexpected token",
		     ruleno);
	    goto err;
	}
    }

err:
    delete rschunks;
    delete rlchunks;
    delete strtab;
    delete ruletab;
    EC->error(buffer);
    return NULL;
}
