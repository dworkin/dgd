# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "swap.h"
# include "data.h"
# include "csupport.h"

/* bit values for dataspace->modified */
# define M_VARIABLE		0x01
# define M_ARRAY		0x02
# define M_ARRAYREF		0x04
# define M_STRINGREF		0x08
# define M_NEWCALLOUT		0x10
# define M_CALLOUT		0x20

# define ARR_MOD		0x80000000L

typedef struct {
    uindex nsectors;		/* # sectors in part one */
    short ninherits;		/* # objects in inherit table */
    Uint compiled;		/* time of compilation */
    unsigned short progsize;	/* size of program code */
    unsigned short nstrings;	/* # strings in string constant table */
    long strsize;		/* size of string constant table */
    char nfuncdefs;		/* # entries in function definition table */
    char nvardefs;		/* # entries in variable definition table */
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
    unsigned short nfloatdefs;	/* # float definitions */
    unsigned short nfloats;	/* # float vars */
} scontrol;

typedef struct {
    uindex nsectors;		/* number of sectors in data space */
    uindex narrays;		/* number of array values */
    long eltsize;		/* total size of array elements */
    uindex nstrings;		/* number of strings */
    long strsize;		/* total size of strings */
    uindex ncallouts;		/* number of callouts */
    uindex fcallouts;		/* first free callout */
} sdataspace;

typedef struct _svalue_ {
    char type;			/* object, number, string, array */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Uint objcnt;		/* object creation count */
	struct {
	    short inherit;
	    uindex index;
	} string;		/* string */
	uindex array;		/* array */
    } u;
} svalue;

# define SFLT_GET(s, v)	((v)->oindex = (s)->oindex, \
			 (v)->u.objcnt = (s)->u.objcnt)
# define SFLT_PUT(s, v)	((s)->oindex = (v)->oindex, \
			 (s)->u.objcnt = (v)->u.objcnt)

typedef struct _sarray_ {
    long index;			/* index in array value table */
    unsigned short size;	/* size of array */
    uindex ref;			/* refcount */
    long tag;			/* unique value for each array */
} sarray;

typedef struct _sstring_ {
    long index;			/* index in string text table */
    unsigned short len;		/* length of string */
    uindex ref;			/* refcount */
} sstring;

typedef struct _strref_ {
    string *string;		/* string value */
    struct _dataspace_ *data;	/* dataspace this string is in */
    uindex ref;			/* # of refs */
} strref;

typedef struct _arrref_ {
    array *array;		/* array value */
    struct _dataspace_ *data;	/* dataspace this array is in */
    long index;			/* selts index */
    uindex ref;			/* # of refs */
} arrref;

typedef struct _dcallout_ {
    Uint time;			/* time of call */
    unsigned short nargs;	/* number of arguments */
    value val[4];		/* function name, 3 direct arguments */
} dcallout;

typedef struct {
    Uint time;			/* time of call */
    unsigned short nargs;	/* number of arguments */
    svalue val[4];		/* function name, 3 direct arguments */
} scallout;

# define co_prev	time
# define co_next	nargs

static control *chead, *ctail;	/* list of control blocks */
static uindex nctrl;		/* # control blocks */
static dataspace *dhead, *dtail;/* list of dataspace blocks */
static uindex ndata;		/* # dataspace blocks */


/*
 * NAME:	data->new_control()
 * DESCRIPTION:	create a new control block
 */
control *d_new_control()
{
    register control *ctrl;

    ctrl = ALLOC(control, 1);
    ctrl->prev = (control *) NULL;
    ctrl->next = chead;
    if (chead != (control *) NULL) {
	chead->prev = ctrl;
    } else {
	ctail = ctrl;
    }
    chead = ctrl;
    ctrl->ndata = 0;
    nctrl++;

    ctrl->nsectors = 0;		/* nothing on swap device yet */
    ctrl->sectors = (sector *) NULL;
    ctrl->ninherits = 0;
    ctrl->inherits = (dinherit *) NULL;
    ctrl->progsize = 0;
    ctrl->prog = (char *) NULL;
    ctrl->nstrings = 0;
    ctrl->strings = (string **) NULL;
    ctrl->sstrings = (dstrconst *) NULL;
    ctrl->stext = (char *) NULL;
    ctrl->nfuncdefs = 0;
    ctrl->funcdefs = (dfuncdef *) NULL;
    ctrl->nvardefs = 0;
    ctrl->vardefs = (dvardef *) NULL;
    ctrl->nfuncalls = 0;
    ctrl->funcalls = (char *) NULL;
    ctrl->nsymbols = 0;
    ctrl->symbols = (dsymbol *) NULL;
    ctrl->nvariables = 0;
    ctrl->nfloatdefs = 0;
    ctrl->nfloats = 0;

    return ctrl;
}

/*
 * NAME:	data->new_dataspace()
 * DESCRIPTION:	create a new dataspace block
 */
dataspace *d_new_dataspace(obj)
object *obj;
{
    register dataspace *data;

    data = ALLOC(dataspace, 1);
    data->prev = (dataspace *) NULL;
    data->next = dhead;
    if (dhead != (dataspace *) NULL) {
	dhead->prev = data;
    } else {
	dtail = data;
    }
    dhead = data;
    ndata++;

    data->achange = 0;
    data->schange = 0;
    data->obj = obj;
    data->ctrl = o_control(obj);
    data->ctrl->ndata++;

    data->modified = 0;

    /* sectors */
    data->nsectors = 0;			/* nothing on swap device yet */
    data->sectors = (sector *) NULL;

    /* variables */
    data->nvariables = data->ctrl->nvariables;
    data->variables = (value *) NULL;
    data->svariables = (svalue *) NULL;

    /* arrays */
    data->narrays = 0;
    data->eltsize = 0;
    data->arrays = (arrref *) NULL;
    data->sarrays = (sarray *) NULL;
    data->selts = (svalue *) NULL;

    /* strings */
    data->nstrings = 0;
    data->strsize = 0;
    data->strings = (strref *) NULL;
    data->sstrings = (sstring *) NULL;
    data->stext = (char *) NULL;

    /* callouts */
    data->ncallouts = 0;
    data->fcallouts = 0;
    data->callouts = (dcallout *) NULL;

    return data;
}

/*
 * NAME:	data->load_control()
 * DESCRIPTION:	load a control block from the swap device
 */
control *d_load_control(obj)
object *obj;
{
    register control *ctrl;

    ctrl = ALLOC(control, 1);
    ctrl->prev = (control *) NULL;
    ctrl->next = chead;
    if (chead != (control *) NULL) {
	chead->prev = ctrl;
    } else {
	ctail = ctrl;
    }
    chead = ctrl;
    ctrl->ndata = 0;
    nctrl++;

    if (obj->flags & O_COMPILED) {
	/* initialize control block of compiled object */
	pc_control(ctrl, obj);
    } else {
	scontrol header;
	register long size;

	/* header */
	sw_readv((char *) &header, &obj->cfirst, (long) sizeof(scontrol), 0L);
	ctrl->nsectors = header.nsectors;
	ctrl->sectors = ALLOC(sector, header.nsectors);
	ctrl->sectors[0] = obj->cfirst;
	size = header.nsectors * (long) sizeof(sector);
	if (header.nsectors > 1) {
	    sw_readv((char *) ctrl->sectors, ctrl->sectors, size,
		     (long) sizeof(scontrol));
	}
	size += sizeof(scontrol);

	/* inherits */
	ctrl->ninherits = header.ninherits; /* there is at least one */
	ctrl->inherits = ALLOC(dinherit, header.ninherits);
	sw_readv((char *) ctrl->inherits, ctrl->sectors,
		 header.ninherits * (long) sizeof(dinherit), size);
	size += header.ninherits * (long) sizeof(dinherit);

	/* compile time */
	ctrl->compiled = header.compiled;
	/* program */
	ctrl->progoffset = size;
	ctrl->progsize = header.progsize;
	ctrl->prog = (char *) NULL;
	size += header.progsize;

	/* string constants */
	ctrl->stroffset = size;
	ctrl->nstrings = header.nstrings;
	ctrl->strings = (string **) NULL;
	ctrl->sstrings = (dstrconst *) NULL;
	ctrl->stext = (char *) NULL;
	ctrl->strsize = header.strsize;
	size += header.nstrings * (long) sizeof(dstrconst) + header.strsize;

	/* function definitions */
	ctrl->funcdoffset = size;
	ctrl->nfuncdefs = UCHAR(header.nfuncdefs);
	ctrl->funcdefs = (dfuncdef *) NULL;
	size += UCHAR(header.nfuncdefs) * (long) sizeof(dfuncdef);

	/* variable definitions */
	ctrl->vardoffset = size;
	ctrl->nvardefs = UCHAR(header.nvardefs);
	ctrl->vardefs = (dvardef *) NULL;
	size += UCHAR(header.nvardefs) * (long) sizeof(dvardef);

	/* function call table */
	ctrl->funccoffset = size;
	ctrl->nfuncalls = header.nfuncalls;
	ctrl->funcalls = (char *) NULL;
	size += header.nfuncalls * 2L;

	/* symbol table */
	ctrl->symboffset = size;
	ctrl->nsymbols = header.nsymbols;
	ctrl->symbols = (dsymbol *) NULL;

	/* # variables */
	ctrl->nvariables = header.nvariables;
	ctrl->nfloatdefs = header.nfloatdefs;
	ctrl->nfloats = header.nfloats;
    }

