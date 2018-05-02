/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
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

class Object : public Hashtab::Entry {
public:
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
    sector cfirst;		/* first sector of control block */
    sector dfirst;		/* first sector of dataspace block */

    Object *clone();
    void lightWeight();
    void upgrade(Control*, Frame*);
    void upgraded(Object*);
    void del(Frame*);

    const char *objName(char*);
    Control *control();
    Dataspace *dataspace();

    static void init(unsigned int, Uint);
    static void newPlane();
    static void	commitPlane();
    static void	discardPlane();

    static Object *oread(unsigned int);
    static Object *owrite(unsigned int);
    static Object *create(char*, Control*);
    static const char *builtinName(Int);
    static Object *find(char*, int);

    static bool	space();
    static void	clean();
    static uindex ocount();
    static uindex dobjects();
    static bool save(int, bool);
    static void restore(int, bool);
    static bool	copy(Uint);

    static void	swapout();
    static void	dumpState(bool);
    static void finish(bool);

    static Object *otable;
    static Uint *ocmap;
    static bool obase, swap, dump, incr, stop, boot;
    static Uint odcount;

private:
    static Object *access(unsigned int, int);
    static void sweep(uindex);
    static Uint recount(uindex);
};

# define O_MASTER		0x01
# define O_AUTO			0x02
# define O_DRIVER		0x04
# define O_TOUCHED		0x08
# define O_USER			0x10
# define O_EDITOR		0x20
# define O_LWOBJ		0x80

# define O_SPECIAL		0x30

# define OBJ_LAYOUT		"xceuuuiiippdd"

# define OBJ(i)			(&Object::otable[i])
# define OBJR(i)		((BTST(Object::ocmap, (i))) ? Object::oread((i)) : &Object::otable[i])
# define OBJW(i)		((!Object::obase) ? Object::owrite((i)) : &Object::otable[i])

# define O_UPGRADING(o)		((o)->cref > (o)->ref)
# define O_INHERITED(o)		((o)->ref - 1 != (o)->cref)
# define O_HASDATA(o)		((o)->data != (Dataspace *) NULL || \
				 (o)->dfirst != SW_UNUSED)

# define OACC_READ		0x00	/* read access */
# define OACC_MODIFY		0x01	/* write access */

# define OBJ_NONE		UINDEX_MAX

# ifdef CLOSURES
# define BUILTIN_FUNCTION	0
# endif
