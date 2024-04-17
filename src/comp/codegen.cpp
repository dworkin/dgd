/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "control.h"
# include "data.h"
# include "interpret.h"
# include "ext.h"
# include "table.h"
# include "node.h"
# include "codegen.h"
# include "compile.h"

# define LINE_CHUNK	128

class LineChunk : public Allocated {
public:
    static void byte(int byte);
    static int fix(unsigned short newline);
    static void make(char *buf);
    static void clear();

private:
    LineChunk *next;			/* next in list */
    char info[LINE_CHUNK];		/* chunk of line number info */
};

static LineChunk *lline, *tline;		/* line chunk list */
static LineChunk *fline;			/* free line chunk list */
static unsigned int lchunksz = LINE_CHUNK;	/* line chunk size */
static unsigned short line;			/* current line number */
static unsigned int line_info_size;		/* size of all line info */

/*
 * output a line description byte
 */
void LineChunk::byte(int byte)
{
    if (lchunksz == LINE_CHUNK) {
	LineChunk *l;

	/* new chunk */
	if (fline != (LineChunk *) NULL) {
	    /* from free list */
	    l = fline;
	    fline = l->next;
	} else {
	    l = new LineChunk;
	}
	l->next = (LineChunk *) NULL;
	if (tline != (LineChunk *) NULL) {
	    tline->next = l;
	} else {
	    lline = l;
	}
	tline = l;
	lchunksz = 0;
    }

    tline->info[lchunksz++] = byte;
    line_info_size++;
}

/*
 * Fix the new line number.  Return 0 .. 2 for simple offsets,
 * 3 for special ones.
 */
int LineChunk::fix(unsigned short newline)
{
    short offset;

    if (newline == 0) {
	/* nothing changes */
	return 0;
    }
    offset = newline - line;
    if (line != 0 && offset >= 0 && offset <= 2) {
	/* simple offset */
	line = newline;
	return offset;
    }

    if (offset >= -64 && offset <= 63) {
	/* one byte offset */
	byte(offset + 128 + 64);
    } else {
	/* two byte offset */
	offset += 16384;
	byte((offset >> 8) & 127);
	byte(offset);
    }
    line = newline;
    return 3;
}

/*
 * put line number info after the function
 */
void LineChunk::make(char *buf)
{
    LineChunk *l;

    /* collect all line blocks in one large block */
    for (l = lline; l != tline; l = l->next) {
	memcpy(buf, l->info, LINE_CHUNK);
	buf += LINE_CHUNK;
    }
    memcpy(buf, l->info, lchunksz);

    /* add blocks to free list */
    tline->next = fline;
    fline = lline;
    /* clear blocks */
    lline = (LineChunk *) NULL;
    tline = (LineChunk *) NULL;
    lchunksz = LINE_CHUNK;
    line = 0;
    line_info_size = 0;
}

/*
 * clean up line number info chunks
 */
void LineChunk::clear()
{
    LineChunk *l, *f;

    for (l = fline; l != (LineChunk *) NULL; ) {
	f = l;
	l = l->next;
	delete f;
    }
    fline = (LineChunk *) NULL;
}


# define CODE_CHUNK	128

class CodeChunk : public Allocated {
public:
    static void byte(char byte);
    static void word(unsigned short word);
    static void instr(int i, unsigned short line);
    static void kfun(int kf, unsigned short line);
    static void ckfun(int kf, unsigned short line);
    static char *make(unsigned short depth, int nlocals, unsigned short *size);
    static void clear();

private:
    CodeChunk *next;			/* next in list */
    char code[CODE_CHUNK];		/* chunk of code */
};

static CodeChunk *lcode, *tcode;		/* code chunk list */
static CodeChunk *fcode;			/* free code chunk list */
static unsigned int cchunksz = CODE_CHUNK;	/* code chunk size */
static Uint here;				/* current offset */
static unsigned short caught, max;		/* number of catches */
static char *last_instruction;			/* last instruction's address */

/*
 * output a byte of code
 */
void CodeChunk::byte(char byte)
{
    if (cchunksz == CODE_CHUNK) {
	CodeChunk *l;

	/* new chunk */
	if (fcode != (CodeChunk *) NULL) {
	    /* from free list */
	    l = fcode;
	    fcode = l->next;
	} else {
	    l = new CodeChunk;
	}
	l->next = (CodeChunk *) NULL;
	if (tcode != (CodeChunk *) NULL) {
	    tcode->next = l;
	} else {
	    lcode = l;
	}
	tcode = l;
	cchunksz = 0;
    }
    tcode->code[cchunksz++] = byte;
    here++;
}

/*
 * output a word of code
 */
void CodeChunk::word(unsigned short word)
{
    byte(word >> 8);
    byte(word);
}

/*
 * generate an instruction code
 */
void CodeChunk::instr(int i, unsigned short line)
{
    byte(i | (LineChunk::fix(line) << I_LINE_SHIFT));
    last_instruction = &tcode->code[cchunksz - 1];
}

/*
 * generate code for a builtin kfun
 */
void CodeChunk::kfun(int kf, unsigned short line)
{
    if (kf < 256) {
	instr(I_CALL_KFUNC, line);
	byte(kf);
    } else {
	instr(I_CALL_EFUNC, line);
	word(kf);
    }
}

/*
 * generate code for a builtin kfun
 */
void CodeChunk::ckfun(int kf, unsigned short line)
{
    if (kf < 256) {
	instr(I_CALL_CKFUNC, line);
	byte(kf);
    } else {
	instr(I_CALL_CEFUNC, line);
	word(kf);
    }
}

/*
 * create function code block
 */
char *CodeChunk::make(unsigned short depth, int nlocals, unsigned short *size)
{
    CodeChunk *l;
    char *code;
    Uint sz;

    *size = sz = 7 + here + line_info_size;

    if (sz > USHRT_MAX) {
	Compile::error("function too large");
    }
    code = ALLOC(char, sz);
    *code++ = depth >> 8;
    *code++ = depth;
    *code++ = max >> 8;
    *code++ = max;
    *code++ = nlocals;
    *code++ = here >> 8;
    *code++ = here;

    /* collect all code blocks in one large block */
    for (l = lcode; l != tcode; l = l->next) {
	memcpy(code, l->code, CODE_CHUNK);
	code += CODE_CHUNK;
    }
    memcpy(code, l->code, cchunksz);
    code += cchunksz;
    LineChunk::make(code);
    code -= 7 + here;

    /* add blocks to free list */
    tcode->next = fcode;
    fcode = lcode;
    /* clear blocks */
    lcode = (CodeChunk *) NULL;
    tcode = (CodeChunk *) NULL;
    cchunksz = CODE_CHUNK;

    here = 0;
    caught = max = 0;
    return code;
}

/*
 * clean up the code chunks
 */
void CodeChunk::clear()
{
    CodeChunk *l, *f;

    for (l = fcode; l != (CodeChunk *) NULL; ) {
	f = l;
	l = l->next;
	FREE(f);
    }
    fcode = (CodeChunk *) NULL;

    LineChunk::clear();
}


# define JUMP_CHUNK	128

class JmpList : public ChunkAllocated {
public:
    static JmpList *addr(JmpList *list);
    static JmpList *jump(int i, unsigned short line, JmpList *list);
    static void resolve(JmpList *list, Uint to);
    static void make(char *code);
    static void clear();

    Uint where;				/* where to jump from */
    Uint to;				/* where to jump to */
    Node *label;			/* label to jump to */
    JmpList *next;			/* next in list */
};

static class JmpChunk : public Chunk<JmpList, JUMP_CHUNK> {
public:
    /*
     * resolve jumps when iterating through items
     */
    virtual bool item(JmpList *j) {
	code[j->where    ] = j->to >> 8;
	code[j->where + 1] = j->to;
	if ((code[j->to] & I_INSTR_MASK) == I_JUMP) {
	    /*
	     * add to jump-to-jump list
	     */
	    j->next = jmpjmp;
	    jmpjmp = j;
	}
	return TRUE;
    }

