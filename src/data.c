# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"
# include "csupport.h"

/* bit values for ctrl->flags */
# define CTRL_COMPILED		0x01	/* precompiled control block */
# define CTRL_VARMAP		0x02	/* varmap updated */
# define CTRL_PROGCMP		0x0c	/* program compressed */
# define CTRL_STRCMP		0x30	/* strings compressed */

/* bit values for dataspace->flags */
# define DATA_MODIFIED		0x3f
# define DATA_VARIABLE		0x01	/* variable changed */
# define DATA_ARRAY		0x02	/* array element changed */
# define DATA_ARRAYREF		0x04	/* array reference changed */
# define DATA_STRINGREF		0x08	/* string reference changed */
# define DATA_NEWCALLOUT	0x10	/* new callout added */
# define DATA_CALLOUT		0x20	/* callout changed */
# define DATA_STRCMP		0xc0	/* strings compressed */

/* data compression */
# define CMP_TYPE		0x03
# define CMP_NONE		0x00	/* no compression */
# define CMP_PRED		0x01	/* predictor compression */

# define CMPLIMIT		2048	/* compress if >= CMPLIMIT */

# define ARR_MOD		0x80000000L	/* in array->index */

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
    unsigned short nfloatdefs;	/* # float definitions */
    unsigned short nfloats;	/* # float vars */
    unsigned short vmapsize;	/* size of variable map, or 0 for none */
} scontrol;

static char sc_layout[] = "dcciisiccusssss";

typedef struct {
    uindex oindex;		/* index in object table */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset */
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

typedef struct _svalue_ {
    short type;			/* object, number, string, array */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Uint string;		/* string */
	Uint objcnt;		/* object creation count */
	Uint array;		/* array */
    } u;
} svalue;

static char sv_layout[] = "sui";

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

static char sa_layout[] = "isii";

typedef struct _sstring_ {
    Uint index;			/* index in string text table */
    unsigned short len;		/* length of string */
    Uint ref;			/* refcount */
} sstring;

static char ss_layout[] = "isi";

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

static char sco_layout[] = "is[sui][sui][sui][sui]";

# define co_prev	time
# define co_next	nargs

static control *chead, *ctail, *cone;	/* list of control blocks */
static dataspace *dhead, *dtail, *done;	/* list of dataspace blocks */
static uindex nctrl;			/* # control blocks */
static uindex ndata;			/* # dataspace blocks */


/*
 * NAME:	data->init()
 * DESCRIPTION:	initialize swapped data handling
 */
void d_init()
{
    chead = ctail = cone = (control *) NULL;
    dhead = dtail = done = (dataspace *) NULL;
    nctrl = ndata = 0;
}

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

    ctrl->flags = 0;

    ctrl->nsectors = 0;		/* nothing on swap device yet */
    ctrl->sectors = (sector *) NULL;
    ctrl->obj = (object *) NULL;
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
    ctrl->vmapsize = 0;
    ctrl->vmap = (unsigned short *) NULL;

    return ctrl;
}

/*
 * NAME:	d_alloc_dataspace()
 * DESCRIPTION:	allocate a new dataspace block
 */
static dataspace *d_alloc_dataspace(obj)
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
    data->flags = 0;

    data->obj = obj;
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
 * NAME:	data->new_dataspace()
 * DESCRIPTION:	create a new dataspace block
 */
dataspace *d_new_dataspace(obj)
object *obj;
{
    register dataspace *data;

    data = d_alloc_dataspace(obj);
    data->flags = DATA_VARIABLE;
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
object *obj;
{
    register control *ctrl;

    ctrl = d_new_control();
    ctrl->obj = obj;

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

	ctrl->flags = header.flags << 2;

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
		inherits->obj = &otable[sinherits->oindex];
		inherits->funcoffset = sinherits->funcoffset;
		(inherits++)->varoffset = (sinherits++)->varoffset;
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
	size += UCHAR(header.nvardefs) * (Uint) sizeof(dvardef);

	/* function call table */
	ctrl->funccoffset = size;
	ctrl->nfuncalls = header.nfuncalls;
	size += header.nfuncalls * (Uint) 2;

	/* symbol table */
	ctrl->symboffset = size;
	ctrl->nsymbols = header.nsymbols;

	/* # variables */
	ctrl->nvariables = header.nvariables;
	ctrl->nfloatdefs = header.nfloatdefs;
	ctrl->nfloats = header.nfloats;
    }

    return ctrl;
}

