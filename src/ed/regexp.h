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

/*
 *   Regular expressions, ex-style. Allocating and freeing memory for each
 * regular expression would cause memory problems, so a buffer is allocated
 * instead in which a regular expression can be compiled.
 */
# define RXBUFSZ	2048
# define NSUBEXP	9

class RxBuf : public Allocated {
public:
    RxBuf();
    virtual ~RxBuf();

    const char *comp(const char *pattern);
    int exec(const char *text, int idx, bool ic);

    bool valid;			/* is the present matcher valid? */
    bool anchor;		/* is the match anchored (^pattern) */
    char firstc;		/* first character in match, if any */
    const char *start;		/* start of matching sequence */
    int size;			/* size of matching sequence */
    struct {
	const char *start;	/* start of subexpression */
	int size;		/* size of subexpression */
    } se[NSUBEXP];
    char buffer[RXBUFSZ];	/* buffer to hold matcher */

private:
    bool match(const char *start, const char *text, bool ic, char *m,
	       const char *t);
};
