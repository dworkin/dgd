# define INCLUDE_FILE_IO
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "path.h"
# include "ed.h"
# include "call_out.h"
# include "comm.h"
# include "macro.h"
# include "token.h"
# include "ppcontrol.h"
# include "node.h"
# include "parser.h"
# include "compile.h"
# include "csupport.h"

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
# define CALL_OUTS	2
				{ "call_outs",		INT_CONST, FALSE,
							0, UINDEX_MAX },
# define CALL_STACK	3
				{ "call_stack",		INT_CONST, FALSE,
							10 },
# define CREATE		4
				{ "create",		STRING_CONST, FALSE },
# define DIRECTORY	5
				{ "directory",		STRING_CONST, FALSE },
# define DRIVER_OBJECT	6
				{ "driver_object",	STRING_CONST, FALSE },
# define DUMP_FILE	7
				{ "dump_file",		STRING_CONST, FALSE },
# define DYNAMIC_CHUNK	8
				{ "dynamic_chunk",	INT_CONST, FALSE },
# define ED_TMPFILE	9
				{ "ed_tmpfile",		STRING_CONST, FALSE },
# define EDITORS	10
				{ "editors",		INT_CONST, FALSE,
							1, 255 },
# define INCLUDE_DIRS	11
				{ "include_dirs",	'(', FALSE },
# define INCLUDE_FILE	12
				{ "include_file",	STRING_CONST, FALSE },
# define MAX_COST	13
				{ "max_cost",		INT_CONST, FALSE,
							100000L },
# define OBJECTS	14
				{ "objects",		INT_CONST, FALSE,
							100 },
# define PORT_NUMBER	15
				{ "port_number",	INT_CONST, FALSE,
							1000 },
# define RESERVED_CSTACK 16
				{ "reserved_cstack",	INT_CONST, FALSE,
							5 },
# define RESERVED_VSTACK 17
				{ "reserved_vstack",	INT_CONST, FALSE,
							20 },
# define STATIC_CHUNK	18
				{ "static_chunk",	INT_CONST, FALSE },
# define SWAP_CACHE	19
				{ "swap_cache",		INT_CONST, FALSE,
							100, UINDEX_MAX },
# define SWAP_FILE	20
				{ "swap_file",		STRING_CONST, FALSE },
# define SWAP_FRAGMENT	21
				{ "swap_fragment",	INT_CONST, FALSE },
# define SWAP_SECTOR	22
				{ "swap_sector",	INT_CONST, FALSE,
							512, 8192 },
# define SWAP_SIZE	23
				{ "swap_size",		INT_CONST, FALSE,
							1024, UINDEX_MAX - 1 },
# define TYPECHECKING	24
				{ "typechecking",	INT_CONST, FALSE,
							0, 1 },
# define USERS		25
				{ "users",		INT_CONST, FALSE,
							1, 255 },
# define VALUE_STACK	26
				{ "value_stack",	INT_CONST, FALSE,
							100 },
# define NR_OPTIONS	27
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

static char header[16];	/* dump file header */


/*
 * NAME:	conf->dump()
 * DESCRIPTION:	dump system state on file
 */
void conf_dump()
{
    char buffer[STRINGSZ + 4];
    int fd;
    long t;

    sprintf(buffer, "%s.old", conf[DUMP_FILE].u.str);
    unlink(buffer);
    rename(conf[DUMP_FILE].u.str, buffer);
    fd = open(conf[DUMP_FILE].u.str, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0) {
	fatal("cannot create dump file \"%s\"", conf[DUMP_FILE].u.str);
    }

    header[0] = 0;
    t = P_time();
    if (write(fd, header, sizeof(header)) >= 0 &&
	write(fd, &t, sizeof(long)) >= 0 &&
	sw_dump(fd) && o_dump(fd) && co_dump(fd)) {
	header[0] = 1;
	lseek(fd, 0L, SEEK_SET);
	write(fd, header, 1);
	fprintf(stderr, "*** State dumped.\n");
    } else {
	fprintf(stderr, "*** State dump failed.\n");
    }
    close(fd);
}

/*
 * NAME:	conf->restore()
 * DESCRIPTION:	restore system stare from file
 */
