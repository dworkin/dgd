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

class ErrorContext {
public:
    typedef void (*Handler) (Frame*, Int);

    virtual jmp_buf *push(Handler handler = NULL) = 0;
    virtual void pop() = 0;

    virtual void setException(String *err) = 0;
    virtual String *exception() = 0;
    virtual void clearException() = 0;

    virtual void error(String *str) = 0;
    virtual void error(const char *format, ...) = 0;
    virtual void message(const char *format, ...) = 0;
    virtual void fatal(const char *format, ...) = 0;

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

extern ErrorContext *ec;
