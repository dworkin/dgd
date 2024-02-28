/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

struct StrRef {
    String *str;		/* string value */
    Dataspace *data;		/* dataspace this string is in */
    Uint ref;			/* # of refs */
};

struct ArrRef {
    Array *arr;			/* array value */
    Dataplane *plane;		/* value plane this array is in */
    Dataspace *data;		/* dataspace this array is in */
    short state;		/* state of mapping */
    Uint ref;			/* # of refs */
};

class Value {
public:
    void ref();
    void del();
    static void copy(Value*, Value*, unsigned int);
    static char *typeName(char *buf, unsigned int type);

    static void init(bool stricttc);

    char type;			/* value type */
    bool modified;		/* dirty bit */
    uindex oindex;		/* index in object table */
    union {
	LPCint number;		/* number */
	LPCuint objcnt;		/* object creation count */
	String *string;		/* string */
	Array *array;		/* array or mapping */
    };
};

extern Value zeroInt, zeroFloat, nil;

# define T_TYPE		0x0f	/* type mask */
# define T_NIL		0x00
# define T_INT		0x01
# define T_FLOAT	0x02
# define T_STRING	0x03
# define T_OBJECT	0x04
# define T_ARRAY	0x05	/* value type only */
# define T_MAPPING	0x06
# define T_LWOBJECT	0x07	/* runtime only */
# define T_CLASS	0x07	/* typechecking only */
# define T_MIXED	0x08	/* declaration type only */
# define T_VOID		0x09	/* function return type only */
# define T_LVALUE	0x0a	/* address of a value */

# define T_VARARGS	0x10	/* or'ed with declaration type */
# define T_ELLIPSIS	0x10	/* or'ed with declaration type */

# define T_REF		0xf0	/* reference count mask */
# define REFSHIFT	4

# define T_ARITHMETIC(t) ((t) <= T_FLOAT)
# define T_ARITHSTR(t)	((t) <= T_STRING)
# define T_POINTER(t)	((t) >= T_STRING)
# define T_INDEXED(t)	((t) >= T_ARRAY)   /* T_ARRAY, T_MAPPING, T_LWOBJECT */

# define TYPENAMES	{ "nil", "int", "float", "string", "object", \
			  "array", "mapping", "object", "mixed", "void" }
# define TNBUFSIZE	24

# define VAL_NIL(v)	((v)->type == nil.type && (v)->number == 0)
# define VAL_TRUE(v)	((v)->number != 0 || (v)->type > T_FLOAT ||	\
			 ((v)->type == T_FLOAT && (v)->oindex != 0))

struct DCallOut {
    Uint time;			/* time of call */
    unsigned short mtime;	/* time of call milliseconds */
    uindex nargs;		/* number of arguments */
    Value val[4];		/* function name, 3 direct arguments */
};

class Dataplane : public Allocated {
public:
    Dataplane(Dataspace *data);
    Dataplane(Dataspace *data, LPCint level);

    Array::Backup **commitArray(Array *arr, Dataplane *old);
    void discardArray(Array *arr);

    static void commit(LPCint level, Value *retval);
    static void discard(LPCint level);

    LPCint level;		/* dataplane level */

    short flags;		/* modification flags */
    long schange;		/* # string changes */
    long achange;		/* # array changes */
    long imports;		/* # array imports */

    Value *original;		/* original variables */
    ArrRef alocal;		/* primary of new local arrays */
    ArrRef *arrays;		/* i/o? arrays */
    Array::Backup *achunk;	/* chunk of array backup info */
    StrRef *strings;		/* i/o? string constant table */
    class COPTable *coptab;	/* callout patch table */

    Dataplane *prev;		/* previous in per-dataspace linked list */

private:
    void commitCallouts(bool merge);
    void discardCallouts();

    static void commitValues(Value *v, unsigned int n, LPCint level);

    Dataplane *plist;		/* next in per-level linked list */
};

class Dataspace : public Allocated {
public:
    void ref();
    void deref();
    void del();

    Value *variable(unsigned int idx);
    void assignVar(Value *var, Value *val);
    void assignElt(Array *arr, Value *elt, Value *val);
    uindex allocCallOut(uindex handle, Uint time, unsigned short mtime,
			int nargs, Value *v);
    void freeCallOut(unsigned int handle);
    uindex newCallOut(String *func, LPCint delay, unsigned int mdelay, Frame *f,
		      int nargs);
    LPCint delCallOut(Uint handle, unsigned short *mtime);
    String *callOut(unsigned int handle, Frame *f, int *nargs);
    Array *listCallouts(Dataspace *data);
    void upgrade(unsigned int nvar, unsigned short *vmap, Object *tmpl);

    static Dataspace *create(Object *obj);
    static Dataspace *load(Object *obj);
    static void newVars(Control *ctrl, Value *val);
    static Value *elts(Array *arr);
    static Dataspace *restore(Object *obj, Uint *counttab,
			      void (*readv) (char*, Sector*, Uint, Uint));
    static void refImports(Array *arr);
    static void changeMap(Mapping *map);
    static Value *extra(Dataspace *data);
    static void setExtra(Dataspace *data, Value *val);
    static void wipeExtra(Dataspace *data);
    static Object *upgradeLWO(LWO *lwobj, Object *obj);
    static void xport();
    static void init();
    static void initConv(bool c14, bool c16, bool cfloat);
    static void converted();
    static Sector swapout(unsigned int frag);
    static void upgradeMemory(Object *tmpl, Object *newob);
    static void restoreObject(Object *obj, Uint instance, Uint *counttab,
			      bool cactive, bool dactive);

