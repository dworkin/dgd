# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"
# include "parse.h"
# include "csupport.h"

/* bit values for ctrl->flags */
# define CTRL_PROGCMP		0x03	/* program compressed */
# define CTRL_STRCMP		0x0c	/* strings compressed */
# define CTRL_COMPILED		0x10	/* precompiled control block */
# define CTRL_VARMAP		0x20	/* varmap updated */

/* bit values for dataspace->flags */
# define DATA_STRCMP		0x03	/* strings compressed */

/* bit values for dataspace->values->flags */
# define MOD_ALL		0x3f
# define MOD_VARIABLE		0x01	/* variable changed */
# define MOD_ARRAY		0x02	/* array element changed */
# define MOD_ARRAYREF		0x04	/* array reference changed */
# define MOD_STRINGREF		0x08	/* string reference changed */
# define MOD_CALLOUT		0x10	/* callout changed */
# define MOD_NEWCALLOUT		0x20	/* new callout added */

/* data compression */
# define CMP_TYPE		0x03
# define CMP_NONE		0x00	/* no compression */
# define CMP_PRED		0x01	/* predictor compression */

# define CMPLIMIT		2048	/* compress if >= CMPLIMIT */

# define ARR_MOD		0x80000000L	/* in arrref->ref */
# define PRIV			0x8000		/* in sinherit->varoffset */

# define AR_UNCHANGED		0	/* mapping unchanged */
# define AR_CHANGED		1	/* mapping changed */
# define AR_ALOCAL		2	/* array/mapping not swapped in */

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
} scontrol;

static char sc_layout[] = "dcciisiccusssss";

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

# define COP_ADD	0	/* add callout patch */
# define COP_REMOVE	1	/* remove callout patch */
# define COP_REPLACE	2	/* replace callout patch */

typedef struct _copatch_ {
    short type;			/* add, remove, replace */
    uindex handle;		/* callout handle */
    dataplane *plane;		/* dataplane */
    Uint time;			/* start time */
    unsigned short mtime;	/* start time millisec component */
    cbuf *queue;		/* callout queue */
    struct _copatch_ *next;	/* next in linked list */
    dcallout aco;		/* added callout */
    dcallout rco;		/* removed callout */
} copatch;

# define COPCHUNKSZ	32

typedef struct _copchunk_ {
    copatch cop[COPCHUNKSZ];	/* callout patches */
    struct _copchunk_ *next;	/* next in linked list */
} copchunk;

typedef struct _coptable_ {
    copatch *cop[COPATCHHTABSZ];	/* hash table of callout patches */
    copchunk *chunk;			/* callout patch chunk */
    unsigned short chunksz;		/* size of callout patch chunk */
    copatch *flist;			/* free list of callout patches */
} coptable;

static control *chead, *ctail;		/* list of control blocks */
static dataspace *dhead, *dtail;	/* list of dataspace blocks */
static dataplane *plist;		/* list of dataplanes */
static uindex nctrl;			/* # control blocks */
static uindex ndata;			/* # dataspace blocks */
static bool nilisnot0;			/* nil != int 0 */
static uindex ncallout;			/*  # callouts added */


/*
 * NAME:	data->init()
 * DESCRIPTION:	initialize swapped data handling
 */
void d_init(flag)
bool flag;
{
    chead = ctail = (control *) NULL;
    dhead = dtail = (dataspace *) NULL;
    plist = (dataplane *) NULL;
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
 * NAME:	d_alloc_dataspace()
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
    } else {
	/* list was empty */
	data->prev = data->next = (dataspace *) NULL;
	dhead = dtail = data;
    }
    ndata++;

    data->ilist = (dataspace *) NULL;
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

    /* strings */
    data->schange = 0;
    data->nstrings = 0;
    data->strsize = 0;
    data->strings = (strref *) NULL;
    data->sstrings = (sstring *) NULL;
    data->stext = (char *) NULL;

    /* callouts */
    data->ncallouts = 0;
    data->fcallouts = 0;
    data->callouts = (dcallout *) NULL;

    /* value plane */
    data->basic.level = 0;
    data->basic.flags = 0;
    data->basic.achange = 0;
    data->basic.imports = 0;
    data->basic.alocal.values = &data->basic;
    data->basic.alocal.arr = (array *) NULL;
    data->basic.alocal.data = data;
    data->basic.alocal.state = AR_ALOCAL;
    data->basic.arrays = (arrref *) NULL;
    data->basic.coptab = (coptable *) NULL;
    data->basic.prev = (dataplane *) NULL;
    data->basic.plist = (dataplane *) NULL;
    data->values = &data->basic;

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
    data->values->flags = MOD_VARIABLE;
    data->ctrl = o_control(obj);
    data->ctrl->ndata++;
    data->nvariables = data->ctrl->nvariables + 1;

    return data;
}

