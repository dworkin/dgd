# include <ctype.h>
# define INCLUDE_FILE_IO
# include "lex.h"
# include "macro.h"
# include "special.h"
# include "ppstr.h"
# include "token.h"

/*
 * The functions for getting a (possibly preprocessed) token from the input
 * stream.
 */

# define TCHUNKSZ	8

typedef struct _tbuf_ {
    char *buffer;		/* token buffer */
    char *p;			/* token buffer pointer */
    int inbuf;			/* # chars in token buffer */
    char ubuf[4];		/* unget buffer */
    char *up;			/* unget buffer pointer */
    bool eof;			/* TRUE if empty(buffer) -> EOF */
    unsigned short line;	/* line number */
    int fd;			/* file descriptor */
    union {
	char *filename;		/* file name */
	macro *mc;		/* macro this buffer is an expansion of */
    } u;
    struct _tbuf_ *prev;	/* previous token buffer */
} tbuf;

typedef struct _tchunk_ {
    tbuf t[TCHUNKSZ];		/* chunk of token buffers */
    struct _tchunk_ *next;	/* next in list */
} tchunk;

char *yytext;			/* for strings and identifiers */
static char *yytext1, *yytext2;	/* internal buffers */
long yynumber;			/* integer constants */
static tchunk *tlist;		/* list of token buffer chunks */
static int tchunksz;		/* token buffer chunk size */
static tbuf *flist;		/* free token buffer list */
static tbuf *tbuffer;		/* current token buffer */
static tbuf *ibuffer;		/* current input buffer */
static int pp_level;		/* the recursive preprocesing level */
static int include_level;	/* the nested include level */
static bool do_include;		/* treat < and strings specially */
static bool seen_nl;		/* just seen a newline */

/*
 * NAME:	token->init()
 * DESCRIPTION:	initialize the new token input buffer
 */
void tk_init()
{
    yytext1 = ALLOC(char, MAX_LINE_SIZE);
    yytext2 = ALLOC(char, MAX_LINE_SIZE);
    tlist = (tchunk *) NULL;
    tchunksz = TCHUNKSZ;
    flist = (tbuf *) NULL;
    tbuffer = (tbuf *) NULL;
    ibuffer = (tbuf *) NULL;
    pp_level = 0;
    include_level = 0;
    do_include = FALSE;
}

/*
 * NAME:	push()
 * DESCRIPTION:	Push a buffer on the token input stream. If eof is false, then
 *		the buffer will automatically be dropped when all is read.
 */
static void push(mc, buffer, eof)
macro *mc;
char *buffer;
bool eof;
{
    register tbuf *tb;

    if (flist != (tbuf *) NULL) {
	/* from free list */
	tb = flist;
	flist = tb->prev;
    } else {
	/* allocate new one */
	if (tchunksz == TCHUNKSZ) {
	    register tchunk *l;

	    l = ALLOC(tchunk, 1);
	    l->next = tlist;
	    tlist = l;
	    tchunksz = 0;
	}
	tb = &tlist->t[tchunksz++];
    }
    tb->p = tb->buffer = buffer;
    tb->inbuf = strlen(buffer);
    tb->up = tb->ubuf;
    tb->eof = eof;
    tb->fd = -1;
    tb->u.mc = mc;
    tb->prev = tbuffer;
    tbuffer = tb;
}

/*
 * NAME:	pop()
 * DESCRIPTION:	Drop the current token input buffer. If the associated macro
 *		is function-like, the token buffer will have to be deallocated.
 */
static void pop()
{
    register tbuf *tb;

    tb = tbuffer;
    if (tb->fd < 0) {
	if (tb->u.mc != (macro *) NULL) {
	    if (tb->u.mc->narg > 0) {
		/* in the buffer a function-like macro has been expanded */
		FREE(tb->buffer);
	    }
	}
    } else {
	close(tb->fd);
	--include_level;
	ibuffer = tbuffer->prev;
	FREE(tb->u.filename);
	FREE(tb->buffer);
    }
    tbuffer = tb->prev;

    tb->prev = flist;
    flist = tb;
}

