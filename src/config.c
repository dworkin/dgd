# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "path.h"
# include "editor.h"
# include "call_out.h"
# include "comm.h"
# include "version.h"
# include "macro.h"
# include "token.h"
# include "ppcontrol.h"
# include "node.h"
# include "parser.h"
# include "compile.h"
# include "csupport.h"
# include "table.h"

typedef struct {
    char *name;		/* name of the option */
    short type;		/* option type */
    bool set;		/* TRUE if option is set */
    Uint low, high;	/* lower and higher bound, for numeric values */
    union {
	long num;	/* numeric value */
	char *str;	/* string value */
    } u;
} config;

static config conf[] = {
# define ARRAY_SIZE	0
				{ "array_size",		INT_CONST, FALSE,
							1, USHRT_MAX / 2 },
# define AUTO_OBJECT	1
				{ "auto_object",	STRING_CONST, TRUE },
# define BINARY_PORT	2
				{ "binary_port",	INT_CONST, FALSE,
							0, USHRT_MAX },
# define CACHE_SIZE	3
				{ "cache_size",		INT_CONST, FALSE,
							2, UINDEX_MAX },
# define CALL_OUTS	4
				{ "call_outs",		INT_CONST, FALSE,
							0, UINDEX_MAX },
# define CREATE		5
				{ "create",		STRING_CONST, FALSE },
# define DIRECTORY	6
				{ "directory",		STRING_CONST, FALSE },
# define DRIVER_OBJECT	7
				{ "driver_object",	STRING_CONST, TRUE },
# define DUMP_FILE	8
				{ "dump_file",		STRING_CONST, FALSE },
# define DYNAMIC_CHUNK	9
				{ "dynamic_chunk",	INT_CONST, FALSE },
# define ED_TMPFILE	10
				{ "ed_tmpfile",		STRING_CONST, FALSE },
# define EDITORS	11
				{ "editors",		INT_CONST, FALSE,
							0, UCHAR_MAX },
# define INCLUDE_DIRS	12
				{ "include_dirs",	'(', FALSE },
# define INCLUDE_FILE	13
				{ "include_file",	STRING_CONST, TRUE },
# define OBJECTS	14
				{ "objects",		INT_CONST, FALSE,
							2, UINDEX_MAX },
# define SECTOR_SIZE	15
				{ "sector_size",	INT_CONST, FALSE,
							512, 8192 },
# define STATIC_CHUNK	16
				{ "static_chunk",	INT_CONST, FALSE },
# define SWAP_FILE	17
				{ "swap_file",		STRING_CONST, FALSE },
# define SWAP_FRAGMENT	18
				{ "swap_fragment",	INT_CONST, FALSE,
							0, UINDEX_MAX },
# define SWAP_SIZE	19
				{ "swap_size",		INT_CONST, FALSE,
							1024, SW_UNUSED },
# define TELNET_PORT	20
				{ "telnet_port",	INT_CONST, FALSE,
							0, USHRT_MAX },
# define TYPECHECKING	21
				{ "typechecking",	INT_CONST, FALSE,
							0, 2 },
# define USERS		22
				{ "users",		INT_CONST, FALSE,
							1, UCHAR_MAX },
# define NR_OPTIONS	23
};


typedef struct { char fill; char c;	} alignc;
typedef struct { char fill; short s;	} aligns;
typedef struct { char fill; Int i;	} aligni;
typedef struct { char fill; char *p;	} alignp;
typedef struct { char c;		} alignz;

typedef char dumpinfo[28];

# define FORMAT_VERSION	2

# define DUMP_VALID	0	/* valud dump flag */
# define DUMP_VERSION	1	/* dump file version number */
# define DUMP_DRIVER	2	/* 0: vanilla DGD, 1: iChat DGD */
# define DUMP_TYPECHECK	3	/* global typechecking */
# define DUMP_SECSIZE	4	/* sector size */
# define DUMP_TYPE	4	/* first XX bytes, dump type */
# define DUMP_STARTTIME	20	/* start time */
# define DUMP_ELAPSED	24	/* elapsed time */

static dumpinfo header;		/* dumpfile header */
# define s0	(header[ 6])	/* short, msb */
# define s1	(header[ 7])	/* short, lsb */
# define i0	(header[ 8])	/* Int, msb */
# define i1	(header[ 9])
# define i2	(header[10])
# define i3	(header[11])	/* Int, lsb */
# define usize	(header[12])	/* sizeof(uindex) */
# define dsize	(header[13])	/* sizeof(sector) */
# define psize	(header[14])	/* sizeof(char*) */
# define calign	(header[15])	/* align(char) */
# define salign	(header[16])	/* align(short) */
# define ialign	(header[17])	/* align(Int) */
# define palign	(header[18])	/* align(char*) */
# define zalign	(header[19])	/* align(struct) */
static int ualign;		/* align(uindex) */
static int dalign;		/* align(sector) */
static dumpinfo rheader;	/* restored header */
# define rs0	(rheader[ 6])	/* short, msb */
# define rs1	(rheader[ 7])	/* short, lsb */
# define ri0	(rheader[ 8])	/* Int, msb */
# define ri1	(rheader[ 9])
# define ri2	(rheader[10])
# define ri3	(rheader[11])	/* Int, lsb */
# define rusize	(rheader[12])	/* sizeof(uindex) */
# define rdsize	(rheader[13])	/* sizeof(sector) */
# define rpsize	(rheader[14])	/* sizeof(char*) */
# define rcalign (rheader[15])	/* align(char) */
# define rsalign (rheader[16])	/* align(short) */
# define rialign (rheader[17])	/* align(Int) */
# define rpalign (rheader[18])	/* align(char*) */
# define rzalign (rheader[19])	/* align(struct) */
static int rualign;		/* align(uindex) */
static int rdalign;		/* align(sector) */
static Uint starttime;		/* start time */
static Uint elapsed;		/* elapsed time */
static Uint boottime;		/* boot time */

