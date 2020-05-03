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

struct Inherit {
    uindex oindex;		/* inherited object */
    uindex progoffset;		/* program offset */
    uindex funcoffset;		/* function call offset */
    unsigned short varoffset;	/* variable offset */
    bool priv;			/* privately inherited? */
};

struct FuncDef {
    char sclass;		/* function class */
    char inherit;		/* function name inherit index */
    unsigned short index;	/* function name index */
    Uint offset;		/* offset in program text */
};

# define DF_LAYOUT	"ccsi"

struct VarDef {
    char sclass;		/* variable class */
    char type;			/* variable type */
    char inherit;		/* variable name inherit index */
    unsigned short index;	/* variable name index */
};

# define DV_LAYOUT	"cccs"

struct Symbol {
    char inherit;		/* function object index */
    char index;			/* function index */
    unsigned short next;	/* next in hash table */
};

# define DSYM_LAYOUT	"ccs"

class Control : public Allocated {
public:
    void ref();
    void deref();
    void del();

    unsigned short *varmap(Control *octrl);
    void setVarmap(unsigned short *vmap);
    char *program();
    String *strconst(int inherit, Uint idx);
    FuncDef *funcs();
    VarDef *vars();
    char *funCalls();
    char *varTypes();
    Uint progSize();
    Symbol *symb(const char *func, unsigned int len);
    Array *undefined(Dataspace *data);

    static void prepare();
    static bool inherit(Frame *f, char *from, Object *obj, String *label,
			int priv);
    static void create();
    static long defString(String *str);
    static void defProto(String *str, char *proto, String *sclass);
    static void defFunc(String *str, char *proto, String *sclass);
    static void defProgram(char *prog, unsigned int size);
    static void defVar(String *str, unsigned int sclass, unsigned int type,
		       String *cvstr);
    static char *iFunCall(String *str, const char *label, String **cfstr,
			  long *call);
    static char *funCall(String *str, String **cfstr, long *call,
			 int typechecking);
    static unsigned short genCall(long call);
    static unsigned short var(String *str, long *ref, String **cvstr);
    static int nInherits();
    static bool checkFuncs();
    static Control *construct();
    static void clear();

    static Control *load(Object *obj, Uint instance);
    static Control *restore(Object *obj, Uint instance,
			    void(*)(char*, Sector*, Uint, Uint));
    static void init();
    static void initConv(bool c14, bool c15, bool c16);
    static void converted();
    static void swapout(unsigned int frag);

    uindex ndata;		/* # of data blocks using this control block */

    Sector nsectors;		/* o # of sectors */
    Sector *sectors;		/* o vector with sectors */

    uindex oindex;		/* i object */
    Uint instance;		/* i instance */

    char flags;			/* various bitflags */
    char version;		/* program version */

    short ninherits;		/* i/o # inherited objects */
    Inherit *inherits;		/* i/o inherit objects */

    uindex imapsz;		/* i/o inherit map size */
    char *imap;			/* i/o inherit map */

    Uint compiled;		/* time of compilation */

    char *prog;			/* i program text */
    Uint progsize;		/* i/o program text size */

    unsigned short nstrings;	/* i/o # strings */
    String **strings;		/* i/o? string table */
    char *stext;		/* o sstrings text */
    Uint strsize;		/* o sstrings text size */

    unsigned short nfuncdefs;	/* i/o # function definitions */
    FuncDef *funcdefs;		/* i/o? function definition table */

    unsigned short nvardefs;	/* i/o # variable definitions */
    unsigned short nclassvars;	/* i/o # class variable definitions */
    VarDef *vardefs;		/* i/o? variable definitions */

    uindex nfuncalls;		/* i/o # function calls */
    char *funcalls;		/* i/o? function calls */

    unsigned short nsymbols;	/* i/o # symbols */
    Symbol *symbols;		/* i/o? symbol table */

    unsigned short nvariables;	/* i/o # variables */

