# define INCLUDE_FILE_IO
# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "table.h"
# include "control.h"
# include "node.h"
# include "compile.h"
# include "csupport.h"

static dinherit *inherits;	/* inherited objects */
static int *itab;		/* inherit index table */
static uindex *map;		/* object -> precompiled */
static uindex nprecomps;	/* # precompiled objects */

pcfunc *pcfunctions;		/* table of precompiled functions */

static char *auto_name;		/* name of auto object */
static char *driver_name;	/* name of driver object */

/*
 * NAME:	precomp->inherits()
 * DESCRIPTION:	handle inherited objects
 */
static bool pc_inherits(inh, pcinh, ninherits, compiled)
register dinherit *inh;
register pcinherit *pcinh;
register int ninherits;
Uint compiled;
{
    register Uint cc;
    register object *obj;

    cc = 0;
    while (--ninherits > 0) {
	obj = o_find(pcinh->name, OACC_READ);
	if (obj == (object *) NULL) {
	    message("Precompiled: cannot inherit /%s from /%s\012",	/* LF */
		    pcinh->name, pcinh[ninherits].name);
	    return FALSE;
	}
	inh->oindex = obj->index;
	if (precompiled[inh->oindex]->compiled > cc) {
	    cc = precompiled[inh->oindex]->compiled;
	}

	inh->progoffset = pcinh->progoffset;
	inh->funcoffset = pcinh->funcoffset;
	inh->varoffset = pcinh->varoffset;
	(inh++)->priv = (pcinh++)->priv;
    }
    if (cc > compiled) {
	message("Precompiled: object out of date: /%s\012",		/* LF */
		pcinh->name);
	return FALSE;
    }
    if (o_find(pcinh->name, OACC_READ) != (object *) NULL) {
	message("Precompiled: object precompiled twice: /%s\012",	/* LF */
		pcinh->name);
	return FALSE;
    }
    inh->progoffset = pcinh->progoffset;
    inh->funcoffset = pcinh->funcoffset;
    inh->varoffset = pcinh->varoffset;
    inh->priv = pcinh->priv;

    return TRUE;
}

/*
 * NAME:	precomp->funcdefs()
 * DESCRIPTION:	handle function definitions
 */
static void pc_funcdefs(program, funcdefs, nfuncdefs, nfuncs)
char *program;
register dfuncdef *funcdefs;
register unsigned short nfuncdefs;
register Uint nfuncs;
{
    register char *p;
    register Uint index;

    while (nfuncdefs > 0) {
	p = program + funcdefs->offset;
	if (!(PROTO_CLASS(p) & C_UNDEFINED)) {
	    p += PROTO_SIZE(p);
	    index = nfuncs + ((UCHAR(p[3]) << 16) | (UCHAR(p[4]) << 8) |
			      UCHAR(p[5]));
	    p[3] = index >> 16;
	    p[4] = index >> 8;
	    p[5] = index;
	}
	funcdefs++;
	--nfuncdefs;
    }
}

/*
 * NAME:	pc->obj()
 * DESCRIPTION:	create a precompiled object
 */
static uindex pc_obj(name, inherits, ninherits)
char *name;
dinherit *inherits;
int ninherits;
{
    control ctrl;
    register object *obj;

    ctrl.inherits = inherits;
    ctrl.ninherits = ninherits;
    obj = o_new(name, &ctrl);
    obj->flags |= O_COMPILED;
    if (strcmp(name, driver_name) == 0) {
	obj->flags |= O_DRIVER;
    } else if (strcmp(name, auto_name) == 0) {
	obj->flags |= O_AUTO;
    }
    obj->ctrl = (control *) NULL;

    return obj->index;
}

/*
 * NAME:	hash_add()
 * DESCRIPTION:	add object->elt to hash table
 */
static void hash_add(oindex, elt)
uindex oindex, elt;
{
    register uindex i, j;

    i = oindex % nprecomps;
    if (map[2 * i] == nprecomps) {
	map[2 * i] = elt;
	map[2 * i + 1] = nprecomps;
    } else {
	for (j = 0; map[2 * j] != nprecomps; j++) ;
	map[2 * j] = map[2 * i];
	map[2 * j + 1] = map[2 * i + 1];
	map[2 * i] = elt;
	map[2 * i + 1] = j;
    }
}

/*
 * NAME:	hash_find()
 * DESCRIPTION:	find element in hash table
 */
static uindex hash_find(oindex)
uindex oindex;
{
    register uindex i;
    uindex j;

    i = oindex % nprecomps;
    while (precompiled[j = map[2 * i]]->oindex != oindex) {
	i = map[2 * i + 1];
    }
    return j;
}