/*
 * NAME:	conf->dumpinit()
 * DESCRIPTION:	initialize dump file information
 */
static void conf_dumpinit()
{
    short s;
    Int i;
    alignc cdummy;
    aligns sdummy;
    aligni idummy;
    alignp pdummy;

    header[DUMP_VALID] = TRUE;			/* valid dump flag */
    header[DUMP_VERSION] = FORMAT_VERSION;	/* dump file version number */
    header[DUMP_DRIVER] = 0;			/* vanilla DGD */
    header[DUMP_TYPECHECK] = conf[TYPECHECKING].u.num;
    header[DUMP_SECSIZE + 0] = conf[SECTOR_SIZE].u.num >> 8;
    header[DUMP_SECSIZE + 1] = conf[SECTOR_SIZE].u.num;

    starttime = boottime = P_time();

    s = 0x1234;
    i = 0x12345678L;
    s0 = strchr((char *) &s, 0x12) - (char *) &s;
    s1 = strchr((char *) &s, 0x34) - (char *) &s;
    i0 = strchr((char *) &i, 0x12) - (char *) &i;
    i1 = strchr((char *) &i, 0x34) - (char *) &i;
    i2 = strchr((char *) &i, 0x56) - (char *) &i;
    i3 = strchr((char *) &i, 0x78) - (char *) &i;
    usize = sizeof(uindex);
    dsize = sizeof(sector);
    psize = sizeof(char*);
    calign = (char *) &cdummy.c - (char *) &cdummy.fill;
    salign = (char *) &sdummy.s - (char *) &sdummy.fill;
    ialign = (char *) &idummy.i - (char *) &idummy.fill;
    palign = (char *) &pdummy.p - (char *) &pdummy.fill;
    zalign = sizeof(alignz);

    ualign = (usize == sizeof(short)) ? salign : ialign;
    dalign = (dsize == sizeof(short)) ? salign : ialign;
}

/*
 * NAME:	conf->dump()
 * DESCRIPTION:	dump system state on file
 */
void conf_dump()
{
    int fd;
    Uint etime;

    header[DUMP_TYPECHECK] = conf[TYPECHECKING].u.num;
    header[DUMP_STARTTIME + 0] = starttime >> 24;
    header[DUMP_STARTTIME + 1] = starttime >> 16;
    header[DUMP_STARTTIME + 2] = starttime >> 8;
    header[DUMP_STARTTIME + 3] = starttime;
    etime = elapsed + P_time() - boottime;
    header[DUMP_ELAPSED + 0] = etime >> 24;
    header[DUMP_ELAPSED + 1] = etime >> 16;
    header[DUMP_ELAPSED + 2] = etime >> 8;
    header[DUMP_ELAPSED + 3] = etime;

    fd = sw_dump(conf[DUMP_FILE].u.str);
    if (!kf_dump(fd)) {
	fatal("failed to dump kfun table");
    }
    if (!o_dump(fd)) {
	fatal("failed to dump object table");
    }
    if (!pc_dump(fd)) {
	fatal("failed to dump precompiled objects");
    }
    if (!co_dump(fd)) {
	fatal("failed to dump callout table");
    }

    P_lseek(fd, 0L, SEEK_SET);
    P_write(fd, header, sizeof(dumpinfo));
}

/*
 * NAME:	conf->restore()
 * DESCRIPTION:	restore system state from file
 */
static void conf_restore(fd)
int fd;
{
    unsigned int secsize;
    long posn;

    if (P_read(fd, rheader, sizeof(dumpinfo)) != sizeof(dumpinfo) ||
	memcmp(header, rheader, DUMP_TYPE) != 0) {
	error("Bad or incompatible restore file header");
    }

    starttime = (UCHAR(rheader[DUMP_STARTTIME + 0]) << 24) |
		(UCHAR(rheader[DUMP_STARTTIME + 1]) << 16) |
		(UCHAR(rheader[DUMP_STARTTIME + 2]) << 8) |
		 UCHAR(rheader[DUMP_STARTTIME + 3]);
    elapsed =  (UCHAR(rheader[DUMP_ELAPSED + 0]) << 24) |
	       (UCHAR(rheader[DUMP_ELAPSED + 1]) << 16) |
	       (UCHAR(rheader[DUMP_ELAPSED + 2]) << 8) |
		UCHAR(rheader[DUMP_ELAPSED + 3]);
    rualign = (rusize == sizeof(short)) ? rsalign : rialign;
    rdalign = (rdsize == sizeof(short)) ? rsalign : rialign;
    if (usize < rusize || dsize < rdsize) {
	error("Cannot restore uindex or sector of greater width");
    }
    secsize = (UCHAR(rheader[DUMP_SECSIZE + 0]) << 8) |
	       UCHAR(rheader[DUMP_SECSIZE + 1]);
    if (secsize > conf[SECTOR_SIZE].u.num) {
	error("Cannot restore bigger sector size");
    }

    sw_restore(fd, secsize);
    kf_restore(fd);
    o_restore(fd);

    posn = P_lseek(fd, 0L, SEEK_CUR);	/* preserve current file position */
    o_conv();				/* convert all objects */
    P_lseek(fd, posn, SEEK_SET);	/* restore file position */

    pc_restore(fd);
    boottime = P_time();
    co_restore(fd, boottime);
}

