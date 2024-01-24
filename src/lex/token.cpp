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

# define INCLUDE_FILE_IO
# define INCLUDE_CTYPE
# include "lex.h"
# include "path.h"
# include "macro.h"
# include "special.h"
# include "ppstr.h"
# include "token.h"
# include "ppcontrol.h"

/*
 * The functions for getting a (possibly preprocessed) token from the input
 * stream.
 */

# define TCHUNKSZ	8

static Chunk<TokenBuf, TCHUNKSZ> tchunk;

char *yytext;			/* for strings and identifiers */
static char *yytext1, *yytext2;	/* internal buffers */
static char *yyend;		/* end of current buffer */
int yyleng;			/* length of token */
LPCint yynumber;		/* integer constant */
Float yyfloat;			/* floating point constant */

static TokenBuf *tbuffer;	/* current token buffer */
static TokenBuf *ibuffer;	/* current input buffer */
static int pp_level;		/* the recursive preprocesing level */
static bool do_include;		/* treat < and strings specially */
static bool seen_nl;		/* just seen a newline */

/*
 * initialize the new token input buffer
 */
void TokenBuf::init()
{
    yytext1 = ALLOC(char, MAX_LINE_SIZE);
    yytext2 = ALLOC(char, MAX_LINE_SIZE);
    tbuffer = (TokenBuf *) NULL;
    ibuffer = (TokenBuf *) NULL;
    pp_level = 0;
    do_include = FALSE;
}

/*
 * Push a buffer on the token input stream. If eof is false, then
 * the buffer will automatically be dropped when all is read.
 */
void TokenBuf::push(Macro *mc, char *buffer, unsigned int buflen, bool eof)
{
    TokenBuf *tb;

    tb = chunknew (tchunk) TokenBuf;
    tb->p = tb->buffer = buffer;
    tb->inbuf = buflen;
    tb->up = tb->ubuf;
    tb->file = FALSE;
    tb->eof = eof;
    tb->fd = -2;
    tb->mc = mc;
    tb->prev = tbuffer;
    tb->iprev = NULL;
    tbuffer = tb;
}

/*
 * push a copied buffer on the input stream
 */
void TokenBuf::push(char *buffer, unsigned int buflen)
{
    push((Macro *) NULL,
	 (char *) memcpy(ALLOC(char, buflen + 1), buffer, buflen),
	 buflen, FALSE);
    tbuffer->file = TRUE;
    tbuffer->fd = -1;

}

/*
 * Drop the current token input buffer. If the associated macro
 * is function-like, the token buffer will have to be deallocated.
 */
void TokenBuf::pop()
{
    if (fd < -1) {
	if (mc != (Macro *) NULL) {
	    if (mc->narg > 0) {
		/* in the buffer a function-like macro has been expanded */
		FREE(buffer);
	    }
	}
    } else {
	if (fd >= 0) {
	    P_close(fd);
	}
	if (_filename != (char *) NULL) {
	    FREE(_filename);
	    ibuffer = iprev;
	}
	FREE(buffer);
    }
    tbuffer = prev;

    delete this;
}

/*
 * clear all of the token input buffers
 */
void TokenBuf::clear()
{
    while (tbuffer != (TokenBuf *) NULL) {
	tbuffer->pop();
    }
    tchunk.clean();
    if (yytext1 != (char *) NULL) {
	FREE(yytext2);
	FREE(yytext1);
	yytext1 = (char *) NULL;
	yytext2 = (char *) NULL;
    }
}

/*
 * push a file on the input stream
 */
