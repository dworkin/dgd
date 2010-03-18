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

#ifdef NETWORK_EXTENSIONS
# define  P_TCP      6
# define  P_UDP      17
# define  P_TELNET   1
#endif

typedef struct _connection_ connection;

extern bool	   conn_init	 (int, char**, char**, unsigned short*,
				    unsigned short*, int, int);
extern void	   conn_finish	 (void);
extern void	   conn_listen	 (void);
extern connection *conn_tnew6	 (int);
extern connection *conn_tnew	 (int);
extern connection *conn_bnew6	 (int);
extern connection *conn_bnew	 (int);
extern bool	   conn_udp	 (connection*, char*, unsigned int);
extern void	   conn_del	 (connection*);
extern void	   conn_block	 (connection*, int);
extern int	   conn_select	 (Uint, unsigned int);
extern bool	   conn_udpcheck (connection*);
extern int	   conn_read	 (connection*, char*, unsigned int);
extern int	   conn_udpread	 (connection*, char*, unsigned int);
extern int	   conn_write	 (connection*, char*, unsigned int);
extern int	   conn_udpwrite (connection*, char*, unsigned int);
extern bool	   conn_wrdone	 (connection*);
extern void	   conn_ipnum	 (connection*, char*);
extern void	   conn_ipname	 (connection*, char*);

#ifdef NETWORK_EXTENSIONS
extern bool	comm_init	(int, int, char**, char**, unsigned short*,
				   unsigned short*, int, int);
#else
extern bool	comm_init	(int, char**, char**, unsigned short*,
				   unsigned short*, int, int);
#endif

extern void	comm_finish	(void);
extern void	comm_listen	(void);
extern int	comm_send	(object*, string*);
extern int	comm_udpsend	(object*, string*);
extern bool	comm_echo	(object*, int);
extern void	comm_challenge	(object*, string*);
extern void	comm_flush	(void);
extern void	comm_block	(object*, int);
extern void	comm_receive	(frame*, Uint, unsigned int);
extern string  *comm_ip_number	(object*);
extern string  *comm_ip_name	(object*);
extern void	comm_close	(frame*, object*);
extern object  *comm_user	(void);
#ifdef NETWORK_EXTENSIONS
extern void	comm_openport	(frame *f, object *obj, unsigned char protocol, 
				   unsigned short portnr);
extern void	comm_connect	(frame *f, object *obj, char *addr, 
				   unsigned char protocol, unsigned short port);
extern connection *conn_connect (char *addr, unsigned short port);
extern int 	comm_senddatagram (object *obj, string *str, string *ip, int port);
extern connection * conn_openlisten (unsigned char protocol, unsigned short port);
extern int 	conn_at		(connection *conn);
extern int 	conn_checkaddr	(char *ip);
extern int 	conn_udpsend	(connection *conn, char *buf, unsigned int len, 
				   char *addr, unsigned short port);
extern int 	conn_check_connected (connection *conn);
extern int 	conn_udpreceive (connection *conn, char *buffer, int size, char **host, 
	 			   int *port);
extern bool     comm_is_connection (object*);
extern array   *comm_users      (dataspace *, bool);
#else
extern array   *comm_users	(dataspace*);
#endif