/*
 * NAME:	conf->dsize()
 * DESCRIPTION:	compute the size and alignment of a struct
 *		0x000000ff size in dump file
 *		0x0000ff00 alignment in dump file
 *		0x00ff0000 size
 *		0xff000000 alignment
 */
Uint conf_dsize(layout)
char *layout;
{
    register char *p;
    register Uint sz, rsz, al, ral;
    register Uint size, rsize, align, ralign;

    p = layout;
    size = rsize = 0;
    align = ralign = 1;

    for (;;) {
	switch (*p++) {
	case 'c':	/* character */
	    sz = rsz = sizeof(char);
	    al = calign;
	    ral = rcalign;
	    break;

	case 's':	/* short */
	    sz = rsz = sizeof(short);
	    al = salign;
	    ral = rsalign;
	    break;

	case 'u':	/* uindex */
	    sz = usize;
	    rsz = rusize;
	    al = ualign;
	    ral = rualign;
	    break;

	case 'i':	/* Int */
	    sz = rsz = sizeof(Int);
	    al = ialign;
	    ral = rialign;
	    break;

	case 'd':	/* sector */
	    sz = dsize;
	    rsz = rdsize;
	    al = dalign;
	    ral = rdalign;
	    break;

	case 'p':	/* pointer */
	    sz = psize;
	    rsz = rpsize;
	    al = palign;
	    ral = rpalign;
	    break;

	case 'x':	/* hte */
	    size = ALGN(size, zalign);
	    size = ALGN(size, palign);
	    size += psize;
	    size = ALGN(size, palign);
	    size += psize;
	    size = ALGN(size, zalign);
	    rsize = ALGN(rsize, rzalign);
	    rsize = ALGN(rsize, rpalign);
	    rsize += rpsize;
	    rsize = ALGN(rsize, rpalign);
	    rsize += rpsize;
	    rsize = ALGN(rsize, rzalign);
	    align = ALGN(align, palign);
	    ralign = ALGN(ralign, rpalign);
	    continue;

	case '[':	/* struct */
	    sz = conf_dsize(p);
	    al = (sz >> 8) & 0xff;
	    rsz = (sz >> 16) & 0xff;
	    ral = sz >> 24;
	    sz &= 0xff;
	    p = strchr(p, ']') + 1;
	    break;

	case ']':
	case '\0':	/* end of layout */
	    if (p != layout + 2) {
		/* a stuct and not an array element */
		align = ALGN(align, zalign);
		ralign = ALGN(ralign, rzalign);
	    }
	    return ALGN(rsize, ralign) |
	    	   (ralign << 8) |
		   (ALGN(size, align) << 16) |
		   (align << 24);
	}

	size = ALGN(size, al) + sz;
	rsize = ALGN(rsize, ral) + rsz;
	align = ALGN(align, al);
	ralign = ALGN(ralign, ral);
    }
}

/*
 * NAME:	conf_dconv()
 * DESCRIPTION:	convert structs from dumpfile format
 */
