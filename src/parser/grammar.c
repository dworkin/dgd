# define INCLUDE_CTYPE
# include "dgd.h"
# include "hash.h"
# include "str.h"
# include "grammar.h"

# define STORE2(p, n)	((p)[0] = (n) >> 8, (p)[1] = (n))

# define TOK_NULL	0	/* nothing */
# define TOK_REGEXP	1	/* regular expression */
# define TOK_STRING	2	/* string */
# define TOK_PRODSYM	3	/* left hand of production rule */
# define TOK_TOKSYM	4	/* left hand of token rule */
# define TOK_SYMBOL	5	/* symbol in rhs of production rule */
# define TOK_QUEST	6	/* question mark */
# define TOK_ERROR	7	/* bad token */
# define TOK_BADREGEXP  8	/* malformed regular expression */
# define TOK_TOOBIGRGX  9	/* too big regular expression */
# define TOK_BADSTRING 10	/* malformed string constant */
# define TOK_TOOBIGSTR 11	/* string constant too long */
# define TOK_TOOBIGSYM 12	/* symbol too long */

typedef struct {
    unsigned short type;	/* node type */
    unsigned short left;	/* left child node or other info */
    unsigned short right;	/* right child node or other info */
    char offset;		/* high byte of offset */
    char len;			/* length */
} rgxnode;

# define RGX_CHAR	0	/* single char */
# define RGX_CONCAT	1	/* concatenation */
# define RGX_STAR	2	/* rgx*, rgx+, rgx? */
# define RGX_ALT	3	/* rgx|rgx */
# define RGX_PAREN	4	/* (rgx) */

/*
 * NAME:	rgxtok()
 * DESCRIPTION:	construct pre-parsed regular expression
 */
static int rgxtok(buffer, len, str, node, thisnode, lastp)
char *buffer, *str;
register int len, thisnode;
register rgxnode *node;
int *lastp;
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
	    len = rgxtok(buffer, len, str, node, node[thisnode].left,
			 &last);
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
	    len = rgxtok(buffer, len, str, node, node[thisnode].left,
			 &last);
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
	    len = rgxtok(buffer, len, str, node, node[thisnode].left,
			 &last);
	    buffer[n] = len;
	    n = -1;
	    len = rgxtok(buffer, len, str, node, node[thisnode].right, &n);
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
 * NAME:	gramtok()
 * DESCRIPTION:	get a token from the grammar string
 */
