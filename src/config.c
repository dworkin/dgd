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
# include "control.h"
# include "csupport.h"
# include "table.h"

typedef struct {
    char *name;		/* name of the option */
    short type;		/* option type */
    bool resolv;	/* TRUE if path name must be resolved */
    bool set;		/* TRUE if option is set */
    Uint low, high;	/* lower and higher bound, for numeric values */
    union {
	long num;	/* numeric value */
	char *str;	/* string value */
    } u;
} config;

static config conf[] = {
# define ARRAY_SIZE	0
				{ "array_size",		INT_CONST, FALSE, FALSE,
							1, USHRT_MAX / 2 },
# define AUTO_OBJECT	1
				{ "auto_object",	STRING_CONST, TRUE },
# define BINARY_PORT	2
				{ "binary_port",	'[', FALSE, FALSE,
							1, USHRT_MAX },
# define CACHE_SIZE	3
				{ "cache_size",		INT_CONST, FALSE, FALSE,
							2, UINDEX_MAX },
# define CALL_OUTS	4
				{ "call_outs",		INT_CONST, FALSE, FALSE,
							0, UINDEX_MAX - 1 },
# define CREATE		5
				{ "create",		STRING_CONST },
# define DIRECTORY	6
				{ "directory",		STRING_CONST },
# define DRIVER_OBJECT	7
				{ "driver_object",	STRING_CONST, TRUE },
# define DUMP_FILE	8
				{ "dump_file",		STRING_CONST },
# define DUMP_INTERVAL	9
				{ "dump_interval",	INT_CONST },
# define DYNAMIC_CHUNK	10
				{ "dynamic_chunk",	INT_CONST },
# define ED_TMPFILE	11
				{ "ed_tmpfile",		STRING_CONST },
# define EDITORS	12
				{ "editors",		INT_CONST, FALSE, FALSE,
							0, EINDEX_MAX },
# define INCLUDE_DIRS	13
				{ "include_dirs",	'(' },
# define INCLUDE_FILE	14
				{ "include_file",	STRING_CONST, TRUE },
# define OBJECTS	15
				{ "objects",		INT_CONST, FALSE, FALSE,
							2, UINDEX_MAX },
# define SECTOR_SIZE	16
				{ "sector_size",	INT_CONST, FALSE, FALSE,
							512, 8192 },
# define STATIC_CHUNK	17
				{ "static_chunk",	INT_CONST },
# define SWAP_FILE	18
				{ "swap_file",		STRING_CONST },
# define SWAP_FRAGMENT	19
				{ "swap_fragment",	INT_CONST, FALSE, FALSE,
							0, SW_UNUSED },
# define SWAP_SIZE	20
				{ "swap_size",		INT_CONST, FALSE, FALSE,
							1024, SW_UNUSED },
# define TELNET_PORT	21
				{ "telnet_port",	'[', FALSE, FALSE,
							1, USHRT_MAX },
# define TYPECHECKING	22
				{ "typechecking",	INT_CONST, FALSE, FALSE,
							0, 2 },
# define USERS		23
				{ "users",		INT_CONST, FALSE, FALSE,
							1, EINDEX_MAX },
# define NR_OPTIONS	24
};


typedef struct { char fill; char c;	} alignc;
typedef struct { char fill; short s;	} aligns;
typedef struct { char fill; Int i;	} aligni;
typedef struct { char fill; char *p;	} alignp;
typedef struct { char c;		} alignz;

typedef char dumpinfo[50];

# define FORMAT_VERSION	5

# define DUMP_VALID	0	/* valid dump flag */
# define DUMP_VERSION	1	/* dump file version number */
# define DUMP_DRIVER	2	/* 0: vanilla DGD, 1: iChat DGD */
# define DUMP_TYPECHECK	3	/* global typechecking */
# define DUMP_SECSIZE	4	/* sector size */
# define DUMP_TYPE	4	/* first XX bytes, dump type */
# define DUMP_STARTTIME	20	/* start time */
# define DUMP_ELAPSED	24	/* elapsed time */
# define DUMP_HEADERSZ	28	/* header size */
# define DUMP_VSTRING	29	/* version string */

