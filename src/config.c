# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "path.h"
# include "ed.h"
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
							1, USHRT_MAX },
# define AUTO_OBJECT	1
				{ "auto_object",	STRING_CONST, FALSE },
# define BINARY_PORT	2
				{ "binary_port",	INT_CONST, FALSE,
							0, USHRT_MAX },
# define CACHE_SIZE	3
				{ "cache_size",		INT_CONST, FALSE,
							100, UINDEX_MAX },
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


typedef struct {
    char fill;		/* filler */
    char c;		/* char */
} calign;

typedef struct {
    char fill;		/* filler */
    short s;		/* short */
} salign;

typedef struct {
    char fill;		/* filler */
    Int i;		/* Int */
} ialign;

typedef struct {
    char fill;		/* filler */
    long l;		/* long */
} lalign;

typedef struct {
    char fill;		/* filler */
    char *p;		/* char* */
} palign;

typedef struct {	/* struct align */
    char c;
} align;


typedef struct {
    char b[18];		/* various things */
    Int sectorsz;	/* sector size */
    Uint btime;		/* boot time */
    Uint etime;		/* elapsed time */
} dumpinfo;

static dumpinfo header;	/* dumpfile header */
static Uint boottime;	/* boot time */
static Uint elapsed;	/* elapsed time */
static Uint starttime;	/* start time */

/*
 * NAME:	conf->dump()
 * DESCRIPTION:	dump system state on file
 */
void conf_dump()
{
    int fd;

    header.b[2] = conf[TYPECHECKING].u.num;
    header.sectorsz = conf[SECTOR_SIZE].u.num;
    header.btime = boottime;
    header.etime = elapsed + P_time() - starttime;

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
    dumpinfo info;

    if (read(fd, &info, sizeof(dumpinfo)) != sizeof(dumpinfo) ||
	memcmp(info.b, header.b, sizeof(info.b)) != 0) {
	fatal("bad or incompatible restore file header");
    }
    boottime = info.btime;
    elapsed = info.etime;
    sw_restore(fd, (int) info.sectorsz);
    kf_restore(fd);
    o_restore(fd);
    pc_restore(fd);
    starttime = P_time();
    co_restore(fd, starttime);
}

/*
 * NAME:	conferr()
 * DESCRIPTION:	cause a fatal error during the configuration phase
 */
static void conferr(err)
char *err;
{
    fatal("config file, line %u: %s", tk_line(), err);
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
    if ((fd=open(path_file(fname = path_resolve(file)),
		 O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644)) < 0) {
	fatal("cannot create \"/%s\"", fname);
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
	fatal("cannot write \"/%s\"", fname);
    }
    close(fd);
}

# define MAX_DIRS	32

/*
 * NAME:	config->init()
 * DESCRIPTION:	initialize the driver
 */
void conf_init(configfile, dumpfile)
char *configfile, *dumpfile;
{
    static char *nodir[1], *dirs[MAX_DIRS];
    char buf[BUF_SIZE], buffer[STRINGSZ];
    register char *p;
    register int h, l, m, c;
    short s;
    Int i;
    int fd;
    calign cdummy;
    salign sdummy;
    ialign idummy;
    lalign ldummy;
    palign pdummy;

    if (!pp_init(configfile, nodir, 0)) {
	message("Cannot open config file\012");	/* LF */
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
	fd = open(dumpfile, O_RDONLY | O_BINARY);
	if (fd < 0) {
	    fatal("cannot open restore file");
	}
    }

    /* change directory */
    if (chdir(conf[DIRECTORY].u.str) < 0) {
	fatal("bad base directory \"%s\"", conf[DIRECTORY].u.str);
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
	   dirs,
	   (int) conf[TYPECHECKING].u.num);

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
    s = 0x1234;
    i = 0x12345678L;
    header.b[0] = 1;				/* valid dump flag */
    header.b[1] = 1;				/* dump file version number */
    header.b[2] = conf[TYPECHECKING].u.num;	/* global typechecking */
    header.b[3] = sizeof(uindex);		/* sizeof uindex */
    header.b[4] = sizeof(long);			/* sizeof long */
    header.b[5] = sizeof(char *);		/* sizeof char* */
    header.b[6] = ((char *) &s)[0];		/* 1 of 2 */
    header.b[7] = ((char *) &s)[1];		/* 2 of 2 */
    header.b[8] = ((char *) &i)[0];		/* 1 of 4 */
    header.b[9] = ((char *) &i)[1];		/* 2 of 4 */
    header.b[10] = ((char *) &i)[2];		/* 3 of 4 */
    header.b[11] = ((char *) &i)[3];		/* 4 of 4 */
    header.b[12] = (char *) &cdummy.c - (char *) &cdummy.fill; /* char align */
    header.b[13] = (char *) &sdummy.s - (char *) &sdummy.fill; /* short align */
    header.b[14] = (char *) &idummy.i - (char *) &idummy.fill; /* int align */
    header.b[15] = (char *) &ldummy.l - (char *) &ldummy.fill; /* long align */
    header.b[16] = (char *) &pdummy.p - (char *) &pdummy.fill; /* ptr align */
    header.b[17] = sizeof(align);			     /* struct align */

    starttime = boottime = P_time();
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
    i_del_value(sp++);

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

    a = arr_new(25L);
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
    (v++)->u.number = i_get_depth();
    v->type = T_INT;
    (v++)->u.number = i_get_ticks();

    /* precompiled objects */
    v->type = T_ARRAY;
    arr_ref(v->u.array = pc_list());

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
    dataspace *data;

    clist = co_list(obj);
    ctrl = o_control(obj);
    data = o_dataspace(obj);
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
	(v++)->u.number = ctrl->nsectors + data->nsectors;
    } else {
	(v++)->u.number = data->nsectors;
    }
    v->type = T_ARRAY;
    arr_ref(v->u.array = clist);

    return a;
}