Uint conf_dconv(buf, rbuf, layout, n)
register char *buf, *rbuf;
char *layout;
Uint n;
{
    register Uint i, ri, j, size, rsize;
    register char *p;

    rsize = conf_dsize(layout);
    size = (rsize >> 16) & 0xff;
    rsize &= 0xff;
    while (n > 0) {
	i = ri = 0;
	for (p = layout; *p != '\0' && *p != ']'; p++) {
	    switch (*p) {
	    case 'c':
		i = ALGN(i, calign);
		ri = ALGN(ri, rcalign);
		buf[i] = rbuf[ri];
		i += sizeof(char);
		ri += sizeof(char);
		break;

	    case 's':
		i = ALGN(i, salign);
		ri = ALGN(ri, rsalign);
		buf[i + s0] = rbuf[ri + rs0];
		buf[i + s1] = rbuf[ri + rs1];
		i += sizeof(short);
		ri += sizeof(short);
		break;

	    case 'u':
		i = ALGN(i, ualign);
		ri = ALGN(ri, rualign);
		if (usize == rusize) {
		    if (usize == sizeof(short)) {
			buf[i + s0] = rbuf[ri + rs0];
			buf[i + s1] = rbuf[ri + rs1];
			i += sizeof(short);
			ri += sizeof(short);
		    } else {
			buf[i + i0] = rbuf[ri + ri0];
			buf[i + i1] = rbuf[ri + ri1];
			buf[i + i2] = rbuf[ri + ri2];
			buf[i + i3] = rbuf[ri + ri3];
			i += sizeof(Int);
			ri += sizeof(Int);
		    }
		} else {
		    j = (UCHAR(rbuf[ri + rs0] & rbuf[ri + rs1]) == 0xff) ?
			 -1 : 0;
		    buf[i + i0] = j;
		    buf[i + i1] = j;
		    buf[i + i2] = rbuf[ri + rs0];
		    buf[i + i3] = rbuf[ri + rs1];
		    i += sizeof(Int);
		    ri += sizeof(short);
		}
		break;

	    case 'i':
		i = ALGN(i, ialign);
		ri = ALGN(ri, rialign);
		buf[i + i0] = rbuf[ri + ri0];
		buf[i + i1] = rbuf[ri + ri1];
		buf[i + i2] = rbuf[ri + ri2];
		buf[i + i3] = rbuf[ri + ri3];
		i += sizeof(Int);
		ri += sizeof(Int);
		break;

	    case 'd':
		i = ALGN(i, dalign);
		ri = ALGN(ri, rdalign);
		if (dsize == rdsize) {
		    if (dsize == sizeof(short)) {
			buf[i + s0] = rbuf[ri + rs0];
			buf[i + s1] = rbuf[ri + rs1];
			i += sizeof(short);
			ri += sizeof(short);
		    } else {
			buf[i + i0] = rbuf[ri + ri0];
			buf[i + i1] = rbuf[ri + ri1];
			buf[i + i2] = rbuf[ri + ri2];
			buf[i + i3] = rbuf[ri + ri3];
			i += sizeof(Int);
			ri += sizeof(Int);
		    }
		} else {
		    j = (UCHAR(rbuf[ri + rs0] & rbuf[ri + rs1]) == 0xff) ?
			 -1 : 0;
		    buf[i + i0] = j;
		    buf[i + i1] = j;
		    buf[i + i2] = rbuf[ri + rs0];
		    buf[i + i3] = rbuf[ri + rs1];
		    i += sizeof(Int);
		    ri += sizeof(short);
		}
		break;

	    case 'p':
		i = ALGN(i, palign);
		ri = ALGN(ri, rpalign);
		for (j = psize; j > 0; --j) {
		    buf[i++] = 0;
		}
		ri += rpsize;
		break;

	    case '[':
		j = conf_dsize(++p);
		i = ALGN(i, j >> 24);
		ri = ALGN(ri, (j >> 8) & 0xff);
		j = conf_dconv(buf + i, rbuf + ri, p, (Uint) 1);
		i += (j >> 16) & 0xff;
		ri += j & 0xff;
		p = strchr(p, ']');
		break;

	    case 'x':
		i = ALGN(i, zalign);
		i = ALGN(i, palign);
		for (j = psize; j > 0; --j) {
		    buf[i++] = 0;
		}
		i = ALGN(i, palign);
		for (j = psize; j > 0; --j) {
		    buf[i++] = 0;
		}
		ri = ALGN(ri, rzalign);
		ri = ALGN(ri, rpalign);
		ri += rpsize;
		ri = ALGN(ri, rpalign);
		for (j = rpsize; j > 0; --j) {
		    if (rbuf[ri] != 0) {
			buf[i - 1] = 1;
			break;
		    }
		    ri++;
		}
		ri += j;
		i = ALGN(i, zalign);
		ri = ALGN(ri, rzalign);
		break;
	    }
	}

	buf += size;
	rbuf += rsize;
	--n;
    }

    return (size << 16) | rsize;
}

/*
 * NAME:	conf->dread()
 * DESCRIPTION:	read from dumpfile
 */
void conf_dread(fd, buf, layout, n)
int fd;
char *buf, *layout;
register Uint n;
{
    char buffer[16384];
    register unsigned int i, size, rsize;
    Uint tmp;

    tmp = conf_dsize(layout);
    size = (tmp >> 16) & 0xff;
    rsize = tmp & 0xff;
    while (n != 0) {
	i = sizeof(buffer) / rsize;
	if (i > n) {
	    i = n;
	}
	if (P_read(fd, buffer, i * rsize) != i * rsize) {
	    fatal("cannot read from dump file");
	}
	conf_dconv(buf, buffer, layout, (Uint) i);
	buf += size * i;
	n -= i;
    }
}


# define MAX_DIRS	32

static char *dirs[MAX_DIRS];

/*
 * NAME:	conferr()
 * DESCRIPTION:	error during the configuration phase
 */
static void conferr(err)
char *err;
{
    message("Config error, line %u: %s\012", tk_line(), err);	/* LF */
}

/*
 * NAME:	config->config()
 * DESCRIPTION:	read config file
 */
static bool conf_config()
{
    char buf[STRINGSZ];
    register char *p;
    register int h, l, m, c;

    for (h = NR_OPTIONS; h > 0; ) {
	conf[--h].u.num = 0;
    }
    memset(dirs, '\0', sizeof(dirs));

    while ((c=pp_gettok()) != EOF) {
	if (c != IDENTIFIER) {
	    conferr("option expected");
	    return FALSE;
	}

	l = 0;
	h = NR_OPTIONS;
	for (;;) {
	    c = strcmp(yytext, conf[m = (l + h) >> 1].name);
	    if (c == 0) {
		break;	/* found */
	    } else if (c < 0) {
		h = m;	/* search in lower half */
	    } else {
		l = m + 1;	/* search in upper half */
	    }
	    if (l >= h) {
		conferr("unknown option");
		return FALSE;
	    }
	}

	if (pp_gettok() != '=') {
	    conferr("'=' expected");
	    return FALSE;
	}

	if ((c=pp_gettok()) != conf[m].type) {
	    if (c != INT_CONST && c != STRING_CONST && c != '(') {
		conferr("syntax error");
		return FALSE;
	    } else {
		conferr("bad value type");
		return FALSE;
	    }
	}

	switch (c) {
	case INT_CONST:
	    if (yylval.number < conf[m].low ||
		(conf[m].high != 0 && yylval.number > conf[m].high)) {
		conferr("int value out of range");
		return FALSE;
	    }
	    conf[m].u.num = yylval.number;
	    break;

	case STRING_CONST:
	    p = (conf[m].set) ? path_resolve(buf, yytext) : yytext;
	    l = strlen(p);
	    if (l >= STRINGSZ) {
		l = STRINGSZ - 1;
		p[l] = '\0';
	    }
	    m_static();
	    conf[m].u.str = strcpy(REALLOC(conf[m].u.str, char, 0, l + 1), p);
	    m_dynamic();
	    break;

	case '(':
	    if (pp_gettok() != '{') {
		conferr("'{' expected");
		return FALSE;
	    }
	    l = 0;
	    for (;;) {
		if (pp_gettok() != STRING_CONST) {
		    conferr("string expected");
		    return FALSE;
		}
		if (l == MAX_DIRS - 1) {
		    conferr("too many include directories");
		    return FALSE;
		}
		m_static();
		dirs[l] = strcpy(REALLOC(dirs[l], char, 0, strlen(yytext) + 1),
				 yytext);
		l++;
		m_dynamic();
		if ((c=pp_gettok()) == '}') {
		    break;
		}
		if (c != ',') {
		    conferr("',' expected");
		    return FALSE;
		}
	    }
	    if (pp_gettok() != ')') {
		conferr("')' expected");
		return FALSE;
	    }
	    dirs[l] = (char *) NULL;
	    break;
	}
	conf[m].set = TRUE;
	if (pp_gettok() != ';') {
	    conferr("';' expected");
	    return FALSE;
	}
    }

    for (l = 0; l < NR_OPTIONS; l++) {
	if (!conf[l].set) {
	    char buffer[64];

	    sprintf(buffer, "unspecified option %s", conf[l].name);
	    conferr(buffer);
	    return FALSE;
	}
    }

    return TRUE;
}

