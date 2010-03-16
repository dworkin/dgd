/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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

struct _object_ {
    hte chain;			/* object name hash table */
    char flags;			/* object status */
    eindex etabi;		/* index in external table */
    uindex cref;		/* # clone references (sometimes) */
    uindex prev;		/* previous in issue list */
    uindex index;		/* index in object table */
    Uint count;			/* object creation count */
    Uint update;		/* object update count */
    Uint ref;			/* ref count (if master object) */
    control *ctrl;		/* control block (master object only) */
    dataspace *data;		/* dataspace block */
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
# define O_COMPILED		0x40
# define O_LWOBJ		0x80

# define O_SPECIAL		0x30

# define OBJ_LAYOUT		"xceuuuiiippdd"

# define OBJ(i)			(&otable[i])
# define OBJR(i)		((BTST(ocmap, (i))) ? o_oread((i)) : &otable[i])
# define OBJW(i)		((!obase) ? o_owrite((i)) : &otable[i])

# define O_UPGRADING(o)		((o)->cref > (o)->u_ref)
# define O_INHERITED(o)		((o)->u_ref - 1 != (o)->cref)
# define O_HASDATA(o)		((o)->data != (dataspace *) NULL || \
				 (o)->dfirst != SW_UNUSED)

# define OACC_READ		0x00	/* read access */
# define OACC_MODIFY		0x01	/* write access */

# define OBJ_NONE		UINDEX_MAX

extern void	  o_init		(unsigned int, Uint);
extern object	 *o_oread		(unsigned int);
extern object	 *o_owrite		(unsigned int);
extern void	  o_new_plane		(void);
extern void	  o_commit_plane	(void);
extern void	  o_discard_plane	(void);

extern bool	  o_space		(void);
extern object	 *o_new			(char*, control*);
extern object	 *o_clone		(object*);
extern void	  o_lwobj		(object*);
extern void	  o_upgrade		(object*, control*, frame*);
extern void	  o_upgraded		(object*, object*);
extern void	  o_del			(object*, frame*);

extern char	 *o_name		(char*, object*);
extern object	 *o_find		(char*, int);
extern control   *o_control		(object*);
extern dataspace *o_dataspace		(object*);

extern void	  o_clean		(void);
extern uindex	  o_count		(void);
extern bool	  o_dump		(int);
extern void	  o_restore		(int, unsigned int);
extern bool	  o_copy		(Uint);

extern void	  swapout		(void);
extern void	  dump_state		(void);
extern void	  finish		(void);

extern object    *otable;
extern char	 *ocmap;
extern bool	  obase, swap, dump, stop;
extern Uint	  odcount;
