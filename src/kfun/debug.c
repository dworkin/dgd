# ifndef FUNCDEF
# include "kfun.h"
# include "fcontrol.h"
# include "table.h"
# endif

# ifdef DUMP_FUNCS
# ifndef FUNCDEF
static void showclass(class)
register short class;
{
    if (class & C_TYPECHECKED) printf("typechecked ");
    if (class & C_UNDEFINED) printf("undefined ");
    if (class & C_VARARGS) printf("varargs ");
    if (class & C_PRIVATE) printf("private ");
    if (class & C_STATIC) printf("static ");
    if (class & C_LOCAL) printf("local ");
    if (class & C_NOMASK) printf("nomask ");
}

static void show_proto(func, proto)
char *func, *proto;
{
    int i;

    showclass(PROTO_CLASS(proto));
    printf("%s %s(", i_typename(PROTO_FTYPE(proto)), func);
    for (i = 0; i < PROTO_NARGS(proto) - 1; i++) {
	printf("%s, ", i_typename(PROTO_ARGS(proto)[i]));
    }
    if (i < PROTO_NARGS(proto)) {
	printf("%s", i_typename(PROTO_ARGS(proto)[i] & ~T_ELLIPSIS));
	if (PROTO_ARGS(proto)[i] & T_ELLIPSIS) {
	    printf("...");
	}
    }
    putchar(')');
}

static void showctrl(ctrl)
control *ctrl;
{
    register unsigned short i;

    printf("inherits:");
    for (i = 0; i < ctrl->ninherits; i++) {
	printf(" %s(%u %u)", o_name(ctrl->inherits[i].obj),
	       ctrl->inherits[i].funcoffset,
	       ctrl->inherits[i].varoffset);
    }
    putchar('\n');
    printf("progsize: %u\n", ctrl->progsize);
    if (ctrl->nstrings > 0) {
	printf("string constants:\n");
	for (i = 0; i < ctrl->nstrings; i++) {
	    printf(" %u: %s\n", i, d_get_strconst(ctrl, ctrl->ninherits - 1,
						  i)->text);
	}
    }
    if (ctrl->nfuncdefs > 0) {
	d_get_funcdefs(ctrl);
	printf("function definitions:\n");
	for (i = 0; i < ctrl->nfuncdefs; i++) {
	    printf(" %u: %04x ", i, ctrl->funcdefs[i].offset);
	    show_proto(d_get_strconst(ctrl, ctrl->funcdefs[i].inherit,
				      ctrl->funcdefs[i].index)->text,
		       d_get_prog(ctrl) + ctrl->funcdefs[i].offset);
	    putchar('\n');
	}
    }
    if (ctrl->nvardefs > 0) {
	d_get_vardefs(ctrl);
	printf("variable definitions:\n");
	for (i = 0; i < ctrl->nvardefs; i++) {
	    printf(" %u: ", i);
	    showclass(ctrl->vardefs[i].class);
	    printf("%s %s\n", i_typename(ctrl->vardefs[i].type),
		   d_get_strconst(ctrl, ctrl->vardefs[i].inherit,
				  ctrl->vardefs[i].index)->text);
	}
    }
    if (ctrl->nfuncalls > 0) {
	d_get_funcalls(ctrl);
	printf("funcalls:\n");
	for (i = 0; i < ctrl->nfuncalls; i++) {
	    control *c2;
	    dfuncdef *f;

	    c2 = ctrl->inherits[UCHAR(ctrl->funcalls[2 * i])].obj->ctrl;
	    f = d_get_funcdefs(c2) + UCHAR(ctrl->funcalls[2 * i + 1]);
	    printf(" %u: %s(%d, %d)\n", i,
		   d_get_strconst(c2, f->inherit, f->index)->text,
		   UCHAR(ctrl->funcalls[2 * i]),
		   UCHAR(ctrl->funcalls[2 * i + 1]));
	}
    }
    if (ctrl->nsymbols > 0) {
	d_get_symbols(ctrl);
	printf("symbols:\n");
	for (i = 0; i < ctrl->nsymbols; i++) {
	    control *c2;
	    dfuncdef *f;
	    char *name;

	    printf(" %u: (%u) ", i, ctrl->symbols[i].next);
	    c2 = ctrl->inherits[UCHAR(ctrl->symbols[i].inherit)].obj->ctrl;
	    f = d_get_funcdefs(c2) + UCHAR(ctrl->symbols[i].index);
	    name = d_get_strconst(c2, f->inherit, f->index)->text;
	    show_proto(name, d_get_prog(c2) + f->offset);
	    putchar('\n');
	}
    }
    printf("%u variables\n", ctrl->nvariables);
}

