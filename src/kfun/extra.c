# ifndef FUNCDEF
# include "kfun.h"
# include <ctype.h>
# endif


# ifdef FUNCDEF
FUNCDEF("crypt", kf_crypt, p_crypt)
# else
char p_crypt[] = { C_TYPECHECKED | C_STATIC | C_VARARGS | C_LOCAL, T_STRING, 2,
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

    p = crypt(sp->u.string->text, salt);
    str_del(sp->u.string);
    str_ref(sp->u.string = str_new(p, (long) strlen(p)));
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("ctime", kf_ctime, p_ctime)
# else
char p_ctime[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_STRING, 1, T_INT };

/*
 * NAME:	kfun->ctime()
 * DESCRIPTION:	convert a time value to a string
 */
int kf_ctime()
{
    sp->type = T_STRING;
    str_ref(sp->u.string = str_new(P_ctime((Uint) sp->u.number), 24L));

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("explode", kf_explode, p_explode)
# else
char p_explode[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
		     T_STRING | (1 << REFSHIFT), 2, T_STRING, T_STRING };

/*
 * NAME:	kfun->explode()
 * DESCRIPTION:	explode a string
 */
int kf_explode()
{
    register int len, slen, size;
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

    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("implode", kf_implode, p_implode)
# else
char p_implode[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_STRING, 2,
		     T_STRING | (1 << REFSHIFT), T_STRING };

/*
 * NAME:	kfun->implode()
 * DESCRIPTION:	implode an array
 */
int kf_implode()
{
    register long len;
    register int slen, i;
    register char *p, *s;
    register value *v;
    string *str;

    s = sp->u.string->text;
    slen = sp->u.string->len;

    /* first, determine the size of the imploded string */
    i = sp[1].u.array->size;
    if (i != 0) {
	len = --i * (long) slen;	/* size of all separators */
	for (v = d_get_elts(sp[1].u.array); i >= 0; v++, --i) {
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
FUNCDEF("random", kf_random, p_random)
# else
char p_random[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_INT, 1, T_INT };

/*
 * NAME:	kfun->random()
 * DESCRIPTION:	return a random number
 */
int kf_random()
{
    sp->u.number = (sp->u.number > 0) ? P_random() % sp->u.number : 0;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sscanf", kf_sscanf, p_sscanf)
# else
char p_sscanf[] = { C_TYPECHECKED | C_STATIC | C_VARARGS | C_LOCAL, T_INT, 3,
		    T_STRING, T_STRING, T_LVALUE | T_ELLIPSIS };

/*
 * NAME:	kfun->sscanf()
 * DESCRIPTION:	scan a string
 */
int kf_sscanf(nargs)
int nargs;
{
    register int len, flen, size, n;
    register char *f, *pct;
    value values[MAX_LOCALS - 2];
    static value *val;
    char *p, *q;
    xfloat flt;
    int matches;
    bool skip;

    if (nargs < 2) {
	return -1;
    } else if (nargs > MAX_LOCALS) {
	return 4;
    }
    p = sp[nargs - 1].u.string->text;
    len = sp[nargs - 1].u.string->len;
    f = sp[nargs - 2].u.string->text;
    flen = sp[nargs - 2].u.string->len;

    nargs -= 2;
    val = values;
    matches = 0;

    if (ec_push()) {
	/*
	 * free any values left unassigned
	 */
	while (val > values) {
	    if ((--val)->type == T_STRING) {
		str_ref(val->u.string);
		str_del(val->u.string);
	    }
	}
	error((char *) NULL);	/* pass on error */
    }

    while (flen > 0) {
	/*
	 * find first %
	 */
	pct = (char *) memchr(f, '%', flen);
	if (pct == (char *) NULL) {
	    /* nothing else to match */
	    break;
	}
	if ((size=pct - f) != 0) {
	    /*
	     * compare part before the first %
	     */
	    if (memcmp(f, p, size) != 0) {
		/* no match */
		break;
	    }
	    p += size;
	    len -= size;
	}
	f += ++size;
	flen -= size + 1;

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

	switch (*f++) {
	case 's':
	    /* %s */
	    pct = (char *) memchr(f, '%', flen);
	    if (pct == f) {
		/*
		 * %s%
		 */
		if (*++pct == '%') {
		    /*
		     * %s%%
		     */
		    pct++;
		    flen -= 2;
		    f = (char *) memchr(p, '%', len);
		    if (f == (char *) NULL) {
			goto no_match;
		    }
		    n = 1;
		} else {
		    if (*pct == '*') {
			pct++;
		    }
		    if (*pct == 'd') {
			/*
			 * %s%d
			 */
			pct = f;
			for (f = p, size = len; *f != '-' && !isdigit(*f); f++)
			{
			    if (--size <= 0) {
				goto no_match;
			    }
			}
		    } else if (*pct == 'f') {
			/*
			 * %s%f
			 */
			pct = f;
			for (f = p, size = len; *f != '-' && !isdigit(*f); f++)
			{
			    if (f[0] == '.' && isdigit(f[1])) {
				break;
			    }
			    if (--size <= 0) {
				goto no_match;
			    }
			}
		    } else {
			error("Bad sscanf format string");
		    }
		    n = 0;
		}
		size = f - p;
	    } else {
		/*
		 * %s followed by non-%
		 */
		if (pct == (char *) NULL) {
		    /* end of format string */
		    pct = f + flen;
		}
		n = pct - f;

		if (n == 0) {
		    /* all the rest in one string */
		    size = len;
		} else {
		    size = -1;
		    do {
			if (len < ++size + n) {
			    goto no_match;
			}
			q = (char *) memchr(p + size, f[0], len - n - size + 1);
			if (q == (char *) NULL) {
			    goto no_match;
			}
			size = q - p;
		    } while (memcmp(q, f, n) != 0);
		    flen -= n;
		}
	    }

	    if (!skip) {
		if (nargs == 0) {
		    error("No lvalue for %%s");
		}
		--nargs;
		val->type = T_STRING;
		val->u.string = str_new(p, (long) size);
		val++;
	    }
	    size += n;
	    p += size;
	    len -= size;
	    f = pct;
	    break;

	case 'd':
	    /* %d */
	    pct = p;
	    val->u.number = strtol(p, &p, 10);
	    if (p == pct) {
		goto no_match;
	    }
	    len -= p - pct;
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
	    pct = p;
	    if (*pct == '-') {
		pct++;
	    }
	    if (isdigit(*pct)) {
		while (isdigit(*++pct)) ;
		if (*pct == '.') {
		    while (isdigit(*++pct));
		}
	    } else if (*pct++ == '.' && isdigit(*pct)) {
		while (isdigit(*++pct)) ;
	    } else {
		goto no_match;
	    }
	    q = pct;
	    if (*pct == 'e' || *pct == 'E') {
		if (*++pct == '+' || *pct == '-') {
		    pct++;
		}
		if (!isdigit(*pct)) {
		    pct = q;
		} else {
		    while (isdigit(*++pct)) ;
		}
	    }
	    if (!flt_atof(p, &flt)) {
		goto no_match;
	    }
	    if (!skip) {
		if (nargs == 0) {
		    error("No lvalue for %%f");
		}
		--nargs;
		val->type = T_FLOAT;
		VFLT_PUT(val, flt);
		val++;
	    }
	    len -= pct - p;
	    p = pct;
	    break;

	case 'c':
	    /* %c */
	    if (len == 0) {
		goto no_match;
	    }
	    if (!skip) {
		if (nargs == 0) {
		    error("No lvalue for %%c");
		}
		--nargs;
		val->type = T_INT;
		val->u.number = UCHAR(*p);
		val++;
	    }
	    p++;
	    --len;
	    break;

	case '%':
	    /* %% */
	    if (*p++ != '%') {
		goto no_match;
	    }
	    --len;
	    continue;

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
	i_store(sp, --val);
	sp++;
    }
    ec_pop();

    str_del((sp++)->u.string);
    str_del(sp->u.string);
    sp->type = T_INT;
    sp->u.number = matches;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("parse_string", kf_parse_string, p_parse_string)
# else
char p_parse_string[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
			  T_MIXED | (1 << REFSHIFT), 2, T_STRING, T_STRING };

/*
 * NAME:	kfun->parse_string()
 * DESCRIPTION:	parse a string
 */
int kf_parse_string()
{
    error("Not yet implemented");
}
# endif
