# include "hash.h"
# include "swap.h"

struct _object_ {
    hte chain;			/* object name hash table */
    char flags;			/* object status */
    char eduser;		/* index in user/editor array */
    uindex index;		/* index in object table */
    Uint count;			/* object creation count */
    union {
	long ref;		/* ref count (if master object) */
	struct _object_ *master;/* pointer to master object */
    } u;
    struct _control_ *ctrl;	/* control block (master object only) */
    struct _dataspace_ *data;	/* dataspace block */
    sector cfirst;		/* first sector of control block */
    sector dfirst;		/* first sector of dataspace block */
};

# define O_MASTER		0x01
# define O_AUTO			0x02
# define O_DRIVER		0x04
# define O_CREATED		0x08
# define O_USER			0x10
# define O_EDITOR		0x20
# define O_COMPILED		0x40

extern void		   o_init	P((int));
extern object		  *o_new	P((char*, object*, struct _control_*));
extern void		   o_del	P((object*));
extern object		  *o_object	P((uindex, Int));
extern char		  *o_name	P((object*));
extern void		   o_rename	P((object*, char*));
extern object		  *o_find	P((char*));
extern struct _control_	  *o_control	P((object*));
extern struct _dataspace_ *o_dataspace	P((object*));
extern void		   o_clean	P((void));
extern uindex		   o_count	P((void));
extern bool		   o_dump	P((int));
extern void		   o_restore	P((int, long));