/*
 * NAME:	precomp->preload()
 * DESCRIPTION:	preload compiled objects
 */
bool pc_preload(auto_obj, driver_obj)
char *auto_obj, *driver_obj;
{
    register precomp **pc, *l;
    register uindex n, ninherits;
    register Uint nfuncs;

    auto_name = auto_obj;
    driver_name = driver_obj;

    n = ninherits = nfuncs = 0;
    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	n++;
	ninherits += (*pc)->ninherits;
	nfuncs += (*pc)->nfunctions;
    }
    nprecomps = n;

    if (n > 0) {
	m_static();
	map = ALLOC(uindex, 2 * n);
	itab = ALLOC(int, n);
	inherits = ALLOC(dinherit, ninherits);
	if (nfuncs > 0) {
	    pcfunctions = ALLOC(pcfunc, nfuncs);
	}
	m_dynamic();

	n = ninherits = nfuncs = 0;
	for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	    l = *pc;

	    if (l->typechecking != conf_typechecking()) {
		message("Precompiled object /%s has typechecking level %d",
			l->inherits[l->ninherits - 1].name, l->typechecking);
		return FALSE;
	    }
	    if (!pc_inherits(inherits + ninherits, l->inherits, l->ninherits,
			     l->compiled)) {
		return FALSE;
	    }
	    itab[n] = ninherits;
	    l->oindex = pc_obj(l->inherits[l->ninherits - 1].name,
			       inherits + ninherits, l->ninherits);
	    ninherits += l->ninherits;

	    pc_funcdefs(l->program, l->funcdefs, l->nfuncdefs, nfuncs);
	    memcpy(pcfunctions + nfuncs, l->functions,
		   sizeof(pcfunc) * l->nfunctions);
	    nfuncs += l->nfunctions;

	    map[2 * n] = n;
	    map[2 * n++ + 1] = nprecomps;
	}
    }

    return TRUE;
}

/*
 * NAME:	precomp->list()
 * DESCRIPTION:	return an array with all precompiled objects
 */
array *pc_list(data)
dataspace *data;
{
    array *a;
    register object *obj;
    register uindex n;
    register value *v;
    register precomp **pc;

    for (pc = precompiled, n = 0; *pc != (precomp *) NULL; pc++) {
	if ((*pc)->oindex != UINDEX_MAX &&
	    (obj=OBJR((*pc)->oindex))->count != 0 && (obj->flags & O_COMPILED))
	{
	    n++;
	}
    }
    if (n > conf_array_size()) {
	return (array *) NULL;
    }

    a = arr_new(data, (long) n);
    v = a->elts;
    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	if ((*pc)->oindex != UINDEX_MAX &&
	    (obj=OBJR((*pc)->oindex))->count != 0 && (obj->flags & O_COMPILED))
	{
	    v->type = T_OBJECT;
	    v->oindex = obj->index;
	    v->u.objcnt = obj->count;
	    v++;
	}
    }

    return a;
}

/*
 * NAME:	precomp->control()
 * DESCRIPTION:	initialize the control block of a precompiled object
 */
void pc_control(ctrl, obj)
register control *ctrl;
object *obj;
{
    register precomp *l;
    uindex i;

    l = precompiled[i = hash_find(obj->index)];

    ctrl->ninherits = l->ninherits;
    ctrl->inherits = inherits + itab[i];

    ctrl->imapsz = l->imapsz;
    ctrl->imap = l->imap;
    ctrl->progindex = l->ninherits - 1;

    ctrl->compiled = l->compiled;

    ctrl->progsize = l->progsize;
    ctrl->prog = l->program;

    ctrl->nstrings = l->nstrings;
    ctrl->sstrings = l->sstrings;
    ctrl->stext = l->stext;
    ctrl->strsize = l->stringsz;

    ctrl->nfuncdefs = l->nfuncdefs;
    ctrl->funcdefs = l->funcdefs;

    ctrl->nvardefs = l->nvardefs;
    ctrl->vardefs = l->vardefs;

    ctrl->nfuncalls = l->nfuncalls;
    ctrl->funcalls = l->funcalls;

    ctrl->nsymbols = l->nsymbols;
    ctrl->symbols = l->symbols;

    ctrl->nvariables = l->nvariables;
    ctrl->vtypes = l->vtypes;
}


typedef struct {
    uindex nprecomps;		/* # precompiled objects */
    Uint ninherits;		/* total # inherits */
    Uint imapsz;		/* total imap size */
    Uint nstrings;		/* total # strings */
    Uint stringsz;		/* total strings size */
    Uint nfuncdefs;		/* total # funcdefs */
    Uint nvardefs;		/* total # vardefs */
    Uint nfuncalls;		/* total # function calls */
} dump_header;