    /*
     * resolve jumps, and return list of jumps to jumps
     */
    JmpList *make(char *prog) {
	jmpjmp = NULL;
	code = prog;
	items();
	return jmpjmp;
    }

private:
    JmpList *jmpjmp;			/* list of jumps to jumps */
    char *code;				/* program code */
} jchunk;

static JmpList *true_list;		/* list of true jumps */
static JmpList *false_list;		/* list of false jumps */
static JmpList *break_list;		/* list of break jumps */
static JmpList *continue_list;		/* list of continue jumps */
static JmpList *goto_list;		/* list of goto jumps */

/*
 * generate a jump
 */
JmpList *JmpList::addr(JmpList *list)
{
    JmpList *j;

    j = chunknew (jchunk) JmpList;
    j->where = here;
    j->next = list;
    CodeChunk::word(0);	/* empty space in code block filled in later */

    return j;
}

/*
 * create a jump
 */
JmpList *JmpList::jump(int i, unsigned short line, JmpList *list)
{
    CodeChunk::instr(i, line);
    return JmpList::addr(list);
}

/*
 * resolve all jumps in a jump list
 */
void JmpList::resolve(JmpList *list, Uint to)
{
    while (list != (JmpList *) NULL) {
	list->to = to;
	list = list->next;
    }
}

/*
 * fill in all jumps in a code block
 */
void JmpList::make(char *code)
{
    JmpList *j;

    for (j = goto_list; j != (JmpList *) NULL; j = j->next) {
	j->to = j->label->mod;
    }

    for (j = jchunk.make(code); j != (JmpList *) NULL; j = j->next) {
	Uint where, to;

	/*
	 * replace jump-to-jump by a direct jump to destination
	 */
	where = j->where;
	to = j->to;
	while ((code[to] & I_INSTR_MASK) == I_JUMP && to != where - 1) {
	    /*
	     * Change to jump across the next jump.  If there is a loop, it
	     * will eventually result in a jump to itself.
	     */
	    code[where    ] = code[to + 1];
	    code[where + 1] = code[to + 2];
	    where = to + 1;
	    to = (UCHAR(code[to + 1]) << 8) | UCHAR(code[to + 2]);
	}
	/*
	 * jump to final destination
	 */
	code[j->where    ] = to >> 8;
	code[j->where + 1] = to;
    }

    jchunk.clean();
}

/*
 * clean up the jump chunks
 */
void JmpList::clear()
{
    goto_list = (JmpList *) NULL;
    jchunk.clean();
}


static LPCint kd_allocate, kd_allocate_int, kd_allocate_float;
static int nparams;		/* number of parameters */

/*
 * return the type of a node
 */
int Codegen::type(Node *n, long *l)
{
    int type;

    type = n->mod;
    if ((type & T_REF) != 0) {
	type = T_ARRAY;
    }
    if (type == T_CLASS) {
	*l = Control::defString(n->sclass);
    }
    return (type != T_MIXED) ? type : 0;
}

/*
 * generate code for a cast
 */
void Codegen::cast(Node *n)
{
    int t;
    long l;

    t = type(n, &l);
    if (t != 0) {
	CodeChunk::instr(I_CAST, 0);
	CodeChunk::byte(t);
	if (t == T_CLASS) {
	    CodeChunk::byte(l >> 16);
	    CodeChunk::word(l);
	}
    }
}

/*
 * generate code for an lvalue
 */
int Codegen::lvalue(Node *n, int fetch)
{
    int stack;
    Node *m, *l;

    stack = 0;
    m = n;
    if (m->type == N_CAST) {
	m = m->l.left;
    }
    if (m->type == N_INDEX) {
	l = m->l.left;
	if (l->type == N_CAST) {
	    l = l->l.left;
	}
	if (l->type == N_INDEX) {
	    expr(l->l.left, FALSE);
	    expr(l->r.right, FALSE);
	    CodeChunk::instr(I_INDEX2, l->line);
	    stack += 2;
	} else {
	    expr(l, FALSE);
	}
	if (m->l.left->type == N_CAST) {
	    cast(m->l.left);
	}
	stack += 2;
	expr(m->r.right, FALSE);
	if (fetch) {
	    CodeChunk::instr(I_INDEX2, n->line);
	    if (n->type == N_CAST) {
		cast(n);
	    }
	    stack++;
	}
    } else if (fetch) {
	expr(n, FALSE);
	stack++;
    }
    return stack;
}

/*
 * generate code for a store
 */
void Codegen::store(Node *n)
{
    if (n->type == N_CAST) {
	n = n->l.left;
    }
    switch (n->type) {
    case N_LOCAL:
	CodeChunk::instr(I_STORE_LOCAL, n->line);
	CodeChunk::byte(nparams - (int) n->r.number - 1);
	break;

    case N_GLOBAL:
	if ((n->r.number >> 8) == Control::nInherits()) {
	    CodeChunk::instr(I_STORE_GLOBAL, n->line);
	} else {
	    CodeChunk::instr(I_STORE_FAR_GLOBAL, n->line);
	    CodeChunk::byte(n->r.number >> 8);
	}
	CodeChunk::byte(n->r.number);
	break;

    case N_INDEX:
	n = n->l.left;
	if (n->type == N_CAST) {
	    n = n->l.left;
	}
	switch (n->type) {
	case N_LOCAL:
	    CodeChunk::instr(I_STORE_LOCAL_INDEX, n->line);
	    CodeChunk::byte(nparams - (int) n->r.number - 1);
	    break;

	case N_GLOBAL:
	    if ((n->r.number >> 8) == Control::nInherits()) {
		CodeChunk::instr(I_STORE_GLOBAL_INDEX, n->line);
	    } else {
		CodeChunk::instr(I_STORE_FAR_GLOBAL_INDEX, n->line);
		CodeChunk::byte(n->r.number >> 8);
	    }
	    CodeChunk::byte(n->r.number);
	    break;

	case N_INDEX:
	    CodeChunk::instr(I_STORE_INDEX_INDEX, n->line);
	    break;

	default:
	    CodeChunk::instr(I_STORE_INDEX, n->line);
	    break;
	}
	break;
    }
}

/*
 * generate code for an assignment operator
 */
void Codegen::assign(Node *n, int op)
{
    lvalue(n->l.left, TRUE);
    expr(n->r.right, FALSE);
    CodeChunk::kfun(op, n->line);
    store(n->l.left);
}

/*
 * generate code for an aggregate
 */
int Codegen::aggr(Node *n)
{
    int i;

    if (n == (Node *) NULL) {
	return 0;
    }
    for (i = 1; n->type == N_PAIR; i++) {
	expr(n->l.left, FALSE);
	n = n->r.right;
    }
    expr(n, FALSE);
    return i;
}

/*
 * generate code for a mapping aggregate
 */
int Codegen::mapAggr(Node *n)
{
    int i;

    if (n == (Node *) NULL) {
	return 0;
    }
    for (i = 2; n->type == N_PAIR; i += 2) {
	expr(n->l.left->l.left, FALSE);
	expr(n->l.left->r.right, FALSE);
	n = n->r.right;
    }
    expr(n->l.left, FALSE);
    expr(n->r.right, FALSE);
    return i;
}

/*
 * generate code for an lvalue aggregate
 */
int Codegen::lvalAggr(Node **l)
{
    int i;
    Node *n, *m;

    i = 1;
    n = *l;
    if (n->type == N_PAIR) {
	for (m = n->l.left; ; m = n->l.left->r.right) {
	    lvalue(m, FALSE);
	    i++;
	    m = n->r.right;
	    if (m->type != N_PAIR) {
		break;
	    }
	    /* (a, (b, c)) => ((a, b), c) */
	    n->r.right = m->l.left;
	    m->l.left = n;
	    n = m;
	}
	*l = n;
	n = m;
    }
    lvalue(n, FALSE);

    return i;
}

