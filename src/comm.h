# define INBUF_SIZE	2048
# define OUTBUF_SIZE	2048
# define BINBUF_SIZE	8192

typedef struct _connection_ connection;

extern void	   conn_init	P((int, unsigned int, unsigned int));
extern void	   conn_finish	P((void));
extern connection *conn_tnew	P((void));
extern connection *conn_bnew	P((void));
extern void	   conn_del	P((connection*));
extern int	   conn_select	P((int));
extern int	   conn_read	P((connection*, char*, unsigned int));
extern int	   conn_write	P((connection*, char*, unsigned int, int));
extern bool	   conn_wrdone	P((connection*));
extern char	  *conn_ipnum	P((connection*));

extern void	comm_init	P((int, unsigned int, unsigned int));
extern void	comm_finish	P((void));
extern int	comm_send	P((object*, string*));
extern void	comm_echo	P((object*, int));
extern void	comm_flush	P((int));
extern void	comm_receive	P((void));
extern string  *comm_ip_number	P((object*));
extern void	comm_close	P((object*));
extern object  *comm_user	P((void));
extern array   *comm_users	P((void));
extern bool	comm_active	P((void));
