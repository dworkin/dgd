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

static uindex *map;		/* object -> precompiled */
static dinherit *inherits;	/* inherited objects */
static int *itab;		/* inherit index table */
static uindex nprecomps;	/* # precompiled objects */

static uindex *rmap;		/* object -> restored precompiled */
static dinherit *rinherits;	/* restored inherited objects */
static int *ritab;		/* restored inherit index table */
static precomp *restored;	/* restored precompiled objects */
static uindex nrestored;	/* # restored precompiled objects */

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
    obj = o_new(name, (object *) NULL, &ctrl);
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
 * DESCRIPTION:	add object to precompiled hash table
 */
static void hash_add(obj, idx, map, size)
object *obj;
uindex idx;
register uindex *map, size;
{
    register uindex i, j;

    i = obj->index % size;
    if (map[2 * i] == size) {
	map[2 * i] = idx;
	map[2 * i + 1] = size;
    } else {
	for (j = 0; map[2 * j] != size; j++) ;
	map[2 * j] = map[2 * i];
	map[2 * j + 1] = map[2 * i + 1];
	map[2 * i] = idx;
	map[2 * i + 1] = j;
    }
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
	if ((*pc)->obj->count != 0 && ((*pc)->obj->flags & O_COMPILED)) {
	    n++;
	}
    }

    a = arr_new((long) n);
    v = a->elts;
    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	if ((*pc)->obj->count != 0 && ((*pc)->obj->flags & O_COMPILED)) {
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
    register uindex i;
    register precomp *l;

    i = obj->index % nprecomps;
    for (;;) {
	if ((l=precompiled[map[2 * i]]) != (precomp *) NULL && l->obj == obj) {
	    ctrl->inherits = inherits + itab[i];
	    break;
	}
	i = map[2 * i + 1];
	if (i == nprecomps) {
	    /* look among the restored objects */
	    for (i = obj->index % nrestored;
		 (l=&restored[rmap[2 * i]])->obj != obj;
		 i = rmap[2 * i + 1]) ;
	    ctrl->inherits = rinherits + ritab[i];
	    break;
	}
    }

    ctrl->nsectors = 0;
    ctrl->sectors = (sector *) NULL;

    ctrl->ninherits = l->ninherits;
    ctrl->niinherits = l->niinherits;
    ctrl->iinherits = l->iinherits;

    ctrl->compiled = l->compiled;

    ctrl->progsize = l->progsize;
    ctrl->prog = l->program;

    ctrl->nstrings = l->nstrings;
    ctrl->strings = (string **) NULL;
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

typedef struct {
    uindex oindex;		/* object index */
    uindex funcoffset;		/* function offset */
    unsigned short varoffset;	/* variable offset */
} dump_inherit;

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

    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	if (((*pc)->obj->flags & O_COMPILED) && (*pc)->obj->u.ref != 0) {
	    dh.nprecomps++;
	    dh.ninherits += (*pc)->ninherits;
	    dh.nstrings += (*pc)->nstrings;
	    dh.stringsz += (*pc)->stringsz;
	    dh.nfuncdefs += (*pc)->nfuncdefs;
	    dh.nvardefs += (*pc)->nvardefs;
	    dh.nfuncalls += (*pc)->nfuncalls;
	}
    }
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
	    if (((*pc)->obj->flags & O_COMPILED) && (*pc)->obj->u.ref != 0) {
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
		    funcdefs += dpc->nfuncdefs;
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
 * NAME:	precomp->restore()
 * DESCRIPTION:	restore precompiled objects
 */
void pc_restore(fd)
int fd;
{
    dump_header dh;
    dump_precomp *dpc;
    dump_inherit *inh;
    dstrconst *strings;
    char *stext, *funcalls;
    dfuncdef *funcdefs;
    dvardef *vardefs;
    object *obj, **changed;
    register precomp **pc;
    register uindex i;
    register char *name;

    if (read(fd, (char *) &dh, sizeof(dump_header)) != sizeof(dump_header)) {
	fatal("cannot restore precompiled objects header");
    }

    if (dh.nprecomps != 0) {
	/*
	 * restore precompiled objects
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

	if (read(fd, (char *) dpc, dh.nprecomps * sizeof(dump_precomp)) !=
					dh.nprecomps * sizeof(dump_precomp) ||
	    read(fd, (char *) inh, dh.ninherits * sizeof(dump_inherit)) !=
					dh.ninherits * sizeof(dump_inherit) ||
	    (dh.nstrings != 0 &&
	     read(fd, (char *) strings, dh.nstrings * sizeof(dstrconst)) !=
					dh.nstrings * sizeof(dstrconst)) ||
	    (dh.stringsz != 0 && read(fd, stext, dh.stringsz) != dh.stringsz) ||
	    (dh.nfuncdefs != 0 &&
	     read(fd, (char *) funcdefs, dh.nfuncdefs * sizeof(dfuncdef)) !=
					dh.nfuncdefs * sizeof(dfuncdef)) ||
	    (dh.nvardefs != 0 &&
	     read(fd, (char *) vardefs, dh.nvardefs * sizeof(dvardef)) !=
					    dh.nvardefs * sizeof(dvardef)) ||
	    (dh.nfuncalls != 0 &&
	     read(fd, funcalls, 2 * dh.nfuncalls) != 2 * dh.nfuncalls)) {
	    fatal("cannot restore precompiled objects");
	}

	restored = ALLOCA(precomp, nrestored = dh.nprecomps);
	rinherits = ALLOCA(dinherit, dh.ninherits);
	ritab = ALLOCA(int, dh.nprecomps);
	rmap = ALLOCA(uindex, 2 * dh.nprecomps);

	/* restored inherits */
	for (i = dh.ninherits; i > 0; --i) {
	    rinherits->obj = o_objref(inh->oindex);
	    rinherits->funcoffset = inh->funcoffset;
	    (rinherits++)->varoffset = (inh++)->varoffset;
	}
	rinherits -= dh.ninherits;
	AFREE(inh - dh.ninherits);

	/* initialize empty rmap */
	for (i = dh.nprecomps; i > 0; ) {
	    rmap[2 * --i] = dh.nprecomps;
	}

	dh.ninherits = 0;
	for (i = 0; i < dh.nprecomps; i++) {
	    restored->obj = rinherits[dpc->ninherits - 1].obj;
	    ritab[i] = dh.ninherits;
	    dh.ninherits += dpc->ninherits;
	    rinherits += dpc->ninherits;

	    restored->ninherits = dpc->ninherits;
	    restored->inherits = (pcinherit *) NULL;
	    restored->niinherits = 0;
	    restored->iinherits = (char *) NULL;

	    restored->compiled = dpc->compiled;

	    restored->progsize = 0;
	    restored->program = (char *) NULL;

	    restored->nstrings = dpc->nstrings;
	    restored->sstrings = strings;
	    restored->stext = stext;
	    restored->stringsz = dpc->stringsz;
	    strings += dpc->nstrings;
	    stext += dpc->stringsz;

	    restored->nfunctions = 0;
	    restored->functions = (pcfunc *) NULL;
	    restored->nfuncdefs = dpc->nfuncdefs;
	    restored->funcdefs = funcdefs;
	    funcdefs += dpc->nfuncdefs;

	    restored->nvardefs = dpc->nvardefs;
	    restored->vardefs = vardefs;
	    vardefs += dpc->nvardefs;

	    restored->nfuncalls = dpc->nfuncalls;
	    restored->funcalls = funcalls;
	    funcalls += 2 * dpc->nfuncalls;

	    restored->nsymbols = 0;
	    restored->symbols = (dsymbol *) NULL;

	    restored->nvariables = dpc->nvariables;
	    restored->nfloatdefs = 0;
	    restored->nfloats = 0;

	    /* all restored precompiled objects must still be precompiled */
	    name = restored->obj->chain.name;
	    pc = precompiled;
	    for (;;) {
		if (*pc == (precomp *) NULL) {
		    fatal("object not precompiled: /%s", name);
		}
		if (strcmp(name,
			   (*pc)->inherits[(*pc)->ninherits - 1].name) == 0) {
		    break;
		}
		pc++;
	    }

	    hash_add(restored->obj, i, rmap, dh.nprecomps);

	    restored++;
	    dpc++;
	}
	restored -= dh.nprecomps;
	rinherits -= dh.ninherits;
	strings -= dh.nstrings;
	stext -= dh.stringsz;
	funcdefs -= dh.nfuncdefs;
	vardefs -= dh.nvardefs;
	funcalls -= 2 * dh.nfuncalls;
	AFREE(dpc - dh.nprecomps);

	/* initialize empty map */
	for (i = nprecomps; i > 0; ) {
	    map[2 * --i] = nprecomps;
	}

	/* go through the list of precompiled objects and find changes */
	changed = ALLOCA(object*, nprecomps);
	i = 0;
	for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	    name = (*pc)->inherits[(*pc)->ninherits - 1].name;
	    obj = o_find(name);
	    if (obj == (object *) NULL) {
		/* new object */
		changed[i++] = obj = pc_obj(name,
					    inherits + itab[pc - precompiled],
					    (*pc)->ninherits);
	    } else if (!(obj->flags & O_COMPILED) ||
		       (*pc)->compiled != o_control(obj)->compiled) {
		char buf[STRINGSZ + 1];

		/* changed object */
		sprintf(buf, "/%s", name);
		changed[i++] = obj = pc_obj(buf,
					    inherits + itab[pc - precompiled],
					    (*pc)->ninherits);
	    }
	    hash_add((*pc)->obj = obj, pc - precompiled, map, nprecomps);
	}

	if (i != nprecomps) {
	    /* refresh control blocks */
	    d_swapout(1);
	}

# if 0
	if (i != 0 && !c_upgrade(changed, i)) {
	    fatal("failed to upgrade precompiled objects");
	}
# endif

	AFREE(changed);
	AFREE(rmap);
	AFREE(ritab);
	AFREE(rinherits);
	AFREE(restored);

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
    }
}

/*
 * NAME:	pc->remap()
 * DESCRIPTION:	change the hash table mapping for a precompiled object
 */
void pc_remap(from, to)
object *from, *to;
{
    register uindex *m, i, idx;
    object *obj;

    m = ALLOCA(uindex, 2 * nprecomps);
    for (i = 0; i < nprecomps; i++) {
	m[2 * i] = nprecomps;
    }
    for (i = 0; i < nprecomps; i++) {
	obj = precompiled[idx]->obj;
	idx = map[2 * i];
	if (obj == from) {
	    precompiled[idx]->obj = to;
	    hash_add(to, idx, m, nprecomps);
	} else {
	    hash_add(obj, idx, m, nprecomps);
	}
    }
    memcpy(map, m, 2 * nprecomps * sizeof(uindex));
    AFREE(m);
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
    if (sp->type == T_INT) {
	return (sp++)->u.number != 0;
    }
    if (sp->type == T_FLOAT) {
	sp++;
	return !VFLT_ISZERO(sp - 1);
    }
    i_del_value(sp++);
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
	do {
	    ec_pop();
	    i_set_rllevel(cstack[--csi]);
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