static char *fname;		/* file name */
static int fd;			/* file descriptor */
static char *obuf;		/* output buffer */
static unsigned int bufsz;	/* buffer size */

/*
 * NAME:	config->open()
 * DESCRIPTION:	create a new file
 */
static bool copen(file)
char *file;
{
    char fname[STRINGSZ];

    path_resolve(fname, file);
    if ((fd=P_open(fname, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644)) < 0) {
	message("Config error: cannot create \"/%s\"\012", fname);	/* LF */
	return FALSE;
    }
    bufsz = 0;

    return TRUE;
}

/*
 * NAME:	config->put()
 * DESCRIPTION:	write a string to a file
 */
static void cputs(str)
register char *str;
{
    register unsigned int len, chunk;

    len = strlen(str);
    while (bufsz + len > BUF_SIZE) {
	chunk = BUF_SIZE - bufsz;
	memcpy(obuf + bufsz, str, chunk);
	P_write(fd, obuf, BUF_SIZE);
	str += chunk;
	len -= chunk;
	bufsz = 0;
    }
    if (len > 0) {
	memcpy(obuf + bufsz, str, len);
	bufsz += len;
    }
}

/*
 * NAME:	config->close()
 * DESCRIPTION:	close a file
 */
static bool cclose()
{
    if (bufsz > 0 && P_write(fd, obuf, bufsz) != bufsz) {
	message("Config error: cannot write \"/%s\"\012", fname);	/* LF */
	P_close(fd);
	return FALSE;
    }
    P_close(fd);

    return TRUE;
}

/*
 * NAME:	config->includes()
 * DESCRIPTION:	create include files
 */
