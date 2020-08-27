/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2020 DGD Authors (see the commit log for details)
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
# include "control.h"
# include "data.h"
# include "interpret.h"
# include "path.h"
# include "editor.h"
# include "call_out.h"
# include "comm.h"
# include "ext.h"
# include "version.h"
# include "macro.h"
# include "token.h"
# include "ppcontrol.h"
# include "node.h"
# include "parser.h"
# include "compile.h"
# include "table.h"

static Config conf[] = {
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
# define DATAGRAM_PORT	6
				{ "datagram_port",	'[', FALSE, FALSE,
							1, USHRT_MAX },
# define DATAGRAM_USERS	7
				{ "datagram_users",	INT_CONST, FALSE, FALSE,
							0, EINDEX_MAX },
# define DIRECTORY	8
				{ "directory",		STRING_CONST },
# define DRIVER_OBJECT	9
				{ "driver_object",	STRING_CONST, TRUE },
# define DUMP_FILE	10
				{ "dump_file",		STRING_CONST },
# define DUMP_INTERVAL	11
				{ "dump_interval",	INT_CONST },
# define DYNAMIC_CHUNK	12
				{ "dynamic_chunk",	INT_CONST, FALSE, FALSE,
							1024 },
# define ED_TMPFILE	13
				{ "ed_tmpfile",		STRING_CONST },
# define EDITORS	14
				{ "editors",		INT_CONST, FALSE, FALSE,
							0, EINDEX_MAX },
# define HOTBOOT	15
				{ "hotboot",		'(' },
# define INCLUDE_DIRS	16
				{ "include_dirs",	'(' },
# define INCLUDE_FILE	17
				{ "include_file",	STRING_CONST, TRUE },
# define MODULES	18
				{ "modules",		']' },
# define OBJECTS	19
				{ "objects",		INT_CONST, FALSE, FALSE,
							2, UINDEX_MAX },
# define SECTOR_SIZE	20
				{ "sector_size",	INT_CONST, FALSE, FALSE,
							512, 65535 },
# define STATIC_CHUNK	21
				{ "static_chunk",	INT_CONST },
# define SWAP_FILE	22
				{ "swap_file",		STRING_CONST },
# define SWAP_FRAGMENT	23
				{ "swap_fragment",	INT_CONST, FALSE, FALSE,
							0, SW_UNUSED },
# define SWAP_SIZE	24
				{ "swap_size",		INT_CONST, FALSE, FALSE,
							1024, SW_UNUSED },
# define TELNET_PORT	25
				{ "telnet_port",	'[', FALSE, FALSE,
							1, USHRT_MAX },
# define TYPECHECKING	26
				{ "typechecking",	INT_CONST, FALSE, FALSE,
							0, 2 },
# define USERS		27
				{ "users",		INT_CONST, FALSE, FALSE,
							0, EINDEX_MAX },
# define NR_OPTIONS	28
};


struct alignc { char fill; char c;	};
struct aligns { char fill; short s;	};
struct aligni { char fill; Int i;	};
struct alignl { char fill; Uuint l;	};
struct alignp { char fill; char *p;	};
struct alignz { char c;			};

# define FORMAT_VERSION	17

# define DUMP_TYPE	4	/* first XX bytes, dump type */
# define DUMP_HEADERSZ	28	/* header size */

# define FLAGS_PARTIAL	0x01	/* partial snapshot */
# define FLAGS_COMP159	0x02	/* all programs compiled by 1.5.9+ */
# define FLAGS_HOTBOOT	0x04	/* hotboot snapshot */

static SnapshotInfo header;	/* snapshot header */
static int ialign;		/* align(Int) */
static int lalign;		/* align(Uuint) */
static int ualign;		/* align(uindex) */
static int talign;		/* align(ssizet) */
static int dalign;		/* align(sector) */
static int ealign;		/* align(eindex) */
static SnapshotInfo rheader;	/* restored header */
static int rusize;		/* sizeof(uindex) */
static int rtsize;		/* sizeof(ssizet) */
static int rdsize;		/* sizeof(sector) */
static int resize;		/* sizeof(eindex) */
static int rialign;		/* align(Int) */
static int rlalign;		/* align(Uuint) */
static int rualign;		/* align(uindex) */
static int rtalign;		/* align(ssizet) */
static int rdalign;		/* align(sector) */
static int realign;		/* align(eindex) */
static Uint starttime;		/* start time */
static Uint elapsed;		/* elapsed time */
static Uint boottime;		/* boot time */

/*
 * restore a snapshot header
 */
unsigned int SnapshotInfo::restore(int fd)
{
    unsigned int size;
    off_t posn;

    for (;;) {
	if (P_read(fd, (char *) this, sizeof(SnapshotInfo)) !=
							sizeof(SnapshotInfo) ||
		   valid != 1 || version < 14 || version > FORMAT_VERSION) {
	    error("Bad or incompatible restore file header");
	}
	size = (UCHAR(secsize[0]) << 8) | UCHAR(secsize[1]);
	posn = (UCHAR(offset[0]) << 24) |
	       (UCHAR(offset[1]) << 16) |
	       (UCHAR(offset[2]) << 8) |
		UCHAR(offset[3]);
	if (posn == 0) {
	    P_lseek(fd, size - sizeof(SnapshotInfo), SEEK_CUR);
	    return size;
	}

	P_lseek(fd, posn * size, SEEK_SET);
    }
}

/*
 * initialize snapshot information
 */
void Config::dumpinit()
{
    short s;
    Int i;
    Uuint l;
    alignc cdummy;
    aligns sdummy;
    aligni idummy;
    alignl ldummy;
    alignp pdummy;

    header.valid = TRUE;			/* valid dump flag */
    header.version = FORMAT_VERSION;	/* snapshot version number */
    header.model = 0;			/* vanilla DGD */
    header.typecheck = conf[TYPECHECKING].num;
    header.secsize[0] = conf[SECTOR_SIZE].num >> 8;
    header.secsize[1] = conf[SECTOR_SIZE].num;
    strcpy(header.vstr, VERSION);

    starttime = boottime = P_time();

    s = 0x1234;
    i = 0x12345678L;
    l = 0x1234567890abcdefLL;
    header.s[0] = strchr((char *) &s, 0x12) - (char *) &s;
    header.s[1] = strchr((char *) &s, 0x34) - (char *) &s;
    header.i[0] = strchr((char *) &i, 0x12) - (char *) &i;
    header.i[1] = strchr((char *) &i, 0x34) - (char *) &i;
    header.i[2] = strchr((char *) &i, 0x56) - (char *) &i;
    header.i[3] = strchr((char *) &i, 0x78) - (char *) &i;
    header.l[0] = strchr((char *) &l, 0x12) - (char *) &l;
    header.l[1] = strchr((char *) &l, 0x34) - (char *) &l;
    header.l[2] = strchr((char *) &l, 0x56) - (char *) &l;
    header.l[3] = strchr((char *) &l, 0x78) - (char *) &l;
    header.l[4] = strchr((char *) &l, 0x90) - (char *) &l;
    header.l[5] = strchr((char *) &l, 0xab) - (char *) &l;
    header.l[6] = strchr((char *) &l, 0xcd) - (char *) &l;
    header.l[7] = strchr((char *) &l, 0xef) - (char *) &l;
    header.utsize = sizeof(uindex) | (sizeof(ssizet) << 4);
    header.desize = sizeof(Sector) | (sizeof(eindex) << 4);
    header.psize = sizeof(char*) | (sizeof(char) << 4);
    header.calign = (char *) &cdummy.c - (char *) &cdummy.fill;
    header.salign = (char *) &sdummy.s - (char *) &sdummy.fill;
    ialign = (char *) &idummy.i - (char *) &idummy.fill;
    lalign = (char *) &ldummy.l - (char *) &ldummy.fill;
    header.ilalign = ialign | (lalign << 4);
    header.palign = (char *) &pdummy.p - (char *) &pdummy.fill;
    header.zalign = sizeof(alignz);
    header.zero1 = header.zero2 = 0;

    ualign = (sizeof(uindex) == sizeof(short)) ? header.salign : ialign;
    talign = (sizeof(ssizet) == sizeof(short)) ? header.salign : ialign;
    dalign = (sizeof(Sector) == sizeof(short)) ? header.salign : ialign;
    switch (sizeof(eindex)) {
    case sizeof(char):	ealign = header.calign; break;
    case sizeof(short):	ealign = header.salign; break;
    case sizeof(Int):	ealign = ialign; break;
    }
}

