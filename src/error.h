/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
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

class ErrorContext : public Allocated {
public:
    typedef void (*Handler) (Frame*, Int);

    static void push(Handler handler = NULL);
    static void pop();

    static void setException(String *err);
    static String *exception();
    static void clearException();

    Frame *f;				/* frame context */
    unsigned short offset;		/* sp offset */
    bool atomic;			/* atomic status */
    RLInfo *rlim;			/* rlimits info */
    Handler handler;			/* error handler */
    ErrorContext *next;			/* next in linked list */

private:
    ErrorContext(Frame *frame, Handler handler);

    static String *err;			/* current error string */
};

extern void error	(const char *format, ...);
extern void error	(String *str);
extern void message	(const char *, ...);
extern void fatal	(const char *, ...);
