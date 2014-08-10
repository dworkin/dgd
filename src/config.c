/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2014 DGD Authors (see the commit log for details)
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

# define INCLUDE_FILE_IO
# define INCLUDE_CTYPE
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
							1, UINDEX_MAX },
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
				{ "dynamic_chunk",	INT_CONST, FALSE, FALSE,
							1024 },
# define ED_TMPFILE	11
				{ "ed_tmpfile",		STRING_CONST },
# define EDITORS	12
				{ "editors",		INT_CONST, FALSE, FALSE,
							0, EINDEX_MAX },
# define HOTBOOT	13
				{ "hotboot",		'(' },
# define INCLUDE_DIRS	14
				{ "include_dirs",	'(' },
# define INCLUDE_FILE	15
				{ "include_file",	STRING_CONST, TRUE },
# define MODULES	16
				{ "modules",		'(' },
# define OBJECTS	17
				{ "objects",		INT_CONST, FALSE, FALSE,
							2, UINDEX_MAX },
# define PORTS		18
				{ "ports",		INT_CONST, FALSE, FALSE,
							1, 32 },
# define SECTOR_SIZE	19
				{ "sector_size",	INT_CONST, FALSE, FALSE,
							512, 65535 },
# define STATIC_CHUNK	20
				{ "static_chunk",	INT_CONST },
# define SWAP_FILE	21
				{ "swap_file",		STRING_CONST },
# define SWAP_FRAGMENT	22
				{ "swap_fragment",	INT_CONST, FALSE, FALSE,
							0, SW_UNUSED },
# define SWAP_SIZE	23
				{ "swap_size",		INT_CONST, FALSE, FALSE,
							1024, SW_UNUSED },
# define TELNET_PORT	24
				{ "telnet_port",	'[', FALSE, FALSE,
							1, USHRT_MAX },
# define TYPECHECKING	25
				{ "typechecking",	INT_CONST, FALSE, FALSE,
							0, 2 },
# define USERS		26
				{ "users",		INT_CONST, FALSE, FALSE,
							1, EINDEX_MAX },
# define NR_OPTIONS	27
};


typedef struct { char fill; char c;	} alignc;
typedef struct { char fill; short s;	} aligns;
typedef struct { char fill; Int i;	} aligni;
typedef struct { char fill; char *p;	} alignp;
typedef struct { char c;		} alignz;

# define FORMAT_VERSION	14

# define DUMP_VALID	0	/* valid dump flag */
# define DUMP_VERSION	1	/* snapshot version number */
# define DUMP_MODEL	2	/* 0: vanilla DGD, 1: iChat DGD */
# define DUMP_TYPECHECK	3	/* global typechecking */
# define DUMP_SECSIZE	4	/* sector size */
# define DUMP_TYPE	4	/* first XX bytes, dump type */
# define DUMP_HEADERSZ	28	/* header size */
# define DUMP_STARTTIME	28	/* start time */
# define DUMP_ELAPSED	32	/* elapsed time */
# define DUMP_VSTRING	42	/* version string */

# define FLAGS_PARTIAL	0x01	/* partial snapshot */
# define FLAGS_HOTBOOT	0x04	/* hotboot snapshot */

typedef char dumpinfo[64];

