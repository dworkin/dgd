# define INCLUDE_FILE_IO
# define INCLUDE_CTYPE
# include "lex.h"
# include "path.h"
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
    struct _tchunk_ *next;	/* next in list */
    tbuf t[TCHUNKSZ];		/* chunk of token buffers */
} tchunk;

char *yytext;			/* for strings and identifiers */
static char *yytext1, *yytext2;	/* internal buffers */
static char *yyend;		/* end of current buffer */
int yyleng;			/* length of token */
long yynumber;			/* integer constant */
xfloat yyfloat;			/* floating point constant */

static tchunk *tlist;		/* list of token buffer chunks */
static int tchunksz;		/* token buffer chunk size */
static tbuf *flist;		/* free token buffer list */
static tbuf *tbuffer;		/* current token buffer */
static tbuf *ibuffer;		/* current input buffer */
static int pp_level;		/* the recursive preprocesing level */
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
    do_include = FALSE;
}

/*
 * NAME:	push()
 * DESCRIPTION:	Push a buffer on the token input stream. If eof is false, then
 *		the buffer will automatically be dropped when all is read.
 */
static void push(mc, buffer, buflen, eof)
macro *mc;
char *buffer;
unsigned int buflen;
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
    tb->inbuf = buflen;
    tb->up = tb->ubuf;
    tb->eof = eof;
    tb->fd = -2;
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
    if (tb->fd < -1) {
	if (tb->u.mc != (macro *) NULL) {
	    if (tb->u.mc->narg > 0) {
		/* in the buffer a function-like macro has been expanded */
		FREE(tb->buffer);
	    }
	}
    } else {
	if (tb->fd >= 0) {
	    P_close(tb->fd);
	    FREE(tb->buffer);
	}
	ibuffer = tbuffer->prev;
	FREE(tb->u.filename);
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
    tlist = (tchunk *) NULL;
    if (yytext1 != (char *) NULL) {
	FREE(yytext2);
	FREE(yytext1);
	yytext1 = (char *) NULL;
	yytext2 = (char *) NULL;
    }
}

/*
 * NAME:	token->include()
 * DESCRIPTION:	push a file on the input stream
 */
bool tk_include(file, buf, len)
char *file, *buf;
register unsigned int len;
{
    int fd;

    if (file != (char *) NULL) {
	if (buf == (char *) NULL) {
	    struct stat sbuf;

	    /* read from file */
	    fd = P_open(file, O_RDONLY | O_BINARY, 0);
	    if (fd < 0) {
		return FALSE;
	    }

	    P_fstat(fd, &sbuf);
	    if ((sbuf.st_mode & S_IFMT) != S_IFREG) {
		/* no source this */
		P_close(fd);
		return FALSE;
	    }
					 
	    push((macro *) NULL, ALLOC(char, BUF_SIZE), 0, TRUE);
	} else {
	    /* read from string */
	    fd = -1;
	    push((macro *) NULL, buf, len, TRUE);
	}

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

    return FALSE;
}

/*
 * NAME:	token->endinclude()
 * DESCRIPTION:	end an #inclusion
 */
void tk_endinclude()
{
    pop();
    seen_nl = TRUE;
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
unsigned int line;
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
    register unsigned int len;

    len = strlen(file);
    if (len >= STRINGSZ) {
	len = STRINGSZ - 1;
    }
    ibuffer->u.filename = memcpy(REALLOC(ibuffer->u.filename, char, 0, len + 1),
				 file, len);
    ibuffer->u.filename[len] = '\0';
}

/*
 * NAME:	token->header()
 * DESCRIPTION:	set the current include string mode. if TRUE, '<' will be
 *		specially processed.
 */
void tk_header(incl)
int incl;
{
    do_include = incl;
}

/*
 * NAME:	token->setpp()
 * DESCRIPTION:	if the argument is true, do not translate escape sequences in
 *		strings, and don't report errors.
 */
void tk_setpp(pp)
int pp;
{
    pp_level = (int) pp;
}

# define uc(c)	do { \
		    if ((c) != EOF) { \
			if ((c) == LF && tbuffer == ibuffer) ibuffer->line--; \
			*(tbuffer->up)++ = (c); \
		    } \
		} while (FALSE)

