/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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

# ifndef FUNCDEF
# include "kfun.h"
# include "control.h"
# include "table.h"
# endif

# ifdef DUMP_FUNCS
# ifndef FUNCDEF
static void showclass(class)
register short class;
{
    if (class & C_COMPILED) printf("compiled ");
    if (class & C_TYPECHECKED) printf("typechecked ");
    if (class & C_UNDEFINED) printf("undefined ");
    if (class & C_ATOMIC) printf("atomic ");
    if (class & C_PRIVATE) printf("private ");
    if (class & C_STATIC) printf("static ");
    if (class & C_NOMASK) printf("nomask ");
}

static char *typename(ctrl, buffer, proto)
control *ctrl;
char *buffer;
register char *proto;
{
    char tnbuf[17], *p;
    Uint class;

    if ((*proto & T_TYPE) == T_CLASS) {
	p = strchr(i_typename(tnbuf, FETCH1U(proto)), ' ');
	FETCH3U(proto, class);
	sprintf(buffer, "object /%s", d_get_strconst(ctrl, class >> 16,
						     class & 0xffff)->text);
	if (p != (char *) NULL) {
	    strcat(buffer, p);
	}
    } else {
	i_typename(buffer, FETCH1U(proto));
    }
    return proto;
}

static void show_proto(ctrl, func, proto)
control *ctrl;
char *func, *proto;
{
    char buffer[STRINGSZ * 2];
    int c, i, n, v;

    showclass(c = PROTO_CLASS(proto));
    v = PROTO_NARGS(proto);
    n = PROTO_VARGS(proto) + v;
    proto = typename(ctrl, buffer, &PROTO_FTYPE(proto));
    printf("%s %s(", buffer, func);
    for (i = 0; i < n - 1; i++) {
	if (i == v) {
	    printf("varargs ");
	}
	proto = typename(ctrl, buffer, proto);
	printf("%s, ", buffer);
    }
    if (i < n) {
	if (i == v && !(c & C_ELLIPSIS)) {
	    printf("varargs ");
	}
	proto = typename(ctrl, buffer, proto);
	printf("%s", buffer);
	if (c & C_ELLIPSIS) {
	    printf("...");
	}
    }
    putchar(')');
}

static void showctrl(ctrl)
control *ctrl;
{
    char tnbuf[17];
    register unsigned short i;

    printf("inherits:\n");
    for (i = 0; i < ctrl->ninherits; i++) {
	printf("  /%s\n", OBJR(ctrl->inherits[i].oindex)->chain.name);
    }
    printf("progsize: %lu\n", (unsigned long) ctrl->progsize);
    if (ctrl->nstrings > 0) {
	printf("string constants:\n");
	for (i = 0; i < ctrl->nstrings; i++) {
	    printf("%3u: \"%s\"\n", i,
		   d_get_strconst(ctrl, ctrl->ninherits - 1, i)->text);
	}
    }
    if (ctrl->nfuncdefs > 0) {
	d_get_funcdefs(ctrl);
	printf("function definitions:\n");
	for (i = 0; i < ctrl->nfuncdefs; i++) {
	    printf("%3u: %08lx ", i, (unsigned long) ctrl->funcdefs[i].offset);
	    show_proto(ctrl, d_get_strconst(ctrl, ctrl->funcdefs[i].inherit,
					    ctrl->funcdefs[i].index)->text,
		       d_get_prog(ctrl) + ctrl->funcdefs[i].offset);
	    putchar('\n');
	}
    }
    if (ctrl->nvardefs > 0) {
	d_get_vardefs(ctrl);
	printf("variable definitions:\n");
	for (i = 0; i < ctrl->nvardefs; i++) {
	    printf("%3u: ", i);
	    showclass(ctrl->vardefs[i].class);
	    if ((ctrl->vardefs[i].type & T_TYPE) == T_CLASS) {
		char *p;

		p = strchr(i_typename(tnbuf, ctrl->vardefs[i].type), ' ');
		printf("object /%s", ctrl->cvstrings[i]->text);
		if (p != (char *) NULL) {
		    printf("%s", p);
		}
		printf(" %s\n", d_get_strconst(ctrl, ctrl->vardefs[i].inherit,
					       ctrl->vardefs[i].index)->text);
	    } else {
		printf("%s %s\n", i_typename(tnbuf, ctrl->vardefs[i].type),
		       d_get_strconst(ctrl, ctrl->vardefs[i].inherit,
				      ctrl->vardefs[i].index)->text);
	    }
	}
    }
    printf("%u variables\n", ctrl->nvariables);
}