static bool conf_includes()
{
    char buf[BUF_SIZE], buffer[STRINGSZ];

    /* create status.h file */
    obuf = buf;
    sprintf(buffer, "%s/status.h", dirs[0]);
    if (!copen(buffer)) {
	return FALSE;
    }
    cputs("/*\012 * This file defines the fields of the array returned ");
    cputs("by the\012 * status() kfun.  It is automatically generated ");
    cputs("by DGD on startup.\012 */\012\012");
    cputs("# define ST_VERSION\t0\t/* driver version */\012");
    cputs("# define ST_STARTTIME\t1\t/* system start time */\012");
    cputs("# define ST_BOOTTIME\t2\t/* system reboot time */\012");
    cputs("# define ST_UPTIME\t3\t/* system virtual uptime */\012");
    cputs("# define ST_SWAPSIZE\t4\t/* # sectors on swap device */\012");
    cputs("# define ST_SWAPUSED\t5\t/* # sectors allocated */\012");
    cputs("# define ST_SECTORSIZE\t6\t/* size of swap sector */\012");
    cputs("# define ST_SWAPRATE1\t7\t/* # objects swapped out per minute */\012");
    cputs("# define ST_SWAPRATE5\t8\t/* # objects swapped out per five minutes */\012");
    cputs("# define ST_SMEMSIZE\t9\t/* static memory allocated */\012");
    cputs("# define ST_SMEMUSED\t10\t/* static memory in use */\012");
    cputs("# define ST_DMEMSIZE\t11\t/* dynamic memory allocated */\012");
    cputs("# define ST_DMEMUSED\t12\t/* dynamic memory in use */\012");
    cputs("# define ST_OTABSIZE\t13\t/* object table size */\012");
    cputs("# define ST_NOBJECTS\t14\t/* # objects in the system */\012");
    cputs("# define ST_COTABSIZE\t15\t/* callouts table size */\012");
    cputs("# define ST_NCOSHORT\t16\t/* # short-term callouts */\012");
    cputs("# define ST_NCOLONG\t17\t/* # long-term & millisecond callouts */\012");
    cputs("# define ST_UTABSIZE\t18\t/* user table size */\012");
    cputs("# define ST_ETABSIZE\t19\t/* editor table size */\012");
    cputs("# define ST_STRSIZE\t20\t/* max string size */\012");
    cputs("# define ST_ARRAYSIZE\t21\t/* max array/mapping size */\012");
    cputs("# define ST_STACKDEPTH\t22\t/* remaining stack depth */\012");
    cputs("# define ST_TICKS\t23\t/* remaining ticks */\012");
    cputs("# define ST_PRECOMPILED\t24\t/* precompiled objects */\012");

    cputs("\012# define O_COMPILETIME\t0\t/* time of compilation */\012");
    cputs("# define O_PROGSIZE\t1\t/* program size of object */\012");
    cputs("# define O_DATASIZE\t2\t/* # variables in object */\012");
    cputs("# define O_NSECTORS\t3\t/* # sectors used by object */\012");
    cputs("# define O_CALLOUTS\t4\t/* callouts in object */\012");
    cputs("# define O_INDEX\t5\t/* unique ID for master object */\012");

    cputs("\012# define CO_HANDLE\t0\t/* callout handle */\012");
    cputs("# define CO_FUNCTION\t1\t/* function name */\012");
    cputs("# define CO_DELAY\t2\t/* delay */\012");
    cputs("# define CO_FIRSTXARG\t3\t/* first extra argument */\012");
    if (!cclose()) {
	return FALSE;
    }

    /* create type.h file */
    sprintf(buffer, "%s/type.h", dirs[0]);
    if (!copen(buffer)) {
	return FALSE;
    }
    cputs("/*\012 * This file gives definitions for the value returned ");
    cputs("by the\012 * typeof() kfun.  It is automatically generated ");
    cputs("by DGD on startup.\012 */\012\012");
    sprintf(buffer, "# define T_NIL\t\t%d\012", T_NIL);
    cputs(buffer);
    sprintf(buffer, "# define T_INT\t\t%d\012", T_INT);
    cputs(buffer);
    sprintf(buffer, "# define T_FLOAT\t%d\012", T_FLOAT);
    cputs(buffer);
    sprintf(buffer, "# define T_STRING\t%d\012", T_STRING);
    cputs(buffer);
    sprintf(buffer, "# define T_OBJECT\t%d\012", T_OBJECT);
    cputs(buffer);
    sprintf(buffer, "# define T_ARRAY\t%d\012", T_ARRAY);
    cputs(buffer);
    sprintf(buffer, "# define T_MAPPING\t%d\012", T_MAPPING);
    cputs(buffer);
    if (!cclose()) {
	return FALSE;
    }

    /* create limits.h file */
    sprintf(buffer, "%s/limits.h", dirs[0]);
    if (!copen(buffer)) {
	return FALSE;
    }
    cputs("/*\012 * This file defines some basic sizes of datatypes and ");
    cputs("resources.\012 * It is automatically generated by DGD on ");
    cputs("startup.\012 */\012\012");
    cputs("# define CHAR_BIT\t\t8\t\t/* # bits in character */\012");
    cputs("# define CHAR_MIN\t\t0\t\t/* min character value */\012");
    cputs("# define CHAR_MAX\t\t255\t\t/* max character value */\012\012");
    cputs("# define INT_MIN\t\t0x80000000\t/* -2147483648 */\012");
    cputs("# define INT_MAX\t\t2147483647\t/* max integer value */\012\012");
    sprintf(buffer, "# define MAX_STRING_SIZE\t%u\t\t/* max string size */\012",
	    USHRT_MAX);
    cputs(buffer);
    if (!cclose()) {
	return FALSE;
    }

    /* create float.h file */
    sprintf(buffer, "%s/float.h", dirs[0]);
    if (!copen(buffer)) {
	return FALSE;
    }
    cputs("/*\012 * This file describes the floating point type. It is ");
    cputs("automatically\012 * generated by DGD on startup.\012 */\012\012");
    cputs("# define FLT_RADIX\t2\t\t\t/* binary */\012");
    cputs("# define FLT_ROUNDS\t1\t\t\t/* round to nearest */\012");
    cputs("# define FLT_EPSILON\t7.2759576142E-12\t/* smallest x: 1.0 + x != 1.0 */\012");
    cputs("# define FLT_DIG\t10\t\t\t/* decimal digits of precision*/\012");
    cputs("# define FLT_MANT_DIG\t36\t\t\t/* binary digits of precision */\012");
    cputs("# define FLT_MIN\t2.22507385851E-308\t/* positive minimum */\012");
    cputs("# define FLT_MIN_EXP\t(-1021)\t\t\t/* minimum binary exponent */\012");
    cputs("# define FLT_MIN_10_EXP\t(-307)\t\t\t/* minimum decimal exponent */\012");
    cputs("# define FLT_MAX\t1.79769313485E+308\t/* positive maximum */\012");
    cputs("# define FLT_MAX_EXP\t1024\t\t\t/* maximum binary exponent */\012");
    cputs("# define FLT_MAX_10_EXP\t308\t\t\t/* maximum decimal exponent */\012");
    if (!cclose()) {
	return FALSE;
    }

    /* create trace.h file */
    sprintf(buffer, "%s/trace.h", dirs[0]);
    if (!copen(buffer)) {
	return FALSE;
    }
    cputs("/*\012 * This file describes the fields of the array returned for ");
    cputs("every stack\012 * frame by the call_trace() function.  It is ");
    cputs("automatically generated by DGD\012 * on startup.\012 */\012\012");
    cputs("# define TRACE_OBJNAME\t0\t/* name of the object */\012");
    cputs("# define TRACE_PROGNAME\t1\t/* name of the object the function is in */\012");
    cputs("# define TRACE_FUNCTION\t2\t/* function name */\012");
    cputs("# define TRACE_LINE\t3\t/* line number */\012");
    cputs("# define TRACE_EXTERNAL\t4\t/* external call flag */\012");
    cputs("# define TRACE_FIRSTARG\t5\t/* first argument to function */\012");
    return cclose();
}