/*
 * generate stores for an lvalue aggregate
 */
void Codegen::storeAggr(Node *n)
{
    while (n->type == N_PAIR) {
	cast(n->r.right);
	store(n->r.right);
	n = n->l.left;
    }
    cast(n);
    store(n);
}

/*
 * generate code for summand arguments
 */
int Codegen::sumargs(Node *n)
{
    int i;
    Node *m;

    if (n->type == N_SUM) {
	i = sumargs(n->l.left);
	n = n->r.right;
    } else {
	i = 0;
    }

    switch (n->type) {
    case N_AGGR:
	n->type = N_INT;
	n->l.number = SUM_AGGREGATE - aggr(n->l.left);
	expr(n, FALSE);
	break;

    case N_RANGE:
	expr(n->l.left, FALSE);
	n = n->r.right;
	if (n->l.left != (Node *) NULL) {
	    expr(n->l.left, FALSE);
	    if (n->r.right != (Node *) NULL) {
		expr(n->r.right, FALSE);
		CodeChunk::kfun(KF_CKRANGEFT, n->line);
	    } else {
		CodeChunk::kfun(KF_CKRANGEF, n->line);
	    }
	} else if (n->r.right != (Node *) NULL) {
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_CKRANGET, n->line);
	} else {
	    CodeChunk::kfun(KF_RANGE, n->line);
	    CodeChunk::instr(I_PUSH_INT1, 0);
	    CodeChunk::byte(SUM_SIMPLE);
	}
	break;

    case N_FUNC:
	m = n->l.left->r.right;
	if (m != (Node *) NULL && m->type != N_PAIR && m->type != N_SPREAD &&
	    m->mod == T_INT) {
	    if (n->r.number == kd_allocate) {
		expr(m, FALSE);
		CodeChunk::instr(I_PUSH_INT1, 0);
		CodeChunk::byte(SUM_ALLOCATE_NIL);
		break;
	    } else if (n->r.number == kd_allocate_int) {
		expr(m, FALSE);
		CodeChunk::instr(I_PUSH_INT1, 0);
		CodeChunk::byte(SUM_ALLOCATE_INT);
		break;
	    } else if (n->r.number == kd_allocate_float) {
		expr(m, FALSE);
		CodeChunk::instr(I_PUSH_INT1, 0);
		CodeChunk::byte(SUM_ALLOCATE_FLT);
		break;
	    }
	}
	/* fall through */
    default:
	expr(n, FALSE);
	CodeChunk::instr(I_PUSH_INT1, 0);
	CodeChunk::byte(SUM_SIMPLE);		/* no range */
	break;
    }

    return i + 1;
}

/*
 * generate code for function arguments
 */
int Codegen::funargs(Node **l, int *nargs, bool *spread)
{
    Node *n, *m;
    int stack;

    *spread = FALSE;
    n = *l;
    *l = (Node *) NULL;
    *nargs = 0;
    if (n == (Node *) NULL) {
	return 0;
    }

    stack = 0;
    while (n->type == N_PAIR) {
	m = n;
	n = n->r.right;
	if (m->l.left->type == N_LVALUE) {
	    stack += lvalue(m->l.left->l.left, FALSE);
	    if (*l != (Node *) NULL) {
		(*l)->r.right = m->l.left;
		m->l.left = *l;
	    }
	    *l = m;
	    (*nargs)++;
	} else {
	    expr(m->l.left, FALSE);
	    stack++;
	}
    }
    if (n->type == N_SPREAD) {
	expr(n->l.left, FALSE);
	stack++;
	CodeChunk::instr(I_SPREAD, n->line);
	CodeChunk::byte(-(short) n->mod - 2);
	if ((short) n->mod >= 0) {
	    if (*l == (Node *) NULL) {
		*l = n;
	    }
	    (*nargs)++;
	}
	*spread = TRUE;
    } else if (n->type == N_LVALUE) {
	stack += lvalue(n->l.left, FALSE);
	if (*l == (Node *) NULL) {
	    *l = n;
	}
	(*nargs)++;
    } else {
	expr(n, FALSE);
	stack++;
    }
    return stack;
}

/*
 * generate storage code for one lvalue argument
 */
void Codegen::storearg(Node *n)
{
    if (n->type == N_SPREAD) {
	int t;
	long l;

	CodeChunk::instr(I_SPREAD, n->line);
	CodeChunk::byte(n->mod);
	n = n->l.left;
	if (n->mod & T_REF) {
	    n->mod -= (1 << REFSHIFT);
	}
	t = type(n, &l);
	CodeChunk::byte(t);
	if (t == T_CLASS) {
	    CodeChunk::byte(l >> 16);
	    CodeChunk::word(l);
	}
    } else {
	/* N_LVALUE */
	cast(n->l.left);
	store(n->l.left);
    }
}

/*
 * generate storage code for lvalue arguments
 */
void Codegen::storeargs(Node *n)
{
    while (n->type == N_PAIR) {
	storearg(n->r.right);
	n = n->l.left;
    }
    storearg(n);
}

/*
 * return builtin code of kfun, or 0
 */
int Codegen::math(const char *name)
{
    static const char *keyword[] = {
	"cos", "cosh", "asin", "atan", "fabs", "tanh", "tan", "pow",
	"log10", "log", "modf", "sinh", "sin", "acos", "ldexp", "frexp",
	"atan2", "sqrt", "exp", "floor", "fmod", "ceil"
    };
    static char value[] = {
	 8,  0, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  4,  0, 13,  0,  3, 10,  9,  1, 10,  0,  0,
	18, 20,  2,  0, 14, 16,  1,  0,  1,  0,  0,  7,  4,  0,  0
    };
    static char builtin[] = {
	KF_COS, KF_COSH, KF_ASIN, KF_ATAN, KF_FABS, KF_TANH, KF_TAN, KF_POW,
	KF_LOG10, KF_LOG, KF_MODF, KF_SINH, KF_SIN, KF_ACOS, KF_LDEXP,
	KF_FREXP, KF_ATAN2, KF_SQRT, KF_EXP, KF_FLOOR, KF_FMOD, KF_CEIL
    };
    int n;

    n = strlen(name);
    if (n >= 3) {
	n = (value[name[1] - '0'] + value[name[n - 1] - '0']) % 22;
	if (strcmp(keyword[n], name) == 0) {
	    return builtin[n];
	}
    }

    return 0;
}

/*
 * generate code for an expression
 */
