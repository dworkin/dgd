/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

class Connection {
public:
    virtual bool attach() = 0;
    virtual bool udp(char *challenge, unsigned int len) = 0;
    virtual void del() = 0;
    virtual void block(int flag) = 0;
    virtual void stop() = 0;
    virtual bool udpCheck() = 0;
    virtual int read(char *buf, unsigned int len) = 0;
    virtual int readUdp(char *buf, unsigned int len) = 0;
    virtual int write(char *buf, unsigned int len) = 0;
    virtual int writeUdp(char *buf, unsigned int len) = 0;
    virtual bool wrdone() = 0;
    virtual void ipnum(char *buf) = 0;
    virtual void ipname(char *buf) = 0;
    virtual int	checkConnected(int *errcode) = 0;
    virtual bool cexport(int *fd, char *addr, unsigned short *port, short *at,
			 int *npkts, int *bufsz, char **buf, char *flags) = 0;

    static bool init(int maxusers, char **thosts, char **bhosts, char **dhosts,
		     unsigned short *tports, unsigned short *bports,
		     unsigned short *dports, int ntports, int nbports,
		     int ndports);
    static void clear();
    static void finish();
    static void listen();
    static int select(Uint t, unsigned int mtime);
    static void *host(char *addr, unsigned short port, int *len);
    static int fdcount();
    static void fdlist(int *list);
    static void fdclose(int *list, int n);

    static Connection *createTelnet6(int port);
    static Connection *createTelnet(int port);
    static Connection *create6(int port);
    static Connection *create(int port);
    static Connection *createDgram6(int port);
    static Connection *createDgram(int port);
    static Connection *connect(void *addr, int len);
    static Connection *connectDgram(int uport, void *addr, int len);
    static Connection *import(int fd, char *addr, unsigned short port, short at,
			      int npkts, int bufsz, char *buf, char flags,
			      bool telnet);
};

class Comm {
public:
    static bool init(int n, int p, char **thosts, char **bhosts, char **dhosts,
		     unsigned short *tports, unsigned short *bports,
		     unsigned short *dports, int ntelnet, int nbinary,
		     int ndatagram);
    static void clear();
    static void finish();
    static void listen();
    static int send(Object *obj, String *str);
    static int udpsend(Object *obj, String *str);
    static bool echo(Object *obj, int echo);
    static void challenge(Object *obj, String *str);
    static void flush();
    static void block(Object *obj, int block);
    static void stop(Object *obj);
    static void receive(Frame*, Uint, unsigned int);
    static String *ipNumber(Object*);
    static String *ipName(Object*);
    static void close(Frame*, Object*);
    static Object *user();
    static void connect(Frame *f, Object *obj, char *addr, unsigned short port);
    static void connectDgram(Frame *f, Object *obj, int uport, char *addr,
			     unsigned short port);
    static eindex numUsers();
    static Array *listUsers(Dataspace*);
    static bool isConnection(Object*);
    static bool save(int);
    static bool restore(int);

private:
    static void acceptTelnet(Frame *f, Connection *conn, int port);
    static void accept(Frame *f, Connection *conn, int port);
    static void acceptDgram(Frame *f, Connection *conn, int port);
};
