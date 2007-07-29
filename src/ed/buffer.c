# include "ed.h"
# include "buffer.h"

/*
 * This file defines the basic editing operations.
 */

/*
 * NAME:	editbuf->new()
 * DESCRIPTION:	create a new edit buffer
 */
editbuf *eb_new(tmpfile)
char *tmpfile;
{
    register editbuf *eb;

    eb = ALLOC(editbuf, 1);
    eb->lb = lb_new((linebuf *) NULL, tmpfile);
    eb->buffer = (block) 0;
    eb->lines = 0;

    return eb;
}

/*
 * NAME:	editbuf->del()
 * DESCRIPTION:	delete an edit buffer
 */
void eb_del(eb)
editbuf *eb;
{
    lb_del(eb->lb);
    FREE(eb);
}

/*
 * NAME:	editbuf->clear()
 * DESCRIPTION:	reinitialize an edit buffer
 */
void eb_clear(eb)
register editbuf *eb;
{
    lb_new(eb->lb, (char *) NULL);
    eb->buffer = (block) 0;
    eb->lines = 0;
}

/*
 * NAME:	editbuf->add()
 * DESCRIPTION:	add a new block of lines to the edit buffer after a given line.
 *		If this line is 0 the block is inserted before the other lines
 *		in the edit buffer.
 */
void eb_add(eb, ln, getline)
register editbuf *eb;
register Int ln;
char *(*getline) P((void));
{
    register block b;

    b = bk_new(eb->lb, getline);
    if (b != (block) 0) {
	Int size;

	size = eb->lines + bk_size(eb->lb, b);
	if (size < 0) {
	    error("Too many lines");
	}

	if (ln == 0) {
	    if (eb->lines == 0) {
		eb->buffer = b;
	    } else {
		eb->buffer = bk_cat(eb->lb, b, eb->buffer);
	    }
	} else if (ln == eb->lines) {
	    eb->buffer = bk_cat(eb->lb, eb->buffer, b);
	} else {
	    block head, tail;

	    bk_split(eb->lb, eb->buffer, ln, &head, &tail);
	    eb->buffer = bk_cat(eb->lb, bk_cat(eb->lb, head, b), tail);
	}

	eb->lines = size;
    }
}

/*
 * NAME:	editbuf->delete()
 * DESCRIPTION:	delete a subrange of lines in the edit buffer
 */
block eb_delete(eb, first, last)
register editbuf *eb;
register Int first, last;
{
    block head, mid, tail;
    Int size;

    size = last - first + 1;

    if (last < eb->lines) {
	bk_split(eb->lb, eb->buffer, last, &mid, &tail);
	if (first > 1) {
	    bk_split(eb->lb, mid, first - 1, &head, &mid);
	    eb->buffer = bk_cat(eb->lb, head, tail);
	} else {
	    eb->buffer = tail;
	}
    } else {
	mid = eb->buffer;
	if (first > 1) {
	    bk_split(eb->lb, mid, first - 1, &head, &mid);
	    eb->buffer = head;
	} else {
	    eb->buffer = (block) 0;
	}
    }
    eb->lines -= size;

    return mid;
}

/*
 * NAME:	editbuf->change()
 * DESCRIPTION:	change a subrange of lines in the edit buffer
 */
void eb_change(eb, first, last, b)
register editbuf *eb;
register Int first, last;
register block b;
{
    Int size;
    block head, tail;

    size = eb->lines - (last - first + 1);
    if (b != (block) 0) {
	size += bk_size(eb->lb, b);
	if (size < 0) {
	    error("Too many lines");
	}
    }

    if (last < eb->lines) {
	if (first > 1) {
	    bk_split(eb->lb, eb->buffer, first - 1, &head, (block *) NULL);
	    bk_split(eb->lb, eb->buffer, last, (block *) NULL, &tail);
	    if (b != (block) 0) {
		b = bk_cat(eb->lb, bk_cat(eb->lb, head, b), tail);
	    } else {
		b = bk_cat(eb->lb, head, tail);
	    }
	} else {
	    bk_split(eb->lb, eb->buffer, last, (block *) NULL, &tail);
	    if (b != (block) 0) {
		b = bk_cat(eb->lb, b, tail);
	    } else {
		b = tail;
	    }
	}
    } else if (first > 1) {
	bk_split(eb->lb, eb->buffer, first - 1, &head, (block *) NULL);
	if (b != (block) 0) {
	    b = bk_cat(eb->lb, head, b);
	} else {
	    b = head;
	}
    }
    eb->buffer = b;
    eb->lines = size;
}

