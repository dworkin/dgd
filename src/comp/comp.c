# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "path.h"
# include "hash.h"
# include "swap.h"
# include "comm.h"
# include "ed.h"
# include "call_out.h"
# include "node.h"
# include "compile.h"
# include "csupport.h"

static int size;		/* current size of the dumped line */

/*
 * NAME:	dump()
 * DESCRIPTION:	output a number
 */
static void dump(n)
unsigned short n;
{
    if (size == 16) {
	putchar('\n');
	size = 0;
    }
    printf("%u, ", n);
    size++;
}

/*
 * NAME:	dump_chars()
 * DESCRIPTION:	output a range of characters
 */
static void dump_chars(p, n)
register char *p;
register unsigned int n;
{
    while (n > 0) {
	dump(UCHAR(*p++));
	--n;
    }
}

/*
 * NAME:	dump_inherits()
 * DESCRIPTION:	output the inherited objects
 */
static void dump_inherits(ctrl)
register control *ctrl;
{
    register int i;

    printf("\nstatic char *inherits[] = {\n");
    for (i = 0; i < ctrl->ninherits; i++) {
	printf("\"%s\",\n", o_name(ctrl->inherits[i].obj));
    }
    printf("};\n");
}

/*
 * NAME:	dump_program()
 * DESCRIPTION:	output the program
 */
static void dump_program(ctrl)
control *ctrl;
{
    if (ctrl->progsize != 0) {
	printf("\nstatic char program[] = {\n");
	size = 0;
	dump_chars(ctrl->prog, ctrl->progsize);
	printf("\n};\n");
    }
}

/*
 * NAME:	dump_strings()
 * DESCRIPTION:	output the strings
 */
static void dump_strings(ctrl)
register control *ctrl;
{
    register int i;

    if (ctrl->nstrings != 0) {
	printf("\nstatic char stext[] = {\n");
	size = 0;
	for (i = 0; i < ctrl->nstrings; i++) {
	    dump_chars(ctrl->strings[i]->text, ctrl->strings[i]->len);
	}
	printf("\n};\n\nstatic unsigned short slength[] = {\n");
	size = 0;
	for (i = 0; i < ctrl->nstrings; i++) {
	    dump(ctrl->strings[i]->len);
	}
	printf("\n};\n");
    }
}

/*
 * NAME:	dump_functions()
 * DESCRIPTION:	output the function table
 */
static void dump_functions(ctrl, nfuncs)
register control *ctrl;
register int nfuncs;
{
    register int i;

    if (nfuncs != 0) {
	printf("\nstatic pcfunc functions[] = {\n");
	for (i = 1; i <= nfuncs; i++) {
	    printf("func%d,\n", i);
	}
	printf("};\n");
    }
}

/*
 * NAME:	dump_funcdefs()
 * DESCRIPTION:	output the function definitions
 */
static void dump_funcdefs(ctrl)
register control *ctrl;
{
    register int i;

    if (ctrl->nfuncdefs != 0) {
	printf("\nstatic dfuncdef funcdefs[] = {\n");
	for (i = 0; i < ctrl->nfuncdefs; i++) {
	    printf("{ %d, %d, %u, %u },\n",
		   UCHAR(ctrl->funcdefs[i].class),
		   UCHAR(ctrl->funcdefs[i].inherit),
		   ctrl->funcdefs[i].index,
		   ctrl->funcdefs[i].offset);
	}
	printf("};\n");
    }
}

/*
 * NAME:	dump_vardefs()
 * DESCRIPTION:	output the variable definitions
 */
static void dump_vardefs(ctrl)
register control *ctrl;
{
    register int i;

    if (ctrl->nvardefs != 0) {
	printf("\nstatic dvardef vardefs[] = {\n");
	for (i = 0; i < ctrl->nvardefs; i++) {
	    printf("{ %d, %d, %u, %u },\n",
		   UCHAR(ctrl->vardefs[i].class),
		   UCHAR(ctrl->vardefs[i].inherit),
		   ctrl->vardefs[i].index,
		   ctrl->vardefs[i].type);
	}
	printf("};\n");
    }
}

/*
 * NAME:	dump_funcalls()
 * DESCRIPTION:	output the function call table
 */
