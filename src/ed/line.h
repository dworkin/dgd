/*
 *   The basic data type is a line buffer, in which blocks of lines are
 * allocated. The line buffer can be made inactive, to make it use as little
 * system resources as possible.
 *   Blocks can be created, deleted, queried for their size, split in two, or
 * concatenated. Blocks are never actually deleted in a line buffer, but a
 * fake delete operation is added for the sake of completeness.
 */
typedef Int block;

typedef struct _btbuf_ {
    long offset;			/* offset in tmpfile */
    struct _btbuf_ *prev;		/* prev in linked list */
    struct _btbuf_ *next;		/* next in linked list */
    char *buf;				/* buffer with blocks and text */
} btbuf;

typedef struct {
    char *file;				/* tmpfile name */
    int fd;				/* tmpfile fd */
    char *buf;				/* current low-level buffer */
    int blksz;				/* block size in write buffer */
    int txtsz;				/* text size in write buffer */
    void (*putline) P((char*, char*));	/* output line function */
    char *context;			/* context for putline */
    bool reverse;			/* for bk_put() */
    btbuf *wb;				/* write buffer */
    btbuf bt[NR_EDBUFS];		/* read & write buffers */
} linebuf;

extern linebuf *lb_new	  P((linebuf*, char*));
extern void	lb_del	  P((linebuf*));
extern void	lb_inact  P((linebuf*));

extern block	bk_new	  P((linebuf*, char*(*)(char*), char*));
# define	bk_del(linebuf, block)	/* nothing */
extern Int	bk_size	  P((linebuf*, block));
extern void	bk_split  P((linebuf*, block, Int, block*, block*));
extern block	bk_cat	  P((linebuf*, block, block));
extern void	bk_put	  P((linebuf*, block, Int, Int, void(*)(char*, char*),
			     char*, int));
