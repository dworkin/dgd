# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "fcontrol.h"
# include "table.h"
# include "control.h"
# include "csupport.h"

static object **inherits;	/* inherited objects */
static string **strings;	/* strings defined */
pcfunc *pcfunctions;		/* table of precompiled functions */

/*
 * NAME:	preload->inherits()
 * DESCRIPTION:	handle inherited objects
 */
static void pl_inherits(inh, ninherits)
register char **inh;
register int ninherits;
{
    char label[5];
    register int i;
    register object *obj;

    for (i = 0; i < ninherits - 1; i++) {
	obj = o_find(*inh);
	if (obj == (object *) NULL) {
	    fatal("cannot inherit \"/%s\"", *inh);
	}
	inherits[i] = obj;
	ctrl_inherit(obj, (string *) NULL);
	inh++;
    }
    inherits[i++] = (object *) NULL;
}

/*
 * NAME:	preload->strings()
 * DESCRIPTION:	handle string constants
 */
static void pl_strings(stext, slength, nstrings)
register char *stext;
register unsigned short *slength, nstrings;
{
    register unsigned short i;

    for (i = 0; i < nstrings; i++) {
	ctrl_dstring(strings[i] = str_new(stext, (long) *slength));
	stext += *slength++;
    }
}

/*
 * NAME:	getstring()
 * DESCRIPTION:	handle function definitions
 */
static string *getstring(inherit, index)
register unsigned short inherit, index;
{
    register control *ctrl;

    if (inherits[inherit] == (object *) NULL) {
	return strings[index];
    }
    ctrl = inherits[inherit]->ctrl;
    return d_get_strconst(ctrl, ctrl->ninherits - 1, index);
}

/*
 * NAME:	preload->funcdefs()
 * DESCRIPTION:	handle function definitions
 */
static void pl_funcdefs(program, funcdefs, nfuncdefs, nfuncs)
char *program;
register dfuncdef *funcdefs;
register unsigned short nfuncdefs, nfuncs;
{
    register char *p;
    register unsigned short index;

    --nfuncs;
    while (nfuncdefs > 0) {
	p = program + funcdefs->offset;
	ctrl_dfunc(getstring(UCHAR(funcdefs->inherit), funcdefs->index), p);
	if (!(PROTO_CLASS(p) & C_UNDEFINED)) {
	    p += PROTO_SIZE(p);
	    p = (char *) memcpy(ALLOC(char, 5), p, 5);
	    index = (UCHAR(p[3]) << 8) | UCHAR(p[4]);
	    index += nfuncs;
	    p[3] = index >> 8;
	    p[4] = index;
	    ctrl_dprogram(p, 5);
	}
	funcdefs++;
	--nfuncdefs;
    }
}

/*
 * NAME:	preload->vardefs()
 * DESCRIPTION:	handle variable definitions
 */
static void pl_vardefs(vardefs, nvardefs)
register dvardef *vardefs;
register unsigned short nvardefs;
{
    while (nvardefs > 0) {
	ctrl_dvar(getstring(UCHAR(vardefs->inherit), vardefs->index),
		  vardefs->class, vardefs->type);
	vardefs++;
	--nvardefs;
    }
}

/*
 * NAME:	preload->funcalls()
 * DESCRIPTION:	handle function calls
 */
static void pl_funcalls(funcalls, nfuncalls)
register char *funcalls;
register unsigned short nfuncalls;
{
    while (nfuncalls > 0) {
	ctrl_funcall(funcalls[0], funcalls[1]);
	funcalls += 2;
	--nfuncalls;
    }
}

/*
/*
 * NAME:	preload()
 * DESCRIPTION:	preload compiled objects
 */
void preload()
{
    register precomp **pc, *l;
    register unsigned short nfuncs;

    nfuncs = 0;
    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	nfuncs += (*pc)->nfunctions;
    }

    if (nfuncs > 0) {
	mstatic();
	pcfunctions = ALLOC(pcfunc, nfuncs);
	mdynamic();
	nfuncs = 0;
    }

    for (pc = precompiled; *pc != (precomp *) NULL; pc++) {
	l = *pc;
	message("Precompiled: \"/%s\"\n",
		l->inherits[l->ninherits - 1]);

	inherits = ALLOC(object*, l->ninherits);
	if (l->nstrings > 0) {
	    strings = ALLOC(string*, l->nstrings);
	}

	pl_inherits(l->inherits, l->ninherits);
	ctrl_create(l->inherits[l->ninherits - 1]);
	pl_strings(l->stext, l->slength, l->nstrings);
	pl_funcdefs(l->program, l->funcdefs, l->nfuncdefs, nfuncs);
	memcpy(pcfunctions + nfuncs, l->functions,
	       sizeof(pcfunc) * l->nfunctions);
	nfuncs += l->nfunctions;
	pl_vardefs(l->vardefs, l->nvardefs);
	pl_funcalls(l->funcalls, l->nfuncalls);

	if (l->nstrings > 0) {
	    FREE(strings);
	}
	FREE(inherits);

	ctrl_construct();
	ctrl_clear();
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
 * NAME:	check_int()
 * DESCRIPTION:	check if a value is an integer
 */
void check_int(v)
value *v;
{
    if (v->type != T_NUMBER) {
	error("Value is not a number");
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
    if (sp->type == T_NUMBER) {
	return (sp++)->u.number != 0;
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
void post_catch()
{
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

    if (v->type == T_NUMBER && v->u.number == 0) {
	return (tab[0] == 0);
    } else if (v->type != T_STRING) {
	i_del_value(v);
	return 0;
    }

    s = v->u.string;
    ctrl = o_control(i_this_object());
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
