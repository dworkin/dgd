/*
 *   The basic data type is a line buffer, in which blocks of lines are
 * allocated. The line buffer can be made inactive, to make it use as little
 * system resources as possible.
 *   Blocks can be created, deleted, queried for their size, split in two, or
 * concatenated. Blocks are never actually deleted in a line buffer, but a
 * fake delete operation is added for the sake of completeness.
 */
typedef Int block;

extern struct _linebuf_*lb_new	 P((struct _linebuf_*, char*));
extern void		lb_del	 P((struct _linebuf_*));
extern void		lb_inact P((struct _linebuf_*));

extern block		bk_new	 P((struct _linebuf_*, char*(*)(char*), char*));
extern Int		bk_size	 P((struct _linebuf_*, block));
extern void		bk_split P((struct _linebuf_*, block, Int, block*,
				    block*));
extern block		bk_cat	 P((struct _linebuf_*, block, block));
extern void		bk_put	 P((struct _linebuf_*, block, Int, Int,
				    void(*)(char*, char*), char*, int));