static char dh_layout[] = "uiiiiiii";

typedef struct {
    uindex nprecomps;		/* # precompiled objects */
    Uint ninherits;		/* total # inherits */
    Uint nstrings;		/* total # strings */
    Uint stringsz;		/* total strings size */
    Uint nfuncdefs;		/* total # funcdefs */
    Uint nvardefs;		/* total # vardefs */
    Uint nfuncalls;		/* total # function calls */
} odump_header;

static char odh_layout[] = "uiiiiii";

typedef struct {
    Uint compiled;		/* compile time */
    short ninherits;		/* # inherits */
    uindex imapsz;		/* imap size */
    unsigned short nstrings;	/* # strings */
    Uint stringsz;		/* strings size */
    short nfuncdefs;		/* # funcdefs */
    short nvardefs;		/* # vardefs */
    uindex nfuncalls;		/* # function calls */
    short nvariables;		/* # variables */
} dump_precomp;

static char dp_layout[] = "isusissus";

typedef struct {
    uindex oindex;		/* object index */
    uindex progoffset;		/* program offset */
    uindex funcoffset;		/* function offset */
    unsigned short varoffset;	/* variable offset */
    bool priv;			/* privately inherited? */
} dump_inherit;

static char di_layout[] = "uuusc";

/*
 * NAME:	precomp->dump()
 * DESCRIPTION:	dump precompiled objects
 */