# define FETCH1S(pc)	SCHAR(*(pc)++)
# define FETCH1U(pc)	UCHAR(*(pc)++)
# define FETCH2S(pc, v)	((short) (v = *(pc)++ << 8, v |= UCHAR(*(pc)++)))
# define FETCH2U(pc, v)	((unsigned short) (v = *(pc)++ << 8, \
					   v |= UCHAR(*(pc)++)))
# define FETCH3S(pc, v)	((Int) (v = *(pc)++ << 8, \
				v |= UCHAR(*(pc)++), v <<= (Int) 8, \
				v |= UCHAR(*(pc)++)))
# define FETCH4S(pc, v)	((Int) (v = *(pc)++ << 8, \
				v |= UCHAR(*(pc)++), v <<= (Int) 8, \
				v |= UCHAR(*(pc)++), v <<= (Int) 8, \
				v |= UCHAR(*(pc)++)))
# define FETCH4U(pc, v)	((Uint) (v = *(pc)++ << 8, \
				 v |= UCHAR(*(pc)++), v <<= (Int) 8, \
				 v |= UCHAR(*(pc)++), v <<= (Int) 8, \
				 v |= UCHAR(*(pc)++)))

static unsigned short addr;
static unsigned short line;
static unsigned short newline;
static char *code;
static bool pop;
static int codesize;

static void show_instr(s)
char *s;
{
    register int size;

    printf("%04x\t", addr);
    if (line != newline) {
	line = newline;
	printf("%4u\t", line);
    } else {
	putchar('\t');
    }
    size = codesize;
    if (size > 4) {
	size = 4;
    }
    switch (size) {
    case 0:
	printf("\t    ");
	break;

    case 1:
	printf("%02x\t    ", UCHAR(code[0]));
	break;

    case 2:
	printf("%02x %02x\t    ", UCHAR(code[0]), UCHAR(code[1]));
	break;

    case 3:
	printf("%02x %02x %02x    ", UCHAR(code[0]), UCHAR(code[1]),
	       UCHAR(code[2]));
	break;

    case 4:
	printf("%02x %02x %02x %02x ", UCHAR(code[0]), UCHAR(code[1]),
	       UCHAR(code[2]), UCHAR(code[3]));
	break;
    }
    addr += size;
    code += size;
    codesize -= size;

    if (pop) {
	printf("P\t");
    } else {
	putchar('\t');
    }
    pop = FALSE;
    if (s != (char *) NULL) {
	printf("%s", s);
    }
    putchar('\n');
    fflush(stdout);
}