void Codegen::expr(Node *n, int pop)
{
    JmpList *jlist, *j2list;
    unsigned short i;
    long l;
    Node *args;
    int nargs;
    bool spread;
# ifdef LARGENUM
    Float flt;
    unsigned short fhigh;
    Uint flow;
# endif

    switch (n->type) {
    case N_ADD:
	expr(n->l.left, FALSE);
	if (n->r.right->type == N_FLOAT) {
	    if (NFLT_ISONE(n->r.right)) {
		CodeChunk::kfun(KF_ADD1, n->line);
		break;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		CodeChunk::kfun(KF_SUB1, n->line);
		break;
	    }
	}
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_ADD, n->line);
	break;

    case N_ADD_INT:
	expr(n->l.left, FALSE);
	if (n->r.right->type == N_INT) {
	    if (n->r.right->l.number == 1) {
		CodeChunk::kfun(KF_ADD1_INT, n->line);
		break;
	    }
	    if (n->r.right->l.number == -1) {
		CodeChunk::kfun(KF_SUB1_INT, n->line);
		break;
	    }
	}
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_ADD_INT, n->line);
	break;

    case N_ADD_FLOAT:
	expr(n->l.left, FALSE);
	if (n->r.right->type == N_FLOAT) {
	    if (NFLT_ISONE(n->r.right)) {
		CodeChunk::kfun(KF_ADD1_FLT, n->line);
		break;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		CodeChunk::kfun(KF_SUB1_FLT, n->line);
		break;
	    }
	}
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_ADD_FLT, n->line);
	break;


    case N_ADD_EQ:
	assign(n, KF_ADD);
	break;

    case N_ADD_EQ_INT:
	assign(n, KF_ADD_INT);
	break;

    case N_ADD_EQ_FLOAT:
	assign(n, KF_ADD_FLT);
	break;

    case N_ADD_EQ_1:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_ADD1, 0);
	store(n->l.left);
	break;

    case N_ADD_EQ_1_INT:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_ADD1_INT, 0);
	store(n->l.left);
	break;

    case N_ADD_EQ_1_FLOAT:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_ADD1_FLT, 0);
	store(n->l.left);
	break;

    case N_AGGR:
	if (n->mod == T_MAPPING) {
	    i = mapAggr(n->l.left);
	    CodeChunk::instr(I_AGGREGATE, n->line);
	    CodeChunk::byte(1);
	} else {
	    i = aggr(n->l.left);
	    CodeChunk::instr(I_AGGREGATE, n->line);
	    CodeChunk::byte(0);
	}
	CodeChunk::word(i);
	break;

    case N_AND:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_AND, n->line);
	break;

    case N_AND_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_AND_INT, n->line);
	break;

    case N_AND_EQ:
	assign(n, KF_AND);
	break;

    case N_AND_EQ_INT:
	assign(n, KF_AND_INT);
	break;

    case N_ASSIGN:
	if (n->l.left->type == N_AGGR) {
	    char *instr;

	    l = lvalAggr(&n->l.left->l.left);
	    expr(n->r.right, FALSE);
	    CodeChunk::instr(I_STORES, n->line);
	    CodeChunk::word(l);
	    instr = last_instruction;
	    storeAggr(n->l.left->l.left);
	    last_instruction = instr;
	    break;
	}

	lvalue(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	store(n->l.left);
	break;

    case N_CAST:
	expr(n->l.left, FALSE);
	cast(n);
	break;

    case N_CATCH:
	jlist = JmpList::jump((pop) ? I_CATCH | I_POP_BIT : I_CATCH, 0,
			      (JmpList *) NULL);
	if (++caught > max) {
	    max = caught;
	}
	expr(n->l.left, TRUE);
	--caught;
	if (!pop) {
	    CodeChunk::kfun(KF_NIL, 0);
	}
	CodeChunk::instr(I_RETURN, 0);
	JmpList::resolve(jlist, here);
	return;

    case N_COMMA:
	expr(n->l.left, TRUE);
	expr(n->r.right, pop);
	return;

    case N_DIV:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_DIV, n->line);
	break;

    case N_DIV_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_DIV_INT, n->line);
	break;

    case N_DIV_FLOAT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_DIV_FLT, n->line);
	break;

    case N_DIV_EQ:
	assign(n, KF_DIV);
	break;

    case N_DIV_EQ_INT:
	assign(n, KF_DIV_INT);
	break;

    case N_DIV_EQ_FLOAT:
	assign(n, KF_DIV_FLT);
	break;

    case N_EQ:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_EQ, n->line);
	break;

    case N_EQ_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_EQ_INT, n->line);
	break;

    case N_EQ_FLOAT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_EQ_FLT, n->line);
	break;

    case N_EXCEPTION:
	store(n->l.left);
	break;

    case N_FLOAT:
# ifdef LARGENUM
	flt.high = n->l.fhigh;
	flt.low = n->r.flow;
	if (Ext::smallFloat(&fhigh, &flow, &flt)) {
	    CodeChunk::instr(I_PUSH_FLOAT6, n->line);
	    CodeChunk::word(fhigh);
	    CodeChunk::word((int) (flow >> 16));
	    CodeChunk::word((int) flow);
	} else {
	    CodeChunk::instr(I_PUSH_FLOAT12, n->line);
	    CodeChunk::word((int) (n->l.fhigh >> 16));
	    CodeChunk::word((int) n->l.fhigh);
	    CodeChunk::word((int) (n->r.flow >> 48));
	    CodeChunk::word((int) (n->r.flow >> 32));
	    CodeChunk::word((int) (n->r.flow >> 16));
	    CodeChunk::word((int) n->r.flow);
	}
# else
	CodeChunk::instr(I_PUSH_FLOAT6, n->line);
	CodeChunk::word(n->l.fhigh);
	CodeChunk::word((int) (n->r.flow >> 16));
	CodeChunk::word((int) n->r.flow);
