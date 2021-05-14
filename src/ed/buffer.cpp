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

# include "ed.h"
# include "buffer.h"

/*
 * This file defines the basic editing operations.
 */

/*
 * create a new edit buffer
 */
EditBuf::EditBuf(char *tmpfile) :
    LineBuf(tmpfile)
{
    buffer = (Block) 0;
    lines = 0;
}

/*
 * delete an edit buffer
 */
EditBuf::~EditBuf()
{
}

/*
 * reinitialize an edit buffer
 */
void EditBuf::clear()
{
    reset();
    buffer = (Block) 0;
    lines = 0;
}

/*
 * add a new block of lines to the edit buffer after a given line.
 * If this line is 0 the block is inserted before the other lines
 * in the edit buffer.
 */
void EditBuf::add(Int ln, char *(*get)())
{
    Block b;

    getLine = get;
    b = create();
    if (b != (Block) 0) {
	Int size;

	size = lines + LineBuf::size(b);
	if (size < 0) {
	    EDC->error("Too many lines");
	}

	if (ln == 0) {
	    if (lines == 0) {
		buffer = b;
	    } else {
		buffer = cat(b, buffer);
	    }
	} else if (ln == lines) {
	    buffer = cat(buffer, b);
	} else {
	    Block head, tail;

	    split(buffer, ln, &head, &tail);
	    buffer = cat(cat(head, b), tail);
	}

	lines = size;
    }
}

/*
 * delete a subrange of lines in the edit buffer
 */
Block EditBuf::del(Int first, Int last)
{
    Block head, mid, tail;
    Int size;

    size = last - first + 1;

    if (last < lines) {
	split(buffer, last, &mid, &tail);
	if (first > 1) {
	    split(mid, first - 1, &head, &mid);
	    buffer = cat(head, tail);
	} else {
	    buffer = tail;
	}
    } else {
	mid = buffer;
	if (first > 1) {
	    split(mid, first - 1, &head, &mid);
	    buffer = head;
	} else {
	    buffer = (Block) 0;
	}
    }
    lines -= size;

    return mid;
}

/*
 * change a subrange of lines in the edit buffer
 */
void EditBuf::change(Int first, Int last, Block b)
{
    Int size;
    Block head, tail;

    size = lines - (last - first + 1);
    if (b != (Block) 0) {
	size += LineBuf::size(b);
	if (size < 0) {
	    EDC->error("Too many lines");
	}
    }

    if (last < lines) {
	if (first > 1) {
	    split(buffer, first - 1, &head, (Block *) NULL);
	    split(buffer, last, (Block *) NULL, &tail);
	    if (b != (Block) 0) {
		b = cat(cat(head, b), tail);
	    } else {
		b = cat(head, tail);
	    }
	} else {
	    split(buffer, last, (Block *) NULL, &tail);
	    if (b != (Block) 0) {
		b = cat(b, tail);
	    } else {
		b = tail;
	    }
	}
    } else if (first > 1) {
	split(buffer, first - 1, &head, (Block *) NULL);
	if (b != (Block) 0) {
	    b = cat(head, b);
	} else {
	    b = head;
	}
    }
    buffer = b;
    lines = size;
}

/*
 * return a subrange block of the edit buffer
 */
Block EditBuf::yank(Int first, Int last)
{
    Block head, mid, tail;

    if (last < lines) {
	split(buffer, last, &mid, &tail);
    } else {
	mid = buffer;
    }
    if (first > 1) {
	split(mid, first - 1, &head, &mid);
    }

    return mid;
}

/*
 * put a block after a line in the edit buffer. The block is
 * supplied immediately.
 */
void EditBuf::put(Int ln, Block b)
{
    Int size;

    size = lines + LineBuf::size(b);
    if (size < 0) {
	EDC->error("Too many lines");
    }

    if (ln == 0) {
	if (lines == 0) {
	    buffer = b;
	} else {
	    buffer = cat(b, buffer);
	}
    } else if (ln == lines) {
	buffer = cat(buffer, b);
    } else {
	Block head, tail;

	split(buffer, ln, &head, &tail);
	buffer = cat(cat(head, b), tail);
    }

    lines = size;
}

/*
 * output a subrange of the edit buffer, without first making
 * a subrange block for it
 */
void EditBuf::range(Int first, Int last, void (*put)(const char*),
		    bool reverse)
{
    putLine = put;
    LineBuf::put(buffer, first - 1, last - first + 1, reverse);
}

/*
 * return the next line from the lines buffer
 */
char *EditBuf::getline()
{
    if (getLine != (char *(*)()) NULL) {
	return (*getLine)();
    }

    if (szlines > 0) {
	char *p;
	int len;

	len = strlen(p = llines) + 1;
	llines += len;
	szlines -= len;
	return p;
    }
    return (char *) NULL;
}

/*
 * output line of text
 */
void EditBuf::putline(const char *line)
{
    (*putLine)(line);
}

/*
 * flush the lines buffer into a block
 */
void EditBuf::flushLine()
{
    Block b;

    llines = llbuf;
    getLine = (char *(*)()) NULL;
    b = create();
    if (flines == (Block) 0) {
	flines = b;
    } else {
	flines = cat(flines, b);
    }
}

/*
 * start a block of lines
 */
void EditBuf::startblock()
{
    flines = (Block) 0;
    szlines = 0;
}

/*
 * add a line to the current block of lines
 */
void EditBuf::addblock(const char *text)
{
    int len;

    len = strlen(text) + 1;

    if (szlines + len >= sizeof(llines)) {
	flushLine();
    }
    memcpy(llbuf + szlines, text, len);
    szlines += len;
}

/*
 * finish the current block
 */
void EditBuf::endblock()
{
    if (szlines > 0) {
	flushLine();
    }
}