/*
 * dump system state on file
 */
void Config::dump(bool incr, bool boot)
{
    int fd;
    Uint etime;

    header.version = FORMAT_VERSION;
    header.typecheck = conf[TYPECHECKING].num;
    header.start[0] = starttime >> 24;
    header.start[1] = starttime >> 16;
    header.start[2] = starttime >> 8;
    header.start[3] = starttime;
    etime = P_time();
    if (etime < boottime) {
	etime = boottime;
    }
    etime += elapsed - boottime;
    header.elapsed[0] = etime >> 24;
    header.elapsed[1] = etime >> 16;
    header.elapsed[2] = etime >> 8;
    header.elapsed[3] = etime;

    if (!incr) {
	Object::copy(0);
    }
    Dataspace::swapout(1);
    header.dflags = 0;
    if (Object::dobjects() > 0) {
	header.dflags |= FLAGS_PARTIAL;
    }
    fd = Swap::save(conf[DUMP_FILE].str, header.dflags & FLAGS_PARTIAL);
    if (!KFun::dump(fd)) {
	fatal("failed to dump kfun table");
    }
    if (!Object::save(fd, incr)) {
	fatal("failed to dump object table");
    }
    if (!CallOut::save(fd)) {
	fatal("failed to dump callout table");
    }
    if (boot) {
	boot = Comm::save(fd);
	if (boot) {
	    header.dflags |= FLAGS_HOTBOOT;
	}
    }

    Swap::save2(&header, sizeof(SnapshotInfo), incr);
}

/*
 * restore system state from file
 */
bool Config::restore(int fd, int fd2)
{
    bool conv_14, conv_15, conv_16;
    unsigned int secsize;

    secsize = rheader.restore(fd);
    conv_14 = conv_15 = conv_16 = FALSE;
    if (rheader.version < 14) {
	error("Incompatible snapshot version");
    }
    if (rheader.version < 15) {
	if (!(rheader.dflags & FLAGS_COMP159)) {
	    error("Snapshot contains legacy programs");
	}
	conv_14 = TRUE;
    }
    if (rheader.version < 16) {
	conv_15 = TRUE;
    }
    if (rheader.version < 17) {
	conv_16 = TRUE;
    }
    header.version = rheader.version;
    if (memcmp(&header, &rheader, DUMP_TYPE) != 0 || rheader.zero1 != 0 ||
	rheader.zero2 != 0 || rheader.zero3 != 0 || rheader.zero4 != 0 ||
	rheader.zero5 != 0) {
	error("Bad or incompatible snapshot header");
    }
    if (rheader.dflags & FLAGS_PARTIAL) {
	SnapshotInfo h;

	/* secondary snapshot required */
	if (fd2 < 0) {
	    error("Missing secondary snapshot");
	}
	h.restore(fd2);
	if (memcmp(&rheader, &h, DUMP_HEADERSZ) != 0) {
	    error("Secondary snapshot has different type");
	}
	Swap::restore2(fd2);
    }

    starttime = (UCHAR(rheader.start[0]) << 24) |
		(UCHAR(rheader.start[1]) << 16) |
		(UCHAR(rheader.start[2]) << 8) |
		 UCHAR(rheader.start[3]);
    elapsed =  (UCHAR(rheader.elapsed[0]) << 24) |
	       (UCHAR(rheader.elapsed[1]) << 16) |
	       (UCHAR(rheader.elapsed[2]) << 8) |
		UCHAR(rheader.elapsed[3]);
    rusize = rheader.utsize & 0xf;
    rtsize = rheader.utsize >> 4;
    if (rtsize == 0) {
	rtsize = sizeof(unsigned short);	/* backward compat */
    }
    rdsize = rheader.desize & 0xf;
    resize = rheader.desize >> 4;
    if (resize == 0) {
	resize = sizeof(char);			/* backward compat */
    }
    if ((rheader.calign >> 4) != 0) {
	error("Cannot restore arrsize > 2");
    }
    if ((rheader.salign >> 4) != 0) {
	error("Cannot restore Int size > 4");
    }
    rialign = rheader.ilalign & 0xf;
    rlalign = UCHAR(rheader.ilalign) >> 4;
    rualign = (rusize == sizeof(short)) ? rheader.salign : rialign;
    rtalign = (rtsize == sizeof(short)) ? rheader.salign : rialign;
    rdalign = (rdsize == sizeof(short)) ? rheader.salign : rialign;
    switch (resize) {
    case sizeof(char):	realign = rheader.calign; break;
    case sizeof(short):	realign = rheader.salign; break;
    case sizeof(Int):	realign = rialign; break;
    }
    if (sizeof(uindex) < rusize || sizeof(ssizet) < rtsize ||
	sizeof(Sector) < rdsize) {
	error("Cannot restore uindex, ssizet or sector of greater width");
    }
    if ((rheader.psize >> 4) > 1) {
	error("Cannot restore hindex > 1");	/* Hydra only */
    }
    rheader.psize &= 0xf;

    Swap::restore(fd, secsize);
    KFun::restore(fd);
    Object::restore(fd, rheader.dflags & FLAGS_PARTIAL);
    Dataspace::initConv(conv_14, conv_16);
    Control::initConv(conv_14, conv_15, conv_16);
    if (conv_14) {
	struct {
	    uindex nprecomps;
	    Uint ninherits;
	    Uint imapsz;
	    Uint nstrings;
	    Uint stringsz;
	    Uint nfuncdefs;
	    Uint nvardefs;
	    Uint nfuncalls;
	} dh;

	dread(fd, (char *) &dh, "uiiiiiii", (Uint) 1);
	if (dh.nprecomps != 0) {
	    fatal("precompiled objects in snapshot");
	}
    }
    boottime = P_time();
    CallOut::restore(fd, boottime, conv_16);

    if (fd2 >= 0) {
	P_close(fd2);
    }

    return ((rheader.dflags & FLAGS_HOTBOOT) && Comm::restore(fd));
}

/*
 * compute the size and alignment of a struct
 * 0x000000ff size in snapshot
 * 0x0000ff00 alignment in snapshot
 * 0x00ff0000 size
 * 0xff000000 alignment
 */
