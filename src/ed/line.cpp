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

# define lfirst	prev
# define llast	next
# define type	s.u_index1
# define depth	s.u_index2
# define index1	s.u_index1
# define index2	s.u_index2

# define BLOCK(lb, blk)	\
	(Block) ((lb)->wb->offset + (intptr_t) (blk) - (intptr_t) (lb)->wb->buf)

# define EDFULLTREE	0x8000
# define EDDEPTH	0x7fff
# define EDMAXDEPTH	10000

/*
 * Create a new line buffer.  Arg 2 is the tmp file name.
 */
LineBuf::LineBuf(char *filename)
{
    int i;
    BTBuf *bt;

    file = strcpy(ALLOC(char, strlen(filename) + 1), filename);

    bt = this->bt;
    for (i = NR_EDBUFS; i > 0; --i) {
	bt->prev = bt - 1;
	bt->next = bt + 1;
	bt->buf = ALLOC(char, BLOCK_SIZE);
	bt++;
    }
    --bt;
    this->bt[0].prev = bt;
    bt->next = this->bt;

    init();
}

/*
 * delete a line buffer
 */
LineBuf::~LineBuf()
{
    char buf[STRINGSZ];
    int i;
    BTBuf *bt;

    /* close tmpfile */
    inact();

    /* remove tmpfile */
    P_unlink(path_native(buf, file));
    FREE(file);

    /* release memory */
    bt = this->bt;
    for (i = NR_EDBUFS; i > 0; --i) {
	FREE(bt->buf);
	bt++;
    }
}

/*
 * initialize line buffer
 */
void LineBuf::init()
{
    char buf[STRINGSZ];
    int i;
    BTBuf *bt;

    /* initialize */
    wb = bt = this->bt;
    (bt++)->offset = 0;		/* in use, but empty */
    for (i = NR_EDBUFS - 1; i > 0; --i) {
	(bt++)->offset = -BLOCK_SIZE;	/* not in use */
    }
    blksz = 0;
    txtsz = 0;

    /* create or truncate tmpfile */
    fd = P_open(path_native(buf, file), O_CREAT | O_TRUNC | O_RDWR | O_BINARY,
		0600);
    if (fd < 0) {
	EDC->fatal("cannot create editor tmpfile \"%s\"", file);
    }
}

/*
 * reset line buffer
 */
void LineBuf::reset()
{
    inact();	/* close the old tmpfile */
    init();
}

/*
 * make the line buffer inactive, i.e. make it use as few resources
 * as possible. A further operation on the line buffer will
 * re-activate it.
 */
void LineBuf::inact()
{
    /* close tmpfile, to save descriptors */
    if (fd >= 0) {
	P_close(fd);
	fd = -1;
    }
}

/*
 * make the line buffer active, this has to be done before each
 * operation on the tmpfile
 */
void LineBuf::act()
{
    char buf[STRINGSZ];

    if (fd < 0) {
	fd = P_open(path_native(buf, file), O_RDWR | O_BINARY, 0);
	if (fd < 0) {
	    EDC->fatal("cannot reopen editor tmpfile \"%s\"", file);
	}
    }
}

/*
 * Write the output buffer to the tmpfile.
 */
void LineBuf::write()
{
    if (blksz > 0) {
	long offset;

# ifdef TMPFILE_SIZE
	if (wb->offset >= TMPFILE_SIZE - BLOCK_SIZE) {
	    EDC->error("Editor tmpfile too large");
	}
# endif

	/* make the line buffer active */
	act();

	/* write in tmpfile */
	P_lseek(fd, offset = wb->offset, SEEK_SET);	/* EOF */
	if (P_write(fd, wb->buf, BLOCK_SIZE) < 0) {
	    EDC->error("Failed to write editor tmpfile");
	}
	/* cycle buffers */
	wb = wb->prev;
	wb->offset = offset + BLOCK_SIZE;
	blksz = 0;
	txtsz = 0;
    }
}

/*
 * return a pointer to the blk struct of arg 1. If needed, it is
 * loaded in memory first.
 */