/*
 * NAME:	token->clear()
 * DESCRIPTION:	clear all of the token input buffers
 */
void tk_clear()
{
    register tchunk *l, *f;

    while (tbuffer != (tbuf *) NULL) {
	pop();
    }
    for (l = tlist; l != (tchunk *) NULL; ) {
	f = l;
	l = l->next;
	FREE(f);
    }
    FREE(yytext2);
    FREE(yytext1);
}

/*
 * NAME:	token->include()
 * DESCRIPTION:	push a file on the input stream
 */
bool tk_include(file)
char *file;
{
    int fd;

    if (file != (char *) NULL) {
	fd = open(file, O_RDONLY);
	if (fd >= 0) {
	    char *buffer;
	    register int len;

	    if (include_level == 8) {
		close(fd);
		yyerror("#include nesting too deep");
		return TRUE;	/* no further errors */
	    }
	    include_level++;
	    buffer = ALLOC(char, BUF_SIZE);
	    buffer[0] = '\0';
	    push((macro *) NULL, buffer, (tbuffer == (tbuf *) NULL));
	    ibuffer = tbuffer;
	    ibuffer->fd = fd;
	    len = strlen(file);
	    if (len >= STRINGSZ) {
		len = STRINGSZ - 1;
	    }
	    ibuffer->u.filename = strncpy(ALLOC(char, len + 1), file, len);
	    ibuffer->u.filename[len] = '\0';
	    ibuffer->line = 1;
	    seen_nl = TRUE;

	    return TRUE;
	}
    }

    return FALSE;
}

/*
 * NAME:	token->line()
 * DESCRIPTION:	return the current line number (possibly adjusted)
 */
unsigned short tk_line()
{
    return ibuffer->line - (unsigned short) seen_nl;
}

/*
 * NAME:	token->filename()
 * DESCRIPTION:	return the current file name
 */
char *tk_filename()
{
    return ibuffer->u.filename;
}

/*
 * NAME:	token->setline()
 * DESCRIPTION:	set the current line number
 */
void tk_setline(line)
unsigned short line;
{
    ibuffer->line = line;
}

/*
 * NAME:	token->setfilename()
 * DESCRIPTION:	set the current file name
 */
void tk_setfilename(file)
char *file;
{
    register int len;

    FREE(ibuffer->u.filename);
    len = strlen(file);
    if (len >= STRINGSZ) {
	len = STRINGSZ - 1;
    }
    ibuffer->u.filename = strncpy(ALLOC(char, len + 1), file, len);
    ibuffer->u.filename[len] = '\0';
}

/*
 * NAME:	token->header()
 * DESCRIPTION:	set the current include string mode. if TRUE, '<' will be
 *		specially processed.
 */
void tk_header(incl)
bool incl;
{
    do_include = incl;
}

/*
 * NAME:	token->setpp()
 * DESCRIPTION:	if the argument is true, do not translate escape sequences in
 *		strings.
 */
void tk_setpp(pp)
bool pp;
{
    pp_level = (int) pp;
}

/*
 * NAME:	uc()
 * DESCRIPTION:	unget a character on the input
 */
static void uc(c)
int c;
{
    if (c != EOF) {	/* don't unget EOF */
	if (c == '\n' && tbuffer == ibuffer) {
	    ibuffer->line--;
	}
	*(tbuffer->up)++ = c;
    }
}

/*
 * NAME:	gc()
 * DESCRIPTION:	get a character from the input
 */