/*
 * NAME:	editbuf->yank()
 * DESCRIPTION:	return a subrange block of the edit buffer
 */
block eb_yank(eb, first, last)
register editbuf *eb;
register Int first, last;
{
    block head, mid, tail;

    if (last < eb->lines) {
	bk_split(eb->lb, eb->buffer, last, &mid, &tail);
    } else {
	mid = eb->buffer;
    }
    if (first > 1) {
	bk_split(eb->lb, mid, first - 1, &head, &mid);
    }

    return mid;
}

/*
 * NAME:	editbuf->put()
 * DESCRIPTION:	put a block after a line in the edit buffer. The block is
 *		supplied immediately.
 */
void eb_put(eb, ln, b)
register editbuf *eb;
register Int ln;
register block b;
{
    Int size;

    size = eb->lines + bk_size(eb->lb, b);
    if (size < 0) {
	error("Too many lines");
    }

    if (ln == 0) {
	if (eb->lines == 0) {
	    eb->buffer = b;
	} else {
	    eb->buffer = bk_cat(eb->lb, b, eb->buffer);
	}
    } else if (ln == eb->lines) {
	eb->buffer = bk_cat(eb->lb, eb->buffer, b);
    } else {
	block head, tail;

	bk_split(eb->lb, eb->buffer, ln, &head, &tail);
	eb->buffer = bk_cat(eb->lb, bk_cat(eb->lb, head, b), tail);
    }

    eb->lines = size;
}

/*
 * NAME:	editbuf->range()
 * DESCRIPTION:	output a subrange of the edit buffer, without first making
 *		a subrange block for it
 */
void eb_range(eb, first, last, putline, reverse)
register editbuf *eb;
Int first, last;
void (*putline) P((char*));
int reverse;
{
    bk_put(eb->lb, eb->buffer, first - 1, last - first + 1, putline, reverse);
}

/*
 * Routines to add lines to a block in pieces. It would be nice if bk_new could
 * be used for this, but this is only possible if the editor functions as a
 * stand-alone program.
 * Lines are stored in a local buffer, which is flushed into a block when full.
 */

static editbuf *eeb;	/* editor buffer */

/*
 * NAME:	add_line()
 * DESCRIPTION:	return the next line from the lines buffer
 */
static char *add_line()
{
    register editbuf *eb;

    eb = eeb;
    if (eb->szlines > 0) {
	char *p;
	int len;

	len = strlen(p = eb->llines) + 1;
	eb->llines += len;
	eb->szlines -= len;
	return p;
    }
    return (char *) NULL;
}

/*
 * NAME:	flush_line()
 * DESCRIPTION:	flush the lines buffer into a block
 */
static void flush_line(eb)
register editbuf *eb;
{
    block b;

    eb->llines = eb->llbuf;
    eeb = eb;
    b = bk_new(eb->lb, add_line);
    if (eb->flines == (block) 0) {
	eb->flines = b;
    } else {
	eb->flines = bk_cat(eb->lb, eb->flines, b);
    }
}

/*
 * NAME:	editbuf->startblock()
 * DESCRIPTION:	start a block of lines
 */
void eb_startblock(eb)
register editbuf *eb;
{
    eb->flines = (block) 0;
    eb->szlines = 0;
}

/*
 * NAME:	editbuf->addblock()
 * DESCRIPTION:	add a line to the current block of lines
 */
void eb_addblock(eb, text)
register editbuf *eb;
register char *text;
{
    register int len;

    len = strlen(text) + 1;

    if (eb->szlines + len >= sizeof(eb->llines)) {
	flush_line(eb);
    }
    memcpy(eb->llbuf + eb->szlines, text, len);
    eb->szlines += len;
}

/*
 * NAME:	editbuf->endblock()
 * DESCRIPTION:	finish the current block
 */
void eb_endblock(eb)
register editbuf *eb;
{
    if (eb->szlines > 0) {
	flush_line(eb);
    }
}
