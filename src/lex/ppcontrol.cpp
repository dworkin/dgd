/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

# include "lex.h"
# include "macro.h"
# include "special.h"
# include "ppstr.h"
# include "token.h"
# include "path.h"
# include "ppcontrol.h"

/*
 * Get a token from a file, handling preprocessor control directives.
 */

# define ICHUNKSZ	8

class IFState : public ChunkAllocated {
public:
    void pop();

    static void push();

    bool active;		/* is this ifstate active? */
    bool skipping;		/* skipping this part? */
    bool expect_else;		/* expect #else or #endif? */
    char level;			/* include level */
    IFState *prev;		/* previous ifstate */
};

static Chunk<IFState, ICHUNKSZ> ichunk;

static int include_level;	/* current #include level */
static IFState *ifs;		/* current conditional inclusion state */

/*
 * push a new ifstate on the stack
 */
void IFState::push()
{
    IFState *s;

    s = chunknew (ichunk) IFState;
    s->active = !ifs->skipping;
    s->skipping = TRUE;	/* ! */
    s->expect_else = TRUE;
    s->level = include_level + 1;
    s->prev = ifs;
    ifs = s;
}

/*
 * pop an ifstate from the stack
 */
void IFState::pop()
{
    ifs = prev;

    delete this;
}


static char **idirs;		/* include directory array */
static char pri[NR_TOKENS + 1];	/* operator priority table */
static bool init_pri;		/* has the priority table been initialized? */
static IFState top;		/* initial ifstate */

# define UNARY	0x10

/*
 * initialize preprocessor. Return TRUE if the input file could
 * be opened.
 */
bool Preproc::init(char *file, char **id, char *buffer, unsigned int buflen,
		   int level)
{
    top.active = TRUE;
    top.skipping = FALSE;
    top.expect_else = FALSE;
    top.level = 0;
    top.prev = (IFState *) NULL;

    TokenBuf::init();
    if (!include(file, buffer, buflen)) {
	TokenBuf::clear();
	return FALSE;
    }
    Macro::init();
    Special::define();
    Macro::define("__DGD__", "\0111\011", -1);	/* HT 1 HT */
    include_level = level;
    ifs = &top;

    if (!init_pri) {
	/* #if operator priority table */
	pri['~' + 1]    =
	pri['!' + 1]    = UNARY;
	pri['*' + 1]    =
	pri['/' + 1]    =
	pri['%' + 1]    = 11;
	pri['+' + 1]    =
	pri['-' + 1]    = UNARY | 10;
	pri[LSHIFT + 1] =
	pri[RSHIFT + 1] = 9;
	pri['<' + 1]    =
	pri['>' + 1]    =
	pri[LE + 1]     =
	pri[GE + 1]     = 8;
	pri[EQ + 1]     =
	pri[NE + 1]     = 7;
	pri['&' + 1]    = 6;
	pri['^' + 1]    = 5;
	pri['|' + 1]    = 4;
	pri[LAND + 1]   = 3;
	pri[LOR + 1]    = 2;
	pri['?' + 1]    = 1;

	init_pri = TRUE;
    }
    idirs = id;

    return TRUE;
}

/*
 * terminate preprocessor
 */
void Preproc::clear()
{
    Str::clear();
    while (ifs != &top) {
	ifs->pop();
    }
    ichunk.clean();
    TokenBuf::clear();
    Macro::clear();
}

/*
 * include a file
 */
bool Preproc::include(char *file, char *buffer, unsigned int buflen)
{
    return TokenBuf::include(file, buffer, buflen);
}

/*
 * include a string buffer
 */
void Preproc::push(char *buffer, unsigned int buflen)
{
    TokenBuf::push(buffer, buflen);
}

/*
 * current filename
 */
char *Preproc::filename()
{
    return TokenBuf::filename();
}

/*
 * current line number
 */
unsigned short Preproc::line()
{
    return TokenBuf::line();
}