static unsigned short addr;
static unsigned short line;
static unsigned short newline;
static char *code, *pc;
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
	printf(" P\t");
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
    char *pc, *end, *linenumbers, tnbuf[STRINGSZ * 2], buffer[1000];
    control *cc;
    register unsigned short u, u2, u3;
    register unsigned long l;
    unsigned short a, progsize;
    int sz;
    xfloat flt;
    char fltbuf[18];

    pc = d_get_prog(ctrl) + d_get_funcdefs(ctrl)[func].offset;
    show_proto(ctrl, d_get_strconst(ctrl, ctrl->funcdefs[func].inherit,
				    ctrl->funcdefs[func].index)->text, pc);
    u2 = PROTO_CLASS(pc);
    if (u2 & C_UNDEFINED) {
	putchar('\n');
	return;
    }
    pc += PROTO_SIZE(pc);
    printf("; depth %u,", FETCH2U(pc, u));
    printf(" %u local vars\n", FETCH1U(pc));
    if (u2 & C_COMPILED) {
	return;
    }

    progsize = FETCH2U(pc, u);
    end = linenumbers = pc + progsize;
    addr = 0;
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

	case I_PUSH_FLOAT:
	    codesize = 7;
	    flt.high = FETCH2U(pc, u);
	    flt.low = FETCH4U(pc, l);
	    flt_ftoa(&flt, fltbuf);
	    sprintf(buffer, "PUSH_FLOAT %s", fltbuf);
	    show_instr(buffer);
	    break;

	case I_PUSH_STRING:
	    codesize = 2;
	    u = FETCH1U(pc);
	    sprintf(buffer, "PUSH_STRING \"%s\"",
		    d_get_strconst(ctrl, ctrl->ninherits - 1, u)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_NEAR_STRING:
	    codesize = 3;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    sprintf(buffer, "PUSH_NEAR_STRING \"%s\"",
		    d_get_strconst(ctrl, u, u2)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_FAR_STRING:
	    codesize = 4;
	    u = FETCH1U(pc);
	    FETCH2U(pc, u2);
	    sprintf(buffer, "PUSH_FAR_STRING \"%s\"",
		    d_get_strconst(ctrl, u, u2)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_LOCAL:
	    codesize = 2;
	    sprintf(buffer, "PUSH_LOCAL %d", FETCH1S(pc));
	    show_instr(buffer);
	    break;

	case I_PUSH_GLOBAL:
	    codesize = 2;
	    u = FETCH1U(pc);
	    d_get_vardefs(ctrl);
	    sprintf(buffer, "PUSH_GLOBAL %s",
		    d_get_strconst(ctrl, ctrl->vardefs[u].inherit,
				   ctrl->vardefs[u].index)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_FAR_GLOBAL:
	    codesize = 3;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    cc = OBJR(ctrl->inherits[u].oindex)->ctrl;
	    d_get_vardefs(cc);
	    sprintf(buffer, "PUSH_FAR_GLOBAL %s",
		    d_get_strconst(cc, cc->vardefs[u2].inherit,
				   cc->vardefs[u2].index)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_LOCAL_LVAL:
	    if (pop) {
		pop = 0;
		u = FETCH1S(pc);
		pc = typename(ctrl, tnbuf, pc);
		sprintf(buffer, "PUSH_LOCAL_LVALUE %d (%s)", (short) u, tnbuf);
	    } else {
		sprintf(buffer, "PUSH_LOCAL_LVALUE %d", FETCH1S(pc));
	    }
	    codesize = pc - code;
	    show_instr(buffer);
	    break;

	case I_PUSH_GLOBAL_LVAL:
	    if (pop) {
		pop = 0;
		u = FETCH1U(pc);
		d_get_vardefs(ctrl);
		pc = typename(ctrl, tnbuf, pc);
		sprintf(buffer, "PUSH_GLOBAL_LVALUE %s (%s)",
			d_get_strconst(ctrl, ctrl->vardefs[u].inherit,
				       ctrl->vardefs[u].index)->text,
			tnbuf);
	    } else {
		u = FETCH1U(pc);
		d_get_vardefs(ctrl);
		sprintf(buffer, "PUSH_GLOBAL_LVALUE %s",
			d_get_strconst(ctrl, ctrl->vardefs[u].inherit,
				       ctrl->vardefs[u].index)->text);
	    }
	    codesize = pc - code;
	    show_instr(buffer);
	    break;

	case I_PUSH_FAR_GLOBAL_LVAL:
	    if (pop) {
		pop = 0;
		u = FETCH1U(pc);
		u2 = FETCH1U(pc);
		cc = OBJR(ctrl->inherits[u].oindex)->ctrl;
		d_get_vardefs(cc);
		pc = typename(ctrl, tnbuf, pc);
		sprintf(buffer, "PUSH_FAR_GLOBAL_LVALUE %s (%s)",
			d_get_strconst(cc, cc->vardefs[u2].inherit,
				       cc->vardefs[u2].index)->text,
			tnbuf);
	    } else {
		u = FETCH1U(pc);
		u2 = FETCH1U(pc);
		cc = OBJR(ctrl->inherits[u].oindex)->ctrl;
		d_get_vardefs(cc);
		sprintf(buffer, "PUSH_FAR_GLOBAL_LVALUE %s",
			d_get_strconst(cc, cc->vardefs[u2].inherit,
				       cc->vardefs[u2].index)->text);
	    }
	    codesize = pc - code;
	    show_instr(buffer);
	    break;

	case I_INDEX:
	    codesize = 1;
	    show_instr("INDEX");
	    break;

	case I_INDEX_LVAL:
	    if (pop) {
		pop = 0;
		pc = typename(ctrl, tnbuf, pc);
		sprintf(buffer, "INDEX_LVALUE (%s)", tnbuf);
		codesize = pc - code;
		show_instr(buffer);
	    } else {
		codesize = 1;
		show_instr("INDEX_LVALUE");
	    }
	    break;

	case I_AGGREGATE:
	    codesize = 4;
	    u = FETCH1U(pc);
	    sprintf(buffer, "AGGREGATE %s %u", (u) ? "mapping" : "array",
		    FETCH2U(pc, u2));
	    show_instr(buffer);
	    break;

	case I_SPREAD:
	    if (pop) {
		pop = 0;
		u = FETCH1S(pc);
		pc = typename(ctrl, tnbuf, pc);
		sprintf(buffer, "SPREAD %u (%s)", u, tnbuf);
	    } else {
		sprintf(buffer, "SPREAD %u", FETCH1S(pc));
	    }
	    codesize = pc - code;
	    show_instr(buffer);
	    break;

	case I_CAST:
	    pc = typename(ctrl, tnbuf, pc);
	    sprintf(buffer, "CAST %s", tnbuf);
	    codesize = pc - code;
	    show_instr(buffer);
	    break;

	case I_DUP:
	    codesize = 1;
	    show_instr("DUP");
	    break;

	case I_STORE:
	    codesize = 1;
	    show_instr("STORE");
	    break;

	case I_JUMP:
	    codesize = 3;
	    sprintf(buffer, "JUMP %04x", FETCH2U(pc, u));
	    show_instr(buffer);
	    break;

	case I_JUMP_ZERO:
	    codesize = 3;
	    sprintf(buffer, "JUMP_ZERO %04x", FETCH2U(pc, u));
	    show_instr(buffer);
	    break;

	case I_JUMP_NONZERO:
	    codesize = 3;
	    sprintf(buffer, "JUMP_NONZERO %04x", FETCH2U(pc, u));
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
		sprintf(buffer, " DEFAULT: %04x", FETCH2U(pc, u2));
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
		    sprintf(buffer, " CASE %ld: %04x", l, FETCH2U(pc, u2));
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
		sprintf(buffer, " DEFAULT: %04x", FETCH2U(pc, u2));
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
			    FETCH2U(pc, u2));
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
		sprintf(buffer, " DEFAULT: %04x", FETCH2U(pc, u2));
		show_instr(buffer);
		a += 3;
		if (FETCH1U(pc) == 0) {
		    codesize -= 3;
		    sprintf(buffer, " CASE 0: %04x", FETCH2U(pc, u2));
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
			    FETCH2U(pc, u2));
		    show_instr(buffer);
		    a += 5;
		    --u;
		}
		break;
	    }
	    break;

	case I_CALL_KFUNC:
	    u = FETCH1U(pc);
	    if (PROTO_VARGS(KFUN(u).proto) != 0) {
		codesize = 3;
		u2 = FETCH1U(pc);
	    } else {
		codesize = 2;
		u2 = PROTO_NARGS(KFUN(u).proto);
	    }
	    sprintf(buffer, "CALL_KFUNC %s%s %u", KFUN(u).name,
		    (PROTO_CLASS(KFUN(u).proto) & C_TYPECHECKED) ? " (tc)" : "",
		    u2);
	    show_instr(buffer);
	    break;

	case I_CALL_AFUNC:
	    codesize = 3;
	    u = FETCH1U(pc);
	    cc = OBJR(ctrl->inherits[0].oindex)->ctrl;
	    d_get_funcdefs(cc);
	    sprintf(buffer, "CALL_AFUNC %s %u",
		    d_get_strconst(cc, cc->funcdefs[u].inherit,
				   cc->funcdefs[u].index)->text,
		    FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_CALL_DFUNC:
	    codesize = 4;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    cc = OBJR(ctrl->inherits[u].oindex)->ctrl;
	    d_get_funcdefs(cc);
	    sprintf(buffer, "CALL_DFUNC %s %u",
		    d_get_strconst(cc, cc->funcdefs[u2].inherit,
				   cc->funcdefs[u2].index)->text,
		    FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_CALL_FUNC:
	    codesize = 4;
	    FETCH2U(pc, u);
	    u += ctrl->inherits[ctrl->ninherits - 1].funcoffset;
	    u2 = UCHAR(d_get_funcalls(ctrl)[2 * u]);
	    u3 = UCHAR(ctrl->funcalls[2 * u + 1]);
	    cc = OBJR(ctrl->inherits[u2].oindex)->ctrl;
	    d_get_funcdefs(cc);
	    sprintf(buffer, "CALL_FUNC [%u, %u] %s %u", u2, u3,
		    d_get_strconst(cc, cc->funcdefs[u3].inherit,
				   cc->funcdefs[u3].index)->text,
		    FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_CATCH:
	    codesize = 3;
	    sprintf(buffer, "CATCH %04x", FETCH2U(pc, u));
	    show_instr(buffer);
	    break;

	case I_RLIMITS:
	    codesize = 2;
	    sprintf(buffer, "RLIMITS%s", (!FETCH1U(pc)) ? " (checked)" : "");
	    show_instr(buffer);
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
	    show_instr((char *) NULL);
	}
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("dump_object", kf_dump_object, pt_dump_object, 0)
# else
char pt_dump_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID,
			  T_OBJECT };

int kf_dump_object(f)
frame *f;
{
    uindex n;

    if (f->sp->type == T_OBJECT) {
	n = f->sp->oindex;
    } else {
	n = f->sp->u.array->elts[0].oindex;
	arr_del(f->sp->u.array);
    }
    showctrl(o_control(OBJR(n)));
    fflush(stdout);
    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("dump_function", kf_dump_function, pt_dump_function, 0)
# else
char pt_dump_function[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_VOID,
			    T_OBJECT, T_STRING };

int kf_dump_function(f)
frame *f;
{
    uindex n;
    control *ctrl;
    dsymbol *symb;

    if (f->sp[1].type == T_OBJECT) {
	n = f->sp[1].oindex;
    } else {
	n = f->sp[1].u.array->elts[0].oindex;
	arr_del(f->sp[1].u.array);
    }
    ctrl = o_control(OBJR(n));
    symb = ctrl_symb(ctrl, f->sp->u.string->text, f->sp->u.string->len);
    if (symb != (dsymbol *) NULL) {
	disasm(o_control(OBJR(ctrl->inherits[UCHAR(symb->inherit)].oindex)),
	       UCHAR(symb->index));
	fflush(stdout);
    }
    str_del((f->sp++)->u.string);
    *f->sp = nil_value;
    return 0;
}
# endif
# endif /* DUMP_FUNCS */
