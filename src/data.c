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
    char ninherits;		/* # objects in inherit table */
    char niinherits;		/* # immediately inherited objects */
    Uint compiled;		/* time of compilation */
    Uint progsize;		/* size of program code */
    unsigned short nstrings;	/* # strings in string constant table */
    Uint strsize;		/* size of string constant table */
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
    Uint narrays;		/* number of array values */
    Uint eltsize;		/* total size of array elements */
    Uint nstrings;		/* number of strings */
    Uint strsize;		/* total size of strings */
    uindex ncallouts;		/* number of callouts */
    uindex fcallouts;		/* first free callout */
} sdataspace;

typedef struct _svalue_ {
    short type;			/* object, number, string, array */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Int string;		/* string */
	Uint objcnt;		/* object creation count */
	Uint array;		/* array */
    } u;
} svalue;

# define SFLT_GET(s, v)	((v)->oindex = (s)->oindex, \
			 (v)->u.objcnt = (s)->u.objcnt)
# define SFLT_PUT(s, v)	((s)->oindex = (v)->oindex, \
			 (s)->u.objcnt = (v)->u.objcnt)

typedef struct _sarray_ {
    Uint index;			/* index in array value table */
    unsigned short size;	/* size of array */
    Uint ref;			/* refcount */
    Uint tag;			/* unique value for each array */
} sarray;

typedef struct _sstring_ {
    Uint index;			/* index in string text table */
    unsigned short len;		/* length of string */
    Uint ref;			/* refcount */
} sstring;

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

static control *chead, *ctail, *cone;	/* list of control blocks */
static dataspace *dhead, *dtail, *done;	/* list of dataspace blocks */
static uindex nctrl;		/* # control blocks */
static uindex ndata;		/* # dataspace blocks */


/*
 * NAME:	data->new_control()
 * DESCRIPTION:	create a new control block
 */
