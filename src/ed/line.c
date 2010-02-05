/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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
# include "line.h"

/*
 *   The blocks in a line buffer are written in a temporary file, and read back
 * if needed. There are at least 3 temporary file buffers: a write buffer and 
 * two read buffers. If a block is not in one of those buffers, it is loaded
 * in the read buffer that wasn't used in the last read buffer access.
 *   The write buffer is filled with blocks on one side and text on the other.
 * If there is no more room for another block or more text, the buffer is
 * written to the tmpfile, the "most recently used" read buffer becomes the
 * write buffer, and the write buffer becomes the other read buffer (which is
 * emptied first of course).
 *   The switching between buffers ensures that it is always possible to have
 * two blocks from the tmpfile in memory. Loading a 3rd block might erase one of
 * the two previous blocks though.
 *
 *   There are 3 types of blocks: on the lowest level is the chain block, which
 * contains pointers to previous and next chain blocks (if they exist) and is
 * followed by an array of text pointers.
 *   The second type is the subrange block, which contains pointers to a first
 * and last chain block (not nessecarily first and last in the full chain) and
 * indices of the first line in the first chain block and the last line in the
 * last chain block.
 *   The third type is the CAT block, which contains pointers to two other CAT
 * or subrange blocks.
 *
 *   Blocks are NEVER discarded.
 *   Creating a new block results in a chain of chain blocks, and a subrange
 * block with pointers to the first and last chain blocks; the subrange block
 * is the whole block.
 *   Concatenating two blocks results in a new CAT block with pointers to the
 * two blocks.
 *   Splitting a subrange block creates two new subrange blocks with pointers in
 * the same chain block. Splitting a CAT block creates new subrange blocks and
 * a whole new tree of CAT blocks, if needed.
 */

# define BLOCK_SIZE	(2 * (MAX_LINE_SIZE))
# define BLOCK_MASK	(~(BLOCK_SIZE-1))
# define CAT		-1

typedef struct {
    block prev, next;		/* first and last */
    Int lines;			/* size of this block */
    union {
	Int u_index;		/* index from start of chain block */
	struct {
	    short u_index1;	/* index in first chain block */
	    short u_index2;	/* index in last chain block */
	} s;
    } u;
} blk;

# define lfirst	prev
# define llast	next
# define type	u.s.u_index1
# define depth	u.s.u_index2
# define lindex	u.u_index
# define index1	u.s.u_index1
# define index2	u.s.u_index2

# define BLOCK(lb, blk)	\
	(block) ((lb)->wb->offset + (long) (blk) - (long) (lb)->wb->buf)

# define EDFULLTREE	0x8000
# define EDDEPTH	0x7fff
# define EDMAXDEPTH	10000

/*
 * NAME:	linebuf->new()
 * DESCRIPTION:	If the first argument is 0, create a new line buffer,
 *		otherwise refresh the line buffer given as arg 1.
 *		If a new line buffer is created, arg 2 is the tmp file name.
 *		Return the new/refreshed line buffer.
 */
linebuf *lb_new(lb, filename)
register linebuf *lb;
char *filename;
{
    char buf[STRINGSZ];
    register int i;
    register btbuf *bt;

    if (lb != (linebuf *) NULL) {
	/* refresh; close the old tmpfile */
	lb_inact(lb);
    } else {
	/* allocate new line buffer */
	lb = ALLOC(linebuf, 1);

	lb->file = strcpy(ALLOC(char, strlen(filename) + 1), filename);

	bt = lb->bt;
	for (i = NR_EDBUFS; i > 0; --i) {
	    bt->prev = bt - 1;
	    bt->next = bt + 1;
	    bt->buf = ALLOC(char, BLOCK_SIZE);
	    bt++;
	}
	--bt;
	lb->bt[0].prev = bt;
	bt->next = lb->bt;
    }

    /* initialize */
    lb->wb = bt = lb->bt;
    (bt++)->offset = 0;		/* in use, but empty */
    for (i = NR_EDBUFS - 1; i > 0; --i) {
	(bt++)->offset = -BLOCK_SIZE;	/* not in use */
    }
    lb->blksz = 0;
    lb->txtsz = 0;

    /* create or truncate tmpfile */
    lb->fd = P_open(path_native(buf, lb->file),
		    O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0600);
    if (lb->fd < 0) {
	fatal("cannot create editor tmpfile \"%s\"", lb->file);
    }

    return lb;
}