bool TokenBuf::include(char *file, char *buffer, unsigned int buflen)
{
    int fd;
    ssizet len;

    if (file != (char *) NULL) {
	if (buffer == (char *) NULL) {
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

	    push((Macro *) NULL, ALLOC(char, BUF_SIZE), 0, TRUE);
	} else {
	    /* read from copied buffer */
	    push((Macro *) NULL,
		 (char *) memcpy(ALLOC(char, buflen + 1), buffer, buflen),
		 buflen, TRUE);
	    fd = -1;
	}

	tbuffer->iprev = ibuffer;
	ibuffer = tbuffer;
	ibuffer->file = TRUE;
	ibuffer->fd = fd;
	len = strlen(file);
	if (len >= STRINGSZ - 1) {
	    len = STRINGSZ - 2;
	}
	ibuffer->_filename = ALLOC(char, len + 2);
	strncpy(ibuffer->_filename + 1, file, len);
	ibuffer->_filename[0] = '/';
	ibuffer->_filename[len + 1] = '\0';
	ibuffer->_line = 1;
	seen_nl = TRUE;

	return TRUE;
    }

    return FALSE;
}

/*
 * end an #inclusion
 */
void TokenBuf::endinclude()
{
    tbuffer->pop();
    seen_nl = TRUE;
}

/*
 * return the current line number (possibly adjusted)
 */
unsigned short TokenBuf::line()
{
    return ibuffer->_line - (unsigned short) seen_nl;
}

/*
 * return the current file name
 */
char *TokenBuf::filename()
{
    return ibuffer->_filename;
}

/*
 * set the current line number
 */
void TokenBuf::setline(unsigned short line)
{
    ibuffer->_line = line;
}

/*
 * set the current file name
 */
void TokenBuf::setfilename(char *file)
{
    unsigned int len;

    len = strlen(file);
    if (len >= STRINGSZ) {
	len = STRINGSZ - 1;
    }
    ibuffer->_filename = (char *) memcpy(REALLOC(ibuffer->_filename, char, 0,
						 len + 1),
					 file, len);
    ibuffer->_filename[len] = '\0';
}

/*
 * set the current include string mode. if TRUE, '<' will be
 * specially processed.
 */
void TokenBuf::header(bool incl)
{
    do_include = incl;
}

/*
 * if the argument is true, do not translate escape sequences in
 * strings, and don't report errors.
 */
void TokenBuf::setpp(int pp)
{
    pp_level = (int) pp;
}

# define uc(c)	{ \
		    if ((c) != EOF) { \
			if ((c) == LF && tbuffer->file) ibuffer->_line--; \
			*(tbuffer->up)++ = (c); \
		    } \
		}

/*
 * get a character from the input
 */
int TokenBuf::gc()
{
    TokenBuf *tb;
    int c;
    bool backslash;

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
		} else if (tb->eof) {
		    if (backslash) {
			return '\\';
		    }
		    return EOF;
		} else {
		    /* otherwise, pop the current token input buffer */
		    tbuffer->pop();
		    tb = tbuffer;
		    continue;
		}
	    }
	    tb->inbuf--;
	    c = UCHAR(*(tb->p)++);
	}

	if (c == LF && tb->file) {
	    ibuffer->_line++;
	    if (!backslash) {
		return c;
	    }
	    backslash = FALSE;
	} else if (backslash) {
	    uc(c);
	    return '\\';
	} else if (c == '\\' && tb->file) {
	    backslash = TRUE;
	} else {
	    return c;
	}
    }
}

/*
 * skip a single comment
 */
void TokenBuf::skip_comment()
{
    int c;

    do {
	do {
	    c = gc();
	    if (c == EOF) {
		PP->error("EOF in comment");
		return;
	    }
	    if (c == LF) {
		seen_nl = TRUE;
	    }
	} while (c != '*');

	do {
	    c = gc();
	} while (c == '*');
    } while (c != '/');
}

/*
 * skip c++ style comment
 */
void TokenBuf::skip_alt_comment()
{
    int c;

    do {
	c = gc();
	if (c == EOF) {
	    return;
	}
    } while (c != LF);
    uc(c);
}

/*
 * skip comments and white space
 */