control *d_new_control()
{
    register control *ctrl;

    ctrl = ALLOC(control, 1);
    if (cone != (control *) NULL) {
	/* insert before first 1 */
	if (chead != cone) {
	    ctrl->prev = cone->prev;
	    ctrl->prev->next = ctrl;
	} else {
	    /* at beginning */
	    chead = ctrl;
	    ctrl->prev = (control *) NULL;
	}
	cone->prev = ctrl;
	ctrl->next = cone;
    } else if (ctail != (control *) NULL) {
	/* append at end of list */
	ctail->next = ctrl;
	ctrl->prev = ctail;
	ctrl->next = (control *) NULL;
	ctail = ctrl;
    } else {
	/* list was empty */
	ctrl->prev = ctrl->next = (control *) NULL;
	chead = ctail = ctrl;
    }
    ctrl->refc = 1;
    ctrl->ndata = 0;
    cone = ctrl;
    nctrl++;

    ctrl->nsectors = 0;		/* nothing on swap device yet */
    ctrl->sectors = (sector *) NULL;
    ctrl->ninherits = 0;
    ctrl->inherits = (dinherit *) NULL;
    ctrl->niinherits = 0;
    ctrl->iinherits = (char *) NULL;
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
    if (done != (dataspace *) NULL) {
	/* insert before first 1 */
	if (dhead != done) {
	    data->prev = done->prev;
	    data->prev->next = data;
	} else {
	    /* at beginning */
	    dhead = data;
	    data->prev = (dataspace *) NULL;
	}
	done->prev = data;
	data->next = done;
    } else if (dtail != (dataspace *) NULL) {
	/* append at end of list */
	dtail->next = data;
	data->prev = dtail;
	data->next = (dataspace *) NULL;
	dtail = data;
    } else {
	/* list was empty */
	data->prev = data->next = (dataspace *) NULL;
	dhead = dtail = data;
    }
    data->refc = 1;
    done = data;
    ndata++;

    data->achange = 0;
    data->schange = 0;
    data->imports = 0;
    data->ilist = (dataspace *) NULL;
    data->modified = 0;

    data->obj = obj;
    data->ctrl = o_control(obj);
    data->ctrl->ndata++;

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
    data->alocal.arr = (array *) NULL;
    data->alocal.data = data;
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
    if (cone != (control *) NULL) {
	/* insert before first 1 */
	if (chead != cone) {
	    ctrl->prev = cone->prev;
	    ctrl->prev->next = ctrl;
	} else {
	    /* at beginning */
	    chead = ctrl;
	    ctrl->prev = (control *) NULL;
	}
	cone->prev = ctrl;
	ctrl->next = cone;
    } else if (ctail != (control *) NULL) {
	/* append at end of list */
	ctail->next = ctrl;
	ctrl->prev = ctail;
	ctrl->next = (control *) NULL;
	ctail = ctrl;
    } else {
	/* list was empty */
	ctrl->prev = ctrl->next = (control *) NULL;
	chead = ctail = ctrl;
    }
    ctrl->refc = 1;
    ctrl->ndata = 0;
    cone = ctrl;
    nctrl++;

    if (obj->flags & O_COMPILED) {
	/* initialize control block of compiled object */
	pc_control(ctrl, obj);
    } else {
	scontrol header;
	register Uint size;

	/* header */
	sw_readv((char *) &header, &obj->cfirst, (Uint) sizeof(scontrol),
		 (Uint) 0);
	ctrl->nsectors = header.nsectors;
	ctrl->sectors = ALLOC(sector, header.nsectors);
	ctrl->sectors[0] = obj->cfirst;
	size = header.nsectors * (Uint) sizeof(sector);
	if (header.nsectors > 1) {
	    sw_readv((char *) ctrl->sectors, ctrl->sectors, size,
		     (Uint) sizeof(scontrol));
	}
	size += sizeof(scontrol);

	/* inherits */
	ctrl->ninherits = UCHAR(header.ninherits); /* there is at least one */
	ctrl->inherits = ALLOC(dinherit, UCHAR(header.ninherits));
	sw_readv((char *) ctrl->inherits, ctrl->sectors,
		 UCHAR(header.ninherits) * (Uint) sizeof(dinherit), size);
	size += UCHAR(header.ninherits) * sizeof(dinherit);
	/* immediate inherits */
	ctrl->iinhoffset = size;
	ctrl->niinherits = UCHAR(header.niinherits);
	ctrl->iinherits = (char *) NULL;
	size += UCHAR(header.niinherits);

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
	size += header.nstrings * (Uint) sizeof(dstrconst) + header.strsize;

	/* function definitions */
	ctrl->funcdoffset = size;
	ctrl->nfuncdefs = UCHAR(header.nfuncdefs);
	ctrl->funcdefs = (dfuncdef *) NULL;
	size += UCHAR(header.nfuncdefs) * (Uint) sizeof(dfuncdef);

	/* variable definitions */
	ctrl->vardoffset = size;
	ctrl->nvardefs = UCHAR(header.nvardefs);
	ctrl->vardefs = (dvardef *) NULL;
	size += UCHAR(header.nvardefs) * (Uint) sizeof(dvardef);

	/* function call table */
	ctrl->funccoffset = size;
	ctrl->nfuncalls = header.nfuncalls;
	ctrl->funcalls = (char *) NULL;
	size += header.nfuncalls * (Uint) 2;

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
    register Uint size;

    data = ALLOC(dataspace, 1);
    if (done != (dataspace *) NULL) {
	/* insert before first 1 */
	if (dhead != done) {
	    data->prev = done->prev;
	    data->prev->next = data;
	} else {
	    /* at beginning */
	    dhead = data;
	    data->prev = (dataspace *) NULL;
	}
	done->prev = data;
	data->next = done;
    } else if (dtail != (dataspace *) NULL) {
	/* append at end of list */
	dtail->next = data;
	data->prev = dtail;
	data->next = (dataspace *) NULL;
	dtail = data;
    } else {
	/* list was empty */
	data->prev = data->next = (dataspace *) NULL;
	dhead = dtail = data;
    }
    data->refc = 1;
    done = data;
    ndata++;

    data->achange = 0;
    data->schange = 0;
    data->imports = 0;
    data->ilist = (dataspace *) NULL;
    data->modified = 0;

    /* header */
    sw_readv((char *) &header, &obj->dfirst, (Uint) sizeof(sdataspace),
	     (Uint) 0);
    data->nsectors = header.nsectors;
    data->sectors = ALLOC(sector, header.nsectors);
    data->sectors[0] = obj->dfirst;
    size = header.nsectors * (Uint) sizeof(sector);
    if (header.nsectors > 1) {
	sw_readv((char *) data->sectors, data->sectors, size,
		 (Uint) sizeof(sdataspace));
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
    size += data->nvariables * (Uint) sizeof(svalue);

    /* arrays */
    data->arroffset = size;
    data->narrays = header.narrays;
    data->eltsize = header.eltsize;
    data->alocal.arr = (array *) NULL;
    data->alocal.data = data;
    data->arrays = (arrref *) NULL;
    data->sarrays = (sarray *) NULL;
    data->selts = (svalue *) NULL;
    size += header.narrays * (Uint) sizeof(sarray) +
	    header.eltsize * sizeof(svalue);

    /* strings */
    data->stroffset = size;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    data->strings = (strref *) NULL;
    data->sstrings = (sstring *) NULL;
    data->stext = (char *) NULL;
    size += header.nstrings * sizeof(sstring) + header.strsize;

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
    if (ctrl->refc == 0) {
	cone = ctrl;
    } else if (cone == ctrl) {
	cone = ctrl->next;
    }
    ctrl->refc++;
    if (ctrl != chead && ctrl->refc >= ctrl->prev->refc) {
	register control *c;

	/* remove from linked list */
	ctrl->prev->next = ctrl->next;
	if (ctrl != ctail) {
	    ctrl->next->prev = ctrl->prev;
	} else {
	    ctail = ctrl->prev;
	}

	/* insert in proper place */
	c = ctrl->prev;
	for (;;) {
	    if (c == chead) {
		/* at beginning */
		ctrl->prev = (control *) NULL;
		ctrl->next = c;
		chead = ctrl;
		c->prev = ctrl;
		break;
	    }
	    if (c->prev->refc > ctrl->refc) {
		/* insert */
		ctrl->prev = c->prev;
		ctrl->next = c;
		c->prev->next = ctrl;
		c->prev = ctrl;
		break;
	    }
	    c = c->prev;
	}
    }
}

/*
 * NAME:	data->ref_dataspace()
 * DESCRIPTION:	reference data block
 */
void d_ref_dataspace(data)
register dataspace *data;
{
    if (data->refc == 0) {
	done = data;
    } else if (done == data) {
	done = data->next;
    }
    data->refc++;
    if (data != dhead && data->refc >= data->prev->refc) {
	register dataspace *d;

	/* remove from linked list */
	data->prev->next = data->next;
	if (data != dtail) {
	    data->next->prev = data->prev;
	} else {
	    dtail = data->prev;
	}

	/* insert in proper place */
	d = data->prev;
	for (;;) {
	    if (d == dhead) {
		/* at beginning */
		data->prev = (dataspace *) NULL;
		data->next = d;
		dhead = data;
		d->prev = data;
		break;
	    }
	    if (d->prev->refc > data->refc) {
		/* insert */
		data->prev = d->prev;
		data->next = d;
		d->prev->next = data;
		d->prev = data;
		break;
	    }
	    d = d->prev;
	}
    }
}


/*
 * NAME:	data->get_iinherits()
 * DESCRIPTION:	get the immediately inherited object indices
 */
char *d_get_iinherits(ctrl)
register control *ctrl;
{
    if (ctrl->iinherits == (char *) NULL && ctrl->niinherits > 0) {
	ctrl->iinherits = ALLOC(char, ctrl->niinherits);
	sw_readv(ctrl->iinherits, ctrl->sectors, (Uint) ctrl->niinherits,
		 (Uint) ctrl->iinhoffset);
    }
    return ctrl->iinherits;
}

/*
 * NAME:	data->get_prog()
 * DESCRIPTION:	get the program
 */
char *d_get_prog(ctrl)
register control *ctrl;
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
string *d_get_strconst(ctrl, inherit, idx)
register control *ctrl;
register int inherit;
unsigned int idx;
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
		     ctrl->nstrings * (Uint) sizeof(dstrconst),
		     ctrl->stroffset);
	    if (ctrl->strsize > 0) {
		/* load strings text */
		ctrl->stext = ALLOC(char, ctrl->strsize);
		sw_readv(ctrl->stext, ctrl->sectors, ctrl->strsize,
			 ctrl->stroffset +
				     ctrl->nstrings * (Uint) sizeof(dstrconst));
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
		 ctrl->nfuncdefs * (Uint) sizeof(dfuncdef), ctrl->funcdoffset);
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
		 ctrl->nvardefs * (Uint) sizeof(dvardef), ctrl->vardoffset);
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
	sw_readv((char *) ctrl->funcalls, ctrl->sectors,
		 ctrl->nfuncalls * (Uint) 2, ctrl->funccoffset);
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
		 ctrl->nsymbols * (Uint) sizeof(dsymbol), ctrl->symboffset);
    }
    return ctrl->symbols;
}

