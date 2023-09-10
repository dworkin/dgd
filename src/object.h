/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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

# include "hash.h"
# include "swap.h"

# define OACC_READ		0x00	/* read access */
# define OACC_MODIFY		0x01	/* write access */

class Object : public Hash::Entry {
public:
    Object *clone();
    void lightWeight();
    void upgrade(Control *ctrl, Frame *f);
    void upgraded(Object *tmpl);
    void del(Frame *f);

    const char *objName(char *name);
    Control *control();
    Dataspace *dataspace();

    static void init(unsigned int n, Uint interval);
    static void newPlane();
    static void commitPlane();
    static void discardPlane();

    static Object *oread(unsigned int index) {
	return (BTST(ocmap, index)) ?
		access(index, OACC_READ) : &objTable[index];
    }
    static Object *owrite(unsigned int index) {
	return (!base) ? access(index, OACC_MODIFY) : &objTable[index];
    }
    static Object *create(char *name, Control *ctrl);
    static const char *builtinName(LPCint type);
    static Object *find(char *name, int access);

    static bool space();
    static void clean();
    static uindex ocount();
    static uindex dobjects();
    static bool save(int fd, bool incr);
    static void restore(int fd, bool part);
    static bool copy(Uint time);

    static void swapout();
    static void dumpState(bool incr);
    static void finish(bool boot);

    char flags;			/* object status */
    eindex etabi;		/* index in external table */
    uindex cref;		/* # clone references (sometimes) */
    uindex prev;		/* previous in issue list */
    uindex index;		/* index in object table */
    Uint count;			/* object creation count */
    Uint update;		/* object update count */
    union {
	Uint ref;		/* ref count (if master object) */
	Uint master;		/* master (if clone) */
    };
    Control *ctrl;		/* control block (master object only) */
    Dataspace *data;		/* dataspace block */
    Sector cfirst;		/* first sector of control block */
    Sector dfirst;		/* first sector of dataspace block */

    static Object *objTable;
    static bool swap, dump, incr, stop, boot;
    static Uint objDestrCount;

private:
    void remove(Frame *f);
    void restoreObject(bool cactive, bool dactive);
    bool purgeUpgrades();

    static Object *access(unsigned int index, int access);
    static Object *alloc();
    static void sweep(uindex n);
    static Uint recount(uindex n);
    static void cleanUpgrades();

    static Uint *ocmap;
    static bool base;
    static bool rcount;
    static uindex otabsize;
    static uindex uobjects;
    static Uint *omap;
    static Uint *counttab;
    static Uint *insttab;
    static Object *upgradeList;
    static uindex ndobject, dobject;
    static uindex mobjects;
    static uindex dchunksz;
    static Uint dinterval;
    static Uint dtime;
};

# define O_MASTER		0x01
# define O_AUTO			0x02
# define O_DRIVER		0x04
# define O_TOUCHED		0x08
# define O_USER			0x10
# define O_EDITOR		0x20
# define O_COMPILED		0x40
# define O_LWOBJ		0x80

# define O_SPECIAL		0x30

# define OBJ_LAYOUT		"xceuuuiiippdd"

# define OBJ(i)			(&Object::objTable[i])
# define OBJR(i)		(Object::oread((i)))
# define OBJW(i)		(Object::owrite((i)))

# define O_UPGRADING(o)		((o)->cref > (o)->ref)
# define O_INHERITED(o)		((o)->ref - 1 != (o)->cref)
# define O_HASDATA(o)		((o)->data != (Dataspace *) NULL || \
				 (o)->dfirst != SW_UNUSED)

# define OBJ_NONE		UINDEX_MAX

# ifdef CLOSURES
# define BUILTIN_FUNCTION	0
# endif
