# define INCLUDE_CTYPE
# include "dgd.h"
# include <stdarg.h>

/*
 * ctype.h
 */

# define A		CTYPE_ALPHA
# define N		CTYPE_ALNUM
# define U		CTYPE_UPPER
# define L		CTYPE_LOWER
# define D		CTYPE_DIGIT
# define X		CTYPE_XDIGIT

char ctype[] = {
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
      N|D|X,   N|D|X,   N|D|X,   N|D|X,   N|D|X,   N|D|X,   N|D|X,   N|D|X,
      N|D|X,   N|D|X,       0,       0,       0,       0,       0,       0,
          0, A|N|U|X, A|N|U|X, A|N|U|X, A|N|U|X, A|N|U|X, A|N|U|X,   A|N|U,
      A|N|U,   A|N|U,   A|N|U,   A|N|U,   A|N|U,   A|N|U,   A|N|U,   A|N|U,
      A|N|U,   A|N|U,   A|N|U,   A|N|U,   A|N|U,   A|N|U,   A|N|U,   A|N|U,
      A|N|U,   A|N|U,   A|N|U,       0,       0,       0,       0,       0,
          0, A|N|L|X, A|N|L|X, A|N|L|X, A|N|L|X, A|N|L|X, A|N|L|X,   A|N|L,
      A|N|L,   A|N|L,   A|N|L,   A|N|L,   A|N|L,   A|N|L,   A|N|L,   A|N|L,
      A|N|L,   A|N|L,   A|N|L,   A|N|L,   A|N|L,   A|N|L,   A|N|L,   A|N|L,
      A|N|L,   A|N|L,   A|N|L,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0,
          0,       0,       0,       0,       0,       0,       0,       0
};

/*
 * NAME:	tolower()
 * DESCRIPTION:	convert to lower case
 */
int tolower(int c)
{
    return (isupper(c)) ? c + 32 : c;
}

/*
 * NAME:	toupper()
 * DESCRIPTION:	convert to upper case
 */
int toupper(int c)
{
    return (islower(c)) ? c - 32 : c;
}


/*
 * string.h
 */

/*
 * NAME:	memcmp()
 * DESCRIPTION:	compare memory
 */
int memcmp(const void *b1, const void *b2, size_t len)
{
    const char *p, *q;

    for (p = b1, q = b2; len != 0; --len) {
	if (*p++ != *q++) {
	    return UCHAR(p[-1]) - UCHAR(q[-1]);
	}
    }
    return 0;
}

/*
 * NAME:	memchr()
 * DESCRIPTION:	find character in memory
 */
void *memchr(const void *b, int c, size_t len)
{
    const char *p;

    for (p = b; len != 0; --len) {
	if (UCHAR(*p++) == UCHAR(c)) {
	    return (void *) (p - 1);
	}
    }
    return NULL;
}

/*
 * NAME:	memset()
 * DESCRIPTION:	fill memory
 */
void *memset(void *b, int c, size_t len)
{
    char *p;

    p = b;
    if ((((long) p) & 1) == 0) {
	long *l, cccc;

	l = (long *) p;
	cccc = (UCHAR(c) << 24) | (UCHAR(c) << 16) | (UCHAR(c) << 8) | UCHAR(c);
	while (len >= 4) {
	    *l++ = cccc;
	    len -= 4;
	}
	p = (char *) l;
    }
    while (len != 0) {
	*p++ = c;
	--len;
    }

    return b;
}

/*
 * NAME:	memcpy()
 * DESCRIPTION:	copy memory
 */
void *memcpy(void *dst, const void *src, size_t len)
{
    char *p;
    const char *q;

    p = dst;
    q = src;
    while (len >= 0x01000000) {
	BlockMove((Ptr) q, (Ptr) p, 0x00800000);
	p += 0x00800000;
	q += 0x00800000;
	len -= 0x00800000;
    }
    if (len != 0) {
	BlockMove((Ptr) q, (Ptr) p, len);
    }
    return dst;
}

/*
 * NAME:	strlen()
 * DESCRIPTION:	get length of string
 */
size_t strlen(const char *s)
{
    const char *p;

    p = s;
    while (*p++ != '\0') ;
    return (long) p - (long) s - 1;
}

/*
 * NAME:	strcmp()
 * DESCRIPTION:	compare strings
 */
int strcmp(const char *p, const char *q)
{
    while (*p != '\0' && *p == *q) {
	p++, q++;
    }
    return UCHAR(*p) - UCHAR(*q);
}

/*
 * NAME:	strncmp()
 * DESCRIPTION:	compare strings up to a certain length
 */
int strncmp(const char *p, const char *q, size_t len)
{
    while (len != 0) {
	if (*p == '\0' || *p != *q) {
	    return UCHAR(*p) - UCHAR(*q);
	}
	p++;
	q++;
	--len;
    }
    return 0;
}

/*
 * NAME:	strchr()
 * DESCRIPTION:	find a character in a string
 */
char *strchr(const char *p, int c)
{
    do {
	if (UCHAR(*p) == UCHAR(c)) {
	    return (void *) p;
	}
    } while (*p++ != '\0');
    return NULL;
}

/*
 * NAME:	strrchr()
 * DESCRIPTION:	find a character in a string, backwards
 */
