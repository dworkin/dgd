struct _array_ {
    unsigned short size;		/* number of elements */
    Uint ref;				/* number of references */
    Uint tag;				/* used in sorting */
    Uint odcount;			/* last destructed object count */
    value *elts;			/* elements */
    struct _maphash_ *hashed;		/* hashed mapping elements */
    struct _arrref_ *primary;		/* primary reference */
    array *prev, *next;			/* per-object linked list */
};

extern void		arr_init	P((unsigned int));
extern struct _arrenv_ *arr_new_env	P((void));
extern array	       *arr_alloc	P((lpcenv*, unsigned int));
extern array	       *arr_new		P((dataspace*, long));
extern array	       *arr_ext_new	P((dataspace*, long));
# define arr_ref(a)	((a)->ref++)
extern void		arr_del		P((lpcenv*, array*));
extern void		arr_freelist	P((lpcenv*, array*));
extern void		arr_freeall	P((lpcenv*));

extern struct _arrmerge_ *arr_merge	P((lpcenv*));
extern Uint		arr_put		P((lpcenv*, struct _arrmerge_*, array*,
					   Uint));
extern void		arr_clear	P((lpcenv*, struct _arrmerge_*));

extern struct _abchunk_*arr_backup	P((lpcenv*, struct _abchunk_*, array*));
extern void		arr_commit	P((lpcenv*, struct _abchunk_*,
					   dataplane*, int));
extern void		arr_discard	P((lpcenv*, struct _abchunk_*));

extern array	       *arr_add		P((dataspace*, array*, array*));
extern array	       *arr_sub		P((dataspace*, array*, array*));
extern array	       *arr_intersect	P((dataspace*, array*, array*));
extern array	       *arr_setadd	P((dataspace*, array*, array*));
extern array	       *arr_setxadd	P((dataspace*, array*, array*));
extern unsigned short	arr_index	P((lpcenv*, array*, long));
extern void		arr_ckrange	P((lpcenv*, array*, long, long));
extern array	       *arr_range	P((dataspace*, array*, long, long));

extern array	       *map_new		P((dataspace*, long));
extern void		map_sort	P((lpcenv*, array*));
extern void		map_compact	P((array*));
extern unsigned short	map_size	P((array*));
extern array	       *map_add		P((dataspace*, array*, array*));
extern array	       *map_sub		P((dataspace*, array*, array*));
extern array	       *map_intersect	P((dataspace*, array*, array*));
extern value	       *map_index	P((dataspace*, array*, value*, value*));
extern array	       *map_range	P((dataspace*, array*, value*, value*));
extern array	       *map_indices	P((dataspace*, array*));
extern array	       *map_values	P((dataspace*, array*));

extern array	       *lwo_new		P((dataspace*, object*));
extern array	       *lwo_copy	P((dataspace*, array*));
