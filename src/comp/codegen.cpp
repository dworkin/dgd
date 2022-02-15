/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2022 DGD Authors (see the commit log for details)
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
# include "interpret.h"
# include "data.h"
# include "table.h"
# include "node.h"
# include "control.h"
# include "codegen.h"
# include "compile.h"

# define LINE_CHUNK	128

struct linechunk {
    linechunk *next;			/* next in list */
    char info[LINE_CHUNK];		/* chunk of line number info */
};

static linechunk *lline, *tline;		/* line chunk list */
static linechunk *fline;			/* free line chunk list */
static unsigned int lchunksz = LINE_CHUNK;	/* line chunk size */
static unsigned short line;			/* current line number */
static unsigned int line_info_size;		/* size of all line info */

/*
 * NAME:	line->byte()
 * DESCRIPTION:	output a line description byte
 */
static void line_byte(int byte)
{
    if (lchunksz == LINE_CHUNK) {
	linechunk *l;

	/* new chunk */
	if (fline != (linechunk *) NULL) {
	    /* from free list */
	    l = fline;
	    fline = l->next;
	} else {
	    l = ALLOC(linechunk, 1);
	}
	l->next = (linechunk *) NULL;
	if (tline != (linechunk *) NULL) {
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
 * NAME:	line->fix()
 * DESCRIPTION:	Fix the new line number.  Return 0 .. 2 for simple offsets,
 *		3 for special ones.
 */
static int line_fix(unsigned short newline)
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
	line_byte(offset + 128 + 64);
    } else {
	/* two byte offset */
	offset += 16384;
	line_byte((offset >> 8) & 127);
	line_byte(offset);
    }
    line = newline;
    return 3;
}

/*
 * NAME:	line->make()
 * DESCRIPTION:	put line number info after the function
 */
static void line_make(char *buf)
{
    linechunk *l;

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
    lline = (linechunk *) NULL;
    tline = (linechunk *) NULL;
    lchunksz = LINE_CHUNK;
    line = 0;
    line_info_size = 0;
}

/*
 * NAME:	line->clear()
 * DESCRIPTION:	clean up line number info chunks
 */
static void line_clear()
{
    linechunk *l, *f;

    for (l = fline; l != (linechunk *) NULL; ) {
	f = l;
	l = l->next;
	FREE(f);
    }
    fline = (linechunk *) NULL;
}


# define CODE_CHUNK	128

struct codechunk {
    codechunk *next;			/* next in list */
    char code[CODE_CHUNK];		/* chunk of code */
};

static codechunk *lcode, *tcode;		/* code chunk list */
static codechunk *fcode;			/* free code chunk list */
static unsigned int cchunksz = CODE_CHUNK;	/* code chunk size */
static Uint here;				/* current offset */
static char *last_instruction;			/* last instruction's address */

/*
 * NAME:	code->byte()
 * DESCRIPTION:	output a byte of code
 */
static void code_byte(char byte)
{
    if (cchunksz == CODE_CHUNK) {
	codechunk *l;

	/* new chunk */
	if (fcode != (codechunk *) NULL) {
	    /* from free list */
	    l = fcode;
	    fcode = l->next;
	} else {
	    l = ALLOC(codechunk, 1);
	}
	l->next = (codechunk *) NULL;
	if (tcode != (codechunk *) NULL) {
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
 * NAME:	code->word()
 * DESCRIPTION:	output a word of code
 */
static void code_word(unsigned short word)
{
    code_byte(word >> 8);
    code_byte(word);
}

/*
 * NAME:	code->instr()
 * DESCRIPTION:	generate an instruction code
 */
static void code_instr(int i, unsigned short line)
{
    code_byte(i | (line_fix(line) << I_LINE_SHIFT));
    last_instruction = &tcode->code[cchunksz - 1];
}

/*
 * NAME:	code->kfun()
 * DESCRIPTION:	generate code for a builtin kfun
 */
static void code_kfun(int kf, unsigned short line)
{
    if (kf < 256) {
	code_instr(I_CALL_KFUNC, line);
	code_byte(kf);
    } else {
	code_instr(I_CALL_EFUNC, line);
	code_word(kf);
    }
}

/*
 * NAME:	code->ckfun()
 * DESCRIPTION:	generate code for a builtin kfun
 */
static void code_ckfun(int kf, unsigned short line)
{
    if (kf < 256) {
	code_instr(I_CALL_CKFUNC, line);
	code_byte(kf);
    } else {
	code_instr(I_CALL_CEFUNC, line);
	code_word(kf);
    }
}

/*
 * NAME:	code->make()
 * DESCRIPTION:	create function code block
 */
static char *code_make(unsigned short depth, int nlocals, unsigned short *size)
{
    codechunk *l;
    char *code;
    Uint sz;

    *size = sz = 5 + here + line_info_size;

    if (sz > USHRT_MAX) {
	c_error("function too large");
    }
    code = ALLOC(char, sz);
    *code++ = depth >> 8;
    *code++ = depth;
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
    line_make(code);
    code -= 5 + here;

    /* add blocks to free list */
    tcode->next = fcode;
    fcode = lcode;
    /* clear blocks */
    lcode = (codechunk *) NULL;
    tcode = (codechunk *) NULL;
    cchunksz = CODE_CHUNK;

    here = 0;
    return code;
}

/*
 * NAME:	code->clear()
 * DESCRIPTION:	clean up the code chunks
 */
static void code_clear()
{
    codechunk *l, *f;

    for (l = fcode; l != (codechunk *) NULL; ) {
	f = l;
	l = l->next;
	FREE(f);
    }
    fcode = (codechunk *) NULL;

    line_clear();
}


# define JUMP_CHUNK	128

struct jmplist {
    Uint where;				/* where to jump from */
    Uint to;				/* where to jump to */
    node *label;			/* label to jump to */
    jmplist *next;			/* next in list */
};

static class jmpchunk : public Chunk<jmplist, JUMP_CHUNK> {
public:
    /*
     * NAME:		item()
     * DESCRIPTION:	resolve jumps when iterating through items
     */
    virtual bool item(jmplist *j) {
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
     * NAME:		mkjumps()
     * DESCRIPTION:	resolve jumps, and return list of jumps to jumps
     */
    jmplist *mkjumps(char *prog) {
	jmpjmp = NULL;
	code = prog;
	items();
	return jmpjmp;
    }

private:
    jmplist *jmpjmp;			/* list of jumps to jumps */
    char *code;				/* program code */
} jchunk;

static jmplist *true_list;		/* list of true jumps */
static jmplist *false_list;		/* list of false jumps */
static jmplist *break_list;		/* list of break jumps */
static jmplist *continue_list;		/* list of continue jumps */
static jmplist *goto_list;		/* list of goto jumps */

/*
 * NAME:	jump->addr()
 * DESCRIPTION:	generate a jump
 */
static jmplist *jump_addr(jmplist *list)
{
    jmplist *j;

    j = jchunk.alloc();
    j->where = here;
    j->next = list;
    code_word(0);	/* empty space in code block filled in later */

    return j;
}

/*
 * NAME:	jump()
 * DESCRIPTION:	create a jump
 */
static jmplist *jump(int i, unsigned short line, jmplist *list)
{
    code_instr(i, line);
    return jump_addr(list);
}

/*
 * NAME:	jump->resolve()
 * DESCRIPTION:	resolve all jumps in a jump list
 */
static void jump_resolve(jmplist *list, Uint to)
{
    while (list != (jmplist *) NULL) {
	list->to = to;
	list = list->next;
    }
}

/*
 * NAME:	jump->make()
 * DESCRIPTION:	fill in all jumps in a code block
 */
static void jump_make(char *code)
{
    jmplist *j;

    for (j = goto_list; j != (jmplist *) NULL; j = j->next) {
	j->to = j->label->mod;
    }

    for (j = jchunk.mkjumps(code); j != (jmplist *) NULL; j = j->next) {
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
 * NAME:	jump->clear()
 * DESCRIPTION:	clean up the jump chunks
 */
static void jump_clear()
{
    goto_list = (jmplist *) NULL;
    jchunk.clean();
}


static void cg_expr (node*, int);
static void cg_cond (node*, int);
static void cg_stmt (node*);

static Int kd_allocate, kd_allocate_int, kd_allocate_float;
static int nparams;		/* number of parameters */

/*
 * NAME:	codegen->type()
 * DESCRIPTION:	return the type of a node
 */
static int cg_type(node *n, long *l)
{
    int type;

    type = n->mod;
    if ((type & T_REF) != 0) {
	type = T_ARRAY;
    }
    if (type == T_CLASS) {
	*l = ctrl_dstring(n->sclass);
    }
    return (type != T_MIXED) ? type : 0;
}

/*
 * NAME:	codegen->cast()
 * DESCRIPTION:	generate code for a cast
 */
static void cg_cast(node *n)
{
    int type;
    long l;

    type = cg_type(n, &l);
    if (type != 0) {
	code_instr(I_CAST, 0);
	code_byte(type);
	if (type == T_CLASS) {
	    code_byte(l >> 16);
	    code_word(l);
	}
    }
}

/*
 * NAME:	codegen->lvalue()
 * DESCRIPTION:	generate code for an lvalue
 */
static int cg_lvalue(node *n, int fetch)
{
    int stack;
    node *m, *l;

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
	    cg_expr(l->l.left, FALSE);
	    cg_expr(l->r.right, FALSE);
	    code_instr(I_INDEX2, l->line);
	    stack += 2;
	} else {
	    cg_expr(l, FALSE);
	}
	if (m->l.left->type == N_CAST) {
	    cg_cast(m->l.left);
	}
	stack += 2;
	cg_expr(m->r.right, FALSE);
	if (fetch) {
	    code_instr(I_INDEX2, n->line);
	    if (n->type == N_CAST) {
		cg_cast(n);
	    }
	    stack++;
	}
    } else if (fetch) {
	cg_expr(n, FALSE);
	stack++;
    }
    return stack;
}

/*
 * NAME:	codegen->store()
 * DESCRIPTION:	generate code for a store
 */
static void cg_store(node *n)
{
    if (n->type == N_CAST) {
	n = n->l.left;
    }
    switch (n->type) {
    case N_LOCAL:
	code_instr(I_STORE_LOCAL, n->line);
	code_byte(nparams - (int) n->r.number - 1);
	break;

    case N_GLOBAL:
	if ((n->r.number >> 8) == ctrl_ninherits()) {
	    code_instr(I_STORE_GLOBAL, n->line);
	} else {
	    code_instr(I_STORE_FAR_GLOBAL, n->line);
	    code_byte(n->r.number >> 8);
	}
	code_byte(n->r.number);
	break;

    case N_INDEX:
	n = n->l.left;
	if (n->type == N_CAST) {
	    n = n->l.left;
	}
	switch (n->type) {
	case N_LOCAL:
	    code_instr(I_STORE_LOCAL_INDEX, n->line);
	    code_byte(nparams - (int) n->r.number - 1);
	    break;

	case N_GLOBAL:
	    if ((n->r.number >> 8) == ctrl_ninherits()) {
		code_instr(I_STORE_GLOBAL_INDEX, n->line);
	    } else {
		code_instr(I_STORE_FAR_GLOBAL_INDEX, n->line);
		code_byte(n->r.number >> 8);
	    }
	    code_byte(n->r.number);
	    break;

	case N_INDEX:
	    code_instr(I_STORE_INDEX_INDEX, n->line);
	    break;

	default:
	    code_instr(I_STORE_INDEX, n->line);
	    break;
	}
	break;
    }
}

/*
 * NAME:	codegen->asgnop()
 * DESCRIPTION:	generate code for an assignment operator
 */
static void cg_asgnop(node *n, int op)
{
    cg_lvalue(n->l.left, TRUE);
    cg_expr(n->r.right, FALSE);
    code_kfun(op, n->line);
    cg_store(n->l.left);
}

/*
 * NAME:	codegen->aggr()
 * DESCRIPTION:	generate code for an aggregate
 */
static int cg_aggr(node *n)
{
    int i;

    if (n == (node *) NULL) {
	return 0;
    }
    for (i = 1; n->type == N_PAIR; i++) {
	cg_expr(n->l.left, FALSE);
	n = n->r.right;
    }
    cg_expr(n, FALSE);
    return i;
}

/*
 * NAME:	codegen->map_aggr()
 * DESCRIPTION:	generate code for a mapping aggregate
 */
static int cg_map_aggr(node *n)
{
    int i;

    if (n == (node *) NULL) {
	return 0;
    }
    for (i = 2; n->type == N_PAIR; i += 2) {
	cg_expr(n->l.left->l.left, FALSE);
	cg_expr(n->l.left->r.right, FALSE);
	n = n->r.right;
    }
    cg_expr(n->l.left, FALSE);
    cg_expr(n->r.right, FALSE);
    return i;
}

/*
 * NAME:	codegen->lval_aggr()
 * DESCRIPTION:	generate code for an lvalue aggregate
 */
static int cg_lval_aggr(node **l)
{
    int i;
    node *n, *m;

    i = 1;
    n = *l;
    if (n->type == N_PAIR) {
	for (m = n->l.left; ; m = n->l.left->r.right) {
	    cg_lvalue(m, FALSE);
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
    cg_lvalue(n, FALSE);

    return i;
}

/*
 * NAME:	CodeGen->store_aggr()
 * DESCRIPTION:	generate stores for an lvalue aggregate
 */
static void cg_store_aggr(node *n)
{
    while (n->type == N_PAIR) {
	cg_cast(n->r.right);
	cg_store(n->r.right);
	n = n->l.left;
    }
    cg_cast(n);
    cg_store(n);
}

/*
 * NAME:	codegen->sumargs()
 * DESCRIPTION:	generate code for summand arguments
 */
static int cg_sumargs(node *n)
{
    int i;
    node *m;

    if (n->type == N_SUM) {
	i = cg_sumargs(n->l.left);
	n = n->r.right;
    } else {
	i = 0;
    }

    switch (n->type) {
    case N_AGGR:
	n->type = N_INT;
	n->l.number = SUM_AGGREGATE - cg_aggr(n->l.left);
	cg_expr(n, FALSE);
	break;

    case N_RANGE:
	cg_expr(n->l.left, FALSE);
	n = n->r.right;
	if (n->l.left != (node *) NULL) {
	    cg_expr(n->l.left, FALSE);
	    if (n->r.right != (node *) NULL) {
		cg_expr(n->r.right, FALSE);
		code_kfun(KF_CKRANGEFT, n->line);
	    } else {
		code_kfun(KF_CKRANGEF, n->line);
	    }
	} else if (n->r.right != (node *) NULL) {
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_CKRANGET, n->line);
	} else {
	    code_kfun(KF_RANGE, n->line);
	    code_instr(I_PUSH_INT1, 0);
	    code_byte(SUM_SIMPLE);
	}
	break;

    case N_FUNC:
	m = n->l.left->r.right;
	if (m != (node *) NULL && m->type != N_PAIR && m->type != N_SPREAD &&
	    m->mod == T_INT) {
	    if (n->r.number == kd_allocate) {
		cg_expr(m, FALSE);
		code_instr(I_PUSH_INT1, 0);
		code_byte(SUM_ALLOCATE_NIL);
		break;
	    } else if (n->r.number == kd_allocate_int) {
		cg_expr(m, FALSE);
		code_instr(I_PUSH_INT1, 0);
		code_byte(SUM_ALLOCATE_INT);
		break;
	    } else if (n->r.number == kd_allocate_float) {
		cg_expr(m, FALSE);
		code_instr(I_PUSH_INT1, 0);
		code_byte(SUM_ALLOCATE_FLT);
		break;
	    }
	}
	/* fall through */
    default:
	cg_expr(n, FALSE);
	code_instr(I_PUSH_INT1, 0);
	code_byte(SUM_SIMPLE);		/* no range */
	break;
    }

    return i + 1;
}

/*
 * NAME:	codegen->funargs()
 * DESCRIPTION:	generate code for function arguments
 */
static int cg_funargs(node **l, int *nargs, bool *spread)
{
    node *n, *m;
    int stack;

    *spread = FALSE;
    n = *l;
    *l = (node *) NULL;
    *nargs = 0;
    if (n == (node *) NULL) {
	return 0;
    }

    stack = 0;
    while (n->type == N_PAIR) {
	m = n;
	n = n->r.right;
	if (m->l.left->type == N_LVALUE) {
	    stack += cg_lvalue(m->l.left->l.left, FALSE);
	    if (*l != (node *) NULL) {
		(*l)->r.right = m->l.left;
		m->l.left = *l;
	    }
	    *l = m;
	    (*nargs)++;
	} else {
	    cg_expr(m->l.left, FALSE);
	    stack++;
	}
    }
    if (n->type == N_SPREAD) {
	cg_expr(n->l.left, FALSE);
	stack++;
	code_instr(I_SPREAD, n->line);
	code_byte(-(short) n->mod - 2);
	if ((short) n->mod >= 0) {
	    if (*l == (node *) NULL) {
		*l = n;
	    }
	    (*nargs)++;
	}
	*spread = TRUE;
    } else if (n->type == N_LVALUE) {
	stack += cg_lvalue(n->l.left, FALSE);
	if (*l == (node *) NULL) {
	    *l = n;
	}
	(*nargs)++;
    } else {
	cg_expr(n, FALSE);
	stack++;
    }
    return stack;
}

/*
 * NAME:	codegen->storearg()
 * DESCRIPTION:	generate storage code for one lvalue argument
 */
static void cg_storearg(node *n)
{
    if (n->type == N_SPREAD) {
	int type;
	long l;

	code_instr(I_SPREAD, n->line);
	code_byte(n->mod);
	n = n->l.left;
	if (n->mod & T_REF) {
	    n->mod -= (1 << REFSHIFT);
	}
	type = cg_type(n, &l);
	code_byte(type);
	if (type == T_CLASS) {
	    code_byte(l >> 16);
	    code_word(l);
	}
    } else {
	/* N_LVALUE */
	cg_cast(n->l.left);
	cg_store(n->l.left);
    }
}

/*
 * NAME:	codegen->storeargs()
 * DESCRIPTION:	generate storage code for lvalue arguments
 */
static void cg_storeargs(node *n)
{
    while (n->type == N_PAIR) {
	cg_storearg(n->r.right);
	n = n->l.left;
    }
    cg_storearg(n);
}

/*
 * NAME:	codegen->expr()
 * DESCRIPTION:	generate code for an expression
 */
static void cg_expr(node *n, int pop)
{
    jmplist *jlist, *j2list;
    unsigned short i;
    long l;
    node *args;
    int nargs;
    bool spread;

    switch (n->type) {
    case N_ADD:
	cg_expr(n->l.left, FALSE);
	if (n->r.right->type == N_FLOAT) {
	    if (NFLT_ISONE(n->r.right)) {
		code_kfun(KF_ADD1, n->line);
		break;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		code_kfun(KF_SUB1, n->line);
		break;
	    }
	}
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_ADD, n->line);
	break;

    case N_ADD_INT:
	cg_expr(n->l.left, FALSE);
	if (n->r.right->type == N_INT) {
	    if (n->r.right->l.number == 1) {
		code_kfun(KF_ADD1_INT, n->line);
		break;
	    }
	    if (n->r.right->l.number == -1) {
		code_kfun(KF_SUB1_INT, n->line);
		break;
	    }
	}
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_ADD_INT, n->line);
	break;

    case N_ADD_FLOAT:
	cg_expr(n->l.left, FALSE);
	if (n->r.right->type == N_FLOAT) {
	    if (NFLT_ISONE(n->r.right)) {
		code_kfun(KF_ADD1_FLT, n->line);
		break;
	    }
	    if (NFLT_ISMONE(n->r.right)) {
		code_kfun(KF_SUB1_FLT, n->line);
		break;
	    }
	}
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_ADD_FLT, n->line);
	break;


    case N_ADD_EQ:
	cg_asgnop(n, KF_ADD);
	break;

    case N_ADD_EQ_INT:
	cg_asgnop(n, KF_ADD_INT);
	break;

    case N_ADD_EQ_FLOAT:
	cg_asgnop(n, KF_ADD_FLT);
	break;

    case N_ADD_EQ_1:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_ADD1, 0);
	cg_store(n->l.left);
	break;

    case N_ADD_EQ_1_INT:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_ADD1_INT, 0);
	cg_store(n->l.left);
	break;

    case N_ADD_EQ_1_FLOAT:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_ADD1_FLT, 0);
	cg_store(n->l.left);
	break;

    case N_AGGR:
	if (n->mod == T_MAPPING) {
	    i = cg_map_aggr(n->l.left);
	    code_instr(I_AGGREGATE, n->line);
	    code_byte(1);
	} else {
	    i = cg_aggr(n->l.left);
	    code_instr(I_AGGREGATE, n->line);
	    code_byte(0);
	}
	code_word(i);
	break;

    case N_AND:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_AND, n->line);
	break;

    case N_AND_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_AND_INT, n->line);
	break;

    case N_AND_EQ:
	cg_asgnop(n, KF_AND);
	break;

    case N_AND_EQ_INT:
	cg_asgnop(n, KF_AND_INT);
	break;

    case N_ASSIGN:
	if (n->l.left->type == N_AGGR) {
	    l = cg_lval_aggr(&n->l.left->l.left);
	    cg_expr(n->r.right, FALSE);
	    code_instr(I_STORES, n->line);
	    code_byte(l);
	    cg_store_aggr(n->l.left->l.left);
	} else {
	    cg_lvalue(n->l.left, FALSE);
	    cg_expr(n->r.right, FALSE);
	    cg_store(n->l.left);
	}
	break;

    case N_CAST:
	cg_expr(n->l.left, FALSE);
	cg_cast(n);
	break;

    case N_CATCH:
	jlist = jump((pop) ? I_CATCH | I_POP_BIT : I_CATCH, 0,
		     (jmplist *) NULL);
	cg_expr(n->l.left, TRUE);
	code_instr(I_RETURN, 0);
	jump_resolve(jlist, here);
	return;

    case N_COMMA:
	cg_expr(n->l.left, TRUE);
	cg_expr(n->r.right, pop);
	return;

    case N_DIV:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_DIV, n->line);
	break;

    case N_DIV_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_DIV_INT, n->line);
	break;

    case N_DIV_FLOAT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_DIV_FLT, n->line);
	break;

    case N_DIV_EQ:
	cg_asgnop(n, KF_DIV);
	break;

    case N_DIV_EQ_INT:
	cg_asgnop(n, KF_DIV_INT);
	break;

    case N_DIV_EQ_FLOAT:
	cg_asgnop(n, KF_DIV_FLT);
	break;

    case N_EQ:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_EQ, n->line);
	break;

    case N_EQ_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_EQ_INT, n->line);
	break;

    case N_EQ_FLOAT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_EQ_FLT, n->line);
	break;

    case N_EXCEPTION:
	cg_store(n->l.left);
	break;

    case N_FLOAT:
	code_instr(I_PUSH_FLOAT6, n->line);
	code_word(n->l.fhigh);
	code_word((int) (n->r.flow >> 16));
	code_word((int) n->r.flow);
	break;

    case N_FUNC:
	args = n->l.left->r.right;
	i = cg_funargs(&args, &nargs, &spread);
	switch (n->r.number >> 24) {
	case KFCALL:
	case KFCALL_LVAL:
	    if (PROTO_VARGS(KFUN((short) n->r.number).proto) != 0) {
		code_kfun((short) n->r.number, n->line);
		code_byte(i);
	    } else if (spread) {
		code_ckfun((short) n->r.number, n->line);
		code_byte(i);
	    } else {
		code_kfun((short) n->r.number, n->line);
	    }
	    if (pop) {
		*last_instruction |= I_POP_BIT;
	    }
	    if ((n->r.number >> 24) == KFCALL_LVAL) {
		/* generate stores */
		code_instr(I_STORES, n->line);
		code_byte(nargs);
		if (args != (node *) NULL) {
		    cg_storeargs(args);
		}
	    }
	    return;

	case DFCALL:
	    if ((n->r.number & 0xff00) == 0) {
		/* auto object */
		code_instr(I_CALL_AFUNC, n->line);
		code_byte((int) n->r.number);
	    } else {
		code_instr(I_CALL_DFUNC, n->line);
		code_word((int) n->r.number);
	    }
	    code_byte(i);
	    break;

	case FCALL:
	    code_instr(I_CALL_FUNC, n->line);
	    code_word(ctrl_gencall((long) n->r.number));
	    code_byte(i);
	    break;
	}
	break;

    case N_GE:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_GE, n->line);
	break;

    case N_GE_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_GE_INT, n->line);
	break;

    case N_GE_FLOAT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_GE_FLT, n->line);
	break;

    case N_GLOBAL:
	if ((n->r.number >> 8) == ctrl_ninherits()) {
	    code_instr(I_PUSH_GLOBAL, n->line);
	    code_byte((int) n->r.number);
	} else {
	    code_instr(I_PUSH_FAR_GLOBAL, n->line);
	    code_word((int) n->r.number);
	}
	break;

    case N_GT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_GT, n->line);
	break;

    case N_GT_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_GT_INT, n->line);
	break;

    case N_GT_FLOAT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_GT_FLT, n->line);
	break;

    case N_INDEX:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_instr(I_INDEX, n->line);
	break;

    case N_INSTANCEOF:
	cg_expr(n->l.left, FALSE);
	code_instr(I_INSTANCEOF, n->line);
	l = ctrl_dstring(n->r.right->l.string) & 0xffffffL;
	code_byte(l >> 16);
	code_word(l);
	break;

    case N_INT:
	if (n->l.number >= -128 && n->l.number <= 127) {
	    code_instr(I_PUSH_INT1, n->line);
	    code_byte((int) n->l.number);
	} else {
	    code_instr(I_PUSH_INT4, n->line);
	    code_word((int) (n->l.number >> 16));
	    code_word((int) n->l.number);
	}
	break;

    case N_LAND:
	if (!pop) {
	    jlist = true_list;
	    true_list = (jmplist *) NULL;
	    cg_cond(n, TRUE);
	    code_instr(I_PUSH_INT1, 0);
	    code_byte(0);
	    j2list = jump(I_JUMP, 0, (jmplist *) NULL);
	    jump_resolve(true_list, here);
	    true_list = jlist;
	    code_instr(I_PUSH_INT1, 0);
	    code_byte(1);
	    jump_resolve(j2list, here);
	} else {
	    jlist = false_list;
	    false_list = (jmplist *) NULL;
	    cg_cond(n->l.left, FALSE);
	    cg_expr(n->r.right, TRUE);
	    jump_resolve(false_list, here);
	    false_list = jlist;
	}
	return;

    case N_LE:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_LE, n->line);
	break;

    case N_LE_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_LE_INT, n->line);
	break;

    case N_LE_FLOAT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_LE_FLT, n->line);
	break;

    case N_LOCAL:
	code_instr(I_PUSH_LOCAL, n->line);
	code_byte(nparams - (int) n->r.number - 1);
	break;

    case N_LOR:
	if (!pop) {
	    jlist = false_list;
	    false_list = (jmplist *) NULL;
	    cg_cond(n, FALSE);
	    code_instr(I_PUSH_INT1, 0);
	    code_byte(1);
	    j2list = jump(I_JUMP, 0, (jmplist *) NULL);
	    jump_resolve(false_list, here);
	    false_list = jlist;
	    code_instr(I_PUSH_INT1, 0);
	    code_byte(0);
	    jump_resolve(j2list, here);
	} else {
	    jlist = true_list;
	    true_list = (jmplist *) NULL;
	    cg_cond(n->l.left, TRUE);
	    cg_expr(n->r.right, TRUE);
	    jump_resolve(true_list, here);
	    true_list = jlist;
	}
	return;

    case N_LSHIFT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_LSHIFT, n->line);
	break;

    case N_LSHIFT_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_LSHIFT_INT, n->line);
	break;

    case N_LSHIFT_EQ:
	cg_asgnop(n, KF_LSHIFT);
	break;

    case N_LSHIFT_EQ_INT:
	cg_asgnop(n, KF_LSHIFT_INT);
	break;

    case N_LT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_LT, n->line);
	break;

    case N_LT_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_LT_INT, n->line);
	break;

    case N_LT_FLOAT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_LT_FLT, n->line);
	break;

    case N_MOD:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_MOD, n->line);
	break;

    case N_MOD_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_MOD_INT, n->line);
	break;

    case N_MOD_EQ:
	cg_asgnop(n, KF_MOD);
	break;

    case N_MOD_EQ_INT:
	cg_asgnop(n, KF_MOD_INT);
	break;

    case N_MULT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_MULT, n->line);
	break;

    case N_MULT_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_MULT_INT, n->line);
	break;

    case N_MULT_FLOAT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_MULT_FLT, n->line);
	break;

    case N_MULT_EQ:
	cg_asgnop(n, KF_MULT);
	break;

    case N_MULT_EQ_INT:
	cg_asgnop(n, KF_MULT_INT);
	break;

    case N_MULT_EQ_FLOAT:
	cg_asgnop(n, KF_MULT_FLT);
	break;

    case N_NE:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_NE, n->line);
	break;

    case N_NE_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_NE_INT, n->line);
	break;

    case N_NE_FLOAT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_NE_FLT, n->line);
	break;

    case N_NEG:
	cg_expr(n->l.left, FALSE);
	code_kfun(KF_NEG, n->line);
	break;

    case N_NIL:
	code_kfun(KF_NIL, n->line);
	break;

    case N_NOT:
	cg_expr(n->l.left, FALSE);
	code_kfun((n->l.left->mod == T_INT) ? KF_NOT_INT : KF_NOT, n->line);
	break;

    case N_OR:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_OR, n->line);
	break;

    case N_OR_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_OR_INT, n->line);
	break;

    case N_OR_EQ:
	cg_asgnop(n, KF_OR);
	break;

    case N_OR_EQ_INT:
	cg_asgnop(n, KF_OR_INT);
	break;

    case N_QUEST:
	if (n->r.right->l.left != (node *) NULL) {
	    jlist = false_list;
	    false_list = (jmplist *) NULL;
	    cg_cond(n->l.left, FALSE);
	    cg_expr(n->r.right->l.left, pop);
	    if (n->r.right->r.right != (node *) NULL) {
		j2list = jump(I_JUMP, 0, (jmplist *) NULL);
		jump_resolve(false_list, here);
		false_list = jlist;
		cg_expr(n->r.right->r.right, pop);
		jump_resolve(j2list, here);
	    } else {
		jump_resolve(false_list, here);
		false_list = jlist;
	    }
	} else {
	    jlist = true_list;
	    true_list = (jmplist *) NULL;
	    cg_cond(n->l.left, TRUE);
	    if (n->r.right->r.right != (node *) NULL) {
		cg_expr(n->r.right->r.right, pop);
	    }
	    jump_resolve(true_list, here);
	    true_list = jlist;
	}
	return;

    case N_RANGE:
	cg_expr(n->l.left, FALSE);
	n = n->r.right;
	if (n->l.left != (node *) NULL) {
	    cg_expr(n->l.left, FALSE);
	    if (n->r.right != (node *) NULL) {
		cg_expr(n->r.right, FALSE);
		code_kfun(KF_RANGEFT, n->line);
	    } else {
		code_kfun(KF_RANGEF, n->line);
	    }
	} else if (n->r.right != (node *) NULL) {
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_RANGET, n->line);
	} else {
	    code_kfun(KF_RANGE, n->line);
	}
	break;

    case N_RSHIFT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_RSHIFT, n->line);
	break;

    case N_RSHIFT_INT:
	cg_expr(n->l.left, FALSE);
	cg_expr(n->r.right, FALSE);
	code_kfun(KF_RSHIFT_INT, n->line);
	break;

    case N_RSHIFT_EQ:
	cg_asgnop(n, KF_RSHIFT);
	break;

    case N_RSHIFT_EQ_INT:
	cg_asgnop(n, KF_RSHIFT_INT);
	break;

    case N_STR:
	l = ctrl_dstring(n->l.string);
	if ((l & 0x01000000L) && (unsigned short) l < 256) {
	    code_instr(I_PUSH_STRING, n->line);
	    code_byte((int) l);
	} else if ((unsigned short) l < 256) {
	    code_instr(I_PUSH_NEAR_STRING, n->line);
	    code_byte((int) (l >> 16));
	    code_byte((int) l);
	} else {
	    code_instr(I_PUSH_FAR_STRING, n->line);
	    code_byte((int) (l >> 16));
	    code_word((int) l);
	}
	break;

    case N_SUB:
	if ((n->l.left->type == N_INT && n->l.left->l.number == 0) ||
	    (n->l.left->type == N_FLOAT && NFLT_ISZERO(n->l.left))) {
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_UMIN, n->line);
	} else {
	    cg_expr(n->l.left, FALSE);
	    if (n->r.right->type == N_FLOAT) {
		if (NFLT_ISONE(n->r.right)) {
		    code_kfun(KF_SUB1, n->line);
		    break;
		}
		if (NFLT_ISMONE(n->r.right)) {
		    code_kfun(KF_ADD1, n->line);
		    break;
		}
	    }
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_SUB, n->line);
	}
	break;

    case N_SUB_INT:
	if (n->l.left->type == N_INT && n->l.left->l.number == 0) {
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_UMIN_INT, n->line);
	} else {
	    cg_expr(n->l.left, FALSE);
	    if (n->r.right->type == N_INT) {
		if (n->r.right->l.number == 1) {
		    code_kfun(KF_SUB1_INT, n->line);
		    break;
		}
		if (n->r.right->l.number == -1) {
		    code_kfun(KF_ADD1_INT, n->line);
		    break;
		}
	    }
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_SUB_INT, n->line);
	}
	break;

    case N_SUB_FLOAT:
	if (n->l.left->type == N_FLOAT && n->l.left->l.number == 0) {
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_UMIN_FLT, n->line);
	} else {
	    cg_expr(n->l.left, FALSE);
	    if (n->r.right->type == N_FLOAT) {
		if (NFLT_ISONE(n->r.right)) {
		    code_kfun(KF_SUB1_FLT, n->line);
		    break;
		}
		if (NFLT_ISMONE(n->r.right)) {
		    code_kfun(KF_ADD1_FLT, n->line);
		    break;
		}
	    }
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_SUB_FLT, n->line);
	}
	break;

    case N_SUB_EQ:
	cg_asgnop(n, KF_SUB);
	break;

    case N_SUB_EQ_INT:
	cg_asgnop(n, KF_SUB_INT);
	break;

    case N_SUB_EQ_FLOAT:
	cg_asgnop(n, KF_SUB);
	break;

    case N_SUB_EQ_1:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_SUB1, 0);
	cg_store(n->l.left);
	break;

    case N_SUB_EQ_1_INT:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_SUB1_INT, 0);
	cg_store(n->l.left);
	break;

    case N_SUB_EQ_1_FLOAT:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_SUB1_FLT, 0);
	cg_store(n->l.left);
	break;


    case N_SUM:
	i = cg_sumargs(n);
	code_kfun(KF_SUM, 0);
	code_byte(i);
	break;

    case N_SUM_EQ:
	cg_lvalue(n->l.left, TRUE);
	code_instr(I_PUSH_INT1, 0);
	code_byte(SUM_SIMPLE);
	i = cg_sumargs(n->r.right) + 1;
	code_kfun(KF_SUM, 0);
	code_byte(i);
	cg_store(n->l.left);
	break;

    case N_TOFLOAT:
	cg_expr(n->l.left, FALSE);
	code_kfun(KF_TOFLOAT, n->line);
	break;

    case N_TOINT:
	cg_expr(n->l.left, FALSE);
	code_kfun(KF_TOINT, n->line);
	break;

    case N_TOSTRING:
	cg_expr(n->l.left, FALSE);
	code_kfun(KF_TOSTRING, n->line);
	break;

    case N_TST:
	cg_expr(n->l.left, FALSE);
	if( n->l.left->mod == T_INT ) {
	    code_kfun(KF_TST_INT, n->line);
	} else if( n->l.left->mod == T_FLOAT ){
	    code_kfun(KF_TST_FLT, n->line);
	} else {
	    code_kfun(KF_TST, n->line);
	}
	break;

    case N_UMIN:
	cg_expr(n->l.left, FALSE);
	code_kfun(KF_UMIN, n->line);
	break;

    case N_XOR:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    cg_expr(n->l.left, FALSE);
	    code_kfun(KF_NEG, n->line);
	} else {
	    cg_expr(n->l.left, FALSE);
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_XOR, n->line);
	}
	break;

    case N_XOR_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    cg_expr(n->l.left, FALSE);
	    code_kfun(KF_NEG_INT, n->line);
	} else {
	    cg_expr(n->l.left, FALSE);
	    cg_expr(n->r.right, FALSE);
	    code_kfun(KF_XOR_INT, n->line);
	}
	break;

    case N_XOR_EQ:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    cg_lvalue(n->l.left, TRUE);
	    code_kfun(KF_NEG, 0);
	    cg_store(n->l.left);
	} else {
	    cg_asgnop(n, KF_XOR);
	}
	break;

    case N_XOR_EQ_INT:
	if (n->r.right->type == N_INT && n->r.right->l.number == -1) {
	    cg_lvalue(n->l.left, TRUE);
	    code_kfun(KF_NEG_INT, 0);
	    cg_store(n->l.left);
	} else {
	    cg_asgnop(n, KF_XOR_INT);
	}
	break;

    case N_MIN_MIN:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_SUB1, 0);
	cg_store(n->l.left);
	if( n->mod == T_INT ) {
	    code_kfun(KF_ADD1_INT, 0);
	} else if( n->mod == T_FLOAT ) {
	    code_kfun(KF_ADD1_FLT, 0);
	} else {
	    code_kfun(KF_ADD1, 0);
	}
	break;

    case N_MIN_MIN_INT:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_SUB1_INT, 0);
	cg_store(n->l.left);
	code_kfun(KF_ADD1_INT, 0);
	break;

    case N_MIN_MIN_FLOAT:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_SUB1_FLT, 0);
	cg_store(n->l.left);
	code_kfun(KF_ADD1_FLT, 0);
	break;

    case N_PLUS_PLUS:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_ADD1, 0);
	cg_store(n->l.left);
	if( n->mod == T_INT ) {
	    code_kfun(KF_SUB1_INT, 0);
	} else if( n->mod == T_FLOAT ) {
	    code_kfun(KF_SUB1_FLT, 0);
	} else {
	    code_kfun(KF_SUB1, 0);
	}
	break;

    case N_PLUS_PLUS_INT:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_ADD1_INT, 0);
	cg_store(n->l.left);
	code_kfun(KF_SUB1_INT, 0);
	break;

    case N_PLUS_PLUS_FLOAT:
	cg_lvalue(n->l.left, TRUE);
	code_kfun(KF_ADD1_FLT, 0);
	cg_store(n->l.left);
	code_kfun(KF_SUB1_FLT, 0);
	break;