/*
 * NAME:	linebuf->del()
 * DESCRIPTION:	delete a line buffer
 */
void lb_del(lb)
register linebuf *lb;
{
    char buf[STRINGSZ];
    register int i;
    register btbuf *bt;

    /* close tmpfile */
    lb_inact(lb);

    /* remove tmpfile */
    P_unlink(path_native(buf, lb->file));
    FREE(lb->file);

    /* release memory */
    bt = lb->bt;
    for (i = NR_EDBUFS; i > 0; --i) {
	FREE(bt->buf);
	bt++;
    }
    FREE(lb);
}

/*
 * NAME:	linebuf->inact()
 * DESCRIPTION:	make the line buffer inactive, i.e. make it use as few resources
 *		as possible. A further operation on the line buffer will
 *		re-activate it.
 */
void lb_inact(lb)
register linebuf *lb;
{
    /* close tmpfile, to save descriptors */
    if (lb->fd >= 0) {
	P_close(lb->fd);
	lb->fd = -1;
    }
}

/*
 * NAME:	linebuf->act()
 * DESCRIPTION:	make the line buffer active, this has to be done before each
 *		operation on the tmpfile
 */
static void lb_act(lb)
register linebuf *lb;
{
    char buf[STRINGSZ];

    if (lb->fd < 0) {
	lb->fd = P_open(path_native(buf, lb->file), O_RDWR | O_BINARY, 0);
	if (lb->fd < 0) {
	    fatal("cannot reopen editor tmpfile \"%s\"", lb->file);
	}
    }
}

/*
 * NAME:	linebuf->write()
 * DESCRIPTION:	Write the output buffer to the tmpfile.
 */
static void lb_write(lb)
register linebuf *lb;
{
    if (lb->blksz > 0) {
	long offset;

# ifdef TMPFILE_SIZE
	if (lb->wb->offset >= TMPFILE_SIZE - BLOCK_SIZE) {
	    error("Editor tmpfile too large");
	}
# endif

	/* make the line buffer active */
	lb_act(lb);

	/* write in tmpfile */
	P_lseek(lb->fd, offset = lb->wb->offset, SEEK_SET);	/* EOF */
	if (P_write(lb->fd, lb->wb->buf, BLOCK_SIZE) < 0) {
	    error("Failed to write editor tmpfile");
	}
	/* cycle buffers */
	lb->wb = lb->wb->prev;
	lb->wb->offset = offset + BLOCK_SIZE;
	lb->blksz = 0;
	lb->txtsz = 0;
    }
}

/*
 * NAME:	linebuf->load()
 * DESCRIPTION:	return a pointer to the blk struct of arg 1. If needed, it is
 *		loaded in memory first.
 */
static blk *bk_load(lb, b)
register linebuf *lb;
block b;
{
    register btbuf *bt;

    /* check the write buffer */
    bt = lb->wb;
    if (b < bt->offset || b >= bt->offset + lb->blksz) {
	/*
	 * walk through the read buffers to see if the block can be found
	 */
	for (;;) {
	    bt = bt->next;
	    if (bt == lb->wb) {
		/*
		 * refill read buffer
		 */
		lb_act(lb);
		bt = bt->prev;
		P_lseek(lb->fd, bt->offset = b - (b % BLOCK_SIZE), SEEK_SET);
		if (P_read(lb->fd, bt->buf, BLOCK_SIZE) != BLOCK_SIZE) {
		    fatal("cannot read editor tmpfile \"%s\"", lb->file);
		}
	    }

	    if (b >= bt->offset && b < bt->offset + BLOCK_SIZE) {
		register btbuf *rd;

		rd = lb->wb->next;
		if (bt != rd) {
		    /*
		     * make this buffer the "first" read buffer
		     */
		    bt->prev->next = bt->next;
		    bt->next->prev = bt->prev;
		    bt->prev = rd->prev;
		    bt->next = rd;
		    rd->prev->next = bt;
		    rd->prev = bt;
		}
		break;
	    }
	}
    }

    return (blk *) ((lb->buf = bt->buf) + b - bt->offset);
}

/*
 * NAME:	linebuf->putblk()
 * DESCRIPTION:	put blk in write buffer. if text is non-zero, it is a chain
 *		block with lines following it. return the copy in the buffer.
 */