static int gc()
{
    register int c;
    register bool backslash;

    backslash = FALSE;

    for (;;) {
	if (tbuffer->up != tbuffer->ubuf) {
	    /* get a character from unget buffer */
	    c = UCHAR(*--(tbuffer->up));
	} else {
	    if (tbuffer->inbuf <= 0) {
		/* Current input buffer is empty. Try a refill. */
		if (tbuffer->fd >= 0 &&
		    (tbuffer->inbuf =
		     read(tbuffer->fd, tbuffer->buffer, BUF_SIZE)) > 0) {
		    tbuffer->p = tbuffer->buffer;
		} else if (backslash) {
		    return '\\';
		} else if (tbuffer->eof) {
		    return EOF;
		} else {
		    /* otherwise, pop the current token input buffer */
		    pop();
		    continue;
		}
	    }
	    tbuffer->inbuf--;
	    c = UCHAR(*(tbuffer->p)++);
	}

	if (c == '\n' && tbuffer == ibuffer) {
	    ibuffer->line++;
	    if (!backslash) {
		return c;
	    }
	    backslash = FALSE;
	} else {
	    if (backslash) {
		uc(c);
		return '\\';
	    } else if (c == '\\' && tbuffer == ibuffer) {
		backslash = TRUE;
	    } else {
		return c;
	    }
	}
    }
}

/*
 * NAME:	comment()
 * DESCRIPTION:	skip comments and white space
 */
static void comment()
{
    register int c;

    for (;;) {
	/* first skip this comment */
	do {
	    do {
		c = gc();
		if (c == EOF) {
		    yyerror("EOF in comment");
		    return;
		}
	    } while (c != '*');
	    do {
		c = gc();
	    } while (c == '*');
	} while (c != '/');

	/* skip following whitespace */
	do {
	    c = gc();
	} while (c == ' ' || c == '\t');

	/* check if a new comment starts after this one */
	if (c != '/') {
	    uc(c);
	    break;
	}
	c = gc();
	if (c != '*') {
	    uc(c);
	    uc('/');
	    break;
	}
    }
}

/*
 * NAME:	token->string()
 * DESCRIPTION:	handle a string. If pp_level > 0, don't translate escape
 *		sequences.
 */
static int tk_string(quote)
char quote;
{
    register char *p;
    register int c;

    p = yytext;
    if (pp_level > 0) {
	/* keep the quotes if not on top level */
	p++;
    }

    for (;;) {
	c = gc();
	if (c == quote) {
	    break;
	} else if (c == EOF) {
	    yyerror("EOF in string");
	    break;
	} else if (c == '\\') {
	    c = gc();
	    if (c == EOF) {
		continue;
	    }
	    if (pp_level == 0 && !do_include) {
		/* translate escape sequences */
		switch (c) {
		case 'n':
		    c = '\n';
		    break;

		case 't':
		    c = '\t';
		    break;
		}
	    } else {
		*p++ = '\\';
	    }
	}
	if (p >= yytext + MAX_LINE_SIZE - 2) {
	    yyerror("string too long");
	    p = yytext + MAX_LINE_SIZE - 2;
	    break;
	}
	*p++ = c;
    }

    if (pp_level > 0) {
	/* keep the quotes if not on top level */
	*p++ = quote;
    }
    *p = '\0';
    return (quote == '>') ? INCL_CONST : STRING_CONST;
}

/*
 * NAME:	token->gettok()
 * DESCRIPTION:	get a token from the input stream.
 */