    return ctrl;
}

/*
 * NAME:	data->load_dataspace()
 * DESCRIPTION:	load the dataspace header block of an object from the swap
 */
dataspace *d_load_dataspace(obj)
object *obj;
{
    sdataspace header;
    register dataspace *data;
    register long size;

    data = ALLOC(dataspace, 1);
    data->prev = (dataspace *) NULL;
    data->next = dhead;
    if (dhead != (dataspace *) NULL) {
	dhead->prev = data;
    } else {
	dtail = data;
    }
    dhead = data;
    ndata++;

    data->achange = 0;
    data->schange = 0;
    data->modified = 0;

    /* header */
    sw_readv((char *) &header, &obj->dfirst, (long) sizeof(sdataspace), 0L);
    data->nsectors = header.nsectors;
    data->sectors = ALLOC(sector, header.nsectors);
    data->sectors[0] = obj->dfirst;
    size = header.nsectors * (long) sizeof(sector);
    if (header.nsectors > 1) {
	sw_readv((char *) data->sectors, data->sectors, size,
		 (long) sizeof(sdataspace));
    }
    size += sizeof(sdataspace);
    data->obj = obj;
    data->ctrl = o_control(obj);
    data->ctrl->ndata++;

    /* variables */
    data->varoffset = size;
    data->nvariables = data->ctrl->nvariables;
    data->variables = (value *) NULL;
    data->svariables = (svalue *) NULL;
    size += data->nvariables * (long) sizeof(svalue);

    /* arrays */
    data->arroffset = size;
    data->narrays = header.narrays;
    data->eltsize = header.eltsize;
    data->arrays = (arrref *) NULL;
    data->sarrays = (sarray *) NULL;
    data->selts = (svalue *) NULL;
    size += header.narrays * (long) sizeof(sarray) +
	    header.eltsize * sizeof(svalue);

    /* strings */
    data->stroffset = size;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    data->strings = (strref *) NULL;
    data->sstrings = (sstring *) NULL;
    data->stext = (char *) NULL;
    size += header.nstrings * (long) sizeof(sstring) + header.strsize;

    /* callouts */
    data->cooffset = size;
    data->ncallouts = header.ncallouts;
    data->fcallouts = header.fcallouts;
    data->callouts = (dcallout *) NULL;

    return data;
}

/*
 * NAME:	data->ref_control()
 * DESCRIPTION:	reference control block
 */
void d_ref_control(ctrl)
register control *ctrl;
{
    if (ctrl != chead) {
	ctrl->prev->next = ctrl->next;
	if (ctrl != ctail) {
	    ctrl->next->prev = ctrl->prev;
	} else {
	    ctail = ctrl->prev;
	}
	ctrl->prev = (control *) NULL;
	ctrl->next = chead;
	chead->prev = ctrl;
	chead = ctrl;
    }
}

/*
 * NAME:	data->ref_dataspace()
 * DESCRIPTION:	reference data block
 */
void d_ref_dataspace(data)
register dataspace *data;
{
    if (data != dhead) {
	data->prev->next = data->next;
	if (data != dtail) {
	    data->next->prev = data->prev;
	} else {
	    dtail = data->prev;
	}
	data->prev = (dataspace *) NULL;
	data->next = dhead;
	dhead->prev = data;
	dhead = data;
    }
}


/*
 * NAME:	data->get_prog()
 * DESCRIPTION:	get the program
 */
char *d_get_prog(ctrl)
control *ctrl;
{
    if (ctrl->prog == (char *) NULL && ctrl->progsize > 0) {
	ctrl->prog = ALLOC(char, ctrl->progsize);
	sw_readv(ctrl->prog, ctrl->sectors, (long) ctrl->progsize,
		 ctrl->progoffset);
    }
    return ctrl->prog;
}

/*
 * NAME:	data->get_strconst()
 * DESCRIPTION:	get a string constant
 */
string *d_get_strconst(ctrl, inherit, idx)
register control *ctrl;
register char inherit;
unsigned short idx;
{
    if (UCHAR(inherit) < ctrl->ninherits - 1) {
	/* get the proper control block */
	ctrl = o_control(ctrl->inherits[UCHAR(inherit)].obj);
    }

    if (ctrl->strings == (string **) NULL) {
	/* make string pointer block */
	ctrl->strings = ALLOC(string*, ctrl->nstrings);
	memset(ctrl->strings, '\0', ctrl->nstrings * sizeof(string *));

	if (ctrl->sstrings == (dstrconst *) NULL) {
	    /* load strings */
	    ctrl->sstrings = ALLOC(dstrconst, ctrl->nstrings);
	    sw_readv((char *) ctrl->sstrings, ctrl->sectors,
		     ctrl->nstrings * (long) sizeof(dstrconst),
		     ctrl->stroffset);
	    if (ctrl->strsize > 0) {
		/* load strings text */
		ctrl->stext = ALLOC(char, ctrl->strsize);
		sw_readv(ctrl->stext, ctrl->sectors, ctrl->strsize,
			 ctrl->stroffset +
				     ctrl->nstrings * (long) sizeof(dstrconst));
	    }
	}
    }

    if (ctrl->strings[idx] == (string *) NULL) {
	register string *str;

	str = str_new(ctrl->stext + ctrl->sstrings[idx].index,
		      (long) ctrl->sstrings[idx].len);
	str->ref = STR_CONST | 1;
	ctrl->strings[idx] = str;
	str->u.strconst = &ctrl->strings[idx];
    }

    return ctrl->strings[idx];
}

/*
 * NAME:	data->get_funcdefs()
 * DESCRIPTION:	get function definitions
 */
dfuncdef *d_get_funcdefs(ctrl)
register control *ctrl;
{
    if (ctrl->funcdefs == (dfuncdef *) NULL && ctrl->nfuncdefs > 0) {
	ctrl->funcdefs = ALLOC(dfuncdef, ctrl->nfuncdefs);
	sw_readv((char *) ctrl->funcdefs, ctrl->sectors,
		 ctrl->nfuncdefs * (long) sizeof(dfuncdef), ctrl->funcdoffset);
    }
    return ctrl->funcdefs;
}

/*
 * NAME:	data->get_vardefs()
 * DESCRIPTION:	get variable definitions
 */
dvardef *d_get_vardefs(ctrl)
register control *ctrl;
{
    if (ctrl->vardefs == (dvardef *) NULL && ctrl->nvardefs > 0) {
	ctrl->vardefs = ALLOC(dvardef, ctrl->nvardefs);
	sw_readv((char *) ctrl->vardefs, ctrl->sectors,
		 ctrl->nvardefs * (long) sizeof(dvardef), ctrl->vardoffset);
    }
    return ctrl->vardefs;
}

/*
 * NAME:	data->get_funcalls()
 * DESCRIPTION:	get function call table
 */
char *d_get_funcalls(ctrl)
register control *ctrl;
{
    if (ctrl->funcalls == (char *) NULL && ctrl->nfuncalls > 0) {
	ctrl->funcalls = ALLOC(char, 2 * ctrl->nfuncalls);
	sw_readv((char *) ctrl->funcalls, ctrl->sectors, ctrl->nfuncalls * 2L,
		 ctrl->funccoffset);
    }
    return ctrl->funcalls;
}

