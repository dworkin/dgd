# include "dgd.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"

/* bit values for dataspace->modified */
# define M_VARIABLE		0x01
# define M_ARRAY		0x02
# define M_ARRAYREF		0x04
# define M_STRINGREF		0x08

# define ARR_MOD		0x80000000L

typedef struct {
    uindex nsectors;		/* # sectors in part one */
    char ninherits;		/* # objects in inherit table */
    char nvirtuals;		/* # virtually inherited objects */
    unsigned short progsize;	/* size of program code */
    unsigned short nstrings;	/* # strings in string constant table */
    long strsize;		/* size of string constant table */
    char nfuncdefs;		/* # entries in function definition table */
    char nvardefs;		/* # entries in variable definition table */
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
} scontrol;

typedef struct _sstrconst_ {
    long index;			/* index in control block */
    unsigned short len;		/* string length */
} sstrconst;


typedef struct {
    uindex nsectors;		/* number of sectors in data space */
    uindex narrays;		/* number of array values */
    long eltsize;		/* total size of array elements */
    uindex nstrings;		/* number of strings */
    long strsize;		/* total size of strings */
} sdataspace;

typedef struct _svalue_ {
    char type;			/* object, number, string, array */
    union {
	objkey object;		/* object */
	long number;		/* number */
	struct {
	    short inherit;
	    uindex index;
	} string;		/* string */
	uindex array;		/* array */
    } u;
} svalue;

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


static control *clist;		/* list of control blocks */
static dataspace *dlist;	/* list of dataspace blocks */

/*
 * NAME:	data->new_control()
 * DESCRIPTION:	create a new control block
 */
control *d_new_control()
{
    register control *ctrl;

    ctrl = ALLOC(control, 1);
    ctrl->prev = (control *) NULL;
    ctrl->next = clist;
    if (clist != (control *) NULL) {
	clist->prev = ctrl;
    }
    clist = ctrl;

    ctrl->nsectors = 0;		/* nothing on swap device yet */
    ctrl->sectors = (sector *) NULL;
    ctrl->ninherits = 0;
    ctrl->nvirtuals = 0;
    ctrl->inherits = (dinherit *) NULL;
    ctrl->progsize = 0;
    ctrl->prog = (char *) NULL;
    ctrl->nstrings = 0;
    ctrl->strings = (string **) NULL;
    ctrl->nfuncdefs = 0;
    ctrl->funcdefs = (dfuncdef *) NULL;
    ctrl->nvardefs = 0;
    ctrl->vardefs = (dvardef *) NULL;
    ctrl->nfuncalls = 0;
    ctrl->funcalls = (char *) NULL;
    ctrl->nsymbols = 0;
    ctrl->symbols = (dsymbol *) NULL;

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
    data->next = dlist;
    if (dlist != (dataspace *) NULL) {
	dlist->prev = data;
    }
    dlist = data;
    data->achange = 0;
    data->schange = 0;
    data->obj = obj;
    data->ctrl = o_control(obj);

    /* sectors */
    data->nsectors = 0;			/* nothing on swap device yet */
    data->sectors = (sector *) NULL;

    /* variables */
    if ((data->nvariables=data->ctrl->nvariables) > 0) {
	register value *v;
	register unsigned short i;

	i = data->nvariables;
	v = data->variables = ALLOC(value, i);
	data->svariables = ALLOC(svalue, i);
	do {
	    v->type = T_NUMBER;
	    v->modified = TRUE;
	    (v++)->u.number = 0;
	} while (--i > 0);
	data->modified = M_VARIABLE;
    } else {
	data->variables = (value *) NULL;
	data->svariables = (svalue *) NULL;
	data->modified = 0;
    }

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

    return data;
}

/*
 * NAME:	data->load_control()
 * DESCRIPTION:	load a control block from the swap device
 */