static int gramtok(str, strlen, buffer, buflen)
string *str;
ssizet *strlen;
register char *buffer;
unsigned int *buflen;
{
    rgxnode node[2 * STRINGSZ];
    short nstack[STRINGSZ];
    int paren, thisnode, topnode, lastnode;
    ssizet offset;
    register char *p;
    char *q;
    register ssizet size;
    register unsigned int len;

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
		    node[thisnode].offset = offset >> 16;
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
	    len = rgxtok(buffer, 0, str->text, node, topnode, &thisnode);
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
	    return TOK_STRING;

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


typedef struct _rulesym_ {
    struct _rule_ *rule;	/* symbol */
    struct _rulesym_ *next;	/* next in rule */
} rulesym;

typedef struct _rule_ {
    hte chain;			/* hash table chain */
    string *symb;		/* rule symbol */
    short type;			/* unknown, token or production rule */
    unsigned short num;		/* number of alternatives, or symbol number */
    unsigned short len;		/* length of rule, or offset in grammar */
    union {
	string *rgx;		/* regular expression */
	rulesym *syms;		/* linked list of rule elements */
    } u;
    string *func;		/* optional LPC function */
    struct _rule_ *alt, **last;	/* first and last in alternatives list */
    struct _rule_ *next;	/* next in linked list */
} rule;

# define RSCHUNKSZ	64
# define RLCHUNKSZ	32

typedef struct _rschunk_ {
    int chunksz;		/* current chunk size */
    struct _rschunk_ *next;	/* next in list */
    rulesym rs[RSCHUNKSZ];	/* rulesym chunk */
} rschunk;

typedef struct _rlchunk_ {
    int chunksz;		/* current chunk size */
    struct _rlchunk_ *next;	/* next in list */
    rule rl[RLCHUNKSZ];		/* rule chunk */
} rlchunk;

# define RULE_UNKNOWN	0	/* unknown rule symbol */
# define RULE_REGEXP	1	/* regular expression rule */
# define RULE_STRING	2	/* string rule */
# define RULE_PROD	3	/* production rule */

/*
 * NAME:	rulesym->new()
 * DESCRIPTION:	allocate a new rulesym
 */
static rulesym *rs_new(c, rl)
register rschunk **c;
rule *rl;
{
    register rulesym *rs;

    if (*c == (rschunk *) NULL || (*c)->chunksz == RSCHUNKSZ) {
	rschunk *x;

	x = ALLOC(rschunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }

    rs = &(*c)->rs[(*c)->chunksz++];
    rs->rule = rl;
    rs->next = (rulesym *) NULL;
    return rs;
}

/*
 * NAME:	rulesym->clear()
 * DESCRIPTION:	free all rulesyms
 */
static void rs_clear(c)
register rschunk *c;
{
    register rschunk *f;

    while (c != (rschunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }
}

/*
 * NAME:	rule->new()
 * DESCRIPTION:	allocate a new rule
 */
static rule *rl_new(c, type)
register rlchunk **c;
int type;
{
    register rule *rl;

    if (*c == (rlchunk *) NULL || (*c)->chunksz == RLCHUNKSZ) {
	rlchunk *x;

	x = ALLOC(rlchunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }
    rl = &(*c)->rl[(*c)->chunksz++];
    rl->symb = (string *) NULL;
    rl->type = type;
    rl->num = 0;
    rl->len = 0;
    rl->u.syms = (rulesym *) NULL;
    rl->func = (string *) NULL;
    rl->alt = rl->next = (rule *) NULL;
    rl->last = &rl->alt;

    return rl;
}

/*
 * NAME:	rule->clear()
 * DESCRIPTION:	free all rules
 */
static void rl_clear(c)
register rlchunk *c;
{
    register rlchunk *f;
    register rule *rl;
    register int i;

    while (c != (rlchunk *) NULL) {
	for (rl = c->rl, i = c->chunksz; i != 0; rl++, --i) {
	    if (rl->symb != (string *) NULL) {
		str_del(rl->symb);
	    }
	    if (rl->type == RULE_REGEXP && rl->u.rgx != (string *) NULL) {
		str_del(rl->u.rgx);
	    }
	    if (rl->func != (string *) NULL) {
		str_del(rl->func);
	    }
	}
	f = c;
	c = c->next;
	FREE(f);
    }
}

/*
 * Internal grammar string description:
 *
 * header	[1]	version number
 *		[x][y]	whitespace rule or -1
 *		[x][y]	nomatch rule or -1
 *		[x][y]	# regexp rules
 *		[x][y]	# total regexp rules (+ alternatives)
 *		[x][y]	# string rules
 *		[x][y]	# production rules (first is starting rule)
 *		[x][y]	# total production rules (+ alternatives)
 *
 * rgx offset	[x][y]	regexp rule offsets
 *		...
 *
 * str offset	[x][y]	string rule offsets
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
 *			[x][y] 	token or rule ...	}
 *			[...]	optional: function name	}
 */

/*
 * NAME:	make_grammar()
 * DESCRIPTION:	create a pre-processed grammar string
 */
static string *make_grammar(rgxlist, strlist, prodlist, nrgx, nstr, nprod, size)
rule *rgxlist, *strlist, *prodlist;
int nrgx, nstr, nprod;
long size;
{
    int start, prod1;
    string *gram;
    register char *p, *q;
    register rule *rl, *r;
    register rulesym *rs;
    register int n;

    gram = str_new((char *) NULL, size);

    /* header */
    p = gram->text;
    *p++ = GRAM_VERSION;	/* version number */
    STORE2(p, -1); p += 2;	/* whitespace rule */
    STORE2(p, -1); p += 2;	/* nomatch rule */
    STORE2(p, nrgx); p += 4;	/* # regular expression rules */
    STORE2(p, nstr); p += 2;	/* # string rules */
    nprod++;			/* +1 for start rule */
    STORE2(p, nprod);		/* # production rules */
    n = nrgx + nstr + nprod;
    prod1 = nrgx + nstr + 1;
    q = p + 4 + (n << 1);
    p = gram->text + size;

    /* determine production rule offsets */
    for (rl = prodlist; rl != (rule *) NULL; rl = rl->next) {
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
    for (rl = strlist; rl != (rule *) NULL; rl = rl->next) {
	size -= rl->symb->len + 1;
	p -= rl->symb->len + 1;
	q -= 2; STORE2(q, size);
	rl->num = --n;
	*p = rl->symb->len;
	memcpy(p + 1, rl->symb->text, rl->symb->len);
    }

    /* deal with regexps */
    nrgx = 0;
    for (rl = rgxlist; rl != (rule *) NULL; rl = rl->next) {
	size -= rl->num + rl->len + 2;
	q -= 2; STORE2(q, size);
	p = gram->text + size;
	STORE2(p, rl->num);
	rl->num = --n;
	p += 2;
	for (r = rl; r != (rule *) NULL; r = r->alt) {
	    if (r->u.rgx != (string *) NULL) {
		*p++ = r->u.rgx->len;
		memcpy(p, r->u.rgx->text, r->u.rgx->len);
		p += r->u.rgx->len;
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
    for (rl = prodlist; rl != (rule *) NULL; rl = rl->next) {
	q = gram->text + rl->len + 2;
	for (r = rl; r != (rule *) NULL; r = r->alt) {
	    p = q + 2;
	    n = 0;
	    for (rs = r->u.syms; rs != (rulesym *) NULL; rs = rs->next) {
		STORE2(p, rs->rule->num); p += 2;
		n++;
	    }
	    if (r->func != (string *) NULL) {
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

    p = gram->text + 13;
    STORE2(p, nprod);

    return gram;
}

/*
 * NAME:	parse_grammar()
 * DESCRIPTION:	check the grammar, return a pre-processed version
 */
string *parse_grammar(gram)
string *gram;
{
    char buffer[STRINGSZ];
    hashtab *ruletab, *strtab;
    rschunk *rschunks;
    rlchunk *rlchunks;
    rule *rgxlist, *strlist, *prodlist, *tmplist, *rr, *rrl;
    int token, ruleno, nrgx, nstr, nprod;
    ssizet glen;
    unsigned int buflen;
    bool nomatch;
    register rulesym **rs;
    register rule *rl, **r;
    register long size;
    register unsigned int len;

# if MAX_STRLEN > 0xffffffL
    if (gram->len > 0xffffffL) {
	error("Grammar string too large");
    }
# endif

    /* initialize */
    ruletab = ht_new(PARSERULTABSZ, PARSERULHASHSZ, FALSE);
    strtab = ht_new(PARSERULTABSZ, PARSERULHASHSZ, FALSE);
    rschunks = (rschunk *) NULL;
    rlchunks = (rlchunk *) NULL;
    rgxlist = strlist = prodlist = tmplist = (rule *) NULL;
    nrgx = nstr = nprod = 0;
    size = 15 + 8;	/* size of header + start rule */
    glen = gram->len;
    nomatch = FALSE;

    token = gramtok(gram, &glen, buffer, &buflen);
    for (ruleno = 1; ; ruleno++) {
	switch (token) {
	case TOK_TOKSYM:
	    /*
	     * token rule definition
	     */
	    r = (rule **) ht_lookup(ruletab, buffer, TRUE);
	    if (*r != (rule *) NULL) {
		if ((*r)->type == RULE_UNKNOWN) {
		    /* replace unknown rule */
		    rl = *r;
		    rl->type = RULE_REGEXP;
		    size += 4;
		    nrgx++;

		    if (rl->alt != (rule *) NULL) {
			rl->alt->next = rl->next;
		    } else {
			tmplist = rl->next;
		    }
		    if (rl->next != (rule *) NULL) {
			rl->next->alt = rl->alt;
		    }
		    rl->alt = (rule *) NULL;
		    rl->next = rgxlist;
		    rgxlist = rl;
		} else if ((*r)->type == RULE_REGEXP) {
		    /* new alternative regexp */
		    rl = rl_new(&rlchunks, RULE_REGEXP);

		    *((*r)->last) = rl;
		    (*r)->last = &rl->alt;
		} else {
		    sprintf(buffer,
			    "Rule %d previously defined as production rule",
			    ruleno);
		    goto err;
		}
	    } else {
		/* new rule */
		rl = rl_new(&rlchunks, RULE_REGEXP);
		str_ref(rl->symb = str_new(buffer, (long) buflen));
		rl->chain.name = rl->symb->text;
		rl->chain.next = (hte *) *r;
		*r = rl;
		size += 4;
		nrgx++;

		rl->next = rgxlist;
		rgxlist = rl;
	    }

	    switch (gramtok(gram, &glen, buffer, &buflen)) {
	    case TOK_REGEXP:
		str_ref(rl->u.rgx = str_new(buffer, (long) buflen));
		(*r)->num++;
		(*r)->len += buflen;
		size += buflen + 1;
		break;

	    case TOK_BADREGEXP:
		sprintf(buffer, "Rule %d: malformed regular expression",
			ruleno);
		goto err;

	    case TOK_TOOBIGRGX:
		sprintf(buffer, "Rule %d: regular expression too large",
			ruleno);
		goto err;

	    case TOK_SYMBOL:
		if (buflen == 7 && strcmp(buffer, "nomatch") == 0) {
		    if (nomatch) {
			sprintf(buffer, "Rule %d: extra nomatch rule", ruleno);
			goto err;
		    }
		    nomatch = TRUE;
		    rl->u.rgx = (string *) NULL;
		    break;
		}
		/* fall through */
	    default:
		sprintf(buffer, "Rule %d: regular expression expected", ruleno);
		goto err;
	    }

	    /* next token */
	    token = gramtok(gram, &glen, buffer, &buflen);
	    break;

	case TOK_PRODSYM:
	    /*
	     * production rule definition
	     */
	    r = (rule **) ht_lookup(ruletab, buffer, TRUE);
	    if (*r != (rule *) NULL) {
		if ((*r)->type == RULE_UNKNOWN) {
		    /* replace unknown rule */
		    rl = *r;
		    rl->type = RULE_PROD;
		    size += 4;
		    nprod++;

		    if (rl->alt != (rule *) NULL) {
			rl->alt->next = rl->next;
		    } else {
			tmplist = rl->next;
		    }
		    if (rl->next != (rule *) NULL) {
			rl->next->alt = rl->alt;
		    }
		    rl->alt = (rule *) NULL;
		    rl->next = prodlist;
		    prodlist = rl;
		} else if ((*r)->type == RULE_PROD) {
		    /* new alternative production */
		    rl = rl_new(&rlchunks, RULE_PROD);

		    *((*r)->last) = rl;
		    (*r)->last = &rl->alt;
		} else {
		    sprintf(buffer, "Rule %d previously defined as token rule",
			    ruleno);
		    goto err;
		}
	    } else {
		/* new rule */
		rl = rl_new(&rlchunks, RULE_PROD);
		str_ref(rl->symb = str_new(buffer, (long) buflen));
		rl->chain.name = rl->symb->text;
		rl->chain.next = (hte *) *r;
		*r = rl;
		size += 4;
		nprod++;

		rl->next = prodlist;
		prodlist = rl;
	    }

	    rr = *r;
	    rrl = rl;
	    rs = &rl->u.syms;
	    len = 0;
	    for (;;) {
		switch (token = gramtok(gram, &glen, buffer, &buflen)) {
		case TOK_SYMBOL:
		    /*
		     * symbol
		     */
		    r = (rule **) ht_lookup(ruletab, buffer, TRUE);
		    if (*r == (rule *) NULL) {
			/* new unknown rule */
			rl = rl_new(&rlchunks, RULE_UNKNOWN);
			str_ref(rl->symb = str_new(buffer, (long) buflen));
			rl->chain.name = rl->symb->text;
			rl->chain.next = (hte *) *r;
			*r = rl;

			rl->next = tmplist;
			if (tmplist != (rule *) NULL) {
			    tmplist->alt = rl;
			}
			tmplist = rl;
		    } else {
			/* previously known rule */
			rl = *r;
		    }
		    *rs = rs_new(&rschunks, rl);
		    rs = &(*rs)->next;
		    len += 2;
		    continue;

		case TOK_STRING:
		    /*
		     * string
		     */
		    r = (rule **) ht_lookup(strtab, buffer, FALSE);
		    while (*r != (rule *) NULL) {
			if ((*r)->symb->len == buflen &&
			    memcmp((*r)->symb->text, buffer, buflen) == 0) {
			    break;
			}
			r = (rule **) &(*r)->chain.next;
		    }
		    if (*r == (rule *) NULL) {
			/* new string rule */
			rl = rl_new(&rlchunks, RULE_STRING);
			str_ref(rl->symb = str_new(buffer, (long) buflen));
			rl->chain.name = rl->symb->text;
			rl->chain.next = (hte *) *r;
			*r = rl;
			size += 3 + buflen;
			nstr++;

			rl->next = strlist;
			strlist = rl;
		    } else {
			/* existing string rule */
			rl = *r;
		    }
		    *rs = rs_new(&rschunks, rl);
		    rs = &(*rs)->next;
		    len += 2;
		    continue;

		case TOK_QUEST:
		    /*
		     * ? function
		     */
		    if (gramtok(gram, &glen, buffer, &buflen) != TOK_SYMBOL) {
			sprintf(buffer, "Rule %d: function name expected",
				ruleno);
			goto err;
		    }
		    str_ref(rrl->func = str_new(buffer, (long) buflen));
		    len += buflen + 1;

		    token = gramtok(gram, &glen, buffer, &buflen);
		    /* fall through */
		default:
		    break;
		}
		break;
	    }

	    if (len > 255) {
		sprintf(buffer, "Rule %d is too long", ruleno);
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
	    if (tmplist != (rule *) NULL) {
		sprintf(buffer, "Undefined symbol %s", tmplist->symb->text);
		goto err;
	    }
	    if (rgxlist == (rule *) NULL) {
		strcpy(buffer, "No tokens");
		goto err;
	    }
	    if (prodlist == (rule *) NULL) {
		strcpy(buffer, "No starting rule");
		goto err;
	    }
	    if (size > (long) USHRT_MAX) {
		strcpy(buffer, "Grammar too large");
		goto err;
	    }
	    gram = make_grammar(rgxlist, strlist, prodlist, nrgx, nstr, nprod,
				size);
	    rs_clear(rschunks);
	    rl_clear(rlchunks);
	    ht_del(strtab);
	    ht_del(ruletab);
	    return gram;

	case TOK_ERROR:
	    sprintf(buffer, "Rule %d: bad token", ruleno);
	    goto err;

	case TOK_BADREGEXP:
	    sprintf(buffer, "Rule %d: malformed regular expression", ruleno);
	    goto err;

	case TOK_TOOBIGRGX:
	    sprintf(buffer, "Rule %d: regular expression too large", ruleno);
	    goto err;

	case TOK_BADSTRING:
	    sprintf(buffer, "Rule %d: malformed string constant", ruleno);
	    goto err;

	case TOK_TOOBIGSTR:
	    sprintf(buffer, "Rule %d: string too long", ruleno);
	    goto err;

	case TOK_TOOBIGSYM:
	    sprintf(buffer, "Rule %d: symbol too long", ruleno);
	    goto err;

	default:
	    sprintf(buffer, "Rule %d: unexpected token", ruleno);
	    goto err;
	}
    }

err:
    rs_clear(rschunks);
    rl_clear(rlchunks);
    ht_del(strtab);
    ht_del(ruletab);
    error(buffer);
}
