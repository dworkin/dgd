# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "sdata.h"
# include "interpret.h"
# include "csupport.h"


# define DALLOC(type, size)	ALLOC(datapool, type, size)
# define DALLOCA(type, size)	ALLOCA(datapool, type, size)
# define DREALLOC(mem, type, size1, size2) \
				REALLOC(datapool, mem, type, size1, size2)
# define DFREE(mem)		FREE(datapool, mem)
# define DFREEA(mem)		FREEA(datapool, mem)

/* bit values for sctrl->flags */
# define CTRL_PROGCMP		0x0003	/* program compressed */
# define CTRL_STRCMP		0x000c	/* strings compressed */
# define CTRL_MODIFIED		0x0010	/* modified since last saved */

/* bit values for sdataspace->flags */
# define DATA_STRCMP		0x0003	/* strings compressed */
# define DATA_MODIFIED		0x0004	/* modified since last saved */

/* data compression */
# define CMP_TYPE		0x03
# define CMP_NONE		0x00	/* no compression */
# define CMP_PRED		0x01	/* predictor compression */

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
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
    unsigned short nifdefs;	/* # int/float definitions */
    unsigned short nvinit;	/* # variables requiring initialization */
    unsigned short vmapsize;	/* size of variable map, or 0 for none */
} cheader;

static char sc_layout[] = "dcciisiccusssss";

typedef struct {
    uindex oindex;		/* index in object table */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset + private bit */
} sinherit;

static char si_layout[] = "uus";

typedef struct _scontrol_ {
    sector nsectors;		/* # of sectors */
    sector *sectors;		/* vector with sectors */

    uindex oindex;		/* object */

    short flags;		/* various bitflags */

    short ninherits;		/* # inherited objects */
    sinherit *inherits;		/* inherit objects */

    Uint compiled;		/* time of compilation */

    char *prog;			/* program text */
    Uint progsize;		/* program text size */
    Uint progoffset;		/* program text offset */

    unsigned short nstrings;	/* # strings */
    dstrconst *sstrings;	/* sstrings */
    char *stext;		/* sstrings text */
    Uint strsize;		/* sstrings text size */
    Uint stroffset;		/* offset of string index table */

    unsigned short nfuncdefs;	/* # function definitions */
    dfuncdef *funcdefs;		/* function definition table */
    Uint funcdoffset;		/* offset of function definition table */

    unsigned short nvardefs;	/* # variable definitions */
    dvardef *vardefs;		/* variable definitions */
    Uint vardoffset;		/* offset of variable definition table */

    uindex nfuncalls;		/* # function calls */
    char *funcalls;		/* function calls */
    Uint funccoffset;		/* offset of function call table */

    unsigned short nsymbols;	/* # symbols */
    dsymbol *symbols;		/* symbol table */
    Uint symboffset;		/* offset of symbol table */

    unsigned short nvariables;	/* # variables */
    unsigned short nifdefs;	/* # int/float definitions */
    unsigned short nvinit;	/* # variables requiring initialization */

    unsigned short vmapsize;	/* size of variable mapping */
    unsigned short *vmap;	/* variable mapping */
} scontrol;

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
} dheader;

static char sd_layout[] = "dssiiiiuu";

typedef struct {
    Uint time;			/* time of call */
    unsigned short nargs;	/* number of arguments */
    svalue val[4];		/* function name, 3 direct arguments */
} scallout;

static char sco_layout[] = "is[sui][sui][sui][sui]";

typedef struct _sdataspace_ {
    sector *sectors;		/* vector of sectors */
    sector nsectors;		/* # sectors */

    short flags;		/* various bitflags */
    uindex oindex;		/* object this dataspace belongs to */

    unsigned short nvariables;	/* # variables */
    svalue *svariables;		/* svariables */
    Uint varoffset;		/* offset of variables in data space */

    Uint narrays;		/* # arrays */
    Uint eltsize;		/* total size of array elements */
    sarray *sarrays;		/* sarrays */
    svalue *selts;		/* sarray elements */
    Uint arroffset;		/* offset of array table in data space */

    Uint nstrings;		/* # strings */
    Uint strsize;		/* total size of string text */
    sstring *sstrings;		/* sstrings */
    char *stext;		/* sstrings text */
    Uint stroffset;		/* offset of string table */

    uindex ncallouts;		/* # callouts */
    uindex fcallouts;		/* free callout list */
    scallout *scallouts;	/* scallouts */
    Uint cooffset;		/* offset of callout table */
} sdataspace;

typedef struct {
    lpcenv *env;			/* LPC environment */
    struct _arrmerge_ *amerge;		/* array merge table */
    struct _strmerge_ *smerge;		/* string merge table */
    Uint narr;				/* # of arrays */
    Uint nstr;				/* # of strings */
    Uint arrsize;			/* # of array elements */
    Uint strsize;			/* total string size */
    sarray *sarrays;			/* save arrays */
    svalue *selts;			/* save array elements */
    sstring *sstrings;			/* save strings */
    char *stext;			/* save string elements */
    Uint *counttab;			/* object count table */
} savedata;

static struct _mempool_ *datapool;	/* dataspace memory pool */


/*
 * NAME:	sdata->init()
 * DESCRIPTION:	initialize swapped data handling
 */
void sd_init(pool)
struct _mempool_ *pool;
{
    datapool = pool;
}

/*
 * NAME:	sdata->new_scontrol()
 * DESCRIPTION:	create a new scontrol block
 */
static scontrol *sd_new_scontrol(oindex)
unsigned int oindex;
{
    register scontrol *sctrl;

    sctrl = DALLOC(scontrol, 1);

    sctrl->flags = 0;

    sctrl->nsectors = 0;		/* nothing on swap device yet */
    sctrl->sectors = (sector *) NULL;
    sctrl->oindex = oindex;
    sctrl->ninherits = 0;
    sctrl->inherits = (sinherit *) NULL;
    sctrl->progsize = 0;
    sctrl->prog = (char *) NULL;
    sctrl->nstrings = 0;
    sctrl->sstrings = (dstrconst *) NULL;
    sctrl->stext = (char *) NULL;
    sctrl->nfuncdefs = 0;
    sctrl->funcdefs = (dfuncdef *) NULL;
    sctrl->nvardefs = 0;
    sctrl->vardefs = (dvardef *) NULL;
    sctrl->nfuncalls = 0;
    sctrl->funcalls = (char *) NULL;
    sctrl->nsymbols = 0;
    sctrl->symbols = (dsymbol *) NULL;
    sctrl->nvariables = 0;
    sctrl->nifdefs = 0;
    sctrl->nvinit = 0;
    sctrl->vmapsize = 0;
    sctrl->vmap = (unsigned short *) NULL;

    return sctrl;
}

/*
 * NAME:	sdata->alloc_sdataspace()
 * DESCRIPTION:	allocate a new sdataspace block
 */
static sdataspace *sd_alloc_sdataspace(oindex)
unsigned int oindex;
{
    register sdataspace *sdata;

    sdata = DALLOC(sdataspace, 1);
    sdata->oindex = oindex;

    sdata->flags = 0;

    /* sectors */
    sdata->nsectors = 0;
    sdata->sectors = (sector *) NULL;

    /* variables */
    sdata->nvariables = 0;
    sdata->svariables = (svalue *) NULL;

    /* arrays */
    sdata->narrays = 0;
    sdata->eltsize = 0;
    sdata->sarrays = (sarray *) NULL;
    sdata->selts = (svalue *) NULL;

    /* strings */
    sdata->nstrings = 0;
    sdata->strsize = 0;
    sdata->sstrings = (sstring *) NULL;
    sdata->stext = (char *) NULL;

    /* callouts */
    sdata->ncallouts = 0;
    sdata->fcallouts = 0;
    sdata->scallouts = (scallout *) NULL;

    return sdata;
}

