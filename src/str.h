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

class String : public ChunkAllocated {
public:
    ~String();
    void ref() { refCount++; }
    void del();
    int cmp(String *str);
    String *add(String *str);
    ssizet index(long idx);
    void checkRange(long from, long to);
    String *range(long from, long to);
    Uint put(Uint n);

    static String *alloc(const char *text, long length);
    static String *create(const char *text, long length);
    static void clean();
    static void merge();
    static void clear();

    struct strref *primary;	/* primary reference */
    Uint refCount;		/* number of references */
    ssizet len;			/* string length */
    char *text;			/* string text */

private:
    String(const char *text, long length);
};
