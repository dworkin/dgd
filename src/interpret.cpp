/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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

# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "control.h"
# include "data.h"
# include "interpret.h"
# include "ext.h"
# include "table.h"

# ifdef DEBUG
# undef EXTRA_STACK
# define EXTRA_STACK  0
# endif


static Value stack[MIN_STACK];	/* initial stack */
static Frame topframe;		/* top frame */
static RLInfo rlim;		/* top rlimits info */
Frame *cframe;			/* current frame */
static char *creator;		/* creator function name */
static unsigned int clen;	/* creator function name length */
static bool stricttc;		/* strict typechecking */
static char ihash[INHASHSZ];	/* instanceof hashtable */

/*
 * initialize the interpreter
 */
void Frame::init(char *create, bool flag)
{
    topframe.oindex = OBJ_NONE;
    topframe.fp = topframe.sp = ::stack + MIN_STACK;
    topframe.stack = ::stack;
    ::rlim.maxdepth = 0;
    ::rlim.ticks = 0;
    ::rlim.nodepth = TRUE;
    ::rlim.noticks = TRUE;
    topframe.rlim = &::rlim;
    topframe.level = 0;
    topframe.atomic = FALSE;
    cframe = &topframe;

    creator = create;
    clen = strlen(create);
    stricttc = flag;

    Value::init(stricttc);
}

/*
 * check if there is room on the stack for new values; if not, make space
 */
void Frame::growStack(int size)
{
    if (sp < stack + size + MIN_STACK) {
	int spsize;
	Value *v, *stk;

	/*
	 * extend the local ::stack
	 */
	spsize = fp - sp;
	size = ALGN(spsize + size + MIN_STACK, 8);
	stk = ALLOC(Value, size);

	/* move stack values */
	v = stk + size;
	if (spsize != 0) {
	    memcpy(v - spsize, sp, spsize * sizeof(Value));
	}
	sp = v - spsize;

	/* replace old stack */
	if (sos) {
	    /* stack on stack: alloca'd */
	    AFREE(stack);
	    sos = FALSE;
	} else if (stack != ::stack) {
	    FREE(stack);
	}
	stack = stk;
	fp = stk + size;
    }
}

/*
 * push a value on the stack
 */
void Frame::pushValue(Value *v)
{
    Value *o;

    *--sp = *v;
    switch (v->type) {
    case T_STRING:
	v->string->ref();
	break;

    case T_OBJECT:
	if (DESTRUCTED(v)) {
	    /*
	     * can't wipe out the original, since it may be a value from a
	     * mapping
	     */
	    *sp = Value::nil;
	}
	break;

    case T_LWOBJECT:
	o = Dataspace::elts(v->array);
	if (o->type == T_OBJECT && DESTRUCTED(o)) {
	    /*
	     * can't wipe out the original, since it may be a value from a
	     * mapping
	     */
	    *sp = Value::nil;
	    break;
	}
	/* fall through */
    case T_ARRAY:
    case T_MAPPING:
	v->array->ref();
	break;
    }
}

/*
 * pop a number of values from the stack
 */
void Frame::pop(int n)
{
    while (--n >= 0) {
	(sp++)->del();
    }
}

/*
 * replace all occurrences of an object on the stack by nil
 */
void Frame::objDest(Object *obj)
{
    Frame *prev, *f;
    Uint index;
    Value *v;
    unsigned short n;

    index = obj->index;

    /* wipe out objects in stack frames */
    prev = this;
    for (;;) {
	f = prev;
	for (v = f->sp; v < f->fp; v++) {
	    switch (v->type) {
	    case T_OBJECT:
		if (v->oindex == index) {
		    *v = Value::nil;
		}
		break;

	    case T_LWOBJECT:
		if (v->array->elts[0].type == T_OBJECT &&
		    v->array->elts[0].oindex == index) {
		    v->array->del();
		    *v = Value::nil;
		}
		break;
	    }
	}

	prev = f->prev;
	if (prev == (Frame *) NULL) {
	    break;
	}
	if ((f->func->sclass & C_ATOMIC) && !prev->atomic) {
	    /*
	     * wipe out objects in arguments to atomic function call
	     */
	    for (n = f->nargs, v = prev->sp; n != 0; --n, v++) {
		switch (v->type) {
		case T_OBJECT:
		    if (v->oindex == index) {
			*v = Value::nil;
		    }
		    break;

		case T_LWOBJECT:
		    if (v->array->elts[0].type == T_OBJECT &&
			v->array->elts[0].oindex == index) {
			v->array->del();
			*v = Value::nil;
		    }
		    break;
		}
	    }
	    break;
	}
    }
}

/*
 * push a string constant on the stack
 */
void Frame::string(int inherit, unsigned int index)
{
    PUSH_STRVAL(this, p_ctrl->strconst(inherit, index));
}

/*
 * create an array on the stack
 */
void Frame::aggregate(unsigned int size)
{
    Array *a;

    if (size == 0) {
	a = Array::create(data, 0);
    } else {
	Value *elts;

	addTicks(size);
	a = Array::create(data, size);
	elts = a->elts + size;
	do {
	    *--elts = *sp++;
	} while (--size != 0);
	Dataspace::refImports(a);
    }
    PUSH_ARRVAL(this, a);
}

/*
 * create a mapping on the stack
 */
void Frame::mapAggregate(unsigned int size)
{
    Mapping *a;

    if (size == 0) {
	a = Mapping::create(data, 0);
    } else {
	Value *elts;

	addTicks(size);
	a = Mapping::create(data, size);
	elts = a->elts + size;
	do {
	    *--elts = *sp++;
	} while (--size != 0);
	try {
	    EC->push();
	    a->sort();
	    EC->pop();
	} catch (const char*) {
	    /* error in sorting, delete mapping and pass on error */
	    a->ref();
	    a->del();
	    EC->error((char *) NULL);
	}
	Dataspace::refImports(a);
    }
    PUSH_MAPVAL(this, a);
}

/*
 * push the values in an array on the stack, return the number of extra
 * arguments pushed
 */
int Frame::spread(int n)
{
    Array *a;
    int i;
    Value *v;

    if (sp->type != T_ARRAY) {
	EC->error("Spread of non-array");
    }
    a = sp->array;

    if (n < 0) {
	/* no lvalues */
	n = a->size;
	addTicks(n);
	sp++;
	growStack(n);
	for (i = 0, v = Dataspace::elts(a); i < n; i++, v++) {
	    pushValue(v);
	}
	a->del();

	return n - 1;
    } else {
	/* including lvalues */
	if (n > a->size) {
	    n = a->size;
	}
	addTicks(n);
	growStack(n);
	sp++;
	for (i = 0, v = Dataspace::elts(a); i < n; i++, v++) {
	    pushValue(v);
	}
	--sp;
	PUT_ARRVAL_NOREF(sp, a);

	return n;
    }
}

/*
 * push a global value on the stack
 */
Value *Frame::global(int inherit, int index)
{
    addTicks(4);
    inherit = UCHAR(ctrl->imap[p_index + inherit]);
    inherit = ctrl->inherits[inherit].varoffset;
    if (lwobj == (LWO *) NULL) {
	return data->variable(inherit + index);
    } else {
	return &lwobj->elts[2 + inherit + index];
    }
}

/*
 * index or indexed assignment
 */
void Frame::oper(LWO *lwobj, const char *op, int nargs, Value *var, Value *idx,
		 Value *val)
{
    pushValue(idx);
    if (nargs > 1) {
	pushValue(val);
    }
    if (!call((Object *) NULL, lwobj, op, strlen(op), TRUE, nargs)) {
	EC->error("Index on bad type");
    }

    *var = *sp++;
}

/*
 * index a value
 */
void Frame::index(Value *aval, Value *ival, Value *val, bool keep)
{
    int i;

    addTicks(2);
    switch (aval->type) {
    case T_STRING:
	if (ival->type != T_INT) {
	    EC->error("Non-numeric string index");
	}
	i = UCHAR(aval->string->text[aval->string->index(ival->number)]);
	if (!keep) {
	    aval->string->del();
	}
	PUT_INTVAL(val, i);
	return;

    case T_ARRAY:
	if (ival->type != T_INT) {
	    EC->error("Non-numeric array index");
	}
	*val = Dataspace::elts(aval->array)[aval->array->index(ival->number)];
	break;

    case T_MAPPING:
	*val = *dynamic_cast<Mapping *> (aval->array)->index(data, ival, NULL,
							     NULL);
	if (!keep) {
	    ival->del();
	}
	break;

    case T_LWOBJECT:
	oper(dynamic_cast<LWO *> (aval->array), "[]", 1, val, ival,
	     (Value *) NULL);
	if (!keep) {
	    ival->del();
	    aval->array->del();
	}
	return;

    default:
	EC->error("Index on bad type");
    }

    switch (val->type) {
    case T_STRING:
	val->string->ref();
	break;

    case T_OBJECT:
	if (DESTRUCTED(val)) {
	    *val = Value::nil;
	}
	break;

    case T_LWOBJECT:
	ival = Dataspace::elts(val->array);
	if (ival->type == T_OBJECT && DESTRUCTED(ival)) {
	    *val = Value::nil;
	    break;
	}
	/* fall through */
    case T_ARRAY:
    case T_MAPPING:
	val->array->ref();
	break;
    }

    if (!keep) {
	aval->array->del();
    }
}