/*
 * NAME:	sdata->load_scontrol()
 * DESCRIPTION:	load a scontrol block from the swap device
 */
static scontrol *sd_load_scontrol(obj)
register object *obj;
{
    cheader header;
    register scontrol *sctrl;
    register Uint size;

    sctrl = sd_new_scontrol(obj->index);

    /* header */
    sw_readv((char *) &header, &obj->cfirst, (Uint) sizeof(cheader), (Uint) 0);
    sctrl->nsectors = header.nsectors;
    sctrl->sectors = DALLOC(sector, header.nsectors);
    sctrl->sectors[0] = obj->cfirst;
    size = header.nsectors * (Uint) sizeof(sector);
    if (header.nsectors > 1) {
	sw_readv((char *) sctrl->sectors, sctrl->sectors, size,
		 (Uint) sizeof(cheader));
    }
    size += sizeof(cheader);

    sctrl->flags = header.flags;

    /* compile time */
    sctrl->compiled = header.compiled;

    if (header.vmapsize != 0) {
	/*
	 * Control block for outdated issue; only vmap can be loaded.
	 * The load offsets will be invalid (and unused).
	 */
	sctrl->vmapsize = header.vmapsize;
	sctrl->vmap = DALLOC(unsigned short, header.vmapsize);
	sw_readv((char *) sctrl->vmap, sctrl->sectors,
		 header.vmapsize * (Uint) sizeof(unsigned short), size);
    } else {
	/* inherits */
	sctrl->ninherits = UCHAR(header.ninherits);
	sctrl->inherits = DALLOC(sinherit, sctrl->ninherits);
	sw_readv((char *) sctrl->inherits, sctrl->sectors,
		 sctrl->ninherits * (Uint) sizeof(sinherit), size);
	size += UCHAR(header.ninherits) * sizeof(sinherit);

	/* program */
	sctrl->progoffset = size;
	sctrl->progsize = header.progsize;
	size += header.progsize;

	/* string constants */
	sctrl->stroffset = size;
	sctrl->nstrings = header.nstrings;
	sctrl->strsize = header.strsize;
	size += header.nstrings * (Uint) sizeof(dstrconst) + header.strsize;

	/* function definitions */
	sctrl->funcdoffset = size;
	sctrl->nfuncdefs = UCHAR(header.nfuncdefs);
	size += UCHAR(header.nfuncdefs) * (Uint) sizeof(dfuncdef);

	/* variable definitions */
	sctrl->vardoffset = size;
	sctrl->nvardefs = UCHAR(header.nvardefs);
	size += UCHAR(header.nvardefs) * (Uint) sizeof(dvardef);

	/* function call table */
	sctrl->funccoffset = size;
	sctrl->nfuncalls = header.nfuncalls;
	size += header.nfuncalls * (Uint) 2;

	/* symbol table */
	sctrl->symboffset = size;
	sctrl->nsymbols = header.nsymbols;

	/* # variables */
	sctrl->nvariables = header.nvariables;
	sctrl->nifdefs = header.nifdefs;
	sctrl->nvinit = header.nvinit;
    }

    return sctrl;
}

/*
 * NAME:	sdata->load_control()
 * DESCRIPTION:	load a control block from the swap device
 */
control *sd_load_control(env, obj)
lpcenv *env;
register object *obj;
{
    register control *ctrl;

    ctrl = d_new_control(env);
    ctrl->oindex = obj->index;

    if (obj->flags & O_COMPILED) {
	/* initialize control block of compiled object */
	pc_control(ctrl, obj);
	ctrl->flags |= CTRL_COMPILED;
    } else {
	register scontrol *sctrl;

	ctrl->sctrl = sctrl = sd_load_scontrol(obj);

	/* compile time */
	ctrl->compiled = sctrl->compiled;

	if (sctrl->vmapsize != 0) {
	    ctrl->vmapsize = sctrl->vmapsize;
	    ctrl->vmap = sctrl->vmap;
	} else {
	    register int n;
	    register dinherit *inherits;
	    register sinherit *sinherits;

	    /* load inherits */
	    ctrl->ninherits = n = sctrl->ninherits;	/* at least one */
	    ctrl->inherits = inherits = IALLOC(env, dinherit, n);
	    sinherits = sctrl->inherits;
	    do {
		inherits->oindex = sinherits->oindex;
		inherits->funcoffset = sinherits->funcoffset;
		inherits->varoffset = sinherits->varoffset & ~PRIV;
		(inherits++)->priv = (((sinherits++)->varoffset & PRIV) != 0);
	    } while (--n > 0);
	}

	/* program */
	ctrl->progsize = sctrl->progsize;

	/* string constants */
	ctrl->nstrings = sctrl->nstrings;
	ctrl->strsize = sctrl->strsize;

	/* function definitions */
	ctrl->nfuncdefs = sctrl->nfuncdefs;

	/* variable definitions */
	ctrl->nvardefs = sctrl->nvardefs;

	/* function call table */
	ctrl->nfuncalls = sctrl->nfuncalls;

	/* symbol table */
	ctrl->nsymbols = sctrl->nsymbols;

	/* # variables */
	ctrl->nvariables = sctrl->nvariables;
	ctrl->nifdefs = sctrl->nifdefs;
	ctrl->nvinit = sctrl->nvinit;
    }

    return ctrl;
}

/*
 * NAME:	sdata->load_sdataspace()
 * DESCRIPTION:	load the dataspace header block of an object from the swap
 */
static sdataspace *sd_load_sdataspace(obj)
object *obj;
{
    dheader header;
    register sdataspace *sdata;
    register Uint size;

    sdata = sd_alloc_sdataspace(obj->index);

    /* header */
    sw_readv((char *) &header, &obj->dfirst, (Uint) sizeof(dheader), (Uint) 0);
    sdata->nsectors = header.nsectors;
    sdata->sectors = DALLOC(sector, header.nsectors);
    sdata->sectors[0] = obj->dfirst;
    size = header.nsectors * (Uint) sizeof(sector);
    if (header.nsectors > 1) {
	sw_readv((char *) sdata->sectors, sdata->sectors, size,
		 (Uint) sizeof(dheader));
    }
    size += sizeof(dheader);

    sdata->flags = header.flags;

    /* variables */
    sdata->varoffset = size;
    sdata->nvariables = header.nvariables;
    size += sdata->nvariables * (Uint) sizeof(svalue);

    /* arrays */
    sdata->arroffset = size;
    sdata->narrays = header.narrays;
    sdata->eltsize = header.eltsize;
    size += header.narrays * (Uint) sizeof(sarray) +
	    header.eltsize * sizeof(svalue);

    /* strings */
    sdata->stroffset = size;
    sdata->nstrings = header.nstrings;
    sdata->strsize = header.strsize;
    size += header.nstrings * sizeof(sstring) + header.strsize;

    /* callouts */
    sdata->cooffset = size;
    sdata->ncallouts = header.ncallouts;
    sdata->fcallouts = header.fcallouts;

    return sdata;
}

/*
 * NAME:	sdata->load_dataspace()
 * DESCRIPTION:	load the dataspace header block of an object from the swap
 */
dataspace *sd_load_dataspace(env, obj)
lpcenv *env;
object *obj;
{
    register dataspace *data;
    register sdataspace *sdata;

    data = d_alloc_dataspace(env, obj);
    data->sdata = sdata = sd_load_sdataspace(obj);
    data->ctrl = o_control(env, obj);
    data->ctrl->ndata++;