static dumpinfo header;		/* dumpfile header */
# define s0	(header[ 6])	/* short, msb */
# define s1	(header[ 7])	/* short, lsb */
# define i0	(header[ 8])	/* Int, msb */
# define i1	(header[ 9])
# define i2	(header[10])
# define i3	(header[11])	/* Int, lsb */
# define utsize	(header[12])	/* sizeof(uindex) + sizeof(ssizet) */
# define desize	(header[13])	/* sizeof(sector) + sizeof(eindex) */
# define psize	(header[14])	/* sizeof(char*) */
# define calign	(header[15])	/* align(char) */
# define salign	(header[16])	/* align(short) */
# define ialign	(header[17])	/* align(Int) */
# define palign	(header[18])	/* align(char*) */
# define zalign	(header[19])	/* align(struct) */
static int ualign;		/* align(uindex) */
static int talign;		/* align(ssizet) */
static int dalign;		/* align(sector) */
static int ealign;		/* align(eindex) */
static dumpinfo rheader;	/* restored header */
# define rs0	(rheader[ 6])	/* short, msb */
# define rs1	(rheader[ 7])	/* short, lsb */
# define ri0	(rheader[ 8])	/* Int, msb */
# define ri1	(rheader[ 9])
# define ri2	(rheader[10])
# define ri3	(rheader[11])	/* Int, lsb */
# define rutsize (rheader[12])	/* sizeof(uindex) + sizeof(ssizet) */
# define rdesize (rheader[13])	/* sizeof(sector) + sizeof(eindex) */
# define rpsize	(rheader[14])	/* sizeof(char*) */
# define rcalign (rheader[15])	/* align(char) */
# define rsalign (rheader[16])	/* align(short) */
# define rialign (rheader[17])	/* align(Int) */
# define rpalign (rheader[18])	/* align(char*) */
# define rzalign (rheader[19])	/* align(struct) */
static int rusize;		/* sizeof(uindex) */
static int rtsize;		/* sizeof(ssizet) */
static int rdsize;		/* sizeof(sector) */
static int resize;		/* sizeof(eindex) */
static int rualign;		/* align(uindex) */
static int rtalign;		/* align(ssizet) */
static int rdalign;		/* align(sector) */
static int realign;		/* align(eindex) */
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
    strcpy(header + DUMP_VSTRING, VERSION);

    starttime = boottime = P_time();

    s = 0x1234;
    i = 0x12345678L;
    s0 = strchr((char *) &s, 0x12) - (char *) &s;
    s1 = strchr((char *) &s, 0x34) - (char *) &s;
    i0 = strchr((char *) &i, 0x12) - (char *) &i;
    i1 = strchr((char *) &i, 0x34) - (char *) &i;
    i2 = strchr((char *) &i, 0x56) - (char *) &i;
    i3 = strchr((char *) &i, 0x78) - (char *) &i;
    utsize = sizeof(uindex) | (sizeof(ssizet) << 4);
    desize = sizeof(sector) | (sizeof(eindex) << 4);
    psize = sizeof(char*) | (sizeof(char) << 4);
    calign = (char *) &cdummy.c - (char *) &cdummy.fill;
    salign = (char *) &sdummy.s - (char *) &sdummy.fill;
    ialign = (char *) &idummy.i - (char *) &idummy.fill;
    palign = (char *) &pdummy.p - (char *) &pdummy.fill;
    zalign = sizeof(alignz);

    ualign = (sizeof(uindex) == sizeof(short)) ? salign : ialign;
    talign = (sizeof(ssizet) == sizeof(short)) ? salign : ialign;
    dalign = (sizeof(sector) == sizeof(short)) ? salign : ialign;
    switch (sizeof(eindex)) {
    case sizeof(char):	ealign = calign; break;
    case sizeof(short):	ealign = salign; break;
    case sizeof(Int):	ealign = ialign; break;
    }
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
    bool conv_callouts, conv_lwos, conv_ctrls;
    unsigned int secsize;
    long posn;

    if (P_read(fd, rheader, DUMP_HEADERSZ) != DUMP_HEADERSZ ||
	       rheader[DUMP_VERSION] < 2 ||
	       rheader[DUMP_VERSION] > FORMAT_VERSION) {
	error("Bad or incompatible restore file header");
    }
    conv_callouts = conv_lwos = conv_ctrls = FALSE;
    if (rheader[DUMP_VERSION] < 3) {
	conv_callouts = TRUE;
    }
    if (rheader[DUMP_VERSION] < 4) {
	conv_lwos = TRUE;
    }
    if (rheader[DUMP_VERSION] < 5) {
	conv_ctrls = TRUE;
    }
    rheader[DUMP_VERSION] = FORMAT_VERSION;
    if (memcmp(header, rheader, DUMP_TYPE) != 0) {
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
    rusize = rutsize & 0xf;
    rtsize = rutsize >> 4;
    if (rtsize == 0) {
	rtsize = sizeof(unsigned short);	/* backward compat */
    }
    rdsize = rdesize & 0xf;
    resize = rdesize >> 4;
    if (resize == 0) {
	resize = sizeof(char);			/* backward compat */
    }
    rualign = (rusize == sizeof(short)) ? rsalign : rialign;
    rtalign = (rtsize == sizeof(short)) ? rsalign : rialign;
    rdalign = (rdsize == sizeof(short)) ? rsalign : rialign;
    switch (resize) {
    case sizeof(char):	realign = rcalign; break;
    case sizeof(short):	realign = rsalign; break;
    case sizeof(Int):	realign = rialign; break;
    }
    if (sizeof(uindex) < rusize || sizeof(ssizet) < rtsize ||
	sizeof(sector) < rdsize) {
	error("Cannot restore uindex, ssizet or sector of greater width");
    }
    secsize = (UCHAR(rheader[DUMP_SECSIZE + 0]) << 8) |
	       UCHAR(rheader[DUMP_SECSIZE + 1]);
    if ((rpsize >> 4) > 1) {
	error("Cannot restore hindex != 1");
    }
    rpsize &= 0xf;

    sw_restore(fd, secsize);
    kf_restore(fd);
    o_restore(fd, (uindex) ((conv_lwos) ? 1 << (rusize * 8 - 1) : 0));

    posn = P_lseek(fd, 0L, SEEK_CUR);	/* preserve current file position */
    o_conv(conv_callouts, conv_lwos, conv_ctrls); /* convert all objects */
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
	    sz = sizeof(uindex);
	    rsz = rusize;
	    al = ualign;
	    ral = rualign;
	    break;

	case 'i':	/* Int */
	    sz = rsz = sizeof(Int);
	    al = ialign;
	    ral = rialign;
	    break;

	case 't':	/* ssizet */
	    sz = sizeof(ssizet);
	    rsz = rtsize;
	    al = talign;
	    ral = rtalign;
	    break;

	case 'd':	/* sector */
	    sz = sizeof(sector);
	    rsz = rdsize;
	    al = dalign;
	    ral = rdalign;
	    break;

	case 'e':	/* eindex */
	    sz = sizeof(eindex);
	    rsz = resize;
	    al = ealign;
	    ral = realign;
	    break;

	case 'p':	/* pointer */
	    sz = sizeof(char*);
	    rsz = rpsize;
	    al = palign;
	    ral = rpalign;
	    break;

	case 'x':	/* hte */
	    size = ALGN(size, zalign);
	    size = ALGN(size, palign);
	    size += sizeof(char*);
	    size = ALGN(size, palign);
	    size += sizeof(char*);
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
		if (sizeof(uindex) == rusize) {
		    if (sizeof(uindex) == sizeof(short)) {
			buf[i + s0] = rbuf[ri + rs0];
			buf[i + s1] = rbuf[ri + rs1];
		    } else {
			buf[i + i0] = rbuf[ri + ri0];
			buf[i + i1] = rbuf[ri + ri1];
			buf[i + i2] = rbuf[ri + ri2];
			buf[i + i3] = rbuf[ri + ri3];
		    }
		} else {
		    j = (UCHAR(rbuf[ri + rs0] & rbuf[ri + rs1]) == 0xff) ?
			 -1 : 0;
		    buf[i + i0] = j;
		    buf[i + i1] = j;
		    buf[i + i2] = rbuf[ri + rs0];
		    buf[i + i3] = rbuf[ri + rs1];
		}
		i += sizeof(uindex);
		ri += rusize;
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

	    case 't':
		i = ALGN(i, talign);
		ri = ALGN(ri, rtalign);
		if (sizeof(ssizet) == rtsize) {
		    if (sizeof(ssizet) == sizeof(short)) {
			buf[i + s0] = rbuf[ri + rs0];
			buf[i + s1] = rbuf[ri + rs1];
		    } else {
			buf[i + i0] = rbuf[ri + ri0];
			buf[i + i1] = rbuf[ri + ri1];
			buf[i + i2] = rbuf[ri + ri2];
			buf[i + i3] = rbuf[ri + ri3];
		    }
		} else {
		    buf[i + i0] = 0;
		    buf[i + i1] = 0;
		    buf[i + i2] = rbuf[ri + rs0];
		    buf[i + i3] = rbuf[ri + rs1];
		}
		i += sizeof(ssizet);
		ri += rtsize;
		break;

	    case 'd':
		i = ALGN(i, dalign);
		ri = ALGN(ri, rdalign);
		if (sizeof(sector) == rdsize) {
		    if (sizeof(sector) == sizeof(short)) {
			buf[i + s0] = rbuf[ri + rs0];
			buf[i + s1] = rbuf[ri + rs1];
		    } else {
			buf[i + i0] = rbuf[ri + ri0];
			buf[i + i1] = rbuf[ri + ri1];
			buf[i + i2] = rbuf[ri + ri2];
			buf[i + i3] = rbuf[ri + ri3];
		    }
		} else {
		    j = (UCHAR(rbuf[ri + rs0] & rbuf[ri + rs1]) == 0xff) ?
			 -1 : 0;
		    buf[i + i0] = j;
		    buf[i + i1] = j;
		    buf[i + i2] = rbuf[ri + rs0];
		    buf[i + i3] = rbuf[ri + rs1];
		}
		i += sizeof(sector);
		ri += rdsize;
		break;

	    case 'e':
		i = ALGN(i, ealign);
		ri = ALGN(ri, realign);
		i += sizeof(eindex);
		ri += resize;
		break;

	    case 'p':
		i = ALGN(i, palign);
		ri = ALGN(ri, rpalign);
		for (j = sizeof(char*); j > 0; --j) {
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
		for (j = sizeof(char*); j > 0; --j) {
		    buf[i++] = 0;
		}
		i = ALGN(i, palign);
		for (j = sizeof(char*); j > 0; --j) {
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


# define MAX_PORTS	32
# define MAX_DIRS	32

static char *dirs[MAX_DIRS], *bhosts[MAX_PORTS], *thosts[MAX_PORTS];
static unsigned short bports[MAX_PORTS], tports[MAX_PORTS];
static int ntports, nbports;

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
    char **hosts;
    unsigned short *ports;

    for (h = NR_OPTIONS; h > 0; ) {
	conf[--h].set = FALSE;
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
	    } else if (conf[m].type != '[' || c != INT_CONST) {
		if (conf[m].type == '[' && c == '(') {
		    c = '[';
		} else {
		    conferr("bad value type");
		    return FALSE;
		}
	    }
	}

	switch (c) {
	case INT_CONST:
	    if (yylval.number < conf[m].low ||
		(conf[m].high != 0 && yylval.number > conf[m].high)) {
		conferr("int value out of range");
		return FALSE;
	    }
	    switch (m) {
	    case BINARY_PORT:
		bhosts[0] = (char *) NULL;
		bports[0] = yylval.number;
		nbports = 1;
		break;

	    case TELNET_PORT:
		thosts[0] = (char *) NULL;
		tports[0] = yylval.number;
		ntports = 1;
		break;

	    default:
		conf[m].u.num = yylval.number;
		break;
	    }
	    break;

	case STRING_CONST:
	    p = (conf[m].resolv) ? path_resolve(buf, yytext) : yytext;
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
		dirs[l] = strcpy(ALLOC(char, strlen(yytext) + 1), yytext);
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

	case '[':
	    if (pp_gettok() != '[') {
		conferr("'[' expected");
		return FALSE;
	    }
	    l = 0;
	    if ((c=pp_gettok()) != ']') {
		if (m == BINARY_PORT) {
		    hosts = bhosts;
		    ports = bports;
		} else {
		    hosts = thosts;
		    ports = tports;
		}
		for (;;) {
		    if (l == MAX_PORTS) {
			conferr("too many ports");
			return FALSE;
		    }
		    if (c != STRING_CONST) {
			conferr("string expected");
			return FALSE;
		    }
		    if (strcmp(yytext, "*") == 0) {
			hosts[l] = (char *) NULL;
		    } else {
			m_static();
			hosts[l] = strcpy(ALLOC(char, strlen(yytext) + 1),
					  yytext);
			m_dynamic();
		    }
		    if (pp_gettok() != ':') {
			conferr("':' expected");
			return FALSE;
		    }
		    if (pp_gettok() != INT_CONST) {
			conferr("integer expected");
			return FALSE;
		    }
		    if (yylval.number <= 0 || yylval.number > USHRT_MAX) {
			conferr("int value out of range");
			return FALSE;
		    }
		    ports[l++] = yylval.number;
		    if ((c=pp_gettok()) == ']') {
			break;
		    }
		    if (c != ',') {
			conferr("',' expected");
			return FALSE;
		    }
		    c = pp_gettok();
		}
	    }
	    if (pp_gettok() != ')') {
		conferr("')' expected");
		return FALSE;
	    }
	    if (m == TELNET_PORT) {
		ntports = l;
	    } else {
		nbports = l;
	    }
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
    cputs("# define ST_SWAPUSED\t5\t/* # sectors in use */\012");
    cputs("# define ST_SECTORSIZE\t6\t/* size of swap sector */\012");
    cputs("# define ST_SWAPRATE1\t7\t/* # objects swapped out last minute */\012");
    cputs("# define ST_SWAPRATE5\t8\t/* # objects swapped out last five minutes */\012");
    cputs("# define ST_SMEMSIZE\t9\t/* static memory allocated */\012");
    cputs("# define ST_SMEMUSED\t10\t/* static memory in use */\012");
    cputs("# define ST_DMEMSIZE\t11\t/* dynamic memory allocated */\012");
    cputs("# define ST_DMEMUSED\t12\t/* dynamic memory in use */\012");
    cputs("# define ST_OTABSIZE\t13\t/* object table size */\012");
    cputs("# define ST_NOBJECTS\t14\t/* # objects in use */\012");
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
    cputs("# define ST_TELNETPORTS\t25\t/* telnet ports */\012");
    cputs("# define ST_BINARYPORTS\t26\t/* binary ports */\012");

    cputs("\012# define O_COMPILETIME\t0\t/* time of compilation */\012");
    cputs("# define O_PROGSIZE\t1\t/* program size of object */\012");
    cputs("# define O_DATASIZE\t2\t/* # variables in object */\012");
    cputs("# define O_NSECTORS\t3\t/* # sectors used by object */\012");
    cputs("# define O_CALLOUTS\t4\t/* callouts in object */\012");
    cputs("# define O_INDEX\t5\t/* unique ID for master object */\012");
    cputs("# define O_UNDEFINED\t6\t/* undefined functions */\012");

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
    sprintf(buffer, "# define MAX_STRING_SIZE\t%u\t\t/* max string size (obsolete) */\012",
	    MAX_STRLEN);
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


# ifdef DGD_EXTENSION
extern void extension_init	P((void));
# endif

void (*ext_restore)	P((object*));
void (*ext_swapout)	P((object*));
void (*ext_destruct)	P((object*));
bool (*ext_funcall)	P((frame*, int, value*, char*));
void (*ext_cleanup)	P((void));
void (*ext_finish)	P((void));

/*
 * NAME:	config->init()
 * DESCRIPTION:	initialize the driver
 */
bool conf_init(configfile, dumpfile, fragment)
char *configfile, *dumpfile;
sector *fragment;
{
    char buf[STRINGSZ];
    int fd;
    bool init;

    /*
     * process config file
     */
    if (!pp_init(path_native(buf, configfile), (char **) NULL, (char *) NULL,
		 0, 0)) {
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
		   thosts, bhosts,
		   tports, bports,
		   ntports, nbports)) {
	comm_finish();
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* initialize arrays */
    arr_init((int) conf[ARRAY_SIZE].u.num);

    /* initialize objects */
    o_init((uindex) conf[OBJECTS].u.num);

    /* initialize swap device */
    sw_init(conf[SWAP_FILE].u.str,
	    (sector) conf[SWAP_SIZE].u.num,
	    (sector) conf[CACHE_SIZE].u.num,
	    (unsigned int) conf[SECTOR_SIZE].u.num,
	    (Uint) conf[DUMP_INTERVAL].u.num);

    /* initialize swapped data handler */
    d_init(conf[TYPECHECKING].u.num == 2);
    *fragment = conf[SWAP_FRAGMENT].u.num;

    /* initalize editor */
    ed_init(conf[ED_TMPFILE].u.str,
	    (int) conf[EDITORS].u.num);

    /* initialize call_outs */
    if (!co_init((uindex) conf[CALL_OUTS].u.num)) {
	sw_finish();
	comm_finish();
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    ext_restore =  (void (*) P((object*))) NULL;
    ext_swapout =  (void (*) P((object*))) NULL;
    ext_destruct = (void (*) P((object*))) NULL;
    ext_funcall =  (bool (*) P((frame*, int, value*, char*))) NULL;
    ext_cleanup =  (void (*) P((void))) NULL;
    ext_finish =   (void (*) P((void))) NULL;

    /* remove previously added kfuns */
    kf_clear();

# ifdef DGD_EXTENSION
    extension_init();
# endif

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
	sw_finish();
	comm_finish();
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* load precompiled objects */
    if (!pc_preload(conf[AUTO_OBJECT].u.str, conf[DRIVER_OBJECT].u.str)) {
	sw_finish();
	comm_finish();
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* initialize dumpfile header */
    conf_dumpinit();

    m_static();				/* allocate error context statically */
    ec_push((ec_ftn) NULL);		/* guard error context */
    if (ec_push((ec_ftn) NULL)) {
	message((char *) NULL);
	endthread();
	message("Config error: initialization failed\012");	/* LF */
	ec_pop();			/* remove guard */

	sw_finish();
	comm_finish();
	ed_finish();
	if (dumpfile != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }
    m_dynamic();
    if (dumpfile == (char *) NULL) {
	/* initialize mudlib */
	if (ec_push((ec_ftn) errhandler)) {
	    error((char *) NULL);
	}
	call_driver_object(cframe, "initialize", 0);
	ec_pop();
    } else {
	/* restore dump file */
	conf_restore(fd);
	P_close(fd);

	/* notify mudlib */
	if (ec_push((ec_ftn) errhandler)) {
	    error((char *) NULL);
	}
	call_driver_object(cframe, "restored", 0);
	ec_pop();
    }
    ec_pop();
    i_del_value(cframe->sp++);
    endthread();
    ec_pop();				/* remove guard */

    /* start accepting connections */
    comm_listen();

    return TRUE;
}

/*
 * NAME:	config->ext_callback()
 * DESCRIPTION:	initialize callbacks for extension interface
 */
void conf_ext_callback(restore, swapout, destruct, funcall, cleanup, finish)
void (*restore) P((object*));
void (*swapout) P((object*));
void (*destruct) P((object*));
bool (*funcall) P((frame*, int, value*, char*));
void (*cleanup) P((void));
void (*finish) P((void));
{
    ext_restore =  restore;
    ext_swapout =  swapout;
    ext_destruct = destruct;
    ext_funcall =  funcall;
    ext_cleanup =  cleanup;
    ext_finish =   finish;
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
register frame *f;
Int idx;
register value *v;
{
    char *version;
    uindex ncoshort, ncolong;
    array *a;
    register int i;

    switch (idx) {
    case 0:	/* ST_VERSION */
	version = VERSION;
	PUT_STRVAL(v, str_new(version, (long) strlen(version)));
	break;

    case 1:	/* ST_STARTTIME */
	PUT_INTVAL(v, starttime);
	break;

    case 2:	/* ST_BOOTTIME */
	PUT_INTVAL(v, boottime);
	break;

    case 3:	/* ST_UPTIME */
	PUT_INTVAL(v, elapsed + P_time() - boottime);
	break;

    case 4:	/* ST_SWAPSIZE */
	PUT_INTVAL(v, conf[SWAP_SIZE].u.num);
	break;

    case 5:	/* ST_SWAPUSED */
	PUT_INTVAL(v, sw_count());
	break;

    case 6:	/* ST_SECTORSIZE */
	PUT_INTVAL(v, conf[SECTOR_SIZE].u.num);
	break;

    case 7:	/* ST_SWAPRATE1 */
	PUT_INTVAL(v, co_swaprate1());
	break;

    case 8:	/* ST_SWAPRATE5 */
	PUT_INTVAL(v, co_swaprate5());
	break;

    case 9:	/* ST_SMEMSIZE */
	PUT_INTVAL(v, m_info()->smemsize);
	break;

    case 10:	/* ST_SMEMUSED */
	PUT_INTVAL(v, m_info()->smemused);
	break;

    case 11:	/* ST_DMEMSIZE */
	PUT_INTVAL(v, m_info()->dmemsize);
	break;

    case 12:	/* ST_DMEMUSED */
	PUT_INTVAL(v, m_info()->dmemused);
	break;

    case 13:	/* ST_OTABSIZE */
	PUT_INTVAL(v, conf[OBJECTS].u.num);
	break;

    case 14:	/* ST_NOBJECTS */
	PUT_INTVAL(v, o_count());
	break;

    case 15:	/* ST_COTABSIZE */
	PUT_INTVAL(v, conf[CALL_OUTS].u.num);
	break;

    case 16:	/* ST_NCOSHORT */
	co_info(&ncoshort, &ncolong);
	PUT_INTVAL(v, ncoshort);
	break;

    case 17:	/* ST_NCOLONG */
	co_info(&ncoshort, &ncolong);
	PUT_INTVAL(v, ncolong);
	break;

    case 18:	/* ST_UTABSIZE */
	PUT_INTVAL(v, conf[USERS].u.num);
	break;

    case 19:	/* ST_ETABSIZE */
	PUT_INTVAL(v, conf[EDITORS].u.num);
	break;

    case 20:	/* ST_STRSIZE */
	PUT_INTVAL(v, MAX_STRLEN);
	break;

    case 21:	/* ST_ARRAYSIZE */
	PUT_INTVAL(v, conf[ARRAY_SIZE].u.num);
	break;

    case 22:	/* ST_STACKDEPTH */
	PUT_INTVAL(v, i_get_depth(f));
	break;

    case 23:	/* ST_TICKS */
	PUT_INTVAL(v, i_get_ticks(f));
	break;

    case 24:	/* ST_PRECOMPILED */
	a = pc_list(f->data);
	if (a != (array *) NULL) {
	    PUT_ARRVAL(v, a);
	} else {
	    *v = nil_value;
	}
	break;

    case 25:	/* ST_TELNETPORTS */
	a = arr_new(f->data, (long) ntports);
	PUT_ARRVAL(v, a);
	for (i = 0, v = a->elts; i < ntports; i++, v++) {
	    PUT_INTVAL(v, tports[i]);
	}
	break;

    case 26:	/* ST_BINARYPORTS */
	a = arr_new(f->data, (long) nbports);
	PUT_ARRVAL(v, a);
	for (i = 0, v = a->elts; i < nbports; i++, v++) {
	    PUT_INTVAL(v, bports[i]);
	}
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

    if (ec_push((ec_ftn) NULL)) {
	arr_ref(a);
	arr_del(a);
	error((char *) NULL);
    }
    a = arr_ext_new(f->data, 27L);
    for (i = 0, v = a->elts; i < 27; i++, v++) {
	conf_statusi(f, i, v);
    }
    ec_pop();

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

    prog = (obj->flags & O_MASTER) ? obj : OBJR(obj->u_master);
    ctrl = (O_UPGRADING(prog)) ? OBJR(prog->prev)->ctrl : o_control(prog);

    switch (idx) {
    case 0:	/* O_COMPILETIME */
	PUT_INTVAL(v, ctrl->compiled);
	break;

    case 1:	/* O_PROGSIZE */
	PUT_INTVAL(v, d_get_progsize(ctrl));
	break;

    case 2:	/* O_DATASIZE */
	PUT_INTVAL(v, ctrl->nvariables);
	break;

    case 3:	/* O_NSECTORS */
	PUT_INTVAL(v, (O_HASDATA(obj)) ?  o_dataspace(obj)->nsectors : 0);
	if (obj->flags & O_MASTER) {
	    v->u.number += ctrl->nsectors;
	}
	break;

    case 4:	/* O_CALLOUTS */
	if (O_HASDATA(obj)) {
	    a = d_list_callouts(data, o_dataspace(obj));
	    if (a != (array *) NULL) {
		PUT_ARRVAL(v, a);
	    } else {
		*v = nil_value;
	    }
	} else {
	    PUT_ARRVAL(v, arr_new(data, 0L));
	}
	break;

    case 5:	/* O_INDEX */
	PUT_INTVAL(v, (obj->flags & O_MASTER) ?
		       (Uint) obj->index : obj->u_master);
	break;

    case 6:	/* O_UNDEFINED */
	if (ctrl->flags & CTRL_UNDEFINED) {
	    PUT_MAPVAL(v, ctrl_undefined(data, ctrl));
	} else {
	    *v = nil_value;
	}
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

    a = arr_ext_new(data, 7L);
    if (ec_push((ec_ftn) NULL)) {
	arr_ref(a);
	arr_del(a);
	error((char *) NULL);
    }
    for (i = 0, v = a->elts; i < 7; i++, v++) {
	conf_objecti(data, obj, i, v);
    }
    ec_pop();

    return a;
}


/*
 * NAME:	strtoint()
 * DESCRIPTION:	retrieve an Int from a string (utility function)
 */
Int strtoint(str)
char **str;
{
    register char *p;
    register Int i;
    Int sign;

    p = *str;
    if (*p == '-') {
	p++;
	sign = -1;
    } else {
	sign = 1;
    }

    i = 0;
    while (isdigit(*p)) {
	if ((Uint) i > (Uint) 214748364L) {
	    return 0;
	}
	i = i * 10 + *p++ - '0';
	if (i < 0 && ((Uint) i != (Uint) 0x80000000L || sign > 0)) {
	    return 0;
	}
    }

    *str = p;
    return sign * i;
}