/*
 * NAME:	data->get_string()
 * DESCRIPTION:	get a string from the dataspace
 */
static string *d_get_string(data, idx)
register dataspace *data;
register Uint idx;
{
    if (data->strings == (strref *) NULL) {
	register strref *strs;
	register Uint i;

	/* initialize string pointers */
	strs = data->strings = ALLOC(strref, data->nstrings);
	for (i = data->nstrings; i > 0; --i) {
	    (strs++)->str = (string *) NULL;
	}

	if (data->sstrings == (sstring *) NULL) {
	    /* load strings */
	    data->sstrings = ALLOC(sstring, data->nstrings);
	    sw_readv((char *) data->sstrings, data->sectors,
		     data->nstrings * sizeof(sstring), data->stroffset);
	    if (data->strsize > 0) {
		/* load strings text */
		data->stext = ALLOC(char, data->strsize);
		sw_readv(data->stext, data->sectors, data->strsize,
			 data->stroffset + data->nstrings * sizeof(sstring));
	    }
	}
    }

    if (data->strings[idx].str == (string *) NULL) {
	register string *s;

	s = str_new(data->stext + data->sstrings[idx].index,
		    (long) data->sstrings[idx].len);
	s->u.primary = &data->strings[idx];
	s->u.primary->str = s;
	s->u.primary->data = data;
	s->u.primary->ref = data->sstrings[idx].ref;
	return s;
    }
    return data->strings[idx].str;
}

/*
 * NAME:	data->get_array()
 * DESCRIPTION:	get an array from the dataspace
 */
static array *d_get_array(data, idx)
register dataspace *data;
register Uint idx;
{
    register Uint i;

    if (data->arrays == (arrref *) NULL) {
	register arrref *a;

	/* create array pointers */
	a = data->arrays = ALLOC(arrref, data->narrays);
	for (i = data->narrays; i > 0; --i) {
	    (a++)->arr = (array *) NULL;
	}

	if (data->sarrays == (sarray *) NULL) {
	    /* load arrays */
	    data->sarrays = ALLOC(sarray, data->narrays);
	    sw_readv((char *) data->sarrays, data->sectors,
		     data->narrays * (Uint) sizeof(sarray), data->arroffset);
	}
    }

    if (data->arrays[idx].arr == (array *) NULL) {
	register array *a;

	a = arr_alloc(data->sarrays[idx].size);
	a->tag = data->sarrays[idx].tag;
	a->primary = &data->arrays[idx];
	a->primary->arr = a;
	a->primary->data = data;
	a->primary->index = data->sarrays[idx].index;
	a->primary->ref = data->sarrays[idx].ref;
	return a;
    }
    return data->arrays[idx].arr;
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
	    str_ref(v->u.string = (sv->u.string < 0) ?
		    d_get_string(data, sv->u.string & 0x7fffffff) :
		    d_get_strconst(data->ctrl, sv->u.string >> 23,
				   sv->u.string & 0x007fffff));
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
register unsigned int idx;
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

	    if (data->svariables == (svalue *) NULL) {
		/* load svalues */
		data->svariables = ALLOC(svalue, data->nvariables);
		sw_readv((char *) data->svariables, data->sectors,
			 data->nvariables * (Uint) sizeof(svalue),
			 data->varoffset);
	    }
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
register array *arr;
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
		     data->arroffset + data->narrays * sizeof(sarray));
	}
	d_get_values(data, &data->selts[arr->primary->index], v, arr->size);
    }

    return v;
}