# ifdef DEBUG
    default:
	fatal("unknown expression type %d", n->type);
# endif
    }

    if (pop) {
	*last_instruction |= I_POP_BIT;
    }
}

/*
 * NAME:	codegen->cond()
 * DESCRIPTION:	generate code for a condition
 */
static void cg_cond(node *n, int jmptrue)
{
    jmplist *jlist;

    for (;;) {
	switch (n->type) {
	case N_INT:
	    if (jmptrue) {
		if (n->l.number != 0) {
		    true_list = jump(I_JUMP, 0, true_list);
		}
	    } else if (n->l.number == 0) {
		false_list = jump(I_JUMP, 0, false_list);
	    }
	    break;

	case N_LAND:
	    if (jmptrue) {
		jlist = false_list;
		false_list = (jmplist *) NULL;
		cg_cond(n->l.left, FALSE);
		cg_cond(n->r.right, TRUE);
		jump_resolve(false_list, here);
		false_list = jlist;
	    } else {
		cg_cond(n->l.left, FALSE);
		cg_cond(n->r.right, FALSE);
	    }
	    break;

	case N_LOR:
	    if (!jmptrue) {
		jlist = true_list;
		true_list = (jmplist *) NULL;
		cg_cond(n->l.left, TRUE);
		cg_cond(n->r.right, FALSE);
		jump_resolve(true_list, here);
		true_list = jlist;
	    } else {
		cg_cond(n->l.left, TRUE);
		cg_cond(n->r.right, TRUE);
	    }
	    break;

	case N_NOT:
	    jlist = true_list;
	    true_list = false_list;
	    false_list = jlist;
	    cg_cond(n->l.left, !jmptrue);
	    jlist = true_list;
	    true_list = false_list;
	    false_list = jlist;
	    break;

	case N_COMMA:
	    cg_expr(n->l.left, TRUE);
	    n = n->r.right;
	    continue;

	default:
	    cg_expr(n, FALSE);
	    if (jmptrue) {
		true_list = jump(I_JUMP_NONZERO, 0, true_list);
	    } else {
		false_list = jump(I_JUMP_ZERO, 0, false_list);
	    }
	    break;
	}
	break;
    }
}

