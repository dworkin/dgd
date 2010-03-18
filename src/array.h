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

struct _array_ {
    unsigned short size;		/* number of elements */
    bool hashmod;			/* hashed part contains new elements */
    Uint ref;				/* number of references */
    Uint tag;				/* used in sorting */
    Uint odcount;			/* last destructed object count */
    value *elts;			/* elements */
    struct _maphash_ *hashed;		/* hashed mapping elements */
    struct _arrref_ *primary;		/* primary reference */
    array *prev, *next;			/* per-object linked list */
};

typedef struct _arrmerge_ arrmerge;	/* array merge table */
typedef struct _abchunk_ abchunk;	/* array backup chunk */

extern void		arr_init	(unsigned int);
extern array	       *arr_alloc	(unsigned int);
extern array	       *arr_new		(dataspace*, long);
extern array	       *arr_ext_new	(dataspace*, long);
# define arr_ref(a)	((a)->ref++)
extern void		arr_del		(array*);
extern void		arr_freelist	(array*);
extern void		arr_freeall	(void);

extern void		arr_merge	(void);
extern Uint		arr_put		(array*, Uint);
extern void		arr_clear	(void);

extern void		arr_backup	(abchunk**, array*);
extern void		arr_commit	(abchunk**, dataplane*, int);
extern void		arr_discard	(abchunk**);

extern array	       *arr_add		(dataspace*, array*, array*);
extern array	       *arr_sub		(dataspace*, array*, array*);
extern array	       *arr_intersect	(dataspace*, array*, array*);
extern array	       *arr_setadd	(dataspace*, array*, array*);
extern array	       *arr_setxadd	(dataspace*, array*, array*);
extern unsigned short	arr_index	(array*, long);
extern void		arr_ckrange	(array*, long, long);
extern array	       *arr_range	(dataspace*, array*, long, long);

extern array	       *map_new		(dataspace*, long);
extern void		map_sort	(array*);
extern void		map_rmhash	(array*);
extern void		map_compact	(dataspace*, array*);
extern unsigned short	map_size	(dataspace*, array*);
extern array	       *map_add		(dataspace*, array*, array*);
extern array	       *map_sub		(dataspace*, array*, array*);
extern array	       *map_intersect	(dataspace*, array*, array*);
extern value	       *map_index	(dataspace*, array*, value*, value*);
extern array	       *map_range	(dataspace*, array*, value*, value*);
extern array	       *map_indices	(dataspace*, array*);
extern array	       *map_values	(dataspace*, array*);

extern array	       *lwo_new		(dataspace*, object*);
extern array	       *lwo_copy	(dataspace*, array*);
