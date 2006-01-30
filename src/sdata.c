# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"
# include "parse.h"
# include "control.h"
# include "csupport.h"


# define CMPLIMIT		2048	/* compress if >= CMPLIMIT */
# define PRIV			0x8000	/* in sinherit->varoffset */

typedef struct {
    sector nsectors;		/* # sectors in part one */
    char flags;			/* control flags: compression */
    char ninherits;		/* # objects in inherit table */
    Uint compiled;		/* time of compilation */
    Uint progsize;		/* size of program code */
    unsigned short nstrings;	/* # strings in string constant table */
    Uint strsize;		/* size of string constant table */
    char nfuncdefs;		/* # entries in function definition table */
    char nvardefs;		/* # entries in variable definition table */
    char nclassvars;		/* # class variables */
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
    unsigned short nifdefs;	/* # int/float definitions */
    unsigned short nvinit;	/* # variables requiring initialization */
    unsigned short vmapsize;	/* size of variable map, or 0 for none */
} scontrol;

static char sc_layout[] = "dcciisicccusssss";

typedef struct {
    sector nsectors;		/* # sectors in part one */
    char flags;			/* control flags: compression */
    char ninherits;		/* # objects in inherit table */
    Uint compiled;		/* time of compilation */
    Uint progsize;		/* size of program code */
    unsigned short nstrings;	/* # strings in string constant table */
    Uint strsize;		/* size of string constant table */
    char nfuncdefs;		/* # entries in function definition table */
    char nvardefs;		/* # entries in variable definition table */
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
    unsigned short nifdefs;	/* # int/float definitions */
    unsigned short nvinit;	/* # variables requiring initialization */
    unsigned short vmapsize;	/* size of variable map, or 0 for none */
} oscontrol;

static char os_layout[] = "dcciisiccusssss";

typedef struct {
    uindex oindex;		/* index in object table */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset + private bit */
} sinherit;

static char si_layout[] = "uus";

typedef struct {
    sector nsectors;		/* number of sectors in data space */
    short flags;		/* dataspace flags: compression */
    unsigned short nvariables;	/* number of variables */
    Uint narrays;		/* number of array values */
    Uint eltsize;		/* total size of array elements */
    Uint nstrings;		/* number of strings */
    Uint strsize;		/* total size of strings */
    uindex ncallouts;		/* number of callouts */
    uindex fcallouts;		/* first free callout */
} sdataspace;

static char sd_layout[] = "dssiiiiuu";

struct _svalue_ {
    short type;			/* object, number, string, array */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Uint string;		/* string */
	Uint objcnt;		/* object creation count */
	Uint array;		/* array */
    } u;
};

static char sv_layout[] = "sui";

typedef struct _sarray_ {
    Uint index;			/* index in array value table */
    unsigned short size;	/* size of array */
    Uint ref;			/* refcount */
    Uint tag;			/* unique value for each array */
} sarray;

static char sa_layout[] = "isii";

typedef struct _sstring_ {
    Uint index;			/* index in string text table */
    ssizet len;			/* length of string */
    Uint ref;			/* refcount */
} sstring;

static char ss_layout[] = "iti";

typedef struct _scallout_ {
    Uint time;			/* time of call */
    uindex nargs;		/* number of arguments */
    svalue val[4];		/* function name, 3 direct arguments */
} scallout;

static char sco_layout[] = "iu[sui][sui][sui][sui]";

typedef struct {
    Uint time;			/* time of call */
    unsigned short nargs;	/* number of arguments */
    svalue val[4];		/* function name, 3 direct arguments */
} oscallout;

static char osc_layout[] = "is[sui][sui][sui][sui]";

typedef struct {
    arrmerge *amerge;			/* array merge table */
    strmerge *smerge;			/* string merge table */
    Uint narr;				/* # of arrays */
    Uint nstr;				/* # of strings */
    Uint arrsize;			/* # of array elements */
    Uint strsize;			/* total string size */
    sarray *sarrays;			/* save arrays */
    svalue *selts;			/* save array elements */
    sstring *sstrings;			/* save strings */
    char *stext;			/* save string elements */
    Uint *counttab;			/* object count table */
    bool counting;			/* currently counting */
    array alist;			/* linked list sentinel */
} savedata;

static control *chead, *ctail;		/* list of control blocks */
static dataspace *dhead, *dtail;	/* list of dataspace blocks */
static dataspace *gcdata;		/* next dataspace to garbage collect */
static sector nctrl;			/* # control blocks */
static sector ndata;			/* # dataspace blocks */
static bool nilisnot0;			/* nil != int 0 */


/*
 * NAME:	data->init()
 * DESCRIPTION:	initialize swapped data handling
 */
void d_init(flag)
int flag;
{
    chead = ctail = (control *) NULL;
    dhead = dtail = (dataspace *) NULL;
    gcdata = (dataspace *) NULL;
    nctrl = ndata = 0;
    nilisnot0 = flag;
}

/*
 * NAME:	data->new_control()
 * DESCRIPTION:	create a new control block
 */
