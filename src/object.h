# include "hash.h"
# include "swap.h"

typedef struct _object_ {
    hte chain;			/* object name hash table */
    char flags;			/* object status */
    char connection;		/* index in connection array (if any) */
    objkey key;			/* object key */
    union {
	long ref;		/* ref count (if master object) */
	struct _object_ *master;/* pointer to master object */
    } u;
    struct _control_ *ctrl;	/* control block (master object only) */
    struct _dataspace_ *data;	/* dataspace block */
    sector cfirst;		/* first sector of control block */
    sector dfirst;		/* first sector of dataspace block */
} object;

# define O_MASTER		0x01
# define O_DESTRUCTED		0x02
# define O_CONNECTED		0x04

extern void		   o_init	P((int));
extern object		  *o_new	P((char*, object*, struct _control_*));
extern void		   o_del	P((object*));
extern object		  *o_object	P((objkey*));
extern char		  *o_name	P((object*));
extern void		   o_rename	P((object*, char*));
extern object		  *o_find	P((char*));
extern struct _control_	  *o_control	P((object*));
extern struct _dataspace_ *o_dataspace	P((object*));
extern void		   o_clean	P((void));
extern uindex		   o_count	P((void));

# define DESTRUCTED(o)		(o_object(&(o)) == (object *) NULL)