static void d_upgrade_clone P((dataspace*));

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

    data->flags = header.flags << 6;

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

    if (!(obj->flags & O_MASTER) && obj->update != otable[obj->u_master].update)
    {
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
 * NAME:	data->varmap()
 * DESCRIPTION:	add a variable mapping to a control block
 */
void d_varmap(ctrl, nvar, vmap)
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
		if (ctrl->flags & CTRL_STRCMP) {
		    ctrl->stext = decompress(ctrl->sectors, sw_readv,
					     ctrl->strsize,
					     ctrl->stroffset +
					     ctrl->nstrings * sizeof(dstrconst),
					     &ctrl->strsize);
		} else {
		    ctrl->stext = ALLOC(char, ctrl->strsize);
		    sw_readv(ctrl->stext, ctrl->sectors, ctrl->strsize,
			     ctrl->stroffset +
				     ctrl->nstrings * (Uint) sizeof(dstrconst));
		}
	    }
	}
    }

    if (ctrl->strings[idx] == (string *) NULL) {
	register string *str;

	str = str_new(ctrl->stext + ctrl->sstrings[idx].index,
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
    }

    if (data->strings[idx].str == (string *) NULL) {
	register string *s;

	s = str_new(data->stext + data->sstrings[idx].index,
		    (long) data->sstrings[idx].len);
	s->primary = &data->strings[idx];
	s->primary->str = s;
	s->primary->data = data;
	s->primary->ref = data->sstrings[idx].ref;
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
	    str_ref(v->u.string = d_get_string(data, sv->u.string));
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
    register unsigned short nfdefs, nvars, nfloats;
    register value *val;
    register dvardef *var;
    register control *ctrl;
    register dinherit *inh;

    /*
     * initialize all variables to integer 0
     */
    for (val = data->variables, nvars = data->nvariables; nvars > 0; --nvars) {
	*val++ = zero_value;
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
	    d_new_variables(data);
	} else {
	    /*
	     * variables must be loaded from the swap
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

	data = arr->primary->data;
	if (data->selts == (svalue *) NULL) {
	    /* load array elements */
	    data->selts = (svalue *) ALLOC(svalue, data->eltsize);
	    sw_readv((char *) data->selts, data->sectors,
		     data->eltsize * sizeof(svalue),
		     data->arroffset + data->narrays * sizeof(sarray));
	}
	v = arr->elts = ALLOC(value, arr->size);
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
	    if (str->primary->ref++ == 0) {
		data->schange--;	/* first reference restored */
	    }
	    data->flags |= DATA_STRINGREF;
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
		data->flags |= DATA_ARRAYREF;
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
	if (str->primary != (strref *) NULL && str->primary->data == data) {
	    /* in this object */
	    if (--(str->primary->ref) == 0) {
		data->schange++;	/* last reference removed */
	    }
	    data->flags |= DATA_STRINGREF;
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
		data->flags |= DATA_ARRAYREF;
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
	data->flags |= DATA_VARIABLE;
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
	    data->flags |= DATA_ARRAY;
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
    register value *v;
    register unsigned short n;
    register dataspace *data;

    if (arr->primary->ref == 0 && (v=arr->elts) != (value *) NULL) {
	/*
	 * Completely delete a swapped-in array.  Update the local
	 * reference counts for all arrays referenced by it.
	 */
	n = arr->size;
	data = arr->primary->data;
	do {
	    del_lhs(data, v++);
	} while (--n != 0);
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
	data->flags |= DATA_NEWCALLOUT;
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
	    data->flags |= DATA_CALLOUT;
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
	    data->flags |= DATA_NEWCALLOUT;
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
string *d_get_call_out(data, handle, t, nargs)
dataspace *data;
unsigned int handle;
Uint *t;
int *nargs;
{
    string *str;
    register dcallout *co;
    register value *v;
    register uindex n;

    if (handle == 0 || handle > data->ncallouts) {
	/* no such callout */
	return (string *) NULL;
    }
    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
    }

    co = &data->callouts[handle - 1];
    if (co->val[0].type == T_INVALID) {
	/* invalid callout */
	return (string *) NULL;
    }
    i_grow_stack((*nargs = co->nargs) + 1);
    *t = co->time;
    v = co->val;

    del_lhs(data, &v[0]);
    *--sp = v[0];
    str = v[0].u.string;
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

    data->flags |= DATA_CALLOUT;
    return str;
}

static int cmp P((cvoid*, cvoid*));

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
    uindex max_args;

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
 * NAME:	data->swapalloc()
 * DESCRIPTION:	allocate swapspace for something
 */
static sector d_swapalloc(size, nsectors, sectors)
Uint size;
register sector nsectors, **sectors;
{
    register sector n, *s;

    n = sw_mapsize(size);
    if (nsectors == 0) {
	/* no sectors yet */
	*sectors = s = ALLOC(sector, nsectors = n);
	while (n > 0) {
	    *s++ = sw_new();
	    --n;
	}
    } else if (nsectors < n) {
	/* not enough sectors */
	s = ALLOC(sector, n);
	memcpy(s, *sectors, nsectors * sizeof(sector));
	FREE(*sectors);
	*sectors = s;
	s += nsectors;
	n -= nsectors;
	nsectors += n;
	while (n > 0) {
	    *s++ = sw_new();
	    --n;
	}
    } else if (nsectors > n) {
	/* too many sectors */
	s = *sectors + nsectors;
	n = nsectors - n;
	nsectors -= n;
	while (n > 0) {
	    sw_del(*--s);
	    --n;
	}
    }

    return nsectors;
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
    header.flags = 0;
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
	       header.nfuncalls * (Uint) 2 +
	       header.nsymbols * (Uint) sizeof(dsymbol);
    }
    ctrl->nsectors = header.nsectors = d_swapalloc(size, ctrl->nsectors,
						   &ctrl->sectors);
    ctrl->obj->cfirst = ctrl->sectors[0];

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
	    sinherits->oindex = inherits->obj->index;
	    sinherits->funcoffset = inherits->funcoffset;
	    (sinherits++)->varoffset = (inherits++)->varoffset;
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
	    if (str_put(v->u.string, nstr) >= nstr) {
		nstr++;
		strsize += v->u.string->len;
	    }
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    if (arr_put(v->u.array) >= narr) {
		if (v->type == T_MAPPING &&
		    v->u.array->hashed != (struct _maphash_ *) NULL) {
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
    register Uint i;

    while (n > 0) {
	switch (sv->type = v->type) {
	case T_INT:
	    sv->u.number = v->u.number;
	    break;

	case T_FLOAT:
	    SFLT_PUT(sv, v);
	    break;

	case T_STRING:
	    i = str_put(v->u.string, nstr);
	    sv->u.string = i;
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
		sv->u.string = v->u.string->primary - sdata->strings;
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
	if (data->flags & DATA_ARRAY) {
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
		s->str->primary = (strref *) NULL;
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

    if (data->nsectors != 0 && data->achange == 0 && data->schange == 0 &&
	!(data->flags & DATA_NEWCALLOUT)) {
	bool mod;

	/*
	 * No strings/arrays added or deleted. Check individual variables and
	 * array elements.
	 */
	if (data->flags & DATA_VARIABLE) {
	    /*
	     * variables changed
	     */
	    d_put_values(data->svariables, data->variables, data->nvariables);
	    sw_writev((char *) data->svariables, data->sectors,
		      data->nvariables * (Uint) sizeof(svalue),
		      data->varoffset);
	}
	if (data->flags & DATA_ARRAYREF) {
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
	if (data->flags & DATA_ARRAY) {
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
	if (data->flags & DATA_STRINGREF) {
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
	if (data->flags & DATA_CALLOUT) {
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
	char *text;
	register Uint size;

	/*
	 * count the number and sizes of strings and arrays
	 */
	narr = 0;
	nstr = 0;
	cstr = 0;
	arrsize = 0;
	strsize = 0;

	d_get_variable(data, 0);
	if (data->svariables == (svalue *) NULL) {
	    data->svariables = ALLOC(svalue, data->nvariables);
	}
	d_count(data->variables, data->nvariables);

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
	header.flags = 0;
	header.nvariables = data->nvariables;
	header.narrays = narr;
	header.eltsize = arrsize;
	header.nstrings = nstr - cstr;
	header.strsize = strsize;
	header.ncallouts = data->ncallouts;
	header.fcallouts = data->fcallouts;

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

	text = stext;
	if (header.strsize >= CMPLIMIT) {
	    text = ALLOCA(char, header.strsize);
	    size = compress(text, stext, header.strsize);
	    if (size != 0) {
		header.flags |= CMP_PRED;
		header.strsize = size;
	    } else {
		AFREE(text);
		text = stext;
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
	data->obj->dfirst = data->sectors[0];

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
		sw_writev(text, data->sectors, header.strsize, size);
		size += header.strsize;
		if (text != stext) {
		    AFREE(text);
		}
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
	data->strsize = strsize;

	data->achange = 0;
	data->schange = 0;
    }

    data->flags &= ~DATA_MODIFIED;
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
	    register array *a;
	    register Uint i, j;

	    a = val->u.array;
	    if (a->primary->data != data) {
		/*
		 * imported array
		 */
		i = arr_put(a);
		if (i >= narr) {
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
			if (a->primary->arr != (array *) NULL) {
			    /* remove from old dataspace */
			    d_get_elts(a);
			    d_del_array(a);
			    a->primary->arr = (array *) NULL;
			}
			a->primary = &data->alocal;
		    } else {
			/*
			 * make new array
			 */
			a = arr_alloc(a->size);
			a->tag = val->u.array->tag;
			a->odcount = val->u.array->odcount;
			a->primary = &data->alocal;

			if (a->size > 0) {
			    register value *v;

			    /*
			     * copy elements
			     */
			    a->elts = v = ALLOC(value, j = a->size);
			    memcpy(v, d_get_elts(val->u.array),
				   j * sizeof(value));
			    do {
				switch (v->type) {
				case T_STRING:
				    str_ref(v->u.string);
				    break;

				case T_ARRAY:
				case T_MAPPING:
				    arr_ref(v->u.array);
				    break;
				}
				v++;
			    } while (--j != 0);
			}

			/*
			 * replace
			 */
			arr_del(val->u.array);
			arr_ref(val->u.array = a);
			narr++;
		    }

		    /*
		     * store in itab
		     */
		    if (i >= itabsz) {
			array **tmp;

			/*
			 * increase size of itab
			 */
			for (j = itabsz; j <= i; j += j) ;
			tmp = ALLOC(array*, j);
			memcpy(tmp, itab, itabsz * sizeof(array*));
			FREE(itab);
			itab = tmp;
			itabsz = j;
		    }
		    arr_put(itab[i] = a);
		    narr++;

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
		    arr_ref(a = itab[i]);
		    arr_del(val->u.array);
		    val->u.array = a;
		}
	    } else if (arr_put(a) >= narr) {
		/*
		 * not previously encountered mapping or array
		 */
		narr++;
		if (a->hashed != (struct _maphash_ *) NULL) {
		    map_compact(a);
		    d_import(data, a->elts, a->size);
		} else if (a->elts != (value *) NULL) {
		    d_import(data, a->elts, a->size);
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
			    } else if (a->arr->elts != (value *) NULL) {
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
 * NAME:	data->upgrade()
 * DESCRIPTION:	upgrade the dataspace for one object
 */
static void d_upgrade(data, nvar, vmap, old)
register dataspace *data;
register unsigned short nvar, *vmap;
object *old;
{
    register value *v;
    register unsigned short n;
    value *vars;

    /* make sure variables are in memory */
    vars = d_get_variable(data, 0);

    /* map variables */
    for (n = nvar, v = ALLOC(value, n); n > 0; --n) {
	if (NEW_VAR(*vmap)) {
	    *v++ = (*vmap == NEW_INT) ? zero_value : zero_float;
	} else {
	    *v = vars[*vmap];
	    i_ref_value(&vars[*vmap]);	/* don't wipe out objects */
	    v->modified = TRUE;
	    ref_rhs(data, v++);
	}
	vmap++;
    }
    vars = v - nvar;

    /* deref old values */
    v = data->variables;
    for (n = data->nvariables; n > 0; --n) {
	del_lhs(data, v);
	i_del_value(v++);
    }

    /* replace old with new */
    FREE(data->variables);
    data->variables = vars;

    data->flags |= DATA_VARIABLE;
    if (data->nvariables != nvar) {
	if (data->svariables != (svalue *) NULL) {
	    FREE(data->svariables);
	    data->svariables = (svalue *) NULL;
	}
	data->nvariables = nvar;
	data->achange++;	/* force rebuild on swapout */
    }

    o_upgraded(old, data->obj);
}

/*
 * NAME:	data->upgrade_clone()
 * DESCRIPTION:	upgrade a clone object
 */
static void d_upgrade_clone(data)
register dataspace *data;
{
    register object *obj, *old;
    register unsigned short nvar, *vmap;
    register Uint update;

    /*
     * the program for the clone was upgraded since last swapin
     */
    obj = data->obj;
    update = obj->update;
    obj = &otable[obj->u_master];
    old = &otable[obj->prev];
    if (O_UPGRADING(obj)) {
	/* in the middle of an upgrade */
	old = &otable[old->prev];
    }
    nvar = data->ctrl->nvariables + 1;
    vmap = o_control(old)->vmap;

    if (old->update != update) {
	register unsigned short *m1, *m2, n;

	m1 = vmap;
	vmap = ALLOCA(unsigned short, n = nvar);
	do {
	    old = &otable[old->prev];
	    m2 = o_control(old)->vmap;
	    while (n > 0) {
		*vmap++ = (NEW_VAR(*m1)) ? *m1++ : m2[*m1++];
		--n;
	    }
	    n = nvar;
	    vmap -= n;
	    m1 = vmap;
	} while (old->update != update);
    }

    d_upgrade(data, nvar, vmap, old);
    if (vmap != old->ctrl->vmap) {
	AFREE(vmap);
    }
}

/*
 * NAME:	data->upgrade_all()
 * DESCRIPTION:	upgrade all obj and all objects cloned from obj that have
 *		dataspaces in memory
 */
void d_upgrade_all(old, new)
register object *old, *new;
{
    register dataspace *data;
    register unsigned int nvar;
    register unsigned short *vmap;

    nvar = old->ctrl->vmapsize;
    vmap = old->ctrl->vmap;

    data = new->data;
    if (data != (dataspace *) NULL) {
	/* upgrade dataspace of master object */
	if (nvar != 0) {
	    d_upgrade(data, nvar, vmap, old);
	}
	data->ctrl->ndata--;
	data->ctrl = new->ctrl;
	data->ctrl->ndata++;
    }

    for (data = dtail; data != (dataspace *) NULL; data = data->prev) {
	if (!(data->obj->flags & O_MASTER) && data->obj->u_master == new->index)
	{
	    /* upgrade clone */
	    if (nvar != 0) {
		d_upgrade(data, nvar, vmap, old);
	    }
	    data->ctrl->ndata--;
	    data->ctrl = new->ctrl;
	    data->ctrl->ndata++;
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
    register string **strs;

    if (ctrl->obj != (object *) NULL) {
	ctrl->obj->ctrl = (control *) NULL;
    }

    /* delete strings */
    if (ctrl->strings != (string **) NULL) {
	register unsigned short i;

	strs = ctrl->strings;
	for (i = ctrl->nstrings; i > 0; --i) {
	    if (*strs != (string *) NULL) {
		str_del(*strs);
	    }
	    strs++;
	}
	FREE(ctrl->strings);
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
    register control *ctrl;

    count = 0;

    /* swap out dataspace blocks */
    data = dtail;
    for (n = ndata / frag; n > 0; --n) {
	register dataspace *prev;

	prev = data->prev;
	if (!(data->obj->flags & O_PENDIO) || frag == 1) {
	    if (data->flags & DATA_MODIFIED) {
		d_save_dataspace(data);
		count++;
	    }
	    d_free_dataspace(data);
	}
	data = prev;
    }
    /* multiply ref counts for leftover datablocks by 3/4 */
    done = (dataspace *) NULL;
    for (data = dtail; data != dhead; data = data->prev) {
	data->refc = data->refc * 3 / 4;
	if (data->refc <= 1) {
	    done = data;
	}
    }

    /* swap out control blocks */
    ctrl = ctail;
    for (n = nctrl / frag; n > 0; --n) {
	register control *prev;

	prev = ctrl->prev;
	if (ctrl->ndata == 0) {
	    if ((ctrl->sectors == (sector *) NULL &&
		 !(ctrl->flags & CTRL_COMPILED)) || (ctrl->flags & CTRL_VARMAP))
	    {
		d_save_control(ctrl);
	    }
	    d_free_control(ctrl);
	}
	ctrl = prev;
    }
    /* multiply ref counts for leftover control blocks by 3/4 */
    cone = (control *) NULL;
    for (ctrl = ctail; ctrl != chead; ctrl = ctrl->prev) {
	ctrl->refc = ctrl->refc * 3 / 4;
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
	if ((ctrl->sectors == (sector *) NULL &&
	     !(ctrl->flags & CTRL_COMPILED)) || (ctrl->flags & CTRL_VARMAP)) {
	    d_save_control(ctrl);
	}
    }

    /* save dataspace blocks */
    for (data = dtail; data != (dataspace *) NULL; data = data->prev) {
	if (data->flags & DATA_MODIFIED) {
	    d_save_dataspace(data);
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
 * NAME:	data->conv_control()
 * DESCRIPTION:	convert control block
 */
void d_conv_control(obj)
register object *obj;
{
    scontrol header;
    register control *ctrl;
    register Uint size;
    register sector *s;
    register unsigned int n;

    ctrl = d_new_control();
    ctrl->obj = obj;

    /*
     * restore from dump file
     */
    size = d_conv((char *) &header, &obj->cfirst, sc_layout, (Uint) 1,
		  (Uint) 0);
    ctrl->ninherits = UCHAR(header.ninherits);
    ctrl->compiled = header.compiled;
    ctrl->progsize = header.progsize;
    ctrl->nstrings = header.nstrings;
    ctrl->strsize = header.strsize;
    ctrl->nfuncdefs = UCHAR(header.nfuncdefs);
    ctrl->nvardefs = UCHAR(header.nvardefs);
    ctrl->nfuncalls = header.nfuncalls;
    ctrl->nsymbols = header.nsymbols;
    ctrl->nvariables = header.nvariables;
    ctrl->nfloatdefs = header.nfloatdefs;
    ctrl->nfloats = header.nfloats;
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
	    inherits->obj = &otable[sinherits->oindex];
	    inherits->funcoffset = sinherits->funcoffset;
	    (inherits++)->varoffset = (sinherits++)->varoffset;
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
	}

	if (header.nvardefs != 0) {
	    /* variable definitions */
	    ctrl->vardefs = ALLOC(dvardef, UCHAR(header.nvardefs));
	    size += d_conv((char *) ctrl->vardefs, s, DV_LAYOUT,
			   (Uint) UCHAR(header.nvardefs), size);
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
	    if (v->u.objcnt == otable[v->oindex].count) {
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
void d_conv_dataspace(obj, counttab)
object *obj;
Uint *counttab;
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
	scallout *scallouts;
	register scallout *sco;
	register dcallout *co;

	/* callouts */
	co = data->callouts = ALLOC(dcallout, header.ncallouts);
	sco = scallouts = ALLOCA(scallout, header.ncallouts);
	d_conv((char *) scallouts, s, sco_layout, (Uint) header.ncallouts,
	       size);

	for (n = data->ncallouts; n > 0; --n) {
	    co->time = sco->time;
	    co->nargs = sco->nargs;
	    if (sco->val[0].type != T_INVALID) {
		if (sco->nargs > 3) {
		    d_fixobjs(sco->val, (Uint) 4, counttab);
		    d_get_values(data, sco->val, co->val, 4);
		} else {
		    d_fixobjs(sco->val, sco->nargs + (Uint) 1, counttab);
		    d_get_values(data, sco->val, co->val, sco->nargs + 1);
		}
	    } else {
		co->val[0].type = T_INVALID;
	    }
	    sco++;
	    co++;
	}

	AFREE(scallouts);
    }

    AFREE(s);

    if (!(obj->flags & O_MASTER) && obj->update != otable[obj->u_master].update)
    {
	/* handle object upgrading right away */
	data->ctrl = o_control(obj);
	data->ctrl->ndata++;
	d_upgrade_clone(data);
    }

    d_save_dataspace(data);
    d_free_dataspace(data);
}


/*
 * NAME:	data->del_control()
 * DESCRIPTION:	delete a control block from swap and memory
 */
void d_del_control(ctrl)
register control *ctrl;
{
    if (ctrl->sectors != (sector *) NULL) {
	register sector i, *s;

	for (i = ctrl->nsectors, s = ctrl->sectors + i; i > 0; --i) {
	    sw_del(*--s);
	}
    }
    if (ctrl->obj != (object *) NULL) {
	ctrl->obj->cfirst = SW_UNUSED;
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
    if (data->ncallouts != 0) {
	register unsigned short n;
	register dcallout *co;

	/*
	 * remove callouts from callout table
	 */
	if (data->callouts == (dcallout *) NULL) {
	    d_get_callouts(data);
	}
	for (n = data->ncallouts, co = data->callouts + n; n > 0; --n) {
	    if ((--co)->val[0].type != T_INVALID) {
		co_del(data->obj, n);
	    }
	}
    }
    if (data->sectors != (sector *) NULL) {
	register sector i, *s;

	for (i = data->nsectors, s = data->sectors + i; i > 0; --i) {
	    sw_del(*--s);
	}
    }
    data->obj->dfirst = SW_UNUSED;
    d_free_dataspace(data);
}
