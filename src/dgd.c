# include "dgd.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "path.h"
# include "ed.h"
# include "call_out.h"
# include "comm.h"
# include "lex.h"
# include "comp.h"

typedef struct {
    char *name;		/* name of the option */
    short type;		/* option type */
    long low, high;	/* lower and higher bound, for numeric values */
    union {
	long num;	/* numeric value */
	char *str;	/* string value */
    } u;
} config;

static config conf[] = {
# define ARRAY_SIZE	0
				{ "array_size",		INT_CONST, 1000 },
# define AUTO_OBJECT	1
				{ "auto_object",	STRING_CONST },
# define CALL_OUTS	2
				{ "call_outs",		INT_CONST, 1 },
# define CALL_STACK	3
				{ "call_stack",		INT_CONST, 10 },
# define CREATE		4
				{ "create",		STRING_CONST },
# define DIRECTORY	5
				{ "directory",		STRING_CONST },
# define DRIVER_OBJECT	6
				{ "driver_object",	STRING_CONST },
# define ED_TMPFILE	7
				{ "ed_tmpfile",		STRING_CONST },
# define EDITORS	8
				{ "editors",		INT_CONST, 1, 255 },
# define INCLUDE_DIRS	9
				{ "include_dirs",	'(' },
# define INCLUDE_FILE	10
				{ "include_file",	STRING_CONST },
# define MAX_COST	11
				{ "max_cost",		INT_CONST, 100000L },
# define OBJECTS	12
				{ "objects",		INT_CONST, 100 },
# define PORT_NUMBER	13
				{ "port_number",	INT_CONST, 2000 },
# define RESERVED_STACK	14
				{ "reserved_stack",	INT_CONST, 5 },
# define SWAP_CACHE	15
				{ "swap_cache",		INT_CONST, 100 },
# define SWAP_FILE	16
				{ "swap_file",		STRING_CONST },
# define SWAP_SECTOR	17
				{ "swap_sector",	INT_CONST, 512, 8192 },
# define SWAP_SIZE	18
				{ "swap_size",		INT_CONST, 1024 },
# define TIME_INTERVAL	19
				{ "time_interval",	INT_CONST, 1 },
# define TYPECHECKING	20
				{ "typechecking",	INT_CONST, 0, 1 },
# define USERS		21
				{ "users",		INT_CONST, 1, 255 },
# define VALUE_STACK	22
				{ "value_stack",	INT_CONST, 100 },
# define NR_OPTIONS	23
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
 * NAME:	initialize()
 * DESCRIPTION:	initialize the driver
 */
static void initialize(configfile)
char *configfile;
{
    static char *dirs[MAX_DIRS];
    register char *p;
    register int h, l, m, c;

    if (!pp_init(configfile, dirs)) {
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
	if (pp_gettok() != ';') {
	    conferr("';' expected");
	}
    }
    pp_clear();

    for (l = 0; l < NR_OPTIONS; l++) {
	char buf[128];

	switch (conf[l].type) {
	case INT_CONST:
	    if (conf[l].u.num >= conf[l].low) {
		continue;
	    }
	    break;

	case STRING_CONST:
	    if (conf[l].u.str != (char *) NULL) {
		continue;
	    }
	    break;

	case '(':
	    if (dirs[0] != (char *) NULL) {
		continue;
	    }
	    break;
	}
	sprintf(buf, "unspecified configuration parameter %s", conf[l].name);
	conferr(buf);
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
    co_init(conf[TIME_INTERVAL].u.num,
	    (int) conf[CALL_OUTS].u.num);

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
	   conf[MAX_COST].u.num,
	   conf[CREATE].u.str);

    /* initialize mudlib */
    i_lock();
    call_driver_object("initialize", 0);
    i_unlock();

    /* initialize communications */
    comm_init((int) conf[USERS].u.num,
	      (int) conf[PORT_NUMBER].u.num);
}

/*
 * NAME:	call_driver_object()
 * DESCRIPTION:	call a function in the driver object
 */
void call_driver_object(func, narg)
char *func;
int narg;
{
    static object *driver;
    static long dcount;

    if (driver == (object *) NULL || dcount != driver->key.count) {
	driver = o_find(conf[DRIVER_OBJECT].u.str);
	if (driver == (object *) NULL) {
	    driver = c_compile(conf[DRIVER_OBJECT].u.str);
	}
	dcount = driver->key.count;
    }
    if (!i_call(driver, func, TRUE, narg)) {
	fatal("missing function %s in driver object", func);
    }
}

static object *usr;

/*
 * NAME:	this_user()
 * DESCRIPTION:	return the current user object
 */
object *this_user()
{
    return (usr != (object *) NULL && usr->key.count != 0) ?
	    usr : (object *) NULL;
}

/*
 * NAME:	main()
 * DESCRIPTION:	the main function of DGD
 */
int main(argc, argv)
int argc;
char *argv[];
{
    char buf[INBUF_SIZE];
    int size;

    host_init();

    if (argc != 2) {
	fprintf(stderr, "Usage: %s config_file\n", argv[0]);
	return 2;
    }

    if (ec_push()) {
	warning((char *) NULL);
	fatal("error during initialiation");
    }
    initialize(argv[1]);
    ec_pop();

    while (ec_push()) {
	char *err;

	warning((char *) NULL);
	i_dump_trace(stderr);
	host_error();
	i_clear();
	i_log_error();
	comm_flush();
    }

    for (;;) {
	usr = comm_receive(buf, &size);
	if (usr != (object *) NULL) {
	    (--sp)->type = T_STRING;
	    str_ref(sp->u.string = str_new(buf, (long) size));
	    if (i_call(usr, "receive_message", TRUE, 1)) {
		i_del_value(sp++);
	    }
	}
	co_call();

	comm_flush();
	o_clean();
    }
}
