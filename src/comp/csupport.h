typedef struct {
    char *name;			/* name of object */
    unsigned short funcoffset;	/* function call offset */
    unsigned short varoffset;	/* variable offset */
} pcinherit;

typedef void (*pcfunc) P((void));

typedef struct {
    object *obj;		/* precompiled object */

    short ninherits;		/* # of inherits */
    pcinherit *inherits;	/* inherits */

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
    dvardef *vardefs;		/* variable definitions */

    uindex nfuncalls;		/* # function calls */
    char *funcalls;		/* function calls */

    uindex nsymbols;		/* # symbols */
    dsymbol *symbols;		/* symbols */

    unsigned short nvariables;	/* # variables */
    unsigned short nfloatdefs;	/* # float definitions */
    unsigned short nfloats;	/* # floats */
} precomp;

extern precomp	*precompiled[];	/* table of precompiled objects */
extern pcfunc	*pcfunctions;	/* table of precompiled functions */


void   pc_preload	P((char*, char*));
array *pc_list		P((void));
void   pc_control	P((control*, object*));
bool   pc_dump		P((int));
void   pc_restore	P((int));


# define PUSH_NUMBER		(--sp)->type = T_INT, sp->u.number =
# define push_lvalue(v, t)	((--sp)->type = T_LVALUE, sp->oindex = (t), \
				 sp->u.lval = (v))
# define store()		(i_store(sp + 1, sp), sp[1] = sp[0], sp++)
# define store_int()		(i_store(sp + 1, sp), sp += 2, sp[-2].u.number)
# define truthval(v)		(((v)->type != T_INT || (v)->u.number != 0) && \
			 	((v)->type != T_FLOAT || !VFLT_ISZERO(v)))
# define i_foffset(n)		(&f->ctrl->funcalls[2L * (f->foffset + (n))])

/*
 * prototypes for kfuns that might be called directly from precompiled code
 */
int kf_add P((void)), kf_add1 P((void)), kf_and P((void)), kf_div P((void)),
    kf_eq P((void)), kf_ge P((void)), kf_gt P((void)), kf_le P((void)),
    kf_lshift P((void)), kf_lt P((void)), kf_mod P((void)), kf_mult P((void)),
    kf_ne P((void)), kf_neg P((void)), kf_not P((void)), kf_or P((void)),
    kf_rangeft P((void)), kf_rangef P((void)), kf_ranget P((void)),
    kf_range P((void)), kf_rshift P((void)), kf_sub P((void)),
    kf_sub1 P((void)), kf_tofloat P((void)), kf_toint P((void)),
    kf_tst P((void)), kf_umin P((void)), kf_xor P((void)),
    kf_tostring P((void)), kf_ckrangeft P((void)), kf_ckrangef P((void)),
    kf_ckranget P((void)), kf_sum P((int));

int kf_this_object P((void)), kf_call_trace P((void)), kf_this_user P((void)),
    kf_users P((void)), kf_time P((void)), kf_swapout P((void)),
    kf_dump_state P((void)), kf_shutdown P((void));

void call_kfun		P((int));
void call_kfun_arg	P((int, int));
Int  xdiv		P((Int, Int));
Int  xmod		P((Int, Int));
bool poptruthval	P((void));
void pre_catch		P((void));
void post_catch		P((int));
int  pre_rlimits	P((void));
int  switch_range	P((Int, Int*, int));
int  switch_str		P((value*, char*, int));
