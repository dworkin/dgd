/*
 *   Regular expressions, ex-style. Allocating and freeing memory for each
 * regular expression would cause memory problems, so a buffer is allocated
 * instead in which a regular expression can be compiled.
 */
# define RXBUFSZ	2048
# define NSUBEXP	9

typedef struct {
    bool valid;			/* is the present matcher valid? */
    bool anchor;		/* is the match anchored (^pattern) */
    char firstc;		/* first character in match, if any */
    char *start;		/* start of matching sequence */
    int size;			/* size of matching sequence */
    struct {
	char *start;		/* start of subexpression */
	int size;		/* size of subexpression */
    } se[NSUBEXP];
    char buffer[RXBUFSZ];	/* buffer to hold matcher */
} rxbuf;

extern rxbuf *rx_new  P((void));
extern void   rx_del  P((rxbuf*));
extern char  *rx_comp P((rxbuf*, char*));
extern int    rx_exec P((rxbuf*, char*, int, int));