bool pc_dump(fd)
int fd;
{
    dump_header dh;
    register precomp **pc;
    register object *obj;
    bool ok;

    dh.nprecomps = 0;
    dh.ninherits = 0;
    dh.imapsz = 0;
    dh.nstrings = 0;
    dh.stringsz = 0;
    dh.nfuncdefs = 0;
    dh.nvardefs = 0;
    dh.nfuncalls = 0;

    /* first compute sizes of data to dump */
    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	if ((*pc)->oindex != UINDEX_MAX &&
	    ((obj=OBJ((*pc)->oindex))->flags & O_COMPILED) && obj->u_ref != 0) {
	    dh.nprecomps++;
	    dh.ninherits += (*pc)->ninherits;
	    dh.imapsz += (*pc)->imapsz;
	    dh.nstrings += (*pc)->nstrings;
	    dh.stringsz += (*pc)->stringsz;
	    dh.nfuncdefs += (*pc)->nfuncdefs;
	    dh.nvardefs += (*pc)->nvardefs;
	    dh.nfuncalls += (*pc)->nfuncalls;
	}
    }

    /* write header */
    if (P_write(fd, (char *) &dh, sizeof(dump_header)) != sizeof(dump_header)) {
	return FALSE;
    }

    ok = TRUE;

    if (dh.nprecomps != 0) {
	register dump_precomp *dpc;
	register int i;
	dump_inherit *inh;
	dinherit *inh2;
	char *imap;
	dstrconst *strings;
	char *stext, *funcalls;
	dfuncdef *funcdefs;
	dvardef *vardefs;

	/*
	 * Save only the necessary information.
	 */
	dpc = ALLOCA(dump_precomp, dh.nprecomps);
	inh = ALLOCA(dump_inherit, dh.ninherits);
	imap = ALLOCA(char, dh.imapsz);
	if (dh.nstrings != 0) {
	    strings = ALLOCA(dstrconst, dh.nstrings);
	    if (dh.stringsz != 0) {
		stext = ALLOCA(char, dh.stringsz);
	    }
	}
	if (dh.nfuncdefs != 0) {
	    funcdefs = ALLOCA(dfuncdef, dh.nfuncdefs);
	}
	if (dh.nvardefs != 0) {
	    vardefs = ALLOCA(dvardef, dh.nvardefs);
	}
	if (dh.nfuncalls != 0) {
	    funcalls = ALLOCA(char, 2 * dh.nfuncalls);
	}

	for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	    if ((*pc)->oindex != UINDEX_MAX &&
		((obj=OBJ((*pc)->oindex))->flags & O_COMPILED) &&
		obj->u_ref != 0) {
		dpc->compiled = (*pc)->compiled;
		dpc->ninherits = (*pc)->ninherits;
		dpc->imapsz = (*pc)->imapsz;
		dpc->nstrings = (*pc)->nstrings;
		dpc->stringsz = (*pc)->stringsz;
		dpc->nfuncdefs = (*pc)->nfuncdefs;
		dpc->nvardefs = (*pc)->nvardefs;
		dpc->nfuncalls = (*pc)->nfuncalls;
		dpc->nvariables = (*pc)->nvariables;

		inh2 = inherits + itab[pc - precompiled];
		for (i = dpc->ninherits; i > 0; --i) {
		    inh->oindex = inh2->oindex;
		    inh->progoffset = inh2->progoffset;
		    inh->funcoffset = inh2->funcoffset;
		    inh->varoffset = inh2->varoffset;
		    (inh++)->priv = (inh2++)->priv;
		}

		memcpy(imap, (*pc)->imap, dpc->imapsz);
		imap += dpc->imapsz;

		if (dpc->nstrings > 0) {
		    memcpy(strings, (*pc)->sstrings,
			   dpc->nstrings * sizeof(dstrconst));
		    strings += dpc->nstrings;
		    if (dpc->stringsz > 0) {
			memcpy(stext, (*pc)->stext, dpc->stringsz);
			stext += dpc->stringsz;
		    }
		}

		if (dpc->nfuncdefs > 0) {
		    memcpy(funcdefs, (*pc)->funcdefs,
			   dpc->nfuncdefs * sizeof(dfuncdef));
		    for (i = 0; i < dpc->nfuncdefs; i++) {
			funcdefs[i].offset =
				    PROTO_FTYPE((*pc)->program +
						(*pc)->funcdefs[i].offset);
		    }
		    funcdefs += i;
		}

		if (dpc->nvardefs > 0) {
		    memcpy(vardefs, (*pc)->vardefs,
			   dpc->nvardefs * sizeof(dvardef));
		    vardefs += dpc->nvardefs;
		}

		if (dpc->nfuncalls > 0) {
		    memcpy(funcalls, (*pc)->funcalls, 2 * dpc->nfuncalls);
		    funcalls += 2 * dpc->nfuncalls;
		}

		dpc++;
	    }
	}

	dpc -= dh.nprecomps;
	inh -= dh.ninherits;
	imap -= dh.imapsz;
	strings -= dh.nstrings;
	stext -= dh.stringsz;
	funcdefs -= dh.nfuncdefs;
	vardefs -= dh.nvardefs;
	funcalls -= 2 * dh.nfuncalls;

	if (P_write(fd, (char *) dpc, dh.nprecomps * sizeof(dump_precomp)) !=
					dh.nprecomps * sizeof(dump_precomp) ||
	    P_write(fd, (char *) inh, dh.ninherits * sizeof(dump_inherit)) !=
					dh.ninherits * sizeof(dump_inherit) ||
	    P_write(fd, imap, dh.imapsz) != dh.imapsz ||
	    (dh.nstrings != 0 &&
	     P_write(fd, (char *) strings, dh.nstrings * sizeof(dstrconst)) !=
					    dh.nstrings * sizeof(dstrconst)) ||
	    (dh.stringsz != 0 &&
	     P_write(fd, stext, dh.stringsz) != dh.stringsz) ||
	    (dh.nfuncdefs != 0 &&
	     P_write(fd, (char *) funcdefs, dh.nfuncdefs * sizeof(dfuncdef)) !=
					    dh.nfuncdefs * sizeof(dfuncdef)) ||
	    (dh.nvardefs != 0 &&
	     P_write(fd, (char *) vardefs, dh.nvardefs * sizeof(dvardef)) !=
					    dh.nvardefs * sizeof(dvardef)) ||
	    (dh.nfuncalls != 0 &&
	     P_write(fd, funcalls, 2 * dh.nfuncalls) != 2 * dh.nfuncalls)) {
	    ok = FALSE;
	}

	if (dh.nfuncalls != 0) {
	    AFREE(funcalls);
	}
	if (dh.nvardefs != 0) {
	    AFREE(vardefs);
	}
	if (dh.nfuncdefs != 0) {
	    AFREE(funcdefs);
	}
	if (dh.nstrings != 0) {
	    if (dh.stringsz != 0) {
		AFREE(stext);
	    }
	    AFREE(strings);
	}
	AFREE(imap);
	AFREE(inh);
	AFREE(dpc);
    }

    return ok;
}

/*
 * NAME:	fixinherits()
 * DESCRIPTION:	fix the inherited object pointers that may be wrong after
 *		a restore
 */
static void fixinherits(inh, pcinh, ninherits)
register dinherit *inh;
register pcinherit *pcinh;
register int ninherits;
{
    register char *name;
    register precomp **pc;

    do {
	name = (pcinh++)->name;
	for (pc = precompiled;
	     strcmp((*pc)->inherits[(*pc)->ninherits - 1].name, name) != 0;
	     pc++) ;
	(inh++)->oindex = (*pc)->oindex;
    } while (--ninherits != 0);
}

