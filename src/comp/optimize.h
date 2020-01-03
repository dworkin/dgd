/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2020 DGD Authors (see the commit log for details)
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

class Optimize {
public:
    static void init();
    static Node *stmt(Node *first, Uint *depth);

private:
    static Uint max2(Uint a, Uint b);
    static Uint max3(Uint a, Uint b, Uint c);
    static Node **sideStart(Node **n, Uint *depth);
    static void sideAdd(Node **n, Uint depth);
    static Uint sideEnd(Node **n, Node *side, Node **oldside, Uint olddepth);
    static Uint lvalue(Node *n);
    static Uint binconst(Node **m);
    static Node *tst(Node *n);
    static Node *_not(Node *n);
    static Uint binop(Node **m);
    static Uint assignExpr(Node **m, bool pop);
    static bool ctest(Node *n);
    static Uint cond(Node **m, bool pop);
    static Uint expr(Node **m, bool pop);
    static int constant(Node *n);
    static Node *skip(Node *n);
};
