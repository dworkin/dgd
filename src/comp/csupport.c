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
static void pc_inherits(inh, pcinh, ninherits, compiled)
register dinherit *inh;
register pcinherit *pcinh;
register int ninherits;
Uint compiled;
{
    register Uint cc;

    cc = 0;
    while (--ninherits > 0) {
	if ((inh->obj=o_find(pcinh->name)) == (object *) NULL) {
	    fatal("cannot inherit /%s from /%s", pcinh->name,
		  pcinh[ninherits].name);
	}
	if (precompiled[inh->obj->index]->compiled > cc) {
	    cc = precompiled[inh->obj->index]->compiled;
	}

	inh->funcoffset = pcinh->funcoffset;
	(inh++)->varoffset = (pcinh++)->varoffset;
    }
    if (cc > compiled) {
	fatal("object out of date: /%s", pcinh->name);
    }
    if (o_find(pcinh->name) != (object *) NULL) {
	fatal("object precompiled twice: /%s", pcinh->name);
    }
    inh->funcoffset = pcinh->funcoffset;
    inh->varoffset = pcinh->varoffset;
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

    --nfuncs;
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
static object *pc_obj(name, inherits, ninherits)
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

    return obj;
}

/*
 * NAME:	hash_add()
 * DESCRIPTION:	add object->elt to hash table
 */
