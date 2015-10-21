/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2015 DGD Authors (see the commit log for details)
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

struct Object : public hte {
    char flags;			/* object status */
    eindex etabi;		/* index in external table */
    uindex cref;		/* # clone references (sometimes) */
    uindex prev;		/* previous in issue list */
    uindex index;		/* index in object table */
    Uint count;			/* object creation count */
    Uint update;		/* object update count */
    Uint ref;			/* ref count (if master object) */
    Control *ctrl;		/* control block (master object only) */
    Dataspace *data;		/* dataspace block */
    sector cfirst;		/* first sector of control block */
    sector dfirst;		/* first sector of dataspace block */
};
# define u_ref			ref
# define u_master		ref

# define O_MASTER		0x01
# define O_AUTO			0x02
# define O_DRIVER		0x04
# define O_TOUCHED		0x08
# define O_USER			0x10
# define O_EDITOR		0x20
# define O_LWOBJ		0x80

# define O_SPECIAL		0x30

# define OBJ_LAYOUT		"xceuuuiiippdd"

# define OBJ(i)			(&otable[i])
# define OBJR(i)		((BTST(ocmap, (i))) ? o_oread((i)) : &otable[i])
# define OBJW(i)		((!obase) ? o_owrite((i)) : &otable[i])

# define O_UPGRADING(o)		((o)->cref > (o)->u_ref)
# define O_INHERITED(o)		((o)->u_ref - 1 != (o)->cref)
# define O_HASDATA(o)		((o)->data != (Dataspace *) NULL || \
				 (o)->dfirst != SW_UNUSED)

# define OACC_READ		0x00	/* read access */
# define OACC_MODIFY		0x01	/* write access */

# define OBJ_NONE		UINDEX_MAX

extern void	  o_init		(unsigned int, Uint);
extern Object	 *o_oread		(unsigned int);
extern Object	 *o_owrite		(unsigned int);
extern void	  o_new_plane		(void);
extern void	  o_commit_plane	(void);
extern void	  o_discard_plane	(void);

extern bool	  o_space		(void);
extern Object	 *o_new			(char*, Control*);
extern Object	 *o_clone		(Object*);
extern void	  o_lwobj		(Object*);
extern void	  o_upgrade		(Object*, Control*, Frame*);
extern void	  o_upgraded		(Object*, Object*);
extern void	  o_del			(Object*, Frame*);

extern const char	 *o_name		(char*, Object*);
extern char	 *o_builtin_name	(Int);
extern Object	 *o_find		(char*, int);
extern Control   *o_control		(Object*);
extern Dataspace *o_dataspace		(Object*);

extern void	  o_clean		(void);
extern uindex	  o_count		(void);
extern uindex	  o_dobjects		(void);
extern bool	  o_dump		(int, bool);
extern void	  o_restore		(int, unsigned int, bool);
extern bool	  o_copy		(Uint);

extern void	  swapout		(void);
extern void	  dump_state		(bool);
extern void	  finish		(bool);

extern Object    *otable;
extern Uint	 *ocmap;
extern bool	  obase, swap, dump, incr, stop, boot;
extern Uint	  odcount;


# ifdef CLOSURES
# define BUILTIN_FUNCTION	0
# endif