Uint Config::dsize(const char *layout)
{
    const char *p;
    Uint sz, rsz, al, ral;
    Uint size, rsize, align, ralign;
    Uint msize, rmsize, malign, rmalign;

    p = layout;
    size = rsize = msize = rmsize = 0;
    align = ralign = malign = rmalign = 1;
    sz = rsz = al = ral = 0;

    for (;;) {
	switch (*p++) {
	case 'c':	/* character */
	    sz = rsz = sizeof(char);
	    al = header.calign;
	    ral = rheader.calign;
	    break;

	case 's':	/* short */
	    sz = rsz = sizeof(short);
	    al = header.salign;
	    ral = rheader.salign;
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

	case 'l':	/* Uuint */
	    sz = rsz = sizeof(Uuint);
	    al = lalign;
	    ral = rlalign;
	    break;

	case 't':	/* ssizet */
	    sz = sizeof(ssizet);
	    rsz = rtsize;
	    al = talign;
	    ral = rtalign;
	    break;

	case 'd':	/* sector */
	    sz = sizeof(Sector);
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
	    rsz = rheader.psize;
	    al = header.palign;
	    ral = rheader.palign;
	    break;

	case 'x':	/* hte */
	    size = ALGN(size, header.zalign);
	    size = ALGN(size, header.palign);
	    size += sizeof(char*);
	    size = ALGN(size, header.palign);
	    size += sizeof(char*);
	    size = ALGN(size, header.zalign);
	    rsize = ALGN(rsize, rheader.zalign);
	    rsize = ALGN(rsize, rheader.palign);
	    rsize += rheader.psize;
	    rsize = ALGN(rsize, rheader.palign);
	    rsize += rheader.psize;
	    rsize = ALGN(rsize, rheader.zalign);
	    align = ALGN(align, header.palign);
	    ralign = ALGN(ralign, rheader.palign);
	    continue;

	case '[':	/* struct */
	    rsz = dsize(p);
	    ral = (rsz >> 8) & 0xff;
	    sz = (rsz >> 16) & 0xff;
	    al = rsz >> 24;
	    rsz &= 0xff;
	    p = strchr(p, ']') + 1;
	    break;

	case '|':	/* union */
	    msize = size;
	    rmsize = rsize;
	    malign = ALGN(align, header.zalign);
	    rmalign = ALGN(ralign, rheader.zalign);
	    size = rsize = 0;
	    align = ralign = 1;
	    continue;

	case ']':
	case '\0':	/* end of layout */
	    if (p != layout + 2) {
		/* a stuct and not an array element */
		align = ALGN(align, header.zalign);
		ralign = ALGN(ralign, rheader.zalign);
	    }
	    if (size < msize) {
		size = msize;
	    }
	    if (rsize < rmsize) {
		rsize = rmsize;
	    }
	    if (align < malign) {
		align = malign;
	    }
	    if (ralign < rmalign) {
		ralign = rmalign;
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
 * convert structs from snapshot format
 */
Uint Config::dconv(char *buf, char *rbuf, const char *layout, Uint n)
{
    Uint i, ri, j, size, rsize;
    const char *p;

    rsize = Config::dsize(layout);
    size = (rsize >> 16) & 0xff;
    rsize &= 0xff;
    while (n > 0) {
	i = ri = 0;
	for (p = layout; *p != '\0' && *p != '|' && *p != ']'; p++) {
	    switch (*p) {
	    case 'c':
		i = ALGN(i, header.calign);
		ri = ALGN(ri, rheader.calign);
		buf[i] = rbuf[ri];
		i += sizeof(char);
		ri += sizeof(char);
		break;

	    case 's':
		i = ALGN(i, header.salign);
		ri = ALGN(ri, rheader.salign);
		buf[i + header.s[0]] = rbuf[ri + rheader.s[0]];
		buf[i + header.s[1]] = rbuf[ri + rheader.s[1]];
		i += sizeof(short);
		ri += sizeof(short);
		break;

	    case 'u':
		i = ALGN(i, ualign);
		ri = ALGN(ri, rualign);
		if (sizeof(uindex) == rusize) {
		    if (sizeof(uindex) == sizeof(short)) {
			buf[i + header.s[0]] = rbuf[ri + rheader.s[0]];
			buf[i + header.s[1]] = rbuf[ri + rheader.s[1]];
		    } else {
			buf[i + header.i[0]] = rbuf[ri + rheader.i[0]];
			buf[i + header.i[1]] = rbuf[ri + rheader.i[1]];
			buf[i + header.i[2]] = rbuf[ri + rheader.i[2]];
			buf[i + header.i[3]] = rbuf[ri + rheader.i[3]];
		    }
		} else {
		    j = (UCHAR(rbuf[ri + rheader.s[0]] & rbuf[ri + rheader.s[1]]) == 0xff) ?
			 -1 : 0;
		    buf[i + header.i[0]] = j;
		    buf[i + header.i[1]] = j;
		    buf[i + header.i[2]] = rbuf[ri + rheader.s[0]];
		    buf[i + header.i[3]] = rbuf[ri + rheader.s[1]];
		}
		i += sizeof(uindex);
		ri += rusize;
		break;

	    case 'i':
		i = ALGN(i, ialign);
		ri = ALGN(ri, rialign);
		buf[i + header.i[0]] = rbuf[ri + rheader.i[0]];
		buf[i + header.i[1]] = rbuf[ri + rheader.i[1]];
		buf[i + header.i[2]] = rbuf[ri + rheader.i[2]];
		buf[i + header.i[3]] = rbuf[ri + rheader.i[3]];
		i += sizeof(Int);
		ri += sizeof(Int);
		break;

	    case 'l':
		i = ALGN(i, lalign);
		ri = ALGN(ri, rlalign);
		buf[i + header.l[0]] = rbuf[ri + rheader.l[0]];
		buf[i + header.l[1]] = rbuf[ri + rheader.l[1]];
		buf[i + header.l[2]] = rbuf[ri + rheader.l[2]];
		buf[i + header.l[3]] = rbuf[ri + rheader.l[3]];
		buf[i + header.l[4]] = rbuf[ri + rheader.l[4]];
		buf[i + header.l[5]] = rbuf[ri + rheader.l[5]];
		buf[i + header.l[6]] = rbuf[ri + rheader.l[6]];
		buf[i + header.l[7]] = rbuf[ri + rheader.l[7]];
		i += sizeof(Uuint);
		ri += sizeof(Uuint);
		break;

	    case 't':
		i = ALGN(i, talign);
		ri = ALGN(ri, rtalign);
		if (sizeof(ssizet) == rtsize) {
		    if (sizeof(ssizet) == sizeof(short)) {
			buf[i + header.s[0]] = rbuf[ri + rheader.s[0]];
			buf[i + header.s[1]] = rbuf[ri + rheader.s[1]];
		    } else {
			buf[i + header.i[0]] = rbuf[ri + rheader.i[0]];
			buf[i + header.i[1]] = rbuf[ri + rheader.i[1]];
			buf[i + header.i[2]] = rbuf[ri + rheader.i[2]];
			buf[i + header.i[3]] = rbuf[ri + rheader.i[3]];
		    }
		} else {
		    buf[i + header.i[0]] = 0;
		    buf[i + header.i[1]] = 0;
		    buf[i + header.i[2]] = rbuf[ri + rheader.s[0]];
		    buf[i + header.i[3]] = rbuf[ri + rheader.s[1]];
		}
		i += sizeof(ssizet);
		ri += rtsize;
		break;

	    case 'd':
		i = ALGN(i, dalign);
		ri = ALGN(ri, rdalign);
		if (sizeof(Sector) == rdsize) {
		    if (sizeof(Sector) == sizeof(short)) {
			buf[i + header.s[0]] = rbuf[ri + rheader.s[0]];
			buf[i + header.s[1]] = rbuf[ri + rheader.s[1]];
		    } else {
			buf[i + header.i[0]] = rbuf[ri + rheader.i[0]];
			buf[i + header.i[1]] = rbuf[ri + rheader.i[1]];
			buf[i + header.i[2]] = rbuf[ri + rheader.i[2]];
			buf[i + header.i[3]] = rbuf[ri + rheader.i[3]];
		    }
		} else {
		    j = (UCHAR(rbuf[ri + rheader.s[0]] & rbuf[ri + rheader.s[1]]) == 0xff) ?
			 -1 : 0;
		    buf[i + header.i[0]] = j;
		    buf[i + header.i[1]] = j;
		    buf[i + header.i[2]] = rbuf[ri + rheader.s[0]];
		    buf[i + header.i[3]] = rbuf[ri + rheader.s[1]];
		}
		i += sizeof(Sector);
		ri += rdsize;
		break;

	    case 'e':
		i = ALGN(i, ealign);
		ri = ALGN(ri, realign);
		i += sizeof(eindex);
		ri += resize;
		break;

	    case 'p':
		i = ALGN(i, header.palign);
		ri = ALGN(ri, rheader.palign);
		for (j = sizeof(char*); j > 0; --j) {
		    buf[i++] = 0;
		}
		ri += rheader.psize;
		break;

	    case '[':
		j = Config::dsize(++p);
		i = ALGN(i, j >> 24);
		ri = ALGN(ri, (j >> 8) & 0xff);
		j = Config::dconv(buf + i, rbuf + ri, p, (Uint) 1);
		i += (j >> 16) & 0xff;
		ri += j & 0xff;
		p = strchr(p, ']');
		break;

	    case 'x':
		i = ALGN(i, header.zalign);
		i = ALGN(i, header.palign);
		for (j = sizeof(char*); j > 0; --j) {
		    buf[i++] = 0;
		}
		i = ALGN(i, header.palign);
		for (j = sizeof(char*); j > 0; --j) {
		    buf[i++] = 0;
		}
		ri = ALGN(ri, rheader.zalign);
		ri = ALGN(ri, rheader.palign);
		ri += rheader.psize;
		ri = ALGN(ri, rheader.palign);
		for (j = rheader.psize; j > 0; --j) {
		    if (rbuf[ri] != 0) {
			buf[i - 1] = 1;
			break;
		    }
		    ri++;
		}
		ri += j;
		i = ALGN(i, header.zalign);
		ri = ALGN(ri, rheader.zalign);
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
 * read from snapshot
 */
void Config::dread(int fd, char *buf, const char *layout, Uint n)
{
    char buffer[16384];
    unsigned int i, size, rsize;
    Uint tmp;

    tmp = Config::dsize(layout);
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
	Config::dconv(buf, buffer, layout, (Uint) i);
	buf += size * i;
	n -= i;
    }
}


# define MAX_PORTS	32
# define MAX_STRINGS	32

static char *hotboot[MAX_STRINGS], *dirs[MAX_STRINGS];
static char *modules[MAX_STRINGS + 1], *modconf[MAX_STRINGS + 1];
static void (*mfdlist[MAX_STRINGS + 1])(int*, int);
static void (*mfinish[MAX_STRINGS + 1])();
static char *bhosts[MAX_PORTS], *dhosts[MAX_PORTS], *thosts[MAX_PORTS];
static unsigned short bports[MAX_PORTS], dports[MAX_PORTS], tports[MAX_PORTS];
static bool attached[MAX_PORTS];
static int ntports, nbports, ndports;

/*
 * error during the configuration phase
 */
void Config::err(const char *err)
{
    message("Config error, line %u: %s\012", TokenBuf::line(), err);	/* LF */
}

/*
 * read config file
 */
bool Config::config()
{
    char buf[STRINGSZ];
    char *p;
    int h, l, m, c;
    char **strs;
    unsigned short *ports;

    for (h = NR_OPTIONS; h > 0; ) {
	conf[--h].num = 0;
	conf[h].set = FALSE;
    }
    memset(dirs, '\0', sizeof(dirs));
    strs = (char **) NULL;

    while ((c=PP::gettok()) != EOF) {
	if (c != IDENTIFIER) {
	    err("option expected");
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
		err("unknown option");
		return FALSE;
	    }
	}

	if (PP::gettok() != '=') {
	    err("'=' expected");
	    return FALSE;
	}

	if ((c=PP::gettok()) != conf[m].type) {
	    if (c != INT_CONST && c != STRING_CONST && c != '(') {
		err("syntax error");
		return FALSE;
	    } else if (conf[m].type != '[' || c != INT_CONST) {
		if ((conf[m].type == '[' || conf[m].type == ']') && c == '(') {
		    c = conf[m].type;
		} else {
		    err("bad value type");
		    return FALSE;
		}
	    }
	}

	switch (c) {
	case INT_CONST:
	    if (yylval.number < conf[m].low ||
		(conf[m].high != 0 && yylval.number > conf[m].high)) {
		err("int value out of range");
		return FALSE;
	    }
	    switch (m) {
	    case BINARY_PORT:
		bhosts[0] = (char *) NULL;
		bports[0] = yylval.number;
		nbports = 1;
		break;

	    case DATAGRAM_PORT:
		dhosts[0] = (char *) NULL;
		dports[0] = yylval.number;
		ndports = 1;
		break;

	    case TELNET_PORT:
		thosts[0] = (char *) NULL;
		tports[0] = yylval.number;
		ntports = 1;
		break;

	    default:
		conf[m].num = yylval.number;
		break;
	    }
	    break;

	case STRING_CONST:
	    p = (conf[m].resolv) ? Path::resolve(buf, yytext) : yytext;
	    l = strlen(p);
	    if (l >= STRINGSZ) {
		l = STRINGSZ - 1;
		p[l] = '\0';
	    }
	    Alloc::staticMode();
	    conf[m].str = strcpy(ALLOC(char, l + 1), p);
	    Alloc::dynamicMode();
	    break;

	case '(':
	    if (PP::gettok() != '{') {
		err("'{' expected");
		return FALSE;
	    }
	    l = 0;
	    switch (m) {
	    case HOTBOOT:	strs = hotboot; break;
	    case INCLUDE_DIRS:	strs = dirs; break;
	    }
	    for (;;) {
		if (PP::gettok() != STRING_CONST) {
		    err("string expected");
		    return FALSE;
		}
		if (l == MAX_STRINGS - 1) {
		    err("array too large");
		    return FALSE;
		}
		Alloc::staticMode();
		strs[l] = strcpy(ALLOC(char, strlen(yytext) + 1), yytext);
		l++;
		Alloc::dynamicMode();
		if ((c=PP::gettok()) == '}') {
		    break;
		}
		if (c != ',') {
		    err("',' expected");
		    return FALSE;
		}
	    }
	    if (PP::gettok() != ')') {
		err("')' expected");
		return FALSE;
	    }
	    strs[l] = (char *) NULL;
	    break;

	case '[':
	    if (PP::gettok() != '[') {
		err("'[' expected");
		return FALSE;
	    }
	    l = 0;
	    if ((c=PP::gettok()) != ']') {
		switch (m) {
		case BINARY_PORT:
		    strs = bhosts;
		    ports = bports;
		    break;

		case DATAGRAM_PORT:
		    strs = dhosts;
		    ports = dports;
		    break;

		case TELNET_PORT:
		    strs = thosts;
		    ports = tports;
		    break;
		}
		for (;;) {
		    if (l == MAX_PORTS) {
			err("too many ports");
			return FALSE;
		    }
		    if (c != STRING_CONST) {
			err("string expected");
			return FALSE;
		    }
		    if (strcmp(yytext, "*") == 0) {
			strs[l] = (char *) NULL;
		    } else {
			Alloc::staticMode();
			strs[l] = strcpy(ALLOC(char, strlen(yytext) + 1),
					 yytext);
			Alloc::dynamicMode();
		    }
		    if (PP::gettok() != ':') {
			err("':' expected");
			return FALSE;
		    }
		    if (PP::gettok() != INT_CONST) {
			err("integer expected");
			return FALSE;
		    }
		    if (yylval.number <= 0 || yylval.number > USHRT_MAX) {
			err("int value out of range");
			return FALSE;
		    }
		    ports[l++] = yylval.number;
		    if ((c=PP::gettok()) == ']') {
			break;
		    }
		    if (c != ',') {
			err("',' expected");
			return FALSE;
		    }
		    c = PP::gettok();
		}
	    }
	    if (PP::gettok() != ')') {
		err("')' expected");
		return FALSE;
	    }
	    switch (m) {
	    case TELNET_PORT:
		ntports = l;
		break;

	    case BINARY_PORT:
		nbports = l;
		break;

	    case DATAGRAM_PORT:
		ndports = l;
		break;
	    }
	    break;

	case ']':
	    if (PP::gettok() != '[') {
		err("'[' expected");
		return FALSE;
	    }
	    l = 0;
	    if ((c=PP::gettok()) != ']') {
		for (;;) {
		    if (l == MAX_STRINGS - 1) {
			err("mapping too large");
			return FALSE;
		    }
		    if (c != STRING_CONST) {
			err("string expected");
			return FALSE;
		    }
		    Alloc::staticMode();
		    modules[l] = strcpy(ALLOC(char, strlen(yytext) + 1),
					yytext);
		    Alloc::dynamicMode();
		    if (PP::gettok() != ':') {
			err("':' expected");
			return FALSE;
		    }
		    if (PP::gettok() != STRING_CONST) {
			err("string expected");
			return FALSE;
		    }
		    Alloc::staticMode();
		    modconf[l++] = strcpy(ALLOC(char, strlen(yytext) + 1),
					  yytext);
		    Alloc::dynamicMode();
		    if ((c=PP::gettok()) == ']') {
			break;
		    }
		    if (c != ',') {
			err("',' expected");
			return FALSE;
		    }
		    c = PP::gettok();
		}
	    }
	    if (PP::gettok() != ')') {
		err("')' expected");
		return FALSE;
	    }
	    modules[l] = modconf[l] = (char *) NULL;
	    break;
	}
	conf[m].set = TRUE;
	if (PP::gettok() != ';') {
	    err("';' expected");
	    return FALSE;
	}
    }

    for (l = 0; l < NR_OPTIONS; l++) {
	if (!conf[l].set && l != HOTBOOT && l != MODULES && l != CACHE_SIZE &&
	    l != DATAGRAM_PORT && l != DATAGRAM_USERS) {
	    char buffer[64];

	    sprintf(buffer, "unspecified option %s", conf[l].name);
	    err(buffer);
	    return FALSE;
	}
    }

    if (conf[USERS].num + conf[DATAGRAM_USERS].num == 0) {
	err("no users");
	return FALSE;
    }
    if (conf[USERS].num + conf[DATAGRAM_USERS].num > EINDEX_MAX) {
	err("total number of users too high");
	return FALSE;
    }

    h = (nbports < ndports) ? nbports : ndports;
    for (l = 0; l < h; l++) {
	attached[l] = (bports[l] == dports[l]);
    }
    while (l < ndports) {
	attached[l++] = FALSE;
    }

    return TRUE;
}

/*
 * pass fdlist on to loaded modules
 */
void Config::fdlist()
{
    int i, size, *list;

    size = Connection::fdcount();
    if (size != 0) {
	list = ALLOCA(int, size);
	Connection::fdlist(list);
    }

    for (i = 0; modules[i] != NULL; i++) {
	if (mfdlist[i] != NULL) {
	    (*mfdlist[i])(list, size);
	}
    }

    if (size != 0) {
	AFREE(list);
    }
}

/*
 * call finish for loaded modules
 */
void Config::modFinish()
{
    int i;

    for (i = 0; modules[i] != NULL; i++) {
	if (mfinish[i] != NULL) {
	    (*mfinish[i])();
	}
    }
}

static int fd;			/* file descriptor */
static char *obuf;		/* output buffer */
static unsigned int bufsz;	/* buffer size */

/*
 * create a new file
 */
bool Config::open(char *file)
{
    char fname[STRINGSZ];

    Path::resolve(fname, file);
    if ((fd=P_open(fname, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644)) < 0) {
	message("Config error: cannot create \"/%s\"\012", fname);	/* LF */
	return FALSE;
    }
    bufsz = 0;

    return TRUE;
}

/*
 * write a string to a file
 */
void Config::puts(const char *str)
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
 * close a file
 */
bool Config::close()
{
    if (bufsz > 0 && P_write(fd, obuf, bufsz) != bufsz) {
	message("Config error: cannot write include file\012");		/* LF */
	P_close(fd);
	return FALSE;
    }
    P_close(fd);

    return TRUE;
}

/*
 * create include files
 */
bool Config::includes()
{
    char buf[BUF_SIZE], buffer[STRINGSZ];
    int i;
    char *p;

    /* create status.h file */
    obuf = buf;
    sprintf(buffer, "%s/status.h", dirs[0]);
    if (!open(buffer)) {
	return FALSE;
    }
    puts("/*\012 * This file defines the fields of the array returned ");
    puts("by the\012 * status() kfun.  It is automatically generated ");
    puts("by DGD on startup.\012 */\012\012");
    puts("# define ST_VERSION\t0\t/* driver version */\012");
    puts("# define ST_STARTTIME\t1\t/* system start time */\012");
    puts("# define ST_BOOTTIME\t2\t/* system reboot time */\012");
    puts("# define ST_UPTIME\t3\t/* system virtual uptime */\012");
    puts("# define ST_SWAPSIZE\t4\t/* # sectors on swap device */\012");
    puts("# define ST_SWAPUSED\t5\t/* # sectors in use */\012");
    puts("# define ST_SECTORSIZE\t6\t/* size of swap sector */\012");
    puts("# define ST_SWAPRATE1\t7\t/* # objects swapped out last minute */\012");
    puts("# define ST_SWAPRATE5\t8\t/* # objects swapped out last five minutes */\012");
    puts("# define ST_SMEMSIZE\t9\t/* static memory allocated */\012");
    puts("# define ST_SMEMUSED\t10\t/* static memory in use */\012");
    puts("# define ST_DMEMSIZE\t11\t/* dynamic memory allocated */\012");
    puts("# define ST_DMEMUSED\t12\t/* dynamic memory in use */\012");
    puts("# define ST_OTABSIZE\t13\t/* object table size */\012");
    puts("# define ST_NOBJECTS\t14\t/* # objects in use */\012");
    puts("# define ST_COTABSIZE\t15\t/* callout table size */\012");
    puts("# define ST_NCOSHORT\t16\t/* # short-term callouts */\012");
    puts("# define ST_NCOLONG\t17\t/* # long-term & millisecond callouts */\012");
    puts("# define ST_UTABSIZE\t18\t/* user table size */\012");
    puts("# define ST_ETABSIZE\t19\t/* editor table size */\012");
    puts("# define ST_STRSIZE\t20\t/* max string size */\012");
    puts("# define ST_ARRAYSIZE\t21\t/* max array/mapping size */\012");
    puts("# define ST_STACKDEPTH\t22\t/* remaining stack depth */\012");
    puts("# define ST_TICKS\t23\t/* remaining ticks */\012");
    puts("# define ST_DATAGRAMPORTS 24\t/* datagram ports */\012");
    puts("# define ST_TELNETPORTS\t25\t/* telnet ports */\012");
    puts("# define ST_BINARYPORTS\t26\t/* binary ports */\012");

    puts("\012# define O_COMPILETIME\t0\t/* time of compilation */\012");
    puts("# define O_PROGSIZE\t1\t/* program size of object */\012");
    puts("# define O_DATASIZE\t2\t/* # variables in object */\012");
    puts("# define O_NSECTORS\t3\t/* # sectors used by object */\012");
    puts("# define O_CALLOUTS\t4\t/* callouts in object */\012");
    puts("# define O_INDEX\t5\t/* unique ID for master object */\012");
    puts("# define O_UNDEFINED\t6\t/* undefined functions */\012");

    puts("\012# define CO_HANDLE\t0\t/* callout handle */\012");
    puts("# define CO_FUNCTION\t1\t/* function name */\012");
    puts("# define CO_DELAY\t2\t/* delay */\012");
    puts("# define CO_FIRSTXARG\t3\t/* first extra argument */\012");
    if (!close()) {
	return FALSE;
    }

    /* create type.h file */
    sprintf(buffer, "%s/type.h", dirs[0]);
    if (!open(buffer)) {
	return FALSE;
    }
    puts("/*\012 * This file gives definitions for the value returned ");
    puts("by the\012 * typeof() kfun.  It is automatically generated ");
    puts("by DGD on startup.\012 */\012\012");
    sprintf(buffer, "# define T_NIL\t\t%d\012", T_NIL);
    puts(buffer);
    sprintf(buffer, "# define T_INT\t\t%d\012", T_INT);
    puts(buffer);
    sprintf(buffer, "# define T_FLOAT\t%d\012", T_FLOAT);
    puts(buffer);
    sprintf(buffer, "# define T_STRING\t%d\012", T_STRING);
    puts(buffer);
    sprintf(buffer, "# define T_OBJECT\t%d\012", T_OBJECT);
    puts(buffer);
    sprintf(buffer, "# define T_ARRAY\t%d\012", T_ARRAY);
    puts(buffer);
    sprintf(buffer, "# define T_MAPPING\t%d\012", T_MAPPING);
    puts(buffer);
    if (!close()) {
	return FALSE;
    }

    /* create limits.h file */
    sprintf(buffer, "%s/limits.h", dirs[0]);
    if (!open(buffer)) {
	return FALSE;
    }
    puts("/*\012 * This file defines some basic sizes of datatypes and ");
    puts("resources.\012 * It is automatically generated by DGD on ");
    puts("startup.\012 */\012\012");
    puts("# define CHAR_BIT\t\t8\t\t/* # bits in character */\012");
    puts("# define CHAR_MIN\t\t0\t\t/* min character value */\012");
    puts("# define CHAR_MAX\t\t255\t\t/* max character value */\012\012");
    puts("# define INT_MIN\t\t0x80000000\t/* -2147483648 */\012");
    puts("# define INT_MAX\t\t2147483647\t/* max integer value */\012");
    if (!close()) {
	return FALSE;
    }

    /* create float.h file */
    sprintf(buffer, "%s/float.h", dirs[0]);
    if (!open(buffer)) {
	return FALSE;
    }
    puts("/*\012 * This file describes the floating point type. It is ");
    puts("automatically\012 * generated by DGD on startup.\012 */\012\012");
    puts("# define FLT_RADIX\t2\t\t\t/* binary */\012");
    puts("# define FLT_ROUNDS\t1\t\t\t/* round to nearest */\012");
    puts("# define FLT_EPSILON\t7.2759576142E-12\t/* smallest x: 1.0 + x != 1.0 */\012");
    puts("# define FLT_DIG\t11\t\t\t/* decimal digits of precision*/\012");
    puts("# define FLT_MANT_DIG\t37\t\t\t/* binary digits of precision */\012");
    puts("# define FLT_MIN\t2.22507385851E-308\t/* positive minimum */\012");
    puts("# define FLT_MIN_EXP\t(-1021)\t\t\t/* minimum binary exponent */\012");
    puts("# define FLT_MIN_10_EXP\t(-307)\t\t\t/* minimum decimal exponent */\012");
    puts("# define FLT_MAX\t1.79769313485E+308\t/* positive maximum */\012");
    puts("# define FLT_MAX_EXP\t1024\t\t\t/* maximum binary exponent */\012");
    puts("# define FLT_MAX_10_EXP\t308\t\t\t/* maximum decimal exponent */\012");
    if (!close()) {
	return FALSE;
    }

    /* create trace.h file */
    sprintf(buffer, "%s/trace.h", dirs[0]);
    if (!open(buffer)) {
	return FALSE;
    }
    puts("/*\012 * This file describes the fields of the array returned for ");
    puts("every stack\012 * frame by the call_trace() function.  It is ");
    puts("automatically generated by DGD\012 * on startup.\012 */\012\012");
    puts("# define TRACE_OBJNAME\t0\t/* name of the object */\012");
    puts("# define TRACE_PROGNAME\t1\t/* name of the object the function is in */\012");
    puts("# define TRACE_FUNCTION\t2\t/* function name */\012");
    puts("# define TRACE_LINE\t3\t/* line number */\012");
    puts("# define TRACE_EXTERNAL\t4\t/* external call flag */\012");
    puts("# define TRACE_FIRSTARG\t5\t/* first argument to function */\012");
    if (!close()) {
	return FALSE;
    }

    /* create kfun.h file */
    sprintf(buffer, "%s/kfun.h", dirs[0]);
    if (!open(buffer)) {
	return FALSE;
    }
    puts("/*\012 * This file defines the version of each supported kfun.  ");
    puts("It is automatically\012 * generated by DGD on ");
    puts("startup.\012 */\012\012");
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
	    puts(buffer);
	}
    }
    puts("\012\012/*\012 * Supported ciphers and hashing algorithms.\012 */");
    puts("\012\012# define ENCRYPT_CIPHERS\t");
    for (i = 0; ; ) {
	sprintf(buffer, "\"%s\"", kfenc[i].name);
	puts(buffer);
	if (++i == ne) {
	    break;
	}
	puts(", ");
    }
    puts("\012# define DECRYPT_CIPHERS\t");
    for (i = 0; ; ) {
	sprintf(buffer, "\"%s\"", kfdec[i].name);
	puts(buffer);
	if (++i == nd) {
	    break;
	}
	puts(", ");
    }
    puts("\012# define HASH_ALGORITHMS\t");
    for (i = 0; ; ) {
	sprintf(buffer, "\"%s\"", kfhsh[i].name);
	puts(buffer);
	if (++i == nh) {
	    break;
	}
	puts(", ");
    }
    puts("\012");

    return close();
}


/*
 * initialize the driver
 */
bool Config::init(char *configfile, char *snapshot, char *snapshot2,
		  char *module, Sector *fragment)
{
    char buf[STRINGSZ];
    char abs_path[STRINGSZ];
    int fd, fd2, i, j;
    bool init;
    Sector cache;

    fd = fd2 = -1;

    /*
     * process config file
     */
    if (!PP::init(path_native(buf, configfile), (char **) NULL,
		  (String **) NULL, 0, 0)) {
	message("Config error: cannot open config file\012");	/* LF */
	Alloc::finish();
	return FALSE;
    }
    init = config();
    PP::clear();
    Alloc::purge();
    if (!init) {
	Alloc::finish();
	return FALSE;
    }

    /* try to open the snapshot if one was provided */
    if (snapshot != (char *) NULL) {
	fd = P_open(path_native(buf, snapshot), O_RDONLY | O_BINARY, 0);
	if (fd < 0) {
	    P_message("Config error: cannot open restore file\012");    /* LF */
	    Alloc::finish();
	    return FALSE;
	}
    }
    if (snapshot2 != (char *) NULL) {
	fd2 = P_open(path_native(buf, snapshot2), O_RDONLY | O_BINARY, 0);
	if (fd2 < 0) {
	    P_message("Config error: cannot open secondary restore file\012");    /* LF */
	    if (snapshot != (char *) NULL) {
		P_close(fd);
	    }
	    Alloc::finish();
	    return FALSE;
	}
    }

    Alloc::staticMode();

    /* remove previously added kfuns */
    KFun::clear();

    memset(mfdlist, '\0', MAX_STRINGS * sizeof(void (*)(int*, int)));
    memset(mfinish, '\0', MAX_STRINGS * sizeof(void (*)()));
    for (i = 0; modules[i] != NULL; i++) {
	if (!Ext::load(modules[i], modconf[i], &mfdlist[i], &mfinish[i])) {
	    message("Config error: cannot load runtime extension \"%s\"\012",
		    modules[i]);
	    if (snapshot2 != (char *) NULL) {
		P_close(fd2);
	    }
	    if (snapshot != (char *) NULL) {
		P_close(fd);
	    }
	    modFinish();
	    Ext::finish();
	    Alloc::finish();
	    return FALSE;
	}
    }
    if (module != (char *) NULL &&
	!Ext::load(modules[i] = module, NULL, &mfdlist[i], &mfinish[i])) {
	message("Config error: cannot load runtime extension \"%s\"\012",/* LF*/
		module);
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	modFinish();
	Ext::finish();
	Alloc::finish();
	return FALSE;
    }

    /* initialize kfuns */
    KFun::init();

    /* if conf[DIRECTORY] is empty, set it to the directory containing
       the config file */
    if (conf[DIRECTORY].str[0] == 0) {
        if (realpath(configfile, abs_path) == NULL) {
            message("Config error: couldn't get real path for config file \"%s\"\012", /* LF */
                configfile);
            if (snapshot2 != (char *) NULL) {
                P_close(fd2);
            }
            if (snapshot != (char *) NULL) {
                P_close(fd);
            }
            modFinish();
            Ext::finish();
            Alloc::finish();
            return FALSE;
        }

        /* cut off the final filename, leave only the absolute directory */
        for(i = 0, j = 0; abs_path[i] != 0; i++) {
            if(abs_path[i] == '/') {
                j = i;
            }
        }
        if (abs_path[j] == '/') {
            abs_path[j] = 0;
        }

        Alloc::staticMode();
        Alloc::free(conf[DIRECTORY].str);
        conf[DIRECTORY].str = strcpy(ALLOC(char, strlen(abs_path) + 1), abs_path);
        Alloc::dynamicMode();
    }

    /* change directory */
    if (P_chdir(path_native(buf, conf[DIRECTORY].str)) < 0) {
	message("Config error: bad base directory \"%s\"\012",	/* LF */
		conf[DIRECTORY].str);
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	modFinish();
	Ext::finish();
	Alloc::finish();
	return FALSE;
    }

    /* initialize communications */
    if (!Comm::init((int) conf[USERS].num,
		    (int) conf[DATAGRAM_USERS].num,
		    thosts, bhosts, dhosts,
		    tports, bports, dports,
		    ntports, nbports, ndports)) {
	Comm::clear();
	Comm::finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	modFinish();
	Ext::finish();
	Alloc::finish();
	return FALSE;
    }

    /* initialize arrays */
    Array::init((int) conf[ARRAY_SIZE].num);

    /* initialize objects */
    Object::init((uindex) conf[OBJECTS].num,
		 (Uint) conf[DUMP_INTERVAL].num);

    /* initialize swap device */
    cache = (Sector) ((conf[CACHE_SIZE].set) ? conf[CACHE_SIZE].num : 100);
    Swap::init(conf[SWAP_FILE].str, (Sector) conf[SWAP_SIZE].num, cache,
	       (unsigned int) conf[SECTOR_SIZE].num);

    /* initialize swapped data handler */
    Dataspace::init();
    Control::init();
    *fragment = conf[SWAP_FRAGMENT].num;

    /* initalize editor */
    Editor::init(conf[ED_TMPFILE].str,
		 (int) conf[EDITORS].num);

    /* initialize call_outs */
    if (!CallOut::init((uindex) conf[CALL_OUTS].num)) {
	Swap::finish();
	Comm::clear();
	Comm::finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	modFinish();
	Ext::finish();
	Alloc::finish();
	return FALSE;
    }

    /* initialize interpreter */
    Frame::init(conf[CREATE].str, conf[TYPECHECKING].num == 2);

    /* initialize compiler */
    Compile::init(conf[AUTO_OBJECT].str,
		  conf[DRIVER_OBJECT].str,
		  conf[INCLUDE_FILE].str,
		  dirs,
		  (int) conf[TYPECHECKING].num);

    Alloc::dynamicMode();

    /* initialize memory manager */
    Alloc::init((size_t) conf[STATIC_CHUNK].num,
		(size_t) conf[DYNAMIC_CHUNK].num);

    /*
     * create include files
     */
    if (!includes()) {
	Swap::finish();
	Comm::clear();
	Comm::finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	modFinish();
	Ext::finish();
	Alloc::finish();
	return FALSE;
    }

    /* initialize snapshot header */
    dumpinit();

    Alloc::staticMode();		/* allocate error context statically */
    ErrorContext::push();		/* guard error context */
    try {
	ErrorContext::push();
	Alloc::dynamicMode();
	if (snapshot == (char *) NULL) {
	    /* no restored connections */
	    fdlist();

	    /* prepare JIT compiler */
	    KFun::jit();

	    /* initialize mudlib */
	    Control::converted();
	    Dataspace::converted();
	    try {
		ErrorContext::push(DGD::errHandler);
		DGD::callDriver(cframe, "initialize", 0);
		ErrorContext::pop();
	    } catch (...) {
		error((char *) NULL);
	    }
	} else {
	    bool hotbooted;

	    /* restore snapshot */
	    hotbooted = restore(fd, fd2);

	    /* inform extension modules about restored connections */
	    fdlist();

	    /* prepare JIT compiler */
	    KFun::jit();

	    /* notify mudlib */
	    try {
		ErrorContext::push(DGD::errHandler);
		if (hotbooted) {
		    PUSH_INTVAL(cframe, TRUE);
		    DGD::callDriver(cframe, "restored", 1);
		} else {
		    DGD::callDriver(cframe, "restored", 0);
		}
		ErrorContext::pop();
	    } catch (...) {
		error((char *) NULL);
	    }
	}
	ErrorContext::pop();
    } catch (...) {
	message((char *) NULL);
	DGD::endTask();
	message("Config error: initialization failed\012");	/* LF */
	ErrorContext::pop();		/* remove guard */

	Swap::finish();
	Comm::clear();
	Comm::finish();
	Editor::finish();
	if (snapshot2 != (char *) NULL) {
	    P_close(fd2);
	}
	if (snapshot != (char *) NULL) {
	    P_close(fd);
	}
	Array::freeall();
	String::clean();
	modFinish();
	Ext::finish();
	Alloc::finish();
	return FALSE;
    }
    (cframe->sp++)->del();
    DGD::endTask();
    ErrorContext::pop();		/* remove guard */

    /* start accepting connections */
    Comm::listen();
    return TRUE;
}

/*
 * return the driver base directory
 */
char *Config::baseDir()
{
    return conf[DIRECTORY].str;
}

/*
 * return the driver object name
 */
char *Config::driver()
{
    return conf[DRIVER_OBJECT].str;
}

/*
 * return the hotboot executable
 */
char **Config::hotbootExec()
{
    return (conf[HOTBOOT].set) ? hotboot : (char **) NULL;
}

/*
 * return the global typechecking flag
 */
int Config::typechecking()
{
    return conf[TYPECHECKING].num;
}

/*
 * return TRUE if datagram channel can be attached to connections on this port
 */
bool Config::attach(int port)
{
    return (port >= 0 && port < ndports && attached[port]);
}

/*
 * return the maximum array size
 */
unsigned short Config::arraySize()
{
    return conf[ARRAY_SIZE].num;
}

/*
 * store a size_t as an integer or as a float approximation
 */
void Config::putval(Value *v, size_t n)
{
    Float f1, f2;

    if (n <= 0x7fffffffL) {
	PUT_INTVAL(v, n);
    } else {
	Float::itof(n >> 31, &f1);
	f1.ldexp(31);
	Float::itof(n & 0x7fffffffL, &f2);
	f1.add(f2);
	PUT_FLTVAL(v, f1);
    }
}

/*
 * return resource usage information
 */
bool Config::statusi(Frame *f, Int idx, Value *v)
{
    const char *version;
    uindex ncoshort, ncolong;
    Array *a;
    Uint t;
    int i;

    switch (idx) {
    case 0:	/* ST_VERSION */
	version = VERSION;
	PUT_STRVAL(v, String::create(version, strlen(version)));
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
	PUT_INTVAL(v, conf[SWAP_SIZE].num);
	break;

    case 5:	/* ST_SWAPUSED */
	PUT_INTVAL(v, Swap::count());
	break;

    case 6:	/* ST_SECTORSIZE */
	PUT_INTVAL(v, conf[SECTOR_SIZE].num);
	break;

    case 7:	/* ST_SWAPRATE1 */
	PUT_INTVAL(v, CallOut::swaprate1());
	break;

    case 8:	/* ST_SWAPRATE5 */
	PUT_INTVAL(v, CallOut::swaprate5());
	break;

    case 9:	/* ST_SMEMSIZE */
	putval(v, Alloc::info()->smemsize);
	break;

    case 10:	/* ST_SMEMUSED */
	putval(v, Alloc::info()->smemused);
	break;

    case 11:	/* ST_DMEMSIZE */
	putval(v, Alloc::info()->dmemsize);
	break;

    case 12:	/* ST_DMEMUSED */
	putval(v, Alloc::info()->dmemused);
	break;

    case 13:	/* ST_OTABSIZE */
	PUT_INTVAL(v, conf[OBJECTS].num);
	break;

    case 14:	/* ST_NOBJECTS */
	PUT_INTVAL(v, Object::ocount());
	break;

    case 15:	/* ST_COTABSIZE */
	PUT_INTVAL(v, conf[CALL_OUTS].num);
	break;

    case 16:	/* ST_NCOSHORT */
	CallOut::info(&ncoshort, &ncolong);
	PUT_INTVAL(v, ncoshort);
	break;

    case 17:	/* ST_NCOLONG */
	CallOut::info(&ncoshort, &ncolong);
	PUT_INTVAL(v, ncolong);
	break;

    case 18:	/* ST_UTABSIZE */
	PUT_INTVAL(v, conf[USERS].num + conf[DATAGRAM_USERS].num);
	break;

    case 19:	/* ST_ETABSIZE */
	PUT_INTVAL(v, conf[EDITORS].num);
	break;

    case 20:	/* ST_STRSIZE */
	PUT_INTVAL(v, MAX_STRLEN);
	break;

    case 21:	/* ST_ARRAYSIZE */
	PUT_INTVAL(v, conf[ARRAY_SIZE].num);
	break;

    case 22:	/* ST_STACKDEPTH */
	PUT_INTVAL(v, f->getDepth());
	break;

    case 23:	/* ST_TICKS */
	PUT_INTVAL(v, f->getTicks());
	break;

    case 24:	/* ST_DATAGRAMPORTS */
	a = Array::create(f->data, ndports);
	PUT_ARRVAL(v, a);
	for (i = 0, v = a->elts; i < ndports; i++, v++) {
	    PUT_INTVAL(v, dports[i]);
	}
	break;

    case 25:	/* ST_TELNETPORTS */
	a = Array::create(f->data, ntports);
	PUT_ARRVAL(v, a);
	for (i = 0, v = a->elts; i < ntports; i++, v++) {
	    PUT_INTVAL(v, tports[i]);
	}
	break;

    case 26:	/* ST_BINARYPORTS */
	a = Array::create(f->data, nbports);
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
 * return an array with information about resource usage
 */
Array *Config::status(Frame *f)
{
    Value *v;
    Int i;
    Array *a;

    try {
	ErrorContext::push();
	a = Array::createNil(f->data, 27);
	for (i = 0, v = a->elts; i < 27; i++, v++) {
	    statusi(f, i, v);
	}
	ErrorContext::pop();
    } catch (...) {
	a->ref();
	a->del();
	error((char *) NULL);
    }

    return a;
}

/*
 * return object resource usage information
 */
bool Config::objecti(Dataspace *data, Object *obj, Int idx, Value *v)
{
    Control *ctrl;
    Object *prog;
    Array *a;

    prog = (obj->flags & O_MASTER) ? obj : OBJR(obj->master);
    ctrl = (O_UPGRADING(prog)) ? OBJR(prog->prev)->ctrl : prog->control();

    switch (idx) {
    case 0:	/* O_COMPILETIME */
	PUT_INTVAL(v, ctrl->compiled);
	break;

    case 1:	/* O_PROGSIZE */
	PUT_INTVAL(v, ctrl->progSize());
	break;

    case 2:	/* O_DATASIZE */
	PUT_INTVAL(v, ctrl->nvariables);
	break;

    case 3:	/* O_NSECTORS */
	PUT_INTVAL(v, (O_HASDATA(obj)) ?  obj->dataspace()->nsectors : 0);
	if (obj->flags & O_MASTER) {
	    v->number += ctrl->nsectors;
	}
	break;

    case 4:	/* O_CALLOUTS */
	if (O_HASDATA(obj)) {
	    a = data->listCallouts(obj->dataspace());
	    if (a != (Array *) NULL) {
		PUT_ARRVAL(v, a);
	    } else {
		*v = Value::nil;
	    }
	} else {
	    PUT_ARRVAL(v, Array::create(data, 0));
	}
	break;

    case 5:	/* O_INDEX */
	PUT_INTVAL(v, (obj->flags & O_MASTER) ?
		       (Uint) obj->index : obj->master);
	break;

    case 6:	/* O_UNDEFINED */
	if (ctrl->flags & CTRL_UNDEFINED) {
	    PUT_MAPVAL(v, ctrl->undefined(data));
	} else {
	    *v = Value::nil;
	}
	break;

    default:
	return FALSE;
    }

    return TRUE;
}

/*
 * return resource usage of an object
 */
Array *Config::object(Dataspace *data, Object *obj)
{
    Value *v;
    Int i;
    Array *a;

    a = Array::createNil(data, 7);
    try {
	ErrorContext::push();
	for (i = 0, v = a->elts; i < 7; i++, v++) {
	    objecti(data, obj, i, v);
	}
	ErrorContext::pop();
    } catch (...) {
	a->ref();
	a->del();
	error((char *) NULL);
    }

    return a;
}


/*
 * retrieve an Int from a string (utility function)
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