void TokenBuf::comment(bool flag)
{
    int c;

    for (;;) {
	/* first skip the current comment */
	if (flag) {
	   skip_alt_comment();
	} else {
	   skip_comment();
	}

	/* skip any whitespace */
	do {
	    c = gc();
	} while (c == ' ' || c == HT || c == VT || c == FF || c == CR);

	/* check if a new comment follows */
	if (c != '/') {
	    uc(c);
	    break;
	}
	c = gc();
	if (c == '*') {
	    flag = FALSE;
# ifdef SLASHSLASH
	} else if (c == '/') {
	    flag = TRUE;
# endif
	} else {
	    uc(c);
	    c = '/';
	    uc(c);
	    break;
	}
    }
}

/*
 * handle an escaped character, leaving the value in yynumber
 */
char *TokenBuf::esc(char *p)
{
    int c, i, n;

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
	    n = 2;
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
 * handle a string. If pp_level > 0, don't translate escape
 * sequences.
 */
int TokenBuf::string(char quote)
{
    char *p;
    int c, n;

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
		p = esc(p);
		c = *--p;
	    } else {
		/* translate escape sequence */
		n += esc(p) - p;
		c = yynumber;
	    }
	} else if (c == LF || c == EOF) {
	    if (pp_level == 0) {
		PP->error("unterminated string");
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
	PP->error("string too long");
    }
    *p = '\0';
    yyleng = p - yytext;
    return (quote == '>') ? INCL_CONST : STRING_CONST;
}

/*
 * get a token from the input stream.
 */