    Sector *sectors;		/* o vector of sectors */
    Sector nsectors;		/* o # sectors */

    uindex oindex;		/* object this dataspace belongs to */

    unsigned short nvariables;	/* o # variables */
    Value *variables;		/* i/o variables */

    Uint narrays;		/* i/o # arrays */
    Array alist;		/* array linked list sentinel */

    Uint nstrings;		/* i/o # strings */

    uindex ncallouts;		/* # callouts */
    uindex fcallouts;		/* free callout list */
    DCallOut *callouts;		/* callouts */

    class Parser *parser;	/* parse_string data */

    Dataplane *plane;		/* current value plane */
    Dataplane base;		/* basic value plane */

private:
    Dataspace(Object *obj);
    virtual ~Dataspace();

    void freeValues();
# ifdef LARGENUM
    void expand();
# endif
    void loadStrings(void (*readv) (char*, Sector*, Uint, Uint));
    String *string(Uint idx);
    void loadArrays(void (*readv) (char*, Sector*, Uint, Uint));
    Array *array(Uint idx, int type);
    void loadValues(struct SValue *sv, Value *v, int n);
    void loadVars(void (*readv) (char*, Sector*, Uint, Uint));
    void loadElts(void (*readv) (char*, Sector*, Uint, Uint));
    void loadCallouts(void (*readv) (char*, Sector*, Uint, Uint));
    void loadCallouts();
    void saveValues(struct SValue *sv, Value *v, unsigned short n);
    bool save(bool swap);
    void fix(Uint *counttab);
    void refRhs(Value *rhs);
    void delLhs(Value *lhs);
    void upgradeClone();
    void import(struct ArrImport *imp, Value *val, unsigned short n);

    static Dataspace *load(Object *obj,
			   void (*readv) (char*, Sector*, Uint, Uint));
    static Uint convSArray0(struct SArray *sa, Sector *s, Uint n, Uint offset,
			    void (*readv) (char*, Sector*, Uint, Uint));
    static Uint convSString0(struct SString *ss, Sector *s, Uint n, Uint offset,
			     void (*readv) (char*, Sector*, Uint, Uint));
    static void convSCallOut0(struct SCallOut *sco, Sector *s, Uint n,
			      Uint offset,
			      void (*readv) (char*, Sector*, Uint, Uint));
# ifdef LARGENUM
    static void expandValues(struct SValue *v, Uint n);
# endif
    static Dataspace *conv(Object *obj, Uint *counttab,
			   void (*readv) (char*, Sector*, Uint, Uint));
    static void fixObjs(struct SValue *v, Uint n, Uint *ctab);
    static unsigned short *varmap(Object **obj, Uint update,
				  unsigned short *nvariables);

    Dataspace *prev, *next;	/* swap list */
    Dataspace *gcprev, *gcnext;	/* garbage collection list */

    Dataspace *iprev;		/* previous in import list */
    Dataspace *inext;		/* next in import list */

    short flags;		/* various bitflags */
    Control *ctrl;		/* control block */

    struct SValue *svariables;	/* o svariables */
    Uint varoffset;		/* o offset of variables in data space */

    Uint eltsize;		/* o total size of array elements */
    struct SArray *sarrays;	/* o sarrays */
    Uint *saindex;		/* o sarrays index */
    struct SValue *selts;	/* o sarray elements */
    Uint arroffset;		/* o offset of array table in data space */

    Uint strsize;		/* o total size of string text */
    struct SString *sstrings;	/* o sstrings */
    Uint *ssindex;		/* o sstrings index */
    char *stext;		/* o sstrings text */
    Uint stroffset;		/* o offset of string table */

    struct SCallOut *scallouts;	/* o scallouts */
    Uint cooffset;		/* offset of callout table */
};

# define THISPLANE(a)		((a)->plane == (a)->data->plane)
# define SAMEPLANE(d1, d2)	((d1)->plane->level == (d2)->plane->level)

/* bit values for dataspace->flags */
# define DATA_STRCMP		0x03	/* strings compressed */

/* bit values for dataspace->plane->flags */
# define MOD_ALL		0x3f
# define MOD_VARIABLE		0x01	/* variable changed */
# define MOD_ARRAY		0x02	/* array element changed */
# define MOD_ARRAYREF		0x04	/* array reference changed */
# define MOD_STRINGREF		0x08	/* string reference changed */
# define MOD_CALLOUT		0x10	/* callout changed */
# define MOD_NEWCALLOUT		0x20	/* new callout added */
# define PLANE_MERGE		0x40	/* merge planes on commit */
# define MOD_SAVE		0x80	/* save on next full swapout */

# define ARR_MOD		0x80000000L	/* in ArrRef->ref */

# define AR_UNCHANGED		0	/* mapping unchanged */
# define AR_CHANGED		1	/* mapping changed */