static blk *bk_putblk(lb, bp, text)
register linebuf *lb;
blk *bp;
char *text;
{
    register blk *bp2;
    register int blksz, txtsz;

    /* determine blocksize and textsize */
    blksz = sizeof(blk);
    if (text != (char *) NULL) {
	blksz += sizeof(short);
	txtsz = strlen(text) + 1;
    } else {
	txtsz = 0;
    }
    if (STRUCT_AL > 2) {
	lb->blksz = ALGN(lb->blksz, STRUCT_AL);
    }

    /* flush write buffer if needed */
    if (lb->blksz + lb->txtsz + blksz + txtsz > BLOCK_SIZE) {
	/* write buffer full */
	lb_write(lb);
    }


    /* store block */
    bp2 = (blk *) (lb->wb->buf + lb->blksz);
    *bp2 = *bp;
    lb->blksz += blksz;

    if (txtsz != 0) {
	/* store text */
	bp2->prev = -1;
	bp2->lines = 1;
	bp2->lindex = 0;

	lb->txtsz += txtsz;
	txtsz = BLOCK_SIZE - lb->txtsz;
	strcpy(lb->wb->buf + txtsz, text);
	*((short *)(bp2 + 1)) = txtsz;
    }

    return bp2;
}

/*
 * NAME:	linebuf->putln()
 * DESCRIPTION:	append a line after the current block (type chain). If the
 *		block becomes too large for the buffer, continue it in a new
 *		buffer. Return the new current block.
 */
static blk *bk_putln(lb, bp, text)
register linebuf *lb;
register blk *bp;
char *text;
{
    register int blksz, txtsz;

    /* determine blocksize and textsize */
    blksz = sizeof(short);
    txtsz = strlen(text) + 1;

    /* flush write buffer if needed */
    if (lb->blksz + lb->txtsz + blksz + txtsz > BLOCK_SIZE) {
	Int offset;
	block prev;

	/* write full buffer */
	bp->next = lb->wb->offset + BLOCK_SIZE;
	offset = bp->lindex + bp->lines;
	prev = BLOCK(lb, bp);
	lb_write(lb);

	/* create new block */
	bp = (blk *) lb->wb->buf;
	bp->prev = prev;
	bp->lines = 0;
	bp->lindex = offset;
	blksz += sizeof(blk);
    }

    /* store text */
    lb->txtsz += txtsz;
    txtsz = BLOCK_SIZE - lb->txtsz;
    strcpy(lb->wb->buf + txtsz, text);
    ((short *)(bp + 1)) [bp->lines++] = txtsz;

    lb->blksz += blksz;

    return bp;
}

/*
 * NAME:	linebuf->new()
 * DESCRIPTION:	read a block of lines from function getline. continue until
 *		getline returns 0. Return the block.
 */
block bk_new(lb, getline)
register linebuf *lb;
char *(*getline) P((void));
{
    register blk *bp;
    register char *text;
    blk bb;

    /* get first line */
    text = (*getline)();
    if (text == (char *) NULL) {
	return (block) 0;
    }

    /* create block */
    bp = bk_putblk(lb, &bb, text);
    bb.lfirst = BLOCK(lb, bp);
    bb.lines = 1;
    bb.index1 = 0;
    bb.index2 = 0;

    /* append lines */
    while ((text=(*getline)()) != (char *) NULL) {
	bp = bk_putln(lb, bp, text);
	bb.lines++;
    }

    /* finish block */
    bp->next = -1;
    bb.llast = BLOCK(lb, bp);
    bp = bk_putblk(lb, &bb, (char *) NULL);

    return BLOCK(lb, bp);
}

/*
 * NAME:	linebuf->size()
 * DESCRIPTION:	return the size of a block
 */
Int bk_size(lb, b)
linebuf *lb;
block b;
{
    return bk_load(lb, b)->lines;
}

/*
 * NAME:	bk_split1()
 * DESCRIPTION:	split blk in two, arg 3 is size of first block
 *		(local for bk_split)
 */