control *d_load_control(sec)
sector sec;
{
    scontrol header;
    register control *ctrl;
    register long size;

    ctrl = ALLOC(control, 1);
    ctrl->prev = (control *) NULL;
    ctrl->next = clist;
    if (clist != (control *) NULL) {
	clist->prev = ctrl;
    }
    clist = ctrl;

    /* header */
    sw_readv((char *) &header, &sec, (long) sizeof(scontrol), 0L);
    ctrl->nsectors = header.nsectors;
    ctrl->sectors = ALLOC(sector, header.nsectors);
    ctrl->sectors[0] = sec;
    size = header.nsectors * (long) sizeof(sector);
    sw_readv((char *) ctrl->sectors, ctrl->sectors, size,
	     (long) sizeof(scontrol));
    size += sizeof(scontrol);

    /* inherits */
    ctrl->ninherits = header.ninherits; /* there is at least one */
    ctrl->nvirtuals = header.nvirtuals;
    ctrl->inherits = ALLOC(dinherit, header.ninherits);
    sw_readv((char *) ctrl->inherits, ctrl->sectors,
	     header.ninherits * (long) sizeof(dinherit), size);
    size += header.ninherits * (long) sizeof(dinherit);

    /* program */
    ctrl->progoffset = size;
    ctrl->progsize = header.progsize;
    size += header.progsize;

    /* string constants */
    ctrl->stroffset = size;
    ctrl->nstrings = header.nstrings;
    ctrl->strings = (string **) NULL;
    ctrl->sstrings = (sstrconst *) NULL;
    ctrl->stext = (char *) NULL;
    ctrl->strsize = header.strsize;
    size += header.nstrings * (long) sizeof(sstrconst) + header.strsize;

    /* function definitions */
    ctrl->funcdoffset = size;
    ctrl->nfuncdefs = header.nfuncdefs;
    ctrl->funcdefs = (dfuncdef *) NULL;
    size += header.nfuncdefs * (long) sizeof(dfuncdef);

    /* variable definitions */
    ctrl->vardoffset = size;
    ctrl->nvardefs = header.nvardefs;
    ctrl->vardefs = (dvardef *) NULL;
    size += header.nvardefs * (long) sizeof(dvardef);

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

    return ctrl;
}

/*
 * NAME:	data->load_dataspace()
 * DESCRIPTION:	load the dataspace header block of an object from the swap
 */
dataspace *d_load_dataspace(obj, sec)
object *obj;
sector sec;
{
    sdataspace header;
    register dataspace *data;
    register long size;

    data = ALLOC(dataspace, 1);
    data->prev = (dataspace *) NULL;
    data->next = dlist;
    if (dlist != (dataspace *) NULL) {
	dlist->prev = data;
    }
    dlist = data;
    data->achange = 0;
    data->schange = 0;
    data->modified = 0;

    /* header */
    sw_readv((char *) &header, &sec, (long) sizeof(sdataspace), 0L);
    data->nsectors = header.nsectors;
    data->sectors = ALLOC(sector, header.nsectors);
    data->sectors[0] = sec;
    size = header.nsectors * (long) sizeof(sector);
    sw_readv((char *) data->sectors, data->sectors, size,
	     (long) sizeof(sdataspace));
    size += sizeof(sdataspace);
    data->obj = obj;
    data->ctrl = o_control(obj);

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
	    header.eltsize * (long) sizeof(svalue);

    /* strings */
    data->stroffset = size;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    data->strings = (strref *) NULL;
    data->sstrings = (sstring *) NULL;
    data->stext = (char *) NULL;

    return data;
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
	sw_readv(ctrl->prog, ctrl->sectors, ctrl->progsize, ctrl->progoffset);
    }
    return ctrl->prog;
}

/*
 * NAME:	data->get_strconst()
 * DESCRIPTION:	get a string constant
 */
