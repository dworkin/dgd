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

extern void		arr_init	P((unsigned int));
extern array	       *arr_alloc	P((unsigned int));
extern array	       *arr_new		P((dataspace*, long));
extern array	       *arr_ext_new	P((dataspace*, long));
# define arr_ref(a)	((a)->ref++)
extern void		arr_del		P((array*));
extern void		arr_freelist	P((array*));
extern void		arr_freeall	P((void));

extern arrmerge	       *arr_merge	P((void));
extern Uint		arr_put		P((arrmerge*, array*, Uint));
extern void		arr_clear	P((arrmerge*));

extern void		arr_backup	P((abchunk**, array*));
extern void		arr_commit	P((abchunk**, dataplane*, int));
extern void		arr_discard	P((abchunk**));

extern array	       *arr_add		P((dataspace*, array*, array*));
extern array	       *arr_sub		P((dataspace*, array*, array*));
extern array	       *arr_intersect	P((dataspace*, array*, array*));
extern array	       *arr_setadd	P((dataspace*, array*, array*));
extern array	       *arr_setxadd	P((dataspace*, array*, array*));
extern unsigned short	arr_index	P((array*, long));
extern void		arr_ckrange	P((array*, long, long));
extern array	       *arr_range	P((dataspace*, array*, long, long));

extern array	       *map_new		P((dataspace*, long));
extern void		map_sort	P((array*));
extern void		map_rmhash	P((array*));
extern void		map_compact	P((dataspace*, array*));
extern unsigned short	map_size	P((dataspace*, array*));
extern array	       *map_add		P((dataspace*, array*, array*));
extern array	       *map_sub		P((dataspace*, array*, array*));
extern array	       *map_intersect	P((dataspace*, array*, array*));
extern value	       *map_index	P((dataspace*, array*, value*, value*));
extern array	       *map_range	P((dataspace*, array*, value*, value*));
extern array	       *map_indices	P((dataspace*, array*));
extern array	       *map_values	P((dataspace*, array*));

extern array	       *lwo_new		P((dataspace*, object*));
extern array	       *lwo_copy	P((dataspace*, array*));