static void dump_funcalls(ctrl)
register control *ctrl;
{
    register unsigned short foffset;

    foffset = ctrl->inherits[ctrl->nvirtuals - 1].funcoffset;
    if (ctrl->nfuncalls != foffset) {
	printf("\nstatic char funcalls[] = {\n");
	dump_chars(ctrl->funcalls + (foffset << 1),
		   (ctrl->nfuncalls - foffset) << 1);
	printf("\n};\n");
    }
}

/*
 * NAME:	main()
 * DESCRIPTION:	main routine of the precompiler
 */
int main(argc, argv)
int argc;
char *argv[];
{
    register int len;
    register control *ctrl;
    char *file, tag[9];
    int nfuncs;

    host_init();

    if (argc != 3) {
	fprintf(stderr, "usage: %s config_file file\n", argv[0]);
	host_finish();
	return 2;
    }

    if (ec_push()) {
	fprintf(stderr, "error in initialization\n");
	host_finish();
	return 2;
    }
    conf_init(argv[1]);
    ec_pop();

    len = strlen(file = path_resolve(argv[2]));
    if (len > 2 && file[len - 2] == '.' && file[len - 1] == 'c') {
	file[len -= 2] = '\0';
    }
    sprintf(tag, "T%03x%04x", hashstr(file, len, 0x1000),
	    (unsigned short) random());

    printf("/*\n * This file was compiled from LPC with the DGD precompiler.");
    printf("\n * DGD is copyright by Felix A. Croes.");
    printf("  See the file \"Copyright\" for details.\n *\n");
    printf(" * File: \"/%s.c\"\n */\n", file);

    printf("\n# ifdef TAG\nTAG(%s)\n# else\n", tag);
    printf("# include \"dgd.h\"\n# include \"str.h\"\n");
    printf("# include \"array.h\"\n# include \"object.h\"\n");
    printf("# include \"interpret.h\"\n# include \"data.h\"\n");
    printf("# include \"csupport.h\"\n");

    if (ec_push()) {
	warning((char *) NULL);
	host_finish();
	return 1;
    }
    ctrl = c_compile(file)->ctrl;
    ec_pop();

    dump_inherits(ctrl);
    dump_program(ctrl);
    dump_strings(ctrl);
    dump_functions(ctrl, nfuncs = cg_nfuncs());
    dump_funcdefs(ctrl);
    dump_vardefs(ctrl);
    dump_funcalls(ctrl);

    printf("\nprecomp %s = {\n%d, %d, inherits,\n", tag,
	   UCHAR(ctrl->ninherits), UCHAR(ctrl->nvirtuals));
    if (ctrl->progsize == 0) {
	printf("0, 0,\n");
    } else {
	printf("%u, program,\n", ctrl->progsize);
    }
    if (ctrl->nstrings == 0) {
	printf("0, 0, 0,\n");
    } else {
	printf("%u, stext, slength,\n", ctrl->nstrings);
    }
    if (nfuncs == 0) {
	printf("0, 0,\n");
    } else {
	printf("%u, functions,\n", nfuncs);
    }
    if (ctrl->nfuncdefs == 0) {
	printf("0, 0,\n");
    } else {
	printf("%u, funcdefs,\n", ctrl->nfuncdefs);
    }
    if (ctrl->nvardefs == 0) {
	printf("0, 0,\n");
    } else {
	printf("%u, vardefs,\n", ctrl->nvardefs);
    }
    nfuncs = ctrl->nfuncalls - ctrl->inherits[ctrl->nvirtuals - 1].funcoffset;
    if (nfuncs == 0) {
	printf("0, 0\n");
    } else {
	printf("%u, funcalls\n", nfuncs);
    }
    printf("};\n# endif\n");

    host_finish();
    return 0;
}


/*
 * "Dummy" routines.
 */

/*
 * NAME:	call_driver_object()
 * DESCRIPTION:	pretend to call a function in the driver object
 */
bool call_driver_object(func, narg)
char *func;
int narg;
{
    i_pop(narg);
    (--sp)->type = T_NUMBER;
    sp->u.number = 0;
    return FALSE;
}

/*
 * NAME:	this_user()
 * DESCRIPTION:	pretend to return the current user object
 */
object *this_user()
{
    return (object *) NULL;
}

pcfunc *pcfunctions;	/* dummy */