string *d_get_strconst(ctrl, inherit, index)
register control *ctrl;
register char inherit;
unsigned short index;
{
    if (UCHAR(inherit) < ctrl->nvirtuals - 1) {
	/* get the proper control block */
	ctrl = o_control(ctrl->inherits[UCHAR(inherit)].obj);
    }

    if (ctrl->strings == (string **) NULL) {
	/* make string pointer block */
	ctrl->strings = ALLOC(string*, ctrl->nstrings);
	memset(ctrl->strings, 0, ctrl->nstrings * sizeof(string *));

	/* load strings */
	ctrl->sstrings = ALLOC(sstrconst, ctrl->nstrings);
	sw_readv((char *) ctrl->sstrings, ctrl->sectors,
		 ctrl->nstrings * (long) sizeof(sstring), ctrl->stroffset);
	/* load strings text */
	ctrl->stext = ALLOC(char, ctrl->strsize);
	sw_readv(ctrl->stext, ctrl->sectors, ctrl->strsize,
		 ctrl->stroffset + ctrl->nstrings * (long) sizeof(sstring));
    }

    if (ctrl->strings[index] == (string *) NULL) {
	sstrconst strcon;
	register string *str;

	str = str_new(ctrl->stext + ctrl->sstrings[index].index,
		      (long) ctrl->sstrings[index].len);
	str->ref |= STR_CONST;
	str_ref(ctrl->strings[index] = str);
	str->u.strconst = &ctrl->strings[index];
    }

    return ctrl->strings[index];
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
		 ctrl->nfuncdefs * (long) sizeof(dfuncdef),
		 ctrl->funcdoffset);
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
static string *d_get_string(data, index)
register dataspace *data;
register uindex index;
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
	/* load strings text */
	data->stext = ALLOC(char, data->strsize);
	sw_readv(data->stext, data->sectors, data->strsize,
		 data->stroffset + data->nstrings * (long) sizeof(sstring));
    }

    if (data->strings[index].string == (string *) NULL) {
	register string *s;

	s = str_new(data->stext + data->sstrings[index].index,
		    (long) data->sstrings[index].len);
	s->u.primary = &data->strings[index];
	str_ref(s->u.primary->string = s);
	s->u.primary->data = data;
	s->u.primary->ref = data->sstrings[index].ref;
    }
    return data->strings[index].string;
}

/*
 * NAME:	data->get_array()
 * DESCRIPTION:	get an array from the dataspace
 */
static array *d_get_array(data, index)
register dataspace *data;
register uindex index;
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
	sw_readv((char *) data->sarrays, data->sectors, data->arroffset,
		 data->narrays * (long) sizeof(sarray));
	/* load array elements */
	data->selts = (svalue *) ALLOC(char, data->eltsize);
	sw_readv((char *) data->selts, data->sectors, data->eltsize,
		 data->arroffset + data->narrays * (long) sizeof(sarray));
    }

    if (data->arrays[index].array == (array *) NULL) {
	register array *a;
	register value *v;

	a = arr_new((long) (i = data->sarrays[index].size));
	if (i > 0) {
	    a->elts[0].type = T_INVALID;
	}
	a->tag = data->sarrays[index].tag;
	a->primary = &data->arrays[index];
	arr_ref(a->primary->array = a);
	a->primary->data = data;
	a->primary->index = data->sarrays[index].index;
	a->primary->ref = data->sarrays[index].ref;
    }
    return data->arrays[index].array;
}

/*
 * NAME:	data->get_value()
 * DESCRIPTION:	get a value from the dataspace
 */
static void d_get_value(data, sv, val)
register dataspace *data;
register svalue *sv;
register value *val;
{
    val->modified = FALSE;
    switch (val->type = sv->type) {
    case T_NUMBER:
	val->u.number = sv->u.number;
	break;

    case T_STRING:
	str_ref(val->u.string = (sv->u.string.inherit < 0) ?
	  d_get_string(data, sv->u.string.index) :
	  d_get_strconst(data->ctrl, sv->u.string.inherit, sv->u.string.index));
	break;

    case T_OBJECT:
	val->u.object = sv->u.object;
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_ref(val->u.array = d_get_array(data, sv->u.array));
	break;
    }
}