/*
 * NAME:	inh1cmp()
 * DESCRIPTION:	compare inherited object lists
 */
static bool inh1cmp(dinh, inh, ninherits)
register dump_inherit *dinh;
register dinherit *inh;
register int ninherits;
{
    do {
	if (dinh->oindex != inh->oindex ||
	    dinh->progoffset != inh->progoffset ||
	    dinh->funcoffset != inh->funcoffset ||
	    dinh->varoffset != inh->varoffset ||
	    dinh->priv != inh->priv) {
	    return FALSE;
	}
	dinh++;
	inh++;
    } while (--ninherits != 0);
    return TRUE;
}

/*
 * NAME:	inh2cmp()
 * DESCRIPTION:	compare inherited object lists
 */
static bool inh2cmp(dinh, inh, ninherits)
register dinherit *dinh, *inh;
register int ninherits;
{
    do {
	if (dinh->oindex != inh->oindex ||
	    dinh->progoffset != inh->progoffset ||
	    dinh->funcoffset != inh->funcoffset ||
	    dinh->varoffset != inh->varoffset ||
	    dinh->priv != inh->priv) {
	    return FALSE;
	}
	dinh++;
	inh++;
    } while (--ninherits != 0);
    return TRUE;
}

/*
 * NAME:	dstrcmp()
 * DESCRIPTION:	compare string tables
 */
static bool dstrcmp(dstrings, strings, nstrings)
register dstrconst *dstrings, *strings;
register int nstrings;
{
    while (nstrings != 0) {
	if (dstrings->index != strings->index ||
	    dstrings->len != strings->len) {
	    return FALSE;
	}
	dstrings++;
	strings++;
	--nstrings;
    }
    return TRUE;
}

/*
 * NAME:	func1cmp()
 * DESCRIPTION:	compare function tables
 */
static bool func1cmp(dfuncdefs, funcdefs, prog, nfuncdefs)
register dfuncdef *dfuncdefs, *funcdefs;
register char *prog;
register int nfuncdefs;
{
    while (nfuncdefs != 0) {
	if (dfuncdefs->class != funcdefs->class ||
	    dfuncdefs->inherit != funcdefs->inherit ||
	    dfuncdefs->index != funcdefs->index ||
	    dfuncdefs->offset != (Uint) PROTO_FTYPE(prog + funcdefs->offset)) {
	    return FALSE;
	}
	dfuncdefs++;
	funcdefs++;
	--nfuncdefs;
    }
    return TRUE;
}

/*
 * NAME:	func2cmp()
 * DESCRIPTION:	compare function tables
 */
static bool func2cmp(dfuncdefs, funcdefs, dprog, prog, nfuncdefs)
register dfuncdef *dfuncdefs, *funcdefs;
register char *dprog, *prog;
register int nfuncdefs;
{
    while (nfuncdefs != 0) {
	if (dfuncdefs->class != (funcdefs->class & ~C_COMPILED) ||
	    dfuncdefs->inherit != funcdefs->inherit ||
	    dfuncdefs->index != funcdefs->index ||
	    PROTO_FTYPE(dprog + dfuncdefs->offset) !=
					PROTO_FTYPE(prog + funcdefs->offset)) {
	    return FALSE;
	}
	dfuncdefs++;
	funcdefs++;
	--nfuncdefs;
    }
    return TRUE;
}

/*
 * NAME:	varcmp()
 * DESCRIPTION:	compare variable tables
 */
static bool varcmp(dvardefs, vardefs, nvardefs)
register dvardef *dvardefs, *vardefs;
register int nvardefs;
{
    while (nvardefs != 0) {
	if (dvardefs->class != vardefs->class ||
	    dvardefs->inherit != vardefs->inherit ||
	    dvardefs->index != vardefs->index ||
	    dvardefs->type != vardefs->type) {
	    return FALSE;
	}
	dvardefs++;
	vardefs++;
	--nvardefs;
    }
    return TRUE;
}

/*
 * NAME:	precomp->restore()
 * DESCRIPTION:	restore and replace precompiled objects
 */
