# define INCLUDE_FILE_IO
# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "path.h"
# include "hash.h"
# include "swap.h"
# include "comm.h"
# include "editor.h"
# include "call_out.h"
# include "node.h"
# include "codegen.h"
# include "compile.h"
# include "csupport.h"

static int size;		/* current size of the dumped line */

/*
 * NAME:	dump_int()
 * DESCRIPTION:	output a number
 */
static void dump_int(n)
int n;
{
    if (size == 16) {
	putchar('\n');
	size = 0;
    }
    printf("%d, ", n);
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
	dump_int(*p++);
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

    printf("\nstatic pcinherit inherits[] = {\n");
    for (i = 0; i < ctrl->ninherits; i++) {
	printf("{ \"%s\", %u, %u, %d },\n",
	       OBJ(ctrl->inherits[i].oindex)->chain.name,
	       ctrl->inherits[i].funcoffset,
	       ctrl->inherits[i].varoffset,
	       ctrl->inherits[i].priv);
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
    register long len;

    if (ctrl->nstrings != 0) {
	printf("\nstatic dstrconst sstrings[] = {\n");
	len = 0;
	for (i = 0; i < ctrl->nstrings; i++) {
	    printf("{ %ld, %u },\n", len, ctrl->strings[i]->len);
	    len += ctrl->strings[i]->len;
	}
	printf("};\n\nstatic char stext[] = {\n");
	size = 0;
	for (i = 0; i < ctrl->nstrings; i++) {
	    dump_chars(ctrl->strings[i]->text, ctrl->strings[i]->len);
	}
	printf("\n};\n");
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
		   ctrl->funcdefs[i].class,
		   ctrl->funcdefs[i].inherit,
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
		   ctrl->vardefs[i].class,
		   ctrl->vardefs[i].inherit,
		   ctrl->vardefs[i].index,
		   ctrl->vardefs[i].type);
	}
	printf("};\n");
	if (ctrl->nclassvars != 0) {
	    printf("\nstatic char classvars[] = {\n");
	    size = 0;
	    dump_chars(ctrl->classvars, ctrl->nclassvars * 3);
	    printf("\n};\n");
	}
    }
}

/*
 * NAME:	dump_funcalls()
 * DESCRIPTION:	output the function call table
 */
static void dump_funcalls(ctrl)
register control *ctrl;
{
    if (ctrl->nfuncalls > 0) {
	printf("\nstatic char funcalls[] = {\n");
	size = 0;
	dump_chars(ctrl->funcalls, ctrl->nfuncalls << 1);
	printf("\n};\n");
    }
}

/*
 * NAME:	dump_symbols()
 * DESCRIPTION:	output the symbol table
 */
static void dump_symbols(ctrl)
register control *ctrl;
{
    register uindex i;

    if (ctrl->nsymbols != 0) {
	printf("\nstatic dsymbol symbols[] = {\n");
	for (i = 0; i < ctrl->nsymbols; i++) {
	    printf("{ %d, %d, %u },\n",
		   ctrl->symbols[i].inherit,
		   ctrl->symbols[i].index,
		   ctrl->symbols[i].next);
	}
	printf("};\n");
    }
}

/*
 * NAME:	dgd_main()
 * DESCRIPTION:	main routine of the precompiler
 */
