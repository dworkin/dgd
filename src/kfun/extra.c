# ifndef FUNCDEF
# define INCLUDE_CTYPE
# include "kfun.h"
# endif


# ifdef FUNCDEF
FUNCDEF("crypt", kf_crypt, pt_crypt)
# else
char pt_crypt[] = { C_TYPECHECKED | C_STATIC | C_VARARGS, T_STRING, 2,
		    T_STRING, T_STRING };

/*
 * NAME:	kfun->crypt()
 * DESCRIPTION:	encrypt a string
 */
int kf_crypt(f, nargs)
register frame *f;
int nargs;
{
    static char salts[] =
	    "0123456789./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char salt[3], *p;
    
    if (nargs == 0) {
	return -1;
    }

    if (nargs == 2 && f->sp->u.string->len >= 2) {
	/* fixed salt */
	salt[0] = f->sp->u.string->text[0];
	salt[1] = f->sp->u.string->text[1];
    } else {
	/* random salt */
	salt[0] = salts[P_random() % 64];
	salt[1] = salts[P_random() % 64];
    }
    salt[2] = '\0';
    if (nargs == 2) {
	str_del((f->sp++)->u.string);
    }

    i_add_ticks(f, 400);
    p = P_crypt(f->sp->u.string->text, salt);
    str_del(f->sp->u.string);
    str_ref(f->sp->u.string = str_new(p, (long) strlen(p)));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ctime", kf_ctime, pt_ctime)
# else
char pt_ctime[] = { C_TYPECHECKED | C_STATIC, T_STRING, 1, T_INT };

/*
 * NAME:	kfun->ctime()
 * DESCRIPTION:	convert a time value to a string
 */
int kf_ctime(f)
frame *f;
{
    i_add_ticks(f, 5);
    f->sp->type = T_STRING;
    str_ref(f->sp->u.string = str_new(P_ctime((Uint) f->sp->u.number), 24L));

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("explode", kf_explode, pt_explode)
# else
char pt_explode[] = { C_TYPECHECKED | C_STATIC, T_STRING | (1 << REFSHIFT), 2,
		      T_STRING, T_STRING };

/*
 * NAME:	kfun->explode()
 * DESCRIPTION:	explode a string
 */
int kf_explode(f)
register frame *f;
{
    register unsigned int len, slen, size;
    register char *p, *s;
    register value *v;
    array *a;

    p = f->sp[1].u.string->text;
    len = f->sp[1].u.string->len;
    s = f->sp->u.string->text;
    slen = f->sp->u.string->len;

    if (len == 0) {
	/*
	 * exploding "" always gives an empty array
	 */
	a = arr_new(f->data, 0L);
    } else if (slen == 0) {
	/*
	 * the sepatator is ""; split string into single characters
	 */
	a = arr_new(f->data, (long) len);
	for (v = a->elts; len > 0; v++, --len) {
	    v->type = T_STRING;
	    str_ref(v->u.string = str_new(p, 1L));
	    p++;
	}
    } else {
	/*
	 * split up the string with the separator
	 */
	size = 1;
	if (len >= slen && memcmp(p, s, slen) == 0) {
	    /* skip leading separator */
	    p += slen;
	    len -= slen;
	}
	while (len > slen) {
	    if (memcmp(p, s, slen) == 0) {
		/* separator found */
		p += slen;
		len -= slen;
		size++;
	    } else {
		/* next char */
		p++;
		--len;
	    }
	}

	a = arr_new(f->data, (long) size);
	v = a->elts;

	p = f->sp[1].u.string->text;
	len = f->sp[1].u.string->len;
	size = 0;
	if (len > slen && memcmp(p, s, slen) == 0) {
	    /* skip leading separator */
	    p += slen;
	    len -= slen;
	}
	while (len > slen) {
	    if (memcmp(p, s, slen) == 0) {
		/* separator found */
		v->type = T_STRING;
		str_ref(v->u.string = str_new(p - size, (long) size));
		v++;
		p += slen;
		len -= slen;
		size = 0;
	    } else {
		/* next char */
		p++;
		--len;
		size++;
	    }
	}
	if (len != slen || memcmp(p, s, slen) != 0) {
	    /* remainder isn't a sepatator */
	    size += len;
	    p += len;
	}
	/* final array element */
	v->type = T_STRING;
	str_ref(v->u.string = str_new(p - size, (long) size));
    }

    str_del((f->sp++)->u.string);
    str_del(f->sp->u.string);
    f->sp->type = T_ARRAY;
    arr_ref(f->sp->u.array = a);
    i_add_ticks(f, (Int) 2 * a->size);

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("implode", kf_implode, pt_implode)
# else
char pt_implode[] = { C_TYPECHECKED | C_STATIC, T_STRING, 2,
		      T_STRING | (1 << REFSHIFT), T_STRING };

/*
 * NAME:	kfun->implode()
 * DESCRIPTION:	implode an array
 */
int kf_implode(f)
register frame *f;
{
    register long len;
    register unsigned int i, slen;
    register char *p, *s;
    register value *v;
    string *str;

    s = f->sp->u.string->text;
    slen = f->sp->u.string->len;

    /* first, determine the size of the imploded string */
    i = f->sp[1].u.array->size;
    i_add_ticks(f, i);
    if (i != 0) {
	len = (i - 1) * (long) slen;	/* size of all separators */
	for (v = d_get_elts(f->sp[1].u.array); i > 0; v++, --i) {
	    if (v->type != T_STRING) {
		/* not a (string *) */
		return 1;
	    }
	    len += v->u.string->len;
	}
	str = str_new((char *) NULL, len);

	/* create the imploded string */
	p = str->text;
	for (i = f->sp[1].u.array->size, v -= i; i > 1; --i, v++) {
	    /* copy array part */
	    memcpy(p, v->u.string->text, v->u.string->len);
	    p += v->u.string->len;
	    /* copy separator */
	    memcpy(p, s, slen);
	    p += slen;
	}
	/* copy final array part */
	memcpy(p, v->u.string->text, v->u.string->len);
    } else {
	/* zero size array gives zero size string */
	str = str_new((char *) NULL, 0L);
    }

    str_del((f->sp++)->u.string);
    arr_del(f->sp->u.array);
    f->sp->type = T_STRING;
    str_ref(f->sp->u.string = str);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("random", kf_random, pt_random)
# else
char pt_random[] = { C_TYPECHECKED | C_STATIC, T_INT, 1, T_INT };

/*
 * NAME:	kfun->random()
 * DESCRIPTION:	return a random number
 */
int kf_random(f)
register frame *f;
{
    i_add_ticks(f, 1);
    f->sp->u.number = (f->sp->u.number > 0) ? P_random() % f->sp->u.number : 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sscanf", kf_sscanf, pt_sscanf)
# else
char pt_sscanf[] = { C_TYPECHECKED | C_STATIC | C_VARARGS, T_INT, 3,
		     T_STRING, T_STRING, T_LVALUE | T_ELLIPSIS };

/*
 * NAME:	match
 * DESCRIPTION:	match a string possibly including %%, up to the next %[sdfc] or
 *		the end of the string
 */
static bool match(f, s, flenp, slenp)
register char *f, *s;
unsigned int *flenp, *slenp;
{
    register char *p;
    register unsigned int flen, slen;

    flen = *flenp;
    slen = *slenp;

    while (flen > 0) {
	/* look for first % */
	p = (char *) memchr(f, '%', flen);

	if (p == (char *) NULL) {
	    /* no remaining % */
	    if (memcmp(f, s, flen) == 0) {
		*slenp -= slen - flen;
		return TRUE;
	    } else {
		return FALSE;	/* no match */
	    }
	}

	if (p[1] == '%') {
	    /* %% */
	    if (memcmp(f, s, ++p - f) == 0) {
		/* matched up to and including the first % */
		s += p - f;
		slen -= p - f;
		flen -= ++p - f;
		f = p;
	    } else {
		return FALSE;	/* no match */
	    }
	} else if (memcmp(f, s, p - f) == 0) {
	    /* matched up to the first % */
	    *flenp -= flen - (p - f);
	    *slenp -= slen - (p - f);
	    return TRUE;
	} else {
	    return FALSE;	/* no match */
	}
    }

    *slenp -= slen;
    return TRUE;
}

/*
 * NAME:	kfun->sscanf()
 * DESCRIPTION:	scan a string
 */
int kf_sscanf(f, nargs)
register frame *f;
int nargs;
{
    register unsigned int flen, slen, size;
    register char *format, *x;
    value values[MAX_LOCALS], *val;
    unsigned int fl, sl;
    int matches;
    char *s;
    xfloat flt;
    bool skip;

    if (nargs < 2) {
	return -1;
    } else if (nargs > MAX_LOCALS + 2) {
	return 4;
    }
    s = f->sp[nargs - 1].u.string->text;
    slen = f->sp[nargs - 1].u.string->len;
    format = f->sp[nargs - 2].u.string->text;
    flen = f->sp[nargs - 2].u.string->len;

    nargs -= 2;
    i_add_ticks(f, 8 * nargs);
    val = values;
    matches = 0;

    while (flen > 0) {
	if (format[0] != '%' || format[1] == '%') {
	    /* match initial part */
	    fl = flen;
	    sl = slen;
	    if (!match(format, s, &fl, &sl) || fl == flen) {
		goto no_match;
	    }
	    format += fl;
	    flen -= fl;
	    s += sl;
	    slen -= sl;
	}

	/* skip first % */
	format++;
	--flen;

	/*
	 * check for %*
	 */
	if (*format == '*') {
	    /* no assignment */
	    format++;
	    --flen;
	    skip = TRUE;
	} else {
	    skip = FALSE;
	}

	--flen;
	switch (*format++) {
	case 's':
	    /* %s */
	    if (format[0] == '%' && format[1] != '%') {
		switch ((format[1] == '*') ? format[2] : format[1]) {
		case 'd':
		    /*
		     * %s%d
		     */
		    size = slen;
		    x = s;
		    while (!isdigit(*x)) {
			if (slen == 0) {
			    goto no_match;
			}
			if (x[0] == '-' && isdigit(x[1])) {
			    break;
			}
			x++;
			--slen;
		    }
		    size -= slen;
		    break;

		case 'f':
		    /*
		     * %s%f
		     */
		    size = slen;
		    x = s;
		    while (!isdigit(*x)) {
			if (slen == 0) {
			    goto no_match;
			}
			if ((x[0] == '-' || x[0] == '.') && isdigit(x[1])) {
			    break;
			}
			x++;
			--slen;
		    }
		    size -= slen;
		    break;

		default:
		    s = "Bad sscanf format string";
		    goto err;
		}
	    } else {
		/*
		 * %s followed by non-%
		 */
		if (flen == 0) {
		    /* match whole string */
		    size = slen;
		    x = s + slen;
		    slen = 0;
		} else {
		    /* get # of chars to match after string */
		    for (x = format, size = 0; (x - format) != flen;
			 x++, size++) {
			x = (char *) memchr(x, '%', flen - (x - format));
			if (x == (char *) NULL) {
			    x = format + flen;
			    break;
			} else if (x[1] != '%') {
			    break;
			}
		    }
		    size = (x - format) - size;

		    x = s;
		    for (;;) {
			sl = slen - (x - s);
			if (sl < size) {
			    goto no_match;
			}
			x = (char *) memchr(x, format[0], sl - size + 1);
			if (x == (char *) NULL) {
			    goto no_match;
			}
			fl = flen;
			if (match(format, x, &fl, &sl)) {
			    format += fl;
			    flen -= fl;
			    size = x - s;
			    x += sl;
			    slen -= size + sl;
			    break;
			}
			x++;
		    }
		}
	    }

	    if (!skip) {
		if (nargs == 0) {
		    s = "No lvalue for %%s";
		    goto err;
		}
		--nargs;
		val->type = T_STRING;
		val->u.string = str_new(s, (long) size);
		val++;
	    }
	    s = x;
	    break;

	case 'd':
	    /* %d */
	    x = s;
	    val->u.number = strtol(s, &s, 10);
	    if (s == x) {
		goto no_match;
	    }
	    slen -= (s - x);

	    if (!skip) {
		if (nargs == 0) {
		    s = "No lvalue for %%d";
		    goto err;
		}
		--nargs;
		val->type = T_INT;
		val++;
	    }
	    break;

	case 'f':
	    /* %f */
	    x = s;
	    if (!flt_atof(&s, &flt)) {
		goto no_match;
	    }
	    slen -= (s - x);

	    if (!skip) {
		if (nargs == 0) {
		    s = "No lvalue for %%f";
		    goto err;
		}
		--nargs;
		val->type = T_FLOAT;
		VFLT_PUT(val, flt);
		val++;
	    }
	    break;

	case 'c':
	    /* %c */
	    if (slen == 0) {
		goto no_match;
	    }
	    if (!skip) {
		if (nargs == 0) {
		    s = "No lvalue for %%c";
		    goto err;
		}
		--nargs;
		val->type = T_INT;
		val->u.number = UCHAR(*s);
		val++;
	    }
	    s++;
	    --slen;
	    break;

	default:
	    s = "Bad sscanf format string";
	    goto err;
	}
	matches++;
    }

no_match:
    if (nargs > 0) {
	/* pop superfluous arguments */
	i_pop(f, nargs);
    }
    while (val > values) {
	i_store(f, f->sp, (value *) val - 1);
	f->sp++;
	--val;
    }

    str_del((f->sp++)->u.string);
    str_del(f->sp->u.string);
    f->sp->type = T_INT;
    f->sp->u.number = matches;
    return 0;

err:
    /*
     * free any values left unassigned
     */
    while (val > values) {
	if ((--val)->type == T_STRING) {
	    str_ref(val->u.string);
	    str_del(val->u.string);
	}
    }
    error(s);
}
# endif


# ifdef FUNCDEF
FUNCDEF("parse_string", kf_parse_string, pt_parse_string)
# else
char pt_parse_string[] = { C_TYPECHECKED | C_STATIC, T_MIXED | (1 << REFSHIFT),
			   2, T_STRING, T_STRING };

/*
 * NAME:	kfun->parse_string()
 * DESCRIPTION:	parse a string
 */
int kf_parse_string()
{
    error("Not yet implemented");
}
# endif