void pc_restore(fd, conv)
int fd, conv;
{
    dump_header dh;
    register precomp *l, **pc;
    register Uint i;
    register uindex oindex;
    register char *name;
    long posn;

    if (nprecomps != 0) {
	/* re-initialize tables before restore */
	for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	    (*pc)->oindex = UINDEX_MAX;
	}
	for (i = nprecomps; i > 0; ) {
	    map[2 * --i] = nprecomps;
	}
    }

    /* read header */
    if (conv) {
	odump_header odh;

	conf_dread(fd, (char *) &odh, odh_layout, (Uint) 1);
	if (nprecomps != 0 || odh.nprecomps != 0) {
	    fatal("precompiled objects during conversion");
	}
	dh.nprecomps = 0;
    } else {
	conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);
    }

    if (dh.nprecomps != 0) {
	register dump_precomp *dpc;
	register dump_inherit *dinh;
	register char *imap;
	register dstrconst *strings;
	register char *stext;
	register dfuncdef *funcdefs;
	register dvardef *vardefs;
	register char *funcalls;

	/*
	 * Restore old precompiled objects.
	 */
	dpc = ALLOCA(dump_precomp, dh.nprecomps);
	conf_dread(fd, (char *) dpc, dp_layout, (Uint) dh.nprecomps);
	dinh = ALLOCA(dump_inherit, dh.ninherits);
	conf_dread(fd, (char *) dinh, di_layout, dh.ninherits);
	imap = ALLOCA(char, dh.imapsz);
	if (P_read(fd, imap, dh.imapsz) != dh.imapsz) {
	    fatal("cannot read from dump file");
	}
	if (dh.nstrings != 0) {
	    strings = ALLOCA(dstrconst, dh.nstrings);
	    conf_dread(fd, (char *) strings, DSTR_LAYOUT, dh.nstrings);
	    if (dh.stringsz != 0) {
		stext = ALLOCA(char, dh.stringsz);
		if (P_read(fd, stext, dh.stringsz) != dh.stringsz) {
		    fatal("cannot read from dump file");
		}
	    }
	}
	if (dh.nfuncdefs != 0) {
	    funcdefs = ALLOCA(dfuncdef, dh.nfuncdefs);
	    conf_dread(fd, (char *) funcdefs, DF_LAYOUT, dh.nfuncdefs);
	}
	if (dh.nvardefs != 0) {
	    vardefs = ALLOCA(dvardef, dh.nvardefs);
	    conf_dread(fd, (char *) vardefs, DV_LAYOUT, dh.nvardefs);
	}
	if (dh.nfuncalls != 0) {
	    funcalls = ALLOCA(char, 2 * dh.nfuncalls);
	    if (P_read(fd, funcalls, 2 * dh.nfuncalls) != 2 * dh.nfuncalls) {
		fatal("cannot read from dump file");
	    }
	}

	for (i = dh.nprecomps; i > 0; --i) {
	    /* restored object must still be precompiled */
	    oindex = dinh[dpc->ninherits - 1].oindex;
	    name = OBJ(oindex)->chain.name;
	    for (pc = precompiled; ; pc++) {
		l = *pc;
		if (l == (precomp *) NULL) {
		    error("Restored object not precompiled: /%s", name);
		}
		if (strcmp(name, l->inherits[l->ninherits - 1].name) == 0) {
		    hash_add(l->oindex = oindex, pc - precompiled);
		    fixinherits(inherits + itab[pc - precompiled], l->inherits,
				l->ninherits);
		    if (dpc->ninherits != l->ninherits ||
			dpc->imapsz != l->imapsz ||
			dpc->nstrings != l->nstrings ||
			dpc->stringsz != l->stringsz ||
			dpc->nfuncdefs != l->nfuncdefs ||
			dpc->nvardefs != l->nvardefs ||
			dpc->nfuncalls != l->nfuncalls ||
			!inh1cmp(dinh, inherits + itab[pc - precompiled],
				 l->ninherits) ||
			memcmp(imap, l->imap, l->imapsz) != 0 ||
			!dstrcmp(strings, l->sstrings, l->nstrings) ||
			memcmp(stext, l->stext, l->stringsz) != 0 ||
			!func1cmp(funcdefs, l->funcdefs, l->program,
				  l->nfuncdefs) ||
			!varcmp(vardefs, l->vardefs, l->nvardefs) ||
			memcmp(funcalls, l->funcalls, 2 * l->nfuncalls) != 0) {
			/* not the same */
			error("Restored different precompiled object /%s",
			      name);
		    }
		    break;
		}
	    }

	    dinh += dpc->ninherits;
	    imap += dpc->imapsz;
	    strings += dpc->nstrings;
	    stext += dpc->stringsz;
	    funcdefs += dpc->nfuncdefs;
	    vardefs += dpc->nvardefs;
	    funcalls += 2 * dpc->nfuncalls;
	    dpc++;
	}

	if (dh.nfuncalls != 0) {
	    AFREE(funcalls - dh.nfuncalls);
	}
	if (dh.nvardefs != 0) {
	    AFREE(vardefs - dh.nvardefs);
	}
	if (dh.nfuncdefs != 0) {
	    AFREE(funcdefs - dh.nfuncdefs);
	}
	if (dh.nstrings != 0) {
	    if (dh.stringsz != 0) {
		AFREE(stext - dh.stringsz);
	    }
	    AFREE(strings - dh.nstrings);
	}
	AFREE(imap - dh.imapsz);
	AFREE(dinh - dh.ninherits);
	AFREE(dpc - dh.nprecomps);
    }

    posn = P_lseek(fd, 0L, SEEK_CUR);	/* preserve seek position */
    for (pc = precompiled, i = 0; *pc != (precomp *) NULL; pc++, i++) {
	l = *pc;
	if (l->oindex == UINDEX_MAX) {
	    register object *obj;

	    obj = o_find(name = l->inherits[l->ninherits - 1].name, OACC_READ);
	    if (obj != (object *) NULL) {
		register control *ctrl;

		ctrl = o_control(obj);
		if (ctrl->compiled > l->compiled) {
		    /* interpreted object is more recent */
		    continue;
		}

		/*
		 * replace by precompiled
		 */
		l->oindex = obj->index;
		fixinherits(inherits + itab[i], l->inherits, l->ninherits);
		if (ctrl->nstrings != 0) {
		    d_get_strconst(ctrl, ctrl->ninherits - 1, 0);
		}
		if (ctrl->ninherits != l->ninherits ||
		    ctrl->imapsz != l->imapsz ||
		    ctrl->nstrings != l->nstrings ||
		    ctrl->strsize != l->stringsz ||
		    ctrl->nfuncdefs != l->nfuncdefs ||
		    ctrl->nvardefs != l->nvardefs ||
		    ctrl->nclassvars != l->nclassvars ||
		    ctrl->nfuncalls != l->nfuncalls ||
		    !inh2cmp(ctrl->inherits, inherits + itab[pc - precompiled],
			     l->ninherits) ||
		    memcmp(ctrl->imap, l->imap, l->imapsz) != 0 ||
		    !dstrcmp(ctrl->sstrings, l->sstrings, l->nstrings) ||
		    memcmp(ctrl->stext, l->stext, l->stringsz) != 0 ||
		    !func2cmp(d_get_funcdefs(ctrl), l->funcdefs,
			      d_get_prog(ctrl), l->program, l->nfuncdefs) ||
		    !varcmp(d_get_vardefs(ctrl), l->vardefs,
			    l->nvardefs) ||
		    memcmp(ctrl->classvars, l->classvars,
			   l->nclassvars * 3) != 0 ||
		    memcmp(d_get_funcalls(ctrl), l->funcalls,
			   2 * l->nfuncalls) != 0) {
		    /* not the same */
		    error("Precompiled object != restored object /%s", name);
		}
		d_del_control(ctrl);
		hash_add(l->oindex, (uindex) i);

		obj->flags |= O_COMPILED;
		obj->cfirst = SW_UNUSED;
		obj->ctrl = ctrl = d_new_control();
		ctrl->oindex = obj->index;
		pc_control(ctrl, obj);
		ctrl->flags |= CTRL_COMPILED;

		if (obj->data != (dataspace *) NULL) {
		    obj->data->ctrl = ctrl;
		    ctrl->ndata++;
		}
	    } else {
		/*
		 * new precompiled object
		 */
		fixinherits(inherits + itab[i], l->inherits, l->ninherits);
		l->oindex = pc_obj(name, inherits + itab[i], l->ninherits);
		hash_add(l->oindex, (uindex) i);
	    }
	}
    }
    P_lseek(fd, posn, SEEK_SET);	/* restore position */
}