/*
 * get an unpreprocessed token, skipping white space
 */
int Preproc::wsgettok()
{
    int token;

    do {
	token = TokenBuf::gettok();
    } while (token == ' ');

    return token;
}

/*
 * get a token, while expanding macros
 */
int Preproc::mcgtok()
{
    int token;
    Macro *mc;

    for (;;) {
	token = TokenBuf::gettok();
	if (token == IDENTIFIER) {
	    mc = Macro::lookup(yytext);
	    if (mc != (Macro *) NULL && TokenBuf::expand(mc) > 0) {
		continue;
	    }
	}
	return token;
    }
}

/*
 * get a preprocessed token, skipping white space
 */
int Preproc::wsmcgtok()
{
    int token;

    do {
	token = mcgtok();
    } while (token == ' ' || token == HT);

    return token;
}

static int expr_keep;	/* single character unget buffer for expr_get() */

# define expr_unget(c)	expr_keep = c
# define expr_uncng()	(expr_keep != EOF)
# define expr_unclr()	expr_keep = EOF

/*
 * get a token from the input stream, handling defined(IDENT),
 * replacing undefined identifiers by 0
 */
int Preproc::expr_get()
{
    char buf[MAX_LINE_SIZE];
    int token;

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
		    error("missing identifier in defined");
		}
		if (paren && token != ')') {
		    token = wsgettok();
		    if (token != ')') {
			error("missing ) in defined");
			expr_unget(token);
		    }
		}
		yynumber = (Macro::lookup(buf) != (Macro *) NULL);
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
 * evaluate an expression following #if
 */