int dgd_main(argc, argv)
int argc;
char *argv[];
{
    char buf[STRINGSZ], tag[9];
    register unsigned int len;
    register control *ctrl;
    register char *file;
    int nfuncs;
    sector fragment;

    file = argv[2];
    if ((argc != 3 && argc != 4) ||
	(len=strlen(file)) < 2 || file[len - 2] != '.' || file[len - 1] != 'c')
    {
	P_message("usage: precomp config_file lpc_file [c_file]\012");	/* LF */
	return 2;
    }

    /* open output file */
    if (argc == 4 && freopen(argv[3], "w", stdout) == (FILE *) NULL) {
	P_message("cannot open output file\012");	/* LF */
	return 2;
    }

    /* initialize */
    if (!conf_init(argv[1], (char *) NULL, &fragment)) {
	P_message("Initialization failed\012");	/* LF */
	return 2;
    }

    len = strlen(file = path_resolve(buf, file));
    file[len - 2] = '\0';
    sprintf(tag, "T%03x%04x", hashstr(file, len) & 0xfff,
	    (unsigned short) P_random());

    printf("/*\n * This file was compiled from LPC with the DGD precompiler.");
    printf("\n * DGD is copyright by BeeHive Internet Technologies, Inc.\n");
    printf(" * See the file \"Copyright\" for details.\n *\n");
    printf(" * File: \"/%s.c\"\n */\n", file);

    printf("\n# ifdef TAG\nTAG(%s)\n# else\n", tag);
    printf("# include \"dgd.h\"\n# include \"str.h\"\n");
    printf("# include \"array.h\"\n# include \"object.h\"\n");
    printf("# include \"interpret.h\"\n# include \"data.h\"\n");
    printf("# include \"xfloat.h\"\n# include \"csupport.h\"\n");

    if (ec_push((ec_ftn) NULL)) {
	message("Failed to compile \"%s.c\"\012", file);	/* LF */
	printf("\n# error Error while compiling\n");
	fclose(stdout);
	if (argc == 4) {
	    /* remove output file: may fail if path is not absolute */
	    unlink(argv[3]);
	}
	return 1;
    }

    /* compile file */
    ctrl = c_compile(cframe, file, (object *) NULL, (string *) NULL,
		     FALSE)->ctrl;
    nfuncs = cg_nfuncs();
    ec_pop();

    /* dump tables */
    dump_inherits(ctrl);
    dump_program(ctrl);
    dump_strings(ctrl);
    dump_funcdefs(ctrl);
    dump_vardefs(ctrl);
    dump_funcalls(ctrl);
    dump_symbols(ctrl);

    printf("\nprecomp %s = {\nUINDEX_MAX,\n%d, inherits,\n", tag,
	   ctrl->ninherits);
    printf("%ldL,\n", (long) ctrl->compiled);
    if (ctrl->progsize == 0) {
	printf("0, 0,\n");
    } else {
	printf("%u, program,\n", ctrl->progsize);
    }
    if (ctrl->nstrings == 0) {
	printf("0, 0, 0, 0,\n");
    } else {
	printf("%u, sstrings, stext, %luL,\n", ctrl->nstrings,
	       (unsigned long) ctrl->strsize);
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
	printf("0, 0, 0, 0,\n");
    } else {
	printf("%u, %u, vardefs, ", ctrl->nvardefs, ctrl->nclassvars);
	if (ctrl->nclassvars == 0) {
	    printf("0,\n");
	} else {
	    printf("classvars,\n");
	}
    }
    if (ctrl->nfuncalls == 0) {
	printf("0, 0,\n");
    } else {
	printf("%u, funcalls,\n", ctrl->nfuncalls);
    }
    if (ctrl->nsymbols == 0) {
	printf("0, 0,\n");
    } else {
	printf("%u, symbols,\n", ctrl->nsymbols);
    }
    printf("%u, %u, %u\n", ctrl->nvariables, ctrl->nifdefs, ctrl->nvinit);
    printf("};\n# endif\n");

    return 0;
}


/*
 * "Dummy" routines.
 */

/*
 * NAME:	call_driver_object()
 * DESCRIPTION:	pretend to call a function in the driver object
 */
bool call_driver_object(f, func, narg)
register frame *f;
char *func;
int narg;
{
    i_pop(f, narg);
    (--f->sp)->type = T_INT;
    f->sp->u.number = 0;
    return FALSE;
}

/*
 * NAME:	errhandler()
 * DESCRIPTION:	pretend to have a default error handler
 */
void errhandler(f, depth)
frame *f;
Int depth;
{
}

/*
 * NAME:	interrupt()
 * DESCRIPTION:	pretend to register an interrupt
 */
void interrupt()
{
}

/*
 * NAME:	endthread()
 * DESCRIPTION:	pretend to clean up after a thread has terminated
 */
void endthread()
{
}

# ifndef PRECOMPILER_PRECOMPILED

pcfunc *pcfunctions;	/* dummy */

/*
 * NAME:	pc_preload()
 * DESCRIPTION:	pretend to preload compiled objects
 */