/*
 * NAME:	data->get_variable()
 * DESCRIPTION:	get a variable from the dataspace
 */
value *d_get_variable(data, index)
register dataspace *data;
register unsigned short index;
{
    if (data->variables == (value *) NULL) {
	register value *v;
	register unsigned short i;

	/* create room for variables */
	v = data->variables = ALLOC(value, data->nvariables);
	/* variables must be loaded from the swap */
	for (i = data->nvariables; i > 0; --i) {
	    (v++)->type = T_INVALID;
	}

	/* load svalues */
	data->svariables = ALLOC(svalue, data->nvariables);
	sw_readv((char *) data->svariables, data->sectors,
		 data->nvariables * (long) sizeof(svalue), data->varoffset);
    }

    if (data->variables[index].type == T_INVALID) {
	d_get_value(data, &data->svariables[index], &data->variables[index]);
    }
    return &data->variables[index];
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
	register svalue *sv;
	register unsigned short n;

	data = arr->primary->data;
	sv = &data->selts[arr->primary->index];
	for (n = arr->size; n > 0; --n) {
	    d_get_value(data, sv++, v++);
	}
    }

    return arr->elts;
}

/*
 * NAME:	strconst_obj()
 * DESCRIPTION:	find out in which object in the inherit list a string constant
 *		is defined.  Return the index in the list if found and -1
 *		otherwise.
 */
static int strconst_obj(data, str)
dataspace *data;
string *str;
{
    register control *ctrl;
    register string **strconst;
    register dinherit *inh;
    register int n;

    ctrl = data->ctrl;
    n = ctrl->nvirtuals;
    inh = ctrl->inherits + n;
    strconst = str->u.strconst;
    while (--n > 0) {
	/*
	 * no need to force loading the control block here -- if it
	 * isn't loaded, the string constant cannot be in it
	 */
	ctrl = (--inh)->obj->ctrl;
	if (ctrl != (control *) NULL && strconst >= ctrl->strings &&
	    strconst < ctrl->strings + ctrl->nstrings) {
	    return n;
	}
    }

    return -1;	/* not a constant in this object */
}

/*
 * NAME:	check_assign()
 * DESCRIPTION:	Do an assignment, dealing with increased/decreased references
 *		and status flags in the data block (if any).
 */
static void check_assign(data, old, new)
register dataspace *data;
register value *old, *new;
{
    register string *str;
    register array *arr;

    switch (old->type) {
    case T_STRING:
	str = old->u.string;
	if (data != (dataspace *) NULL) {
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
	}
	str_del(str);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr = old->u.array;
	if (data != (dataspace *) NULL) {
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
	}
	arr_del(arr);
	break;
    }

    switch (new->type) {
    case T_STRING:
	str = new->u.string;
	if (data != (dataspace *) NULL) {
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
	}
	str_ref(str);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr = new->u.array;
	if (data != (dataspace *) NULL) {
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
	}
	arr_ref(arr);
	break;
    }

    *old = *new;
    old->modified = TRUE;
}

/*
 * NAME:	data->assign_var()
 * DESCRIPTION:	assign a value to a variable
 */
void d_assign_var(data, var, val)
register dataspace *data;
register value *var;
value *val;
{
    if (var >= data->variables && var < data->variables + data->nvariables) {
	data->modified |= M_VARIABLE;
    } else {
	data = (dataspace *) NULL;
    }
    check_assign(data, var, val);
}

/*
 * NAME:	data->assign_elt()
 * DESCRIPTION:	assign a value to an array element
 */