static void bk_split1(lb, bp, size, b1, b2)
register linebuf *lb;
register blk *bp;
register Int size;
block *b1, *b2;
{
    register Int lines;
    register Int first, last;

    if (bp->type == CAT) {
	/* block consists of two concatenated blocks */
	first = bp->lfirst;
	last = bp->llast;

	bp = bk_load(lb, first);
	lines = bp->lines;
	if (lines > size) {
	    /* the first split block is contained in the first block */
	    bk_split1(lb, bp, size, b1, b2);
	    if (b2 != (block *) NULL) {
		*b2 = bk_cat(lb, *b2, last);
	    }
	} else if (lines < size) {
	    /* the second split block is contained in the last block */
	    bk_split1(lb, bk_load(lb, last), size - lines, b1, b2);
	    if (b1 != (block *) NULL) {
		*b1 = bk_cat(lb, first, *b1);
	    }
	} else {
	    /* splitting on the edge of cat */
	    if (b1 != (block *) NULL) {
		*b1 = first;
	    }
	    if (b2 != (block *) NULL) {
		*b2 = last;
	    }
	}
    } else {
	blk bb1, bb2;
	register Int offset, mid;

	/* block is a (sub)range block */
	lines = bp->lines;

	/* create two new subrange blocks */
	bb1.lfirst = bp->lfirst;
	bb1.lines = size;
	bb1.index1 = bp->index1;

	bb2.llast = bp->llast;
	bb2.lines = lines - size;
	bb2.index2 = bp->index2;

	last = bp->llast + BLOCK_SIZE;
	lines += bb1.index1;
	size += bb1.index1;
	bp = bk_load(lb, mid = bp->lfirst);
	offset = bp->lindex;

	while (size < 0 || size >= bp->lines) {
	    if (size < 0) {
		last = mid;
		lines = bp->lindex - offset;
		size += lines;
	    } else {
		first = bp->next;
		lines -= bp->lindex + bp->lines - offset;
		size -= bp->lines;
		offset = bp->lindex + bp->lines;
	    }
	    mid = first + ((((last - first) / lines) * size) & BLOCK_MASK);
	    bp = bk_load(lb, mid);
	    size -= bp->lindex - offset;
	}

	if (size == 0) {
	    bb1.llast = bp->prev;
	    bb1.index2 = 0;
	} else {
	    bb1.llast = mid;
	    bb1.index2 = bp->lines - size;
	}
	bb2.lfirst = mid;
	bb2.index1 = size;

	/* block 1 */
	if (b1 != (block *) NULL) {
	    bp = bk_putblk(lb, &bb1, (char *) NULL);
	    *b1 = BLOCK(lb, bp);
	}

	/* block 2 */
	if (b2 != (block *) NULL) {
	    bp = bk_putblk(lb, &bb2, (char *) NULL);
	    *b2 = BLOCK(lb, bp);
	}
    }
}

/*
 * NAME:	linebuf->split()
 * DESCRIPTION:	split block in two, arg 3 is size of first block
 */
void bk_split(lb, b, size, b1, b2)
linebuf *lb;
block b, *b1, *b2;
Int size;
{
    bk_split1(lb, bk_load(lb, b), size, b1, b2);
}

/*
 * NAME:	linebuf->cat()
 * DESCRIPTION:	return the concatenation of the arguments
 */
block bk_cat(lb, b1, b2)
register linebuf *lb;
block b1, b2;
{
    register blk *bp1, *bp2;
    unsigned short depth1, depth2;
    blk bb;

    /* get information about blocks to concatenate */
    bp1 = bk_load(lb, b1);
    depth1 = (bp1->type == CAT) ? bp1->depth & EDDEPTH : 1;
    bp2 = bk_load(lb, b2);
    depth2 = (bp2->type == CAT) ? bp2->depth & EDDEPTH : 1;

    /* start new block */
    bb.type = CAT;
    bb.lines = bp1->lines + bp2->lines;

    if (depth1 < depth2 && !(bp2->depth & EDFULLTREE)) {
	/* concat b1 and the first subblock of b2 */
	b2 = bp2->llast;
	b1 = bk_cat(lb, b1, bp2->lfirst);

	bp1 = bk_load(lb, b1);
	depth1 = bp1->depth & EDDEPTH;
	bp2 = bk_load(lb, b2);
	depth2 = (bp2->type == CAT) ? bp2->depth & EDDEPTH : 1;
    } else if (depth1 > depth2 && !(bp1->depth & EDFULLTREE)) {
	/* concat the last subblock of b1 and b2 */
	b1 = bp1->lfirst;
	b2 = bk_cat(lb, bp1->llast, b2);

	bp1 = bk_load(lb, b1);
	depth1 = (bp1->type == CAT) ? bp1->depth & EDDEPTH : 1;
	bp2 = bk_load(lb, b2);
	depth2 = bp2->depth & EDDEPTH;
    }

    /* finish new block */
    bb.lfirst = b1;
    bb.llast = b2;
    bb.depth = ((depth1 > depth2) ? depth1 : depth2) + 1;
    if (bb.depth > EDMAXDEPTH) {
	error("Editor line tree too large");
    }
    if (depth1 == depth2 &&
	(depth1 == 1 || (bp1->depth & bp2->depth & EDFULLTREE))) {
	bb.depth |= EDFULLTREE;
    }

    /* put it in the write buffer */
    bp1 = bk_putblk(lb, &bb, (char *) NULL);
    /* return the block */
    return BLOCK(lb, bp1);
}

