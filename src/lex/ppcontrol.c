# include "lex.h"
# include "macro.h"
# include "special.h"
# include "ppstr.h"
# include "token.h"
# include "path.h"
# include "version.h"
# include "ppcontrol.h"

/*
 * Get a token from a file, handling preprocessor control directives.
 */

# define ICHUNKSZ	8

typedef struct _ifstate_ {
    bool active;		/* is this ifstate active? */
    bool skipping;		/* skipping this part? */
    bool expect_else;		/* expect #else or #endif? */
    char level;			/* include level */
    struct _ifstate_ *prev;	/* previous ifstate */
} ifstate;

typedef struct _ichunk_ {
    ifstate i[ICHUNKSZ];	/* chunk of ifstates */
    struct _ichunk_ *next;	/* next in list */
} ichunk;

static char **idirs;		/* include directory array */
static char pri[NR_TOKENS];	/* operator priority table */
static bool init_pri;		/* has the priority table been initialized? */
static int include_level;	/* current #include level */
static ichunk *ilist;		/* list of ifstate chunks */
static int ichunksz;		/* ifstate chunk size */
static ifstate *flist;		/* free ifstate list */
static ifstate *ifs;		/* current conditional inclusion state */

static ifstate top = {		/* initial ifstate */
    TRUE, FALSE, FALSE, 0, (ifstate *) NULL
};

# define UNARY	0x10

/*
 * NAME:	pp->init()
 * DESCRIPTION:	initialize preprocessor. Return TRUE if the input file could
 *		be opened.
 */
bool pp_init(file, id, level)
char *file, **id;
int level;
{
    tk_init();
    if (!tk_include(file)) {
	tk_clear();
	return FALSE;
    }
    mc_init();
    special_define();
    mc_define("__DGD__", "\t1\t", -1);
    mc_define("__VERSION__", VERSION, -1);
    pps_init();
    include_level = level;
    ifs = &top;
    ilist = (ichunk *) NULL;
    ichunksz = ICHUNKSZ;
    flist = (ifstate *) NULL;

    if (!init_pri) {
	/* #if operator priority table */
	pri['~']    =
	pri['!']    = UNARY;
	pri['*']    =
	pri['/']    =
	pri['%']    = 11;
	pri['+']    =
	pri['-']    = UNARY | 10;
	pri[LSHIFT] =
	pri[RSHIFT] = 9;
	pri['<']    =
	pri['>']    =
	pri[LE]     =
	pri[GE]     = 8;
	pri[EQ]     =
	pri[NE]     = 7;
	pri['&']    = 6;
	pri['^']    = 5;
	pri['|']    = 4;
	pri[LAND]   = 3;
	pri[LOR]    = 2;
	pri['?']    = 1;

	init_pri = TRUE;
    }
    idirs = id;

    return TRUE;
}

/*
 * NAME:	push()
 * DESCRIPTION:	push a new ifstate on the stack
 */
static void push()
{
    register ifstate *s;

    if (flist != (ifstate *) NULL) {
	/* from free list */
	s = flist;
	flist = s->prev;
    } else {
	/* allocate new one */
	if (ichunksz == ICHUNKSZ) {
	    register ichunk *l;

	    l = ALLOC(ichunk, 1);
	    l->next = ilist;
	    ilist = l;
	    ichunksz = 0;
	}
	s = &ilist->i[ichunksz++];
    }
    s->active = !ifs->skipping;
    s->skipping = TRUE;	/* ! */
    s->expect_else = TRUE;
    s->level = include_level;
    s->prev = ifs;
    ifs = s;
}

/*
 * NAME:	pop()
 * DESCRIPTION:	pop an ifstate from the stack
 */
static void pop()
{
    register ifstate *s;

    s = ifs;
    ifs = ifs->prev;

    s->prev = flist;
    flist = s;
}

/*
 * NAME:	pp->clear()
 * DESCRIPTION:	terminate preprocessor
 */
void pp_clear()
{
    register ichunk *l, *f;

    pps_clear();
    while (ifs != &top) {
	pop();
    }
    for (l = ilist; l != (ichunk *) NULL; ) {
	f = l;
	l = l->next;
	FREE(f);
    }
    mc_clear();
    tk_clear();
}

/*
 * NAME:	wsgettok()
 * DESCRIPTION:	get an unpreprocessed token, skipping white space
 */