# endif
	break;

    case N_FUNC:
	args = n->l.left->r.right;
	i = funargs(&args, &nargs, &spread);
	switch (n->r.number >> 24) {
	case KFCALL:
	case KFCALL_LVAL:
	    if (spread &&
		(i < PROTO_NARGS(KFUN((short) n->r.number).proto) ||
		 !(PROTO_CLASS(KFUN((short) n->r.number).proto) & C_ELLIPSIS)))
	    {
		CodeChunk::ckfun((short) n->r.number, n->line);
		CodeChunk::byte(i);
	    } else if (PROTO_VARGS(KFUN((short) n->r.number).proto) != 0) {
		CodeChunk::kfun((short) n->r.number, n->line);
		CodeChunk::byte(i);
	    } else {
		i = math(KFUN((short) n->r.number).name);
		if (i != 0) {
		    Node *argp;
		    char *proto;

		    /*
		     * see if math kfun can be remapped to a builtin
		     */
		    argp = n->l.left->r.right;
		    proto = KFUN((short) n->r.number).proto;
		    if (PROTO_NARGS(proto) == 2) {
			if (PROTO_ARGS(proto)[0] == argp->l.left->mod &&
			    PROTO_ARGS(proto)[1] == argp->r.right->mod) {
			    n->r.number = i;
			}
		    } else if (PROTO_ARGS(proto)[0] == argp->mod) {
			n->r.number = i;
		    }
		}

		CodeChunk::kfun((short) n->r.number, n->line);
	    }
	    if ((n->r.number >> 24) == KFCALL_LVAL) {
		/* generate stores */
		CodeChunk::instr(I_STORES, n->line);
		CodeChunk::word(nargs);
		if (args != (Node *) NULL) {
		    char *instr;

		    instr = last_instruction;
		    storeargs(args);
		    last_instruction = instr;
		}
	    }
	    break;

	case DFCALL:
	    if ((n->r.number & 0xff00) == 0) {
		/* auto object */
		CodeChunk::instr(I_CALL_AFUNC, n->line);
		CodeChunk::byte((int) n->r.number);
	    } else {
		CodeChunk::instr(I_CALL_DFUNC, n->line);
		CodeChunk::word((int) n->r.number);
	    }
	    CodeChunk::byte(i);
	    break;

	case FCALL:
	    CodeChunk::instr(I_CALL_FUNC, n->line);
	    CodeChunk::word(Control::genCall((long) n->r.number));
	    CodeChunk::byte(i);
	    break;
	}
	break;

    case N_GE:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_GE, n->line);
	break;

    case N_GE_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_GE_INT, n->line);
	break;

    case N_GE_FLOAT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_GE_FLT, n->line);
	break;

    case N_GLOBAL:
	if ((n->r.number >> 8) == Control::nInherits()) {
	    CodeChunk::instr(I_PUSH_GLOBAL, n->line);
	    CodeChunk::byte((int) n->r.number);
	} else {
	    CodeChunk::instr(I_PUSH_FAR_GLOBAL, n->line);
	    CodeChunk::word((int) n->r.number);
	}
	break;

    case N_GT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_GT, n->line);
	break;

    case N_GT_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_GT_INT, n->line);
	break;

    case N_GT_FLOAT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_GT_FLT, n->line);
	break;

    case N_INDEX:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::instr(I_INDEX, n->line);
	break;

    case N_INSTANCEOF:
	expr(n->l.left, FALSE);
	CodeChunk::instr(I_INSTANCEOF, n->line);
	l = Control::defString(n->r.right->l.string) & 0xffffffL;
	CodeChunk::byte(l >> 16);
	CodeChunk::word(l);
	break;

    case N_INT:
	if (n->l.number >= -128 && n->l.number <= 127) {
	    CodeChunk::instr(I_PUSH_INT1, n->line);
	    CodeChunk::byte((int) n->l.number);
# ifdef LARGENUM
	} else if (n->l.number < -2147483648LL || n->l.number > 2147483647LL) {
	    CodeChunk::instr(I_PUSH_INT8, n->line);
	    CodeChunk::word((int) (n->l.number >> 48));
	    CodeChunk::word((int) (n->l.number >> 32));
	    CodeChunk::word((int) (n->l.number >> 16));
	    CodeChunk::word((int) n->l.number);
# endif
	} else {
	    CodeChunk::instr(I_PUSH_INT4, n->line);
	    CodeChunk::word((int) (n->l.number >> 16));
	    CodeChunk::word((int) n->l.number);
	}
	break;

    case N_LAND:
	if (!pop) {
	    jlist = true_list;
	    true_list = (JmpList *) NULL;
	    cond(n, TRUE);
	    CodeChunk::instr(I_PUSH_INT1, 0);
	    CodeChunk::byte(0);
	    j2list = JmpList::jump(I_JUMP, 0, (JmpList *) NULL);
	    JmpList::resolve(true_list, here);
	    true_list = jlist;
	    CodeChunk::instr(I_PUSH_INT1, 0);
	    CodeChunk::byte(1);
	    JmpList::resolve(j2list, here);
	} else {
	    jlist = false_list;
	    false_list = (JmpList *) NULL;
	    cond(n->l.left, FALSE);
	    expr(n->r.right, TRUE);
	    JmpList::resolve(false_list, here);
	    false_list = jlist;
	}
	return;

    case N_LE:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_LE, n->line);
	break;

    case N_LE_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_LE_INT, n->line);
	break;

    case N_LE_FLOAT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_LE_FLT, n->line);
	break;

    case N_LOCAL:
	CodeChunk::instr(I_PUSH_LOCAL, n->line);
	CodeChunk::byte(nparams - (int) n->r.number - 1);
	break;

    case N_LOR:
	if (!pop) {
	    jlist = false_list;
	    false_list = (JmpList *) NULL;
	    cond(n, FALSE);
	    CodeChunk::instr(I_PUSH_INT1, 0);
	    CodeChunk::byte(1);
	    j2list = JmpList::jump(I_JUMP, 0, (JmpList *) NULL);
	    JmpList::resolve(false_list, here);
	    false_list = jlist;
	    CodeChunk::instr(I_PUSH_INT1, 0);
	    CodeChunk::byte(0);
	    JmpList::resolve(j2list, here);
	} else {
	    jlist = true_list;
	    true_list = (JmpList *) NULL;
	    cond(n->l.left, TRUE);
	    expr(n->r.right, TRUE);
	    JmpList::resolve(true_list, here);
	    true_list = jlist;
	}
	return;

    case N_LSHIFT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_LSHIFT, n->line);
	break;

    case N_LSHIFT_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_LSHIFT_INT, n->line);
	break;

    case N_LSHIFT_EQ:
	assign(n, KF_LSHIFT);
	break;

    case N_LSHIFT_EQ_INT:
	assign(n, KF_LSHIFT_INT);
	break;

    case N_LT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_LT, n->line);
	break;

    case N_LT_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_LT_INT, n->line);
	break;

    case N_LT_FLOAT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_LT_FLT, n->line);
	break;

    case N_MOD:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_MOD, n->line);
	break;

    case N_MOD_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_MOD_INT, n->line);
	break;

    case N_MOD_EQ:
	assign(n, KF_MOD);
	break;

    case N_MOD_EQ_INT:
	assign(n, KF_MOD_INT);
	break;

    case N_MULT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_MULT, n->line);
	break;

    case N_MULT_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_MULT_INT, n->line);
	break;

    case N_MULT_FLOAT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_MULT_FLT, n->line);
	break;

    case N_MULT_EQ:
	assign(n, KF_MULT);
	break;

    case N_MULT_EQ_INT:
	assign(n, KF_MULT_INT);
	break;

    case N_MULT_EQ_FLOAT:
	assign(n, KF_MULT_FLT);
	break;

    case N_NE:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_NE, n->line);
	break;

    case N_NE_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_NE_INT, n->line);
	break;

    case N_NE_FLOAT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_NE_FLT, n->line);
	break;

    case N_NEG:
	expr(n->l.left, FALSE);
	CodeChunk::kfun(KF_NEG, n->line);
	break;

    case N_NIL:
	CodeChunk::kfun(KF_NIL, n->line);
	break;

    case N_NOT:
	expr(n->l.left, FALSE);
	CodeChunk::kfun((n->l.left->mod == T_INT) ? KF_NOT_INT : KF_NOT,
			n->line);
	break;

    case N_OR:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_OR, n->line);
	break;

    case N_OR_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_OR_INT, n->line);
	break;

    case N_OR_EQ:
	assign(n, KF_OR);
	break;

    case N_OR_EQ_INT:
	assign(n, KF_OR_INT);
	break;

    case N_QUEST:
	if (n->r.right->l.left != (Node *) NULL) {
	    jlist = false_list;
	    false_list = (JmpList *) NULL;
	    cond(n->l.left, FALSE);
	    expr(n->r.right->l.left, pop);
	    if (n->r.right->r.right != (Node *) NULL) {
		j2list = JmpList::jump(I_JUMP, 0, (JmpList *) NULL);
		JmpList::resolve(false_list, here);
		false_list = jlist;
		expr(n->r.right->r.right, pop);
		JmpList::resolve(j2list, here);
	    } else {
		JmpList::resolve(false_list, here);
		false_list = jlist;
	    }
	} else {
	    jlist = true_list;
	    true_list = (JmpList *) NULL;
	    cond(n->l.left, TRUE);
	    if (n->r.right->r.right != (Node *) NULL) {
		expr(n->r.right->r.right, pop);
	    }
	    JmpList::resolve(true_list, here);
	    true_list = jlist;
	}
	return;

    case N_RANGE:
	expr(n->l.left, FALSE);
	n = n->r.right;
	if (n->l.left != (Node *) NULL) {
	    expr(n->l.left, FALSE);
	    if (n->r.right != (Node *) NULL) {
		expr(n->r.right, FALSE);
		CodeChunk::kfun(KF_RANGEFT, n->line);
	    } else {
		CodeChunk::kfun(KF_RANGEF, n->line);
	    }
	} else if (n->r.right != (Node *) NULL) {
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_RANGET, n->line);
	} else {
	    CodeChunk::kfun(KF_RANGE, n->line);
	}
	break;

    case N_RSHIFT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_RSHIFT, n->line);
	break;

    case N_RSHIFT_INT:
	expr(n->l.left, FALSE);
	expr(n->r.right, FALSE);
	CodeChunk::kfun(KF_RSHIFT_INT, n->line);
	break;

    case N_RSHIFT_EQ:
	assign(n, KF_RSHIFT);
	break;

    case N_RSHIFT_EQ_INT:
	assign(n, KF_RSHIFT_INT);
	break;

    case N_STR:
	l = Control::defString(n->l.string);
	if ((l & 0x01000000L) && (unsigned short) l < 256) {
	    CodeChunk::instr(I_PUSH_STRING, n->line);
	    CodeChunk::byte((int) l);
	} else if ((unsigned short) l < 256) {
	    CodeChunk::instr(I_PUSH_NEAR_STRING, n->line);
	    CodeChunk::byte((int) (l >> 16));
	    CodeChunk::byte((int) l);
	} else {
	    CodeChunk::instr(I_PUSH_FAR_STRING, n->line);
	    CodeChunk::byte((int) (l >> 16));
	    CodeChunk::word((int) l);
	}
	break;

    case N_SUB:
	if ((n->l.left->type == N_INT && n->l.left->l.number == 0) ||
	    (n->l.left->type == N_FLOAT && NFLT_ISZERO(n->l.left))) {
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_UMIN, n->line);
	} else {
	    expr(n->l.left, FALSE);
	    if (n->r.right->type == N_FLOAT) {
		if (NFLT_ISONE(n->r.right)) {
		    CodeChunk::kfun(KF_SUB1, n->line);
		    break;
		}
		if (NFLT_ISMONE(n->r.right)) {
		    CodeChunk::kfun(KF_ADD1, n->line);
		    break;
		}
	    }
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_SUB, n->line);
	}
	break;

    case N_SUB_INT:
	if (n->l.left->type == N_INT && n->l.left->l.number == 0) {
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_UMIN_INT, n->line);
	} else {
	    expr(n->l.left, FALSE);
	    if (n->r.right->type == N_INT) {
		if (n->r.right->l.number == 1) {
		    CodeChunk::kfun(KF_SUB1_INT, n->line);
		    break;
		}
		if (n->r.right->l.number == -1) {
		    CodeChunk::kfun(KF_ADD1_INT, n->line);
		    break;
		}
	    }
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_SUB_INT, n->line);
	}
	break;

    case N_SUB_FLOAT:
	if (n->l.left->type == N_FLOAT && n->l.left->l.number == 0) {
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_UMIN_FLT, n->line);
	} else {
	    expr(n->l.left, FALSE);
	    if (n->r.right->type == N_FLOAT) {
		if (NFLT_ISONE(n->r.right)) {
		    CodeChunk::kfun(KF_SUB1_FLT, n->line);
		    break;
		}
		if (NFLT_ISMONE(n->r.right)) {
		    CodeChunk::kfun(KF_ADD1_FLT, n->line);
		    break;
		}
	    }
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_SUB_FLT, n->line);
	}
	break;

    case N_SUB_EQ:
	assign(n, KF_SUB);
	break;

    case N_SUB_EQ_INT:
	assign(n, KF_SUB_INT);
	break;

    case N_SUB_EQ_FLOAT:
	assign(n, KF_SUB);
	break;

    case N_SUB_EQ_1:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_SUB1, 0);
	store(n->l.left);
	break;

    case N_SUB_EQ_1_INT:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_SUB1_INT, 0);
	store(n->l.left);
	break;

    case N_SUB_EQ_1_FLOAT:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_SUB1_FLT, 0);
	store(n->l.left);
	break;


    case N_SUM:
	i = sumargs(n);
	CodeChunk::kfun(KF_SUM, 0);
	CodeChunk::byte(i);
	break;

    case N_SUM_EQ:
	lvalue(n->l.left, TRUE);
	CodeChunk::instr(I_PUSH_INT1, 0);
	CodeChunk::byte(SUM_SIMPLE);
	i = sumargs(n->r.right) + 1;
	CodeChunk::kfun(KF_SUM, 0);
	CodeChunk::byte(i);
	store(n->l.left);
	break;

    case N_TOFLOAT:
	expr(n->l.left, FALSE);
	CodeChunk::kfun(KF_TOFLOAT, n->line);
	break;

    case N_TOINT:
	expr(n->l.left, FALSE);
	CodeChunk::kfun(KF_TOINT, n->line);
	break;

    case N_TOSTRING:
	expr(n->l.left, FALSE);
	CodeChunk::kfun(KF_TOSTRING, n->line);
	break;

    case N_TST:
	expr(n->l.left, FALSE);
	if( n->l.left->mod == T_INT ) {
	    CodeChunk::kfun(KF_TST_INT, n->line);
	} else if( n->l.left->mod == T_FLOAT ){
	    CodeChunk::kfun(KF_TST_FLT, n->line);
	} else {
	    CodeChunk::kfun(KF_TST, n->line);
	}
	break;

    case N_UMIN:
	expr(n->l.left, FALSE);
	CodeChunk::kfun(KF_UMIN, n->line);
	break;

    case N_XOR:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    expr(n->l.left, FALSE);
	    CodeChunk::kfun(KF_NEG, n->line);
	} else {
	    expr(n->l.left, FALSE);
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_XOR, n->line);
	}
	break;

    case N_XOR_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    expr(n->l.left, FALSE);
	    CodeChunk::kfun(KF_NEG_INT, n->line);
	} else {
	    expr(n->l.left, FALSE);
	    expr(n->r.right, FALSE);
	    CodeChunk::kfun(KF_XOR_INT, n->line);
	}
	break;

    case N_XOR_EQ:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    lvalue(n->l.left, TRUE);
	    CodeChunk::kfun(KF_NEG, 0);
	    store(n->l.left);
	} else {
	    assign(n, KF_XOR);
	}
	break;

    case N_XOR_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    lvalue(n->l.left, TRUE);
	    CodeChunk::kfun(KF_NEG_INT, 0);
	    store(n->l.left);
	} else {
	    assign(n, KF_XOR_INT);
	}
	break;

    case N_MIN_MIN:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_SUB1, 0);
	store(n->l.left);
	if( n->mod == T_INT ) {
	    CodeChunk::kfun(KF_ADD1_INT, 0);
	} else if( n->mod == T_FLOAT ) {
	    CodeChunk::kfun(KF_ADD1_FLT, 0);
	} else {
	    CodeChunk::kfun(KF_ADD1, 0);
	}
	break;

    case N_MIN_MIN_INT:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_SUB1_INT, 0);
	store(n->l.left);
	CodeChunk::kfun(KF_ADD1_INT, 0);
	break;

    case N_MIN_MIN_FLOAT:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_SUB1_FLT, 0);
	store(n->l.left);
	CodeChunk::kfun(KF_ADD1_FLT, 0);
	break;

    case N_PLUS_PLUS:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_ADD1, 0);
	store(n->l.left);
	if( n->mod == T_INT ) {
	    CodeChunk::kfun(KF_SUB1_INT, 0);
	} else if( n->mod == T_FLOAT ) {
	    CodeChunk::kfun(KF_SUB1_FLT, 0);
	} else {
	    CodeChunk::kfun(KF_SUB1, 0);
	}
	break;

    case N_PLUS_PLUS_INT:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_ADD1_INT, 0);
	store(n->l.left);
	CodeChunk::kfun(KF_SUB1_INT, 0);
	break;

    case N_PLUS_PLUS_FLOAT:
	lvalue(n->l.left, TRUE);
	CodeChunk::kfun(KF_ADD1_FLT, 0);
	store(n->l.left);
	CodeChunk::kfun(KF_SUB1_FLT, 0);
	break;

