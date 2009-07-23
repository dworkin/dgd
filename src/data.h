# include "swap.h"

typedef struct _svalue_ svalue;

typedef struct {
    uindex oindex;		/* inherited object */
    uindex progoffset;		/* program offset */
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

    sector nsectors;		/* o # of sectors */
    sector *sectors;		/* o vector with sectors */

    uindex oindex;		/* i object */

    short flags;		/* various bitflags */

    short ninherits;		/* i/o # inherited objects */
    dinherit *inherits;		/* i/o inherit objects */
    short progindex;		/* i/o program index */

    uindex imapsz;		/* i/o inherit map size */
    char *imap;			/* i/o inherit map */

    Uint compiled;		/* time of compilation */

    char *prog;			/* i program text */
    Uint progsize;		/* i/o program text size */
    Uint progoffset;		/* o program text offset */

    unsigned short nstrings;	/* i/o # strings */
    string **strings;		/* i/o? string table */
    dstrconst *sstrings;	/* o sstrings */
    char *stext;		/* o sstrings text */
    Uint strsize;		/* o sstrings text size */
    Uint stroffset;		/* o offset of string index table */

    unsigned short nfuncdefs;	/* i/o # function definitions */
    dfuncdef *funcdefs;		/* i/o? function definition table */
    Uint funcdoffset;		/* o offset of function definition table */

    unsigned short nvardefs;	/* i/o # variable definitions */
    unsigned short nclassvars;	/* i/o # class variable definitions */
    dvardef *vardefs;		/* i/o? variable definitions */
    string **cvstrings;		/* variable class strings */
    char *classvars;		/* variable classes */
    Uint vardoffset;		/* o offset of variable definition table */

    uindex nfuncalls;		/* i/o # function calls */
    char *funcalls;		/* i/o? function calls */
    Uint funccoffset;		/* o offset of function call table */

    unsigned short nsymbols;	/* i/o # symbols */
    dsymbol *symbols;		/* i/o? symbol table */
    Uint symboffset;		/* o offset of symbol table */

    unsigned short nvariables;	/* i/o # variables */
    char *vtypes;		/* i/o? variable types */
    Uint vtypeoffset;		/* o offset of variable types */

    unsigned short vmapsize;	/* i/o size of variable mapping */
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
    unsigned short mtime;	/* time of call milliseconds */
    uindex nargs;		/* number of arguments */
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
    arrref *arrays;		/* i/o? arrays */
    abchunk *achunk;		/* chunk of array backup info */
    strref *strings;		/* i/o? string constant table */
    struct _coptable_ *coptab;	/* callout patch table */

    dataplane *prev;		/* previous in per-dataspace linked list */
    dataplane *plist;		/* next in per-level linked list */
};

struct _dataspace_ {
    dataspace *prev, *next;	/* swap list */
    dataspace *gcprev, *gcnext;	/* garbage collection list */

    dataspace *iprev;		/* previous in import list */
    dataspace *inext;		/* next in import list */

    sector *sectors;		/* o vector of sectors */
    sector nsectors;		/* o # sectors */

    short flags;		/* various bitflags */
    control *ctrl;		/* control block */
    uindex oindex;		/* object this dataspace belongs to */

    unsigned short nvariables;	/* o # variables */
    value *variables;		/* i/o variables */
    struct _svalue_ *svariables;/* o svariables */
    Uint varoffset;		/* o offset of variables in data space */

    Uint narrays;		/* i/o # arrays */
    Uint eltsize;		/* o total size of array elements */
    struct _sarray_ *sarrays;	/* o sarrays */
    struct _svalue_ *selts;	/* o sarray elements */
    array alist;		/* array linked list sentinel */
    Uint arroffset;		/* o offset of array table in data space */

    Uint nstrings;		/* i/o # strings */
    Uint strsize;		/* o total size of string text */
    struct _sstring_ *sstrings;	/* o sstrings */
    char *stext;		/* o sstrings text */
    Uint stroffset;		/* o offset of string table */

    uindex ncallouts;		/* # callouts */
    uindex fcallouts;		/* free callout list */
    dcallout *callouts;		/* callouts */
    struct _scallout_ *scallouts; /* o scallouts */
    Uint cooffset;		/* offset of callout table */

    dataplane base;		/* basic value plane */
    dataplane *plane;		/* current value plane */

    struct _parser_ *parser;	/* parse_string data */
};

# define THISPLANE(a)		((a)->plane == (a)->data->plane)
# define SAMEPLANE(d1, d2)	((d1)->plane->level == (d2)->plane->level)