/*
 * return the name of a class
 */
char *Frame::className(Uint sclass)
{
    return p_ctrl->strconst(sclass >> 16, sclass & 0xffff)->text;
}

/*
 * is an object an instance of the named program?
 */
int Frame::instanceOf(unsigned int oindex, char *prog, Uint hash)
{
    char *h;
    unsigned short i;
    Inherit *inh;
    Object *obj;
    Control *ctrl;

    /* first try hash table */
    obj = OBJR(oindex);
    if (!(obj->flags & O_MASTER)) {
	oindex = obj->master;
	obj = OBJR(oindex);
    }
    ctrl = obj->control();
    h = &ihash[((oindex << 2) ^ hash) % INHASHSZ];
    if (*h < ctrl->ninherits &&
	strcmp(OBJR(ctrl->inherits[UCHAR(*h)].oindex)->name, prog) == 0) {
	return (ctrl->inherits[UCHAR(*h)].priv) ? -1 : 1;	/* found it */
    }

    /* next, search for it the hard way */
    for (i = ctrl->ninherits, inh = ctrl->inherits + i; i != 0; ) {
	--i;
	--inh;
	if (strcmp(prog, OBJR(inh->oindex)->name) == 0) {
	    /* found it; update hashtable */
	    *h = i;
	    return (ctrl->inherits[i].priv) ? -1 : 1;
	}
    }
    return FALSE;
}

/*
 * is an object an instance of the named program?
 */
int Frame::instanceOf(unsigned int oindex, Uint sclass)
{
    return instanceOf(oindex, className(sclass), sclass);
}

/*
 * is an object on the stack an instance of the named program?
 */
int Frame::instanceOf(Uint sclass)
{
    int instance;

    switch (sp->type) {
    case T_OBJECT:
	instance = instanceOf(sp->oindex, sclass);
	break;

    case T_LWOBJECT:
	if (sp->array->elts->type != T_OBJECT) {
	    instance = (strcmp(Object::builtinName(sp->array->elts->number),
			       className(sclass)) == 0);
	} else {
	    instance = instanceOf(sp->array->elts->oindex, sclass);
	}
	sp->array->del();
	break;

    default:
	EC->error("Instance of bad type");
    }

    return instance;
}

/*
 * is an object an instance of the named program?
 */
int Frame::instanceOf(unsigned int oindex, char *prog)
{
    return instanceOf(oindex, prog, HM->hashstr(prog, OBJHASHSZ));
}

/*
 * cast a value to a type
 */
void Frame::cast(Value *val, unsigned int type, Uint sclass)
{
    char tnbuf[TNBUFSIZE];
    Value *elts;

    if (type == T_CLASS) {
	if (val->type == T_OBJECT) {
	    if (instanceOf(val->oindex, sclass) <= 0) {
		EC->error("Value is not of object type /%s", className(sclass));
	    }
	    return;
	} else if (val->type == T_LWOBJECT) {
	    elts = Dataspace::elts(val->array);
	    if (elts->type == T_OBJECT) {
		if (instanceOf(elts->oindex, sclass) <= 0) {
		    EC->error("Value is not of object type /%s",
			      className(sclass));
		}
	    } else if (strcmp(Object::builtinName(elts->number),
			      className(sclass)) != 0) {
		/*
		 * builtin types can only be cast to their own type
		 */
		EC->error("Value is not of object type /%s", className(sclass));
	    }
	    return;
	}
	type = T_OBJECT;
    }
    if (val->type != type && (val->type != T_LWOBJECT || type != T_OBJECT) &&
	(!VAL_NIL(val) || !T_POINTER(type))) {
	Value::typeName(tnbuf, type);
	if (strchr("aeiuoy", tnbuf[0]) != (char *) NULL) {
	    EC->error("Value is not an %s", tnbuf);
	} else {
	    EC->error("Value is not a %s", tnbuf);
	}
    }
}

/*
 * assign a value to a parameter
 */
void Frame::storeParam(int param, Value *val)
{
    data->assignVar(argp + param, val);
}

/*
 * assign a value to a local variable
 */
void Frame::storeLocal(int local, Value *val)
{
    data->assignVar(fp - local, val);
}

/*
 * assign a value to a global variable
 */
void Frame::storeGlobal(int inherit, int index, Value *val)
{
    unsigned short offset;

    addTicks(5);
    inherit = ctrl->imap[p_index + inherit];
    offset = ctrl->inherits[inherit].varoffset + index;
    if (lwobj == NULL) {
	data->assignVar(data->variable(offset), val);
    } else {
	data->assignElt(lwobj, &lwobj->elts[2 + offset], val);
    }
}

/*
 * perform an indexed assignment
 */
bool Frame::storeIndex(Value *var, Value *aval, Value *ival, Value *val)
{
    ssizet i;
    String *str;
    Array *arr;

    addTicks(3);
    switch (aval->type) {
    case T_STRING:
	if (ival->type != T_INT) {
	    EC->error("Non-numeric string index");
	}
	if (val->type != T_INT) {
	    EC->error("Non-numeric value in indexed string assignment");
	}
	i = aval->string->index(ival->number);
	str = String::create(aval->string->text, aval->string->len);
	str->text[i] = val->number;
	PUT_STRVAL(var, str);
	return TRUE;

    case T_ARRAY:
	if (ival->type != T_INT) {
	    EC->error("Non-numeric array index");
	}
	arr = aval->array;
	aval = &Dataspace::elts(arr)[arr->index(ival->number)];
	if (var->type != T_STRING ||
	    (aval->type == T_STRING && var->string == aval->string)) {
	    data->assignElt(arr, aval, val);
	}
	arr->del();
	break;

    case T_MAPPING:
	arr = aval->array;
	if (var->type != T_STRING) {
	    var = NULL;
	}
	dynamic_cast<Mapping *> (arr)->index(data, ival, val, var);
	ival->del();
	arr->del();
	break;

    case T_LWOBJECT:
	arr = aval->array;
	oper(dynamic_cast<LWO *> (arr), "[]=", 2, var, ival, val);
	var->del();
	ival->del();
	arr->del();
	break;

    default:
	EC->error("Index on bad type");
    }

    return FALSE;
}

/*
 * perform an indexed assignment
 */
void Frame::storeIndex(Value *val)
{
    Value var;

    var = Value::nil;
    if (storeIndex(&var, sp + 2, sp + 1, val)) {
	sp[2].string->del();
	var.string->del();
    }
    sp[2] = sp[0];
    sp += 2;
}

/*
 * perform an indexed parameter assignment
 */
void Frame::storeParamIndex(int param, Value *val)
{
    Value var, *lvar;

    var = Value::nil;
    if (storeIndex(&var, sp + 2, sp + 1, val)) {
	lvar = argp + param;
	if (lvar->type == T_STRING && lvar->string == sp[2].string) {
	    data->assignVar(lvar, &var);
	}
	sp[2].string->del();
	var.string->del();
    }
    sp[2] = sp[0];
    sp += 2;
}

/*
 * perform an indexed local variable assignment
 */
void Frame::storeLocalIndex(int local, Value *val)
{
    Value var, *lvar;

    var = Value::nil;
    if (storeIndex(&var, sp + 2, sp + 1, val)) {
	lvar = fp - local;
	if (lvar->type == T_STRING && lvar->string == sp[2].string) {
	    data->assignVar(lvar, &var);
	}
	sp[2].string->del();
	var.string->del();
    }
    sp[2] = sp[0];
    sp += 2;
}

/*
 * perform an indexed global var assignment
 */
void Frame::storeGlobalIndex(int inherit, int index, Value *val)
{
    unsigned short offset;
    Value var, *gvar;

    var = Value::nil;
    if (storeIndex(&var, sp + 2, sp + 1, val)) {
	addTicks(5);
	inherit = ctrl->imap[p_index + inherit];
	offset = ctrl->inherits[inherit].varoffset + index;
	if (lwobj == NULL) {
	    gvar = data->variable(offset);
	    if (gvar->type == T_STRING && gvar->string == sp[2].string) {
		data->assignVar(gvar, &var);
	    }
	} else {
	    gvar = &lwobj->elts[2 + offset];
	    if (gvar->type == T_STRING && gvar->string == sp[2].string) {
		data->assignElt(lwobj, gvar, &var);
	    }
	}
	sp[2].string->del();
	var.string->del();
    }
    sp[2] = sp[0];
    sp += 2;
}

/*
 * perform an indexed indexed assignment
 */
void Frame::storeIndexIndex(Value *val)
{
    Value var;

    var = Value::nil;
    if (storeIndex(&var, sp + 2, sp + 1, val)) {
	sp[1] = var;
	storeIndex(sp + 2, sp + 4, sp + 3, sp + 1);
	sp[1].string->del();
	sp[2].string->del();
    } else {
	sp[3].del();
	sp[4].del();
    }
    sp[4] = sp[0];
    sp += 4;
}

/*
 * skip indexed store
 */
void Frame::storeSkip()
{
    sp[1].del();
    sp[2].del();
    sp[2] = sp[0];
    sp += 2;
}

