/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2012 DGD Authors (see the commit log for details)
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

# define  P_TCP      6
# define  P_UDP      17
# define  P_TELNET   1

typedef struct _connection_ connection;

extern bool	   conn_init	 (int, char**, char**, unsigned short*,
				    unsigned short*, int, int);
extern void	   conn_clear	 (void);
extern void	   conn_finish	 (void);
#ifndef NETWORK_EXTENSIONS
extern void	   conn_listen	 (void);
extern connection *conn_tnew6	 (int);
extern connection *conn_tnew	 (int);
extern connection *conn_bnew6	 (int);
extern connection *conn_bnew	 (int);
extern bool	   conn_udp	 (connection*, char*, unsigned int);
#endif
extern void	   conn_del	 (connection*);
extern void	   conn_block	 (connection*, int);
extern int	   conn_select	 (Uint, unsigned int);
#ifndef NETWORK_EXTENSIONS
extern bool	   conn_udpcheck (connection*);
#endif
extern int	   conn_read	 (connection*, char*, unsigned int);
extern int	   conn_udpread	 (connection*, char*, unsigned int);
extern int	   conn_write	 (connection*, char*, unsigned int);
extern int	   conn_udpwrite (connection*, char*, unsigned int);
extern bool	   conn_wrdone	 (connection*);
extern void	   conn_ipnum	 (connection*, char*);
extern void	   conn_ipname	 (connection*, char*);
extern void	  *conn_host	 (char*, unsigned short, int*);
extern connection *conn_connect	 (void*, int);
extern int	   conn_check_connected (connection*, bool*);
# ifdef NETWORK_EXTENSIONS
extern connection *conn_openlisten (unsigned char, unsigned short);
extern int	   conn_at	 (connection*);
extern int	   conn_checkaddr (char*);
extern int	   conn_udpsend	 (connection*, char*, unsigned int, char*,
				   unsigned short);
extern int	   conn_udpreceive (connection*, char*, int, char**, int*);
extern connection *conn_accept	 (connection*);
# endif
extern bool	   conn_export	 (connection*, int*, unsigned short*, short*,
				  int*, int*, char**, char*);
extern connection *conn_import	 (int, unsigned short, short, int, int, char*,
				  char, bool);

#ifdef NETWORK_EXTENSIONS
extern bool	comm_init	(int, int, char**, char**, unsigned short*,
				   unsigned short*, int, int);
#else
extern bool	comm_init	(int, char**, char**, unsigned short*,
				   unsigned short*, int, int);
#endif

extern void	comm_clear	(void);
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
extern void	comm_connect	(frame *f, object *obj, char *addr,
				   unsigned char protocol, unsigned short port);
#ifdef NETWORK_EXTENSIONS
extern void	comm_openport	(frame *f, object *obj, unsigned char protocol,
				   unsigned short portnr);
extern int	comm_senddatagram (object *obj, string *str, string *ip, int port);
extern array   *comm_ports      (dataspace*);
#endif
extern array   *comm_users	(dataspace*);
extern bool     comm_is_connection (object*);
extern bool	comm_dump	(int);
extern bool	comm_restore	(int);
