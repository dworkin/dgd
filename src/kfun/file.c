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
# define INCLUDE_FILE_IO
# define INCLUDE_CTYPE
# include "kfun.h"
# include "path.h"
# include "editor.h"
# endif

# ifdef FUNCDEF
FUNCDEF("editor", kf_editor, pt_editor, 0)
# else
char pt_editor[] = { C_TYPECHECKED | C_STATIC, 0, 1, 0, 7, T_STRING, T_STRING };

/*
 * NAME:	kfun->editor()
 * DESCRIPTION:	handle an editor command
 */
int kf_editor(f, nargs)
register frame *f;
int nargs;
{
    object *obj;
    string *str;

    if (f->lwobj != (array *) NULL) {
	error("editor() in non-persistent object");
    }
    obj = OBJW(f->oindex);
    if (obj->count == 0) {
	error("editor() in destructed object");
    }
    if ((obj->flags & O_SPECIAL) && (obj->flags & O_SPECIAL) != O_EDITOR) {
	error("editor() in special purpose object");
    }
    if (f->level != 0) {
	error("editor() within atomic function");
    }
    if (!(obj->flags & O_EDITOR)) {
	ed_new(obj);
    }
    if (nargs == 0) {
	*--f->sp = nil_value;
    } else {
	str = ed_command(obj, f->sp->u.string->text);
	str_del(f->sp->u.string);
	if (str != (string *) NULL) {
	    PUT_STR(f->sp, str);
	} else {
	    *f->sp = nil_value;
	}
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("query_editor", kf_query_editor, pt_query_editor, 0)
# else
char pt_query_editor[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_STRING,
			   T_OBJECT };

/*
 * NAME:	kfun->query_editor()
 * DESCRIPTION:	query the editing status of an object
 */
int kf_query_editor(f)
register frame *f;
{
    object *obj;
    char *status;

    if (f->sp->type == T_OBJECT) {
	obj = OBJR(f->sp->oindex);
	if ((obj->flags & O_SPECIAL) == O_EDITOR) {
	    status = ed_status(obj);
	    PUT_STRVAL(f->sp, str_new(status, (long) strlen(status)));
	    return 0;
	}
    } else {
	arr_del(f->sp->u.array);
    }

    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("save_object", kf_save_object, pt_save_object, 0)
# else
typedef struct {
    int fd;			/* save/restore file descriptor */
    char *buffer;		/* save/restore buffer */
    unsigned int bufsz;		/* size of save/restore buffer */
    Uint narrays;		/* number of arrays/mappings encountered */
} savecontext;

/*
 * NAME:	put()
 * DESCRIPTION:	output a number of characters
 */
static void put(x, buf, len)
register savecontext *x;
register char *buf;
register unsigned int len;
{
    register unsigned int chunk;

    while (x->bufsz + len > BUF_SIZE) {
	chunk = BUF_SIZE - x->bufsz;
	memcpy(x->buffer + x->bufsz, buf, chunk);
	P_write(x->fd, x->buffer, BUF_SIZE);
	buf += chunk;
	len -= chunk;
	x->bufsz = 0;
    }
    if (len > 0) {
	memcpy(x->buffer + x->bufsz, buf, len);
	x->bufsz += len;
    }
}

/*
 * NAME:	save_string()
 * DESCRIPTION:	save a string
 */
static void save_string(x, str)
savecontext *x;
string *str;
{
    char buf[STRINGSZ];
    register char *p, *q, c;
    register ssizet len;
    register unsigned int size;

    p = str->text;
    q = buf;
    *q++ = '"';
    for (len = str->len, size = 1; len > 0; --len, size++) {
	if (size >= STRINGSZ - 2) {
	    put(x, q = buf, size);
	    size = 0;
	}
	switch (c = *p++) {
	case '\0': c = '0'; break;
	case BEL: c = 'a'; break;
	case BS: c = 'b'; break;
	case HT: c = 't'; break;
	case LF: c = 'n'; break;
	case VT: c = 'v'; break;
	case FF: c = 'f'; break;
	case CR: c = 'r'; break;
	case '"':
	case '\\':
	    break;

	default:
	    /* ordinary character */
	    *q++ = c;
	    continue;
	}
	/* escaped character */
	*q++ = '\\';
	size++;
	*q++ = c;
    }
    *q++ = '"';
    put(x, buf, size + 1);
}

static void save_mapping	P((savecontext*, array*));

/*
 * NAME:	save_array()
 * DESCRIPTION:	save an array
 */
static void save_array(x, a)
register savecontext *x;
array *a;
{
    char buf[16];
    register Uint i;
    register value *v;
    xfloat flt;

    i = arr_put(a, x->narrays);
    if (i < x->narrays) {
	/* same as some previous array */
	sprintf(buf, "#%lu", (unsigned long) i);
	put(x, buf, strlen(buf));
	return;
    }
    x->narrays++;

    sprintf(buf, "({%d|", a->size);
    put(x, buf, strlen(buf));
    for (i = a->size, v = d_get_elts(a); i > 0; --i, v++) {
	switch (v->type) {
	case T_NIL:
	    put(x, "nil", 3);
	    break;

	case T_INT:
	    sprintf(buf, "%ld", (long) v->u.number);
	    put(x, buf, strlen(buf));
	    break;

	case T_FLOAT:
	    GET_FLT(v, flt);
	    flt_ftoa(&flt, buf);
	    put(x, buf, strlen(buf));
	    sprintf(buf, "=%04x%08lx", flt.high, (long) flt.low);
	    put(x, buf, 13);
	    break;

	case T_STRING:
	    save_string(x, v->u.string);
	    break;

	case T_OBJECT:
	case T_LWOBJECT:
	    if (conf_typechecking() >= 2) {
		put(x, "nil", 3);
	    } else {
		put(x, "0", 1);
	    }
	    break;

	case T_ARRAY:
	    save_array(x, v->u.array);
	    break;

	case T_MAPPING:
	    save_mapping(x, v->u.array);
	    break;
	}
	put(x, ",", 1);
    }
    put(x, "})", 2);
}

/*
 * NAME:	save_mapping()
 * DESCRIPTION:	save a mapping
 */
static void save_mapping(x, a)
register savecontext *x;
array *a;
{
    char buf[16];
    register Uint i;
    register uindex n;
    register value *v;
    xfloat flt;

    i = arr_put(a, x->narrays);
    if (i < x->narrays) {
	/* same as some previous mapping */
	sprintf(buf, "@%lu", (unsigned long) i);
	put(x, buf, strlen(buf));
	return;
    }
    x->narrays++;
    map_compact(a->primary->data, a);

    /*
     * skip index/value pairs of which either is an object
     */
    for (i = n = a->size >> 1, v = d_get_elts(a); i > 0; --i) {
	if (v->type == T_OBJECT || v->type == T_LWOBJECT) {
	    /* skip object index */
	    --n;
	    v += 2;
	    continue;
	}
	v++;
	if (v->type == T_OBJECT || v->type == T_LWOBJECT) {
	    /* skip object value */
	    --n;
	}
	v++;
    }
    sprintf(buf, "([%d|", n);
    put(x, buf, strlen(buf));

    for (i = a->size >> 1, v = a->elts; i > 0; --i) {
	if (v[0].type == T_OBJECT || v[0].type == T_LWOBJECT ||
	    v[1].type == T_OBJECT || v[1].type == T_LWOBJECT) {
	    v += 2;
	    continue;
	}
	switch (v->type) {
	case T_NIL:
	    put(x, "nil", 3);
	    break;

	case T_INT:
	    sprintf(buf, "%ld", (long) v->u.number);
	    put(x, buf, strlen(buf));
	    break;

	case T_FLOAT:
	    GET_FLT(v, flt);
	    flt_ftoa(&flt, buf);
	    put(x, buf, strlen(buf));
	    sprintf(buf, "=%04x%08lx", flt.high, (long) flt.low);
	    put(x, buf, 13);
	    break;

	case T_STRING:
	    save_string(x, v->u.string);
	    break;

	case T_ARRAY:
	    save_array(x, v->u.array);
	    break;

	case T_MAPPING:
	    save_mapping(x, v->u.array);
	    break;
	}
	put(x, ":", 1);
	v++;
	switch (v->type) {
	case T_INT:
	    sprintf(buf, "%ld", (long) v->u.number);
	    put(x, buf, strlen(buf));
	    break;

	case T_FLOAT:
	    GET_FLT(v, flt);
	    flt_ftoa(&flt, buf);
	    put(x, buf, strlen(buf));
	    sprintf(buf, "=%04x%08lx", flt.high, (long) flt.low);
	    put(x, buf, 13);
	    break;

	case T_STRING:
	    save_string(x, v->u.string);
	    break;

	case T_ARRAY:
	    save_array(x, v->u.array);
	    break;

	case T_MAPPING:
	    save_mapping(x, v->u.array);
	    break;
	}
	put(x, ",", 1);
	v++;
    }
    put(x, "])", 2);
}

char pt_save_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_VOID,
			  T_STRING };

/*
 * NAME:	kfun->save_object()
 * DESCRIPTION:	save the variables of the current object
 */
int kf_save_object(f)
register frame *f;
{
    static unsigned short count;
    register unsigned short i, j, nvars;
    register value *var;
    register dvardef *v;
    register control *ctrl;
    register string *str;
    register dinherit *inh;
    char file[STRINGSZ], buf[16], tmp[STRINGSZ + 8], *_tmp;
    savecontext x;
    xfloat flt;

    if (path_string(file, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 1;
    }
    if (f->level != 0) {
	error("save_object() within atomic function");
    }

    /*
     * First save in a different file in the same directory, so a possibly
     * existing old instance will not be lost if something goes wrong.
     */
    i_add_ticks(f, 2000);	/* arbitrary */
    strcpy(tmp, file);
    _tmp = strrchr(tmp, '/');
    _tmp = (_tmp == (char *) NULL) ? tmp : _tmp + 1;
    sprintf(_tmp, "_tmp%04x", ++count);
    x.fd = P_open(tmp, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0664);
    if (x.fd < 0) {
	error("Cannot create temporary save file \"/%s\"", tmp);
    }
    x.buffer = ALLOCA(char, BUF_SIZE);
    x.bufsz = 0;

    ctrl = f->ctrl;
    arr_merge();
    x.narrays = 0;
    if (f->lwobj != (array *) NULL) {
	var = &f->lwobj->elts[2];
    } else {
	var = d_get_variable(f->data, 0);
    }
    nvars = 0;
    for (i = ctrl->ninherits, inh = ctrl->inherits; i > 0; --i, inh++) {
	if (inh->varoffset == nvars) {
	    /*
	     * This is the program that has the next variables in the object.
	     * Save non-static variables.
	     */
	    ctrl = o_control(OBJR(inh->oindex));
	    if (inh->priv) {
		/* skip privately inherited variables */
		var += ctrl->nvardefs;
		nvars += ctrl->nvardefs;
		continue;
	    }
	    for (j = ctrl->nvardefs, v = d_get_vardefs(ctrl); j > 0; --j, v++) {
		if (!(v->class & C_STATIC) && var->type != T_OBJECT &&
		    var->type != T_LWOBJECT && VAL_TRUE(var)) {
		    /*
		     * don't save object values, nil or 0
		     */
		    str = d_get_strconst(ctrl, v->inherit, v->index);
		    put(&x, str->text, str->len);
		    put(&x, " ", 1);
		    switch (var->type) {
		    case T_INT:
			sprintf(buf, "%ld", (long) var->u.number);
			put(&x, buf, strlen(buf));
			break;

		    case T_FLOAT:
			GET_FLT(var, flt);
			flt_ftoa(&flt, buf);
			put(&x, buf, strlen(buf));
			sprintf(buf, "=%04x%08lx", flt.high, (long) flt.low);
			put(&x, buf, 13);
			break;

		    case T_STRING:
			save_string(&x, var->u.string);
			break;

		    case T_ARRAY:
			save_array(&x, var->u.array);
			break;

		    case T_MAPPING:
			save_mapping(&x, var->u.array);
			break;
		    }
		    put(&x, "\012", 1);	/* LF */
		}
		var++;
		nvars++;
	    }
	}
    }

    arr_clear();
    if (x.bufsz > 0 && P_write(x.fd, x.buffer, x.bufsz) != x.bufsz) {
	P_close(x.fd);
	AFREE(x.buffer);
	P_unlink(tmp);
	error("Cannot write to temporary save file \"/%s\"", tmp);
    }
    P_close(x.fd);
    AFREE(x.buffer);

    P_unlink(file);
    if (P_rename(tmp, file) < 0) {
	P_unlink(tmp);
	error("Cannot rename temporary save file to \"/%s\"", file);
    }

    str_del(f->sp->u.string);
    *f->sp = nil_value;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("restore_object", kf_restore_object, pt_restore_object, 0)
# else
# define ACHUNKSZ	16

typedef struct _achunk_ {
    struct _achunk_ *next;	/* next in list */
    value a[ACHUNKSZ];		/* chunk of arrays */
} achunk;

typedef struct {
    int line;			/* current line number */
    frame *f;			/* interpreter frame */
    achunk *alist;		/* list of array chunks */
    int achunksz;		/* size of current array chunk */
    Uint narrays;		/* # of arrays/mappings */
    char file[STRINGSZ];	/* current restore file */
} restcontext;

/*
 * NAME:	achunk->put()
 * DESCRIPTION:	put an array into the array chunks
 */
static void ac_put(x, type, a)
register restcontext *x;
short type;
array *a;
{
    if (x->achunksz == ACHUNKSZ) {
	register achunk *l;

	l = ALLOC(achunk, 1);
	l->next = x->alist;
	x->alist = l;
	x->achunksz = 0;
    }
    x->alist->a[x->achunksz].type = type;
    x->alist->a[x->achunksz++].u.array = a;
    x->narrays++;
}

/*
 * NAME:	achunk->get()
 * DESCRIPTION:	get an array from the array chunks
 */
static value *ac_get(x, n)
restcontext *x;
register Uint n;
{
    register Uint sz;
    register achunk *l;

    n = x->narrays - n;
    for (sz = x->achunksz, l = x->alist; n > sz; l = l->next, sz = ACHUNKSZ) {
	n -= sz;
    }
    return &l->a[sz - n];
}

/*
 * NAME:	achunk->clear()
 * DESCRIPTION:	clear the array chunks
 */
static void ac_clear(x)
restcontext *x;
{
    register achunk *l, *f;

    for (l = x->alist; l != (achunk *) NULL; ) {
	f = l;
	l = l->next;
	FREE(f);
    }
}


/*
 * NAME:	restore_error()
 * DESCRIPTION:	handle an error while restoring
 */
static void restore_error(x, err)
restcontext *x;
char *err;
{
    error("Format error in \"/%s\", line %d: %s", x->file, x->line, err);
}
 
/*
 * NAME:	restore_int()
 * DESCRIPTION:	restore an integer
 */
static char *restore_int(x, buf, val)
restcontext *x;
char *buf;
value *val;
{
    char *p;

    p = buf;
    PUT_INTVAL(val, strtoint(&p));
    if (p == buf) {
	restore_error(x, "digit expected");
    }

    return p;
}

/*
 * NAME:	restore_number()
 * DESCRIPTION:	restore a number
 */
static char *restore_number(x, buf, val)
register restcontext *x;
char *buf;
register value *val;
{
    register char *p;
    register int i;
    char *q;
    xfloat flt;
    bool isfloat;

    q = buf;
    PUT_INTVAL(val, strtoint(&buf));
    if (buf == q) {
	restore_error(x, "digit expected");
    }

    isfloat = FALSE;
    p = buf;
    if (*p == '.') {
	isfloat = TRUE;
	while (isdigit(*++p)) ;
    }
    if (*p == 'e' || *p == 'E') {
	isfloat = TRUE;
	p++;
	if (*p == '+' || *p == '-') {
	    p++;
	}
	if (!isdigit(*p)) {
	    restore_error(x, "digit expected");
	}
	while (isdigit(*++p)) ;
    }
    if (*p == '=') {
	flt.high = flt.low = 0;
	for (i = 4; i > 0; --i) {
	    if (!isxdigit(*++p)) {
		restore_error(x, "hexadecimal digit expected");
	    }
	    flt.high <<= 4;
	    if (isdigit(*p)) {
		flt.high += *p - '0';
	    } else {
		flt.high += toupper(*p) + 10 - 'A';
	    }
	}
	if ((flt.high & 0x7ff0) == 0x7ff0) {
	    restore_error(x, "illegal exponent");
	}
	for (i = 8; i > 0; --i) {
	    if (!isxdigit(*++p)) {
		restore_error(x, "hexadecimal digit expected");
	    }
	    flt.low <<= 4;
	    if (isdigit(*p)) {
		flt.low += *p - '0';
	    } else {
		flt.low += toupper(*p) + 10 - 'A';
	    }
	}

	PUT_FLTVAL(val, flt);
	return p + 1;
    } else if (isfloat) {
	if (!flt_atof(&q, &flt)) {
	    restore_error(x, "float too large");
	}
	PUT_FLTVAL(val, flt);
	return p;
    }

    return p;
}

/*
 * NAME:	restore_string()
 * DESCRIPTION:	restore a string
 */
static char *restore_string(x, buf, val)
restcontext *x;
register char *buf;
value *val;
{
    register char *p, *q;

    if (*buf++ != '"') {
	restore_error(x, "'\"' expected");
    }
    for (p = q = buf; *p != '"'; p++) {
	if (*p == '\\') {
	    switch (*++p) {
	    case '0': *q++ = '\0'; continue;
	    case 'a': *q++ = BEL; continue;
	    case 'b': *q++ = BS; continue;
	    case 't': *q++ = HT; continue;
	    case 'n': *q++ = LF; continue;
	    case 'v': *q++ = VT; continue;
	    case 'f': *q++ = FF; continue;
	    case 'r': *q++ = CR; continue;
	    }
	}
	if (*p == '\0' || *p == LF) {
	    restore_error(x, "unterminated string");
	}
	*q++ = *p;
    }

    PUT_STRVAL_NOREF(val, str_new(buf, (long) q - (long) buf));
    return p + 1;
}

static char *restore_value	P((restcontext*, char*, value*));
static char *restore_mapping	P((restcontext*, char*, value*));

/*
 * NAME:	restore_array()
 * DESCRIPTION:	restore an array
 */
static char *restore_array(x, buf, val)
register restcontext *x;
register char *buf;
value *val;
{
    register unsigned short i;
    register value *v;
    array *a;
    
    /* match ({ */
    if (*buf++ != '(' || *buf++ != '{') {
	restore_error(x, "'({' expected");
    }
    /* get array size */
    buf = restore_int(x, buf, val);
    if (*buf++ != '|') {
	restore_error(x, "'|' expected");
    }

    ac_put(x, T_ARRAY, a = arr_new(x->f->data, (long) val->u.number));
    for (i = a->size, v = a->elts; i > 0; --i) {
	*v++ = nil_value;
    }
    i = a->size;
    v = a->elts;
    if (ec_push((ec_ftn) NULL)) {
	arr_ref(a);
	arr_del(a);
	error((char *) NULL);	/* pass on the error */
    }
    /* restore the values */
    while (i > 0) {
	buf = restore_value(x, buf, v);
	i_ref_value(v++);
	if (*buf++ != ',') {
	    restore_error(x, "',' expected");
	}
	--i;
    }
    /* match }) */
    if (*buf++ != '}' || *buf++ != ')') {
	restore_error(x, "'})' expected");
    }
    ec_pop();

    PUT_ARRVAL_NOREF(val, a);
    return buf;
}

/*
 * NAME:	restore_mapping()
 * DESCRIPTION:	restore a mapping
 */
static char *restore_mapping(x, buf, val)
register restcontext *x;
register char *buf;
value *val;
{
    register unsigned short i;
    register value *v;
    array *a;
    
    /* match ([ */
    if (*buf++ != '(' || *buf++ != '[') {
	restore_error(x, "'([' expected");
    }
    /* get mapping size */
    buf = restore_int(x, buf, val);
    if (*buf++ != '|') {
	restore_error(x, "'|' expected");
    }

    ac_put(x, T_MAPPING, a = map_new(x->f->data, (long) val->u.number << 1));
    for (i = a->size, v = a->elts; i > 0; --i) {
	*v++ = nil_value;
    }
    i = a->size;
    v = a->elts;
    if (ec_push((ec_ftn) NULL)) {
	arr_ref(a);
	arr_del(a);
	error((char *) NULL);	/* pass on the error */
    }
    /* restore the values */
    while (i > 0) {
	buf = restore_value(x, buf, v);
	i_ref_value(v++);
	if (*buf++ != ':') {
	    restore_error(x, "':' expected");
	}
	buf = restore_value(x, buf, v);
	i_ref_value(v++);
	if (*buf++ != ',') {
	    restore_error(x, "',' expected");
	}
	i -= 2;
    }
    /* match ]) */
    if (*buf++ != ']' || *buf++ != ')') {
	restore_error(x, "'])' expected");
    }
    map_sort(a);
    ec_pop();

    PUT_MAPVAL_NOREF(val, a);
    return buf;
}

/*
 * NAME:	restore_value()
 * DESCRIPTION:	restore a value
 */
static char *restore_value(x, buf, val)
register restcontext *x;
register char *buf;
register value *val;
{
    switch (*buf) {
    case '"':
	return restore_string(x, buf, val);

    case '(':
	if (buf[1] == '{') {
	    return restore_array(x, buf, val);
	} else {
	    return restore_mapping(x, buf, val);
	}

    case 'n':
	/* nil */
	if (buf[1] != 'i' || buf[2] != 'l') {
	    restore_error(x, "nil expected");
	}
	*val = nil_value;
	return buf + 3;

    case '#':
	buf = restore_int(x, buf + 1, val);
	if ((Uint) val->u.number >= x->narrays) {
	    restore_error(x, "bad array reference");
	}
	*val = *ac_get(x, (Uint) val->u.number);
	if (val->type != T_ARRAY) {
	    restore_error(x, "bad array reference");
	}
	return buf;

    case '@':
	buf = restore_int(x, buf + 1, val);
	if ((Uint) val->u.number >= x->narrays) {
	    restore_error(x, "bad mapping reference");
	}
	*val = *ac_get(x, (Uint) val->u.number);
	if (val->type != T_MAPPING) {
	    restore_error(x, "bad mapping reference");
	}
	return buf;

    default:
	return restore_number(x, buf, val);
    }
}

char pt_restore_object[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT,
			     T_STRING };

/*
 * NAME:	kfun->restore_object()
 * DESCRIPTION:	restore the variables of the current object from file
 */
int kf_restore_object(f)
register frame *f;
{
    struct stat sbuf;
    register int i, j;
    register unsigned short nvars, checkpoint;
    register char *buf;
    register value *var;
    register dvardef *v;
    register control *ctrl;
    register dataspace *data;
    register dinherit *inh;
    restcontext x;
    object *obj;
    int fd;
    char *buffer, *name;
    bool onstack, pending;

    obj = OBJR(f->oindex);
    if (path_string(x.file, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 1;
    }

    i_add_ticks(f, 2000);	/* arbitrary */
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, 0);
    fd = P_open(x.file, O_RDONLY | O_BINARY, 0);
    if (fd < 0) {
	/* restore failed */
	return 0;
    }
    P_fstat(fd, &sbuf);
    if ((sbuf.st_mode & S_IFMT) != S_IFREG) {
	/* not a save file */
	P_close(fd);
	return 0;
    }
    buffer = ALLOCA(char, sbuf.st_size + 1);
    if (buffer == (char *) NULL) {
	buffer = ALLOC(char, sbuf.st_size + 1);
	onstack = FALSE;
    } else {
	onstack = TRUE;
    }
    if (P_read(fd, buffer, (unsigned int) sbuf.st_size) != sbuf.st_size) {
	/* read failed (should never happen, but...) */
        P_close(fd);
	if (onstack) {
	    AFREE(buffer);
	} else {
	    FREE(buffer);
	}
	return 0;
    }
    buffer[sbuf.st_size] = '\0';
    P_close(fd);

    /*
     * First, reset all non-static variables that do not hold object values.
     */
    ctrl = o_control(obj);
    data = o_dataspace(obj);
    if (f->lwobj != (array *) NULL) {
	var = &f->lwobj->elts[2];
    } else {
	var = d_get_variable(data, 0);
    }
    nvars = 0;
    for (i = ctrl->ninherits, inh = ctrl->inherits; i > 0; --i, inh++) {
	if (inh->varoffset == nvars) {
	    /*
	     * This is the program that has the next variables in the object.
	     */
	    ctrl = o_control(OBJR(inh->oindex));
	    if (inh->priv) {
		/* skip privately inherited variables */
		var += ctrl->nvardefs;
		nvars += ctrl->nvardefs;
		continue;
	    }
	    for (j = ctrl->nvardefs, v = d_get_vardefs(ctrl); j > 0; --j, v++) {
		if (!(v->class & C_STATIC) && var->type != T_OBJECT &&
		    var->type != T_LWOBJECT) {
		    d_assign_var(data, var,
				 (v->type == T_INT) ?
				  &zero_int : (v->type == T_FLOAT) ?
					       &zero_float : &nil_value);
		}
		var++;
		nvars++;
	    }
	}
    }

    x.line = 1;
    x.f = f;
    x.alist = (achunk *) NULL;
    x.achunksz = ACHUNKSZ;
    x.narrays = 0;
    buf = buffer;
    pending = FALSE;
    if (ec_push((ec_ftn) NULL)) {
	/* error; clean up */
	ac_clear(&x);
	if (onstack) {
	    AFREE(buffer);
	} else {
	    FREE(buffer);
	}
	error((char *) NULL);	/* pass on error */
    }
    for (;;) {
	if (f->lwobj != (array *) NULL) {
	    var = &f->lwobj->elts[2];
	} else {
	    var = data->variables;
	}
	nvars = 0;
	for (i = ctrl->ninherits, inh = ctrl->inherits; i > 0; --i, inh++) {
	    if (inh->varoffset == nvars) {
		/*
		 * Restore non-static variables.
		 */
		ctrl = OBJR(inh->oindex)->ctrl;
		if (inh->priv) {
		    /* skip privately inherited variables */
		    var += ctrl->nvardefs;
		    nvars += ctrl->nvardefs;
		    continue;
		}
		for (j = ctrl->nvardefs, v = ctrl->vardefs; j > 0; --j, v++) {
		    if (pending && nvars == checkpoint) {
			/*
			 * The saved variable is not in this object.
			 * Skip it.
			 */
			buf = strchr(buf, LF);
			if (buf == (char *) NULL) {
			    restore_error(&x, "'\\n' expected");
			}
			buf++;
			x.line++;
			pending = FALSE;
		    }
		    if (!pending) {
			/*
			 * get a new variable name from the save file
			 */
			while (*buf == '#') {
			    /* skip comment */
			    buf = strchr(buf, LF);
			    if (buf == (char *) NULL) {
				restore_error(&x, "'\\n' expected");
			    }
			    buf++;
			    x.line++;
			}
			if (*buf == '\0') {
			    /* end of file */
			    break;
			}

			name = buf;
			if (!isalpha(*buf) && *buf != '_') {
			    restore_error(&x, "alphanumeric expected");
			}
			do {
			    buf++;
			} while (isalnum(*buf) || *buf == '_');
			if (*buf != ' ') {
			    restore_error(&x, "' ' expected");
			}

			*buf++ = '\0';		/* terminate name */
			pending = TRUE;		/* start checking variables */
			checkpoint = nvars;	/* from here */
		    }

		    if (!(v->class & C_STATIC) &&
			strcmp(name, d_get_strconst(ctrl, v->inherit,
						    v->index)->text) == 0) {
			value tmp;

			/*
			 * found the proper variable to restore
			 */
			buf = restore_value(&x, buf, &tmp);
			if (v->type != tmp.type && v->type != T_MIXED &&
			    conf_typechecking() &&
			    (!VAL_NIL(&tmp) || !T_POINTER(v->type)) &&
			    (tmp.type != T_ARRAY || (v->type & T_REF) == 0)) {
			    i_ref_value(&tmp);
			    i_del_value(&tmp);
			    restore_error(&x, "value has wrong type");
			}
			if (f->lwobj != (array *) NULL) {
			    d_assign_elt(data, f->lwobj, var, &tmp);
			} else {
			    d_assign_var(data, var, &tmp);
			}
			if (*buf++ != LF) {
			    restore_error(&x, "'\\n' expected");
			}
			x.line++;
			pending = FALSE;
		    }
		    var++;
		    nvars++;
		}
		if (!pending && *buf == '\0') {
		    /*
		     * finished restoring
		     */
		    ec_pop();
		    ac_clear(&x);
		    if (onstack) {
			AFREE(buffer);
		    } else {
			FREE(buffer);
		    }
		    f->sp->u.number = 1;
		    return 0;
		}
	    }
	}
    }
}
# endif


# ifdef FUNCDEF
FUNCDEF("write_file", kf_write_file, pt_write_file, 0)
# else
char pt_write_file[] = { C_TYPECHECKED | C_STATIC, 2, 1, 0, 9, T_INT, T_STRING,
			 T_STRING, T_INT };

/*
 * NAME:	kfun->write_file()
 * DESCRIPTION:	write a string to a file
 */
int kf_write_file(f, nargs)
register frame *f;
int nargs;
{
    char file[STRINGSZ];
    struct stat sbuf;
    register Int l;
    int fd;

    l = (nargs < 3) ? 0 : (f->sp++)->u.number;
    if (path_string(file, f->sp[1].u.string->text,
		    f->sp[1].u.string->len) == (char *) NULL) {
	return 1;
    }
    if (f->level != 0) {
	error("write_file() within atomic function");
    }

    i_add_ticks(f, 1000 + (Int) 2 * f->sp->u.string->len);
    str_del(f->sp[1].u.string);
    PUT_INTVAL(&f->sp[1], 0);

    fd = P_open(file, O_CREAT | O_WRONLY | O_BINARY, 0664);
    if (fd < 0) {
	str_del((f->sp++)->u.string);
	return 0;
    }

    P_fstat(fd, &sbuf);
    if (l == 0) {
	/* the default is to append to the file */
	l = sbuf.st_size;
    } else if (l < 0) {
	/* offset from the end of the file */
	l += sbuf.st_size;
    }
    if (l < 0 || l > sbuf.st_size || (l != 0 && P_lseek(fd, l, SEEK_SET) < 0)) {
	/* bad offset */
	P_close(fd);
	return 3;
    }

    if (P_write(fd, f->sp->u.string->text, f->sp->u.string->len) ==
							f->sp->u.string->len) {
	/* succesful write */
	PUT_INT(&f->sp[1], 1);
    }
    P_close(fd);

    str_del((f->sp++)->u.string);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("read_file", kf_read_file, pt_read_file, 0)
# else
char pt_read_file[] = { C_TYPECHECKED | C_STATIC, 1, 2, 0, 9, T_STRING,
			T_STRING, T_INT, T_INT };

/*
 * NAME:	kfun->read_file()
 * DESCRIPTION:	read a string from file
 */
int kf_read_file(f, nargs)
register frame *f;
int nargs;
{
    char file[STRINGSZ], *buf;
    struct stat sbuf;
    register Int l, size;
    static int fd;

    l = 0;
    size = 0;
    switch (nargs) {
    case 3:
	size = (f->sp++)->u.number;
    case 2:
	l = (f->sp++)->u.number;	/* offset in file */
	break;
    }
    if (path_string(file, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 1;
    }

    str_del(f->sp->u.string);
    *f->sp = nil_value;

    if (size < 0) {
	/* size has to be >= 0 */
	return 3;
    }
    i_add_ticks(f, 1000);
    fd = P_open(file, O_RDONLY | O_BINARY, 0);
    if (fd < 0) {
	/* cannot open file */
	return 0;
    }
    P_fstat(fd, &sbuf);
    if ((sbuf.st_mode & S_IFMT) == S_IFDIR) {
	/* don't read from a directory */
	P_close(fd);
	return 0;
    }

    if (l != 0) {
	/*
	 * seek in file
	 */
	if (l < 0) {
	    /* offset from end of file */
	    l += sbuf.st_size;
	}
	if (l < 0 || l > sbuf.st_size ||
	    (l != 0 && P_lseek(fd, l, SEEK_SET) < 0)) {
	    /* bad seek */
	    P_close(fd);
	    return 1;
	}
	sbuf.st_size -= l;
    }

    if (size == 0 || size > sbuf.st_size) {
	size = sbuf.st_size;
    }
    if (size > (Uint) MAX_STRLEN) {
	P_close(fd);
	error("String too long");
    }
    buf = (size != 0) ? ALLOCA(char, size) : (char *) NULL;
    if (size > 0 && (size=P_read(fd, buf, (unsigned int) size)) < 0) {
	/* read failed */
	P_close(fd);
	if (buf != (char *) NULL) {
	    AFREE(buf);
	}
	error("Read failed in read_file()");
    }
    P_close(fd);
    i_add_ticks(f, 2 * size);

    PUT_STRVAL(f->sp, str_new((char *) buf, size));
    if (buf != (char *) NULL) {
	AFREE(buf);
    }

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("rename_file", kf_rename_file, pt_rename_file, 0)
# else
char pt_rename_file[] = { C_TYPECHECKED | C_STATIC, 2, 0, 0, 8, T_INT,
			  T_STRING, T_STRING };

/*
 * NAME:	kfun->rename_file()
 * DESCRIPTION:	rename a file
 */
int kf_rename_file(f)
register frame *f;
{
    char from[STRINGSZ], to[STRINGSZ];

    if (path_string(from, f->sp[1].u.string->text,
		    f->sp[1].u.string->len) == (char *) NULL) {
	return 1;
    }
    if (path_string(to, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 2;
    }

    i_add_ticks(f, 1000);
    str_del((f->sp++)->u.string);
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, (P_access(from, W_OK) >= 0 && P_access(to, F_OK) < 0 &&
		       P_rename(from, to) >= 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("remove_file", kf_remove_file, pt_remove_file, 0)
# else
char pt_remove_file[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT,
			  T_STRING };

/*
 * NAME:	kfun->remove_file()
 * DESCRIPTION:	remove a file
 */
int kf_remove_file(f)
register frame *f;
{
    char file[STRINGSZ];

    if (path_string(file, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 1;
    }
    if (f->level != 0) {
	error("remove_file() within atomic function");
    }

    i_add_ticks(f, 1000);
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, (P_access(file, W_OK) >= 0 && P_unlink(file) >= 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("make_dir", kf_make_dir, pt_make_dir, 0)
# else
char pt_make_dir[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT, T_STRING };

/*
 * NAME:	kfun->make_dir()
 * DESCRIPTION:	create a directory
 */
int kf_make_dir(f)
register frame *f;
{
    char file[STRINGSZ];

    if (path_string(file, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 1;
    }
    if (f->level != 0) {
	error("make_dir() within atomic function");
    }

    i_add_ticks(f, 1000);
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, (P_mkdir(file, 0775) >= 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("remove_dir", kf_remove_dir, pt_remove_dir, 0)
# else
char pt_remove_dir[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7, T_INT,
			 T_STRING };

/*
 * NAME:	kfun->remove_dir()
 * DESCRIPTION:	remove an empty directory
 */
int kf_remove_dir(f)
register frame *f;
{
    char file[STRINGSZ];

    if (path_string(file, f->sp->u.string->text,
		    f->sp->u.string->len) == (char *) NULL) {
	return 1;
    }
    if (f->level != 0) {
	error("remove_dir() within atomic function");
    }

    i_add_ticks(f, 1000);
    str_del(f->sp->u.string);
    PUT_INTVAL(f->sp, (P_rmdir(file) >= 0));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("get_dir", kf_get_dir, pt_get_dir, 0)
# else
/*
 * NAME:	match()
 * DESCRIPTION:	match a regular expression
 */
static int match(pat, text)
register char *pat, *text;
{
    bool found, reversed;
    int matched;

    for (;;) {
	switch (*pat) {
	case '\0':
	    /* end of pattern */
	    return (*text == '\0');

	case '?':
	    /* any single character */
	    if (*text == '\0') {
		return 0;
	    }
	    break;

	case '*':
	    /* any string */
	    pat++;
	    if (*pat == '\0') {
		/* quick check */
		return 1;
	    }
	    do {
		matched = match(pat, text);
		if (matched != 0) {
		    return matched;
		}
	    } while (*text++ != '\0');
	    return -1;

	case '[':
	    /* character class */
	    pat++;
	    found = FALSE;
	    if (*pat == '^') {
		reversed = TRUE;
		pat++;
	    } else {
		reversed = FALSE;
	    }
	    for (;;) {
		if (*pat == '\0') {
		    /* missing ']' */
		    return 0;
		}
		if (*pat == ']') {
		    /* end of character class */
		    if (found != reversed) {
			break;
		    }
		    return 0;
		}
		if (*pat == '\\') {
		    /* escaped char (should be ']') */
		    ++pat;
		    if (*pat == '\0') {
			return 0;
		    }
		}
		if (pat[1] == '-') {
		    /* character range */
		    pat += 2;
		    if (*pat == '\0') {
			return 0;
		    }
		    if (UCHAR(*text) >= UCHAR(pat[-2]) &&
			UCHAR(*text) <= UCHAR(pat[0])) {
			found = TRUE;
		    }
		} else if (*pat == *text) {
		    /* matched single character */
		    found = TRUE;
		}
		pat++;
	    }
	    break;

	case '\\':
	    /* escaped character */
	    if (*++pat == '\0') {
		/* malformed pattern */
		return 0;
	    }
	default:
	    /* ordinary character */
	    if (*pat != *text) {
		return 0;
	    }
	}
	pat++;
	text++;
    }
}

typedef struct _fileinfo_ {
    string *name;		/* file name */
    Int size;			/* file size */
    Int time;			/* file time */
} fileinfo;

/*
 * NAME:	getinfo()
 * DESCRIPTION:	get info about a file
 */
static bool getinfo(path, file, finf)
char *path, *file;
register fileinfo *finf;
{
    struct stat sbuf;

    if (P_stat(path, &sbuf) < 0) {
	/*
	 * the file does not exist
	 */
	return FALSE;
    }

    str_ref(finf->name = str_new(file, (long) strlen(file)));
    if ((sbuf.st_mode & S_IFMT) == S_IFDIR) {
	finf->size = -2;	/* special value for directory */
    } else {
	finf->size = sbuf.st_size;
    }
    finf->time = sbuf.st_mtime;

    return TRUE;
}

static int cmp P((cvoid*, cvoid*));

/*
 * NAME:	cmp()
 * DESCRIPTION:	compare two file info structs
 */
static int cmp(cv1, cv2)
cvoid *cv1, *cv2;
{
    return strcmp(((fileinfo *) cv1)->name->text,
		  ((fileinfo *) cv2)->name->text);
}

char pt_get_dir[] = { C_TYPECHECKED | C_STATIC, 1, 0, 0, 7,
		      T_MIXED | (2 << REFSHIFT), T_STRING };

# define FILEINFO_CHUNK	128

/*
 * NAME:	kfun->get_dir()
 * DESCRIPTION:	get directory filelist + info
 */
int kf_get_dir(f)
frame *f;
{
    register unsigned int i, nfiles, ftabsz;
    register fileinfo *ftable;
    char *file, *dir, *pat, buf[STRINGSZ], dirbuf[STRINGSZ];
    fileinfo finf;
    array *a;

    file = path_string(buf, f->sp->u.string->text, f->sp->u.string->len);

    strcpy(dirbuf, buf);
    pat = strrchr(dirbuf, '/');
    if (pat == (char *) NULL) {
	dir = ".";
	pat = dirbuf;
    } else {
	/* separate directory and pattern */
	dir = dirbuf;
	*pat++ = '\0';
    }

    ftable = ALLOCA(fileinfo, ftabsz = FILEINFO_CHUNK);
    nfiles = 0;
    if (strpbrk(pat, "?*[\\") == (char *) NULL &&
	getinfo(file, pat, &ftable[0])) {
	/*
	 * single file
	 */
	nfiles++;
    } else if (strcmp(dir, ".") == 0 || P_chdir(dir) >= 0) {
	if (P_opendir(".")) {
	    /*
	     * read files from directory
	     */
	    i = conf_array_size();
	    while (nfiles < i && (file=P_readdir()) != (char *) NULL) {
		if (match(pat, file) > 0 && getinfo(file, file, &finf)) {
		    /* add file */
		    if (nfiles == ftabsz) {
			fileinfo *tmp;

			tmp = ALLOCA(fileinfo, ftabsz + FILEINFO_CHUNK);
			memcpy(tmp, ftable, ftabsz * sizeof(fileinfo));
			ftabsz += FILEINFO_CHUNK;
			AFREE(ftable);
			ftable = tmp;
		    }
		    ftable[nfiles++] = finf;
		}
	    }
	    P_closedir();
	}

	if (strcmp(dir, ".") != 0 &&
	    P_chdir(path_native(buf, conf_base_dir())) < 0) {
	    fatal("cannot chdir back to base dir");
	}
    }

    /* prepare return value */
    str_del(f->sp->u.string);
    PUT_ARRVAL(f->sp, a = arr_new(f->data, 3L));
    PUT_ARRVAL(&a->elts[0], arr_new(f->data, (long) nfiles));
    PUT_ARRVAL(&a->elts[1], arr_new(f->data, (long) nfiles));
    PUT_ARRVAL(&a->elts[2], arr_new(f->data, (long) nfiles));

    i_add_ticks(f, 1000 + 5 * nfiles);

    if (nfiles != 0) {
	register value *n, *s, *t;

	qsort(ftable, nfiles, sizeof(fileinfo), cmp);
	n = a->elts[0].u.array->elts;
	s = a->elts[1].u.array->elts;
	t = a->elts[2].u.array->elts;
	for (i = nfiles; i > 0; --i, ftable++) {
	    PUT_STRVAL_NOREF(n, ftable->name);
	    PUT_INTVAL(s, ftable->size);
	    PUT_INTVAL(t, ftable->time);
	    n++, s++, t++;
	}
	ftable -= nfiles;
    }
    AFREE(ftable);

    return 0;
}
# endif
