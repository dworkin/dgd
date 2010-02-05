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

/*
 *   The basic data type is a line buffer, in which blocks of lines are
 * allocated. The line buffer can be made inactive, to make it use as little
 * system resources as possible.
 *   Blocks can be created, deleted, queried for their size, split in two, or
 * concatenated. Blocks are never actually deleted in a line buffer, but a
 * fake delete operation is added for the sake of completeness.
 */
typedef Int block;

typedef struct _btbuf_ {
    long offset;			/* offset in tmpfile */
    struct _btbuf_ *prev;		/* prev in linked list */
    struct _btbuf_ *next;		/* next in linked list */
    char *buf;				/* buffer with blocks and text */
} btbuf;

typedef struct {
    char *file;				/* tmpfile name */
    int fd;				/* tmpfile fd */
    char *buf;				/* current low-level buffer */
    int blksz;				/* block size in write buffer */
    int txtsz;				/* text size in write buffer */
    void (*putline) P((char*));		/* output line function */
    bool reverse;			/* for bk_put() */
    btbuf *wb;				/* write buffer */
    btbuf bt[NR_EDBUFS];		/* read & write buffers */
} linebuf;

extern linebuf *lb_new	  P((linebuf*, char*));
extern void	lb_del	  P((linebuf*));
extern void	lb_inact  P((linebuf*));

extern block	bk_new	  P((linebuf*, char*(*)(void)));
# define	bk_del(linebuf, block)	/* nothing */
extern Int	bk_size	  P((linebuf*, block));
extern void	bk_split  P((linebuf*, block, Int, block*, block*));
extern block	bk_cat	  P((linebuf*, block, block));
extern void	bk_put	  P((linebuf*, block, Int, Int, void(*)(char*), int));
