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

# include <sys/time.h> 
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <errno.h>
# include "dgd.h"


/*
 * NAME:	login_dgd()
 * DESCRIPTION:	connect to DGD server, login, and return a socket
 */
int login_dgd(char *hostname, int port, int in, int out)
{
    char input[10240], output[10240];
    struct addrinfo hint, *res;
    struct sockaddr_in6 sin;
    int sock, i, n;
    fd_set fds;

    /*
     * get server address
     */
    memset(&hint, '\0', sizeof(struct addrinfo));
    hint.ai_family = PF_INET6;
    if (getaddrinfo(hostname, NULL, &hint, &res) != 0) {
	memset(&hint, '\0', sizeof(struct addrinfo));
	hint.ai_family = PF_INET;
	if (getaddrinfo(hostname, NULL, &hint, &res) != 0) {
	    fprintf(stderr, "unknown host: %s\n", hostname);
	    return -1;
	}
    }
    memcpy(&sin, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    sin.sin6_port = htons(port);

    /*
     * establish connection
     */
    sock = socket(sin.sin6_family, SOCK_STREAM, 0);
    if (sock < 0) {
	perror("socket");
	return -1;
    }
    if (connect(sock, (struct sockaddr *) &sin,
		(sin.sin6_family == AF_INET6) ?
		 sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)) < 0)
    {
	perror("connect");
	close(sock);
	return -1;
    }

    sleep(1);	/* give server time to send banner */
    memset(&fds, '\0', sizeof(fd_set));
    i = 0;

    for (;;) {
	/*
	 * wait for input
	 */
	FD_SET(in, &fds);
	FD_SET(sock, &fds);
	if (select(sock + 1, &fds, NULL, NULL, NULL) <= 0) {
	    perror("select");
	    close(sock);
	    return -1;
	}

	if (FD_ISSET(in, &fds)) {
	    /*
	     * input from stdin: read one character
	     */
	    n = read(in, input + i, 1);
	    if (n < 0) {
		perror("read");
		close(sock);
		return -1;
	    }
	    if (n == 0) {
		/*
		 * no more input; flush the input buffer if needed, and
		 * return the socket
		 */
		if (i != 0 && write(sock, input, i) != i) {
		    perror("socket write");
		    close(sock);
		    return -1;
		}
		return sock;
	    }
	}

	if (FD_ISSET(sock, &fds)) {
	    /*
	     * input from socket: copy directly to stdout
	     */
	    n = read(sock, output, sizeof(output));
	    if (n <= 0) {
		perror("socket read");
		close(sock);
		return -1;
	    }
	    if (write(out, output, n) != n) {
		perror("write");
		close(sock);
		return -1;
	    }
	}

	if (FD_ISSET(in, &fds) && (input[i++] == '\n' || i == sizeof(input))) {
	    /*
	     * forward a line of input to the server
	     */
	    if (write(sock, input, i) != i) {
		perror("socket write");
		close(sock);
		return -1;
	    }
	    i = 0;
	    sleep(1);	/* give server time to respond */
	}
    }
}