control *d_new_control()
{
    register control *ctrl;

    ctrl = ALLOC(control, 1);
    if (chead != (control *) NULL) {
	/* insert at beginning of list */
	chead->prev = ctrl;
	ctrl->prev = (control *) NULL;
	ctrl->next = chead;
	chead = ctrl;
    } else {
	/* list was empty */
	ctrl->prev = ctrl->next = (control *) NULL;
	chead = ctail = ctrl;
    }
    ctrl->ndata = 0;
    nctrl++;

    ctrl->flags = 0;

    ctrl->nsectors = 0;		/* nothing on swap device yet */
    ctrl->sectors = (sector *) NULL;
    ctrl->oindex = UINDEX_MAX;
    ctrl->ninherits = 0;
    ctrl->inherits = (dinherit *) NULL;
    ctrl->compiled = 0;
    ctrl->progsize = 0;
    ctrl->prog = (char *) NULL;
    ctrl->nstrings = 0;
    ctrl->strings = (string **) NULL;
    ctrl->sstrings = (dstrconst *) NULL;
    ctrl->stext = (char *) NULL;
    ctrl->nfuncdefs = 0;
    ctrl->funcdefs = (dfuncdef *) NULL;
    ctrl->nvardefs = 0;
    ctrl->nclassvars = 0;
    ctrl->vardefs = (dvardef *) NULL;
    ctrl->cvstrings = (string **) NULL;
    ctrl->classvars = (char *) NULL;
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
static dataspace *d_alloc_dataspace(obj)
object *obj;
{
    register dataspace *data;

    data = ALLOC(dataspace, 1);
    if (dhead != (dataspace *) NULL) {
	/* insert at beginning of list */
	dhead->prev = data;
	data->prev = (dataspace *) NULL;
	data->next = dhead;
	dhead = data;
	data->gcprev = gcdata->gcprev;
	data->gcnext = gcdata;
	data->gcprev->gcnext = data;
	gcdata->gcprev = data;
    } else {
	/* list was empty */
	data->prev = data->next = (dataspace *) NULL;
	dhead = dtail = data;
	gcdata = data;
	data->gcprev = data->gcnext = data;
    }
    ndata++;

    data->iprev = (dataspace *) NULL;
    data->inext = (dataspace *) NULL;
    data->flags = 0;

    data->oindex = obj->index;
    data->ctrl = (control *) NULL;

    /* sectors */
    data->nsectors = 0;
    data->sectors = (sector *) NULL;

    /* variables */
    data->nvariables = 0;
    data->variables = (value *) NULL;
    data->svariables = (svalue *) NULL;

    /* arrays */
    data->narrays = 0;
    data->eltsize = 0;
    data->sarrays = (sarray *) NULL;
    data->selts = (svalue *) NULL;
    data->alist.prev = data->alist.next = &data->alist;

    /* strings */
    data->nstrings = 0;
    data->strsize = 0;
    data->sstrings = (sstring *) NULL;
    data->stext = (char *) NULL;

    /* callouts */
    data->ncallouts = 0;
    data->fcallouts = 0;
    data->callouts = (dcallout *) NULL;
    data->scallouts = (scallout *) NULL;

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
dataspace *d_new_dataspace(obj)
object *obj;
{
    register dataspace *data;

    data = d_alloc_dataspace(obj);
    data->base.flags = MOD_VARIABLE;
    data->ctrl = o_control(obj);
    data->ctrl->ndata++;
    data->nvariables = data->ctrl->nvariables + 1;

    return data;
}

/*
 * NAME:	data->load_control()
 * DESCRIPTION:	load a control block from the swap device
 */
control *d_load_control(obj)
register object *obj;
{
    register control *ctrl;

    ctrl = d_new_control();
    ctrl->oindex = obj->index;

    if (obj->flags & O_COMPILED) {
	/* initialize control block of compiled object */
	pc_control(ctrl, obj);
	ctrl->flags |= CTRL_COMPILED;
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

	ctrl->flags = header.flags;

	/* inherits */
	ctrl->ninherits = UCHAR(header.ninherits);

	if (header.vmapsize != 0) {
	    /*
	     * Control block for outdated issue; only vmap can be loaded.
	     * The load offsets will be invalid (and unused).
	     */
	    ctrl->vmapsize = header.vmapsize;
	    ctrl->vmap = ALLOC(unsigned short, header.vmapsize);
	    sw_readv((char *) ctrl->vmap, ctrl->sectors,
		     header.vmapsize * (Uint) sizeof(unsigned short), size);
	} else {
	    register int n;
	    register dinherit *inherits;
	    register sinherit *sinherits;

	    /* load inherits */
	    n = UCHAR(header.ninherits); /* at least one */
	    ctrl->inherits = inherits = ALLOC(dinherit, n);
	    sinherits = ALLOCA(sinherit, n);
	    sw_readv((char *) sinherits, ctrl->sectors,
		     n * (Uint) sizeof(sinherit), size);
	    size += n * sizeof(sinherit);
	    do {
		inherits->oindex = sinherits->oindex;
		inherits->funcoffset = sinherits->funcoffset;
		inherits->varoffset = sinherits->varoffset & ~PRIV;
		(inherits++)->priv = (((sinherits++)->varoffset & PRIV) != 0);
	    } while (--n > 0);
	    AFREE(sinherits - UCHAR(header.ninherits));
	}

	/* compile time */
	ctrl->compiled = header.compiled;

	/* program */
	ctrl->progoffset = size;
	ctrl->progsize = header.progsize;
	size += header.progsize;

	/* string constants */
	ctrl->stroffset = size;
	ctrl->nstrings = header.nstrings;
	ctrl->strsize = header.strsize;
	size += header.nstrings * (Uint) sizeof(dstrconst) + header.strsize;

	/* function definitions */
	ctrl->funcdoffset = size;
	ctrl->nfuncdefs = UCHAR(header.nfuncdefs);
	size += UCHAR(header.nfuncdefs) * (Uint) sizeof(dfuncdef);

	/* variable definitions */
	ctrl->vardoffset = size;
	ctrl->nvardefs = UCHAR(header.nvardefs);
	ctrl->nclassvars = UCHAR(header.nclassvars);
	size += UCHAR(header.nvardefs) * (Uint) sizeof(dvardef) +
		UCHAR(header.nclassvars) * (Uint) 3;

	/* function call table */
	ctrl->funccoffset = size;
	ctrl->nfuncalls = header.nfuncalls;
	size += header.nfuncalls * (Uint) 2;

	/* symbol table */
	ctrl->symboffset = size;
	ctrl->nsymbols = header.nsymbols;

	/* # variables */
	ctrl->nvariables = header.nvariables;
	ctrl->nifdefs = header.nifdefs;
	ctrl->nvinit = header.nvinit;
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

    data = d_alloc_dataspace(obj);
    data->ctrl = o_control(obj);
    data->ctrl->ndata++;

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

    data->flags = header.flags;

    /* variables */
    data->varoffset = size;
    data->nvariables = header.nvariables;
    size += data->nvariables * (Uint) sizeof(svalue);

    /* arrays */
    data->arroffset = size;
    data->narrays = header.narrays;
    data->eltsize = header.eltsize;
    size += header.narrays * (Uint) sizeof(sarray) +
	    header.eltsize * sizeof(svalue);

    /* strings */
    data->stroffset = size;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    size += header.nstrings * sizeof(sstring) + header.strsize;

    /* callouts */
    data->cooffset = size;
    data->ncallouts = header.ncallouts;
    data->fcallouts = header.fcallouts;

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->u_master)->update &&
	obj->count != 0) {
	d_upgrade_clone(data);
    }

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
	/* move to head of list */
	ctrl->prev->next = ctrl->next;
	if (ctrl->next != (control *) NULL) {
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
	/* move to head of list */
	data->prev->next = data->next;
	if (data->next != (dataspace *) NULL) {
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
 * NAME:	compress()
 * DESCRIPTION:	compress data
 */
static Uint compress(data, text, size)
char *data, *text;
register Uint size;
{
    char htab[16384];
    register unsigned short buf, bufsize, x;
    register char *p, *q;
    register Uint cspace;

    if (size <= 4 + 1) {
	/* can't get smaller than this */
	return 0;
    }

    /* clear the hash table */
    memset(htab, '\0', sizeof(htab));

    buf = bufsize = 0;
    x = 0;
    p = text;
    q = data;
    *q++ = size >> 24;
    *q++ = size >> 16;
    *q++ = size >> 8;
    *q++ = size;
    cspace = size - 4;

    while (size != 0) {
	if (htab[x] == *p) {
	    buf >>= 1;
	    bufsize += 1;
	} else {
	    htab[x] = *p;
	    buf = (buf >> 9) + 0x0080 + (UCHAR(*p) << 8);
	    bufsize += 9;
	}
	x = ((x << 3) & 0x3fff) ^ UCHAR(strhashtab[UCHAR(*p++)]);

	if (bufsize >= 8) {
	    if (bufsize == 16) {
		if ((Int) (cspace-=2) <= 0) {
		    return 0;	/* out of space */
		}
		*q++ = buf;
		*q++ = buf >> 8;
		bufsize = 0;
	    } else {
		if (--cspace == 0) {
		    return 0;	/* out of space */
		}
		*q++ = buf >> (16 - bufsize);
		bufsize -= 8;
	    }
	}

	--size;
    }
    if (bufsize != 0) {
	if (--cspace == 0) {
	    return 0;	/* compression did not reduce size */
	}
	/* add last incomplete byte */
	*q++ = (buf >> (16 - bufsize)) + (0xff << bufsize);
    }

    return (long) q - (long) data;
}

/*
 * NAME:	decompress()
 * DESCRIPTION:	read and decompress data from the swap file
 */
static char *decompress(sectors, readv, size, offset, dsize)
sector *sectors;
void (*readv) P((char*, sector*, Uint, Uint));
Uint size, offset;
Uint *dsize;
{
    char buffer[8192], htab[16384];
    register unsigned short buf, bufsize, x;
    register Uint n;
    register char *p, *q;

    buf = bufsize = 0;
    x = 0;

    /* clear the hash table */
    memset(htab, '\0', sizeof(htab));

    n = sizeof(buffer);
    if (n > size) {
	n = size;
    }
    (*readv)(p = buffer, sectors, n, offset);
    size -= n;
    offset += n;
    *dsize = (UCHAR(p[0]) << 24) | (UCHAR(p[1]) << 16) | (UCHAR(p[2]) << 8) |
	     UCHAR(p[3]);
    q = ALLOC(char, *dsize);
    p += 4;
    n -= 4;

    for (;;) {
	for (;;) {
	    if (bufsize == 0) {
		if (n == 0) {
		    break;
		}
		--n;
		buf = UCHAR(*p++);
		bufsize = 8;
	    }
	    if (buf & 1) {
		if (n == 0) {
		    break;
		}
		--n;
		buf += UCHAR(*p++) << bufsize;

		*q = htab[x] = buf >> 1;
		buf >>= 9;
	    } else {
		*q = htab[x];
		buf >>= 1;
	    }
	    --bufsize;

	    x = ((x << 3) & 0x3fff) ^ UCHAR(strhashtab[UCHAR(*q++)]);
	}

	if (size == 0) {
	    return q - *dsize;
	}
	n = sizeof(buffer);
	if (n > size) {
	    n = size;
	}
	(*readv)(p = buffer, sectors, n, offset);
	size -= n;
	offset += n;
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
	if (ctrl->flags & CTRL_PROGCMP) {
	    ctrl->prog = decompress(ctrl->sectors, sw_readv, ctrl->progsize,
				    ctrl->progoffset, &ctrl->progsize);
	} else {
	    ctrl->prog = ALLOC(char, ctrl->progsize);
	    sw_readv(ctrl->prog, ctrl->sectors, ctrl->progsize,
		     ctrl->progoffset);
	}
    }
    return ctrl->prog;
}

/*
 * NAME:	data->get_stext()
 * DESCRIPTION:	load strings text
 */
static void d_get_stext(ctrl)
register control *ctrl;
{
    /* load strings text */
    if (ctrl->flags & CTRL_STRCMP) {
	ctrl->stext = decompress(ctrl->sectors, sw_readv,
				 ctrl->strsize,
				 ctrl->stroffset +
				 ctrl->nstrings * sizeof(dstrconst),
				 &ctrl->strsize);
    } else {
	ctrl->stext = ALLOC(char, ctrl->strsize);
	sw_readv(ctrl->stext, ctrl->sectors, ctrl->strsize,
		 ctrl->stroffset + ctrl->nstrings * (Uint) sizeof(dstrconst));
    }
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
	ctrl = o_control(OBJR(ctrl->inherits[UCHAR(inherit)].oindex));
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
	    if (ctrl->strsize > 0 && ctrl->stext == (char *) NULL) {
		d_get_stext(ctrl);	/* load strings text */
	    }
	}
    }

    if (ctrl->strings[idx] == (string *) NULL) {
	register string *str;

	str = str_alloc(ctrl->stext + ctrl->sstrings[idx].index,
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
    if (ctrl->vardefs == (dvardef *) NULL && ctrl->nvardefs != 0) {
	ctrl->vardefs = ALLOC(dvardef, ctrl->nvardefs);
	sw_readv((char *) ctrl->vardefs, ctrl->sectors,
		 ctrl->nvardefs * (Uint) sizeof(dvardef), ctrl->vardoffset);
	if (ctrl->nclassvars != 0) {
	    register char *p;
	    register dvardef *vars;
	    register string **strs;
	    register unsigned short n, inherit, u;

	    ctrl->classvars = ALLOC(char, ctrl->nclassvars * 3);
	    sw_readv(ctrl->classvars, ctrl->sectors, ctrl->nclassvars * 3,
		     ctrl->vardoffset + ctrl->nvardefs * sizeof(dvardef));
	    ctrl->cvstrings = strs = ALLOC(string*, ctrl->nvardefs);
	    memset(strs, '\0', ctrl->nvardefs * sizeof(string*));
	    p = ctrl->classvars;
	    for (n = ctrl->nclassvars, vars = ctrl->vardefs; n != 0; vars++) {
		if ((vars->type & T_TYPE) == T_CLASS) {
		    inherit = FETCH1U(p);
		    str_ref(*strs = d_get_strconst(ctrl, inherit,
						   FETCH2U(p, u)));
		    --n;
		}
		strs++;
	    }
	}
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
	ctrl->funcalls = ALLOC(char, 2L * ctrl->nfuncalls);
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
 * NAME:	data->get_progsize()
 * DESCRIPTION:	get the size of a control block
 */
Uint d_get_progsize(ctrl)
register control *ctrl;
{
    if (ctrl->progsize != 0 && ctrl->prog == (char *) NULL &&
	(ctrl->flags & CTRL_PROGCMP)) {
	d_get_prog(ctrl);	/* decompress program */
    }
    if (ctrl->strsize != 0 && ctrl->stext == (char *) NULL &&
	(ctrl->flags & CTRL_STRCMP)) {
	d_get_stext(ctrl);	/* decompress strings */
    }

    return ctrl->ninherits * sizeof(dinherit) +
	   ctrl->progsize +
	   ctrl->nstrings * (Uint) sizeof(dstrconst) +
	   ctrl->strsize +
	   ctrl->nfuncdefs * sizeof(dfuncdef) +
	   ctrl->nvardefs * sizeof(dvardef) +
	   ctrl->nclassvars * (Uint) 3 +
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
	    data->sstrings = ALLOC(sstring, data->nstrings);
	    sw_readv((char *) data->sstrings, data->sectors,
		     data->nstrings * sizeof(sstring), data->stroffset);
	    if (data->strsize > 0) {
		/* load strings text */
		if (data->flags & DATA_STRCMP) {
		    data->stext = decompress(data->sectors, sw_readv,
					     data->strsize,
					     data->stroffset +
					       data->nstrings * sizeof(sstring),
					     &data->strsize);
		} else {
		    data->stext = ALLOC(char, data->strsize);
		    sw_readv(data->stext, data->sectors, data->strsize,
			     data->stroffset +
					    data->nstrings * sizeof(sstring));
		}
	    }
	}

	str = str_alloc(data->stext + data->sstrings[idx].index,
			(long) data->sstrings[idx].len);
	str->ref = 0;
	p = data->plane;

	do {
	    if (p->strings == (strref *) NULL) {
		/* initialize string pointers */
		s = p->strings = ALLOC(strref, data->nstrings);
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
	    data->sarrays = ALLOC(sarray, data->narrays);
	    sw_readv((char *) data->sarrays, data->sectors,
		     data->narrays * (Uint) sizeof(sarray), data->arroffset);
	}

	arr = arr_alloc(data->sarrays[idx].size);
	arr->ref = 0;
	arr->tag = data->sarrays[idx].tag;
	p = data->plane;

	do {
	    if (p->arrays == (arrref *) NULL) {
		/* create array pointers */
		a = p->arrays = ALLOC(arrref, data->narrays);
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
static void d_get_values(data, sv, v, n)
register dataspace *data;
register svalue *sv;
register value *v;
register int n;
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
void d_new_variables(ctrl, variables)
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
		ctrl = o_control(OBJR(inh->oindex));
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
 * NAME:	data->get_variable()
 * DESCRIPTION:	get a variable from the dataspace
 */
value *d_get_variable(data, idx)
register dataspace *data;
register unsigned int idx;
{
    if (data->variables == (value *) NULL) {
	/* create room for variables */
	data->variables = ALLOC(value, data->nvariables);
	if (data->nsectors == 0 && data->svariables == (svalue *) NULL) {
	    /* new datablock */
	    d_new_variables(data->ctrl, data->variables);
	    data->variables[data->nvariables - 1] = nil_value;	/* extra var */
	} else {
	    /*
	     * variables must be loaded from swap
	     */
	    if (data->svariables == (svalue *) NULL) {
		/* load svalues */
		data->svariables = ALLOC(svalue, data->nvariables);
		sw_readv((char *) data->svariables, data->sectors,
			 data->nvariables * (Uint) sizeof(svalue),
			 data->varoffset);
	    }
	    d_get_values(data, data->svariables, data->variables,
			 data->nvariables);
	}
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
    if (v == (value *) NULL && arr->size != 0) {
	register dataspace *data;
	Uint idx;

	data = arr->primary->data;
	if (data->selts == (svalue *) NULL) {
	    /* load array elements */
	    data->selts = (svalue *) ALLOC(svalue, data->eltsize);
	    sw_readv((char *) data->selts, data->sectors,
		     data->eltsize * sizeof(svalue),
		     data->arroffset + data->narrays * sizeof(sarray));
	}
	v = arr->elts = ALLOC(value, arr->size);
	idx = data->sarrays[arr->primary - data->plane->arrays].index;
	d_get_values(data, &data->selts[idx], v, arr->size);
    }

    return v;
}

/*
 * NAME:	data->get_callouts()
 * DESCRIPTION:	load callouts from swap
 */
void d_get_callouts(data)
register dataspace *data;
{
    register scallout *sco;
    register dcallout *co;
    register uindex n;

    if (data->scallouts == (scallout *) NULL) {
	data->scallouts = ALLOC(scallout, data->ncallouts);
	sw_readv((char *) data->scallouts, data->sectors,
		 data->ncallouts * (Uint) sizeof(scallout), data->cooffset);
    }
    sco = data->scallouts;
    co = data->callouts = ALLOC(dcallout, data->ncallouts);

    for (n = data->ncallouts; n > 0; --n) {
	co->time = sco->time;
	co->nargs = sco->nargs;
	if (sco->val[0].type == T_STRING) {
	    d_get_values(data, sco->val, co->val,
			 (sco->nargs > 3) ? 4 : sco->nargs + 1);
	} else {
	    co->val[0] = nil_value;
	}
	sco++;
	co++;
    }
}


/*
 * NAME:	data->swapalloc()
 * DESCRIPTION:	allocate swapspace for something
 */
static sector d_swapalloc(size, nsectors, sectors)
Uint size;
register sector nsectors, **sectors;
{
    register sector n, *s;

    s = *sectors;
    if (nsectors != 0) {
	/* wipe old sectors */
	sw_wipev(s, nsectors);
    }

    n = sw_mapsize(size);
    if (nsectors > n) {
	/* too many sectors */
	sw_delv(s + n, nsectors - n);
    }

    s = *sectors = REALLOC(*sectors, sector, nsectors, n);
    if (nsectors < n) {
	/* not enough sectors */
	sw_newv(s + nsectors, n - nsectors);
    }

    return n;
}

/*
 * NAME:	data->save_control()
 * DESCRIPTION:	save the control block
 */
static void d_save_control(ctrl)
register control *ctrl;
{
    scontrol header;
    char *prog, *stext, *text;
    dstrconst *sstrings;
    register Uint size, i;
    register sinherit *sinherits;
    register dinherit *inherits;

    /*
     * Save a control block.
     */

    /* create header */
    header.flags = ctrl->flags & CTRL_UNDEFINED;
    header.ninherits = ctrl->ninherits;
    header.compiled = ctrl->compiled;
    header.progsize = ctrl->progsize;
    header.nstrings = ctrl->nstrings;
    header.strsize = ctrl->strsize;
    header.nfuncdefs = ctrl->nfuncdefs;
    header.nvardefs = ctrl->nvardefs;
    header.nclassvars = ctrl->nclassvars;
    header.nfuncalls = ctrl->nfuncalls;
    header.nsymbols = ctrl->nsymbols;
    header.nvariables = ctrl->nvariables;
    header.nifdefs = ctrl->nifdefs;
    header.nvinit = ctrl->nvinit;
    header.vmapsize = ctrl->vmapsize;

    /* create sector space */
    if (header.vmapsize != 0) {
	size = sizeof(scontrol) +
	       header.vmapsize * (Uint) sizeof(unsigned short);
    } else {
	prog = ctrl->prog;
	if (header.progsize >= CMPLIMIT) {
	    prog = ALLOCA(char, header.progsize);
	    size = compress(prog, ctrl->prog, header.progsize);
	    if (size != 0) {
		header.flags |= CMP_PRED;
		header.progsize = size;
	    } else {
		AFREE(prog);
		prog = ctrl->prog;
	    }
	}

	sstrings = ctrl->sstrings;
	stext = ctrl->stext;
	if (header.nstrings > 0 && sstrings == (dstrconst *) NULL) {
	    register string **strs;
	    register Uint strsize;
	    register dstrconst *s;
	    register char *t;

	    sstrings = ALLOCA(dstrconst, header.nstrings);
	    if (header.strsize > 0) {
		stext = ALLOCA(char, header.strsize);
	    }

	    strs = ctrl->strings;
	    strsize = 0;
	    s = sstrings;
	    t = stext;
	    for (i = header.nstrings; i > 0; --i) {
		s->index = strsize;
		strsize += s->len = (*strs)->len;
		memcpy(t, (*strs++)->text, s->len);
		t += (s++)->len;
	    }
	}

	text = stext;
	if (header.strsize >= CMPLIMIT) {
	    text = ALLOCA(char, header.strsize);
	    size = compress(text, stext, header.strsize);
	    if (size != 0) {
		header.flags |= CMP_PRED << 2;
		header.strsize = size;
	    } else {
		AFREE(text);
		text = stext;
	    }
	}

	size = sizeof(scontrol) +
	       UCHAR(header.ninherits) * sizeof(sinherit) +
	       header.progsize +
	       header.nstrings * (Uint) sizeof(dstrconst) +
	       header.strsize +
	       UCHAR(header.nfuncdefs) * sizeof(dfuncdef) +
	       UCHAR(header.nvardefs) * sizeof(dvardef) +
	       UCHAR(header.nclassvars) * (Uint) 3 +
	       header.nfuncalls * (Uint) 2 +
	       header.nsymbols * (Uint) sizeof(dsymbol);
    }
    ctrl->nsectors = header.nsectors = d_swapalloc(size, ctrl->nsectors,
						   &ctrl->sectors);
    OBJ(ctrl->oindex)->cfirst = ctrl->sectors[0];

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

    if (header.vmapsize != 0) {
	/*
	 * save only vmap
	 */
	sw_writev((char *) ctrl->vmap, ctrl->sectors,
		  header.vmapsize * (Uint) sizeof(unsigned short), size);
    } else {
	/* save inherits */
	inherits = ctrl->inherits;
	sinherits = ALLOCA(sinherit, i = UCHAR(header.ninherits));
	do {
	    sinherits->oindex = inherits->oindex;
	    sinherits->funcoffset = inherits->funcoffset;
	    sinherits->varoffset = inherits->varoffset;
	    if (inherits->priv) {
		sinherits->varoffset |= PRIV;
	    }
	    inherits++;
	    sinherits++;
	} while (--i > 0);
	sinherits -= UCHAR(header.ninherits);
	sw_writev((char *) sinherits, ctrl->sectors,
		  UCHAR(header.ninherits) * (Uint) sizeof(sinherit), size);
	size += UCHAR(header.ninherits) * sizeof(sinherit);
	AFREE(sinherits);

	/* save program */
	if (header.progsize > 0) {
	    sw_writev(prog, ctrl->sectors, (Uint) header.progsize, size);
	    size += header.progsize;
	    if (prog != ctrl->prog) {
		AFREE(prog);
	    }
	}

	/* save string constants */
	if (header.nstrings > 0) {
	    sw_writev((char *) sstrings, ctrl->sectors,
		      header.nstrings * (Uint) sizeof(dstrconst), size);
	    size += header.nstrings * (Uint) sizeof(dstrconst);
	    if (header.strsize > 0) {
		sw_writev(text, ctrl->sectors, header.strsize, size);
		size += header.strsize;
		if (text != stext) {
		    AFREE(text);
		}
		if (stext != ctrl->stext) {
		    AFREE(stext);
		}
	    }
	    if (sstrings != ctrl->sstrings) {
		AFREE(sstrings);
	    }
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
	    if (UCHAR(header.nclassvars) > 0) {
		sw_writev(ctrl->classvars, ctrl->sectors,
			  UCHAR(header.nclassvars) * (Uint) 3, size);
		size += UCHAR(header.nclassvars) * (Uint) 3;
	    }
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
}


static void d_count P((savedata*, value*, unsigned int));

/*
 * NAME:	data->arrcount()
 * DESCRIPTION:	count the number of arrays and strings in an array
 */
static void d_arrcount(register savedata *save, register array *arr)
{
    arr->prev->next = arr->next;
    arr->next->prev = arr->prev;
    arr->prev = &save->alist;
    arr->next = save->alist.next;
    arr->next->prev = arr;
    save->alist.next = arr;
    save->narr++;

    if (!save->counting) {
	save->counting = TRUE;
	do {
	    save->arrsize += arr->size;
	    d_count(save, d_get_elts(arr), arr->size);
	    arr = arr->prev;
	} while (arr != &save->alist);
	save->counting = FALSE;
    }
}

/*
 * NAME:	data->count()
 * DESCRIPTION:	count the number of arrays and strings in an object
 */
static void d_count(save, v, n)
register savedata *save;
register value *v;
register unsigned int n;
{
    register object *obj;
    register value *elts;
    Uint count;

    while (n > 0) {
	switch (v->type) {
	case T_STRING:
	    if (str_put(save->smerge, v->u.string, save->nstr) == save->nstr) {
		save->nstr++;
		save->strsize += v->u.string->len;
	    }
	    break;

	case T_ARRAY:
	    if (arr_put(save->amerge, v->u.array, save->narr) == save->narr) {
		d_arrcount(save, v->u.array);
	    }
	    break;

	case T_MAPPING:
	    if (arr_put(save->amerge, v->u.array, save->narr) == save->narr) {
		if (v->u.array->hashmod) {
		    map_compact(v->u.array->primary->data, v->u.array);
		}
		d_arrcount(save, v->u.array);
	    }
	    break;

	case T_LWOBJECT:
	    elts = d_get_elts(v->u.array);
	    obj = OBJ(elts->oindex);
	    if (save->counttab != (Uint *) NULL) {
		count = save->counttab[elts->oindex];
	    } else {
		count = obj->count;
	    }
	    if (elts[1].type == T_INT) {
		/* convert to new LWO type */
		elts[1].type = T_FLOAT;
		elts[1].oindex = FALSE;
	    }
	    if (arr_put(save->amerge, v->u.array, save->narr) == save->narr) {
		if (elts->u.objcnt == count && elts[1].u.objcnt != obj->update)
		{
		    d_upgrade_lwobj(v->u.array, obj);
		    elts = v->u.array->elts;
		}
		if (save->counttab != (Uint *) NULL) {
		    elts[1].u.objcnt = 0;
		}
		d_arrcount(save, v->u.array);
	    }
	    break;
	}

	v++;
	--n;
    }
}

/*
 * NAME:	data->save()
 * DESCRIPTION:	recursively save the values in an object
 */
static void d_save(save, sv, v, n)
register savedata *save;
register svalue *sv;
register value *v;
register unsigned short n;
{
    register Uint i;

    while (n > 0) {
	switch (sv->type = v->type) {
	case T_NIL:
	    sv->oindex = 0;
	    sv->u.number = 0;
	    break;

	case T_INT:
	    sv->oindex = 0;
	    sv->u.number = v->u.number;
	    break;

	case T_STRING:
	    i = str_put(save->smerge, v->u.string, save->nstr);
	    sv->oindex = 0;
	    sv->u.string = i;
	    if (save->sstrings[i].ref++ == 0) {
		/* new string value */
		save->sstrings[i].index = save->strsize;
		save->sstrings[i].len = v->u.string->len;
		memcpy(save->stext + save->strsize, v->u.string->text,
		       v->u.string->len);
		save->strsize += v->u.string->len;
	    }
	    break;

	case T_FLOAT:
	case T_OBJECT:
	    sv->oindex = v->oindex;
	    sv->u.objcnt = v->u.objcnt;
	    break;

	case T_ARRAY:
	case T_MAPPING:
	case T_LWOBJECT:
	    i = arr_put(save->amerge, v->u.array, save->narr);
	    sv->oindex = 0;
	    sv->u.array = i;
	    save->sarrays[i].ref++;
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
static void d_put_values(data, sv, v, n)
register dataspace *data;
register svalue *sv;
register value *v;
register unsigned short n;
{
    while (n > 0) {
	if (v->modified) {
	    switch (sv->type = v->type) {
	    case T_NIL:
		sv->oindex = 0;
		sv->u.number = 0;
		break;

	    case T_INT:
		sv->oindex = 0;
		sv->u.number = v->u.number;
		break;

	    case T_STRING:
		sv->oindex = 0;
		sv->u.string = v->u.string->primary - data->base.strings;
		break;

	    case T_FLOAT:
	    case T_OBJECT:
		sv->oindex = v->oindex;
		sv->u.objcnt = v->u.objcnt;
		break;

	    case T_ARRAY:
	    case T_MAPPING:
	    case T_LWOBJECT:
		sv->oindex = 0;
		sv->u.array = v->u.array->primary - data->base.arrays;
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

    /* free parse_string data */
    if (data->parser != (struct _parser_ *) NULL) {
	ps_del(data->parser);
	data->parser = (struct _parser_ *) NULL;
    }

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
	    if (v->type == T_STRING) {
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
    if (data->base.arrays != (arrref *) NULL) {
	register arrref *a;

	for (i = data->narrays, a = data->base.arrays; i > 0; --i, a++) {
	    if (a->arr != (array *) NULL) {
		arr_del(a->arr);
	    }
	}

	FREE(data->base.arrays);
	data->base.arrays = (arrref *) NULL;
    }

    /* free strings */
    if (data->base.strings != (strref *) NULL) {
	register strref *s;

	for (i = data->nstrings, s = data->base.strings; i > 0; --i, s++) {
	    if (s->str != (string *) NULL) {
		s->str->primary = (strref *) NULL;
		str_del(s->str);
	    }
	}

	FREE(data->base.strings);
	data->base.strings = (strref *) NULL;
    }

    /* free any left-over arrays */
    if (data->alist.next != &data->alist) {
	data->alist.prev->next = data->alist.next;
	data->alist.next->prev = data->alist.prev;
	arr_freelist(data->alist.next);
	data->alist.prev = data->alist.next = &data->alist;
    }
}

/*
 * NAME:	data->save_dataspace()
 * DESCRIPTION:	save all values in a dataspace block
 */
static bool d_save_dataspace(data, swap, counttab)
register dataspace *data;
bool swap;
Uint *counttab;
{
    sdataspace header;
    register Uint n;

    if (data->parser != (struct _parser_ *) NULL) {
	ps_save(data->parser);
    }
    if (data->base.flags == 0) {
	return FALSE;
    }

    if ((data->nsectors != 0 || !swap) && data->base.achange == 0 &&
	data->base.schange == 0 && !(data->base.flags & MOD_NEWCALLOUT)) {
	bool mod;

	/*
	 * No strings/arrays added or deleted. Check individual variables and
	 * array elements.
	 */
	if (data->base.flags & MOD_VARIABLE) {
	    /*
	     * variables changed
	     */
	    d_put_values(data, data->svariables, data->variables,
			 data->nvariables);
	    if (swap) {
		sw_writev((char *) data->svariables, data->sectors,
			  data->nvariables * (Uint) sizeof(svalue),
			  data->varoffset);
	    }
	}
	if (data->base.flags & MOD_ARRAYREF) {
	    register sarray *sa;
	    register arrref *a;

	    /*
	     * references to arrays changed
	     */
	    sa = data->sarrays;
	    a = data->base.arrays;
	    mod = FALSE;
	    for (n = data->narrays; n > 0; --n) {
		if (a->arr != (array *) NULL && sa->ref != (a->ref & ~ARR_MOD))
		{
		    sa->ref = a->ref & ~ARR_MOD;
		    mod = TRUE;
		}
		sa++;
		a++;
	    }
	    if (mod && swap) {
		sw_writev((char *) data->sarrays, data->sectors,
			  data->narrays * sizeof(sarray), data->arroffset);
	    }
	}
	if (data->base.flags & MOD_ARRAY) {
	    register arrref *a;
	    Uint idx;

	    /*
	     * array elements changed
	     */
	    a = data->base.arrays;
	    for (n = 0; n < data->narrays; n++) {
		if (a->arr != (array *) NULL && (a->ref & ARR_MOD)) {
		    a->ref &= ~ARR_MOD;
		    idx = data->sarrays[n].index;
		    d_put_values(data, &data->selts[idx], a->arr->elts,
				 a->arr->size);
		    if (swap) {
			sw_writev((char *) &data->selts[idx], data->sectors,
				  a->arr->size * (Uint) sizeof(svalue),
				  data->arroffset +
					      data->narrays * sizeof(sarray) +
					      idx * sizeof(svalue));
		    }
		}
		a++;
	    }
	}
	if (data->base.flags & MOD_STRINGREF) {
	    register sstring *ss;
	    register strref *s;

	    /*
	     * string references changed
	     */
	    ss = data->sstrings;
	    s = data->base.strings;
	    mod = FALSE;
	    for (n = data->nstrings; n > 0; --n) {
		if (s->str != (string *) NULL && ss->ref != s->ref) {
		    ss->ref = s->ref;
		    mod = TRUE;
		}
		ss++;
		s++;
	    }
	    if (mod && swap) {
		sw_writev((char *) data->sstrings, data->sectors,
			  data->nstrings * sizeof(sstring),
			  data->stroffset);
	    }
	}
	if (data->base.flags & MOD_CALLOUT) {
	    register scallout *sco;
	    register dcallout *co;

	    sco = data->scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->nargs = co->nargs;
		if (co->val[0].type == T_STRING) {
		    co->val[0].modified = TRUE;
		    co->val[1].modified = TRUE;
		    co->val[2].modified = TRUE;
		    co->val[3].modified = TRUE;
		    d_put_values(data, sco->val, co->val,
				 (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
		}
		sco++;
		co++;
	    }

	    if (swap) {
		/* save new (?) fcallouts value */
		sw_writev((char *) &data->fcallouts, data->sectors,
			  (Uint) sizeof(uindex),
			  (Uint) ((char *)&header.fcallouts - (char *)&header));

		/* save scallouts */
		sw_writev((char *) data->scallouts, data->sectors,
			  data->ncallouts * (Uint) sizeof(scallout),
			  data->cooffset);
	    }
	}
    } else {
	savedata save;
	char *text;
	register Uint size;
	register array *arr;
	register sarray *sarr;

	/*
	 * count the number and sizes of strings and arrays
	 */
	save.amerge = arr_merge();
	save.smerge = str_merge();
	save.narr = 0;
	save.nstr = 0;
	save.arrsize = 0;
	save.strsize = 0;
	save.counttab = counttab;
	save.counting = FALSE;
	save.alist.prev = save.alist.next = &save.alist;

	d_get_variable(data, 0);
	if (data->svariables == (svalue *) NULL) {
	    data->svariables = ALLOC(svalue, data->nvariables);
	}
	d_count(&save, data->variables, data->nvariables);

	if (data->ncallouts > 0) {
	    register dcallout *co;

	    if (data->callouts == (dcallout *) NULL) {
		d_get_callouts(data);
	    }
	    /* remove empty callouts at the end */
	    for (n = data->ncallouts, co = data->callouts + n; n > 0; --n) {
		if ((--co)->val[0].type == T_STRING) {
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
		for (co = data->callouts; n > 0; --n, co++) {
		    if (co->val[0].type == T_STRING) {
			d_count(&save, co->val,
				(co->nargs > 3) ? 4 : co->nargs + 1);
		    }
		}
	    }
	}

	/* fill in header */
	header.flags = 0;
	header.nvariables = data->nvariables;
	header.narrays = save.narr;
	header.eltsize = save.arrsize;
	header.nstrings = save.nstr;
	header.strsize = save.strsize;
	header.ncallouts = data->ncallouts;
	header.fcallouts = data->fcallouts;

	/*
	 * put everything in a saveable form
	 */
	save.sstrings = data->sstrings =
			REALLOC(data->sstrings, sstring, 0, header.nstrings);
	memset(save.sstrings, '\0', save.nstr * sizeof(sstring));
	save.stext = data->stext =
		     REALLOC(data->stext, char, 0, header.strsize);
	save.sarrays = data->sarrays =
		       REALLOC(data->sarrays, sarray, 0, header.narrays);
	memset(save.sarrays, '\0', save.narr * sizeof(sarray));
	save.selts = data->selts =
		     REALLOC(data->selts, svalue, 0, header.eltsize);
	save.narr = 0;
	save.nstr = 0;
	save.arrsize = 0;
	save.strsize = 0;
	data->scallouts = REALLOC(data->scallouts, scallout, 0,
				  header.ncallouts);

	d_save(&save, data->svariables, data->variables, data->nvariables);
	if (header.ncallouts > 0) {
	    register scallout *sco;
	    register dcallout *co;

	    sco = data->scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->nargs = co->nargs;
		if (co->val[0].type == T_STRING) {
		    d_save(&save, sco->val, co->val,
			   (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
		}
		sco++;
		co++;
	    }
	}
	for (arr = save.alist.prev, sarr = save.sarrays; arr != &save.alist;
	     arr = arr->prev, sarr++) {
	    sarr->index = save.arrsize;
	    sarr->size = arr->size;
	    sarr->tag = arr->tag;
	    d_save(&save, save.selts + save.arrsize, arr->elts, arr->size);
	    save.arrsize += arr->size;
	}
	if (arr->next != &save.alist) {
	    data->alist.next->prev = arr->prev;
	    arr->prev->next = data->alist.next;
	    data->alist.next = arr->next;
	    arr->next->prev = &data->alist;
	}

	/* clear merge tables */
	arr_clear(save.amerge);
	str_clear(save.smerge);

	if (swap) {
	    text = save.stext;
	    if (header.strsize >= CMPLIMIT) {
		text = ALLOCA(char, header.strsize);
		size = compress(text, save.stext, header.strsize);
		if (size != 0) {
		    header.flags |= CMP_PRED;
		    header.strsize = size;
		} else {
		    AFREE(text);
		    text = save.stext;
		}
	    }

	    /* create sector space */
	    size = sizeof(sdataspace) +
		   (header.nvariables + header.eltsize) * sizeof(svalue) +
		   header.narrays * sizeof(sarray) +
		   header.nstrings * sizeof(sstring) +
		   header.strsize +
		   header.ncallouts * (Uint) sizeof(scallout);
	    header.nsectors = d_swapalloc(size, data->nsectors, &data->sectors);
	    data->nsectors = header.nsectors;
	    OBJ(data->oindex)->dfirst = data->sectors[0];

	    /* save header */
	    size = sizeof(sdataspace);
	    sw_writev((char *) &header, data->sectors, size, (Uint) 0);
	    sw_writev((char *) data->sectors, data->sectors,
		      header.nsectors * (Uint) sizeof(sector), size);
	    size += header.nsectors * (Uint) sizeof(sector);

	    /* save variables */
	    data->varoffset = size;
	    sw_writev((char *) data->svariables, data->sectors,
		      data->nvariables * (Uint) sizeof(svalue), size);
	    size += data->nvariables * (Uint) sizeof(svalue);

	    /* save arrays */
	    data->arroffset = size;
	    if (header.narrays > 0) {
		sw_writev((char *) save.sarrays, data->sectors,
			  header.narrays * sizeof(sarray), size);
		size += header.narrays * sizeof(sarray);
		if (header.eltsize > 0) {
		    sw_writev((char *) save.selts, data->sectors,
			      header.eltsize * sizeof(svalue), size);
		    size += header.eltsize * sizeof(svalue);
		}
	    }

	    /* save strings */
	    data->stroffset = size;
	    if (header.nstrings > 0) {
		sw_writev((char *) save.sstrings, data->sectors,
			  header.nstrings * sizeof(sstring), size);
		size += header.nstrings * sizeof(sstring);
		if (header.strsize > 0) {
		    sw_writev(text, data->sectors, header.strsize, size);
		    size += header.strsize;
		    if (text != save.stext) {
			AFREE(text);
		    }
		}
	    }

	    /* save callouts */
	    data->cooffset = size;
	    if (header.ncallouts > 0) {
		sw_writev((char *) data->scallouts, data->sectors,
			  header.ncallouts * (Uint) sizeof(scallout), size);
	    }
	}

	d_free_values(data);

	data->flags = header.flags;
	data->narrays = header.narrays;
	data->eltsize = header.eltsize;
	data->nstrings = header.nstrings;
	data->strsize = save.strsize;

	data->base.schange = 0;
	data->base.achange = 0;
    }

    data->base.flags = 0;
    return TRUE;
}


/*
 * NAME:	data->swapout()
 * DESCRIPTION:	Swap out a portion of the control and dataspace blocks in
 *		memory.  Return the number of dataspace blocks swapped out.
 */
sector d_swapout(frag)
unsigned int frag;
{
    register sector n, count;
    register dataspace *data;
    register control *ctrl;

    count = 0;
    n = ndata;

    /* perform garbage collection for one dataspace */
    if (gcdata != (dataspace *) NULL && (frag == 0 || n - 1 >= frag)) {
	if (d_save_dataspace(gcdata, (frag != 0), (Uint *) NULL)) {
	    count++;
	    if (frag != 1) {
		--n;
	    }
	}
	gcdata = gcdata->gcnext;
    }

    if (frag != 0) {
	/* swap out dataspace blocks */
	data = dtail;
	for (n /= frag; n > 0; --n) {
	    register dataspace *prev;

	    prev = data->prev;
	    if (d_save_dataspace(data, TRUE, (Uint *) NULL)) {
		count++;
	    }
	    OBJ(data->oindex)->data = (dataspace *) NULL;
	    d_free_dataspace(data);
	    data = prev;
	}

	/* swap out control blocks */
	ctrl = ctail;
	for (n = nctrl / frag; n > 0; --n) {
	    register control *prev;

	    prev = ctrl->prev;
	    if (ctrl->ndata == 0) {
		if ((ctrl->sectors == (sector *) NULL &&
		     !(ctrl->flags & CTRL_COMPILED)) ||
		    (ctrl->flags & CTRL_VARMAP)) {
		    d_save_control(ctrl);
		}
		OBJ(ctrl->oindex)->ctrl = (control *) NULL;
		d_free_control(ctrl);
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
void d_swapsync()
{
    register control *ctrl;
    register dataspace *data;

    /* save control blocks */
    for (ctrl = ctail; ctrl != (control *) NULL; ctrl = ctrl->prev) {
	if ((ctrl->sectors == (sector *) NULL &&
	     !(ctrl->flags & CTRL_COMPILED)) || (ctrl->flags & CTRL_VARMAP)) {
	    d_save_control(ctrl);
	}
    }

    /* save dataspace blocks */
    for (data = dtail; data != (dataspace *) NULL; data = data->prev) {
	d_save_dataspace(data, TRUE, (Uint *) NULL);
    }
}

/*
 * NAME:	data->upgrade_mem()
 * DESCRIPTION:	upgrade all obj and all objects cloned from obj that have
 *		dataspaces in memory
 */
void d_upgrade_mem(tmpl, new)
register object *tmpl, *new;
{
    register dataspace *data;
    register unsigned int nvar;
    register unsigned short *vmap;
    register object *obj;

    nvar = tmpl->ctrl->vmapsize;
    vmap = tmpl->ctrl->vmap;

    for (data = dtail; data != (dataspace *) NULL; data = data->prev) {
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
 * NAME:	data->conv()
 * DESCRIPTION:	convert something from the dump file
 */
static Uint d_conv(m, vec, layout, n, idx)
char *m, *layout;
sector *vec;
Uint n, idx;
{
    Uint bufsize;
    char *buf;

    bufsize = (conf_dsize(layout) & 0xff) * n;
    buf = ALLOCA(char, bufsize);
    sw_dreadv(buf, vec, bufsize, idx);
    conf_dconv(m, buf, layout, n);
    AFREE(buf);

    return bufsize;
}

/*
 * NAME:	data->conv_proto()
 * DESCRIPTION:	convert a prototype to the new standard
 */
static void d_conv_proto(old, new)
char **old, **new;
{
    register char *p, *args;
    register unsigned short i, n, type;
    unsigned short class, nargs, vargs;
    bool varargs;

    p = *old;
    class = UCHAR(*p++);
    type = UCHAR(*p++);
    n = UCHAR(*p++);

    varargs = (class & C_VARARGS);
    class &= ~C_VARARGS;
    nargs = vargs = 0;
    args = &PROTO_FTYPE(*new);
    *args++ = (type & T_TYPE) | ((type >> 1) & T_REF);

    for (i = 0; i < n; i++) {
	if (varargs) {
	    vargs++;
	} else {
	    nargs++;
	}
	type = UCHAR(*p++);
	if (type & T_VARARGS) {
	    if (i == n - 1) {
		class |= C_ELLIPSIS;
		if (!varargs) {
		    --nargs;
		    vargs++;
		}
	    }
	    varargs = TRUE;
	}
	*args++ = (type & T_TYPE) | ((type >> 1) & T_REF);
    }

    *old = p;
    p = *new;
    *new = args;

    PROTO_CLASS(p) = class;
    PROTO_NARGS(p) = nargs;
    PROTO_VARGS(p) = vargs;
    PROTO_HSIZE(p) = (6 + n) >> 8;
    PROTO_LSIZE(p) = 6 + n;
}

/*
 * NAME:	data->conv_control()
 * DESCRIPTION:	convert control block
 */
void d_conv_control(oindex, conv)
unsigned int oindex;
int conv;
{
    scontrol header;
    register control *ctrl;
    register Uint size;
    register sector *s;
    register unsigned int n;
    object *obj;

    ctrl = d_new_control();
    ctrl->oindex = oindex;
    obj = OBJ(oindex);

    /*
     * restore from dump file
     */
    if (conv) {
	oscontrol oheader;

	size = d_conv((char *) &oheader, &obj->cfirst, os_layout, (Uint) 1,
		      (Uint) 0);
	header.nsectors = oheader.nsectors;
	header.flags = oheader.flags;
	header.ninherits = oheader.ninherits;
	header.compiled = oheader.compiled;
	header.progsize = oheader.progsize;
	header.nstrings = oheader.nstrings;
	header.strsize = oheader.strsize;
	header.nfuncdefs = oheader.nfuncdefs;
	header.nvardefs = oheader.nvardefs;
	header.nclassvars = 0;
	header.nfuncalls = oheader.nfuncalls;
	header.nsymbols = oheader.nsymbols;
	header.nvariables = oheader.nvariables;
	header.nifdefs = oheader.nifdefs;
	header.nvinit = oheader.nvinit;
	header.vmapsize = oheader.vmapsize;
    } else {
	size = d_conv((char *) &header, &obj->cfirst, sc_layout, (Uint) 1,
		      (Uint) 0);
    }
    if (header.nvariables >= PRIV) {
	fatal("too many variables in restored object");
    }
    ctrl->ninherits = UCHAR(header.ninherits);
    ctrl->compiled = header.compiled;
    ctrl->progsize = header.progsize;
    ctrl->nstrings = header.nstrings;
    ctrl->strsize = header.strsize;
    ctrl->nfuncdefs = UCHAR(header.nfuncdefs);
    ctrl->nvardefs = UCHAR(header.nvardefs);
    ctrl->nclassvars = UCHAR(header.nclassvars);
    ctrl->nfuncalls = header.nfuncalls;
    ctrl->nsymbols = header.nsymbols;
    ctrl->nvariables = header.nvariables;
    ctrl->nifdefs = header.nifdefs;
    ctrl->nvinit = header.nvinit;
    ctrl->vmapsize = header.vmapsize;

    /* sectors */
    s = ALLOCA(sector, header.nsectors);
    s[0] = obj->cfirst;
    for (n = 0; n < header.nsectors; n++) {
	size += d_conv((char *) (s + n), s, "d", (Uint) 1, size);
    }

    if (header.vmapsize != 0) {
	/* only vmap */
	ctrl->vmap = ALLOC(unsigned short, header.vmapsize);
	d_conv((char *) ctrl->vmap, s, "s", (Uint) header.vmapsize, size);
    } else {
	register dinherit *inherits;
	register sinherit *sinherits;

	/* inherits */
	n = UCHAR(header.ninherits); /* at least one */
	ctrl->inherits = inherits = ALLOC(dinherit, n);
	sinherits = ALLOCA(sinherit, n);
	size += d_conv((char *) sinherits, s, si_layout, (Uint) n, size);
	do {
	    inherits->oindex = sinherits->oindex;
	    inherits->funcoffset = sinherits->funcoffset;
	    inherits->varoffset = sinherits->varoffset & ~PRIV;
	    (inherits++)->priv = (((sinherits++)->varoffset & PRIV) != 0);
	} while (--n > 0);
	AFREE(sinherits - UCHAR(header.ninherits));

	if (header.progsize != 0) {
	    /* program */
	    if (header.flags & CMP_TYPE) {
		ctrl->prog = decompress(s, sw_dreadv, header.progsize, size,
					&ctrl->progsize);
	    } else {
		ctrl->prog = ALLOC(char, header.progsize);
		sw_dreadv(ctrl->prog, s, header.progsize, size);
	    }
	    size += header.progsize;
	}

	if (header.nstrings != 0) {
	    /* strings */
	    ctrl->sstrings = ALLOC(dstrconst, header.nstrings);
	    size += d_conv((char *) ctrl->sstrings, s, DSTR_LAYOUT,
			   (Uint) header.nstrings, size);
	    if (header.strsize != 0) {
		if (header.flags & (CMP_TYPE << 2)) {
		    ctrl->stext = decompress(s, sw_dreadv, header.strsize, size,
					     &ctrl->strsize);
		} else {
		    ctrl->stext = ALLOC(char, header.strsize);
		    sw_dreadv(ctrl->stext, s, header.strsize, size);
		}
		size += header.strsize;
	    }
	}

	if (header.nfuncdefs != 0) {
	    /* function definitions */
	    ctrl->funcdefs = ALLOC(dfuncdef, UCHAR(header.nfuncdefs));
	    size += d_conv((char *) ctrl->funcdefs, s, DF_LAYOUT,
			   (Uint) UCHAR(header.nfuncdefs), size);
	    if (conv) {
		char *prog, *old, *new;
		Uint offset, funcsize;

		/*
		 * convert restored program to new prototype standard
		 */
		prog = ALLOC(char, ctrl->progsize + (Uint) 3 * ctrl->nfuncdefs);
		new = prog;
		for (n = 0; n < ctrl->nfuncdefs; n++) {
		    /* convert prototype */
		    old = ctrl->prog + ctrl->funcdefs[n].offset;
		    ctrl->funcdefs[n].offset = new - prog;
		    d_conv_proto(&old, &new);

		    /* copy program */
		    offset = old - ctrl->prog;
		    if (n < ctrl->nfuncdefs - 1) {
			funcsize = ctrl->funcdefs[n + 1].offset - offset;
		    } else {
			funcsize = ctrl->progsize - offset;
		    }
		    if (funcsize != 0) {
			memcpy(new, old, funcsize);
			new += funcsize;
		    }
		}

		/* replace program */
		FREE(ctrl->prog);
		ctrl->prog = prog;
		ctrl->progsize = new - prog;
	    }
	}

	if (header.nvardefs != 0) {
	    /* variable definitions */
	    ctrl->vardefs = ALLOC(dvardef, UCHAR(header.nvardefs));
	    size += d_conv((char *) ctrl->vardefs, s, DV_LAYOUT,
			   (Uint) UCHAR(header.nvardefs), size);
	    if (conv) {
		register unsigned short type;

		for (n = 0; n < ctrl->nvardefs; n++) {
		    type = ctrl->vardefs[n].type;
		    ctrl->vardefs[n].type =
					(type & T_TYPE) | ((type >> 1) & T_REF);
		}
	    } else if (ctrl->nclassvars != 0) {
		register char *p;
		register dvardef *vars;
		register string **strs;
		register unsigned short n, inherit, u;

		ctrl->classvars = ALLOC(char, ctrl->nclassvars * 3);
		sw_dreadv(ctrl->classvars, s, ctrl->nclassvars * (Uint) 3,
			  size);
		size += ctrl->nclassvars * (Uint) 3;
		ctrl->cvstrings = strs = ALLOC(string*, ctrl->nvardefs);
		memset(strs, '\0', ctrl->nvardefs * sizeof(string*));
		p = ctrl->classvars;
		for (n = ctrl->nclassvars, vars = ctrl->vardefs; n != 0; vars++)
		{
		    if ((vars->type & T_TYPE) == T_CLASS) {
			inherit = FETCH1U(p);
			str_ref(*strs = d_get_strconst(ctrl, inherit,
						       FETCH2U(p, u)));
			--n;
		    }
		    strs++;
		}
	    }
	}

	if (header.nfuncalls != 0) {
	    /* function calls */
	    ctrl->funcalls = ALLOC(char, 2 * header.nfuncalls);
	    sw_dreadv(ctrl->funcalls, s, header.nfuncalls * (Uint) 2, size);
	    size += header.nfuncalls * (Uint) 2;
	}

	if (header.nsymbols != 0) {
	    /* symbol table */
	    ctrl->symbols = ALLOC(dsymbol, header.nsymbols);
	    d_conv((char *) ctrl->symbols, s, DSYM_LAYOUT,
		   (Uint) header.nsymbols, size);
	}
    }

    AFREE(s);

    d_save_control(ctrl);
    OBJ(ctrl->oindex)->ctrl = (control *) NULL;
    d_free_control(ctrl);
}

/*
 * NAME:	data->fixobjs()
 * DESCRIPTION:	fix objects in dataspace
 */
static void d_fixobjs(v, n, ctab)
register svalue *v;
register Uint n, *ctab;
{
    while (n != 0) {
	if (v->type == T_OBJECT) {
	    if (v->u.objcnt == OBJ(v->oindex)->count) {
		/* fix object count */
		v->u.objcnt = ctab[v->oindex];
	    } else {
		/* destructed object; mark as invalid */
		v->u.objcnt = 1;
	    }
	}
	v++;
	--n;
    }
}

/*
 * NAME:	data->conv_dataspace()
 * DESCRIPTION:	convert dataspace
 */
void d_conv_dataspace(obj, counttab, convert)
object *obj;
Uint *counttab;
int convert;
{
    sdataspace header;
    register dataspace *data;
    register Uint size;
    register sector *s;
    register unsigned int n;

    data = d_alloc_dataspace(obj);

    /*
     * restore from dump file
     */
    size = d_conv((char *) &header, &obj->dfirst, sd_layout, (Uint) 1,
		  (Uint) 0);
    data->nvariables = header.nvariables;
    data->narrays = header.narrays;
    data->eltsize = header.eltsize;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    data->ncallouts = header.ncallouts;
    data->fcallouts = header.fcallouts;

    /* sectors */
    s = ALLOCA(sector, header.nsectors);
    s[0] = obj->dfirst;
    for (n = 0; n < header.nsectors; n++) {
	size += d_conv((char *) (s + n), s, "d", (Uint) 1, size);
    }

    /* variables */
    data->svariables = ALLOC(svalue, header.nvariables);
    size += d_conv((char *) data->svariables, s, sv_layout,
		   (Uint) header.nvariables, size);
    d_fixobjs(data->svariables, (Uint) header.nvariables, counttab);

    if (header.narrays != 0) {
	/* arrays */
	data->sarrays = ALLOC(sarray, header.narrays);
	size += d_conv((char *) data->sarrays, s, sa_layout, header.narrays,
		       size);
	if (header.eltsize != 0) {
	    data->selts = ALLOC(svalue, header.eltsize);
	    size += d_conv((char *) data->selts, s, sv_layout, header.eltsize,
			   size);
	    d_fixobjs(data->selts, header.eltsize, counttab);
	}
    }

    if (header.nstrings != 0) {
	/* strings */
	data->sstrings = ALLOC(sstring, header.nstrings);
	size += d_conv((char *) data->sstrings, s, ss_layout, header.nstrings,
		       size);
	if (header.strsize != 0) {
	    if (header.flags & CMP_TYPE) {
		data->stext = decompress(s, sw_dreadv, header.strsize, size,
					 &data->strsize);
	    } else {
		data->stext = ALLOC(char, header.strsize);
		sw_dreadv(data->stext, s, header.strsize, size);
	    }
	    size += header.strsize;
	}
    }

    if (header.ncallouts != 0) {
	register scallout *sco;
	register dcallout *co;

	/* callouts */
	co = data->callouts = ALLOC(dcallout, header.ncallouts);
	sco = data->scallouts = ALLOC(scallout, header.ncallouts);
	if (convert) {
	    register oscallout *osc;

	    /*
	     * convert old format callouts
	     */
	    osc = ALLOCA(oscallout, header.ncallouts);
	    d_conv((char *) osc, s, osc_layout, (Uint) header.ncallouts, size);
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = osc->time;
		sco->nargs = osc->nargs;
		memcpy(sco->val, osc->val, 4 * sizeof(svalue));
		sco++;
		osc++;
	    }
	    sco -= data->ncallouts;
	    AFREE(osc - data->ncallouts);
	} else {
	    d_conv((char *) data->scallouts, s, sco_layout,
		   (Uint) header.ncallouts, size);
	}

	for (n = data->ncallouts; n > 0; --n) {
	    co->time = sco->time;
	    co->nargs = sco->nargs;
	    if (sco->val[0].type == T_STRING) {
		if (sco->nargs > 3) {
		    d_fixobjs(sco->val, (Uint) 4, counttab);
		    d_get_values(data, sco->val, co->val, 4);
		} else {
		    d_fixobjs(sco->val, sco->nargs + (Uint) 1, counttab);
		    d_get_values(data, sco->val, co->val, sco->nargs + 1);
		}
	    } else {
		co->val[0] = nil_value;
	    }
	    sco++;
	    co++;
	}
    }

    AFREE(s);

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->u_master)->update) {
	/* handle object upgrading right away */
	data->ctrl = o_control(obj);
	data->ctrl->ndata++;
	d_upgrade_clone(data);
    }

    data->base.flags |= MOD_ALL;
    d_save_dataspace(data, TRUE, counttab);
    OBJ(data->oindex)->data = (dataspace *) NULL;
    d_free_dataspace(data);
}


/*
 * NAME:	data->free_control()
 * DESCRIPTION:	remove the control block from memory
 */
void d_free_control(ctrl)
register control *ctrl;
{
    register string **strs;
    register unsigned short i;

    /* delete strings */
    if (ctrl->strings != (string **) NULL) {
	strs = ctrl->strings;
	for (i = ctrl->nstrings; i > 0; --i) {
	    if (*strs != (string *) NULL) {
		str_del(*strs);
	    }
	    strs++;
	}
	FREE(ctrl->strings);
    }
    if (ctrl->cvstrings != (string **) NULL) {
	strs = ctrl->cvstrings;
	for (i = ctrl->nvardefs; i > 0; --i) {
	    if (*strs != (string *) NULL) {
		str_del(*strs);
	    }
	    strs++;
	}
	FREE(ctrl->cvstrings);
    }

    /* delete vmap */
    if (ctrl->vmap != (unsigned short *) NULL) {
	FREE(ctrl->vmap);
    }

    if (!(ctrl->flags & CTRL_COMPILED)) {
	/* delete sectors */
	if (ctrl->sectors != (sector *) NULL) {
	    FREE(ctrl->sectors);
	}

	if (ctrl->inherits != (dinherit *) NULL) {
	    /* delete inherits */
	    FREE(ctrl->inherits);
	}

	if (ctrl->prog != (char *) NULL) {
	    FREE(ctrl->prog);
	}

	/* delete string constants */
	if (ctrl->sstrings != (dstrconst *) NULL) {
	    FREE(ctrl->sstrings);
	}
	if (ctrl->stext != (char *) NULL) {
	    FREE(ctrl->stext);
	}

	/* delete function definitions */
	if (ctrl->funcdefs != (dfuncdef *) NULL) {
	    FREE(ctrl->funcdefs);
	}

	/* delete variable definitions */
	if (ctrl->vardefs != (dvardef *) NULL) {
	    FREE(ctrl->vardefs);
	    if (ctrl->classvars != (char *) NULL) {
		FREE(ctrl->classvars);
	    }
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
 * NAME:	data->free_dataspace()
 * DESCRIPTION:	remove the dataspace block from memory
 */
void d_free_dataspace(data)
register dataspace *data;
{
    /* free values */
    d_free_values(data);

    /* delete sectors */
    if (data->sectors != (sector *) NULL) {
	FREE(data->sectors);
    }

    /* free scallouts */
    if (data->scallouts != (scallout *) NULL) {
	FREE(data->scallouts);
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

    if (data->ctrl != (control *) NULL) {
	data->ctrl->ndata--;
    }

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
    data->gcprev->gcnext = data->gcnext;
    data->gcnext->gcprev = data->gcprev;
    if (data == gcdata) {
	gcdata = (data != data->gcnext) ? data->gcnext : (dataspace *) NULL;
    }
    --ndata;

    FREE(data);
}
