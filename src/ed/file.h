typedef struct {
    long lines;	/* number of lines */
    long chars;	/* number of characters */
    long zero;	/* number of zeroes discarded */
    long split;	/* number of splits of too long lines */
    bool ill;	/* incomplete last line */
} io;

extern io *io_load P((editbuf*, char*, long));
extern io *io_save P((editbuf*, char*, long, long, bool));
