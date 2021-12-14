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

class ASN {
public:
    static String *add(Frame *f, String *s1, String *s2, String *s3);
    static String *sub(Frame *f, String *s1, String *s2, String *s3);
    static int cmp(Frame *f, String *s1, String *s2);
    static String *mult(Frame *f, String *s1, String *s2, String *s3);
    static String *div(Frame *f, String *s1, String *s2, String *s3);
    static String *mod(Frame *f, String *s1, String *s2);
    static String *pow(Frame *f, String *s1, String *s2, String *s3);
    static String *modinv(Frame *f, String *s1, String *s2);
    static String *lshift(Frame *f, String *s1, LPCint shift, String *s2);
    static String *rshift(Frame *f, String *s, LPCint shift);
    static String *_and(Frame *f, String *s1, String *s2);
    static String *_or(Frame *f, String *s1, String *s2);
    static String *_xor(Frame *f, String *s1, String *s2);

private:
    static bool ticks(Frame *f, LPCuint ticks);
};