/*
 * NAME:	call_kfun()
 * DESCRIPTION:	call a kernel function
 */
void call_kfun(f, n)
frame *f;
register int n;
{
    register kfunc *kf;

    kf = &kftab[n];
    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
	i_typecheck(f, (frame *) NULL, kf->name, "kfun", kf->proto,
		    PROTO_NARGS(kf->proto), TRUE);
    }
    n = (*kf->func)(f);
    if (n != 0) {
	error("Bad argument %d for kfun %s", n, kf->name);
    }
}

/*
 * NAME:	call_kfun_arg()
 * DESCRIPTION:	call a kernel function with variable # of arguments
 */
void call_kfun_arg(f, n, nargs)
frame *f;
register int n, nargs;
{
    register kfunc *kf;

    kf = &kftab[n];
    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
	i_typecheck(f, (frame *) NULL, kf->name, "kfun", kf->proto, nargs,
		    TRUE);
    }
    n = (*kf->func)(f, nargs);
    if (n != 0) {
	error("Bad argument %d for kfun %s", n, kf->name);
    }
}

/*
 * NAME:	xdiv()
 * DESCRIPTION:	perform integer division
 */
Int xdiv(i, d)
register Int i, d;
{
    if (d == 0) {
	error("Division by zero");
    }
    if ((i | d) < 0) {
	Int r;

	r = ((Uint) ((i < 0) ? -i : i)) / ((Uint) ((d < 0) ? -d : d));
	return ((i ^ d) < 0) ? -r : r;
    }
    return ((Uint) i) / ((Uint) d);
}