/*
 * NAME:	bk_put1()
 * DESCRIPTION:	output of a subrange of a blk
 *		(local for bk_put)
 */
static void bk_put1(lb, bp, idx, size)
register linebuf *lb;
register blk *bp;
register Int idx, size;
{
    register Int lines, last;

    lines = bp->lines;

    if (bp->type == CAT) {
	if (!lb->reverse) {
	    last = bp->llast;
	    bp = bk_load(lb, bp->lfirst);
	} else {
	    last = bp->lfirst;
	    bp = bk_load(lb, bp->llast);
	}
	lines = bp->lines;
	if (lines > idx) {
	    lines -= idx;
	    if (lines > size) {
		lines = size;
	    }
	    bk_put1(lb, bp, idx, lines);
	    size -= lines;
	    idx = 0;
	} else {
	    idx -= lines;
	}
	if (size > 0) {
	    bk_put1(lb, bk_load(lb, last), idx, size);
	}
    } else {
	register Int first, offset, mid;

	last = bp->llast + BLOCK_SIZE;
	lines += bp->index1;
	if (!lb->reverse) {
	    idx += bp->index1;
	} else {
	    idx = lines - idx - 1;
	}
	bp = bk_load(lb, mid = bp->lfirst);
	offset = bp->lindex;

	while (idx < 0 || idx >= bp->lines) {
	    if (idx < 0) {
		last = mid;
		lines = bp->lindex - offset;
		idx += lines;
	    } else {
		first = bp->next;
		lines -= bp->lindex + bp->lines - offset;
		idx -= bp->lines;
		offset = bp->lindex + bp->lines;
	    }
	    mid = first + ((((last - first) / lines) * idx) & BLOCK_MASK);
	    bp = bk_load(lb, mid);
	    idx -= bp->lindex - offset;
	}

	if (!lb->reverse) {
	    for (;;) {
		lines = size;
		if (lines > bp->lines - idx) {
		    lines = bp->lines - idx;
		}
		size -= lines;

		do {
		    (*lb->putline)(lb->buf + *((short *)(bp + 1) + idx++));
		    bp = bk_load(lb, mid);
		} while (--lines > 0);

		if (size == 0) {
		    return;
		}

		bp = bk_load(lb, mid = bp->next);
		idx = 0;
	    }
	} else {
	    idx = bp->lines - idx - 1;
	    for (;;) {
		lines = size;
		if (lines > bp->lines - idx) {
		    lines = bp->lines - idx;
		}
		size -= lines;

		idx = bp->lines - idx;
		do {
		    (*lb->putline)(lb->buf + *((short *)(bp + 1) + --idx));
		    bp = bk_load(lb, mid);
		} while (--lines > 0);

		if (size == 0) {
		    return;
		}

		bp = bk_load(lb, mid = bp->prev);
		idx = 0;
	    }
	}
    }
}

/*
 * NAME:	linebuf->put()
 * DESCRIPTION:	output of a subrange of a block
 */
void bk_put(lb, b, idx, size, putline, reverse)
register linebuf *lb;
block b;
Int idx, size;
void (*putline) P((char*));
int reverse;
{
    blk *bp;

    lb->putline = putline;
    lb->reverse = reverse;
    bp = bk_load(lb, b);
    bk_put1(lb, bp, (reverse) ? bp->lines - idx - size : idx, size);
}
