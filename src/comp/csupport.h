typedef void (*pcfunc)();

typedef struct {
    short ninherits;		/* # of inherits */
    char **inherits;		/* inherits */

    unsigned short progsize;	/* program size */
    char *program;		/* program */

    unsigned short nstrings;	/* # of strings */
    char *stext;		/* string text */
    unsigned short *slength;	/* string lengths */

    unsigned short nfunctions;	/* # functions */
    pcfunc *functions;		/* functions */

    short nfuncdefs;		/* # function definitions */
    dfuncdef *funcdefs;		/* function definitions */

    short nvardefs;		/* # variable definitions */
    dvardef *vardefs;		/* variable definitions */

    uindex nfuncalls;		/* # function calls */
    char *funcalls;		/* function calls */
} precomp;

extern precomp	*precompiled[];	/* table of precompiled objects */
extern pcfunc	*pcfunctions;	/* table of precompiled functions */


void preload		P((void));

# define PUSH_NUMBER	(--sp)->type = T_NUMBER, sp->u.number =
# define push_lvalue(v)	((--sp)->type = T_LVALUE, sp->u.lval = (v))
# define store()	(i_store(sp + 1, sp), sp[1] = sp[0], sp++)
# define store_int()	(i_store(sp + 1, sp), sp += 2, sp[-2].u.number)
# define truthval(v)	((v)->type != T_NUMBER || (v)->u.number != 0)

void call_kfun		P((int));
void call_kfun_arg	P((int, int));
void check_int		P((value*));
Int  xdiv		P((Int, Int));
Int  xmod		P((Int, Int));
bool poptruthval	P((void));
void pre_catch		P((void));
void post_catch		P((void));
int  switch_range	P((Int, Int*, int));
int  switch_str		P((value*, char*, int));
