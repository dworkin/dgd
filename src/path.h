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

class Path {
public:
    virtual char *resolve(char *buf, char *file) = 0;
    virtual char *string(char *buf, char *file, unsigned int len) = 0;
    virtual char *from(char *buf, char *from, char *file) = 0;
    virtual char *edRead(char *buf, char *file) = 0;
    virtual char *edWrite(char *buf, char *file) = 0;
    virtual char *include(char *buf, char *from, char *file, String ***strs,
			  int *nstr) = 0;
};

class PathImpl : public Path {
public:
    virtual char *resolve(char *buf, char *file);
    virtual char *string(char *buf, char *file, unsigned int len);
    virtual char *from(char *buf, char *from, char *file);
    virtual char *edRead(char *buf, char *file);
    virtual char *edWrite(char *buf, char *file);
    virtual char *include(char *buf, char *from, char *file, String ***strs,
			  int *nstr);
};

extern Path *PM;
