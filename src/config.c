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
				{ "auto_object",	STRING_CONST, FALSE },
# define BINARY_PORT	2
				{ "binary_port",	INT_CONST, FALSE,
							0, USHRT_MAX },
# define CACHE_SIZE	3
				{ "cache_size",		INT_CONST, FALSE,
							1, UINDEX_MAX },
# define CALL_OUTS	4
				{ "call_outs",		INT_CONST, FALSE,
							0, UINDEX_MAX },
# define CREATE		5
				{ "create",		STRING_CONST, FALSE },
# define DIRECTORY	6
				{ "directory",		STRING_CONST, FALSE },
# define DRIVER_OBJECT	7
				{ "driver_object",	STRING_CONST, FALSE },
# define DUMP_FILE	8
				{ "dump_file",		STRING_CONST, FALSE },
# define DYNAMIC_CHUNK	9
				{ "dynamic_chunk",	INT_CONST, FALSE },
# define ED_TMPFILE	10
				{ "ed_tmpfile",		STRING_CONST, FALSE },
# define EDITORS	11
				{ "editors",		INT_CONST, FALSE,
							1, UCHAR_MAX },
# define INCLUDE_DIRS	12
				{ "include_dirs",	'(', FALSE },
# define INCLUDE_FILE	13
				{ "include_file",	STRING_CONST, FALSE },
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
				{ "swap_fragment",	INT_CONST, FALSE },
# define SWAP_SIZE	19
				{ "swap_size",		INT_CONST, FALSE,
							1024, UINDEX_MAX },
# define TELNET_PORT	20
				{ "telnet_port",	INT_CONST, FALSE,
							1024, USHRT_MAX },
# define TYPECHECKING	21
				{ "typechecking",	INT_CONST, FALSE,
							0, 1 },
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
# define DUMP_PLATFORM	20	/* if first XX bytes the same, no conversion */
# define DUMP_BOOTTIME	20	/* boot time */
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
static Uint boottime;		/* boot time */
static Uint elapsed;		/* elapsed time */
static Uint starttime;		/* start time */

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
    header[DUMP_BOOTTIME + 0] = boottime >> 24;
    header[DUMP_BOOTTIME + 1] = boottime >> 16;
    header[DUMP_BOOTTIME + 2] = boottime >> 8;
    header[DUMP_BOOTTIME + 3] = boottime;

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
    etime = elapsed + P_time() - starttime;
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
    if (!co_dump(fd)) {
	fatal("failed to dump callout table");
    }
    if (!pc_dump(fd)) {
	fatal("failed to dump precompiled objects");
    }

    lseek(fd, 0L, SEEK_SET);
    write(fd, &header, sizeof(dumpinfo));
}

/*
 * NAME:	conf->restore()
 * DESCRIPTION:	restore system state from file
 */
static void conf_restore(fd)
int fd;
{
    int secsize;
    long posn;

    if (read(fd, rheader, sizeof(dumpinfo)) != sizeof(dumpinfo) ||
	memcmp(header, rheader, DUMP_TYPE) != 0) {
	fatal("bad or incompatible restore file header");
    }

    boottime = (UCHAR(rheader[DUMP_BOOTTIME + 0]) << 24) |
	       (UCHAR(rheader[DUMP_BOOTTIME + 1]) << 16) |
	       (UCHAR(rheader[DUMP_BOOTTIME + 2]) << 8) |
		UCHAR(rheader[DUMP_BOOTTIME + 3]);
    elapsed =  (UCHAR(rheader[DUMP_ELAPSED + 0]) << 24) |
	       (UCHAR(rheader[DUMP_ELAPSED + 1]) << 16) |
	       (UCHAR(rheader[DUMP_ELAPSED + 2]) << 8) |
		UCHAR(rheader[DUMP_ELAPSED + 3]);
    rualign = (rusize == sizeof(short)) ? rsalign : rialign;
    rdalign = (rdsize == sizeof(short)) ? rsalign : rialign;
    if (usize < rusize || dsize < rdsize) {
	fatal("cannot restore uindex or sector of greater width");
    }
    secsize = (UCHAR(rheader[DUMP_SECSIZE + 0]) << 8) |
	       UCHAR(rheader[DUMP_SECSIZE + 1]);
    if (secsize > conf[SECTOR_SIZE].u.num) {
	fatal("cannot decrease sector size");
    }

    sw_restore(fd, secsize);
    kf_restore(fd);
    o_restore(fd);

    posn = lseek(fd, 0L, SEEK_CUR);	/* preserve current file position */
    o_conv();				/* convert all objects */
    lseek(fd, posn, SEEK_SET);		/* restore file position */

    starttime = P_time();
    co_restore(fd, starttime);
    pc_restore(fd);
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
	if (read(fd, buffer, i * rsize) != i * rsize) {
	    fatal("cannot read from dump file");
	}
	conf_dconv(buf, buffer, layout, (Uint) i);
	buf += size * i;
	n -= i;
    }
}