static void conf_restore(file)
char *file;
{
    char buffer[sizeof(header)];
    int fd;
    long t;

    fd = open(file, O_RDONLY);
    if (fd < 0) {
	fatal("cannot open restore file");
    }
    header[0] = 1;
    if (read(fd, buffer, sizeof(header)) != sizeof(header) ||
	memcmp(buffer, header, sizeof(header)) != 0 ||
	read(fd, &t, sizeof(long)) != sizeof(long)) {
	fatal("bad restore file header");
    }
    sw_restore(fd);
    t = P_time() - t;
    o_restore(fd, t);
    co_restore(fd, t);
    close(fd);
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
    calign cdummy;
    salign sdummy;
    ialign idummy;
    lalign ldummy;
    palign pdummy;
    align dummy;

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
	    (int) conf[SWAP_CACHE].u.num,
	    (int) conf[SWAP_SECTOR].u.num);

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

    /* create limits file */
    sprintf(buffer, "%s/limits.h", dirs[0]);
    fp = fopen(path_resolve(buffer), "w");
    if (fp == (FILE *) NULL) {
	fatal("cannot create \"%s\"", buffer);
    }
    fprintf(fp, "/*\n * This file defines some basic sizes of datatypes and ");
    fprintf(fp, "resources.\n * It is automatically generated by DGD on ");
    fprintf(fp, "startup.\n */\n\n");
    fprintf(fp, "# define CHAR_BIT\t\t8\t\t/* # bits in character */\n");
    fprintf(fp, "# define CHAR_MIN\t\t0\t\t/* min character value */\n");
    fprintf(fp, "# define CHAR_MAX\t\t255\t\t/* max character value */\n\n");
    fprintf(fp, "# define INT_MIN\t\t%ld\t/* min integer value */\n",
	    (long) (Int) 0x80000000L);
    fprintf(fp, "# define INT_MAX\t\t%ld\t/* max integer value */\n\n",
	    (long) (Int) 0x7fffffffL);
    fprintf(fp, "# define MAX_STRING_SIZE\t%u\t\t/* max string size */\n",
	    USHRT_MAX - sizeof(string));
    fprintf(fp, "# define MAX_ARRAY_SIZE\t\t%ld\t\t/* max array size */\n",
	    conf[ARRAY_SIZE].u.num);
    fprintf(fp, "# define MAX_MAPPING_SIZE\t%ld\t\t/* max mapping size */\n\n",
	    conf[ARRAY_SIZE].u.num);
    fprintf(fp, "# define MAX_EXEC_COST\t\t%ld\t\t/* max execution cost */\n",
	    conf[MAX_COST].u.num);
    fprintf(fp, "# define MAX_OBJECTS\t\t%ld\t\t/* max # of objects */\n",
	    conf[OBJECTS].u.num);
    fprintf(fp, "# define MAX_CALL_OUTS\t\t%ld\t\t/* max # of call_outs */\n\n",
	    conf[CALL_OUTS].u.num);
    fprintf(fp, "# define MAX_USERS\t\t%ld\t\t/* max # of users */\n",
	    conf[USERS].u.num);
    fprintf(fp, "# define MAX_EDITORS\t\t%ld\t\t/* max # of editors */\n",
	    conf[EDITORS].u.num);
    fclose(fp);

    /* create status file */
    sprintf(buffer, "%s/status.h", dirs[0]);
    fp = fopen(path_resolve(buffer), "w");
    if (fp == (FILE *) NULL) {
	fatal("cannot create \"%s\"", buffer);
    }
    fprintf(fp, "/*\n * This file defines the fields of the array returned ");
    fprintf(fp, "by the\n * status() kfun.  It is automatically generated by ");
    fprintf(fp, "DGD on startup.\n */\n\n");
    fprintf(fp, "# define ST_SWAPSIZE\t0\t/* # sectors on swap device */\n");
    fprintf(fp, "# define ST_SWAPUSED\t1\t/* # sectors allocated */\n");
    fprintf(fp,
	   "# define ST_SWAPRATE\t2\t/* # objects swapped out per minute */\n");
    fprintf(fp, "# define ST_MEMSIZE\t3\t/* size of memory allocated */\n");
    fprintf(fp, "# define ST_MEMUSED\t4\t/* size of memory in use */\n");
    fprintf(fp, "# define ST_NOBJECTS\t5\t/* # objects in the system */\n");
    fprintf(fp, "# define ST_NCALLOUTS\t6\t/* # call_outs in the system */\n");
    fprintf(fp, "\n# define O_PROGSIZE\t0\t/* program size of object */\n");
    fprintf(fp, "# define O_NSECTORS\t1\t/* # sectors used by object */\n");
    fprintf(fp, "# define O_NCALLOUTS\t2\t/* # call_outs in object */\n");
    fclose(fp);

    /* preload compiled objects */
    pc_preload(conf[AUTO_OBJECT].u.str, conf[DRIVER_OBJECT].u.str);

    /* initialize dumpfile header */
    s = 0x1234;
    i = 0x12345678L;
    header[0] = 0;
    header[1] = sizeof(uindex);
    header[2] = sizeof(long);
    header[3] = sizeof(char *);
    header[4] = ((char *) &s)[0];
    header[5] = ((char *) &s)[1];
    header[6] = ((char *) &i)[0];
    header[7] = ((char *) &i)[1];
    header[8] = ((char *) &i)[2];
    header[9] = ((char *) &i)[3];
    header[10] = (char *) &cdummy.c - (char *) &cdummy.fill;
    header[11] = (char *) &sdummy.s - (char *) &sdummy.fill;
    header[12] = (char *) &idummy.i - (char *) &idummy.fill;
    header[13] = (char *) &ldummy.l - (char *) &ldummy.fill;
    header[14] = (char *) &pdummy.p - (char *) &pdummy.fill;
    header[15] = sizeof(align);

    i_set_cost((Int) conf[MAX_COST].u.num);
    if (dumpfile == (char *) NULL) {
	/* initialize mudlib */
	call_driver_object("initialize", 0);
	i_del_value(sp++);
    } else {
	/* restore dump file */
	conf_restore(dumpfile);
	call_driver_object("restored", 0);
	i_del_value(sp++);
    }

    /* initialize communications */
    mstatic();
    comm_init((int) conf[USERS].u.num,
	      (int) conf[PORT_NUMBER].u.num);
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
    array *a;
    register value *v;

    a = arr_new(7L);
    v = a->elts;
    v->type = T_NUMBER;
    (v++)->u.number = conf[SWAP_SIZE].u.num;
    v->type = T_NUMBER;
    (v++)->u.number = sw_count();
    v->type = T_NUMBER;
    (v++)->u.number = co_swaprate();
    v->type = T_NUMBER;
    (v++)->u.number = memsize();
    v->type = T_NUMBER;
    (v++)->u.number = memused();
    v->type = T_NUMBER;
    (v++)->u.number = o_count();
    v->type = T_NUMBER;
    v->u.number = co_count();

    return a;
}

/*
 * NAME:	config->object()
 * DESCRIPTION:	return resource usage of an object
 */
array *conf_object(obj)
object *obj;
{
    array *a;
    register value *v;
    dataspace *data;

    a = arr_new(3L);
    v = a->elts;
    data = o_dataspace(obj);
    if (obj->flags & O_MASTER) {
	register control *ctrl;

	ctrl = o_control(obj);
	v->type = T_NUMBER;
	(v++)->u.number = ctrl->ninherits * sizeof(dinherit) +
			  ctrl->progsize +
			  ctrl->nstrings * (long) sizeof(dstrconst) +
			  ctrl->strsize +
			  ctrl->nfuncdefs * sizeof(dfuncdef) +
			  ctrl->nvardefs * sizeof(dvardef) +
			  ctrl->nfuncalls * 2L +
			  ctrl->nsymbols * (long) sizeof(dsymbol);
	v->type = T_NUMBER;
	(v++)->u.number = ctrl->nsectors + data->nsectors;
    } else {
	v->type = T_NUMBER;
	(v++)->u.number = 0;
	v->type = T_NUMBER;
	(v++)->u.number = data->nsectors;
    }
    v->type = T_NUMBER;
    v->u.number = d_ncallouts(data);

    return a;
}
