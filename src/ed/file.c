# define INCLUDE_FILE_IO
# include "ed.h"
# include "buffer.h"
# include "path.h"
# include "file.h"

/*
 * The file I/O operations of the editor.
 */

static io iobuf;	/* local status buffer */

static int fd;		/* read/write file descriptor */
static char *buffer, *lbuf, *lbuflast;	/* buffer pointers */
static int inbuf;	/* # bytes in buffer */

/*
 * NAME:	getline()
 * DESCRIPTION:	read a line from the input, return as '\0'-terminated string
 *		without '\n'
 */
static char *getline()
{
    static char *bufp;
    register char c, *p, *bp;
    register int i;

    if (iobuf.ill) {	/* previous line was incomplete, therefore the last */
	return (char *) NULL;
    }

    p = lbuf;
    bp = bufp;
    i = inbuf;
    do {
	if (i == 0) {	/* buffer empty */
	    i = read(fd, buffer, BUF_SIZE);
	    if (i <= 0) {
		/* eof or error */
		if (p == lbuf) {
		    return (char *) NULL;
		} else {
		    p++;	/* make room for terminating '\0' */
		    iobuf.ill = TRUE;
		    break;
		}
	    }
	    bp = buffer;
	}
	--i;
	c = *bp++;
	if (c == '\0') {
	    iobuf.zero++;	/* skip zeroes */
	} else {
	    if (p == lbuflast && c != LF) {
		iobuf.split++;
		i++;
		--bp;
		c = LF;
	    }
	    *p++ = c;
	}
    } while (c != LF);	/* eoln */

    iobuf.lines++;
    iobuf.chars += p - lbuf;	/* including terminating '\0' */
    bufp = bp;
    inbuf = i;
    *--p = '\0';
    return lbuf;
}

/*
 * NAME:	io_load()
 * DESCRIPTION:	append block read from file after a line
 */
io *io_load(eb, filename, l)
editbuf *eb;
Int l;
char *filename;
{
    char b[MAX_LINE_SIZE];
    char buf[BUF_SIZE];

    /* open file */
    filename = path_ed_read(filename);
    if (filename == (char *) NULL) {
	return (io *) NULL;
    }
    fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0) {
	return (io *) NULL;
    }

    /* initialize buffers */
    buffer = buf;
    lbuf = b;
    lbuflast = &b[MAX_LINE_SIZE - 1];
    inbuf = 0;

    /* initialize statistics */
    iobuf.lines = 0;
    iobuf.chars = 0;
    iobuf.zero = 0;
    iobuf.split = 0;
    iobuf.ill = FALSE;

    /* add the block to the edit buffer */
    if (ec_push()) {
	close(fd);
	error((char *) NULL);	/* pass on error */
    }
    eb_add(eb, l, getline);
    ec_pop();
    close(fd);
    return &iobuf;
}

/*
 * NAME:	putline()
 * DESCRIPTION:	write a line to a file
 */
static void putline(text)
register char *text;
{
    register int len;

    len = strlen(text);
    iobuf.lines += 1;
    iobuf.chars += len + 1;
    while (inbuf + len >= BUF_SIZE) {	/* flush buffer */
	if (inbuf != BUF_SIZE) {	/* room left for a piece of line */
	    register int chunk;

	    chunk = BUF_SIZE - inbuf;
	    memcpy(buffer + inbuf, text, chunk);
	    text += chunk;
	    len -= chunk;
	}
	write(fd, buffer, BUF_SIZE);
	inbuf = 0;
    }
    if (len > 0) {			/* piece of line left */
	memcpy(buffer + inbuf, text, len);
	inbuf += len;
    }
    buffer[inbuf++] = LF;
}

/*
 * NAME:	io_save()
 * DESCRIPTION:	write a range of lines to a file
 */
io *io_save(eb, filename, first, last, append)
editbuf *eb;
char *filename;
Int first, last;
bool append;
{
    int sz;
    char buf[BUF_SIZE];

    filename = path_ed_write(filename);
    if (filename == (char *) NULL) {
	return (io *) NULL;
    }
    /* create file */
    fd = open(filename,
	      (append) ? O_CREAT | O_APPEND | O_WRONLY | O_BINARY :
		         O_CREAT | O_TRUNC | O_WRONLY | O_BINARY,
	      0664);
    if (fd < 0) {
	return (io *) NULL;
    }

    /* initialize buffer */
    buffer = buf;
    inbuf = 0;

    /* initialize statistics */
    iobuf.lines = 0;
    iobuf.chars = 0;
    iobuf.zero = 0;
    iobuf.split = 0;
    iobuf.ill = FALSE;

    /* write range */
    if (ec_push()) {
	close(fd);
	error((char *) NULL);	/* pass on error */
    }
    eb_range(eb, first, last, putline, FALSE);
    sz = write(fd, buffer, inbuf);
    ec_pop();
    close(fd);
    if (sz != inbuf) {
	error("error while writing file \"/%s\"", filename);
    }
    return &iobuf;
}