# ifdef DEBUG
    default:
	EC->fatal("unknown expression type %d", n->type);
# endif
    }

    if (pop) {
	*last_instruction |= I_POP_BIT;
    }
}

/*
 * generate code for a condition
 */
void Codegen::cond(Node *n, int jmptrue)
{
    JmpList *jlist;

    for (;;) {
	switch (n->type) {
	case N_INT:
	    if (jmptrue) {
		if (n->l.number != 0) {
		    true_list = JmpList::jump(I_JUMP, 0, true_list);
		}
	    } else if (n->l.number == 0) {
		false_list = JmpList::jump(I_JUMP, 0, false_list);
	    }
	    break;

	case N_LAND:
	    if (jmptrue) {
		jlist = false_list;
		false_list = (JmpList *) NULL;
		cond(n->l.left, FALSE);
		cond(n->r.right, TRUE);
		JmpList::resolve(false_list, here);
		false_list = jlist;
	    } else {
		cond(n->l.left, FALSE);
		cond(n->r.right, FALSE);
	    }
	    break;

	case N_LOR:
	    if (!jmptrue) {
		jlist = true_list;
		true_list = (JmpList *) NULL;
		cond(n->l.left, TRUE);
		cond(n->r.right, FALSE);
		JmpList::resolve(true_list, here);
		true_list = jlist;
	    } else {
		cond(n->l.left, TRUE);
		cond(n->r.right, TRUE);
	    }
	    break;

	case N_NOT:
	    jlist = true_list;
	    true_list = false_list;
	    false_list = jlist;
	    cond(n->l.left, !jmptrue);
	    jlist = true_list;
	    true_list = false_list;
	    false_list = jlist;
	    break;

	case N_COMMA:
	    expr(n->l.left, TRUE);
	    n = n->r.right;
	    continue;

	default:
	    expr(n, FALSE);
	    if (jmptrue) {
		true_list = JmpList::jump(I_JUMP_NONZERO, 0, true_list);
	    } else {
		false_list = JmpList::jump(I_JUMP_ZERO, 0, false_list);
	    }
	    break;
	}
	break;
    }
}