/*
 * NAME:	config->init()
 * DESCRIPTION:	initialize the driver
 */
bool conf_init(configfile, dumpfile, fragment)
char *configfile, *dumpfile;
uindex *fragment;
{
    char buf[STRINGSZ];
    int fd;
    bool init;

    /*
     * process config file
     */
    if (!pp_init(path_native(buf, configfile), (char **) NULL, 0)) {
	message("Config error: cannot open config file\012");	/* LF */
	m_finish();
	return FALSE;
    }
    init = conf_config();
    pp_clear();
    if (!init) {
	m_finish();
	return FALSE;
    }
    if (dumpfile != (char *) NULL) {
	fd = P_open(path_native(buf, dumpfile), O_RDONLY | O_BINARY, 0);
	if (fd < 0) {
	    P_message("Config error: cannot open restore file\012");    /* LF */
	    return FALSE;
	}
    }

    /* change directory */
    if (P_chdir(path_native(buf, conf[DIRECTORY].u.str)) < 0) {
	message("Config error: bad base directory \"%s\"\012",	/* LF */
		conf[DIRECTORY].u.str);
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    m_static();

    /* initialize communications */
    if (!comm_init((int) conf[USERS].u.num,
		   (unsigned int) conf[TELNET_PORT].u.num,
		   (unsigned int) conf[BINARY_PORT].u.num)) {
	comm_finish();
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* initialize strings */
    str_init();

    /* initialize arrays */
    arr_init((int) conf[ARRAY_SIZE].u.num);

    /* initialize objects */
    o_init((uindex) conf[OBJECTS].u.num);

    /* initialize swap device */
    sw_init(conf[SWAP_FILE].u.str,
	    (sector) conf[SWAP_SIZE].u.num,
	    (sector) conf[CACHE_SIZE].u.num,
	    (unsigned int) conf[SECTOR_SIZE].u.num);

    /* initialize swapped data handler */
    d_init(conf[TYPECHECKING].u.num == 2);
    *fragment = conf[SWAP_FRAGMENT].u.num;

    /* initalize editor */
    ed_init(conf[ED_TMPFILE].u.str,
	    (int) conf[EDITORS].u.num);

    /* initialize call_outs */
    co_init((uindex) conf[CALL_OUTS].u.num);

    /* initialize kfuns */
    kf_init();

    /* initialize interpreter */
    i_init(conf[CREATE].u.str, conf[TYPECHECKING].u.num == 2);

    /* initialize compiler */
    c_init(conf[AUTO_OBJECT].u.str,
	   conf[DRIVER_OBJECT].u.str,
	   conf[INCLUDE_FILE].u.str,
	   dirs,
	   (int) conf[TYPECHECKING].u.num);

    m_dynamic();

    /* initialize memory manager */
    m_init((size_t) conf[STATIC_CHUNK].u.num,
    	   (size_t) conf[DYNAMIC_CHUNK].u.num);

    /*
     * create include files
     */
    if (!conf_includes()) {
	comm_finish();
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* preload compiled objects */
    pc_preload(conf[AUTO_OBJECT].u.str, conf[DRIVER_OBJECT].u.str);

    /* initialize dumpfile header */
    conf_dumpinit();

    if (ec_push((ec_ftn) NULL)) {
	endthread();
	message((char *) NULL);
	message("Config error: initialization failed\012");	/* LF */
	comm_finish();
	ed_finish();
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }
    if (dumpfile == (char *) NULL) {
	/* initialize mudlib */
	call_driver_object(cframe, "initialize", 0);
    } else {
	/* restore dump file */
	conf_restore(fd);
	P_close(fd);

	/* notify mudlib */
	call_driver_object(cframe, "restored", 0);
    }
    ec_pop();
    i_del_value(cframe->sp++);
    endthread();

    /* start accepting connections */
    comm_listen();

    return TRUE;
}

/*
 * NAME:	config->base_dir()
 * DESCRIPTION:	return the driver base directory
 */
char *conf_base_dir()
{
    return conf[DIRECTORY].u.str;
}

/*
 * NAME:	config->driver()
 * DESCRIPTION:	return the driver object name
 */
char *conf_driver()
{
    return conf[DRIVER_OBJECT].u.str;
}

/*
 * NAME:	config->typechecking()
 * DESCRIPTION:	return the global typechecking flag
 */
int conf_typechecking()
{
    return conf[TYPECHECKING].u.num;
}

/*
 * NAME:	config->array_size()
 * DESCRIPTION:	return the maximum array size
 */
unsigned short conf_array_size()
{
    return conf[ARRAY_SIZE].u.num;
}

/*
 * NAME:	config->statusi()
 * DESCRIPTION:	return resource usage information
 */
bool conf_statusi(f, idx, v)
frame *f;
Int idx;
register value *v;
{
    char *version;
    allocinfo *mstat;
    uindex ncoshort, ncolong;

    switch (idx) {
    case 0:	/* ST_VERSION */
	v->type = T_STRING;
	version = VERSION;
	str_ref(v->u.string = str_new(version, (long) strlen(version)));
	break;

    case 1:	/* ST_STARTTIME */
	v->type = T_INT;
	v->u.number = starttime;
	break;

    case 2:	/* ST_BOOTTIME */
	v->type = T_INT;
	v->u.number = boottime;
	break;

    case 3:	/* ST_UPTIME */
	v->type = T_INT;
	v->u.number = elapsed + P_time() - boottime;
	break;

    case 4:	/* ST_SWAPSIZE */
	v->type = T_INT;
	v->u.number = conf[SWAP_SIZE].u.num;
	break;

    case 5:	/* ST_SWAPUSED */
	v->type = T_INT;
	v->u.number = sw_count();
	break;

    case 6:	/* ST_SECTORSIZE */
	v->type = T_INT;
	v->u.number = conf[SECTOR_SIZE].u.num;
	break;

    case 7:	/* ST_SWAPRATE1 */
	v->type = T_INT;
	v->u.number = co_swaprate1();
	break;

    case 8:	/* ST_SWAPRATE5 */
	v->type = T_INT;
	v->u.number = co_swaprate5();
	break;

    case 9:	/* ST_SMEMSIZE */
	mstat = m_info();
	v->type = T_INT;
	v->u.number = mstat->smemsize;
	break;

    case 10:	/* ST_SMEMUSED */
	mstat = m_info();
	v->type = T_INT;
	v->u.number = mstat->smemused;
	break;

    case 11:	/* ST_DMEMSIZE */
	mstat = m_info();
	v->type = T_INT;
	v->u.number = mstat->dmemsize;
	break;

    case 12:	/* ST_DMEMUSED */
	mstat = m_info();
	v->type = T_INT;
	v->u.number = mstat->dmemused;
	break;

    case 13:	/* ST_OTABSIZE */
	v->type = T_INT;
	v->u.number = conf[OBJECTS].u.num;
	break;

    case 14:	/* ST_NOBJECTS */
	v->type = T_INT;
	v->u.number = o_count();
	break;

    case 15:	/* ST_COTABSIZE */
	v->type = T_INT;
	v->u.number = conf[CALL_OUTS].u.num;
	break;

    case 16:	/* ST_NCOSHORT */
	co_info(&ncoshort, &ncolong);
	v->type = T_INT;
	v->u.number = ncoshort;
	break;

    case 17:	/* ST_NCOLONG */
	co_info(&ncoshort, &ncolong);
	v->type = T_INT;
	v->u.number = ncolong;
	break;

    case 18:	/* ST_UTABSIZE */
	v->type = T_INT;
	v->u.number = conf[USERS].u.num;
	break;

    case 19:	/* ST_ETABSIZE */
	v->type = T_INT;
	v->u.number = conf[EDITORS].u.num;
	break;

    case 20:	/* ST_STRSIZE */
	v->type = T_INT;
	v->u.number = USHRT_MAX;
	break;

    case 21:	/* ST_ARRAYSIZE */
	v->type = T_INT;
	v->u.number = conf[ARRAY_SIZE].u.num;
	break;

    case 22:	/* ST_STACKDEPTH */
	v->type = T_INT;
	v->u.number = i_get_depth(f);
	break;

    case 23:	/* ST_TICKS */
	v->type = T_INT;
	v->u.number = i_get_ticks(f);
	break;

    case 24:	/* ST_PRECOMPILED */
	v->type = T_ARRAY;
	arr_ref(v->u.array = pc_list(f->data));
	break;

    default:
	return FALSE;
    }

    return TRUE;
}

/*
 * NAME:	config->status()
 * DESCRIPTION:	return an array with information about resource usage
 */
array *conf_status(f)
register frame *f;
{
    register value *v;
    register Int i;
    array *a;

    a = arr_new(f->data, 25L);
    for (i = 0, v = a->elts; i < 25; i++, v++) {
	conf_statusi(f, i, v);
    }
    return a;
}

/*
 * NAME:	config->objecti()
 * DESCRIPTION:	return object resource usage information
 */
bool conf_objecti(data, obj, idx, v)
dataspace *data;
register object *obj;
Int idx;
register value *v;
{
    register control *ctrl;
    object *prog;
    array *a;

    prog = (obj->flags & O_MASTER) ? obj : &otable[obj->u_master];
    ctrl = (O_UPGRADING(prog)) ? otable[prog->prev].ctrl : o_control(prog);

    switch (idx) {
    case 0:	/* O_COMPILETIME */
	v->type = T_INT;
	v->u.number = ctrl->compiled;
	break;

    case 1:	/* O_PROGSIZE */
	v->type = T_INT;
	v->u.number = d_get_progsize(ctrl);
	break;

    case 2:	/* O_DATASIZE */
	v->type = T_INT;
	v->u.number = ctrl->nvariables;
	break;

    case 3:	/* O_NSECTORS */
	v->type = T_INT;
	v->u.number = (obj->flags & O_CREATED) ? o_dataspace(obj)->nsectors : 0;
	if (obj->flags & O_MASTER) {
	    v->u.number += ctrl->nsectors;
	}
	break;

    case 4:	/* O_CALLOUTS */
	a = (obj->flags & O_CREATED) ? co_list(data, obj) : arr_new(data, 0L);
	v->type = T_ARRAY;
	arr_ref(v->u.array = a);
	break;

    case 5:	/* O_INDEX */
	v->type = T_INT;
	v->u.number = (obj->flags & O_MASTER) ?
		       (Uint) obj->index : obj->u_master;
	break;


    default:
	return FALSE;
    }

    return TRUE;
}

/*
 * NAME:	config->object()
 * DESCRIPTION:	return resource usage of an object
 */
array *conf_object(data, obj)
register dataspace *data;
register object *obj;
{
    register value *v;
    register Int i;
    array *a;

    a = arr_new(data, 6L);
    for (i = 0, v = a->elts; i < 6; i++, v++) {
	conf_objecti(data, obj, i, v);
    }
    return a;
}
