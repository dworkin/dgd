# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "sdata.h"
# include "interpret.h"
# include "call_out.h"
# include "parse.h"
# include "csupport.h"


# define COP_ADD	0	/* add callout patch */
# define COP_REMOVE	1	/* remove callout patch */
# define COP_REPLACE	2	/* replace callout patch */

typedef struct _copatch_ {
    short type;			/* add, remove, replace */
    uindex handle;		/* callout handle */
    dataplane *plane;		/* dataplane */
    Uint time;			/* start time */
    unsigned short mtime;	/* start time millisec component */
    struct _cbuf_ *queue;	/* callout queue */
    struct _copatch_ *next;	/* next in linked list */
    dcallout aco;		/* added callout */
    dcallout rco;		/* removed callout */
} copatch;

# define COPCHUNKSZ	32

typedef struct _copchunk_ {
    struct _copchunk_ *next;	/* next in linked list */
    copatch cop[COPCHUNKSZ];	/* callout patches */
} copchunk;

typedef struct _coptable_ {
    copchunk *chunk;			/* callout patch chunk */
    unsigned short chunksz;		/* size of callout patch chunk */
    copatch *flist;			/* free list of callout patches */
    copatch *cop[COPATCHHTABSZ];	/* hash table of callout patches */
} coptable;

typedef struct {
    array **itab;			/* imported array replacement table */
    Uint itabsz;			/* size of table */
    struct _arrmerge_ *merge;		/* array merge table */
    Uint narr;				/* # of arrays */
} arrimport;

typedef struct _dataenv_ {
    control *chead, *ctail;		/* list of control blocks */
    dataspace *dhead, *dtail;		/* list of dataspace blocks */
    dataspace *gcdata;			/* next dataspace to garbage collect */
    sector nctrl;			/* # control blocks */
    sector ndata;			/* # dataspace blocks */
    dataplane *plist;			/* list of dataplanes */
    dataspace *ifirst;			/* list of dataspaces with imports */
    uindex ncallout;			/* # callouts added */
} dataenv;


static bool nilisnot0;			/* nil != int 0 */


/*
 * NAME:	data->init()
 * DESCRIPTION:	initialize data handling
 */
void d_init(flag)
bool flag;
{
    nilisnot0 = flag;
}

/*
 * NAME:	data->new_env()
 * DESCRIPTION:	create a new dataspace environment
 */
dataenv *d_new_env()
{
    register dataenv *de;

    de = SALLOC(dataenv, 1);
    de->chead = de->ctail = (control *) NULL;
    de->dhead = de->dtail = (dataspace *) NULL;
    de->gcdata = (dataspace *) NULL;
    de->nctrl = de->ndata = 0;
    de->plist = (dataplane *) NULL;
    de->ifirst = (dataspace *) NULL;
    de->ncallout = 0;

    return de;
}

/*
 * NAME:	data->new_control()
 * DESCRIPTION:	create a new control block
 */
control *d_new_control(env)
lpcenv *env;
{
    register control *ctrl;
    register dataenv *de;

    ctrl = IALLOC(env, control, 1);
    de = env->de;
    if (de->chead != (control *) NULL) {
	/* insert at beginning of list */
	de->chead->prev = ctrl;
	ctrl->prev = (control *) NULL;
	ctrl->next = de->chead;
	de->chead = ctrl;
    } else {
	/* list was empty */
	ctrl->prev = ctrl->next = (control *) NULL;
	de->chead = de->ctail = ctrl;
    }
    ctrl->ndata = 0;
    de->nctrl++;

    ctrl->flags = 0;

    ctrl->sctrl = (struct _scontrol_ *) NULL;
    ctrl->oindex = UINDEX_MAX;
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
    ctrl->nifdefs = 0;
    ctrl->nvinit = 0;
    ctrl->vmapsize = 0;
    ctrl->vmap = (unsigned short *) NULL;

    return ctrl;
}

/*
 * NAME:	data->alloc_dataspace()
 * DESCRIPTION:	allocate a new dataspace block
 */
dataspace *d_alloc_dataspace(env, obj)
lpcenv *env;
object *obj;
{
    register dataspace *data;
    register dataenv *de;

    data = IALLOC(env, dataspace, 1);
    de = env->de;
    if (de->dhead != (dataspace *) NULL) {
	/* insert at beginning of list */
	de->dhead->prev = data;
	data->prev = (dataspace *) NULL;
	data->next = de->dhead;
	de->dhead = data;
	data->gcprev = de->gcdata->gcprev;
	data->gcnext = de->gcdata;
	data->gcprev->gcnext = data;
	de->gcdata->gcprev = data;
    } else {
	/* list was empty */
	data->prev = data->next = (dataspace *) NULL;
	de->dhead = de->dtail = data;
	de->gcdata = data;
	data->gcprev = data->gcnext = data;
    }
    de->ndata++;

    data->env = env;
    data->iprev = (dataspace *) NULL;
    data->inext = (dataspace *) NULL;

    data->oindex = obj->index;
    data->ctrl = (control *) NULL;
    data->sdata = (struct _sdataspace_ *) NULL;

    /* variables */
    data->nvariables = 0;
    data->variables = (value *) NULL;

    /* arrays */
    data->narrays = 0;
    data->sarrays = (struct _sarray_ *) NULL;
    data->selts = (struct _svalue_ *) NULL;
    data->alist.prev = data->alist.next = &data->alist;

    /* strings */
    data->nstrings = 0;
    data->strsize = 0;
    data->sstrings = (struct _sstring_ *) NULL;
    data->stext = (char *) NULL;

    /* callouts */
    data->ncallouts = 0;
    data->fcallouts = 0;
    data->callouts = (dcallout *) NULL;

    /* value plane */
    data->base.level = 0;
    data->base.flags = 0;
    data->base.schange = 0;
    data->base.achange = 0;
    data->base.imports = 0;
    data->base.alocal.arr = (array *) NULL;
    data->base.alocal.plane = &data->base;
    data->base.alocal.data = data;
    data->base.alocal.state = AR_CHANGED;
    data->base.arrays = (arrref *) NULL;
    data->base.strings = (strref *) NULL;
    data->base.coptab = (struct _coptable_ *) NULL;
    data->base.prev = (dataplane *) NULL;
    data->base.plist = (dataplane *) NULL;
    data->plane = &data->base;

    /* parse_string data */
    data->parser = (struct _parser_ *) NULL;

    return data;
}

/*
 * NAME:	data->new_dataspace()
 * DESCRIPTION:	create a new dataspace block
 */
dataspace *d_new_dataspace(env, obj)
lpcenv *env;
object *obj;
{
    register dataspace *data;

    data = d_alloc_dataspace(env, obj);
    data->base.flags = MOD_VARIABLE;
    data->ctrl = o_control(env, obj);
    data->ctrl->ndata++;
    data->nvariables = data->ctrl->nvariables + 1;

    return data;
}

/*
 * NAME:	data->ref_control()
 * DESCRIPTION:	reference control block
 */
void d_ref_control(env, ctrl)
lpcenv *env;
register control *ctrl;
{
    register dataenv *de;

    de = env->de;
    if (ctrl != de->chead) {
	/* move to head of list */
	ctrl->prev->next = ctrl->next;
	if (ctrl->next != (control *) NULL) {
	    ctrl->next->prev = ctrl->prev;
	} else {
	    de->ctail = ctrl->prev;
	}
	ctrl->prev = (control *) NULL;
	ctrl->next = de->chead;
	de->chead->prev = ctrl;
	de->chead = ctrl;
    }
}

/*
 * NAME:	data->ref_dataspace()
 * DESCRIPTION:	reference data block
 */
void d_ref_dataspace(data)
register dataspace *data;
{
    register dataenv *de;

    de = data->env->de;
    if (data != de->dhead) {
	/* move to head of list */
	data->prev->next = data->next;
	if (data->next != (dataspace *) NULL) {
	    data->next->prev = data->prev;
	} else {
	    de->dtail = data->prev;
	}
	data->prev = (dataspace *) NULL;
	data->next = de->dhead;
	de->dhead->prev = data;
	de->dhead = data;
    }
}

/*
 * NAME:	data->get_prog()
 * DESCRIPTION:	get the program
 */
char *d_get_prog(ctrl)
register control *ctrl;
{
    if (ctrl->prog == (char *) NULL && ctrl->progsize != 0) {
	ctrl->prog = sd_get_prog(ctrl->sctrl, &ctrl->progsize);
    }
    return ctrl->prog;
}

/*
 * NAME:	data->get_strconst()
 * DESCRIPTION:	get a string constant
 */