LineBuf::Blk *LineBuf::load(Block b)
{
    BTBuf *bt;

    /* check the write buffer */
    bt = wb;
    if (b < bt->offset || b >= bt->offset + blksz) {
	/*
	 * walk through the read buffers to see if the block can be found
	 */
	for (;;) {
	    bt = bt->next;
	    if (bt == wb) {
		/*
		 * refill read buffer
		 */
		act();
		bt = bt->prev;
		P_lseek(fd, bt->offset = b - (b % BLOCK_SIZE), SEEK_SET);
		if (P_read(fd, bt->buf, BLOCK_SIZE) != BLOCK_SIZE) {
		    EDC->fatal("cannot read editor tmpfile \"%s\"", file);
		}
	    }

	    if (b >= bt->offset && b < bt->offset + BLOCK_SIZE) {
		BTBuf *rd;

		rd = wb->next;
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

    return (Blk *) ((buf = bt->buf) + b - bt->offset);
}

/*
 * put blk in write buffer. if text is non-zero, it is a chain
 * block with lines following it. return the copy in the buffer.
 */
LineBuf::Blk *LineBuf::putblk(Blk *bp, char *text)
{
    Blk *bp2;
    int blksz, txtsz;
    size_t strcsz;

    /* determine blocksize and textsize */
    blksz = sizeof(Blk);
    if (text != (char *) NULL) {
	blksz += sizeof(short);
	txtsz = strlen(text) + 1;
    } else {
	txtsz = 0;
    }

    strcsz = STRUCT_AL;
    if (strcsz > 2) {
	this->blksz = ALGN(this->blksz, STRUCT_AL);
    }

    /* flush write buffer if needed */
    if (this->blksz + this->txtsz + blksz + txtsz > BLOCK_SIZE) {
	/* write buffer full */
	write();
    }


    /* store block */
    bp2 = (Blk *) (wb->buf + this->blksz);
    *bp2 = *bp;
    this->blksz += blksz;

    if (txtsz != 0) {
	/* store text */
	bp2->prev = -1;
	bp2->lines = 1;
	bp2->lindex = 0;

	this->txtsz += txtsz;
	txtsz = BLOCK_SIZE - this->txtsz;
	strcpy(wb->buf + txtsz, text);
	*((short *)(bp2 + 1)) = txtsz;
    }

    return bp2;
}

/*
 * append a line after the current block (type chain). If the
 * block becomes too large for the buffer, continue it in a new
 * buffer. Return the new current block.
 */
LineBuf::Blk *LineBuf::putln(Blk *bp, char *text)
{
    int blksz, txtsz;

    /* determine blocksize and textsize */
    blksz = sizeof(short);
    txtsz = strlen(text) + 1;

    /* flush write buffer if needed */
    if (this->blksz + this->txtsz + blksz + txtsz > BLOCK_SIZE) {
	Int offset;
	Block prev;

	/* write full buffer */
	bp->next = wb->offset + BLOCK_SIZE;
	offset = bp->lindex + bp->lines;
	prev = BLOCK(this, bp);
	write();

	/* create new block */
	bp = (Blk *) wb->buf;
	bp->prev = prev;
	bp->lines = 0;
	bp->lindex = offset;
	blksz += sizeof(Blk);
    }

    /* store text */
    this->txtsz += txtsz;
    txtsz = BLOCK_SIZE - this->txtsz;
    strcpy(wb->buf + txtsz, text);
    ((short *)(bp + 1)) [bp->lines++] = txtsz;

    this->blksz += blksz;

    return bp;
}

/*
 * read a block of lines from getline. continue until
 * getline returns 0. Return the block.
 */
Block LineBuf::create()
{
    Blk *bp;
    char *text;
    Blk bb;

    /* get first line */
    text = getline();
    if (text == (char *) NULL) {
	return (Block) 0;
    }

    /* create block */
    bp = putblk(&bb, text);
    bb.lfirst = BLOCK(this, bp);
    bb.lines = 1;
    bb.index1 = 0;
    bb.index2 = 0;

    /* append lines */
    while ((text=getline()) != (char *) NULL) {
	bp = putln(bp, text);
	bb.lines++;
    }

    /* finish block */
    bp->next = -1;
    bb.llast = BLOCK(this, bp);
    bp = putblk(&bb, (char *) NULL);

    return BLOCK(this, bp);
}

/*
 * return the size of a block
 */
Int LineBuf::size(Block b)
{
    return load(b)->lines;
}

/*
 * split blk in two, arg 3 is size of first block (local for bk_split)
 */
void LineBuf::split1(Blk *bp, Int size, Block *b1, Block *b2)
{
    Int lines;
    Int first, last;

    first = 0;

    if (bp->type == CAT) {
	/* block consists of two concatenated blocks */
	first = bp->lfirst;
	last = bp->llast;

	bp = load(first);
	lines = bp->lines;
	if (lines > size) {
	    /* the first split block is contained in the first block */
	    split1(bp, size, b1, b2);
	    if (b2 != (Block *) NULL) {
		*b2 = cat(*b2, last);
	    }
	} else if (lines < size) {
	    /* the second split block is contained in the last block */
	    split1(load(last), size - lines, b1, b2);
	    if (b1 != (Block *) NULL) {
		*b1 = cat(first, *b1);
	    }
	} else {
	    /* splitting on the edge of cat */
	    if (b1 != (Block *) NULL) {
		*b1 = first;
	    }
	    if (b2 != (Block *) NULL) {
		*b2 = last;
	    }
	}
    } else {
	Blk bb1, bb2;
	Int offset, mid;

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
	bp = load(mid = bp->lfirst);
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
	    bp = load(mid);
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
	if (b1 != (Block *) NULL) {
	    bp = putblk(&bb1, (char *) NULL);
	    *b1 = BLOCK(this, bp);
	}

	/* block 2 */
	if (b2 != (Block *) NULL) {
	    bp = putblk(&bb2, (char *) NULL);
	    *b2 = BLOCK(this, bp);
	}
    }
}

/*
 * split block in two, arg 3 is size of first block
 */
void LineBuf::split(Block b, Int size, Block *b1, Block *b2)
{
    split1(load(b), size, b1, b2);
}

/*
 * return the concatenation of the arguments
 */
Block LineBuf::cat(Block b1, Block b2)
{
    Blk *bp1, *bp2;
    unsigned short depth1, depth2;
    Blk bb;

    /* get information about blocks to concatenate */
    bp1 = load(b1);
    depth1 = (bp1->type == CAT) ? bp1->depth & EDDEPTH : 1;
    bp2 = load(b2);
    depth2 = (bp2->type == CAT) ? bp2->depth & EDDEPTH : 1;

    /* start new block */
    bb.type = CAT;
    bb.lines = bp1->lines + bp2->lines;

    if (depth1 < depth2 && !(bp2->depth & EDFULLTREE)) {
	/* concat b1 and the first subblock of b2 */
	b2 = bp2->llast;
	b1 = cat(b1, bp2->lfirst);

	bp1 = load(b1);
	depth1 = bp1->depth & EDDEPTH;
	bp2 = load(b2);
	depth2 = (bp2->type == CAT) ? bp2->depth & EDDEPTH : 1;
    } else if (depth1 > depth2 && !(bp1->depth & EDFULLTREE)) {
	/* concat the last subblock of b1 and b2 */
	b1 = bp1->lfirst;
	b2 = cat(bp1->llast, b2);

	bp1 = load(b1);
	depth1 = (bp1->type == CAT) ? bp1->depth & EDDEPTH : 1;
	bp2 = load(b2);
	depth2 = bp2->depth & EDDEPTH;
    }

    /* finish new block */
    bb.lfirst = b1;
    bb.llast = b2;
    bb.depth = ((depth1 > depth2) ? depth1 : depth2) + 1;
    if (bb.depth > EDMAXDEPTH) {
	EDC->error("Editor line tree too large");
    }
    if (depth1 == depth2 &&
	(depth1 == 1 || (bp1->depth & bp2->depth & EDFULLTREE))) {
	bb.depth |= EDFULLTREE;
    }

    /* put it in the write buffer */
    bp1 = putblk(&bb, (char *) NULL);
    /* return the block */
    return BLOCK(this, bp1);
}

/*
 * output of a subrange of a blk (local for bk_put)
 */
void LineBuf::put1(Blk *bp, Int idx, Int size)
{
    Int lines, last;

    lines = bp->lines;

    if (bp->type == CAT) {
	if (!reverse) {
	    last = bp->llast;
	    bp = load(bp->lfirst);
	} else {
	    last = bp->lfirst;
	    bp = load(bp->llast);
	}
	lines = bp->lines;
	if (lines > idx) {
	    lines -= idx;
	    if (lines > size) {
		lines = size;
	    }
	    put1(bp, idx, lines);
	    size -= lines;
	    idx = 0;
	} else {
	    idx -= lines;
	}
	if (size > 0) {
	    put1(load(last), idx, size);
	}
    } else {
	Int first, offset, mid;

	first = 0;
	last = bp->llast + BLOCK_SIZE;
	lines += bp->index1;
	if (!reverse) {
	    idx += bp->index1;
	} else {
	    idx = lines - idx - 1;
	}
	bp = load(mid = bp->lfirst);
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
	    bp = load(mid);
	    idx -= bp->lindex - offset;
	}

	if (!reverse) {
	    for (;;) {
		lines = size;
		if (lines > bp->lines - idx) {
		    lines = bp->lines - idx;
		}
		size -= lines;

		do {
		    putline(buf + *((short *)(bp + 1) + idx++));
		    bp = load(mid);
		} while (--lines > 0);

		if (size == 0) {
		    return;
		}

		bp = load(mid = bp->next);
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
		    putline(buf + *((short *)(bp + 1) + --idx));
		    bp = load(mid);
		} while (--lines > 0);

		if (size == 0) {
		    return;
		}

		bp = load(mid = bp->prev);
		idx = 0;
	    }
	}
    }
}

/*
 * output of a subrange of a block
 */
void LineBuf::put(Block b, Int idx, Int size, bool reverse)
{
    Blk *bp;

    this->reverse = reverse;
    bp = load(b);
    put1(bp, (reverse) ? bp->lines - idx - size : idx, size);
}