static dataspace *ifirst, *ilast;	/* list of dataspaces with imports */

/*
 * NAME:	data->ref_imports()
 * DESCRIPTION:	check the elements of an array for imports
 */
void d_ref_imports(arr)
array *arr;
{
    register dataspace *data;
    register unsigned short n;
    register value *v;

    data = arr->primary->data;
    for (n = arr->size, v = arr->elts; n > 0; --n, v++) {
	if (T_INDEXED(v->type) && data != v->u.array->primary->data) {
	    /* mark as imported */
	    if (data->imports++ == 0 && data->ilist == (dataspace *) NULL &&
		ilast != data) {
		/* add to imports list */
		if (ifirst == (dataspace *) NULL) {
		    ifirst = data;
		} else {
		    ilast->ilist = data;
		}
		ilast = data;
		data->ilist = (dataspace *) NULL;
	    }
	}
    }
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
	return ((ctrl->ninherits - 1L) << 23) | (strconst - ctrl->strings);
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
	if (arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->arr != (array *) NULL) {
		/* swapped in */
		if (arr->primary->ref++ == 0) {
		    data->achange--;	/* first reference restored */
		    if (arr->primary->index & ARR_MOD) {
			/* add extra reference */
			arr_ref(arr);
		    }
		}
		data->modified |= M_ARRAYREF;
	    } else {
		/* ref new array */
		data->achange++;
	    }
	} else {
	    /* not in this object: ref imported array */
	    if (data->imports++ == 0 && data->ilist == (dataspace *) NULL &&
		ilast != data) {
		/* add to imports list */
		if (ifirst == (dataspace *) NULL) {
		    ifirst = data;
		} else {
		    ilast->ilist = data;
		}
		ilast = data;
		data->ilist = (dataspace *) NULL;
	    }
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
	if (arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->arr != (array *) NULL) {
		/* swapped in */
		if (--(arr->primary->ref) == 0) {
		    data->achange++;	/* last reference removed */
		    if (arr->primary->index & ARR_MOD) {
			/* remove extra reference */
			arr_del(arr);
		    }
		}
		data->modified |= M_ARRAYREF;
	    } else {
		/* deref new array */
		data->achange--;
	    }
	} else {
	    /* not in this object: deref imported array */
	    data->imports--;
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
register value *elt, *val;
{
    register dataspace *data;

    data = arr->primary->data;
    if (arr->primary->arr != (array *) NULL) {
	/*
	 * the array is in the loaded dataspace of some object
	 */
	if ((arr->primary->index & ARR_MOD) == 0) {
	    /*
	     * Swapped-in array changed for the first time.  Add an extra
	     * reference so the changes are not lost.
	     */
	    arr->primary->index |= ARR_MOD;
	    arr_ref(arr);
	    data->modified |= M_ARRAY;
	}
	ref_rhs(data, val);
	del_lhs(data, elt);
    } else {
	if (T_INDEXED(val->type) && data != val->u.array->primary->data) {
	    /* mark as imported */
	    if (data->imports++ == 0 && data->ilist == (dataspace *) NULL &&
		ilast != data) {
		/* add to imports list */
		if (ifirst == (dataspace *) NULL) {
		    ifirst = data;
		} else {
		    ilast->ilist = data;
		}
		ilast = data;
		data->ilist = (dataspace *) NULL;
	    }
	}
	if (T_INDEXED(elt->type) && data != elt->u.array->primary->data) {
	    /* mark as unimported */
	    data->imports--;
	}
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
    if (map->primary->arr != (array *) NULL) {
	map->primary->data->achange++;
    }
}

/*
 * NAME:	data->del_array()
 * DESCRIPTION:	delete an array in a dataspace
 */
void d_del_array(arr)
register array *arr;
{
    if (arr->primary->arr != (array *) NULL) {
	register unsigned short n;
	register value *v;
	register dataspace *data;

	if (arr->primary->ref == 0 && (n=arr->size) != 0 &&
	    (v=arr->elts)[0].type != T_INVALID) {
	    /*
	     * Completely delete a swapped-in array.  Update the local
	     * reference counts for all arrays referenced by it.
	     */
	    data = arr->primary->data;
	    do {
		del_lhs(data, v++);
	    } while (--n != 0);
	}
	arr->primary->arr = (array *) NULL;
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
	     data->ncallouts * (Uint) sizeof(scallout), data->cooffset);

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
 * DESCRIPTION:	add a new callout
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
	    if (data->ncallouts == UINDEX_MAX) {
		error("Too many callouts");
	    }
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
	arr_ref(v[3].u.array = arr_new((long) nargs));
	memcpy(v[3].u.array->elts, sp, nargs * sizeof(value));
	d_ref_imports(v[3].u.array);
	ref_rhs(data, &v[3]);
	break;
    }
    sp += nargs;

    return co - data->callouts + 1;
}

/*
 * NAME:	data->get_call_out()
 * DESCRIPTION:	get a callout
 */
char *d_get_call_out(data, handle, t, nargs)
dataspace *data;
unsigned int handle;
Uint *t;
int *nargs;
{
    static char func[STRINGSZ];
    register dcallout *co;
    register value *v;
    register uindex n;

    if (handle == 0 || handle > data->ncallouts) {
	/* no such callout */
	return (char *) NULL;
    }
    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    co = &data->callouts[handle - 1];
    if (co->val[0].type == T_INVALID) {
	/* invalid callout */
	return (char *) NULL;
    }
    i_grow_stack(*nargs = co->nargs);
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
	v[3].u.array->elts[0].type = T_INVALID;	/* pretend */
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
static int cmp(cv1, cv2)
cvoid *cv1, *cv2;
{
    return ((value *) cv1)->u.array->elts[2].u.number -
	   ((value *) cv2)->u.array->elts[2].u.number;
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
    int max_args;

    if (data->ncallouts == 0) {
	return arr_new(0L);
    }
    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    /* get the number of callouts in this object */
    count = data->ncallouts;
    for (n = data->fcallouts; n != 0; n = data->callouts[n - 1].co_next) {
	--count;
    }

    list = arr_new((long) count);
    elts = list->elts;
    max_args = conf_array_size() - 3;

    for (co = data->callouts; count > 0; co++) {
	if (co->val[0].type != T_INVALID) {
	    size = co->nargs;
	    if (size > max_args) {
		/* unlikely, but possible */
		size = max_args;
	    }
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
	    d_ref_imports(a);

	    /* put in list */
	    elts->type = T_ARRAY;
	    arr_ref((elts++)->u.array = a);
	    --count;
	}
    }

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
    register Uint size;
    register uindex i;
    register sector *v;

    /*
     * Save a control block. This is only done once for each control block.
     */

    /* calculate the size of the control block */
    size = sizeof(scontrol) +
           ctrl->ninherits * sizeof(dinherit) +
	   ctrl->niinherits +
    	   ctrl->progsize +
    	   ctrl->nstrings * (Uint) sizeof(dstrconst) +
    	   ctrl->strsize +
    	   ctrl->nfuncdefs * sizeof(dfuncdef) +
    	   ctrl->nvardefs * sizeof(dvardef) +
    	   ctrl->nfuncalls * (Uint) 2 +
    	   ctrl->nsymbols * (Uint) sizeof(dsymbol);

    /* create sector space */
    ctrl->nsectors = header.nsectors = i = sw_mapsize(size);
    ctrl->sectors = v = ALLOC(sector, i);
    do {
	*v++ = sw_new();
    } while (--i > 0);
    ctrl->inherits[ctrl->ninherits - 1].obj->cfirst = ctrl->sectors[0];

    /* create header */
    header.ninherits = ctrl->ninherits;
    header.niinherits = ctrl->niinherits;
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
    sw_writev((char *) &header, ctrl->sectors, (Uint) sizeof(scontrol),
	      (Uint) 0);
    size = sizeof(scontrol);

    /* save sector map */
    sw_writev((char *) ctrl->sectors, ctrl->sectors,
	      header.nsectors * (Uint) sizeof(sector), size);
    size += header.nsectors * (Uint) sizeof(sector);

    /* save inherits */
    sw_writev((char *) ctrl->inherits, ctrl->sectors,
	      UCHAR(header.ninherits) * (Uint) sizeof(dinherit), size);
    size += UCHAR(header.ninherits) * sizeof(dinherit);
    /* save immediate inherits */
    if (ctrl->niinherits > 0) {
	sw_writev(ctrl->iinherits, ctrl->sectors,
		  (Uint) UCHAR(header.niinherits), size);
	size += UCHAR(header.niinherits);
    }

    /* save program */
    if (header.progsize > 0) {
	sw_writev(ctrl->prog, ctrl->sectors, (Uint) header.progsize, size);
	size += header.progsize;
    }

    /* save string constants */
    if (header.nstrings > 0) {
	dstrconst *ss;
	char *tt;
	register string **strs;
	register dstrconst *s;
	register Uint strsize;
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
		  header.nstrings * (Uint) sizeof(dstrconst), size);
	size += header.nstrings * (Uint) sizeof(dstrconst);
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
		  UCHAR(header.nfuncdefs) * (Uint) sizeof(dfuncdef), size);
	size += UCHAR(header.nfuncdefs) * (Uint) sizeof(dfuncdef);
    }

    /* save variable definitions */
    if (UCHAR(header.nvardefs) > 0) {
	sw_writev((char *) ctrl->vardefs, ctrl->sectors,
		  UCHAR(header.nvardefs) * (Uint) sizeof(dvardef), size);
	size += UCHAR(header.nvardefs) * (Uint) sizeof(dvardef);
    }

    /* save function call table */
    if (header.nfuncalls > 0) {
	sw_writev((char *) ctrl->funcalls, ctrl->sectors,
		  header.nfuncalls * (Uint) 2, size);
	size += header.nfuncalls * (Uint) 2;
    }

    /* save symbol table */
    if (header.nsymbols > 0) {
	sw_writev((char *) ctrl->symbols, ctrl->sectors,
		  header.nsymbols * (Uint) sizeof(dsymbol), size);
    }
}


