# define INBUF_SIZE	2048
# define OUTBUF_SIZE	1024

typedef struct _connection_ connection;

extern void	   conn_init	P((int, unsigned short, unsigned short));
extern void	   conn_finish	P((void));
extern connection *conn_tnew	P((void));
extern connection *conn_bnew	P((void));
extern void	   conn_del	P((connection*));
extern int	   conn_select	P((bool));
extern int	   conn_read	P((connection*, char*, int));
extern void	   conn_write	P((connection*, char*, int));
extern char	  *conn_ipnum	P((connection*));

extern void	comm_init	P((int, int, int));
extern void	comm_finish	P((void));
extern void	comm_send	P((object*, string*));
extern void	comm_echo	P((object*, bool));
extern void	comm_flush	P((bool));
extern object  *comm_receive	P((char*, int*));
extern string  *comm_ip_number	P((object*));
extern void	comm_close	P((object*));
extern object  *comm_user	P((void));
extern array   *comm_users	P((void));
