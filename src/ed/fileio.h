typedef struct {
    Int lines;	/* number of lines */
    Int chars;	/* number of characters */
    Int zero;	/* number of zeroes discarded */
    Int split;	/* number of splits of too long lines */
    bool ill;	/* incomplete last line */
} io;

extern bool io_load P((editbuf*, lpcenv*, char*, Int, io*));
extern bool io_save P((editbuf*, lpcenv*, char*, Int, Int, int, io*));