    data->nvariables = sdata->nvariables;
    data->narrays = sdata->narrays;
    data->nstrings = sdata->nstrings;
    data->strsize = sdata->strsize;
    data->ncallouts = sdata->ncallouts;
    data->fcallouts = sdata->fcallouts;

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->u_master)->update &&
	obj->count != 0) {
	d_upgrade_clone(data);
    }

    return data;
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
    q = DALLOC(char, *dsize);
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
 * NAME:	sdata->get_csize()
 * DESCRIPTION:	return the number of sectors taken by a scontrol block
 */
sector sd_get_csize(sctrl)
scontrol *sctrl;
{
    return sctrl->nsectors;
}

/*
 * NAME:	sdata->get_prog()
 * DESCRIPTION:	get the program
 */
char *sd_get_prog(sctrl, progsize)
register scontrol *sctrl;
Uint *progsize;
{
    if (sctrl->prog == (char *) NULL) {
	if (sctrl->flags & CTRL_PROGCMP) {
	    sctrl->prog = decompress(sctrl->sectors, sw_readv, sctrl->progsize,
				     sctrl->progoffset, &sctrl->progsize);
	    *progsize = sctrl->progsize;
	} else {
	    sctrl->prog = DALLOC(char, sctrl->progsize);
	    sw_readv(sctrl->prog, sctrl->sectors, sctrl->progsize,
		     sctrl->progoffset);
	}
    }
    return sctrl->prog;
}

/*
 * NAME:	sdata->get_strconsts()
 * DESCRIPTION:	get string constant table
 */
dstrconst *sd_get_strconsts(sctrl)
register scontrol *sctrl;
{
    if (sctrl->sstrings == (dstrconst *) NULL) {
	sctrl->sstrings = DALLOC(dstrconst, sctrl->nstrings);
	sw_readv((char *) sctrl->sstrings, sctrl->sectors,
		 sctrl->nstrings * (Uint) sizeof(dstrconst),
		 sctrl->stroffset);
    }
    return sctrl->sstrings;
}

/*
 * NAME:	sdata->get_ctext()
 * DESCRIPTION:	load control block strings text
 */
char *sd_get_ctext(sctrl, strsize)
register scontrol *sctrl;
Uint *strsize;
{
    if (sctrl->stext == (char *) NULL) {
	/* load strings text */
	if (sctrl->flags & CTRL_STRCMP) {
	    sctrl->stext = decompress(sctrl->sectors, sw_readv,
				      sctrl->strsize,
				      sctrl->stroffset +
				      sctrl->nstrings * sizeof(dstrconst),
				      &sctrl->strsize);
	    *strsize = sctrl->strsize;
	} else {
	    sctrl->stext = DALLOC(char, sctrl->strsize);
	    sw_readv(sctrl->stext, sctrl->sectors, sctrl->strsize,
		     sctrl->stroffset +
		     sctrl->nstrings * (Uint) sizeof(dstrconst));
	}
    }
    return sctrl->stext;
}

/*
 * NAME:	sdata->get_funcdefs()
 * DESCRIPTION:	get function definitions
 */
dfuncdef *sd_get_funcdefs(sctrl)
register scontrol *sctrl;
{
    if (sctrl->funcdefs == (dfuncdef *) NULL) {
	sctrl->funcdefs = DALLOC(dfuncdef, sctrl->nfuncdefs);
	sw_readv((char *) sctrl->funcdefs, sctrl->sectors,
		 sctrl->nfuncdefs * (Uint) sizeof(dfuncdef),
		 sctrl->funcdoffset);
    }
    return sctrl->funcdefs;
}

/*
 * NAME:	sdata->get_vardefs()
 * DESCRIPTION:	get variable definitions
 */
dvardef *sd_get_vardefs(sctrl)
register scontrol *sctrl;
{
    if (sctrl->vardefs == (dvardef *) NULL) {
	sctrl->vardefs = DALLOC(dvardef, sctrl->nvardefs);
	sw_readv((char *) sctrl->vardefs, sctrl->sectors,
		 sctrl->nvardefs * (Uint) sizeof(dvardef), sctrl->vardoffset);
    }
    return sctrl->vardefs;
}

/*
 * NAME:	sdata->get_funcalls()
 * DESCRIPTION:	get function call table
 */
char *sd_get_funcalls(sctrl)
register scontrol *sctrl;
{
    if (sctrl->funcalls == (char *) NULL) {
	sctrl->funcalls = DALLOC(char, 2L * sctrl->nfuncalls);
	sw_readv((char *) sctrl->funcalls, sctrl->sectors,
		 sctrl->nfuncalls * (Uint) 2, sctrl->funccoffset);
    }
    return sctrl->funcalls;
}

/*
 * NAME:	data->get_symbols()
 * DESCRIPTION:	get symbol table
 */
dsymbol *sd_get_symbols(sctrl)
register scontrol *sctrl;
{
    if (sctrl->symbols == (dsymbol *) NULL) {
	sctrl->symbols = DALLOC(dsymbol, sctrl->nsymbols);
	sw_readv((char *) sctrl->symbols, sctrl->sectors,
		 sctrl->nsymbols * (Uint) sizeof(dsymbol), sctrl->symboffset);
    }
    return sctrl->symbols;
}


/*
 * NAME:	sdata->get_dsize()
 * DESCRIPTION:	return the number of sectors taken by a sdataspace block
 */
sector sd_get_dsize(sdata)
sdataspace *sdata;
{
    return sdata->nsectors;
}

/*
 * NAME:	sdata->get_svariables()
 * DESCRIPTION:	get svariables from swap
 */
svalue *sd_get_svariables(sdata)
register sdataspace *sdata;
{
    if (sdata->svariables == (svalue *) NULL) {
	/* load svalues */
	sdata->svariables = DALLOC(svalue, sdata->nvariables);
	sw_readv((char *) sdata->svariables, sdata->sectors,
		 sdata->nvariables * (Uint) sizeof(svalue), sdata->varoffset);
    }
    return sdata->svariables;
}

/*
 * NAME:	sdata->get_sstrings()
 * DESCRIPTION:	get strings
 */
sstring *sd_get_sstrings(sdata)
register sdataspace *sdata;
{
    if (sdata->sstrings == (sstring *) NULL) {
	/* load strings */
	sdata->sstrings = DALLOC(sstring, sdata->nstrings);
	sw_readv((char *) sdata->sstrings, sdata->sectors,
		 sdata->nstrings * sizeof(sstring), sdata->stroffset);
    }
    return sdata->sstrings;
}

/*
 * NAME:	sdata->get_dtext()
 * DESCRIPTION:	get strings text
 */
char *sd_get_dtext(sdata, strsize)
register sdataspace *sdata;
Uint *strsize;
{
    if (sdata->stext == (char *) NULL) {
	/* load strings text */
	if (sdata->flags & DATA_STRCMP) {
	    sdata->stext = decompress(sdata->sectors, sw_readv,
				      sdata->strsize,
				      sdata->stroffset +
					sdata->nstrings * sizeof(sstring),
				      &sdata->strsize);
	    *strsize = sdata->strsize;
	} else {
	    sdata->stext = DALLOC(char, sdata->strsize);
	    sw_readv(sdata->stext, sdata->sectors, sdata->strsize,
		     sdata->stroffset + sdata->nstrings * sizeof(sstring));
	}
    }
    return sdata->stext;
}

/*
 * NAME:	sdata->get_sarrays()
 * DESCRIPTION:	get arrays from swap
 */