static char *fname;	/* file name */
static int fd;		/* file descriptor */
static char *obuf;	/* output buffer */
static int bufsz;	/* buffer size */

/*
 * NAME:	config->open()
 * DESCRIPTION:	create a new file
 */
static void copen(file)
char *file;
{
    if ((file=path_file(fname=path_resolve(file))) == (char *) NULL ||
	(fd=open(file, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644)) < 0) {
	message("Config error: cannot create \"/%s\"\012", fname);	/* LF */
	exit(1);
    }
    bufsz = 0;
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
	write(fd, obuf, BUF_SIZE);
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
static void cclose()
{
    if (bufsz > 0 && write(fd, obuf, bufsz) != bufsz) {
	message("Config error: cannot write \"/%s\"\012", fname);	/* LF */
	exit(1);
    }
    close(fd);
}

/*
 * NAME:	conferr()
 * DESCRIPTION:	error during the configuration phase
 */
static void conferr(err)
char *err;
{
    message("Config error, line %u: %s\012", tk_line(), err);	/* LF */
    exit(1);
}

# define MAX_DIRS	32

/*
 * NAME:	config->init()
 * DESCRIPTION:	initialize the driver
 */
void conf_init(configfile, dumpfile)
char *configfile, *dumpfile;
{
    static char *dirs[MAX_DIRS];
    char buf[BUF_SIZE], buffer[STRINGSZ];
    register char *p;
    register int h, l, m, c;
    int fd;

    if (!pp_init(configfile, (char **) NULL, 0)) {
	message("Config error: cannot open config file\012");	/* LF */
	exit(2);
    }
    while ((c=pp_gettok()) != EOF) {
	if (c != IDENTIFIER) {
	    conferr("option expected");
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
	    }
	}

	if (pp_gettok() != '=') {
	    conferr("'=' expected");
	}

	if ((c=pp_gettok()) != conf[m].type) {
	    if (c != INT_CONST && c != STRING_CONST && c != '(') {
		conferr("syntax error");
	    } else {
		conferr("bad value type");
	    }
	}

	switch (c) {
	case INT_CONST:
	    if (yylval.number < conf[m].low ||
		(conf[m].high != 0 && yylval.number > conf[m].high)) {
		conferr("int value out of range");
	    }
	    conf[m].u.num = yylval.number;
	    break;

	case STRING_CONST:
	    p = (m == AUTO_OBJECT || m == DRIVER_OBJECT || m == INCLUDE_FILE) ?
		 path_resolve(yytext) : yytext;
	    if (conf[m].u.str != (char *) NULL) {
		FREE(conf[m].u.str);
	    }
	    l = strlen(p);
	    if (l >= STRINGSZ) {
		l = STRINGSZ - 1;
		p[l] = '\0';
	    }
	    m_static();
	    conf[m].u.str = strcpy(ALLOC(char, l + 1), p);
	    m_dynamic();
	    break;

	case '(':
	    if (pp_gettok() != '{') {
		conferr("'{' expected");
	    }
	    l = 0;
	    for (;;) {
		if (pp_gettok() != STRING_CONST) {
		    conferr("string expected");
		}
		if (l == MAX_DIRS - 1) {
		    conferr("too many include directories");
		}
		if (dirs[l] != (char *) NULL) {
		    FREE(dirs[l]);
		}
		m_static();
		dirs[l++] = strcpy(ALLOC(char, strlen(yytext) + 1), yytext);
		m_dynamic();
		if ((c=pp_gettok()) == '}') {
		    break;
		}
		if (c != ',') {
		    conferr("',' expected");
		}
	    }
	    if (pp_gettok() != ')') {
		conferr("')' expected");
	    }
	    dirs[l] = (char *) NULL;
	    break;
	}
	conf[m].set = TRUE;
	if (pp_gettok() != ';') {
	    conferr("';' expected");
	}
    }

    for (l = 0; l < NR_OPTIONS; l++) {
	if (!conf[l].set) {
	    sprintf(buffer, "unspecified option %s", conf[l].name);
	    conferr(buffer);
	}
    }
    pp_clear();

    if (dumpfile != (char *) NULL) {
	fd = open(dumpfile, O_RDONLY | O_BINARY, 0);
	if (fd < 0) {
	    message("Config error: cannot open restore file\012");	/* LF */
	    exit(2);
	}
    }

    /* change directory */
    if (chdir(conf[DIRECTORY].u.str) < 0) {
	message("Config error: bad base directory \"%s\"\012",	/* LF */
		conf[DIRECTORY].u.str);
	exit(1);
    }

    m_static();

    /* initialize strings */
    str_init();

    /* initialize arrays */
    arr_init((int) conf[ARRAY_SIZE].u.num);

    /* initialize objects */
    o_init((int) conf[OBJECTS].u.num);

    /* initialize swap device */
    sw_init(conf[SWAP_FILE].u.str,
	    (int) conf[SWAP_SIZE].u.num,
	    (int) conf[CACHE_SIZE].u.num,
	    (int) conf[SECTOR_SIZE].u.num);

    /* initalize editor */
    ed_init(conf[ED_TMPFILE].u.str,
	    (int) conf[EDITORS].u.num);

    /* initialize call_outs */
    co_init((uindex) conf[CALL_OUTS].u.num,
	    (int) conf[SWAP_FRAGMENT].u.num);

    /* initialize kfuns */
    kf_init();

    /* initialize interpreter */
    i_init(conf[CREATE].u.str);

    /* initialize compiler */
    c_init(conf[AUTO_OBJECT].u.str,
	   conf[DRIVER_OBJECT].u.str,
	   conf[INCLUDE_FILE].u.str,
	   dirs);

    m_dynamic();

    /* initialize memory manager */
    m_init((unsigned int) conf[STATIC_CHUNK].u.num,
    	   (unsigned int) conf[DYNAMIC_CHUNK].u.num);

    /* create status.h file */
    obuf = buf;
    sprintf(buffer, "%s/status.h", dirs[0]);
    copen(buffer);
    cputs("/*\012 * This file defines the fields of the array returned ");
    cputs("by the\012 * status() kfun.  It is automatically generated ");
    cputs("by DGD on startup.\012 */\012\012");
    cputs("# define ST_VERSION\t0\t/* driver version */\012");
    cputs("# define ST_STARTTIME\t1\t/* system start time */\012");
    cputs("# define ST_BOOTTIME\t2\t/* system boot time */\012");
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
    cputs("# define ST_NCOLONG\t17\t/* # long-term callouts */\012");
    cputs("# define ST_UTABSIZE\t18\t/* user table size */\012");
    cputs("# define ST_ETABSIZE\t19\t/* editor table size */\012");
    cputs("# define ST_STRSIZE\t20\t/* max string size */\012");
    cputs("# define ST_ARRAYSIZE\t21\t/* max array/mapping size */\012");
    cputs("# define ST_STACKDEPTH\t22\t/* remaining stack depth */\012");
    cputs("# define ST_TICKS\t23\t/* remaining ticks */\012");
    cputs("# define ST_PRECOMPILED\t24\t/* precompiled objects */\012");
    cputs("# define ST_ASTACKDEPTH\t25\t/* actual remaining stack depth */\012");
    cputs("# define ST_ATICKS\t26\t/* actual remaining ticks */\012");

    cputs("\012# define O_COMPILETIME\t0\t/* time of compilation */\012");
    cputs("# define O_PROGSIZE\t1\t/* program size of object */\012");
    cputs("# define O_DATASIZE\t2\t/* data size of object */\012");
    cputs("# define O_NSECTORS\t3\t/* # sectors used by object */\012");
    cputs("# define O_CALLOUTS\t4\t/* callouts in object */\012");
    cclose();

    /* create type.h file */
    sprintf(buffer, "%s/type.h", dirs[0]);
    copen(buffer);
    cputs("/*\012 * This file gives definitions for the value returned ");
    cputs("by the\012 * typeof() kfun.  It is automatically generated ");
    cputs("by DGD on startup.\012 */\012\012");
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
    cclose();

    /* create limits.h file */
    sprintf(buffer, "%s/limits.h", dirs[0]);
    copen(buffer);
    cputs("/*\012 * This file defines some basic sizes of datatypes and ");
    cputs("resources.\012 * It is automatically generated by DGD on ");
    cputs("startup.\012 */\012\012");
    cputs("# define CHAR_BIT\t\t8\t\t/* # bits in character */\012");
    cputs("# define CHAR_MIN\t\t0\t\t/* min character value */\012");
    cputs("# define CHAR_MAX\t\t255\t\t/* max character value */\012\012");
    cputs("# define INT_MIN\t\t0x80000000\t/* -2147483648 */\012");
    cputs("# define INT_MAX\t\t2147483647\t/* max integer value */\012\012");
    sprintf(buffer, "# define MAX_STRING_SIZE\t%u\t\t/* max string size */\012",
	    USHRT_MAX - sizeof(string));
    cputs(buffer);
    sprintf(buffer, "# define MAX_ARRAY_SIZE\t\t%ld\t\t/* max array size */\012",
	    conf[ARRAY_SIZE].u.num);
    cputs(buffer);
    sprintf(buffer, "# define MAX_MAPPING_SIZE\t%ld\t\t/* max mapping size */\012",
	    conf[ARRAY_SIZE].u.num);
    cputs(buffer);
    cclose();

    /* create float.h file */
    sprintf(buffer, "%s/float.h", dirs[0]);
    copen(buffer);
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
    cclose();

    /* preload compiled objects */
    pc_preload(conf[AUTO_OBJECT].u.str, conf[DRIVER_OBJECT].u.str);

    /* initialize dumpfile header */
    conf_dumpinit();

    if (ec_push((ec_ftn) NULL)) {
	message((char *) NULL);
	message("Config error: initialization failed\012");	/* LF */
	exit(1);
    }
    if (dumpfile == (char *) NULL) {
	/* initialize mudlib */
	call_driver_object("initialize", 0);
    } else {
	/* restore dump file */
	conf_restore(fd);
	close(fd);

	/* notify mudlib */
	call_driver_object("restored", 0);
    }
    ec_pop();
    i_del_value(sp++);
    endthread();

    /* initialize communications */
    m_static();
    comm_init((int) conf[USERS].u.num,
	      (int) conf[TELNET_PORT].u.num,
	      (int) conf[BINARY_PORT].u.num);
    m_dynamic();
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
bool conf_typechecking()
{
    return (bool) conf[TYPECHECKING].u.num;
}

/*
 * NAME:	config->array_size()
 * DESCRIPTION:	return the maximum array size
 */
int conf_array_size()
{
    return conf[ARRAY_SIZE].u.num;
}

/*
 * NAME:	config->status()
 * DESCRIPTION:	return an array with information about resource usage
 */
array *conf_status()
{
    register value *v;
    array *a;
    char *version;
    allocinfo *mstat;
    uindex ncoshort, ncolong;

    a = arr_new(27L);
    v = a->elts;

    /* version */
    v->type = T_STRING;
    version = VERSION;
    str_ref((v++)->u.string = str_new(version, (long) strlen(version)));

    /* uptime */
    v->type = T_INT;
    (v++)->u.number = boottime;
    v->type = T_INT;
    (v++)->u.number = starttime;
    v->type = T_INT;
    (v++)->u.number = elapsed + P_time() - starttime;

    /* swap */
    v->type = T_INT;
    (v++)->u.number = conf[SWAP_SIZE].u.num;
    v->type = T_INT;
    (v++)->u.number = sw_count();
    v->type = T_INT;
    (v++)->u.number = conf[SECTOR_SIZE].u.num;
    v->type = T_INT;
    (v++)->u.number = co_swaprate1();
    v->type = T_INT;
    (v++)->u.number = co_swaprate5();

    /* memory */
    mstat = m_info();
    v->type = T_INT;
    (v++)->u.number = mstat->smemsize;
    v->type = T_INT;
    (v++)->u.number = mstat->smemused;
    v->type = T_INT;
    (v++)->u.number = mstat->dmemsize;
    v->type = T_INT;
    (v++)->u.number = mstat->dmemused;

    /* objects */
    v->type = T_INT;
    (v++)->u.number = conf[OBJECTS].u.num;
    v->type = T_INT;
    (v++)->u.number = o_count();

    /* callouts */
    co_info(&ncoshort, &ncolong);
    v->type = T_INT;
    (v++)->u.number = conf[CALL_OUTS].u.num;
    v->type = T_INT;
    (v++)->u.number = ncoshort;
    v->type = T_INT;
    (v++)->u.number = ncolong;

    /* users & editors */
    v->type = T_INT;
    (v++)->u.number = conf[USERS].u.num;
    v->type = T_INT;
    (v++)->u.number = conf[EDITORS].u.num;

    /* limits */
    v->type = T_INT;
    (v++)->u.number = USHRT_MAX - sizeof(string);
    v->type = T_INT;
    (v++)->u.number = conf[ARRAY_SIZE].u.num;
    v->type = T_INT;
    (v++)->u.number = i_get_depth(FALSE);
    v->type = T_INT;
    (v++)->u.number = i_get_ticks(FALSE);

    /* precompiled objects */
    v->type = T_ARRAY;
    arr_ref((v++)->u.array = pc_list());

    /* more limits */
    v->type = T_INT;
    (v++)->u.number = i_get_depth(TRUE);
    v->type = T_INT;
    v->u.number = i_get_ticks(TRUE);

    return a;
}

/*
 * NAME:	config->object()
 * DESCRIPTION:	return resource usage of an object
 */
array *conf_object(obj)
object *obj;
{
    register control *ctrl;
    register value *v;
    array *clist, *a;
    sector nsectors;

    ctrl = o_control(obj);
    if (obj->flags & O_CREATED) {
	clist = co_list(obj);
	nsectors = obj->data->nsectors;
    } else {
	/* avoid creating a dataspace for this object */
	clist = arr_new(0L);
	nsectors = 0;
    }
    a = arr_new(5L);

    v = a->elts;
    v->type = T_INT;
    (v++)->u.number = ctrl->compiled;
    v->type = T_INT;
    (v++)->u.number = ctrl->ninherits * sizeof(dinherit) +
		      ctrl->progsize +
		      ctrl->nstrings * (long) sizeof(dstrconst) +
		      ctrl->strsize +
		      ctrl->nfuncdefs * sizeof(dfuncdef) +
		      ctrl->nvardefs * sizeof(dvardef) +
		      ctrl->nfuncalls * 2L +
		      ctrl->nsymbols * (long) sizeof(dsymbol);
    v->type = T_INT;
    (v++)->u.number = ctrl->nvariables * sizeof(value);
    v->type = T_INT;
    if (obj->flags & O_MASTER) {
	(v++)->u.number = ctrl->nsectors + nsectors;
    } else {
	(v++)->u.number = nsectors;
    }
    v->type = T_ARRAY;
    arr_ref(v->u.array = clist);

    return a;
}