string *d_get_strconst(env, ctrl, inherit, idx)
register lpcenv *env;
register control *ctrl;
register int inherit;
unsigned int idx;
{
    if (UCHAR(inherit) < ctrl->ninherits - 1) {
	/* get the proper control block */
	ctrl = o_control(env, OBJR(env, ctrl->inherits[UCHAR(inherit)].oindex));
    }

    if (ctrl->strings == (string **) NULL) {
	/* make string pointer block */
	ctrl->strings = IALLOC(env, string*, ctrl->nstrings);
	memset(ctrl->strings, '\0', ctrl->nstrings * sizeof(string *));

	if (ctrl->sstrings == (dstrconst *) NULL) {
	    /* load strings */
	    ctrl->sstrings = sd_get_strconsts(ctrl->sctrl);
	    if (ctrl->strsize > 0 && ctrl->stext == (char *) NULL) {
		ctrl->stext = sd_get_ctext(ctrl->sctrl, &ctrl->strsize);
	    }
	}
    }

    if (ctrl->strings[idx] == (string *) NULL) {
	register string *str;

	str = str_alloc(env, ctrl->stext + ctrl->sstrings[idx].index,
			(long) ctrl->sstrings[idx].len);
	str_ref(ctrl->strings[idx] = str);
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
    if (ctrl->funcdefs == (dfuncdef *) NULL && ctrl->nfuncdefs != 0) {
	ctrl->funcdefs = sd_get_funcdefs(ctrl->sctrl);
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
    if (ctrl->vardefs == (dvardef *) NULL && ctrl->nvardefs != 0) {
	ctrl->vardefs = sd_get_vardefs(ctrl->sctrl);
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
    if (ctrl->funcalls == (char *) NULL && ctrl->nfuncalls != 0) {
	ctrl->funcalls = sd_get_funcalls(ctrl->sctrl);
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
	ctrl->symbols = sd_get_symbols(ctrl->sctrl);
    }
    return ctrl->symbols;
}

/*
 * NAME:	data->get_progsize()
 * DESCRIPTION:	get the size of a control block
 */
Uint d_get_progsize(ctrl)
register control *ctrl;
{
    if (ctrl->sctrl != (struct _scontrol_ *) NULL) {
	if (ctrl->prog == (char *) NULL && ctrl->progsize != 0) {
	    /* decompress program */
	    ctrl->prog = sd_get_prog(ctrl->sctrl, &ctrl->progsize);
	}
	if (ctrl->stext == (char *) NULL && ctrl->strsize != 0) {
	    /* decompress strings */
	    ctrl->stext = sd_get_ctext(ctrl->sctrl, &ctrl->strsize);
	}
    }

    return ctrl->ninherits * sizeof(dinherit) +
	   ctrl->progsize +
	   ctrl->nstrings * (Uint) sizeof(dstrconst) +
	   ctrl->strsize +
	   ctrl->nfuncdefs * sizeof(dfuncdef) +
	   ctrl->nvardefs * sizeof(dvardef) +
	   ctrl->nfuncalls * (Uint) 2 +
	   ctrl->nsymbols * (Uint) sizeof(dsymbol);
}


/*
 * NAME:	data->get_string()
 * DESCRIPTION:	get a string from the dataspace
 */
static string *d_get_string(data, idx)
register dataspace *data;
register Uint idx;
{
    if (data->plane->strings == (strref *) NULL ||
	data->plane->strings[idx].str == (string *) NULL) {
	register string *str;
	register strref *s;
	register dataplane *p;
	register Uint i;

	if (data->sstrings == (sstring *) NULL) {
	    /* load strings */
	    data->sstrings = sd_get_sstrings(data->sdata);
	    if (data->strsize > 0) {
		data->stext = sd_get_dtext(data->sdata, &data->strsize);
	    }
	}

	str = str_alloc(data->env, data->stext + data->sstrings[idx].index,
			(long) data->sstrings[idx].len);
	str->ref = 0;
	p = data->plane;

	do {
	    if (p->strings == (strref *) NULL) {
		/* initialize string pointers */
		s = p->strings = IALLOC(data->env, strref, data->nstrings);
		for (i = data->nstrings; i > 0; --i) {
		    (s++)->str = (string *) NULL;
		}
	    }
	    s = &p->strings[idx];
	    str_ref(s->str = str);
	    s->data = data;
	    s->ref = data->sstrings[idx].ref;
	    p = p->prev;
	} while (p != (dataplane *) NULL);

	str->primary = &data->plane->strings[idx];
	return str;
    }
    return data->plane->strings[idx].str;
}

/*
 * NAME:	data->get_array()
 * DESCRIPTION:	get an array from the dataspace
 */
static array *d_get_array(data, idx)
register dataspace *data;
register Uint idx;
{
    if (data->plane->arrays == (arrref *) NULL ||
	data->plane->arrays[idx].arr == (array *) NULL) {
	register array *arr;
	register arrref *a;
	register dataplane *p;
	register Uint i;

	if (data->sarrays == (sarray *) NULL) {
	    /* load arrays */
	    data->sarrays = sd_get_sarrays(data->sdata);
	}

	arr = arr_alloc(data->env, data->sarrays[idx].size);
	arr->ref = 0;
	arr->tag = data->sarrays[idx].tag;
	p = data->plane;

	do {
	    if (p->arrays == (arrref *) NULL) {
		/* create array pointers */
		a = p->arrays = IALLOC(data->env, arrref, data->narrays);
		for (i = data->narrays; i > 0; --i) {
		    (a++)->arr = (array *) NULL;
		}
	    }
	    a = &p->arrays[idx];
	    arr_ref(a->arr = arr);
	    a->plane = &data->base;
	    a->data = data;
	    a->state = AR_UNCHANGED;
	    a->ref = data->sarrays[idx].ref;
	    p = p->prev;
	} while (p != (dataplane *) NULL);

	arr->primary = &data->plane->arrays[idx];
	arr->prev = &data->alist;
	arr->next = data->alist.next;
	arr->next->prev = arr;
	data->alist.next = arr;
	return arr;
    }
    return data->plane->arrays[idx].arr;
}

/*
 * NAME:	data->get_values()
 * DESCRIPTION:	get values from the dataspace
 */
void d_get_values(data, sv, v, n)
register dataspace *data;       
register svalue *sv;
register value *v;
register unsigned int n;
{   
    while (n > 0) {
	v->modified = FALSE;    
	switch (v->type = sv->type) {
	case T_NIL:
	    v->u.number = 0;
	    break; 
    
	case T_INT:
	    v->u.number = sv->u.number;
	    break;

	case T_STRING:
	    str_ref(v->u.string = d_get_string(data, sv->u.string));
	    break;

	case T_FLOAT:
	case T_OBJECT:
	    v->oindex = sv->oindex;
	    v->u.objcnt = sv->u.objcnt;
	    break;

	case T_ARRAY:
	case T_MAPPING:
	case T_LWOBJECT:
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
void d_new_variables(env, ctrl, variables)
register lpcenv *env;
register control *ctrl;
register value *variables;
{
    register unsigned short nifdefs, nvars, nvinit;
    register dvardef *var;
    register dinherit *inh;

    /*
     * first, initialize all variables to nil
     */
    for (nvars = ctrl->nvariables, variables += nvars; nvars > 0; --nvars) {
	*--variables = nil_value;
    }

    if (ctrl->nvinit != 0) {
	/*
	 * explicitly initialize some variables
	 */
	nvars = 0;
	for (nvinit = ctrl->nvinit, inh = ctrl->inherits; nvinit > 0; inh++) {
	    if (inh->varoffset == nvars) {
		ctrl = o_control(env, OBJR(env, inh->oindex));
		if (ctrl->nifdefs != 0) {
		    nvinit -= ctrl->nifdefs;
		    for (nifdefs = ctrl->nifdefs, var = d_get_vardefs(ctrl);
			 nifdefs > 0; var++) {
			if (var->type == T_INT && nilisnot0) {
			    variables[nvars] = zero_int;
			    --nifdefs;
			} else if (var->type == T_FLOAT) {
			    variables[nvars] = zero_float;
			    --nifdefs;
			}
			nvars++;
		    }
		}
		nvars = inh->varoffset + ctrl->nvardefs;
	    }
	}
    }
}

/*
 * NAME:	data->get_variables()
 * DESCRIPTION:	get variables from the dataspace
 */
value *d_get_variables(data)
register dataspace *data;
{
    if (data->variables == (value *) NULL) {
	/* create room for variables */
	data->variables = IALLOC(data->env, value, data->nvariables);
	if (data->sdata == (struct _sdataspace_ *) NULL) {
	    /*
	     * new datablock
	     */
	    d_new_variables(data->env, data->ctrl, data->variables);
	    data->variables[data->nvariables - 1] = nil_value;	/* extra var */
	} else {
	    /*
	     * variables must be loaded from the swap
	     */
	    d_get_values(data, sd_get_svariables(data->sdata), data->variables,
			 data->nvariables);
	}
    }
    return data->variables;
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
    if (v == (value *) NULL && arr->size != 0) {
	register dataspace *data;
	Uint idx;

	data = arr->primary->data;
	if (data->selts == (svalue *) NULL) {
	    data->selts = sd_get_selts(data->sdata);
	}
	v = arr->elts = IALLOC(data->env, value, arr->size);
	idx = data->sarrays[arr->primary - data->plane->arrays].index;
	d_get_values(data, &data->selts[idx], v, arr->size);
    }

    return v;
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
	if (str->primary != (strref *) NULL && str->primary->data == data) {
	    /* in this object */
	    str->primary->ref++;
	    data->plane->flags |= MOD_STRINGREF;
	} else {
	    /* not in this object: ref imported string */
	    data->plane->schange++;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr = rhs->u.array;
	if (arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->arr != (array *) NULL) {
		/* swapped in */
		arr->primary->ref++;
		data->plane->flags |= MOD_ARRAYREF;
	    } else {
		/* ref new array */
		data->plane->achange++;
	    }
	} else {
	    register dataenv *de;

	    /* not in this object: ref imported array */
	    de = data->env->de;
	    if (data->plane->imports++ == 0 && de->ifirst != data &&
		data->iprev == (dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (dataspace *) NULL;
		data->inext = de->ifirst;
		if (de->ifirst != (dataspace *) NULL) {
		    de->ifirst->iprev = data;
		}
		de->ifirst = data;
	    }
	    data->plane->achange++;
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
	if (str->primary != (strref *) NULL && str->primary->data == data) {
	    /* in this object */
	    if (--(str->primary->ref) == 0) {
		str->primary->str = (string *) NULL;
		str->primary = (strref *) NULL;
		str_del(data->env, str);
		data->plane->schange++;	/* last reference removed */
	    }
	    data->plane->flags |= MOD_STRINGREF;
	} else {
	    /* not in this object: deref imported string */
	    data->plane->schange--;
	}
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr = lhs->u.array;
	if (arr->primary->data == data) {
	    /* in this object */
	    if (arr->primary->arr != (array *) NULL) {
		/* swapped in */
		data->plane->flags |= MOD_ARRAYREF;
		if ((--(arr->primary->ref) & ~ARR_MOD) == 0) {
		    register unsigned short n;

		    /* last reference removed */
		    if (arr->hashed != (struct _maphash_ *) NULL) {
			map_compact(arr);
		    } else {
			d_get_elts(arr);
		    }
		    arr->primary->arr = (array *) NULL;
		    arr->primary = &arr->primary->plane->alocal;

		    for (n = arr->size, lhs = arr->elts; n != 0; --n, lhs++) {
			del_lhs(data, lhs);
		    }

		    arr_del(data->env, arr);
		    data->plane->achange++;
		}
	    } else {
		/* deref new array */
		data->plane->achange--;
	    }
	} else {
	    /* not in this object: deref imported array */
	    data->plane->imports--;
	    data->plane->achange--;
	}
	break;
    }
}


/*
 * NAME:	data->alloc_call_out()
 * DESCRIPTION:	allocate a new callout
 */
static uindex d_alloc_call_out(data, handle, time, nargs, v)
register dataspace *data;
register uindex handle;
Uint time;
int nargs;
register value *v;
{
    register dcallout *co;

    if (data->ncallouts == 0) {
	/*
	 * the first in this object
	 */
	co = data->callouts = IALLOC(data->env, dcallout, 1);
	data->ncallouts = handle = 1;
	data->plane->flags |= MOD_NEWCALLOUT;
    } else {
	if (data->callouts == (dcallout *) NULL) {
	    sd_load_callouts(data);
	}
	if (handle != 0) {
	    /*
	     * get a specific callout from the free list
	     */
	    co = &data->callouts[handle - 1];
	    if (handle == data->fcallouts) {
		data->fcallouts = co->co_next;
	    } else {
		data->callouts[co->co_prev - 1].co_next = co->co_next;
		if (co->co_next != 0) {
		    data->callouts[co->co_next - 1].co_prev = co->co_prev;
		}
	    }
	} else {
	    handle = data->fcallouts;
	    if (handle != 0) {
		/*
		 * from free list
		 */
		co = &data->callouts[handle - 1];
		if (co->co_next == 0 || co->co_next > handle) {
		    /* take 1st free callout */
		    data->fcallouts = co->co_next;
		} else {
		    /* take 2nd free callout */
		    co = &data->callouts[co->co_next - 1];
		    data->callouts[handle - 1].co_next = co->co_next;
		    if (co->co_next != 0) {
			data->callouts[co->co_next - 1].co_prev = handle;
		    }
		    handle = co - data->callouts + 1;
		}
		data->plane->flags |= MOD_CALLOUT;
	    } else {
		/*
		 * add new callout
		 */
		handle = data->ncallouts;
		co = data->callouts = IREALLOC(data->env, data->callouts,
					       dcallout, handle, handle + 1);
		co += handle;
		data->ncallouts = ++handle;
		data->plane->flags |= MOD_NEWCALLOUT;
	    }
	}
    }

    co->time = time;
    co->nargs = nargs;
    memcpy(co->val, v, sizeof(co->val));
    switch (nargs) {
    default:
	ref_rhs(data, &v[3]);
    case 2:
	ref_rhs(data, &v[2]);
    case 1:
	ref_rhs(data, &v[1]);
    case 0:
	ref_rhs(data, &v[0]);
	break;
    }

    return handle;
}

/*
 * NAME:	data->free_call_out()
 * DESCRIPTION:	freeove a callout
 */
static void d_free_call_out(data, handle)
register dataspace *data;
unsigned int handle;
{
    register dcallout *co;
    register value *v;
    uindex n;

    co = &data->callouts[handle - 1];
    v = co->val;
    switch (co->nargs) {
    default:
	del_lhs(data, &v[3]);
	i_del_value(data->env, &v[3]);
    case 2:
	del_lhs(data, &v[2]);
	i_del_value(data->env, &v[2]);
    case 1:
	del_lhs(data, &v[1]);
	i_del_value(data->env, &v[1]);
    case 0:
	del_lhs(data, &v[0]);
	str_del(data->env, v[0].u.string);
	break;
    }
    v[0] = nil_value;

    n = data->fcallouts;
    if (n != 0) {
	data->callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    data->fcallouts = handle;

    data->plane->flags |= MOD_CALLOUT;
}


/*
 * NAME:	copatch->init()
 * DESCRIPTION:	initialize copatch table
 */
static void cop_init(env, plane)
lpcenv *env;
dataplane *plane;
{
    memset(plane->coptab = IALLOC(env, coptable, 1), '\0', sizeof(coptable));
}

/*
 * NAME:	copatch->clean()
 * DESCRIPTION:	free copatch table
 */
static void cop_clean(env, plane)
register lpcenv *env;
dataplane *plane;
{
    register copchunk *c, *f;

    c = plane->coptab->chunk;
    while (c != (copchunk *) NULL) {
	f = c;
	c = c->next;
	IFREE(env, f);
    }

    IFREE(env, plane->coptab);
    plane->coptab = (coptable *) NULL;
}

/*
 * NAME:	copatch->new()
 * DESCRIPTION:	create a new callout patch
 */
static copatch *cop_new(env, plane, c, type, handle, co, time, mtime, q)
lpcenv *env;
dataplane *plane;
copatch **c;
int type;
unsigned int handle, mtime;
register dcallout *co;
Uint time;
struct _cbuf_ *q;
{
    register coptable *tab;
    register copatch *cop;
    register int i;
    register value *v;

    /* allocate */
    tab = plane->coptab;
    if (tab->flist != (copatch *) NULL) {
	/* from free list */
	cop = tab->flist;
	tab->flist = cop->next;
    } else {
	/* newly allocated */
	if (tab->chunk == (copchunk *) NULL || tab->chunksz == COPCHUNKSZ) {
	    register copchunk *cc;

	    /* create new chunk */
	    cc = IALLOC(env, copchunk, 1);
	    cc->next = tab->chunk;
	    tab->chunk = cc;
	    tab->chunksz = 0;
	}

	cop = &tab->chunk->cop[tab->chunksz++];
    }

    /* initialize */
    cop->type = type;
    cop->handle = handle;
    if (type == COP_ADD) {
	cop->aco = *co;
    } else {
	cop->rco = *co;
    }
    for (i = (co->nargs > 3) ? 4 : co->nargs + 1, v = co->val; i > 0; --i) {
	i_ref_value(v++);
    }
    cop->time = time;
    cop->mtime = mtime;
    cop->plane = plane;
    cop->queue = q;

    /* add to hash table */
    cop->next = *c;
    return *c = cop;
}

/*
 * NAME:	copatch->del()
 * DESCRIPTION:	delete a callout patch
 */
static void cop_del(env, plane, c, del)
register lpcenv *env;
dataplane *plane;
copatch **c;
bool del;
{
    register copatch *cop;
    register dcallout *co;
    register int i;
    register value *v;
    coptable *tab;

    /* remove from hash table */
    cop = *c;
    *c = cop->next;

    if (del) {
	/* free referenced callout */
	co = (cop->type == COP_ADD) ? &cop->aco : &cop->rco;
	v = co->val;
	for (i = (co->nargs > 3) ? 4 : co->nargs + 1; i > 0; --i) {
	    i_del_value(env, v++);
	}
    }

    /* add to free list */
    tab = plane->coptab;
    cop->next = tab->flist;
    tab->flist = cop;
}

/*
 * NAME:	copatch->replace()
 * DESCRIPTION:	replace one callout patch with another
 */
static void cop_replace(cop, co, time, mtime, q)
register copatch *cop;
register dcallout *co;
Uint time;
unsigned int mtime;
struct _cbuf_ *q;
{
    register int i;
    register value *v;

    cop->type = COP_REPLACE;
    cop->aco = *co;
    for (i = (co->nargs > 3) ? 4 : co->nargs + 1, v = co->val; i > 0; --i) {
	i_ref_value(v++);
    }
    cop->time = time;
    cop->mtime = mtime;
    cop->queue = q;
}

/*
 * NAME:	copatch->commit()
 * DESCRIPTION:	commit a callout replacement
 */
static void cop_commit(env, cop)
register lpcenv *env;
register copatch *cop;
{
    register int i;
    register value *v;

    cop->type = COP_ADD;
    for (i = (cop->rco.nargs > 3) ? 4 : cop->rco.nargs + 1, v = cop->rco.val;
	 i > 0; --i) {
	i_del_value(env, v++);
    }
}

/*
 * NAME:	copatch->release()
 * DESCRIPTION:	remove a callout replacement
 */
static void cop_release(env, cop)
register lpcenv *env;
register copatch *cop;
{
    register int i;
    register value *v;

    cop->type = COP_REMOVE;
    for (i = (cop->aco.nargs > 3) ? 4 : cop->aco.nargs + 1, v = cop->aco.val;
	 i > 0; --i) {
	i_del_value(env, v++);
    }
}

/*
 * NAME:	copatch->discard()
 * DESCRIPTION:	discard replacement
 */
static void cop_discard(cop)
copatch *cop;
{
    /* force unref of proper component later */
    cop->type = COP_ADD;
}


/*
 * NAME:	data->new_plane()
 * DESCRIPTION:	create a new dataplane
 */
void d_new_plane(data, level)
register dataspace *data;
Int level;
{
    register dataplane *p;
    register Uint i;

    p = IALLOC(data->env, dataplane, 1);

    p->level = level;
    p->flags = data->plane->flags;
    p->schange = data->plane->schange;
    p->achange = data->plane->achange;
    p->imports = data->plane->imports;

    /* copy value information from previous plane */
    p->original = (value *) NULL;
    p->alocal.arr = (array *) NULL;
    p->alocal.plane = p;
    p->alocal.data = data;
    p->alocal.state = AR_CHANGED;
    p->coptab = data->plane->coptab;

    if (data->plane->arrays != (arrref *) NULL) {
	register arrref *a, *b;

	p->arrays = IALLOC(data->env, arrref, i = data->narrays);
	for (a = p->arrays, b = data->plane->arrays; i != 0; a++, b++, --i) {
	    if (b->arr != (array *) NULL) {
		*a = *b;
		a->arr->primary = a;
		arr_ref(a->arr);
	    } else {
		a->arr = (array *) NULL;
	    }
	}
    } else {
	p->arrays = (arrref *) NULL;
    }
    p->achunk = (struct _abchunk_ *) NULL;

    if (data->plane->strings != (strref *) NULL) {
	register strref *s, *t;

	p->strings = IALLOC(data->env, strref, i = data->nstrings);
	for (s = p->strings, t = data->plane->strings; i != 0; s++, t++, --i) {
	    if (t->str != (string *) NULL) {
		*s = *t;
		s->str->primary = s;
		str_ref(s->str);
	    } else {
		s->str = (string *) NULL;
	    }
	}
    } else {
	p->strings = (strref *) NULL;
    }

    p->prev = data->plane;
    data->plane = p;
    p->plist = data->env->de->plist;
    data->env->de->plist = p;
}

/*
 * NAME:	commit_values()
 * DESCRIPTION:	commit non-swapped arrays among the values
 */
static void commit_values(v, n, level)
register value *v;
register unsigned int n;
register Int level;
{
    register array *arr;

    while (n != 0) {
	if (T_INDEXED(v->type)) {
	    arr = v->u.array;
	    if (arr->primary->arr == (array *) NULL &&
		arr->primary->plane->level > level) {
		if (arr->hashed != (struct _maphash_ *) NULL) {
		    map_compact(arr);
		}
		arr->primary = &arr->primary->plane->prev->alocal;
		commit_values(arr->elts, arr->size, level);
	    }

	}
	v++;
	--n;
    }
}

/*
 * NAME:	commit_callouts()
 * DESCRIPTION:	commit callout patches to previous plane
 */
static void commit_callouts(env, plane, merge)
register lpcenv *env;
register dataplane *plane;
bool merge;
{
    register dataplane *prev;
    register copatch **c, **n, *cop;
    copatch **t, **next;
    int i;

    prev = plane->prev;
    for (i = COPATCHHTABSZ, t = plane->coptab->cop; --i >= 0; t++) {
	if (*t != (copatch *) NULL && (*t)->plane == plane) {
	    /*
	     * find previous plane in hash chain
	     */
	    next = t;
	    do {
		next = &(*next)->next;
	    } while (*next != (copatch *) NULL && (*next)->plane == plane);

	    c = t;
	    do {
		cop = *c;
		if (cop->type != COP_REMOVE) {
		    commit_values(cop->aco.val + 1,
				  (cop->aco.nargs > 3) ? 3 : cop->aco.nargs,
				  prev->level);
		}

		if (prev->level == 0) {
		    /*
		     * commit to last plane
		     */
		    switch (cop->type) {
		    case COP_ADD:
			co_new(plane->alocal.data->oindex, cop->handle,
			       cop->time, cop->mtime, cop->queue);
			--env->de->ncallout;
			break;

		    case COP_REMOVE:
			co_del(plane->alocal.data->oindex, cop->handle,
			       cop->rco.time);
			env->de->ncallout++;
			break;

		    case COP_REPLACE:
			co_del(plane->alocal.data->oindex, cop->handle,
			       cop->rco.time);
			co_new(plane->alocal.data->oindex, cop->handle,
			       cop->time, cop->mtime, cop->queue);
			cop_commit(env, cop);
			break;
		    }

		    if (next == &cop->next) {
			next = c;
		    }
		    cop_del(env, plane, c, TRUE);
		} else {
		    /*
		     * commit to previous plane
		     */
		    cop->plane = prev;
		    if (merge) {
			for (n = next;
			     *n != (copatch *) NULL && (*n)->plane == prev;
			     n = &(*n)->next) {
			    if (cop->handle == (*n)->handle) {
				switch (cop->type) {
				case COP_ADD:
				    /* turn old remove into replace, del new */
				    cop_replace(*n, &cop->aco, cop->time,
						cop->mtime, cop->queue);
				    if (next == &cop->next) {
					next = c;
				    }
				    cop_del(env, prev, c, TRUE);
				    break;

				case COP_REMOVE:
				    if ((*n)->type == COP_REPLACE) {
					/* turn replace back into remove */
					cop_release(env, *n);
				    } else {
					/* del old */
					cop_del(env, prev, n, TRUE);
				    }
				    /* del new */
				    if (next == &cop->next) {
					next = c;
				    }
				    cop_del(env, prev, c, TRUE);
				    break;

				case COP_REPLACE:
				    if ((*n)->type == COP_REPLACE) {
					/* merge replaces into old, del new */
					cop_release(env, *n);
					cop_replace(*n, &cop->aco, cop->time,
						    cop->mtime, cop->queue);
					if (next == &cop->next) {
					    next = c;
					}
					cop_del(env, prev, c, TRUE);
				    } else {
					/* make replace into add, remove old */
					cop_del(env, prev, n, TRUE);
					cop_commit(env, cop);
				    }
				    break;
				}
				break;
			    }
			}
		    }

		    if (*c == cop) {
			c = &cop->next;
		    }
		}
	    } while (c != next);
	}
    }
}

/*
 * NAME:	data->commit_plane()
 * DESCRIPTION:	commit the current data plane
 */
void d_commit_plane(env, level, retval)
register lpcenv *env;
Int level;
value *retval;
{
    register dataplane *p, *commit, **r, **cr;
    register dataspace *data;
    register value *v;
    register Uint i;
    register dataenv *de;
    dataplane *clist;

    /*
     * pass 1: construct commit planes
     */
    clist = (dataplane *) NULL;
    cr = &clist;
    de = env->de;
    for (r = &de->plist, p = *r; p != (dataplane *) NULL && p->level == level;
	 r = &p->plist, p = *r) {
	if (p->prev->level != level - 1) {
	    /* insert commit plane */
	    commit = IALLOC(env, dataplane, 1);
	    commit->level = level - 1;
	    commit->original = (value *) NULL;
	    commit->alocal.arr = (array *) NULL;
	    commit->alocal.plane = commit;
	    commit->alocal.data = p->alocal.data;
	    commit->alocal.state = AR_CHANGED;
	    commit->arrays = p->arrays;
	    commit->achunk = p->achunk;
	    commit->strings = p->strings;
	    commit->coptab = p->coptab;
	    commit->prev = p->prev;
	    *cr = commit;
	    cr = &commit->plist;

	    p->prev = commit;
	} else {
	    p->flags |= PLANE_MERGE;
	}
    }
    if (clist != (dataplane *) NULL) {
	/* insert commit planes in plane list */
	*cr = p;
	*r = clist;
    }
    clist = *r;	/* sentinel */

    /*
     * pass 2: commit
     */
    for (p = de->plist; p != clist; p = p->plist) {
	/*
	 * commit changes to previous plane
	 */
	data = p->alocal.data;
	if (p->original != (value *) NULL) {
	    if (p->level == 1 || p->prev->original != (value *) NULL) {
		/* free backed-up variable values */
		for (v = p->original, i = data->nvariables; i != 0; v++, --i) {
		    i_del_value(env, v);
		}
		IFREE(env, p->original);
	    } else {
		/* move originals to previous plane */
		p->prev->original = p->original;
	    }
	    commit_values(data->variables, data->nvariables, level - 1);
	}

	if (p->coptab != (coptable *) NULL) {
	    /* commit callout changes */
	    commit_callouts(env, p, p->flags & PLANE_MERGE);
	    if (p->level == 1) {
		cop_clean(env, p);
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	arr_commit(env, p->achunk, p->prev, p->flags & PLANE_MERGE);
	if (p->flags & PLANE_MERGE) {
	    if (p->arrays != (arrref *) NULL) {
		register arrref *a;

		/* remove old array refs */
		for (a = p->prev->arrays, i = data->narrays; i != 0; a++, --i) {
		    if (a->arr != (array *) NULL) {
			if (a->arr->primary == &p->alocal) {
			    a->arr->primary = &p->prev->alocal;
			}
			arr_del(env, a->arr);
		    }
		}
		IFREE(env, p->prev->arrays);
		p->prev->arrays = p->arrays;
	    }

	    if (p->strings != (strref *) NULL) {
		register strref *s;

		/* remove old string refs */
		for (s = p->prev->strings, i = data->nstrings; i != 0; s++, --i)
		{
		    if (s->str != (string *) NULL) {
			str_del(env, s->str);
		    }
		}
		IFREE(env, p->prev->strings);
		p->prev->strings = p->strings;
	    }
	}

	data->plane = p->prev;
    }
    commit_values(retval, 1, level - 1);

    /*
     * pass 3: deallocate
     */
    for (p = de->plist; p != clist; p = de->plist) {
	p->prev->flags = p->flags & MOD_ALL;
	p->prev->schange = p->schange;
	p->prev->achange = p->achange;
	p->prev->imports = p->imports;
	de->plist = p->plist;
	IFREE(env, p);
    }
}

/*
 * NAME:	discard_callouts()
 * DESCRIPTION:	discard callout patches on current plane, restoring old callouts
 */
static void discard_callouts(env, plane)
register lpcenv *env;
register dataplane *plane;
{
    register copatch *cop, **c, **t;
    register dataspace *data;
    register int i;

    data = plane->alocal.data;
    for (i = COPATCHHTABSZ, t = plane->coptab->cop; --i >= 0; t++) {
	c = t;
	while (*c != (copatch *) NULL && (*c)->plane == plane) {
	    cop = *c;
	    switch (cop->type) {
	    case COP_ADD:
		d_free_call_out(data, cop->handle);
		cop_del(env, plane, c, TRUE);
		--env->de->ncallout;
		break;

	    case COP_REMOVE:
		d_alloc_call_out(data, cop->handle, cop->rco.time,
				 cop->rco.nargs, cop->rco.val);
		cop_del(env, plane, c, FALSE);
		env->de->ncallout++;
		break;

	    case COP_REPLACE:
		d_free_call_out(data, cop->handle);
		d_alloc_call_out(data, cop->handle, cop->rco.time,
				 cop->rco.nargs, cop->rco.val);
		cop_discard(cop);
		cop_del(env, plane, c, TRUE);
		break;
	    }
	}
    }
}

/*
 * NAME:	data->discard_plane()
 * DESCRIPTION:	discard the current data plane without committing it
 */
void d_discard_plane(env, level)
register lpcenv *env;
Int level;
{
    register dataplane *p;
    register dataspace *data;
    register value *v;
    register Uint i;

    for (p = env->de->plist; p != (dataplane *) NULL && p->level == level;
	 p = p->plist) {
	/*
	 * discard changes except for callout mods
	 */
	p->prev->flags |= p->flags & (MOD_CALLOUT | MOD_NEWCALLOUT);

	data = p->alocal.data;
	if (p->original != (value *) NULL) {
	    /* restore original variable values */
	    for (v = data->variables, i = data->nvariables; i != 0; --i, v++) {
		i_del_value(env, v);
	    }
	    memcpy(data->variables, p->original,
		   data->nvariables * sizeof(value));
	    IFREE(env, p->original);
	}

	if (p->coptab != (coptable *) NULL) {
	    /* undo callout changes */
	    discard_callouts(env, p);
	    if (p->prev == &data->base) {
		cop_clean(env, p);
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	arr_discard(env, p->achunk);
	if (p->arrays != (arrref *) NULL) {
	    register arrref *a;

	    /* delete new array refs */
	    for (a = p->arrays, i = data->narrays; i != 0; a++, --i) {
		if (a->arr != (array *) NULL) {
		    arr_del(env, a->arr);
		}
	    }
	    IFREE(env, p->arrays);
	    /* fix old ones */
	    for (a = p->prev->arrays, i = data->narrays; i != 0; a++, --i) {
		if (a->arr != (array *) NULL) {
		    a->arr->primary = a;
		}
	    }
	}

	if (p->strings != (strref *) NULL) {
	    register strref *s;

	    /* delete new string refs */
	    for (s = p->strings, i = data->nstrings; i != 0; s++, --i) {
		if (s->str != (string *) NULL) {
		    str_del(env, s->str);
		}
	    }
	    IFREE(env, p->strings);
	    /* fix old ones */
	    for (s = p->prev->strings, i = data->nstrings; i != 0; s++, --i) {
		if (s->str != (string *) NULL) {
		    s->str->primary = s;
		}
	    }
	}

	data->plane = p->prev;
	env->de->plist = p->plist;
	IFREE(env, p);
    }
}


/*
 * NAME:	data->commit_arr()
 * DESCRIPTION:	commit array to previous plane
 */
struct _abchunk_ **d_commit_arr(arr, prev, old)
register array *arr;
dataplane *prev, *old;
{
    if (arr->primary->plane != prev) {
	if (arr->hashed != (struct _maphash_ *) NULL) {
	    map_compact(arr);
	}

	if (arr->primary->arr == (array *) NULL) {
	    arr->primary = &prev->alocal;
	} else {
	    arr->primary->plane = prev;
	}
	commit_values(arr->elts, arr->size, prev->level);
    }

    return (prev == old) ? (struct _abchunk_ **) NULL : &prev->achunk;
}

/*
 * NAME:	data->discard_arr()
 * DESCRIPTION:	restore array to previous plane
 */
void d_discard_arr(arr, plane)
array *arr;
dataplane *plane;
{
    /* swapped-in arrays will be fixed later */
    arr->primary = &plane->alocal;
}


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
    register dataenv *de;

    data = arr->primary->data;
    de = data->env->de;
    for (n = arr->size, v = arr->elts; n > 0; --n, v++) {
	if (T_INDEXED(v->type) && data != v->u.array->primary->data) {
	    /* mark as imported */
	    if (data->plane->imports++ == 0 && de->ifirst != data &&
		data->iprev == (dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (dataspace *) NULL;
		data->inext = de->ifirst;
		if (de->ifirst != (dataspace *) NULL) {
		    de->ifirst->iprev = data;
		}
		de->ifirst = data;
	    }
	}
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
	if (data->plane->level != 0 &&
	    data->plane->original == (value *) NULL) {
	    /*
	     * back up variables
	     */
	    i_copy(data->env, data->plane->original = IALLOC(data->env, value,
							     data->nvariables),
		   data->variables, data->nvariables);
	}
	ref_rhs(data, val);
	del_lhs(data, var);
	data->plane->flags |= MOD_VARIABLE;
    }

    i_ref_value(val);
    i_del_value(data->env, var);

    *var = *val;
    var->modified = TRUE;
}

/*
 * NAME:	data->get_extravar()
 * DESCRIPTION:	get an object's special value
 */
value *d_get_extravar(data)
dataspace *data;
{
    return &d_get_variables(data)[data->nvariables - 1];
}

/*
 * NAME:	data->set_extravar()
 * DESCRIPTION:	set an object's special value
 */
void d_set_extravar(data, val)
register dataspace *data;
value *val;
{
    d_assign_var(data, &d_get_variables(data)[data->nvariables - 1], val);
}

/*
 * NAME:	data->wipe_extravar()
 * DESCRIPTION:	wipe out an object's special value
 */
void d_wipe_extravar(data)
register dataspace *data;
{
    d_assign_var(data, &d_get_variables(data)[data->nvariables - 1],
		 &nil_value);

    if (data->parser != (struct _parser_ *) NULL) {
	/*
	 * get rid of the parser, too
	 */
	ps_del(data->parser, data->env);
	data->parser = (struct _parser_ *) NULL;
    }
}

/*
 * NAME:	data->assign_elt()
 * DESCRIPTION:	assign a value to an array element
 */
void d_assign_elt(data, arr, elt, val)
register dataspace *data;
register array *arr;
register value *elt, *val;
{
    if (data->plane->level != arr->primary->data->plane->level) {
	/*
	 * bring dataspace of imported array up to the current plane level
	 */
	d_new_plane(arr->primary->data, data->plane->level);
    }

    data = arr->primary->data;
    if (arr->primary->plane != data->plane) {
	/*
	 * backup array's current elements
	 */
	data->plane->achunk = arr_backup(data->env, data->plane->achunk, arr);
	if (arr->primary->arr != (array *) NULL) {
	    arr->primary->plane = data->plane;
	} else {
	    arr->primary = &data->plane->alocal;
	}
    }

    if (arr->primary->arr != (array *) NULL) {
	/*
	 * the array is in the loaded dataspace of some object
	 */
	if ((arr->primary->ref & ARR_MOD) == 0) {
	    arr->primary->ref |= ARR_MOD;
	    data->plane->flags |= MOD_ARRAY;
	}
	ref_rhs(data, val);
	del_lhs(data, elt);
    } else {
	if (T_INDEXED(val->type) && data != val->u.array->primary->data) {
	    register dataenv *de;

	    /* mark as imported */
	    de = data->env->de;
	    if (data->plane->imports++ == 0 && de->ifirst != data &&
		data->iprev == (dataspace *) NULL) {
		/* add to imports list */
		data->iprev = (dataspace *) NULL;
		data->inext = de->ifirst;
		if (de->ifirst != (dataspace *) NULL) {
		    de->ifirst->iprev = data;
		}
		de->ifirst = data;
	    }
	}
	if (T_INDEXED(elt->type) && data != elt->u.array->primary->data) {
	    /* mark as unimported */
	    data->plane->imports--;
	}
    }

    i_ref_value(val);
    i_del_value(data->env, elt);

    *elt = *val;
    elt->modified = TRUE;
}

/*
 * NAME:	data->change_map()
 * DESCRIPTION:	mark a mapping as changed in size
 */
void d_change_map(map)
array *map;
{
    register arrref *a;

    a = map->primary;
    if (a->state == AR_UNCHANGED) {
	a->plane->achange++;
	a->state = AR_CHANGED;
    }
}


/*
 * NAME:	data->new_call_out()
 * DESCRIPTION:	add a new callout
 */
uindex d_new_call_out(data, func, delay, mdelay, f, nargs)
register dataspace *data;
string *func;
Int delay;
unsigned int mdelay;
register frame *f;
int nargs;
{
    Uint ct, t;
    unsigned short m;
    struct _cbuf_ *q;
    value v[4];
    uindex handle;

    ct = co_check(data->env, data->env->de->ncallout, delay, mdelay, &t, &m,
		  &q);
    if (ct == 0 && q == (struct _cbuf_ *) NULL) {
	/* callouts are disabled */
	return 0;
    }
    if (data->ncallouts >= conf_array_size() && data->fcallouts == 0) {
	error(data->env, "Too many callouts in object");
    }

    PUT_STRVAL(&v[0], func);
    switch (nargs) {
    case 3:
	v[3] = f->sp[2];
    case 2:
	v[2] = f->sp[1];
    case 1:
	v[1] = f->sp[0];
    case 0:
	break;

    default:
	v[1] = f->sp[0];
	v[2] = f->sp[1];
	PUT_ARRVAL(&v[3], arr_new(data, nargs - 2L));
	memcpy(v[3].u.array->elts, f->sp + 2, (nargs - 2) * sizeof(value));
	d_ref_imports(v[3].u.array);
	break;
    }
    f->sp += nargs;
    handle = d_alloc_call_out(data, 0, ct, nargs, v);

    if (data->plane->level == 0) {
	/*
	 * add normal callout
	 */
	co_new(data->oindex, handle, t, m, q);
    } else {
	register dataplane *plane;
	register copatch **c, *cop;
	dcallout *co;
	copatch **cc;

	/*
	 * add callout patch
	 */
	plane = data->plane;
	if (plane->coptab == (coptable *) NULL) {
	    cop_init(data->env, plane);
	}
	co = &data->callouts[handle - 1];
	cc = c = &plane->coptab->cop[handle % COPATCHHTABSZ];
	for (;;) {
	    cop = *c;
	    if (cop == (copatch *) NULL || cop->plane != plane) {
		/* add new */
		cop_new(data->env, plane, cc, COP_ADD, handle, co, t, m, q);
		break;
	    }

	    if (cop->handle == handle) {
		/* replace removed */
		cop_replace(cop, co, t, m, q);
		break;
	    }

	    c = &cop->next;
	}

	data->env->de->ncallout++;
    }

    return handle;
}

/*
 * NAME:	data->del_call_out()
 * DESCRIPTION:	remove a callout
 */
Int d_del_call_out(data, handle)
dataspace *data;
unsigned int handle;
{
    register dcallout *co;
    Int t;

    if (handle == 0 || handle > data->ncallouts) {
	/* no such callout */
	return -1;
    }
    if (data->callouts == (dcallout *) NULL) {
	sd_load_callouts(data);
    }

    co = &data->callouts[handle - 1];
    if (co->val[0].type != T_STRING) {
	/* invalid callout */
	return -1;
    }

    t = co_remaining(co->time);
    if (data->plane->level == 0) {
	/*
	 * remove normal callout
	 */
	co_del(data->oindex, handle, co->time);
    } else {
	register dataplane *plane;
	register copatch **c, *cop;
	copatch **cc;

	/*
	 * add/remove callout patch
	 */
	--data->env->de->ncallout;

	plane = data->plane;
	if (plane->coptab == (coptable *) NULL) {
	    cop_init(data->env, plane);
	}
	cc = c = &plane->coptab->cop[handle % COPATCHHTABSZ];
	for (;;) {
	    cop = *c;
	    if (cop == (copatch *) NULL || cop->plane != plane) {
		/* delete new */
		cop_new(data->env, plane, cc, COP_REMOVE, handle, co, (Uint) 0,
			0, (struct _cbuf_ *) NULL);
		break;
	    }
	    if (cop->handle == handle) {
		/* delete existing */
		cop_del(data->env, plane, c, TRUE);
		break;
	    }
	    c = &cop->next;
	}
    }
    d_free_call_out(data, handle);

    return t;
}

/*
 * NAME:	data->get_call_out()
 * DESCRIPTION:	get a callout
 */
string *d_get_call_out(data, handle, f, nargs)
register dataspace *data;
unsigned int handle;
register frame *f;
int *nargs;
{
    string *str;
    register dcallout *co;
    register value *v, *o;
    register uindex n;

    if (data->callouts == (dcallout *) NULL) {
	sd_load_callouts(data);
    }

    co = &data->callouts[handle - 1];
    v = co->val;
    del_lhs(data, &v[0]);
    str = v[0].u.string;

    i_grow_stack(f, (*nargs = co->nargs) + 1);
    *--f->sp = v[0];

    switch (co->nargs) {
    case 3:
	del_lhs(data, &v[3]);
	*--f->sp = v[3];
    case 2:
	del_lhs(data, &v[2]);
	*--f->sp = v[2];
    case 1:
	del_lhs(data, &v[1]);
	*--f->sp = v[1];
    case 0:
	break;

    default:
	n = co->nargs - 2;
	f->sp -= n;
	memcpy(f->sp, d_get_elts(v[3].u.array), n * sizeof(value));
	del_lhs(data, &v[3]);
	IFREE(data->env, v[3].u.array->elts);
	v[3].u.array->elts = (value *) NULL;
	arr_del(data->env, v[3].u.array);
	del_lhs(data, &v[2]);
	*--f->sp = v[2];
	del_lhs(data, &v[1]);
	*--f->sp = v[1];
	break;
    }

    /* wipe out destructed objects */
    for (n = co->nargs, v = f->sp; n > 0; --n, v++) {
	switch (v->type) {
	case T_OBJECT:
	    if (DESTRUCTED(data->env, v)) {
		*v = nil_value;
	    }
	    break;

	case T_LWOBJECT:
	    o = d_get_elts(v->u.array);
	    if (DESTRUCTED(data->env, o)) {
		arr_del(data->env, v->u.array);
		*v = nil_value;
	    }
	    break;
	}
    }

    co->val[0] = nil_value;
    n = data->fcallouts;
    if (n != 0) {
	data->callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    data->fcallouts = handle;

    data->plane->flags |= MOD_CALLOUT;
    return str;
}

/*
 * NAME:	data->list_callouts()
 * DESCRIPTION:	list all call_outs in an object
 */
array *d_list_callouts(host, data)
dataspace *host;
register dataspace *data;
{
    register uindex n, count, size;
    register dcallout *co;
    register value *v, *v2, *elts;
    array *list, *a;
    uindex max_args;

    if (data->ncallouts == 0) {
	return arr_new(host, 0L);
    }
    if (data->callouts == (dcallout *) NULL) {
	sd_load_callouts(data);
    }

    /* get the number of callouts in this object */
    count = data->ncallouts;
    for (n = data->fcallouts; n != 0; n = data->callouts[n - 1].co_next) {
	--count;
    }

    list = arr_new(host, (long) count);
    elts = list->elts;
    max_args = conf_array_size() - 3;

    for (co = data->callouts; count > 0; co++) {
	if (co->val[0].type == T_STRING) {
	    size = co->nargs;
	    if (size > max_args) {
		/* unlikely, but possible */
		size = max_args;
	    }
	    a = arr_new(host, size + 3L);
	    v = a->elts;

	    /* handle */
	    PUT_INTVAL(v, co - data->callouts + 1);
	    v++;
	    /* function */
	    PUT_STRVAL(v, co->val[0].u.string);
	    v++;
	    /* time */
	    PUT_INTVAL(v, co->time);
	    v++;

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
	    PUT_ARRVAL(elts, a);
	    elts++;
	    --count;
	}
    }
    co_list(data->env, list);

    return list;
}


/*
 * NAME:	data->set_varmap()
 * DESCRIPTION:	add a variable mapping to a control block
 */
void d_set_varmap(ctrl, nvar, vmap)
register control *ctrl;
unsigned int nvar;
unsigned short *vmap;
{
    ctrl->vmapsize = nvar;
    ctrl->vmap = vmap;

    /* varmap modified */
    ctrl->flags |= CTRL_VARMAP;
}

/*
 * NAME:	data->get_varmap()
 * DESCRIPTION:	get the variable mapping for an object
 */
static unsigned short *d_get_varmap(env, obj, update, nvariables)
register lpcenv *env;
object **obj;
register Uint update;
unsigned short *nvariables;
{
    register object *tmpl;
    register unsigned short nvar, *vmap;

    tmpl = OBJ((*obj)->prev);
    if (O_UPGRADING(*obj)) {
	/* in the middle of an upgrade */
	tmpl = OBJ(tmpl->prev);
    }
    vmap = o_control(env, tmpl)->vmap;
    nvar = tmpl->ctrl->vmapsize;

    if (tmpl->update != update) {
	register unsigned short *m1, *m2, n;

	m1 = vmap;
	vmap = IALLOC(env, unsigned short, n = nvar);
	do {
	    tmpl = OBJ(tmpl->prev);
	    m2 = o_control(env, tmpl)->vmap;
	    while (n > 0) {
		*vmap++ = (NEW_VAR(*m1)) ? *m1++ : m2[*m1++];
		--n;
	    }
	    n = nvar;
	    vmap -= n;
	    m1 = vmap;
	} while (tmpl->update != update);
    }

    *obj = tmpl;
    *nvariables = nvar;
    return vmap;
}

/*
 * NAME:	data->upgrade_data()
 * DESCRIPTION:	upgrade the dataspace for one object
 */
void d_upgrade_data(data, nvar, vmap, tmpl)
register dataspace *data;
register unsigned short nvar, *vmap;
object *tmpl;
{
    register value *v;
    register unsigned short n;
    value *vars;

    /* make sure variables are in memory */
    vars = d_get_variables(data);

    /* map variables */
    for (n = nvar, v = IALLOC(data->env, value, n); n > 0; --n) {
	switch (*vmap) {
	case NEW_INT:
	    *v++ = zero_int;
	    break;

	case NEW_FLOAT:
	    *v++ = zero_float;
	    break;

	case NEW_POINTER:
	    *v++ = nil_value;
	    break;

	default:
	    *v = vars[*vmap];
	    i_ref_value(v);
	    v->modified = TRUE;
	    ref_rhs(data, v++);
	    break;
	}
	vmap++;
    }
    vars = v - nvar;

    /* deref old values */
    v = data->variables;
    for (n = data->nvariables; n > 0; --n) {
	del_lhs(data, v);
	i_del_value(data->env, v++);
    }

    /* replace old with new */
    IFREE(data->env, data->variables);
    data->variables = vars;

    data->base.flags |= MOD_VARIABLE;
    if (data->nvariables != nvar) {
	data->nvariables = nvar;
	data->base.achange++;	/* force rebuild on swapout */
    }

    o_upgraded(tmpl, OBJ(data->oindex));
}

/*
 * NAME:	data->upgrade_clone()
 * DESCRIPTION:	upgrade a clone object
 */
void d_upgrade_clone(data)
dataspace *data;
{
    object *obj;
    unsigned short *vmap, nvar;
    Uint update;

    /*
     * the program for the clone was upgraded since last swapin
     */
    obj = OBJ(data->oindex);
    update = obj->update;
    obj = OBJ(obj->u_master);
    vmap = d_get_varmap(data->env, &obj, update, &nvar);
    d_upgrade_data(data, nvar, vmap, obj);
    if (vmap != obj->ctrl->vmap) {
	IFREE(data->env, vmap);
    }
}

/*
 * NAME:	data->upgrade_lwobj()
 * DESCRIPTION:	upgrade a non-persistent object
 */
object *d_upgrade_lwobj(env, lwobj, obj)
register lpcenv *env;
register array *lwobj;
object *obj;
{
    register arrref *a;
    register unsigned short n;
    register value *v;
    Uint update;
    unsigned short nvar, *vmap;
    value *vars;

    a = lwobj->primary;
    update = obj->update;
    vmap = d_get_varmap(env, &obj, (Uint) lwobj->elts[1].u.number, &nvar);
    --nvar;

    /* map variables */
    v = IALLOC(env, value, nvar + 2);
    *v++ = lwobj->elts[0];
    PUT_INTVAL(v, update);
    v++;

    vars = lwobj->elts + 2;
    for (n = nvar; n > 0; --n) {
	switch (*vmap) {
	case NEW_INT:
	    *v++ = zero_int;
	    break;

	case NEW_FLOAT:
	    *v++ = zero_float;
	    break;

	case NEW_POINTER:
	    *v++ = nil_value;
	    break;

	default:
	    *v = vars[*vmap];
	    if (a->arr != (array *) NULL) {
		ref_rhs(a->data, v);
	    }
	    i_ref_value(v);
	    (v++)->modified = TRUE;
	    break;
	}
	vmap++;
    }
    vars = v - (nvar + 2);

    v = lwobj->elts + 2;
    if (a->arr != (array *) NULL) {
	/* swapped-in */
	if (a->state == AR_UNCHANGED) {
	    register dataplane *p;

	    a->state = AR_CHANGED;
	    for (p = a->data->plane; p != (dataplane *) NULL; p = p->prev) {
		p->achange++;
	    }
	}

	/* deref old values */
	for (n = nvar; n > 0; --n) {
	    del_lhs(a->data, v);
	    i_del_value(env, v++);
	}
    } else {
	/* deref old values */
	for (n = nvar; n > 0; --n) {
	    i_del_value(env, v++);
	}
    }

    /* replace old with new */
    lwobj->size = nvar + 2;
    IFREE(env, lwobj->elts);
    lwobj->elts = vars;

    return obj;
}

/*
 * NAME:	data->upgrade_mem()
 * DESCRIPTION:	upgrade all obj and all objects cloned from obj that have
 *		dataspaces in memory
 */
void d_upgrade_mem(env, tmpl, new)
lpcenv *env;
register object *tmpl, *new;
{
    register dataspace *data;
    register unsigned int nvar;
    register unsigned short *vmap;
    register object *obj;

    nvar = tmpl->ctrl->vmapsize;
    vmap = tmpl->ctrl->vmap;

    for (data = env->de->dtail; data != (dataspace *) NULL; data = data->prev) {
	obj = OBJ(data->oindex);
	if ((obj == new ||
	     (!(obj->flags & O_MASTER) && obj->u_master == new->index)) &&
	    obj->count != 0) {
	    /* upgrade clone */
	    if (nvar != 0) {
		d_upgrade_data(data, nvar, vmap, tmpl);
	    }
	    data->ctrl->ndata--;
	    data->ctrl = new->ctrl;
	    data->ctrl->ndata++;
	}
    }
}


/*
 * NAME:	data->import()
 * DESCRIPTION:	copy imported arrays to current dataspace
 */
static void d_import(imp, data, val, n)
register arrimport *imp;
register dataspace *data;
register value *val;
register unsigned short n;
{
    while (n > 0) {
	if (T_INDEXED(val->type)) {
	    register array *a;
	    register Uint i, j;

	    a = val->u.array;
	    if (a->primary->data != data) {
		/*
		 * imported array
		 */
		i = arr_put(imp->merge, a, imp->narr);
		if (i == imp->narr) {
		    /*
		     * first time encountered
		     */
		    if (a->hashed != (struct _maphash_ *) NULL) {
			map_compact(a);
		    }

		    if (a->ref == 2) {	/* + 1 for array merge table */
			/*
			 * move array to new dataspace
			 */
			a->primary = &data->base.alocal;
			a->prev->next = a->next;
			a->next->prev = a->prev;
		    } else {
			/*
			 * make new array
			 */
			a = arr_alloc(data->env, a->size);
			a->tag = val->u.array->tag;
			a->odcount = val->u.array->odcount;
			a->primary = &data->base.alocal;

			if (a->size > 0) {
			    /*
			     * copy elements
			     */
			    i_copy(data->env,
				   a->elts = IALLOC(data->env, value, a->size),
				   d_get_elts(val->u.array), a->size);
			}

			/*
			 * replace
			 */
			arr_del(data->env, val->u.array);
			arr_ref(val->u.array = a);
			imp->narr++;
		    }
		    a->prev = &data->alist;
		    a->next = data->alist.next;
		    a->next->prev = a;
		    data->alist.next = a;

		    /*
		     * store in itab
		     */
		    if (i >= imp->itabsz) {
			/*
			 * increase size of itab
			 */
			for (j = imp->itabsz; j <= i; j += j) ;
			imp->itab = IREALLOC(data->env, imp->itab, array*,
					     imp->itabsz, j);
			imp->itabsz = j;
		    }
		    arr_put(imp->merge, imp->itab[i] = a, imp->narr++);

		    if (a->size > 0) {
			/*
			 * import elements too
			 */
			d_import(imp, data, a->elts, a->size);
		    }
		} else {
		    /*
		     * array was previously replaced
		     */
		    arr_ref(a = imp->itab[i]);
		    arr_del(data->env, val->u.array);
		    val->u.array = a;
		}
	    } else if (arr_put(imp->merge, a, imp->narr) == imp->narr) {
		/*
		 * not previously encountered mapping or array
		 */
		imp->narr++;
		if (a->hashed != (struct _maphash_ *) NULL) {
		    map_compact(a);
		    d_import(imp, data, a->elts, a->size);
		} else if (a->elts != (value *) NULL) {
		    d_import(imp, data, a->elts, a->size);
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
void d_export(env)
lpcenv *env;
{
    register dataspace *data;
    register Uint n;
    arrimport imp;

    if (env->de->ifirst != (dataspace *) NULL) {
	imp.itab = IALLOC(env, array*, imp.itabsz = 64);

	for (data = env->de->ifirst; data != (dataspace *) NULL;
	     data = data->inext) {
	    if (data->base.imports != 0) {
		data->base.imports = 0;
		imp.merge = arr_merge(env);
		imp.narr = 0;

		if (data->variables != (value *) NULL) {
		    d_import(&imp, data, data->variables, data->nvariables);
		}
		if (data->base.arrays != (arrref *) NULL) {
		    register arrref *a;

		    for (n = data->narrays, a = data->base.arrays; n > 0;
			 --n, a++) {
			if (a->arr != (array *) NULL) {
			    if (a->arr->hashed != (struct _maphash_ *) NULL) {
				/* mapping */
				map_compact(a->arr);
				d_import(&imp, data, a->arr->elts,
					 a->arr->size);
			    } else if (a->arr->elts != (value *) NULL) {
				d_import(&imp, data, a->arr->elts,
					 a->arr->size);
			    }
			}
		    }
		}
		if (data->callouts != (dcallout *) NULL) {
		    register dcallout *co;

		    co = data->callouts;
		    for (n = data->ncallouts; n > 0; --n) {
			if (co->val[0].type == T_STRING) {
			    d_import(&imp, data, co->val,
				     (co->nargs > 3) ? 4 : co->nargs + 1);
			}
			co++;
		    }
		}
		arr_clear(imp.merge);	/* clear merge table */
	    }
	    data->iprev = (dataspace *) NULL;
	}
	env->de->ifirst = (dataspace *) NULL;

	IFREE(env, imp.itab);
    }
}


/*
 * NAME:	data->swapout()
 * DESCRIPTION:	Swap out a portion of the control and dataspace blocks in
 *		memory.  Return the number of dataspace blocks swapped out.
 */
sector d_swapout(env, frag)
register lpcenv *env;
unsigned int frag;
{
    register dataenv *de;
    register sector n, count;
    register dataspace *data;
    register control *ctrl;

    de = env->de;
    count = 0;

    /* perform garbage collection for one dataspace */
    if (de->gcdata != (dataspace *) NULL) {
	if (sd_save_dataspace(de->gcdata, (frag != 0), (Uint *) NULL)) {
	    count++;
	}
	de->gcdata = de->gcdata->gcnext;
    }

    if (frag != 0) {
	/* swap out dataspace blocks */
	data = de->dtail;
	for (n = de->ndata / frag; n > 0; --n) {
	    register dataspace *prev;

	    prev = data->prev;
	    if (!(OBJ(data->oindex)->flags & O_PENDIO) || frag == 1) {
		if ((OBJ(data->oindex)->flags & O_SPECIAL) == O_SPECIAL &&
		    ext_swapout != (void (*) P((object*))) NULL) {
		    (*ext_swapout)(OBJ(data->oindex));
		}
		if (sd_save_dataspace(data, TRUE, (Uint *) NULL)) {
		    count++;
		}
		OBJ(data->oindex)->data = (dataspace *) NULL;
		d_free_dataspace(data);
	    }
	    data = prev;
	}

	/* swap out control blocks */
	ctrl = de->ctail;
	for (n = de->nctrl / frag; n > 0; --n) {
	    register control *prev;

	    prev = ctrl->prev;
	    if (ctrl->ndata == 0) {
		if ((ctrl->sctrl == (struct _scontrol_ *) NULL &&
		     !(ctrl->flags & CTRL_COMPILED)) ||
		    (ctrl->flags & CTRL_VARMAP)) {
		    sd_save_control(env, ctrl);
		}
		OBJ(ctrl->oindex)->ctrl = (control *) NULL;
		d_free_control(env, ctrl);
	    }
	    ctrl = prev;
	}
    }

    return count;
}

/*
 * NAME:	data->swapsync()
 * DESCRIPTION:	Synchronize the swap file with the state of memory, swapping
 *		out as little as possible.
 */
void d_swapsync(env)
register lpcenv *env;
{
    register control *ctrl;
    register dataspace *data;
    dataenv *de;

    de = env->de;

    /* save control blocks */
    for (ctrl = de->ctail; ctrl != (control *) NULL; ctrl = ctrl->prev) {
	if ((ctrl->sctrl == (struct _scontrol_ *) NULL &&
	     !(ctrl->flags & CTRL_COMPILED)) || (ctrl->flags & CTRL_VARMAP)) {
	    sd_save_control(env, ctrl);
	}
    }

    /* save dataspace blocks */
    for (data = de->dtail; data != (dataspace *) NULL; data = data->prev) {
	if ((OBJ(data->oindex)->flags & O_SPECIAL) == O_SPECIAL &&
	    ext_swapout != (void (*) P((object*))) NULL) {
	    (*ext_swapout)(OBJ(data->oindex));
	}
	sd_save_dataspace(data, TRUE, (Uint *) NULL);
    }
}


/*
 * NAME:	data->free_control()
 * DESCRIPTION:	remove the control block from memory
 */
void d_free_control(env, ctrl)
register lpcenv *env;
register control *ctrl;
{
    register string **strs;
    register dataenv *de;

    /* delete strings */
    if (ctrl->strings != (string **) NULL) {
	register unsigned short i;

	strs = ctrl->strings;
	for (i = ctrl->nstrings; i > 0; --i) {
	    if (*strs != (string *) NULL) {
		str_del(env, *strs);
	    }
	    strs++;
	}
	IFREE(env, ctrl->strings);
    }

    if (ctrl->sctrl != (struct _scontrol_ *) NULL) {
	sd_free_scontrol(ctrl->sctrl);
	if (ctrl->inherits != (dinherit *) NULL) {
	    /* delete inherits */
	    IFREE(env, ctrl->inherits);
	}
    } else if (!(ctrl->flags & CTRL_COMPILED)) {
	if (ctrl->inherits != (dinherit *) NULL) {
	    /* delete inherits */
	    IFREE(env, ctrl->inherits);
	}

	if (ctrl->prog != (char *) NULL) {
	    IFREE(env, ctrl->prog);
	}

	/* delete function definitions */
	if (ctrl->funcdefs != (dfuncdef *) NULL) {
	    IFREE(env, ctrl->funcdefs);
	}

	/* delete variable definitions */
	if (ctrl->vardefs != (dvardef *) NULL) {
	    IFREE(env, ctrl->vardefs);
	}

	/* delete function call table */
	if (ctrl->funcalls != (char *) NULL) {
	    IFREE(env, ctrl->funcalls);
	}

	/* delete symbol table */
	if (ctrl->symbols != (dsymbol *) NULL) {
	    IFREE(env, ctrl->symbols);
	}
    }

    /* delete vmap */
    if (ctrl->flags & CTRL_VARMAP) {
	IFREE(env, ctrl->vmap);
    }

    de = env->de;
    if (ctrl != de->chead) {
	ctrl->prev->next = ctrl->next;
    } else {
	de->chead = ctrl->next;
	if (de->chead != (control *) NULL) {
	    de->chead->prev = (control *) NULL;
	}
    }
    if (ctrl != de->ctail) {
	ctrl->next->prev = ctrl->prev;
    } else {
	de->ctail = ctrl->prev;
	if (de->ctail != (control *) NULL) {
	    de->ctail->next = (control *) NULL;
	}
    }
    --de->nctrl;

    IFREE(env, ctrl);
}

/*
 * NAME:	data->free_values()
 * DESCRIPTION:	free values in a dataspace block
 */
void d_free_values(data)
register dataspace *data;
{
    register Uint i;

    /* free parse_string data */
    if (data->parser != (struct _parser_ *) NULL) {
	ps_del(data->parser, data->env);
	data->parser = (struct _parser_ *) NULL;
    }

    /* free variables */
    if (data->variables != (value *) NULL) {
	register value *v;

	for (i = data->nvariables, v = data->variables; i > 0; --i, v++) {
	    i_del_value(data->env, v);
	}

	IFREE(data->env, data->variables);
	data->variables = (value *) NULL;
    }

    /* free callouts */
    if (data->callouts != (dcallout *) NULL) {
	register dcallout *co;
	register value *v;
	register int j;

	for (i = data->ncallouts, co = data->callouts; i > 0; --i, co++) {
	    v = co->val;
	    if (v->type == T_STRING) {
		j = 1 + co->nargs;
		if (j > 4) {
		    j = 4;
		}
		do {
		    i_del_value(data->env, v++);
		} while (--j > 0);
	    }
	}

	IFREE(data->env, data->callouts);
	data->callouts = (dcallout *) NULL;
    }

    /* free arrays */
    if (data->base.arrays != (arrref *) NULL) {
	register arrref *a;

	for (i = data->narrays, a = data->base.arrays; i > 0; --i, a++) {
	    if (a->arr != (array *) NULL) {
		arr_del(data->env, a->arr);
	    }
	}

	IFREE(data->env, data->base.arrays);
	data->base.arrays = (arrref *) NULL;
    }

    /* free strings */
    if (data->base.strings != (strref *) NULL) {
	register strref *s;

	for (i = data->nstrings, s = data->base.strings; i > 0; --i, s++) {
	    if (s->str != (string *) NULL) {
		s->str->primary = (strref *) NULL;
		str_del(data->env, s->str);
	    }
	}

	IFREE(data->env, data->base.strings);
	data->base.strings = (strref *) NULL;
    }

    /* free any left-over arrays */
    if (data->alist.next != &data->alist) {
	data->alist.prev->next = data->alist.next;
	data->alist.next->prev = data->alist.prev;
	arr_freelist(data->env, data->alist.next);
	data->alist.prev = data->alist.next = &data->alist;
    }
}

/*
 * NAME:	data->free_dataspace()
 * DESCRIPTION:	remove the dataspace block from memory
 */
void d_free_dataspace(data)
register dataspace *data;
{
    register dataenv *de;

    /* free values */
    d_free_values(data);

    if (data->sdata != (struct _sdataspace_ *) NULL) {
	sd_free_sdataspace(data->sdata);
    }

    if (data->ctrl != (control *) NULL) {
	data->ctrl->ndata--;
    }

    de = data->env->de;
    if (data != de->dhead) {
	data->prev->next = data->next;
    } else {
	de->dhead = data->next;
	if (de->dhead != (dataspace *) NULL) {
	    de->dhead->prev = (dataspace *) NULL;
	}
    }
    if (data != de->dtail) {
	data->next->prev = data->prev;
    } else {
	de->dtail = data->prev;
	if (de->dtail != (dataspace *) NULL) {
	    de->dtail->next = (dataspace *) NULL;
	}
    }
    data->gcprev->gcnext = data->gcnext;
    data->gcnext->gcprev = data->gcprev;
    if (data == de->gcdata) {
	de->gcdata = (data != data->gcnext) ? data->gcnext : (dataspace *) NULL;
    }
    --de->ndata;

    IFREE(data->env, data);
}

/*
 * NAME:	data->del_control()
 * DESCRIPTION:	delete a control block from swap and memory
 */
void d_del_control(env, ctrl)
lpcenv *env;
register control *ctrl;
{
    if (ctrl->sctrl != (struct _scontrol_ *) NULL) {
	sd_del_scontrol(ctrl->sctrl);
    }
    d_free_control(env, ctrl);
}

/*
 * NAME:	data->del_dataspace()
 * DESCRIPTION:	delete a dataspace block from swap and memory
 */
void d_del_dataspace(data)
register dataspace *data;
{
    if (data->sdata != (struct _sdataspace_ *) NULL) {
	sd_del_sdataspace(data->sdata);
    }

    if (data->iprev != (dataspace *) NULL) {
	data->iprev->inext = data->inext;
	if (data->inext != (dataspace *) NULL) {
	    data->inext->iprev = data->iprev;
	}
    } else if (data->env->de->ifirst == data) {
	data->env->de->ifirst = data->inext;
	if (data->env->de->ifirst != (dataspace *) NULL) {
	    data->env->de->ifirst->iprev = (dataspace *) NULL;
	}
    }

    if (data->ncallouts != 0) {
	register uindex n;
	register dcallout *co;

	/*
	 * remove callouts from callout table
	 */
	if (data->callouts == (dcallout *) NULL) {
	    sd_load_callouts(data);
	}
	for (n = data->ncallouts, co = data->callouts + n; n > 0; --n) {
	    if ((--co)->val[0].type == T_STRING) {
		d_del_call_out(data, n);
	    }
	}
    }

    d_free_dataspace(data);
}