/*
 * NAME:	data->load_control()
 * DESCRIPTION:	load a control block from the swap device
 */
control *d_load_control(oindex)
unsigned int oindex;
{
    register control *ctrl;
    register object *obj;

    ctrl = d_new_control();
    ctrl->oindex = oindex;
    obj = OBJ(oindex);

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
	ctrl->nifdefs = header.nifdefs;
	ctrl->nvinit = header.nvinit;
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

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->u_master)->update) {
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
 * NAME:	d_get_stext()
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
	ctrl = o_control(OBJ(ctrl->inherits[UCHAR(inherit)].oindex));
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
	s->ref = 1;
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
    if (data->values->arrays == (arrref *) NULL ||
	data->values->arrays[idx].arr == (array *) NULL) {
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
	p = data->values;

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
	    a->values = &data->basic;
	    a->data = data;
	    a->state = AR_UNCHANGED;
	    a->ref = data->sarrays[idx].ref;
	    p = p->prev;
	} while (p != (dataplane *) NULL);

	arr->primary = &data->values->arrays[idx];
	return arr;
    }
    return data->values->arrays[idx].arr;
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
    register unsigned short nifdefs, nvars, nvinit;
    register value *val;
    register dvardef *var;
    register control *ctrl;
    register dinherit *inh;

    /*
     * first, initialize all variables to nil
     */
    for (val = data->variables, nvars = data->nvariables; nvars > 0; --nvars) {
	*val++ = nil_value;
    }

    if (data->ctrl->nvinit != 0) {
	/*
	 * explicitly initialize some variables
	 */
	nvars = 0;
	for (nvinit = data->ctrl->nvinit, inh = data->ctrl->inherits;
	     nvinit > 0; inh++) {
	    if (inh->varoffset == nvars) {
		ctrl = o_control(OBJ(inh->oindex));
		if (ctrl->nifdefs != 0) {
		    nvinit -= ctrl->nifdefs;
		    for (nifdefs = ctrl->nifdefs, var = d_get_vardefs(ctrl);
			 nifdefs > 0; var++) {
			if (var->type == T_INT && nilisnot0) {
			    data->variables[nvars] = zero_int;
			    --nifdefs;
			} else if (var->type == T_FLOAT) {
			    data->variables[nvars] = zero_float;
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
	idx = data->sarrays[arr->primary - data->values->arrays].index;
	d_get_values(data, &data->selts[idx], v, arr->size);
    }

    return v;
}


static dataspace *ifirst, *ilast;	/* list of dataspaces with imports */

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
	    data->values->flags |= MOD_STRINGREF;
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
		arr->primary->ref++;
		data->values->flags |= MOD_ARRAYREF;
	    } else {
		/* ref new array */
		data->values->achange++;
	    }
	} else {
	    /* not in this object: ref imported array */
	    if (data->values->imports++ == 0 &&
		data->ilist == (dataspace *) NULL &&
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
	    data->values->achange++;
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
		str_del(str);
		data->schange++;	/* last reference removed */
	    }
	    data->values->flags |= MOD_STRINGREF;
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
		data->values->flags |= MOD_ARRAYREF;
		if ((--(arr->primary->ref) & ~ARR_MOD) == 0) {
		    /* last reference removed */
		    arr->primary->arr = (array *) NULL;
		    data->values->achange++;

		    if (arr->hashed != (struct _maphash_ *) NULL) {
			map_compact(arr);
		    }
		    /*
		     * If the array is not loaded, don't bother to load it now.
		     */
		    if ((lhs=arr->elts) != (value *) NULL) {
			register unsigned short n;

			n = arr->size;
			data = arr->primary->data;
			do {
			    del_lhs(data, lhs++);
			} while (--n != 0);
		    }
		    arr_del(arr);
		}
	    } else {
		/* deref new array */
		data->values->achange--;
	    }
	} else {
	    /* not in this object: deref imported array */
	    data->values->imports--;
	    data->values->achange--;
	}
	break;
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
	if (sco->val[0].type == T_STRING) {
	    d_get_values(data, sco->val, co->val,
			 (sco->nargs > 3) ? 4 : sco->nargs + 1);
	} else {
	    co->val[0] = nil_value;
	}
	sco++;
	co++;
    }

    AFREE(scallouts);
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
	co = data->callouts = ALLOC(dcallout, 1);
	data->ncallouts = handle = 1;
	data->values->flags |= MOD_NEWCALLOUT;
    } else {
	if (data->callouts == (dcallout *) NULL) {
	    d_get_callouts(data);
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
		data->values->flags |= MOD_CALLOUT;
	    } else {
		/*
		 * add new callout
		 */
		handle = data->ncallouts;
		co = data->callouts = REALLOC(data->callouts, dcallout, handle,
					      handle + 1);
		co += handle;
		data->ncallouts = ++handle;
		data->values->flags |= MOD_NEWCALLOUT;
	    }
	}
    }

    co->time = time;
    co->nargs = nargs;
    memcpy(co->val, v, sizeof(co->val));
    switch (nargs) {
    case 3:
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
    del_lhs(data, &v[0]);
    str_del(v[0].u.string);
    v[0] = nil_value;

    switch (co->nargs) {
    default:
	del_lhs(data, &v[3]);
	i_del_value(&v[3]);
    case 2:
	del_lhs(data, &v[2]);
	i_del_value(&v[2]);
    case 1:
	del_lhs(data, &v[1]);
	i_del_value(&v[1]);
    case 0:
	break;
    }

    n = data->fcallouts;
    if (n != 0) {
	data->callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    data->fcallouts = handle;

    data->values->flags |= MOD_CALLOUT;
}


/*
 * NAME:	copatch->init()
 * DESCRIPTION:	initialize copatch table
 */
static void cop_init(plane)
dataplane *plane;
{
    memset(plane->coptab = ALLOC(coptable, 1), '\0', sizeof(coptable));
}

/*
 * NAME:	copatch->clean()
 * DESCRIPTION:	free copatch table
 */
static void cop_clean(plane)
dataplane *plane;
{
    register copchunk *c, *f;

    c = plane->coptab->chunk;
    while (c != (copchunk *) NULL) {
	f = c;
	c = c->next;
	FREE(f);
    }

    FREE(plane->coptab);
    plane->coptab = (coptable *) NULL;
}

/*
 * NAME:	copatch->new()
 * DESCRIPTION:	create a new callout patch
 */
static copatch *cop_new(plane, c, type, handle, co, time, mtime, q)
dataplane *plane;
copatch **c;
int type;
unsigned int handle, mtime;
register dcallout *co;
Uint time;
cbuf *q;
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
	    cc = ALLOC(copchunk, 1);
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
static void cop_del(plane, c, del)
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
	    i_del_value(v++);
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
cbuf *q;
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
static void cop_commit(cop)
register copatch *cop;
{
    register int i;
    register value *v;

    cop->type = COP_ADD;
    for (i = (cop->rco.nargs > 3) ? 4 : cop->rco.nargs + 1, v = cop->rco.val;
	 i > 0; --i) {
	i_del_value(v++);
    }
}

/*
 * NAME:	copatch->release()
 * DESCRIPTION:	remove a callout replacement
 */
static void cop_release(cop)
register copatch *cop;
{
    register int i;
    register value *v;

    cop->type = COP_REMOVE;
    for (i = (cop->aco.nargs > 3) ? 4 : cop->aco.nargs + 1, v = cop->aco.val;
	 i > 0; --i) {
	i_del_value(v++);
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
 * NAME:	commit_callouts()
 * DESCRIPTION:	commit callout patches to previous plane
 */
static void commit_callouts(plane)
register dataplane *plane;
{
    register dataplane *prev;
    register copatch **c, **n, *cop;
    copatch **t, **next;
    int i;

    prev = plane->prev;
    for (i = COPATCHHTABSZ, t = plane->coptab->cop; --i >= 0; t++) {
	if (*t != (copatch *) NULL && (*t)->plane == plane) {
	    /*
	     * find previous plane
	     */
	    next = t;
	    do {
		next = &(*next)->next;
	    } while (*next != (copatch *) NULL && (*next)->plane == plane);

	    c = t;
	    do {
		cop = *c;
		if (prev->level == 0) {
		    /*
		     * commit to last plane
		     */
		    switch (cop->type) {
		    case COP_ADD:
			co_new(plane->alocal.data->oindex, cop->handle,
			       cop->time, cop->mtime, cop->queue);
			--ncallout;
			break;

		    case COP_REMOVE:
			co_del(plane->alocal.data->oindex, cop->handle,
			       cop->rco.time);
			ncallout++;
			break;

		    case COP_REPLACE:
			co_del(plane->alocal.data->oindex, cop->handle,
			       cop->rco.time);
			co_new(plane->alocal.data->oindex, cop->handle,
			       cop->time, cop->mtime, cop->queue);
			cop_commit(cop);
			break;
		    }

		    if (next == &cop->next) {
			next = c;
		    }
		    cop_del(plane, c, TRUE);
		} else {
		    /*
		     * commit to previous plane
		     */
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
				cop_del(plane, c, TRUE);
				cop = (copatch *) NULL;
				break;

			    case COP_REMOVE:
				if ((*n)->type == COP_REPLACE) {
				    /* turn replace back into remove */
				    cop_release(*n);
				} else {
				    /* del old */
				    cop_del(plane, n, TRUE);
				}
				/* del new */
				if (next == &cop->next) {
				    next = c;
				}
				cop_del(plane, c, TRUE);
				cop = (copatch *) NULL;
				break;

			    case COP_REPLACE:
				if ((*n)->type == COP_REPLACE) {
				    /* merge replaces into old, del new */
				    cop_release(*n);
				    cop_replace(*n, &cop->aco, cop->time,
						cop->mtime, cop->queue);
				    if (next == &cop->next) {
					next = c;
				    }
				    cop_del(plane, c, TRUE);
				    cop = (copatch *) NULL;
				} else {
				    /* make replace into add, remove old */
				    cop_del(plane, n, TRUE);
				    cop_commit(cop);
				}
				break;
			    }
			    break;
			}
		    }

		    if (cop != (copatch *) NULL) {
			cop->plane = prev;
			c = &cop->next;
		    }
		}
	    } while (c != next);
	}
    }
}