    unsigned short vmapsize;	/* i/o size of variable mapping */
    unsigned short *vmap;	/* variable mapping */

private:
    Control();
    virtual ~Control();

    void inheritVars(class ObjHash *ohash);
    bool compareClass(Uint s1, Control *ctrl, Uint s2);
    bool compareProto(char *prot1, Control *ctrl, char *prot2);
    void inheritFunc(int idx, class ObjHash *ohash);
    void inheritFuncs(class ObjHash *ohash);
    void inheritMap();
    void loadProgram(void (*readv) (char*, Sector*, Uint, Uint));
    void loadStext(void (*readv) (char*, Sector*, Uint, Uint));
    void loadStrconsts(void (*readv) (char*, Sector*, Uint, Uint));
    void loadFuncdefs(void (*readv) (char*, Sector*, Uint, Uint));
    void loadVardefs(void (*readv) (char*, Sector*, Uint, Uint));
    void loadFuncalls(void (*readv) (char*, Sector*, Uint, Uint));
    void loadSymbols(void (*readv) (char*, Sector*, Uint, Uint));
    void loadVtypes(void (*readv) (char*, Sector*, Uint, Uint));
    Symbol *symbs();
    void save();

    static void makeStrings();
    static void makeFuncs();
    static void makeVars();
    static void makeFunCalls();
    static void makeSymbols();
    static void makeVarTypes();
    static Control *load(Object *obj, Uint instance,
			 void (*readv) (char*, Sector*, Uint, Uint));
    static Control *conv(Object *obj, Uint instance,
			 void (*readv) (char*, Sector*, Uint, Uint));

    Control *prev, *next;

    ssizet *sslength;		/* o sstrings length */
    Uint *ssindex;		/* o sstrings index */

    String **cvstrings;		/* variable class strings */
    char *classvars;		/* variable classes */

    char *vtypes;		/* i/o? variable types */

    Uint progoffset;		/* o program text offset */
    Uint stroffset;		/* o offset of string index table */
    Uint funcdoffset;		/* o offset of function definition table */
    Uint vardoffset;		/* o offset of variable definition table */
    Uint funccoffset;		/* o offset of function call table */
    Uint symboffset;		/* o offset of symbol table */
    Uint vtypeoffset;		/* o offset of variable types */
};

# define NEW_INT		((unsigned short) -1)
# define NEW_FLOAT		((unsigned short) -2)
# define NEW_POINTER		((unsigned short) -3)
# define NEW_VAR(x)		((x) >= NEW_POINTER)

/* bit values for ctrl->flags */
# define CTRL_PROGCMP		0x003	/* program compressed */
# define CTRL_STRCMP		0x00c	/* strings compressed */
# define CTRL_UNDEFINED		0x010	/* has undefined functions */
# define CTRL_VARMAP		0x020	/* varmap updated */

/* data compression */
# define CMP_TYPE		0x03
# define CMP_NONE		0x00	/* no compression */
# define CMP_PRED		0x01	/* predictor compression */

# define PROTO_CLASS(prot)	((prot)[0])
# define PROTO_NARGS(prot)	((prot)[1])
# define PROTO_VARGS(prot)	((prot)[2])
# define PROTO_HSIZE(prot)	((prot)[3])
# define PROTO_LSIZE(prot)	((prot)[4])
# define PROTO_SIZE(prot)	((PROTO_HSIZE(prot) << 8) | PROTO_LSIZE(prot))
# define PROTO_FTYPE(prot)	((prot)[5])
# define PROTO_ARGS(prot)	((prot) +				      \
				 (((PROTO_FTYPE(prot) & T_TYPE) == T_CLASS) ? \
				   9 : 6))

# define T_IMPLICIT		(T_VOID | (1 << REFSHIFT))

# define KFCALL			0
# define KFCALL_LVAL		1
# define DFCALL			2
# define FCALL			4