/*
 * NAME:	gc()
 * DESCRIPTION:	get a character from the input
 */
static int gc()
{
    register tbuf *tb;
    register int c;
    register bool backslash;

    tb = tbuffer;
    backslash = FALSE;

    for (;;) {
	if (tb->up != tb->ubuf) {
	    /* get a character from unget buffer */
	    c = UCHAR(*--(tb->up));
	} else {
	    if (tb->inbuf <= 0) {
		/* Current input buffer is empty. Try a refill. */
		if (tb->fd >= 0 &&
		    (tb->inbuf = P_read(tb->fd, tb->buffer, BUF_SIZE)) > 0) {
		    tb->p = tb->buffer;
		} else if (backslash) {
		    return '\\';
		} else if (tb->eof) {
		    return EOF;
		} else {
		    /* otherwise, pop the current token input buffer */
		    pop();
		    tb = tbuffer;
		    continue;
		}
	    }
	    tb->inbuf--;
	    c = UCHAR(*(tb->p)++);
	}

	if (c == LF && tb == ibuffer) {
	    ibuffer->line++;
	    if (!backslash) {
		return c;
	    }
	    backslash = FALSE;
	} else if (backslash) {
	    uc(c);
	    return '\\';
	} else if (c == '\\' && tb == ibuffer) {
	    backslash = TRUE;
	} else {
	    return c;
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
		    error("EOF in comment");
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
	} while (c == ' ' || c == HT || c == VT || c == FF || c == CR);

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
 * NAME:	token->esc()
 * DESCRIPTION:	handle an escaped character, leaving the value in yynumber
 */
static char *tk_esc(p)
register char *p;
{
    register int c, i, n;

    switch (c = *p++ = gc()) {
    case 'a': c = BEL; break;
    case 'b': c = BS; break;
    case 't': c = HT; break;
    case 'n': c = LF; break;
    case 'v': c = VT; break;
    case 'f': c = FF; break;
    case 'r': c = CR; break;

    case LF:
	/* newline in string or character constant */
	uc(c);
	return p - 1;

    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
	/* octal constant */
	i = 0;
	n = 3;
	--p;
	do {
	    *p++ = c;
	    i <<= 3;
	    i += c - '0';
	    c = gc();
	} while (--n > 0 && c >= '0' && c <= '7');
	uc(c);
	c = UCHAR(i);
	break;

    case 'x':
	/* hexadecimal constant */
	c = gc();
	if (isxdigit(c)) {
	    i = 0;
	    n = 3;
	    do {
		*p++ = c;
		i <<= 4;
		if (isdigit(c)) {
		    i += c - '0';
		} else {
		    i += toupper(c) + 10 - 'A';
		}
		c = gc();
	    } while (--n > 0 && isxdigit(c));
	} else {
	    i = 'x';
	}
	uc(c);
	c = UCHAR(i);
	break;
    }

    yynumber = c;
    return p;
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
    register int c, n;

    p = yytext;
    if (pp_level > 0) {
	/* keep the quotes if not on top level */
	p++;
	n = 0;
    } else {
	n = 2;
    }

    for (;;) {
	c = gc();
	if (c == quote) {
	    if (pp_level > 0) {
		/* keep the quotes if not on top level */
		*p++ = c;
	    }
	    break;
	} else if (c == '\\') {
	    if (pp_level > 0 || do_include) {
		/* recognize, but do not translate escape sequence */
		*p++ = c;
		p = tk_esc(p);
		c = *--p;
	    } else {
		/* translate escape sequence */
		n += tk_esc(p) - p;
		c = yynumber;
	    }
	} else if (c == LF || c == EOF) {
	    if (pp_level == 0) {
		error("unterminated string");
	    }
	    uc(c);
	    break;
	}
	*p++ = c;
	if (p > yyend - 4) {
	    n += p - (yyend - 4);
	    p = yyend - 4;
	}
    }

    if (pp_level == 0 && p + n > yyend - 4) {
	error("string too long");
    }
    *p = '\0';
    yyleng = p - yytext;
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
    register bool overflow;
    bool is_float, badoctal;

# define TEST(x, tok)	if (c == x) { c = tok; break; }
# define CHECK(x, tok)	*p++ = c = gc(); TEST(x, tok); --p; uc(c)

    result = 0;
    overflow = FALSE;
    is_float = FALSE;
    yytext = (yytext == yytext1) ? yytext2 : yytext1;
    yyend = yytext + MAX_LINE_SIZE - 1;
    p = yytext;
    *p++ = c = gc();
    switch (c) {
    case LF:
	if (tbuffer == ibuffer) {
	    seen_nl = TRUE;
	    *p = '\0';
	    return c;
	}
	c = (pp_level > 0) ? MARK : ' ';
	break;

    case HT:
	if (tbuffer != ibuffer) {
	    /* expanding a macro: keep separator */
	    break;
	}
	/* fall through */
    case ' ':
    case VT:
    case FF:
    case CR:
	/* white space */
	do {
	    c = gc();
	} while (c == ' ' || (c == HT && tbuffer == ibuffer) || c == VT ||
		 c == FF || c == CR);

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
	yyleng = 1;
	*p = '\0';
	return p[-1] = ' ';

    case '!':
	CHECK('=', NE);
	c = '!';
	break;

    case '#':
	if (!seen_nl) {
	    CHECK('#', HASH_HASH);
	    c = HASH;
	}
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
	TEST('>', RARROW);
	TEST('-', MIN_MIN);
	TEST('=', MIN_EQ);
	--p; uc(c);
	c = '-';
	break;

    case '.':
	c = gc();
	if (isdigit(c)) {
	    /*
	     * Come here when a decimal '.' has been spotted; c holds the next
	     * character.
	     */
	fraction:
	    is_float = TRUE;
	    while (isdigit(c)) {
		if (p < yyend) {
		    *p++ = c;
		}
		c = gc();
	    }
	    if (c == 'e' || c == 'E') {
		char *q, exp, sign;

		/*
		 * Come here when 'e' or 'E' has been spotted after a number.
		 */
	    exponent:
		exp = c;
		sign = 0;
		q = p;
		if (p < yyend) {
		    *p++ = c;
		}
		c = gc();
		if (c == '+' || c == '-') {
		    if (p < yyend) {
			*p++ = c;
		    }
		    sign = c;
		    c = gc();
		}
		if (isdigit(c)) {
		    do {
			if (p < yyend) {
			    *p++ = c;
			}
			c = gc();
		    } while (isdigit(c));
		    is_float = TRUE;
		} else {
		    /*
		     * assume the e isn't part of this token
		     */
		    uc(c);
		    if (sign != 0) {
			uc(sign);
		    }
		    c = exp;
		    p = q;
		}
	    }
	    uc(c);

	    if (is_float) {
		yyfloat.high = 0;
		yyfloat.low = 0;
		if (pp_level == 0) {
		    if (p == yyend) {
			error("too long floating point constant");
		    } else {
			char *buf;

			*p = '\0';
			buf = yytext;
			if (!flt_atof(&buf, &yyfloat)) {
			    error("overflow in floating point constant");
			}
		    }
		}
		c = FLOAT_CONST;
	    } else {
		if (pp_level == 0) {
		    /* unclear if this was decimal or octal */
		    if (p == yyend) {
			error("too long integer constant");
		    } else if (overflow) {
			error("overflow in integer constant");
		    }
		}
		c = INT_CONST;
	    }
	    break;
	} else if (c == '.') {
	    *p++ = c;
	    CHECK('.', ELLIPSIS);
	    c = DOT_DOT;
	} else {
	    uc(c);
	    c = '.';
	}
	break;

    case '/':
	c = gc();
	if (c == '*') {
	    comment();
	    yyleng = 1;
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
	TEST('-', LARROW);
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
	badoctal = FALSE;
	c = gc();
	if (c == 'x' || c == 'X') {
	    *p++ = c;
	    c = gc();
	    if (isxdigit(c)) {
		do {
		    if (p < yyend) {
			*p++ = c;
		    }
		    if (result > 0x0fffffffL) {
			overflow = TRUE;
		    }
		    if (isdigit(c)) {
			c -= '0';
		    } else {
			c = toupper(c) + 10 - 'A';
		    }
		    result <<= 4;
		    result += c;
		    c = gc();
		} while (isxdigit(c));
	    } else {
		/* not a hexadecimal constant */
		uc(c);
		c = *--p;
	    }
	    yynumber = result;
	} else {
	    while (c >= '0' && c <= '9') {
		if (c >= '8') {
		    badoctal = TRUE;
		}
		if (p < yyend) {
		    *p++ = c;
		}
		if (result > 0x1fffffffL) {
		    overflow = TRUE;
		}
		result <<= 3;
		result += c - '0';
		c = gc();
	    }
	    yynumber = result;

	    if (c == '.') {
		if (p < yyend) {
		    *p++ = c;
		}
		c = gc();
		if (c != '.') {
		    goto fraction;
		}
		--p; uc(c);
	    } else if (c == 'e' || c == 'E') {
		goto exponent;
	    }
	}
	uc(c);
	if (pp_level == 0) {
	    if (p == yyend) {
		error("too long integer constant");
	    } else if (badoctal) {
		error("bad octal constant");
	    } else if (overflow) {
		error("overflow in integer constant");
	    }
	}
	c = INT_CONST;
	break;

    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
	for (;;) {
	    if (result >= 214748364L && (result > 214748364L || c >= '8')) {
		overflow = TRUE;
	    }
	    result *= 10;
	    result += c - '0';
	    c = gc();
	    if (!isdigit(c)) {
		break;
	    }
	    if (p < yyend) {
		*p++ = c;
	    }
	}
	yynumber = result;

	if (c == '.') {
	    if (p < yyend) {
		*p++ = c;
	    }
	    c = gc();
	    if (c != '.') {
		goto fraction;
	    }
	    --p; uc(c);
	}
	if (c == 'e' || c == 'E') {
	    goto exponent;
	}
	uc(c);
	if (pp_level == 0) {
	    if (p == yyend) {
		error("too long integer constant");
	    } else if (overflow) {
		error("overflow in integer constant");
	    }
	}
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
	    if (p < yyend) {
		*p++ = c;
	    }
	}
	uc(c);
	if (pp_level == 0 && p == yyend) {
	    error("too long identifier");
	}
	c = IDENTIFIER;
	break;

    case '\'':
	*p++ = c = gc();
	if (c == '\'') {
	    if (pp_level == 0) {
		error("too short character constant");
	    }
	} else if (c == LF || c == EOF) {
	    if (pp_level == 0) {
		error("unterminated character constant");
	    }
	    uc(c);
	} else {
	    if (c == '\\') {
		p = tk_esc(p);
	    } else {
		yynumber = c;
	    }
	    *p++ = c = gc();
	    if (c != '\'') {
		if (pp_level == 0) {
		    error("illegal character constant");
		}
		uc(c);
	    }
	}
	c = INT_CONST;
	break;

    case '"':
	seen_nl = FALSE;
	return tk_string('"');
    }
    *p = '\0';
    yyleng = p - yytext;
    seen_nl = FALSE;

    return c;
}

/*
 * NAME:	token->skiptonl()
 * DESCRIPTION:	skip tokens until a newline or EOF is found. If the argument is
 *		TRUE, only whitespace is allowed.
 */
void tk_skiptonl(ws)
int ws;
{
    pp_level++;
    for (;;) {
	switch (tk_gettok()) {
	case EOF:
	    error("unterminated line");
	    --pp_level;
	    return;

	case LF:
	    --pp_level;
	    return;

	case ' ':
	case HT:
	    break;

	default:
	    if (ws) {
		error("bad token in control");
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
	if (token == LF) {
	    return -1;
	}
	uc(token);

	tb = tbuffer;
	do {
	    if (tb->fd < -1 && tb->u.mc != (macro *) NULL &&
	      strcmp(mc->chain.name, tb->u.mc->chain.name) == 0) {
		return -1;
	    }
	    tb = tb->prev;
	} while (tb != ibuffer);
    }

    if (mc->narg >= 0) {
	char *args[MAX_NARG], *arg, ppbuf[MAX_REPL_SIZE];
	register int narg;
	register str *s;
	unsigned short startline, line;
	int errcount;

	startline = ibuffer->line;

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
	} while (token == ' ' || token == HT || token == LF);

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
	} while (token == ' ' || token == HT || token == LF);

	if (token != ')' || mc->narg != 0) {
	    int paren;
	    bool seen_space, seen_sep;

	    paren = 0;
	    seen_space = FALSE;
	    seen_sep = FALSE;

	    for (;;) {
		if (token == EOF) {	/* sigh */
		    line = ibuffer->line;
		    ibuffer->line = startline;
		    error("EOF in macro call");
		    ibuffer->line = line;
		    errcount++;
		    break;
		}

		if ((token == ',' || token == ')') && paren == 0) {
		    if (s->len < 0) {
			line = ibuffer->line;
			ibuffer->line = startline;
			error("macro argument too long");
			ibuffer->line = line;
			errcount++;
		    } else if (narg < mc->narg) {
			arg = ALLOCA(char, s->len + 1);
			args[narg] = strcpy(arg, ppbuf);
		    }
		    narg++;
		    if (token == ')') {
			break;
		    }

		    s->len = 0;

		    do {
			token = tk_gettok();
		    } while (token == ' ' || token == HT || token == LF);
		    seen_space = FALSE;
		    seen_sep = FALSE;
		} else {
		    if (seen_space) {
			pps_ccat(s, ' ');
			seen_space = FALSE;
			seen_sep = FALSE;
		    } else if (seen_sep) {
			pps_ccat(s, HT);
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
			if (token == ' ' || token == LF) {
			    seen_space = TRUE;
			} else if (token == HT) {
			    seen_sep = TRUE;
			} else {
			    break;
			}
		    }
		}
	    }
	}
	--pp_level;

	if (errcount == 0 && narg != mc->narg) {
	    error("macro argument count mismatch");
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
	    push((macro *) NULL, mc->replace, strlen(mc->replace), TRUE);
	    s->len = 0;

	    pp_level++;
	    while ((token=tk_gettok()) != EOF) {
		if (token == MARK) {	/* macro argument follows */
		    token = gc();
		    narg = token & MA_NARG;
		    if (token & MA_STRING) {
			register char *p;

			/* copy it, inserting \ before \ and " */
			push((macro *) NULL, args[narg], strlen(args[narg]),
			     TRUE);
			pps_ccat(s, '"');
			while ((token=tk_gettok()) != EOF) {
			    if (token != HT) {
				p = yytext;
				if (*p == '\'' || *p == '"') {
				    /* escape \ and " */
				    do {
					if (*p == '"' || *p == '\\') {
					    pps_ccat(s, '\\');
					}
					pps_ccat(s, *p++);
				    } while (*p != '\0');
				} else {
				    /* just add token */
				    pps_scat(s, yytext);
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
			if (s->len > 0 && ppbuf[s->len - 1] == LF) {
			    s->len--;
			}

			push((macro *) NULL, args[narg], strlen(args[narg]),
			     TRUE);
			token = tk_gettok();
			/*
			 * if the first token of the argument is a
			 * not-to-expand macro, make it a normal identifier
			 */
			if (token == IDENTIFIER && (narg=gc()) != LF) {
			    uc(narg);
			}
			while (token != EOF) {
			    pps_scat(s, yytext);
			    token = tk_gettok();
			}
			pop();
		    } else {

			/* preprocess the argument */
			push((macro *) NULL, args[narg], strlen(args[narg]),
			     TRUE);
			while ((token=tk_gettok()) != EOF) {
			    if (token == IDENTIFIER) {
				macro *m;

				if ((m=mc_lookup(yytext)) != (macro *) NULL) {
				    token = tk_expand(m);
				    if (token > 0) {
					continue;
				    }
				    if (token < 0) {
					pps_scat(s, yytext);
					pps_ccat(s, LF);
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
		error("macro expansion too large");
	    } else {
		push(mc, strcpy(ALLOC(char, narg + 1), ppbuf), narg, FALSE);
	    }
	    return 1;
	}
    }

    /* manifest constant, or function-like macro without arguments */
    if (mc->replace != (char *) NULL) {
	push(mc, mc->replace, strlen(mc->replace), FALSE);
    } else {
	char *p;

	p = special_replace(mc->chain.name);
	push(mc, p, strlen(p), FALSE);
    }

    return 1;
}
