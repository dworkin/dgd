# include "hash.h"
# include "swap.h"

struct _object_ {
    hte chain;			/* object name hash table */
    char flags;			/* object status */
    char etabi;			/* index in external table */
    uindex cref;		/* # clone references (sometimes) */
    uindex prev;		/* previous in issue list */
    uindex index;		/* index in object table */
    Uint count;			/* object creation count */
    Uint update;		/* object update count */
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
# define O_PENDIO		0x80

# define O_UPGRADING(o)		((o)->cref > (o)->u.ref)
# define O_INHERITED(o)		((o)->u.ref - 1 != (o)->cref)

extern void		   o_init	P((unsigned int));
extern object		  *o_new	P((char*, struct _control_*));
extern object		  *o_clone	P((object*));
extern void		   o_upgrade	P((object*, struct _control_*));
extern void		   o_upgraded	P((object*, object*));
extern void		   o_del	P((object*));
extern object		  *o_object	P((unsigned int, Uint));
extern object		  *o_objref	P((unsigned int));
extern char		  *o_name	P((object*));
extern object		  *o_find	P((char*));
extern struct _control_	  *o_control	P((object*));
extern struct _dataspace_ *o_dataspace	P((object*));
extern void		   o_clean	P((void));
extern uindex		   o_count	P((void));
extern bool		   o_dump	P((int));
extern void		   o_restore	P((int));
