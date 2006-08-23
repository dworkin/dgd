# define INCLUDE_FILE_IO
# include "ed.h"
# include "buffer.h"
# include "path.h"
# include "fileio.h"

/*
 * The file I/O operations of the editor.
 */

typedef struct {
    int fd;			/* read/write file descriptor */
    char *buffer;		/* file buffer */
    char *bufp;			/* buffer pointer */
    unsigned int inbuf;		/* # bytes in buffer */
    char *lbuf;			/* line buffer */
    char *lbuflast;		/* end of line buffer */
    io *iostat;			/* I/O status */
    char filename[STRINGSZ];	/* file name */
} fiocontext;

/*
 * NAME:	get_line()
 * DESCRIPTION:	read a line from the input, return as '\0'-terminated string
 *		without '\n'
 */
static char *get_line(ptr)
char *ptr;
{
    register fiocontext *x;
    register char c, *p, *bp;
    register int i;

    x = (fiocontext *) ptr;

    if (x->iostat->ill) {
	/* previous line was incomplete, therefore the last */
	return (char *) NULL;
    }

    p = x->lbuf;
    bp = x->bufp;
    i = x->inbuf;
    do {
	if (i == 0) {	/* buffer empty */
	    i = P_read(x->fd, x->buffer, BUF_SIZE);
	    if (i <= 0) {
		/* eof or error */
		if (i < 0) {
		    error("error while reading file \"/%s\"", x->filename);
		}
		if (p == x->lbuf) {
		    return (char *) NULL;
		} else {
		    p++;	/* make room for terminating '\0' */
		    x->iostat->ill = TRUE;
		    break;
		}
	    }
	    bp = x->buffer;
	}
	--i;
	c = *bp++;
	if (c == '\0') {
	    x->iostat->zero++;	/* skip zeroes */
	} else {
	    if (p == x->lbuflast && c != LF) {
		x->iostat->split++;
		i++;
		--bp;
		c = LF;
	    }
	    *p++ = c;
	}
    } while (c != LF);	/* eoln */

    x->iostat->lines++;
    x->iostat->chars += p - x->lbuf;	/* including terminating '\0' */
    x->bufp = bp;
    x->inbuf = i;
    *--p = '\0';
    return x->lbuf;
}

/*
 * NAME:	io_load()
 * DESCRIPTION:	append block read from file after a line
 */
bool io_load(eb, filename, l, iobuf)
editbuf *eb;
char *filename;
Int l;
io *iobuf;
{
    char b[MAX_LINE_SIZE], buf[BUF_SIZE];
    struct stat sbuf;
    fiocontext x;

    /* open file */
    if (path_ed_read(x.filename, filename) == (char *) NULL ||
	P_stat(x.filename, &sbuf) < 0 || (sbuf.st_mode & S_IFMT) != S_IFREG) {
	return FALSE;
    }

    x.fd = P_open(x.filename, O_RDONLY | O_BINARY, 0);
    if (x.fd < 0) {
	return FALSE;
    }

    /* initialize buffers */
    x.buffer = buf;
    x.inbuf = 0;
    x.lbuf = b;
    x.lbuflast = &b[MAX_LINE_SIZE - 1];

    /* initialize statistics */
    x.iostat = iobuf;
    x.iostat->lines = 0;
    x.iostat->chars = 0;
    x.iostat->zero = 0;
    x.iostat->split = 0;
    x.iostat->ill = FALSE;

    /* add the block to the edit buffer */
    if (ec_push((ec_ftn) NULL)) {
	P_close(x.fd);
	error((char *) NULL);	/* pass on error */
    }
    eb_add(eb, l, get_line, (char *) &x);
    ec_pop();
    P_close(x.fd);

    return TRUE;
}

/*
 * NAME:	put_line()
 * DESCRIPTION:	write a line to a file
 */
static void put_line(ptr, text)
char *ptr;
register char *text;
{
    register fiocontext *x;
    register unsigned int len;

    x = (fiocontext *) ptr;
    len = strlen(text);
    x->iostat->lines += 1;
    x->iostat->chars += len + 1;
    while (x->inbuf + len >= BUF_SIZE) {	/* flush buffer */
	if (x->inbuf != BUF_SIZE) {	/* room left for a piece of line */
	    register unsigned int chunk;

	    chunk = BUF_SIZE - x->inbuf;
	    memcpy(x->buffer + x->inbuf, text, chunk);
	    text += chunk;
	    len -= chunk;
	}
	if (P_write(x->fd, x->buffer, BUF_SIZE) != BUF_SIZE) {
	    error("error while writing file \"/%s\"", x->filename);
	}
	x->inbuf = 0;
    }
    if (len > 0) {			/* piece of line left */
	memcpy(x->buffer + x->inbuf, text, len);
	x->inbuf += len;
    }
    x->buffer[x->inbuf++] = LF;
}

/*
 * NAME:	io_save()
 * DESCRIPTION:	write a range of lines to a file
 */
bool io_save(eb, filename, first, last, append, iobuf)
editbuf *eb;
char *filename;
Int first, last;
int append;
io *iobuf;
{
    char buf[BUF_SIZE];
    struct stat sbuf;
    fiocontext x;

    if (path_ed_write(x.filename, filename) == (char *) NULL ||
	(P_stat(x.filename, &sbuf) >= 0 && (sbuf.st_mode & S_IFMT) != S_IFREG))
    {
	return FALSE;
    }
    /* create file */
    x.fd = P_open(x.filename,
		  (append) ? O_CREAT | O_APPEND | O_WRONLY | O_BINARY :
			     O_CREAT | O_TRUNC | O_WRONLY | O_BINARY,
	      0664);
    if (x.fd < 0) {
	return FALSE;
    }

    /* initialize buffer */
    x.buffer = buf;
    x.inbuf = 0;

    /* initialize statistics */
    x.iostat = iobuf;
    x.iostat->lines = 0;
    x.iostat->chars = 0;
    x.iostat->zero = 0;
    x.iostat->split = 0;
    x.iostat->ill = FALSE;

    /* write range */
    if (ec_push((ec_ftn) NULL)) {
	P_close(x.fd);
	error((char *) NULL);	/* pass on error */
    }
    eb_range(eb, first, last, put_line, (char *) &x, FALSE);
    if (P_write(x.fd, x.buffer, x.inbuf) != x.inbuf) {
	error("error while writing file \"/%s\"", x.filename);
    }
    ec_pop();
    P_close(x.fd);

    return TRUE;
}
