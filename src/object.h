# include "hash.h"
# include "swap.h"

# define OBJ(i)			(&otable[i])
# define OBJR(env, i)		((BTST((env)->oe->ocmap, (i))) ? o_oread((env), (i)) : &otable[i])
# define OBJW(env, i)		((!(env)->oe->obase) ? o_owrite((env), (i)) : &otable[i])
# define OBJF(env, i)		OBJW((env), i)

# define O_CLONE		(UINDEX_MAX >> 1)
# define O_LWOBJ		(O_CLONE + 1)

# define O_UPGRADING(o)		(((o)->cref & O_CLONE) > (o)->u_ref)
# define O_INHERITED(o)		((o)->u_ref - 1 != ((o)->cref & O_CLONE))

# define OACC_READ		0x00	/* read access */
# define OACC_REFCHANGE		0x01	/* modify refcount */
# define OACC_MODIFY		0x02	/* write access */

# define OBJ_NONE		UINDEX_MAX


struct _object_ {
    hte chain;			/* object name hash table */
    char flags;			/* object status */
    eindex etabi;		/* index in external table */
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

# define O_SPECIAL		0x30

# define OBJ_LAYOUT		"xceuuuiiippdd"

typedef struct _objenv_ {
    struct _objplane_ *plane;	/* current object plane */
    char *ocmap;		/* object change map */
    bool obase;			/* object base plane flag */
    Uint odcount;		/* objects destructed count */
} objenv;


extern void	  o_init		P((unsigned int));
extern objenv	 *o_new_env		P((void));
extern object	 *o_oread		P((lpcenv*, unsigned int));
extern object	 *o_owrite		P((lpcenv*, unsigned int));
extern void	  o_new_plane		P((lpcenv*));
extern void	  o_commit_plane	P((lpcenv*));
extern void	  o_discard_plane	P((lpcenv*));

extern bool	  o_space		P((lpcenv*));
extern object	 *o_new			P((lpcenv*, char*, control*));
extern object	 *o_clone		P((lpcenv*, object*));
extern void	  o_lwobj		P((object*));
extern void	  o_upgrade		P((object*, control*, frame*));
extern void	  o_upgraded		P((object*, object*));
extern void	  o_del			P((object*, frame*));

extern char	 *o_name		P((lpcenv*, char*, object*));
extern object	 *o_find		P((lpcenv*, char*, int));
extern control   *o_control		P((lpcenv*, object*));
extern dataspace *o_dataspace		P((lpcenv*, object*));

extern void	  o_clean		P((void));
extern uindex	  o_count		P((lpcenv*));
extern bool	  o_dump		P((int));
extern void	  o_restore		P((lpcenv*, int, unsigned int));
extern void	  o_conv		P((lpcenv*));

extern object    *otable;
