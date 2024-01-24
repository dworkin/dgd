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

class TokenBuf : public ChunkAllocated {
public:
    static void init();
    static void clear();
    static void push(char *buffer, unsigned int buflen);
    static bool include(char *file, char *buffer, unsigned int buflen);
    static void endinclude();
    static unsigned short line();
    static char *filename();
    static void setline(unsigned short line);
    static void setfilename(char *file);
    static void header(bool incl);
    static void setpp(int pp);
    static int gettok();
    static void skiptonl(int ws);
    static int expand(Macro *mc);

private:
    void pop();

    static void push(Macro *mc, char *buffer, unsigned int buflen, bool eof);
    static int gc();
    static void skip_comment();
    static void skip_alt_comment();
    static void comment(bool flag);
    static char *esc(char *p);
    static int string(char quote);

    char *buffer;		/* token buffer */
    char *p;			/* token buffer pointer */
    int inbuf;			/* # chars in token buffer */
    char ubuf[4];		/* unget buffer */
    char *up;			/* unget buffer pointer */
    bool file;			/* file buffer? */
    bool eof;			/* TRUE if empty(buffer) -> EOF */
    unsigned short _line;	/* line number */
    int fd;			/* file descriptor */
    union {
	char *_filename;	/* file name */
	Macro *mc;		/* macro this buffer is an expansion of */
    };
    TokenBuf *prev;		/* previous token buffer */
    TokenBuf *iprev;		/* previous token ibuffer */
};
