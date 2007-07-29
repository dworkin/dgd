# include "line.h"

typedef struct {
    linebuf *lb;		/* line buffer */
    block buffer;		/* the actual edit buffer */
    Int lines;			/* # lines in edit buffer */

    block flines;		/* block of first lines to add */
    int szlines;		/* size of "last" insert add */
    char *llines;		/* llbuf pointer */
    char llbuf[4 * MAX_LINE_SIZE]; /* last lines buffer */
} editbuf;

extern editbuf *eb_new		P((char*));
extern void	eb_del		P((editbuf*));
extern void	eb_clear	P((editbuf*));
extern void	eb_add		P((editbuf*, Int, char*(*)(void)));
extern block	eb_delete	P((editbuf*, Int, Int));
extern void	eb_change	P((editbuf*, Int, Int, block));
extern block	eb_yank		P((editbuf*, Int, Int));
extern void	eb_put		P((editbuf*, Int, block));
extern void	eb_range	P((editbuf*, Int, Int, void(*)(char*), int));
extern void	eb_startblock	P((editbuf*));
extern void	eb_addblock	P((editbuf*, char*));
extern void	eb_endblock	P((editbuf*));
