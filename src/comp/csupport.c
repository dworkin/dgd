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
# include "csupport.h"

static dinherit *inherits;	/* inherited objects */
static int *itab;		/* inherit index table */
pcfunc *pcfunctions;		/* table of precompiled functions */

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
	    fatal("cannot inherit /%s", pcinh->name);
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
register unsigned short nfuncdefs, nfuncs;
{
    register char *p;
    register unsigned short index;

    --nfuncs;
    while (nfuncdefs > 0) {
	p = program + funcdefs->offset;
	if (!(PROTO_CLASS(p) & C_UNDEFINED)) {
	    p += PROTO_SIZE(p);
	    index = nfuncs + ((UCHAR(p[3]) << 8) | UCHAR(p[4]));
	    p[3] = index >> 8;
	    p[4] = index;
	}
	funcdefs++;
	--nfuncdefs;
    }
}

/*
 * NAME:	precomp->preload()
 * DESCRIPTION:	preload compiled objects
 */
void pc_preload(auto_name, driver_name)
char *auto_name, *driver_name;
{
    register precomp **pc, *l;
    register uindex nobjects, ninherits, nfuncs;
    register object *obj;
    control ctrl;
    char *name;

    nobjects = ninherits = nfuncs = 0;
    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	nobjects++;
	ninherits += (*pc)->ninherits;
	nfuncs += (*pc)->nfunctions;
    }

    if (nobjects > 0) {
	m_static();
	itab = ALLOC(int, nobjects + 1);
	inherits = ALLOC(dinherit, ninherits);
	if (nfuncs > 0) {
	    pcfunctions = ALLOC(pcfunc, nfuncs);
	}
	m_dynamic();

	itab[0] = 0;
	nobjects = ninherits = nfuncs = 0;

	for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	    l = *pc;
	    pc_inherits(ctrl.inherits = inherits + ninherits, l->inherits,
			ctrl.ninherits = l->ninherits, l->compiled);
	    ninherits += l->ninherits;
	    itab[++nobjects] = ninherits;

	    pc_funcdefs(l->program, l->funcdefs, l->nfuncdefs, nfuncs);
	    memcpy(pcfunctions + nfuncs, l->functions,
		   sizeof(pcfunc) * l->nfunctions);
	    nfuncs += l->nfunctions;

	    obj = o_new(name = l->inherits[l->ninherits - 1].name,
			(object *) NULL, &ctrl);
	    obj->flags |= O_COMPILED;
	    if (strcmp(name, driver_name) == 0) {
		obj->flags |= O_DRIVER;
	    } else if (strcmp(name, auto_name) == 0) {
		obj->flags |= O_AUTO;
	    }
	    obj->ctrl = (control *) NULL;
	    l->obj = obj;
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

    l = precompiled[i = obj->index];

    ctrl->nsectors = 0;
    ctrl->sectors = (sector *) NULL;

    ctrl->ninherits = itab[i + 1] - itab[i];
    ctrl->inherits = inherits + itab[i];

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
} dump_header;

typedef struct {
    short ninherits;		/* # inherits */
    unsigned short nstrings;	/* # strings */
    Uint stringsz;		/* strings size */
    short nfuncdefs;		/* # funcdefs */
    short nvardefs;		/* # vardefs */
    short nvariables;		/* # variables */
} dump_precomp;

typedef struct {
    uindex oindex;		/* object index */
    Uint ocount;		/* object count */
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

    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	if (((*pc)->obj->flags & O_COMPILED) && (*pc)->obj->u.ref != 0) {
	    dh.nprecomps++;
	    dh.ninherits += (*pc)->ninherits;
	    dh.nstrings += (*pc)->nstrings;
	    dh.stringsz += (*pc)->stringsz;
	    dh.nfuncdefs += (*pc)->nfuncdefs;
	    dh.nvardefs += (*pc)->nvardefs;
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
	char *stext;
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

	for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	    if (((*pc)->obj->flags & O_COMPILED) && (*pc)->obj->u.ref != 0) {
		dpc->ninherits = (*pc)->ninherits;
		dpc->nstrings = (*pc)->nstrings;
		dpc->stringsz = (*pc)->stringsz;
		dpc->nfuncdefs = (*pc)->nfuncdefs;
		dpc->nvardefs = (*pc)->nvardefs;
		dpc->nvariables = (*pc)->nvariables;

		inh2 = inherits + itab[(*pc)->obj->index];
		for (i = dpc->ninherits; i > 0; --i) {
		    inh->oindex = inh2->obj->index;
		    inh->ocount = inh2->obj->count;
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

		dpc++;
	    }
	}

	dpc -= dh.nprecomps;
	inh -= dh.ninherits;
	strings -= dh.nstrings;
	stext -= dh.stringsz;
	funcdefs -= dh.nfuncdefs;
	vardefs -= dh.nvardefs;

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
					    dh.nvardefs * sizeof(dvardef))) {
	    ok = FALSE;
	}

	AFREE(dpc);
	AFREE(inh);
	if (dh.nstrings != 0) {
	    AFREE(strings);
	    if (dh.stringsz != 0) {
		AFREE(stext);
	    }
	}
	if (dh.nfuncdefs != 0) {
	    AFREE(funcdefs);
	}
	if (dh.nvardefs != 0) {
	    AFREE(vardefs);
	}
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

    if (read(fd, (char *) &dh, sizeof(dump_header)) != sizeof(dump_header)) {
	fatal("cannot restore precompiled objects header");
    }

    if (dh.nprecomps != 0) {
	dump_precomp *dpc;
	dump_inherit *inh;
	dstrconst *strings;
	char *stext;
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

	if (read(fd, (char *) dpc, dh.nprecomps * sizeof(dump_precomp)) !=
					dh.nprecomps * sizeof(dump_precomp) ||
	    read(fd, (char *) inh, dh.ninherits * sizeof(dump_inherit)) !=
					dh.ninherits * sizeof(dump_inherit) ||
	    (dh.nstrings != 0 &&
	     read(fd, (char *) strings, dh.nstrings * sizeof(dstrconst)) !=
					dh.nstrings * sizeof(dstrconst)) ||
	    (dh.stringsz != 0 &&
	     read(fd, (char *) stext, dh.stringsz) != dh.stringsz) ||
	    (dh.nfuncdefs != 0 &&
	     read(fd, (char *) funcdefs, dh.nfuncdefs * sizeof(dfuncdef)) !=
					dh.nfuncdefs * sizeof(dfuncdef)) ||
	    (dh.nvardefs != 0 &&
	     read(fd, (char *) vardefs, dh.nvardefs * sizeof(dvardef)) !=
					dh.nvardefs * sizeof(dvardef))) {
	    fatal("cannot restore precompiled objects");
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
void post_catch()
{
    i_set_rllevel(cstack[--csi]);
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
