# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "fcontrol.h"
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
static void pc_inherits(inh, pcinh, ninherits)
register dinherit *inh;
register pcinherit *pcinh;
register int ninherits;
{
    while (--ninherits > 0) {
	if ((inh->obj=o_find(pcinh->name)) == (object *) NULL) {
	    fatal("cannot inherit /%s", pcinh->name);
	}
	inh->funcoffset = pcinh->funcoffset;
	(inh++)->varoffset = (pcinh++)->varoffset;
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
	mstatic();
	itab = ALLOC(int, nobjects + 1);
	inherits = ALLOC(dinherit, ninherits);
	if (nfuncs > 0) {
	    pcfunctions = ALLOC(pcfunc, nfuncs);
	}
	mdynamic();

	itab[0] = 0;
	nobjects = ninherits = nfuncs = 0;

	for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	    l = *pc;
	    message("Precompiled: /%s\n",
		    name = l->inherits[l->ninherits - 1].name);

	    pc_inherits(ctrl.inherits = inherits + ninherits, l->inherits,
			ctrl.ninherits = l->ninherits);
	    ninherits += l->ninherits;
	    itab[++nobjects] = ninherits;

	    pc_funcdefs(l->program, l->funcdefs, l->nfuncdefs, nfuncs);
	    memcpy(pcfunctions + nfuncs, l->functions,
		   sizeof(pcfunc) * l->nfunctions);
	    nfuncs += l->nfunctions;

	    obj = o_new(name, (object *) NULL, &ctrl);
	    obj->flags |= O_COMPILED;
	    if (strcmp(name, driver_name) == 0) {
		obj->flags |= O_DRIVER;
	    } else if (strcmp(name, auto_name) == 0) {
		obj->flags |= O_AUTO;
	    }
	    obj->ctrl = (control *) NULL;
	}
    }
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

    ctrl->compiled = l->compiled;

    ctrl->progsize = l->progsize;
    ctrl->prog = l->program;

    ctrl->nstrings = l->nstrings;
    ctrl->strings = (string **) NULL;
    ctrl->sstrings = l->sstrings;
    ctrl->stext = l->stext;
    if (ctrl->nstrings == 0) {
	ctrl->strsize = 0;
    } else {
	ctrl->strsize = ctrl->sstrings[ctrl->nstrings - 1].index +
			ctrl->sstrings[ctrl->nstrings - 1].len;
    }

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

/*
 * NAME:	call_kfun()
 * DESCRIPTION:	call a kernel function
 */
void call_kfun(n)
register int n;
{
    register kfunc *kf;

    kf = &KFUN(n);
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

    kf = &KFUN(n);
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
Int i, d;
{
    if (d == 0) {
	error("Division by zero");
    }
    return i / d;
}

/*
 * NAME:	xmod()
 * DESCRIPTION:	perform integer modulus
 */
Int xmod(i, d)
Int i, d;
{
    if (d == 0) {
	error("Modulus by zero");
    }
    return i % d;
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

typedef struct {
    value *sp;			/* stack pointer */
    int frame;			/* function call frame level */
    unsigned short lock;	/* lock value */
} catchinfo;

static catchinfo cstack[ERRSTACKSZ];	/* catch stack */
static int csi;				/* catch stack index */

/*
 * NAME:	pre_catch()
 * DESCRIPTION:	prepare for a catch
 */
void pre_catch()
{
    if (csi == ERRSTACKSZ) {
	error("Too deep catch() nesting");
    }
    cstack[csi].sp = sp;
    cstack[csi].frame = i_query_frame();
    cstack[csi++].lock = i_query_lock();
}

/*
 * NAME:	post_catch()
 * DESCRIPTION:	clean up after a catch
 */
void post_catch(flag)
bool flag;
{
    if (flag) {
	i_log_error(TRUE);
    }
    i_pop(cstack[--csi].sp - sp);
    i_set_frame(cstack[csi].frame);
    i_set_lock(cstack[csi].lock);
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
    ctrl = i_this_program();
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