void d_assign_elt(arr, val, new)
register array *arr;
value *val, *new;
{
    if (arr->primary != (arrref *) NULL) {
	/* the array is in the dataspace of some object */
	check_assign(arr->primary->data, val, new);
	arr->primary->data->modified |= M_ARRAY;
	arr->primary->index |= ARR_MOD;
    } else {
	/* recently allocated array */
	check_assign((dataspace *) NULL, val, new);
    }
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
 * NAME:	data->save_control()
 * DESCRIPTION:	save the control block
 */
static void d_save_control(ctrl)
register control *ctrl;
{
    scontrol header;
    register long size, strsize;
    register uindex i;
    register string **strs;
    register sector *v;

    /*
     * Save the control block. This is only done once.
     */

    /* calculate the size of the control block */
    size = sizeof(header);
    size += ctrl->ninherits * sizeof(dinherit);
    size += ctrl->progsize;
    size += ctrl->nstrings * (long) sizeof(sstrconst);
    strs = ctrl->strings;
    strsize = 0;
    for (i = ctrl->nstrings; i > 0; --i) {
	strsize += strlen((*strs++)->text);
    }
    size += strsize;
    size += ctrl->nfuncdefs * (long) sizeof(dfuncdef);
    size += ctrl->nvardefs * (long) sizeof(dvardef);
    size += ctrl->nfuncalls * 2L;
    size += ctrl->nsymbols * (long) sizeof(dsymbol);

    /* create sector space */
    ctrl->nsectors = header.nsectors = i = sw_mapsize(size);
    ctrl->sectors = v = ALLOC(sector, i);
    do {
	*v++ = sw_new();
    } while (--i > 0);

    /* create header */
    header.ninherits = ctrl->ninherits;
    header.nvirtuals = ctrl->nvirtuals;
    header.progsize = ctrl->progsize;
    header.nstrings = ctrl->nstrings;
    header.strsize = strsize;
    header.nfuncdefs = ctrl->nfuncdefs;
    header.nvardefs = ctrl->nvardefs;
    header.nfuncalls = ctrl->nfuncalls;
    header.nsymbols = ctrl->nsymbols;
    header.nvariables = ctrl->nvariables;

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
    ctrl->progoffset = size;
    if (header.progsize > 0) {
	sw_writev(ctrl->prog, ctrl->sectors, (long) header.progsize, size);
	size += header.progsize;
    }

    /* save string constants */
    ctrl->stroffset = size;
    if (header.nstrings > 0) {
	sstrconst *ss;
	char *tt;
	register sstrconst *s;
	register char *t;

	ss = ALLOCA(sstrconst, header.nstrings);
	tt = ALLOCA(char, strsize);

	strs = ctrl->strings;
	strsize = size + header.nstrings * (long) sizeof(sstrconst);
	s = ss;
	t = tt;
	for (i = header.nstrings; i > 0; --i) {
	    s->index = strsize;
	    strsize += s->len = strlen((*strs)->text);
	    memcpy(t, (*strs++)->text, s->len);
	    t += (s++)->len;
	}

	sw_writev((char *) ss, ctrl->sectors, size,
		  header.nstrings * (long) sizeof(sstrconst));
	sw_writev(tt, ctrl->sectors,
		  size + header.nstrings * (long) sizeof(sstrconst),
		  strsize - size);
	size = strsize;

	AFREE(tt);
	AFREE(ss);
    }

    /* save function definitions */
    ctrl->funcdoffset = size;
    if (header.nfuncdefs > 0) {
	sw_writev((char *) ctrl->funcdefs, ctrl->sectors,
		  header.nfuncdefs * (long) sizeof(dfuncdef), size);
	size += header.nfuncdefs * (long) sizeof(dfuncdef);
    }

    /* save variable definitions */
    ctrl->vardoffset = size;
    if (header.nvardefs > 0) {
	sw_writev((char *) ctrl->vardefs, ctrl->sectors,
		  header.nvardefs * (long) sizeof(dvardef), size);
	size += header.nvardefs * (long) sizeof(dvardef);
    }

    /* save function call table */
    ctrl->funccoffset = size;
    if (header.nfuncalls > 0) {
	sw_writev((char *) ctrl->funcalls, ctrl->sectors, header.nfuncalls * 2L,
		  size);
	size += header.nfuncalls * 2L;
    }

    /* save symbol table */
    ctrl->symboffset = size;
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
static void d_count(v, n, load, sv)
register value *v;
register unsigned short n;
dataspace *load;
register svalue *sv;
{
    while (n > 0) {
	switch (v->type) {
	case T_INVALID:
	    d_get_value(load, sv, v);
	    continue;	/* try again */

	case T_STRING:
	    if (v->u.string->ref & STR_CONST) {
		register int i;

		/*
		 * The string is a constant.  See if it
		 * is among the string constants of this object and the objects
		 * inherited by it.
		 */
		i = strconst_obj(sdata, v->u.string);
		if (i >= 0) {
		    if (str_put(v->u.string, -1L - i) > 0) {
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
	    if (str_put(v->u.string, nstr) >= nstr) {
		nstr++;
		strsize += v->u.string->len;
	    }
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    if (arr_put(v->u.array) >= narr) {
		register arrref *primary;

		narr++;
		if (v->u.array->hashed != (struct _maphash_ *) NULL) {
		    map_compact(v->u.array);
		}
		arrsize += v->u.array->size;
		primary = v->u.array->primary;
		if (primary != (arrref *) NULL) {
		    /* array in some object */
		    d_count(v->u.array->elts, v->u.array->size, primary->data,
			    &primary->data->selts[primary->index & ~ARR_MOD]);
		} else {
		    /* newly-made array */
		    d_count(v->u.array->elts, v->u.array->size,
			    (dataspace *) NULL, (svalue *) NULL);
		}
	    }
	    break;
	}

	--n;
	v++;
	sv++;
    }
}

static dinherit *inherits;	/* the inherit list of the object being saved */
static sarray *sarrays;		/* the arrays */
static svalue *selts;		/* the array elements */
static sstring *sstrings;	/* the strings */
static char *stext;		/* the string texts */

/*
 * NAME:	data->array()
 * DESCRIPTION:	recursively save the values in an object
 */
static void d_save(v, n, sv)
register value *v;
register unsigned short n;
register svalue *sv;
{
    register long i;

    while (n > 0) {
	switch (v->type) {
	case T_NUMBER:
	    sv->type = T_NUMBER;
	    sv->u.number = v->u.number;
	    break;

	case T_OBJECT:
	    sv->type = T_OBJECT;
	    sv->u.object = v->u.object;
	    break;

	case T_STRING:
	    i = str_put(v->u.string, nstr);
	    sv->type = T_STRING;
	    if (i < 0) {
		/* string constant (in this object) */
		sv->u.string.index = v->u.string->u.strconst -
		  inherits[sv->u.string.inherit = -1 - i].obj->ctrl->strings;
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

	case T_ARRAY:
	case T_MAPPING:
	    i = arr_put(v->u.array);
	    sv->type = v->type;
	    sv->u.array = i;
	    if (i >= narr) {
		svalue *sv;

		/* new array */
		sarrays[i].index = arrsize;
		sarrays[i].size = v->u.array->size;
		sarrays[i].ref = 0;
		sarrays[i].tag = v->u.array->tag;
		sv = selts + arrsize;
		arrsize += v->u.array->size;
		narr++;
		d_save(v->u.array->elts, v->u.array->size, sv);
	    }
	    sarrays[i].ref++;
	    break;
	}
	v++;
	sv++;
	--n;
    }
}

/*
 * NAME:	data->put_values()
 * DESCRIPTION:	save modified values as svalues
 */
static void d_put_values(sv, n, v)
register svalue *sv;
register unsigned short n;
register value *v;
{
    while (n > 0) {
	if (v->modified) {
	    switch (sv->type = v->type) {
	    case T_NUMBER:
		sv->u.number = v->u.number;
		break;

	    case T_OBJECT:
		sv->u.object = v->u.object;
		break;

	    case T_STRING:
		if (v->u.string->ref & STR_CONST) {
		    /* string constant */
		    sv->u.string.inherit = strconst_obj(sdata, v->u.string);
		    sv->u.string.index = v->u.string->u.strconst -
		      inherits[sv->u.string.inherit].obj->ctrl->strings;
		} else {
		    /* string value */
		    sv->u.string.inherit = -1;
		    sv->u.string.index = v->u.string->u.primary -
					 sdata->strings;
		}
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		sv->u.array = v->u.array->primary - sdata->arrays;
		break;
	    }
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * NAME:	data->save_dataspace()
 * DESCRIPTION:	save all values in a dataspace block
 */
static void d_save_dataspace(data)
register dataspace *data;
{
    sdataspace header;
    register uindex n;

    sdata = data;
    inherits = sdata->ctrl->inherits;

    if (data->achange == 0 && data->schange == 0) {
	bool mod;

	/*
	 * No strings/arrays added or deleted. Check individual variables and
	 * array elements.
	 */
	if (data->modified & M_VARIABLE) {
	    /*
	     * variables changed
	     */
	    d_put_values(data->svariables, data->nvariables, data->variables);
	    sw_writev((char *) data->svariables, data->sectors,
		      data->nvariables * (long) sizeof(svalue),
		      data->varoffset +
			data->nvariables * (long) sizeof(sstring));
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
		if (a->array != (array *) NULL && sa->ref != a->array->ref) {
		    sa->ref = a->array->ref;
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
		    d_put_values(&data->selts[a->index], a->array->size,
				 a->array->elts);
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
		if (s->string != (string *) NULL && ss->ref != s->string->ref) {
		    ss->ref = s->string->ref;
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
    } else {
	svalue *svariables;
	register long size;

	/*
	 * count the number and sizes of strings and arrays
	 */
	narr = 0;
	nstr = 0;
	cstr = 0;
	arrsize = 0;
	strsize = 0;

	d_count(data, data->variables, data->varoffset, data->nvariables);

	/* fill in header */
	header.narrays = narr;
	header.eltsize = arrsize;
	header.nstrings = nstr - cstr;
	header.strsize = strsize;
	header.nsectors =
	  sw_mapsize((data->nvariables + arrsize) * (long) sizeof(svalue) +
		     header.narrays * (long) sizeof(sarray) +
		     header.nstrings * (long) sizeof(sstring) +
		     strsize);

	/*
	 * put everything into a savable form
	 */
	svariables = ALLOCA(svalue, data->nvariables);
	if (header.narrays > 0) {
	    sarrays = ALLOCA(sarray, header.narrays);
	    selts = ALLOCA(svalue, arrsize);
	}
	if (header.nstrings > 0) {
	    sstrings = ALLOCA(sstring, header.nstrings);
	    stext = ALLOCA(char, strsize);
	}
	narr = 0;
	nstr = 0;
	arrsize = 0;
	strsize = 0;

	d_save(data->variables, data->nvariables, svariables);

	/* clear hash tables */
	str_clear();
	arr_clear();

	/*
	 * create the sectors to save everything on
	 */
	if (data->sectors == (sector *) NULL) {
	    /* no sectors yet */
	    data->sectors = ALLOC(sector, header.nsectors);
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
	sw_writev((char *) svariables, data->sectors,
		  data->nvariables * (long) sizeof(svalue), size);
	size += data->nvariables * (long) sizeof(svalue);
	AFREE(svariables);

	/* save arrays */
	if (header.narrays > 0) {
	    sw_writev((char *) sarrays, data->sectors,
		      header.narrays * (long) sizeof(sarray), size);
	    size += header.narrays * (long) sizeof(sarray);
	    sw_writev((char *) selts, data->sectors,
		      arrsize * (long) sizeof(svalue), size);
	    size += arrsize * (long) sizeof(svalue);
	    AFREE(selts);
	    AFREE(sarrays);
	}

	/* save strings */
	if (header.nstrings > 0) {
	    sw_writev((char *) sstrings, data->sectors,
		      header.nstrings * (long) sizeof(sstring), size);
	    size += header.nstrings * (long) sizeof(sstring);
	    sw_writev((char *) stext, data->sectors, strsize, size);
	    AFREE(stext);
	    AFREE(sstrings);
	}
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

    /* delete sectors */
    if (ctrl->sectors != (sector *) NULL) {
	FREE(ctrl->sectors);
    }

    /* delete inherits */
    if (ctrl->inherits != (dinherit *) NULL) {
	FREE(ctrl->inherits);
    }

    /* delete strings */
    if (ctrl->strings != (string **) NULL) {
	strs = ctrl->strings;
	for (i = ctrl->nstrings; i > 0; --i) {
	    if (*strs != (string *) NULL) {
		(*strs)->u.strconst = (string **) NULL;
		str_del(*strs);
	    }
	    strs++;
	}
	FREE(ctrl->strings);
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

    if (ctrl->prev != (control *) NULL) {
	ctrl->prev->next = ctrl->next;
    } else {
	clist = ctrl->next;
    }
    if (ctrl->next != (control *) NULL) {
	ctrl->next->prev = ctrl->prev;
    }

    FREE(ctrl);
}

/*
 * NAME:	data->free_dataspace()
 * DESCRIPTION:	remove the dataspace block from memory
 */
static void d_free_dataspace(data)
register dataspace *data;
{
    register uindex i;

    /* delete sectors */
    if (data->sectors != (sector *) NULL) {
	FREE(data->sectors);
    }

    /* free variables */
    if (data->variables != (value *) NULL) {
	register value *v;

	for (i = data->nvariables, v = data->variables; i > 0; --i, v++) {
	    switch (v->type) {
	    case T_STRING:
		str_del(v->u.string);
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		arr_del(v->u.array);
		break;
	    }
	}

	if (data->svariables != (svalue *) NULL) {
	    FREE(data->svariables);
	}
	FREE(data->variables);
    }

    /* free arrays */
    if (data->arrays != (arrref *) NULL) {
	register arrref *a;

	for (i = data->narrays, a = data->arrays; i > 0; --i, a++) {
	    if (a->array != (array *) NULL) {
		a->array->primary = (arrref *) NULL;
		arr_del(a->array);
	    }
	}

	FREE(data->selts);
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

	FREE(data->stext);
	FREE(data->sstrings);
	FREE(data->strings);
    }

    if (data->prev != (dataspace *) NULL) {
	data->prev->next = data->next;
    } else {
	dlist = data->next;
    }
    if (data->next != (dataspace *) NULL) {
	data->next->prev = data->prev;
    }

    FREE(data);
}

/*
 * NAME:	data->clean()
 * DESCRIPTION:	remove active control and dataspace blocks from memory
 */
void d_clean()
{
    /* save and free all dataspace blocks */
    while (dlist != (dataspace *) NULL) {
	if (dlist->modified != 0) {
	    d_save_dataspace(dlist);
	    dlist->obj->dfirst = dlist->sectors[0];
	}
	d_free_dataspace(dlist);
    }

    /* save and free all control blocks */
    while (clist != (control *) NULL) {
	if (clist->sectors == (sector *) NULL) {
	    d_save_control(clist);
	    clist->inherits[clist->nvirtuals - 1].obj->cfirst =
							clist->sectors[0];
	}
	d_free_control(clist);
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

    if (data->sectors != (sector *) NULL) {
	for (i = data->nsectors, s = data->sectors + i; i > 0; --i) {
	    sw_del(*--s);
	}
    }
    d_free_dataspace(data);
}