sarray *sd_get_sarrays(sdata)
register sdataspace *sdata;
{
    if (sdata->sarrays == (sarray *) NULL) {
	sdata->sarrays = DALLOC(sarray, sdata->narrays);
	sw_readv((char *) sdata->sarrays, sdata->sectors,
		 sdata->narrays * (Uint) sizeof(sarray), sdata->arroffset);
    }
    return sdata->sarrays;
}

/*
 * NAME:	sdata->get_selts()
 * DESCRIPTION:	get array elements from swap
 */
svalue *sd_get_selts(sdata)
register sdataspace *sdata;
{
    if (sdata->selts == (svalue *) NULL) {
	sdata->selts = DALLOC(svalue, sdata->eltsize);
	sw_readv((char *) sdata->selts, sdata->sectors,
		 sdata->eltsize * sizeof(svalue),
		 sdata->arroffset + sdata->narrays * sizeof(sarray));
    }
    return sdata->selts;
}

/*
 * NAME:	sdata->load_callouts()
 * DESCRIPTION:	load callouts from swap
 */
void sd_load_callouts(data)
register dataspace *data;
{
    register sdataspace *sdata;
    register scallout *sco;
    register dcallout *co;
    register uindex n;

    sdata = data->sdata;
    sco = sdata->scallouts;
    if (sdata->scallouts == (scallout *) NULL) {
	sdata->scallouts = sco = DALLOC(scallout, sdata->ncallouts);
	sw_readv((char *) sco, sdata->sectors,
		 sdata->ncallouts * (Uint) sizeof(scallout), sdata->cooffset);
    }
    co = data->callouts = IALLOC(data->env, dcallout, data->ncallouts);

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

    s = *sectors = DREALLOC(*sectors, sector, nsectors, n);
    if (nsectors < n) {
	/* not enough sectors */
	sw_newv(s + nsectors, n - nsectors);
    }

    return n;
}

/*
 * NAME:	sdata->save_scontrol()
 * DESCRIPTION:	save an scontrol block
 */
static void sd_save_scontrol(sctrl)
register scontrol *sctrl;
{
    cheader header;
    char *prog, *stext;
    register Uint size;

    /* create header */
    memset(&header, '\0', sizeof(cheader));
    header.compiled = sctrl->compiled;
    header.vmapsize = sctrl->vmapsize;

    if (header.vmapsize != 0) {
	size = sizeof(cheader) +
	       header.vmapsize * (Uint) sizeof(unsigned short);
    } else {
	header.ninherits = sctrl->ninherits;
	header.progsize = sctrl->progsize;
	header.nstrings = sctrl->nstrings;
	header.strsize = sctrl->strsize;
	header.nfuncdefs = sctrl->nfuncdefs;
	header.nvardefs = sctrl->nvardefs;
	header.nfuncalls = sctrl->nfuncalls;
	header.nsymbols = sctrl->nsymbols;
	header.nvariables = sctrl->nvariables;
	header.nifdefs = sctrl->nifdefs;
	header.nvinit = sctrl->nvinit;

	prog = sctrl->prog;
	if (header.progsize >= CMPLIMIT) {
	    prog = DALLOCA(char, header.progsize);
	    size = compress(prog, sctrl->prog, header.progsize);
	    if (size != 0) {
		header.flags |= CMP_PRED;
		header.progsize = size;
	    } else {
		DFREEA(prog);
		prog = sctrl->prog;
	    }
	}

	stext = sctrl->stext;
	if (header.strsize >= CMPLIMIT) {
	    stext = DALLOCA(char, header.strsize);
	    size = compress(stext, sctrl->stext, header.strsize);
	    if (size != 0) {
		header.flags |= CMP_PRED << 2;
		header.strsize = size;
	    } else {
		DFREEA(stext);
		stext = sctrl->stext;
	    }
	}

	size = sizeof(cheader) +
	       UCHAR(header.ninherits) * sizeof(sinherit) +
	       header.progsize +
	       header.nstrings * (Uint) sizeof(dstrconst) +
	       header.strsize +
	       UCHAR(header.nfuncdefs) * sizeof(dfuncdef) +
	       UCHAR(header.nvardefs) * sizeof(dvardef) +
	       header.nfuncalls * (Uint) 2 +
	       header.nsymbols * (Uint) sizeof(dsymbol);
    }
    sctrl->nsectors = header.nsectors = d_swapalloc(size, sctrl->nsectors,
						    &sctrl->sectors);
    OBJ(sctrl->oindex)->cfirst = sctrl->sectors[0];

    /*
     * Copy everything to the swap device.
     */

    /* save header */
    sw_writev((char *) &header, sctrl->sectors, (Uint) sizeof(cheader),
	      (Uint) 0);
    size = sizeof(cheader);

    /* save sector map */
    sw_writev((char *) sctrl->sectors, sctrl->sectors,
	      header.nsectors * (Uint) sizeof(sector), size);
    size += header.nsectors * (Uint) sizeof(sector);

    if (header.vmapsize != 0) {
	/*
	 * save only vmap
	 */
	sw_writev((char *) sctrl->vmap, sctrl->sectors,
		  header.vmapsize * (Uint) sizeof(unsigned short), size);
    } else {
	/* save inherits */
	sw_writev((char *) sctrl->inherits, sctrl->sectors,
		  UCHAR(header.ninherits) * (Uint) sizeof(sinherit), size);
	size += UCHAR(header.ninherits) * sizeof(sinherit);

	/* save program */
	if (header.progsize > 0) {
	    sw_writev(prog, sctrl->sectors, (Uint) header.progsize, size);
	    size += header.progsize;
	    if (prog != sctrl->prog) {
		DFREEA(prog);
	    }
	}

	/* save string constants */
	if (header.nstrings > 0) {
	    sw_writev((char *) sctrl->sstrings, sctrl->sectors,
		      header.nstrings * (Uint) sizeof(dstrconst), size);
	    size += header.nstrings * (Uint) sizeof(dstrconst);
	    if (header.strsize > 0) {
		sw_writev(stext, sctrl->sectors, header.strsize, size);
		size += header.strsize;
		if (stext != sctrl->stext) {
		    DFREEA(stext);
		}
	    }
	}

	/* save function definitions */
	if (UCHAR(header.nfuncdefs) > 0) {
	    sw_writev((char *) sctrl->funcdefs, sctrl->sectors,
		      UCHAR(header.nfuncdefs) * (Uint) sizeof(dfuncdef), size);
	    size += UCHAR(header.nfuncdefs) * (Uint) sizeof(dfuncdef);
	}

	/* save variable definitions */
	if (UCHAR(header.nvardefs) > 0) {
	    sw_writev((char *) sctrl->vardefs, sctrl->sectors,
		      UCHAR(header.nvardefs) * (Uint) sizeof(dvardef), size);
	    size += UCHAR(header.nvardefs) * (Uint) sizeof(dvardef);
	}

	/* save function call table */
	if (header.nfuncalls > 0) {
	    sw_writev((char *) sctrl->funcalls, sctrl->sectors,
		      header.nfuncalls * (Uint) 2, size);
	    size += header.nfuncalls * (Uint) 2;
	}

	/* save symbol table */
	if (header.nsymbols > 0) {
	    sw_writev((char *) sctrl->symbols, sctrl->sectors,
		      header.nsymbols * (Uint) sizeof(dsymbol), size);
	}
    }
}

/*
 * NAME:	sdata->save_control()
 * DESCRIPTION:	save the control block
 */
