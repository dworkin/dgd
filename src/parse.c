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
 * NAME:	charset->firstc()
 * DESCRIPTION:	find the first char in a charset
 */
static int cs_firstc(cset, c)
register Uint *cset;
register int c;
{
    register Uint x;

    while (c < 256) {
	if ((x=(cset[c >> 5] >> (c & 31)) != 0) {
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


typedef struct {
    hte chain;			/* hash table chain */
    char *rgx;			/* regular expression this position is in */
    short len;			/* length of position string */
    short nposn;		/* position number */
    short ruleno;		/* the rule this position is in */
    bool final;			/* final position? */
    char *storage;		/* storage for the position string */
} lexposn;

# define LPCHUNKSZ	64

typedef struct _lpchunk_ {
    lexposn lp[LPCHUNKSZ];	/* lexposn chunk */
    int chunksz;		/* size of chunk */
    struct _lpchunk_ *next;	/* next in linked list */
} lpchunk;

/*
 * NAME:	lexposn->new()
 * DESCRIPTION:	allocate a new lexposn
 */
static lexposn *lp_new(c, posn, len, nposn, final, ruleno, alloc, next)
register lpchunk **c;
char *posn;
int len, nposn, ruleno;
bool final, alloc;
lexposn **next;
{
    register lexposn *lp;

    if (*c == (lpchunk *) NULL || (*c)->chunksz == LPCHUNKSZ) {
	lpchunk *x;

	x = ALLOC(lpchunk, 1);
	x->next = *c;
	*c = x;
	x->chunksz = 0;
    }

/* add rgx */

    lp = &(*c)->lp[(*c)->chunksz++];
    lp->chain.next = (hte *) *next;
    *next = lp;
    if (alloc) {
	strcpy(lp->storage = ALLOC(char, len + 1), posn);
	posn = lp->storage;
    } else {
	lp->storage = (char *) NULL;
    }
    lp->chain.name = posn;
    lp->len = len;
    lp->nposn = nposn;
    lp->final = final;
    lp->ruleno = ruleno;

    return lp;
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
	    if (lp->storage != (char *) NULL) {
		FREE(lp->storage);
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
		}

		/* add to heap */
		for (i = ++len, j = i >> 1;
		     UCHAR(heap[j]) > place;
		     i = j, j >>= 1) {
		     heap[i] = heap[j];
		}
		heap[i] = place;
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
	if (*q != lp->rgx[0]) {
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
	}

	cset += 8;
    }
}

/*
 * NAME:	lexposn->trans()
 * DESCRIPTION:	perform a transition on a position, given an input set
 */
static bool lp_trans(lp, cset, buf, buflen)
lexposn *lp;
char *buf;
Uint *cset;
int *buflen;
{
    char trans[256];
    register char *p, *q;
    register int c, n, x;
    char *t;
    Uint found;
    bool negate;

    t = trans;
    for (q = lp->chain.name + 2; *q != '\0'; q++) {
	if (*q != lp->rgx[0]) {
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
		    found ^= 0xffffffffL;
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
    }
    *t = '\0';

    return lp_transposn(lp->rgx, trans, buf, buflen);
}


typedef struct {
    lexposn **posn;		/* regexp positions */
    char *str;			/* strings */
    char *trans;		/* transitions */
    short nposn;		/* number of positions */
    short nstr;			/* number of string positions */
    short ntrans;		/* number of transitions */
    short final;		/* -2: unresolved, -1: not final */
    string *strstorage;		/* storage for strings */
    string *transstorage;	/* storage for transitions */
} lexstate;

typedef struct {
    string *grammar;		/* reference grammar */
    char *regexp;		/* offset of regular expressions in grammar */
    char *strings;		/* offset of strings in grammar */
    short nregexp;		/* # regexps */
    short nstrings;		/* # strings */
    bool whitespace;		/* true if lexer token 0 is whitespace */

    lexstate *states;		/* lexer states */
    unsigned short nstates;	/* # states */
    unsigned short stsize;	/* state table size */

    hashtab *posnhash;		/* position hash table */
    unsigned int nposn;		/* total number of unique positions */
    lpchunk *lpc;		/* current lexposn chunk */

    hashtab *statehash;		/* state hash table */
    unsigned int sthsize;	/* size of state hash table */

    hashtab *transhash;		/* transition hash table */
    unsigned int transhashsz;	/* size of transition hash table */

    int ecnum;			/* number of equivalence classes */
    char eclass[256];		/* equivalence classes */
    char ecmembers[256];	/* members per equivalence class */
    Uint ecset[8][256];		/* equivalence class sets */
} lexer;

/*
 * NAME:	lexer->expand()
 * DESCRIPTION:	expand a state
 */
static void lx_expand(lx, state, len)
lexer *lx;
lexstate *state;
unsigned int len;
{
    Uint ec[256][8];
    bool ecflag[256];
    Uint iset[8];

    memset(ec, '\0', sizeof(ec));
    memset(ecflag, '\0', sizeof(ecflag));
    memset(iset, '\0', sizeof(iset));

    /* allocate character sets for strings and positions */
    ncset = state->nstr;
    for (i = 0; i < state->nposn; i++) {
	ncset += state->posn[i]->len;	/* XXX -2? */
    }
    cset = ALLOCA(Uint, 8 * ncset);

    /* construct character sets for all string chars */
    for (i = 0; i < state->nstr; i++) {
	p = state->str + 2 * i;
	p = lx->grammar->text + 256 * UCHAR(p[0]) + UCHAR(p[1]);
	c = UCHAR(p[1 + len]);
	memset(cset, '\0', 32);
	cset[c >> 5] |= 1 << (c & 31);
	iset[c >> 5] |= 1 << (c & 31));	/* also add to input set */
	cset += 8;
    }

    /* construct character sets for all posns */
    for (i = 0; i < state->nposn; i++) {
	lp_cset(state->posn[i], csets);
	cs_or(iset, cset);			/* also add to input set */
	cset += 8 * state->posn[i]->len;	/* XXX -2? */
    }
    cset -= 8 * ncset;

    /*
     * Check and adjust the equivalence classes as follows:
     * - for the first char in the input set, get the equivalence class
     * - compare with original input set, if no overlap skip to next;
     * - match will all character classes, possibly breaking up the ec;
     *   always keep the ec that contains the first char; stop when down
     *   to 1;
     * - remove resulting ec from input set and repeat for next first char,
     *   until the input set is empty
     */
}

/*
 * NAME:	lexer->lazyscan()
 * DESCRIPTION:	scan the input, meanwhile lazily constructing a DFA
 */
static int lx_lazyscan(lx, str, strlen)
register lexer *lx;
string *str;
unsigned int *strlen;
{
    register unsigned int size, len;
    register char *p, *q;
    register lexstate *state;
    int lastfinal;
    unsigned int lastflen;

    size = *strlen;
    if (size == 0) {
	return -1;	/* end of string */
    }

    for (;;) {
	state = &lx->states[1];
	lastfinal = -1;
	len = 0;
	p = str->text + str->len - size;

	for (;;) {
	    if (state->ntrans == 0) {
		if (state == lx->states) {
		    break;	/* stuck in state 0 */
		}
		lx_expand(lx, state, len);
	    }

	    if (size == 0) {
		break;
	    }
	    --size;

	    /* transition */
	    q = &state->trans[UCHAR(lx->eclass[UCHAR(*p++) << 1])];
	    state = &lx->states[(UCHAR(q[0]) << 8) + UCHAR(q[1])];
	    len++;

	    /* check if final state */
	    if (state->final >= 0) {
		lastfinal = state->final;
		lastflen = len;
	    }
	}

	if (lastfinal >= 0) {
	    /* in a final state */
	    size += len - lastflen;
	    if (lastfinal != 0 || !lx->whitespace) {
		*strlen = size;
		return lastfinal;
	    }
	    /* else continue */
	} else {
	    return -2;	/* reject */
	}
    }
}

/*
 * NAME:	parse_string()
 * DESCRIPTION:	parse a string
 */
array *parse_string(data, gstr, sstr)
dataspace *data;
string *gstr;
string *sstr;
{
    string *str;
    char buf[256], *q;
    Uint cset[8];
    int buflen;
    lexposn foo;

    str = parse_grammar(gstr);
    q = str->text + 10;
    q = str->text + 256 * UCHAR(q[0]) + UCHAR(q[1]) + 2;
    lp_transposn(q, (char *) NULL, buf + 2, &buflen);
    buf[0] = 1;
    buf[1] = 1;
    foo.chain.name = buf;
    lp_cset(&foo, q, cset);
    while (!lp_trans(&foo, q, cset, buf + 2, &buflen)) ;

    return (array *) NULL;
}
