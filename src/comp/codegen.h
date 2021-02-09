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

class Codegen {
public:
    static void init(int inherited);
    static char *function(String *fname, Node *n, int nvar, int npar,
			  unsigned int depth, unsigned short *size);
    static int nfuncs();
    static void clear();

private:
    static int type(Node *n, long *l);
    static void cast(Node *n);
    static int lvalue(Node *n, int fetch);
    static void store(Node *n);
    static void assign(Node *n, int op);
    static int aggr(Node *n);
    static int mapAggr(Node *n);
    static int lvalAggr(Node **l);
    static void storeAggr(Node *n);
    static int sumargs(Node *n);
    static int funargs(Node **l, int *nargs, bool *spread);
    static void storearg(Node *n);
    static void storeargs(Node *n);
    static int math(const char *name);
    static void expr(Node *n, int pop);
    static void cond(Node *n, int jmptrue);
    static void switchStart(Node *n);
    static void switchInt(Node *n);
    static void switchRange(Node *n);
    static void switchStr(Node *n);
    static void stmt(Node *n);
};