int TokenBuf::gettok()
{
    int c;
    LPCint result;
    char *p;
    bool overflow;
    bool is_float, badoctal;

# define TEST(x, tok)	if (c == x) { c = tok; break; }
# define CHECK(x, tok)	c = gc(); *p++ = c; TEST(x, tok); --p; uc(c)

    result = 0;
    overflow = FALSE;
    is_float = FALSE;
    yytext = (yytext == yytext1) ? yytext2 : yytext1;
    yyend = yytext + MAX_LINE_SIZE - 1;
    p = yytext;
    c = gc();
    *p++ = c;
    switch (c) {
    case LF:
	if (tbuffer->file) {
	    seen_nl = TRUE;
	    *p = '\0';
	    return c;
	}
	c = (pp_level > 0) ? MARK : ' ';
	break;

    case HT:
	if (!tbuffer->file) {
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
	} while (c == ' ' || (c == HT && tbuffer->file) || c == VT ||
		 c == FF || c == CR);

	/* check for comment after white space */
	if (c == '/') {
	    c = gc();
	    if (c == '*') {
		comment(FALSE);
# ifdef SLASHSLASH
	    } else if (c == '/') {
		comment(TRUE);
# endif
	    } else {
		uc(c);
		c = '/';
		uc(c);
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
	c = gc();
	*p++ = c;
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
	c = gc();
	*p++ = c;
	TEST('+', PLUS_PLUS);
	TEST('=', PLUS_EQ);
	--p; uc(c);
	c = '+';
	break;

    case '-':
	c = gc();
	*p++ = c;
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
		char *q, exp;
		int sign;

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
			PP->error("too long floating point constant");
		    } else {
			char *buf;

			*p = '\0';
			buf = yytext;
			if (!Float::atof(&buf, &yyfloat)) {
			    PP->error("overflow in floating point constant");
			}
		    }
		}
		c = FLOAT_CONST;
	    } else {
		if (pp_level == 0) {
		    /* unclear if this was decimal or octal */
		    if (p == yyend) {
			PP->error("too long integer constant");
		    } else if (overflow) {
			PP->error("overflow in integer constant");
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
	    comment(FALSE);
	    yyleng = 1;
	    *p = '\0';
	    return p[-1] = ' ';
# ifdef SLASHSLASH
	} else if (c == '/') {
	    comment(TRUE);
	    yyleng = 1;
	    *p = '\0';
	    return p[-1] = ' ';
# endif
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
	    return string('>');
	}
	c = gc();
	*p++ = c;
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
	c = gc();
	*p++ = c;
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
	c = gc();
	*p++ = c;
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
		    if (result > ((LPCuint) LPCUINT_MAX >> 4)) {
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
		if (result > ((LPCuint) LPCUINT_MAX >> 3)) {
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
		PP->error("too long integer constant");
	    } else if (badoctal) {
		PP->error("bad octal constant");
	    } else if (overflow) {
		PP->error("overflow in integer constant");
	    }
	}
	c = INT_CONST;
	break;

    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
	for (;;) {
	    if (result >= LPCINT_MAX / 10 &&
		(result > LPCINT_MAX / 10 || c >= '8')) {
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
		PP->error("too long integer constant");
	    } else if (overflow) {
		PP->error("overflow in integer constant");
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
	    PP->error("too long identifier");
	}
	c = IDENTIFIER;
	break;

    case '\'':
	c = gc();
	*p++ = c;
	if (c == '\'') {
	    if (pp_level == 0) {
		PP->error("too short character constant");
	    }
	} else if (c == LF || c == EOF) {
	    if (pp_level == 0) {
		PP->error("unterminated character constant");
	    }
	    uc(c);
	} else {
	    if (c == '\\') {
		p = esc(p);
	    } else {
		yynumber = c;
	    }
	    c = gc();
	    *p++ = c;
	    if (c != '\'') {
		if (pp_level == 0) {
		    PP->error("illegal character constant");
		}
		uc(c);
	    }
	}
	c = INT_CONST;
	break;

    case '"':
	seen_nl = FALSE;
	return string('"');
    }
    *p = '\0';
    yyleng = p - yytext;
    seen_nl = FALSE;

    return c;
}

/*
 * skip tokens until a newline or EOF is found. If the argument is
 * TRUE, only whitespace is allowed.
 */
void TokenBuf::skiptonl(int ws)
{
    pp_level++;
    for (;;) {
	switch (gettok()) {
	case EOF:
	    PP->error("unterminated line");
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
		PP->error("bad token in control");
		ws = FALSE;
	    }
	    break;
	}
    }
}

/*
 * expand a macro, pushing it on the input stream
 * return: -1 if the macro is nested and is not expanded
 *	   0 if the macro is ftn-like and the call isn't
 *	   1 if the macro was expanded
 */
int TokenBuf::expand(Macro *mc)
{
    int token;

    if (!tbuffer->file) {
	TokenBuf *tb;

	token = gc();
	if (token == LF) {
	    return -1;
	}
	uc(token);

	tb = tbuffer;
	do {
	    if (tb->fd < -1 && tb->mc != (Macro *) NULL &&
	      strcmp(mc->name, tb->mc->name) == 0) {
		return -1;
	    }
	    tb = tb->prev;
	} while (!tb->file);
    }

    if (mc->narg >= 0) {
	char *args[MAX_NARG], *arg, ppbuf[MAX_REPL_SIZE];
	int narg;
	Str *s;
	unsigned short startline, line;
	int errcount;

	startline = ibuffer->_line;

	do {
	    token = gc();
	    if (token == '/') {
		token = gc();
		if (token == '*') {
		    comment(FALSE);
		    token = gc();
# ifdef SLASHSLASH
		} else if (token == '/') {
		    comment(TRUE);
		    token = gc();
# endif
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
	s = Str::create(ppbuf, sizeof(ppbuf));
	do {
	    token = gettok();
	} while (token == ' ' || token == HT || token == LF);

	if (token != ')' || mc->narg != 0) {
	    int paren;
	    bool seen_space, seen_sep;

	    paren = 0;
	    seen_space = FALSE;
	    seen_sep = FALSE;

	    for (;;) {
		if (token == EOF) {	/* sigh */
		    line = ibuffer->_line;
		    ibuffer->_line = startline;
		    PP->error("EOF in macro call");
		    ibuffer->_line = line;
		    errcount++;
		    break;
		}

		if ((token == ',' || token == ')') && paren == 0) {
		    if (s->len < 0) {
			line = ibuffer->_line;
			ibuffer->_line = startline;
			PP->error("macro argument too long");
			ibuffer->_line = line;
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
			token = gettok();
		    } while (token == ' ' || token == HT || token == LF);
		    seen_space = FALSE;
		    seen_sep = FALSE;
		} else {
		    if (seen_space) {
			s->append(' ');
			seen_space = FALSE;
			seen_sep = FALSE;
		    } else if (seen_sep) {
			s->append(HT);
			seen_sep = FALSE;
		    }
		    s->append(yytext);
		    if (token == '(') {
			paren++;
		    } else if (token == ')') {
			--paren;
		    }

		    for (;;) {
			token = gettok();
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
	    PP->error("macro argument count mismatch");
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
	    delete s;
	    return 1;	/* skip this macro */
	}

	if (narg > 0) {
	    push((Macro *) NULL, mc->replace, strlen(mc->replace), TRUE);
	    s->len = 0;

	    pp_level++;
	    while ((token=gettok()) != EOF) {
		if (token == MARK) {	/* macro argument follows */
		    token = gc();
		    narg = token & MA_NARG;
		    if (token & MA_STRING) {
			char *p;

			/* copy it, inserting \ before \ and " */
			push((Macro *) NULL, args[narg], strlen(args[narg]),
			     TRUE);
			s->append('"');
			while ((token=gettok()) != EOF) {
			    if (token != HT) {
				p = yytext;
				if (*p == '\'' || *p == '"') {
				    /* escape \ and " */
				    do {
					if (*p == '"' || *p == '\\') {
					    s->append('\\');
					}
					s->append(*p++);
				    } while (*p != '\0');
				} else {
				    /* just add token */
				    s->append(yytext);
				}
			    }
			}
			s->append('"');
			tbuffer->pop();
		    } else if (token & MA_NOEXPAND) {

			/*
			 * if the previous token was a not-to-expand macro,
			 * make it a normal identifier
			 */
			if (s->len > 0 && ppbuf[s->len - 1] == LF) {
			    s->len--;
			}

			push((Macro *) NULL, args[narg], strlen(args[narg]),
			     TRUE);
			token = gettok();
			/*
			 * if the first token of the argument is a
			 * not-to-expand macro, make it a normal identifier
			 */
			if (token == IDENTIFIER && (narg=gc()) != LF) {
			    uc(narg);
			}
			while (token != EOF) {
			    s->append(yytext);
			    token = gettok();
			}
			tbuffer->pop();
		    } else {

			/* preprocess the argument */
			push((Macro *) NULL, args[narg], strlen(args[narg]),
			     TRUE);
			while ((token=gettok()) != EOF) {
			    if (token == IDENTIFIER) {
				Macro *m;

				if ((m=Macro::lookup(yytext)) != (Macro *) NULL)
				{
				    token = expand(m);
				    if (token > 0) {
					continue;
				    }
				    if (token < 0) {
					s->append(yytext);
					s->append(LF);
					continue;
				    }
				}
			    }
			    s->append(yytext);
			}
			tbuffer->pop();
		    }
		} else {
		    /* copy this token */
		    s->append(yytext);
		}
	    }
	    --pp_level;
	    tbuffer->pop();

	    /* cleanup */
	    narg = mc->narg;
	    do {
		--narg;
		AFREE(args[narg]);
	    } while (narg > 0);

	    narg = s->len;	/* so s can be deleted before the push */
	    delete s;
	    if (narg < 0) {
		PP->error("macro expansion too large");
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

	p = Special::replace(mc->name);
	push(mc, p, strlen(p), FALSE);
    }

    return 1;
}