static void hash_add(obj, elt)
object *obj;
uindex elt;
{
    register uindex i, j;

    i = obj->index % nprecomps;
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
static uindex hash_find(obj)
register object *obj;
{
    register uindex i;
    uindex j;

    i = obj->index % nprecomps;
    while (precompiled[j = map[2 * i]]->obj != obj) {
	i = map[2 * i + 1];
    }
    return j;
}

/*
 * NAME:	precomp->preload()
 * DESCRIPTION:	preload compiled objects
 */
void pc_preload(auto_obj, driver_obj)
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

	    pc_inherits(inherits + ninherits, l->inherits, l->ninherits,
			l->compiled);
	    itab[n] = ninherits;
	    l->obj = pc_obj(l->inherits[l->ninherits - 1].name,
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
}

/*
 * NAME:	precomp->list()
 * DESCRIPTION:	return an array with all precompiled objects
 */
array *pc_list()
{
    array *a;
    register uindex n;
    register value *v;
    register precomp **pc;

    for (pc = precompiled, n = 0; *pc != (precomp *) NULL; pc++) {
	if ((*pc)->obj != (object *) NULL && (*pc)->obj->count != 0 &&
	    ((*pc)->obj->flags & O_COMPILED)) {
	    n++;
	}
    }

    a = arr_new((long) n);
    v = a->elts;
    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	if ((*pc)->obj != (object *) NULL && (*pc)->obj->count != 0 &&
	    ((*pc)->obj->flags & O_COMPILED)) {
	    v->type = T_OBJECT;
	    v->oindex = (*pc)->obj->index;
	    v->u.objcnt = (*pc)->obj->count;
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

    l = precompiled[i = hash_find(obj)];

    ctrl->ninherits = l->ninherits;
    ctrl->inherits = inherits + itab[i];

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
    ctrl->nfloatdefs = l->nfloatdefs;
    ctrl->nfloats = l->nfloats;
}


typedef struct {
    uindex nprecomps;		/* # precompiled objects */
    Uint ninherits;		/* total # inherits */
    Uint nstrings;		/* total # strings */
    Uint stringsz;		/* total strings size */
    Uint nfuncdefs;		/* total # funcdefs */
    Uint nvardefs;		/* total # vardefs */
    Uint nfuncalls;		/* total # function calls */
} dump_header;

static char dh_layout[] = "uiiiiii";

typedef struct {
    Uint compiled;		/* compile time */
    short ninherits;		/* # inherits */
    unsigned short nstrings;	/* # strings */
    Uint stringsz;		/* strings size */
    short nfuncdefs;		/* # funcdefs */
    short nvardefs;		/* # vardefs */
    uindex nfuncalls;		/* # function calls */
    short nvariables;		/* # variables */
} dump_precomp;

static char dp_layout[] = "ississus";

typedef struct {
    uindex oindex;		/* object index */
    uindex funcoffset;		/* function offset */
    unsigned short varoffset;	/* variable offset */
} dump_inherit;

static char di_layout[] = "uus";

/*
 * NAME:	precomp->dump()
 * DESCRIPTION:	dump precompiled objects
 */
bool pc_dump(fd)
int fd;
{
    dump_header dh;
    register precomp **pc;
    bool ok;

    dh.nprecomps = 0;
    dh.ninherits = 0;
    dh.nstrings = 0;
    dh.stringsz = 0;
    dh.nfuncdefs = 0;
    dh.nvardefs = 0;
    dh.nfuncalls = 0;

    /* first compute sizes of data to dump */
    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	if ((*pc)->obj != (object *) NULL && ((*pc)->obj->flags & O_COMPILED) &&
	    (*pc)->obj->u_ref != 0) {
	    dh.nprecomps++;
	    dh.ninherits += (*pc)->ninherits;
	    dh.nstrings += (*pc)->nstrings;
	    dh.stringsz += (*pc)->stringsz;
	    dh.nfuncdefs += (*pc)->nfuncdefs;
	    dh.nvardefs += (*pc)->nvardefs;
	    dh.nfuncalls += (*pc)->nfuncalls;
	}
    }

    /* write header */
    if (write(fd, (char *) &dh, sizeof(dump_header)) != sizeof(dump_header)) {
	return FALSE;
    }

    ok = TRUE;

    if (dh.nprecomps != 0) {
	register dump_precomp *dpc;
	register int i;
	dump_inherit *inh;
	dinherit *inh2;
	dstrconst *strings;
	char *stext, *funcalls;
	dfuncdef *funcdefs;
	dvardef *vardefs;

	/*
	 * Save only the necessary information.
	 */
	dpc = ALLOCA(dump_precomp, dh.nprecomps);
	inh = ALLOCA(dump_inherit, dh.ninherits);
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
	    if ((*pc)->obj != (object *) NULL &&
		((*pc)->obj->flags & O_COMPILED) && (*pc)->obj->u_ref != 0) {
		dpc->compiled = (*pc)->compiled;
		dpc->ninherits = (*pc)->ninherits;
		dpc->nstrings = (*pc)->nstrings;
		dpc->stringsz = (*pc)->stringsz;
		dpc->nfuncdefs = (*pc)->nfuncdefs;
		dpc->nvardefs = (*pc)->nvardefs;
		dpc->nfuncalls = (*pc)->nfuncalls;
		dpc->nvariables = (*pc)->nvariables;

		inh2 = inherits + itab[pc - precompiled];
		for (i = dpc->ninherits; i > 0; --i) {
		    inh->oindex = inh2->obj->index;
		    inh->funcoffset = inh2->funcoffset;
		    (inh++)->varoffset = (inh2++)->varoffset;
		}

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
	strings -= dh.nstrings;
	stext -= dh.stringsz;
	funcdefs -= dh.nfuncdefs;
	vardefs -= dh.nvardefs;
	funcalls -= 2 * dh.nfuncalls;

	if (write(fd, (char *) dpc, dh.nprecomps * sizeof(dump_precomp)) !=
					dh.nprecomps * sizeof(dump_precomp) ||
	    write(fd, (char *) inh, dh.ninherits * sizeof(dump_inherit)) !=
					dh.ninherits * sizeof(dump_inherit) ||
	    (dh.nstrings != 0 &&
	     write(fd, (char *) strings, dh.nstrings * sizeof(dstrconst)) !=
					    dh.nstrings * sizeof(dstrconst)) ||
	    (dh.stringsz != 0 &&
	     write(fd, stext, dh.stringsz) != dh.stringsz) ||
	    (dh.nfuncdefs != 0 &&
	     write(fd, (char *) funcdefs, dh.nfuncdefs * sizeof(dfuncdef)) !=
					    dh.nfuncdefs * sizeof(dfuncdef)) ||
	    (dh.nvardefs != 0 &&
	     write(fd, (char *) vardefs, dh.nvardefs * sizeof(dvardef)) !=
					    dh.nvardefs * sizeof(dvardef)) ||
	    (dh.nfuncalls != 0 &&
	     write(fd, funcalls, 2 * dh.nfuncalls) != 2 * dh.nfuncalls)) {
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
	(inh++)->obj = (*pc)->obj;
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
	if (dinh->oindex != inh->obj->index ||
	    dinh->funcoffset != inh->funcoffset ||
	    dinh->varoffset != inh->varoffset) {
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
	if (dinh->obj != inh->obj ||
	    dinh->funcoffset != inh->funcoffset ||
	    dinh->varoffset != inh->varoffset) {
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
void pc_restore(fd)
int fd;
{
    dump_header dh;
    register precomp *l, **pc;
    register Uint i;
    register object *obj;
    register char *name;

    if (nprecomps != 0) {
	/* re-initialize tables before restore */
	for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	    (*pc)->obj = (object *) NULL;
	}
	for (i = nprecomps; i > 0; ) {
	    map[2 * --i] = nprecomps;
	}
    }

    /* read header */
    conf_dread(fd, (char *) &dh, dh_layout, (Uint) 1);

    if (dh.nprecomps != 0) {
	register dump_precomp *dpc;
	register dump_inherit *dinh;
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
	if (dh.nstrings != 0) {
	    strings = ALLOCA(dstrconst, dh.nstrings);
	    conf_dread(fd, (char *) strings, DSTR_LAYOUT, dh.nstrings);
	    if (dh.stringsz != 0) {
		stext = ALLOCA(char, dh.stringsz);
		if (read(fd, stext, dh.stringsz) != dh.stringsz) {
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
	    if (read(fd, funcalls, 2 * dh.nfuncalls) != 2 * dh.nfuncalls) {
		fatal("cannot read from dump file");
	    }
	}

	for (i = dh.nprecomps; i > 0; --i) {
	    /* restored object must still be precompiled */
	    obj = &otable[dinh[dpc->ninherits - 1].oindex];
	    name = obj->chain.name;
	    for (pc = precompiled; ; pc++) {
		l = *pc;
		if (l == (precomp *) NULL) {
		    fatal("restored object not precompiled: /%s", name);
		}
		if (strcmp(name, l->inherits[l->ninherits - 1].name) == 0) {
		    hash_add(l->obj = obj, pc - precompiled);
		    fixinherits(inherits + itab[pc - precompiled], l->inherits,
				l->ninherits);
		    if (dpc->ninherits != l->ninherits ||
			dpc->nstrings != l->nstrings ||
			dpc->stringsz != l->stringsz ||
			dpc->nfuncdefs != l->nfuncdefs ||
			dpc->nvardefs != l->nvardefs ||
			dpc->nfuncalls != l->nfuncalls ||
			!inh1cmp(dinh, inherits + itab[pc - precompiled],
				 l->ninherits) ||
			!dstrcmp(strings, l->sstrings, l->nstrings) ||
			memcmp(stext, l->stext, l->stringsz) != 0 ||
			!func1cmp(funcdefs, l->funcdefs, l->program,
				  l->nfuncdefs) ||
			!varcmp(vardefs, l->vardefs, l->nvardefs) ||
			memcmp(funcalls, l->funcalls, 2 * l->nfuncalls) != 0) {
			/* not the same */
			fatal("restored different precompiled object /%s",
			      name);
		    }
		    break;
		}
	    }

	    dinh += dpc->ninherits;
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
	AFREE(dinh - dh.ninherits);
	AFREE(dpc - dh.nprecomps);
    }

    for (pc = precompiled, i = 0; *pc != (precomp *) NULL; pc++, i++) {
	l = *pc;
	if (l->obj == (object *) NULL) {
	    obj = o_find(name = l->inherits[l->ninherits - 1].name);
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
		l->obj = obj;
		fixinherits(inherits + itab[i], l->inherits, l->ninherits);
		if (ctrl->nstrings != 0) {
		    d_get_strconst(ctrl, ctrl->ninherits - 1, 0);
		}
		if (ctrl->ninherits != l->ninherits ||
		    ctrl->nstrings != l->nstrings ||
		    ctrl->strsize != l->stringsz ||
		    ctrl->nfuncdefs != l->nfuncdefs ||
		    ctrl->nvardefs != l->nvardefs ||
		    ctrl->nfuncalls != l->nfuncalls ||
		    !inh2cmp(ctrl->inherits, inherits + itab[pc - precompiled],
			     l->ninherits) ||
		    !dstrcmp(ctrl->sstrings, l->sstrings, l->nstrings) ||
		    memcmp(ctrl->stext, l->stext, l->stringsz) != 0 ||
		    !func2cmp(d_get_funcdefs(ctrl), l->funcdefs,
			      d_get_prog(ctrl), l->program, l->nfuncdefs) ||
		    !varcmp(d_get_vardefs(ctrl), l->vardefs, l->nvardefs) ||
		    memcmp(d_get_funcalls(ctrl), l->funcalls,
			   2 * l->nfuncalls) != 0) {
		    /* not the same */
		    fatal("precompiled object != restored object /%s", name);
		}

		d_del_control(ctrl);
		obj->flags |= O_COMPILED;
	    } else {
		/*
		 * new precompiled object
		 */
		l->obj = pc_obj(name, inherits + itab[i], l->ninherits);
		fixinherits(inherits + itab[i], l->inherits, l->ninherits);
	    }
	    hash_add(l->obj, (uindex) i);
	}
    }
}


/*
 * NAME:	call_kfun()
 * DESCRIPTION:	call a kernel function
 */
void call_kfun(n)
register int n;
{
    register kfunc *kf;

    kf = &kftab[n];
    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
	i_typecheck(kf->name, "kfun", kf->proto, PROTO_NARGS(kf->proto), TRUE);
    }
    n = (*kf->func)();
    if (n != 0) {
	error("Bad argument %d for kfun %s", n, kf->name);
    }
}

/*
 * NAME:	call_kfun_arg()
 * DESCRIPTION:	call a kernel function with variable # of arguments
 */
void call_kfun_arg(n, nargs)
register int n, nargs;
{
    register kfunc *kf;

    kf = &kftab[n];
    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
	i_typecheck(kf->name, "kfun", kf->proto, nargs, TRUE);
    }
    n = (*kf->func)(nargs);
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
    if ((i | d) < 0) {
	Int r;

	r = ((Uint) ((i < 0) ? -i : i)) % ((Uint) ((d < 0) ? -d : d));
	return ((i ^ d) < 0) ? -r : r;
    }
    return ((Uint) i) % ((Uint) d);
}

/*
 * NAME:	poptruthval()
 * DESCRIPTION:	pop a truth value from the stack
 */
bool poptruthval()
{
    switch (sp->type) {
    case T_INT:
	return (sp++)->u.number != 0;

    case T_FLOAT:
	sp++;
	return !VFLT_ISZERO(sp - 1);

    case T_STRING:
	str_del(sp->u.string);
	break;

    case T_ARRAY:
    case T_MAPPING:
	arr_del(sp->u.array);
	break;
    }
    sp++;
    return TRUE;
}

static int cstack[ERRSTACKSZ];	/* catch stack */
static int csi;			/* catch stack index */

/*
 * NAME:	pre_catch()
 * DESCRIPTION:	prepare for a catch
 */
void pre_catch()
{
    if (csi == ERRSTACKSZ) {
	error("Too deep catch() nesting");
    }
    cstack[csi++] = i_get_rllevel();
}

/*
 * NAME:	post_catch()
 * DESCRIPTION:	clean up after a catch
 */
void post_catch(n)
int n;
{
    if (n == 0) {
	i_set_rllevel(cstack[--csi]);
    } else {
	/* break, continue, return out of catch */
	i_set_rllevel(cstack[csi -= n]);
	do {
	    ec_pop();
	} while (--n != 0);
    }
}

/*
 * NAME:	pre_rlimits()
 * DESCRIPTION:	prepare for rlimits
 */
int pre_rlimits()
{
    if (sp[1].type != T_INT) {
	error("Bad rlimits depth type");
    }
    if (sp->type != T_INT) {
	error("Bad rlimits ticks type");
    }
    sp += 2;

    return i_set_rlimits(sp[-1].u.number, sp[-2].u.number);
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
int switch_str(v, tab, h)
value *v;
register char *tab;
register int h;
{
    register int l, m, c;
    register char *t;
    register string *s;
    register control *ctrl;

    if (v->type == T_INT && v->u.number == 0) {
	return (tab[0] == 0);
    } else if (v->type != T_STRING) {
	i_del_value(v);
	return 0;
    }

    s = v->u.string;
    ctrl = cframe->p_ctrl;
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