/*
 * skip indexed indexed store
 */
void Frame::storeSkipSkip()
{
    sp[1].del();
    sp[2].del();
    sp[3].del();
    sp[4].del();
    sp[4] = sp[0];
    sp += 4;
}

/*
 * perform a sequence of special stores
 */
void Frame::stores(int skip, int assign)
{
    unsigned short u, instr;
    Uint sclass;

    /*
     * stores to skip
     */
    while (skip != 0) {
	instr = FETCH1U(pc);
	switch (instr & I_INSTR_MASK) {
	case I_CAST:
	case I_CAST | I_POP_BIT:
	    if (FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    continue;

	case I_STORE_LOCAL:
	case I_STORE_LOCAL | I_POP_BIT:
	case I_STORE_GLOBAL:
	case I_STORE_GLOBAL | I_POP_BIT:
	    pc++;
	    break;

	case I_STORE_FAR_GLOBAL:
	case I_STORE_FAR_GLOBAL | I_POP_BIT:
	    pc += 2;
	    break;

	case I_STORE_INDEX:
	case I_STORE_INDEX | I_POP_BIT:
	    storeSkip();
	    break;

	case I_STORE_LOCAL_INDEX:
	case I_STORE_LOCAL_INDEX | I_POP_BIT:
	case I_STORE_GLOBAL_INDEX:
	case I_STORE_GLOBAL_INDEX | I_POP_BIT:
	    pc++;
	    storeSkip();
	    break;

	case I_STORE_FAR_GLOBAL_INDEX:
	case I_STORE_FAR_GLOBAL_INDEX | I_POP_BIT:
	    pc += 2;
	    storeSkip();
	    break;

	case I_STORE_INDEX_INDEX:
	case I_STORE_INDEX_INDEX | I_POP_BIT:
	    storeSkipSkip();
	    break;

# ifdef DEBUG
	default:
	    EC->fatal("invalid store");
# endif
	}
	--skip;
    }

    /*
     * stores to perform
     */
    sclass = 0;
    while (assign != 0) {
	instr = FETCH1U(pc);
	switch (instr & I_INSTR_MASK) {
	case I_CAST:
	case I_CAST | I_POP_BIT:
	    u = FETCH1U(pc);
	    if (u == T_CLASS) {
		FETCH3U(pc, sclass);
	    }
	    cast(&sp->array->elts[assign - 1], u, sclass);
	    continue;

	case I_STORE_LOCAL:
	case I_STORE_LOCAL | I_POP_BIT:
	    u = FETCH1U(pc);
	    if (SCHAR(u) >= 0) {
		storeParam(u, &sp->array->elts[assign - 1]);
	    } else {
		storeLocal(-SCHAR(u), &sp->array->elts[assign - 1]);
	    }
	    break;

	case I_STORE_GLOBAL:
	case I_STORE_GLOBAL | I_POP_BIT:
	    storeGlobal(p_ctrl->ninherits - 1, FETCH1U(pc),
			&sp->array->elts[assign - 1]);
	    break;

	case I_STORE_FAR_GLOBAL:
	case I_STORE_FAR_GLOBAL | I_POP_BIT:
	    u = FETCH1U(pc);
	    storeGlobal(u, FETCH1U(pc), &sp->array->elts[assign - 1]);
	    break;

	case I_STORE_INDEX:
	case I_STORE_INDEX | I_POP_BIT:
	    storeIndex(&sp->array->elts[assign - 1]);
	    break;

	case I_STORE_LOCAL_INDEX:
	case I_STORE_LOCAL_INDEX | I_POP_BIT:
	    u = FETCH1S(pc);
	    if (SCHAR(u) >= 0) {
		storeParamIndex(u, &sp->array->elts[assign - 1]);
	    } else {
		storeLocalIndex(-SCHAR(u), &sp->array->elts[assign - 1]);
	    }
	    break;

	case I_STORE_GLOBAL_INDEX:
	case I_STORE_GLOBAL_INDEX | I_POP_BIT:
	    storeGlobalIndex(p_ctrl->ninherits - 1, FETCH1U(pc),
			     &sp->array->elts[assign - 1]);
	    break;

	case I_STORE_FAR_GLOBAL_INDEX:
	case I_STORE_FAR_GLOBAL_INDEX | I_POP_BIT:
	    u = FETCH1U(pc);
	    storeGlobalIndex(u, FETCH1U(pc), &sp->array->elts[assign - 1]);
	    break;

	case I_STORE_INDEX_INDEX:
	case I_STORE_INDEX_INDEX | I_POP_BIT:
	    storeIndexIndex(&sp->array->elts[assign - 1]);
	    break;

# ifdef DEBUG
	default:
	    EC->fatal("invalid store");
# endif
	}
	--assign;
    }
}

/*
 * spread lvalues
 */
unsigned short Frame::storesSpread(int n, int offset, int type, Uint sclass)
{
    unsigned short nassign, nspread;

    nassign = sp->array->size;
    if (n < nassign && sp[1].array->size > offset) {
	nspread = sp[1].array->size - offset;
	if (nspread >= nassign - n) {
	    nspread = nassign - n;
	    addTicks(nspread * 3);
	    while (nspread != 0) {
		--nassign;
		if (type != 0) {
		    cast(&sp->array->elts[nassign], type, sclass);
		}
		--nspread;
		data->assignElt(sp[1].array,
				&sp[1].array->elts[offset + nspread],
				&sp->array->elts[nassign]);
	    }
	}
    }

    sp[1].array->del();
    sp[1] = sp[0];
    sp++;

    return nassign;
}

/*
 * perform assignments for lvalue arguments
 */
void Frame::lvalues(int n)
{
    char *pc;
    int offset, type;
    unsigned short nassign;
    Uint sclass;

    pc = this->pc;
    nassign = 0;

    if (n != 0) {
	nassign = sp->array->size;

	if ((FETCH1U(pc) & I_INSTR_MASK) == I_SPREAD) {
	    /*
	     * lvalue spread
	     */
	    sclass = 0;
	    offset = FETCH1U(pc);
	    type = FETCH1U(pc);
	    if (type == T_CLASS) {
		FETCH3U(pc, sclass);
	    }
	    this->pc = pc;

	    nassign = storesSpread(--n, offset, type, sclass);
	}

	if (n < nassign) {
	    EC->error("Missing lvalue");
	}
    }

    stores(n - nassign, nassign);
}

/*
 * integer division
 */
LPCint Frame::div(LPCint num, LPCint denom)
{
    if (denom == 0) {
	EC->error("Division by zero");
    }
    return num / denom;
}

/*
 * left shift
 */
LPCint Frame::lshift(LPCint num, LPCint shift)
{
    if ((shift & ~(LPCINT_BITS - 1)) != 0) {
	if (shift < 0) {
	    EC->error("Negative left shift");
	}
	return 0;
    } else {
	return (LPCuint) num << shift;
    }
}

/*
 * integer modulus
 */
LPCint Frame::mod(LPCint num, LPCint denom)
{
    if (denom == 0) {
	EC->error("Modulus by zero");
    }
    return num % denom;
}

/*
 * right shift
 */
LPCint Frame::rshift(LPCint num, LPCint shift)
{
    if ((shift & ~(LPCINT_BITS - 1)) != 0) {
	if (shift < 0) {
	    EC->error("Negative right shift");
	}
	return 0;
    } else {
	return (LPCuint) num >> shift;
    }
}

/*
 * convert to float
 */
void Frame::toFloat(Float *flt)
{
    addTicks(1);
    if (sp->type == T_INT) {
	/* from int */
	Float::itof(sp->number, flt);
    } else if (sp->type == T_STRING) {
	char *p;

	/* from string */
	p = sp->string->text;
	if (!Float::atof(&p, flt) ||
	    p != sp->string->text + sp->string->len) {
	    EC->error("String cannot be converted to float");
	}
	sp->string->del();
    } else if (sp->type == T_FLOAT) {
	GET_FLT(sp, *flt);
    } else {
	EC->error("Value is not a float");
    }

    sp++;
}

/*
 * convert to integer
 */
LPCint Frame::toInt()
{
    Float flt;

    if (sp->type == T_FLOAT) {
	/* from float */
	GET_FLT(sp, flt);
	sp++;
	return flt.ftoi();
    } else if (sp->type == T_STRING) {
	char *p;
	LPCint i;

	/* from string */
	p = sp->string->text;
	i = strtoint(&p);
	if (p != sp->string->text + sp->string->len) {
	    EC->error("String cannot be converted to int");
	}
	sp->string->del();
	sp++;
	return i;
    } else if (sp->type != T_INT) {
	EC->error("Value is not an int");
    }

    return (sp++)->number;
}

/*
 * get the remaining stack depth (-1: infinite)
 */
LPCint Frame::getDepth()
{
    RLInfo *rlim;

    rlim = this->rlim;
    if (rlim->nodepth) {
	return -1;
    }
    return rlim->maxdepth - depth;
}

/*
 * get the remaining ticks (-1: infinite)
 */
LPCint Frame::getTicks()
{
    RLInfo *rlim;

    rlim = this->rlim;
    if (rlim->noticks) {
	return -1;
    } else {
	return (rlim->ticks < 0) ? 0 : rlim->ticks << level;
    }
}

/*
 * check if this rlimits call is valid
 */
void Frame::checkRlimits()
{
    Object *obj;

    obj = OBJR(oindex);
    if (obj->count == 0) {
	EC->error("Illegal use of rlimits");
    }
    --sp;
    sp[0] = sp[1];
    sp[1] = sp[2];
    if (lwobj == (LWO *) NULL) {
	PUT_OBJVAL(&sp[2], obj);
    } else {
	PUT_LWOVAL(&sp[2], lwobj);
    }

    /* obj, stack, ticks */
    DGD::callDriver(this, "runtime_rlimits", 3);

    if (!VAL_TRUE(sp)) {
	EC->error("Illegal use of rlimits");
    }
    (sp++)->del();
}

/*
 * create new rlimits scope
 */
void Frame::newRlimits(LPCint depth, LPCint t)
{
    RLInfo *rlim;

    rlim = ALLOC(RLInfo, 1);
    memset(rlim, '\0', sizeof(RLInfo));
    if (depth != 0) {
	if (depth < 0) {
	    rlim->nodepth = TRUE;
	} else {
	    rlim->maxdepth = this->depth + depth;
	    rlim->nodepth = FALSE;
	}
    } else {
	rlim->maxdepth = this->rlim->maxdepth;
	rlim->nodepth = this->rlim->nodepth;
    }
    if (t != 0) {
	if (t < 0) {
	    rlim->noticks = TRUE;
	} else {
	    t >>= level;
	    this->rlim->ticks -= t;
	    rlim->ticks = t;
	    rlim->noticks = FALSE;
	}
    } else {
	rlim->ticks = this->rlim->ticks;
	rlim->noticks = this->rlim->noticks;
	this->rlim->ticks = 0;
    }

    rlim->next = this->rlim;
    this->rlim = rlim;
}

/*
 * start rlimits scope
 */
void Frame::rlimits(bool privileged)
{
    LPCint newdepth, newticks;

    if (sp[1].type != T_INT) {
	EC->error("Bad rlimits depth type");
    }
    if (sp->type != T_INT) {
	EC->error("Bad rlimits ticks type");
    }
    newdepth = sp[1].number;
    newticks = sp->number;
    if (!privileged) {
	/* runtime check */
	checkRlimits();
    } else {
	/* pop limits */
	sp += 2;
    }
    newRlimits(newdepth, newticks);
}

/*
 * restore rlimits to an earlier state
 */
void Frame::setRlimits(RLInfo *rlim)
{
    RLInfo *r, *next;

    r = this->rlim;
    if (r->ticks < 0) {
	r->ticks = 0;
    }
    while (r != rlim) {
	next = r->next;
	if (!r->noticks) {
	    next->ticks += r->ticks;
	}
	FREE(r);
	r = next;
    }
    this->rlim = rlim;
}

/*
 * set the current stack pointer
 */
Frame *Frame::setSp(Value *sp)
{
    Value *v;
    Frame *f;

    for (f = this; ; f = f->prev) {
	v = f->sp;
	for (;;) {
	    if (v == sp) {
		f->sp = v;
		return f;
	    }
	    if (v == f->fp) {
		break;
	    }
	    (v++)->del();
	}

	if (f->lwobj != (LWO *) NULL) {
	    f->lwobj->del();
	}
	if (f->sos) {
	    /* stack on stack */
	    AFREE(f->stack);
	} else if (f->oindex != OBJ_NONE) {
	    FREE(f->stack);
	}
    }
}

/*
 * return the nth previous object in the call_other chain
 */
Frame *Frame::prevObject(int n)
{
    Frame *f;

    f = this;
    while (n >= 0) {
	/* back to last external call */
	while (!f->external) {
	    f = f->prev;
	}
	f = f->prev;
	if (f->oindex == OBJ_NONE) {
	    return (Frame *) NULL;
	}
	--n;
    }
    return f;
}

/*
 * return the nth previous program in the function call chain
 */
const char *Frame::prevProgram(int n)
{
    Frame *f;

    f = this;
    while (n >= 0) {
	f = f->prev;
	if (f->oindex == OBJ_NONE) {
	    return (char *) NULL;
	}
	--n;
    }

    return OBJR(f->p_ctrl->oindex)->name;
}

/*
 * check the argument types given to a function
 */
void Frame::typecheck(Frame *f, const char *name, const char *ftype,
		      char *proto, int nargs, bool strict)
{
    char tnbuf[TNBUFSIZE];
    int i, n, atype, ptype;
    char *args;
    bool ellipsis;
    Uint sclass;
    Value *elts;

    sclass = 0;
    i = nargs;
    n = PROTO_NARGS(proto) + PROTO_VARGS(proto);
    ellipsis = ((PROTO_CLASS(proto) & C_ELLIPSIS) != 0);
    args = PROTO_ARGS(proto);
    while (n > 0 && i > 0) {
	--i;
	ptype = *args++;
	if ((ptype & T_TYPE) == T_CLASS) {
	    FETCH3U(args, sclass);
	}
	if (n == 1 && ellipsis) {
	    if (ptype == T_MIXED || ptype == T_LVALUE) {
		return;
	    }
	    if ((ptype & T_TYPE) == T_CLASS) {
		args -= 4;
	    } else {
		--args;
	    }
	} else {
	    --n;
	}

	if (ptype != T_MIXED) {
	    atype = sp[i].type;
	    if (atype == T_LWOBJECT) {
		atype = T_OBJECT;
	    }
	    if ((ptype & T_TYPE) == T_CLASS && ptype == T_CLASS &&
		atype == T_OBJECT) {
		if (sp[i].type == T_OBJECT) {
		    if (f->instanceOf(sp[i].oindex, sclass) <= 0) {
			EC->error("Bad object argument %d for function %s",
				  nargs - i, name);
		    }
		} else {
		    elts = Dataspace::elts(sp[i].array);
		    if (elts->type == T_OBJECT) {
			if (f->instanceOf(elts->oindex, sclass) <= 0) {
			    EC->error("Bad object argument %d for function %s",
				      nargs - i, name);
			}
		    } else if (strcmp(Object::builtinName(elts->number),
				      f->className(sclass)) != 0) {
			EC->error("Bad object argument %d for function %s",
				  nargs - i, name);
		    }
		}
		continue;
	    }
	    if (ptype != atype && (atype != T_ARRAY || !(ptype & T_REF))) {
		if (!VAL_NIL(sp + i) || !T_POINTER(ptype)) {
		    /* wrong type */
		    EC->error("Bad argument %d (%s) for %s %s", nargs - i,
			      Value::typeName(tnbuf, atype), ftype, name);
		} else if (strict) {
		    /* nil argument */
		    EC->error("Bad argument %d for %s %s", nargs - i, ftype,
			      name);
		}
	    }
	}
    }
}

/*
 * handle an int switch
 */
unsigned short Frame::switchInt(char *pc)
{
    unsigned short h, l, m, sz, dflt;
    LPCint num;
    char *p;

    FETCH2U(pc, h);
    sz = FETCH1U(pc);
    FETCH2U(pc, dflt);
    if (sp->type != T_INT) {
	return dflt;
    }

    l = 0;
    --h;
    switch (sz) {
    case 1:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 3 * m;
	    num = FETCH1S(p);
	    if (sp->number == num) {
		return FETCH2U(p, l);
	    } else if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 2:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 4 * m;
	    FETCH2S(p, num);
	    if (sp->number == num) {
		return FETCH2U(p, l);
	    } else if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 3:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 5 * m;
	    FETCH3S(p, num);
	    if (sp->number == num) {
		return FETCH2U(p, l);
	    } else if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 4:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 6 * m;
	    FETCH4S(p, num);
	    if (sp->number == num) {
		return FETCH2U(p, l);
	    } else if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

# ifdef LARGENUM
    case 5:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 7 * m;
	    FETCH5S(p, num);
	    if (sp->number == num) {
		return FETCH2U(p, l);
	    } else if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 6:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 8 * m;
	    FETCH6S(p, num);
	    if (sp->number == num) {
		return FETCH2U(p, l);
	    } else if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 7:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 9 * m;
	    FETCH7S(p, num);
	    if (sp->number == num) {
		return FETCH2U(p, l);
	    } else if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 8:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 10 * m;
	    FETCH8S(p, num);
	    if (sp->number == num) {
		return FETCH2U(p, l);
	    } else if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	}
	break;
# endif
    }

    return dflt;
}

