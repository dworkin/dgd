/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "call_out.h"
# include "parse.h"
# include "control.h"


# define PRIV			0x0001	/* in sinherit->flags */

struct scontrol {
    sector nsectors;		/* # sectors in part one */
    short flags;		/* control flags: compression */
    short ninherits;		/* # objects in inherit table */
    uindex imapsz;		/* inherit map size */
    Uint progsize;		/* size of program code */
    Uint compiled;		/* time of compilation */
    unsigned short comphigh;	/* time of compilation high word */
    unsigned short nstrings;	/* # strings in string constant table */
    Uint strsize;		/* size of string constant table */
    char nfuncdefs;		/* # entries in function definition table */
    char nvardefs;		/* # entries in variable definition table */
    char nclassvars;		/* # class variables */
    uindex nfuncalls;		/* # entries in function call table */
    unsigned short nsymbols;	/* # entries in symbol table */
    unsigned short nvariables;	/* # variables */
    unsigned short vmapsize;	/* size of variable map, or 0 for none */
};

static char sc_layout[] = "dssuiissicccusss";

struct sinherit {
    uindex oindex;		/* index in object table */
    uindex progoffset;		/* program offset */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset */
    unsigned short flags;	/* bit flags */
};

static char si_layout[] = "uuuss";

struct sdataspace {
    sector nsectors;		/* number of sectors in data space */
    short flags;		/* dataspace flags: compression */
    unsigned short nvariables;	/* number of variables */
    Uint narrays;		/* number of array values */
    Uint eltsize;		/* total size of array elements */
    Uint nstrings;		/* number of strings */
    Uint strsize;		/* total size of strings */
    uindex ncallouts;		/* number of callouts */
    uindex fcallouts;		/* first free callout */
};

static char sd_layout[] = "dssiiiiuu";

struct svalue {
    char type;			/* object, number, string, array */
    char pad;			/* 0 */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Uint string;		/* string */
	Uint objcnt;		/* object creation count */
	Uint array;		/* array */
    } u;
};

static char sv_layout[] = "ccui";

struct sarray {
    Uint tag;			/* unique value for each array */
    Uint ref;			/* refcount */
    char type;			/* array type */
    unsigned short size;	/* size of array */
};

static char sa_layout[] = "iics";

struct sarray1 {
    Uint index;			/* index in array value table */
    char type;			/* array type */
    unsigned short size;	/* size of array */
    Uint ref;			/* refcount */
    Uint tag;			/* unique value for each array */
};

static char sa1_layout[] = "icsii";

struct sstring {
    Uint ref;			/* refcount */
    ssizet len;			/* length of string */
};

static char ss_layout[] = "it";

struct sstring0 {
    Uint index;			/* index in string text table */
    ssizet len;			/* length of string */
    Uint ref;			/* refcount */
};

static char ss0_layout[] = "iti";

struct scallout {
    Uint time;			/* time of call */
    unsigned short htime;	/* time of call, high word */
    unsigned short mtime;	/* time of call milliseconds */
    uindex nargs;		/* number of arguments */
    svalue val[4];		/* function name, 3 direct arguments */
};

static char sco_layout[] = "issu[ccui][ccui][ccui][ccui]";

struct savedata {
    Uint narr;				/* # of arrays */
    Uint nstr;				/* # of strings */
    Uint arrsize;			/* # of array elements */
    Uint strsize;			/* total string size */
    sarray *sarrays;			/* save arrays */
    svalue *selts;			/* save array elements */
    sstring *sstrings;			/* save strings */
    char *stext;			/* save string elements */
    Array alist;			/* linked list sentinel */
};

static Control *chead, *ctail;		/* list of control blocks */
static Dataspace *dhead, *dtail;	/* list of dataspace blocks */
static Dataspace *gcdata;		/* next dataspace to garbage collect */
static sector nctrl;			/* # control blocks */
static sector ndata;			/* # dataspace blocks */
static bool conv_14;			/* convert arrays & strings? */
static bool converted;			/* conversion complete? */


/*
 * NAME:	data->init()
 * DESCRIPTION:	initialize swapped data handling
 */
void d_init()
{
    chead = ctail = (Control *) NULL;
    dhead = dtail = (Dataspace *) NULL;
    gcdata = (Dataspace *) NULL;
    nctrl = ndata = 0;
    conv_14 = FALSE;
    converted = FALSE;
}

/*
 * NAME:	data->init_conv()
 * DESCRIPTION:	prepare for conversions
 */
void d_init_conv(bool c14)
{
    conv_14 = c14;
}

/*
 * NAME:	data->new_control()
 * DESCRIPTION:	create a new control block
 */
