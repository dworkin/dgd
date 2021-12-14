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

class ErrorContext {
public:
    typedef void (*Handler) (Frame*, LPCint);

    virtual jmp_buf *push(Handler handler = NULL) {
	UNREFERENCED_PARAMETER(handler);
	return (jmp_buf *) NULL;
    }
    virtual void pop() { }

    virtual void setException(String *err) {
	UNREFERENCED_PARAMETER(err);
    }
    virtual String *exception() {
	return (String *) NULL;
    }
    virtual void clearException() { }

    virtual void error(String *str) {
	UNREFERENCED_PARAMETER(str);
    }
    virtual void error(const char *format, ...) {
	va_list args;

	if (format != (char *) NULL) {
	    va_start(args, format);
	    vprintf(format, args);
	    va_end(args);
	    putchar('\n');
	}
	throw "error";
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

    jmp_buf *env;			/* current error env */
};

class ErrorContextImpl : public ErrorContext {
public:
    virtual jmp_buf *push(Handler handler);
    virtual void pop();

    virtual void setException(String *err);
    virtual String *exception();
    virtual void clearException();

    virtual void error(String *str);
    virtual void error(const char *format, ...);
    virtual void message(const char *format, ...);
    virtual void fatal(const char *format, ...);

private:
    class ErrorFrame : public Allocated {
    public:
	ErrorFrame(Frame *frame, Handler handler);

	Frame *f;			/* frame context */
	unsigned short offset;		/* sp offset */
	bool atomic;			/* atomic status */
	struct RLInfo *rlim;		/* rlimits info */
	Handler handler;		/* error handler */
	ErrorFrame *next;		/* next in linked list */
	jmp_buf env;			/* extension error env */
    };

    ErrorFrame *eFrame;			/* current error frame */
    ErrorFrame *atomicFrame;		/* first frame beyond atomic */
    String *err;			/* current error string */
};

extern ErrorContext *EC;