static int wsgettok()
{
    register int token;

    do {
	token = tk_gettok();
    } while (token == ' ');

    return token;
}

/*
 * NAME:	mcgtok()
 * DESCRIPTION:	get a token, while expanding macros
 */
static int mcgtok()
{
    register int token;
    register macro *mc;

    for (;;) {
	token = tk_gettok();
	if (token == IDENTIFIER) {
	    mc = mc_lookup(yytext);
	    if (mc != (macro *) NULL && tk_expand(mc) > 0) {
		continue;
	    }
	}
	return token;
    }
}

/*
 * NAME:	wsmcgtok()
 * DESCRIPTION:	get a preprocessed token, skipping white space
 */
static int wsmcgtok()
{
    register int token;

    do {
	token = mcgtok();
    } while (token == ' ' || token == '\t');

    return token;
}

static int expr_keep;	/* single character unget buffer for expr_get() */

# define expr_unget(c)	expr_keep = c
# define expr_uncng()	(expr_keep != EOF)
# define expr_unclr()	expr_keep = EOF

/*
 * NAME:	expr_get()
 * DESCRIPTION:	get a token from the input stream, handling defined(IDENT),
 *		replacing undefined identifiers by 0
 */
static int expr_get()
{
    char buf[MAX_LINE_SIZE];
    register int token;

    if (expr_uncng()) {
	/* the unget buffer is not empty */
	token = expr_keep;
	expr_unclr();	/* clear unget buffer */
    } else {
	token = wsmcgtok();
	if (token == IDENTIFIER) {
	    if (strcmp(yytext, "defined") == 0) {
		bool paren;

		token = wsgettok();
		if (token == '(') {
		    paren = TRUE;
		    token = wsgettok();
		} else {
		    paren = FALSE;
		}
		strcpy(buf, yytext);
		if (token != IDENTIFIER) {
		    yyerror("missing identifier in defined");
		}
		if (paren && token != ')') {
		    token = wsgettok();
		    if (token != ')') {
			yyerror("missing ) in defined");
			expr_unget(token);
		    }
		}
		yynumber = (mc_lookup(buf) != (macro *) NULL);
	    } else {
		/* this identifier isn't a macro */
		yynumber = 0;
	    }
	    token = INT_CONST;
	}
    }
    return token;
}

/*
 * NAME:	eval_expr()
 * DESCRIPTION:	evaluate an expression following #if
 */
static long eval_expr(priority)
int priority;
{
    register int token;
    register long expr, expr2;

    token = expr_get();
    if (token == '(') {
	/* get an expression between parenthesis */
	expr = eval_expr(0);
	token = expr_get();
	if (token != ')') {
	    yyerror("missing ) in conditional control");
	    expr_unget(token);
	}
    } else if (pri[token] & UNARY) {
	/* unary operator */
	expr = eval_expr(11);
	switch (token) {
	case '~': expr = ~expr; break;
	case '!': expr = !expr; break;
	case '-': expr = -expr; break;
	}
    } else if (token == INT_CONST) {
	/* integer constant */
	expr = yynumber;
    } else {
	/* bad token */
	if (token == '\n') {
	    yyerror("missing expression in conditional control");
	}
	expr_unget(token);
	return 0;
    }

    for (;;) {
	/* get (binary) operator, ), : or \n */
	token = expr_get();
	expr2 = pri[token] & ~UNARY;
	if (expr2 == 0 || priority >= expr2) {
	    expr_unget(token);
	    break;	/* return current expression */
	} else {
	    /* get second operand */
	    expr2 = eval_expr((int) expr2);
	    if (expr2 == 0) {
		if (token == '/') {
		    yyerror("division by zero in conditional control");
		} else if (token == '%') {
		    yyerror("modulus by zero in conditional control");
		}
	    } else {
		switch (token) {
		case '/':	expr /= expr2;		break;
		case '%':	expr %= expr2;		break;
		case '*':	expr *= expr2;		break;
		case '+':	expr += expr2;		break;
		case '-':	expr -= expr2;		break;
		case LSHIFT:	expr <<= expr2;		break;
		case RSHIFT:	expr >>= expr2;		break;
		case '<':	expr = expr < expr2;	break;
		case '>':	expr = expr > expr2;	break;
		case LE:	expr = expr <= expr2;	break;
		case GE:	expr = expr >= expr2;	break;
		case EQ:	expr = expr == expr2;	break;
		case NE:	expr = expr != expr2;	break;
		case '&':	expr &= expr2;		break;
		case '^':	expr ^= expr2;		break;
		case '|':	expr |= expr2;		break;
		case LAND:	expr = expr && expr2;	break;
		case LOR:	expr = expr || expr2;	break;
		case '?':
		    token = expr_get();
		    if (token != ':') {
			yyerror("? without : in conditional control");
			expr_unget(token);
		    } else if (expr == 0) {
			expr = eval_expr(0);
		    } else {
			eval_expr(0);
			expr = expr2;
		    }
		}
	    }
	}
    }

    return expr;
}