static dataspace *sdata;	/* the dataspace block currently being saved */
static Uint narr, nstr, cstr;	/* # of arrays, strings, string constants */
static Uint arrsize, strsize;	/* # of array elements, total string size */

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
		sv->u.string = -1 - i;
	    } else {
		/* string value */
		sv->u.string = ((Int) 0x80000000) | i;
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
		    /* string constant */
		    sv->u.string = strconst_obj(sdata, v->u.string);
		} else {
		    /* string value */
		    sv->u.string = ((Int) 0x80000000) |
				   (v->u.string->u.primary - sdata->strings);
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
 * NAME:	data->free_values()
 * DESCRIPTION:	free values in a dataspace block
 */
static void d_free_values(data)
register dataspace *data;
{
    register Uint i;

    /* free variables */
    if (data->variables != (value *) NULL) {
	register value *v;

	for (i = data->nvariables, v = data->variables; i > 0; --i, v++) {
	    i_del_value(v);
	}

	FREE(data->variables);
	data->variables = (value *) NULL;
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
	data->callouts = (dcallout *) NULL;
    }

    /* free arrays */
    if (data->arrays != (arrref *) NULL) {
	if (data->modified & M_ARRAY) {
	    register arrref *a;

	    /*
	     * Modified arrays have gotten an extra reference.  Free them
	     * now.
	     */
	    for (i = data->narrays, a = data->arrays; i > 0; --i, a++) {
		if (a->arr != (array *) NULL && (a->index & ARR_MOD)) {
		    arr_del(a->arr);
		}
	    }
	}

	FREE(data->arrays);
	data->arrays = (arrref *) NULL;
    }

    /* free strings */
    if (data->strings != (strref *) NULL) {
	register strref *s;

	for (i = data->nstrings, s = data->strings; i > 0; --i, s++) {
	    if (s->str != (string *) NULL) {
		s->str->u.primary = (strref *) NULL;
	    }
	}

	FREE(data->strings);
	data->strings = (strref *) NULL;
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
    register Uint n;

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
		      data->nvariables * (Uint) sizeof(svalue),
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
		if (a->arr != (array *) NULL && sa->ref != a->ref) {
		    sa->ref = a->ref;
		    mod = TRUE;
		}
		sa++;
		a++;
	    }
	    if (mod) {
		sw_writev((char *) data->sarrays, data->sectors,
			  data->narrays * sizeof(sarray), data->arroffset);
	    }
	}
	if (data->modified & M_ARRAY) {
	    register arrref *a;

	    /*
	     * array elements changed
	     */
	    a = data->arrays;
	    for (n = data->narrays; n > 0; --n) {
		if (a->arr != (array *) NULL && (a->index & ARR_MOD)) {
		    a->index &= ~ARR_MOD;
		    d_put_values(&data->selts[a->index], a->arr->elts,
				 a->arr->size);
		    sw_writev((char *) &data->selts[a->index], data->sectors,
			      a->arr->size * (Uint) sizeof(svalue),
			      data->arroffset + data->narrays * sizeof(sarray) +
				a->index * sizeof(svalue));
		    arr_del(a->arr);	/* remove extra reference */
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
		if (s->str != (string *) NULL && ss->ref != s->ref) {
		    ss->ref = s->ref;
		    mod = TRUE;
		}
		ss++;
		s++;
	    }
	    if (mod) {
		sw_writev((char *) data->sstrings, data->sectors,
			  data->nstrings * sizeof(sstring),
			  data->stroffset);
	    }
	}
	if (data->modified & M_CALLOUT) {
	    scallout *scallouts;
	    register scallout *sco;
	    register dcallout *co;

	    /* save new (?) fcallouts value */
	    sw_writev((char *) &data->fcallouts, data->sectors,
		      (Uint) sizeof(uindex),
		      (Uint) ((char *) &header.fcallouts - (char *) &header));

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
		      data->ncallouts * (Uint) sizeof(scallout),
		      data->cooffset);
	    AFREE(scallouts);
	}
    } else {
	scallout *scallouts;
	register Uint size;

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
		    /* first callout in the free list */
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
			  header.narrays * sizeof(sarray) +
			  header.nstrings * sizeof(sstring) +
			  header.strsize +
			  header.ncallouts * (Uint) sizeof(scallout));

	/*
	 * put everything into a saveable form
	 */
	if (header.nstrings > 0) {
	    if (header.nstrings <= data->nstrings &&
		data->sstrings != (sstring *) NULL) {
		sstrings = data->sstrings;
	    } else {
		if (data->sstrings != (sstring *) NULL) {
		    FREE(data->sstrings);
		}
		sstrings = data->sstrings = ALLOC(sstring, header.nstrings);
	    }
	    if (header.strsize > 0) {
		if (header.strsize <= data->strsize &&
		    data->stext != (char *) NULL) {
		    stext = data->stext;
		} else {
		    if (data->stext != (char *) NULL) {
			FREE(data->stext);
		    }
		    stext = data->stext = ALLOC(char, header.strsize);
		}
	    }
	}
	if (header.nstrings == 0 && data->sstrings != (sstring *) NULL) {
	    FREE(data->sstrings);
	    data->sstrings = (sstring *) NULL;
	}
	if (header.strsize == 0 && data->stext != (char *) NULL) {
	    FREE(data->stext);
	    data->stext = (char *) NULL;
	}
	if (header.narrays > 0) {
	    if (header.narrays <= data->narrays &&
		data->sarrays != (sarray *) NULL) {
		sarrays = data->sarrays;
	    } else {
		if (data->sarrays != (sarray *) NULL) {
		    FREE(data->sarrays);
		}
		sarrays = data->sarrays = ALLOC(sarray, header.narrays);
	    }
	    if (header.eltsize > 0) {
		if (header.eltsize <= data->eltsize &&
		    data->selts != (svalue *) NULL) {
		    selts = data->selts;
		} else {
		    if (data->selts != (svalue *) NULL) {
			FREE(data->selts);
		    }
		    selts = data->selts = ALLOC(svalue, header.eltsize);
		}
	    }
	}
	if (header.narrays == 0 && data->sarrays != (sarray *) NULL) {
	    FREE(data->sarrays);
	    data->sarrays = (sarray *) NULL;
	}
	if (header.eltsize == 0 && data->selts != (svalue *) NULL) {
	    FREE(data->selts);
	    data->selts = (svalue *) NULL;
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
	sw_writev((char *) &header, data->sectors, size, (Uint) 0);
	sw_writev((char *) data->sectors, data->sectors,
		  header.nsectors * (Uint) sizeof(sector), size);
	size += header.nsectors * (Uint) sizeof(sector);

	/* save variables */
	data->varoffset = size;
	if (data->nvariables > 0) {
	    sw_writev((char *) data->svariables, data->sectors,
		      data->nvariables * (Uint) sizeof(svalue), size);
	    size += data->nvariables * (Uint) sizeof(svalue);
	}

	/* save arrays */
	data->arroffset = size;
	if (header.narrays > 0) {
	    sw_writev((char *) sarrays, data->sectors,
		      header.narrays * sizeof(sarray), size);
	    size += header.narrays * sizeof(sarray);
	    if (header.eltsize > 0) {
		sw_writev((char *) selts, data->sectors,
			  header.eltsize * sizeof(svalue), size);
		size += header.eltsize * sizeof(svalue);
	    }
	}

	/* save strings */
	data->stroffset = size;
	if (header.nstrings > 0) {
	    sw_writev((char *) sstrings, data->sectors,
		      header.nstrings * sizeof(sstring), size);
	    size += header.nstrings * sizeof(sstring);
	    if (header.strsize > 0) {
		sw_writev(stext, data->sectors, header.strsize, size);
		size += header.strsize;
	    }
	}

	/* save callouts */
	data->cooffset = size;
	if (header.ncallouts > 0) {
	    sw_writev((char *) scallouts, data->sectors,
		      header.ncallouts * (Uint) sizeof(scallout), size);
	    AFREE(scallouts);
	}

	d_free_values(data);

	data->narrays = header.narrays;
	data->eltsize = header.eltsize;
	data->nstrings = header.nstrings;
	data->strsize = header.strsize;

	data->achange = 0;
	data->schange = 0;
    }

    data->modified = 0;
}