Control *d_new_control()
{
    Control *ctrl;

    ctrl = ALLOC(Control, 1);
    if (chead != (Control *) NULL) {
	/* insert at beginning of list */
	chead->prev = ctrl;
	ctrl->prev = (Control *) NULL;
	ctrl->next = chead;
	chead = ctrl;
    } else {
	/* list was empty */
	ctrl->prev = ctrl->next = (Control *) NULL;
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
    ctrl->imapsz = 0;
    ctrl->imap = (char *) NULL;
    ctrl->compiled = 0;
    ctrl->progsize = 0;
    ctrl->prog = (char *) NULL;
    ctrl->nstrings = 0;
    ctrl->strings = (String **) NULL;
    ctrl->sslength = (ssizet *) NULL;
    ctrl->ssindex = (Uint *) NULL;
    ctrl->stext = (char *) NULL;
    ctrl->nfuncdefs = 0;
    ctrl->funcdefs = (dfuncdef *) NULL;
    ctrl->nvardefs = 0;
    ctrl->nclassvars = 0;
    ctrl->vardefs = (dvardef *) NULL;
    ctrl->cvstrings = (String **) NULL;
    ctrl->classvars = (char *) NULL;
    ctrl->nfuncalls = 0;
    ctrl->funcalls = (char *) NULL;
    ctrl->nsymbols = 0;
    ctrl->symbols = (dsymbol *) NULL;
    ctrl->nvariables = 0;
    ctrl->vtypes = (char *) NULL;
    ctrl->vmapsize = 0;
    ctrl->vmap = (unsigned short *) NULL;

    return ctrl;
}

/*
 * NAME:	data->alloc_dataspace()
 * DESCRIPTION:	allocate a new dataspace block
 */
static Dataspace *d_alloc_dataspace(Object *obj)
{
    Dataspace *data;

    data = ALLOC(Dataspace, 1);
    if (dhead != (Dataspace *) NULL) {
	/* insert at beginning of list */
	dhead->prev = data;
	data->prev = (Dataspace *) NULL;
	data->next = dhead;
	dhead = data;
	data->gcprev = gcdata->gcprev;
	data->gcnext = gcdata;
	data->gcprev->gcnext = data;
	gcdata->gcprev = data;
    } else {
	/* list was empty */
	data->prev = data->next = (Dataspace *) NULL;
	dhead = dtail = data;
	gcdata = data;
	data->gcprev = data->gcnext = data;
    }
    ndata++;

    data->iprev = (Dataspace *) NULL;
    data->inext = (Dataspace *) NULL;
    data->flags = 0;

    data->oindex = obj->index;
    data->ctrl = (Control *) NULL;

    /* sectors */
    data->nsectors = 0;
    data->sectors = (sector *) NULL;

    /* variables */
    data->nvariables = 0;
    data->variables = (Value *) NULL;
    data->svariables = (svalue *) NULL;

    /* arrays */
    data->narrays = 0;
    data->eltsize = 0;
    data->sarrays = (sarray *) NULL;
    data->saindex = (Uint *) NULL;
    data->selts = (svalue *) NULL;
    data->alist.prev = data->alist.next = &data->alist;

    /* strings */
    data->nstrings = 0;
    data->strsize = 0;
    data->sstrings = (sstring *) NULL;
    data->ssindex = (Uint *) NULL;
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
    data->base.alocal.arr = (Array *) NULL;
    data->base.alocal.plane = &data->base;
    data->base.alocal.data = data;
    data->base.alocal.state = AR_CHANGED;
    data->base.arrays = (arrref *) NULL;
    data->base.strings = (strref *) NULL;
    data->base.coptab = (class coptable *) NULL;
    data->base.prev = (Dataplane *) NULL;
    data->base.plist = (Dataplane *) NULL;
    data->plane = &data->base;

    /* parse_string data */
    data->parser = (struct parser *) NULL;

    return data;
}

/*
 * NAME:	data->new_dataspace()
 * DESCRIPTION:	create a new dataspace block
 */
Dataspace *d_new_dataspace(Object *obj)
{
    Dataspace *data;

    data = d_alloc_dataspace(obj);
    data->base.flags = MOD_VARIABLE;
    data->ctrl = obj->control();
    data->ctrl->ndata++;
    data->nvariables = data->ctrl->nvariables + 1;

    return data;
}

/*
 * NAME:	load_control()
 * DESCRIPTION:	load a control block
 */
static Control *load_control(Object *obj, void (*readv) (char*, sector*, Uint, Uint))
{
    Control *ctrl;
    scontrol header;
    Uint size;

    ctrl = d_new_control();
    ctrl->oindex = obj->index;

    /* header */
    (*readv)((char *) &header, &obj->cfirst, (Uint) sizeof(scontrol), (Uint) 0);
    ctrl->nsectors = header.nsectors;
    ctrl->sectors = ALLOC(sector, header.nsectors);
    ctrl->sectors[0] = obj->cfirst;
    size = header.nsectors * (Uint) sizeof(sector);
    if (header.nsectors > 1) {
	(*readv)((char *) ctrl->sectors, ctrl->sectors, size,
		 (Uint) sizeof(scontrol));
    }
    size += sizeof(scontrol);

    ctrl->flags = header.flags;

    /* inherits */
    ctrl->ninherits = header.ninherits;

    if (header.vmapsize != 0) {
	/*
	 * Control block for outdated issue; only vmap can be loaded.
	 * The load offsets will be invalid (and unused).
	 */
	ctrl->vmapsize = header.vmapsize;
	ctrl->vmap = ALLOC(unsigned short, header.vmapsize);
	(*readv)((char *) ctrl->vmap, ctrl->sectors,
		 header.vmapsize * (Uint) sizeof(unsigned short), size);
    } else {
	int n;
	dinherit *inherits;
	sinherit *sinherits;

	/* load inherits */
	n = header.ninherits; /* at least one */
	ctrl->inherits = inherits = ALLOC(dinherit, n);
	sinherits = ALLOCA(sinherit, n);
	(*readv)((char *) sinherits, ctrl->sectors, n * (Uint) sizeof(sinherit),
		 size);
	size += n * sizeof(sinherit);
	do {
	    inherits->oindex = sinherits->oindex;
	    inherits->progoffset = sinherits->progoffset;
	    inherits->funcoffset = sinherits->funcoffset;
	    inherits->varoffset = sinherits->varoffset;
	    (inherits++)->priv = (sinherits++)->flags;
	} while (--n > 0);
	AFREE(sinherits - header.ninherits);

	/* load iindices */
	ctrl->imapsz = header.imapsz;
	ctrl->imap = ALLOC(char, header.imapsz);
	(*readv)(ctrl->imap, ctrl->sectors, ctrl->imapsz, size);
	size += ctrl->imapsz;
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
    size += header.nstrings * (Uint) sizeof(ssizet) + header.strsize;

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
    size += header.nsymbols * (Uint) sizeof(dsymbol);

    /* # variables */
    ctrl->vtypeoffset = size;
    ctrl->nvariables = header.nvariables;

    return ctrl;
}

/*
 * NAME:	data->load_control()
 * DESCRIPTION:	load a control block from the swap device
 */
Control *d_load_control(Object *obj)
{
    return load_control(obj, sw_readv);
}

/*
 * NAME:	load_dataspace()
 * DESCRIPTION:	load the dataspace header block
 */
static Dataspace *load_dataspace(Object *obj, void (*readv) (char*, sector*, Uint, Uint))
{
    sdataspace header;
    Dataspace *data;
    Uint size;

    data = d_alloc_dataspace(obj);
    data->ctrl = obj->control();
    data->ctrl->ndata++;

    /* header */
    (*readv)((char *) &header, &obj->dfirst, (Uint) sizeof(sdataspace),
	     (Uint) 0);
    data->nsectors = header.nsectors;
    data->sectors = ALLOC(sector, header.nsectors);
    data->sectors[0] = obj->dfirst;
    size = header.nsectors * (Uint) sizeof(sector);
    if (header.nsectors > 1) {
	(*readv)((char *) data->sectors, data->sectors, size,
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

    return data;
}

/*
 * NAME:	data->load_dataspace()
 * DESCRIPTION:	load the dataspace header block of an object from swap
 */
Dataspace *d_load_dataspace(Object *obj)
{
    Dataspace *data;

    data = load_dataspace(obj, sw_readv);

    if (!(obj->flags & O_MASTER) && obj->update != OBJ(obj->master)->update &&
	obj->count != 0) {
	d_upgrade_clone(data);
    }

    return data;
}

/*
 * NAME:	data->ref_control()
 * DESCRIPTION:	reference control block
 */
void d_ref_control(Control *ctrl)
{
    if (ctrl != chead) {
	/* move to head of list */
	ctrl->prev->next = ctrl->next;
	if (ctrl->next != (Control *) NULL) {
	    ctrl->next->prev = ctrl->prev;
	} else {
	    ctail = ctrl->prev;
	}
	ctrl->prev = (Control *) NULL;
	ctrl->next = chead;
	chead->prev = ctrl;
	chead = ctrl;
    }
}

/*
 * NAME:	data->ref_dataspace()
 * DESCRIPTION:	reference data block
 */
void d_ref_dataspace(Dataspace *data)
{
    if (data != dhead) {
	/* move to head of list */
	data->prev->next = data->next;
	if (data->next != (Dataspace *) NULL) {
	    data->next->prev = data->prev;
	} else {
	    dtail = data->prev;
	}
	data->prev = (Dataspace *) NULL;
	data->next = dhead;
	dhead->prev = data;
	dhead = data;
    }
}


/*
 * NAME:	compress()
 * DESCRIPTION:	compress data
 */
static Uint compress(char *data, char *text, Uint size)
{
    char htab[16384];
    unsigned short buf, bufsize, x;
    char *p, *q;
    Uint cspace;

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
	x = ((x << 3) & 0x3fff) ^ Hashtab::hashchar(UCHAR(*p++));

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

    return (intptr_t) q - (intptr_t) data;
}

/*
 * NAME:	decompress()
 * DESCRIPTION:	read and decompress data from the swap file
 */
static char *decompress(sector *sectors, void (*readv) (char*, sector*, Uint, Uint), Uint size, Uint offset, Uint *dsize)
{
    char buffer[8192], htab[16384];
    unsigned short buf, bufsize, x;
    Uint n;
    char *p, *q;

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

	    x = ((x << 3) & 0x3fff) ^ Hashtab::hashchar(UCHAR(*q++));
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
 * NAME:	get_prog()
 * DESCRIPTION:	get the program
 */
static void get_prog(Control *ctrl, void (*readv) (char*, sector*, Uint, Uint))
{
    if (ctrl->progsize != 0) {
	if (ctrl->flags & CTRL_PROGCMP) {
	    ctrl->prog = decompress(ctrl->sectors, readv, ctrl->progsize,
				    ctrl->progoffset, &ctrl->progsize);
	} else {
	    ctrl->prog = ALLOC(char, ctrl->progsize);
	    (*readv)(ctrl->prog, ctrl->sectors, ctrl->progsize,
		     ctrl->progoffset);
	}
    }
}

/*
 * NAME:	data->get_prog()
 * DESCRIPTION:	get the program
 */
char *d_get_prog(Control *ctrl)
{
    if (ctrl->prog == (char *) NULL && ctrl->progsize != 0) {
	get_prog(ctrl, sw_readv);
    }
    return ctrl->prog;
}

/*
 * NAME:	get_stext()
 * DESCRIPTION:	load strings text
 */
static void get_stext(Control *ctrl, void (*readv) (char*, sector*, Uint, Uint))
{
    /* load strings text */
    if (ctrl->flags & CTRL_STRCMP) {
	ctrl->stext = decompress(ctrl->sectors, readv,
				 ctrl->strsize,
				 ctrl->stroffset +
				 ctrl->nstrings * sizeof(ssizet),
				 &ctrl->strsize);
    } else {
	ctrl->stext = ALLOC(char, ctrl->strsize);
	(*readv)(ctrl->stext, ctrl->sectors, ctrl->strsize,
		 ctrl->stroffset + ctrl->nstrings * (Uint) sizeof(ssizet));
    }
}

/*
 * NAME:	get_strconsts()
 * DESCRIPTION:	load string constants
 */
static void get_strconsts(Control *ctrl, void (*readv) (char*, sector*, Uint, Uint))
{
    if (ctrl->nstrings != 0) {
	/* load strings */
	ctrl->sslength = ALLOC(ssizet, ctrl->nstrings);
	(*readv)((char *) ctrl->sslength, ctrl->sectors,
		 ctrl->nstrings * (Uint) sizeof(ssizet), ctrl->stroffset);
	if (ctrl->strsize > 0 && ctrl->stext == (char *) NULL) {
	    get_stext(ctrl, readv);	/* load strings text */
	}
    }
}

/*
 * NAME:	data->get_strconst()
 * DESCRIPTION:	get a string constant
 */
String *d_get_strconst(Control *ctrl, int inherit, Uint idx)
{
    if (UCHAR(inherit) < ctrl->ninherits - 1) {
	/* get the proper control block */
	ctrl = OBJR(ctrl->inherits[UCHAR(inherit)].oindex)->control();
    }

    if (ctrl->strings == (String **) NULL) {
	/* make string pointer block */
	ctrl->strings = ALLOC(String*, ctrl->nstrings);
	memset(ctrl->strings, '\0', ctrl->nstrings * sizeof(String *));

	if (ctrl->sslength == (ssizet *) NULL) {
	    get_strconsts(ctrl, sw_readv);
	}
	if (ctrl->ssindex == (Uint *) NULL) {
	    Uint size;
	    unsigned short i;

	    ctrl->ssindex = ALLOC(Uint, ctrl->nstrings);
	    for (size = 0, i = 0; i < ctrl->nstrings; i++) {
		ctrl->ssindex[i] = size;
		size += ctrl->sslength[i];
	    }
	}
    }

    if (ctrl->strings[idx] == (String *) NULL) {
	String *str;

	str = str_alloc(ctrl->stext + ctrl->ssindex[idx],
			(long) ctrl->sslength[idx]);
	str_ref(ctrl->strings[idx] = str);
    }

    return ctrl->strings[idx];
}

/*
 * NAME:	get_funcdefs()
 * DESCRIPTION:	load function definitions
 */
static void get_funcdefs(Control *ctrl, void (*readv) (char*, sector*, Uint, Uint))
{
    if (ctrl->nfuncdefs != 0) {
	ctrl->funcdefs = ALLOC(dfuncdef, ctrl->nfuncdefs);
	(*readv)((char *) ctrl->funcdefs, ctrl->sectors,
		 ctrl->nfuncdefs * (Uint) sizeof(dfuncdef), ctrl->funcdoffset);
    }
}

/*
 * NAME:	data->get_funcdefs()
 * DESCRIPTION:	get function definitions
 */
dfuncdef *d_get_funcdefs(Control *ctrl)
{
    if (ctrl->funcdefs == (dfuncdef *) NULL && ctrl->nfuncdefs != 0) {
	get_funcdefs(ctrl, sw_readv);
    }
    return ctrl->funcdefs;
}

/*
 * NAME:	get_vardefs()
 * DESCRIPTION:	load variable definitions
 */
static void get_vardefs(Control *ctrl, void (*readv) (char*, sector*, Uint, Uint))
{
    if (ctrl->nvardefs != 0) {
	ctrl->vardefs = ALLOC(dvardef, ctrl->nvardefs);
	(*readv)((char *) ctrl->vardefs, ctrl->sectors,
		 ctrl->nvardefs * (Uint) sizeof(dvardef), ctrl->vardoffset);
	if (ctrl->nclassvars != 0) {
	    ctrl->classvars = ALLOC(char, ctrl->nclassvars * 3);
	    (*readv)(ctrl->classvars, ctrl->sectors, ctrl->nclassvars * 3,
		     ctrl->vardoffset + ctrl->nvardefs * sizeof(dvardef));
	}
    }
}

/*
 * NAME:	data->get_vardefs()
 * DESCRIPTION:	get variable definitions
 */
dvardef *d_get_vardefs(Control *ctrl)
{
    if (ctrl->vardefs == (dvardef *) NULL && ctrl->nvardefs != 0) {
	get_vardefs(ctrl, sw_readv);
    }
    if (ctrl->cvstrings == (String **) NULL && ctrl->nclassvars != 0) {
	char *p;
	dvardef *vars;
	String **strs;
	unsigned short n, inherit, u;

	ctrl->cvstrings = strs = ALLOC(String*, ctrl->nvardefs);
	memset(strs, '\0', ctrl->nvardefs * sizeof(String*));
	p = ctrl->classvars;
	for (n = ctrl->nclassvars, vars = ctrl->vardefs; n != 0; vars++) {
	    if ((vars->type & T_TYPE) == T_CLASS) {
		inherit = FETCH1U(p);
		str_ref(*strs = d_get_strconst(ctrl, inherit, FETCH2U(p, u)));
		--n;
	    }
	    strs++;
	}
    }
    return ctrl->vardefs;
}

/*
 * NAME:	get_funcalls()
 * DESCRIPTION:	get function call table
 */
static void get_funcalls(Control *ctrl, void (*readv) (char*, sector*, Uint, Uint))
{
    if (ctrl->nfuncalls != 0) {
	ctrl->funcalls = ALLOC(char, 2L * ctrl->nfuncalls);
	(*readv)((char *) ctrl->funcalls, ctrl->sectors,
		 ctrl->nfuncalls * (Uint) 2, ctrl->funccoffset);
    }
}

/*
 * NAME:	data->get_funcalls()
 * DESCRIPTION:	get function call table
 */
char *d_get_funcalls(Control *ctrl)
{
    if (ctrl->funcalls == (char *) NULL && ctrl->nfuncalls != 0) {
	get_funcalls(ctrl, sw_readv);
    }
    return ctrl->funcalls;
}

/*
 * NAME:	get_symbols()
 * DESCRIPTION:	get symbol table
 */
static void get_symbols(Control *ctrl, void (*readv) (char*, sector*, Uint, Uint))
{
    if (ctrl->nsymbols > 0) {
	ctrl->symbols = ALLOC(dsymbol, ctrl->nsymbols);
	(*readv)((char *) ctrl->symbols, ctrl->sectors,
		 ctrl->nsymbols * (Uint) sizeof(dsymbol), ctrl->symboffset);
    }
}

/*
 * NAME:	data->get_symbols()
 * DESCRIPTION:	get symbol table
 */
dsymbol *d_get_symbols(Control *ctrl)
{
    if (ctrl->symbols == (dsymbol *) NULL && ctrl->nsymbols > 0) {
	get_symbols(ctrl, sw_readv);
    }
    return ctrl->symbols;
}

/*
 * NAME:	get_vtypes()
 * DESCRIPTION:	get variable types
 */
static void get_vtypes(Control *ctrl, void (*readv) (char*, sector*, Uint, Uint))
{
    if (ctrl->nvariables > ctrl->nvardefs) {
	ctrl->vtypes = ALLOC(char, ctrl->nvariables - ctrl->nvardefs);
	(*readv)(ctrl->vtypes, ctrl->sectors, ctrl->nvariables - ctrl->nvardefs,
		 ctrl->vtypeoffset);
    }
}

/*
 * NAME:	data->get_vtypes()
 * DESCRIPTION:	get variable types
 */
static char *d_get_vtypes(Control *ctrl)
{
    if (ctrl->vtypes == (char *) NULL && ctrl->nvariables > ctrl->nvardefs) {
	get_vtypes(ctrl, sw_readv);
    }
    return ctrl->vtypes;
}

/*
 * NAME:	data->get_progsize()
 * DESCRIPTION:	get the size of a control block
 */
Uint d_get_progsize(Control *ctrl)
{
    if (ctrl->progsize != 0 && ctrl->prog == (char *) NULL &&
	(ctrl->flags & CTRL_PROGCMP)) {
	get_prog(ctrl, sw_readv);	/* decompress program */
    }
    if (ctrl->strsize != 0 && ctrl->stext == (char *) NULL &&
	(ctrl->flags & CTRL_STRCMP)) {
	get_stext(ctrl, sw_readv);	/* decompress strings */
    }

    return ctrl->ninherits * sizeof(dinherit) +
	   ctrl->imapsz +
	   ctrl->progsize +
	   ctrl->nstrings * (Uint) sizeof(ssizet) +
	   ctrl->strsize +
	   ctrl->nfuncdefs * sizeof(dfuncdef) +
	   ctrl->nvardefs * sizeof(dvardef) +
	   ctrl->nclassvars * (Uint) 3 +
	   ctrl->nfuncalls * (Uint) 2 +
	   ctrl->nsymbols * (Uint) sizeof(dsymbol) +
	   ctrl->nvariables - ctrl->nvardefs;
}


/*
 * NAME:	get_strings()
 * DESCRIPTION:	load strings for dataspace
 */
static void get_strings(Dataspace *data, void (*readv) (char*, sector*, Uint, Uint))
{
    if (data->nstrings != 0) {
	/* load strings */
	data->sstrings = ALLOC(sstring, data->nstrings);
	(*readv)((char *) data->sstrings, data->sectors,
		 data->nstrings * sizeof(sstring), data->stroffset);
	if (data->strsize > 0) {
	    /* load strings text */
	    if (data->flags & DATA_STRCMP) {
		data->stext = decompress(data->sectors, readv, data->strsize,
					 data->stroffset +
					       data->nstrings * sizeof(sstring),
					 &data->strsize);
	    } else {
		data->stext = ALLOC(char, data->strsize);
		(*readv)(data->stext, data->sectors, data->strsize,
			 data->stroffset + data->nstrings * sizeof(sstring));
	    }
	}
    }
}

/*
 * NAME:	data->get_string()
 * DESCRIPTION:	get a string from the dataspace
 */
static String *d_get_string(Dataspace *data, Uint idx)
{
    if (data->plane->strings == (strref *) NULL ||
	data->plane->strings[idx].str == (String *) NULL) {
	String *str;
	strref *s;
	Dataplane *p;
	Uint i;

	if (data->sstrings == (sstring *) NULL) {
	    get_strings(data, sw_readv);
	}
	if (data->ssindex == (Uint *) NULL) {
	    Uint size;

	    data->ssindex = ALLOC(Uint, data->nstrings);
	    for (size = 0, i = 0; i < data->nstrings; i++) {
		data->ssindex[i] = size;
		size += data->sstrings[i].len;
	    }
	}

	str = str_alloc(data->stext + data->ssindex[idx],
			(long) data->sstrings[idx].len);
	str->ref = 0;
	p = data->plane;

	do {
	    if (p->strings == (strref *) NULL) {
		/* initialize string pointers */
		s = p->strings = ALLOC(strref, data->nstrings);
		for (i = data->nstrings; i > 0; --i) {
		    (s++)->str = (String *) NULL;
		}
	    }
	    s = &p->strings[idx];
	    str_ref(s->str = str);
	    s->data = data;
	    s->ref = data->sstrings[idx].ref;
	    p = p->prev;
	} while (p != (Dataplane *) NULL);

	str->primary = &data->plane->strings[idx];
	return str;
    }
    return data->plane->strings[idx].str;
}

/*
 * NAME:	get_arrays()
 * DESCRIPTION:	load arrays for dataspace
 */
static void get_arrays(Dataspace *data, void (*readv) (char*, sector*, Uint, Uint))
{
    if (data->narrays != 0) {
	/* load arrays */
	data->sarrays = ALLOC(sarray, data->narrays);
	(*readv)((char *) data->sarrays, data->sectors,
		 data->narrays * (Uint) sizeof(sarray), data->arroffset);
    }
}

/*
 * NAME:	data->get_array()
 * DESCRIPTION:	get an array from the dataspace
 */
static Array *d_get_array(Dataspace *data, Uint idx)
{
    if (data->plane->arrays == (arrref *) NULL ||
	data->plane->arrays[idx].arr == (Array *) NULL) {
	Array *arr;
	arrref *a;
	Dataplane *p;
	Uint i;

	if (data->sarrays == (sarray *) NULL) {
	    /* load arrays */
	    get_arrays(data, sw_readv);
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
		    (a++)->arr = (Array *) NULL;
		}
	    }
	    a = &p->arrays[idx];
	    arr_ref(a->arr = arr);
	    a->plane = &data->base;
	    a->data = data;
	    a->state = AR_UNCHANGED;
	    a->ref = data->sarrays[idx].ref;
	    p = p->prev;
	} while (p != (Dataplane *) NULL);

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
 * DESCRIPTION:	get values from the Dataspace
 */
static void d_get_values(Dataspace *data, svalue *sv, Value *v, int n)
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
void d_new_variables(Control *ctrl, Value *val)
{
    unsigned short n;
    char *type;
    dvardef *var;

    memset(val, '\0', ctrl->nvariables * sizeof(Value));
    for (n = ctrl->nvariables - ctrl->nvardefs, type = d_get_vtypes(ctrl);
	 n != 0; --n, type++) {
	val->type = *type;
	val++;
    }
    for (n = ctrl->nvardefs, var = d_get_vardefs(ctrl); n != 0; --n, var++) {
	if (T_ARITHMETIC(var->type)) {
	    val->type = var->type;
	} else {
	    val->type = nil_type;
	}
	val++;
    }
}

/*
 * NAME:	get_variables()
 * DESCRIPTION:	load variables
 */
static void get_variables(Dataspace *data, void (*readv) (char*, sector*, Uint, Uint))
{
    data->svariables = ALLOC(svalue, data->nvariables);
    (*readv)((char *) data->svariables, data->sectors,
	     data->nvariables * (Uint) sizeof(svalue), data->varoffset);
}

/*
 * NAME:	data->get_variable()
 * DESCRIPTION:	get a variable from the dataspace
 */
Value *d_get_variable(Dataspace *data, unsigned int idx)
{
    if (data->variables == (Value *) NULL) {
	/* create room for variables */
	data->variables = ALLOC(Value, data->nvariables);
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
		get_variables(data, sw_readv);
	    }
	    d_get_values(data, data->svariables, data->variables,
			 data->nvariables);
	}
    }

    return &data->variables[idx];
}

/*
 * NAME:	get_elts()
 * DESCRIPTION:	load elements
 */
static void get_elts(Dataspace *data, void (*readv) (char*, sector*, Uint, Uint))
{
    if (data->eltsize != 0) {
	/* load array elements */
	data->selts = (svalue *) ALLOC(svalue, data->eltsize);
	(*readv)((char *) data->selts, data->sectors,
		 data->eltsize * sizeof(svalue),
		 data->arroffset + data->narrays * sizeof(sarray));
    }
}

/*
 * NAME:	data->get_elts()
 * DESCRIPTION:	get the elements of an array
 */
Value *d_get_elts(Array *arr)
{
    Value *v;

    v = arr->elts;
    if (v == (Value *) NULL && arr->size != 0) {
	Dataspace *data;
	Uint idx;

	data = arr->primary->data;
	if (data->selts == (svalue *) NULL) {
	    get_elts(data, sw_readv);
	}
	if (data->saindex == (Uint *) NULL) {
	    Uint size;

	    data->saindex = ALLOC(Uint, data->narrays);
	    for (size = 0, idx = 0; idx < data->narrays; idx++) {
		data->saindex[idx] = size;
		size += data->sarrays[idx].size;
	    }
	}

	v = arr->elts = ALLOC(Value, arr->size);
	idx = data->saindex[arr->primary - data->plane->arrays];
	d_get_values(data, &data->selts[idx], v, arr->size);
    }

    return v;
}

/*
 * NAME:	get_callouts()
 * DESCRIPTION:	load callouts from swap
 */
static void get_callouts(Dataspace *data, void (*readv) (char*, sector*, Uint, Uint))
{
    if (data->ncallouts != 0) {
	data->scallouts = ALLOC(scallout, data->ncallouts);
	(*readv)((char *) data->scallouts, data->sectors,
		 data->ncallouts * (Uint) sizeof(scallout), data->cooffset);
    }
}

/*
 * NAME:	data->get_callouts()
 * DESCRIPTION:	load callouts from swap
 */
void d_get_callouts(Dataspace *data)
{
    scallout *sco;
    dcallout *co;
    uindex n;

    if (data->scallouts == (scallout *) NULL) {
	get_callouts(data, sw_readv);
    }
    sco = data->scallouts;
    co = data->callouts = ALLOC(dcallout, data->ncallouts);

    for (n = data->ncallouts; n > 0; --n) {
	co->time = sco->time;
	co->mtime = sco->mtime;
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
static sector d_swapalloc(Uint size, sector nsectors, sector **sectors)
{
    sector n, *s;

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
static void d_save_control(Control *ctrl)
{
    scontrol header;
    char *prog, *stext, *text;
    ssizet *sslength;
    Uint size, i;
    sinherit *sinherits;
    dinherit *inherits;

    sslength = NULL;
    prog = stext = text = NULL;

    /*
     * Save a control block.
     */

    /* create header */
    header.flags = ctrl->flags & (CTRL_UNDEFINED | CTRL_VM_2_1);
    header.ninherits = ctrl->ninherits;
    header.imapsz = ctrl->imapsz;
    header.compiled = ctrl->compiled;
    header.comphigh = 0;
    header.progsize = ctrl->progsize;
    header.nstrings = ctrl->nstrings;
    header.strsize = ctrl->strsize;
    header.nfuncdefs = ctrl->nfuncdefs;
    header.nvardefs = ctrl->nvardefs;
    header.nclassvars = ctrl->nclassvars;
    header.nfuncalls = ctrl->nfuncalls;
    header.nsymbols = ctrl->nsymbols;
    header.nvariables = ctrl->nvariables;
    header.vmapsize = ctrl->vmapsize;

    /* create sector space */
    if (header.vmapsize != 0) {
	size = sizeof(scontrol) +
	       header.vmapsize * (Uint) sizeof(unsigned short);
    } else {
	prog = ctrl->prog;
	if (header.progsize >= CMPLIMIT) {
	    prog = ALLOC(char, header.progsize);
	    size = compress(prog, ctrl->prog, header.progsize);
	    if (size != 0) {
		header.flags |= CMP_PRED;
		header.progsize = size;
	    } else {
		FREE(prog);
		prog = ctrl->prog;
	    }
	}

	sslength = ctrl->sslength;
	stext = ctrl->stext;
	if (header.nstrings > 0 && sslength == (ssizet *) NULL) {
	    String **strs;
	    Uint strsize;
	    ssizet *l;
	    char *t;

	    sslength = ALLOC(ssizet, header.nstrings);
	    if (header.strsize > 0) {
		stext = ALLOC(char, header.strsize);
	    }

	    strs = ctrl->strings;
	    strsize = 0;
	    l = sslength;
	    t = stext;
	    for (i = header.nstrings; i > 0; --i) {
		strsize += *l = (*strs)->len;
		memcpy(t, (*strs++)->text, *l);
		t += *l++;
	    }
	}

	text = stext;
	if (header.strsize >= CMPLIMIT) {
	    text = ALLOC(char, header.strsize);
	    size = compress(text, stext, header.strsize);
	    if (size != 0) {
		header.flags |= CMP_PRED << 2;
		header.strsize = size;
	    } else {
		FREE(text);
		text = stext;
	    }
	}

	size = sizeof(scontrol) +
	       header.ninherits * sizeof(sinherit) +
	       header.imapsz +
	       header.progsize +
	       header.nstrings * (Uint) sizeof(ssizet) +
	       header.strsize +
	       UCHAR(header.nfuncdefs) * sizeof(dfuncdef) +
	       UCHAR(header.nvardefs) * sizeof(dvardef) +
	       UCHAR(header.nclassvars) * (Uint) 3 +
	       header.nfuncalls * (Uint) 2 +
	       header.nsymbols * (Uint) sizeof(dsymbol) +
	       header.nvariables - UCHAR(header.nvardefs);
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
	sinherits = ALLOCA(sinherit, i = header.ninherits);
	do {
	    sinherits->oindex = inherits->oindex;
	    sinherits->progoffset = inherits->progoffset;
	    sinherits->funcoffset = inherits->funcoffset;
	    sinherits->varoffset = inherits->varoffset;
	    sinherits->flags = inherits->priv;
	    inherits++;
	    sinherits++;
	} while (--i > 0);
	sinherits -= header.ninherits;
	sw_writev((char *) sinherits, ctrl->sectors,
		  header.ninherits * (Uint) sizeof(sinherit), size);
	size += header.ninherits * sizeof(sinherit);
	AFREE(sinherits);

	/* save iindices */
	sw_writev(ctrl->imap, ctrl->sectors, ctrl->imapsz, size);
	size += ctrl->imapsz;

	/* save program */
	if (header.progsize > 0) {
	    sw_writev(prog, ctrl->sectors, (Uint) header.progsize, size);
	    size += header.progsize;
	    if (prog != ctrl->prog) {
		FREE(prog);
	    }
	}

	/* save string constants */
	if (header.nstrings > 0) {
	    sw_writev((char *) sslength, ctrl->sectors,
		      header.nstrings * (Uint) sizeof(ssizet), size);
	    size += header.nstrings * (Uint) sizeof(ssizet);
	    if (header.strsize > 0) {
		sw_writev(text, ctrl->sectors, header.strsize, size);
		size += header.strsize;
		if (text != stext) {
		    FREE(text);
		}
		if (stext != ctrl->stext) {
		    FREE(stext);
		}
	    }
	    if (sslength != ctrl->sslength) {
		FREE(sslength);
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
	    size += header.nsymbols * sizeof(dsymbol);
	}

	/* save variable types */
	if (header.nvariables > UCHAR(header.nvardefs)) {
	    sw_writev(ctrl->vtypes, ctrl->sectors,
		      header.nvariables - UCHAR(header.nvardefs), size);
	}
    }
}


/*
 * NAME:	data->arrcount()
 * DESCRIPTION:	count the number of arrays and strings in an array
 */
static void d_arrcount(savedata *save, Array *arr)
{
    arr->prev->next = arr->next;
    arr->next->prev = arr->prev;
    arr->prev = &save->alist;
    arr->next = save->alist.next;
    arr->next->prev = arr;
    save->alist.next = arr;
    save->narr++;
}

/*
 * NAME:	data->count()
 * DESCRIPTION:	count the number of arrays and strings in an object
 */
static void d_count(savedata *save, Value *v, Uint n)
{
    Object *obj;
    Value *elts;
    Uint count;

    while (n > 0) {
	switch (v->type) {
	case T_STRING:
	    if (str_put(v->u.string, save->nstr) == save->nstr) {
		save->nstr++;
		save->strsize += v->u.string->len;
	    }
	    break;

	case T_ARRAY:
	    if (arr_put(v->u.array, save->narr) == save->narr) {
		d_arrcount(save, v->u.array);
	    }
	    break;

	case T_MAPPING:
	    if (arr_put(v->u.array, save->narr) == save->narr) {
		if (v->u.array->hashmod) {
		    map_compact(v->u.array->primary->data, v->u.array);
		}
		d_arrcount(save, v->u.array);
	    }
	    break;

	case T_LWOBJECT:
	    elts = d_get_elts(v->u.array);
	    if (elts->type == T_OBJECT) {
		obj = OBJ(elts->oindex);
		count = obj->count;
		if (elts[1].type == T_INT) {
		    /* convert to new LWO type */
		    elts[1].type = T_FLOAT;
		    elts[1].oindex = FALSE;
		}
		if (arr_put(v->u.array, save->narr) == save->narr) {
		    if (elts->u.objcnt == count &&
			elts[1].u.objcnt != obj->update) {
			d_upgrade_lwobj(v->u.array, obj);
		    }
		    d_arrcount(save, v->u.array);
		}
	    } else if (arr_put(v->u.array, save->narr) == save->narr) {
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
 * DESCRIPTION:	save the values in an object
 */
static void d_save(savedata *save, svalue *sv, Value *v, unsigned short n)
{
    Uint i;

    while (n > 0) {
	sv->pad = '\0';
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
	    i = str_put(v->u.string, save->nstr);
	    sv->oindex = 0;
	    sv->u.string = i;
	    if (save->sstrings[i].ref++ == 0) {
		/* new string value */
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
	    i = arr_put(v->u.array, save->narr);
	    sv->oindex = 0;
	    sv->u.array = i;
	    if (save->sarrays[i].ref++ == 0) {
		/* new array value */
		save->sarrays[i].type = sv->type;
	    }
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
static void d_put_values(Dataspace *data, svalue *sv, Value *v, unsigned short n)
{
    while (n > 0) {
	if (v->modified) {
	    sv->pad = '\0';
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
static void d_free_values(Dataspace *data)
{
    Uint i;

    /* free parse_string data */
    if (data->parser != (struct parser *) NULL) {
	ps_del(data->parser);
	data->parser = (struct parser *) NULL;
    }

    /* free variables */
    if (data->variables != (Value *) NULL) {
	Value *v;

	for (i = data->nvariables, v = data->variables; i > 0; --i, v++) {
	    i_del_value(v);
	}

	FREE(data->variables);
	data->variables = (Value *) NULL;
    }

    /* free callouts */
    if (data->callouts != (dcallout *) NULL) {
	dcallout *co;
	Value *v;
	int j;

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
	arrref *a;

	for (i = data->narrays, a = data->base.arrays; i > 0; --i, a++) {
	    if (a->arr != (Array *) NULL) {
		arr_del(a->arr);
	    }
	}

	FREE(data->base.arrays);
	data->base.arrays = (arrref *) NULL;
    }

    /* free strings */
    if (data->base.strings != (strref *) NULL) {
	strref *s;

	for (i = data->nstrings, s = data->base.strings; i > 0; --i, s++) {
	    if (s->str != (String *) NULL) {
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
static bool d_save_dataspace(Dataspace *data, bool swap)
{
    sdataspace header;
    Uint n;

    if (data->parser != (struct parser *) NULL &&
	!(OBJ(data->oindex)->flags & O_SPECIAL)) {
	ps_save(data->parser);
    }
    if (swap && (data->base.flags & MOD_SAVE)) {
	data->base.flags |= MOD_ALL;
    } else if (!(data->base.flags & MOD_ALL)) {
	return FALSE;
    }

    if (data->svariables != (svalue *) NULL && data->base.achange == 0 &&
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
	    sarray *sa;
	    arrref *a;

	    /*
	     * references to arrays changed
	     */
	    sa = data->sarrays;
	    a = data->base.arrays;
	    mod = FALSE;
	    for (n = data->narrays; n > 0; --n) {
		if (a->arr != (Array *) NULL && sa->ref != (a->ref & ~ARR_MOD))
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
	    arrref *a;
	    Uint idx;

	    /*
	     * array elements changed
	     */
	    a = data->base.arrays;
	    for (n = 0; n < data->narrays; n++) {
		if (a->arr != (Array *) NULL && (a->ref & ARR_MOD)) {
		    a->ref &= ~ARR_MOD;
		    idx = data->saindex[n];
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
	    sstring *ss;
	    strref *s;

	    /*
	     * string references changed
	     */
	    ss = data->sstrings;
	    s = data->base.strings;
	    mod = FALSE;
	    for (n = data->nstrings; n > 0; --n) {
		if (s->str != (String *) NULL && ss->ref != s->ref) {
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
	    scallout *sco;
	    dcallout *co;

	    sco = data->scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->htime = 0;
		sco->mtime = co->mtime;
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
	Uint size;
	Array *arr;
	sarray *sarr;

	/*
	 * count the number and sizes of strings and arrays
	 */
	arr_merge();
	str_merge();
	save.narr = 0;
	save.nstr = 0;
	save.arrsize = 0;
	save.strsize = 0;
	save.alist.prev = save.alist.next = &save.alist;

	d_get_variable(data, 0);
	if (data->svariables == (svalue *) NULL) {
	    data->svariables = ALLOC(svalue, data->nvariables);
	}
	d_count(&save, data->variables, data->nvariables);

	if (data->ncallouts > 0) {
	    dcallout *co;

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

	for (arr = save.alist.prev; arr != &save.alist; arr = arr->prev) {
	    save.arrsize += arr->size;
	    d_count(&save, d_get_elts(arr), arr->size);
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
	    scallout *sco;
	    dcallout *co;

	    sco = data->scallouts;
	    co = data->callouts;
	    for (n = data->ncallouts; n > 0; --n) {
		sco->time = co->time;
		sco->htime = 0;
		sco->mtime = co->mtime;
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
	arr_clear();
	str_clear();

	if (swap) {
	    text = save.stext;
	    if (header.strsize >= CMPLIMIT) {
		text = ALLOC(char, header.strsize);
		size = compress(text, save.stext, header.strsize);
		if (size != 0) {
		    header.flags |= CMP_PRED;
		    header.strsize = size;
		} else {
		    FREE(text);
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
			FREE(text);
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
	if (data->saindex != (Uint *) NULL) {
	    FREE(data->saindex);
	    data->saindex = NULL;
	}
	if (data->ssindex != (Uint *) NULL) {
	    FREE(data->ssindex);
	    data->ssindex = NULL;
	}

	data->flags = header.flags;
	data->narrays = header.narrays;
	data->eltsize = header.eltsize;
	data->nstrings = header.nstrings;
	data->strsize = save.strsize;

	data->base.schange = 0;
	data->base.achange = 0;
    }

    if (swap) {
	data->base.flags = 0;
    } else {
	data->base.flags = MOD_SAVE;
    }
    return TRUE;
}


/*
 * NAME:	data->swapout()
 * DESCRIPTION:	Swap out a portion of the control and dataspace blocks in
 *		memory.  Return the number of dataspace blocks swapped out.
 */
sector d_swapout(unsigned int frag)
{
    sector n, count;
    Dataspace *data;
    Control *ctrl;

    count = 0;

    if (frag != 0) {
	/* swap out dataspace blocks */
	data = dtail;
	for (n = ndata / frag, n -= (n > 0 && frag != 1); n > 0; --n) {
	    Dataspace *prev;

	    prev = data->prev;
	    if (d_save_dataspace(data, TRUE)) {
		count++;
	    }
	    OBJ(data->oindex)->data = (Dataspace *) NULL;
	    d_free_dataspace(data);
	    data = prev;
	}

	/* swap out control blocks */
	ctrl = ctail;
	for (n = nctrl / frag; n > 0; --n) {
	    Control *prev;

	    prev = ctrl->prev;
	    if (ctrl->ndata == 0) {
		if (ctrl->sectors == (sector *) NULL ||
		    (ctrl->flags & CTRL_VARMAP)) {
		    d_save_control(ctrl);
		}
		OBJ(ctrl->oindex)->ctrl = (Control *) NULL;
		d_free_control(ctrl);
	    }
	    ctrl = prev;
	}
    }

    /* perform garbage collection for one dataspace */
    if (gcdata != (Dataspace *) NULL) {
	if (d_save_dataspace(gcdata, (frag != 0)) && frag != 0) {
	    count++;
	}
	gcdata = gcdata->gcnext;
    }

    return count;
}

/*
 * NAME:	data->upgrade_mem()
 * DESCRIPTION:	upgrade all obj and all objects cloned from obj that have
 *		dataspaces in memory
 */
void d_upgrade_mem(Object *tmpl, Object *newob)
{
    Dataspace *data;
    Uint nvar;
    unsigned short *vmap;
    Object *obj;

    nvar = tmpl->ctrl->vmapsize;
    vmap = tmpl->ctrl->vmap;

    for (data = dtail; data != (Dataspace *) NULL; data = data->prev) {
	obj = OBJ(data->oindex);
	if ((obj == newob ||
	     (!(obj->flags & O_MASTER) && obj->master == newob->index)) &&
	    obj->count != 0) {
	    /* upgrade clone */
	    if (nvar != 0) {
		d_upgrade_data(data, nvar, vmap, tmpl);
	    }
	    data->ctrl->ndata--;
	    data->ctrl = newob->ctrl;
	    data->ctrl->ndata++;
	}
    }
}

/*
 * NAME:	data->conv()
 * DESCRIPTION:	convert something from the snapshot
 */
static Uint d_conv(char *m, sector *vec, const char *layout, Uint n, Uint idx,
		   void (*readv) (char*, sector*, Uint, Uint))
{
    Uint bufsize;
    char *buf;

    bufsize = (conf_dsize(layout) & 0xff) * n;
    buf = ALLOC(char, bufsize);
    (*readv)(buf, vec, bufsize, idx);
    conf_dconv(m, buf, layout, n);
    FREE(buf);

    return bufsize;
}

/*
 * NAME:	data->conv_control()
 * DESCRIPTION:	convert control block
 */
static Control *d_conv_control(Object *obj,
			       void (*readv) (char*, sector*, Uint, Uint))
{
    scontrol header;
    Control *ctrl;
    Uint size;
    unsigned int n;

    ctrl = d_new_control();
    ctrl->oindex = obj->index;

    /*
     * restore from snapshot
     */
    size = d_conv((char *) &header, &obj->cfirst, sc_layout, (Uint) 1, (Uint) 0,
		  readv);
    ctrl->flags = header.flags;
    ctrl->ninherits = header.ninherits;
    ctrl->imapsz = header.imapsz;
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
    ctrl->vmapsize = header.vmapsize;

    /* sectors */
    ctrl->sectors = ALLOC(sector, ctrl->nsectors = header.nsectors);
    ctrl->sectors[0] = obj->cfirst;
    for (n = 0; n < header.nsectors; n++) {
	size += d_conv((char *) (ctrl->sectors + n), ctrl->sectors, "d",
		       (Uint) 1, size, readv);
    }

    if (header.vmapsize != 0) {
	/* only vmap */
	ctrl->vmap = ALLOC(unsigned short, header.vmapsize);
	d_conv((char *) ctrl->vmap, ctrl->sectors, "s", (Uint) header.vmapsize,
	       size, readv);
    } else {
	dinherit *inherits;
	sinherit *sinherits;

	/* inherits */
	n = header.ninherits; /* at least one */
	ctrl->inherits = inherits = ALLOC(dinherit, n);

	sinherits = ALLOCA(sinherit, n);
	size += d_conv((char *) sinherits, ctrl->sectors, si_layout, (Uint) n,
		       size, readv);
	do {
	    inherits->oindex = sinherits->oindex;
	    inherits->progoffset = sinherits->progoffset;
	    inherits->funcoffset = sinherits->funcoffset;
	    inherits->varoffset = sinherits->varoffset;
	    (inherits++)->priv = (sinherits++)->flags;
	} while (--n > 0);
	AFREE(sinherits - header.ninherits);

	ctrl->imap = ALLOC(char, header.imapsz);
	(*readv)(ctrl->imap, ctrl->sectors, header.imapsz, size);
	size += header.imapsz;

	if (header.progsize != 0) {
	    /* program */
	    if (header.flags & CMP_TYPE) {
		ctrl->prog = decompress(ctrl->sectors, readv, header.progsize,
					size, &ctrl->progsize);
	    } else {
		ctrl->prog = ALLOC(char, header.progsize);
		(*readv)(ctrl->prog, ctrl->sectors, header.progsize, size);
	    }
	    size += header.progsize;
	}

	if (header.nstrings != 0) {
	    /* strings */
	    ctrl->sslength = ALLOC(ssizet, header.nstrings);
	    if (conv_14) {
		dstrconst0 *sstrings;
		unsigned short i;

		sstrings = ALLOCA(dstrconst0, header.nstrings);
		size += d_conv((char *) sstrings, ctrl->sectors, DSTR0_LAYOUT,
			       (Uint) header.nstrings, size, readv);
		for (i = 0; i < header.nstrings; i++) {
		    ctrl->sslength[i] = sstrings[i].len;
		}
		AFREE(sstrings);
	    } else {
		size += d_conv((char *) ctrl->sslength, ctrl->sectors, "t",
			       (Uint) header.nstrings, size, readv);
	    }
	    if (header.strsize != 0) {
		if (header.flags & (CMP_TYPE << 2)) {
		    ctrl->stext = decompress(ctrl->sectors, readv,
					     header.strsize, size,
					     &ctrl->strsize);
		} else {
		    ctrl->stext = ALLOC(char, header.strsize);
		    (*readv)(ctrl->stext, ctrl->sectors, header.strsize, size);
		}
		size += header.strsize;
	    }
	}

	if (header.nfuncdefs != 0) {
	    /* function definitions */
	    ctrl->funcdefs = ALLOC(dfuncdef, UCHAR(header.nfuncdefs));
	    size += d_conv((char *) ctrl->funcdefs, ctrl->sectors, DF_LAYOUT,
			   (Uint) UCHAR(header.nfuncdefs), size, readv);
	}

	if (header.nvardefs != 0) {
	    /* variable definitions */
	    ctrl->vardefs = ALLOC(dvardef, UCHAR(header.nvardefs));
	    size += d_conv((char *) ctrl->vardefs, ctrl->sectors, DV_LAYOUT,
			   (Uint) UCHAR(header.nvardefs), size, readv);
	    if (ctrl->nclassvars != 0) {
		ctrl->classvars = ALLOC(char, ctrl->nclassvars * 3);
		(*readv)(ctrl->classvars, ctrl->sectors,
			 ctrl->nclassvars * (Uint) 3, size);
		size += ctrl->nclassvars * (Uint) 3;
	    }
	}

	if (header.nfuncalls != 0) {
	    /* function calls */
	    ctrl->funcalls = ALLOC(char, 2 * header.nfuncalls);
	    (*readv)(ctrl->funcalls, ctrl->sectors, header.nfuncalls * (Uint) 2,
		     size);
	    size += header.nfuncalls * (Uint) 2;
	}

	if (header.nsymbols != 0) {
	    /* symbol table */
	    ctrl->symbols = ALLOC(dsymbol, header.nsymbols);
	    size += d_conv((char *) ctrl->symbols, ctrl->sectors, DSYM_LAYOUT,
			   (Uint) header.nsymbols, size, readv);
	}

	if (header.nvariables > UCHAR(header.nvardefs)) {
	    /* variable types */
	    ctrl->vtypes = ALLOC(char, header.nvariables -
				       UCHAR(header.nvardefs));
	    (*readv)(ctrl->vtypes, ctrl->sectors,
		     header.nvariables - UCHAR(header.nvardefs), size);
	}
    }

    return ctrl;
}

/*
 * NAME:	data->conv_sarray1()
 * DESCRIPTION:	convert old sarrays
 */
static Uint d_conv_sarray1(sarray *sa, sector *s, Uint n, Uint size)
{
    sarray1 *osa;
    Uint i;

    osa = ALLOC(sarray1, n);
    size = d_conv((char *) osa, s, sa1_layout, n, size, &sw_conv);
    for (i = 0; i < n; i++) {
	sa->tag = osa->tag;
	sa->ref = osa->ref;
	sa->type = osa->type;
	(sa++)->size = (osa++)->size;
    }
    FREE(osa - n);
    return size;
}

/*
 * NAME:	data->conv_sstring0()
 * DESCRIPTION:	convert old sstrings
 */
static Uint d_conv_sstring0(sstring *ss, sector *s, Uint n, Uint size)
{
    sstring0 *oss;
    Uint i;

    oss = ALLOC(sstring0, n);
    size = d_conv((char *) oss, s, ss0_layout, n, size, &sw_conv);
    for (i = 0; i < n; i++) {
	ss->ref = oss->ref;
	(ss++)->len = (oss++)->len;
    }
    FREE(oss - n);
    return size;
}

/*
 * NAME:	data->fixobjs()
 * DESCRIPTION:	fix objects in dataspace
 */
static void d_fixobjs(svalue *v, Uint n, Uint *ctab)
{
    while (n != 0) {
	if (v->type == T_OBJECT) {
	    if (v->u.objcnt == ctab[v->oindex] && OBJ(v->oindex)->count != 0) {
		/* fix object count */
		v->u.objcnt = OBJ(v->oindex)->count;
	    } else {
		/* destructed object; mark as invalid */
		v->oindex = 0;
		v->u.objcnt = 1;
	    }
	}
	v++;
	--n;
    }
}

/*
 * NAME:	data->fixdata()
 * DESCRIPTION:	fix a dataspace
 */
static void d_fixdata(Dataspace *data, Uint *counttab)
{
    scallout *sco;
    unsigned int n;

    d_fixobjs(data->svariables, (Uint) data->nvariables, counttab);
    d_fixobjs(data->selts, data->eltsize, counttab);
    for (n = data->ncallouts, sco = data->scallouts; n > 0; --n, sco++) {
	if (sco->val[0].type == T_STRING) {
	    if (sco->nargs > 3) {
		d_fixobjs(sco->val, (Uint) 4, counttab);
	    } else {
		d_fixobjs(sco->val, sco->nargs + (Uint) 1, counttab);
	    }
	}
    }
}

/*
 * NAME:	data->conv_datapace()
 * DESCRIPTION:	convert dataspace
 */
static Dataspace *d_conv_dataspace(Object *obj, Uint *counttab,
				   void (*readv) (char*, sector*, Uint, Uint))
{
    sdataspace header;
    Dataspace *data;
    Uint size;
    unsigned int n;

    UNREFERENCED_PARAMETER(counttab);

    data = d_alloc_dataspace(obj);

    /*
     * restore from snapshot
     */
    size = d_conv((char *) &header, &obj->dfirst, sd_layout, (Uint) 1,
		  (Uint) 0, readv);
    data->nvariables = header.nvariables;
    data->narrays = header.narrays;
    data->eltsize = header.eltsize;
    data->nstrings = header.nstrings;
    data->strsize = header.strsize;
    data->ncallouts = header.ncallouts;
    data->fcallouts = header.fcallouts;

    /* sectors */
    data->sectors = ALLOC(sector, data->nsectors = header.nsectors);
    data->sectors[0] = obj->dfirst;
    for (n = 0; n < header.nsectors; n++) {
	size += d_conv((char *) (data->sectors + n), data->sectors, "d",
		       (Uint) 1, size, readv);
    }

    /* variables */
    data->svariables = ALLOC(svalue, header.nvariables);
    size += d_conv((char *) data->svariables, data->sectors, sv_layout,
		   (Uint) header.nvariables, size, readv);

    if (header.narrays != 0) {
	/* arrays */
	data->sarrays = ALLOC(sarray, header.narrays);
	if (conv_14) {
	    size += d_conv_sarray1(data->sarrays, data->sectors,
				   header.narrays, size);
	} else {
	    size += d_conv((char *) data->sarrays, data->sectors, sa_layout,
			   header.narrays, size, readv);
	}
	if (header.eltsize != 0) {
	    data->selts = ALLOC(svalue, header.eltsize);
	    size += d_conv((char *) data->selts, data->sectors, sv_layout,
			   header.eltsize, size, readv);
	}
    }

    if (header.nstrings != 0) {
	/* strings */
	data->sstrings = ALLOC(sstring, header.nstrings);
	if (conv_14) {
	    size += d_conv_sstring0(data->sstrings, data->sectors,
				    (Uint) header.nstrings, size);
	} else {
	    size += d_conv((char *) data->sstrings, data->sectors, ss_layout,
			   header.nstrings, size, readv);
	}
	if (header.strsize != 0) {
	    if (header.flags & CMP_TYPE) {
		data->stext = decompress(data->sectors, readv, header.strsize,
					 size, &data->strsize);
	    } else {
		data->stext = ALLOC(char, header.strsize);
		(*readv)(data->stext, data->sectors, header.strsize, size);
	    }
	    size += header.strsize;
	}
    }

    if (header.ncallouts != 0) {
	scallout *sco;
	unsigned short dummy;

	/* callouts */
	co_time(&dummy);
	sco = data->scallouts = ALLOC(scallout, header.ncallouts);
	d_conv((char *) data->scallouts, data->sectors, sco_layout,
	       (Uint) header.ncallouts, size, readv);
    }

    data->ctrl = obj->control();
    data->ctrl->ndata++;

    return data;
}

/*
 * NAME:	data->restore_ctrl()
 * DESCRIPTION:	restore a control block
 */
Control *d_restore_ctrl(Object *obj, void (*readv) (char*, sector*, Uint, Uint))
{
    Control *ctrl;

    ctrl = (Control *) NULL;
    if (obj->cfirst != SW_UNUSED) {
	if (!converted) {
	    ctrl = d_conv_control(obj, readv);
	} else {
	    ctrl = load_control(obj, readv);
	    if (ctrl->vmapsize == 0) {
		get_prog(ctrl, readv);
		get_strconsts(ctrl, readv);
		get_funcdefs(ctrl, readv);
		get_vardefs(ctrl, readv);
		get_funcalls(ctrl, readv);
		get_symbols(ctrl, readv);
		get_vtypes(ctrl, readv);
	    }
	}
	d_save_control(ctrl);
	obj->ctrl = ctrl;
    }

    return ctrl;
}

/*
 * NAME:	data->restore_data()
 * DESCRIPTION:	restore a dataspace
 */
Dataspace *d_restore_data(Object *obj, Uint *counttab,
			  void (*readv) (char*, sector*, Uint, Uint))
{
    Dataspace *data;

    data = (Dataspace *) NULL;
    if (OBJ(obj->index)->count != 0 && OBJ(obj->index)->dfirst != SW_UNUSED) {
	if (!converted) {
	    data = d_conv_dataspace(obj, counttab, readv);
	} else {
	    data = load_dataspace(obj, readv);
	    get_variables(data, readv);
	    get_arrays(data, readv);
	    get_elts(data, readv);
	    get_strings(data, readv);
	    get_callouts(data, readv);
	}
	obj->data = data;
	if (counttab != (Uint *) NULL) {
	    d_fixdata(data, counttab);
	}

	if (!(obj->flags & O_MASTER) &&
	    obj->update != OBJ(obj->master)->update) {
	    /* handle object upgrading right away */
	    d_upgrade_clone(data);
	}
	data->base.flags |= MOD_ALL;
    }

    return data;
}

/*
 * NAME:	data->restore_obj()
 * DESCRIPTION:	restore an object
 */
void d_restore_obj(Object *obj, Uint *counttab, bool cactive, bool dactive)
{
    Control *ctrl;
    Dataspace *data;

    if (!converted) {
	ctrl = d_restore_ctrl(obj, sw_conv);
	data = d_restore_data(obj, counttab, sw_conv);
    } else {
	ctrl = d_restore_ctrl(obj, sw_dreadv);
	data = d_restore_data(obj, counttab, sw_dreadv);
    }

    if (!cactive) {
	/* swap this out first */
	if (ctrl != (Control *) NULL && ctrl != ctail) {
	    if (chead == ctrl) {
		chead = ctrl->next;
		chead->prev = (Control *) NULL;
	    } else {
		ctrl->prev->next = ctrl->next;
		ctrl->next->prev = ctrl->prev;
	    }
	    ctail->next = ctrl;
	    ctrl->prev = ctail;
	    ctrl->next = (Control *) NULL;
	    ctail = ctrl;
	}
    }
    if (!dactive) {
	/* swap this out first */
	if (data != (Dataspace *) NULL && data != dtail) {
	    if (dhead == data) {
		dhead = data->next;
		dhead->prev = (Dataspace *) NULL;
	    } else {
		data->prev->next = data->next;
		data->next->prev = data->prev;
	    }
	    dtail->next = data;
	    data->prev = dtail;
	    data->next = (Dataspace *) NULL;
	    dtail = data;
	}
    }
}

/*
 * NAME:	data->converted()
 * DESCRIPTION:	snapshot conversion is complete
 */
void d_converted()
{
    converted = TRUE;
}

/*
 * NAME:	data->free_control()
 * DESCRIPTION:	remove the control block from memory
 */
void d_free_control(Control *ctrl)
{
    String **strs;
    unsigned short i;

    /* delete strings */
    if (ctrl->strings != (String **) NULL) {
	strs = ctrl->strings;
	for (i = ctrl->nstrings; i > 0; --i) {
	    if (*strs != (String *) NULL) {
		str_del(*strs);
	    }
	    strs++;
	}
	FREE(ctrl->strings);
    }
    if (ctrl->cvstrings != (String **) NULL) {
	strs = ctrl->cvstrings;
	for (i = ctrl->nvardefs; i > 0; --i) {
	    if (*strs != (String *) NULL) {
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

    /* delete inherit indices */
    if (ctrl->imap != (char *) NULL) {
	FREE(ctrl->imap);
    }

    /* delete string constants */
    if (ctrl->sslength != (ssizet *) NULL) {
	FREE(ctrl->sslength);
    }
    if (ctrl->ssindex != (Uint *) NULL) {
	FREE(ctrl->ssindex);
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

    /* delete variable types */
    if (ctrl->vtypes != (char *) NULL) {
	FREE(ctrl->vtypes);
    }

    if (ctrl != chead) {
	ctrl->prev->next = ctrl->next;
    } else {
	chead = ctrl->next;
	if (chead != (Control *) NULL) {
	    chead->prev = (Control *) NULL;
	}
    }
    if (ctrl != ctail) {
	ctrl->next->prev = ctrl->prev;
    } else {
	ctail = ctrl->prev;
	if (ctail != (Control *) NULL) {
	    ctail->next = (Control *) NULL;
	}
    }
    --nctrl;

    FREE(ctrl);
}

/*
 * NAME:	data->free_dataspace()
 * DESCRIPTION:	remove the dataspace block from memory
 */
void d_free_dataspace(Dataspace *data)
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
	if (data->saindex != (Uint *) NULL) {
	    FREE(data->saindex);
	}
	FREE(data->sarrays);
    }

    /* free sstrings */
    if (data->sstrings != (sstring *) NULL) {
	if (data->stext != (char *) NULL) {
	    FREE(data->stext);
	}
	if (data->ssindex != (Uint *) NULL) {
	    FREE(data->ssindex);
	}
	FREE(data->sstrings);
    }

    /* free svariables */
    if (data->svariables != (svalue *) NULL) {
	FREE(data->svariables);
    }

    if (data->ctrl != (Control *) NULL) {
	data->ctrl->ndata--;
    }

    if (data != dhead) {
	data->prev->next = data->next;
    } else {
	dhead = data->next;
	if (dhead != (Dataspace *) NULL) {
	    dhead->prev = (Dataspace *) NULL;
	}
    }
    if (data != dtail) {
	data->next->prev = data->prev;
    } else {
	dtail = data->prev;
	if (dtail != (Dataspace *) NULL) {
	    dtail->next = (Dataspace *) NULL;
	}
    }
    data->gcprev->gcnext = data->gcnext;
    data->gcnext->gcprev = data->gcprev;
    if (data == gcdata) {
	gcdata = (data != data->gcnext) ? data->gcnext : (Dataspace *) NULL;
    }
    --ndata;

    FREE(data);
}
