/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2012 DGD Authors (see the commit log for details)
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
static void dump_int(int n)
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
static void dump_chars(char *p, unsigned int n)
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
static void dump_inherits(control *ctrl)
{
    int i;

    printf("\nstatic pcinherit inherits[] = {\n");
    for (i = 0; i < ctrl->ninherits; i++) {
	printf("{ \"%s\", %u, %u, %u, %d },\n",
	       OBJ(ctrl->inherits[i].oindex)->chain.name,
	       ctrl->inherits[i].progoffset,
	       ctrl->inherits[i].funcoffset,
	       ctrl->inherits[i].varoffset,
	       ctrl->inherits[i].priv);
    }
    printf("};\n");
}

/*
 * NAME:	dump_imap()
 * DESCRIPTION:	output imap table
 */
static void dump_imap(control *ctrl)
{
    int i;

    printf("\nstatic char imap[] = {\n");
    size = 0;
    for (i = 0; i < ctrl->imapsz; i++) {
	dump_int(ctrl->imap[i]);
    }
    printf("\n};\n");
}

/*
 * NAME:	dump_program()
 * DESCRIPTION:	output the program
 */
static void dump_program(control *ctrl)
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
static void dump_strings(control *ctrl)
{
    int i;
    long len;

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
static void dump_funcdefs(control *ctrl)
{
    int i;

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
static void dump_vardefs(control *ctrl)
{
    int i;

    if (ctrl->nvardefs != 0) {
	printf("\nstatic dvardef vardefs[] = {\n");
	for (i = 0; i < ctrl->nvardefs; i++) {
	    printf("{ %d, %d, %d, %u },\n",
		   ctrl->vardefs[i].class,
		   ctrl->vardefs[i].type,
		   ctrl->vardefs[i].inherit,
		   ctrl->vardefs[i].index);
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
static void dump_funcalls(control *ctrl)
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
static void dump_symbols(control *ctrl)
{
    uindex i;

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
 * NAME:	dump_vtypes()
 * DESCRIPTION:	output the variable types
 */
static void dump_vtypes(control *ctrl)
{
    if (ctrl->nvariables > ctrl->nvardefs) {
	printf("\nstatic char vtypes[] = {\n");
	size = 0;
	dump_chars(ctrl->vtypes, ctrl->nvariables - ctrl->nvardefs);
	printf("\n};\n");
    }
}

/*
 * NAME:	dgd_main()
 * DESCRIPTION:	main routine of the precompiler
 */
int dgd_main(int argc, char *argv[])
{
    char buf[STRINGSZ], tag[14];
    unsigned int len;
    control *ctrl;
    char *program, *module, *file;
    int nfuncs;
    sector fragment;

    --argc;
    program = *argv++;
    module = (char *) NULL;
    if (argc > 1 && argv[0][0] == '-' && argv[0][1] == 'e') {
	if (argv[0][2] == '\0') {
	    --argc;
	    argv++;
	    module = argv[0];
	} else {
	    module = argv[0] + 2;
	}
	--argc;
	argv++;
    }
    file = argv[1];
    if ((argc != 2 && argc != 3) ||
	(len=strlen(file)) < 2 || file[len - 2] != '.' || file[len - 1] != 'c')
    {
	message("usage: %s [-e module] config_file lpc_file [c_file]\012",/*LF*/
		program);
	return 2;
    }

    /* open output file */
    if (argc == 3 && freopen(argv[2], "w", stdout) == (FILE *) NULL) {
	P_message("cannot open output file\012");	/* LF */
	return 2;
    }

    /* initialize */
    if (!conf_init(argv[0], (char *) NULL, (char *) NULL, module, &fragment)) {
	P_message("Initialization failed\012");	/* LF */
	return 2;
    }

    len = strlen(file = path_resolve(buf, file));
    file[len - 2] = '\0';
    sprintf(tag, "T%04x%08lx", hashstr(file, len), P_random());

    printf("/*\n * This file was compiled from LPC with the DGD precompiler.");
    printf("\n *\n * Original file: \"/%s.c\"\n */\n", file);

    printf("\n# ifdef TAG\nTAG(%s)\n# else\n", tag);
    printf("# include \"lpc_ext.h\"\n");

    if (ec_push((ec_ftn) NULL)) {
	message("Failed to compile \"%s.c\"\012", file);	/* LF */
	printf("\n# error Error while compiling\n");
	fclose(stdout);
	if (argc == 3) {
	    /* remove output file: may fail if path is not absolute */
	    unlink(argv[2]);
	}
	return 1;
    }

    /* compile file */
    ctrl = c_compile(cframe, file, (object *) NULL, (string **) NULL, 0,
		     FALSE)->ctrl;
    nfuncs = cg_nfuncs();
    ec_pop();

    /* dump tables */
    dump_inherits(ctrl);
    dump_imap(ctrl);
    dump_program(ctrl);
    dump_strings(ctrl);
    dump_funcdefs(ctrl);
    dump_vardefs(ctrl);
    dump_funcalls(ctrl);
    dump_symbols(ctrl);
    dump_vtypes(ctrl);

    printf("\nprecomp %s = {\nUINDEX_MAX,\n%d, inherits,\n", tag,
	   ctrl->ninherits);
    printf("%u, imap,\n", ctrl->imapsz);
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
    printf("%u,\n", ctrl->nvariables);
    if (ctrl->nvariables > ctrl->nvardefs) {
	printf("vtypes,\n");
    } else {
	printf("0,\n");
    }
    printf("%d\n", conf_typechecking());
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
bool call_driver_object(frame *f, char *func, int narg)
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
void errhandler(frame *f, Int depth)
{
}

/*
 * NAME:	interrupt()
 * DESCRIPTION:	pretend to an interrupt
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
bool pc_preload(char *auto_name, char *driver_name)
{
    return TRUE;
}

/*
 * NAME:	pc_list()
 * DESCRIPTION:	pretend to return a list of precompiled objects
 */
array *pc_list(dataspace *data)
{
    return (array *) NULL;
}

/*
 * NAME:	pc_control()
 * DESCRIPTION:	pretend to initialize the control block of a compiled object
 */
void pc_control(control *ctrl, object *obj)
{
}

bool pc_dump(int fd)
{
    return TRUE;
}

void pc_restore(int fd, int conv)
{
}

# endif	/* PRECOMPILER_PRECOMPILED */

/*
 * NAME:	swap->init()
 * DESCRIPTION:	pretend to initialize the swap device
 */
bool sw_init(char *file, unsigned int total, unsigned int cache, unsigned int secsize)
{
    return TRUE;
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
void sw_newv(sector *vec, unsigned int size)
{
}

/*
 * NAME:	swap->wipev()
 * DESCRIPTION:	pretend to wipe a vector of sectors
 */
void sw_wipev(sector *vec, unsigned int size)
{
}

/*
 * NAME:	swap->delv()
 * DESCRIPTION:	pretend to delete a vector of sectors
 */
void sw_delv(sector *vec, unsigned int size)
{
}

/*
 * NAME:	swap->readv()
 * DESCRIPTION:	pretend to read bytes from a vector of sectors
 */
void sw_readv(char *m, sector *vec, Uint size, Uint idx)
{
}

/*
 * NAME:	swap->writev()
 * DESCRIPTION:	pretend to write bytes to a vector of sectors
 */
void sw_writev(char *m, sector *vec, Uint size, Uint idx)
{
}

/*
 * NAME:	swap->creadv()
 * DESCRIPTION:	pretend to read bytes from a vector of sectors in the snapshot
 */
void sw_creadv(char *m, sector *vec, Uint size, Uint idx)
{
}

/*
 * NAME:	swap->dreadv()
 * DESCRIPTION:	pretend to read bytes from a vector of sectors in the snapshot
 */
void sw_dreadv(char *m, sector *vec, Uint size, Uint idx)
{
}

/*
 * NAME:	swap->conv()
 * DESCRIPTION:	pretend to read bytes from a vector of sectors in the snapshot
 */
void sw_conv(char *m, sector *vec, Uint size, Uint idx)
{
}

/*
 * NAME:	swap->conv2()
 * DESCRIPTION:	pretend to restore bytes from a vector of sectors in the secondary snapshot
 */
void sw_conv2(char *m, sector *vec, Uint size, Uint idx)
{
}

/*
 * NAME:	swap->mapsize()
 * DESCRIPTION:	pretend to count the number of sectors required for size bytes
 */
sector sw_mapsize(Uint size)
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
 * DESCRIPTION:	pretend to copy a vector of sectors to a snapshot
 */
bool sw_copy(Uint time)
{
    return FALSE;
}

/*
 * NAME:	swap->dump()
 * DESCRIPTION:	pretend to create snapshot
 */
int sw_dump(char *snapshot, bool keep)
{
    return 0;
}

/*
 * NAME:	swap->dump2()
 * DESCRIPTION:	pretend to finish snapshot
 */
void sw_dump2(char *header, int size, bool incr)
{
}

/*
 * NAME:	swap->restore()
 * DESCRIPTION:	pretend to restore swap file
 */
void sw_restore(int fd, unsigned int secsize)
{
}

/*
 * NAME:	swap->restore2()
 * DESCRIPTION:	pretend to restore secondary snapshot
 */
void sw_restore2(int fd)
{
}

/*
 * NAME:	comm->init()
 * DESCRIPTION:	pretend to initialize communications
 */
#ifdef NETWORK_EXTENSIONS
bool comm_init(int n, int p, char **thosts, char **bhosts,
	unsigned short *tports, unsigned short *bports,
	int ntelnet, int nbinary)
#else
bool comm_init(int n, char **thosts, char **bhosts,
	unsigned short *tports, unsigned short *bports,
	int ntelnet, int nbinary)
#endif
{
    return TRUE;
}

/*
 * NAME:        comm->clear()
 * DESCRIPTION: pretend to clean up connections
 */
void comm_clear()
{
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
void comm_challenge(object *obj, string *str)
{
}

/*
 * NAME:	comm->listen()
 * DESCRIPTION: pretend to start listening on telnet port and binary port
 */
void comm_listen()
{
}

/*
 * NAME:	comm->send()
 * DESCRIPTION:	pretend to send a message to a user
 */
int comm_send(object *obj, string *str)
{
    return 0;
}

/*
 * NAME:	comm->udpsend()
 * DESCRIPTION:	pretend to send a message on the UDP channel of a connection
 */
int comm_udpsend(object *obj, string *str)
{
    return 0;
}

/*
 * NAME:	comm->echo()
 * DESCRIPTION:	pretend to turn on/off input echoing for a user
 */
bool comm_echo(object *obj, int echo)
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
void comm_block(object *obj, int flag)
{
}

/*
 * NAME:	comm->ip_number()
 * DESCRIPTION:	pretend to return the ip number of a user (as a string)
 */
string *comm_ip_number(object *obj)
{
    return (string *) NULL;
}

/*
 * NAME:	comm->ip_name()
 * DESCRIPTION:	pretend to return the ip name of a user
 */
string *comm_ip_name(object *obj)
{
    return (string *) NULL;
}

/*
 * NAME:	comm->close()
 * DESCRIPTION:	pretend to remove a user
 */
void comm_close(frame *f, object *obj)
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
array *comm_users(dataspace *data)
{
    return (array *) NULL;
}

#ifdef NETWORK_EXTENSIONS
array *comm_ports(dataspace *data)
{
    return (array *) NULL;
}

void comm_openport(frame *f, object *obj, unsigned char protocol,
	unsigned short port)
{
}

int comm_senddatagram(object *obj, string *str, string *ip, int port)
{
    return 0;
}
#endif

/*
 * NAME:	comm->connect()
 * DESCRIPTION:	pretend to establish an outbound connection
 */
void comm_connect(frame *f, object *obj, char *addr, unsigned char protocol,
	unsigned short port)
{
}

/*
 * NAME:	comm->is_connection()
 * DESCRIPTION: pretend to test if an object is a connection
 */
bool comm_is_connection(object *obj)
{
    return FALSE;
}

/*
 * NAME:        comm->dump()
 * DESCRIPTION: pretend to save users
 */
bool comm_dump(int fd)
{
    return TRUE;
}

/*
 * NAME:        comm->restore()
 * DESCRIPTION: pretend to restore users
 */
bool comm_restore(int fd)
{
    return TRUE;
}

/*
 * NAME:	ed->init()
 * DESCRIPTION:	pretend to initialize editor handling
 */
void ed_init(char *tmp, int num)
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
void ed_new(object *obj)
{
}

/*
 * NAME:	ed->del()
 * DESCRIPTION:	pretend to delete an editor instance
 */
void ed_del(object *obj)
{
}

/*
 * NAME:	ed->command()
 * DESCRIPTION:	pretend to handle an editor command
 */
string *ed_command(object *obj, char *cmd)
{
    return (string *) NULL;
}

/*
 * NAME:	ed->status()
 * DESCRIPTION:	pretend to return the editor status of an object
 */
char *ed_status(object *obj)
{
    return (char *) NULL;
}

/*
 * NAME:	call_out->init()
 * DESCRIPTION:	pretend to initialize callout handling
 */
bool co_init(unsigned int max)
{
    return TRUE;
}

/*
 * NAME:	call_out->check()
 * DESCRIPTION:	pretend to check a new callout
 */
Uint co_check(unsigned int n, Int delay, unsigned int mdelay,
	Uint *tp, unsigned short *mp, uindex **qp)
{
    return 0;
}

/*
 * NAME:	call_out->new()
 * DESCRIPTION:	pretend to add a new callout
 */
void co_new(unsigned int oindex, unsigned int handle, Uint t,
	unsigned int m, uindex *q)
{
}

/*
 * NAME:	call_out->del()
 * DESCRIPTION:	pretend to remove a callout
 */
void co_del(unsigned int oindex, unsigned int handle, Uint t,
	unsigned int m)
{
}

/*
 * NAME:	call_out->remaining()
 * DESCRIPTION:	pretend to return the time remaining before a callout expires
 */
Int co_remaining(Uint t, unsigned short *m)
{
    return 0;
}

/*
 * NAME:	call_out->list()
 * DESCRIPTION:	pretend to adjust callout delays in an array
 */
void co_list(array *a)
{
}

/*
 * NAME:	call_out->call()
 * DESCRIPTION:	pretend to call expired callouts
 */
void co_call(frame *f)
{
}

/*
 * NAME:	call_out->info()
 * DESCRIPTION:	pretend to return information about callouts
 */
void co_info(uindex *n1, uindex *n2)
{
}

/*
 * NAME:	call_out->decode()
 * DESCRIPTION:	pretend to decode a callout time
 */
Uint co_decode(Uint time, unsigned short *mtime)
{
    return 0;
}

/*
 * NAME:	call_out->time()
 * DESCRIPTION:	pretend to return the current time
 */
Uint co_time(unsigned short *mtime)
{
    return 0;
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
bool co_dump(int fd)
{
    return FALSE;
}

/*
 * NAME:	call_out->restore()
 * DESCRIPTION:	pretend to restore callout table
 */
void co_restore(int fd, Uint t, int conv, int conv2, int conv_time)
{
}