/*
 * NAME:	discard_callouts()
 * DESCRIPTION:	discard callout patches on current plane, restoring old callouts
 */
static void discard_callouts(plane)
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
		cop_del(plane, c, TRUE);
		--ncallout;
		break;

	    case COP_REMOVE:
		d_alloc_call_out(data, cop->handle, cop->rco.time,
				 cop->rco.nargs, cop->rco.val);
		cop_del(plane, c, FALSE);
		ncallout++;
		break;

	    case COP_REPLACE:
		d_free_call_out(data, cop->handle);
		d_alloc_call_out(data, cop->handle, cop->rco.time,
				 cop->rco.nargs, cop->rco.val);
		cop_discard(cop);
		cop_del(plane, c, TRUE);
		break;
	    }
	}
    }
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

    p = ALLOC(dataplane, 1);

    p->level = level;
    p->flags = data->values->flags;
    p->achange = data->values->achange;
    p->imports = data->values->imports;

    /* copy value information from previous plane */
    p->original = (value *) NULL;
    p->alocal.values = p;
    p->alocal.arr = (array *) NULL;
    p->alocal.data = data;
    p->alocal.state = AR_ALOCAL;
    p->coptab = data->values->coptab;

    if (data->values->arrays != (arrref *) NULL) {
	register arrref *a, *b;
	register Uint i;

	p->arrays = ALLOC(arrref, i = data->narrays);
	for (a = p->arrays, b = data->values->arrays, i = data->narrays; i != 0;
	     a++, b++, --i) {
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
    p->achunk = (abchunk *) NULL;

    p->prev = data->values;
    data->values = p;
    p->plist = plist;
    plist = p;
}

/*
 * NAME:	commit_values()
 * DESCRIPTION:	commit non-swapped arrays among the values
 */
static void commit_values(v, n, plane)
register value *v;
register unsigned int n;
dataplane *plane;
{
    register array *arr;

    while (n != 0) {
	if (T_INDEXED(v->type)) {
	    arr = v->u.array;
	    if (arr->primary->state == AR_ALOCAL &&
		arr->primary->values != plane) {
		arr->primary = &plane->alocal;
		if (arr->hashed != (struct _maphash_ *) NULL) {
		    map_compact(arr);
		}
		commit_values(arr->elts, arr->size, plane);
	    }

	}
	v++;
	--n;
    }
}

/*
 * NAME:	data->commit_plane()
 * DESCRIPTION:	commit the current data plane
 */
void d_commit_plane(level)
Int level;
{
    register dataplane *p, **r;
    register dataspace *data;
    register value *v;
    register arrref *a;
    register Uint i;

    for (r = &plist, p = *r; p != (dataplane *) NULL && p->level == level;
	 p = *r) {
	if (p->prev->level == level - 1) {
	    /*
	     * commit changes to previous plane
	     */
	    p->prev->flags = p->flags;
	    p->prev->achange = p->achange;
	    p->prev->imports = p->imports;

	    data = p->alocal.data;
	    if (p->original != (value *) NULL) {
		/* free backed-up variable values */
		for (v = p->original, i = data->nvariables; i != 0; v++, --i) {
		    i_del_value(v);
		}
		FREE(p->original);
		commit_values(data->variables, data->nvariables, p->prev);
	    }

	    if (p->coptab != (coptable *) NULL) {
		/* commit callout changes */
		commit_callouts(p);
		if (p->level == 1) {
		    cop_clean(p);
		} else {
		    p->prev->coptab = p->coptab;
		}
	    }

	    arr_commit(&p->achunk, p->prev);
	    if (p->arrays != (arrref *) NULL) {
		/* replace old array refs */
		for (a = p->prev->arrays, i = data->narrays; i != 0; a++, --i) {
		    if (a->arr != (array *) NULL) {
			arr_del(a->arr);
		    }
		}
		FREE(p->prev->arrays);
		p->prev->arrays = p->arrays;
	    }

	    data->values = p->prev;
	    *r = p->plist;
	    FREE(p);
	} else {
	    /*
	     * move plane to previous level
	     */
	    p->level--;
	    r = &p->plist;
	}
    }
}

/*
 * NAME:	data->discard_plane()
 * DESCRIPTION:	discard the current data plane without committing it
 */
void d_discard_plane(level)
Int level;
{
    register dataplane *p;
    register dataspace *data;
    register value *v;
    register arrref *a;
    register Uint i;

    for (p = plist; p != (dataplane *) NULL && p->level == level; p = p->plist)
    {
	/*
	 * discard changes except for callout mods
	 */
	p->prev->flags |= p->flags & (MOD_CALLOUT | MOD_NEWCALLOUT);

	data = p->alocal.data;
	if (p->original != (value *) NULL) {
	    /* restore original variable values */
	    for (v = data->variables, i = data->nvariables; i != 0; --i, v++) {
		i_del_value(v);
	    }
	    memcpy(data->variables, p->original,
		   data->nvariables * sizeof(value));
	    FREE(p->original);
	}

	if (p->coptab != (coptable *) NULL) {
	    /* undo callout changes */
	    discard_callouts(p);
	    if (p->level == 1) {
		cop_clean(p);
	    } else {
		p->prev->coptab = p->coptab;
	    }
	}

	arr_discard(&p->achunk);
	if (p->arrays != (arrref *) NULL) {
	    /* delete new array refs */
	    for (a = p->arrays, i = data->narrays; i != 0; a++, --i) {
		if (a->arr != (array *) NULL) {
		    arr_del(a->arr);
		}
	    }
	    FREE(p->arrays);
	    /* fix old ones */
	    for (a = p->prev->arrays, i = data->narrays; i != 0; a++, --i) {
		if (a->arr != (array *) NULL) {
		    a->arr->primary = a;
		}
	    }
	}

	data->values = p->prev;
	plist = p->plist;
	FREE(p);
    }
}

/*
 * NAME:	data->commit_arr()
 * DESCRIPTION:	commit array to previous plane
 */
abchunk **d_commit_arr(arr, prev, old)
register array *arr;
dataplane *prev, *old;
{
    if (arr->primary->values != prev) {
	if (arr->primary->state == AR_ALOCAL) {
	    arr->primary = &prev->alocal;
	} else {
	    arr->primary->values = prev;
	}

	if (arr->hashed != (struct _maphash_ *) NULL) {
	    map_compact(arr);
	}
	commit_values(arr->elts, arr->size, prev);
    }

    return (prev == old) ? (abchunk **) NULL : &prev->achunk;
}

/*
 * NAME:	data->discard_arr()
 * DESCRIPTION:	restore array to previous state, if necessary
 */
void d_discard_arr(arr, plane)
register array *arr;
dataplane *plane;
{
    if (arr->primary->state == AR_ALOCAL) {
	arr->primary = &plane->alocal;
    }
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

    data = arr->primary->data;
    for (n = arr->size, v = arr->elts; n > 0; --n, v++) {
	if (T_INDEXED(v->type) && data != v->u.array->primary->data) {
	    /* mark as imported */
	    if (data->values->imports++ == 0 &&
		data->ilist == (dataspace *) NULL && ilast != data) {
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
 * NAME:	data->assign_var()
 * DESCRIPTION:	assign a value to a variable
 */
void d_assign_var(data, var, val)
register dataspace *data;
register value *var;
register value *val;
{
    if (var >= data->variables && var < data->variables + data->nvariables) {
	if (data->values->level != 0 &&
	    data->values->original == (value *) NULL) {
	    /*
	     * back up variables
	     */
	    i_copy(data->values->original = ALLOC(value, data->nvariables),
		   data->variables, data->nvariables);
	}
	ref_rhs(data, val);
	del_lhs(data, var);
	data->values->flags |= MOD_VARIABLE;
    }

    i_ref_value(val);
    i_del_value(var);

    *var = *val;
    var->modified = TRUE;
}

/*
 * NAME:	data->wipe_extravar()
 * DESCRIPTION:	wipe the value of the extra variable
 */
void d_wipe_extravar(data)
register dataspace *data;
{
    d_assign_var(data, d_get_variable(data, data->nvariables - 1), &nil_value);

    if (data->parser != (struct _parser_ *) NULL) {
	/*
	 * get rid of the parser, too
	 */
	ps_del(data->parser);
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

    if (data->values->level != arr->primary->data->values->level) {
	/*
	 * bring dataspace of imported array up to the current plane level
	 */
	d_new_plane(arr->primary->data, data->values->level);
    }

    data = arr->primary->data;
    if (arr->primary->values != data->values) {
	/*
	 * backup array's current elements
	 */
	arr_backup(&data->values->achunk, arr, arr->primary->values);
	if (arr->primary->state != AR_ALOCAL) {
	    arr->primary->values = data->values;
	} else {
	    arr->primary = &data->values->alocal;
	}
    }

    if (arr->primary->arr != (array *) NULL) {
	/*
	 * the array is in the loaded dataspace of some object
	 */
	if ((arr->primary->ref & ARR_MOD) == 0) {
	    arr->primary->ref |= ARR_MOD;
	    data->values->flags |= MOD_ARRAY;
	}
	ref_rhs(data, val);
	del_lhs(data, elt);
    } else {
	if (T_INDEXED(val->type) && data != val->u.array->primary->data) {
	    /* mark as imported */
	    if (data->values->imports++ == 0 &&
		data->ilist == (dataspace *) NULL &&
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
	    data->values->imports--;
	}
    }

    i_ref_value(val);
    i_del_value(elt);

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
	a->values->achange++;
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
    cbuf *q;
    value v[4];
    uindex handle;

    ct = co_check(ncallout, delay, mdelay, &t, &m, &q);
    if (ct == 0 && q == (cbuf *) NULL) {
	/* callouts are disabled */
	return 0;
    }
    if (data->ncallouts >= conf_array_size() && data->fcallouts == 0) {
	error("Too many callouts in object");
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
	v[1] = *f->sp++;
	v[2] = *f->sp++;
	PUT_ARRVAL(&v[3], arr_new(data, nargs - 2L));
	memcpy(v[3].u.array->elts, f->sp, (nargs - 2) * sizeof(value));
	d_ref_imports(v[3].u.array);
	break;
    }
    f->sp += nargs;
    handle = d_alloc_call_out(data, 0, ct, nargs, v);

    if (data->values->level == 0) {
	/*
	 * add normal callout
	 */
	co_new(data->oindex, handle, t, m, q);
    } else {
	register dataplane *plane;
	register copatch **c, *cop;
	dcallout *co;
	uindex i;

	/*
	 * add callout patch
	 */
	plane = data->values;
	if (plane->coptab == (coptable *) NULL) {
	    cop_init(plane);
	}
	co = &data->callouts[handle - 1];
	i = handle % COPATCHHTABSZ;
	c = &plane->coptab->cop[i];
	for (;;) {
	    cop = *c;
	    if (cop == (copatch *) NULL || cop->plane != plane) {
		/* add new */
		cop_new(plane, &plane->coptab->cop[i], COP_ADD,
			handle, co, t, m, q);
		break;
	    }

	    if (cop->handle == handle) {
		/* replace removed */
		cop_replace(cop, co, t, m, q);
		break;
	    }

	    c = &cop->next;
	}

	ncallout++;
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
	d_get_callouts(data);
    }

    co = &data->callouts[handle - 1];
    if (co->val[0].type != T_STRING) {
	/* invalid callout */
	return -1;
    }

    t = co_remaining(co->time);
    if (data->values->level == 0) {
	/*
	 * remove normal callout
	 */
	co_del(data->oindex, handle, co->time);
    } else {
	register dataplane *plane;
	register copatch **c, *cop;
	register value *v;
	uindex i;

	/*
	 * add/remove callout patch
	 */
	--ncallout;

	plane = data->values;
	if (plane->coptab == (coptable *) NULL) {
	    cop_init(plane);
	}
	i = handle % COPATCHHTABSZ;
	c = &plane->coptab->cop[i];
	for (;;) {
	    cop = *c;
	    if (cop == (copatch *) NULL || cop->plane != plane) {
		/* delete new */
		cop_new(plane, &plane->coptab->cop[i], COP_REMOVE,
			handle, co, (Uint) 0, 0, (cbuf *) NULL);
		break;
	    }
	    if (cop->handle == handle) {
		/* delete existing */
		cop_del(plane, c, TRUE);
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
dataspace *data;
unsigned int handle;
register frame *f;
int *nargs;
{
    string *str;
    register dcallout *co;
    register value *v;
    register uindex n;

    if (data->callouts == (dcallout *) NULL) {
	d_get_callouts(data);
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
	FREE(v[3].u.array->elts);
	v[3].u.array->elts = (value *) NULL;
	arr_del(v[3].u.array);
	del_lhs(data, &v[2]);
	*--f->sp = v[2];
	del_lhs(data, &v[1]);
	*--f->sp = v[1];
	break;
    }

    /* wipe out destructed objects */
    for (n = co->nargs, v = f->sp; n > 0; --n, v++) {
	if (v->type == T_OBJECT && DESTRUCTED(v)) {
	    *v = nil_value;
	}
    }

    co->val[0] = nil_value;
    n = data->fcallouts;
    if (n != 0) {
	data->callouts[n - 1].co_prev = handle;
    }
    co->co_next = n;
    data->fcallouts = handle;

    data->values->flags |= MOD_CALLOUT;
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
	d_get_callouts(data);
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
    co_list(list);

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
		if (v->u.array->hashed != (struct _maphash_ *) NULL) {
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
	case T_NIL:
	    sv->oindex = 0;
	    sv->u.number = 0;
	    break;

	case T_INT:
	    sv->oindex = 0;
	    sv->u.number = v->u.number;
	    break;

	case T_STRING:
	    i = str_put(v->u.string, nstr);
	    sv->oindex = 0;
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

	case T_FLOAT:
	case T_OBJECT:
	    sv->oindex = v->oindex;
	    sv->u.objcnt = v->u.objcnt;
	    break;

	case T_ARRAY:
	case T_MAPPING:
	    i = arr_put(v->u.array);
	    sv->oindex = 0;
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
		sv->u.string = v->u.string->primary - sdata->strings;
		break;

	    case T_FLOAT:
	    case T_OBJECT:
		sv->oindex = v->oindex;
		sv->u.objcnt = v->u.objcnt;
		break;

	    case T_ARRAY:
	    case T_MAPPING:
		sv->oindex = 0;
		sv->u.array = v->u.array->primary - sdata->values->arrays;
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
    if (data->values->arrays != (arrref *) NULL) {
	register arrref *a;

	for (i = data->narrays, a = data->values->arrays; i > 0; --i, a++) {
	    if (a->arr != (array *) NULL) {
		arr_del(a->arr);
	    }
	}

	FREE(data->values->arrays);
	data->values->arrays = (arrref *) NULL;
    }

    /* free strings */
    if (data->strings != (strref *) NULL) {
	register strref *s;

	for (i = data->nstrings, s = data->strings; i > 0; --i, s++) {
	    if (s->str != (string *) NULL) {
		s->str->primary = (strref *) NULL;
		str_del(s->str);
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
static bool d_save_dataspace(data)
register dataspace *data;
{
    sdataspace header;
    register Uint n;

    sdata = data;
    if (data->parser != (struct _parser_ *) NULL) {
	ps_save(data->parser);
    }
    if (data->values->flags == 0) {
	return FALSE;
    }

    if (data->nsectors != 0 && data->values->achange == 0 &&
	data->schange == 0 && !(data->values->flags & MOD_NEWCALLOUT)) {
	bool mod;

	/*
	 * No strings/arrays added or deleted. Check individual variables and
	 * array elements.
	 */
	if (data->values->flags & MOD_VARIABLE) {
	    /*
	     * variables changed
	     */
	    d_put_values(data->svariables, data->variables, data->nvariables);
	    sw_writev((char *) data->svariables, data->sectors,
		      data->nvariables * (Uint) sizeof(svalue),
		      data->varoffset);
	}
	if (data->values->flags & MOD_ARRAYREF) {
	    register sarray *sa;
	    register arrref *a;

	    /*
	     * references to arrays changed
	     */
	    sa = data->sarrays;
	    a = data->values->arrays;
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
	    if (mod) {
		sw_writev((char *) data->sarrays, data->sectors,
			  data->narrays * sizeof(sarray), data->arroffset);
	    }
	}
	if (data->values->flags & MOD_ARRAY) {
	    register arrref *a;
	    Uint idx;

	    /*
	     * array elements changed
	     */
	    a = data->values->arrays;
	    for (n = 0; n < data->narrays; n++) {
		if (a->arr != (array *) NULL && (a->ref & ARR_MOD)) {
		    a->ref &= ~ARR_MOD;
		    idx = data->sarrays[n].index;
		    d_put_values(&data->selts[idx], a->arr->elts, a->arr->size);
		    sw_writev((char *) &data->selts[idx], data->sectors,
			      a->arr->size * (Uint) sizeof(svalue),
			      data->arroffset + data->narrays * sizeof(sarray) +
				idx * sizeof(svalue));
		}
		a++;
	    }
	}
	if (data->values->flags & MOD_STRINGREF) {
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
	if (data->values->flags & MOD_CALLOUT) {
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
		if (co->val[0].type == T_STRING) {
		    co->val[0].modified = TRUE;
		    co->val[1].modified = TRUE;
		    co->val[2].modified = TRUE;
		    co->val[3].modified = TRUE;
		    d_put_values(sco->val, co->val,
				 (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
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
		scallouts = ALLOCA(scallout, n);
		for (co = data->callouts; n > 0; --n, co++) {
		    if (co->val[0].type == T_STRING) {
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
	 * put everything in a saveable form
	 */
	sstrings = data->sstrings =
		   REALLOC(data->sstrings, sstring, 0, header.nstrings);
	stext = data->stext = REALLOC(data->stext, char, 0, header.strsize);
	sarrays = data->sarrays =
		  REALLOC(data->sarrays, sarray, 0, header.narrays);
	selts = data->selts = REALLOC(data->selts, svalue, 0, header.eltsize);
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
		if (co->val[0].type == T_STRING) {
		    d_save(sco->val, co->val,
			   (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
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

	data->flags = header.flags;
	data->narrays = header.narrays;
	data->eltsize = header.eltsize;
	data->nstrings = header.nstrings;
	data->strsize = strsize;

	data->schange = 0;
	data->values->achange = 0;
    }

    data->values->flags = 0;
    return TRUE;
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
			d_get_elts(a);
			a->primary = &data->values->alocal;
		    } else {
			/*
			 * make new array
			 */
			a = arr_alloc(a->size);
			a->tag = val->u.array->tag;
			a->odcount = val->u.array->odcount;
			a->primary = &data->values->alocal;

			if (a->size > 0) {
			    /*
			     * copy elements
			     */
			    i_copy(a->elts = ALLOC(value, a->size),
				   d_get_elts(val->u.array), a->size);
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
			/*
			 * increase size of itab
			 */
			for (j = itabsz; j <= i; j += j) ;
			itab = REALLOC(itab, array*, itabsz, j);
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
	    if (data->values->imports != 0) {
		narr = 0;
		if (data->variables != (value *) NULL) {
		    d_import(data, data->variables, data->nvariables);
		}
		if (data->values->arrays != (arrref *) NULL) {
		    register arrref *a;

		    for (n = data->narrays, a = data->values->arrays; n > 0;
			 --n, a++) {
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
			if (co->val[0].type == T_STRING) {
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
	    data->values->imports = 0;
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
	i_del_value(v++);
    }

    /* replace old with new */
    FREE(data->variables);
    data->variables = vars;

    data->values->flags |= MOD_VARIABLE;
    if (data->nvariables != nvar) {
	if (data->svariables != (svalue *) NULL) {
	    FREE(data->svariables);
	    data->svariables = (svalue *) NULL;
	}
	data->nvariables = nvar;
	data->values->achange++;	/* force rebuild on swapout */
    }

    o_upgraded(old, OBJ(data->oindex));
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
    obj = OBJ(data->oindex);
    update = obj->update;
    obj = OBJ(obj->u_master);
    old = OBJ(obj->prev);
    if (O_UPGRADING(obj)) {
	/* in the middle of an upgrade */
	old = OBJ(old->prev);
    }
    nvar = data->ctrl->nvariables + 1;
    vmap = o_control(old)->vmap;

    if (old->update != update) {
	register unsigned short *m1, *m2, n;

	m1 = vmap;
	vmap = ALLOCA(unsigned short, n = nvar);
	do {
	    old = OBJ(old->prev);
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
    register object *obj;

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
	obj = OBJ(data->oindex);
	if (!(obj->flags & O_MASTER) && obj->u_master == new->index) {
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

    if (ctrl->oindex != UINDEX_MAX) {
	OBJ(ctrl->oindex)->ctrl = (control *) NULL;
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

    OBJ(data->oindex)->data = (dataspace *) NULL;
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
    --ndata;

    FREE(data);
}


/*
 * NAME:	data->swapout()
 * DESCRIPTION:	Swap out a portion of the control and dataspace blocks in
 *		memory.  Return the number of dataspace blocks swapped out.
 */
uindex d_swapout(frag)
unsigned int frag;
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
	if (!(OBJ(data->oindex)->flags & O_PENDIO) || frag == 1) {
	    if (d_save_dataspace(data)) {
		count++;
	    }
	    d_free_dataspace(data);
	}
	data = prev;
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
	d_save_dataspace(data);
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
void d_conv_control(oindex)
unsigned int oindex;
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
    size = d_conv((char *) &header, &obj->cfirst, sc_layout, (Uint) 1,
		  (Uint) 0);
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

	AFREE(scallouts);
    }

    AFREE(s);

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->u_master)->update) {
	/* handle object upgrading right away */
	data->ctrl = o_control(obj);
	data->ctrl->ndata++;
	d_upgrade_clone(data);
    }

    data->values->flags |= MOD_ALL;
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
	sw_wipev(ctrl->sectors, ctrl->nsectors);
	sw_delv(ctrl->sectors, ctrl->nsectors);
    }
    if (ctrl->oindex != UINDEX_MAX) {
	OBJ(ctrl->oindex)->cfirst = SW_UNUSED;
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
	    if ((--co)->val[0].type == T_STRING) {
		d_del_call_out(data, n);
	    }
	}
    }
    if (data->sectors != (sector *) NULL) {
	sw_wipev(data->sectors, data->nsectors);
	sw_delv(data->sectors, data->nsectors);
    }
    OBJ(data->oindex)->dfirst = SW_UNUSED;
    d_free_dataspace(data);
}