static dumpinfo header;		/* snapshot header */
# define s0	(header[ 6])	/* short, msb */
# define s1	(header[ 7])	/* short, lsb */
# define i0	(header[ 8])	/* Int, msb */
# define i1	(header[ 9])
# define i2	(header[10])
# define i3	(header[11])	/* Int, lsb */
# define utsize	(header[20])	/* sizeof(uindex) + sizeof(ssizet) */
# define desize	(header[21])	/* sizeof(sector) + sizeof(eindex) */
# define psize	(header[22])	/* sizeof(char*), upper nibble reserved */
# define calign	(header[23])	/* align(char) */
# define salign	(header[24])	/* align(short) */
# define ialign	(header[25])	/* align(Int) */
# define palign	(header[26])	/* align(char*) */
# define zalign	(header[27])	/* align(struct) */
# define zero1	(header[36])	/* reserved (0) */
# define zero2	(header[37])	/* reserved (0) */
# define zero3	(header[38])	/* reserved (0) */
# define zero4	(header[39])	/* reserved (0) */
# define dflags	(header[40])	/* flags */
# define zero5	(header[41])	/* reserved (0) */
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
# define rutsize (rheader[20])	/* sizeof(uindex) + sizeof(ssizet) */
# define rdesize (rheader[21])	/* sizeof(sector) + sizeof(eindex) */
# define rpsize	(rheader[22])	/* sizeof(char*), upper nibble reserved */
# define rcalign (rheader[23])	/* align(char) */
# define rsalign (rheader[24])	/* align(short) */
# define rialign (rheader[25])	/* align(Int) */
# define rpalign (rheader[26])	/* align(char*) */
# define rzalign (rheader[27])	/* align(struct) */
# define rzero1	 (rheader[36])	/* reserved (0) */
# define rzero2	 (rheader[37])	/* reserved (0) */
# define rzero3	 (rheader[38])	/* reserved (0) */
# define rzero4	 (rheader[39])	/* reserved (0) */
# define rdflags (rheader[40])	/* flags */
# define rzero5	 (rheader[41])	/* reserved (0) */
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
 * DESCRIPTION:	initialize snapshot information
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
    header[DUMP_VERSION] = FORMAT_VERSION;	/* snapshot version number */
    header[DUMP_MODEL] = 0;			/* vanilla DGD */
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
    zero1 = zero2 = 0;

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
void conf_dump(bool incr, bool boot)
{
    int fd;
    Uint etime;

    header[DUMP_VERSION] = FORMAT_VERSION;
    header[DUMP_TYPECHECK] = conf[TYPECHECKING].u.num;
    header[DUMP_STARTTIME + 0] = starttime >> 24;
    header[DUMP_STARTTIME + 1] = starttime >> 16;
    header[DUMP_STARTTIME + 2] = starttime >> 8;
    header[DUMP_STARTTIME + 3] = starttime;
    etime = P_time();
    if (etime < boottime) {
	etime = boottime;
    }
    etime += elapsed - boottime;
    header[DUMP_ELAPSED + 0] = etime >> 24;
    header[DUMP_ELAPSED + 1] = etime >> 16;
    header[DUMP_ELAPSED + 2] = etime >> 8;
    header[DUMP_ELAPSED + 3] = etime;

    if (!incr) {
	o_copy(0);
    }
    d_swapout(1);
    dflags = 0;
    if (o_dobjects() > 0) {
	dflags |= FLAGS_PARTIAL;
    }
    fd = sw_dump(conf[DUMP_FILE].u.str, dflags & FLAGS_PARTIAL);
    if (!kf_dump(fd)) {
	fatal("failed to dump kfun table");
    }
    if (!o_dump(fd, incr)) {
	fatal("failed to dump object table");
    }
    if (!pc_dump(fd)) {
	fatal("failed to dump precompiled objects");
    }
    if (!co_dump(fd)) {
	fatal("failed to dump callout table");
    }
    if (boot) {
	boot = comm_dump(fd);
	if (boot) {
	    dflags |= FLAGS_HOTBOOT;
	}
    }

    sw_dump2(header, sizeof(dumpinfo), incr);
}

/*
 * NAME:	conf->header()
 * DESCRIPTION:	restore a snapshot header
 */
static unsigned int conf_header(int fd, dumpinfo h)
{
    unsigned int secsize;
    off_t offset;

    for (;;) {
	if (P_read(fd, h, sizeof(dumpinfo)) != sizeof(dumpinfo) ||
		   h[DUMP_VALID] != 1 ||
		   h[DUMP_VERSION] < 2 ||
		   h[DUMP_VERSION] > FORMAT_VERSION) {
	    error("Bad or incompatible restore file header");
	}
	secsize = (UCHAR(h[DUMP_SECSIZE + 0]) << 8) |
		   UCHAR(h[DUMP_SECSIZE + 1]);
	offset = (UCHAR(h[sizeof(dumpinfo) - 4]) << 24) |
		 (UCHAR(h[sizeof(dumpinfo) - 3]) << 16) |
		 (UCHAR(h[sizeof(dumpinfo) - 2]) << 8) |
		  UCHAR(h[sizeof(dumpinfo) - 1]);
	if (offset == 0) {
	    P_lseek(fd, secsize - sizeof(dumpinfo), SEEK_CUR);
	    return secsize;
	}

	P_lseek(fd, offset * secsize, SEEK_SET);
    }
}

/*
 * NAME:	conf->restore()
 * DESCRIPTION:	restore system state from file
 */
