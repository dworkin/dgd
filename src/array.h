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

struct Array : public ChunkAllocated {
    unsigned short size;		/* number of elements */
    bool hashmod;			/* hashed part contains new elements */
    Uint ref;				/* number of references */
    Uint tag;				/* used in sorting */
    Uint odcount;			/* last destructed object count */
    Value *elts;			/* elements */
    struct maphash *hashed;		/* hashed mapping elements */
    struct arrref *primary;		/* primary reference */
    Array *prev, *next;			/* per-object linked list */
};

class abchunk;				/* array backup chunk */

extern void		arr_init	(unsigned int);
extern Array	       *arr_alloc	(unsigned int);
extern Array	       *arr_new		(Dataspace*, long);
extern Array	       *arr_ext_new	(Dataspace*, long);
# define arr_ref(a)	((a)->ref++)
extern void		arr_del		(Array*);
extern void		arr_freelist	(Array*);
extern void		arr_freeall	();

extern void		arr_merge	();
extern Uint		arr_put		(Array*, Uint);
extern void		arr_clear	();

extern void		arr_backup	(abchunk**, Array*);
extern void		arr_commit	(abchunk**, Dataplane*, bool);
extern void		arr_discard	(abchunk**);

extern Array	       *arr_add		(Dataspace*, Array*, Array*);
extern Array	       *arr_sub		(Dataspace*, Array*, Array*);
extern Array	       *arr_intersect	(Dataspace*, Array*, Array*);
extern Array	       *arr_setadd	(Dataspace*, Array*, Array*);
extern Array	       *arr_setxadd	(Dataspace*, Array*, Array*);
extern unsigned short	arr_index	(Array*, long);
extern void		arr_ckrange	(Array*, long, long);
extern Array	       *arr_range	(Dataspace*, Array*, long, long);

extern Array	       *map_new		(Dataspace*, long);
extern void		map_sort	(Array*);
extern void		map_rmhash	(Array*);
extern void		map_compact	(Dataspace*, Array*);
extern unsigned short	map_size	(Dataspace*, Array*);
extern Array	       *map_add		(Dataspace*, Array*, Array*);
extern Array	       *map_sub		(Dataspace*, Array*, Array*);
extern Array	       *map_intersect	(Dataspace*, Array*, Array*);
extern Value	       *map_index	(Dataspace*, Array*, Value*, Value*,
					 Value*);
extern Array	       *map_range	(Dataspace*, Array*, Value*, Value*);
extern Array	       *map_indices	(Dataspace*, Array*);
extern Array	       *map_values	(Dataspace*, Array*);

extern Array	       *lwo_new		(Dataspace*, Object*);
extern Array	       *lwo_copy	(Dataspace*, Array*);