void sd_save_control(env, ctrl)
lpcenv *env;
register control *ctrl;
{
    register scontrol *sctrl;
    register Uint i;
    register sinherit *sinherits;
    register dinherit *inherits;

    /*
     * Save a control block.
     */
    if (ctrl->sctrl == (scontrol *) NULL) {
	ctrl->sctrl = sctrl = sd_new_scontrol(ctrl->oindex);

	sctrl->compiled = ctrl->compiled;

	sctrl->ninherits = ctrl->ninherits;
	inherits = ctrl->inherits;
	sctrl->inherits = sinherits = DALLOC(sinherit, i = sctrl->ninherits);
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

	sctrl->progsize = ctrl->progsize;
	if (sctrl->progsize != 0) {
	    sctrl->prog = DALLOC(char, sctrl->progsize);
	    memcpy(sctrl->prog, ctrl->prog, sctrl->progsize);
	    IFREE(env, ctrl->prog);
	    ctrl->prog = sctrl->prog;
	}

	sctrl->nstrings = ctrl->nstrings;
	sctrl->strsize = ctrl->strsize;
	if (sctrl->nstrings != 0) {
	    register string **strs;
	    register Uint size;
	    register dstrconst *s;
	    register char *text;

	    sctrl->sstrings = DALLOC(dstrconst, sctrl->nstrings);
	    if (sctrl->strsize > 0) {
		sctrl->stext = DALLOC(char, sctrl->strsize);
	    }

	    strs = ctrl->strings;
	    size = 0;
	    s = sctrl->sstrings;
	    text = sctrl->stext;
	    for (i = sctrl->nstrings; i > 0; --i) {
		s->index = size;
		size += s->len = (*strs)->len;
		memcpy(text, (*strs++)->text, s->len);
		text += (s++)->len;
	    }

	    ctrl->sstrings = sctrl->sstrings;
	    ctrl->stext = sctrl->stext;
	}

	sctrl->nfuncdefs = ctrl->nfuncdefs;
	if (sctrl->nfuncdefs != 0) {
	    sctrl->funcdefs = DALLOC(dfuncdef, sctrl->nfuncdefs);
	    memcpy(sctrl->funcdefs, ctrl->funcdefs,
		   sctrl->nfuncdefs * sizeof(dfuncdef));
	    IFREE(env, ctrl->funcdefs);
	    ctrl->funcdefs = sctrl->funcdefs;
	}

	sctrl->nvardefs = ctrl->nvardefs;
	if (sctrl->nvardefs != 0) {
	    sctrl->vardefs = DALLOC(dvardef, sctrl->nvardefs);
	    memcpy(sctrl->vardefs, ctrl->vardefs,
		   sctrl->nvardefs * sizeof(dvardef));
	    IFREE(env, ctrl->vardefs);
	    ctrl->vardefs = sctrl->vardefs;
	}

	sctrl->nfuncalls = ctrl->nfuncalls;
	if (sctrl->nfuncalls != 0) {
	    sctrl->funcalls = DALLOC(char, sctrl->nfuncalls * 2L);
	    memcpy(sctrl->funcalls, ctrl->funcalls, sctrl->nfuncalls * 2L);
	    IFREE(env, ctrl->funcalls);
	    ctrl->funcalls = sctrl->funcalls;
	}

	sctrl->nsymbols = ctrl->nsymbols;
	if (sctrl->nsymbols != 0) {
	    sctrl->symbols = DALLOC(dsymbol, sctrl->nsymbols);
	    memcpy(sctrl->symbols, ctrl->symbols,
		   sctrl->nsymbols * sizeof(dsymbol));
	    IFREE(env, ctrl->symbols);
	    ctrl->symbols = sctrl->symbols;
	}

	sctrl->nvariables = ctrl->nvariables;
	sctrl->nifdefs = ctrl->nifdefs;
	sctrl->nvinit = ctrl->nvinit;
    }

    sctrl->vmapsize = ctrl->vmapsize;
    if (sctrl->vmapsize != 0) {
	sctrl->vmap = DALLOC(unsigned short,
			     sctrl->vmapsize * (long) sizeof(unsigned short));
	memcpy(sctrl->vmap, ctrl->vmap,
	       sctrl->vmapsize * (long) sizeof(unsigned short));
	IFREE(env, ctrl->vmap);
	ctrl->vmap = sctrl->vmap;
	ctrl->flags &= ~CTRL_VARMAP;
    }

    sd_save_scontrol(sctrl);
}

/*
 * NAME:	sdata->count()
 * DESCRIPTION:	recursively count the number of arrays and strings in an object
 */
static void sd_count(save, v, n)
register savedata *save;
register value *v;
register unsigned short n;
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
	case T_MAPPING:
	    if (arr_put(save->amerge, v->u.array, save->narr) == save->narr) {
		if (v->u.array->hashed != (struct _maphash_ *) NULL) {
		    map_compact(v->u.array);
		}
		save->narr++;
		save->arrsize += v->u.array->size;
		sd_count(save, d_get_elts(v->u.array), v->u.array->size);
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
	    if (elts->u.objcnt == count) {
		if (arr_put(save->amerge, v->u.array, save->narr) == save->narr)
		{
		    if (elts[1].u.number != obj->update) {
			d_upgrade_lwobj(save->env, v->u.array, obj);
			elts = v->u.array->elts;
		    }
		    if (save->counttab != (Uint *) NULL) {
			elts[1].u.number = 0;
		    }
		    save->narr++;
		    save->arrsize += v->u.array->size;
		    sd_count(save, elts, v->u.array->size);
		}
	    } else {
		*v = nil_value;
	    }
	    break;
	}

	v++;
	--n;
    }
}

/*
 * NAME:	sdata->save()
 * DESCRIPTION:	recursively save the values in an object
 */
static void sd_save(save, sv, v, n)
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
	    if (i == save->nstr) {
		/* new string value */
		save->sstrings[i].index = save->strsize;
		save->sstrings[i].len = v->u.string->len;
		save->sstrings[i].ref = 0;
		memcpy(save->stext + save->strsize, v->u.string->text,
		       v->u.string->len);
		save->strsize += v->u.string->len;
		save->nstr++;
	    }
	    save->sstrings[i].ref++;
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
	    if (i == save->narr) {
		svalue *tmp;

		/* new array */
		save->sarrays[i].index = save->arrsize;
		save->sarrays[i].size = v->u.array->size;
		save->sarrays[i].ref = 0;
		save->sarrays[i].tag = v->u.array->tag;
		tmp = save->selts + save->arrsize;
		save->arrsize += v->u.array->size;
		save->narr++;
		sd_save(save, tmp, v->u.array->elts, v->u.array->size);
	    }
	    save->sarrays[i].ref++;
	    break;
	}
	sv++;
	v++;
	--n;
    }
}

/*
 * NAME:	sdata->put_values()
 * DESCRIPTION:	save modified values as svalues
 */
static void sd_put_values(data, sv, v, n)
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
 * NAME:	sdata->save_svariables()
 * DESCRIPTION:	save svariables to swap
 */
static void sd_save_svariables(sdata)
register sdataspace *sdata;
{
    sw_writev((char *) sdata->svariables, sdata->sectors,
	      sdata->nvariables * (Uint) sizeof(svalue),
	      sdata->varoffset);
}

/*
 * NAME:	sdata->save_sarrays()
 * DESCRIPTION:	save sarrays to swap
 */
static void sd_save_sarrays(sdata)
register sdataspace *sdata;
{
    sw_writev((char *) sdata->sarrays, sdata->sectors,
	      sdata->narrays * sizeof(sarray), sdata->arroffset);
}

/*
 * NAME:	sdata->save_arrselts()
 * DESCRIPTION:	save array selts to swap
 */
