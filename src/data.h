# include "swap.h"

typedef struct {
    object *obj;		/* inherited object */
    unsigned short funcoffset;	/* function call offset */
    unsigned short varoffset;	/* variable offset */
} dinherit;

typedef struct {
    long index;			/* index in control block */
    unsigned short len;		/* string length */
} dstrconst;

typedef struct {
    char class;			/* function class */
    char inherit;		/* function name inherit index */
    unsigned short index;	/* function name index */
    unsigned short offset;	/* offset in program text */
} dfuncdef;

typedef struct {
    char class;			/* variable class */
    char inherit;		/* variable name inherit index */
    unsigned short index;	/* variable name index */
    unsigned short type;	/* variable type */
} dvardef;

typedef struct {
    char inherit;		/* function object index */
    char index;			/* function index */
    unsigned short next;	/* next in hash table */
} dsymbol;

typedef struct _control_ {
    struct _control_ *prev, *next;

    uindex nsectors;		/* o # of sectors */
    sector *sectors;		/* o vector with sectors */

    short ninherits;		/* i/o # inherited objects */
    dinherit *inherits;		/* i/o inherit objects */

    Uint compiled;		/* time of compilation */

    char *prog;			/* i program text */
    unsigned short progsize;	/* i/o program text size */
    long progoffset;		/* o program text offset */

    unsigned short nstrings;	/* i/o # strings */
    string **strings;		/* i/o? string table */
    dstrconst *sstrings;	/* o sstrings */
    char *stext;		/* o sstrings text */
    long strsize;		/* o sstrings text size */
    long stroffset;		/* o offset of string index table */

    unsigned short nfuncdefs;	/* i/o # function definitions */
    dfuncdef *funcdefs;		/* i/o? function definition table */
    long funcdoffset;		/* o offset of function definition table */

    unsigned short nvardefs;	/* i/o # variable definitions */
    dvardef *vardefs;		/* i/o? variable definitions */
    long vardoffset;		/* o offset of variable definition table */

    uindex nfuncalls;		/* i/o # function calls */
    char *funcalls;		/* i/o? function calls */
    long funccoffset;		/* o offset of function call table */

    unsigned short nsymbols;	/* i/o # symbols */
    dsymbol *symbols;		/* i/o? symbol table */
    long symboffset;		/* o offset of symbol table */

    unsigned short nvariables;	/* i/o # variables */
    unsigned short nfloatdefs;	/* i/o # float definitions */
    unsigned short nfloats;	/* i/o # floats in object */

    uindex ndata;		/* # of data blocks using this control block */
} control;

typedef struct _dataspace_ {
    struct _dataspace_ *prev, *next;

    long achange;		/* # array changes */
    long schange;		/* # string changes */
    char modified;		/* has a variable or array elt been modified */

    object *obj;		/* object this dataspace belongs to */
    control *ctrl;		/* control block */

    uindex nsectors;		/* o # sectors */
    sector *sectors;		/* o vector of sectors */

    unsigned short nvariables;	/* o # variables */
    struct _value_ *variables;	/* i/o variables */
    struct _svalue_ *svariables;/* o svariables */
    long varoffset;		/* o offset of variables in data space */

    uindex narrays;		/* i/o # arrays */
    long eltsize;		/* o total size of array elements */
    struct _arrref_ *arrays;	/* i/o? arrays */
    struct _sarray_ *sarrays;	/* o sarrays */
    struct _svalue_ *selts;	/* o sarray elements */
    long arroffset;		/* o offset of array table in data space */

    uindex nstrings;		/* i/o # string constants */
    long strsize;		/* o total size of string text */
    struct _strref_ *strings;	/* i/o? string constant table */
    struct _sstring_ *sstrings;	/* o sstrings */
    char *stext;		/* o sstrings text */
    long stroffset;		/* o offset of string table */

    uindex ncallouts;		/* # callouts */
    uindex fcallouts;		/* free callout list */
    struct _dcallout_ *callouts;/* callouts */
    long cooffset;		/* offset of callout table */
} dataspace;

extern control	       *d_new_control	P((void));
extern dataspace       *d_new_dataspace	P((object*));
extern control	       *d_load_control	P((object*));
extern dataspace       *d_load_dataspace P((object*));
extern void		d_ref_control	P((control*));
extern void		d_ref_dataspace	P((dataspace*));

extern char	       *d_get_prog	P((control*));
extern string	       *d_get_strconst	P((control*, char, unsigned short));
extern dfuncdef        *d_get_funcdefs	P((control*));
extern dvardef	       *d_get_vardefs	P((control*));
extern char	       *d_get_funcalls	P((control*));
extern dsymbol	       *d_get_symbols	P((control*));

extern struct _value_  *d_get_variable	P((dataspace*, unsigned short));
extern struct _value_  *d_get_elts	P((array*));

extern void		d_assign_var	P((dataspace*, struct _value_*,
					   struct _value_*));
extern void		d_assign_elt	P((array*, struct _value_*,
					   struct _value_*));
extern void		d_change_map	P((array*));

extern uindex		d_new_call_out	P((dataspace*, string*, Uint, int));
extern char	       *d_get_call_out	P((dataspace*, uindex, Uint*, int*));
extern array	       *d_list_callouts	P((dataspace*, Uint));

extern uindex		d_swapout	P((int));
extern void		d_swapsync	P((void));
extern void		d_patch_ctrl	P((control*, long));
extern void		d_patch_callout	P((dataspace*, long));

extern void		d_del_control	P((control*));
extern void		d_del_dataspace	P((dataspace*));
