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
    Uint ref;			/* ref count (if master object) */
    control *ctrl;		/* control block (master object only) */
    dataspace *data;		/* dataspace block */
    sector cfirst;		/* first sector of control block */
    sector dfirst;		/* first sector of dataspace block */
};
# define u_ref			ref
# define u_master		ref

# define O_MASTER		0x01
# define O_AUTO			0x02
# define O_DRIVER		0x04
# define O_CREATED		0x08
# define O_USER			0x10
# define O_EDITOR		0x20
# define O_COMPILED		0x40
# define O_PENDIO		0x80

# define OBJ_LAYOUT		"xccuuuiiippdd"

# define O_UPGRADING(o)		((o)->cref > (o)->u_ref)
# define O_INHERITED(o)		((o)->u_ref - 1 != (o)->cref)

extern void	  o_init	P((unsigned int));
extern object	 *o_new		P((char*, control*));
extern object	 *o_clone	P((object*));
extern void	  o_upgrade	P((object*, control*, frame*));
extern void	  o_upgraded	P((object*, object*));
extern void	  o_del		P((object*, frame*));
extern char	 *o_name	P((char*, object*));
extern object	 *o_find	P((char*));
extern control   *o_control	P((object*));
extern dataspace *o_dataspace	P((object*));
extern void	  o_clean	P((void));
extern uindex	  o_count	P((void));
extern bool	  o_dump	P((int));
extern void	  o_restore	P((int));
extern void	  o_conv	P((void));

extern object    *otable;
extern Uint	  odcount;
