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

    sector nsectors;		/* o # of sectors */
    sector *sectors;		/* o vector with sectors */

    uindex oindex;		/* i object */

    short flags;		/* various bitflags */

    short ninherits;		/* i/o # inherited objects */
    dinherit *inherits;		/* i/o inherit objects */

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
    dvardef *vardefs;		/* i/o? variable definitions */
    Uint vardoffset;		/* o offset of variable definition table */

    uindex nfuncalls;		/* i/o # function calls */
    char *funcalls;		/* i/o? function calls */
    Uint funccoffset;		/* o offset of function call table */

    unsigned short nsymbols;	/* i/o # symbols */
    dsymbol *symbols;		/* i/o? symbol table */
    Uint symboffset;		/* o offset of symbol table */

    unsigned short nvariables;	/* i/o # variables */
    unsigned short nifdefs;	/* i/o # int/float definitions */
    unsigned short nvinit;	/* i/o # variables requiring initialization */

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
    dataspace *prev, *next;

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
    Uint arroffset;		/* o offset of array table in data space */

    Uint nstrings;		/* i/o # strings */
    Uint strsize;		/* o total size of string text */
    struct _sstring_ *sstrings;	/* o sstrings */
    char *stext;		/* o sstrings text */
    Uint stroffset;		/* o offset of string table */

    uindex ncallouts;		/* # callouts */
    uindex fcallouts;		/* free callout list */
    struct _dcallout_ *callouts;/* callouts */
    Uint cooffset;		/* offset of callout table */

    dataplane base;		/* basic value plane */
    dataplane *plane;		/* current value plane */

    struct _parser_ *parser;	/* parse_string data */
};

# define THISPLANE(a)		((a)->plane == (a)->data->plane)
# define SAMEPLANE(d1, d2)	((d1)->plane->level == (d2)->plane->level)

extern void		d_init		P((bool));
extern control	       *d_new_control	P((void));
extern dataspace       *d_new_dataspace	P((object*));
extern control	       *d_load_control	P((object*));
extern dataspace       *d_load_dataspace P((object*));
extern void		d_ref_control	P((control*));
extern void		d_ref_dataspace	P((dataspace*));

extern void		d_varmap	P((control*, unsigned int,
					   unsigned short*));

extern char	       *d_get_prog	P((control*));
extern string	       *d_get_strconst	P((control*, int, unsigned int));
extern dfuncdef        *d_get_funcdefs	P((control*));
extern dvardef	       *d_get_vardefs	P((control*));
extern char	       *d_get_funcalls	P((control*));
extern dsymbol	       *d_get_symbols	P((control*));
extern Uint		d_get_progsize	P((control*));

extern value	       *d_get_variable	P((dataspace*, unsigned int));
extern value	       *d_get_elts	P((array*));

extern void		d_new_plane	P((dataspace*, Int));
extern void		d_commit_plane	P((Int));
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
extern Int		d_del_call_out	P((dataspace*, unsigned int));
extern string	       *d_get_call_out	P((dataspace*, unsigned int, frame*,
					   int*));
extern array	       *d_list_callouts	P((dataspace*, dataspace*));

extern void		d_export	P((void));
extern void		d_upgrade_all	P((object*, object*));
extern sector		d_swapout	P((unsigned int));
extern void		d_swapsync	P((void));
extern void		d_conv_control	P((unsigned int));
extern void		d_conv_dataspace P((object*, Uint*));

extern void		d_del_control	P((control*));
extern void		d_del_dataspace	P((dataspace*));