/* sdata.c */

extern void		d_init		 P((void));
extern void		d_init_conv	 P((int, int, int, int, int, int, int));

extern control	       *d_new_control	 P((void));
extern dataspace       *d_new_dataspace  P((object*));
extern control	       *d_load_control	 P((object*));
extern dataspace       *d_load_dataspace P((object*));
extern void		d_ref_control	 P((control*));
extern void		d_ref_dataspace  P((dataspace*));

extern char	       *d_get_prog	 P((control*));
extern string	       *d_get_strconst	 P((control*, int, unsigned int));
extern dfuncdef        *d_get_funcdefs	 P((control*));
extern dvardef	       *d_get_vardefs	 P((control*));
extern char	       *d_get_funcalls	 P((control*));
extern dsymbol	       *d_get_symbols	 P((control*));
extern Uint		d_get_progsize	 P((control*));

extern void		d_new_variables	 P((control*, value*));
extern value	       *d_get_variable	 P((dataspace*, unsigned int));
extern value	       *d_get_elts	 P((array*));
extern void		d_get_callouts	 P((dataspace*));

extern sector		d_swapout	 P((unsigned int));
extern void		d_swapsync	 P((void));
extern void		d_upgrade_mem	 P((object*, object*));
extern void		d_restore_obj	 P((object*, Uint*));
extern void		d_converted	 P((void));

extern void		d_free_control	 P((control*));
extern void		d_free_dataspace P((dataspace*));

/* data.c */

extern void		d_new_plane	P((dataspace*, Int));
extern void		d_commit_plane	P((Int, value*));
extern void		d_discard_plane	P((Int));
extern abchunk	      **d_commit_arr	P((array*, dataplane*, dataplane*));
extern void		d_discard_arr	P((array*, dataplane*));

extern void		d_ref_imports	P((array*));
extern void		d_assign_var	P((dataspace*, value*, value*));
extern value	       *d_get_extravar	P((dataspace*));
extern void		d_set_extravar	P((dataspace*, value*));
extern void		d_wipe_extravar	P((dataspace*));
extern void		d_assign_elt	P((dataspace*, array*, value*, value*));
extern void		d_change_map	P((array*));

extern uindex		d_new_call_out	P((dataspace*, string*, Int,
					   unsigned int, frame*, int));
extern Int		d_del_call_out	P((dataspace*, Uint, unsigned short*));
extern string	       *d_get_call_out	P((dataspace*, unsigned int, frame*,
					   int*));
extern array	       *d_list_callouts	P((dataspace*, dataspace*));

extern void		d_set_varmap	P((control*, unsigned int,
					   unsigned short*));
extern void		d_upgrade_data	P((dataspace*, unsigned int,
					   unsigned short*, object*));
extern void		d_upgrade_clone	P((dataspace*));
extern object	       *d_upgrade_lwobj	P((array*, object*));
extern void		d_export	P((void));

extern void		d_del_control	P((control*));
extern void		d_del_dataspace	P((dataspace*));


/* bit values for ctrl->flags */
# define CTRL_PROGCMP		0x03	/* program compressed */
# define CTRL_STRCMP		0x0c	/* strings compressed */
# define CTRL_UNDEFINED		0x10	/* has undefined functions */
# define CTRL_COMPILED		0x20	/* precompiled control block */
# define CTRL_VARMAP		0x40	/* varmap updated */
# define CTRL_CONVERTED		0x80	/* converted control block */

/* bit values for dataspace->flags */
# define DATA_STRCMP		0x03	/* strings compressed */

/* bit values for dataspace->plane->flags */
# define MOD_ALL		0x3f
# define MOD_VARIABLE		0x01	/* variable changed */
# define MOD_ARRAY		0x02	/* array element changed */
# define MOD_ARRAYREF		0x04	/* array reference changed */
# define MOD_STRINGREF		0x08	/* string reference changed */
# define MOD_CALLOUT		0x10	/* callout changed */
# define MOD_NEWCALLOUT		0x20	/* new callout added */
# define PLANE_MERGE		0x40	/* merge planes on commit */
# define MOD_SAVE		0x80	/* save on next full swapout */

/* data compression */
# define CMP_TYPE		0x03
# define CMP_NONE		0x00	/* no compression */
# define CMP_PRED		0x01	/* predictor compression */

# define ARR_MOD		0x80000000L	/* in arrref->ref */

# define AR_UNCHANGED		0	/* mapping unchanged */
# define AR_CHANGED		1	/* mapping changed */