static bool conf_restore(int fd, int fd2)
{
    bool conv_co1, conv_co2, conv_co3, conv_lwo, conv_ctrl1, conv_ctrl2,
    conv_data, conv_type, conv_inherit, conv_time, conv_vm;
    unsigned int secsize;

    secsize = conf_header(fd, rheader);
    conv_co1 = conv_co2 = conv_co3 = conv_lwo = conv_ctrl1 = conv_ctrl2 =
	       conv_data = conv_type = conv_inherit = conv_time = conv_vm =
	       FALSE;
    if (rheader[DUMP_VERSION] < 3) {
	conv_co1 = TRUE;
    }
    if (rheader[DUMP_VERSION] < 4) {
	conv_lwo = TRUE;
    }
    if (rheader[DUMP_VERSION] < 5) {
	conv_ctrl1 = TRUE;
    }
    if (rheader[DUMP_VERSION] < 6) {
	conv_data = TRUE;
	rzero1 = rzero2 = 0;
    }
    if (rheader[DUMP_VERSION] < 7) {
	conv_co2 = TRUE;
    }
    if (rheader[DUMP_VERSION] < 8) {
	conv_type = TRUE;
    }
    if (rheader[DUMP_VERSION] < 9) {
	conv_ctrl2 = TRUE;
    }
    if (rheader[DUMP_VERSION] < 10) {
	conv_inherit = TRUE;
    }
    if (rheader[DUMP_VERSION] < 11) {
	conv_co3 = TRUE;
    }
    if (rheader[DUMP_VERSION] < 12) {
	memmove(rheader + 20, rheader + 12, 18);
	rzero3 = rzero4 = rdflags = rzero5 = 0;
    }
    if (rheader[DUMP_VERSION] < 13) {
	conv_time = TRUE;
    }
    if (rheader[DUMP_VERSION] < 14) {
	conv_vm = TRUE;
    }
    header[DUMP_VERSION] = rheader[DUMP_VERSION];
    if (memcmp(header, rheader, DUMP_TYPE) != 0 || rzero1 != 0 || rzero2 != 0 ||
	rzero3 != 0 || rzero4 != 0 || rzero5 != 0) {
	error("Bad or incompatible restore file header");
    }
    if (rdflags & FLAGS_PARTIAL) {
	dumpinfo h;

	/* secondary snapshot required */
	if (fd2 < 0) {
	    error("Missing secondary snapshot");
	}
	conf_header(fd2, h);
	if (memcmp(rheader, h, DUMP_HEADERSZ) != 0) {
	    error("Secondary snapshot has different type");
	}
	sw_restore2(fd2);
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
    if ((rcalign >> 4) != 0) {
	error("Cannot restore arrsize > 2");
    }
    if ((rsalign >> 4) != 0) {
	error("Cannot restore Int size > 4");
    }
    rialign &= 0xf;
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
    if ((rpsize >> 4) > 1) {
	error("Cannot restore hindex > 1");	/* Hydra only */
    }
    rpsize &= 0xf;

    sw_restore(fd, secsize);
    kf_restore(fd, conv_co1);
    o_restore(fd, (uindex) ((conv_lwo) ? 1 << (rusize * 8 - 1) : 0),
	      rdflags & FLAGS_PARTIAL);
    d_init_conv(conv_ctrl1, conv_ctrl2, conv_data, conv_co1, conv_co2,
		conv_type, conv_inherit, conv_time, conv_vm);
    pc_restore(fd, conv_inherit);
    boottime = P_time();
    co_restore(fd, boottime, conv_co2, conv_co3, conv_time);

    if (fd2 >= 0) {
	P_close(fd2);
    }

    return ((rdflags & FLAGS_HOTBOOT) && comm_restore(fd));
}

/*
 * NAME:	conf->dsize()
 * DESCRIPTION:	compute the size and alignment of a struct
 *		0x000000ff size in snapshot
 *		0x0000ff00 alignment in snapshot
 *		0x00ff0000 size
 *		0xff000000 alignment
 */
Uint conf_dsize(char *layout)
{
    char *p;
    Uint sz, rsz, al, ral;
    Uint size, rsize, align, ralign;

    p = layout;
    size = rsize = 0;
    align = ralign = 1;
    sz = rsz = al = ral = 0;

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
 * DESCRIPTION:	convert structs from snapshot format
 */
Uint conf_dconv(char *buf, char *rbuf, char *layout, Uint n)
{
    Uint i, ri, j, size, rsize;
    char *p;

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
 * DESCRIPTION:	read from snapshot
 */
void conf_dread(int fd, char *buf, char *layout, Uint n)
{
    char buffer[16384];
    unsigned int i, size, rsize;
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
	    fatal("cannot read from snapshot");
	}
	conf_dconv(buf, buffer, layout, (Uint) i);
	buf += size * i;
	n -= i;
    }
}


# define MAX_PORTS	32
# define MAX_STRINGS	32

static char *hotboot[MAX_STRINGS], *dirs[MAX_STRINGS], *modules[MAX_STRINGS];
static char *bhosts[MAX_PORTS], *thosts[MAX_PORTS];
static unsigned short bports[MAX_PORTS], tports[MAX_PORTS];
static int ntports, nbports;

/*
 * NAME:	conferr()
 * DESCRIPTION:	error during the configuration phase
 */
static void conferr(char *err)
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
    char *p;
    int h, l, m, c;
    char **strs;
    unsigned short *ports;

    for (h = NR_OPTIONS; h > 0; ) {
	conf[--h].set = FALSE;
    }
    memset(dirs, '\0', sizeof(dirs));
    strs = (char **) NULL;

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
	    switch (m) {
	    case HOTBOOT:	strs = hotboot; break;
	    case INCLUDE_DIRS:	strs = dirs; break;
	    case MODULES:	strs = modules; break;
	    }
	    for (;;) {
		if (pp_gettok() != STRING_CONST) {
		    conferr("string expected");
		    return FALSE;
		}
		if (l == MAX_STRINGS - 1) {
		    conferr("array too large");
		    return FALSE;
		}
		m_static();
		strs[l] = strcpy(ALLOC(char, strlen(yytext) + 1), yytext);
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
	    strs[l] = (char *) NULL;
	    break;

	case '[':
	    if (pp_gettok() != '[') {
		conferr("'[' expected");
		return FALSE;
	    }
	    l = 0;
	    if ((c=pp_gettok()) != ']') {
		if (m == BINARY_PORT) {
		    strs = bhosts;
		    ports = bports;
		} else {
		    strs = thosts;
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
			strs[l] = (char *) NULL;
		    } else {
			m_static();
			strs[l] = strcpy(ALLOC(char, strlen(yytext) + 1),
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
	if (!conf[l].set && l != HOTBOOT && l != MODULES && l != CACHE_SIZE) {
	    char buffer[64];

#ifndef NETWORK_EXTENSIONS
	    /* don't complain about the ports option not being
	       specified if the network extensions are disabled */
	    if (l == PORTS) {
		continue;
	    }
#endif
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
static bool copen(char *file)
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
static void cputs(char *str)
{
    unsigned int len, chunk;

    len = strlen(str);
    while (bufsz + len > BUF_SIZE) {
	chunk = BUF_SIZE - bufsz;
	memcpy(obuf + bufsz, str, chunk);
	(void) P_write(fd, obuf, BUF_SIZE);
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
    register int i;
    register char *p;

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
    cputs("# define ST_COTABSIZE\t15\t/* callout table size */\012");
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
# ifdef TYPEOFDETAIL

#ifdef CLOSURES
    sprintf(buffer, "# define T_FUNCTION\t%d\012", T_FUNCTION);
    cputs(buffer);
# endif

#endif
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
    cputs("# define INT_MAX\t\t2147483647\t/* max integer value */\012");
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
    cputs("# define FLT_DIG\t11\t\t\t/* decimal digits of precision*/\012");
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
    if (!cclose()) {
	return FALSE;
    }

    /* create kfun.h file */
    sprintf(buffer, "%s/kfun.h", dirs[0]);
    if (!copen(buffer)) {
	return FALSE;
    }
    cputs("/*\012 * This file defines the version of each supported kfun.  ");
    cputs("It is automatically\012 * generated by DGD on ");
    cputs("startup.\012 */\012\012");
    for (i = KF_BUILTINS; i < nkfun; i++) {
	if (!isdigit(kftab[i].name[0])) {
	    sprintf(buffer, "# define kf_%s\t\t%d\012", kftab[i].name,
		    kftab[i].version);
	    for (p = buffer + 9; *p != '\0'; p++) {
		if (*p == '.') {
		    *p = '_';
		} else {
		    *p = toupper(*p);
		}
	    }
	    cputs(buffer);
	}
    }
    cputs("\012\012/*\012 * Supported ciphers and hashing algorithms.\012 */");
    cputs("\012\012# define ENCRYPT_CIPHERS\t");
    for (i = 0; ; ) {
	sprintf(buffer, "\"%s\"", kfenc[i].name);
	cputs(buffer);
	if (++i == ne) {
	    break;
	}
	cputs(", ");
    }
    cputs("\012# define DECRYPT_CIPHERS\t");
    for (i = 0; ; ) {
	sprintf(buffer, "\"%s\"", kfdec[i].name);
	cputs(buffer);
	if (++i == nd) {
	    break;
	}
	cputs(", ");
    }
    cputs("\012# define HASH_ALGORITHMS\t");
    for (i = 0; ; ) {
	sprintf(buffer, "\"%s\"", kfhsh[i].name);
	cputs(buffer);
	if (++i == nh) {
	    break;
	}
	cputs(", ");
    }
    cputs("\012");

    return cclose();
}


extern bool ext_dgd (char*);

/*
 * NAME:	config->init()
 * DESCRIPTION:	initialize the driver
 */
bool conf_init(char *configfile, char *snapshot, char *snapshot2, char *module,
	       sector *fragment)
{
    char buf[STRINGSZ];
    int fd, fd2, i;
    bool init;
    sector cache;

    fd = fd2 = -1;

    /*
     * process config file
     */
    if (!pp_init(path_native(buf, configfile), (char **) NULL, (string **) NULL,
		 0, 0)) {
	message("Config error: cannot open config file\012");	/* LF */
	m_finish();
	return FALSE;
    }
    init = conf_config();
    pp_clear();
    m_purge();
    if (!init) {
	m_finish();
	return FALSE;
    }

    /* make sure that we can handle the swapfile size */
    if( ((off_t) (sector) conf[SWAP_SIZE].u.num * (unsigned int) conf[SECTOR_SIZE].u.num) !=
	((Uuint) (sector) conf[SWAP_SIZE].u.num * (unsigned int) conf[SECTOR_SIZE].u.num)
    ) {
	P_message("Config error: swap file size overflow.\012");
	m_finish();
	return FALSE;
    }

    /* try to open the snapshot if one was provided */
    if (snapshot != (char *) NULL) {
	fd = P_open(path_native(buf, snapshot), O_RDONLY | O_BINARY, 0);
	if (fd < 0) {
	    P_message("Config error: cannot open restore file\012");    /* LF */
	    m_finish();
	    return FALSE;
	}
    }
    if (snapshot2 != (char *) NULL) {
	fd2 = P_open(path_native(buf, snapshot2), O_RDONLY | O_BINARY, 0);
	if (fd2 < 0) {
	    P_message("Config error: cannot open secondary restore file\012");    /* LF */
	    m_finish();
	    return FALSE;
	}
    }

    m_static();

    /* remove previously added kfuns */
    kf_clear();

    for (i = 0; modules[i] != NULL; i++) {
	if (!ext_dgd(modules[i])) {
	    message("Config error: cannot load runtime extension \"%s\"\012",
		    modules[i]);
	    if (snapshot2 != (char *) NULL) {
		P_close(fd2);
	    }
	    if (snapshot != (char *) NULL) {
		P_close(fd);
	    }
	    m_finish();
	    return FALSE;
	}
    }
    if (module != (char *) NULL && !ext_dgd(module)) {
	message("Config error: cannot load runtime extension \"%s\"\012",/* LF*/
		module);
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* initialize kfuns */
    kf_init();

    /* change directory */
    if (P_chdir(path_native(buf, conf[DIRECTORY].u.str)) < 0) {
	message("Config error: bad base directory \"%s\"\012",	/* LF */
		conf[DIRECTORY].u.str);
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* initialize communications */
    if (!comm_init((int) conf[USERS].u.num,
#ifdef NETWORK_EXTENSIONS
		   (int) conf[PORTS].u.num,
#endif
		   thosts, bhosts,
		   tports, bports,
		   ntports, nbports)) {
	comm_clear();
	comm_finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* initialize arrays */
    arr_init((int) conf[ARRAY_SIZE].u.num);

    /* initialize objects */
    o_init((uindex) conf[OBJECTS].u.num, (Uint) conf[DUMP_INTERVAL].u.num);

    /* initialize swap device */
    cache = (sector) ((conf[CACHE_SIZE].set) ? conf[CACHE_SIZE].u.num : 100);
    if (!sw_init(conf[SWAP_FILE].u.str,
	    (sector) conf[SWAP_SIZE].u.num,
	    cache,
	    (unsigned int) conf[SECTOR_SIZE].u.num)) {
	comm_clear();
	comm_finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* initialize swapped data handler */
    d_init();
    *fragment = conf[SWAP_FRAGMENT].u.num;

    /* initalize editor */
    ed_init(conf[ED_TMPFILE].u.str,
	    (int) conf[EDITORS].u.num);

    /* initialize call_outs */
    if (!co_init((uindex) conf[CALL_OUTS].u.num)) {
	sw_finish();
	comm_clear();
	comm_finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

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
	comm_clear();
	comm_finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }

    /* initialize snapshot header */
    conf_dumpinit();

    m_static();				/* allocate error context statically */
    ec_push((ec_ftn) NULL);		/* guard error context */
    if (ec_push((ec_ftn) NULL)) {
	message((char *) NULL);
	endthread();
	message("Config error: initialization failed\012");	/* LF */
	ec_pop();			/* remove guard */

	sw_finish();
	comm_clear();
	comm_finish();
	ed_finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	m_finish();
	return FALSE;
    }
    m_dynamic();
    if (snapshot == (char *) NULL) {
	/* initialize mudlib */
	d_converted();
	if (ec_push((ec_ftn) errhandler)) {
	    error((char *) NULL);
	}
	call_driver_object(cframe, "initialize", 0);
	ec_pop();
    } else {
	bool hotbooted;

	/* restore snapshot */
	hotbooted = conf_restore(fd, fd2);

	/* notify mudlib */
	if (ec_push((ec_ftn) errhandler)) {
	    error((char *) NULL);
	}
	if (hotbooted) {
	    PUSH_INTVAL(cframe, TRUE);
	    call_driver_object(cframe, "restored", 1);
	} else {
	    call_driver_object(cframe, "restored", 0);
	}
	ec_pop();
    }
    ec_pop();
    i_del_value(cframe->sp++);
    endthread();
    ec_pop();				/* remove guard */

#ifndef NETWORK_EXTENSIONS
    /* start accepting connections */
    comm_listen();
#endif
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
 * NAME:	config->hotboot()
 * DESCRIPTION:	return the hotboot executable
 */
char **conf_hotboot()
{
    return (conf[HOTBOOT].set) ? hotboot : (char **) NULL;
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
 * NAME:	putval()
 * DESCRIPTION:	store a size_t as an integer or as a float approximation
 */
static void putval(value *v, size_t n)
{
    xfloat f1, f2;

    if (n <= 0x7fffffffL) {
	PUT_INTVAL(v, n);
    } else {
	flt_itof((Int) (n >> 31), &f1);
	flt_ldexp(&f1, (Int) 31);
	flt_itof((Int) (n & 0x7fffffffL), &f2);
	flt_add(&f1, &f2);
	PUT_FLTVAL(v, f1);
    }
}

/*
 * NAME:	config->statusi()
 * DESCRIPTION:	return resource usage information
 */
bool conf_statusi(frame *f, Int idx, value *v)
{
    char *version;
    uindex ncoshort, ncolong;
    array *a;
    Uint t;
    int i;

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
	t = P_time();
	if (t < boottime) {
	    t = boottime;
	}
	PUT_INTVAL(v, elapsed + t - boottime);
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
	putval(v, m_info()->smemsize);
	break;

    case 10:	/* ST_SMEMUSED */
	putval(v, m_info()->smemused);
	break;

    case 11:	/* ST_DMEMSIZE */
	putval(v, m_info()->dmemsize);
	break;

    case 12:	/* ST_DMEMUSED */
	putval(v, m_info()->dmemused);
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
array *conf_status(frame *f)
{
    value *v;
    Int i;
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
bool conf_objecti(dataspace *data, object *obj, Int idx, value *v)
{
    control *ctrl;
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
array *conf_object(dataspace *data, object *obj)
{
    value *v;
    Int i;
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
Int strtoint(char **str)
{
    char *p;
    Int i;
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
