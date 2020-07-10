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

# include "dgd.h"
# include <signal.h>

extern "C" {

/*
 * catch SIGTERM
 */
static void term(int arg)
{
    signal(SIGTERM, term);
    DGD::interrupt();
}

}

/*
 * main program
 */
int main(int argc, char *argv[])
{
    long seed;
    unsigned short mtime;

    seed = P_mtime(&mtime);
    P_srandom(seed ^ ((long) mtime << 22));
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, term);
    return DGD::main(argc, argv);
}

/*
 * show message
 */
void P_message(const char *mess)
{
    fputs(mess, stderr);
    fflush(stderr);
}
