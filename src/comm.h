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

extern bool	   conn_init	 P((int, char**, char**, unsigned short*,
				    unsigned short*, int, int));
extern void	   conn_finish	 P((void));
extern void	   conn_listen	 P((void));
extern connection *conn_tnew6	 P((int));
extern connection *conn_tnew	 P((int));
extern connection *conn_bnew6	 P((int));
extern connection *conn_bnew	 P((int));
extern bool	   conn_udp	 P((connection*, char*, unsigned int));
extern void	   conn_del	 P((connection*));
extern void	   conn_block	 P((connection*, int));
extern int	   conn_select	 P((Uint, unsigned int));
extern bool	   conn_udpcheck P((connection*));
extern int	   conn_read	 P((connection*, char*, unsigned int));
extern int	   conn_udpread	 P((connection*, char*, unsigned int));
extern int	   conn_write	 P((connection*, char*, unsigned int));
extern int	   conn_udpwrite P((connection*, char*, unsigned int));
extern bool	   conn_wrdone	 P((connection*));
extern void	   conn_ipnum	 P((connection*, char*));
extern void	   conn_ipname	 P((connection*, char*));

#ifdef NETWORK_EXTENSIONS
extern bool	comm_init	P((int, int, char**, char**, unsigned short*,
				   unsigned short*, int, int));
#else
extern bool	comm_init	P((int, char**, char**, unsigned short*,
				   unsigned short*, int, int));
#endif

extern void	comm_finish	P((void));
extern void	comm_listen	P((void));
extern int	comm_send	P((object*, string*));
extern int	comm_udpsend	P((object*, string*));
extern bool	comm_echo	P((object*, int));
extern void	comm_challenge	P((object*, string*));
extern void	comm_flush	P((void));
extern void	comm_block	P((object*, int));
extern void	comm_receive	P((frame*, Uint, unsigned int));
extern string  *comm_ip_number	P((object*));
extern string  *comm_ip_name	P((object*));
extern void	comm_close	P((frame*, object*));
extern object  *comm_user	P((void));
#ifdef NETWORK_EXTENSIONS
extern void	comm_openport	P((frame *f, object *obj, unsigned char protocol, 
				   unsigned short portnr));
extern void	comm_connect	P((frame *f, object *obj, char *addr, 
				   unsigned char protocol, unsigned short port));
extern connection *conn_connect P((char *addr, unsigned short port));
extern int 	comm_senddatagram P((object *obj, string *str, string *ip, int port));
extern connection * conn_openlisten P((unsigned char protocol, unsigned short port));
extern int 	conn_at		P((connection *conn));
extern int 	conn_checkaddr	P((char *ip));
extern int 	conn_udpsend	P((connection *conn, char *buf, unsigned int len, 
				   char *addr, unsigned short port));
extern int 	conn_check_connected P((connection *conn));
extern int 	conn_udpreceive P((connection *conn, char *buffer, int size, char **host, 
	 			   int *port));
extern connection *conn_accept	P((connection * conn));
extern bool     comm_is_connection P((object*));
extern array   *comm_users      P((dataspace *, bool));
#else
extern array   *comm_users	P((dataspace*));
#endif