static void sd_save_arrselts(sdata, n)
register sdataspace *sdata;
Uint n;
{
    register sarray *sa;

    sa = &sdata->sarrays[n];
    sw_writev((char *) &sdata->selts[sa->index], sdata->sectors,
	      sa->size * (Uint) sizeof(svalue),
	      sdata->arroffset + sdata->narrays * sizeof(sarray) +
	      sa->index * sizeof(svalue));
}

/*
 * NAME:	sdata->save_sstrings()
 * DESCRIPTION:	save sstrings to swap
 */
static void sd_save_sstrings(sdata)
register sdataspace *sdata;
{
    sw_writev((char *) sdata->sstrings, sdata->sectors,
	      sdata->nstrings * sizeof(sstring), sdata->stroffset);
}

/*
 * NAME:	sdata->save_scallouts()
 * DESCRIPTION:	save scallouts to swap
 */
static void sd_save_scallouts(sdata)
register sdataspace *sdata;
{
    dheader dummy;

    /* save new (?) fcallouts value */
    sw_writev((char *) &sdata->fcallouts, sdata->sectors, (Uint) sizeof(uindex),
	      (Uint) ((char *)&dummy.fcallouts - (char *)&dummy));

    /* save scallouts */
    sw_writev((char *) sdata->scallouts, sdata->sectors,
	      sdata->ncallouts * (Uint) sizeof(scallout),
	      sdata->cooffset);
}

/*
 * NAME:	sdata->save_sdataspace()
 * DESCRIPTION:	save dataspace to swap
 */
static void sd_save_sdataspace(sdata)
register sdataspace *sdata;
{
    dheader header;
    register char *text;
    register Uint size;

    header.flags = 0;
    header.nvariables = sdata->nvariables;
    header.narrays = sdata->narrays;
    header.eltsize = sdata->eltsize;
    header.nstrings = sdata->nstrings;
    header.strsize = sdata->strsize;
    header.ncallouts = sdata->ncallouts;
    header.fcallouts = sdata->fcallouts;

    text = sdata->stext;
    if (header.strsize >= CMPLIMIT) {
	text = DALLOCA(char, header.strsize);
	size = compress(text, sdata->stext, header.strsize);
	if (size != 0) {
	    header.flags |= CMP_PRED;
	    header.strsize = size;
	} else {
	    DFREEA(text);
	    text = sdata->stext;
	}
    }

    /* create sector space */
    size = sizeof(dheader) +
	   (header.nvariables + header.eltsize) * sizeof(svalue) +
	   header.narrays * sizeof(sarray) +
	   header.nstrings * sizeof(sstring) +
	   header.strsize +
	   header.ncallouts * (Uint) sizeof(scallout);
    sdata->nsectors = d_swapalloc(size, sdata->nsectors, &sdata->sectors);
    header.nsectors = sdata->nsectors;
    OBJ(sdata->oindex)->dfirst = sdata->sectors[0];

    /* save header */
    size = sizeof(dheader);
    sw_writev((char *) &header, sdata->sectors, size, (Uint) 0);
    sw_writev((char *) sdata->sectors, sdata->sectors,
	      header.nsectors * (Uint) sizeof(sector), size);
    size += header.nsectors * (Uint) sizeof(sector);

    /* save variables */
    sdata->varoffset = size;
    sw_writev((char *) sdata->svariables, sdata->sectors,
	      sdata->nvariables * (Uint) sizeof(svalue), size);
    size += sdata->nvariables * (Uint) sizeof(svalue);

    /* save arrays */
    sdata->arroffset = size;
    if (header.narrays > 0) {
	sw_writev((char *) sdata->sarrays, sdata->sectors,
		  header.narrays * sizeof(sarray), size);
	size += header.narrays * sizeof(sarray);
	if (header.eltsize > 0) {
	    sw_writev((char *) sdata->selts, sdata->sectors,
		      header.eltsize * sizeof(svalue), size);
	    size += header.eltsize * sizeof(svalue);
	}
    }

    /* save strings */
    sdata->stroffset = size;
    if (header.nstrings > 0) {
	sw_writev((char *) sdata->sstrings, sdata->sectors,
		  header.nstrings * sizeof(sstring), size);
	size += header.nstrings * sizeof(sstring);
	if (header.strsize > 0) {
	    sw_writev(text, sdata->sectors, header.strsize, size);
	    size += header.strsize;
	    if (text != sdata->stext) {
		DFREEA(text);
	    }
	}
    }

    /* save callouts */
    sdata->cooffset = size;
    if (header.ncallouts > 0) {
	sw_writev((char *) sdata->scallouts, sdata->sectors,
		  header.ncallouts * (Uint) sizeof(scallout), size);
    }
}

/*
 * NAME:	sdata->save_dataspace()
 * DESCRIPTION:	save all values in a dataspace block
 */