static array **itab;	/* imported array replacement table */
static Uint itabsz;	/* size of table */

/*
 * NAME:	data->import()
 * DESCRIPTION:	copy imported arrays to current dataspace
 */
static void d_import(data, val, n)
register dataspace *data;
register value *val;
register unsigned short n;
{
    while (n > 0) {
	if (T_INDEXED(val->type)) {
	    if (val->u.array->primary->data != data) {
		register Uint i;
		register array *a;

		/*
		 * imported array
		 */
		i = arr_put(val->u.array);
		if (i >= narr) {
		    /*
		     * make new array
		     */
		    if (val->u.array->hashed != (struct _maphash_ *) NULL) {
			map_compact(val->u.array);
		    }
		    a = arr_alloc(val->u.array->size);
		    a->tag = val->u.array->tag;
		    a->primary = &data->alocal;

		    /*
		     * store in itab
		     */
		    if (i >= itabsz) {
			array **tmp;
			register Uint n;

			/*
			 * increase size of itab
			 */
			for (n = itabsz; n <= i; n += n) ;
			tmp = ALLOC(array*, n);
			memcpy(tmp, itab, itabsz * sizeof(array*));
			FREE(itab);
			itab = tmp;
			itabsz = n;
		    }
		    arr_put(itab[i] = a);
		    narr += 2;	/* 1 old, 1 new */

		    if (val->u.array->size > 0) {
			register value *v, *w;

			/*
			 * copy elements
			 */
			v = a->elts;
			w = d_get_elts(val->u.array);
			for (i = val->u.array->size; i > 0; --i) {
			    i_ref_value(w);
			    *v++ = *w++;
			}
		    }

		    /*
		     * replace
		     */
		    arr_del(val->u.array);
		    arr_ref(val->u.array = a);

		    if (a->size > 0) {
			/*
			 * import elements too
			 */
			d_import(data, a->elts, a->size);
		    }
		} else {
		    /*
		     * array was previously replaced
		     */
		    a = itab[i];
		    arr_del(val->u.array);
		    arr_ref(val->u.array = a);
		}
	    } else if (arr_put(val->u.array) >= narr) {
		/*
		 * not previously encountered mapping or array
		 */
		narr++;
		if (val->u.array->hashed != (struct _maphash_ *) NULL) {
		    map_compact(val->u.array);
		    d_import(data, val->u.array->elts, val->u.array->size);
		} else if (val->u.array->size != 0 &&
			   val->u.array->elts[0].type != T_INVALID) {
		    d_import(data, val->u.array->elts, val->u.array->size);
		}
	    }
	}
	val++;
	--n;
    }
}

