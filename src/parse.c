# define INCLUDE_CTYPE
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "parse.h"

# define STORE2(p, n)	((p)[0] = (n) >> 8, (p)[1] = (n))

# define PROD_LPAREN	253
# define PROD_RPAREN	254
# define PROD_QUEST	255


# define TOK_NULL	0	/* nothing */
# define TOK_REGEXP	1	/* regular expression */
# define TOK_STRING	2	/* string */
# define TOK_PRODSYM	3	/* left hand of production rule */
# define TOK_TOKSYM	4	/* left hand of token rule */
# define TOK_SYMBOL	5	/* symbol in rhs of production rule */
# define TOK_LPAREN	6	/* left parenthesis */
# define TOK_RPAREN	7	/* right parenthesis */
# define TOK_QUEST	8	/* question mark */
# define TOK_ERROR	9	/* bad token */
# define TOK_BADREGEXP 10	/* malformed regular expression */
# define TOK_TOOBIGRGX 11	/* too big regular expression */
# define TOK_BADSTRING 12	/* malformed string constant */
# define TOK_TOOBIGSTR 13	/* string constant too long */
# define TOK_TOOBIGSYM 14	/* symbol too long */

typedef struct {
    unsigned short type;	/* node type */
    unsigned short left;	/* left child node or other info */
    unsigned short right;	/* right child node or other info */
    unsigned short len;		/* length */
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
	    buffer[node[last].len] = len;
	    last = (short) node[last].left;
	}

	switch (node[thisnode].type) {
	case RGX_CHAR:
	    /*
	     * x_
	     *  ->
	     */
	    memcpy(buffer + len, str + node[thisnode].left, node[thisnode].len);
	    len += node[thisnode].len;
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
		    buffer[node[last].len] = n;
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
unsigned int *strlen, *buflen;
register char *buffer;
{
    rgxnode node[2 * STRINGSZ];
    short nstack[STRINGSZ];
    int paren, thisnode, topnode, lastnode;
    register char *p;
    char *q;
    register unsigned int size, len, n;

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

	case '(':
	    *strlen = size;
	    return TOK_LPAREN;

	case ')':
	    *strlen = size;
	    return TOK_RPAREN;

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
		    node[thisnode].type = RGX_CHAR;
		    node[thisnode].left = p - str->text;
		    node[thisnode].right = -1;

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
		    }
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
		buffer[node[thisnode].len] = len - 1;
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
    short type;			/* symbol, (, ) */
    struct _rule_ *rule;	/* symbol */
    struct _rulesym_ *next;	/* next in rule */
} rulesym;

typedef struct _rule_ {
    hte chain;			/* hash table chain */
    string *symb;		/* rule symbol */
    short type;			/* unknown, token or production rule */
    unsigned short num;		/* various things */
    unsigned short len;		/* length of rule */
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
    rulesym rs[RSCHUNKSZ];	/* rulesym chunk */
    int chunksz;		/* current chunk size */
    struct _rschunk_ *next;	/* next in list */
} rschunk;

typedef struct _rlchunk_ {
    rule rl[RLCHUNKSZ];		/* rule chunk */
    int chunksz;		/* current chunk size */
    struct _rlchunk_ *next;	/* next in list */
} rlchunk;

# define RS_SYMBOL	0	/* symbol in rule */
# define RS_LPAREN	1	/* left parenthesis */
# define RS_RPAREN	2	/* right parenthesis */

# define RULE_UNKNOWN	0	/* unknown rule symbol */
# define RULE_REGEXP	1	/* regular expression rule */
# define RULE_STRING	2	/* string rule */
# define RULE_PROD	3	/* production rule */

/*
 * NAME:	rulesym->new()
 * DESCRIPTION:	allocate a new rulesym
 */
static rulesym *rs_new(c, type, rl)
register rschunk **c;
int type;
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
    rs->type = type;
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
 * header	[0]	version number
 *		[x]	0: no whitespace, 1: first token rule is whitespace
 *		[x][y]	# regexp rules
 *		[x][y]	# total regexp rules (+ alternatives)
 *		[x][y]	# string rules
 *		[x][y]	# production rules (first is starting rule)
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
 *		[x]	length of rule			}
 *		[...]	rule:				}
 *			[253]		(		} ...
 *			[254]		)		}
 *			[255][...]	function name	}
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
    string *gram;
    register char *p, *q;
    register rule *rl, *r;
    register rulesym *rs;

    gram = str_new((char *) NULL, size);

    /* header */
    p = gram->text;
    STORE2(p, 0); p += 2;	/* version number & whitespace */
    STORE2(p, nrgx); p += 4;	/* # regular expression rules */
    STORE2(p, nstr); p += 2;	/* # string rules */
    STORE2(p, nprod);		/* # production rules */
    q = p + 2 + 2 * (nrgx + nstr + nprod);
    p = gram->text + size;

    /* determine production rule offsets */
    for (rl = prodlist; rl != (rule *) NULL; rl = rl->next) {
	size -= rl->num + rl->len + 2;
	p -= rl->num + rl->len + 2;
	q -= 2; STORE2(q, size);
	STORE2(p, rl->num);
	rl->num = size;
    }

    /* deal with strings */
    for (rl = strlist; rl != (rule *) NULL; rl = rl->next) {
	size -= rl->symb->len + 1;
	p -= rl->symb->len + 1;
	q -= 2; STORE2(q, size);
	rl->num = size;
	*p = rl->symb->len;
	memcpy(p + 1, rl->symb->text, rl->symb->len);
    }

    /* deal with regexps */
    nrgx = 0;
    for (rl = rgxlist; rl != (rule *) NULL; rl = rl->next) {
	size -= rl->num + rl->len + 2;
	if (strcmp(rl->symb->text, "whitespace") == 0) {
	    gram->text[1] = 1;
	    p = gram->text + 10;
	    STORE2(p, size);
	} else {
	    q -= 2; STORE2(q, size);
	}
	p = gram->text + size;
	STORE2(p, rl->num); p += 2;
	rl->num = size;
	for (r = rl; r != (rule *) NULL; r = r->alt) {
	    *p++ = r->u.rgx->len;
	    memcpy(p, r->u.rgx->text, r->u.rgx->len);
	    p += r->u.rgx->len;
	    nrgx++;
	}
    }
    p = gram->text + 4;
    STORE2(p, nrgx);

    /* fill in production rules */
    for (rl = prodlist; rl != (rule *) NULL; rl = rl->next) {
	q = gram->text + rl->num + 2;
	for (r = rl; r != (rule *) NULL; r = r->alt) {
	    p = q + 1;
	    for (rs = r->u.syms; rs != (rulesym *) NULL; rs = rs->next) {
		switch (rs->type) {
		case RS_SYMBOL:
		    STORE2(p, rs->rule->num); p += 2;
		    break;

		case RS_LPAREN:
		    *p++ = PROD_LPAREN;
		    break;

		case RS_RPAREN:
		    *p++ = PROD_RPAREN;
		    break;
		}
	    }
	    if (r->func != (string *) NULL) {
		*p++ = PROD_QUEST;
		memcpy(p, r->func->text, r->func->len + 1);
		p += r->func->len + 1;
	    }
	    *q = p - q - 1;
	    q = p;
	}
    }

    return gram;
}