bool sd_save_dataspace(data, swap, counttab)
register dataspace *data;
int swap;
Uint *counttab;
{
    register sdataspace *sdata;
    register Uint n;

    if (data->parser != (struct _parser_ *) NULL) {
	ps_save(data->parser, data->env);
    }
    if (data->base.flags == 0) {
	return FALSE;
    }

    sdata = data->sdata;
    if (sdata != (sdataspace *) NULL && data->base.achange == 0 &&
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
	    sd_put_values(data, sdata->svariables, data->variables,
			  data->nvariables);
	    if (swap) {
		sd_save_svariables(sdata);
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
		sd_save_sarrays(sdata);
	    }
	}
	if (data->base.flags & MOD_ARRAY) {
	    register arrref *a;

	    /*
	     * array elements changed
	     */
	    a = data->base.arrays;
	    for (n = 0; n < data->narrays; n++) {
		if (a->arr != (array *) NULL && (a->ref & ARR_MOD)) {
		    a->ref &= ~ARR_MOD;
		    sd_put_values(data, &data->selts[data->sarrays[n].index],
				  a->arr->elts, a->arr->size);
		    if (swap) {
			sd_save_arrselts(sdata, n);
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
		sd_save_sstrings(sdata);
	    }
	}
	if (data->base.flags & MOD_CALLOUT) {
	    register scallout *sco;
	    register dcallout *co;

	    sco = sdata->scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->nargs = co->nargs;
		if (co->val[0].type == T_STRING) {
		    co->val[0].modified = TRUE;
		    co->val[1].modified = TRUE;
		    co->val[2].modified = TRUE;
		    co->val[3].modified = TRUE;
		    sd_put_values(data, sco->val, co->val,
				  (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
		}
		sco++;
		co++;
	    }

	    sdata->fcallouts = data->fcallouts;
	    if (swap) {
		sd_save_scallouts(sdata);
	    }
	}
    } else {
	savedata save;

	/*
	 * count the number and sizes of strings and arrays
	 */
	save.env = data->env;
	save.amerge = arr_merge(data->env);
	save.smerge = str_merge(data->env);
	save.narr = 0;
	save.nstr = 0;
	save.arrsize = 0;
	save.strsize = 0;
	save.counttab = counttab;

	sd_count(&save, d_get_variables(data), data->nvariables);

	if (data->ncallouts > 0) {
	    register dcallout *co;

	    if (data->callouts == (dcallout *) NULL) {
		sd_load_callouts(data);
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
		IFREE(data->env, data->callouts);
		data->callouts = (dcallout *) NULL;
	    } else {
		/* process callouts */
		for (co = data->callouts; n > 0; --n, co++) {
		    if (co->val[0].type == T_STRING) {
			sd_count(&save, co->val,
				 (co->nargs > 3) ? 4 : co->nargs + 1);
		    }
		}
	    }
	}

	if (sdata == (sdataspace *) NULL) {
	    data->sdata = sdata = sd_alloc_sdataspace(data->oindex);
	    sdata->svariables = DALLOC(svalue, data->nvariables);
	} else {
	    sdata->svariables = DREALLOC(sdata->svariables, svalue, 0,
					 data->nvariables);
	}
	sdata->nvariables = data->nvariables;
	sdata->narrays = save.narr;
	sdata->eltsize = save.arrsize;
	sdata->nstrings = save.nstr;
	sdata->strsize = save.strsize;
	sdata->ncallouts = data->ncallouts;
	sdata->fcallouts = data->fcallouts;

	/*
	 * put everything in a saveable form
	 */
	save.sstrings = data->sstrings = sdata->sstrings =
			DREALLOC(sdata->sstrings, sstring, 0, sdata->nstrings);
	save.stext = data->stext = sdata->stext =
		     DREALLOC(data->stext, char, 0, sdata->strsize);
	save.sarrays = data->sarrays = sdata->sarrays =
		       DREALLOC(data->sarrays, sarray, 0, sdata->narrays);
	save.selts = data->selts = sdata->selts =
		     DREALLOC(data->selts, svalue, 0, sdata->eltsize);
	save.narr = 0;
	save.nstr = 0;
	save.arrsize = 0;
	save.strsize = 0;
	sdata->scallouts = DREALLOC(sdata->scallouts, scallout, 0,
				    sdata->ncallouts);

	sd_save(&save, sdata->svariables, data->variables, data->nvariables);
	if (sdata->ncallouts > 0) {
	    register scallout *sco;
	    register dcallout *co;

	    sco = sdata->scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->nargs = co->nargs;
		if (co->val[0].type == T_STRING) {
		    sd_save(&save, sco->val, co->val,
			    (co->nargs > 3) ? 4 : co->nargs + 1);
		} else {
		    sco->val[0].type = T_NIL;
		}
		sco++;
		co++;
	    }
	}

	/* clear merge tables */
	arr_clear(save.amerge);
	str_clear(save.smerge);

	if (swap) {
	    sd_save_sdataspace(sdata);
	}

	d_free_values(data);

	data->narrays = save.narr;
	data->nstrings = save.nstr;
	data->strsize = save.strsize;
	data->base.schange = 0;
	data->base.achange = 0;
    }

    data->base.flags = 0;
    return swap;
}


/*
 * NAME:	sdata->conv()
 * DESCRIPTION:	convert something from the dump file
 */
static Uint sd_conv(m, vec, layout, n, idx)
char *m, *layout;
sector *vec;
Uint n, idx;
{
    Uint bufsize;
    char *buf;

    bufsize = (conf_dsize(layout) & 0xff) * n;
    buf = DALLOCA(char, bufsize);
    sw_dreadv(buf, vec, bufsize, idx);
    conf_dconv(m, buf, layout, n);
    DFREEA(buf);

    return bufsize;
}

/*
 * NAME:	sdata->conv_control()
 * DESCRIPTION:	convert a control block
 */
void sd_conv_control(oindex)
unsigned int oindex;
{
    cheader header;
    register scontrol *sctrl;
    register Uint size;
    register sector *s;
    register unsigned int n;
    object *obj;

    sctrl = sd_new_scontrol(oindex);
    obj = OBJ(oindex);

    /* header */
    size = sd_conv((char *) &header, &obj->cfirst, sc_layout, (Uint) 1,
		  (Uint) 0);
    if (header.nvariables >= PRIV) {
	fatal("too many variables in restored object");
    }
    s = DALLOCA(sector, header.nsectors);
    s[0] = obj->cfirst;
    for (n = 0; n < header.nsectors; n++) {
	size += sd_conv((char *) (s + n), s, "d", (Uint) 1, size);
    }

    sctrl->flags = header.flags;

    /* compile time */
    sctrl->compiled = header.compiled;

    if (header.vmapsize != 0) {
	/*
	 * Control block for outdated issue; only vmap can be loaded.
	 * The load offsets will be invalid (and unused).
	 */
	sctrl->vmapsize = header.vmapsize;
	sctrl->vmap = DALLOC(unsigned short, header.vmapsize);
	sd_conv((char *) sctrl->vmap, s, "s", (Uint) header.vmapsize, size);
    } else {
	/* inherits */
	sctrl->ninherits = UCHAR(header.ninherits);
	sctrl->inherits = DALLOC(sinherit, sctrl->ninherits);
	size += sd_conv((char *) sctrl->inherits, s, si_layout,
			(Uint) UCHAR(header.ninherits), size);

	/* program */
	sctrl->progsize = header.progsize;
	if (header.progsize != 0) {
	    /* program */
	    if (sctrl->flags & CMP_TYPE) {
		sctrl->prog = decompress(s, sw_dreadv, header.progsize, size,
					 &sctrl->progsize);
	    } else {
		sctrl->prog = DALLOC(char, header.progsize);
		sw_dreadv(sctrl->prog, s, header.progsize, size);
	    }
	    size += header.progsize;
	}

	/* string constants */
	sctrl->nstrings = header.nstrings;
	sctrl->strsize = header.strsize;
	if (header.nstrings != 0) {
	    sctrl->sstrings = DALLOC(dstrconst, header.nstrings);
	    size += sd_conv((char *) sctrl->sstrings, s, DSTR_LAYOUT,
			    (Uint) header.nstrings, size);
	    if (header.strsize != 0) {
		if (sctrl->flags & (CMP_TYPE << 2)) {
		    sctrl->stext = decompress(s, sw_dreadv, header.strsize,
					      size, &sctrl->strsize);
		} else {
		    sctrl->stext = DALLOC(char, header.strsize);
		    sw_dreadv(sctrl->stext, s, header.strsize, size);
		}
		size += header.strsize;
	    }
	}

	/* function definitions */
	sctrl->nfuncdefs = UCHAR(header.nfuncdefs);
	if (header.nfuncdefs != 0) {
	    sctrl->funcdefs = DALLOC(dfuncdef, UCHAR(header.nfuncdefs));
	    size += sd_conv((char *) sctrl->funcdefs, s, DF_LAYOUT,
			    (Uint) UCHAR(header.nfuncdefs), size);
	}

	/* variable definitions */
	sctrl->nvardefs = UCHAR(header.nvardefs);
	if (header.nvardefs != 0) {
	    sctrl->vardefs = DALLOC(dvardef, UCHAR(header.nvardefs));
	    size += sd_conv((char *) sctrl->vardefs, s, DV_LAYOUT,
			    (Uint) UCHAR(header.nvardefs), size);
	}

	/* function call table */
	sctrl->nfuncalls = header.nfuncalls;
	if (header.nfuncalls != 0) {
	    sctrl->funcalls = DALLOC(char, 2 * header.nfuncalls);
	    sw_dreadv(sctrl->funcalls, s, header.nfuncalls * (Uint) 2, size);
	    size += header.nfuncalls * (Uint) 2;
	}

	/* symbol table */
	sctrl->nsymbols = header.nsymbols;
	if (header.nsymbols != 0) {
	    sctrl->symbols = DALLOC(dsymbol, header.nsymbols);
	    sd_conv((char *) sctrl->symbols, s, DSYM_LAYOUT,
		    (Uint) header.nsymbols, size);
	}

	/* # variables */
	sctrl->nvariables = header.nvariables;
	sctrl->nifdefs = header.nifdefs;
	sctrl->nvinit = header.nvinit;
    }
    DFREEA(s);

    sd_save_scontrol(sctrl);
    sd_free_scontrol(sctrl);
}

/*
 * NAME:	sdata->fixobjs()
 * DESCRIPTION:	fix objects in dataspace
 */
static void sd_fixobjs(v, n, ctab)
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
 * NAME:	sdata->conv_dataspace()
 * DESCRIPTION:	convert dataspace
 */
void sd_conv_dataspace(obj, counttab)
object *obj;
Uint *counttab;
{
    dheader header;
    register sdataspace *sdata;
    register dataspace *data;
    register Uint size;
    register sector *s;
    register unsigned int n;

    sdata = sd_alloc_sdataspace(obj->index);

    /*
     * restore from dump file
     */
    size = sd_conv((char *) &header, &obj->dfirst, sd_layout, (Uint) 1,
		   (Uint) 0);
    sdata->nvariables = header.nvariables;
    sdata->narrays = header.narrays;
    sdata->eltsize = header.eltsize;
    sdata->nstrings = header.nstrings;
    sdata->strsize = header.strsize;
    sdata->ncallouts = header.ncallouts;
    sdata->fcallouts = header.fcallouts;

    /* sectors */
    s = DALLOCA(sector, header.nsectors);
    s[0] = obj->dfirst;
    for (n = 0; n < header.nsectors; n++) {
	size += sd_conv((char *) (s + n), s, "d", (Uint) 1, size);
    }

    /* variables */
    sdata->svariables = DALLOC(svalue, header.nvariables);
    size += sd_conv((char *) sdata->svariables, s, SV_LAYOUT,
		    (Uint) header.nvariables, size);
    sd_fixobjs(sdata->svariables, (Uint) header.nvariables, counttab);

    if (header.narrays != 0) {
	/* arrays */
	sdata->sarrays = DALLOC(sarray, header.narrays);
	size += sd_conv((char *) sdata->sarrays, s, SA_LAYOUT, header.narrays,
			size);
	if (header.eltsize != 0) {
	    sdata->selts = DALLOC(svalue, header.eltsize);
	    size += sd_conv((char *) sdata->selts, s, SV_LAYOUT, header.eltsize,
			    size);
	    sd_fixobjs(sdata->selts, header.eltsize, counttab);
	}
    }

    if (header.nstrings != 0) {
	/* strings */
	sdata->sstrings = DALLOC(sstring, header.nstrings);
	size += sd_conv((char *) sdata->sstrings, s, SS_LAYOUT, header.nstrings,
			size);
	if (header.strsize != 0) {
	    if (header.flags & CMP_TYPE) {
		sdata->stext = decompress(s, sw_dreadv, header.strsize, size,
					  &sdata->strsize);
	    } else {
		sdata->stext = DALLOC(char, header.strsize);
		sw_dreadv(sdata->stext, s, header.strsize, size);
	    }
	    size += header.strsize;
	}
    }

    if (header.ncallouts != 0) {
	register scallout *sco;

	/* callouts */
	sco = sdata->scallouts = DALLOC(scallout, header.ncallouts);
	sd_conv((char *) sdata->scallouts, s, sco_layout,
		(Uint) header.ncallouts, size);

	for (n = sdata->ncallouts; n > 0; --n) {
	    if (sco->val[0].type == T_STRING) {
		if (sco->nargs > 3) {
		    sd_fixobjs(sco->val, (Uint) 4, counttab);
		} else {
		    sd_fixobjs(sco->val, sco->nargs + (Uint) 1, counttab);
		}
	    }
	    sco++;
	}
    }

    DFREEA(s);

    data = d_alloc_dataspace(sch_env(), obj);
    data->sdata = sdata;
    data->nvariables = sdata->nvariables;
    data->narrays = sdata->narrays;
    data->nstrings = sdata->nstrings;
    data->strsize = sdata->strsize;
    data->ncallouts = sdata->ncallouts;
    data->fcallouts = sdata->fcallouts;

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->u_master)->update) {
	/* handle object upgrading right away */
	data->ctrl = o_control(sch_env(), obj);
	data->ctrl->ndata++;
	d_upgrade_clone(data);
    }

    data->base.flags |= MOD_ALL;
    sd_save_dataspace(data, TRUE, counttab);
    OBJ(data->oindex)->data = (dataspace *) NULL;
    d_free_dataspace(data);
}