struct case_label {
    Uint where;				/* where to jump to */
    jmplist *jump;			/* list of unresolved jumps */
};

static case_label *switch_table;	/* label table for current switch */

/*
 * NAME:	codegen->switch_start()
 * DESCRIPTION:	generate code for the start of a switch statement
 */
static void cg_switch_start(node *n)
{
    node *m;

    /*
     * initializers
     */
    m = n->r.right->r.right;
    if (m->type == N_BLOCK) {
	m = m->l.left;
    }
# ifdef DEBUG
    if (m->type != N_COMPOUND) {
	fatal("N_COMPOUND expected");
    }
# endif
    cg_stmt(m->r.right);
    m->r.right = NULL;

    /*
     * switch expression
     */
    cg_expr(n->r.right->l.left, FALSE);
    code_instr(I_SWITCH, 0);
}

/*
 * NAME:	codegen->switch_int()
 * DESCRIPTION:	generate single label code for a switch statement
 */
static void cg_switch_int(node *n)
{
    node *m;
    int i, size, sz;
    case_label *table;

    cg_switch_start(n);
    code_byte(SWITCH_INT);
    m = n->l.left;
    size = n->mod;
    sz = n->r.right->mod;
    if (m->l.left == (node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    code_word(size);
    code_byte(sz);

    table = switch_table;
    switch_table = ALLOCA(case_label, size);
    switch_table[0].jump = jump_addr((jmplist *) NULL);
    i = 1;
    do {
	Int l;

	l = m->l.left->l.number;
	switch (sz) {
	case 4:
	    code_word((int) (l >> 16));
	    /* fall through */
	case 2:
	    code_word((int) l);
	    break;

	case 3:
	    code_byte((int) (l >> 16));
	    code_word((int) l);
	    break;

	case 1:
	    code_byte((int) l);
	    break;
	}
	switch_table[i++].jump = jump_addr((jmplist *) NULL);
	m = m->r.right;
    } while (i < size);

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);

    /*
     * resolve jumps
     */
    if (size > n->mod) {
	/* default: across switch */
	switch_table[0].where = here;
    }
    for (i = 0; i < size; i++) {
	jump_resolve(switch_table[i].jump, switch_table[i].where);
    }
    AFREE(switch_table);
    switch_table = table;
}

/*
 * NAME:	codegen->switch_range()
 * DESCRIPTION:	generate range label code for a switch statement
 */
static void cg_switch_range(node *n)
{
    node *m;
    int i, size, sz;
    case_label *table;

    cg_switch_start(n);
    code_byte(SWITCH_RANGE);
    m = n->l.left;
    size = n->mod;
    sz = n->r.right->mod;
    if (m->l.left == (node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    code_word(size);
    code_byte(sz);

    table = switch_table;
    switch_table = ALLOCA(case_label, size);
    switch_table[0].jump = jump_addr((jmplist *) NULL);
    i = 1;
    do {
	Int l;

	l = m->l.left->l.number;
	switch (sz) {
	case 4:
	    code_word((int) (l >> 16));
	    /* fall through */
	case 2:
	    code_word((int) l);
	    break;

	case 3:
	    code_byte((int) (l >> 16));
	    code_word((int) l);
	    break;

	case 1:
	    code_byte((int) l);
	    break;
	}
	l = m->l.left->r.number;
	switch (sz) {
	case 4:
	    code_word((int) (l >> 16));
	    /* fall through */
	case 2:
	    code_word((int) l);
	    break;

	case 3:
	    code_byte((int) (l >> 16));
	    code_word((int) l);
	    break;

	case 1:
	    code_byte((int) l);
	    break;
	}
	switch_table[i++].jump = jump_addr((jmplist *) NULL);
	m = m->r.right;
    } while (i < size);

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);

    /*
     * resolve jumps
     */
    if (size > n->mod) {
	/* default: across switch */
	switch_table[0].where = here;
    }
    for (i = 0; i < size; i++) {
	jump_resolve(switch_table[i].jump, switch_table[i].where);
    }
    AFREE(switch_table);
    switch_table = table;
}

/*
 * NAME:	codegen->switch_str()
 * DESCRIPTION:	generate code for a string switch statement
 */
static void cg_switch_str(node *n)
{
    node *m;
    int i, size;
    case_label *table;

    cg_switch_start(n);
    code_byte(SWITCH_STRING);
    m = n->l.left;
    size = n->mod;
    if (m->l.left == (node *) NULL) {
	/* explicit default */
	m = m->r.right;
    } else {
	/* implicit default */
	size++;
    }
    code_word(size);

    table = switch_table;
    switch_table = ALLOCA(case_label, size);
    switch_table[0].jump = jump_addr((jmplist *) NULL);
    i = 1;
    if (m->l.left->type == nil_node) {
	/*
	 * nil
	 */
	code_byte(0);
	switch_table[i++].jump = jump_addr((jmplist *) NULL);
	m = m->r.right;
    } else {
	/* no 0 case */
	code_byte(1);
    }
    while (i < size) {
	Int l;

	l = ctrl_dstring(m->l.left->l.string);
	code_byte((int) (l >> 16));
	code_word((int) l);
	switch_table[i++].jump = jump_addr((jmplist *) NULL);
	m = m->r.right;
    }

    /*
     * generate code for body
     */
    cg_stmt(n->r.right->r.right);

    /*
     * resolve jumps
     */
    if (size > n->mod) {
	/* default: across switch */
	switch_table[0].where = here;
    }
    for (i = 0; i < size; i++) {
	jump_resolve(switch_table[i].jump, switch_table[i].where);
    }
    AFREE(switch_table);
    switch_table = table;
}

/*
 * NAME:	codegen->stmt()
 * DESCRIPTION:	generate code for a statement
 */
static void cg_stmt(node *n)
{
    node *m;
    jmplist *jlist, *j2list;
    Uint where;

    while (n != (node *) NULL) {
	if (n->type == N_PAIR) {
	    m = n->l.left;
	    n = n->r.right;
	} else {
	    m = n;
	    n = (node *) NULL;
	}
	switch (m->type) {
	case N_BLOCK:
	    if (m->mod == N_BREAK) {
		jlist = break_list;
		break_list = (jmplist *) NULL;
		cg_stmt(m->l.left);
		if (break_list != (jmplist *) NULL) {
		    jump_resolve(break_list, here);
		}
		break_list = jlist;
	    } else {
		jlist = continue_list;
		continue_list = (jmplist *) NULL;
		cg_stmt(m->l.left);
		if (continue_list != (jmplist *) NULL) {
		    jump_resolve(continue_list, here);
		}
		continue_list = jlist;
	    }
	    break;

	case N_BREAK:
	    while (m->mod > 0) {
		code_instr(I_RETURN, 0);
		m->mod--;
	    }
	    break_list = jump(I_JUMP, m->line, break_list);
	    break;

	case N_CASE:
	    switch_table[m->mod].where = here;
	    cg_stmt(m->l.left);
	    break;

	case N_COMPOUND:
	    if (m->r.right != (node *) NULL) {
		cg_stmt(m->r.right);
	    }
	    cg_stmt(m->l.left);
	    break;

	case N_CONTINUE:
	    while (m->mod > 0) {
		code_instr(I_RETURN, 0);
		m->mod--;
	    }
	    continue_list = jump(I_JUMP, m->line, continue_list);
	    break;

	case N_DO:
	    where = here;
	    cg_stmt(m->r.right);
	    jlist = true_list;
	    true_list = (jmplist *) NULL;
	    cg_cond(m->l.left, TRUE);
	    jump_resolve(true_list, where);
	    true_list = jlist;
	    break;

	case N_FOR:
	    if (m->r.right != (node *) NULL) {
		jlist = jump(I_JUMP, 0, (jmplist *) NULL);
		where = here;
		cg_stmt(m->r.right);
		jump_resolve(jlist, here);
	    } else {
		/* empty loop body */
		where = here;
	    }
	    jlist = true_list;
	    true_list = (jmplist *) NULL;
	    cg_cond(m->l.left, TRUE);
	    jump_resolve(true_list, where);
	    true_list = jlist;
	    break;

	case N_FOREVER:
	    where = here;
	    if (m->l.left != (node *) NULL) {
		cg_expr(m->l.left, TRUE);
	    }
	    cg_stmt(m->r.right);
	    jump_resolve(jump(I_JUMP, m->line, (jmplist *) NULL), where);
	    break;

	case N_GOTO:
	    while (m->mod > 0) {
		code_instr(I_RETURN, 0);
		m->mod--;
	    }
	    goto_list = jump(I_JUMP, m->line, goto_list);
	    goto_list->label = m->r.right;
	    break;

	case N_LABEL:
	    m->mod = here;
	    break;

	case N_RLIMITS:
	    cg_expr(m->l.left->l.left, FALSE);
	    cg_expr(m->l.left->r.right, FALSE);
	    code_instr(I_RLIMITS, 0);
	    code_byte(m->mod);
	    cg_stmt(m->r.right);
	    if (!(m->flags & F_END)) {
		code_instr(I_RETURN, 0);
	    }
	    break;

	case N_CATCH:
	    jlist = jump((m->mod) ? I_CATCH | I_POP_BIT : I_CATCH, 0,
			 (jmplist *) NULL);
	    cg_stmt(m->l.left);
	    if (m->l.left->flags & F_END) {
		jump_resolve(jlist, here);
		if (m->r.right != (node *) NULL) {
		    cg_stmt(m->r.right);
		}
	    } else {
		code_instr(I_RETURN, 0);
		if (m->r.right != (node *) NULL) {
		    j2list = jump(I_JUMP, 0, (jmplist *) NULL);
		    jump_resolve(jlist, here);
		    cg_stmt(m->r.right);
		    jump_resolve(j2list, here);
		} else {
		    jump_resolve(jlist, here);
		}
	    }
	    break;

	case N_IF:
	    if (m->r.right->l.left != (node *) NULL &&
		m->r.right->l.left->mod == 0) {
		if (m->r.right->l.left->type == N_BREAK) {
		    jlist = true_list;
		    true_list = break_list;
		    cg_cond(m->l.left, TRUE);
		    break_list = true_list;
		    true_list = jlist;
		    if (m->r.right->r.right != (node *) NULL) {
			/* else */
			cg_stmt(m->r.right->r.right);
		    }
		    break;
		} else if (m->r.right->l.left->type == N_CONTINUE) {
		    jlist = true_list;
		    true_list = continue_list;
		    cg_cond(m->l.left, TRUE);
		    continue_list = true_list;
		    true_list = jlist;
		    if (m->r.right->r.right != (node *) NULL) {
			/* else */
			cg_stmt(m->r.right->r.right);
		    }
		    break;
		}
	    }
	    jlist = false_list;
	    false_list = (jmplist *) NULL;
	    cg_cond(m->l.left, FALSE);
	    cg_stmt(m->r.right->l.left);
	    if (m->r.right->r.right != (node *) NULL) {
		/* else */
		if (m->r.right->l.left != (node *) NULL &&
		    (m->r.right->l.left->flags & F_END)) {
		    jump_resolve(false_list, here);
		    false_list = jlist;
		    cg_stmt(m->r.right->r.right);
		} else {
		    j2list = jump(I_JUMP, 0, (jmplist *) NULL);
		    jump_resolve(false_list, here);
		    false_list = jlist;
		    cg_stmt(m->r.right->r.right);
		    jump_resolve(j2list, here);
		}
	    } else {
		/* no else */
		jump_resolve(false_list, here);
		false_list = jlist;
	    }
	    break;

	case N_NIL:
	    cg_expr(m, FALSE);
	    break;

	case N_PAIR:
	    cg_stmt(m);
	    break;

	case N_POP:
	    cg_expr(m->l.left, TRUE);
	    break;

	case N_RETURN:
	    cg_expr(m->l.left, FALSE);
	    while (m->mod > 0) {
		code_instr(I_RETURN, 0);
		m->mod--;
	    }
	    code_instr(I_RETURN, m->line);
	    break;

	case N_SWITCH_INT:
	    cg_switch_int(m);
	    break;

	case N_SWITCH_RANGE:
	    cg_switch_range(m);
	    break;

	case N_SWITCH_STR:
	    cg_switch_str(m);
	    break;
	}
    }
}


static int nfuncs;		/* # functions generated */

/*
 * NAME:	codegen->init()
 * DESCRIPTION:	initialize the code generator
 */
void cg_init(int inherited)
{
    UNREFERENCED_PARAMETER(inherited);
    kd_allocate = ((Int) KFCALL << 24) | kf_func("allocate");
    kd_allocate_int = ((Int) KFCALL << 24) | kf_func("allocate_int");
    kd_allocate_float = ((Int) KFCALL << 24) | kf_func("allocate_float");
    nfuncs = 0;
}

/*
 * NAME:	codegen->function()
 * DESCRIPTION:	generate code for a function
 */
char *cg_function(String *fname, node *n, int nvar, int npar,
	unsigned int depth, unsigned short *size)
{
    char *prog;

    UNREFERENCED_PARAMETER(fname);

    nparams = npar;
    cg_stmt(n);
    prog = code_make(depth + nvar - npar, nvar - npar, size);
    jump_make(prog + 5);
    nfuncs++;

    return prog;
}

/*
 * NAME:	codegen->nfuncs()
 * DESCRIPTION:	return the number of functions generated
 */
int cg_nfuncs()
{
    return nfuncs;
}

/*
 * NAME:	codegen->clear()
 * DESCRIPTION:	clean up code generator
 */
void cg_clear()
{
    jump_clear();
    code_clear();
}
