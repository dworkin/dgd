# include "regexp.h"
# include "buffer.h"
# include "vars.h"

# define CB_NOIMAGE	0x01
# define CB_INSERT	0x02
# define CB_CHANGE	0x04
# define CB_GLOBAL	0x08
# define CB_EXCL	0x10
# define CB_NUMBER	0x20
# define CB_LIST	0x40
# define CB_RESTRICTED	0x80

typedef struct {
    char *cmd;			/* command to do */
    editbuf *edbuf;		/* edit buffer */
    rxbuf *regexp;		/* current regular expression */
    vars *vars;			/* variables */

    char flags;			/* status flags */
    Int edit;			/* number of edits on file */

    Int this;			/* current line number */
    Int othis;			/* current line number after last operation */
    Int first;			/* first line number of current range */
    Int last;			/* last line number of current range */

    Int a_addr;			/* argument address */
    char a_buffer;		/* argument buffer */

    Int mark[26];		/* line numbers of marks */
    block buf;			/* default yank buffer */
    block zbuf[26];		/* named buffers */

    char fname[STRINGSZ];	/* current filename */

    block undo;			/* undo block */
    Int uthis;			/* current line number after undo */
    Int umark[26];		/* marks after undo */

    char search[STRINGSZ];	/* pattern to search for */
    char replace[STRINGSZ];	/* string to replace with */
} cmdbuf;

# define RET_QUIT	1
# define RET_FLAGS	2

extern cmdbuf *cb_new     P((char*));
extern void    cb_del     P((cmdbuf*));
extern bool    cb_command P((cmdbuf*, char*));
extern int     cb_edit	  P((cmdbuf*));
