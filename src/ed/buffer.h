# include "line.h"

typedef struct {
    linebuf *lb;		/* line buffer */
    block buffer;		/* the actual edit buffer */
    long lines;			/* # lines in edit buffer */

    block flines;		/* block of first lines to add */
    char llines[4 * MAX_LINE_SIZE];	/* last lines to add */
    int szlines;		/* size of "last" insert add */
} editbuf;

extern editbuf *eb_new		P((char*));
extern void	eb_del		P((editbuf*));
extern void	eb_clear	P((editbuf*));
extern void	eb_add		P((editbuf*, long, char*(void)));
extern block	eb_delete	P((editbuf*, long, long));
extern void	eb_change	P((editbuf*, long, long, block));
extern block	eb_yank		P((editbuf*, long, long));
extern void	eb_put		P((editbuf*, long, block));
extern void	eb_range	P((editbuf*, long, long, void(char*), bool));
extern void	eb_startblock	P((editbuf*));
extern void	eb_addblock	P((editbuf*, char*));
extern void	eb_endblock	P((editbuf*));