struct case_label {
    Uint where;				/* where to jump to */
    JmpList *jump;			/* list of unresolved jumps */
};

static case_label *switch_table;	/* label table for current switch */

/*
 * generate code for the start of a switch statement
 */
void Codegen::switchStart(Node *n)
{
    Node *m;

    /*
     * initializers
     */
    m = n->r.right->r.right;
    if (m->type == N_BLOCK) {
	m = m->l.left;
    }
# ifdef DEBUG
    if (m->type != N_COMPOUND) {
	EC->fatal("N_COMPOUND expected");
    }
# endif
    stmt(m->r.right);
    m->r.right = NULL;

    /*
     * switch expression
     */
    expr(n->r.right->l.left, FALSE);
    CodeChunk::instr(I_SWITCH, 0);
}

/*
 * generate single label code for a switch statement
 */
void Codegen::switchInt(Node *n)
{
    Node *m;
    int i, size, sz;
    case_label *table;

    switchStart(n);
    CodeChunk::byte(SWITCH_INT);
    m = n->l.left;
    size = n->mod;
    sz = n->r.right->mod;
    if (m->l.left == (Node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    CodeChunk::word(size);
    CodeChunk::byte(sz);

    table = switch_table;
    switch_table = ALLOCA(case_label, size);
    switch_table[0].jump = JmpList::addr((JmpList *) NULL);
    i = 1;
    do {
	LPCint l;

	l = m->l.left->l.number;
	switch (sz) {
# ifdef LARGENUM
	case 8:
	    CodeChunk::word((int) (l >> 48));
	    /* fall through */
	case 6:
	    CodeChunk::word((int) (l >> 32));
	    /* fall through */
# endif
	case 4:
	    CodeChunk::word((int) (l >> 16));
	    /* fall through */
	case 2:
	    CodeChunk::word((int) l);
	    break;

# ifdef LARGENUM
	case 7:
	    CodeChunk::word((int) (l >> 40));
	    /* fall through */
	case 5:
	    CodeChunk::word((int) (l >> 24));
	    /* fall through */
# endif
	case 3:
	    CodeChunk::byte((int) (l >> 16));
	    CodeChunk::word((int) l);
	    break;

	case 1:
	    CodeChunk::byte((int) l);
	    break;
	}
	switch_table[i++].jump = JmpList::addr((JmpList *) NULL);
	m = m->r.right;
    } while (i < size);

    /*
     * generate code for body
     */
    stmt(n->r.right->r.right);

    /*
     * resolve jumps
     */
    if (size > n->mod) {
	/* default: across switch */
	switch_table[0].where = here;
    }
    for (i = 0; i < size; i++) {
	JmpList::resolve(switch_table[i].jump, switch_table[i].where);
    }
    AFREE(switch_table);
    switch_table = table;
}

/*
 * generate range label code for a switch statement
 */
void Codegen::switchRange(Node *n)
{
    Node *m;
    int i, size, sz;
    case_label *table;

    switchStart(n);
    CodeChunk::byte(SWITCH_RANGE);
    m = n->l.left;
    size = n->mod;
    sz = n->r.right->mod;
    if (m->l.left == (Node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    CodeChunk::word(size);
    CodeChunk::byte(sz);

    table = switch_table;
    switch_table = ALLOCA(case_label, size);
    switch_table[0].jump = JmpList::addr((JmpList *) NULL);
    i = 1;
    do {
	LPCint l;

	l = m->l.left->l.number;
	switch (sz) {
# ifdef LARGENUM
	case 8:
	    CodeChunk::word((int) (l >> 48));
	    /* fall through */
	case 6:
	    CodeChunk::word((int) (l >> 32));
	    /* fall through */
# endif
	case 4:
	    CodeChunk::word((int) (l >> 16));
	    /* fall through */
	case 2:
	    CodeChunk::word((int) l);
	    break;

# ifdef LARGENUM
	case 7:
	    CodeChunk::word((int) (l >> 40));
	    /* fall through */
	case 5:
	    CodeChunk::word((int) (l >> 24));
	    /* fall through */
# endif
	case 3:
	    CodeChunk::byte((int) (l >> 16));
	    CodeChunk::word((int) l);
	    break;

	case 1:
	    CodeChunk::byte((int) l);
	    break;
	}
	l = m->l.left->r.number;
	switch (sz) {
# ifdef LARGENUM
	case 8:
	    CodeChunk::word((int) (l >> 48));
	    /* fall through */
	case 6:
	    CodeChunk::word((int) (l >> 32));
	    /* fall through */
# endif
	case 4:
	    CodeChunk::word((int) (l >> 16));
	    /* fall through */
	case 2:
	    CodeChunk::word((int) l);
	    break;

# ifdef LARGENUM
	case 7:
	    CodeChunk::word((int) (l >> 40));
	    /* fall through */
	case 5:
	    CodeChunk::word((int) (l >> 24));
	    /* fall through */
# endif
	case 3:
	    CodeChunk::byte((int) (l >> 16));
	    CodeChunk::word((int) l);
	    break;

	case 1:
	    CodeChunk::byte((int) l);
	    break;
	}
	switch_table[i++].jump = JmpList::addr((JmpList *) NULL);
	m = m->r.right;
    } while (i < size);

    /*
     * generate code for body
     */
    stmt(n->r.right->r.right);

    /*
     * resolve jumps
     */
    if (size > n->mod) {
	/* default: across switch */
	switch_table[0].where = here;
    }
    for (i = 0; i < size; i++) {
	JmpList::resolve(switch_table[i].jump, switch_table[i].where);
    }
    AFREE(switch_table);
    switch_table = table;
}

/*
 * generate code for a string switch statement
 */
void Codegen::switchStr(Node *n)
{
    Node *m;
    int i, size;
    case_label *table;

    switchStart(n);
    CodeChunk::byte(SWITCH_STRING);
    m = n->l.left;
    size = n->mod;
    if (m->l.left == (Node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    CodeChunk::word(size);

    table = switch_table;
    switch_table = ALLOCA(case_label, size);
    switch_table[0].jump = JmpList::addr((JmpList *) NULL);
    i = 1;
    if (m->l.left->type == nil_node) {
	/*
	 * nil
	 */
	CodeChunk::byte(0);
	switch_table[i++].jump = JmpList::addr((JmpList *) NULL);
	m = m->r.right;
    } else {
	/* no 0 case */
	CodeChunk::byte(1);
    }
    while (i < size) {
	Int l;

	l = Control::defString(m->l.left->l.string);
	CodeChunk::byte((int) (l >> 16));
	CodeChunk::word((int) l);
	switch_table[i++].jump = JmpList::addr((JmpList *) NULL);
	m = m->r.right;
    }

    /*
     * generate code for body
     */
    stmt(n->r.right->r.right);

    /*
     * resolve jumps
     */
    if (size > n->mod) {
	/* default: across switch */
	switch_table[0].where = here;
    }
    for (i = 0; i < size; i++) {
	JmpList::resolve(switch_table[i].jump, switch_table[i].where);
    }
    AFREE(switch_table);
    switch_table = table;
}

/*
 * generate code for a statement
 */
void Codegen::stmt(Node *n)
{
    Node *m;
    JmpList *jlist, *j2list;
    Uint where;

    while (n != (Node *) NULL) {
	if (n->type == N_PAIR) {
	    m = n->l.left;
	    n = n->r.right;
	} else {
	    m = n;
	    n = (Node *) NULL;
	}
	switch (m->type) {
	case N_BLOCK:
	    if (m->mod == N_BREAK) {
		jlist = break_list;
		break_list = (JmpList *) NULL;
		stmt(m->l.left);
		if (break_list != (JmpList *) NULL) {
		    JmpList::resolve(break_list, here);
		}
		break_list = jlist;
	    } else {
		jlist = continue_list;
		continue_list = (JmpList *) NULL;
		stmt(m->l.left);
		if (continue_list != (JmpList *) NULL) {
		    JmpList::resolve(continue_list, here);
		}
		continue_list = jlist;
	    }
	    break;

	case N_BREAK:
	    while (m->mod > 0) {
		CodeChunk::instr(I_RETURN, 0);
		m->mod--;
	    }
	    break_list = JmpList::jump(I_JUMP, m->line, break_list);
	    break;

	case N_CASE:
	    switch_table[m->mod].where = here;
	    stmt(m->l.left);
	    break;

	case N_COMPOUND:
	    if (m->r.right != (Node *) NULL) {
		stmt(m->r.right);
	    }
	    stmt(m->l.left);
	    break;

	case N_CONTINUE:
	    while (m->mod > 0) {
		CodeChunk::instr(I_RETURN, 0);
		m->mod--;
	    }
	    continue_list = JmpList::jump(I_JUMP, m->line, continue_list);
	    break;

	case N_DO:
	    where = here;
	    stmt(m->r.right);
	    jlist = true_list;
	    true_list = (JmpList *) NULL;
	    cond(m->l.left, TRUE);
	    JmpList::resolve(true_list, where);
	    true_list = jlist;
	    break;

	case N_FOR:
	    if (m->r.right != (Node *) NULL) {
		jlist = JmpList::jump(I_JUMP, 0, (JmpList *) NULL);
		where = here;
		stmt(m->r.right);
		JmpList::resolve(jlist, here);
	    } else {
		/* empty loop body */
		where = here;
	    }
	    jlist = true_list;
	    true_list = (JmpList *) NULL;
	    cond(m->l.left, TRUE);
	    JmpList::resolve(true_list, where);
	    true_list = jlist;
	    break;

	case N_FOREVER:
	    where = here;
	    if (m->l.left != (Node *) NULL) {
		expr(m->l.left, TRUE);
	    }
	    stmt(m->r.right);
	    JmpList::resolve(JmpList::jump(I_JUMP, m->line, (JmpList *) NULL),
			     where);
	    break;

	case N_GOTO:
	    while (m->mod > 0) {
		CodeChunk::instr(I_RETURN, 0);
		m->mod--;
	    }
	    goto_list = JmpList::jump(I_JUMP, m->line, goto_list);
	    goto_list->label = m->r.right;
	    break;

	case N_LABEL:
	    m->mod = here;
	    break;

	case N_RLIMITS:
	    expr(m->l.left->l.left, FALSE);
	    expr(m->l.left->r.right, FALSE);
	    CodeChunk::instr(I_RLIMITS, 0);
	    CodeChunk::byte(m->mod);
	    stmt(m->r.right);
	    if (!(m->flags & F_END)) {
		CodeChunk::instr(I_RETURN, 0);
	    }
	    break;

	case N_CATCH:
	    jlist = JmpList::jump((m->mod) ? I_CATCH | I_POP_BIT : I_CATCH, 0,
				  (JmpList *) NULL);
	    if (++caught > max) {
		max = caught;
	    }
	    stmt(m->l.left);
	    --caught;
	    if (m->l.left->flags & F_END) {
		JmpList::resolve(jlist, here);
		if (m->r.right != (Node *) NULL) {
		    stmt(m->r.right);
		}
	    } else {
		CodeChunk::instr(I_RETURN, 0);
		if (m->r.right != (Node *) NULL) {
		    j2list = JmpList::jump(I_JUMP, 0, (JmpList *) NULL);
		    JmpList::resolve(jlist, here);
		    stmt(m->r.right);
		    JmpList::resolve(j2list, here);
		} else {
		    JmpList::resolve(jlist, here);
		}
	    }
	    break;

	case N_IF:
	    if (m->r.right->l.left != (Node *) NULL &&
		m->r.right->l.left->mod == 0) {
		if (m->r.right->l.left->type == N_BREAK) {
		    jlist = true_list;
		    true_list = break_list;
		    cond(m->l.left, TRUE);
		    break_list = true_list;
		    true_list = jlist;
		    if (m->r.right->r.right != (Node *) NULL) {
			/* else */
			stmt(m->r.right->r.right);
		    }
		    break;
		} else if (m->r.right->l.left->type == N_CONTINUE) {
		    jlist = true_list;
		    true_list = continue_list;
		    cond(m->l.left, TRUE);
		    continue_list = true_list;
		    true_list = jlist;
		    if (m->r.right->r.right != (Node *) NULL) {
			/* else */
			stmt(m->r.right->r.right);
		    }
		    break;
		}
	    }
	    jlist = false_list;
	    false_list = (JmpList *) NULL;
	    cond(m->l.left, FALSE);
	    stmt(m->r.right->l.left);
	    if (m->r.right->r.right != (Node *) NULL) {
		/* else */
		if (m->r.right->l.left != (Node *) NULL &&
		    (m->r.right->l.left->flags & F_END)) {
		    JmpList::resolve(false_list, here);
		    false_list = jlist;
		    stmt(m->r.right->r.right);
		} else {
		    j2list = JmpList::jump(I_JUMP, 0, (JmpList *) NULL);
		    JmpList::resolve(false_list, here);
		    false_list = jlist;
		    stmt(m->r.right->r.right);
		    JmpList::resolve(j2list, here);
		}
	    } else {
		/* no else */
		JmpList::resolve(false_list, here);
		false_list = jlist;
	    }
	    break;

	case N_PAIR:
	    stmt(m);
	    break;

	case N_POP:
	    expr(m->l.left, TRUE);
	    break;

	case N_RETURN:
	    expr(m->l.left, FALSE);
	    while (m->mod > 0) {
		CodeChunk::instr(I_RETURN, 0);
		m->mod--;
	    }
	    CodeChunk::instr(I_RETURN, m->line);
	    break;

	case N_SWITCH_INT:
	    switchInt(m);
	    break;

	case N_SWITCH_RANGE:
	    switchRange(m);
	    break;

	case N_SWITCH_STR:
	    switchStr(m);
	    break;
	}
    }
}


static int nfuncs;		/* # functions generated */

/*
 * initialize the code generator
 */
void Codegen::init(int inherited)
{
    UNREFERENCED_PARAMETER(inherited);
    kd_allocate = ((LPCint) KFCALL << 24) | KFun::kfunc("allocate");
    kd_allocate_int = ((LPCint) KFCALL << 24) | KFun::kfunc("allocate_int");
    kd_allocate_float = ((LPCint) KFCALL << 24) | KFun::kfunc("allocate_float");
    ::nfuncs = 0;
}

/*
 * generate code for a function
 */
char *Codegen::function(String *fname, Node *n, int nvar, int npar,
			unsigned int depth, unsigned short *size)
{
    char *prog;

    UNREFERENCED_PARAMETER(fname);

    nparams = npar;
    stmt(n);
    prog = CodeChunk::make(depth + nvar - npar, nvar - npar, size);
    JmpList::make(prog + 7);
    ::nfuncs++;

    return prog;
}

/*
 * return the number of functions generated
 */
int Codegen::nfuncs()
{
    return ::nfuncs;
}

/*
 * clean up code generator
 */
void Codegen::clear()
{
    JmpList::clear();
    CodeChunk::clear();
}