# define PP_ELSE	1
# define PP_ERROR	2
# define PP_LINE	3
# define PP_ELIF	4
# define PP_ENDIF	5
# define PP_IF		6
# define PP_DEFINE	7
# define PP_INCLUDE	8
# define PP_IFDEF	9
# define PP_IFNDEF	10
# define PP_UNDEF	11
# define PP_PRAGMA	12

/*
 * NAME:	pptokenz()
 * DESCRIPTION:	return a number in the range 1..12 specifying which preprocessor
 *		directive the argument is, or 0 if it isn't.
 */
static int pptokenz(key, len)
register char *key;
register int len;
{
    static char *keyword[] = {
      "else", "error", "line", "elif", "endif", "if", "define",
      "include", "ifdef", "ifndef", "undef", "pragma"
    };
    static char value[] = {
      9, 0, 0, 4, 0, 3, 0, 0, 4, 0, 0, 2, 0,
      0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0
    };

    len += value[key[0] - 'a'] + value[key[len - 1] - 'a'] - 3;
    if (len < 1 || len > 12 || strcmp(keyword[len - 1], key) != 0) {
	return 0;
    }
    return len;
}

# define FIRST_KEYWORD	FOR

/*
 * NAME:	tokenz()
 * DESCRIPTION:	return a number in the range 1..22 specifying which keyword
 *		the argument is, or 0 if it isn't. Note that the keywords must
 *		be given in the same order here as in parser.y.
 */
static int tokenz(key, len)
register char *key;
register int len;
{
    static char *keyword[] = {
      "for", "void", "mixed", "inherit", "int", "mapping", "nomask",
      "static", "string", "case", "return", "do", "break", "continue",
      "default", "private", "if", "varargs", "while", "else", "switch",
      "object"
    };
    static char value[] = {
      3, 4, 17, 11, 1, 15, 14, 5, 1, 0, 0, 13, 0,
      2, 0, 0, 0, 9, 6, 6, 13, 0, 3, 0, 0, 0
    };

    if (len >= 2) {
	len = value[key[1] - 'a'] + value[key[len - 2] - 'a'] + 1;
	if (len >= 1 && len <= 22 && strcmp(keyword[len - 1], key) == 0) {
	    return len;
	}
    }
    return 0;
}

/*
 * NAME:	unexpected()
 * DESCRIPTION:	an error has occured, print appropriate errormessage and skip
 *		till \n found
 */
static void unexpected(token, wanted, directive)
int token;
char *wanted, *directive;
{
    if (token == '\n') {
	yyerror("missing %s in #%s", wanted, directive);
    } else {
	yyerror("unexpected token in #%s", directive);
	tk_skiptonl(FALSE);
    }
}

/*
 * NAME:	do_include()
 * DESCRIPTION:	handle an #include preprocessing directive
 */
static void do_include()
{
    char file[MAX_LINE_SIZE], buf[STRINGSZ + MAX_LINE_SIZE], *slash;
    register int token;
    register char **idir;

    if (include_level == 8) {
	yyerror("#include nesting too deep");
	tk_skiptonl(FALSE);
	return;
    }

    tk_header(TRUE);
    token = wsmcgtok();
    tk_header(FALSE);

    if (token == STRING_CONST) {
	strcpy(file, yytext);
	tk_skiptonl(TRUE);

	/* first try the path direct */
	if (tk_include(path_include(tk_filename(), file))) {
	    include_level++;
	    return;
	}
    } else if (token == INCL_CONST) {
	strcpy(file, yytext);
	tk_skiptonl(TRUE);
    } else {
	unexpected(token, "filename", "include");
	return;
    }

    /* search in standard directories */
    for (idir = idirs; *idir != (char *) NULL; idir++) {
	strcpy(buf, *idir);
	strcat(buf, "/");
	strcat(buf, file);
	if (tk_include(path_include(tk_filename(), buf))) {
	    include_level++;
	    return;
	}
    }
    yyerror("cannot find include file \"%s\"", file);
}