/*
 * handle a range switch
 */
unsigned short Frame::switchRange(char *pc)
{
    unsigned short h, l, m, sz, dflt;
    LPCint num;
    char *p;

    FETCH2U(pc, h);
    sz = FETCH1U(pc);
    FETCH2U(pc, dflt);
    if (sp->type != T_INT) {
	return dflt;
    }

    l = 0;
    --h;
    switch (sz) {
    case 1:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 4 * m;
	    num = FETCH1S(p);
	    if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		num = FETCH1S(p);
		if (sp->number <= num) {
		    return FETCH2U(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 2:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 6 * m;
	    FETCH2S(p, num);
	    if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH2S(p, num);
		if (sp->number <= num) {
		    return FETCH2U(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 3:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 8 * m;
	    FETCH3S(p, num);
	    if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH3S(p, num);
		if (sp->number <= num) {
		    return FETCH2U(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 4:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 10 * m;
	    FETCH4S(p, num);
	    if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH4S(p, num);
		if (sp->number <= num) {
		    return FETCH2U(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

# ifdef LARGENUM
    case 5:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 12 * m;
	    FETCH5S(p, num);
	    if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH5S(p, num);
		if (sp->number <= num) {
		    return FETCH2U(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 6:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 14 * m;
	    FETCH6S(p, num);
	    if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH6S(p, num);
		if (sp->number <= num) {
		    return FETCH2U(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 7:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 16 * m;
	    FETCH7S(p, num);
	    if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH7S(p, num);
		if (sp->number <= num) {
		    return FETCH2U(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;

    case 8:
	while (l < h) {
	    m = (l + h) >> 1;
	    p = pc + 18 * m;
	    FETCH8S(p, num);
	    if (sp->number < num) {
		h = m;	/* search in lower half */
	    } else {
		FETCH8S(p, num);
		if (sp->number <= num) {
		    return FETCH2U(p, l);
		}
		l = m + 1;	/* search in upper half */
	    }
	}
	break;
# endif
    }
    return dflt;
}

/*
 * handle a string switch
 */
unsigned short Frame::switchStr(char *pc)
{
    unsigned short h, l, m, u, u2, dflt;
    int cmp;
    char *p;

    FETCH2U(pc, h);
    FETCH2U(pc, dflt);
    if (FETCH1U(pc) == 0) {
	FETCH2U(pc, l);
	if (VAL_NIL(sp)) {
	    return l;
	}
	--h;
    }
    if (sp->type != T_STRING) {
	return dflt;
    }

    l = 0;
    --h;
    while (l < h) {
	m = (l + h) >> 1;
	p = pc + 5 * m;
	u = FETCH1U(p);
	cmp = sp->string->cmp(p_ctrl->strconst(u, FETCH2U(p, u2)));
	if (cmp == 0) {
	    return FETCH2U(p, l);
	} else if (cmp < 0) {
	    h = m;	/* search in lower half */
	} else {
	    l = m + 1;	/* search in upper half */
	}
    }
    return dflt;
}

/*
 * call kernel function
 */
void Frame::kfunc(int n, int nargs)
{
    KFun *kf;

    kf = &KFUN(n);
    if (nargs < PROTO_NARGS(kf->proto)) {
	EC->error("Too few arguments for kfun %s", kf->name);
    } else if (!(PROTO_CLASS(kf->proto) & C_ELLIPSIS) &&
	       nargs > PROTO_NARGS(kf->proto) + PROTO_VARGS(kf->proto)) {
	EC->error("Too many arguments for kfun %s", kf->name);
    }
    if (PROTO_CLASS(kf->proto) & C_TYPECHECKED) {
	typecheck((Frame *) NULL, kf->name, "kfun", kf->proto, nargs, TRUE);
    }
    nargs = (*kf->func)(this, nargs, kf);
    if (nargs != 0) {
	if (nargs < 0) {
	    EC->error("Too few arguments for kfun %s", kf->name);
	} else if (nargs <= PROTO_NARGS(kf->proto) + PROTO_VARGS(kf->proto)) {
	    EC->error("Bad argument %d for kfun %s", nargs, kf->name);
	} else {
	    EC->error("Too many arguments for kfun %s", kf->name);
	}
    }
}

/*
 * call function (virtually)
 */
void Frame::vfunc(int n, int nargs)
{
    char *p;

    p = &ctrl->funcalls[2L * (foffset + n)];
    funcall((Object *) NULL, (LWO *) NULL, UCHAR(p[0]), UCHAR(p[1]), nargs);
}

/*
 * Main interpreter function. Interpret stack machine code.
 */
void Frame::interpret(char *pc)
{
    unsigned short instr, u, u2;
    LPCuint l;
    char *p;
    KFun *kf;
    int size, instance;
    bool atomic;
    Value val;
# ifdef LARGENUM
    Float flt;
# endif

    size = 0;
    l = 0;

    for (;;) {
# ifdef DEBUG
	if (sp < stack + MIN_STACK) {
	    EC->fatal("out of value stack");
	}
# endif
	instr = FETCH1U(pc);
	this->pc = pc;

	switch (instr & I_INSTR_MASK) {
	case I_PUSH_INT1:
	    PUSH_INTVAL(this, FETCH1S(pc));
	    continue;

	case I_PUSH_INT2:
	    PUSH_INTVAL(this, FETCH2S(pc, u));
	    continue;

	case I_PUSH_INT4:
	    PUSH_INTVAL(this, FETCH4S(pc, l));
	    continue;

# ifdef LARGENUM
	case I_PUSH_INT8:
	    PUSH_INTVAL(this, FETCH8S(pc, l));
	    continue;

	case I_PUSH_FLOAT6:
	    FETCH2U(pc, u);
	    Ext::largeFloat(&flt, u, FETCH4U(pc, l));
	    PUSH_FLTVAL(this, flt);
	    continue;

	case I_PUSH_FLOAT12:
	    FETCH4U(pc, l);
	    flt.high = l;
	    FETCH8U(pc, l);
	    flt.low = l;
	    PUSH_FLTVAL(this, flt);
	    continue;
# else
	case I_PUSH_FLOAT6:
	    FETCH2U(pc, u);
	    PUSH_FLTCONST(this, u, FETCH4U(pc, l));
	    continue;
# endif

	case I_PUSH_STRING:
	    PUSH_STRVAL(this, p_ctrl->strconst(p_ctrl->ninherits - 1,
					       FETCH1U(pc)));
	    continue;

	case I_PUSH_NEAR_STRING:
	    u = FETCH1U(pc);
	    PUSH_STRVAL(this, p_ctrl->strconst(u, FETCH1U(pc)));
	    continue;

	case I_PUSH_FAR_STRING:
	    u = FETCH1U(pc);
	    PUSH_STRVAL(this, p_ctrl->strconst(u, FETCH2U(pc, u2)));
	    continue;

	case I_PUSH_LOCAL:
	    u = FETCH1S(pc);
	    pushValue(((short) u < 0) ? fp + (short) u : argp + u);
	    continue;

	case I_PUSH_GLOBAL:
	    pushValue(global(p_ctrl->ninherits - 1, FETCH1U(pc)));
	    continue;

	case I_PUSH_FAR_GLOBAL:
	    u = FETCH1U(pc);
	    pushValue(global(u, FETCH1U(pc)));
	    continue;

	case I_INDEX:
	case I_INDEX | I_POP_BIT:
	    index(sp + 1, sp, &val, FALSE);
	    *++sp = val;
	    break;

	case I_INDEX2:
	    index(sp + 1, sp, &val, TRUE);
	    *--sp = val;
	    continue;

	case I_AGGREGATE:
	case I_AGGREGATE | I_POP_BIT:
	    if (FETCH1U(pc) == 0) {
		aggregate(FETCH2U(pc, u));
	    } else {
		mapAggregate(FETCH2U(pc, u));
	    }
	    break;

	case I_SPREAD:
	    u = FETCH1S(pc);
	    size = spread(-(short) u - 2);
	    continue;

	case I_CAST:
	case I_CAST | I_POP_BIT:
	    u = FETCH1U(pc);
	    if (u == T_CLASS) {
		FETCH3U(pc, l);
	    }
	    cast(sp, u, l);
	    break;

	case I_INSTANCEOF:
	case I_INSTANCEOF | I_POP_BIT:
	    instance = instanceOf(FETCH3U(pc, l));
	    PUT_INTVAL(sp, instance);
	    break;

	case I_STORES:
	case I_STORES | I_POP_BIT:
	    if (p_ctrl->version >= 2) {
		FETCH2U(pc, u);
	    } else {
		u = FETCH1U(pc);
	    }
	    this->pc = pc;
	    if (kflv) {
		kflv = FALSE;
		lvalues(u);

		sp->array->del();
		sp++;
	    } else {
		if (sp->type != T_ARRAY) {
		    EC->error("Value is not an array");
		}
		if (u > sp->array->size) {
		    EC->error("Wrong number of lvalues");
		}
		Dataspace::elts(sp->array);
		stores(0, u);

		if (p_ctrl->version < 3) {
		    sp->array->del();
		    sp++;
		}
	    }
	    pc = this->pc;
	    break;

	case I_STORE_LOCAL:
	case I_STORE_LOCAL | I_POP_BIT:
	    u = FETCH1U(pc);
	    if (SCHAR(u) >= 0) {
		storeParam(u, sp);
	    } else {
		storeLocal(-SCHAR(u), sp);
	    }
	    break;

	case I_STORE_GLOBAL:
	case I_STORE_GLOBAL | I_POP_BIT:
	    storeGlobal(p_ctrl->ninherits - 1, FETCH1U(pc), sp);
	    break;

	case I_STORE_FAR_GLOBAL:
	case I_STORE_FAR_GLOBAL | I_POP_BIT:
	    u = FETCH1U(pc);
	    storeGlobal(u, FETCH1U(pc), sp);
	    break;

	case I_STORE_INDEX:
	case I_STORE_INDEX | I_POP_BIT:
	    storeIndex(sp);
	    break;

	case I_STORE_LOCAL_INDEX:
	case I_STORE_LOCAL_INDEX | I_POP_BIT:
	    u = FETCH1S(pc);
	    if (SCHAR(u) >= 0) {
		storeParamIndex(u, sp);
	    } else {
		storeLocalIndex(-SCHAR(u), sp);
	    }
	    break;

	case I_STORE_GLOBAL_INDEX:
	case I_STORE_GLOBAL_INDEX | I_POP_BIT:
	    storeGlobalIndex(p_ctrl->ninherits - 1, FETCH1U(pc), sp);
	    break;

	case I_STORE_FAR_GLOBAL_INDEX:
	case I_STORE_FAR_GLOBAL_INDEX | I_POP_BIT:
	    u = FETCH1U(pc);
	    storeGlobalIndex(u, FETCH1U(pc), sp);
	    break;

	case I_STORE_INDEX_INDEX:
	case I_STORE_INDEX_INDEX | I_POP_BIT:
	    storeIndexIndex(sp);
	    break;

	case I_JUMP_ZERO:
	    p = prog + FETCH2U(pc, u);
	    if (!VAL_TRUE(sp)) {
		if (p < pc) {
		    loopTicks();
		}
		pc = p;
	    }
	    (sp++)->del();
	    continue;

	case I_JUMP_NONZERO:
	    p = prog + FETCH2U(pc, u);
	    if (VAL_TRUE(sp)) {
		if (p < pc) {
		    loopTicks();
		}
		pc = p;
	    }
	    (sp++)->del();
	    continue;

	case I_JUMP:
	    p = prog + FETCH2U(pc, u);
	    if (p < pc) {
		loopTicks();
	    }
	    pc = p;
	    continue;

	case I_SWITCH:
	    switch (FETCH1U(pc)) {
	    case SWITCH_INT:
		p = prog + switchInt(pc);
		break;

	    case SWITCH_RANGE:
		p = prog + switchRange(pc);
		break;

	    case SWITCH_STRING:
		p = prog + switchStr(pc);
		break;
	    }
	    if (p < pc) {
		loopTicks();
	    }
	    pc = p;
	    (sp++)->del();
	    continue;

	case I_CALL_KFUNC:
	case I_CALL_KFUNC | I_POP_BIT:
	    u = FETCH1U(pc);
	    kf = &KFUN(u);
	    if (PROTO_VARGS(kf->proto) != 0) {
		/* variable # of arguments */
		u2 = FETCH1U(pc) + size;
		size = 0;
	    } else {
		/* fixed # of arguments */
		u2 = PROTO_NARGS(kf->proto);
	    }
	    this->pc = pc;
	    kfunc(u, u2);
	    pc = this->pc;
	    break;

	case I_CALL_EFUNC:
	case I_CALL_EFUNC | I_POP_BIT:
	    FETCH2U(pc, u);
	    kf = &KFUN(u);
	    if (PROTO_VARGS(kf->proto) != 0) {
		/* variable # of arguments */
		u2 = FETCH1U(pc) + size;
		size = 0;
	    } else {
		/* fixed # of arguments */
		u2 = PROTO_NARGS(kf->proto);
	    }
	    this->pc = pc;
	    kfunc(u, u2);
	    pc = this->pc;
	    break;

	case I_CALL_CKFUNC:
	case I_CALL_CKFUNC | I_POP_BIT:
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc) + size;
	    size = 0;
	    this->pc = pc;
	    kfunc(u, u2);
	    pc = this->pc;
	    break;

	case I_CALL_CEFUNC:
	case I_CALL_CEFUNC | I_POP_BIT:
	    FETCH2U(pc, u);
	    u2 = FETCH1U(pc) + size;
	    size = 0;
	    this->pc = pc;
	    kfunc(u, u2);
	    pc = this->pc;
	    break;

	case I_CALL_AFUNC:
	case I_CALL_AFUNC | I_POP_BIT:
	    u = FETCH1U(pc);
	    funcall((Object *) NULL, (LWO *) NULL, 0, u, FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CALL_DFUNC:
	case I_CALL_DFUNC | I_POP_BIT:
	    u = FETCH1U(pc);
	    u2 = FETCH1U(pc);
	    funcall((Object *) NULL, (LWO *) NULL,
		    UCHAR(ctrl->imap[p_index + u]), u2, FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CALL_FUNC:
	case I_CALL_FUNC | I_POP_BIT:
	    FETCH2U(pc, u);
	    vfunc(u, FETCH1U(pc) + size);
	    size = 0;
	    break;

	case I_CATCH:
	case I_CATCH | I_POP_BIT:
	    atomic = this->atomic;
	    p = prog + FETCH2U(pc, u);
	    try {
		EC->push((ErrorContext::Handler) runtimeError);
		this->atomic = FALSE;
		interpret(pc);
		EC->pop();
		pc = this->pc;
		if (p_ctrl->version < 3 || (instr & I_POP_BIT)) {
		    *--sp = Value::nil;
		}
	    } catch (const char*) {
		/* error */
		this->pc = p;
		if (p < pc) {
		    loopTicks();
		}
		pc = p;
		PUSH_STRVAL(this, EC->exception());
	    }
	    this->atomic = atomic;
	    break;

	case I_RLIMITS:
	    rlimits(FETCH1U(pc));
	    interpret(pc);
	    pc = this->pc;
	    setRlimits(rlim->next);
	    continue;

	case I_RETURN:
	    return;

# ifdef DEBUG
	default:
	    EC->fatal("illegal instruction");
# endif
	}

	if (instr & I_POP_BIT) {
	    /* pop the result of the last operation */
	    (sp++)->del();
	}
    }
}

/*
 * Call a function in an object. The arguments must be on the stack already.
 */
void Frame::funcall(Object *obj, LWO *lwobj, int p_ctrli, int funci, int nargs)
{
    char *pc;
    unsigned short n;
    Frame f;
    bool ellipsis;
    Value val;

    f.prev = this;
    if (oindex == OBJ_NONE) {
	/*
	 * top level call
	 */
	f.oindex = obj->index;
	f.lwobj = (LWO *) NULL;
	f.ctrl = obj->ctrl;
	f.data = obj->dataspace();
	f.external = TRUE;
    } else if (lwobj != (LWO *) NULL) {
	/*
	 * call_other to lightweight object
	 */
	f.oindex = obj->index;
	f.lwobj = lwobj;
	f.ctrl = obj->ctrl;
	f.data = lwobj->primary->data;
	f.external = TRUE;
    } else if (obj != (Object *) NULL) {
	/*
	 * call_other to persistent object
	 */
	f.oindex = obj->index;
	f.lwobj = (LWO *) NULL;
	f.ctrl = obj->ctrl;
	f.data = obj->dataspace();
	f.external = TRUE;
    } else {
	/*
	 * local function call
	 */
	f.oindex = oindex;
	f.lwobj = this->lwobj;
	f.ctrl = ctrl;
	f.data = data;
	f.external = FALSE;
    }
    f.depth = depth + 1;
    f.rlim = rlim;
    if (f.depth >= f.rlim->maxdepth && !f.rlim->nodepth) {
	EC->error("Stack overflow");
    }
    if (f.rlim->ticks < 100) {
	if (f.rlim->noticks) {
	    f.rlim->ticks = LPCINT_MAX;
	} else {
	    EC->error("Out of ticks");
	}
    }
    f.kflv = FALSE;

    /* set the program control block */
    obj = OBJR(f.ctrl->inherits[p_ctrli].oindex);
    f.foffset = f.ctrl->inherits[p_ctrli].funcoffset;
    f.p_ctrl = obj->control();
    f.p_index = f.ctrl->inherits[p_ctrli].progoffset;

    /* get the function */
    f.func = &f.p_ctrl->funcs()[funci];
    if (f.func->sclass & C_UNDEFINED) {
	EC->error("Undefined function %s",
		  f.p_ctrl->strconst(f.func->inherit, f.func->index)->text);
    }

    pc = f.p_ctrl->program() + f.func->offset;
    if (f.func->sclass & C_TYPECHECKED) {
	/* typecheck arguments */
	typecheck(&f, f.p_ctrl->strconst(f.func->inherit, f.func->index)->text,
		  "function", pc, nargs, FALSE);
    }

    /* handle arguments */
    ellipsis = ((PROTO_CLASS(pc) & C_ELLIPSIS) != 0);
    n = PROTO_NARGS(pc) + PROTO_VARGS(pc);
    if (nargs < n) {
	int i;

	/* if fewer actual than formal parameters, check for varargs */
	if (nargs < PROTO_NARGS(pc) && stricttc) {
	    EC->error("Insufficient arguments for function %s",
		      f.p_ctrl->strconst(f.func->inherit, f.func->index)->text);
	}

	/* add missing arguments */
	growStack(n - nargs);
	if (ellipsis) {
	    --n;
	}

	pc = &PROTO_FTYPE(pc);
	i = nargs;
	do {
	    if ((FETCH1U(pc) & T_TYPE) == T_CLASS) {
		pc += 3;
	    }
	} while (--i >= 0);
	while (nargs < n) {
	    switch (i=FETCH1U(pc)) {
	    case T_INT:
		*--sp = Value::zeroInt;
		break;

	    case T_FLOAT:
		*--sp = Value::zeroFloat;
		break;

	    default:
		if ((i & T_TYPE) == T_CLASS) {
		    pc += 3;
		}
		*--sp = Value::nil;
		break;
	    }
	    nargs++;
	}
	if (ellipsis) {
	    PUSH_ARRVAL(this, Array::create(f.data, 0));
	    nargs++;
	    if ((FETCH1U(pc) & T_TYPE) == T_CLASS) {
		pc += 3;
	    }
	}
    } else if (ellipsis) {
	Value *v;
	Array *a;

	/* put additional arguments in array */
	nargs -= n - 1;
	a = Array::create(f.data, nargs);
	v = a->elts + nargs;
	do {
	    *--v = *sp++;
	} while (--nargs > 0);
	Dataspace::refImports(a);
	PUSH_ARRVAL(this, a);
	nargs = n;
	pc += PROTO_SIZE(pc);
    } else if (nargs > n) {
	if (stricttc) {
	    EC->error("Too many arguments for function %s",
		      f.p_ctrl->strconst(f.func->inherit, f.func->index)->text);
	}

	/* pop superfluous arguments */
	pop(nargs - n);
	nargs = n;
	pc += PROTO_SIZE(pc);
    } else {
	pc += PROTO_SIZE(pc);
    }
    f.sp = sp;
    f.nargs = nargs;
    cframe = &f;
    if (f.lwobj != (LWO *) NULL) {
	f.lwobj->ref();
    }

    /* deal with atomic functions */
    f.level = level;
    if ((f.func->sclass & C_ATOMIC) && !atomic) {
	Object::newPlane();
	new Dataplane(f.data, ++f.level);
	f.atomic = TRUE;
	if (!f.rlim->noticks) {
	    f.rlim->ticks >>= 1;
	}
    } else {
	if (f.level != f.data->plane->level) {
	    new Dataplane(f.data, f.level);
	}
	f.atomic = atomic;
    }

    f.addTicks(10);

    /* create new local stack */
    f.argp = f.sp;
    FETCH2U(pc, n);
    f.stack = ALLOCA(Value, n + MIN_STACK + EXTRA_STACK);
    f.fp = f.sp = f.stack + n + MIN_STACK + EXTRA_STACK;
    f.sos = TRUE;

    if (f.p_ctrl->version >= 4) {
	pc += 2;
    }

    /* initialize local variables */
    n = FETCH1U(pc);
# ifdef DEBUG
    nargs = n;
# endif
    if (n > 0) {
	do {
	    *--f.sp = Value::nil;
	} while (--n > 0);
    }

    f.ctrl->funCalls();	/* make sure they are available */

    /* execute code */
    f.source = 0;
    if (!Ext::execute(&f, funci)) {
	f.prog = pc += 2;
	f.interpret(pc);
    }
    val = *f.sp++;

    /* clean up stack, move return value to outer stackframe */
# ifdef DEBUG
    if (f.sp != f.fp - nargs) {
	EC->fatal("bad stack pointer after function call");
    }
# endif
    f.pop(f.fp - f.sp);
    if (f.sos) {
	    /* still alloca'd */
	AFREE(f.stack);
    } else {
	/* extended and malloced */
	FREE(f.stack);
    }

    if (f.lwobj != (LWO *) NULL) {
	f.lwobj->del();
    }
    cframe = this;
    pop(f.nargs);
    *--sp = val;

    if ((f.func->sclass & C_ATOMIC) && !atomic) {
	Dataplane::commit(f.level, &val);
	Object::commitPlane();
	if (!f.rlim->noticks) {
	    f.rlim->ticks *= 2;
	}
    }
}

/*
 * Attempt to call a function in an object. Return TRUE if the call succeeded.
 */
bool Frame::call(Object *obj, LWO *lwobj, const char *func, unsigned int len,
		 int call_static, int nargs)
{
    Symbol *symb;
    FuncDef *fdef;
    Control *ctrl;

    if (lwobj != (LWO *) NULL) {
	uindex oindex;
	Float flt;
	Value val;

	GET_FLT(&lwobj->elts[1], flt);
	if (lwobj->elts[0].type == T_OBJECT) {
	    /*
	     * ordinary light-weight object: upgrade first if needed
	     */
	    oindex = lwobj->elts[0].oindex;
	    obj = OBJR(oindex);
	    if (flt.low != obj->update) {
		Dataspace::upgradeLWO(lwobj, obj);
		flt.low = obj->update;
	    }
	}
	if (flt.high != FALSE) {
	    /*
	     * touch the light-weight object
	     */
	    flt.high = FALSE;
	    PUT_FLTVAL(&val, flt);
	    data->assignElt(lwobj, &lwobj->elts[1], &val);
	    PUSH_LWOVAL(this, lwobj);
	    PUSH_STRVAL(this, String::create(func, len));
	    DGD::callDriver(this, "touch", 2);
	    if (VAL_TRUE(sp)) {
		/* preserve through call */
		flt.high = TRUE;
		PUT_FLT(&lwobj->elts[1], flt);
	    }
	    (sp++)->del();
	}
	if (lwobj->elts[0].type == T_INT) {
	    /* no user-callable functions within (right?) */
	    pop(nargs);
	    return FALSE;
	}
    } else if (!(obj->flags & O_TOUCHED)) {
	/*
	 * initialize/touch the object
	 */
	obj = OBJW(obj->index);
	obj->flags |= O_TOUCHED;
	if (O_HASDATA(obj)) {
	    PUSH_OBJVAL(this, obj);
	    PUSH_STRVAL(this, String::create(func, len));
	    DGD::callDriver(this, "touch", 2);
	    if (VAL_TRUE(sp)) {
		obj->flags &= ~O_TOUCHED;	/* preserve though call */
	    }
	    (sp++)->del();
	} else {
	    obj->data = Dataspace::create(obj);
	    if (func != (char *) NULL &&
		call(obj, (LWO *) NULL, creator, clen, TRUE, 0)) {
		(sp++)->del();
	    }
	}
    }
    if (func == (char *) NULL) {
	func = creator;
	len = clen;
    }

    /* find the function in the symbol table */
    ctrl = obj->control();
    symb = ctrl->symb(func, len);
    if (symb == (Symbol *) NULL) {
	/* function doesn't exist in symbol table */
	pop(nargs);
	return FALSE;
    }

    ctrl = OBJR(ctrl->inherits[UCHAR(symb->inherit)].oindex)->ctrl;
    fdef = &ctrl->funcs()[UCHAR(symb->index)];

    /* check if the function can be called */
    if (!call_static && (fdef->sclass & C_STATIC) &&
	(oindex != obj->index || this->lwobj != lwobj)) {
	pop(nargs);
	return FALSE;
    }

    /* call the function */
    funcall(obj, lwobj, UCHAR(symb->inherit), UCHAR(symb->index), nargs);

    return TRUE;
}

/*
 * return the line number the program counter of the specified frame is at
 */
unsigned short Frame::line()
{
    char *pc, *numbers;
    int instr;
    short offset;
    unsigned short line, u, sz;

    line = 0;
    pc = p_ctrl->prog + func->offset;
    pc += PROTO_SIZE(pc) + 3;
    if (p_ctrl->version >= 4) {
	pc += 2;
    }
    FETCH2U(pc, u);
    numbers = pc + u;

    while (pc < this->pc) {
	instr = FETCH1U(pc);

	offset = instr >> I_LINE_SHIFT;
	if (offset <= 2) {
	    /* simple offset */
	    line += offset;
	} else {
	    offset = FETCH1U(numbers);
	    if (offset >= 128) {
		/* one byte offset */
		line += offset - 128 - 64;
	    } else {
		/* two byte offset */
		line += ((offset << 8) | FETCH1U(numbers)) - 16384;
	    }
	}

	switch (instr & I_INSTR_MASK) {
	case I_INDEX:
	case I_INDEX | I_POP_BIT:
	case I_INDEX2:
	case I_STORE_INDEX:
	case I_STORE_INDEX | I_POP_BIT:
	case I_STORE_INDEX_INDEX:
	case I_STORE_INDEX_INDEX | I_POP_BIT:
	case I_RETURN:
	    break;

	case I_CALL_KFUNC:
	case I_CALL_KFUNC | I_POP_BIT:
	    if (PROTO_VARGS(KFUN(FETCH1U(pc)).proto) != 0) {
		pc++;
	    }
	    break;

	case I_STORES:
	case I_STORES | I_POP_BIT:
	    if (p_ctrl->version >= 2) {
		pc++;
	    }
	    /* fall through */
	case I_PUSH_INT1:
	case I_PUSH_STRING:
	case I_PUSH_LOCAL:
	case I_PUSH_GLOBAL:
	case I_STORE_LOCAL:
	case I_STORE_LOCAL | I_POP_BIT:
	case I_STORE_GLOBAL:
	case I_STORE_GLOBAL | I_POP_BIT:
	case I_STORE_LOCAL_INDEX:
	case I_STORE_LOCAL_INDEX | I_POP_BIT:
	case I_STORE_GLOBAL_INDEX:
	case I_STORE_GLOBAL_INDEX | I_POP_BIT:
	case I_RLIMITS:
	    pc++;
	    break;

	case I_SPREAD:
	    if (FETCH1S(pc) < 0) {
		break;
	    }
	    /* fall through */
	case I_CAST:
	case I_CAST | I_POP_BIT:
	    if (FETCH1U(pc) == T_CLASS) {
		pc += 3;
	    }
	    break;

	case I_CALL_EFUNC:
	case I_CALL_EFUNC | I_POP_BIT:
	    if (PROTO_VARGS(KFUN(FETCH2U(pc, u)).proto) != 0) {
		pc++;
	    }
	    break;

	case I_PUSH_INT2:
	case I_PUSH_NEAR_STRING:
	case I_PUSH_FAR_GLOBAL:
	case I_STORE_FAR_GLOBAL:
	case I_STORE_FAR_GLOBAL | I_POP_BIT:
	case I_STORE_FAR_GLOBAL_INDEX:
	case I_STORE_FAR_GLOBAL_INDEX | I_POP_BIT:
	case I_JUMP_ZERO:
	case I_JUMP_NONZERO:
	case I_JUMP:
	case I_CALL_AFUNC:
	case I_CALL_AFUNC | I_POP_BIT:
	case I_CALL_CKFUNC:
	case I_CALL_CKFUNC | I_POP_BIT:
	case I_CATCH:
	case I_CATCH | I_POP_BIT:
	    pc += 2;
	    break;

	case I_PUSH_FAR_STRING:
	case I_AGGREGATE:
	case I_AGGREGATE | I_POP_BIT:
	case I_INSTANCEOF:
	case I_INSTANCEOF | I_POP_BIT:
	case I_CALL_DFUNC:
	case I_CALL_DFUNC | I_POP_BIT:
	case I_CALL_FUNC:
	case I_CALL_FUNC | I_POP_BIT:
	case I_CALL_CEFUNC:
	case I_CALL_CEFUNC | I_POP_BIT:
	    pc += 3;
	    break;

	case I_PUSH_INT4:
	    pc += 4;
	    break;

	case I_PUSH_FLOAT6:
	    pc += 6;
	    break;

# ifdef LARGENUM
	case I_PUSH_INT8:
	    pc += 8;
	    break;

	case I_PUSH_FLOAT12:
	    pc += 12;
	    break;
# endif

	case I_SWITCH:
	    switch (FETCH1U(pc)) {
	    case 0:
		FETCH2U(pc, u);
		sz = FETCH1U(pc);
		pc += 2 + (u - 1) * (sz + 2);
		break;

	    case 1:
		FETCH2U(pc, u);
		sz = FETCH1U(pc);
		pc += 2 + (u - 1) * (2 * sz + 2);
		break;

	    case 2:
		FETCH2U(pc, u);
		pc += 2;
		if (FETCH1U(pc) == 0) {
		    pc += 2;
		    --u;
		}
		pc += (u - 1) * 5;
		break;
	    }
	    break;
	}
    }

    return line;
}

/*
 * return part of a trace of a single function
 */
bool Frame::funcTraceI(LPCint idx, Value *val)
{
    char buffer[STRINGSZ + 12];
    String *str;
    const char *name;

    switch (idx) {
    case 0:
	/* object name */
	name = OBJR(oindex)->objName(buffer);
	if (lwobj == (LWO *) NULL) {
	    PUT_STRVAL(val, str = String::create((char *) NULL,
		       strlen(name) + 1L));
	    str->text[0] = '/';
	    strcpy(str->text + 1, name);
	} else {
	    PUT_STRVAL(val, str = String::create((char *) NULL,
		       strlen(name) + 4L));
	    str->text[0] = '/';
	    strcpy(str->text + 1, name);
	    strcpy(str->text + str->len - 3, "#-1");
	}
	break;

    case 1:
	/* program name */
	name = OBJR(p_ctrl->oindex)->name;
	PUT_STRVAL(val, str = String::create((char *) NULL, strlen(name) + 1L));
	str->text[0] = '/';
	strcpy(str->text + 1, name);
	break;

    case 2:
	/* function name */
	PUT_STRVAL(val, p_ctrl->strconst(func->inherit, func->index));
	break;

    case 3:
	/* line number */
	PUT_INTVAL(val, source != 0 ? source : line());
	break;

    case 4:
	/* external flag */
	PUT_INTVAL(val, external);
	break;

    default:
	if (idx < 0 || idx - 4 > nargs) {
	    return FALSE;
	}

	/* argument */
	*val = argp[nargs - idx + 4];
	val->ref();
	break;
    }

    return TRUE;
}

/*
 * return the trace of a single function
 */
Array *Frame::funcTrace(Dataspace *data)
{
    Value *v;
    unsigned short n;
    Value *args;
    Array *a;
    unsigned short max_args;

    max_args = Config::arraySize() - 5;

    n = nargs;
    args = argp + n;
    if (n > max_args) {
	/* unlikely, but possible */
	n = max_args;
    }
    a = Array::create(data, n + 5L);
    v = a->elts;

    funcTraceI(0, v++);
    funcTraceI(1, v++);
    funcTraceI(2, v++);
    funcTraceI(3, v++);
    funcTraceI(4, v++);

    /* arguments */
    while (n > 0) {
	*v++ = *--args;
	args->ref();
	--n;
    }
    Dataspace::refImports(a);

    return a;
}

/*
 * get part of the trace of a single function
 */
bool Frame::callTraceII(LPCint i, LPCint j, Value *v)
{
    Frame *f;

    if (i < 0 || i >= depth) {
	return FALSE;
    }

    addTicks(4);
    for (f = this, i = depth - i - 1; i != 0; f = f->prev, --i) ;
    return f->funcTraceI(j, v);
}

/*
 * get the trace of a single function
 */
bool Frame::callTraceI(LPCint i, Value *v)
{
    Frame *f;

    if (i < 0 || i >= depth) {
	return FALSE;
    }

    addTicks(12);
    for (f = this, i = depth - i - 1; i != 0; f = f->prev, --i) ;
    PUT_ARRVAL(v, f->funcTrace(data));
    return TRUE;
}

/*
 * return the function call trace
 */
Array *Frame::callTrace()
{
    Frame *f;
    Value *v;
    unsigned short n;
    Array *a;

    n = depth;
    a = Array::create(data, n);
    addTicks(10 * n);
    for (f = this, v = a->elts + n; f->oindex != OBJ_NONE; f = f->prev) {
	--v;
	PUT_ARRVAL(v, f->funcTrace(data));
    }

    return a;
}

/*
 * fake error handler
 */
static void emptyhandler(Frame *f, LPCint depth)
{
    UNREFERENCED_PARAMETER(f);
    UNREFERENCED_PARAMETER(depth);
}

/*
 * Call a function in the driver object at a critical moment.  The function
 * is called with rlimits (-1; -1) and errors caught.
 */
bool Frame::callCritical(const char *func, int narg, int flag)
{
    bool ok;

    newRlimits(-1, -1);
    sp += narg;		/* so the error context knows what to pop */
    try {
	EC->push((ErrorContext::Handler) ((flag) ? NULL : emptyhandler));
	sp -= narg;	/* recover arguments */
	DGD::callDriver(this, func, narg);
	ok = TRUE;
	EC->pop();
    } catch (const char*) {
	ok = FALSE;
    }
    setRlimits(rlim->next);

    return ok;
}

/*
 * handle a runtime error
 */
void Frame::runtimeError(Frame *f, LPCint depth)
{
    PUSH_STRVAL(f, EC->exception());
    PUSH_INTVAL(f, depth);
    PUSH_INTVAL(f, f->getTicks());
    if (!f->callCritical("runtime_error", 3, FALSE)) {
	EC->message("Error within runtime_error:\012");	/* LF */
	EC->message((char *) NULL);
    } else {
	if (f->sp->type == T_STRING) {
	    EC->setException(f->sp->string);
	}
	(f->sp++)->del();
    }
}

/*
 * handle error in atomic code
 */
void Frame::atomicError(LPCint level)
{
    Frame *f;

    for (f = this; f->level != level; f = f->prev) ;

    PUSH_STRVAL(this, EC->exception());
    PUSH_INTVAL(this, f->depth);
    PUSH_INTVAL(this, getTicks());
    if (!callCritical("atomic_error", 3, FALSE)) {
	EC->message("Error within atomic_error:\012");	/* LF */
	EC->message((char *) NULL);
    } else {
	if (sp->type == T_STRING) {
	    EC->setException(sp->string);
	}
	(sp++)->del();
    }
}

/*
 * restore state to given level
 */
Frame *Frame::restore(LPCint level)
{
    Frame *f;

    for (f = this; f->level != level; f = f->prev) ;

    if (f->rlim != rlim) {
	setRlimits(f->rlim);
    }
    if (!f->rlim->noticks) {
	f->rlim->ticks *= 2;
    }
    setSp(f->sp);
    Dataplane::discard(this->level);
    Object::discardPlane();

    return f;
}

/*
 * clean up the interpreter state
 */
void Frame::clear()
{
    Frame *f;

    f = cframe;
    if (f->stack != ::stack) {
	FREE(f->stack);
	f->fp = f->sp = ::stack + MIN_STACK;
	f->stack = ::stack;
    }

    f->rlim = &::rlim;
}
