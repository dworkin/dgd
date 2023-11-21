/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 2021-2023 DGD Authors (see the commit log for details)
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


static Alloc EDMM;
Alloc *MM = &EDMM;

static ErrorContext EDEC;
ErrorContext *EDC = &EDEC;

static Path EDPM;
Path *PM = &EDPM;

/*
 * stand-alone editor
 */
int main(int argc, char *argv[])
{
    char tmp[100], line[2048], *p;
    CmdBuf *ed;

    snprintf(tmp, sizeof(tmp), "/tmp/ed%05d", (int) getpid());
    ed = new CmdBuf(tmp);
    if (argc > 1) {
	snprintf(line, sizeof(line), "e %s", argv[1]);
	try {
	    ed->command(line);
	} catch (const char*) { }
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
	} catch (const char*) { }
    }

    delete ed;
    return 0;
}
