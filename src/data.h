# include "swap.h"

typedef struct {
    object *obj;		/* inherited object */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset */
} dinherit;

typedef struct {
    Uint index;			/* index in control block */
    unsigned short len;		/* string length */
} dstrconst;

typedef struct {
    char class;			/* function class */
    char inherit;		/* function name inherit index */
    unsigned short index;	/* function name index */
    Uint offset;		/* offset in program text */
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
    Uint refc;

    uindex nsectors;		/* o # of sectors */
    sector *sectors;		/* o vector with sectors */

    short ninherits;		/* i/o # inherited objects */
    dinherit *inherits;		/* i/o inherit objects */

    short niinherits;		/* i/o # immediately inherited object indices */
    char *iinherits;		/* i/o immediately inherited object indices */
    Uint iinhoffset;		/* o iinherits offset */

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
    unsigned short nfloatdefs;	/* i/o # float definitions */
    unsigned short nfloats;	/* i/o # floats in object */

    uindex ndata;		/* # of data blocks using this control block */
} control;

typedef struct _strref_ {
    string *str;		/* string value */
    struct _dataspace_ *data;	/* dataspace this string is in */
    Uint ref;			/* # of refs */
} strref;

typedef struct _arrref_ {
    array *arr;			/* array value */
    struct _dataspace_ *data;	/* dataspace this array is in */
    Uint index;			/* selts index */
    Uint ref;			/* # of refs */
} arrref;

typedef struct _dataspace_ {
    struct _dataspace_ *prev, *next;
    Uint refc;

    long achange;		/* # array changes */
    long schange;		/* # string changes */
    long imports;		/* # array imports */
    struct _dataspace_ *ilist;	/* import list */
    char modified;		/* has a variable or array elt been modified */

    object *obj;		/* object this dataspace belongs to */
    control *ctrl;		/* control block */

    uindex nsectors;		/* o # sectors */
    sector *sectors;		/* o vector of sectors */

    unsigned short nvariables;	/* o # variables */
    struct _value_ *variables;	/* i/o variables */
    struct _svalue_ *svariables;/* o svariables */
    Uint varoffset;		/* o offset of variables in data space */

    Uint narrays;		/* i/o # arrays */
    Uint eltsize;		/* o total size of array elements */
    arrref alocal;		/* primary of new local arrays */
    arrref *arrays;		/* i/o? arrays */
    struct _sarray_ *sarrays;	/* o sarrays */
    struct _svalue_ *selts;	/* o sarray elements */
    Uint arroffset;		/* o offset of array table in data space */

    Uint nstrings;		/* i/o # strings */
    Uint strsize;		/* o total size of string text */
    struct _strref_ *strings;	/* i/o? string constant table */
    struct _sstring_ *sstrings;	/* o sstrings */
    char *stext;		/* o sstrings text */
    Uint stroffset;		/* o offset of string table */

    uindex ncallouts;		/* # callouts */
    uindex fcallouts;		/* free callout list */
    struct _dcallout_ *callouts;/* callouts */
    Uint cooffset;		/* offset of callout table */
} dataspace;

extern control	       *d_new_control	P((void));
extern dataspace       *d_new_dataspace	P((object*));
extern control	       *d_load_control	P((object*));
extern dataspace       *d_load_dataspace P((object*));
extern void		d_ref_control	P((control*));
extern void		d_ref_dataspace	P((dataspace*));

extern char	       *d_get_iinherits	P((control*));
extern char	       *d_get_prog	P((control*));
extern string	       *d_get_strconst	P((control*, int, unsigned int));
extern dfuncdef        *d_get_funcdefs	P((control*));
extern dvardef	       *d_get_vardefs	P((control*));
extern char	       *d_get_funcalls	P((control*));
extern dsymbol	       *d_get_symbols	P((control*));

extern struct _value_  *d_get_variable	P((dataspace*, unsigned int));
extern struct _value_  *d_get_elts	P((array*));

extern void		d_ref_imports	P((array*));
extern void		d_assign_var	P((dataspace*, struct _value_*,
					   struct _value_*));
extern void		d_assign_elt	P((array*, struct _value_*,
					   struct _value_*));
extern void		d_change_map	P((array*));
extern void		d_del_string	P((string*));
extern void		d_del_array	P((array*));

extern uindex		d_new_call_out	P((dataspace*, string*, Uint, int));
extern char	       *d_get_call_out	P((dataspace*, unsigned int, Uint*,
					   int*));
extern array	       *d_list_callouts	P((dataspace*, Uint));

extern void		d_export	P((void));

extern uindex		d_swapout	P((int));
extern void		d_swapsync	P((void));
extern void		d_patch_ctrl	P((control*, long));
extern void		d_patch_callout	P((dataspace*, long));

extern void		d_del_control	P((control*));
extern void		d_del_dataspace	P((dataspace*));
