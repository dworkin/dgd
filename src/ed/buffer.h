/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2019 DGD Authors (see the commit log for details)
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

class EditBuf {
public:
    EditBuf(char *tmpfile);
    virtual ~EditBuf();

    void clear();
    void add(Int ln, char *(*getline)());
    Block del(Int first, Int last);
    void change(Int first, Int last, Block b);
    Block yank(Int first, Int last);
    void put(Int ln, Block b);
    void range(Int first, Int last, void (*putline) (const char*),
	       bool reverse);
    void startblock();
    void addblock(const char *text);
    void endblock();

    LineBuf lb;			/* line buffer */
    Block buffer;		/* the actual edit buffer */
    Int lines;			/* # lines in edit buffer */
    Block flines;		/* block of first lines to add */

private:
    void flushLine();

    static char *addLine();

    int szlines;		/* size of "last" insert add */
    char *llines;		/* llbuf pointer */
    char llbuf[4 * MAX_LINE_SIZE]; /* last lines buffer */
};
