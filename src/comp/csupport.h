typedef struct {
    char *name;			/* name of object */
    uindex progoffset;		/* program offset */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset */
    bool priv;			/* privately inherited? */
} pcinherit;

typedef void (*pcfunc) P((frame*));

typedef struct {
    uindex oindex;		/* precompiled object */

    short ninherits;		/* # of inherits */
    pcinherit *inherits;	/* inherits */

    uindex imapsz;		/* inherit map size */
    char *imap;			/* inherit map */

    Uint compiled;		/* compile time */

    unsigned short progsize;	/* program size */
    char *program;		/* program */

    unsigned short nstrings;	/* # of strings */
    dstrconst* sstrings;	/* string constants */
    char *stext;		/* string text */
    Uint stringsz;		/* string size */

    unsigned short nfunctions;	/* # functions */
    pcfunc *functions;		/* functions */

    short nfuncdefs;		/* # function definitions */
    dfuncdef *funcdefs;		/* function definitions */

    short nvardefs;		/* # variable definitions */
    short nclassvars;		/* # class variable definitions */
    dvardef *vardefs;		/* variable definitions */
    char *classvars;		/* variable classes */

    uindex nfuncalls;		/* # function calls */
    char *funcalls;		/* function calls */

    uindex nsymbols;		/* # symbols */
    dsymbol *symbols;		/* symbols */

    unsigned short nvariables;	/* # variables */
    char *vtypes;		/* variable types */

    short typechecking;		/* typechecking level */
} precomp;

extern precomp	*precompiled[];	/* table of precompiled objects */
extern pcfunc	*pcfunctions;	/* table of precompiled functions */


bool   pc_preload	P((char*, char*));
array *pc_list		P((dataspace*));
void   pc_control	P((control*, object*));
bool   pc_dump		P((int));
void   pc_restore	P((int, int));


# define PUSH_NUMBER		(--f->sp)->type = T_INT, f->sp->u.number =
# define push_lvalue(v, t)	((--f->sp)->type = T_LVALUE, \
				 f->sp->oindex = (t), f->sp->u.lval = (v))
# define push_lvclass(l)	(f->lip->type = T_INT, \
				 (f->lip++)->u.number = (l))
# define store()		(i_store(f), f->sp[1] = f->sp[0], f->sp++)
# define store_int()		(i_store(f), f->sp += 2, f->sp[-2].u.number)
# define i_foffset(n)		(&f->ctrl->funcalls[2L * (f->foffset + (n))])

/*
 * prototypes for kfuns that might be called directly from precompiled code
 */
int kf_add P((frame*)), kf_add1 P((frame*)), kf_and P((frame*)),
    kf_div P((frame*)), kf_eq P((frame*)), kf_ge P((frame*)), kf_gt P((frame*)),
    kf_le P((frame*)), kf_lshift P((frame*)), kf_lt P((frame*)),
    kf_mod P((frame*)), kf_mult P((frame*)), kf_ne P((frame*)),
    kf_neg P((frame*)), kf_not P((frame*)), kf_or P((frame*)),
    kf_rangeft P((frame*)), kf_rangef P((frame*)), kf_ranget P((frame*)),
    kf_range P((frame*)), kf_rshift P((frame*)), kf_sub P((frame*)),
    kf_sub1 P((frame*)), kf_tofloat P((frame*)), kf_toint P((frame*)),
    kf_tst P((frame*)), kf_umin P((frame*)), kf_xor P((frame*)),
    kf_tostring P((frame*)), kf_ckrangeft P((frame*)), kf_ckrangef P((frame*)),
    kf_ckranget P((frame*)), kf_sum P((frame*, int));

int kf_this_object P((frame*)), kf_call_trace P((frame*)),
    kf_this_user P((frame*)), kf_users P((frame*)), kf_time P((frame*)),
    kf_swapout P((frame*)), kf_dump_state P((frame*)), kf_shutdown P((frame*));

void call_kfun		P((frame*, int));
void call_kfun_arg	P((frame*, int, int));
Int  xdiv		P((Int, Int));
Int  xmod		P((Int, Int));
Int  xlshift		P((Int, Int));
Int  xrshift		P((Int, Int));
bool poptruthval	P((frame*));
void new_rlimits	P((frame*));
int  switch_range	P((Int, Int*, int));
int  switch_str		P((value*, control*, char*, int));