bool pc_preload(auto_name, driver_name)
char *auto_name, *driver_name;
{
    return TRUE;
}

/*
 * NAME:	pc_list()
 * DESCRIPTION:	pretend to return a list of precompiled objects
 */
array *pc_list(data)
dataspace *data;
{
    return (array *) NULL;
}

/*
 * NAME:	pc_control()
 * DESCRIPTION:	pretend to initialize the control block of a compiled object
 */
void pc_control(ctrl, obj)
control *ctrl;
object *obj;
{
}

bool pc_dump(fd)
int fd;
{
    return TRUE;
}

void pc_restore(fd)
int fd;
{
}

# endif	/* PRECOMPILER_PRECOMPILED */

/*
 * NAME:	swap->init()
 * DESCRIPTION:	pretend to initialize the swap device
 */
void sw_init(file, total, cache, secsize, interval)
char *file;
unsigned int total, cache, secsize;
Uint interval;
{
}

/*
 * NAME:	swap->finish()
 * DESCRIPTION:	pretend to finish swapping
 */
void sw_finish()
{
}

/*
 * NAME:	swap->newv()
 * DESCRIPTION:	pretend to create a new vector of sectors
 */
void sw_newv(vec, size)
sector *vec;
unsigned int size;
{
}

/*
 * NAME:	swap->wipev()
 * DESCRIPTION:	pretend to wipe a vector of sectors
 */
void sw_wipev(vec, size)
sector *vec;
unsigned int size;
{
}

/*
 * NAME:	swap->delv()
 * DESCRIPTION:	pretend to delete a vector of sectors
 */
void sw_delv(vec, size)
sector *vec;
unsigned int size;
{
}

/*
 * NAME:	swap->readv()
 * DESCRIPTION:	pretend to read bytes from a vector of sectors
 */
void sw_readv(m, vec, size, idx)
char *m;
sector *vec;
Uint size, idx;
{
}

/*
 * NAME:	swap->writev()
 * DESCRIPTION:	pretend to write bytes to a vector of sectors
 */
void sw_writev(m, vec, size, idx)
char *m;
sector *vec;
Uint size, idx;
{
}

/*
 * NAME:	swap->dreadv()
 * DESCRIPTION:	pretend to read bytes from a vector of sectors in the dump file
 */
void sw_dreadv(m, vec, size, idx)
char *m;
sector *vec;
Uint size, idx;
{
}

/*
 * NAME:	swap->mapsize()
 * DESCRIPTION:	pretend to count the number of sectors required for size bytes
 */
sector sw_mapsize(size)
Uint size;
{
    return 0;
}

/*
 * NAME:	swap->count()
 * DESCRIPTION:	pretend to return the number of sectors presently in use
 */
sector sw_count()
{
    return 0;
}

/*
 * NAME:	swap->copy()
 * DESCRIPTION:	pretend to copy a vector of sectors to a dump file
 */
bool sw_copy(time)
Uint time;
{
    return FALSE;
}

/*
 * NAME:	swap->dump()
 * DESCRIPTION:	pretend to dump swap file
 */
int sw_dump(dumpfile)
char *dumpfile;
{
    return 0;
}

/*
 * NAME:	swap->restore()
 * DESCRIPTION:	pretend to restore swap file
 */
void sw_restore(fd, secsize)
int fd;
unsigned int secsize;
{
}

/*
 * NAME:	comm->init()
 * DESCRIPTION:	pretend to initialize communications
 */
bool comm_init(n, thosts, bhosts, tports, bports, ntelnet, nbinary)
int n, ntelnet, nbinary;
char **thosts, **bhosts;
unsigned short *tports, *bports;
{
    return TRUE;
}

/*
 * NAME:	comm->finish()
 * DESCRIPTION:	pretend to terminate connections
 */
void comm_finish()
{
}


/*
 * NAME:	comm->finish()
 * DESCRIPTION:	pretend to set the datagram challenge
 */
void comm_challenge(obj, str)
object *obj;
string *str;
{  
}

/*
 * NAME:        comm->listen()
 * DESCRIPTION: pretend to start listening on telnet port and binary port
 */
