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
# define ED_TMPFILE	7
				{ "ed_tmpfile",		STRING_CONST, FALSE },
# define EDITORS	8
				{ "editors",		INT_CONST, FALSE,
							1, 255 },
# define INCLUDE_DIRS	9
				{ "include_dirs",	'(', FALSE },
# define INCLUDE_FILE	10
				{ "include_file",	STRING_CONST, FALSE },
# define MAX_COST	11
				{ "max_cost",		INT_CONST, FALSE,
							100000L },
# define OBJECTS	12
				{ "objects",		INT_CONST, FALSE,
							100 },
# define PORT_NUMBER	13
				{ "port_number",	INT_CONST, FALSE,
							2000 },
# define RESERVED_STACK	14
				{ "reserved_stack",	INT_CONST, FALSE,
							5 },
# define SWAP_CACHE	15
				{ "swap_cache",		INT_CONST, FALSE,
							100, UINDEX_MAX },
# define SWAP_FILE	16
				{ "swap_file",		STRING_CONST, FALSE },
# define SWAP_SECTOR	17
				{ "swap_sector",	INT_CONST, FALSE,
							512, 8192 },
# define SWAP_SIZE	18
				{ "swap_size",		INT_CONST, FALSE,
							1024, UINDEX_MAX },
# define TYPECHECKING	19
				{ "typechecking",	INT_CONST, FALSE,
							0, 1 },
# define USERS		20
				{ "users",		INT_CONST, FALSE,
							1, 255 },
# define VALUE_STACK	21
				{ "value_stack",	INT_CONST, FALSE,
							100 },
# define NR_OPTIONS	22
};

/*
 * NAME:	conferr()
 * DESCRIPTION:	cause a fatal error during the configuration phase
 */
static void conferr(err)
char *err;
{
    fatal("line %u: %s in config file", tk_line(), err);
}

# define MAX_DIRS	32

/*
 * NAME:	config->init()
 * DESCRIPTION:	initialize the driver
 */
void conf_init(configfile)
char *configfile;
{
    static char *dirs[MAX_DIRS], limits[STRINGSZ];
    register char *p;
    register int h, l, m, c;
    FILE *fp;

    if (!pp_init(configfile, dirs, 0)) {
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
	    conf[m].u.str = strcpy(ALLOC(char, l + 1), p);
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
		dirs[l++] = strcpy(ALLOC(char, strlen(yytext) + 1), yytext);
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
    pp_clear();

    for (l = 0; l < NR_OPTIONS; l++) {
	char buf[128];

	if (!conf[l].set) {
	    sprintf(buf, "unspecified configuration parameter %s",
		    conf[l].name);
	    conferr(buf);
	}
    }

    /* change directory */
    if (chdir(conf[DIRECTORY].u.str) < 0) {
	fatal("bad base directory \"%s\"", conf[DIRECTORY].u.str);
    }

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
    co_init((int) conf[CALL_OUTS].u.num);

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
	   (int) conf[CALL_STACK].u.num,
	   (int) conf[RESERVED_STACK].u.num,
	   conf[CREATE].u.str);

    /* create limits file */
    sprintf(limits, "%s/limits.h", path_resolve(dirs[0]));
    fp = fopen(limits, "w");
    if (fp == (FILE *) NULL) {
	fatal("cannot create \"/%s\"", limits);
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

    /* preload precompiled objects */
    preload();

    /* initialize mudlib */
    i_set_cost(conf[MAX_COST].u.num);
    i_lock();
    call_driver_object("initialize", 0);
    i_unlock();
    i_del_value(sp++);

    /* initialize communications */
    comm_init((int) conf[USERS].u.num,
	      (int) conf[PORT_NUMBER].u.num);
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
long conf_exec_cost()
{
    return conf[MAX_COST].u.num;
}