char *strrchr(const char *p, int c)
{
    const char *q;

    for (q = NULL; *p != '\0'; ) {
	if (UCHAR(*p++) == UCHAR(c)) {
	    q = p - 1;
	}
    }
    return (void *) q;
}

/*
 * NAME:	strpbrk()
 * DESCRIPTION:	find a character from a set in a string
 */
char *strpbrk(const char *p, const char *set)
{
    while (*p != '\0') {
	if (strchr(set, *p) != NULL) {
	    return (void *) p;
	}
	p++;
    }
    return NULL;
}

/*
 * NAME:	strcpy()
 * DESCRIPTION:	copy string
 */
char *strcpy(char *s, const char *q)
{
    char *p;

    for (p = s; (*p++=*q++) != '\0'; ) ;
    return s;
}

/*
 * NAME:	strncpy()
 * DESCRIPTION:	copy string up to certain length
 */
char *strncpy(char *s, const char *q, size_t len)
{
    char *p;

    for (p = s; len != 0 && (*p++=*q++) != '\0'; --len) ;
    return s;
}

/*
 * NAME:	strcat()
 * DESCRIPTION:	concatenate strings
 */
char *strcat(char *s, const char *q)
{
    char *p;

    for (p = s; *p++ != '\0'; ) ;
    for (--p; (*p++=*q++) != '\0'; );
    return s;
}

/*
 * NAME:	strtol()
 * DESCRIPTION:	string to long (decimal only, no errno)
 */
long strtol(const char *s, char **end, int base)
{
    const char *p;
    bool neg, overflow;
    long result;

# ifdef DEBUG
    if (base != 10) {
	fatal("strtol for non-decimal base");
    }
# endif
    p = s;
    while (*p == ' ') {
	p++;
    }
    if (*p == '-') {
	p++;
	neg = TRUE;
    } else {
	neg = FALSE;
    }
    if (!isdigit(*p)) {
	if (end != (char **) NULL) {
	    *end = (char *) s;
	}
	return 0;
    }

    result = 0;
    overflow = FALSE;
    do {
	if (result >= 214748364L && (result > 214748364L || *p >= '8' + neg)) {
	    overflow = TRUE;
	}
	result *= 10;
	result += *p++ - '0';
    } while (isdigit(*p));

    if (end != (char **) NULL) {
	*end = (char *) p;
    }
    if (overflow) {
	return (neg) ? LONG_MIN : LONG_MAX;
    } else {
	return (neg) ? -result : result;
    }
}

/*
 * NAME:	sprintf()
 * DESCRIPTION:	print formatted to buffer (limited functionality)
 */
int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    char tmp[11], *p;
    bool zerofill;
    int field, i;
    unsigned int u;

    va_start(ap, fmt);

    for (;;) {
	if (*fmt == '%') {
	    zerofill = FALSE;
	    field = -1;
	    if (*++fmt == '.') {
		/* . digit s */
# ifdef DEBUG
		if (!isdigit(fmt[1]) || fmt[2] != 's') {
		    fatal("bad sprintf format string");
		}
# endif
		u = fmt[1] - '0';
		fmt += 3;
		memcpy(buf, va_arg(ap, char*), u);
		buf += u;
	    } else {
		if (isdigit(*fmt)) {
		    /* field width */
		    if (*fmt == '0') {
			zerofill = TRUE;
			fmt++;
		    }
		    field = *fmt++ - '0';
		}
		if (*fmt == 'l') {
		    fmt++;	/* sizeof(int) == sizeof(long) */
		}
		switch (*fmt++) {
		case 'c':
		    *buf++ = va_arg(ap, int);
		    continue;

		case 's':
		    p = va_arg(ap, char*);
		    while ((*buf++ = *p++) != '\0') ;
		    --buf;
		    continue;

		case 'd':
		    i = va_arg(ap, int);
		    if (i < 0) {
			*buf++ = '-';
			i = -i;
			if (i < 0) {
			    memcpy(p = tmp + sizeof(tmp) - 10, "2147483648",
				   10);
			    break;
			}
		    }
		    p = tmp + sizeof(tmp);
		    do {
			*--p = (i % 10) + '0';
			i /= 10;
		    } while (i != 0);
		    break;

		case 'u':
		    u = va_arg(ap, unsigned int);
		    p = tmp + sizeof(tmp);
		    do {
			*--p = (u % 10) + '0';
			u /= 10;
		    } while (u != 0);
		    break;

		case 'x':
		    u = va_arg(ap, unsigned int);
		    p = tmp + sizeof(tmp);
		    do {
			i = (u & 0x0f);
			*--p = i + ((i <= 9) ? '0' : 'a' - 10);
			u >>= 4;
		    } while (u != 0);
		    break;

		case '%':
		     *buf++ = '%';
		     continue;

# ifdef DEBUG
		default:
		    fatal("bad sprintf format string");
# endif
		}

		if (tmp + sizeof(tmp) - p < field) {
		    i = (zerofill) ? '0' : ' ';
		    do {
			*--p = i;
		    } while (tmp + sizeof(tmp) - p < field);
		}
		u = tmp + sizeof(tmp) - p;
		memcpy(buf, p, u);
		buf += u;
	    }
	} else if ((*buf++ = *fmt++) == '\0') {
	    break;
	}
    }

    va_end(ap);
    return 0;
}
