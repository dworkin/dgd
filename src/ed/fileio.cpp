/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2021 DGD Authors (see the commit log for details)
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
# include "ed.h"
# include "buffer.h"
# include "path.h"
# include "fileio.h"

/*
 * The file I/O operations of the editor.
 */

static IO *iostat;		/* I/O status */

/*
 * read a line from the input, return as '\0'-terminated string
 * without '\n'
 */
char *IO::get_line()
{
    char c, *p, *bp;
    int i;

    if (iostat->ill) {
	/* previous line was incomplete, therefore the last */
	return (char *) NULL;
    }

    p = iostat->lbuf;
    bp = iostat->bufp;
    i = iostat->inbuf;
    do {
	if (i == 0) {	/* buffer empty */
	    i = P_read(iostat->ffd, iostat->buffer, BUF_SIZE);
	    if (i <= 0) {
		/* eof or error */
		if (i < 0) {
		    EDC->error("error while reading file \"/%s\"",
			       iostat->filename);
		}
		if (p == iostat->lbuf) {
		    return (char *) NULL;
		} else {
		    p++;	/* make room for terminating '\0' */
		    iostat->ill = TRUE;
		    break;
		}
	    }
	    bp = iostat->buffer;
	}
	--i;
	c = *bp++;
	if (c == '\0') {
	    iostat->zero++;	/* skip zeroes */
	} else {
	    if (p == iostat->lbuflast && c != LF) {
		iostat->split++;
		i++;
		--bp;
		c = LF;
	    }
	    *p++ = c;
	}
    } while (c != LF);	/* eoln */

    iostat->lines++;
    iostat->chars += p - iostat->lbuf;	/* including terminating '\0' */
    iostat->bufp = bp;
    iostat->inbuf = i;
    *--p = '\0';
    return iostat->lbuf;
}

/*
 * append block read from file after a line
 */
bool IO::load(EditBuf *eb, char *fname, Int l)
{
    char b[MAX_LINE_SIZE], buf[BUF_SIZE];
    struct stat sbuf;

    /* open file */
    if (PM->edRead(filename, fname) == (char *) NULL ||
	P_stat(filename, &sbuf) < 0 || (sbuf.st_mode & S_IFMT) != S_IFREG) {
	return FALSE;
    }

    ffd = P_open(filename, O_RDONLY | O_BINARY, 0);
    if (ffd < 0) {
	return FALSE;
    }

    /* initialize buffers */
    buffer = buf;
    inbuf = 0;
    lbuf = b;
    lbuflast = &b[MAX_LINE_SIZE - 1];

    /* initialize statistics */
    lines = 0;
    chars = 0;
    zero = 0;
    split = 0;
    ill = FALSE;
    iostat = this;

    /* add the block to the edit buffer */
    try {
	EDC->push();
	eb->add(l, get_line);
	EDC->pop();
    } catch (const char*) {
	P_close(ffd);
	EDC->error((char *) NULL);	/* pass on error */
    }
    P_close(ffd);

    return TRUE;
}

/*
 * write a line to a file
 */
void IO::put_line(const char *text)
{
    unsigned int len;

    len = strlen(text);
    iostat->lines += 1;
    iostat->chars += len + 1;
    while (iostat->inbuf + len >= BUF_SIZE) {	/* flush buffer */
	if (iostat->inbuf != BUF_SIZE) { /* room left for a piece of line */
	    unsigned int chunk;

	    chunk = BUF_SIZE - iostat->inbuf;
	    memcpy(iostat->buffer + iostat->inbuf, text, chunk);
	    text += chunk;
	    len -= chunk;
	}
	if (P_write(iostat->ffd, iostat->buffer, BUF_SIZE) != BUF_SIZE) {
	    EDC->error("error while writing file \"/%s\"", iostat->filename);
	}
	iostat->inbuf = 0;
    }
    if (len > 0) {			/* piece of line left */
	memcpy(iostat->buffer + iostat->inbuf, text, len);
	iostat->inbuf += len;
    }
    iostat->buffer[iostat->inbuf++] = LF;
}

/*
 * write a range of lines to a file
 */
bool IO::save(EditBuf *eb, char *fname, Int first, Int last, int append)
{
    char buf[BUF_SIZE];
    struct stat sbuf;

    if (PM->edWrite(filename, fname) == (char *) NULL ||
	(P_stat(filename, &sbuf) >= 0 && (sbuf.st_mode & S_IFMT) != S_IFREG))
    {
	return FALSE;
    }
    /* create file */
    ffd = P_open(filename,
		 (append) ? O_CREAT | O_APPEND | O_WRONLY | O_BINARY :
			    O_CREAT | O_TRUNC | O_WRONLY | O_BINARY,
	      0664);
    if (ffd < 0) {
	return FALSE;
    }

    /* initialize buffer */
    buffer = buf;
    inbuf = 0;

    /* initialize statistics */
    lines = 0;
    chars = 0;
    zero = 0;
    split = 0;
    ill = FALSE;
    iostat = this;

    /* write range */
    try {
	EDC->push();
	eb->range(first, last, put_line, FALSE);
	if (P_write(ffd, buffer, inbuf) != inbuf) {
	    EDC->error("error while writing file \"/%s\"", filename);
	}
	EDC->pop();
    } catch (const char*) {
	P_close(ffd);
	EDC->error((char *) NULL);	/* pass on error */
    }
    P_close(ffd);

    return TRUE;
}

/*
 * show statistics on the file just read/written
 */
void IO::show()
{
    EDC->message("%ld lines, %ld characters", (long) lines,
		 (long) (chars + zero - split - ill));
    if (zero > 0) {
	EDC->message(" [%ld zero]", (long) zero);
    }
    if (split > 0) {
	EDC->message(" [%ld split]", (long) split);
    }
    if (ill) {
	EDC->message(" [incomplete last line]");
    }
    EDC->message("\012");	/* LF */
}