long Preproc::eval_expr(int priority)
{
    int token;
    long expr, expr2;

    token = expr_get();
    if (token == '(') {
	/* get an expression between parenthesis */
	expr = eval_expr(0);
	token = expr_get();
	if (token != ')') {
	    error("missing ) in conditional control");
	    expr_unget(token);
	}
    } else if (pri[token + 1] & UNARY) {
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
	if (token == LF) {
	    error("missing expression in conditional control");
	}
	expr_unget(token);
	return 0;
    }

    for (;;) {
	/* get (binary) operator, ), : or \n */
	token = expr_get();
	expr2 = pri[token + 1] & ~UNARY;
	if (expr2 == 0 || priority >= expr2) {
	    expr_unget(token);
	    break;	/* return current expression */
	} else {
	    /* get second operand */
	    expr2 = eval_expr((int) expr2);
	    if (expr2 == 0) {
		if (token == '/') {
		    error("division by zero in conditional control");
		    continue;
		} else if (token == '%') {
		    error("modulus by zero in conditional control");
		    continue;
		}
	    }
	    switch (token) {
	    case '/':		expr /= expr2;		break;
	    case '%':		expr %= expr2;		break;
	    case '*':		expr *= expr2;		break;
	    case '+':		expr += expr2;		break;
	    case '-':		expr -= expr2;		break;
	    case LSHIFT:	expr <<= expr2;		break;
	    case RSHIFT:	expr >>= expr2;		break;
	    case '<':		expr = expr < expr2;	break;
	    case '>':		expr = expr > expr2;	break;
	    case LE:		expr = expr <= expr2;	break;
	    case GE:		expr = expr >= expr2;	break;
	    case EQ:		expr = expr == expr2;	break;
	    case NE:		expr = expr != expr2;	break;
	    case '&':		expr &= expr2;		break;
	    case '^':		expr ^= expr2;		break;
	    case '|':		expr |= expr2;		break;
	    case LAND:		expr = expr && expr2;	break;
	    case LOR:		expr = expr || expr2;	break;
	    case '?':
		token = expr_get();
		if (token != ':') {
		    error("? without : in conditional control");
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
 * return a number in the range 1..12 specifying which preprocessor
 * directive the argument is, or 0 if it isn't.
 */
int Preproc::pptokenz(char *key, unsigned int len)
{
    static const char *keyword[] = {
      "else", "error", "line", "elif", "endif", "if", "define",
      "include", "ifdef", "ifndef", "undef", "pragma"
    };
    static char value[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 9, 0, 0, 4, 0, 3, 0, 0, 4, 0, 0,
      2, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0
    };

    len += value[key[0] - '0'] + value[key[len - 1] - '0'] - 3;
    if (len < 1 || len > 12 || strcmp(keyword[len - 1], key) != 0) {
	return 0;
    }
    return len;
}

# define FIRST_KEYWORD	NOMASK

/*
 * return a number in the range 1..32 specifying which keyword
 * the argument is, or 0 if it isn't. Note that the keywords must
 * be given in the same order here as in parser.y.
 */
int Preproc::tokenz(char *key, unsigned int len)
{
    static const char *keyword[] = {
      "nomask", "break", "do", "mapping", "else", "case", "object", "default",
      "float", "continue", "static", "int", "for", "if", "operator", "inherit",
      "rlimits", "goto", "function", "return", "mixed", "while", "string",
      "try", "private", "void", "new", "catch", "atomic", "nil", "switch",
      "varargs"
    };
    static char value[] = {
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0, 21,  9,  1,  0,  0,  3, 13, 21,  8,  0, 19,
      19, 15,  7,  0, 17,  0,  6,  3,  0,  0, 21, 16,  0, 20,  0
    };

    len = (len + value[key[0] - '0'] + value[key[len - 1] - '0']) % 32;
    if (strcmp(keyword[len], key) == 0) {
# ifndef CLOSURES
	if (len == FUNCTION - FIRST_KEYWORD) {
	    return 0;
	}
# endif
	return len + 1;
    }
    return 0;
}

/*
 * an error has occured, print appropriate errormessage and skip
 * till \n found
 */
void Preproc::unexpected(int token, const char *wanted, const char *directive)
{
    if (token == LF) {
	error("missing %s in #%s", wanted, directive);
    } else {
	error("unexpected token in #%s", directive);
	TokenBuf::skiptonl(FALSE);
    }
}

/*
 * handle an #include preprocessing directive
 */
void Preproc::do_include()
{
    char file[MAX_LINE_SIZE], path[STRINGSZ + MAX_LINE_SIZE], buf[STRINGSZ];
    int token;
    char **idir;

    if (include_level == INCLUDEDEPTH) {
	error("#include nesting too deep");
	TokenBuf::skiptonl(FALSE);
	return;
    }

    TokenBuf::header(TRUE);
    token = wsmcgtok();
    TokenBuf::header(FALSE);

    if (idirs == (char **) NULL) {
	error("illegal #include from config file");
	return;
    }
    if (token == STRING_CONST) {
	strcpy(file, yytext);
	TokenBuf::skiptonl(TRUE);

	/* first try the path direct */
	if (PM->include(buf, TokenBuf::filename(), file) != (char *) NULL) {
	    include_level++;
	    return;
	}
    } else if (token == INCL_CONST) {
	strcpy(file, yytext);
	TokenBuf::skiptonl(TRUE);
    } else {
	unexpected(token, "filename", "include");
	return;
    }

    /* search in standard directories */
    for (idir = idirs; *idir != (char *) NULL; idir++) {
	strcpy(path, *idir);
	strcat(path, "/");
	strcat(path, file);
	if (PM->include(buf, TokenBuf::filename(), path) != (char *) NULL) {
	    include_level++;
	    return;
	}
    }
    error("cannot include \"%s\"", file);
}

/*
 * return the index in the parameter list if the supplied token is
 * a parameter, -1 otherwise
 */
int Preproc::argnum(char **args, int narg, int token)
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
 * handle a #define preprocessor directive
 */
void Preproc::do_define()
{
    char name[MAX_LINE_SIZE], buf[MAX_REPL_SIZE], *args[MAX_NARG], *arg;
    int token, i, narg, errcount;
    Str *s;
    bool seen_space;

    token = wsgettok();
    if (token != IDENTIFIER) {
	unexpected(token, "identifier", "define");
	return;
    }
    strcpy(name, yytext);

    /* scan parameter list (if any) */
    errcount = 0;
    TokenBuf::setpp(TRUE);
    token = TokenBuf::gettok();
    if (token == '(') {
	narg = 0;
	token = wsgettok();
	if (token != ')') {
	    for (;;) {
		if (token == LF || token == EOF) {
		    error("unterminated macro definition");
		    errcount++;
		    break;
		}
		if (token != IDENTIFIER) {
		    error("unexpected token in macro parameter list");
		    errcount++;
		    TokenBuf::skiptonl(FALSE);
		    break;
		}
		if (narg < MAX_NARG) {
		    arg = ALLOCA(char, strlen(yytext) + 1);
		    args[narg++] = strcpy(arg, yytext);
		} else {
		    error("too many parameters in macro definition");
		    errcount++;
		    TokenBuf::skiptonl(FALSE);
		    break;
		}
		token = wsgettok();
		if (token == ')') {
		    break;
		}
		if (token == LF || token == EOF) {
		    error("unterminated macro definition");
		    errcount++;
		    break;
		}
		if (token != ',') {
		    error("unexpected token in macro parameter list");
		    errcount++;
		    TokenBuf::skiptonl(FALSE);
		    break;
		}
		token = wsgettok();
	    }
	    if (errcount > 0) {
		TokenBuf::setpp(FALSE);
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

    s = Str::create(buf, sizeof(buf));
    s->append(HT);
    seen_space = FALSE;

    /* scan replacement list */
    while (token != LF && token != EOF) {
	if (token == ' ') {
	    seen_space = TRUE;
	} else {
	    if (token == HASH) {
		/* parameter must be made a string */
		token = wsgettok();
		i = argnum(args, narg, token);
		if (i >= 0) {
		    s->append("\011\012");	/* HT LF */
		    s->append((i | MA_TAG | MA_STRING));
		    s->append(HT);
		} else {
		    error("# must be followed by parameter");
		    errcount++;
		    TokenBuf::skiptonl(FALSE);
		    break;
		}
	    } else if (token == HASH_HASH) {
		/* concatenate previous token with next */
		if (s->len == 1) {
		    error("## at start of macro replacement list");
		    errcount++;
		    TokenBuf::skiptonl(FALSE);
		    break;
		}
		token = wsgettok();
		if (token == LF || token == EOF) {
		    error("## at end of macro replacement list");
		    errcount++;
		    break;
		}
		if (s->len >= 3 && buf[s->len - 3] == LF) {
		    /* previous token was a parameter: mark it "noexpand"
		       (this has no effect if it is a string) */
		    s->len--;
		    buf[s->len - 1] |= MA_NOEXPAND;
		}
		i = argnum(args, narg, token);
		if (i >= 0) {
		    s->append(LF);
		    s->append((i | MA_TAG | MA_NOEXPAND));
		    s->append(HT);
		} else {
		    s->append(yytext);
		}
	    } else {
		i = argnum(args, narg, token);
		if (i >= 0) {
		    s->append("\011\012");	/* HT LF */
		    s->append((i | MA_TAG));
		    s->append(HT);
		} else {
		    if (seen_space) {
			s->append(' ');
		    }
		    s->append(yytext);
		}
	    }
	    seen_space = FALSE;
	}
	token = TokenBuf::gettok();
    }
    s->append(HT);
    TokenBuf::setpp(FALSE);

    for (i = narg; i > 0; ) {
	--i;
	AFREE(args[i]);
    }
    i = s->len;
    delete s;

    if (errcount == 0) {
	if (i < 0) {
	    error("macro replacement list too large");
	} else if (Special::replace(name) != (char *) NULL) {
	    error("#define of predefined macro");
	} else if (!Macro::define(name, buf, narg)) {
	    error("macro %s redefined", name);
	}
    }
}

/*
 * get a preprocessed token from the input stream, handling
 * preprocessor directives.
 */
int Preproc::gettok()
{
    int token;
    Macro *mc;

    for (;;) {
	if (ifs->skipping) {
	    TokenBuf::setpp(TRUE);
	    token = wsgettok();
	    TokenBuf::setpp(FALSE);
	    if (token != '#' && token != LF && token != EOF) {
		TokenBuf::skiptonl(FALSE);
		continue;
	    }
	} else {
	    token = TokenBuf::gettok();
	}
	switch (token) {
	case EOF:
	    while (ifs->level > include_level) {
		error("missing #endif");
		ifs->pop();
	    }
	    if (include_level > 0) {
		--include_level;
		TokenBuf::endinclude();
		continue;
	    }
	    return token;

	case ' ':
	case HT:
	case LF:
	    break;

	case '!': case '%': case '&': case '(': case ')': case '*':
	case '+': case ',': case '-': case '/': case ':': case ';':
	case '<': case '=': case '>': case '?': case '[': case ']':
	case '^': case '{': case '|': case '}': case '~':
	case LARROW: case RARROW: case PLUS_PLUS: case MIN_MIN: case LSHIFT:
	case RSHIFT: case LE: case GE: case EQ: case NE: case LAND: case LOR:
	case PLUS_EQ: case MIN_EQ: case MULT_EQ: case DIV_EQ: case MOD_EQ:
	case LSHIFT_EQ: case RSHIFT_EQ: case AND_EQ: case XOR_EQ: case OR_EQ:
	case COLON_COLON: case DOT_DOT: case ELLIPSIS: case STRING_CONST:
	    /* legal operators and constants */
	    return token;

	case INT_CONST:
	    /* integer constant */
	    yylval.number = yynumber;
	    return token;

	case FLOAT_CONST:
	    /* floating point constant */
	    yylval.real = yyfloat;
	    return token;

	case IDENTIFIER:
	    mc = Macro::lookup(yytext);
	    if (mc != (Macro *) NULL && TokenBuf::expand(mc) > 0) {
		break;
	    }
	    token = tokenz(yytext, yyleng);
	    if (token > 0) {
		return token + FIRST_KEYWORD - 1;
	    }
	    return IDENTIFIER;

	case HASH:
	case HASH_HASH:
	    token = '#';
	    /* fall through */
	default:
	    if (token >= 32 && token < 127) {
		error("illegal character: '%c'", token);
	    } else {
		error("illegal character: 0x%02x", token);
	    }
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
		    IFState::push();
		    if (!ifs->active) {
			/* #if within unactive or skipped #if */
			TokenBuf::skiptonl(FALSE);
			break;
		    }

		    /* get #if expression */
		    expr_unclr();
		    ifs->skipping = (eval_expr(0) == 0);
		    if (expr_get() != LF) {
			TokenBuf::skiptonl(TRUE);
		    }
		    break;

		case PP_ELIF:
		    if (ifs == &top) {
			error("#elif without #if");
			TokenBuf::skiptonl(FALSE);
		    } else if (!ifs->expect_else) {
			error("#elif after #else");
			TokenBuf::skiptonl(FALSE);
		    } else if (!ifs->active || !ifs->skipping) {
			/* #elif within unactive/after non-skipped #if */
			ifs->active = FALSE;
			ifs->skipping = TRUE;
			TokenBuf::skiptonl(FALSE);
		    } else {
			/* get #elif expression */
			expr_unclr();
			ifs->skipping = (eval_expr(0) == 0);
			if (expr_get() != LF) {
			    TokenBuf::skiptonl(TRUE);
			}
		    }
		    break;

		case PP_IFDEF:
		    IFState::push();
		    if (!ifs->active) {
			TokenBuf::skiptonl(FALSE);
			break;
		    }
		    token = wsgettok();
		    if (token == IDENTIFIER) {
			ifs->skipping =
			  (Macro::lookup(yytext) == (Macro *) NULL);
			TokenBuf::skiptonl(TRUE);
		    } else {
			unexpected(token, "identifier", "ifdef");
		    }
		    break;

		case PP_IFNDEF:
		    IFState::push();
		    if (!ifs->active) {
			TokenBuf::skiptonl(FALSE);
			break;
		    }
		    token = wsgettok();
		    if (token == IDENTIFIER) {
			ifs->skipping =
			  (Macro::lookup(yytext) != (Macro *) NULL);
			TokenBuf::skiptonl(TRUE);
		    } else {
			unexpected(token, "identifier", "ifndef");
		    }
		    break;

		case PP_ELSE:
		    if (ifs == &top) {
			error("#else without #if");
			TokenBuf::skiptonl(FALSE);
		    } else if (!ifs->expect_else) {
			error("#else after #else");
			TokenBuf::skiptonl(FALSE);
		    } else {
			ifs->expect_else = FALSE;
			ifs->skipping ^= ifs->active;
			TokenBuf::skiptonl(TRUE);
		    }
		    break;

		case PP_ENDIF:
		    if (ifs->level <= include_level) {
			error("#endif without #if");
			TokenBuf::skiptonl(FALSE);
		    } else {
			ifs->pop();
			TokenBuf::skiptonl(TRUE);
		    }
		    break;

		case PP_ERROR:
		    if (!ifs->skipping) {
			char buf[MAX_LINE_SIZE];
			Str *s;

			s = Str::create(buf, sizeof(buf));
			TokenBuf::setpp(TRUE);
			for (;;) {
			    token = mcgtok();
			    if (token == LF || token == EOF) {
				break;
			    }
			    if (token != HT) {
				s->append(yytext);
			    }
			}
			TokenBuf::setpp(FALSE);
			if (s->len == 0) {
			    error("#error directive");
			} else {
			    error("#error:%s", buf);
			}
			delete s;
		    } else {
			TokenBuf::skiptonl(FALSE);
		    }
		    break;

		case PP_LINE:
		    if (ifs->skipping) {
			TokenBuf::skiptonl(FALSE);
			break;
		    }
		    token = wsmcgtok();
		    if (token != INT_CONST) {
			unexpected(token, "number", "line");
			break;
		    }
		    TokenBuf::setline((unsigned short) yynumber - 1);
		    TokenBuf::header(TRUE);
		    token = wsmcgtok();
		    TokenBuf::header(FALSE);
		    if (token == STRING_CONST) {
			TokenBuf::setfilename(yytext);
			token = wsmcgtok();
		    }
		    if (token != LF) {
			unexpected(token, "number", "line");
			/* the "number" is fake, it's never used */
		    }
		    break;

		case PP_INCLUDE:
		    if (ifs->skipping) {
			TokenBuf::skiptonl(FALSE);
			break;
		    }
		    do_include();
		    break;

		case PP_DEFINE:
		    if (ifs->skipping) {
			TokenBuf::skiptonl(FALSE);
			break;
		    }
		    do_define();
		    break;

		case PP_UNDEF:
		    if (ifs->skipping) {
			TokenBuf::skiptonl(FALSE);
			break;
		    }
		    token = wsgettok();
		    if (token == IDENTIFIER) {
			if (Special::replace(yytext) != (char *) NULL) {
			    error("#undef of predefined macro");
			} else {
			    Macro::undef(yytext);
			}
			TokenBuf::skiptonl(TRUE);
		    } else {
			unexpected(token, "identifier", "undef");
		    }
		    break;

		case PP_PRAGMA:
		    /* no pragmas */
		    TokenBuf::skiptonl(FALSE);
		    break;

		default:
		    error("undefined control");
		    TokenBuf::skiptonl(FALSE);
		    break;
		}
	    } else if (token != LF) {
		error("undefined control");
		TokenBuf::skiptonl(FALSE);
	    }
	    break;
	}
    }
}
