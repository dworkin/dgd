/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 2021 DGD Authors (see the commit log for details)
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
# include "path.h"
# include "edcmd.h"
# include <stdarg.h>

/*
 * stand-alone editor
 */

class EdAlloc : public Alloc {
public:
    virtual void init(size_t staticSize, size_t dynamicSize) { }

    virtual void finish() { }

# ifdef MEMDEBUG
    virtual char *alloc(size_t size, const char *file, int line) {
	return (char *) std::malloc(size);
    }

    virtual char *realloc(char *mem, size_t size1, size_t size2,
			  const char *file, int line) {
	return (char *) std::realloc(mem, size2);
    }
# else
    virtual char *alloc(size_t size) {
	return (char *) std::malloc(size);
    }

    virtual char *realloc(char *mem, size_t size1, size_t size2) {
	return (char *) std::realloc(mem, size2);
    }
# endif
    virtual void free(char *mem) {
	std::free(mem);
    }

    virtual void dynamicMode() { }

    virtual void staticMode() { }

    virtual Info *info() {
	return (Info *) NULL;
    }

    virtual bool check() {
	return TRUE;
    }

    virtual void purge() { }
};

static EdAlloc EDMM;
Alloc *MM = &EDMM;

class EdErrorContext : public ErrorContext {
public:
    virtual jmp_buf *push(Handler handler) {
	return (jmp_buf *) NULL;
    }

    virtual void pop() { }

    virtual void setException(String *err) { }

    virtual String *exception() {
	return (String *) NULL;
    }

    virtual void clearException() { }

    virtual void error(String *str) { }

    virtual void error(const char *format, ...) {
	va_list args;

	if (format != (char *) NULL) {
	    va_start(args, format);
	    vprintf(format, args);
	    va_end(args);
	    putchar('\n');
	}
	throw "ed error";
    }

    virtual void message(const char *format, ...) {
	va_list args;

	va_start(args, format);
	vprintf(format, args);
	va_end(args);
    }

    virtual void fatal(const char *format, ...) {
	va_list args;

	printf("Fatal error: ");
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	putchar('\n');

	std::abort();
    }
};

static EdErrorContext EDEC;
ErrorContext *EDC = &EDEC;

class EdPath : public Path {
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
	    sprintf(buf2, "%s/../%s", from, file);
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
	return EdPath::from(buf, from, file);
    }
};

static EdPath EDPM;
Path *PM = &EDPM;

/*
 * minimal wrapper for editor
 */
int main(int argc, char *argv[])
{
    char tmp[100], line[2048], *p;
    CmdBuf *ed;

    sprintf(tmp, "/tmp/ed%05d", (int) getpid());
    ed = new CmdBuf(tmp);
    if (argc > 1) {
	sprintf(line, "e %s", argv[1]);
	try {
	    ed->command(line);
	} catch (...) { }
    }

    for (;;) {
	if (ed->flags & CB_INSERT) {
	    fputs("*\b", stdout);
	} else {
	    putchar(':');
	}
	if (fgets(line, sizeof(line), stdin) == NULL) {
	    break;
	}
	p = strchr(line, '\n');
	if (p != NULL) {
	    *p = '\0';
	}
	try {
	    if (!ed->command(line)) {
		break;
	    }
	} catch (...) { }
    }

    delete ed;
    return 0;
}