int tk_gettok()
{
    register int c;
    register long result;
    register char *p;

# define TEST(x, tok)	if (c == x) { c = tok; break; }
# define CHECK(x, tok)	*p++ = c = gc(); TEST(x, tok); --p; uc(c)

    result = 0;
    yytext = (yytext == yytext1) ? yytext2 : yytext1;
    p = yytext;
    *p++ = c = gc();
    switch (c) {
    case '\n':
	if (tbuffer == ibuffer) {
	    seen_nl = TRUE;
	    *p = '\0';
	    return c;
	}
	c = (pp_level > 0) ? MARK : ' ';
	break;

    case '\t':
	if (tbuffer != ibuffer) {
	    /* expanding a macro: keep separator */
	    break;
	}
	/* fall through */

    case ' ':
	/* white space */
	do {
	    c = gc();
	} while (c == ' ' || (c == '\t' && tbuffer == ibuffer));

	/* check for comment after white space */
	if (c == '/') {
	    c = gc();
	    if (c == '*') {
		comment();
	    } else {
		uc(c);
		uc('/');
	    }
	} else {
	    uc(c);
	}
	*p = '\0';
	return p[-1] = ' ';

    case '!':
	CHECK('=', NE);
	c = '!';
	break;

    case '#':
	if (seen_nl) {
	    break;
	}
	CHECK('#', HASH_HASH);
	c = HASH;
	break;

    case '%':
	CHECK('=', MOD_EQ);
	c = '%';
	break;

    case '&':
	*p++ = c = gc();
	TEST('&', LAND);
	TEST('=', AND_EQ);
	--p; uc(c);
	c = '&';
	break;

    case '*':
	CHECK('=', MULT_EQ);
	c = '*';
	break;

    case '+':
	*p++ = c = gc();
	TEST('+', PLUS_PLUS);
	TEST('=', PLUS_EQ);
	--p; uc(c);
	c = '+';
	break;

    case '-':
	*p++ = c = gc();
	TEST('>', ARROW);
	TEST('-', MIN_MIN);
	TEST('=', MIN_EQ);
	--p; uc(c);
	c = '-';
	break;

    case '.':
	CHECK('.', DOT_DOT);
	c = '.';
	break;

    case '/':
	c = gc();
	if (c == '*') {
	    comment();
	    *p = '\0';
	    return p[-1] = ' ';
	}
	*p++ = c;
	TEST('=', DIV_EQ);
	--p; uc(c);
	c = '/';
	break;

    case ':':
	CHECK(':', COLON_COLON);
	c = ':';
	break;

    case '<':
	if (do_include) {
	    /* #include <header> */
	    seen_nl = FALSE;
	    return tk_string('>');
	}
	*p++ = c = gc();
	TEST('=', LE);
	if (c == '<') {
	    CHECK('=', LSHIFT_EQ);
	    c = LSHIFT;
	    break;
	}
	--p; uc(c);
	c = '<';
	break;

    case '=':
	CHECK('=', EQ);
	c = '=';
	break;

    case '>':
	*p++ = c = gc();
	TEST('=', GE);
	if (c == '>') {
	    CHECK('=', RSHIFT_EQ);
	    c = RSHIFT;
	    break;
	}
	--p; uc(c);
	c = '>';
	break;

    case '^':
	CHECK('=', XOR_EQ);
	c = '^';
	break;

    case '|':
	*p++ = c = gc();
	TEST('|', LOR);
	TEST('=', OR_EQ);
	--p; uc(c);
	c = '|';
	break;

    case '0':
	c = gc();
	if (c == 'x' || c == 'X') {
	    *p++ = c;
	    while (isxdigit(c = gc())) {
		if (p < yytext + MAX_LINE_SIZE - 1) {
		    *p++ = c;
		}
		if (isdigit(c)) {
		    c -= '0';
		} else {
		    c = toupper(c) + 10 - 'A';
		}
		result <<= 4;
		result += c;
	    }
	} else {
	    while (c >= '0' && c <= '7') {
		if (p < yytext + MAX_LINE_SIZE) {
		    *p++ = c;
		}
		result <<= 3;
		result += c - '0';
		c = gc();
	    }
	}
	uc(c);
	yynumber = result;
	c = INT_CONST;
	break;

    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
	for (;;) {
	    result *= 10;
	    result += c - '0';
	    c = gc();
	    if (!isdigit(c)) {
		break;
	    }
	    if (p < yytext + MAX_LINE_SIZE - 1) {
		*p++ = c;
	    }
	}
	uc(c);
	yynumber = result;
	c = INT_CONST;
	break;

    case '_':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
    case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
    case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
	for (;;) {
	    c = gc();
	    if (!isalnum(c) && c != '_') {
		break;
	    }
	    if (p != yytext + MAX_LINE_SIZE - 1) {
		*p++ = c;
	    }
	}
	uc(c);
	c = IDENTIFIER;
	break;

    case '\'':
	*p++ = c = gc();
	if (c == '\'') {
	    yyerror("too short character constant");
	} else if (c == '\\') {
	    *p++ = c = gc();
	    if (c == 'n') {
		c = '\n';
	    } else if (c == 't') {
		c = '\t';
	    }
	}
	if ((*p++ = gc()) != '\'') {
	    yyerror("illegal character constant");
	}
	yynumber = c;
	c = INT_CONST;
	break;

    case '"':
	seen_nl = FALSE;
	return tk_string('"');

    case EOF:
	c = EOF;
	break;
    }
    *p = '\0';
    seen_nl = FALSE;

    return c;
}