/*
 * NAME:	preload()
 * DESCRIPTION:	pretend to preload compiled objects
 */
void preload()
{
}

/*
 * NAME:	swap->init()
 * DESCRIPTION:	pretend to initialize the swap device
 */
void sw_init(file, total, cache, secsize)
char *file;
uindex total, cache, secsize;
{
}

/*
 * NAME:	swap->new()
 * DESCRIPTION:	pretend to return a newly created (empty) swap sector
 */
sector sw_new()
{
    return (sector) 0;
}

/*
 * NAME:	swap->del()
 * DESCRIPTION:	pretend to delete a swap sector
 */
void sw_del(sec)
sector sec;
{
}

/*
 * NAME:	swap->readv()
 * DESCRIPTION:	pretend to read bytes from a vector of sectors
 */
void sw_readv(m, vec, size, idx)
char *m;
sector *vec;
long size, idx;
{
}

/*
 * NAME:	swap->writev()
 * DESCRIPTION:	pretend to write bytes to a vector of sectors
 */
void sw_writev(m, vec, size, idx)
char *m;
sector *vec;
long size, idx;
{
}

/*
 * NAME:	swap->mapsize()
 * DESCRIPTION:	pretend to count the number of sectors required for size bytes
 */
uindex sw_mapsize(size)
long size;
{
    return 0;
}

/*
 * NAME:	swap->count()
 * DESCRIPTION:	pretend to return the number of sectors presently in use
 */
uindex sw_count()
{
    return 0;
}

/*
 * NAME:	comm->init()
 * DESCRIPTION:	pretend to initialize communications
 */
void comm_init(nusers, port_number)
int nusers, port_number;
{
}

/*
 * NAME:	comm->send()
 * DESCRIPTION:	pretend to send a message to a user
 */
void comm_send(obj, str)
object *obj;
string *str;
{
}

/*
 * NAME:	comm->echo()
 * DESCRIPTION:	pretend to turn on/off input echoing for a user
 */
void comm_echo(obj, echo)
object *obj;
bool echo;
{
}

/*
 * NAME:	comm->flush()
 * DESCRIPTION:	pretend to flush output to all users
 */
void comm_flush()
{
}

/*
 * NAME:	comm->ip_number()
 * DESCRIPTION:	pretend to return the ip number of a user (as a string)
 */
string *comm_ip_number(obj)
object *obj;
{
    return (string *) NULL;
}

/*
 * NAME:	comm->close()
 * DESCRIPTION:	pretend to remove a user
 */
void comm_close(obj)
object *obj;
{
}

/*
 * NAME:	comm->users()
 * DESCRIPTION:	pretend to return an array with all user objects
 */
array *comm_users()
{
    return (array *) NULL;
}

/*
 * NAME:	ed->init()
 * DESCRIPTION:	pretend to initialize editor handling
 */
void ed_init(tmp, num)
char *tmp;
int num;
{
}

/*
 * NAME:	ed->new()
 * DESCRIPTION:	pretend to start a new editor
 */
void ed_new(obj)
object *obj;
{
}

/*
 * NAME:	ed->del()
 * DESCRIPTION:	pretend to delete an editor instance
 */
void ed_del(obj)
object *obj;
{
}

/*
 * NAME:	ed->command()
 * DESCRIPTION:	pretend to handle an editor command
 */
void ed_command(obj, cmd)
object *obj;
char *cmd;
{
}

/*
 * NAME:	ed->status()
 * DESCRIPTION:	pretend to return the editor status of an object
 */
char *ed_status(obj)
object *obj;
{
    return (char *) NULL;
}

/*
 * NAME:	call_out->init()
 * DESCRIPTION:	pretend to initialize call_out handling
 */
void co_init(max)
int max;
{
}

/*
 * NAME:	call_out->new()
 * DESCRIPTION:	pretend to add a new call_out
 */
bool co_new(obj, str, delay, args, nargs)
object *obj;
string *str;
long delay;
value *args;
int nargs;
{
    return FALSE;
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	pretend to remove a call_out
 */
long co_del(obj, str)
object *obj;
string *str;
{
    return -1;
}

/*
 * NAME:	call_out->call()
 * DESCRIPTION:	pretend to call expired call_outs
 */
void co_call()
{
}
