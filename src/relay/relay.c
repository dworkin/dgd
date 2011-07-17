/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 2011 DGD Authors (see the file Changelog for details)
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
# include "login.h"

/*
 * NAME:	main()
 * DESCRIPTION:	Relay can be run as follows:
 *
 *		    relay hostname port program+arguments < input > /dev/null
 *
 *		which would connect to the server at hostname:port, read lines
 *		from the file input and forward them to the server, and read
 *		and discard any output from the server received before the
 *		last line of input was read.
 *		Once all lines from the input file are copied to the server,
 *		the given program will be executed with the socket as stdin
 *		and stdout.
 */
int main(int argc, char *argv[], char *envp[])
{
    int port, socket;

    /*
     * check arguments
     */
    if (argc < 4) {
	fprintf(stderr, "Usage: %s hostname port program [arguments...]\n",
		argv[0]);
	return 2;
    }
    port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
	fprintf(stderr, "%s: bad port\n", argv[0]);
	return 2;
    }

    /*
     * login on server
     */
    socket = login_dgd(argv[1], port, 0, 1);
    if (socket < 0) {
	return 1;
    }

    /*
     * execute program
     */
    dup2(socket, 0);
    dup2(socket, 1);
    close(socket);
    execvp(argv[3], argv + 3);
    perror("execvp");
    return 1;
}