/*
 * NAME:	data->export()
 * DESCRIPTION:	handle exporting of arrays shared by more than one object
 */
void d_export()
{
    register dataspace *data, *next;
    register Uint n;

    if (ifirst != (dataspace *) NULL) {
	itab = ALLOC(array*, itabsz = 16);

	for (data = ifirst; data != (dataspace *) NULL; data = data->ilist) {
	    if (data->imports != 0) {
		narr = 0;
		if (data->variables != (value *) NULL) {
		    d_import(data, data->variables, data->nvariables);
		}
		if (data->arrays != (arrref *) NULL) {
		    register arrref *a;

		    for (n = data->narrays, a = data->arrays; n > 0; --n, a++) {
			if (a->arr != (array *) NULL) {
			    if (a->arr->hashed != (struct _maphash_ *) NULL) {
				/* mapping */
				map_compact(a->arr);
				d_import(data, a->arr->elts, a->arr->size);
			    } else if (a->arr->size != 0 &&
				       a->arr->elts[0].type != T_INVALID) {
				d_import(data, a->arr->elts, a->arr->size);
			    }
			}
		    }
		}
		if (data->callouts != (dcallout *) NULL) {
		    register dcallout *co;

		    co = data->callouts;
		    for (n = data->ncallouts; n > 0; --n) {
			if (co->val[0].type != T_INVALID) {
			    d_import(data, co->val,
				     (co->nargs > 3) ? 4 : co->nargs + 1);
			}
			co++;
		    }
		}
		arr_clear();	/* clear hash table */
	    }
	}

	for (data = ifirst; data != (dataspace *) NULL; data = next) {
	    data->imports = 0;
	    next = data->ilist;
	    data->ilist = (dataspace *) NULL;
	}
	ifirst = ilast = (dataspace *) NULL;

	FREE(itab);
    }
}