void comm_listen()
{
}

/*
 * NAME:	comm->send()
 * DESCRIPTION:	pretend to send a message to a user
 */
int comm_send(obj, str)
object *obj;
string *str;
{
    return 0;
}

/*
 * NAME:	comm->udpsend()
 * DESCRIPTION:	pretend to send a message on the UDP channel of a connection
 */
int comm_udpsend(obj, str)
object *obj;
string *str;
{
    return 0;
}

/*
 * NAME:	comm->echo()
 * DESCRIPTION:	pretend to turn on/off input echoing for a user
 */
bool comm_echo(obj, echo)
object *obj;
int echo;
{
    return FALSE;
}

/*
 * NAME:	comm->flush()
 * DESCRIPTION:	pretend to flush output to all users
 */
void comm_flush()
{
}

/*
 * NAME:	comm->block()
 * DESCRIPTION:	pretend to suspend or release input from a user
 */
void comm_block(obj, flag)
object *obj;
int flag;
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
 * NAME:	comm->ip_name()
 * DESCRIPTION:	pretend to return the ip name of a user
 */
string *comm_ip_name(obj)
object *obj;
{
    return (string *) NULL;
}

/*
 * NAME:	comm->close()
 * DESCRIPTION:	pretend to remove a user
 */
void comm_close(f, obj)
frame *f;
object *obj;
{
}

/*
 * NAME:	comm->user()
 * DESCRIPTION:	pretend to return the current user
 */
object *comm_user()
{
    return (object *) NULL;
}

/*
 * NAME:	comm->users()
 * DESCRIPTION:	pretend to return an array with all user objects
 */
array *comm_users(data)
dataspace *data;
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
 * NAME:	ed->finish()
 * DESCRIPTION:	pretend to terminate all editor sessions
 */
void ed_finish()
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
string *ed_command(obj, cmd)
object *obj;
char *cmd;
{
    return (string *) NULL;
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
 * DESCRIPTION:	pretend to initialize callout handling
 */
bool co_init(max)
unsigned int max;
{
    return TRUE;
}

/*
 * NAME:	call_out->check()
 * DESCRIPTION:	pretend to check a new callout
 */
Uint co_check(n, delay, mdelay, tp, mp, qp)
unsigned int n, mdelay;
Int delay;
Uint *tp;
unsigned short *mp;
cbuf **qp;
{
    return 0;
}

/*
 * NAME:	call_out->new()
 * DESCRIPTION:	pretend to add a new callout
 */
void co_new(oindex, handle, t, m, q)
unsigned int oindex, handle, m;
Uint t;
cbuf *q;
{
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	pretend to remove a callout
 */
void co_del(oindex, handle, t)
unsigned int oindex, handle;
Uint t;
{
}

/*
 * NAME:	call_out->remaining()
 * DESCRIPTION:	pretend to return the time remaining before a callout expires
 */
Int co_remaining(t)
Uint t;
{
    return 0;
}

/*
 * NAME:	call_out->list()
 * DESCRIPTION:	pretend to adjust callout delays in an array
 */
void co_list(a)
array *a;
{
}

/*
 * NAME:	call_out->call()
 * DESCRIPTION:	pretend to call expired callouts
 */
void co_call(f)
frame *f;
{
}

/*
 * NAME:	call_out->info()
 * DESCRIPTION:	pretend to return information about callouts
 */
void co_info(n1, n2)
uindex *n1, *n2;
{
}

/*
 * NAME:	call_out->swaprate1()
 * DESCRIPTION:	pretend to return the number of objects swapped out per minute
 */
long co_swaprate1()
{
    return 0;
}

/*
 * NAME:	call_out->swaprate5()
 * DESCRIPTION:	pretend to return the number of objects swapped out per 5 mins
 */
long co_swaprate5()
{
    return 0;
}

/*
 * NAME:	call_out->dump()
 * DESCRIPTION:	pretend to dump callout table
 */
bool co_dump(fd)
int fd;
{
    return FALSE;
}

/*
 * NAME:	call_out->restore()
 * DESCRIPTION:	pretend to restore callout table
 */
void co_restore(fd, t)
int fd;
Uint t;
{
}
