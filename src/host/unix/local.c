/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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

/*
 * NAME:	term()
 * DESCRIPTION:	catch SIGTERM
 */
static void term()
{
    signal(SIGTERM, term);
    interrupt();
}

/*
 * NAME:	main()
 * DESCRIPTION:	main program
 */
int main(argc, argv)
int argc;
char *argv[];
{
    P_srandom((long) P_time());
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, term);
    return dgd_main(argc, argv);
}

/*
 * NAME:	P->message()
 * DESCRIPTION:	show message
 */
void P_message(mess)
char *mess;
{
    fputs(mess, stderr);
    fflush(stderr);
}