/*
 * NAME:	argnum()
 * DESCRIPTION:	return the index in the parameter list if the supplied token is
 *		a parameter, -1 otherwise
 */
static int argnum(args, narg, token)
register char **args;
register int narg, token;
{
    if (token == IDENTIFIER) {
	while (narg > 0) {
	    if (strcmp(args[--narg], yytext) == 0) {
		return narg;
	    }
	}
    }
    return -1;
}

/*
 * NAME:	do_define()
 * DESCRIPTION:	handle a #define preprocessor directive
 */
static void do_define()
{
    char name[MAX_LINE_SIZE], buf[MAX_REPL_SIZE], *args[MAX_NARG];
    register int token, i, narg, errcount;
    register str *s;
    bool seen_space;

    token = wsgettok();
    if (token != IDENTIFIER) {
	unexpected(token, "identifier", "define");
	return;
    }
    strcpy(name, yytext);

    /* scan parameter list (if any) */
    errcount = 0;
    tk_setpp(TRUE);
    token = tk_gettok();
    if (token == '(') {
	narg = 0;
	token = wsgettok();
	if (token != ')') {
	    for (;;) {
		if (token == '\n' || token == EOF) {
		    yyerror("unterminated macro definition");
		    errcount++;
		    break;
		}
		if (token != IDENTIFIER) {
		    yyerror("unexpected token in macro parameter list");
		    errcount++;
		    tk_skiptonl(FALSE);
		    break;
		}
		if (narg < MAX_NARG) {
		    args[narg++] = strcpy(ALLOCA(char, strlen(yytext) + 1),
					  yytext);
		} else {
		    yyerror("too many parameters in macro definition");
		    errcount++;
		    tk_skiptonl(FALSE);
		    break;
		}
		token = wsgettok();
		if (token == ')') {
		    break;
		}
		if (token == '\n' || token == EOF) {
		    yyerror("unterminated macro definition");
		    errcount++;
		    break;
		}
		if (token != ',') {
		    yyerror("unexpected token in macro parameter list");
		    errcount++;
		    tk_skiptonl(FALSE);
		    break;
		}
		token = wsgettok();
	    }
	    if (errcount > 0) {
		tk_setpp(FALSE);
		while (narg > 0) {
		    --narg;
		    AFREE(args[narg]);
		}
		return;
	    }
	}
	token = wsgettok();
    } else {
	/* no parameter list */
	narg = -1;
	if (token == ' ') {
	    token = wsgettok();
	}
    }

    s = pps_new(buf, sizeof(buf));
    pps_ccat(s, '\t');
    seen_space = FALSE;

    /* scan replacement list */
    while (token != '\n' && token != EOF) {
	if (token == ' ') {
	    seen_space = TRUE;
	} else {
	    if (token == HASH) {
		/* parameter must be made a string */
		token = wsgettok();
		i = argnum(args, narg, token);
		if (i >= 0) {
		    pps_scat(s, "\t\n");
		    pps_ccat(s, (i | MA_TAG | MA_STRING));
		    pps_ccat(s, '\t');
		} else {
		    yyerror("# must be followed by parameter");
		    errcount++;
		    tk_skiptonl(FALSE);
		    break;
		}
	    } else if (token == HASH_HASH) {
		/* concatenate previous token with next */
		if (s->len == 1) {
		    yyerror("## at start of macro replacement list");
		    errcount++;
		    tk_skiptonl(FALSE);
		    break;
		}
		token = wsgettok();
		if (token == '\n' || token == EOF) {
		    yyerror("## at end of macro replacement list");
		    errcount++;
		    break;
		}
		if (s->len >= 3 && buf[s->len - 3] == '\n') {
		    /* previous token was a parameter: mark it "noexpand"
		       (this has no effect if it is a string) */
		    s->len--;
		    buf[s->len - 1] |= MA_NOEXPAND;
		}
		i = argnum(args, narg, token);
		if (i >= 0) {
		    pps_ccat(s, '\n');
		    pps_ccat(s, (i | MA_TAG | MA_NOEXPAND));
		    pps_ccat(s, '\t');
		} else {
		    pps_scat(s, yytext);
		}
	    } else {
		i = argnum(args, narg, token);
		if (i >= 0) {
		    pps_scat(s, "\t\n");
		    pps_ccat(s, (i | MA_TAG));
		    pps_ccat(s, '\t');
		} else {
		    if (seen_space) {
			pps_ccat(s, ' ');
		    }
		    pps_scat(s, yytext);
		}
	    }
	    seen_space = FALSE;
	}
	token = tk_gettok();
    }
    pps_ccat(s, '\t');
    tk_setpp(FALSE);

    for (i = narg; i > 0; ) {
	--i;
	AFREE(args[i]);
    }
    i = s->len;
    pps_del(s);

    if (errcount == 0) {
	if (i < 0) {
	    yyerror("macro replacement list too large");
	} else if (special_replace(name) != (char *) NULL) {
	    yyerror("#define of predefined macro");
	} else {
	    mc_define(name, buf, narg);
	}
    }
}

