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

# include "lex.h"
# include "macro.h"
# include "token.h"
# include "special.h"

/*
 * Predefined macro handling.
 */

static char datestr[14];
static char timestr[11];

/*
 * predefine macros
 */
void Special::define()
{
    char buf[26];

    Macro::define("__LINE__", (char *) NULL, -1);
    Macro::define("__FILE__", (char *) NULL, -1);
    Macro::define("__DATE__", (char *) NULL, -1);
    Macro::define("__TIME__", (char *) NULL, -1);

    P_ctime(buf, P_time());
    snprintf(datestr, sizeof(datestr), "\"%.6s %.4s\"", buf + 4, buf + 20);
    snprintf(timestr, sizeof(timestr), "\"%.8s\"", buf + 11);
}

/*
 * return the expansion of a predefined macro
 */
char *Special::replace(const char *name)
{
    static char buf[STRINGSZ + 3];

    if (strcmp(name, "__LINE__") == 0) {
	snprintf(buf, sizeof(buf), " %u ", TokenBuf::line());
	return buf;
    } else if (strcmp(name, "__FILE__") == 0) {
	snprintf(buf, sizeof(buf), "\"%s\"", TokenBuf::filename());
	return buf;
    } else if (strcmp(name, "__DATE__") == 0) {
	return datestr;
    } else if (strcmp(name, "__TIME__") == 0) {
	return timestr;
    }
    return (char *) NULL;
}