/*
 * NAME:	xmod()
 * DESCRIPTION:	perform integer modulus
 */
Int xmod(i, d)
register Int i, d;
{
    if (d == 0) {
	error("Modulus by zero");
    }
    if (d < 0) {
	d = -d;
    }
    if (i < 0) {
	return - (Int) (((Uint) -i) % ((Uint) d));
    }
    return ((Uint) i) % ((Uint) d);
}

/*
 * NAME:	xlshift()
 * DESCRIPTION:	perform left shift
 */
Int xlshift(i, shift)
register Int i, shift;
{
    if ((shift & ~31) != 0) {
	if (shift < 0) {
	    error("Negative left shift");
	}
	return 0;
    }
    return (Uint) i << shift;
}

/*
 * NAME:	xrshift()
 * DESCRIPTION:	perform right shift
 */
Int xrshift(i, shift)
register Int i, shift;
{
    if ((shift & ~31) != 0) {
	if (shift < 0) {
	    error("Negative right shift");
	}
	return 0;
    }
    return (Uint) i >> shift;
}

/*
 * NAME:	poptruthval()
 * DESCRIPTION:	pop a truth value from the stack
 */
bool poptruthval(f)
register frame *f;
{
    switch (f->sp->type) {
    case T_NIL:
	f->sp++;
	return FALSE;

    case T_INT:
	return (f->sp++)->u.number != 0;

    case T_FLOAT:
	f->sp++;
	return !VFLT_ISZERO(f->sp - 1);

    case T_STRING:
	str_del(f->sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
    case T_LWOBJECT:
	arr_del(f->sp->u.array);
	break;
    }
    f->sp++;
    return TRUE;
}

/*
 * NAME:	new_rlimits()   
 * DESCRIPTION:	handle rlimits
 */
void new_rlimits(f)
register frame *f;
{
    if (f->sp[1].type != T_INT) {
	error("Bad rlimits depth type");
    }
    if (f->sp->type != T_INT) {
	error("Bad rlimits ticks type");
    }
    f->sp += 2;

    i_new_rlimits(f, f->sp[-1].u.number, f->sp[-2].u.number);
}

/*
 * NAME:	switch_range()
 * DESCRIPTION:	handle a range switch
 */
int switch_range(i, tab, h)
register Int i, *tab;
register int h;
{
    register int l, m;
    register Int *t;

    l = 0;
    do {
	m = (l + h) >> 1;
	t = tab + (m << 1);
	if (i < *t++) {
	    h = m;	/* search in lower half */
	} else if (i > *t) {
	    l = m + 1;	/* search in upper half */
	} else {
	    return m + 1;	/* found */
	}
    } while (l < h);

    return 0;		/* not found */
}

/*
 * NAME:	switch_str()
 * DESCRIPTION:	handle a str switch
 */
int switch_str(v, ctrl, tab, h)
value *v;
register control *ctrl;
register char *tab;
register int h;
{
    register int l, m, c;
    register char *t;
    register string *s;

    if (VAL_NIL(v)) {
	return (tab[0] == 0);
    } else if (v->type != T_STRING) {
	i_del_value(v);
	return 0;
    }

    s = v->u.string;
    if (*tab++ == 0) {
	tab -= 3;
	l = 1;
    } else {
	l = 0;
    }

    do {
	m = (l + h) >> 1;
	t = tab + 3 * m;
	c = str_cmp(s, d_get_strconst(ctrl, t[0],
				      (UCHAR(t[1]) << 8) + UCHAR(t[2])));
	if (c == 0) {
	    str_del(s);
	    return m + 1;	/* found */
	} else if (c < 0) {
	    h = m;	/* search in lower half */
	} else {
	    l = m + 1;	/* search in upper half */
	}
    } while (l < h);

    str_del(s);
    return 0;		/* not found */
}