/*
 * NAME:	sdata->del_scontrol()
 * DESCRIPTION:	delete a scontrol block from swap
 */
void sd_del_scontrol(sctrl)
register scontrol *sctrl;
{
    if (sctrl->sectors != (sector *) NULL) {
	sw_wipev(sctrl->sectors, sctrl->nsectors);
	sw_delv(sctrl->sectors, sctrl->nsectors);
    }
}

/*
 * NAME:	sdata->del_sdataspace()
 * DESCRIPTION:	delete a sdataspace block from swap
 */
void sd_del_sdataspace(sdata)
register sdataspace *sdata;
{
    if (sdata->sectors != (sector *) NULL) {
	sw_wipev(sdata->sectors, sdata->nsectors);
	sw_delv(sdata->sectors, sdata->nsectors);
    }
}

/*
 * NAME:	sdata->free_scontrol()
 * DESCRIPTION:	remove the scontrol block from memory
 */
void sd_free_scontrol(sctrl)
register scontrol *sctrl;
{
    /* delete sectors */
    if (sctrl->sectors != (sector *) NULL) {
	DFREE(sctrl->sectors);
    }

    /* delete inherits */
    if (sctrl->inherits != (sinherit *) NULL) {
	DFREE(sctrl->inherits);
    }

    /* delete program */
    if (sctrl->prog != (char *) NULL) {
	DFREE(sctrl->prog);
    }

    /* delete string constants */
    if (sctrl->sstrings != (dstrconst *) NULL) {
	DFREE(sctrl->sstrings);
    }
    if (sctrl->stext != (char *) NULL) {
	DFREE(sctrl->stext);
    }

    /* delete function definitions */
    if (sctrl->funcdefs != (dfuncdef *) NULL) {
	DFREE(sctrl->funcdefs);
    }

    /* delete variable definitions */
    if (sctrl->vardefs != (dvardef *) NULL) {
	DFREE(sctrl->vardefs);
    }

    /* delete function call table */
    if (sctrl->funcalls != (char *) NULL) {
	DFREE(sctrl->funcalls);
    }

    /* delete symbol table */
    if (sctrl->symbols != (dsymbol *) NULL) {
	DFREE(sctrl->symbols);
    }

    /* delete vmap */
    if (sctrl->vmap != (unsigned short *) NULL) {
	DFREE(sctrl->vmap);
    }

    DFREE(sctrl);
}

/*
 * NAME:	sdata->free_sdataspace()
 * DESCRIPTION:	remove the sdataspace block from memory
 */
void sd_free_sdataspace(sdata)
register sdataspace *sdata;
{
    /* delete sectors */
    if (sdata->sectors != (sector *) NULL) {
	DFREE(sdata->sectors);
    }

    /* free scallouts */
    if (sdata->scallouts != (scallout *) NULL) {
	DFREE(sdata->scallouts);
    }

    /* free sarrays */
    if (sdata->sarrays != (sarray *) NULL) {
	if (sdata->selts != (svalue *) NULL) {
	    DFREE(sdata->selts);
	}
	DFREE(sdata->sarrays);
    }

    /* free sstrings */
    if (sdata->sstrings != (sstring *) NULL) {
	if (sdata->stext != (char *) NULL) {
	    DFREE(sdata->stext);
	}
	DFREE(sdata->sstrings);
    }

    /* free svariables */
    if (sdata->svariables != (svalue *) NULL) {
	DFREE(sdata->svariables);
    }

    DFREE(sdata);
}
