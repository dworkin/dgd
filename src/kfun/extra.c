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
int kf_crypt(nargs)
int nargs;
{
    static char salts[] =
	    "0123456789./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char salt[3], *p;
    
    if (nargs == 0) {
	return -1;
    }

    if (nargs == 2 && sp->u.string->len >= 2) {
	/* fixed salt */
	salt[0] = sp->u.string->text[0];
	salt[1] = sp->u.string->text[1];
    } else {
	/* random salt */
	salt[0] = salts[P_random() % 64];
	salt[1] = salts[P_random() % 64];
    }
    salt[2] = '\0';
    if (nargs == 2) {
	str_del((sp++)->u.string);
    }

    i_add_ticks(400);
    p = P_crypt(sp->u.string->text, salt);
    str_del(sp->u.string);
    str_ref(sp->u.string = str_new(p, (long) strlen(p)));
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
int kf_ctime()
{
    i_add_ticks(5);
    sp->type = T_STRING;
    str_ref(sp->u.string = str_new(P_ctime((Uint) sp->u.number), 24L));

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
int kf_explode()
{
    register unsigned int len, slen, size;
    register char *p, *s;
    register value *v;
    array *a;

    p = sp[1].u.string->text;
    len = sp[1].u.string->len;
    s = sp->u.string->text;
    slen = sp->u.string->len;

    if (len == 0) {
	/*
	 * exploding "" always gives an empty array
	 */
	a = arr_new(0L);
    } else if (slen == 0) {
	/*
	 * the sepatator is ""; split string into single characters
	 */
	a = arr_new((long) len);
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

	a = arr_new((long) size);
	v = a->elts;

	p = sp[1].u.string->text;
	len = sp[1].u.string->len;
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

    str_del((sp++)->u.string);
    str_del(sp->u.string);
    sp->type = T_ARRAY;
    arr_ref(sp->u.array = a);
    i_add_ticks((Int) 2 * a->size);

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
int kf_implode()
{
    register long len;
    register unsigned int i, slen;
    register char *p, *s;
    register value *v;
    string *str;

    s = sp->u.string->text;
    slen = sp->u.string->len;

    /* first, determine the size of the imploded string */
    i = sp[1].u.array->size;
    i_add_ticks(i);
    if (i != 0) {
	len = (i - 1) * (long) slen;	/* size of all separators */
	for (v = d_get_elts(sp[1].u.array); i > 0; v++, --i) {
	    if (v->type != T_STRING) {
		/* not a (string *) */
		return 1;
	    }
	    len += v->u.string->len;
	}
	str = str_new((char *) NULL, len);

	/* create the imploded string */
	p = str->text;
	for (i = sp[1].u.array->size, v -= i; i > 1; --i, v++) {
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

    str_del((sp++)->u.string);
    arr_del(sp->u.array);
    sp->type = T_STRING;
    str_ref(sp->u.string = str);
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
int kf_random()
{
    i_add_ticks(1);
    sp->u.number = (sp->u.number > 0) ? P_random() % sp->u.number : 0;
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
int kf_sscanf(nargs)
int nargs;
{
    register unsigned int flen, slen, size;
    register char *f, *x;
    value values[MAX_LOCALS];
    value *oldval;
    static value *val;
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
    s = sp[nargs - 1].u.string->text;
    slen = sp[nargs - 1].u.string->len;
    f = sp[nargs - 2].u.string->text;
    flen = sp[nargs - 2].u.string->len;

    nargs -= 2;
    i_add_ticks(8 * nargs);
    oldval = (value *) val;
    val = values;
    matches = 0;

    sp += nargs;	/* to get the error context right */
    if (ec_push((ec_ftn) NULL)) {
	/*
	 * free any values left unassigned
	 */
	while (val > values) {
	    if ((--val)->type == T_STRING) {
		str_ref(val->u.string);
		str_del(val->u.string);
	    }
	}
	val = oldval;
	error((char *) NULL);	/* pass on error */
    }
    sp -= nargs;

    while (flen > 0) {
	if (f[0] != '%' || f[1] == '%') {
	    /* match initial part */
	    fl = flen;
	    sl = slen;
	    if (!match(f, s, &fl, &sl) || fl == flen) {
		goto no_match;
	    }
	    f += fl;
	    flen -= fl;
	    s += sl;
	    slen -= sl;
	}

	/* skip first % */
	f++;
	--flen;

	/*
	 * check for %*
	 */
	if (*f == '*') {
	    /* no assignment */
	    f++;
	    --flen;
	    skip = TRUE;
	} else {
	    skip = FALSE;
	}

	--flen;
	switch (*f++) {
	case 's':
	    /* %s */
	    if (f[0] == '%' && f[1] != '%') {
		switch ((f[1] == '*') ? f[2] : f[1]) {
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
		    error("Bad sscanf format string");
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
		    for (x = f, size = 0; (x - f) != flen; x++, size++) {
			x = (char *) memchr(x, '%', flen - (x - f));
			if (x == (char *) NULL) {
			    x = f + flen;
			    break;
			} else if (x[1] != '%') {
			    break;
			}
		    }
		    size = (x - f) - size;

		    x = s;
		    for (;;) {
			sl = slen - (x - s);
			if (sl < size) {
			    goto no_match;
			}
			x = (char *) memchr(x, f[0], sl - size + 1);
			if (x == (char *) NULL) {
			    goto no_match;
			}
			fl = flen;
			if (match(f, x, &fl, &sl)) {
			    f += fl;
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
		    error("No lvalue for %%s");
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
		    error("No lvalue for %%d");
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
		    error("No lvalue for %%f");
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
		    error("No lvalue for %%c");
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
	    error("Bad sscanf format string");
	}
	matches++;
    }

no_match:
    if (nargs > 0) {
	/* pop superfluous arguments */
	i_pop(nargs);
    }
    while (val > values) {
	i_store(sp, (value *) val - 1);
	sp++;
	--val;
    }
    val = oldval;
    ec_pop();

    str_del((sp++)->u.string);
    str_del(sp->u.string);
    sp->type = T_INT;
    sp->u.number = matches;
    return 0;
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