/*
 * NAME:	data->get_symbols()
 * DESCRIPTION:	get symbol table
 */
dsymbol *d_get_symbols(ctrl)
register control *ctrl;
{
    if (ctrl->symbols == (dsymbol *) NULL && ctrl->nsymbols > 0) {
	ctrl->symbols = ALLOC(dsymbol, ctrl->nsymbols);
	sw_readv((char *) ctrl->symbols, ctrl->sectors,
		 ctrl->nsymbols * (long) sizeof(dsymbol), ctrl->symboffset);
    }
    return ctrl->symbols;
}

/*
 * NAME:	data->get_string()
 * DESCRIPTION:	get a string from the dataspace
 */
static string *d_get_string(data, idx)
register dataspace *data;
register uindex idx;
{
    if (data->strings == (strref *) NULL) {
	register strref *strs;
	register uindex i;

	/* initialize string pointers */
	strs = data->strings = ALLOC(strref, data->nstrings);
	for (i = data->nstrings; i > 0; --i) {
	    (strs++)->string = (string *) NULL;
	}

	/* load strings */
	data->sstrings = ALLOC(sstring, data->nstrings);
	sw_readv((char *) data->sstrings, data->sectors,
		 data->nstrings * (long) sizeof(sstring), data->stroffset);
	if (data->strsize > 0) {
	    /* load strings text */
	    data->stext = ALLOC(char, data->strsize);
	    sw_readv(data->stext, data->sectors, data->strsize,
		     data->stroffset + data->nstrings * (long) sizeof(sstring));
	}
    }

    if (data->strings[idx].string == (string *) NULL) {
	register string *s;

	s = str_new(data->stext + data->sstrings[idx].index,
		    (long) data->sstrings[idx].len);
	s->u.primary = &data->strings[idx];
	str_ref(s->u.primary->string = s);
	s->u.primary->data = data;
	s->u.primary->ref = data->sstrings[idx].ref;
    }
    return data->strings[idx].string;
}

/*
 * NAME:	data->get_array()
 * DESCRIPTION:	get an array from the dataspace
 */
static array *d_get_array(data, idx)
register dataspace *data;
register uindex idx;
{
    register uindex i;

    if (data->arrays == (arrref *) NULL) {
	register arrref *a;

	/* create array pointers */
	a = data->arrays = ALLOC(arrref, data->narrays);
	for (i = data->narrays; i > 0; --i) {
	    (a++)->array = (array *) NULL;
	}

	/* load arrays */
	data->sarrays = ALLOC(sarray, data->narrays);
	sw_readv((char *) data->sarrays, data->sectors,
		 data->narrays * (long) sizeof(sarray), data->arroffset);
    }

    if (data->arrays[idx].array == (array *) NULL) {
	register array *a;

	a = arr_alloc(data->sarrays[idx].size);
	a->tag = data->sarrays[idx].tag;
	a->primary = &data->arrays[idx];
	arr_ref(a->primary->array = a);
	a->primary->data = data;
	a->primary->index = data->sarrays[idx].index;
	a->primary->ref = data->sarrays[idx].ref;
    }
    return data->arrays[idx].array;
}

/*
 * NAME:	data->get_values()
 * DESCRIPTION:	get values from the dataspace
 */
