# define INBUF_SIZE	2048
# define OUTBUF_SIZE	2048
# define TELBUF_SIZE	128
# define BINBUF_SIZE	8192

typedef struct _connection_ connection;

extern bool	   conn_init	P((int, unsigned int, unsigned int));
extern void	   conn_finish	P((void));
extern void	   conn_listen	P((void));
extern connection *conn_tnew	P((void));
extern connection *conn_bnew	P((void));
extern void	   conn_del	P((connection*));
extern void	   conn_block	P((connection*, int));
extern int	   conn_select	P((int));
extern int	   conn_read	P((connection*, char*, unsigned int));
extern int	   conn_write	P((connection*, char*, unsigned int));
extern bool	   conn_wrdone	P((connection*));
extern char	  *conn_ipnum	P((connection*));

extern bool	comm_init	P((int, unsigned int, unsigned int));
extern void	comm_finish	P((void));
extern void	comm_listen	P((void));
extern int	comm_send	P((object*, string*));
extern void	comm_echo	P((object*, int));
extern void	comm_flush	P((int));
extern void	comm_block	P((object*, int));
extern void	comm_receive	P((frame*, int));
extern string  *comm_ip_number	P((object*));
extern void	comm_close	P((frame*, object*));
extern object  *comm_user	P((void));
extern array   *comm_users	P((dataspace*));
extern bool	comm_active	P((void));
