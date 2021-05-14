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

/*
 *   The basic data type is a line buffer, in which blocks of lines are
 * allocated. The line buffer can be made inactive, to make it use as little
 * system resources as possible.
 *   Blocks can be created, deleted, queried for their size, split in two, or
 * concatenated.
 */
typedef Int Block;

class LineBuf : public Allocated {
public:
    LineBuf(char *filename);
    virtual ~LineBuf();

    virtual char *getline() = 0;
    virtual void putline(const char *line) = 0;

    void reset();
    void inact();
    Block create();
    Int size(Block b);
    void split(Block b, Int size, Block *b1, Block *b2);
    Block cat(Block b1, Block b2);
    void put(Block b, Int idx, Int size, bool reverse);

private:
    struct BTBuf {
	long offset;			/* offset in tmpfile */
	BTBuf *prev;			/* prev in linked list */
	BTBuf *next;			/* next in linked list */
	char *buf;			/* buffer with blocks and text */
    };
    struct Blk {
	Block prev, next;		/* first and last */
	Int lines;			/* size of this block */
	union {
	    Int lindex;			/* index from start of chain block */
	    struct {
		short u_index1;		/* index in first chain block */
		short u_index2;		/* index in last chain block */
	    } s;
	};
    };

    void init();
    void act();
    void write();
    Blk *load(Block b);
    Blk *putblk(Blk *bp, char *text);
    Blk *putln(Blk *bp, char *text);
    void split1(Blk *bp, Int size, Block *b1, Block *b2);
    void put1(Blk *bp, Int idx, Int size);

    char *file;				/* tmpfile name */
    int fd;				/* tmpfile fd */
    char *buf;				/* current low-level buffer */
    int blksz;				/* block size in write buffer */
    int txtsz;				/* text size in write buffer */
    bool reverse;			/* for bk_put() */
    BTBuf *wb;				/* write buffer */
    BTBuf bt[NR_EDBUFS];		/* read & write buffers */
};