static void d_get_values(data, sv, v, n)
register dataspace *data;
register svalue *sv;
register value *v;
register int n;
{
    while (n > 0) {
	v->modified = FALSE;
	switch (v->type = sv->type) {
	case T_INT:
	    v->u.number = sv->u.number;
	    break;

	case T_FLOAT:
	    SFLT_GET(sv, v);
	    break;

	case T_STRING:
	    str_ref(v->u.string = (sv->u.string.inherit < 0) ?
		    d_get_string(data, sv->u.string.index) :
		    d_get_strconst(data->ctrl, sv->u.string.inherit,
				   sv->u.string.index));
	    break;

	case T_OBJECT:
	    v->oindex = sv->oindex;
	    v->u.objcnt = sv->u.objcnt;
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    arr_ref(v->u.array = d_get_array(data, sv->u.array));
	    break;
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * NAME:	data->new_variables()
 * DESCRIPTION:	initialize variables in a dataspace block
 */
static void d_new_variables(data)
dataspace *data;
{
    static value zero_int = { T_INT, TRUE };
    static value zero_float = { T_FLOAT, TRUE };
    register unsigned short nfdefs, nvars, nfloats;
    register value *val;
    register dvardef *var;
    register control *ctrl;
    register dinherit *inh;

    /*
     * initialize all variables to integer 0
     */
    for (val = data->variables, nvars = data->nvariables; nvars > 0; --nvars) {
	*val++ = zero_int;
    }

    if (data->ctrl->nfloats != 0) {
	/*
	 * initialize float variables to 0.0
	 */
	nvars = 0;
	for (nfloats = data->ctrl->nfloats, inh = data->ctrl->inherits;
	     nfloats > 0; inh++) {
	    if (inh->varoffset == nvars) {
		ctrl = o_control(inh->obj);
		if (ctrl->nfloatdefs != 0) {
		    nfloats -= ctrl->nfloatdefs;
		    for (nfdefs = ctrl->nfloatdefs, var = d_get_vardefs(ctrl);
			 nfdefs > 0; var++) {
			if (var->type == T_FLOAT) {
			    data->variables[nvars] = zero_float;
			    --nfdefs;
			}
			nvars++;
		    }
		} else {
		    nvars += ctrl->nvardefs;
		}
	    }
	}

	/* don't do this again for the same object */
	data->modified |= M_VARIABLE;
    }
}

/*
 * NAME:	data->get_variable()
 * DESCRIPTION:	get a variable from the dataspace
 */
value *d_get_variable(data, idx)
register dataspace *data;
register unsigned short idx;
{
    if (data->variables == (value *) NULL) {
	register value *v;
	register unsigned short i;

	/* create room for variables */
	v = data->variables = ALLOC(value, data->nvariables);
	if (data->nsectors == 0) {
	    /* new datablock */
	    d_new_variables(data);
	} else {
	    /* variables must be loaded from the swap */
	    for (i = data->nvariables; i > 0; --i) {
		v->type = T_INVALID;
		(v++)->modified = FALSE;
	    }

	    /* load svalues */
	    data->svariables = ALLOC(svalue, data->nvariables);
	    sw_readv((char *) data->svariables, data->sectors,
		     data->nvariables * (long) sizeof(svalue), data->varoffset);
	}
    }

    if (data->variables[idx].type == T_INVALID) {
	d_get_values(data, &data->svariables[idx], &data->variables[idx], 1);
    }
    return &data->variables[idx];
}

/*
 * NAME:	data->get_elts()
 * DESCRIPTION:	get the elements of an array
 */
value *d_get_elts(arr)
array *arr;
{
    register value *v;

    v = arr->elts;
    if (v != (value *) NULL && v[0].type == T_INVALID) {
	register dataspace *data;

	data = arr->primary->data;
	if (data->selts == (svalue *) NULL) {
	    /* load array elements */
	    data->selts = (svalue *) ALLOC(svalue, data->eltsize);
	    sw_readv((char *) data->selts, data->sectors,
		     data->eltsize * sizeof(svalue),
		     data->arroffset + data->narrays * (long) sizeof(sarray));
	}
	d_get_values(data, &data->selts[arr->primary->index], v, arr->size);
    }

    return v;
}

/*
 * NAME:	strconst_obj()
 * DESCRIPTION:	check if the constant is defined in the current object
 */
static long strconst_obj(data, str)
dataspace *data;
string *str;
{
    register control *ctrl;
    register string **strconst;

    ctrl = data->ctrl;
    strconst = str->u.strconst;
    if (strconst >= ctrl->strings && strconst < ctrl->strings + ctrl->nstrings)
    {
	return ((ctrl->ninherits - 1) << 16L) | (strconst - ctrl->strings);
    }

    return -1;	/* not a constant in this object */
}

/*
 * NAME:	ref_rhs()
 * DESCRIPTION:	reference the right-hand side in an assignment
 */
static void ref_rhs(data, rhs)
register dataspace *data;
register value *rhs;
{
    register string *str;
    register array *arr;

    switch (rhs->type) {
    case T_STRING:
	str = rhs->u.string;
	if (str->ref & STR_CONST) {	/* a constant */
	    if (strconst_obj(data, str) < 0) {
		/* not in this object: ref imported string const */
		data->schange++;
	    }
	} else if (str->u.primary != (strref *) NULL &&
		   str->u.primary->data == data) {
	    /* in this object */
	    if (str->u.primary->ref++ == 0) {
		data->schange--;	/* first reference restored */
	    }
	    data->modified |= M_STRINGREF;
	} else {
	    /* not in this object: ref imported string */
	    data->schange++;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr = rhs->u.array;
	if (arr->primary != (arrref *) NULL && arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->ref++ == 0) {
		data->achange--;	/* first reference restored */
	    }
	    data->modified |= M_ARRAYREF;
	} else {
	    /* not in this object: ref imported array */
	    data->achange++;
	}
	break;
    }
}

/*
 * NAME:	del_lhs()
 * DESCRIPTION:	delete the left-hand side in an assignment
 */
static void del_lhs(data, lhs)
register dataspace *data;
register value *lhs;
{
    register string *str;
    register array *arr;

    switch (lhs->type) {
    case T_STRING:
	str = lhs->u.string;
	if (str->ref & STR_CONST) {	/* a constant */
	    if (strconst_obj(data, str) < 0) {
		/* not in this object: deref imported string const */
		data->schange--;
	    }
	} else if (str->u.primary != (strref *) NULL &&
		   str->u.primary->data == data) {
	    /* in this object */
	    if (--(str->u.primary->ref) == 0) {
		data->schange++;	/* last reference removed */
	    }
	    data->modified |= M_STRINGREF;
	} else {
	    /* not in this object: deref imported string */
	    data->schange--;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr = lhs->u.array;
	if (arr->primary != (arrref *) NULL && arr->primary->data == data) {
	    /* in this object */
	    if (--(arr->primary->ref) == 0) {
		data->achange++;	/* last reference removed */
	    }
	    data->modified |= M_ARRAYREF;
	} else {
	    /* not in this object: deref imported array */
	    data->achange--;
	}
	break;
    }
}

/*
 * NAME:	data->assign_var()
 * DESCRIPTION:	assign a value to a variable
 */
void d_assign_var(data, var, val)
register dataspace *data;
register value *var;
register value *val;
{
    if (var >= data->variables && var < data->variables + data->nvariables) {
	ref_rhs(data, val);
	del_lhs(data, var);
	data->modified |= M_VARIABLE;
    }

    i_ref_value(val);
    i_del_value(var);

    *var = *val;
    var->modified = TRUE;
}

/*
 * NAME:	data->assign_elt()
 * DESCRIPTION:	assign a value to an array element
 */
void d_assign_elt(arr, elt, val)
register array *arr;
value *elt, *val;
{
    register dataspace *data;

    if (arr->primary != (arrref *) NULL) {
	/* the array is in the dataspace of some object */
	arr->primary->index |= ARR_MOD;
	data = arr->primary->data;
	ref_rhs(data, val);
	del_lhs(data, elt);
	data->modified |= M_ARRAY;
    }

    i_ref_value(val);
    i_del_value(elt);

    *elt = *val;
    elt->modified = TRUE;
}

/*
 * NAME:	data->change_map()
 * DESCRIPTION:	mark a mapping as changed
 */
void d_change_map(map)
array *map;
{
    if (map->primary != (arrref *) NULL) {
	map->primary->data->achange++;
    }
}

/*
 * NAME:	data->get_callouts()
 * DESCRIPTION:	load callouts from swap
 */
static void d_get_callouts(data)
register dataspace *data;
{
    scallout *scallouts;
    register scallout *sco;
    register dcallout *co;
    register uindex n;

    co = data->callouts = ALLOC(dcallout, data->ncallouts);
    sco = scallouts = ALLOCA(scallout, data->ncallouts);
    sw_readv((char *) scallouts, data->sectors,
	     data->ncallouts * (long) sizeof(scallout), data->cooffset);

    for (n = data->ncallouts; n > 0; --n) {
	co->time = sco->time;
	co->nargs = sco->nargs;
	if (sco->val[0].type != T_INVALID) {
	    d_get_values(data, sco->val, co->val,
			 (sco->nargs > 3) ? 4 : sco->nargs + 1);
	} else {
	    co->val[0].type = T_INVALID;
	}
	sco++;
	co++;
    }

    AFREE(scallouts);
}

/*
 * NAME:	data->new_call_out()
 * DESCRIPTION:	add a new call_out
 */
uindex d_new_call_out(data, func, t, nargs)
register dataspace *data;
string *func;
Uint t;
int nargs;
{
    register dcallout *co;
    register value *v;
    register uindex n;

    if (data->ncallouts == 0) {
	/*
	 * the first in this object
	 */
	data->callouts = ALLOC(dcallout, 1);
	data->ncallouts = 1;
	co = data->callouts;
	data->modified |= M_NEWCALLOUT;
    } else {
	if (data->callouts == (dcallout *) NULL) {
	    d_get_callouts(data);
	}
	n = data->fcallouts;
	if (n != 0) {
	    /*
	     * from free list
	     */
	    co = &data->callouts[n - 1];
	    if (co->co_next == 0 || co->co_next > n) {
		/* take 1st free callout */
		data->fcallouts = co->co_next;
	    } else {
		/* take 2nd free callout */
		co = &data->callouts[co->co_next - 1];
		data->callouts[n - 1].co_next = co->co_next;
		if (co->co_next != 0) {
		    data->callouts[co->co_next - 1].co_prev = n;
		}
	    }
	    data->modified |= M_CALLOUT;
	} else {
	    /*
	     * add new callout
	     */
	    co = ALLOC(dcallout, data->ncallouts + 1);
	    memcpy(co, data->callouts, data->ncallouts * sizeof(dcallout));
	    FREE(data->callouts);
	    data->callouts = co;
	    co += data->ncallouts++;
	    data->modified |= M_NEWCALLOUT;
	}
    }

    co->time = t;
    co->nargs = nargs;
    v = co->val;
    v[0].type = T_STRING;
    str_ref(v[0].u.string = func);
    ref_rhs(data, &v[0]);

    switch (nargs) {
    case 3:
	v[3] = sp[2];
	ref_rhs(data, &v[3]);
    case 2:
	v[2] = sp[1];
	ref_rhs(data, &v[2]);
    case 1:
	v[1] = sp[0];
	ref_rhs(data, &v[1]);
    case 0:
	break;

    default:
	v[1] = *sp++;
	ref_rhs(data, &v[1]);
	v[2] = *sp++;
	ref_rhs(data, &v[2]);
	v[3].type = T_ARRAY;
	nargs -= 2;
	arr_ref(v[3].u.array = arr_alloc((unsigned short) nargs));
	memcpy(v[3].u.array->elts, sp, nargs * sizeof(value));
	ref_rhs(data, &v[3]);
	break;
    }
    sp += nargs;

    return co - data->callouts + 1;
}

/*
 * NAME:	data->get_call_out()
 * DESCRIPTION:	get a call_out
 */
char *d_get_call_out(data, handle, t, nargs)
dataspace *data;
uindex handle;
Uint *t;
int *nargs;
{
    static char func[STRINGSZ];
    register dcallout *co;
    register value *v;
    register uindex n;

    if (handle == 0 || handle > data->ncallouts) {
	/* no such call_out */
	return (char *) NULL;
    }
    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    co = &data->callouts[handle - 1];
    if (co->val[0].type == T_INVALID) {
	/* invalid call_out */
	return (char *) NULL;
    }
    i_check_stack(*nargs = co->nargs);
    *t = co->time;
    v = co->val;

    strncpy(func, v[0].u.string->text, STRINGSZ - 1);
    func[STRINGSZ - 1] = '\0';
    del_lhs(data, &v[0]);
    str_del(v[0].u.string);
    v[0].type = T_INVALID;

    switch (co->nargs) {
    case 3:
	del_lhs(data, &v[3]);
	*--sp = v[3];
    case 2:
	del_lhs(data, &v[2]);
	*--sp = v[2];
    case 1:
	del_lhs(data, &v[1]);
	*--sp = v[1];
    case 0:
	break;

    default:
	n = co->nargs - 2;
	sp -= n;
	memcpy(sp, d_get_elts(v[3].u.array), n * sizeof(value));
	del_lhs(data, &v[3]);
	FREE(v[3].u.array->elts);
	v[3].u.array->elts = (value *) NULL;
	arr_del(v[3].u.array);
	del_lhs(data, &v[2]);
	*--sp = v[2];
	del_lhs(data, &v[1]);
	*--sp = v[1];
	break;
    }

    /* wipe out destructed objects */
    for (n = co->nargs, v = sp; n > 0; --n, v++) {
	if (v->type == T_OBJECT && DESTRUCTED(v)) {
	    v->type = T_INT;
	    v->u.number = 0;
	}
    }

    n = data->fcallouts;
    if (n != 0) {
	data->callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    data->fcallouts = handle;

    data->modified |= M_CALLOUT;
    return func;
}

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two call_outs
 */
static int cmp(v1, v2)
value *v1, *v2;
{
    return v1->u.array->elts[2].u.number - v2->u.array->elts[2].u.number;
}

/*
 * NAME:	data->list_callouts()
 * DESCRIPTION:	list all call_outs in an object
 */
array *d_list_callouts(data, t)
register dataspace *data;
Uint t;
{
    register uindex n, count, size;
    register dcallout *co;
    register value *v, *v2, *elts;
    array *list, *a;

    if (data->ncallouts == 0) {
	return arr_alloc((unsigned short) 0);
    }
    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    /* get the number of callouts in this object */
    count = data->ncallouts;
    for (n = data->fcallouts; n != 0; n = data->callouts[n - 1].co_next) {
	--count;
    }

    list = arr_alloc((unsigned short) count);
    elts = list->elts;
    for (n = count, v = elts; n > 0; --n) {
	(v++)->type = T_INVALID;
    }

    if (ec_push()) {
	/*
	 * Free the list of callouts.  Efficiency is not important here.
	 */
	arr_ref(list);
	arr_del(list);
	error((char *) NULL);	/* pass on error */
    }
    for (co = data->callouts; count > 0; co++) {
	if (co->val[0].type != T_INVALID) {
	    size = co->nargs;
	    a = arr_new(size + 3L);
	    v = a->elts;

	    /* handle */
	    v->type = T_INT;
	    (v++)->u.number = co - data->callouts + 1;
	    /* function */
	    v->type = T_STRING;
	    str_ref((v++)->u.string = co->val[0].u.string);
	    /* time */
	    v->type = T_INT;
	    (v++)->u.number = co->time - t;

	    /* copy arguments */
	    switch (size) {
	    case 3:
		*v++ = co->val[3];
	    case 2:
		*v++ = co->val[2];
	    case 1:
		*v++ = co->val[1];
	    case 0:
		break;

	    default:
		n = size - 2;
		for (v2 = d_get_elts(co->val[3].u.array) + n; n > 0; --n) {
		    *v++ = *--v2;
		}
		*v++ = co->val[2];
		*v++ = co->val[1];
		break;
	    }
	    while (size > 0) {
		i_ref_value(--v);
		--size;
	    }

	    /* put in list */
	    elts->type = T_ARRAY;
	    arr_ref((elts++)->u.array = a);
	    --count;
	}
    }
    ec_pop();

    /* sort by time */
    qsort(list->elts, list->size, sizeof(value), cmp);
    return list;
}

/*
 * NAME:	data->save_control()
 * DESCRIPTION:	save the control block
 */
static void d_save_control(ctrl)
register control *ctrl;
{
    scontrol header;
    register long size;
    register uindex i;
    register sector *v;

    /*
     * Save a control block. This is only done once for each control block.
     */

    /* calculate the size of the control block */
    size = sizeof(scontrol) +
           ctrl->ninherits * sizeof(dinherit) +
    	   ctrl->progsize +
    	   ctrl->nstrings * (long) sizeof(dstrconst) +
    	   ctrl->strsize +
    	   ctrl->nfuncdefs * sizeof(dfuncdef) +
    	   ctrl->nvardefs * sizeof(dvardef) +
    	   ctrl->nfuncalls * 2L +
    	   ctrl->nsymbols * (long) sizeof(dsymbol);

    /* create sector space */
    ctrl->nsectors = header.nsectors = i = sw_mapsize(size);
    ctrl->sectors = v = ALLOC(sector, i);
    do {
	*v++ = sw_new();
    } while (--i > 0);
    ctrl->inherits[ctrl->ninherits - 1].obj->cfirst = ctrl->sectors[0];

    /* create header */
    header.ninherits = ctrl->ninherits;
    header.compiled = ctrl->compiled;
    header.progsize = ctrl->progsize;
    header.nstrings = ctrl->nstrings;
    header.strsize = ctrl->strsize;
    header.nfuncdefs = ctrl->nfuncdefs;
    header.nvardefs = ctrl->nvardefs;
    header.nfuncalls = ctrl->nfuncalls;
    header.nsymbols = ctrl->nsymbols;
    header.nvariables = ctrl->nvariables;
    header.nfloatdefs = ctrl->nfloatdefs;
    header.nfloats = ctrl->nfloats;

    /*
     * Copy everything to the swap device.
     */

    /* save header */
    sw_writev((char *) &header, ctrl->sectors, (long) sizeof(scontrol), 0L);
    size = sizeof(scontrol);

    /* save sector map */
    sw_writev((char *) ctrl->sectors, ctrl->sectors,
	      header.nsectors * (long) sizeof(sector), size);
    size += header.nsectors * (long) sizeof(sector);

    /* save inherits */
    sw_writev((char *) ctrl->inherits, ctrl->sectors,
	      header.ninherits * (long) sizeof(dinherit), size);
    size += header.ninherits * (long) sizeof(dinherit);

    /* save program */
    if (header.progsize > 0) {
	sw_writev(ctrl->prog, ctrl->sectors, (long) header.progsize, size);
	size += header.progsize;
    }

    /* save string constants */
    if (header.nstrings > 0) {
	dstrconst *ss;
	char *tt;
	register string **strs;
	register dstrconst *s;
	register long strsize;
	register char *t;

	ss = ALLOCA(dstrconst, header.nstrings);
	if (ctrl->strsize > 0) {
	    tt = ALLOCA(char, ctrl->strsize);
	}

	strs = ctrl->strings;
	strsize = 0;
	s = ss;
	t = tt;
	for (i = header.nstrings; i > 0; --i) {
	    s->index = strsize;
	    strsize += s->len = (*strs)->len;
	    memcpy(t, (*strs++)->text, s->len);
	    t += (s++)->len;
	}

	sw_writev((char *) ss, ctrl->sectors,
		  header.nstrings * (long) sizeof(dstrconst), size);
	size += header.nstrings * (long) sizeof(dstrconst);
	if (strsize > 0) {
	    sw_writev(tt, ctrl->sectors, strsize, size);
	    size += strsize;
	    AFREE(tt);
	}
	AFREE(ss);
    }

    /* save function definitions */
    if (UCHAR(header.nfuncdefs) > 0) {
	sw_writev((char *) ctrl->funcdefs, ctrl->sectors,
		  UCHAR(header.nfuncdefs) * (long) sizeof(dfuncdef), size);
	size += UCHAR(header.nfuncdefs) * (long) sizeof(dfuncdef);
    }

    /* save variable definitions */
    if (UCHAR(header.nvardefs) > 0) {
	sw_writev((char *) ctrl->vardefs, ctrl->sectors,
		  UCHAR(header.nvardefs) * (long) sizeof(dvardef), size);
	size += UCHAR(header.nvardefs) * (long) sizeof(dvardef);
    }

    /* save function call table */
    if (header.nfuncalls > 0) {
	sw_writev((char *) ctrl->funcalls, ctrl->sectors, header.nfuncalls * 2L,
		  size);
	size += header.nfuncalls * 2L;
    }

    /* save symbol table */
    if (header.nsymbols > 0) {
	sw_writev((char *) ctrl->symbols, ctrl->sectors,
		  header.nsymbols * (long) sizeof(dsymbol), size);
    }
}


static dataspace *sdata;	/* the dataspace block currently being saved */
static uindex narr, nstr, cstr;	/* # of arrays, strings, string constants */
static long arrsize, strsize;	/* # of array elements, total string size */

/*
 * NAME:	data->count()
 * DESCRIPTION:	recursively count the number of arrays and strings in an object
 */
static void d_count(v, n)
register value *v;
register unsigned short n;
{
    while (n > 0) {
	switch (v->type) {
	case T_STRING:
	    if (v->u.string->ref & STR_CONST) {
		register long i;

		/*
		 * The string is a constant.  See if it
		 * is among the string constants of this object and the objects
		 * inherited by it.
		 */
		i = strconst_obj(sdata, v->u.string);
		if (i >= 0) {
		    if (str_put(v->u.string, -1L - i) >= 0) {
			/*
			 * The constant was preceded by an identical
			 * string value, but it is marked as a constant,
			 * now.
			 */
			cstr++;
			strsize -= v->u.string->len;
		    }
		    break;
		}
	    }
	    if (str_put(v->u.string, (long) nstr) >= (long) nstr) {
		nstr++;
		strsize += v->u.string->len;
	    }
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    if (arr_put(v->u.array) >= narr) {
		if (v->type == T_MAPPING) {
		    map_compact(v->u.array);
		}
		narr++;
		arrsize += v->u.array->size;
		d_count(d_get_elts(v->u.array), v->u.array->size);
	    }
	    break;
	}

	v++;
	--n;
    }
}

static sarray *sarrays;		/* save arrays */
static svalue *selts;		/* save array elements */
static sstring *sstrings;	/* save strings */
static char *stext;		/* save string elements */

/*
 * NAME:	data->save()
 * DESCRIPTION:	recursively save the values in an object
 */
static void d_save(sv, v, n)
register svalue *sv;
register value *v;
register unsigned short n;
{
    register long i;

    while (n > 0) {
	switch (sv->type = v->type) {
	case T_INT:
	    sv->u.number = v->u.number;
	    break;

	case T_FLOAT:
	    SFLT_PUT(sv, v);
	    break;

	case T_STRING:
	    i = str_put(v->u.string, (long) nstr);
	    if (i < 0) {
		/* string constant (in this object) */
		i = -1 - i;
		sv->u.string.inherit = i >> 16;
		sv->u.string.index = (unsigned short) i;
	    } else {
		/* string value */
		sv->u.string.inherit = -1;
		sv->u.string.index = i;
		if (i >= nstr) {
		    /* new string value */
		    sstrings[i].index = strsize;
		    sstrings[i].len = v->u.string->len;
		    sstrings[i].ref = 0;
		    memcpy(stext + strsize, v->u.string->text,
			   v->u.string->len);
		    strsize += v->u.string->len;
		    nstr++;
		}
		sstrings[i].ref++;
	    }
	    break;

	case T_OBJECT:
	    sv->oindex = v->oindex;
	    sv->u.objcnt = v->u.objcnt;
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    i = arr_put(v->u.array);
	    sv->u.array = i;
	    if (i >= narr) {
		svalue *tmp;

		/* new array */
		sarrays[i].index = arrsize;
		sarrays[i].size = v->u.array->size;
		sarrays[i].ref = 0;
		sarrays[i].tag = v->u.array->tag;
		tmp = selts + arrsize;
		arrsize += v->u.array->size;
		narr++;
		d_save(tmp, v->u.array->elts, v->u.array->size);
	    }
	    sarrays[i].ref++;
	    break;
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * NAME:	data->put_values()
 * DESCRIPTION:	save modified values as svalues
 */
static void d_put_values(sv, v, n)
register svalue *sv;
register value *v;
register unsigned short n;
{
    while (n > 0) {
	if (v->modified) {
	    switch (sv->type = v->type) {
	    case T_INT:
		sv->u.number = v->u.number;
		break;

	    case T_FLOAT:
		SFLT_PUT(sv, v);
		break;

	    case T_STRING:
		if (v->u.string->ref & STR_CONST) {
		    register long i;

		    /* string constant */
		    i = strconst_obj(sdata, v->u.string);
		    sv->u.string.inherit = i >> 16;
		    sv->u.string.index = (unsigned short) i;
		} else {
		    /* string value */
		    sv->u.string.inherit = -1;
		    sv->u.string.index = v->u.string->u.primary -
					 sdata->strings;
		}
		break;

	    case T_OBJECT:
		sv->oindex = v->oindex;
		sv->u.objcnt = v->u.objcnt;
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		sv->u.array = v->u.array->primary - sdata->arrays;
		break;
	    }
	    v->modified = FALSE;
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * NAME:	data->save_dataspace()
 * DESCRIPTION:	Save all values in a dataspace block.  Return TRUE if the
 *		necessary changes were such that the dataspace block can
 *		continue to be used.
 */
static bool d_save_dataspace(data)
register dataspace *data;
{
    sdataspace header;
    register uindex n;

    sdata = data;

    if (!(data->nsectors == 0 && (data->modified & M_VARIABLE)) &&
	data->achange == 0 && data->schange == 0 &&
	!(data->modified & M_NEWCALLOUT)) {
	bool mod;

	/*
	 * No strings/arrays added or deleted. Check individual variables and
	 * array elements.
	 */
	if (data->modified & M_VARIABLE) {
	    /*
	     * variables changed
	     */
	    d_put_values(data->svariables, data->variables, data->nvariables);
	    sw_writev((char *) data->svariables, data->sectors,
		      data->nvariables * (long) sizeof(svalue),
		      data->varoffset);
	}
	if (data->modified & M_ARRAYREF) {
	    register sarray *sa;
	    register arrref *a;

	    /*
	     * references to arrays changed
	     */
	    sa = data->sarrays;
	    a = data->arrays;
	    mod = FALSE;
	    for (n = data->narrays; n > 0; --n) {
		if (a->array != (array *) NULL && sa->ref != a->ref) {
		    sa->ref = a->ref;
		    mod = TRUE;
		}
		sa++;
		a++;
	    }
	    if (mod) {
		sw_writev((char *) data->sarrays, data->sectors,
			  data->narrays * (long) sizeof(sarray),
			  data->arroffset);
	    }
	}
	if (data->modified & M_ARRAY) {
	    register arrref *a;

	    /*
	     * array elements changed
	     */
	    a = data->arrays;
	    for (n = data->narrays; n > 0; --n) {
		if (a->array != (array *) NULL && (a->index & ARR_MOD)) {
		    a->index &= ~ARR_MOD;
		    d_put_values(&data->selts[a->index], a->array->elts,
				 a->array->size);
		    sw_writev((char *) &data->selts[a->index], data->sectors,
			      a->array->size * (long) sizeof(svalue),
			      data->arroffset +
				data->narrays * (long) sizeof(sarray) +
				a->index * sizeof(svalue));
		}
		a++;
	    }
	}
	if (data->modified & M_STRINGREF) {
	    register sstring *ss;
	    register strref *s;

	    /*
	     * string references changed
	     */
	    ss = data->sstrings;
	    s = data->strings;
	    mod = FALSE;
	    for (n = data->nstrings; n > 0; --n) {
		if (s->string != (string *) NULL && ss->ref != s->ref) {
		    ss->ref = s->ref;
		    mod = TRUE;
		}
		ss++;
		s++;
	    }
	    if (mod) {
		sw_writev((char *) data->sstrings, data->sectors,
			  data->nstrings * (long) sizeof(sstring),
			  data->stroffset);
	    }
	}
	if (data->modified & M_CALLOUT) {
	    scallout *scallouts;
	    register scallout *sco;
	    register dcallout *co;

	    /* save new (?) fcallouts value */
	    sw_writev((char *) &data->fcallouts, data->sectors,
		      (long) sizeof(uindex),
		      (long) ((char *) &header.fcallouts - (char *) &header));

	    sco = scallouts = ALLOCA(scallout, data->ncallouts);
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->nargs = co->nargs;
		if (co->val[0].type != T_INVALID) {
		    co->val[0].modified = TRUE;
		    co->val[1].modified = TRUE;
		    co->val[2].modified = TRUE;
		    co->val[3].modified = TRUE;
		    d_put_values(sco->val, co->val,
				 (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_INVALID;
		}
		sco++;
		co++;
	    }

	    sw_writev((char *) scallouts, data->sectors,
		      data->ncallouts * (long) sizeof(scallout),
		      data->cooffset);
	    AFREE(scallouts);
	}

	/* the data block can remain in memory */
	data->modified = 0;
	return TRUE;

    } else {
	scallout *scallouts;
	register long size;

	/*
	 * count the number and sizes of strings and arrays
	 */
	narr = 0;
	nstr = 0;
	cstr = 0;
	arrsize = 0;
	strsize = 0;

	if (data->nvariables > 0) {
	    d_get_variable(data, 0);
	    if (data->svariables == (svalue *) NULL) {
		data->svariables = ALLOC(svalue, data->nvariables);
	    } else {
		register value *v;
		register svalue *sv;

		sv = data->svariables;
		v = data->variables;
		for (n = data->nvariables; n > 0; --n) {
		    if (v->type == T_INVALID) {
			d_get_values(data, sv, v, 1);
		    }
		    sv++;
		    v++;
		}
	    }
	    d_count(data->variables, data->nvariables);
	}
	if (data->ncallouts > 0) {
	    register dcallout *co;

	    if(data->callouts == (dcallout *) NULL) {
		d_get_callouts(data);
	    }
	    /* remove empty callouts at the end */
	    for (n = data->ncallouts, co = data->callouts + n; n > 0; --n) {
		if ((--co)->val[0].type != T_INVALID) {
		    break;
		}
		if (data->fcallouts == n) {
		    /* first call_out in the free list */
		    data->fcallouts = co->co_next;
		} else {
		    /* connect previous to next */
		    data->callouts[co->co_prev - 1].co_next = co->co_next;
		    if (co->co_next != 0) {
			/* connect next to previous */
			data->callouts[co->co_next - 1].co_prev = co->co_prev;
		    }
		}
	    }
	    data->ncallouts = n;
	    if (n == 0) {
		/* all callouts removed */
		FREE(data->callouts);
		data->callouts = (dcallout *) NULL;
	    } else {
		/* process callouts */
		scallouts = ALLOCA(scallout, n);
		for (co = data->callouts; n > 0; --n, co++) {
		    if (co->val[0].type != T_INVALID) {
			d_count(co->val, (co->nargs > 3) ? 4 : co->nargs + 1);
		    }
		}
	    }
	}

	/* fill in header */
	header.narrays = narr;
	header.eltsize = arrsize;
	header.nstrings = nstr - cstr;
	header.strsize = strsize;
	header.ncallouts = data->ncallouts;
	header.fcallouts = data->fcallouts;
	header.nsectors = sw_mapsize(sizeof(sdataspace) +
			  (data->nvariables + header.eltsize) * sizeof(svalue) +
			  header.narrays * (long) sizeof(sarray) +
			  header.nstrings * (long) sizeof(sstring) +
			  header.strsize +
			  header.ncallouts * (long) sizeof(scallout));

	/*
	 * put everything into a saveable form
	 */
	if (header.nstrings > 0) {
	    sstrings = ALLOCA(sstring, header.nstrings);
	    if (header.strsize > 0) {
		stext = ALLOCA(char, header.strsize);
	    }
	}
	if (header.narrays > 0) {
	    sarrays = ALLOCA(sarray, header.narrays);
	    if (header.eltsize > 0) {
		selts = ALLOCA(svalue, header.eltsize);
	    }
	}
	narr = 0;
	nstr = 0;
	arrsize = 0;
	strsize = 0;

	d_save(data->svariables, data->variables, data->nvariables);
	if (header.ncallouts > 0) {
	    register scallout *sco;
	    register dcallout *co;

	    sco = scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->nargs = co->nargs;
		if (co->val[0].type != T_INVALID) {
		    d_save(sco->val, co->val,
			   (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_INVALID;
		}
		sco++;
		co++;
	    }
	}

	/* clear hash tables */
	str_clear();
	arr_clear();

	/*
	 * create the sectors to save everything on
	 */
	if (data->nsectors == 0) {
	    register sector *sectors;

	    /* no sectors yet */
	    data->sectors = sectors = ALLOC(sector, header.nsectors);
	    for (n = header.nsectors; n > 0; --n) {
		*sectors++ = sw_new();
	    }
	    data->obj->dfirst = data->sectors[0];
	} else if (data->nsectors < header.nsectors) {
	    register sector *sectors;

	    /* not enough sectors */
	    sectors = ALLOC(sector, header.nsectors);
	    memcpy(sectors, data->sectors, data->nsectors * sizeof(sector));
	    FREE(data->sectors);
	    data->sectors = sectors;
	    sectors += data->nsectors;
	    for (n = header.nsectors - data->nsectors; n > 0; --n) {
		*sectors++ = sw_new();
	    }
	} else if (data->nsectors > header.nsectors) {
	    register sector *sectors;

	    /* too many sectors */
	    sectors = data->sectors + data->nsectors;
	    for (n = data->nsectors - header.nsectors; n > 0; --n) {
		sw_del(*--sectors);
	    }
	}
	data->nsectors = header.nsectors;

	/* save header */
	size = sizeof(sdataspace);
	sw_writev((char *) &header, data->sectors, size, 0L);
	sw_writev((char *) data->sectors, data->sectors,
		  header.nsectors * (long) sizeof(sector), size);
	size += header.nsectors * (long) sizeof(sector);

	/* save variables */
	if (data->nvariables > 0) {
	    sw_writev((char *) data->svariables, data->sectors,
		      data->nvariables * (long) sizeof(svalue), size);
	    size += data->nvariables * (long) sizeof(svalue);
	}

	/* save arrays */
	if (header.narrays > 0) {
	    sw_writev((char *) sarrays, data->sectors,
		      header.narrays * (long) sizeof(sarray), size);
	    size += header.narrays * (long) sizeof(sarray);
	    if (header.eltsize > 0) {
		sw_writev((char *) selts, data->sectors,
			  header.eltsize * sizeof(svalue), size);
		size += header.eltsize * sizeof(svalue);
		AFREE(selts);
	    }
	    AFREE(sarrays);
	}

	/* save strings */
	if (header.nstrings > 0) {
	    sw_writev((char *) sstrings, data->sectors,
		      header.nstrings * (long) sizeof(sstring), size);
	    size += header.nstrings * (long) sizeof(sstring);
	    if (header.strsize > 0) {
		sw_writev(stext, data->sectors, header.strsize, size);
		size += header.strsize;
		AFREE(stext);
	    }
	    AFREE(sstrings);
	}

	/* save callouts */
	if (header.ncallouts > 0) {
	    sw_writev((char *) scallouts, data->sectors,
		      header.ncallouts * (long) sizeof(scallout), size);
	    AFREE(scallouts);
	}

	/* the data block must be freed */
	return FALSE;
    }
}

/*
 * NAME:	data->free_control()
 * DESCRIPTION:	remove the control block from memory
 */
static void d_free_control(ctrl)
register control *ctrl;
{
    register uindex i;
    register string **strs;
    object *obj;

    obj = ctrl->inherits[ctrl->ninherits - 1].obj;
    if (obj != (object *) NULL) {
	obj->ctrl = (control *) NULL;
    }

    /* delete strings */
    if (ctrl->strings != (string **) NULL) {
	strs = ctrl->strings;
	for (i = ctrl->nstrings; i > 0; --i) {
	    if (*strs != (string *) NULL) {
		(*strs)->ref &= ~STR_CONST;
		(*strs)->u.primary = (strref *) NULL;
		str_del(*strs);
	    }
	    strs++;
	}
	FREE(ctrl->strings);
    }

    if (obj == (object *) NULL || !(obj->flags & O_COMPILED)) {
	/* delete sectors */
	if (ctrl->sectors != (sector *) NULL) {
	    FREE(ctrl->sectors);
	}

	/* delete inherits */
	FREE(ctrl->inherits);

	if (ctrl->prog != (char *) NULL) {
	    FREE(ctrl->prog);
	}

	/* delete string constants */
	if (ctrl->sstrings != (dstrconst *) NULL) {
	    if (ctrl->stext != (char *) NULL) {
		FREE(ctrl->stext);
	    }
	    FREE(ctrl->sstrings);
	}

	/* delete function definitions */
	if (ctrl->funcdefs != (dfuncdef *) NULL) {
	    FREE(ctrl->funcdefs);
	}

	/* delete variable definitions */
	if (ctrl->vardefs != (dvardef *) NULL) {
	    FREE(ctrl->vardefs);
	}

	/* delete function call table */
	if (ctrl->funcalls != (char *) NULL) {
	    FREE(ctrl->funcalls);
	}

	/* delete symbol table */
	if (ctrl->symbols != (dsymbol *) NULL) {
	    FREE(ctrl->symbols);
	}
    }

    if (ctrl != chead) {
	ctrl->prev->next = ctrl->next;
    } else {
	chead = ctrl->next;
	if (chead != (control *) NULL) {
	    chead->prev = (control *) NULL;
	}
    }
    if (ctrl != ctail) {
	ctrl->next->prev = ctrl->prev;
    } else {
	ctail = ctrl->prev;
	if (ctail != (control *) NULL) {
	    ctail->next = (control *) NULL;
	}
    }
    --nctrl;

    FREE(ctrl);
}

/*
 * NAME:	data->free_values()
 * DESCRIPTION:	free values in a dataspace block
 */
static void d_free_values(data)
register dataspace *data;
{
    register uindex i;

    /* free variables */
    if (data->variables != (value *) NULL) {
	register value *v;

	for (i = data->nvariables, v = data->variables; i > 0; --i, v++) {
	    i_del_value(v);
	}

	if (data->svariables != (svalue *) NULL) {
	    FREE(data->svariables);
	}
	FREE(data->variables);
    }

    /* free callouts */
    if (data->callouts != (dcallout *) NULL) {
	register dcallout *co;
	register value *v;
	register int j;

	for (i = data->ncallouts, co = data->callouts; i > 0; --i, co++) {
	    v = co->val;
	    if (v->type != T_INVALID) {
		j = 1 + co->nargs;
		if (j > 4) {
		    j = 4;
		}
		do {
		    i_del_value(v++);
		} while (--j > 0);
	    }
	}

	FREE(data->callouts);
    }

    if (data->arrays != (arrref *) NULL) {
	register array *a;
	register arrref *aa;
	bool fetch;

	/* export arrays */
	do {
	    fetch = FALSE;
	    for (i = data->narrays, aa = data->arrays; i > 0; --i, aa++) {
		a = aa->array;
		if (a != (array *) NULL && a->ref != 1 && a->size != 0 &&
		    a->elts[0].type == T_INVALID) {
		    d_get_elts(a);
		    fetch = TRUE;
		}
	    }
	} while (fetch);

	/* free arrays */
	for (i = data->narrays, aa = data->arrays; i > 0; --i, aa++) {
	    a = aa->array;
	    if (a != (array *) NULL) {
		a->primary = (arrref *) NULL;
		arr_del(a);
	    }
	}

	if (data->selts != (svalue *) NULL) {
	    FREE(data->selts);
	}
	FREE(data->sarrays);
	FREE(data->arrays);
    }

    /* free strings */
    if (data->strings != (strref *) NULL) {
	register strref *s;

	for (i = data->nstrings, s = data->strings; i > 0; --i, s++) {
	    if (s->string != (string *) NULL) {
		s->string->u.primary = (strref *) NULL;
		str_del(s->string);
	    }
	}

	if (data->stext != (char *) NULL) {
	    FREE(data->stext);
	}
	FREE(data->sstrings);
	FREE(data->strings);
    }
}

/*
 * NAME:	data->free_dataspace()
 * DESCRIPTION:	remove the dataspace block from memory
 */
static void d_free_dataspace(data)
register dataspace *data;
{
    /* delete sectors */
    if (data->sectors != (sector *) NULL) {
	FREE(data->sectors);
    }

    data->obj->data = (dataspace *) NULL;
    data->ctrl->ndata--;

    if (data != dhead) {
	data->prev->next = data->next;
    } else {
	dhead = data->next;
	if (dhead != (dataspace *) NULL) {
	    dhead->prev = (dataspace *) NULL;
	}
    }
    if (data != dtail) {
	data->next->prev = data->prev;
    } else {
	dtail = data->prev;
	if (dtail != (dataspace *) NULL) {
	    dtail->next = (dataspace *) NULL;
	}
    }
    --ndata;

    FREE(data);
}


/*
 * NAME:	data->swapout()
 * DESCRIPTION:	Swap out a portion of the control and dataspace blocks in
 *		memory.  Return the number of dataspace blocks swapped out.
 */
uindex d_swapout(frag)
int frag;
{
    register uindex n, count;
    register control *ctrl, *next;

    count = 0;

    /* swap out dataspace blocks */
    for (n = ndata / frag; n > 0; --n) {
	if (dtail->modified != 0) {
	    d_save_dataspace(dtail);
	    count++;
	}
	d_free_values(dtail);
	d_free_dataspace(dtail);
    }

    /* swap out control blocks */
    ctrl = ctail;
    for (n = nctrl / frag; n > 0; --n) {
	next = ctrl->prev;
	if (ctrl->ndata == 0) {
	    if (ctrl->sectors == (sector *) NULL &&
		!(ctrl->inherits[ctrl->ninherits - 1].obj->flags & O_COMPILED))
	    {
		d_save_control(ctrl);
	    }
	    d_free_control(ctrl);
	}
	ctrl = next;
    }

    return count;
}

/*
 * NAME:	data->swapsync()
 * DESCRIPTION:	Synchronize the swap file with the state of memory, swapping
 *		out as little as possible.
 */
void d_swapsync()
{
    register control *ctrl;
    register dataspace *data, *next;

    /* save control blocks */
    for (ctrl = ctail; ctrl != (control *) NULL; ctrl = ctrl->prev) {
	if (ctrl->sectors == (sector *) NULL &&
	    !(ctrl->inherits[ctrl->ninherits - 1].obj->flags & O_COMPILED)) {
	    d_save_control(ctrl);
	}
    }

    /* save dataspace blocks */
    for (data = dtail; data != (dataspace *) NULL; data = next) {
	next = data->prev;
	if (data->modified != 0 && !d_save_dataspace(data)) {
	    /*
	     * the data block is not in proper shape for another sync; get
	     * it from the swap device, next time
	     */
	    d_free_values(data);
	    d_free_dataspace(data);
	}
    }
}

/*
 * NAME:	data->patch_ctrl()
 * DESCRIPTION:	patch a control block
 */
void d_patch_ctrl(ctrl, offset)
register control *ctrl;
long offset;
{
    register int i;
    register dinherit *inh;

    for (i = ctrl->ninherits, inh = ctrl->inherits; i > 0; --i, inh++) {
	inh->obj = (object *) ((char *) inh->obj + offset);
    }
    sw_writev((char *) ctrl->inherits, ctrl->sectors,
	      ctrl->ninherits * (long) sizeof(dinherit),
	      sizeof(scontrol) + ctrl->nsectors * (long) sizeof(sector));
}

/*
 * NAME:	data->patch_callout()
 * DESCRIPTION:	patch call_outs in a data block
 */
void d_patch_callout(data, offset)
dataspace *data;
long offset;
{
    if (data->ncallouts > 0) {
	scallout *scallouts;
	register scallout *sco;
	register uindex n;
	bool modified;

	modified = FALSE;
	scallouts = ALLOCA(scallout, data->ncallouts);
	sw_readv((char *) scallouts, data->sectors,
		 data->ncallouts * (long) sizeof(scallout), data->cooffset);
	for (n = data->ncallouts, sco = scallouts; n > 0; --n, sco++) {
	    if (sco->val[0].type != T_INVALID) {
		sco->time += offset;
		modified = TRUE;
	    }
	}
	if (modified) {
	    sw_writev((char *) scallouts, data->sectors,
		      data->ncallouts * (long) sizeof(scallout),
		      data->cooffset);
	}
	AFREE(scallouts);
    }
}

/*
 * NAME:	data->del_control()
 * DESCRIPTION:	delete a control block from swap and memory
 */
void d_del_control(ctrl)
register control *ctrl;
{
    register uindex i;
    register sector *s;

    if (ctrl->sectors != (sector *) NULL) {
	for (i = ctrl->nsectors, s = ctrl->sectors + i; i > 0; --i) {
	    sw_del(*--s);
	}
    }
    d_free_control(ctrl);
}

/*
 * NAME:	data->del_dataspace()
 * DESCRIPTION:	delete a dataspace block from swap and memory
 */
void d_del_dataspace(data)
register dataspace *data;
{
    register uindex i;
    register sector *s;

    d_free_values(data);
    if (data->sectors != (sector *) NULL) {
	for (i = data->nsectors, s = data->sectors + i; i > 0; --i) {
	    sw_del(*--s);
	}
    }
    d_free_dataspace(data);
}
