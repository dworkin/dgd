/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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
    virtual char *resolve(char *buf, char *file) {
	strcpy(buf, file);
	return buf;
    }
    virtual char *string(char *buf, char *file, unsigned int len) {
	if (len >= STRINGSZ || strlen(file) != len) {
	    return NULL;
	}
	return resolve(buf, file);
    }
    virtual char *from(char *buf, char *from, char *file) {
	char buf2[STRINGSZ];

	if (file[0] != '/' && strlen(from) + strlen(file) < STRINGSZ - 4) {
	    snprintf(buf2, sizeof(buf2), "%s/../%s", from, file);
	    file = buf2;
	}
	return resolve(buf, file);
    }
    virtual char *edRead(char *buf, char *file) {
	return resolve(buf, file);
    }
    virtual char *edWrite(char *buf, char *file) {
	return resolve(buf, file);
    }
    virtual char *include(char *buf, char *from, char *file, String ***strs,
			  int *nstr) {
	UNREFERENCED_PARAMETER(strs);
	UNREFERENCED_PARAMETER(nstr);
	return Path::from(buf, from, file);
    }
};

class PathImpl : public Path {
public:
    virtual char *resolve(char *buf, char *file);
    virtual char *edRead(char *buf, char *file);
    virtual char *edWrite(char *buf, char *file);
    virtual char *include(char *buf, char *from, char *file, String ***strs,
			  int *nstr);
};

extern Path *PM;
