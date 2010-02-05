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

# include "line.h"

typedef struct {
    linebuf *lb;		/* line buffer */
    block buffer;		/* the actual edit buffer */
    Int lines;			/* # lines in edit buffer */

    block flines;		/* block of first lines to add */
    int szlines;		/* size of "last" insert add */
    char *llines;		/* llbuf pointer */
    char llbuf[4 * MAX_LINE_SIZE]; /* last lines buffer */
} editbuf;

extern editbuf *eb_new		P((char*));
extern void	eb_del		P((editbuf*));
extern void	eb_clear	P((editbuf*));
extern void	eb_add		P((editbuf*, Int, char*(*)(void)));
extern block	eb_delete	P((editbuf*, Int, Int));
extern void	eb_change	P((editbuf*, Int, Int, block));
extern block	eb_yank		P((editbuf*, Int, Int));
extern void	eb_put		P((editbuf*, Int, block));
extern void	eb_range	P((editbuf*, Int, Int, void(*)(char*), int));
extern void	eb_startblock	P((editbuf*));
extern void	eb_addblock	P((editbuf*, char*));
extern void	eb_endblock	P((editbuf*));