/*
 * NAME:	data->free_control()
 * DESCRIPTION:	remove the control block from memory
 */
static void d_free_control(ctrl)
register control *ctrl;
{
    register string **strs;
    object *obj;

    obj = ctrl->inherits[ctrl->ninherits - 1].obj;
    if (obj != (object *) NULL) {
	obj->ctrl = (control *) NULL;
    }

    /* delete strings */
    if (ctrl->strings != (string **) NULL) {
	register unsigned short i;

	strs = ctrl->strings;
	for (i = ctrl->nstrings; i > 0; --i) {
	    if (*strs != (string *) NULL) {
		(*strs)->ref &= ~STR_CONST;
		(*strs)->u.strconst = (string **) NULL;
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
	if (ctrl->iinherits != (char *) NULL) {
	    FREE(ctrl->iinherits);
	}

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
    if (ctrl == cone) {
	cone = ctrl->next;
    }
    --nctrl;

    FREE(ctrl);
}

/*
 * NAME:	data->free_dataspace()
 * DESCRIPTION:	remove the dataspace block from memory
 */
static void d_free_dataspace(data)
register dataspace *data;
{
    /* free values */
    d_free_values(data);

    /* delete sectors */
    if (data->sectors != (sector *) NULL) {
	FREE(data->sectors);
    }

    /* free sarrays */
    if (data->sarrays != (sarray *) NULL) {
	if (data->selts != (svalue *) NULL) {
	    FREE(data->selts);
	}
	FREE(data->sarrays);
    }

    /* free sstrings */
    if (data->sstrings != (sstring *) NULL) {
	if (data->stext != (char *) NULL) {
	    FREE(data->stext);
	}
	FREE(data->sstrings);
    }

    /* free svariables */
    if (data->svariables != (svalue *) NULL) {
	FREE(data->svariables);
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
    if (data == done) {
	done = data->next;
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
    register dataspace *data;
    register control *ctrl, *prev;

    count = 0;

    /* swap out dataspace blocks */
    for (n = ndata / frag; n > 0; --n) {
	if (dtail->modified != 0) {
	    d_save_dataspace(dtail);
	    count++;
	}
	d_free_dataspace(dtail);
    }
    /* divide ref counts for leftover datablocks by 2 */
    done = (dataspace *) NULL;
    for (data = dtail; data != dhead; data = data->prev) {
	data->refc >>= 1;
	if (data->refc <= 1) {
	    done = data;
	}
    }

    /* swap out control blocks */
    ctrl = ctail;
    for (n = nctrl / frag; n > 0; --n) {
	prev = ctrl->prev;
	if (ctrl->ndata == 0) {
	    if (ctrl->sectors == (sector *) NULL &&
		!(ctrl->inherits[ctrl->ninherits - 1].obj->flags & O_COMPILED))
	    {
		d_save_control(ctrl);
	    }
	    d_free_control(ctrl);
	}
	ctrl = prev;
    }
    /* divide ref counts for leftover control blocks by 2 */
    cone = (control *) NULL;
    for (ctrl = ctail; ctrl != chead; ctrl = ctrl->prev) {
	ctrl->refc >>= 1;
	if (ctrl->refc <= 1) {
	    cone = ctrl;
	}
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
    register dataspace *data;

    /* save control blocks */
    for (ctrl = ctail; ctrl != (control *) NULL; ctrl = ctrl->prev) {
	if (ctrl->sectors == (sector *) NULL &&
	    !(ctrl->inherits[ctrl->ninherits - 1].obj->flags & O_COMPILED)) {
	    d_save_control(ctrl);
	}
    }

    /* save dataspace blocks */
    for (data = dtail; data != (dataspace *) NULL; data = data->prev) {
	if (data->modified != 0) {
	    d_save_dataspace(data);
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
	      ctrl->ninherits * (Uint) sizeof(dinherit),
	      sizeof(scontrol) + ctrl->nsectors * (Uint) sizeof(sector));
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
