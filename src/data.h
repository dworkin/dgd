# include "swap.h"

typedef struct {
    uindex oindex;		/* inherited object */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset */
    bool priv;			/* privately inherited? */
} dinherit;

typedef struct {
    Uint index;			/* index in control block */
    ssizet len;			/* string length */
} dstrconst;

# define DSTR_LAYOUT	"it"

typedef struct _dfuncdef_ {
    char class;			/* function class */
    char inherit;		/* function name inherit index */
    unsigned short index;	/* function name index */
    Uint offset;		/* offset in program text */
} dfuncdef;

# define DF_LAYOUT	"ccsi"

typedef struct {
    char class;			/* variable class */
    char inherit;		/* variable name inherit index */
    unsigned short index;	/* variable name index */
    unsigned short type;	/* variable type */
} dvardef;

# define DV_LAYOUT	"ccss"

typedef struct {
    char inherit;		/* function object index */
    char index;			/* function index */
    unsigned short next;	/* next in hash table */
} dsymbol;

# define DSYM_LAYOUT	"ccs"

struct _control_ {
    control *prev, *next;
    uindex ndata;		/* # of data blocks using this control block */

    struct _scontrol_ *sctrl;	/* swap control block */

    uindex oindex;		/* object */

    short flags;		/* various bitflags */

    short ninherits;		/* # inherited objects */
    dinherit *inherits;		/* inherit objects */

    Uint compiled;		/* time of compilation */

    char *prog;			/* program text */
    Uint progsize;		/* program text size */

    unsigned short nstrings;	/* # strings */
    string **strings;		/* string table */
    dstrconst *sstrings;	/* sstrings */
    char *stext;		/* sstrings text */
    Uint strsize;		/* sstrings text size */

    unsigned short nfuncdefs;	/* # function definitions */
    dfuncdef *funcdefs;		/* function definition table */

    unsigned short nvardefs;	/* # variable definitions */
    dvardef *vardefs;		/* variable definitions */

    uindex nfuncalls;		/* # function calls */
    char *funcalls;		/* function calls */

    unsigned short nsymbols;	/* # symbols */
    dsymbol *symbols;		/* symbol table */

    unsigned short nvariables;	/* # variables */
    unsigned short nifdefs;	/* # int/float definitions */
    unsigned short nvinit;	/* # variables requiring initialization */

    unsigned short vmapsize;	/* size of variable mapping */
    unsigned short *vmap;	/* variable mapping */
};

# define NEW_INT		((unsigned short) -1)
# define NEW_FLOAT		((unsigned short) -2)
# define NEW_POINTER		((unsigned short) -3)
# define NEW_VAR(x)		((x) >= NEW_POINTER)

typedef struct _strref_ {
    string *str;		/* string value */
    dataspace *data;		/* dataspace this string is in */
    Uint ref;			/* # of refs */
} strref;

typedef struct _arrref_ {
    array *arr;			/* array value */
    dataplane *plane;		/* value plane this array is in */
    dataspace *data;		/* dataspace this array is in */
    short state;		/* state of mapping */
    Uint ref;			/* # of refs */
} arrref;

struct _value_ {
    char type;			/* value type */
    bool modified;		/* dirty bit */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Uint objcnt;		/* object creation count */
	string *string;		/* string */
	array *array;		/* array or mapping */
	value *lval;		/* lvalue: variable */
    } u;
};

typedef struct {
    Uint time;			/* time of call */
    unsigned short nargs;	/* number of arguments */
    value val[4];		/* function name, 3 direct arguments */
} dcallout;

# define co_prev	time
# define co_next	nargs

struct _dataplane_ {
    Int level;			/* dataplane level */

    short flags;		/* modification flags */
    long schange;		/* # string changes */
    long achange;		/* # array changes */
    long imports;		/* # array imports */

    value *original;		/* original variables */
    arrref alocal;		/* primary of new local arrays */
    arrref *arrays;		/* arrays */
    struct _abchunk_ *achunk;	/* chunk of array backup info */
    strref *strings;		/* string constant table */
    struct _coptable_ *coptab;	/* callout patch table */

    dataplane *prev;		/* previous in per-dataspace linked list */
    dataplane *plist;		/* next in per-level linked list */
};

struct _dataspace_ {
    dataspace *prev, *next;	/* swap list */
    dataspace *gcprev, *gcnext;	/* garbage collection list */

    lpcenv *env;		/* LPC execution environment */
    dataspace *iprev;		/* previous in import list */
    dataspace *inext;		/* next in import list */

    struct _sdataspace_ *sdata;	/* swap dataspace */
    control *ctrl;		/* control block */
    uindex oindex;		/* object this dataspace belongs to */

    unsigned short nvariables;	/* # variables */
    value *variables;		/* variables */

