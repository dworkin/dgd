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
    long low, high;	/* lower and higher bound, for numeric values */
    union {
	long num;	/* numeric value */
	char *str;	/* string value */
    } u;
} config;

static config conf[] = {
# define ARRAY_SIZE	0
				{ "array_size",		INT_CONST, FALSE,
							1000, 8192 },
# define AUTO_OBJECT	1
				{ "auto_object",	STRING_CONST, FALSE },
# define BINARY_PORT	2
				{ "binary_port",	INT_CONST, FALSE,
							1000 },
# define CACHE_SIZE	3
				{ "cache_size",		INT_CONST, FALSE,
							100, UINDEX_MAX },
# define CALL_OUTS	4
				{ "call_outs",		INT_CONST, FALSE,
							0, UINDEX_MAX },
# define CALL_STACK	5
				{ "call_stack",		INT_CONST, FALSE,
							10 },
# define CREATE		6
				{ "create",		STRING_CONST, FALSE },
# define DIRECTORY	7
				{ "directory",		STRING_CONST, FALSE },
# define DRIVER_OBJECT	8
				{ "driver_object",	STRING_CONST, FALSE },
# define DUMP_FILE	9
				{ "dump_file",		STRING_CONST, FALSE },
# define DYNAMIC_CHUNK	10
				{ "dynamic_chunk",	INT_CONST, FALSE },
# define ED_TMPFILE	11
				{ "ed_tmpfile",		STRING_CONST, FALSE },
# define EDITORS	12
				{ "editors",		INT_CONST, FALSE,
							1, 255 },
# define INCLUDE_DIRS	13
				{ "include_dirs",	'(', FALSE },
# define INCLUDE_FILE	14
				{ "include_file",	STRING_CONST, FALSE },
# define MAX_COST	15
				{ "max_cost",		INT_CONST, FALSE,
							100000L },
# define OBJECTS	16
				{ "objects",		INT_CONST, FALSE,
							100 },
# define RESERVED_CSTACK 17
				{ "reserved_cstack",	INT_CONST, FALSE,
							5 },
# define RESERVED_VSTACK 18
				{ "reserved_vstack",	INT_CONST, FALSE,
							20 },
# define SECTOR_SIZE	19
				{ "sector_size",	INT_CONST, FALSE,
							512, 8192 },
# define STATIC_CHUNK	20
				{ "static_chunk",	INT_CONST, FALSE },
# define SWAP_FILE	21
				{ "swap_file",		STRING_CONST, FALSE },
# define SWAP_FRAGMENT	22
				{ "swap_fragment",	INT_CONST, FALSE },
# define SWAP_SIZE	23
				{ "swap_size",		INT_CONST, FALSE,
							1024, UINDEX_MAX },
# define TELNET_PORT	24
				{ "telnet_port",	INT_CONST, FALSE,
							1000 },
# define TYPECHECKING	25
				{ "typechecking",	INT_CONST, FALSE,
							0, 1 },
# define USERS		26
				{ "users",		INT_CONST, FALSE,
							1, 255 },
# define VALUE_STACK	27
				{ "value_stack",	INT_CONST, FALSE,
							100 },
# define NR_OPTIONS	28
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


static char header[18];	/* dump file header */
static Uint starttime;	/* start time */
static Uint elapsed;	/* elapsed time */
static Uint boottime;	/* boot time */

/*
 * NAME:	conf->dump()
 * DESCRIPTION:	dump system state on file
 */
void conf_dump()
{
    Uint t[4];
    int fd;

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

    header[2] = conf[TYPECHECKING].u.num;
    t[0] = conf[SECTOR_SIZE].u.num;
    t[1] = P_time();
    t[2] = starttime;
    t[3] = elapsed + t[1] - boottime;

    lseek(fd, 0L, SEEK_SET);
    write(fd, header, sizeof(header));
    write(fd, (char *) t, sizeof(t));
    P_message("*** State dumped.\012");
}

/*
 * NAME:	conf->restore()
 * DESCRIPTION:	restore system stare from file
 */
static void conf_restore(fd)
int fd;
{
    char buffer[sizeof(header)];
    Uint t[4];

    if (read(fd, buffer, sizeof(header)) != sizeof(header) ||
	buffer[0] != 1 ||
	memcmp(buffer + 2, header + 2, sizeof(header) - 2) != 0 ||
	read(fd, t, sizeof(t)) != sizeof(t)) {
	fatal("bad or incompatible restore file header");
    }
    t[1] = P_time() - t[1];
    starttime = t[2];
    elapsed = t[3];
    sw_restore(fd, (int) t[0]);
    kf_restore(fd);
    o_restore(fd, t[1]);
    co_restore(fd, t[1]);
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

# define MAX_DIRS	32

/*
 * NAME:	config->init()
 * DESCRIPTION:	initialize the driver
 */
void conf_init(configfile, dumpfile)
char *configfile, *dumpfile;
{
    static char *nodir[1], *dirs[MAX_DIRS], buffer[STRINGSZ];
    register char *p;
    register int h, l, m, c;
    FILE *fp;
    short s;
    Int i;
    int fd;
    calign cdummy;
    salign sdummy;
    ialign idummy;
    lalign ldummy;
    palign pdummy;

    if (!pp_init(configfile, nodir, 0)) {
	fatal("cannot open config file");
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
	    mstatic();
	    conf[m].u.str = strcpy(ALLOC(char, l + 1), p);
	    mdynamic();
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
		mstatic();
		dirs[l++] = strcpy(ALLOC(char, strlen(yytext) + 1), yytext);
		mdynamic();
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
	char buf[128];

	if (!conf[l].set) {
	    sprintf(buf, "unspecified option %s", conf[l].name);
	    conferr(buf);
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

    mstatic();

    /* initialize strings */
    str_init();

    /* initialize arrays */
    arr_init((int) conf[ARRAY_SIZE].u.num);

    /* initialize object */
    o_init((int) conf[OBJECTS].u.num);

    /* initialize swap */
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

    /* initialize compiler */
    c_init(conf[AUTO_OBJECT].u.str,
	   conf[DRIVER_OBJECT].u.str,
	   conf[INCLUDE_FILE].u.str,
	   dirs,
	   (int) conf[TYPECHECKING].u.num);

    /* initialize interpreter */
    i_init((int) conf[VALUE_STACK].u.num,
	   (int) conf[RESERVED_VSTACK].u.num,
	   (int) conf[CALL_STACK].u.num,
	   (int) conf[RESERVED_CSTACK].u.num,
	   conf[CREATE].u.str);

    mdynamic();

    /* initialize memory manager */
    minit((unsigned int) conf[STATIC_CHUNK].u.num,
    	  (unsigned int) conf[DYNAMIC_CHUNK].u.num);

    /* create status.h file */
    sprintf(buffer, "%s/status.h", dirs[0]);
    fp = fopen(path_file(path_resolve(buffer)), "w");
    if (fp == (FILE *) NULL) {
	fatal("cannot create \"%s\"", buffer);
    }
    P_fbinio(fp);

    fprintf(fp, "/*\012 * This file defines the fields of the array returned ");
    fprintf(fp, "by the\012 * status() kfun.  It is automatically generated ");
    fprintf(fp, "by DGD on startup.\012 */\012\012");
    fprintf(fp, "# define ST_VERSION\t0\t/* driver version */\012");

    fprintf(fp, "# define ST_STARTTIME\t1\t/* system start time */\012");
    fprintf(fp, "# define ST_BOOTTIME\t2\t/* system boot time */\012");
    fprintf(fp, "# define ST_UPTIME\t3\t/* system virtual uptime */\012");

    fprintf(fp, "# define ST_SWAPSIZE\t4\t/* # sectors on swap device */\012");
    fprintf(fp, "# define ST_SWAPUSED\t5\t/* # sectors allocated */\012");
    fprintf(fp, "# define ST_SECTORSIZE\t6\t/* size of swap sector */\012");
    fprintf(fp, "# define ST_SWAPRATE1\t7\t/* # objects swapped out per minute */\012");
    fprintf(fp, "# define ST_SWAPRATE5\t8\t/* # objects swapped out per five minutes */\012");

    fprintf(fp, "# define ST_SMEMSIZE\t9\t/* static memory allocated */\012");
    fprintf(fp, "# define ST_SMEMUSED\t10\t/* static memory in use */\012");
    fprintf(fp, "# define ST_DMEMSIZE\t11\t/* dynamic memory allocated */\012");
    fprintf(fp, "# define ST_DMEMUSED\t12\t/* dynamic memory in use */\012");

    fprintf(fp, "# define ST_OTABSIZE\t13\t/* object table size */\012");
    fprintf(fp, "# define ST_NOBJECTS\t14\t/* # objects in the system */\012");

    fprintf(fp, "# define ST_COTABSIZE\t15\t/* callouts table size */\012");
    fprintf(fp, "# define ST_NCOSHORT\t16\t/* # short-term callouts */\012");
    fprintf(fp, "# define ST_NCOLONG\t17\t/* # long-term callouts */\012");

    fprintf(fp, "# define ST_UTABSIZE\t18\t/* user table size */\012");
    fprintf(fp, "# define ST_ETABSIZE\t19\t/* editor table size */\012");

    fprintf(fp, "\012# define O_COMPILETIME\t0\t/* time of compilation */\012");
    fprintf(fp, "# define O_PROGSIZE\t1\t/* program size of object */\012");
    fprintf(fp, "# define O_DATASIZE\t2\t/* data size of object */\012");
    fprintf(fp, "# define O_NSECTORS\t3\t/* # sectors used by object */\012");
    fprintf(fp, "# define O_CALLOUTS\t4\t/* callouts in object */\012");
    fclose(fp);

    /* create type.h file */
    sprintf(buffer, "%s/type.h", dirs[0]);
    fp = fopen(path_file(path_resolve(buffer)), "w");
    if (fp == (FILE *) NULL) {
	fatal("cannot create \"%s\"", buffer);
    }
    P_fbinio(fp);

    fprintf(fp, "/*\012 * This file gives definitions for the value returned ");
    fprintf(fp, "by the\012 * typeof() kfun.  It is automatically generated ");
    fprintf(fp, "by DGD on startup.\012 */\012\012");
    fprintf(fp, "# define T_INT\t\t%d\012", T_INT);
    fprintf(fp, "# define T_FLOAT\t%d\012", T_FLOAT);
    fprintf(fp, "# define T_STRING\t%d\012", T_STRING);
    fprintf(fp, "# define T_OBJECT\t%d\012", T_OBJECT);
    fprintf(fp, "# define T_ARRAY\t%d\012", T_ARRAY);
    fprintf(fp, "# define T_MAPPING\t%d\012", T_MAPPING);
    fclose(fp);

    /* create limits.h file */
    sprintf(buffer, "%s/limits.h", dirs[0]);
    fp = fopen(path_file(path_resolve(buffer)), "w");
    if (fp == (FILE *) NULL) {
	fatal("cannot create \"%s\"", buffer);
    }
    P_fbinio(fp);

    fprintf(fp, "/*\012 * This file defines some basic sizes of datatypes and ");
    fprintf(fp, "resources.\012 * It is automatically generated by DGD on ");
    fprintf(fp, "startup.\012 */\012\012");
    fprintf(fp, "# define CHAR_BIT\t\t8\t\t/* # bits in character */\012");
    fprintf(fp, "# define CHAR_MIN\t\t0\t\t/* min character value */\012");
    fprintf(fp, "# define CHAR_MAX\t\t255\t\t/* max character value */\012\012");
    fprintf(fp, "# define INT_MIN\t\t0x80000000\t/* -2147483648 */\012");
    fprintf(fp, "# define INT_MAX\t\t2147483647\t/* max integer value */\012\012");
    fprintf(fp, "# define MAX_STRING_SIZE\t%u\t\t/* max string size */\012",
	    USHRT_MAX - sizeof(string));
    fprintf(fp, "# define MAX_ARRAY_SIZE\t\t%ld\t\t/* max array size */\012",
	    conf[ARRAY_SIZE].u.num);
    fprintf(fp, "# define MAX_MAPPING_SIZE\t%ld\t\t/* max mapping size */\012\012",
	    conf[ARRAY_SIZE].u.num);
    fprintf(fp, "# define MAX_EXEC_COST\t\t%ld\t\t/* max execution cost */\012",
	    conf[MAX_COST].u.num);
    fclose(fp);

    /* create float.h file */
    sprintf(buffer, "%s/float.h", dirs[0]);
    fp = fopen(path_file(path_resolve(buffer)), "w");
    if (fp == (FILE *) NULL) {
	fatal("cannot create \"%s\"", buffer);
    }
    P_fbinio(fp);

    fprintf(fp, "/*\012 * This file describes the floating point type. It is ");
    fprintf(fp, "automatically\012 * generated by DGD on startup.\012 */\012\012");
    fprintf(fp, "# define FLT_RADIX\t2\t\t\t/* binary */\012");
    fprintf(fp, "# define FLT_ROUNDS\t1\t\t\t/* round to nearest */\012");
    fprintf(fp, "# define FLT_EPSILON\t7.2759576142E-12\t/* smallest x: 1.0 + x != 1.0 */\012");
    fprintf(fp, "# define FLT_DIG\t10\t\t\t/* decimal digits of precision*/\012");
    fprintf(fp, "# define FLT_MANT_DIG\t36\t\t\t/* binary digits of precision */\012");
    fprintf(fp, "# define FLT_MIN\t2.22507385851E-308\t/* positive minimum */\012");
    fprintf(fp, "# define FLT_MIN_EXP\t(-1021)\t\t\t/* minimum binary exponent */\012");
    fprintf(fp, "# define FLT_MIN_10_EXP\t(-307)\t\t\t/* minimum decimal exponent */\012");
    fprintf(fp, "# define FLT_MAX\t1.79769313485E+308\t/* positive maximum */\012");
    fprintf(fp, "# define FLT_MAX_EXP\t1024\t\t\t/* maximum binary exponent */\012");
    fprintf(fp, "# define FLT_MAX_10_EXP\t308\t\t\t/* maximum decimal exponent */\012");
    fclose(fp);

    /* preload compiled objects */
    pc_preload(conf[AUTO_OBJECT].u.str, conf[DRIVER_OBJECT].u.str);

    /* initialize dumpfile header */
    s = 0x1234;
    i = 0x12345678L;
    header[0] = 1;				/* valid dump flag */
    header[1] = 0;				/* editor flag */
    header[2] = conf[TYPECHECKING].u.num;	/* global typechecking */
    header[3] = sizeof(uindex);			/* sizeof uindex */
    header[4] = sizeof(long);			/* sizeof long */
    header[5] = sizeof(char *);			/* sizeof char* */
    header[6] = ((char *) &s)[0];		/* 1 of 2 */
    header[7] = ((char *) &s)[1];		/* 2 of 2 */
    header[8] = ((char *) &i)[0];		/* 1 of 4 */
    header[9] = ((char *) &i)[1];		/* 2 of 4 */
    header[10] = ((char *) &i)[2];		/* 3 of 4 */
    header[11] = ((char *) &i)[3];		/* 4 of 4 */
    header[12] = (char *) &cdummy.c - (char *) &cdummy.fill; /* char align */
    header[13] = (char *) &sdummy.s - (char *) &sdummy.fill; /* short align */
    header[14] = (char *) &idummy.i - (char *) &idummy.fill; /* int align */
    header[15] = (char *) &ldummy.l - (char *) &ldummy.fill; /* long align */
    header[16] = (char *) &pdummy.p - (char *) &pdummy.fill; /* pointer align */
    header[17] = sizeof(align);				     /* struct align */

    starttime = boottime = P_time();
    i_set_cost((Int) conf[MAX_COST].u.num);
    if (dumpfile == (char *) NULL) {
	/*
	 * initialize mudlib
	 */
	call_driver_object("initialize", 0);
    } else {
	/*
	 * restore dump file
	 */
	conf_restore(fd);
	close(fd);
	call_driver_object("restored", 0);
    }
    i_del_value(sp++);

    /* initialize communications */
    mstatic();
    comm_init((int) conf[USERS].u.num,
	      (int) conf[TELNET_PORT].u.num,
	      (int) conf[BINARY_PORT].u.num);
    mdynamic();
}

/*
 * NAME:	config->basedir()
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
 * NAME:	config->exec_cost()
 * DESCRIPTION:	return the maximum execution cost
 */
Int conf_exec_cost()
{
    return conf[MAX_COST].u.num;
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

    a = arr_new(20L);
    v = a->elts;

    /* version */
    v->type = T_STRING;
    version = VERSION;
    str_ref((v++)->u.string = str_new(version, (long) strlen(version)));

    /* uptime */
    v->type = T_INT;
    (v++)->u.number = starttime;
    v->type = T_INT;
    (v++)->u.number = boottime;
    v->type = T_INT;
    (v++)->u.number = elapsed + P_time() - boottime;

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
    mstat = minfo();
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
    v->u.number = conf[EDITORS].u.num;

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