/*
 * NAME:	token->skiptonl()
 * DESCRIPTION:	skip tokens until a newline or EOF is found. If the argument is
 *		TRUE, only whitespace is allowed.
 */
void tk_skiptonl(ws)
bool ws;
{
    register int c;

    for (;;) {
	switch (gc()) {
	case EOF:
	    yyerror("unterminated line");
	    return;

	case '\n':
	    seen_nl = TRUE;
	    return;

	case ' ':
	case '\t':
	    break;

	case '/':
	    c = gc();
	    if (c == '*') {
		comment();
		break;
	    } else {
		uc(c);
	    }
	    /* fall through */
	default:
	    if (ws) {
		yyerror("bad token in control");
		ws = FALSE;
	    }
	    break;
	}
    }
}

/*
 * NAME:	token->expand()
 * DESCRIPTION:	expand a macro, pushing it on the input stream
 *		return: -1 if the macro is nested and is not expanded
 *			0 if the macro is ftn-like and the call isn't
 *			1 if the macro was expanded
 */
int tk_expand(mc)
register macro *mc;
{
    register int token;

    if (tbuffer != ibuffer) {
	register tbuf *tb;

	token = gc();
	if (token == '\n') {
	    return -1;
	}
	uc(token);

	tb = tbuffer;
	do {
	    if (tb->fd < 0 && tb->u.mc != (macro *) NULL &&
	      strcmp(mc->chain.name, tb->u.mc->chain.name) == 0) {
		return -1;
	    }
	    tb = tb->prev;
	} while (tb != ibuffer);
    }

    if (mc->narg >= 0) {
	char *args[MAX_NARG], ppbuf[MAX_REPL_SIZE];
	register int narg;
	register str *s;
	int errcount;

	do {
	    token = gc();
	    if (token == '/') {
		token = gc();
		if (token == '*') {
		    comment();
		    token = gc();
		} else {
		    uc(token);
		}
		break;
	    }
	} while (token == ' ' || token == '\t' || token == '\n');

	if (token != '(') {
	    /* macro is function-like, and this is not an invocation */
	    uc(token);
	    return 0;
	}

	/* scan arguments */
	narg = 0;
	errcount = 0;
	pp_level++;
	s = pps_new(ppbuf, sizeof(ppbuf));
	do {
	    token = tk_gettok();
	} while (token == ' ' || token == '\t' || token == '\n');

	if (token != ')' || mc->narg != 0) {
	    int paren;
	    bool seen_space, seen_sep;

	    paren = 0;
	    seen_space = FALSE;
	    seen_sep = FALSE;

	    for (;;) {
		if (token == EOF) {	/* sigh */
		    yyerror("EOF in macro call");
		    errcount++;
		    break;
		}

		if ((token == ',' || token == ')') && paren == 0) {
		    if (s->len < 0) {
			yyerror("macro argument too long");
			errcount++;
		    } else if (narg < mc->narg) {
			args[narg] = strcpy(ALLOCA(char, s->len + 1), ppbuf);
		    }
		    narg++;
		    if (token == ')') {
			break;
		    }

		    s->len = 0;

		    do {
			token = tk_gettok();
		    } while (token == ' ' || token == '\t' || token == '\n');
		    seen_space = FALSE;
		    seen_sep = FALSE;
		} else {
		    if (seen_space) {
			pps_ccat(s, ' ');
			seen_space = FALSE;
			seen_sep = FALSE;
		    } else if (seen_sep) {
			pps_ccat(s, '\t');
			seen_sep = FALSE;
		    }
		    pps_scat(s, yytext);
		    if (token == '(') {
			paren++;
		    } else if (token == ')') {
			--paren;
		    }

		    for (;;) {
			token = tk_gettok();
			if (token == ' ' || token == '\n') {
			    seen_space = TRUE;
			} else if (token == '\t') {
			    seen_sep = TRUE;
			} else {
			    break;
			}
		    }
		}
	    }
	}
	--pp_level;

	if (narg != mc->narg) {
	    yyerror("macro argument mismatch");
	    errcount++;
	}

	if (errcount > 0) {
	    if (narg > mc->narg) {
		narg = mc->narg;
	    }
	    while (narg > 0) {
		--narg;
		AFREE(args[narg]);
	    }
	    pps_del(s);
	    return 1;	/* skip this macro */
	}

	if (narg > 0) {
	    push((macro *) NULL, mc->replace, TRUE);
	    s->len = 0;

	    pp_level++;
	    while ((token=tk_gettok()) != EOF) {
		if (token == MARK) {	/* macro argument follows */
		    token = gc();
		    narg = token & MA_NARG;
		    if (token & MA_STRING) {
			register char *p;

			/* copy it, inserting \ before \ and " */
			push((macro *) NULL, args[narg], TRUE);
			pps_ccat(s, '"');
			while ((token=tk_gettok()) != EOF) {
			    if (token != '\t') {
				p = yytext;
				while (*p != '\0') {
				    if (*p == '"' || *p == '\\') {
					pps_ccat(s, '\\');
				    }
				    pps_ccat(s, *p++);
				}
			    }
			}
			pps_ccat(s, '"');
			pop();
		    } else if (token & MA_NOEXPAND) {

			/*
			 * if the previous token was a not-to-expand macro,
			 * make it a normal identifier
			 */
			if (s->len > 0 && ppbuf[s->len - 1] == '\n') {
			    s->len--;
			}

			push((macro *) NULL, args[narg], TRUE);
			token = tk_gettok();
			/*
			 * if the first token of the argument is a
			 * not-to-expand macro, make it a normal identifier
			 */
			if (token == IDENTIFIER && (narg=gc()) != '\n') {
			    uc(narg);
			}
			while (token != EOF) {
			    pps_scat(s, yytext);
			    token = tk_gettok();
			}
			pop();
		    } else {

			/* preprocess the argument */
			push((macro *) NULL, args[narg], TRUE);
			while ((token=tk_gettok()) != EOF) {
			    if (token == IDENTIFIER) {
				macro *mc;

				if ((mc=mc_lookup(yytext)) != (macro *) NULL) {
				    token = tk_expand(mc);
				    if (token > 0) {
					continue;
				    }
				    if (token < 0) {
					pps_scat(s, yytext);
					pps_ccat(s, '\n');
					continue;
				    }
				}
			    }
			    pps_scat(s, yytext);
			}
			pop();
		    }
		} else {
		    /* copy this token */
		    pps_scat(s, yytext);
		}
	    }
	    --pp_level;
	    pop();

	    /* cleanup */
	    narg = mc->narg;
	    do {
		--narg;
		AFREE(args[narg]);
	    } while (narg > 0);

	    narg = s->len;	/* so s can be deleted before the push */
	    pps_del(s);
	    if (narg < 0) {
		yyerror("macro expansion too large");
	    } else {
		push(mc, strcpy(ALLOC(char, narg + 1), ppbuf), FALSE);
	    }
	    return 1;
	}
    }

    /* manifest constant, or function-like macro without arguments */
    if (mc->replace != (char *) NULL) {
	push(mc, mc->replace, FALSE);
    } else {
	push(mc, special_replace(mc->chain.name), FALSE);
    }

    return 1;
}