    Uint narrays;		/* # arrays */
    struct _sarray_ *sarrays;	/* sarrays */
    struct _svalue_ *selts;	/* sarray elements */
    array alist;		/* array linked list sentinel */
  
    Uint nstrings;		/* # strings */
    Uint strsize;		/* total size of string text */
    struct _sstring_ *sstrings;	/* sstrings */
    char *stext;		/* sstrings text */

    uindex ncallouts;		/* # callouts */
    uindex fcallouts;		/* free callout list */
    dcallout *callouts;		/* callouts */

    dataplane base;		/* basic value plane */
    dataplane *plane;		/* current value plane */

    struct _parser_ *parser;	/* parse_string data */
};

# define THISPLANE(a)		((a)->plane == (a)->data->plane)
# define SAMEPLANE(d1, d2)	((d1)->plane->level == (d2)->plane->level)

extern void		 d_init			P((bool));
extern struct _dataenv_ *d_new_env		P((void));

extern control	        *d_new_control		P((lpcenv*));
extern dataspace        *d_alloc_dataspace	P((lpcenv*, object*));
extern dataspace        *d_new_dataspace	P((lpcenv*, object*));
extern void		 d_ref_control		P((lpcenv*, control*));
extern void		 d_ref_dataspace	P((dataspace*));

extern char	        *d_get_prog		P((control*));
extern string	        *d_get_strconst		P((lpcenv*, control*, int,
						   unsigned int));
extern dfuncdef         *d_get_funcdefs		P((control*));
extern dvardef	        *d_get_vardefs		P((control*));
extern char	        *d_get_funcalls		P((control*));
extern dsymbol	        *d_get_symbols		P((control*));
extern Uint		 d_get_progsize		P((control*));

extern void		 d_get_values		P((dataspace*, struct _svalue_*,
						   value*, unsigned int));
extern void		 d_new_variables	P((lpcenv*, control*, value*));
extern value	        *d_get_variables	P((dataspace*));
extern value	        *d_get_elts		P((array*));

extern void		 d_new_plane		P((dataspace*, Int));
extern void		 d_commit_plane		P((lpcenv*, Int, value*));
extern void		 d_discard_plane	P((lpcenv*, Int));
extern struct _abchunk_**d_commit_arr		P((array*, dataplane*,
						   dataplane*));
extern void		 d_discard_arr		P((array*, dataplane*));

extern void		 d_ref_imports		P((array*));
extern void		 d_assign_var		P((dataspace*, value*, value*));
extern value	        *d_get_extravar		P((dataspace*));
extern void		 d_set_extravar		P((dataspace*, value*));
extern void		 d_wipe_extravar	P((dataspace*));
extern void		 d_assign_elt		P((dataspace*, array*, value*,
						   value*));
extern void		 d_change_map		P((array*));

extern uindex		 d_new_call_out		P((dataspace*, string*, Int,
						   unsigned int, frame*, int));
extern Int		 d_del_call_out		P((dataspace*, unsigned int));
extern string	        *d_get_call_out		P((dataspace*, unsigned int,
						   frame*, int*));
extern array	        *d_list_callouts	P((dataspace*, dataspace*));

extern void		 d_set_varmap		P((control*, unsigned int,
						   unsigned short*));
extern void		 d_upgrade_data		P((dataspace*, unsigned short,
						   unsigned short*, object*));
extern void		 d_upgrade_clone	P((dataspace*));
extern object	        *d_upgrade_lwobj	P((lpcenv*, array*, object*));
extern void		 d_upgrade_mem		P((lpcenv*, object*, object*));
extern void		 d_export		P((lpcenv*));

extern void		 d_clean		P((lpcenv*));
extern sector		 d_swapout		P((lpcenv*, unsigned int));
extern void		 d_swapsync		P((lpcenv*));

extern void		 d_free_control		P((lpcenv*, control*));
extern void		 d_free_dataspace	P((dataspace*));
extern void		 d_del_control		P((lpcenv*, control*));
extern void		 d_del_dataspace	P((dataspace*));


/* bit values for ctrl->flags */
# define CTRL_COMPILED		0x10	/* precompiled control block */
# define CTRL_VARMAP		0x20	/* varmap updated */

/* bit values for dataspace->plane->flags */
# define MOD_ALL		0x3f
# define MOD_VARIABLE		0x01	/* variable changed */
# define MOD_ARRAY		0x02	/* array element changed */
# define MOD_ARRAYREF		0x04	/* array reference changed */
# define MOD_STRINGREF		0x08	/* string reference changed */
# define MOD_CALLOUT		0x10	/* callout changed */
# define MOD_NEWCALLOUT		0x20	/* new callout added */
# define PLANE_MERGE		0x40	/* merge planes on commit */

# define ARR_MOD		0x80000000L	/* in arrref->ref */
