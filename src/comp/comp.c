# include "comp.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "kfun.h"
# include "node.h"
# include "fcontrol.h"
# include "compile.h"

showclass(class)
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

void show_proto(func, proto)
char *func, *proto;
{
    int i;

    showclass(PROTO_CLASS(proto));
    printf("%s %s(", c_typename(PROTO_FTYPE(proto)), func);
    for (i = 0; i < PROTO_NARGS(proto) - 1; i++) {
	printf("%s, ", c_typename(PROTO_ARGS(proto)[i]));
    }
    if (i < PROTO_NARGS(proto)) {
	printf("%s", c_typename(PROTO_ARGS(proto)[i]));
    }
    putchar(')');
}

showctrl(ctrl)
control *ctrl;
{
    register unsigned short i;

    printf("virtual inherits:");
    for (i = 0; i < UCHAR(ctrl->nvirtuals); i++) {
	printf(" %s(%u %u)", o_name(ctrl->inherits[i].obj),
	       ctrl->inherits[i].funcoffset,
	       ctrl->inherits[i].varoffset);
    }
    putchar('\n');
    if (i < UCHAR(ctrl->ninherits)) {
	printf("labeled inherits:");
	for ( ; i < UCHAR(ctrl->ninherits); i++) {
	    printf(" %s(%u %u)", o_name(ctrl->inherits[i].obj),
		   ctrl->inherits[i].funcoffset,
		   ctrl->inherits[i].varoffset);
	}
	putchar('\n');
    }
    printf("progsize: %u\n", ctrl->progsize);
    if (ctrl->nstrings > 0) {
	printf("string constants:\n");
	for (i = 0; i < ctrl->nstrings; i++) {
	    printf(" %u: %s\n", i, ctrl->strings[i]->text);
	}
    }
    if (ctrl->nfuncdefs > 0) {
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
	printf("variable definitions:\n");
	for (i = 0; i < ctrl->nvardefs; i++) {
	    printf(" %u: ", i);
	    showclass(ctrl->vardefs[i].class);
	    printf("%s %s\n", c_typename(ctrl->vardefs[i].type),
		   d_get_strconst(ctrl, ctrl->vardefs[i].inherit,
				  ctrl->vardefs[i].index)->text);
	}
    }
    if (ctrl->nfuncalls > 0) {
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
	printf("symbols:\n");
	for (i = 0; i < ctrl->nsymbols; i++) {
	    control *c2;
	    dfuncdef *f;
	    char *name;

	    c2 = ctrl->inherits[UCHAR(ctrl->symbols[i].inherit)].obj->ctrl;
	    f = d_get_funcdefs(c2) + UCHAR(ctrl->symbols[i].index);
	    name = d_get_strconst(c2, f->inherit, f->index)->text;
	    printf(" %u: %s(%u)", i, name, ctrl->symbols[i].next);
	    if (!(f->class & C_UNDEFINED) &&
		ctrl_symb(ctrl, name) != &ctrl->symbols[i]) {
		printf(" LOOKUP FAILED!");
	    }
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
# define FETCH4S(pc, v)	((Int) (v = *(pc)++ << 8, \
				v |= UCHAR(*(pc)++), v <<= (Int) 8, \
				v |= UCHAR(*(pc)++), v <<= (Int) 8, \
				v |= UCHAR(*(pc)++)))

unsigned short addr;
unsigned short firstline;
unsigned short line;
unsigned short thisline;
char *code;
bool pop;
int codesize;

void show_instr(s)
char *s;
{
    register int size;

    printf("%04x\t", addr);
    if (line != thisline) {
	line = thisline;
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
    char *pc, *end, buffer[100];
    control *cc;
    register unsigned short u, u2, u3;
    register long l;
    unsigned short a;

    pc = end = d_get_prog(ctrl) + ctrl->funcdefs[func].offset;
    show_proto(d_get_strconst(ctrl, ctrl->funcdefs[func].inherit,
			      ctrl->funcdefs[func].index)->text, pc);
    if (func == ctrl->nfuncdefs - 1) {
	end += ctrl->progsize - ctrl->funcdefs[func].offset;
    } else {
	end += ctrl->funcdefs[func + 1].offset - ctrl->funcdefs[func].offset;
    }
    pc += PROTO_SIZE(pc);
    printf("; %d local vars\n", FETCH1U(pc));
    addr = 3;
    line = 0;
    firstline = thisline = FETCH2U(pc, u);
    printf("addr\tline\tcode\t    pop instruction\n");
    while (pc < end) {
	code = pc;
	pop = *code & I_POP_BIT;
	thisline += ((*code & I_LINE_MASK) >> I_LINE_SHIFT) - 1;
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

	case I_PUSH_INT2:
	    codesize = 3;
	    sprintf(buffer, "PUSH_INT2 %d", FETCH2S(pc, u));
	    show_instr(buffer);
	    break;

	case I_PUSH_INT4:
	    codesize = 5;
	    sprintf(buffer, "PUSH_INT4 %ld", FETCH4S(pc, l));
	    show_instr(buffer);
	    break;

	case I_PUSH_STRING:
	    codesize = 2;
	    u = FETCH1U(pc);
	    sprintf(buffer, "PUSH_STRING %02x \"%s\"", u,
		    d_get_strconst(ctrl, ctrl->nvirtuals - 1, u)->text);
	    show_instr(buffer);
	    break;

	case I_PUSH_FAR_STRING:
	    codesize = 4;
	    u = FETCH1U(pc);
	    FETCH2U(pc, u2);
	    sprintf(buffer, "PUSH_FAR_STRING %02x %04x \"%s\"", u, u2,
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
	    sprintf(buffer, "PUSH_GLOBAL %02x %02x (%s)", u, u2,
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
	    sprintf(buffer, "PUSH_GLOBAL_LVALUE %02x %02x (%s)", u, u2,
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

	case I_SWITCH_INT:
	    codesize = 4;
	    FETCH2U(pc, u);
	    sprintf(buffer, "SWITCH_INT %u", u);
	    show_instr(buffer);
	    codesize = --u * 6 + 1;
	    a = addr - 1;
	    sprintf(buffer, " DEFAULT: %04x", a + FETCH2S(pc, u2));
	    show_instr(buffer);
	    a += 2;
	    while (u > 0) {
		long l;

		FETCH4S(pc, l);
		sprintf(buffer, " CASE %ld: %04x", l, a + FETCH2S(pc, u2) + 4);
		show_instr(buffer);
		a += 6;
		--u;
	    }
	    break;

	case I_SWITCH_RANGE:
	    codesize = 4;
	    FETCH2U(pc, u);
	    sprintf(buffer, "SWITCH_RANGE %u", u);
	    show_instr(buffer);
	    codesize = --u * 10 + 1;
	    a = addr - 1;
	    sprintf(buffer, " DEFAULT: %04x", a + FETCH2S(pc, u2));
	    show_instr(buffer);
	    a += 2;
	    while (u > 0) {
		register long h;

		FETCH4S(pc, l);
		FETCH4S(pc, h);
		sprintf(buffer, " CASE %ld .. %ld: %04x", l, h,
			a + FETCH2U(pc, u2) + 8);
		show_instr(buffer);
		a += 10;
		--u;
	    }
	    break;

	case I_SWITCH_STR:
	    codesize = 4;
	    FETCH2U(pc, u);
	    sprintf(buffer, "SWITCH_STR %u", u);
	    show_instr(buffer);
	    codesize = --u * 5 + 2;
	    a = addr - 1;
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

	case I_CALL_KFUNC:
	    u = FETCH1U(pc);
	    if (PROTO_CLASS(kftab[u].proto) & C_VARARGS) {
		codesize = 3;
		sprintf(buffer, "CALL_KFUNC %02x (%s) %d", u, kftab[u].name,
			FETCH1U(pc));
	    } else {
		codesize = 2;
		sprintf(buffer, "CALL_KFUNC %02x (%s) %d", u, kftab[u].name,
			PROTO_NARGS(kftab[u].proto));
	    }
	    show_instr(buffer);
	    break;

	case I_CALL_LFUNC:
	    codesize = 5;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    u3 = FETCH1U(pc);
	    cc = ctrl->inherits[u].obj->ctrl->inherits[u2].obj->ctrl;
	    sprintf(buffer, "CALL_LFUNC %02x %02x %02x (%s) %d", u, u2, u3,
		    d_get_strconst(cc, cc->funcdefs[u3].inherit,
				   cc->funcdefs[u3].index)->text,
		    FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_CALL_DFUNC:
	    codesize = 4;
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    cc = ctrl->inherits[u].obj->ctrl;
	    sprintf(buffer, "CALL_DFUNC %02x %02x (%s) %d", u, u2,
		    d_get_strconst(cc, cc->funcdefs[u2].inherit,
				   cc->funcdefs[u2].index)->text,
		    FETCH1U(pc));
	    show_instr(buffer);
	    break;

	case I_CALL_FUNC:
	    codesize = 4;
	    FETCH2U(pc, u);
	    u2 = ctrl->funcalls[2 * u];
	    u3 = ctrl->funcalls[2 * u + 1];
	    cc = ctrl->inherits[u2].obj->ctrl;
	    sprintf(buffer, "CALL_FUNC %04x (%s) %d", u,
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

	case I_LINE:
	    codesize = 2;
	    u = FETCH1U(pc);
	    thisline = firstline + u;
	    sprintf(buffer, "LINE %u", thisline);
	    show_instr(buffer);
	    break;

	case I_LINE2:
	    codesize = 3;
	    FETCH2U(pc, thisline);
	    sprintf(buffer, "LINE %u", thisline);
	    show_instr(buffer);
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

main(argc, argv)
int argc;
char *argv[];
{
    typedef struct _list_ {
	control *ctrl;
	struct _list_ *next;
    } list;
    list *l, *c;
    int n, i, j;
    static char *paths[] = { "/usr/include", 0 };

    str_init();
    arr_init();
    o_init(100);
    kf_init();
    c_init("auto", "driver", "std.h", paths, FALSE);

    n = 0;
    l = (list *) NULL;
    for (;;) {
	char buffer[100];

	printf("\n[%d compiled] ", n);
	gets(buffer);
	switch (buffer[0]) {
	case 'c':
	    if (ec_push()) {
		warning((char *) NULL);
		return 1;
	    }
	    c = ALLOC(list, 1);
	    c->ctrl = c_compile(buffer + 2)->ctrl;
	    ec_pop();
	    c->next = l;
	    l = c;
	    n++;
	    break;

	case 'v':
	    i = atoi(buffer + 1);
	    if (i <= 0 || i > n) {
		putchar('\007');
		break;
	    }
	    c = l;
	    for (i = n - i; i > 0; --i) {
		c = c->next;
	    }
	    if (c->ctrl == (control *) NULL) {
		printf("<destructed>\n");
	    } else {
		showctrl(c->ctrl);
	    }
	    break;

	case 'p':
	    sscanf(buffer + 1, "%d,%d", &i, &j);
	    if (i <= 0 || i > n) {
		putchar('\007');
		break;
	    }
	    c = l;
	    for (i = n - i; i > 0; --i) {
		c = c->next;
	    }
	    if (c->ctrl == (control *) NULL) {
		printf("<destructed>\n");
	    } else {
		if (j <= 0 || j > c->ctrl->nfuncdefs) {
		    putchar('\007');
		    break;
		}
		disasm(c->ctrl, j - 1);
	    }
	    break;

	case 'd':
	    i = atoi(buffer + 1);
	    if (i <= 0 || i > n) {
		putchar('\007');
		break;
	    }
	    c = l;
	    for (i = n - i; i > 0; --i) {
		c = c->next;
	    }
	    o_del(c->ctrl->inherits[c->ctrl->nvirtuals - 1].obj);
	    o_clean();
	    c->ctrl = (control *) NULL;
	    break;

	case 'q':
	    return 0;

	default:
	    putchar('\007');
	    break;
	}
    }
}

bool i_apply() {}
void i_del_value() {}
void i_pop() {}
value *sp;
object *i_this_object() {}
object *i_prev_object() {}
void i_push_value() {}