/*
 * NAME:	pp->gettok()
 * DESCRIPTION:	get a preprocessed token from the input stream, handling
 *		preprocessor directives.
 */
int pp_gettok()
{
    register int token;
    register macro *mc;

    for (;;) {
	token = tk_gettok();
	if (ifs->skipping && token != '#' && token != '\n' && token != EOF) {
	    tk_skiptonl(FALSE);
	    continue;
	}
	switch (token) {
	case EOF:
	    if (include_level > 0) {
		--include_level;
		while (ifs->level > include_level) {
		    yyerror("missing #endif");
		    pop();
		}
		tk_endinclude();
		continue;
	    }
	    return token;

	case ' ':
	case '\t':
	case '\f':
	case '\13':	/* nope, not all compilers have \v */
	case '\r':
	case '\n':
	    break;

	case '!': case '%': case '&': case '(': case ')': case '*':
	case '+': case ',': case '-': case '/': case ':': case ';':
	case '<': case '=': case '>': case '?': case '[': case ']':
	case '^': case '{': case '|': case '}': case '~':
	case ARROW: case PLUS_PLUS: case MIN_MIN: case LSHIFT: case RSHIFT:
	case LE: case GE: case EQ: case NE: case LAND: case LOR:
	case PLUS_EQ: case MIN_EQ: case MULT_EQ: case DIV_EQ: case MOD_EQ:
	case LSHIFT_EQ: case RSHIFT_EQ: case AND_EQ: case XOR_EQ: case OR_EQ:
	case COLON_COLON: case DOT_DOT: case STRING_CONST:
	    /* legal operators and constants */
	    return token;

	case INT_CONST:
	    /* integer constant */
	    yylval.number = yynumber;
	    return token;

	case IDENTIFIER:
	    mc = mc_lookup(yytext);
	    if (mc != (macro *) NULL && tk_expand(mc) > 0) {
		break;
	    }
	    token = tokenz(yytext, yyleng);
	    if (token > 0) {
		return token + FIRST_KEYWORD - 1;
	    }
	    return IDENTIFIER;

	default:
	    yyerror("illegal character: 0x%02x", token);
	    break;

	case '#':
	    /*
	     * Tk_gettok() returns HASH if a '#' is encountered in a macro
	     * expansion. So currently, no macro is being expanded.
	     */
	    token = wsgettok();
	    if (token == IDENTIFIER) {
		switch (pptokenz(yytext, strlen(yytext))) {
		case PP_IF:
		    push();
		    if (!ifs->active) {
			/* #if within unactive or skipped #if */
			tk_skiptonl(FALSE);
			break;
		    }

		    /* get #if expression */
		    expr_unclr();
		    ifs->skipping = (eval_expr(0) == 0);
		    if (expr_get() != '\n') {
			tk_skiptonl(TRUE);
		    }
		    break;

		case PP_ELIF:
		    if (ifs == &top) {
			yyerror("#elif without #if");
			tk_skiptonl(FALSE);
		    } else if (!ifs->expect_else) {
			yyerror("#elif after #else");
			tk_skiptonl(FALSE);
		    } else if (!ifs->active || !ifs->skipping) {
			/* #elif within unactive/after non-skipped #if */
			ifs->active = FALSE;
			ifs->skipping = TRUE;
			tk_skiptonl(FALSE);
		    } else {
			/* get #elif expression */
			expr_unclr();
			ifs->skipping = (eval_expr(0) == 0);
			if (expr_get() != '\n') {
			    tk_skiptonl(TRUE);
			}
		    }
		    break;

		case PP_IFDEF:
		    push();
		    if (!ifs->active) {
			tk_skiptonl(FALSE);
			break;
		    }
		    token = wsgettok();
		    if (token == IDENTIFIER) {
			ifs->skipping =
			  (mc_lookup(yytext) == (macro *) NULL);
			tk_skiptonl(TRUE);
		    } else {
			unexpected(token, "identifier", "ifdef");
		    }
		    break;

		case PP_IFNDEF:
		    push();
		    if (!ifs->active) {
			tk_skiptonl(FALSE);
			break;
		    }
		    token = wsgettok();
		    if (token == IDENTIFIER) {
			ifs->skipping =
			  (mc_lookup(yytext) != (macro *) NULL);
			tk_skiptonl(TRUE);
		    } else {
			unexpected(token, "identifier", "ifndef");
		    }
		    break;

		case PP_ELSE:
		    if (ifs == &top) {
			yyerror("#else without #if");
			tk_skiptonl(FALSE);
		    } else if (!ifs->expect_else) {
			yyerror("#else after #else");
			tk_skiptonl(FALSE);
		    } else {
			ifs->expect_else = FALSE;
			ifs->skipping ^= ifs->active;
			tk_skiptonl(TRUE);
		    }
		    break;

		case PP_ENDIF:
		    if (ifs == &top || ifs->level < include_level) {
			yyerror("#endif without #if");
			tk_skiptonl(FALSE);
		    } else {
			pop();
			tk_skiptonl(TRUE);
		    }
		    break;

		case PP_ERROR:
		    if (!ifs->skipping) {
			char buf[MAX_LINE_SIZE];
			register str *s;

			s = pps_new(buf, sizeof(buf));
			tk_setpp(TRUE);
			for (;;) {
			    token = mcgtok();
			    if (token == '\n' || token == EOF) {
				break;
			    }
			    if (token != '\t') {
				pps_scat(s, yytext);
			    }
			}
			tk_setpp(FALSE);
			if (s->len == 0) {
			    lexwarning("#error directive");
			} else {
			    lexwarning("#error:%s", buf);
			}
			pps_del(s);
		    } else {
			tk_skiptonl(FALSE);
		    }
		    break;

		case PP_LINE:
		    if (ifs->skipping) {
			tk_skiptonl(FALSE);
			break;
		    }
		    token = wsmcgtok();
		    if (token != INT_CONST) {
			unexpected(token, "number", "line");
			break;
		    }
		    tk_setline((unsigned short) yynumber - 1);
		    tk_header(TRUE);
		    token = wsmcgtok();
		    tk_header(FALSE);
		    if (token == STRING_CONST) {
			tk_setfilename(yytext);
			token = wsmcgtok();
		    }
		    if (token != '\n') {
			unexpected(token, "number", "line");
			/* the "number" is fake, it's never used */
		    }
		    break;

		case PP_INCLUDE:
		    if (ifs->skipping) {
			tk_skiptonl(FALSE);
			break;
		    }
		    do_include();
		    break;

		case PP_DEFINE:
		    if (ifs->skipping) {
			tk_skiptonl(FALSE);
			break;
		    }
		    do_define();
		    break;

		case PP_UNDEF:
		    if (ifs->skipping) {
			tk_skiptonl(FALSE);
			break;
		    }
		    token = wsgettok();
		    if (token == IDENTIFIER) {
			if (special_replace(yytext) != (char *) NULL) {
			    yyerror("#undef of predefined macro");
			} else {
			    mc_undef(yytext);
			}
			tk_skiptonl(TRUE);
		    } else {
			unexpected(token, "identifier", "undef");
		    }
		    break;

		case PP_PRAGMA:
		    /* no pragmas */
		    tk_skiptonl(FALSE);
		    break;

		default:
		    yyerror("undefined control");
		    tk_skiptonl(FALSE);
		    break;
		}
	    } else if (token != '\n') {
		yyerror("undefined control");
		tk_skiptonl(FALSE);
	    }
	    break;
	}
    }
}