void disasm(ctrl, func)
control *ctrl;
int func;
{
    char *pc, *end, *linenumbers, buffer[100];
    control *cc;
    register unsigned short u, u2, u3;
    register unsigned long l;
    unsigned short a, progsize;
    int sz;
    xfloat flt;
    char fltbuf[18];

    pc = d_get_prog(ctrl) + d_get_funcdefs(ctrl)[func].offset;
    show_proto(d_get_strconst(ctrl, ctrl->funcdefs[func].inherit,
			      ctrl->funcdefs[func].index)->text, pc);
    u2 = PROTO_CLASS(pc) & C_COMPILED;
    pc += PROTO_SIZE(pc);
    printf("; depth %u,", FETCH2U(pc, u));
    printf(" %d local vars\n", FETCH1U(pc));
    if (u2) {
	return;
    }
    progsize = FETCH2U(pc, u);
    end = linenumbers = pc + progsize;
    addr = 5;
    line = 0;
    newline = 0;
    printf("addr\tline\tcode\t    pop instruction\n");
    while (pc < end) {
	code = pc;
	pop = *code & I_POP_BIT;
	if (((*code & I_LINE_MASK) >> I_LINE_SHIFT) <= 2) {
	    newline += (*code & I_LINE_MASK) >> I_LINE_SHIFT;
	} else {
	    u = FETCH1U(linenumbers);
	    if (u & 128) {
		newline += u - 128 - 64;
	    } else {
		u = (u << 8) | FETCH1U(linenumbers);
		newline += u - 16384;
	    }
	}
	switch (FETCH1U(pc) & I_INSTR_MASK) {
	case I_PUSH_ZERO:
	    codesize = 1;
	    show_instr("PUSH_ZERO");
	    break;

	case I_PUSH_ONE:
	    codesize = 1;
	    show_instr("PUSH_ONE");
	    break;

	case I_PUSH_INT1:
	    codesize = 2;
	    sprintf(buffer, "PUSH_INT1 %d", FETCH1S(pc));
	    show_instr(buffer);
	    break;

	case I_PUSH_INT4:
	    codesize = 5;
	    sprintf(buffer, "PUSH_INT4 %ld", FETCH4S(pc, l));
	    show_instr(buffer);
	    break;

	case I_PUSH_FLOAT2:
	    codesize = 3;
	    flt.high = FETCH2U(pc, u);
	    flt.low = 0L;
	    flt_ftoa(&flt, fltbuf);
	    sprintf(buffer, "PUSH_FLOAT2 %s", fltbuf);
	    show_instr(buffer);
	    break;

	case I_PUSH_FLOAT6:
	    codesize = 7;
	    flt.high = FETCH2U(pc, u);
	    flt.low = FETCH4U(pc, l);
	    flt_ftoa(&flt, fltbuf);
	    sprintf(buffer, "PUSH_FLOAT6 %s", fltbuf);
	    show_instr(buffer);
	    break;

	case I_PUSH_STRING:
	    codesize = 2;
	    u = FETCH1U(pc);
	    sprintf(buffer, "PUSH_STRING %d \"%s\"", u,
		    d_get_strconst(ctrl, ctrl->ninherits - 1, u)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_NEAR_STRING:
	    codesize = 3;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    sprintf(buffer, "PUSH_NEAR_STRING %d %d \"%s\"", u, u2,
		    d_get_strconst(ctrl, u, u2)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_FAR_STRING:
	    codesize = 4;
	    u = FETCH1U(pc);
	    FETCH2U(pc, u2);
	    sprintf(buffer, "PUSH_FAR_STRING %d %u \"%s\"", u, u2,
		    d_get_strconst(ctrl, u, u2)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_LOCAL:
	    codesize = 2;
	    sprintf(buffer, "PUSH_LOCAL %d", FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_PUSH_GLOBAL:
	    codesize = 3;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    cc = ctrl->inherits[u].obj->ctrl;
	    d_get_vardefs(cc);
	    sprintf(buffer, "PUSH_GLOBAL %d %d (%s)", u, u2,
		    d_get_strconst(cc, cc->vardefs[u2].inherit,
				   cc->vardefs[u2].index)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_LOCAL_LVALUE:
	    codesize = 2;
	    sprintf(buffer, "PUSH_LOCAL_LVALUE %d", FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_PUSH_GLOBAL_LVALUE:
	    codesize = 3;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    cc = ctrl->inherits[u].obj->ctrl;
	    d_get_vardefs(cc);
	    sprintf(buffer, "PUSH_GLOBAL_LVALUE %d %d (%s)", u, u2,
		    d_get_strconst(cc, cc->vardefs[u2].inherit,
				   cc->vardefs[u2].index)->text);
	    show_instr(buffer);
	    break;

	case I_INDEX:
	    codesize = 1;
	    show_instr("INDEX");
	    break;

	case I_INDEX_LVALUE:
	    codesize = 1;
	    show_instr("INDEX_LVALUE");
	    break;

	case I_AGGREGATE:
	    codesize = 3;
	    sprintf(buffer, "AGGREGATE %u", FETCH2U(pc, u));
	    show_instr(buffer);
	    break;

	case I_MAP_AGGREGATE:
	    codesize = 3;
	    sprintf(buffer, "MAP_AGGREGATE %u", FETCH2U(pc, u));
	    show_instr(buffer);
	    break;

	case I_SPREAD:
	    codesize = 2;
	    sprintf(buffer, "SPREAD %d", FETCH1S(pc));
	    show_instr(buffer);
	    break;

	case I_CAST:
	    codesize = 2;
	    sprintf(buffer, "CAST %s", i_typename(FETCH1U(pc)));
	    show_instr(buffer);
	    break;

	case I_FETCH:
	    codesize = 1;
	    show_instr("FETCH");
	    break;

	case I_STORE:
	    codesize = 1;
	    show_instr("STORE");
	    break;

	case I_JUMP:
	    codesize = 3;
	    sprintf(buffer, "JUMP %04x", addr + FETCH2S(pc, u) + 1);
	    show_instr(buffer);
	    break;

	case I_JUMP_ZERO:
	    codesize = 3;
	    sprintf(buffer, "JUMP_ZERO %04x", addr + FETCH2S(pc, u) + 1);
	    show_instr(buffer);
	    break;

	case I_JUMP_NONZERO:
	    codesize = 3;
	    sprintf(buffer, "JUMP_NONZERO %04x", addr + FETCH2S(pc, u) + 1);
	    show_instr(buffer);
	    break;

	case I_SWITCH:
	    switch (FETCH1U(pc)) {
	    case 0:
		codesize = 4;
		FETCH2U(pc, u);
		sz = FETCH1U(pc);
		sprintf(buffer, "SWITCH INT %u", u);
		show_instr(buffer);
		codesize = --u * (sz + 2) + 3;
		a = addr + 1;
		sprintf(buffer, " DEFAULT: %04x", a + FETCH2S(pc, u2));
		show_instr(buffer);
		a += 2;
		while (u > 0) {
		    long l;

		    switch (sz) {
		    case 4:
			FETCH4S(pc, l); break;
		    case 3:
			FETCH3S(pc, l); break;
		    case 2:
			FETCH2S(pc, l); break;
		    case 1:
			l = FETCH1S(pc); break;
		    }
		    sprintf(buffer, " CASE %ld: %04x", l, a + FETCH2S(pc, u2) + sz);
		    show_instr(buffer);
		    a += 2 + sz;
		    --u;
		}
		break;

	    case 1:
		codesize = 4;
		FETCH2U(pc, u);
		sz = FETCH1U(pc);
		sprintf(buffer, "SWITCH RANGE %u", u);
		show_instr(buffer);
		codesize = --u * (2 * sz + 2) + 3;
		a = addr + 1;
		sprintf(buffer, " DEFAULT: %04x", a + FETCH2S(pc, u2));
		show_instr(buffer);
		a += 2;
		while (u > 0) {
		    register long h;

		    switch (sz) {
		    case 4:
			FETCH4S(pc, l); break;
		    case 3:
			FETCH3S(pc, l); break;
		    case 2:
			FETCH2S(pc, l); break;
		    case 1:
			l = FETCH1S(pc); break;
		    }
		    switch (sz) {
		    case 4:
			FETCH4S(pc, h); break;
		    case 3:
			FETCH3S(pc, h); break;
		    case 2:
			FETCH2S(pc, h); break;
		    case 1:
			h = FETCH1S(pc); break;
		    }
		    sprintf(buffer, " CASE %ld .. %ld: %04x", l, h,
			    a + FETCH2U(pc, u2) + 2 * sz);
		    show_instr(buffer);
		    a += 2 + sz * 2;
		    --u;
		}
		break;

	    case 2:
		codesize = 4;
		FETCH2U(pc, u);
		sprintf(buffer, "SWITCH STR %u", u);
		show_instr(buffer);
		codesize = --u * 5 + 3;
		a = addr;
		sprintf(buffer, " DEFAULT: %04x", a + FETCH2U(pc, u2));
		show_instr(buffer);
		a += 3;
		if (FETCH1U(pc) == 0) {
		    codesize -= 3;
		    sprintf(buffer, " CASE 0: %04x", a + FETCH2S(pc, u2));
		    show_instr(buffer);
		    a += 2;
		    --u;
		}
		while (u > 0) {
		    string *str;
		    int i;

		    i = FETCH1U(pc);
		    str = d_get_strconst(ctrl, i, FETCH2S(pc, u2));
		    sprintf(buffer, " CASE \"%s\": %04x", str->text,
			    a + FETCH2U(pc, u2) + 3);
		    show_instr(buffer);
		    a += 5;
		    --u;
		}
		break;
	    }
	    break;

	case I_CALL_KFUNC:
	    u = FETCH1U(pc);
	    if (PROTO_CLASS(KFUN(u).proto) & C_VARARGS) {
		codesize = 3;
		u2 = FETCH1U(pc);
	    } else {
		codesize = 2;
		u2 = PROTO_NARGS(KFUN(u).proto);
	    }
	    sprintf(buffer, "CALL_KFUNC %d (%s%s) %d", u, KFUN(u).name,
		    (PROTO_CLASS(KFUN(u).proto) & C_TYPECHECKED) ? " tc" : "",
		    u2);
	    show_instr(buffer);
	    break;

	case I_CALL_AFUNC:
	    codesize = 3;
	    u = FETCH1U(pc);
	    cc = ctrl->inherits[0].obj->ctrl;
	    d_get_funcdefs(cc);
	    sprintf(buffer, "CALL_AFUNC %d (%s) %d", u,
		    d_get_strconst(cc, cc->funcdefs[u].inherit,
				   cc->funcdefs[u].index)->text,
		    FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_CALL_DFUNC:
	    codesize = 4;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    cc = ctrl->inherits[u].obj->ctrl;
	    d_get_funcdefs(cc);
	    sprintf(buffer, "CALL_DFUNC %d %d (%s) %d", u, u2,
		    d_get_strconst(cc, cc->funcdefs[u2].inherit,
				   cc->funcdefs[u2].index)->text,
		    FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_CALL_FUNC:
	    codesize = 4;
	    FETCH2U(pc, u);
	    u += ctrl->inherits[ctrl->ninherits - 1].funcoffset;
	    u2 = d_get_funcalls(ctrl)[2 * u];
	    u3 = ctrl->funcalls[2 * u + 1];
	    cc = ctrl->inherits[u2].obj->ctrl;
	    d_get_funcdefs(cc);
	    sprintf(buffer, "CALL_FUNC %u (%s) %d", u,
		    d_get_strconst(cc, cc->funcdefs[u3].inherit,
				   cc->funcdefs[u3].index)->text,
		    FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_CATCH:
	    codesize = 3;
	    sprintf(buffer, "CATCH %04x", addr + FETCH2S(pc, u) + 1);
	    show_instr(buffer);
	    break;

	case I_LOCK:
	    codesize = 1;
	    show_instr("LOCK");
	    break;

	case I_RETURN:
	    codesize = 1;
	    show_instr("RETURN");
	    break;

	default:
	    addr += 1;
	    codesize = 0;
	    printf("unknown instruction %d\n", *code & I_INSTR_MASK);
	    break;
	}
	while (codesize > 0) {
	    show_instr((char *) NULL, 0);
	}
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("dump_object", kf_dump_object, p_dump_object)
# else
char p_dump_object[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_VOID, 1,
			 T_OBJECT };

int kf_dump_object()
{
    showctrl(o_control(o_object(sp->oindex, sp->u.objcnt)));
    fflush(stdout);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("dump_function", kf_dump_function, p_dump_function)
# else
char p_dump_function[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_VOID, 2,
			   T_OBJECT, T_STRING };

int kf_dump_function()
{
    dsymbol *symb;

    symb = ctrl_symb(o_control(o_object(sp[1].oindex, sp[1].u.objcnt)),
		     sp->u.string->text);
    if (symb != (dsymbol *) NULL) {
	control *ctrl;

	ctrl = o_control(o_object(sp[1].oindex, sp[1].u.objcnt));
	disasm(o_control(ctrl->inherits[UCHAR(symb->inherit)].obj),
	       UCHAR(symb->index));
	fflush(stdout);
    }
    str_del((sp++)->u.string);
    return 0;
}
# endif
# endif /* DUMP_FUNCS */


/* the rusage code is borrowed from Amylaar's 3.2@ driver */
#ifdef RUSAGE
# ifdef FUNCDEF
FUNCDEF("rusage", kf_rusage, p_rusage)
# else

#include <sys/time.h>
#if defined(sun) && defined(__svr4__) /* solaris */
#include <sys/rusage.h>
#endif /* solaris */
#include <sys/resource.h>
extern int getrusage P((int, struct rusage *));
#ifndef RUSAGE_SELF
#define RUSAGE_SELF	0
#endif

char p_rusage[] = { C_STATIC | C_LOCAL, T_INT | (1 << REFSHIFT), 0 };

#if !defined(sun) || !defined(__svr4__)
#define RUSAGE_TIME(t) (t).tv_sec * 1000 + (t).tv_usec / 1000;
#else
#define RUSAGE_TIME(t) (t).tv_sec * 1000 + (t).tv_nsec / 1000000;
#endif

int kf_rusage()
{
    struct rusage rus;
    value val;

    if (getrusage(RUSAGE_SELF, &rus) < 0) {
	val.type = T_INT;
	val.u.number = 0;
    } else {
	array *a;

	a = arr_new(2L);
	a->elts[0].type = T_INT;
	a->elts[0].u.number = RUSAGE_TIME(rus.ru_utime);
	a->elts[1].type = T_INT;
	a->elts[1].u.number = RUSAGE_TIME(rus.ru_stime);
	val.type = T_ARRAY;
	val.u.array = a;
    }
    i_push_value(&val);
    return 0;
}
# endif
#endif  /* RUSAGE */