/*
 * NAME:	parse_grammar()
 * DESCRIPTION:	check the grammar, return a pre-processed version
 */
static string *parse_grammar(gram)
string *gram;
{
    char buffer[STRINGSZ];
    hashtab *ruletab, *strtab;
    rschunk *rschunks;
    rlchunk *rlchunks;
    rule *rgxlist, *strlist, *prodlist, *tmplist, *rr, *rrl;
    int token, ruleno, nrgx, nstr, nprod;
    unsigned int buflen, glen;
    register rulesym **rs;
    register rule *rl, **r;
    register long size;
    register unsigned int len, paren;

    /* initialize */
    ruletab = ht_new(PARSERULTABSZ, PARSERULHASHSZ);
    strtab = ht_new(PARSERULTABSZ, PARSERULHASHSZ);
    rschunks = (rschunk *) NULL;
    rlchunks = (rlchunk *) NULL;
    rgxlist = strlist = prodlist = tmplist = (rule *) NULL;
    nrgx = nstr = nprod = 0;
    size = 10;	/* size of header */
    glen = gram->len;

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
		break;

	    case TOK_BADREGEXP:
		sprintf(buffer, "Rule %d: malformed regular expression",
			ruleno);
		goto err;

	    case TOK_TOOBIGRGX:
		sprintf(buffer, "Rule %d: regular expression too large",
			ruleno);
		goto err;

	    default:
		sprintf(buffer, "Rule %d: regular expression expected", ruleno);
		goto err;
	    }
	    str_ref(rl->u.rgx = str_new(buffer, (long) buflen));
	    (*r)->num++;
	    (*r)->len += buflen;
	    size += buflen + 1;

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
	    paren = 0;
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
		    *rs = rs_new(&rschunks, RS_SYMBOL, rl);
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
		    *rs = rs_new(&rschunks, RS_SYMBOL, rl);
		    rs = &(*rs)->next;
		    len += 2;
		    continue;

		case TOK_LPAREN:
		    /*
		     * left parenthesis
		     */
		    paren++;
		    *rs = rs_new(&rschunks, RS_LPAREN, (rule *) NULL);
		    rs = &(*rs)->next;
		    len++;
		    continue;

		case TOK_RPAREN:
		    /*
		     * right parenthesis
		     */
		    if (paren == 0) {
			sprintf(buffer, "Rule %d: unbalanced parenthesis",
				ruleno);
			goto err;
		    }
		    --paren;
		    *rs = rs_new(&rschunks, RS_RPAREN, (rule *) NULL);
		    rs = &(*rs)->next;
		    len++;
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
		    len += buflen + 2;

		    token = gramtok(gram, &glen, buffer, &buflen);
		    /* fall through */
		default:
		    break;
		}
		break;
	    }

	    if (paren != 0) {
		sprintf(buffer, "Rule %d: unbalanced parenthesis", ruleno);
		goto err;
	    }
	    if (len > 255) {
		sprintf(buffer, "Rule %d is too long", ruleno);
		goto err;
	    }
	    rr->num++;
	    rr->len += len;
	    size += len + 1;
	    break;

	case TOK_NULL:
	    /*
	     * end of grammar
	     */
	    if (tmplist != (rule *) NULL) {
		sprintf(buffer, "Undefined symbol %s", tmplist->symb->text);
		goto err;
	    }
	    if (prodlist == (rule *) NULL) {
		strcpy(buffer, "No starting rule");
		goto err;
	    }
	    if (size > (long) USHRT_MAX) {
		strcpy(buffer, "Grammar too long");
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
	    sprintf(buffer, "Rule %d: string too large", ruleno);
	    goto err;

	case TOK_TOOBIGSYM:
	    sprintf(buffer, "Rule %d: symbol too large", ruleno);
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


/*
 * NAME:	charset->neg()
 * DESCRIPTION:	negate a charset
 */
static void cs_neg(cs)
register Uint *cs;
{
    cs[0] ^= 0xffffffffL;
    cs[1] ^= 0xffffffffL;
    cs[2] ^= 0xffffffffL;
    cs[3] ^= 0xffffffffL;
    cs[4] ^= 0xffffffffL;
    cs[5] ^= 0xffffffffL;
    cs[6] ^= 0xffffffffL;
    cs[7] ^= 0xffffffffL;
}

/*
 * NAME:	charset->and()
 * DESCRIPTION:	and two charsets
 */
static void cs_and(cs1, cs2)
register Uint *cs1, *cs2;
{
    cs1[0] &= cs2[0];
    cs1[1] &= cs2[1];
    cs1[2] &= cs2[2];
    cs1[3] &= cs2[3];
    cs1[4] &= cs2[4];
    cs1[5] &= cs2[5];
    cs1[6] &= cs2[6];
    cs1[7] &= cs2[7];
}

/*
 * NAME:	charset->or()
 * DESCRIPTION:	or two charsets
 */
static void cs_or(cs1, cs2)
register Uint *cs1, *cs2;
{
    cs1[0] |= cs2[0];
    cs1[1] |= cs2[1];
    cs1[2] |= cs2[2];
    cs1[3] |= cs2[3];
    cs1[4] |= cs2[4];
    cs1[5] |= cs2[5];
    cs1[6] |= cs2[6];
    cs1[7] |= cs2[7];
}

/*
 * NAME:	charset->sub()
 * DESCRIPTION:	subtract a charset from another one
 */
static void cs_sub(cs1, cs2)
register Uint *cs1, *cs2;
{
    cs1[0] &= ~cs2[0];
    cs1[1] &= ~cs2[1];
    cs1[2] &= ~cs2[2];
    cs1[3] &= ~cs2[3];
    cs1[4] &= ~cs2[4];
    cs1[5] &= ~cs2[5];
    cs1[6] &= ~cs2[6];
    cs1[7] &= ~cs2[7];
}

/*
 * NAME:	charset->intersect()
 * DESCRIPTION:	return TRUE if two character sets intersect, FALSE otherwise
 */
static bool cs_intersect(cs1, cs2)
register Uint *cs1, *cs2;
{
    register Uint i;

    i  = cs1[0] & cs2[0];
    i |= cs1[1] & cs2[1];
    i |= cs1[2] & cs2[2];
    i |= cs1[3] & cs2[3];
    i |= cs1[4] & cs2[4];
    i |= cs1[5] & cs2[5];
    i |= cs1[6] & cs2[6];
    i |= cs1[7] & cs2[7];

    return (i != 0);
}

/*
 * NAME:	charset->overlap()
 * DESCRIPTION:	Check if two character sets overlap.  Return TRUE if they do,
 *		or if the first set contains the second one.
 */
static bool cs_overlap(cs1, cs2, cs3, cs4)
register Uint *cs1, *cs2, *cs3, *cs4;
{
    register Uint s3, s4;

    s3  = cs3[0] = cs1[0] & cs2[0];	s4  = cs4[0] = cs1[0] & ~cs3[0];
    s3 |= cs3[1] = cs1[1] & cs2[1];	s4 |= cs4[1] = cs1[1] & ~cs3[1];
    s3 |= cs3[2] = cs1[2] & cs2[2];	s4 |= cs4[2] = cs1[2] & ~cs3[2];
    s3 |= cs3[3] = cs1[3] & cs2[3];	s4 |= cs4[3] = cs1[3] & ~cs3[3];
    s3 |= cs3[4] = cs1[4] & cs2[4];	s4 |= cs4[4] = cs1[4] & ~cs3[4];
    s3 |= cs3[5] = cs1[5] & cs2[5];	s4 |= cs4[5] = cs1[5] & ~cs3[5];
    s3 |= cs3[6] = cs1[6] & cs2[6];	s4 |= cs4[6] = cs1[6] & ~cs3[6];
    s3 |= cs3[7] = cs1[7] & cs2[7];	s4 |= cs4[7] = cs1[7] & ~cs3[7];

    return (s3 != 0 && s4 != 0);
}

/*
 * NAME:	charset->firstc()
 * DESCRIPTION:	find the first char in a charset
 */
static int cs_firstc(cset, c)
register Uint *cset;
register int c;
{
    register Uint x;

    while (c < 256) {
	if ((x=cset[c >> 5] >> (c & 31)) != 0) {
	    while ((x & 0xff) == 0) {
		x >>= 8;
		c += 8;
	    }
	    while ((x & 1) == 0) {
		x >>= 1;
		c++;
	    }
	    return c;
	}
	c += 32;
	c &= ~31;
    }

    /* not found */
    return -1;
}

/*
 * NAME:	charset->eclass()
 * DESCRIPTION:	convert a charset into an equivalence class
 */
static int cs_eclass(cset, eclass, class)
Uint *cset;
char *eclass;
int class;
{
    register int n, c;
    register Uint x;

    n = 0;
    for (c = cs_firstc(cset, 0); c < 256; c += 31, c &= ~31) {
	x = cset[c >> 5] >> (c & 31);
	if (x != 0) {
	    do {
		while ((x & 0xff) == 0) {
		    x >>= 8;
		    c += 8;
		}
		if (x & 1) {
		    eclass[c] = class;
		    n++;
		}
		x >>= 1;
		c++;
	    } while (x != 0);
	} else {
	    c++;
	}
    }

    return n;
}


typedef struct {
    hte chain;			/* hash table chain */
    char *rgx;			/* regular expression this position is in */
    short size;			/* size of position (length of string - 2) */
    short nposn;		/* position number */
    short ruleno;		/* the rule this position is in */
    bool final;			/* final position? */
    bool alloc;			/* position allocated separately? */
} lexposn;

# define LPCHUNKSZ	32

typedef struct _lpchunk_ {
    lexposn lp[LPCHUNKSZ];	/* lexposn chunk */
    int chunksz;		/* size of chunk */
    struct _lpchunk_ *next;	/* next in linked list */
} lpchunk;

/*
 * NAME:	lexposn->alloc()
 * DESCRIPTION:	allocate a new lexposn (or return an old one)
 */
static lexposn *lp_alloc(htab, posn, size, c, rgx, nposn, ruleno, final)
hashtab *htab;
char *posn, *rgx;
int size, nposn, ruleno;
lpchunk **c;
bool final;
{
    register lexposn **llp, *lp;

    llp = (lexposn **) ht_lookup(htab, posn, TRUE);
    if (*llp != (lexposn *) NULL) {
	return *llp;	/* already exists */
    }

    if (*c == (lpchunk *) NULL || (*c)->chunksz == LPCHUNKSZ) {
	lpchunk *x;

	x = ALLOC(lpchunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }
    lp = &(*c)->lp[(*c)->chunksz++];
    lp->chain.next = (hte *) *llp;
    *llp = lp;
    lp->chain.name = posn;
    lp->rgx = rgx;
    lp->size = size;
    lp->nposn = nposn;
    lp->ruleno = ruleno;
    lp->final = final;
    lp->alloc = FALSE;

    return lp;
}

/*
 * NAME:	lexposn->new()
 * DESCRIPTION:	create a new lexposn
 */
static lexposn *lp_new(htab, posn, size, c, rgx, nposn, ruleno, final)
hashtab *htab;
char *posn, *rgx;
int size, nposn, ruleno;
lpchunk **c;
bool final;
{
    register lexposn *lp;

    lp = lp_alloc(htab, posn, size, c, rgx, nposn, ruleno, final);
    strcpy(lp->chain.name = ALLOC(char, size + 3), posn);
    lp->alloc = TRUE;
    return lp;
}

/*
 * NAME:	lexposn->load()
 * DESCRIPTION:	load a lexposn from a buffer
 */
static lexposn *lp_load(htab, c, nposn, buf, grammar)
hashtab *htab;
lpchunk **c;
int nposn;
register char *buf;
char *grammar;
{
    char *rgx;
    int ruleno, size;
    bool final;

    rgx = grammar + UCHAR(buf[0]) * 256 + UCHAR(buf[1]);
    ruleno = UCHAR(buf[2]) * 256 + UCHAR(buf[3]);
    buf += 4;
    if (*buf == '\0') {
	final = TRUE;
	buf++;
    } else {
	final = FALSE;
    }
    size = UCHAR(*buf++);

    return lp_alloc(htab, buf, size, c, rgx, nposn, ruleno, final);
}

/*
 * NAME:	lexposn->save()
 * DESCRIPTION:	save a lexposn to a buffer
 */
static char *lp_save(lp, buf, grammar)
register lexposn *lp;
register char *buf;
char *grammar;
{
    unsigned int rgx;

    rgx = lp->rgx - grammar;
    buf[0] = rgx / 256;
    buf[1] = rgx;
    buf[2] = lp->ruleno / 256;
    buf[3] = lp->ruleno;
    buf += 4;
    if (lp->final) {
	*buf++ = '\0';
    }
    *buf++ = lp->size;
    memcpy(buf, lp->chain.name, lp->size + 1);
    return buf + lp->size + 1;
}

/*
 * NAME:	lexposn->clear()
 * DESCRIPTION:	free all lexposns
 */
static void lp_clear(c)
register lpchunk *c;
{
    register lpchunk *f;
    register lexposn *lp;
    register int i;

    while (c != (lpchunk *) NULL) {
	for (lp = c->lp, i = c->chunksz; i != 0; lp++, --i) {
	    if (lp->alloc) {
		FREE(lp->chain.name);
	    }
	}
	f = c;
	c = c->next;
	FREE(f);
    }
}

/*
 * NAME:	lexposn->transposn()
 * DESCRIPTION:	convert a transition into a position
 */
static bool lp_transposn(rgx, trans, buf, buflen)
char *rgx, *trans, *buf;
int *buflen;
{
    char a[256], b[256], c[256], heap[256];
    register char *p, *q;
    register int n, len, place;
    register unsigned int i, j;

    memset(a, '\0', 256);
    heap[0] = len = 0;

    /* from transitions to places */
    if (trans == (char *) NULL) {
	n = 1;
	b[0] = 1;
    } else {
	n = 0;
	for (p = trans; *p != '\0'; p++) {
	    place = rgx[UCHAR(*p)] + 1;
	    if (!a[place]) {
		a[place] = TRUE;
		if (place != UCHAR(rgx[0])) {
		    switch (rgx[place]) {
		    case '|':
			/* branch */
			b[n++] = place + 2;
			b[n++] = UCHAR(rgx[place + 1]) + 1;
			continue;

		    case '+':
			/* pattern+ */
			b[n++] = place + 2;
			if (place < UCHAR(*p)) {
			    b[n++] = UCHAR(rgx[place + 1]) + 1;
			}
			continue;
		    }

		    /* add to heap */
		    for (i = ++len, j = i >> 1;
			 UCHAR(heap[j]) > place;
			 i = j, j >>= 1) {
			heap[i] = heap[j];
		    }
		    heap[i] = place;
		}
	    }
	}
    }
    b[n] = '\0';

    /* closure: alternate between b and c */
    for (p = b, q = c; *p != '\0'; p = q, q = (q == b) ? c : b) {
	n = 0;
	do {
	    place = UCHAR(*p++);
	    if (!a[place]) {
		a[place] = TRUE;
		if (place != UCHAR(rgx[0])) {
		    switch (rgx[place]) {
		    case '|':
			/* branch */
			q[n++] = place + 2;
			q[n++] = UCHAR(rgx[place + 1]) + 1;
			continue;

		    case '+':
			/* pattern+ */
			q[n++] = place + 2;
			continue;
		    }

		    /* add to heap */
		    for (i = ++len, j = i >> 1;
			 UCHAR(heap[j]) > place;
			 i = j, j >>= 1) {
			heap[i] = heap[j];
		    }
		    heap[i] = place;
		}
	    }
	} while (*p != '\0');
	q[n] = '\0';
    }

    /* from heap to buf */
    *buflen = len;
    for (p = buf; len != 0; --len) {
	*p++ = heap[1];
	n = UCHAR(heap[len]);
	for (i = 1, j = 2; j < len; i = j, j <<= 1) {
	    if (UCHAR(heap[j]) > UCHAR(heap[j + 1])) {
		j++;
	    }
	    if (n <= UCHAR(heap[j])) {
		break;
	    }
	    heap[i] = heap[j];
	}
	heap[i] = n;
    }
    *p = '\0';

    return a[UCHAR(rgx[0])];	/* final? */
}

static Uint bits[] = {
    0x00000001L, 0x00000003L, 0x00000007L, 0x0000000fL,
    0x0000001fL, 0x0000003fL, 0x0000007fL, 0x000000ffL,
    0x000001ffL, 0x000003ffL, 0x000007ffL, 0x00000fffL,
    0x00001fffL, 0x00003fffL, 0x00007fffL, 0x0000ffffL,
    0x0001ffffL, 0x0003ffffL, 0x0007ffffL, 0x000fffffL,
    0x001fffffL, 0x003fffffL, 0x007fffffL, 0x00ffffffL,
    0x01ffffffL, 0x03ffffffL, 0x07ffffffL, 0x0fffffffL,
    0x1fffffffL, 0x3fffffffL, 0x7fffffffL, 0xffffffffL
};

/*
 * NAME:	lexposn->cset()
 * DESCRIPTION:	create an input set for a position
 */
static void lp_cset(lp, cset)
lexposn *lp;
register Uint *cset;
{
    register char *p, *q;
    register int c, n, x;
    bool negate;

    for (q = lp->chain.name + 2; *q != '\0'; q++) {
	memset(cset, '\0', 32);
	p = lp->rgx + UCHAR(*q);
	switch (*p) {
	case '[':
	    /* character class */
	    p++;
	    if (*p == '^') {
		negate = TRUE;
		p++;
	    } else {
		negate = FALSE;
	    }
	    do {
		if (*p == '\\') {
		    p++;
		}
		c = UCHAR(*p++);
		cset[c >> 5] |= 1 << (c & 31);
		if (p[0] == '-' && p[1] != ']') {
		    n = p[1] - c;
		    if (n != 0) {
			x = 32 - (++c & 31);
			if (x > n) {
			    x = n;
			}
			cset[c >> 5] |= bits[x - 1] << (c & 31);
			c += x;
			n -= x;
			while (n >= 32) {
			    cset[c >> 5] |= 0xffffffffL;
			    c += 32;
			    n -= 32;
			}
			if (n != 0) {
			    cset[c >> 5] |= bits[n - 1];
			}
		    }
		    p += 2;
		}
	    } while (*p != ']');
	    if (negate) {
		cs_neg(cset);
	    }
	    break;

	case '.':
	    /* anything */
	    memset(cset, -1, 32);
	    break;

	case '\\':
	    /* escaped char */
	    p++;
	default:
	    /* normal char */
	    c = UCHAR(*p);
	    cset[c >> 5] |= 1 << (c & 31);
	    break;
	}

	cset += 8;
    }
}

/*
 * NAME:	lexposn->trans()
 * DESCRIPTION:	perform a transition on a position, given an input set
 */
static bool lp_trans(lp, cset, posn, size)
lexposn *lp;
Uint *cset;
char *posn;
int *size;
{
    char trans[256];
    register char *p, *q;
    register int c, n, x;
    char *t;
    Uint found;
    bool negate;

    t = trans;
    for (q = lp->chain.name + 2; *q != '\0'; q++) {
	p = lp->rgx + UCHAR(*q);
	found = 0;
	switch (*p) {
	case '[':
	    /* character class */
	    p++;
	    if (*p == '^') {
		negate = TRUE;
		p++;
	    } else {
		negate = FALSE;
	    }
	    do {
		if (*p == '\\') {
		    p++;
		}
		c = UCHAR(*p++);
		found |= cset[c >> 5] & 1 << (c & 31);
		if (p[0] == '-' && p[1] != ']') {
		    n = p[1] - c;
		    if (n != 0) {
			x = 32 - (++c & 31);
			if (x > n) {
			    x = n;
			}
			found |= cset[c >> 5] & (bits[x - 1] << (c & 31));
			c += x;
			n -= x;
			while (n >= 32) {
			    found |= cset[c >> 5] & 0xffffffffL;
			    c += 32;
			    n -= 32;
			}
			if (n != 0) {
			    found |= cset[c >> 5] & bits[n - 1];
			}
		    }
		    p += 2;
		}
	    } while (*p != ']');
	    if (negate) {
		found = !found;
	    }
	    break;

	case '.':
	    /* anything */
	    found = 1;
	    break;

	case '\\':
	    /* escaped char */
	    p++;
	default:
	    /* normal char */
	    c = UCHAR(*p);
	    found = cset[c >> 5] & (1 << (c & 31));
	    break;
	}
	if (found != 0) {
	    *t++ = p - lp->rgx + 1;
	}
    }
    *t = '\0';

    return lp_transposn(lp->rgx, trans, posn, size);
}


typedef struct {
    lexposn **posn;		/* regexp positions */
    short *str;			/* strings */
    char *trans;		/* transitions */
    short nposn;		/* number of positions */
    short nstr;			/* number of string positions */
    short len;			/* string length */
    short ntrans;		/* number of transitions */
    short final;		/* rule number, -1: not final */
    short next;			/* next in hash chain */
    bool alloc;			/* transitions allocated? */
} lexstate;

/*
 * NAME:	lexstate->load()
 * DESCRIPTION:	load a lexstate from a buffer
 */
static char *ls_load(state, buf, ntrans)
register lexstate *state;
register char *buf;
register int ntrans;
{
    state->posn = (lexposn **) NULL;
    state->str = (short *) NULL;
    state->nposn = state->nstr = state->len = 0;
    state->ntrans = ntrans;
    state->alloc = FALSE;
    state->final = UCHAR(buf[0]) * 256 + UCHAR(buf[1]);
    buf += 2;
    if (ntrans != 0) {
	state->trans = buf;
	buf += ntrans * 2;
    } else {
	state->trans = (char *) NULL;
    }

    return buf;
}

/*
 * NAME:	lexstate->loadtmp()
 * DESCRIPTION:	load lexstate temporary data from a buffer
 */
static char *ls_loadtmp(state, sbuf, pbuf, htab, c, nposn, grammar)
register lexstate *state;
register char *sbuf;
char *pbuf, *grammar;
hashtab *htab;
lpchunk **c;
int *nposn;
{
    register int i;
    register lexposn *lp;
    char *posn;

    state->nposn = UCHAR(sbuf[0]) * 256 + UCHAR(sbuf[1]);
    state->nstr = UCHAR(sbuf[2]) * 256 + UCHAR(sbuf[3]);
    sbuf += 4;
    state->len = UCHAR(*sbuf++);

    if (state->nposn != 0) {
	state->posn = ALLOC(lexposn*, state->nposn);
	for (i = 0; i < state->nposn; i++) {
	    posn = pbuf + UCHAR(sbuf[0]) * 256 + UCHAR(sbuf[1]);
	    sbuf += 2;
	    lp = state->posn[i] = lp_load(htab, c, *nposn, posn, grammar);
	    if (lp->nposn == *nposn) {
		*nposn++;
	    }
	}
    }
    if (state->nstr != 0) {
	state->str = ALLOC(short, state->nstr);
	for (i = 0; i < state->nstr; i++) {
	    state->str[i] = UCHAR(sbuf[0]) * 256 + UCHAR(sbuf[1]);
	    sbuf += 2;
	}
    }

    return sbuf;
}

/*
 * NAME:	lexstate->save()
 * DESCRIPTION:	save a lexstate to a buffer
 */
static char *ls_save(state, buf)
register lexstate *state;
register char *buf;
{
    buf[0] = state->final / 256;
    buf[1] = state->final;
    buf += 2;
    if (state->ntrans != 0) {
	memcpy(buf, state->trans, state->ntrans * 2);
	buf += state->ntrans * 2;
    }

    return buf;
}

/*
 * NAME:	lexstate->savetmp()
 * DESCRIPTION:	save lexstate temporary data to a buffer
 */
static char *ls_savetmp(state, sbuf, pbuf, pbase, ptab, nposn, grammar)
register lexstate *state;
char *sbuf, **pbuf, *pbase, *grammar;
short *ptab, *nposn;
{
    register char *p;
    register lexposn *lp;
    register int i;
    register short n;

    sbuf[0] = state->nposn / 256;
    sbuf[1] = state->nposn;
    sbuf[2] = state->nstr / 256;
    sbuf[3] = state->nstr;
    sbuf[4] = state->len;
    sbuf += 5;

    p = *pbuf;
    for (i = 0; i < state->nposn; i++) {
	lp = state->posn[i];
	if (lp->nposn == *nposn) {
	    ptab[(*nposn)++] = p - pbase;
	    p = lp_save(lp, p, grammar);
	}
	n = ptab[lp->nposn];
	sbuf[0] = n / 256;
	sbuf[1] = n;
	sbuf += 2;
    }
    *pbuf = p;
    for (i = 0; i < state->nstr; i++) {
	n = state->str[i];
	sbuf[0] = n / 256;
	sbuf[1] = n;
	sbuf += 2;
    }

    return sbuf;
}

/*
 * NAME:	lexstate->hash()
 * DESCRIPTION:	put a new state in the hash table, or return an old one
 */
static short ls_hash(htab, htabsize, states, idx)
short *htab;
int htabsize, idx;
lexstate *states;
{
    register unsigned long x;
    register int n;
    register lexposn **posn;
    register short *str;
    register lexstate *newstate, *ls;
    short *lls;

    /* hash on position and string pointers */
    newstate = &states[idx];
    x = newstate->len ^ newstate->final;
    for (n = newstate->nposn, posn = newstate->posn; --n >= 0; ) {
	x = (x >> 3) ^ (x << 29) ^ (unsigned long) *posn++;
    }
    for (n = newstate->nstr, str = newstate->str; --n >= 0; ) {
	x = (x >> 3) ^ (x << 29) ^ (unsigned long) *str++;
    }
    x = (Uint) x % htabsize;

    /* check state hash table */
    posn = newstate->posn;
    str = newstate->str;
    lls = &htab[x];
    ls = &states[*lls];
    while (ls != states &&
	   (newstate->len != ls->len || newstate->final != ls->final ||
	    newstate->nposn != ls->nposn || newstate->nstr != ls->nstr ||
	    memcmp(posn, ls->posn, newstate->nposn * sizeof(lexposn*)) != 0 ||
	    memcmp(str, ls->str, newstate->nstr * sizeof(short)) != 0)) {
	lls = &ls->next;
	ls = &states[*lls];
    }

    if (ls != states) {
	return *lls;	/* state already exists */
    }

    newstate->next = *lls;
    return *lls = idx;
}


typedef struct {
    char *grammar;		/* reference grammar */
    char *strings;		/* offset of strings in grammar */
    bool whitespace;		/* true if lexer token 0 is whitespace */

    Int lexsize;		/* size of state machine */
    Int tmpssize;		/* size of temporary state data */
    Int tmppsize;		/* size of temporary posn data */

    unsigned short nregexp;	/* # regexps */
    unsigned short nposn;	/* number of unique positions */
    lpchunk *lpc;		/* current lexposn chunk */
    hashtab *posnhtab;		/* position hash table */

    unsigned short nstates;	/* # states */
    unsigned short sttsize;	/* state table size */
    unsigned short sthsize;	/* size of state hash table */
    unsigned short expanded;	/* # expanded states */
    lexstate *states;		/* lexer states */
    short *sthtab;		/* state hash table */

    short ecnum;		/* number of equivalence classes */
    char eclass[256];		/* equivalence classes */
    char *ecsplit;		/* equivalence class split history */
    char *ecmembers;		/* members per equivalence class */
    Uint *ecset;		/* equivalence class sets */
} lexer;

/*
 * state & eclass format:
 *
 * header	[0]	version number
 *		[x][y]	# states
 *		[x][y]	# expanded states
 *		[x]	# equivalence classes
 * eclass	[...]	256 equivalence classes
 *
 * state 	[x][y]	final
 *		[...]	optional: transitions
 *
 *
 * temporary data format:
 *
 * header	[0]	version number
 * ecsplit	[...]	256 ecsplit data
 *
 * state	[x][y]	# positions
 *		[x][y]	# strings
 * 		[x]	len
 *		[...]   position data
 *		[...]	string data
 *
 * position	[x][y]	regexp
 *		[x][y]	ruleno
 *		[0]	optional: final position
 *		[x]	size
 *		[...]	position data
 */

/*
 * NAME:	lexer->new()
 * DESCRIPTION:	create new lexer instance
 */
static lexer *lx_new(grammar)
register char *grammar;
{
    char posn[258];
    int nstrings, size;
    register lexer *lx;
    register char *rgx;
    register lexposn **llp;
    register int i, j, n;
    bool final;

    lx = ALLOC(lexer, 1);

    /* grammar info */
    lx->grammar = grammar;
    lx->nregexp = UCHAR(grammar[2]) * 256 + UCHAR(grammar[3]);
    nstrings = UCHAR(grammar[6]) * 256 + UCHAR(grammar[7]);
    lx->strings = grammar + 10 + lx->nregexp * 2;
    lx->whitespace = grammar[1];

    /* size info */
    lx->lexsize = 6 + 256 + 2;
    lx->tmpssize = 3 + 256 + 5 + 5;
    lx->tmppsize = 0;

    /* positions */
    lx->nposn = UCHAR(grammar[4]) * 256 + UCHAR(grammar[5]);
    lx->lpc = (lpchunk *) NULL;
    lx->posnhtab = ht_new(4 * (lx->nposn + 1), 257);

    /* states */
    lx->nstates = 2;
    lx->sttsize = 2 * (lx->nposn + nstrings + 1);
    lx->sthsize = 2 * lx->sttsize;
    lx->expanded = 0;
    lx->states = ALLOC(lexstate, lx->sttsize);
    lx->sthtab = ALLOC(short, lx->sthsize);
    memset(lx->sthtab, '\0', sizeof(short) * lx->sthsize);

    /* initial states */
    lx->states[0].posn = (lexposn **) NULL;
    lx->states[0].str = (short *) NULL;
    lx->states[0].trans = (char *) NULL;
    lx->states[0].nposn = lx->states[0].nstr = 0;
    lx->states[0].ntrans = lx->states[0].len = 0;
    lx->states[0].final = -1;
    lx->states[1].posn = (lx->nposn != 0) ?
			  ALLOC(lexposn*, lx->nposn) : (lexposn **) NULL;
    lx->states[1].str = (nstrings != 0) ?
			 ALLOC(short, nstrings) : (short *) NULL;
    lx->states[1].trans = (char *) NULL;
    lx->states[1].nposn = lx->nposn;
    lx->states[1].nstr = nstrings;
    lx->states[1].ntrans = lx->states[1].len = 0;
    lx->states[1].final = -1;
    lx->states[1].alloc = FALSE;
    grammar += 10;
    /* initial positions */
    llp = lx->states[1].posn;
    for (i = j = 0; i < lx->nregexp; i++) {
	rgx = lx->grammar + UCHAR(grammar[0]) * 256 + UCHAR(grammar[1]);
	grammar += 2;
	n = j + UCHAR(rgx[0]) * 256 + UCHAR(rgx[1]);
	rgx += 2;
	while (j < n) {
	    final = lp_transposn(rgx, (char *) NULL, posn + 2, &size);
	    if (final && lx->states[1].final < 0) {
		lx->states[1].final = i;
	    }
	    posn[0] = 1 + j / 255;
	    posn[1] = 1 + j % 255;
	    *llp++ = lp_new(lx->posnhtab, posn, size, &lx->lpc, rgx, j++, i,
			    final);
	    lx->tmpssize += 2;
	    lx->tmppsize += 8 + size + final;
	    rgx += UCHAR(rgx[0]) + 1;
	}
    }
    /* initial strings */
    for (i = nstrings; --i >= 0; ) {
	lx->states[1].str[i] = i;
    }
    lx->tmpssize += nstrings * 2;
    /* add to hashtable */
    ls_hash(lx->sthtab, lx->sthsize, lx->states, 1);

    /* equivalence classes */
    lx->ecnum = 1;
    lx->ecsplit = ALLOC(char, 256 + 256 + 32 * 256);
    lx->ecmembers = lx->ecsplit + 256;
    lx->ecset = (Uint *) (lx->ecmembers + 256);
    memset(lx->eclass, '\0', 256);
    memset(lx->ecsplit, '\0', 256);
    memset(lx->ecmembers, '\0', 256);
    memset(lx->ecset, -1, 32);
    memset(lx->ecset + 8, '\0', 32 * 255);

    return lx;
}

/*
 * NAME:	lexer->load()
 * DESCRIPTION:	load lexer from strings
 */
static lexer *lx_load(grammar, s1, s2)
char *grammar;
string *s1, *s2;
{
# if 0
    char posn[258];
    int nstrings, size;
    register lexer *lx;
    register char *rgx;
    register lexposn **llp;
    register int i, j, n;
    bool final;

    lx = ALLOC(lexer, 1);
    buf = s1->text;

    /* grammar info */
    lx->grammar = grammar;
    lx->nregexp = UCHAR(grammar[2]) * 256 + UCHAR(grammar[3]);
    nstrings = UCHAR(grammar[6]) * 256 + UCHAR(grammar[7]);
    lx->strings = grammar + 10 + lx->nregexp * 2;
    lx->whitespace = grammar[1];

    /* size info */
    lx->lexsize = s1->len;
    lx->tmpssize = 3 + 256 + 5 + 5;
    lx->tmppsize = 0;

    /* positions */
    lx->nposn = 0;
    lx->lpc = (lpchunk *) NULL;
    lx->posnhtab = ht_new(4 * (lx->nposn + 1), 257);

    /* states */
    lx->nstates = UCHAR(buf[1]) * 256 + UCHAR(buf[2]) + 1;
    lx->sttsize = 2 * (lx->nposn + nstrings + 1);
    if (lx->sttsize <= lx->nstates) {
	lx->sttsize = lx->nstates + 1;
    }
    lx->sthsize = 2 * lx->sttsize;
    lx->expanded = UCHAR(buf[3]) * 256 + UCHAR(buf[4]);
    lx->states = ALLOC(lexstate, lx->sttsize);
    lx->sthtab = ALLOC(short, lx->sthsize);
    memset(lx->sthtab, '\0', sizeof(short) * lx->sthsize);

    /* zero state */
    lx->states[0].posn = (lexposn **) NULL;
    lx->states[0].str = (short *) NULL;
    lx->states[0].trans = (char *) NULL;
    lx->states[0].nposn = lx->states[0].nstr = 0;
    lx->states[0].ntrans = lx->states[0].len = 0;
    lx->states[0].final = -1;


    /* equivalence classes */
    lx->ecnum = UCHAR(buf[5]);
    buf += 6;
    memcpy(lx->eclass, buf, 256);
    buf += 256;
    lx->ecsplit = ALLOC(char, 256 + 256 + 32 * 256);
    lx->ecmembers = lx->ecsplit + 256;
    lx->ecset = (Uint *) (lx->ecmembers + 256);
    memset(lx->eclass, '\0', 256);
    memset(lx->ecsplit, '\0', 256);
    memset(lx->ecmembers, '\0', 256);
    memset(lx->ecset, -1, 32);
    memset(lx->ecset + 8, '\0', 32 * 255);

    for (i = 1; i <= lx->extended; i++) {
	state = &lx->states[i];
	buf = ls_load(state, buf, lx->ecnum);
	buf = ls_loadtmp(state, buf, tmpbuf, lx->htab, &lx->lpc, &lx->nposn,
			 grammar);
	ls_hash(lx->sthtab, lx->sthsize, state, i);
    }
    while (i < lx->nstates) {
	buf = ls_load(state, buf, 0);
	buf = ls_loadtmp(state, buf, tmpbuf, lx->htab, &lx->lpc, &lx->nposn,
			 grammar);
	ls_hash(lx->sthtab, lx->sthsize, state, i++);
    }

    return lx;
# endif
}

/*
 * NAME:	lexer->save()
 * DESCRIPTION:	save lexer in strings
 */
static void lx_save(lx, arr, v1, v2)
lexer *lx;
array *arr;
value *v1, *v2;
{
}

/*
 * NAME:	lexer->del()
 * DESCRIPTION:	delete lexer
 */
static void lx_del(lx)
lexer *lx;
{
}

/*
 * NAME:	lexer->ecsplit()
 * DESCRIPTION:	split up equivalence classes along the borders of character
 *		sets
 */
static void lx_ecsplit(lx, iset, cset, ncset)
register lexer *lx;
Uint *iset, *cset;
int ncset;
{
    Uint ec1[8], ec2[8];
    register int i, n, c;

    for (c = cs_firstc(iset, 0); c >= 0; c = cs_firstc(iset, c + 1)) {
	for (i = 0; i < ncset; i++) {
	    /*
	     * get the equivalence class of the first char in the input set
	     */
	    n = UCHAR(lx->eclass[c]);
	    if (lx->ecmembers[n] == 1) {
		break;	/* only one character left */
	    }
	    if (cs_overlap(lx->ecset + 8 * n, cset, ec1, ec2)) {
		/*
		 * create new equivalence class
		 */
		memcpy(lx->ecset + 8 * n, ec1, sizeof(ec1));
		memcpy(lx->ecset + 8 * lx->ecnum, ec2, sizeof(ec2));
		lx->ecsplit[lx->ecnum] = n;
		lx->ecmembers[n] -= lx->ecmembers[lx->ecnum] =
				    cs_eclass(ec2, lx->eclass, lx->ecnum);
		lx->ecnum++;
		lx->lexsize += 2 * lx->expanded;
	    }
	    cset += 8;
	}
	cset -= 8 * i;

	/* remove from input set */
	cs_sub(iset, lx->ecset + 8 * UCHAR(lx->eclass[c]));
    }
}

/*
 * NAME:	lexer->newstate()
 * DESCRIPTION:	get the positions and strings for a new state
 */
static int lx_newstate(lx, state, newstate, ecset, cset)
lexer *lx;
register lexstate *state, *newstate;
Uint *ecset, *cset;
{
    char posn[258];
    register int i, n;
    register lexposn *lp;
    register char *p;
    int size, posnsize;
    bool final;

    newstate->trans = (char *) NULL;
    newstate->nposn = newstate->nstr = newstate->ntrans = 0;
    newstate->len = state->len + 1;
    newstate->final = -1;
    newstate->alloc = FALSE;
    posnsize = 0;

    /* positions */
    for (i = 0; i < state->nposn; i++) {
	lp = state->posn[i];
	for (n = lp->size; n > 0; --n) {
	    if (cs_intersect(ecset, cset)) {
		final = lp_trans(lp, ecset, posn + 2, &size);
		if (size != 0) {
		    posn[0] = lp->chain.name[0];
		    posn[1] = lp->chain.name[1];
		    lp = lp_new(lx->posnhtab, posn, size, &lx->lpc, lp->rgx,
				lx->nposn, lp->ruleno, final);
		    if (lp->nposn == lx->nposn) {
			/* new position */
			lx->nposn++;
			posnsize += 8 + lp->size + final;
		    }
		    newstate->posn[newstate->nposn++] = lp;
		}
		if (final && newstate->final < 0) {
		    newstate->final = lp->ruleno;
		}
		cset += 8 * n;
		break;
	    }
	    cset += 8;
	}
    }

    /* strings */
    for (i = 0; i < state->nstr; i++) {
	p = lx->strings + 2 * state->str[i];
	p = lx->grammar + 256 * UCHAR(p[0]) + UCHAR(p[1]);
	n = UCHAR(p[newstate->len]);
	if (ecset[n >> 5] & (1 << (n & 31))) {
	    if (newstate->len == UCHAR(p[0])) {
		/* end of string */
		newstate->final = lx->nregexp + state->str[i];
	    } else {
		/* add string */
		newstate->str[newstate->nstr++] = state->str[i];
	    }
	}
    }

    return posnsize;
}

/*
 * NAME:	lexer->expand()
 * DESCRIPTION:	expand a state
 */
static lexstate *lx_expand(lx, state)
lexer *lx;
lexstate *state;
{
    Uint iset[8];
    register Uint *cset, *ecset;
    register char *p;
    register int ncset, i, n;
    lexstate *newstate;
    lexposn **newposn;
    short *newstr;
    int size;

    memset(iset, '\0', sizeof(iset));

    /* allocate character sets for strings and positions */
    ncset = state->nstr;
    for (i = 0; i < state->nposn; i++) {
	ncset += state->posn[i]->size;
    }
    cset = ALLOCA(Uint, 8 * ncset);

    /* construct character sets for all string chars */
    for (i = 0; i < state->nstr; i++) {
	p = lx->strings + 2 * state->str[i];
	p = lx->grammar + 256 * UCHAR(p[0]) + UCHAR(p[1]);
	n = UCHAR(p[1 + state->len]);
	memset(cset, '\0', 32);
	cset[n >> 5] |= 1 << (n & 31);
	iset[n >> 5] |= 1 << (n & 31);	/* also add to input set */
	cset += 8;
    }

    /* construct character sets for all positions */
    for (i = 0; i < state->nposn; i++) {
	lp_cset(state->posn[i], cset);
	for (n = state->posn[i]->size; --n >= 0; ) {
	    cs_or(iset, cset);		/* add to input set */
	    cset += 8;
	}
    }
    cset -= 8 * ncset;

    /*
     * adjust equivalence classes
     */
    lx_ecsplit(lx, iset, cset, ncset);

    /*
     * for all equivalence classes, compute transition states
     */
    if (state->nposn != 0) {
	newposn = ALLOCA(lexposn*, state->nposn);
    }
    if (state->nstr != 0) {
	newstr = ALLOCA(short, state->nstr);
    }
    p = state->trans = ALLOC(char, 2 * 256);
    state->ntrans = lx->ecnum;
    state->alloc = TRUE;
    cset += 8 * state->nstr;
    for (i = 0, ecset = lx->ecset; i < lx->ecnum; i++, ecset += 8) {
	/* prepare new state */
	newstate = &lx->states[lx->nstates];

	/* flesh out new state */
	newstate->posn = newposn;
	newstate->str = newstr;
	size = lx_newstate(lx, state, newstate, ecset, cset);

	if (newstate->nposn == 0 && newstate->nstr == 0 && newstate->final < 0)
	{
	    /* stuck in state 0 */
	    n = 0;
	} else {
	    if (newstate->nposn == 0) {
		newstate->posn = (lexposn **) NULL;
	    }
	    if (newstate->nstr == 0) {
		newstate->str = (short *) NULL;
		newstate->len = 0;
	    }

	    n = ls_hash(lx->sthtab, lx->sthsize, lx->states, lx->nstates);
	    if (n == lx->nstates) {
		/*
		 * genuinely new state
		 */
		if (newstate->nposn != 0) {
		    newstate->posn = ALLOC(lexposn*, newstate->nposn);
		    memcpy(newstate->posn, newposn,
			   newstate->nposn * sizeof(lexposn*));
		}
		if (newstate->nstr != 0) {
		    newstate->str = ALLOC(short, newstate->nstr);
		    memcpy(newstate->str, newstr,
			   newstate->nstr * sizeof(short));
		}
		lx->lexsize += 2;
		lx->tmpssize += 5 + (newstate->nposn + newstate->nstr) * 2;
		lx->tmppsize += size;
		lx->nstates++;
		if (lx->nstates == lx->sttsize) {
		    lexstate *table;

		    /* grow table */
		    table = ALLOC(lexstate, lx->sttsize *= 2);
		    memcpy(table, lx->states, lx->nstates * sizeof(lexstate));
		    state = &table[state - lx->states];
		    FREE(lx->states);
		    lx->states = table;
		}
	    }
	}

	*p++ = n / 256;
	*p++ = n;
    }

    if (state->nstr != 0) {
	AFREE(newstr);
    }
    if (state->nposn != 0) {
	AFREE(newposn);
    }
    AFREE(cset - 8 * state->nstr);

    lx->expanded++;
    lx->lexsize += lx->ecnum * 2;
    return state;
}

/*
 * NAME:	lexer->extend()
 * DESCRIPTION:	extend transition table
 */
static void lx_extend(lx, state, limit)
register lexer *lx;
register lexstate *state;
register int limit;
{
    register char *p, *q;
    register unsigned int i;

    /* extend transition table */
    if (!state->alloc) {
	p = ALLOC(char, 2 * 256);
	memcpy(p, state->trans, 2 * state->ntrans);
	state->trans = p;
	state->alloc = TRUE;
    }
    p = state->trans + (state->ntrans << 1);
    for (i = state->ntrans; i <= limit; i++) {
	q = &state->trans[UCHAR(lx->ecsplit[i]) << 1];
	*p++ = q[0];
	*p++ = q[1];
    }
    state->ntrans = i;
}

/*
 * NAME:	lexer->lazyscan()
 * DESCRIPTION:	scan the input, while lazily constructing a DFA
 */
static int lx_lazyscan(lx, str, strlen)
register lexer *lx;
string *str;
unsigned int *strlen;
{
    register unsigned int size, eclass;
    register char *p, *q;
    register lexstate *state;
    int final;
    unsigned int fsize;

    size = *strlen;

    while (size != 0) {
	state = &lx->states[1];
	final = -1;
	p = str->text + str->len - size;

	while (size != 0) {
	    eclass = UCHAR(lx->eclass[UCHAR(*p)]);
	    if (state->ntrans <= eclass) {
		if (state->ntrans == 0) {
		    /* expand state */
		    if (state == lx->states) {
			break;	/* stuck in state 0 */
		    }
		    state = lx_expand(lx, state);
		    eclass = UCHAR(lx->eclass[UCHAR(*p)]);
		} else {
		    /* extend transition table */
		    lx_extend(lx, state, eclass);
		}
	    }

	    /* transition */
	    --size;
	    p++;
	    q = &state->trans[eclass << 1];
	    state = &lx->states[(UCHAR(q[0]) << 8) + UCHAR(q[1])];

	    /* check if final state */
	    if (state->final >= 0) {
		final = state->final;
		fsize = size;
	    }
	}

	if (final >= 0) {
	    /* in a final state */
	    size = fsize;
	    if (final != 0 || !lx->whitespace) {
		*strlen = size;
		return final;
	    }
	    /* else whitespace: continue */
	} else {
	    return -2;	/* reject */
	}
    }

    return -1;	/* end of string */
}


typedef struct _parser_ {
    string *source;		/* grammar source */
    string *grammar;		/* preprocessed grammar */
    lexer *lex;			/* lexical scanner */
} parser;

/*
 * NAME:	parser->save()
 * DESCRIPTION:	save parse_string data
 */
void ps_save(data)
dataspace *data;
{
}

/*
 * NAME:	parser->free()
 * DESCRIPTION:	free parse_string data
 */
void ps_free(data)
dataspace *data;
{
}

/*
 * NAME:	parse_string()
 * DESCRIPTION:	parse a string
 */
array *ps_parse_string(data, grammar, str)
dataspace *data;
string *grammar;
string *str;
{
    register parser *ps;
    string *igram;
    bool same;

    if (data->parser != (parser *) NULL) {
	ps = data->parser;
	same = (str_cmp(ps->source, grammar) == 0);
    } else {
	value *val;

	val = d_get_variable(data, data->nvariables - 1);
	if (val->type == T_ARRAY &&
	    str_cmp(d_get_elts(val->u.array)->u.string, grammar) == 0) {
	    /* load_parser(); */
	    same = TRUE;
	} else {
	    ps = (parser *) NULL;
	    same = FALSE;
	}
    }

    if (!same) {
	igram = parse_grammar(grammar);

	if (data->parser != (parser *) NULL) {
	    ps_free(data);
	}
	data->parser = ps = ALLOC(parser, 1);
	str_ref(ps->source = grammar);
	str_ref(ps->grammar = igram);
	ps->lex = lx_new(igram->text);
    }

    {
	int tokens[1000];
	unsigned int size;
	int i, n;
	array *a;

	size = str->len;
	for (i = 0;; i++) {
	    n = lx_lazyscan(ps->lex, str, &size);
	    if (n < 0) {
		if (n == -2) {
		    error("Invalid token");
		}
		break;
	    }
	    tokens[i] = n;
	}

	a = arr_new(data, (long) i);
	for (n = 0; n < i; n++) {
	    a->elts[n].type = T_INT;
	    a->elts[n].u.number = tokens[n];
	}

	return a;
    }
}
